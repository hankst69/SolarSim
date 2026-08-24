#pragma once

#include "geolib/data_sources/BkgDgm5HeightDataSource.h"
#include "geolib/data_sources/Utm32GridTile.h"

#include <functional>
#include <string>

namespace geo {

/// Locates, downloads and caches the tile files of the German open data set
/// "Digitales Geländemodell Gitterweite 5 m (DGM5)" (BKG, gdz.bkg.bund.de).
///
/// geolib intentionally has no networking dependency, so the actual HTTP GET is
/// injected as a callback (`FetchFunction`). The application can implement it
/// with libcurl, Qt Network, WinHTTP, ... or leave it unset to work purely on
/// an already downloaded local tile directory.
///
/// Tiles are named after the south west corner of their 1 km square in UTM32,
/// e.g. `690_5334` -> `dgm5_32_690_5334_2.xyz`.
class BkgDgm5TileDownloader {
public:
    using TileKey = BkgDgm5HeightDataSource::TileKey;

    /// Downloads `url` into the local file `targetPath`. Returns false if the
    /// tile could not be retrieved (404, no network, ...).
    using FetchFunction = std::function<bool(const std::string& url, const std::string& targetPath)>;

    struct Config {
        /// Directory holding already downloaded / manually placed tiles. Also
        /// used as the download cache.
        std::string cacheDirectory{"dgm5_cache"};
        /// Base URL of the open data tile server (without trailing slash).
        std::string baseUrl{"https://daten.gdz.bkg.bund.de/produkte/dgm/dgm5"};
        /// File extension of the tiles to request.
        std::string fileExtension{".xyz"};
        /// Allow downloading missing tiles. If false only the cache is used.
        bool allowDownload{true};
    };

    explicit BkgDgm5TileDownloader(Config config = {}, FetchFunction fetch = {});

    const Config& config() const { return m_config; }

    /// Official file name of a tile, e.g. "dgm5_32_690_5334_2.xyz".
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
    bool loadTile(const TileKey& key, Utm32GridTile& tile, std::string* error = nullptr) const;

    /// Tile loader callback that can be handed to BkgDgm5HeightDataSource.
    /// The returned loader keeps a reference to this downloader, which must
    /// therefore outlive the data source.
    BkgDgm5HeightDataSource::TileLoader tileLoader() const;

private:
    Config m_config;
    FetchFunction m_fetch;
};

} // namespace geo
