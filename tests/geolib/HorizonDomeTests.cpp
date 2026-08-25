#include "geolib/HorizonDome.h"

#include "TestSupport.h"

#include "geolib/Angle.h"
#include "geolib/HeightDataSourceRegistry.h"

#include <cmath>

using namespace geo;

namespace {

constexpr double kMunichLat = 48.1372;
constexpr double kMunichLon = 11.5756;

GeoLocation munich()
{
    return GeoLocation(kMunichLat, kMunichLon);
}

/// Height data source delivering one constant terrain height everywhere.
class ConstantHeightDataSource : public HeightDataSource {
public:
    explicit ConstantHeightDataSource(double heightM, bool hasData = true)
        : m_heightM(heightM)
        , m_hasData(hasData)
    {
    }

    std::string name() const override { return "ConstantHeightDataSource"; }
    GeoBounds coverage() const override { return GeoBounds::world(); }
    double resolutionM() const override { return 1.0; }

    bool sampleHeight(double, double, double& heightM) const override
    {
        if (!m_hasData) {
            return false;
        }
        heightM = m_heightM;
        return true;
    }

private:
    double m_heightM{0.0};
    bool m_hasData{true};
};

void testDefaults()
{
    const HorizonDome dome(munich());
    CHECK_NEAR(dome.viewHeight(), HorizonDome::kDefaultViewHeightM, 1e-12);
    CHECK_NEAR(HorizonDome::kDefaultViewHeightM, 1.6, 1e-12);
    CHECK_NEAR(dome.standpoint().latitude(), kMunichLat, 1e-12);
    CHECK_NEAR(dome.groundPlane().origin().longitude(), kMunichLon, 1e-12);
}

/// For an eye height of 1.6 m the horizon is roughly 4.5 km away.
void testRadiusMagnitude()
{
    const HorizonDome dome(munich());
    CHECK_NEAR(dome.radius(), 4515.0, 60.0);

    // A taller viewpoint sees further.
    CHECK_TRUE(HorizonDome(munich(), 10.0).radius() > dome.radius());
    CHECK_TRUE(HorizonDome(munich(), 0.5).radius() < dome.radius());

    // 100 m up gives roughly 35 km.
    CHECK_NEAR(HorizonDome(munich(), 100.0).radius(), 35700.0, 500.0);
}

/// radius = R * sqrt(h * (2R + h)) / (R + h) = R * sin(theta)
void testRadiusFormula()
{
    const double heights[] = {0.5, 1.6, 10.0, 100.0, 1000.0};
    for (double h : heights) {
        const HorizonDome dome(munich(), h);
        const double r = EarthModel::defaultModel().localRadius(kMunichLat);
        const double expected = r * std::sqrt(h * (2.0 * r + h)) / (r + h);
        CHECK_NEAR(dome.radius(), expected, 1e-3);

        // Equivalent to R * sin of the geocentric horizon angle.
        CHECK_NEAR(dome.radius(), r * std::sin(dome.geocentricHorizonAngle()), 1e-3);
    }
}

void testZeroViewHeight()
{
    // Standing exactly on the ground the horizon collapses onto the standpoint.
    const HorizonDome dome(munich(), 0.0);
    CHECK_NEAR(dome.radius(), 0.0, 1e-6);
}

/// The tangent line, the earth radius and the eye position form a right
/// triangle: d^2 = h^2 + 2*R*h.
void testLineOfSightDistance()
{
    const double heights[] = {1.6, 25.0, 500.0};
    for (double h : heights) {
        const HorizonDome dome(munich(), h);
        const double r = EarthModel::defaultModel().localRadius(kMunichLat);
        CHECK_NEAR(dome.lineOfSightDistance(), std::sqrt(h * h + 2.0 * r * h), 1e-3);

        // The straight line of sight is marginally longer than the ground
        // radius, since the latter is the projection into the plane.
        CHECK_TRUE(dome.lineOfSightDistance() >= dome.radius() - 1e-6);
    }
}

void testGeocentricHorizonAngle()
{
    const HorizonDome dome(munich());
    const double angle = dome.geocentricHorizonAngle();

    // A small positive angle for a human eye height.
    CHECK_TRUE(angle > 0.0);
    CHECK_TRUE(angle < degToRad(1.0));

    // cos(angle) = R / (R + h).
    const double r = EarthModel::defaultModel().localRadius(kMunichLat);
    CHECK_NEAR(std::cos(angle), r / (r + dome.viewHeight()), 1e-9);

    // A higher viewpoint widens the angle.
    CHECK_TRUE(HorizonDome(munich(), 100.0).geocentricHorizonAngle() > angle);
}

void testArcDistance()
{
    const HorizonDome dome(munich());
    const double r = EarthModel::defaultModel().localRadius(kMunichLat);
    CHECK_NEAR(dome.arcDistanceToHorizon(), r * dome.geocentricHorizonAngle(), 1e-6);

    // The arc is marginally longer than the straight chord in the plane.
    CHECK_TRUE(dome.arcDistanceToHorizon() >= dome.radius() - 1e-6);
    // ...but only by a tiny amount at this scale.
    CHECK_TRUE(dome.arcDistanceToHorizon() - dome.radius() < 1.0);
}

void testCurvatureDropAtHorizon()
{
    const HorizonDome dome(munich());
    const double drop = dome.curvatureDrop();
    CHECK_TRUE(drop > 0.0);

    // At the tangent point the surface has fallen by R * (1 - cos(theta)),
    // which for the horizon is exactly R * h / (R + h) ~ the eye height.
    const double r = EarthModel::defaultModel().localRadius(kMunichLat);
    CHECK_NEAR(drop, r * (1.0 - std::cos(dome.geocentricHorizonAngle())), 1e-6);
    CHECK_NEAR(drop, 1.6, 0.01);
}

void testPointOnDomeAzimuth()
{
    const HorizonDome dome(munich());
    const double r = dome.radius();

    // Azimuth is measured clockwise from north at the rim (elevation 0).
    const Vector3 north = dome.pointOnDome(0.0, 0.0);
    CHECK_NEAR(north.x, 0.0, 1e-6);
    CHECK_NEAR(north.y, r, 1e-6);
    CHECK_NEAR(north.z, 0.0, 1e-6);

    const Vector3 east = dome.pointOnDome(90.0, 0.0);
    CHECK_NEAR(east.x, r, 1e-6);
    CHECK_NEAR(east.y, 0.0, 1e-6);

    const Vector3 south = dome.pointOnDome(180.0, 0.0);
    CHECK_NEAR(south.y, -r, 1e-6);

    const Vector3 west = dome.pointOnDome(270.0, 0.0);
    CHECK_NEAR(west.x, -r, 1e-6);
}

void testPointOnDomeElevation()
{
    const HorizonDome dome(munich());
    const double r = dome.radius();

    // The zenith is straight up regardless of the azimuth.
    for (double azimuth : {0.0, 45.0, 123.0, 300.0}) {
        const Vector3 zenith = dome.pointOnDome(azimuth, 90.0);
        CHECK_NEAR(zenith.x, 0.0, 1e-6);
        CHECK_NEAR(zenith.y, 0.0, 1e-6);
        CHECK_NEAR(zenith.z, r, 1e-6);
    }

    // 45 degrees: horizontal and vertical components are equal.
    const Vector3 diagonal = dome.pointOnDome(0.0, 45.0);
    CHECK_NEAR(diagonal.y, r * std::cos(degToRad(45.0)), 1e-6);
    CHECK_NEAR(diagonal.z, r * std::sin(degToRad(45.0)), 1e-6);
}

/// Every point produced by pointOnDome lies on the sphere of the dome radius.
void testPointOnDomeLiesOnSphere()
{
    const HorizonDome dome(munich());
    for (double azimuth = 0.0; azimuth < 360.0; azimuth += 30.0) {
        for (double elevation = 0.0; elevation <= 90.0; elevation += 15.0) {
            const Vector3 point = dome.pointOnDome(azimuth, elevation);
            CHECK_NEAR(point.length(), dome.radius(), 1e-6);
            CHECK_TRUE(point.z >= -1e-6); // upper half only
        }
    }
}

void testContains()
{
    const HorizonDome dome(munich());
    const double r = dome.radius();

    CHECK_TRUE(dome.contains(Vector3{0.0, 0.0, 0.0}));
    CHECK_TRUE(dome.contains(Vector3{r * 0.5, 0.0, 0.0}));
    CHECK_TRUE(dome.contains(Vector3{0.0, 0.0, r * 0.9}));

    // Outside the radius in any direction.
    CHECK_FALSE(dome.contains(Vector3{r * 1.1, 0.0, 0.0}));
    CHECK_FALSE(dome.contains(Vector3{0.0, 0.0, r * 1.1}));
    CHECK_FALSE(dome.contains(Vector3{r, r, 0.0}));

    // Points on the rim count as inside.
    CHECK_TRUE(dome.contains(dome.pointOnDome(37.0, 12.0)));
}

/// The dome radius depends on the latitude only through the local radius.
void testLatitudeDependence()
{
    const HorizonDome equator(GeoLocation(0.0, 0.0));
    const HorizonDome pole(GeoLocation(89.0, 0.0));

    // Both are in the same ballpark; the polar curvature radius is larger, so
    // the horizon is slightly further away.
    CHECK_TRUE(equator.radius() > 4000.0 && equator.radius() < 5000.0);
    CHECK_TRUE(pole.radius() > equator.radius());
    CHECK_TRUE(pole.radius() - equator.radius() < 50.0);
}

/// The terrain height above sea level adds to the eye height.
void testTerrainHeight()
{
    const HorizonDome dome(munich(), 1.6, 500.0);
    CHECK_NEAR(dome.viewHeight(), 1.6, 1e-12);
    CHECK_NEAR(dome.terrainHeight(), 500.0, 1e-12);
    CHECK_NEAR(dome.observerHeight(), 501.6, 1e-12);

    const double r = EarthModel::defaultModel().localRadius(kMunichLat);
    const double h = 501.6;
    CHECK_NEAR(dome.radius(), r * std::sqrt(h * (2.0 * r + h)) / (r + h), 1e-3);

    // Standing on a mountain the horizon is much further away.
    CHECK_TRUE(dome.radius() > HorizonDome(munich(), 1.6).radius());
    CHECK_NEAR(dome.curvatureDrop(), r * h / (r + h), 1e-6);

    // Without a terrain height the altitude of the standpoint is used.
    const HorizonDome viaAltitude(GeoLocation(kMunichLat, kMunichLon, 500.0), 1.6);
    CHECK_NEAR(viaAltitude.terrainHeight(), 500.0, 1e-12);
    CHECK_NEAR(viaAltitude.radius(), dome.radius(), 1e-6);
}

void testTerrainHeightFromDataSource()
{
    const HeightDataSourcePtr source = std::make_shared<ConstantHeightDataSource>(1200.0);
    const HorizonDome dome = HorizonDome::fromHeightDataSource(munich(), source, 1.6);
    CHECK_NEAR(dome.terrainHeight(), 1200.0, 1e-12);
    CHECK_NEAR(dome.observerHeight(), 1201.6, 1e-12);
    CHECK_TRUE(dome.radius() > HorizonDome(munich(), 1.6).radius());

    // A source without data falls back to the altitude of the standpoint.
    const HeightDataSourcePtr empty =
        std::make_shared<ConstantHeightDataSource>(1200.0, false);
    const HorizonDome fallback = HorizonDome::fromHeightDataSource(
        GeoLocation(kMunichLat, kMunichLon, 300.0), empty, 1.6);
    CHECK_NEAR(fallback.terrainHeight(), 300.0, 1e-12);

    // The same holds for a missing source.
    const HorizonDome noSource = HorizonDome::fromHeightDataSource(
        GeoLocation(kMunichLat, kMunichLon, 300.0), nullptr, 1.6);
    CHECK_NEAR(noSource.terrainHeight(), 300.0, 1e-12);
}

void testTerrainHeightFromRegistry()
{
    HeightDataSourceRegistry& registry = HeightDataSourceRegistry::instance();
    const std::vector<HeightDataSourcePtr> saved = registry.sources();

    registry.clear();
    registry.addSource(std::make_shared<ConstantHeightDataSource>(800.0));

    const HorizonDome dome = HorizonDome::fromHeightDataSourceRegistry(munich(), 1.6);
    CHECK_NEAR(dome.terrainHeight(), 800.0, 1e-12);
    CHECK_NEAR(dome.observerHeight(), 801.6, 1e-12);

    registry.clear();
    for (const HeightDataSourcePtr& source : saved) {
        registry.addSource(source);
    }
}

} // namespace

int main()
{
    testDefaults();
    testRadiusMagnitude();
    testRadiusFormula();
    testZeroViewHeight();
    testLineOfSightDistance();
    testGeocentricHorizonAngle();
    testArcDistance();
    testCurvatureDropAtHorizon();
    testPointOnDomeAzimuth();
    testPointOnDomeElevation();
    testPointOnDomeLiesOnSphere();
    testContains();
    testLatitudeDependence();
    testTerrainHeight();
    testTerrainHeightFromDataSource();
    testTerrainHeightFromRegistry();
    return geotest::summarize("HorizonDomeTests");
}
