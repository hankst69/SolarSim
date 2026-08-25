#include "TestSupport.h"

#include "geolib/GridHeightDataSource.h"
#include "geolib/SunLight.h"

#include <cmath>
#include <memory>

using namespace geo;

namespace {

GeoLocation home()
{
    return GeoLocation(49.56255, 11.14493);
}

TerrainModel makeTerrain()
{
    const HorizonDome dome(home());

    TerrainModel::Config config;
    config.extentM = 100.0;
    config.gridSpacingM = 25.0;
    config.clipToDomeCircle = false;

    return TerrainModel(dome, std::make_shared<FlatHeightDataSource>(0.0), config);
}

/// The light plane normal points at the sun, all rays are parallel.
void testRaysAreParallelToSunDirection()
{
    const SunPosition sun(home(), DateTimeUtc(2026, 6, 21, 10, 0, 0));
    const SunLight light(sun, 100.0);

    const Vector3 expected = sun.direction().normalized();
    CHECK_NEAR(light.directionToSun().x, expected.x, 1e-12);
    CHECK_NEAR(light.directionToSun().y, expected.y, 1e-12);
    CHECK_NEAR(light.directionToSun().z, expected.z, 1e-12);

    // rayDirection is exactly the opposite of directionToSun.
    CHECK_NEAR(light.rayDirection().x, -expected.x, 1e-12);
    CHECK_NEAR(light.rayDirection().y, -expected.y, 1e-12);
    CHECK_NEAR(light.rayDirection().z, -expected.z, 1e-12);
    CHECK_NEAR(light.directionToSun().length(), 1.0, 1e-12);
}

/// The rectangle axes form an orthonormal frame with the sun direction.
void testRectangleFrameIsOrthonormal()
{
    const SunPosition sun(home(), DateTimeUtc(2026, 6, 21, 10, 0, 0));
    const SunLight light(sun, 100.0);

    CHECK_NEAR(light.axisU().length(), 1.0, 1e-12);
    CHECK_NEAR(light.axisV().length(), 1.0, 1e-12);
    CHECK_NEAR(light.axisU().dot(light.axisV()), 0.0, 1e-12);
    CHECK_NEAR(light.axisU().dot(light.directionToSun()), 0.0, 1e-12);
    CHECK_NEAR(light.axisV().dot(light.directionToSun()), 0.0, 1e-12);
}

/// The rectangle is large enough to cover the whole scene.
void testRectangleCoversScene()
{
    const double sceneRadiusM = 100.0;
    const SunPosition sun(home(), DateTimeUtc(2026, 6, 21, 10, 0, 0));
    const SunLight light(sun, sceneRadiusM);

    CHECK_TRUE(light.halfSize() >= sceneRadiusM);
    CHECK_NEAR(light.size(), 2.0 * light.halfSize(), 1e-12);
    CHECK_TRUE(light.nearPlane() > 0.0);
    CHECK_TRUE(light.farPlane() > light.nearPlane());
}

/// Every ray origin lies in the rectangle plane and points at its scene point.
void testRayOriginLiesInLightPlane()
{
    const SunPosition sun(home(), DateTimeUtc(2026, 6, 21, 10, 0, 0));
    const SunLight light(sun, 100.0);

    const Vector3 scenePoint{20.0, -35.0, 4.0};
    const Vector3 origin = light.rayOriginFor(scenePoint);

    // Origin is in the plane through centre with normal directionToSun.
    CHECK_NEAR((origin - light.centre()).dot(light.directionToSun()), 0.0, 1e-6);

    // The ray from the origin to the scene point runs along the sun rays.
    const Vector3 ray = (scenePoint - origin).normalized();
    CHECK_NEAR(ray.x, light.rayDirection().x, 1e-9);
    CHECK_NEAR(ray.y, light.rayDirection().y, 1e-9);
    CHECK_NEAR(ray.z, light.rayDirection().z, 1e-9);
}

/// Below the horizon the light contributes nothing.
void testBelowHorizonHasNoIntensity()
{
    const SunPosition night(home(), DateTimeUtc(2026, 12, 21, 23, 0, 0));
    const SunLight light(night, 100.0);

    CHECK_FALSE(light.isAboveHorizon());
    CHECK_NEAR(light.intensity(), 0.0, 1e-12);
}

/// Constructed from a terrain model the light is sized after the scene mesh.
void testFromTerrainModel()
{
    const TerrainModel terrain = makeTerrain();
    const SunLight light(terrain, DateTimeUtc(2026, 6, 21, 10, 0, 0));

    const double radiusM = SunLight::coveredRadius(terrain.sceneMesh());
    CHECK_TRUE(radiusM > 0.0);
    CHECK_NEAR(light.halfSize(), radiusM * SunLight::kCoverageMargin, 1e-9);
    CHECK_TRUE(light.isAboveHorizon());
}

} // namespace

int main()
{
    testRaysAreParallelToSunDirection();
    testRectangleFrameIsOrthonormal();
    testRectangleCoversScene();
    testRayOriginLiesInLightPlane();
    testBelowHorizonHasNoIntensity();
    testFromTerrainModel();
    return geotest::summarize("SunLightTests");
}
