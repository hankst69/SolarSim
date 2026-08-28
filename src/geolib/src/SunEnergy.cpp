#include "geolib/SunEnergy.h"

#include "geolib/Angle.h"

#include <algorithm>

// The irradiance model is deliberately simple and analytic:
//   * the theoretical value follows the inverse square law with the earth -
//     sun distance of the date (elliptical orbit, about +-3.3% over the year),
//   * the atmospheric damping uses the relative optical air mass (Kasten and
//     Young 1989) with a clear sky transmittance per air mass,
//   * the inclination of the receiving plane enters as the cosine of the
//     angle between the sun direction and the plane normal.

namespace geo {
namespace {

/// Relative optical air mass after Kasten/Young for the given true elevation
/// in degrees. Returns 0 for a sun below the horizon.
double opticalAirMass(double elevationDeg)
{
    if (elevationDeg <= 0.0) {
        return 0.0;
    }
    const double h = std::min(elevationDeg, 90.0);
    return 1.0 / (std::sin(degToRad(h)) + 0.50572 * std::pow(h + 6.07995, -1.6364));
}

} // namespace

SunEnergy::SunEnergy(const DateTimeUtc& utc)
    : m_utc(utc)
    , m_location(GeoLocation(0.0, 0.0))
    , m_sun(GeoLocation(0.0, 0.0), utc)
    , m_hasLocation(false)
{
    computeDistance();
}

SunEnergy::SunEnergy(const GeoLocation& location, const DateTimeUtc& utc)
    : m_utc(utc)
    , m_location(location)
    , m_sun(location, utc)
    , m_hasLocation(true)
{
    computeDistance();
    computeAtmosphere();
}

void SunEnergy::computeDistance()
{
    m_sunDistanceAu = m_sun.sunDistanceAu();
    m_theoreticalIrradiance = kSolarConstant / (m_sunDistanceAu * m_sunDistanceAu);
}

void SunEnergy::computeAtmosphere()
{
    // The refracted elevation describes the apparent path of the light through
    // the atmosphere, which is what the air mass model expects.
    m_airMass = opticalAirMass(m_sun.refractedElevation());
    m_transmittance = m_airMass > 0.0 ? std::pow(kZenithTransmittance, m_airMass) : 0.0;
}

double SunEnergy::directNormalIrradiance() const
{
    if (!m_hasLocation) {
        return 0.0;
    }
    return m_theoreticalIrradiance * m_transmittance;
}

double SunEnergy::incidenceFactor(const Vector3& planeNormalEnu) const
{
    if (!m_hasLocation || !m_sun.isAboveHorizon()) {
        return 0.0;
    }
    const Vector3 normal = planeNormalEnu.normalized();
    if (normal.length() <= 0.0) {
        return 0.0;
    }
    return std::max(0.0, m_sun.direction().dot(normal));
}

double SunEnergy::groundIrradiance() const
{
    return irradianceOnPlane(Vector3(0.0, 0.0, 1.0));
}

double SunEnergy::irradianceOnPlane(const Vector3& planeNormalEnu) const
{
    return directNormalIrradiance() * incidenceFactor(planeNormalEnu);
}

double SunEnergy::irradianceOnTiltedPlane(double tiltDeg, double azimuthDeg) const
{
    const double tilt = degToRad(tiltDeg);
    const double azimuth = degToRad(azimuthDeg);
    // Azimuth is clockwise from north: east = sin, north = cos.
    const Vector3 normal(std::sin(tilt) * std::sin(azimuth), std::sin(tilt) * std::cos(azimuth),
                         std::cos(tilt));
    return irradianceOnPlane(normal);
}

double SunEnergy::irradianceOnGroundPlane(const GroundPlane& plane) const
{
    if (!m_hasLocation) {
        return 0.0;
    }
    // The plane normal is given in ECEF; express it in the local frame of this
    // location so that it can be compared with the sun direction.
    const Vector3 n = plane.normal();
    const Vector3 local(n.dot(m_location.east()), n.dot(m_location.north()), n.dot(m_location.up()));
    return irradianceOnPlane(local);
}

} // namespace geo
