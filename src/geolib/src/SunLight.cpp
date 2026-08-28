#include "geolib/SunLight.h"

#include <algorithm>
#include <cmath>

namespace geo {

SunLight::SunLight(const TerrainModel& terrain, const DateTimeUtc& utc)
    : m_sun(terrain.dome().standpoint(), utc)
{
    double centreHeightM = 0.0;
    terrain.heightAt(0.0, 0.0, centreHeightM);

    double radiusM = coveredRadius(terrain.sceneMesh());
    if (radiusM <= 0.0) {
        radiusM = terrain.extent();
    }
    build(radiusM, centreHeightM);
}

SunLight::SunLight(const SunPosition& sun, double sceneRadiusM, double sceneCentreHeightM)
    : m_sun(sun)
{
    build(sceneRadiusM, sceneCentreHeightM);
}

void SunLight::build(double sceneRadiusM, double sceneCentreHeightM)
{
    m_toSun = m_sun.direction().normalized();

    m_halfSizeM = std::max(sceneRadiusM, 1.0) * kCoverageMargin;

    // The rectangle is pushed far enough away that it never intersects the
    // scene, no matter how low the sun stands.
    m_distanceM = 2.0 * m_halfSizeM;

    const Vector3 sceneCentre{0.0, 0.0, sceneCentreHeightM};
    m_centre = sceneCentre + m_toSun * m_distanceM;

    // Build an orthonormal basis of the rectangle plane. Up is a good
    // reference axis unless the sun stands (nearly) in the zenith.
    Vector3 reference{0.0, 0.0, 1.0};
    if (std::fabs(m_toSun.dot(reference)) > 0.9) {
        reference = Vector3{0.0, 1.0, 0.0};
    }
    m_axisU = reference.cross(m_toSun).normalized();
    m_axisV = m_toSun.cross(m_axisU).normalized();
}

Vector3 SunLight::cornerAt(double u, double v) const
{
    return m_centre + m_axisU * (u * m_halfSizeM) + m_axisV * (v * m_halfSizeM);
}

Vector3 SunLight::rayOriginFor(const Vector3& localPoint) const
{
    // All rays are parallel, so the emission point is the scene point pushed
    // back along the sun direction onto the plane of the rectangle.
    const double t = (m_centre - localPoint).dot(m_toSun);
    return localPoint + m_toSun * t;
}

double SunLight::coveredRadius(const TriangleMesh& mesh)
{
    Vector3 minCorner;
    Vector3 maxCorner;
    if (!mesh.bounds(minCorner, maxCorner)) {
        return 0.0;
    }

    // Radius of the bounding sphere around the local origin: the farthest
    // corner of the axis aligned bounding box decides.
    const double eastM = std::max(std::fabs(minCorner.x), std::fabs(maxCorner.x));
    const double northM = std::max(std::fabs(minCorner.y), std::fabs(maxCorner.y));
    const double upM = std::max(std::fabs(minCorner.z), std::fabs(maxCorner.z));

    return std::sqrt(eastM * eastM + northM * northM + upM * upM);
}

} // namespace geo
