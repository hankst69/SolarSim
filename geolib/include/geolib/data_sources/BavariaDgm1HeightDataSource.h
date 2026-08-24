#pragma once

#include "geolib/GridHeightDataSource.h"
#include "geolib/HeightDataSource.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace geo {

/// Height data source for the Bavarian open data set
/// "openData Digitales Gelaendemodell 1m (DGM1)" published by the Landesamt
/// fuer Digitalisierung, Breitband und Vermessung (LDBV).
///
/// The data is distributed as 1 km x 1 km tiles in UTM zone 32N (EPSG:25832).
/// Decoding/downloading of a tile is not part of geolib; the application
/// supplies a loader callback which turns a tile key into an in-memory raster
/// (see GridHeightDataSource). Loaded tiles are cached.
class BavariaDgm1HeightDataSource : public HeightDataSource {
public:
    /// Key of a DGM1 tile: UTM32 easting/northing of the south west corner in
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
    using TileLoader = std::function<std::shared_ptr<GridHeightDataSource>(const TileKey&)>;

    explicit BavariaDgm1HeightDataSource(TileLoader loader);

    std::string name() const override
    {
        return "openData Digitales Gelaendemodell 1m (DGM1), Bavaria/LDBV";
    }

    /// Bounding box of Bavaria (slightly enlarged).
    GeoBounds coverage() const override { return {47.20, 50.60, 8.90, 13.90}; }

    double resolutionM() const override { return 1.0; }

    bool sampleHeight(double latitudeDeg, double longitudeDeg,
                      double& heightM) const override;

    /// Tile containing the given location.
    static TileKey tileKeyFor(double latitudeDeg, double longitudeDeg);

    /// Projection of geodetic coordinates to UTM zone 32N (EPSG:25832).
    static void toUtm32(double latitudeDeg, double longitudeDeg, double& eastingM,
                        double& northingM);

private:
    std::shared_ptr<GridHeightDataSource> tile(const TileKey& key) const;

    TileLoader m_loader;
    mutable std::map<TileKey, std::shared_ptr<GridHeightDataSource>> m_tiles;
};

} // namespace geo
