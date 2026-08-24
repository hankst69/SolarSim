#pragma once

#include "geolib/data_sources/CopernicusDem30HeightDataSource.h"

#include <functional>
#include <string>

namespace geo {

/// Locates, downloads and caches the tile files of the global Copernicus DEM
/// GLO-30 data set (ESA/Airbus open data).
///
/// geolib intentionally has no networking dependency, so the actual HTTP GET is
/// injected as a callback (`FetchFunction`). The application can implement it
/// with libcurl, Qt Network, WinHTTP, ... or leave it unset to work purely on
/// an already downloaded local tile directory.
///
/// Tiles are named after the south west corner of their 1 deg square, e.g.
/// 48 N / 11 E -> `Copernicus_DSM_COG_10_N48_00_E011_00_DEM.hgt`.
class CopernicusDem30TileDownloader {
public:
    using TileKey = CopernicusDem30HeightDataSource::TileKey;

    /// Downloads `url` into the local file `targetPath`. Returns false if the
    /// tile could not be retrieved (404, no network, ...).
    using FetchFunction = std::function<bool(const std::string& url, const std::string& targetPath)>;

    struct Config {
        /// Directory holding already downloaded / manually placed tiles. Also
        /// used as the download cache.
        std::string cacheDirectory{"copernicus_dem30_cache"};
        /// Base URL of the open data tile server (without trailing slash).
        std::string baseUrl{"https://copernicus-dem-30m.s3.amazonaws.com"};
        /// File extension of the tiles to request.
        std::string fileExtension{".hgt"};
        /// Allow downloading missing tiles. If false only the cache is used.
        bool allowDownload{true};
    };

    explicit CopernicusDem30TileDownloader(Config config = {}, FetchFunction fetch = {});

    const Config& config() const { return m_config; }

    /// Base name of a tile, e.g. "Copernicus_DSM_COG_10_N48_00_E011_00_DEM".
    std::string tileNameFor(const TileKey& key) const;

    /// Official file name of a tile, e.g.
    /// "Copernicus_DSM_COG_10_N48_00_E011_00_DEM.hgt".
    std::string fileNameFor(const TileKey& key) const;

    /// Full download URL of a tile. The tiles are stored in a directory named
    /// after the tile itself.
    std::string urlFor(const TileKey& key) const;

    /// Path of the tile inside the local cache directory.
    std::string cachePathFor(const TileKey& key) const;

    /// Ensures the tile file exists locally, downloading it if necessary.
    /// Returns false if it is neither cached nor downloadable.
    bool ensureLocalFile(const TileKey& key, std::string& localPath,
                         std::string* error = nullptr) const;

    /// Loads and parses a tile. Returns nullptr if unavailable.
    std::shared_ptr<GridHeightDataSource> loadTile(const TileKey& key,
                                                   std::string* error = nullptr) const;

    /// Tile loader callback that can be handed to
    /// CopernicusDem30HeightDataSource. The returned loader keeps a reference to
    /// this downloader, which must therefore outlive the data source.
    CopernicusDem30HeightDataSource::TileLoader tileLoader() const;

private:
    Config m_config;
    FetchFunction m_fetch;
};

} // namespace geo
