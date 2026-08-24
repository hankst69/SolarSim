#include "geolib/HorizonDome.h"

#include "geolib/Angle.h"

namespace geo {

HorizonDome::HorizonDome(const GeoLocation& standpoint, double viewHeightM)
    : m_groundPlane(standpoint)
    , m_viewHeightM(viewHeightM)
    , m_earthRadiusM(standpoint.earthModel().localRadius(standpoint.latitude()))
{
    // Viewpoint at height h above the standpoint, tangent line to the earth
    // sphere of radius R touches the surface at the tangent point T. Seen from
    // the earth centre the tangent point lies at the angle theta with
    // cos(theta) = R / (R + h). The dome radius is the distance from the
    // standpoint to T measured in the ground plane, i.e. R * sin(theta):
    //     r = R * sqrt(h * (2R + h)) / (R + h)
    const double R = m_earthRadiusM;
    const double h = m_viewHeightM;
    m_radiusM = (h > 0.0) ? R * std::sqrt(h * (2.0 * R + h)) / (R + h) : 0.0;
}

double HorizonDome::lineOfSightDistance() const
{
    const double R = m_earthRadiusM;
    const double h = m_viewHeightM;
    return std::sqrt(h * (2.0 * R + h));
}

double HorizonDome::geocentricHorizonAngle() const
{
    const double R = m_earthRadiusM;
    return std::acos(R / (R + m_viewHeightM));
}

double HorizonDome::arcDistanceToHorizon() const
{
    return m_earthRadiusM * geocentricHorizonAngle();
}

double HorizonDome::curvatureDrop() const
{
    const double R = m_earthRadiusM;
    const double h = m_viewHeightM;
    return R * h / (R + h);
}

Vector3 HorizonDome::pointOnDome(double azimuthDeg, double elevationDeg) const
{
    const double az = degToRad(azimuthDeg);
    const double el = degToRad(elevationDeg);
    const double horizontal = m_radiusM * std::cos(el);

    return {horizontal * std::sin(az), horizontal * std::cos(az), m_radiusM * std::sin(el)};
}

bool HorizonDome::contains(const Vector3& localPoint) const
{
    return localPoint.z >= 0.0 && localPoint.length() <= m_radiusM;
}

} // namespace geo
