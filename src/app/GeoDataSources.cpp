#pragma once

#include "GeoDataSources.h"

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

//GeoDataSources::GeoDataSources() {}
//GeoDataSources::~GeoDataSources() = default;

void GeoDataSources::registerDataSources() 
{
    //if (!m_height_data_sources_registry)
    //{
    //  m_height_data_sources_registry = &(HeightDataSourceRegistry::instance());
    //}
    //if (!m_texture_data_sources_registry)
    //{
    //  m_texture_data_sources_registry = &(TextureDataSourceRegistry::instance());
    //}

    registerHeightDataSources();
    registerTextureDataSources();
}

void GeoDataSources::registerTextureDataSources() {}

geo::HeightDataSourceRegistry& GeoDataSources::heightDataSources()
{
    return *(m_height_data_sources_registry);
}

geo::TextureDataSourceRegistry& GeoDataSources::textureDataSources()
{
    return *(m_texture_data_sources_registry);
}


/// Fetches `url` into the local file `targetPath` using Qt Network. Runs a
/// nested event loop, so this must be called from the GUI thread.
bool GeoDataSources::fetchUrl(const std::string& url, const std::string& targetPath,
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
