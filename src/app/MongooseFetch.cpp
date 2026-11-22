
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
    // When > 0, the request will ask the server for a byte-range starting at
    // this offset, so a previously truncated download can be resumed.
    long long rangeStart{0};
    // Total resource size as reported by a 206 Partial Content response's
    // Content-Range header. -1 means unknown.
    long long contentRangeTotal{-1};
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
        std::string rangeHeader;
        if (s->rangeStart > 0) {
            rangeHeader = "Range: bytes=" + std::to_string(s->rangeStart) + "-\r\n";
        }
        mg_printf(c,
                  "GET %s HTTP/1.1\r\n"
                  "Host: %s\r\n"
                  "User-Agent: SolarSim\r\n"
                  "%s"
                  "%s"
                  "Connection: close\r\n\r\n",
                  mg_url_uri(s->url.c_str()), hostHeader.c_str(),
                  s->authHeader.c_str(), rangeHeader.c_str());
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
        if (mg_str* cr = mg_http_get_header(hm, "Content-Range")) {
            // Expected format: "bytes start-end/total"
            const std::string crStr(cr->buf, cr->len);
            const std::size_t slashPos = crStr.find('/');
            if (slashPos != std::string::npos) {
                const std::string totalStr = crStr.substr(slashPos + 1);
                char* end = nullptr;
                const long long total = std::strtoll(totalStr.c_str(), &end, 10);
                if (end != totalStr.c_str() && total >= 0) {
                    s->contentRangeTotal = total;
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


namespace {

// Performs a single HTTP request (following redirects) starting at the given
// byte offset. Returns true if a (possibly partial) response body was
// received and appended to `accumulatedBody`.
bool fetchOnce(std::string url, const std::string& userNamePassAuthHeader,
               long long rangeStart, const int timeOutSeconds,
               std::string& accumulatedBody, long long& totalSize, bool& truncated,
               int& finalStatus)
{
    constexpr int kMaxRedirects = 5;
    const auto kTimeout = timeOutSeconds > 0 ? std::chrono::seconds(timeOutSeconds) : std::chrono::seconds(120);

    MgFetchState state;
    state.url = std::move(url);
    state.authHeader = userNamePassAuthHeader;
    state.rangeStart = rangeStart;

    for (int redirects = 0; redirects <= kMaxRedirects; ++redirects) {
        state.done = false;
        state.gotResponse = false;
        state.status = 0;
        state.location.clear();
        state.body.clear();
        state.error.clear();
        state.contentLength = -1;
        state.contentRangeTotal = -1;
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

    if (!state.gotResponse) {
        return false;
    }

    finalStatus = state.status;
    // Accept both a full response (200) and a resumed partial response (206).
    if (state.status != 200 && state.status != 206) {
        return false;
    }

    accumulatedBody.append(state.body);
    truncated = state.truncated;
    if (state.status == 206 && state.contentRangeTotal >= 0) {
        totalSize = state.contentRangeTotal;
    } else if (state.contentLength >= 0) {
        totalSize = rangeStart + state.contentLength;
    }
    return true;
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

    std::string authHeader;
    if (!userName.empty() && !password.empty()) {
        const QByteArray cred = QByteArray::fromStdString(userName + ':' + password).toBase64();
        authHeader = "Authorization: Basic " + cred.toStdString() + "\r\n";
    }

    // Retry the download a number of times, resuming from the last received
    // byte offset via an HTTP Range request whenever the connection is
    // closed before the full body has been received. This makes the download
    // resilient against the connection being dropped near the end of large
    // transfers.
    constexpr int kMaxAttempts = 5;
    std::string body;
    long long totalSize = -1;
    bool truncated = false;
    int finalStatus = 0;
    bool ok = false;

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const long long rangeStart = static_cast<long long>(body.size());
        truncated = false;
        ok = fetchOnce(url, authHeader, rangeStart, timeOutSeconds, body, totalSize, truncated, finalStatus);
        if (!ok) {
            break;
        }
        if (!truncated) {
            break; // got a complete response
        }
        // Partial response received; loop again to resume from where we left off.
    }

    if (!ok || (finalStatus != 200 && finalStatus != 206)) {
        return false;
    }
    if (truncated) {
        // Ran out of retry attempts while still truncated.
        return false;
    }
    if (totalSize >= 0 && static_cast<long long>(body.size()) < totalSize) {
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
        out.write(body.data(), static_cast<std::streamsize>(body.size()));
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

    return true;
}

} // namespace app
