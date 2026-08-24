#include "geolib/GroundPlane.h"

#include "TestSupport.h"

#include <cmath>

using namespace geo;

namespace {

constexpr double kMunichLat = 48.1372;
constexpr double kMunichLon = 11.5756;

GroundPlane makePlane(double altitude = 0.0)
{
    return GroundPlane(GeoLocation(kMunichLat, kMunichLon, altitude));
}

void testOriginAndAxes()
{
    const GroundPlane plane = makePlane();
    CHECK_NEAR(plane.origin().latitude(), kMunichLat, 1e-12);

    const GeoLocation origin(kMunichLat, kMunichLon);
    const Vector3 expected = origin.toEcef();
    CHECK_NEAR(plane.originEcef().x, expected.x, 1e-6);
    CHECK_NEAR(plane.originEcef().y, expected.y, 1e-6);
    CHECK_NEAR(plane.originEcef().z, expected.z, 1e-6);

    // Orthonormal right handed frame.
    CHECK_NEAR(plane.east().length(), 1.0, 1e-12);
    CHECK_NEAR(plane.north().length(), 1.0, 1e-12);
    CHECK_NEAR(plane.normal().length(), 1.0, 1e-12);
    CHECK_NEAR(plane.east().dot(plane.north()), 0.0, 1e-12);
    CHECK_NEAR(plane.east().dot(plane.normal()), 0.0, 1e-12);
    CHECK_NEAR(plane.north().dot(plane.normal()), 0.0, 1e-12);
}

void testToLocalAtOrigin()
{
    const GroundPlane plane = makePlane();
    const Vector3 local = plane.toLocal(plane.originEcef());
    CHECK_NEAR(local.x, 0.0, 1e-6);
    CHECK_NEAR(local.y, 0.0, 1e-6);
    CHECK_NEAR(local.z, 0.0, 1e-6);
}

void testToLocalAxes()
{
    const GroundPlane plane = makePlane();

    // Move 100 m along each ECEF axis direction of the local frame.
    const Vector3 east = plane.toLocal(plane.originEcef() + plane.east() * 100.0);
    CHECK_NEAR(east.x, 100.0, 1e-6);
    CHECK_NEAR(east.y, 0.0, 1e-6);
    CHECK_NEAR(east.z, 0.0, 1e-6);

    const Vector3 north = plane.toLocal(plane.originEcef() + plane.north() * 250.0);
    CHECK_NEAR(north.y, 250.0, 1e-6);

    const Vector3 up = plane.toLocal(plane.originEcef() + plane.normal() * 30.0);
    CHECK_NEAR(up.z, 30.0, 1e-6);
}

void testToLocalToEcefRoundTrip()
{
    const GroundPlane plane = makePlane();
    const Vector3 samples[] = {{0.0, 0.0, 0.0},
                               {100.0, -250.0, 12.0},
                               {-5000.0, 3000.0, -40.0},
                               {1.0, 1.0, 1.0}};
    for (const Vector3& local : samples) {
        const Vector3 ecef = plane.toEcef(local);
        const Vector3 back = plane.toLocal(ecef);
        CHECK_NEAR(back.x, local.x, 1e-6);
        CHECK_NEAR(back.y, local.y, 1e-6);
        CHECK_NEAR(back.z, local.z, 1e-6);
    }
}

void testHeightAbove()
{
    const GroundPlane plane = makePlane();
    CHECK_NEAR(plane.heightAbove(plane.originEcef()), 0.0, 1e-6);

    // Straight up is positive, down is negative.
    CHECK_NEAR(plane.heightAbove(plane.originEcef() + plane.normal() * 75.0), 75.0, 1e-6);
    CHECK_NEAR(plane.heightAbove(plane.originEcef() - plane.normal() * 20.0), -20.0, 1e-6);

    // Movement inside the plane does not change the height.
    CHECK_NEAR(plane.heightAbove(plane.originEcef() + plane.east() * 500.0), 0.0, 1e-6);
}

void testToLocalOfGeoLocation()
{
    const GroundPlane plane = makePlane();

    // The origin maps to zero.
    const Vector3 self = plane.toLocal(GeoLocation(kMunichLat, kMunichLon));
    CHECK_NEAR(self.length(), 0.0, 1e-6);

    // A point due north has a positive north component and almost no east one.
    const Vector3 north = plane.toLocal(GeoLocation(kMunichLat + 0.01, kMunichLon));
    CHECK_TRUE(north.y > 1000.0);
    CHECK_NEAR(north.x, 0.0, 1.0);

    // A point due east has a positive east component.
    const Vector3 east = plane.toLocal(GeoLocation(kMunichLat, kMunichLon + 0.01));
    CHECK_TRUE(east.x > 700.0);
    CHECK_NEAR(east.y, 0.0, 1.0);

    // Altitude shows up as height above the plane.
    const Vector3 raised = plane.toLocal(GeoLocation(kMunichLat, kMunichLon, 100.0));
    CHECK_NEAR(raised.z, 100.0, 1e-3);
}

void testCurvatureRadii()
{
    const GroundPlane plane = makePlane();
    const EarthModel& model = EarthModel::defaultModel();
    CHECK_NEAR(plane.meridionalRadius(), model.meridionalRadius(kMunichLat), 1e-6);
    CHECK_NEAR(plane.primeVerticalRadius(), model.primeVerticalRadius(kMunichLat), 1e-6);
    CHECK_TRUE(plane.primeVerticalRadius() > plane.meridionalRadius());
}

void testCurvatureDrop()
{
    const GroundPlane plane = makePlane();

    // No drop at the origin.
    CHECK_NEAR(plane.curvatureDrop(0.0, 0.0), 0.0, 1e-9);

    // The drop grows quadratically: d ~ s^2 / (2R).
    const double r = plane.meridionalRadius();
    CHECK_NEAR(plane.curvatureDrop(0.0, 1000.0), 1000.0 * 1000.0 / (2.0 * r), 1e-3);

    const double n = plane.primeVerticalRadius();
    CHECK_NEAR(plane.curvatureDrop(1000.0, 0.0), 1000.0 * 1000.0 / (2.0 * n), 1e-3);

    // About 7.8 cm after 1 km, about 2 m after 5 km.
    CHECK_NEAR(plane.curvatureDrop(1000.0, 0.0), 0.0784, 5e-3);
    CHECK_NEAR(plane.curvatureDrop(5000.0, 0.0), 1.96, 0.05);

    // Symmetric in both directions and always positive.
    CHECK_NEAR(plane.curvatureDrop(-2000.0, 0.0), plane.curvatureDrop(2000.0, 0.0), 1e-9);
    CHECK_NEAR(plane.curvatureDrop(0.0, -2000.0), plane.curvatureDrop(0.0, 2000.0), 1e-9);
    CHECK_TRUE(plane.curvatureDrop(3000.0, 4000.0) > 0.0);

    // Monotonically increasing with distance.
    CHECK_TRUE(plane.curvatureDrop(2000.0, 0.0) > plane.curvatureDrop(1000.0, 0.0));
}

void testToGeoLocationAtOrigin()
{
    const GroundPlane plane = makePlane(500.0);
    const GeoLocation origin = plane.toGeoLocation(Vector3{0.0, 0.0, 0.0});
    CHECK_NEAR(origin.latitude(), kMunichLat, 1e-9);
    CHECK_NEAR(origin.longitude(), kMunichLon, 1e-9);
    CHECK_NEAR(origin.altitude(), 500.0, 1e-6);
}

void testToGeoLocationDirections()
{
    const GroundPlane plane = makePlane();

    // 1000 m north increases the latitude by about 1000 / M radians.
    const GeoLocation north = plane.toGeoLocation(Vector3{0.0, 1000.0, 0.0});
    CHECK_TRUE(north.latitude() > kMunichLat);
    CHECK_NEAR(north.longitude(), kMunichLon, 1e-9);

    // 1000 m east increases the longitude, latitude stays put.
    const GeoLocation east = plane.toGeoLocation(Vector3{1000.0, 0.0, 0.0});
    CHECK_TRUE(east.longitude() > kMunichLon);
    CHECK_NEAR(east.latitude(), kMunichLat, 1e-9);

    // Negative offsets go the other way.
    CHECK_TRUE(plane.toGeoLocation(Vector3{0.0, -1000.0, 0.0}).latitude() < kMunichLat);
    CHECK_TRUE(plane.toGeoLocation(Vector3{-1000.0, 0.0, 0.0}).longitude() < kMunichLon);
}

/// toLocal and toGeoLocation must be inverse to each other.
void testGeoLocationRoundTrip()
{
    const GroundPlane plane = makePlane();
    const Vector3 offsets[] = {
        {0.0, 0.0, 0.0}, {250.0, 400.0, 0.0}, {-1500.0, 900.0, 0.0}, {3000.0, -2500.0, 0.0}};

    for (const Vector3& offset : offsets) {
        const GeoLocation location = plane.toGeoLocation(offset);
        const Vector3 back = plane.toLocal(location);
        // The mapping is a first order (tangential) approximation, so the
        // deviation grows with the distance from the origin.
        const double distance = std::sqrt(offset.x * offset.x + offset.y * offset.y);
        const double tolerance = 0.5 + 1e-3 * distance;
        CHECK_NEAR(back.x, offset.x, tolerance);
        CHECK_NEAR(back.y, offset.y, tolerance);
    }
}

/// The distance of a projected offset must match the offset length. toGeoLocation
/// uses the directional radii of curvature while distanceTo uses the Gaussian
/// mean radius, so a relative deviation of a few parts per thousand is expected.
void testGeoLocationDistanceScaling()
{
    const GroundPlane plane = makePlane();
    const GeoLocation origin = plane.origin();
    const GeoLocation north = plane.toGeoLocation(Vector3{0.0, 2000.0, 0.0});
    CHECK_NEAR(origin.distanceTo(north), 2000.0, 6.0);

    const GeoLocation east = plane.toGeoLocation(Vector3{2000.0, 0.0, 0.0});
    CHECK_NEAR(origin.distanceTo(east), 2000.0, 6.0);
}

/// A plane at the equator and one at high latitude must both stay consistent.
void testDifferentLatitudes()
{
    const double latitudes[] = {-75.0, -30.0, 0.0, 30.0, 75.0};
    for (double lat : latitudes) {
        const GroundPlane plane(GeoLocation(lat, 5.0));
        const Vector3 offset{800.0, -600.0, 0.0};
        const GeoLocation location = plane.toGeoLocation(offset);
        const Vector3 back = plane.toLocal(location);
        CHECK_NEAR(back.x, offset.x, 2.0);
        CHECK_NEAR(back.y, offset.y, 2.0);
    }
}

} // namespace

int main()
{
    testOriginAndAxes();
    testToLocalAtOrigin();
    testToLocalAxes();
    testToLocalToEcefRoundTrip();
    testHeightAbove();
    testToLocalOfGeoLocation();
    testCurvatureRadii();
    testCurvatureDrop();
    testToGeoLocationAtOrigin();
    testToGeoLocationDirections();
    testGeoLocationRoundTrip();
    testGeoLocationDistanceScaling();
    testDifferentLatitudes();
    return geotest::summarize("GroundPlaneTests");
}
