#include "geolib/data_sources/UsaUsgs3Dep1mTileDownloader.h"

#include "geolib/data_sources/UsaUsgs3Dep1mTileReader.h"

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

UsaUsgs3Dep1mTileDownloader::UsaUsgs3Dep1mTileDownloader(Config config, FetchFunction fetch)
    : m_config(std::move(config)), m_fetch(std::move(fetch))
{
}

std::string UsaUsgs3Dep1mTileDownloader::fileNameFor(const TileKey& key) const
{
    const std::string tileName = key.toString();
    if (tileName.empty()) {
        return std::string();
    }
    return m_config.fileNamePrefix + tileName + m_config.fileExtension;
}

std::string UsaUsgs3Dep1mTileDownloader::urlFor(const TileKey& key) const
{
    const std::string fileName = fileNameFor(key);
    if (fileName.empty()) {
        return std::string();
    }
    std::string base = m_config.baseUrl;
    if (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    return base + "/" + std::to_string(key.zone) + "/TIFF/" + fileName;
}

std::string UsaUsgs3Dep1mTileDownloader::cachePathFor(const TileKey& key) const
{
    const std::string fileName = fileNameFor(key);
    if (fileName.empty()) {
        return std::string();
    }
    return joinPath(m_config.cacheDirectory, fileName);
}

bool UsaUsgs3Dep1mTileDownloader::ensureLocalFile(const TileKey& key, std::string& localPath,
                                                  std::string* error) const
{
    localPath = cachePathFor(key);
    if (localPath.empty()) {
        setError(error, "tile has no valid UTM zone");
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

bool UsaUsgs3Dep1mTileDownloader::loadTile(const TileKey& key, UtmGridTile& tile,
                                           std::string* error) const
{
    std::string localPath;
    if (!ensureLocalFile(key, localPath, error)) {
        return false;
    }
    return UsaUsgs3Dep1mTileReader::readFile(localPath, key.zone, tile, error);
}

UsaUsgs3Dep1mHeightDataSource::TileLoader UsaUsgs3Dep1mTileDownloader::tileLoader() const
{
    return [this](const TileKey& key) -> std::shared_ptr<UtmGridTile> {
        auto tile = std::make_shared<UtmGridTile>();
        if (!loadTile(key, *tile)) {
            return nullptr;
        }
        return tile;
    };
}

} // namespace geo
