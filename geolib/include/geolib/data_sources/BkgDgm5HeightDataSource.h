#pragma once

#include "geolib/HeightDataSource.h"
#include "geolib/data_sources/Utm32GridTile.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace geo {

/// Height data source for the nation wide German open data set
/// "Digitales Geländemodell Gitterweite 5 m (DGM5)" published by the
/// Bundesamt für Kartographie und Geodäsie (BKG) under dl-de/by-2-0.
///
/// The data is distributed as 1 km x 1 km tiles in UTM zone 32N (EPSG:25832)
/// with a 5 m grid spacing. Downloading and decoding of a tile is done by
/// BkgDgm5TileDownloader and BkgDgm5TileReader; this class only takes a loader
/// callback which turns a tile key into a raster. Loaded tiles are cached,
/// including negative results.
///
/// Compared to the state data sets (e.g. BavariaDgm1HeightDataSource) this
/// source is coarser but covers all of Germany, so the registry picks it up
/// wherever no 1 m model is registered.
class BkgDgm5HeightDataSource : public HeightDataSource {
public:
    /// Key of a DGM5 tile: UTM32 easting/northing of the south west corner in
    /// full kilometres, as used in the official file names
    /// (e.g. "690_5334" -> 690 km east, 5334 km north).
    struct TileKey {
        int eastKm{0};
        int northKm{0};

        bool operator<(const TileKey& o) const
        {
            return eastKm != o.eastKm ? eastKm < o.eastKm : northKm < o.northKm;
        }

        std::string toString() const;
    };

    /// Loads a single tile. Returns nullptr if the tile is not available.
    using TileLoader = std::function<std::shared_ptr<Utm32GridTile>(const TileKey&)>;

    explicit BkgDgm5HeightDataSource(TileLoader loader);

    std::string name() const override
    {
        return "Digitales Gelaendemodell Gitterweite 5m (DGM5), Germany/BKG";
    }

    /// Bounding box of Germany (slightly enlarged).
    GeoBounds coverage() const override { return {47.10, 55.15, 5.80, 15.10}; }

    double resolutionM() const override { return 5.0; }

    bool sampleHeight(double latitudeDeg, double longitudeDeg,
                      double& heightM) const override;

    /// Tile containing the given location.
    static TileKey tileKeyFor(double latitudeDeg, double longitudeDeg);

    /// Projection of geodetic coordinates to UTM zone 32N (EPSG:25832).
    static void toUtm32(double latitudeDeg, double longitudeDeg, double& eastingM,
                        double& northingM);

private:
    std::shared_ptr<Utm32GridTile> tile(const TileKey& key) const;

    TileLoader m_loader;
    mutable std::map<TileKey, std::shared_ptr<Utm32GridTile>> m_tiles;
};

} // namespace geo
