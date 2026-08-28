#pragma once

#include "geolib/HeightDataSource.h"
#include "geolib/data_sources/BngGridTile.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace geo {

/// Height data source for the open data set "LIDAR Composite DTM 1 m" of the
/// Environment Agency (England), published via the DEFRA Survey Data Download
/// service under the Open Government Licence.
///
/// The data is distributed as 5 km x 5 km tiles on the British National Grid
/// (OSGB36, EPSG:27700). Downloading and decoding of a tile is done by
/// UkEaLidarTileDownloader and UkEaLidarTileReader; this class only takes a
/// loader callback which turns a tile key into a raster. Loaded tiles are
/// cached, including negative results (the composite has gaps).
class UkEaLidarHeightDataSource : public HeightDataSource {
public:
    /// Size of a tile in metres; the EA publishes the composite in 5 km blocks.
    static constexpr double kTileSizeM = 5000.0;

    /// Key of a LIDAR tile, identified the way the official file names do: the
    /// two letter 100 km square, the 10 km cell inside it and the quadrant
    /// letter of the 5 km block (e.g. "sp50ne").
    struct TileKey {
        /// BNG easting/northing of the south west corner of the tile in metres.
        int eastingM{0};
        int northingM{0};

        bool operator<(const TileKey& o) const
        {
            return eastingM != o.eastingM ? eastingM < o.eastingM : northingM < o.northingM;
        }

        /// Official tile name, e.g. "sp50ne". Empty outside the grid.
        std::string toString() const;
    };

    /// Loads a single tile. Returns nullptr if the tile is not available.
    using TileLoader = std::function<std::shared_ptr<BngGridTile>(const TileKey&)>;

    explicit UkEaLidarHeightDataSource(TileLoader loader);

    std::string name() const override
    {
        return "LIDAR Composite DTM 1m, England/Environment Agency";
    }

    /// Bounding box of England and Wales (slightly enlarged). Coverage inside
    /// this box is not complete; missing tiles are reported by sampleHeight().
    GeoBounds coverage() const override { return {49.80, 56.00, -6.50, 2.00}; }

    double resolutionM() const override { return 1.0; }

    bool sampleHeight(double latitudeDeg, double longitudeDeg,
                      double& heightM) const override;

    /// Tile containing the given location.
    static TileKey tileKeyFor(double latitudeDeg, double longitudeDeg);

    /// Projection of geodetic coordinates to the British National Grid.
    static void toBng(double latitudeDeg, double longitudeDeg, double& eastingM,
                      double& northingM);

private:
    std::shared_ptr<BngGridTile> tile(const TileKey& key) const;

    TileLoader m_loader;
    mutable std::map<TileKey, std::shared_ptr<BngGridTile>> m_tiles;
};

} // namespace geo
