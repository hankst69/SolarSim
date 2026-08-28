#pragma once

#include "geolib/GeoLocation.h"

#include <memory>
#include <string>

namespace geo {

/// Axis aligned latitude/longitude bounding box (degrees) describing the area
/// covered by a height data source.
struct GeoBounds {
    double minLatitudeDeg{-90.0};
    double maxLatitudeDeg{90.0};
    double minLongitudeDeg{-180.0};
    double maxLongitudeDeg{180.0};

    static GeoBounds world() { return GeoBounds{}; }

    bool contains(double latitudeDeg, double longitudeDeg) const
    {
        return latitudeDeg >= minLatitudeDeg && latitudeDeg <= maxLatitudeDeg &&
               longitudeDeg >= minLongitudeDeg && longitudeDeg <= maxLongitudeDeg;
    }

    bool contains(const GeoLocation& location) const
    {
        return contains(location.latitude(), location.longitude());
    }
};

/// Abstract provider of terrain heights ("digital elevation/terrain model").
///
/// Implementations wrap a concrete data set, e.g. the Bavarian open data
/// "Digitales Gelaendemodell 1m (DGM1)", SRTM, Copernicus DEM, USGS 3DEP, ...
class HeightDataSource {
public:
    virtual ~HeightDataSource() = default;

    /// Human readable name of the data set, used for logging and selection.
    virtual std::string name() const = 0;

    /// Area for which this source can deliver heights.
    virtual GeoBounds coverage() const = 0;

    /// Nominal ground sample distance of the data set in metres. Smaller values
    /// are considered "better" during source selection.
    virtual double resolutionM() const = 0;

    /// Terrain height in metres above the reference surface at the given
    /// location. Returns false if no value is available (data gap, tile not
    /// downloaded, outside coverage, ...).
    virtual bool sampleHeight(double latitudeDeg, double longitudeDeg,
                              double& heightM) const = 0;

    bool sampleHeight(const GeoLocation& location, double& heightM) const
    {
        return sampleHeight(location.latitude(), location.longitude(), heightM);
    }

    /// True if the given location lies inside the coverage of this source.
    bool covers(double latitudeDeg, double longitudeDeg) const
    {
        return coverage().contains(latitudeDeg, longitudeDeg);
    }

    bool covers(const GeoLocation& location) const
    {
        return coverage().contains(location);
    }
};

using HeightDataSourcePtr = std::shared_ptr<HeightDataSource>;

} // namespace geo
