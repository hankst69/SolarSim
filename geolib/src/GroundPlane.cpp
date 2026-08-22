#include "geolib/GroundPlane.h"

namespace geo {

GroundPlane::GroundPlane(const GeoLocation& origin)
    : m_origin(origin)
    , m_originEcef(origin.toEcef())
    , m_east(origin.east())
    , m_north(origin.north())
    , m_up(origin.up())
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

} // namespace geo
