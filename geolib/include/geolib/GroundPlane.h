#pragma once

#include "geolib/GeoLocation.h"
#include "geolib/Vector3.h"

namespace geo {

/// Tangential plane on the earth surface at a given GeoLocation.
/// The plane origin is the standpoint, the local axes are east/north/up.
class GroundPlane {
public:
    GroundPlane(const GeoLocation& origin);

    const GeoLocation& origin() const { return m_origin; }

    const Vector3& originEcef() const { return m_originEcef; }
    const Vector3& normal() const { return m_up; }
    const Vector3& east() const { return m_east; }
    const Vector3& north() const { return m_north; }

    /// Project an ECEF point into the local east/north/up frame of this plane.
    Vector3 toLocal(const Vector3& ecef) const;

    /// Convert a local east/north/up coordinate back to ECEF.
    Vector3 toEcef(const Vector3& local) const;

    /// Signed distance of an ECEF point above (positive) or below the plane.
    double heightAbove(const Vector3& ecef) const;

    /// Local east/north/up coordinates of the given location.
    Vector3 toLocal(const GeoLocation& location) const;

private:
    GeoLocation m_origin;
    Vector3 m_originEcef;
    Vector3 m_east;
    Vector3 m_north;
    Vector3 m_up;
};

} // namespace geo
