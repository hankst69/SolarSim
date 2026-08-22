#include "geolib/EarthModel.h"

#include "geolib/Angle.h"

namespace geo {

const EarthModel& EarthModel::defaultModel()
{
    static const SphericalEarthModel model;
    return model;
}

SphericalEarthModel::SphericalEarthModel(double radiusM) : m_radiusM(radiusM) {}

double SphericalEarthModel::localRadius(double /*latitudeDeg*/) const
{
    return m_radiusM;
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

} // namespace geo
