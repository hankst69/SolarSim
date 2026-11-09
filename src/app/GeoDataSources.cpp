#pragma once

#include "GeoDataSources.h"
#include "MongooseFetch.h"

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

namespace app {

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
    byConfig.downloadFileExtension = ".zip";
    byConfig.dataFileExtension = ".txt";
    byConfig.allowDownload = true;

    geo::BavariaDgm1TileDownloader::FetchFunction byFetch =
        [this](const std::string& url, const std::string& targetPath) -> bool {
            m_progressCount++;
            progress(m_progressCount, "downloadinig " + targetPath);
            return app::fetchUrlMongoose(url, targetPath, {}, {});
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
            m_progressCount++;
            progress(m_progressCount, "downloadinig " + targetPath);
            return app::fetchUrlMongoose(url, targetPath, kCopernicusUserName, kCopernicusPassword);
            //return fetchUrl(url, targetPath, kCopernicusUserName, kCopernicusPassword);
    };

    m_worldDem30TileDownloader = std::make_shared<geo::WorldCopernicusDem30TileDownloader>(worldConfig, worldFetch);

    m_height_data_sources_registry->addSource(
        std::make_shared<geo::WorldCopernicusDem30HeightDataSource>(m_worldDem30TileDownloader->tileLoader())
    );
}



} //namespace