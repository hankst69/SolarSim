#include "geolib/data_sources/UkEaLidarTileDownloader.h"

#include "geolib/data_sources/UkEaLidarTileReader.h"

#include <fstream>
#include <memory>
#include <string>
#include <utility>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace geo {
namespace {

void setError(std::string* error, const std::string& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

bool fileExists(const std::string& path)
{
    std::ifstream stream(path);
    return static_cast<bool>(stream);
}

void createDirectory(const std::string& path)
{
    if (path.empty()) {
        return;
    }
#if defined(_WIN32)
    _mkdir(path.c_str());
#else
    ::mkdir(path.c_str(), 0755);
#endif
}

std::string joinPath(const std::string& directory, const std::string& fileName)
{
    if (directory.empty()) {
        return fileName;
    }
    const char last = directory.back();
    if (last == '/' || last == '\\') {
        return directory + fileName;
    }
    return directory + "/" + fileName;
}

} // namespace

UkEaLidarTileDownloader::UkEaLidarTileDownloader(Config config, FetchFunction fetch)
    : m_config(std::move(config)), m_fetch(std::move(fetch))
{
}

std::string UkEaLidarTileDownloader::fileNameFor(const TileKey& key) const
{
    const std::string tileName = key.toString();
    if (tileName.empty()) {
        return std::string();
    }
    return m_config.fileNamePrefix + tileName + m_config.fileExtension;
}

std::string UkEaLidarTileDownloader::urlFor(const TileKey& key) const
{
    const std::string fileName = fileNameFor(key);
    if (fileName.empty()) {
        return std::string();
    }
    std::string base = m_config.baseUrl;
    if (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    return base + "/" + fileName;
}

std::string UkEaLidarTileDownloader::cachePathFor(const TileKey& key) const
{
    const std::string fileName = fileNameFor(key);
    if (fileName.empty()) {
        return std::string();
    }
    return joinPath(m_config.cacheDirectory, fileName);
}

bool UkEaLidarTileDownloader::ensureLocalFile(const TileKey& key, std::string& localPath,
                                              std::string* error) const
{
    localPath = cachePathFor(key);
    if (localPath.empty()) {
        setError(error, "tile is outside the British National Grid");
        return false;
    }
    if (fileExists(localPath)) {
        return true;
    }
    if (!m_config.allowDownload || !m_fetch) {
        setError(error, "tile " + key.toString() + " not cached and downloading is disabled");
        return false;
    }

    createDirectory(m_config.cacheDirectory);
    if (!m_fetch(urlFor(key), localPath)) {
        setError(error, "download failed for tile " + key.toString());
        return false;
    }
    if (!fileExists(localPath)) {
        setError(error, "download reported success but file is missing: " + localPath);
        return false;
    }
    return true;
}

bool UkEaLidarTileDownloader::loadTile(const TileKey& key, BngGridTile& tile,
                                       std::string* error) const
{
    std::string localPath;
    if (!ensureLocalFile(key, localPath, error)) {
        return false;
    }
    return UkEaLidarTileReader::readFile(localPath, tile, error);
}

UkEaLidarHeightDataSource::TileLoader UkEaLidarTileDownloader::tileLoader() const
{
    return [this](const TileKey& key) -> std::shared_ptr<BngGridTile> {
        auto tile = std::make_shared<BngGridTile>();
        if (!loadTile(key, *tile)) {
            return nullptr;
        }
        return tile;
    };
}

} // namespace geo
