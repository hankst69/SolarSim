#include "geolib/data_sources/BkgDgm5TileDownloader.h"

#include "geolib/data_sources/BkgDgm5TileReader.h"

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

BkgDgm5TileDownloader::BkgDgm5TileDownloader(Config config, FetchFunction fetch)
    : m_config(std::move(config)), m_fetch(std::move(fetch))
{
}

std::string BkgDgm5TileDownloader::fileNameFor(const TileKey& key) const
{
    return "dgm5_32_" + std::to_string(key.eastKm) + "_" + std::to_string(key.northKm) + "_2" +
           m_config.fileExtension;
}

std::string BkgDgm5TileDownloader::urlFor(const TileKey& key) const
{
    std::string base = m_config.baseUrl;
    if (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    return base + "/" + fileNameFor(key);
}

std::string BkgDgm5TileDownloader::cachePathFor(const TileKey& key) const
{
    return joinPath(m_config.cacheDirectory, fileNameFor(key));
}

bool BkgDgm5TileDownloader::ensureLocalFile(const TileKey& key, std::string& localPath,
                                            std::string* error) const
{
    localPath = cachePathFor(key);
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

bool BkgDgm5TileDownloader::loadTile(const TileKey& key, Utm32GridTile& tile,
                                     std::string* error) const
{
    std::string localPath;
    if (!ensureLocalFile(key, localPath, error)) {
        return false;
    }
    return BkgDgm5TileReader::readFile(localPath, tile, error);
}

BkgDgm5HeightDataSource::TileLoader BkgDgm5TileDownloader::tileLoader() const
{
    return [this](const TileKey& key) -> std::shared_ptr<Utm32GridTile> {
        auto tile = std::make_shared<Utm32GridTile>();
        if (!loadTile(key, *tile)) {
            return nullptr;
        }
        return tile;
    };
}

} // namespace geo
