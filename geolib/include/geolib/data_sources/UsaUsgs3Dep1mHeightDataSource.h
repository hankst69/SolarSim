#pragma once

#include "geolib/HeightDataSource.h"
#include "geolib/data_sources/UtmGridTile.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace geo {

/// Height data source for the open data set "USGS 3DEP 1 meter DEM" of the
/// U.S. Geological Survey (public domain, distributed via The National Map).
///
/// The data is distributed as 10 km x 10 km tiles in the UTM zone of the
/// project area (NAD83, EPSG:269xx). Downloading and decoding of a tile is done
/// by UsaUsgs3Dep1mTileDownloader and UsaUsgs3Dep1mTileReader; this class only
/// takes a loader callback which turns a tile key into a raster. Loaded tiles
/// are cached, including negative results (3DEP 1 m coverage has gaps).
class UsaUsgs3Dep1mHeightDataSource : public HeightDataSource {
public:
    /// Size of a tile in metres; 3DEP publishes the 1 m DEM in 10 km blocks.
    static constexpr double kTileSizeM = 10000.0;

    /// Key of a 3DEP 1 m tile, identified the way the official file names do:
    /// the UTM zone plus the easting/northing of the south west corner in
    /// kilometres (e.g. zone 16, x = 54 km, y = 4400 km -> "16_x54y4400").
    struct TileKey {
        /// Northern hemisphere UTM zone of the tile (1..60).
        int zone{0};
        /// UTM easting/northing of the south west corner of the tile in metres.
        int eastingM{0};
        int northingM{0};

        bool operator<(const TileKey& o) const
        {
            if (zone != o.zone) {
                return zone < o.zone;
            }
            return eastingM != o.eastingM ? eastingM < o.eastingM : northingM < o.northingM;
        }

        /// Tile name fragment, e.g. "16_x54y4400". Empty for an invalid zone.
        std::string toString() const;
    };

    /// Loads a single tile. Returns nullptr if the tile is not available.
    using TileLoader = std::function<std::shared_ptr<UtmGridTile>(const TileKey&)>;

    explicit UsaUsgs3Dep1mHeightDataSource(TileLoader loader);

    std::string name() const override
    {
        return "USGS 3DEP 1m DEM, USA";
    }

    /// Bounding box of the conterminous United States, Alaska and Hawaii.
    /// Coverage inside this box is not complete; missing tiles are reported by
    /// sampleHeight().
    GeoBounds coverage() const override { return {18.00, 72.00, -172.00, -66.00}; }

    double resolutionM() const override { return 1.0; }

    bool sampleHeight(double latitudeDeg, double longitudeDeg,
                      double& heightM) const override;

    /// Tile containing the given location.
    static TileKey tileKeyFor(double latitudeDeg, double longitudeDeg);

    /// Projection of geodetic coordinates into the UTM zone of the location.
    static void toUtm(double latitudeDeg, double longitudeDeg, int& zone, double& eastingM,
                      double& northingM);

private:
    std::shared_ptr<UtmGridTile> tile(const TileKey& key) const;

    TileLoader m_loader;
    mutable std::map<TileKey, std::shared_ptr<UtmGridTile>> m_tiles;
};

} // namespace geo
