#include "geolib/HorizonDome.h"

#include "geolib/Angle.h"
#include "geolib/HeightDataSourceRegistry.h"

namespace geo {

HorizonDome::HorizonDome(const GeoLocation& standpoint, double viewHeightM)
    : HorizonDome(standpoint, viewHeightM, standpoint.altitude())
{
}

HorizonDome::HorizonDome(const GeoLocation& standpoint, double viewHeightM,
                         double terrainHeightM)
    : m_groundPlane(standpoint)
    , m_viewHeightM(viewHeightM)
    , m_terrainHeightM(terrainHeightM)
    , m_earthRadiusM(standpoint.earthModel().localRadius(standpoint.latitude()))
{
    compute();
}

HorizonDome HorizonDome::fromHeightDataSource(const GeoLocation& standpoint,
                                              const HeightDataSourcePtr& source,
                                              double viewHeightM)
{
    double terrainHeightM = standpoint.altitude();
    if (source) {
        double sampledM = 0.0;
        if (source->sampleHeight(standpoint, sampledM)) {
            terrainHeightM = sampledM;
        }
    }
    return HorizonDome(standpoint, viewHeightM, terrainHeightM);
}

HorizonDome HorizonDome::fromHeightDataSourceRegistry(const GeoLocation& standpoint,
                                                      double viewHeightM)
{
    const HeightDataSourcePtr source =
        HeightDataSourceRegistry::instance().selectSource(standpoint);
    return fromHeightDataSource(standpoint, source, viewHeightM);
}

void HorizonDome::compute()
{
    // Viewpoint at height h above the earth surface (terrain height above sea
    // level plus the eye height above the terrain), tangent line to the earth
    // sphere of radius R touches the surface at the tangent point T. Seen from
    // the earth centre the tangent point lies at the angle theta with
    // cos(theta) = R / (R + h). The dome radius is the distance from the
    // standpoint to T measured in the ground plane, i.e. R * sin(theta):
    //     r = R * sqrt(h * (2R + h)) / (R + h)
    const double R = m_earthRadiusM;
    const double h = observerHeight();
    m_radiusM = (h > 0.0) ? R * std::sqrt(h * (2.0 * R + h)) / (R + h) : 0.0;
}

double HorizonDome::lineOfSightDistance() const
{
    const double R = m_earthRadiusM;
    const double h = observerHeight();
    return std::sqrt(h * (2.0 * R + h));
}

double HorizonDome::geocentricHorizonAngle() const
{
    const double R = m_earthRadiusM;
    return std::acos(R / (R + observerHeight()));
}

double HorizonDome::arcDistanceToHorizon() const
{
    return m_earthRadiusM * geocentricHorizonAngle();
}

double HorizonDome::curvatureDrop() const
{
    const double R = m_earthRadiusM;
    const double h = observerHeight();
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
