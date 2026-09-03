#include "HeightDataSources.h"

#include "geolib/GridHeightDataSource.h"
#include "geolib/HeightDataSourceRegistry.h"
#include "geolib/data_sources/BavariaDgm1HeightDataSource.h"
#include "geolib/data_sources/BavariaDgm1TileDownloader.h"
#include "geolib/data_sources/WorldCopernicusDem30HeightDataSource.h"
#include "geolib/data_sources/WorldCopernicusDem30TileDownloader.h"

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

#include <memory>
#include <string>

namespace {

/// Credentials for the Copernicus DEM download endpoint (if it requires
/// authentication for the mirror in use). Leave empty to send anonymous
/// requests.
constexpr const char* kCopernicusUserName = "";
constexpr const char* kCopernicusPassword = "";

/// Fetches `url` into the local file `targetPath` using Qt Network. Runs a
/// nested event loop, so this must be called from the GUI thread.
bool fetchUrlQt(const std::string& url, const std::string& targetPath,
                const std::string& userName, const std::string& password)
{
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

std::string cacheDirectoryFor(const std::string& subDirectory)
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
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

} // namespace

void registerHeightDataSources()
{
    geo::HeightDataSourceRegistry& registry = geo::HeightDataSourceRegistry::instance();
    for (const auto& source : registry.sources()) {
        if (source->name().find("DGM1") != std::string::npos ||
            source->name().find("Copernicus") != std::string::npos) {
            // Already registered.
            return;
        }
    }

    static geo::BavariaDgm1TileDownloader::Config bavariaConfig = [] {
        geo::BavariaDgm1TileDownloader::Config config;
        config.cacheDirectory = cacheDirectoryFor("height_data_cache/bavaria_dgm1");
        config.baseUrl = "https://download1.bayernwolke.de/a/dgm/dgm1xyz";
        config.fileExtension = ".zip";
        config.allowDownload = true;
        return config;
    }();

    static geo::BavariaDgm1TileDownloader bavariaDownloader(
        bavariaConfig, [](const std::string& url, const std::string& targetPath) {
            return fetchUrlQt(url, targetPath, {}, {});
        });

    static geo::WorldCopernicusDem30TileDownloader::Config worldConfig = [] {
        geo::WorldCopernicusDem30TileDownloader::Config config;
        config.cacheDirectory = cacheDirectoryFor("height_data_cache/world_copernicus_dem30");
        config.baseUrl = "https://copernicus-dem-30m.s3.amazonaws.com";
        config.fileExtension = ".hgt";
        config.allowDownload = true;
        return config;
    }();

    static geo::WorldCopernicusDem30TileDownloader worldDownloader(
        worldConfig, [](const std::string& url, const std::string& targetPath) {
            return fetchUrlQt(url, targetPath, kCopernicusUserName, kCopernicusPassword);
        });

    registry.addSource(
        std::make_shared<geo::BavariaDgm1HeightDataSource>(bavariaDownloader.tileLoader()));
    registry.addSource(std::make_shared<geo::WorldCopernicusDem30HeightDataSource>(
        worldDownloader.tileLoader()));
}
