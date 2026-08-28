#pragma once

#include "geolib/data_sources/UsaUsgs3Dep1mHeightDataSource.h"
#include "geolib/data_sources/UtmGridTile.h"

#include <functional>
#include <string>

namespace geo {

/// Locates, downloads and caches the tile files of the USGS open data set
/// "3DEP 1 meter DEM" (public domain, prd-tnm.s3.amazonaws.com).
///
/// geolib intentionally has no networking dependency, so the actual HTTP GET is
/// injected as a callback (`FetchFunction`). The application can implement it
/// with libcurl, Qt Network, WinHTTP, ... or leave it unset to work purely on
/// an already downloaded local tile directory.
///
/// Tiles are named after their 10 km block in the UTM zone of the project, e.g.
/// zone 16, x = 54 km, y = 4400 km -> `USGS_1M_16_x54y4400.asc`.
class UsaUsgs3Dep1mTileDownloader {
public:
    using TileKey = UsaUsgs3Dep1mHeightDataSource::TileKey;

    /// Downloads `url` into the local file `targetPath`. Returns false if the
    /// tile could not be retrieved (404, no network, ...).
    using FetchFunction = std::function<bool(const std::string& url, const std::string& targetPath)>;

    struct Config {
        /// Directory holding already downloaded / manually placed tiles. Also
        /// used as the download cache.
        std::string cacheDirectory{"usa_usgs_3dep_1m_cache"};
        /// Base URL of the open data tile server (without trailing slash).
        std::string baseUrl{"https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/1m"};
        /// Prefix of the tile file names, including the product generation the
        /// application wants to use.
        std::string fileNamePrefix{"USGS_1M_"};
        /// File extension of the tiles to request.
        std::string fileExtension{".asc"};
        /// Allow downloading missing tiles. If false only the cache is used.
        bool allowDownload{true};
    };

    explicit UsaUsgs3Dep1mTileDownloader(Config config = {}, FetchFunction fetch = {});

    const Config& config() const { return m_config; }

    /// Official file name of a tile, e.g. "USGS_1M_16_x54y4400.asc".
    /// Empty for tile keys with an invalid UTM zone.
    std::string fileNameFor(const TileKey& key) const;

    /// Full download URL of a tile. The tiles are stored in a project
    /// directory named after the UTM zone.
    std::string urlFor(const TileKey& key) const;

    /// Path of the tile inside the local cache directory.
    std::string cachePathFor(const TileKey& key) const;

    /// Ensures the tile file exists locally, downloading it if necessary.
    /// Returns false if it is neither cached nor downloadable.
    bool ensureLocalFile(const TileKey& key, std::string& localPath,
                         std::string* error = nullptr) const;

    /// Loads and parses a tile. Returns false if unavailable.
    bool loadTile(const TileKey& key, UtmGridTile& tile, std::string* error = nullptr) const;

    /// Tile loader callback that can be handed to
    /// UsaUsgs3Dep1mHeightDataSource. The returned loader keeps a reference to
    /// this downloader, which must therefore outlive the data source.
    UsaUsgs3Dep1mHeightDataSource::TileLoader tileLoader() const;

private:
    Config m_config;
    FetchFunction m_fetch;
};

} // namespace geo
