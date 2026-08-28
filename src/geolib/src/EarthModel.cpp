#include "geolib/EarthModel.h"

#include "geolib/Angle.h"

namespace geo {

const EarthModel& EarthModel::defaultModel()
{
    static const WGS84EarthModel model;
    return model;
}

const EarthModel& EarthModel::sphericalModel()
{
    static const SphericalEarthModel model;
    return model;
}

SphericalEarthModel::SphericalEarthModel(double radiusM) : m_radiusM(radiusM) {}

double SphericalEarthModel::localRadius(double /*latitudeDeg*/) const
{
    return m_radiusM;
}

double SphericalEarthModel::meridionalRadius(double /*latitudeDeg*/) const
{
    return m_radiusM;
}

double SphericalEarthModel::primeVerticalRadius(double /*latitudeDeg*/) const
{
    return m_radiusM;
}

double SphericalEarthModel::geocentricRadius(double /*latitudeDeg*/) const
{
    return m_radiusM;
}

double SphericalEarthModel::geocentricLatitude(double latitudeDeg) const
{
    return latitudeDeg;
}

Vector3 SphericalEarthModel::toEcef(double latitudeDeg, double longitudeDeg, double altitudeM) const
{
    const double lat = degToRad(latitudeDeg);
    const double lon = degToRad(longitudeDeg);
    const double r = m_radiusM + altitudeM;

    return {r * std::cos(lat) * std::cos(lon),
            r * std::cos(lat) * std::sin(lon),
            r * std::sin(lat)};
}

WGS84EarthModel::WGS84EarthModel()
    : m_a(kSemiMajorAxisM)
    , m_f(1.0 / kInverseFlattening)
    , m_b(kSemiMajorAxisM * (1.0 - 1.0 / kInverseFlattening))
    , m_e2(2.0 * (1.0 / kInverseFlattening) - (1.0 / kInverseFlattening) * (1.0 / kInverseFlattening))
{
}

double WGS84EarthModel::meridionalRadius(double latitudeDeg) const
{
    const double sinLat = std::sin(degToRad(latitudeDeg));
    const double w = 1.0 - m_e2 * sinLat * sinLat;
    return m_a * (1.0 - m_e2) / (w * std::sqrt(w));
}

double WGS84EarthModel::primeVerticalRadius(double latitudeDeg) const
{
    const double sinLat = std::sin(degToRad(latitudeDeg));
    return m_a / std::sqrt(1.0 - m_e2 * sinLat * sinLat);
}

double WGS84EarthModel::localRadius(double latitudeDeg) const
{
    // Gaussian mean radius of curvature sqrt(M * N).
    return std::sqrt(meridionalRadius(latitudeDeg) * primeVerticalRadius(latitudeDeg));
}

double WGS84EarthModel::geocentricRadius(double latitudeDeg) const
{
    const double lat = degToRad(latitudeDeg);
    const double c = std::cos(lat);
    const double s = std::sin(lat);
    const double a2c = m_a * m_a * c;
    const double b2s = m_b * m_b * s;

    return std::sqrt((a2c * a2c + b2s * b2s) / ((m_a * c) * (m_a * c) + (m_b * s) * (m_b * s)));
}

double WGS84EarthModel::geocentricLatitude(double latitudeDeg) const
{
    return radToDeg(std::atan((1.0 - m_e2) * std::tan(degToRad(latitudeDeg))));
}

Vector3 WGS84EarthModel::toEcef(double latitudeDeg, double longitudeDeg, double altitudeM) const
{
    const double lat = degToRad(latitudeDeg);
    const double lon = degToRad(longitudeDeg);
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double n = m_a / std::sqrt(1.0 - m_e2 * sinLat * sinLat);

    return {(n + altitudeM) * cosLat * std::cos(lon),
            (n + altitudeM) * cosLat * std::sin(lon),
            (n * (1.0 - m_e2) + altitudeM) * sinLat};
}

} // namespace geo
