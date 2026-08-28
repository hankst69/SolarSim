#pragma once

#include "geolib/EarthModel.h"
#include "geolib/Vector3.h"

namespace geo {

class GroundPlane;

/// A point on earth given by GPS coordinates (WGS84 style latitude/longitude in
/// degrees and altitude in metres above the reference surface).
class GeoLocation {
public:
    GeoLocation() = default;
    GeoLocation(double latitudeDeg, double longitudeDeg, double altitudeM = 0.0,
                const EarthModel& model = EarthModel::defaultModel());

    double latitude() const { return m_latitudeDeg; }
    double longitude() const { return m_longitudeDeg; }
    double altitude() const { return m_altitudeM; }

    const EarthModel& earthModel() const { return *m_model; }

    /// Cartesian earth centred earth fixed position of this location.
    Vector3 toEcef() const;

    /// Local unit vectors of the east/north/up frame at this location.
    Vector3 up() const;
    Vector3 east() const;
    Vector3 north() const;

    /// Tangential plane touching the earth surface below this location.
    GroundPlane groundPlane() const;

    /// Great circle distance along the earth surface to another location.
    double distanceTo(const GeoLocation& other) const;

private:
    double m_latitudeDeg{0.0};
    double m_longitudeDeg{0.0};
    double m_altitudeM{0.0};
    const EarthModel* m_model{&EarthModel::defaultModel()};
};

} // namespace geo
