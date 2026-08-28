#include "geolib/GeoLocation.h"

#include "geolib/Angle.h"
#include "geolib/GroundPlane.h"

namespace geo {

GeoLocation::GeoLocation(double latitudeDeg, double longitudeDeg, double altitudeM,
                         const EarthModel& model)
    : m_latitudeDeg(latitudeDeg)
    , m_longitudeDeg(longitudeDeg)
    , m_altitudeM(altitudeM)
    , m_model(&model)
{
}

Vector3 GeoLocation::toEcef() const
{
    return m_model->toEcef(m_latitudeDeg, m_longitudeDeg, m_altitudeM);
}

Vector3 GeoLocation::up() const
{
    const double lat = degToRad(m_latitudeDeg);
    const double lon = degToRad(m_longitudeDeg);
    return {std::cos(lat) * std::cos(lon), std::cos(lat) * std::sin(lon), std::sin(lat)};
}

Vector3 GeoLocation::east() const
{
    const double lon = degToRad(m_longitudeDeg);
    return {-std::sin(lon), std::cos(lon), 0.0};
}

Vector3 GeoLocation::north() const
{
    return up().cross(east());
}

GroundPlane GeoLocation::groundPlane() const
{
    return GroundPlane(*this);
}

double GeoLocation::distanceTo(const GeoLocation& other) const
{
    const double lat1 = degToRad(m_latitudeDeg);
    const double lat2 = degToRad(other.m_latitudeDeg);
    const double dLat = lat2 - lat1;
    const double dLon = degToRad(other.m_longitudeDeg - m_longitudeDeg);

    const double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0)
                     + std::cos(lat1) * std::cos(lat2) * std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return m_model->localRadius(0.5 * (m_latitudeDeg + other.m_latitudeDeg)) * c;
}

} // namespace geo
