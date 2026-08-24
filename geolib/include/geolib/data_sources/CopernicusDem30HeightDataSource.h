#pragma once

#include "geolib/GridHeightDataSource.h"
#include "geolib/HeightDataSource.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace geo {

/// Height data source for the global Copernicus DEM GLO-30 data set
/// (ESA/Airbus, distributed as open data, ~30 m ground sample distance).
///
/// The data is distributed as 1 deg x 1 deg tiles on a geographic grid
/// (EPSG:4326), named after the south west corner of the tile. Downloading and
/// decoding of a tile is done by CopernicusDem30TileDownloader and
/// CopernicusDem30TileReader; this class only takes a loader callback which
/// turns a tile key into a raster. Loaded tiles are cached, including negative
/// results (ocean tiles do not exist at all).
class CopernicusDem30HeightDataSource : public HeightDataSource {
public:
    /// Key of a GLO-30 tile: latitude/longitude of the south west corner in
    /// full degrees, as used in the official file names
    /// (e.g. 48/11 -> "N48_00_E011_00").
    struct TileKey {
        int latDeg{0};
        int lonDeg{0};

        bool operator<(const TileKey& o) const
        {
            return latDeg != o.latDeg ? latDeg < o.latDeg : lonDeg < o.lonDeg;
        }

        /// Tile name fragment, e.g. "N48_00_E011_00".
        std::string toString() const;
    };

    /// Loads a single tile. Returns nullptr if the tile is not available.
    using TileLoader = std::function<std::shared_ptr<GridHeightDataSource>(const TileKey&)>;

    explicit CopernicusDem30HeightDataSource(TileLoader loader);

    std::string name() const override
    {
        return "Copernicus DEM GLO-30 (ESA/Airbus), World";
    }

    /// GLO-30 is a global data set; tiles simply do not exist over open water.
    GeoBounds coverage() const override { return {-90.0, 90.0, -180.0, 180.0}; }

    double resolutionM() const override { return 30.0; }

    bool sampleHeight(double latitudeDeg, double longitudeDeg,
                      double& heightM) const override;

    /// Tile containing the given location.
    static TileKey tileKeyFor(double latitudeDeg, double longitudeDeg);

    /// Geographic bounds of a tile (1 deg x 1 deg).
    static GeoBounds boundsFor(const TileKey& key);

private:
    std::shared_ptr<GridHeightDataSource> tile(const TileKey& key) const;

    TileLoader m_loader;
    mutable std::map<TileKey, std::shared_ptr<GridHeightDataSource>> m_tiles;
};

} // namespace geo
