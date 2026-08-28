#pragma once

#include "geolib/data_sources/BngGridTile.h"
#include "geolib/data_sources/UkEaLidarHeightDataSource.h"

#include <functional>
#include <string>

namespace geo {

/// Locates, downloads and caches the tile files of the Environment Agency open
/// data set "LIDAR Composite DTM 1 m" (England,
/// environment.data.gov.uk/survey).
///
/// geolib intentionally has no networking dependency, so the actual HTTP GET is
/// injected as a callback (`FetchFunction`). The application can implement it
/// with libcurl, Qt Network, WinHTTP, ... or leave it unset to work purely on
/// an already downloaded local tile directory.
///
/// Tiles are named after their 5 km block on the British National Grid, e.g.
/// `sp50ne` -> `LIDARCOMP-DTM-1M-sp50ne.asc`.
class UkEaLidarTileDownloader {
public:
    using TileKey = UkEaLidarHeightDataSource::TileKey;

    /// Downloads `url` into the local file `targetPath`. Returns false if the
    /// tile could not be retrieved (404, no network, ...).
    using FetchFunction = std::function<bool(const std::string& url, const std::string& targetPath)>;

    struct Config {
        /// Directory holding already downloaded / manually placed tiles. Also
        /// used as the download cache.
        std::string cacheDirectory{"uk_ea_lidar_cache"};
        /// Base URL of the open data tile server (without trailing slash).
        std::string baseUrl{"https://environment.data.gov.uk/UserDownloads/interactive/lidar/DTM/1M"};
        /// Prefix of the tile file names, including the survey year of the
        /// composite the application wants to use.
        std::string fileNamePrefix{"LIDARCOMP-DTM-1M-"};
        /// File extension of the tiles to request.
        std::string fileExtension{".asc"};
        /// Allow downloading missing tiles. If false only the cache is used.
        bool allowDownload{true};
    };

    explicit UkEaLidarTileDownloader(Config config = {}, FetchFunction fetch = {});

    const Config& config() const { return m_config; }

    /// Official file name of a tile, e.g. "LIDARCOMP-DTM-1M-sp50ne.asc".
    /// Empty for tile keys outside the National Grid.
    std::string fileNameFor(const TileKey& key) const;

    /// Full download URL of a tile.
    std::string urlFor(const TileKey& key) const;

    /// Path of the tile inside the local cache directory.
    std::string cachePathFor(const TileKey& key) const;

    /// Ensures the tile file exists locally, downloading it if necessary.
    /// Returns false if it is neither cached nor downloadable.
    bool ensureLocalFile(const TileKey& key, std::string& localPath,
                         std::string* error = nullptr) const;

    /// Loads and parses a tile. Returns false if unavailable.
    bool loadTile(const TileKey& key, BngGridTile& tile, std::string* error = nullptr) const;

    /// Tile loader callback that can be handed to UkEaLidarHeightDataSource.
    /// The returned loader keeps a reference to this downloader, which must
    /// therefore outlive the data source.
    UkEaLidarHeightDataSource::TileLoader tileLoader() const;

private:
    Config m_config;
    FetchFunction m_fetch;
};

} // namespace geo
