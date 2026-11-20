
#include "MongooseFetch.h"

extern "C" {
#include "mongoose.h"
}

#include <QByteArray>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace app {

namespace {

// Set log printer function which prints one byte. Example:
//
static void console_print_char(char ch, void *param) {
    //hal_uart_write_char(param, ch);
    printf("%c", ch);
}
//mg_log_set_fn(console_print_char, nullptr);

struct MgFetchState {
    std::string url;
    std::string authHeader;
    bool done{false};
    bool gotResponse{false};
    int status{0};
    std::string location;
    std::string body;
    std::string error;
    // -1 means no Content-Length header was present (e.g. chunked transfer).
    long long contentLength{-1};
    bool truncated{false};
};

std::string resolveRedirect(const std::string& baseUrl, const std::string& location)
{
#if DEBUG
    printf("resolveRedirect()... baseUrl='%s' location='%s'\n", baseUrl.c_str(), location.c_str());
#endif
    if (location.empty() || location.find("://") != std::string::npos) {
        return location;
    }

    const std::size_t schemeEnd = baseUrl.find("://");
    const std::size_t authorityEnd = schemeEnd == std::string::npos
        ? std::string::npos
        : baseUrl.find('/', schemeEnd + 3);
    if (schemeEnd == std::string::npos || authorityEnd == std::string::npos) {
        return location;
    }

    const std::string origin = baseUrl.substr(0, authorityEnd);
    if (location.rfind("//", 0) == 0) {
        return baseUrl.substr(0, schemeEnd) + ":" + location;
    }
    if (location.front() == '/') {
        return origin + location;
    }

    const std::string currentUri = mg_url_uri(baseUrl.c_str());
    const std::size_t lastSlash = currentUri.rfind('/');
    return origin + (lastSlash == std::string::npos ? "/" : currentUri.substr(0, lastSlash + 1)) + location;
}

bool sameOrigin(const std::string& first, const std::string& second)
{
#if DEBUG
    printf("sameOrigin()... first='%s' second='%s'\n", first.c_str(), second.c_str());
#endif
    if (mg_url_is_ssl(first.c_str()) != mg_url_is_ssl(second.c_str()) ||
        mg_url_port(first.c_str()) != mg_url_port(second.c_str())) {
        return false;
    }

    const mg_str firstHost = mg_url_host(first.c_str());
    const mg_str secondHost = mg_url_host(second.c_str());
    return firstHost.len == secondHost.len &&
        std::strncmp(firstHost.buf, secondHost.buf, firstHost.len) == 0;
}

void mgFetchHandler(mg_connection* c, int ev, void* evData)
{
#if DEBUG
    printf("mgFetchHandler()... mg_connection='%p' ev='%i' evData='%p'\n", static_cast<void*>(c), ev, evData);
#endif
    auto* s = static_cast<MgFetchState*>(c->fn_data);
    if (ev == MG_EV_CONNECT) {
        if (evData != nullptr && *static_cast<int*>(evData) != 0) {
            s->done = true;
            return;
        }
        mg_str host = mg_url_host(s->url.c_str());
        std::string hostHeader(host.buf, host.len);
        const unsigned short port = mg_url_port(s->url.c_str());
        const unsigned short defaultPort = mg_url_is_ssl(s->url.c_str()) ? 443 : 80;
        if (port != 0 && port != defaultPort) {
            hostHeader += ':' + std::to_string(port);
        }
        if (mg_url_is_ssl(s->url.c_str())) {
            mg_tls_opts opts;
            std::memset(&opts, 0, sizeof(opts));
            opts.name = host;
            mg_tls_init(c, &opts);
        }
        mg_printf(c,
                  "GET %s HTTP/1.1\r\n"
                  "Host: %s\r\n"
                  "User-Agent: SolarSim\r\n"
                  "%s"
                  "Connection: close\r\n\r\n",
                  mg_url_uri(s->url.c_str()), hostHeader.c_str(),
                  s->authHeader.c_str());
    } else if (ev == MG_EV_HTTP_MSG) {
        auto* hm = static_cast<mg_http_message*>(evData);
        s->gotResponse = true;
        s->status = mg_http_status(hm);
        if (mg_str* loc = mg_http_get_header(hm, "Location")) {
            s->location.assign(loc->buf, loc->len);
        }
        s->body.assign(hm->body.buf, hm->body.len);
        // Mongoose still delivers MG_EV_HTTP_MSG when the peer closes the
        // connection before all Content-Length bytes arrived (it just
        // reports whatever partial body made it into the receive buffer).
        // Detect that case explicitly so truncated downloads are not mistaken
        // for successful ones.
        if (mg_str* cl = mg_http_get_header(hm, "Content-Length")) {
            const std::string clStr(cl->buf, cl->len);
            char* end = nullptr;
            const long long expected = std::strtoll(clStr.c_str(), &end, 10);
            if (end != clStr.c_str() && expected >= 0) {
                s->contentLength = expected;
                if (static_cast<long long>(s->body.size()) < expected) {
                    s->truncated = true;
                }
            }
        }
        c->is_closing = 1;
        s->done = true;
    } else if (ev == MG_EV_ERROR) {
        if (evData != nullptr) {
            s->error = static_cast<const char*>(evData);
        }
        s->done = true;
    } else if (ev == MG_EV_CLOSE) {
        s->done = true;
    }
}

} // namespace


bool fetchUrlMongoose(const std::string& url, const std::string& targetPath,
                      const std::string& userName, const std::string& password,
                      const int timeOutSeconds)
{
#if DEBUG
    printf("fetchUrlMongoose()... url='%s' targetPath='%s' userName='%s'\n", url.c_str(), targetPath.c_str(), userName.c_str());
#endif
    mg_log_set_fn(console_print_char, nullptr);

    constexpr int kMaxRedirects = 5;
    const auto kTimeout = timeOutSeconds > 0 ? std::chrono::seconds(timeOutSeconds) : std::chrono::seconds(120);

    MgFetchState state;
    state.url = url;
    if (!userName.empty() && !password.empty()) {
        const QByteArray cred = QByteArray::fromStdString(userName + ':' + password).toBase64();
        state.authHeader = "Authorization: Basic " + cred.toStdString() + "\r\n";
    }

    for (int redirects = 0; redirects <= kMaxRedirects; ++redirects) {
        state.done = false;
        state.gotResponse = false;
        state.status = 0;
        state.location.clear();
        state.body.clear();
        state.error.clear();
        state.contentLength = -1;
        state.truncated = false;

        mg_mgr mgr;
        mg_mgr_init(&mgr);
        mg_connection* c = mg_http_connect(&mgr, state.url.c_str(), mgFetchHandler, &state);
        if (c == nullptr) {
            mg_mgr_free(&mgr);
            return false;
        }

        if (timeOutSeconds > 0) {
            const auto deadline = std::chrono::steady_clock::now() + kTimeout;
            while (!state.done && std::chrono::steady_clock::now() < deadline) {
                mg_mgr_poll(&mgr, 100);
            }
        }
        else {
            while (!state.done) {
                mg_mgr_poll(&mgr, 200);
            }
        }
        mg_mgr_free(&mgr);

        if (!state.gotResponse) {
            return false; // connection error or timeout
        }
        if (state.status >= 300 && state.status < 400 && !state.location.empty()) {
            const std::string nextUrl = resolveRedirect(state.url, state.location);
            if (nextUrl.empty() || nextUrl.find("://") == std::string::npos) {
                return false;
            }
            if (!sameOrigin(state.url, nextUrl)) {
                state.authHeader.clear();
            }
            state.url = nextUrl;
            continue;
        }
        break;
    }

    if (!state.gotResponse || state.status != 200 /*|| state.truncated*/) {
        return false;
    }

    namespace fs = std::filesystem;
    const fs::path target(targetPath);
    std::error_code ec;
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path(), ec);
        if (ec) {
            return false;
        }
    }

    const fs::path tmp = fs::path(targetPath + ".part");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(state.body.data(), static_cast<std::streamsize>(state.body.size()));
        if (!out.good()) {
            out.close();
            fs::remove(tmp, ec);
            return false;
        }
    }
    const bool targetExists = fs::exists(target, ec);
    if (ec || (targetExists && !fs::remove(target, ec)) || ec) {
        fs::remove(tmp, ec);
        return false;
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    
    //return true;
    return !state.truncated;
}

} // namespace app
