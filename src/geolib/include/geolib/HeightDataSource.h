#pragma once

#include "geolib/GeoDataSource.h"

//#include <memory>
#include <string>

namespace geo {

/// Abstract provider of terrain heights ("digital elevation/terrain model").
///
/// Implementations wrap a concrete data set, e.g. the Bavarian open data
/// "Digitales Gelaendemodell 1m (DGM1)", SRTM, Copernicus DEM, USGS 3DEP, ...
class HeightDataSource : public GeoDataSource {
public:
    virtual ~HeightDataSource() = default;

    /// Terrain height in metres above the reference surface at the given
    /// location. Returns false if no value is available (data gap, tile not
    /// downloaded, outside coverage, ...).
    virtual bool sampleHeight(double latitudeDeg, double longitudeDeg,
                              double& heightM) const
    {
        return sampleHeight(latitudeDeg, longitudeDeg, heightM);
    }

    virtual bool sampleHeight(const GeoLocation& location, double& heightM) const
    {
        return sampleHeight(location.latitude(), location.longitude(), heightM);
    }
};

using HeightDataSourcePtr = std::shared_ptr<HeightDataSource>;

} // namespace geo
