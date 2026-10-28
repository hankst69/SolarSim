#pragma once

#include "GeoDataSources.h"

// for specific HeightDataSources
#include "geolib/GridHeightDataSource.h"
#include "geolib/HeightDataSourceRegistry.h"
#include "geolib/data_sources/BavariaDgm1HeightDataSource.h"
#include "geolib/data_sources/BavariaDgm1TileDownloader.h"
#include "geolib/data_sources/WorldCopernicusDem30HeightDataSource.h"
#include "geolib/data_sources/WorldCopernicusDem30TileDownloader.h"

// for Qt based fetchUrl implementation
#include <QByteArray>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QString>
#include <QUrl>

extern "C" {
#include "mongoose.h"
}
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>

GeoDataSources::GeoDataSources() 
{
    m_height_data_sources_registry = &(geo::HeightDataSourceRegistry::instance());
    //m_texture_data_sources_registry = &(geo::TextureDataSourceRegistry::instance());
}

GeoDataSources::~GeoDataSources() = default;


/// Fetches `url` into the local file `targetPath` using Qt Network. Runs a
/// nested event loop, so this must be called from the GUI thread.
bool GeoDataSources::fetchUrl(const std::string& url, const std::string& targetPath,
                const std::string& userName, const std::string& password)
{
    m_progressCount++;
    progress(m_progressCount, "downloadinig " + targetPath);

    static QNetworkAccessManager manager;

    QNetworkRequest request{QUrl(QString::fromStdString(url))};
    if (!userName.empty() && !password.empty()) {
        const QByteArray credentials =
            (QString::fromStdString(userName) + ':' + QString::fromStdString(password))
                .toUtf8()
                .toBase64();
        request.setRawHeader("Authorization", "Basic " + credentials);
    }

    QNetworkReply* reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const bool ok = reply->error() == QNetworkReply::NoError;
    if (ok) {
        const QFileInfo targetInfo(QString::fromStdString(targetPath));
        QDir().mkpath(targetInfo.absolutePath());

        QFile output(targetInfo.absoluteFilePath());
        if (output.open(QIODevice::WriteOnly)) {
            output.write(reply->readAll());
        }
    }

    reply->deleteLater();
    return ok;
}


std::string GeoDataSources::cacheDirectoryFor(const std::string& subDirectory)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    //return QDir(base).filePath(QString::fromStdString(subDirectory)).toStdString();
    const QByteArray utf8 = QDir(base + QStringLiteral("/../"))
        .filePath(QString::fromUtf8(subDirectory.c_str()))
        .toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
    //// attention:
    //// QString::fromStdString() works in tests and Example but not in actual Qt Application
    //// for now we use QString(stdstring.c_str()) instead
    //auto dir = QDir(base + "/../");
    //auto relpath = QString(subDirectory.c_str());
    //auto fullpath = dir.filePath(relpath);
    ////https://stackoverflow.com/questions/4214369/how-to-convert-qstring-to-stdstring
    //auto std_text = fullpath.toStdString();
    //auto utf8_text = fullpath.toUtf8().constData();
    //auto current_locale_text = fullpath.toLocal8Bit().constData();
    //auto stdpath = std::string(current_locale_text);
    //return stdpath;
}


void GeoDataSources::registerDataSources()
{
    if (!m_height_data_sources_registry) {
        m_height_data_sources_registry = &(geo::HeightDataSourceRegistry::instance());
    }

    for (const auto& source : m_height_data_sources_registry->sources()) {
        if (source->name().find("DGM1") != std::string::npos ||
            source->name().find("Copernicus") != std::string::npos) {
            // Already registered.
            return;
        }
    }

    // register the BavariaDgm1 HeightDataSource
    geo::BavariaDgm1TileDownloader::Config byConfig{};
    byConfig.cacheDirectory = cacheDirectoryFor("height_data_cache/bavaria_dgm1");
    byConfig.baseUrl = "https://download1.bayernwolke.de/a/dgm/dgm1xyz";
    byConfig.fileExtension = ".zip";
    byConfig.allowDownload = true;

    geo::BavariaDgm1TileDownloader::FetchFunction byFetch =
        [this](const std::string& url, const std::string& targetPath) -> bool {
            return fetchUrlMongoose(url, targetPath, {}, {});
            //return fetchUrl(url, targetPath, {}, {});
        };

    m_bavariaDgm1TileDownloader = std::make_shared<geo::BavariaDgm1TileDownloader>(byConfig, byFetch);

    m_height_data_sources_registry->addSource(
        std::make_shared<geo::BavariaDgm1HeightDataSource>(m_bavariaDgm1TileDownloader->tileLoader())
    );

    // register the WorldCopernicusDem30 HeightDataSource
    geo::WorldCopernicusDem30TileDownloader::Config worldConfig{};
    worldConfig.cacheDirectory = cacheDirectoryFor("height_data_cache/world_copernicus_dem30");
    worldConfig.baseUrl = "https://copernicus-dem-30m.s3.amazonaws.com";
    worldConfig.fileExtension = ".hgt";
    worldConfig.allowDownload = true;

    static const std::string kCopernicusUserName = "";
    static const std::string kCopernicusPassword = "";

    geo::WorldCopernicusDem30TileDownloader::FetchFunction worldFetch =
        [this](const std::string& url, const std::string& targetPath) -> bool {
            return fetchUrlMongoose(url, targetPath, kCopernicusUserName, kCopernicusPassword);
            //return fetchUrl(url, targetPath, kCopernicusUserName, kCopernicusPassword);
    };

    m_worldDem30TileDownloader = std::make_shared<geo::WorldCopernicusDem30TileDownloader>(worldConfig, worldFetch);

    m_height_data_sources_registry->addSource(
        std::make_shared<geo::WorldCopernicusDem30HeightDataSource>(m_worldDem30TileDownloader->tileLoader())
    );
}



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
};

std::string resolveRedirect(const std::string& baseUrl, const std::string& location)
{
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


bool GeoDataSources::fetchUrlMongoose(const std::string& url, const std::string& targetPath,
                                      const std::string& userName, const std::string& password)
{
    mg_log_set_fn(console_print_char, nullptr);

    m_progressCount++;
    progress(m_progressCount, "downloadinig " + targetPath);

    constexpr int kMaxRedirects = 5;
    constexpr auto kTimeout = std::chrono::seconds(120);

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

        mg_mgr mgr;
        mg_mgr_init(&mgr);
        mg_connection* c = mg_http_connect(&mgr, state.url.c_str(), mgFetchHandler, &state);
        if (c == nullptr) {
            mg_mgr_free(&mgr);
            return false;
        }

        const auto deadline = std::chrono::steady_clock::now() + kTimeout;
        while (!state.done && std::chrono::steady_clock::now() < deadline) {
            mg_mgr_poll(&mgr, 50);
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

    if (!state.gotResponse || state.status != 200) {
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
    return true;
}
