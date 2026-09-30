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

//GeoDataSources::GeoDataSources() {}
//GeoDataSources::~GeoDataSources() = default;


/// Fetches `url` into the local file `targetPath` using Qt Network. Runs a
/// nested event loop, so this must be called from the GUI thread.
bool GeoDataSources::fetchUrl(const std::string& url, const std::string& targetPath,
                const std::string& userName, const std::string& password)
{
    m_progressCount++;
    progress(m_progressCount, targetPath);

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
    return QDir(base + "/../").filePath(QString(subDirectory.c_str())).toStdString();

    // attention:
    // QString::fromStdString() works in tests and Exampke but not in actual Qt Application
    // for now we use QString(stdstring.c_str()) instead
    auto dir = QDir(base + "/../");
    auto relpath = QString(subDirectory.c_str());
    auto fullpath = dir.filePath(relpath);
    //https://stackoverflow.com/questions/4214369/how-to-convert-qstring-to-stdstring
    auto std_text = fullpath.toStdString();
    auto utf8_text = fullpath.toUtf8().constData();
    auto current_locale_text = fullpath.toLocal8Bit().constData();
    auto stdpath = std::string(current_locale_text);
    return stdpath;
}


void GeoDataSources::registerDataSources()
{
    if (!m_height_data_sources_registry) {
        m_height_data_sources_registry = &(geo::HeightDataSourceRegistry::instance());

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
        };

        m_worldDem30TileDownloader = std::make_shared<geo::WorldCopernicusDem30TileDownloader>(worldConfig, worldFetch);

        m_height_data_sources_registry->addSource(
            std::make_shared<geo::WorldCopernicusDem30HeightDataSource>(m_worldDem30TileDownloader->tileLoader())
        );
    }
}


// for Qt based DataSource load prgress 
//#include <QProgressDialog>
//class DataSourceProgress : public geo::GeoDataSource::ProgressClass {
//public:
//  DataSourceProgress(QWidget* parent) {
//    m_progressDialog = std::make_shared<QProgressDialog>("Downloading tiles...", QString(), 0, 0, parent);
//    m_progressDialog->setWindowModality(Qt::WindowModal);
//    m_progressDialog->setCancelButton(nullptr);
//    m_progressDialog->setMinimumDuration(0);
//    m_progressDialog->setWindowTitle("Loading terrain");
//    m_progressDialog->show();
//  }
//
//  ~DataSourceProgress() {
//    m_progressDialog->close();
//    m_progressDialog = nullptr;
//  }
//
//  virtual void progress(int percent, std::string msg) {
//    m_progressDialog->setRange(1, 100);
//    m_progressDialog->setValue(percent);
//  }
//
//private:
//  std::shared_ptr<QProgressDialog> m_progressDialog;
//};

//void GeoDataSources::enableProgress() {
//  auto progressClass = std::make_shared<QtDataSourceProgress>();
//  for (auto& source : m_height_data_sources_registry->sources()) {
//    source->setProgressClass(progressClass);
//  }
//}
//void GeoDataSources::disablesProgress() {
//  for (auto& source : m_height_data_sources_registry->sources()) {
//    source->setProgressClass(nullptr);
//  }
//}

namespace {

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

void mgFetchHandler(mg_connection* c, int ev, void* evData)
{
    auto* s = static_cast<MgFetchState*>(c->fn_data);
    if (ev == MG_EV_CONNECT) {
        mg_str host = mg_url_host(s->url.c_str());
        if (mg_url_is_ssl(s->url.c_str())) {
            mg_tls_opts opts;
            std::memset(&opts, 0, sizeof(opts));
            opts.name = host;
            mg_tls_init(c, &opts);
        }
        mg_printf(c,
                  "GET %s HTTP/1.1\r\n"
                  "Host: %.*s\r\n"
                  "User-Agent: SolarSim\r\n"
                  "%s"
                  "Connection: close\r\n\r\n",
                  mg_url_uri(s->url.c_str()), static_cast<int>(host.len), host.buf,
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
        s->error = static_cast<const char*>(evData);
        s->done = true;
    } else if (ev == MG_EV_CLOSE) {
        s->done = true;
    }
}

} // namespace

bool GeoDataSources::fetchUrlMongoose(const std::string& url, const std::string& targetPath,
                                      const std::string& userName, const std::string& password)
{
    m_progressCount++;
    progress(m_progressCount, targetPath);

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
            state.url = state.location; // absolute Location assumed
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
    fs::rename(tmp, target, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}
