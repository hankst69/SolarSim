#include "geolib/GroundPlane.h"

#include "geolib/Angle.h"

namespace geo {

GroundPlane::GroundPlane(const GeoLocation& origin)
    : m_origin(origin)
    , m_originEcef(origin.toEcef())
    , m_east(origin.east())
    , m_north(origin.north())
    , m_up(origin.up())
    , m_meridionalRadius(origin.earthModel().meridionalRadius(origin.latitude()))
    , m_primeVerticalRadius(origin.earthModel().primeVerticalRadius(origin.latitude()))
{
}

Vector3 GroundPlane::toLocal(const Vector3& ecef) const
{
    const Vector3 d = ecef - m_originEcef;
    return {d.dot(m_east), d.dot(m_north), d.dot(m_up)};
}

Vector3 GroundPlane::toEcef(const Vector3& local) const
{
    return m_originEcef + m_east * local.x + m_north * local.y + m_up * local.z;
}

double GroundPlane::heightAbove(const Vector3& ecef) const
{
    return (ecef - m_originEcef).dot(m_up);
}

Vector3 GroundPlane::toLocal(const GeoLocation& location) const
{
    return toLocal(location.toEcef());
}

double GroundPlane::curvatureDrop(double eastM, double northM) const
{
    // Second order approximation of the ellipsoid surface in the local frame.
    return 0.5 * (eastM * eastM / m_primeVerticalRadius + northM * northM / m_meridionalRadius);
}

GeoLocation GroundPlane::toGeoLocation(const Vector3& local) const
{
    const double latDeg = m_origin.latitude()
                          + radToDeg(local.y / (m_meridionalRadius + m_origin.altitude()));
    const double cosLat = std::cos(degToRad(m_origin.latitude()));
    const double lonDeg =
        m_origin.longitude()
        + radToDeg(local.x / ((m_primeVerticalRadius + m_origin.altitude()) * cosLat));
    const double altM = m_origin.altitude() + local.z - curvatureDrop(local.x, local.y);

    return GeoLocation(latDeg, lonDeg, altM, m_origin.earthModel());
}

} // namespace geo
