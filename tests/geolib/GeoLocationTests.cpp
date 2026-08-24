#include "geolib/GeoLocation.h"

#include "TestSupport.h"

#include "geolib/GroundPlane.h"

#include <cmath>

using namespace geo;

namespace {

constexpr double kMunichLat = 48.1372;
constexpr double kMunichLon = 11.5756;

void testAccessors()
{
    const GeoLocation location(kMunichLat, kMunichLon, 519.0);
    CHECK_NEAR(location.latitude(), kMunichLat, 1e-12);
    CHECK_NEAR(location.longitude(), kMunichLon, 1e-12);
    CHECK_NEAR(location.altitude(), 519.0, 1e-12);

    // Altitude defaults to zero.
    CHECK_NEAR(GeoLocation(0.0, 0.0).altitude(), 0.0, 1e-12);
}

void testDefaultModelIsWgs84()
{
    const GeoLocation location(kMunichLat, kMunichLon);
    CHECK_NEAR(location.earthModel().flattening(), 1.0 / 298.257223563, 1e-15);
    CHECK_TRUE(&location.earthModel() == &EarthModel::defaultModel());
}

void testExplicitModel()
{
    const GeoLocation spherical(kMunichLat, kMunichLon, 0.0, EarthModel::sphericalModel());
    CHECK_NEAR(spherical.earthModel().flattening(), 0.0, 1e-15);

    // The ellipsoidal and spherical positions differ noticeably but stay within
    // the same order of magnitude.
    const GeoLocation ellipsoidal(kMunichLat, kMunichLon);
    const double distance = (ellipsoidal.toEcef() - spherical.toEcef()).length();
    CHECK_TRUE(distance > 1000.0);
    CHECK_TRUE(distance < 30000.0);
}

void testToEcefMatchesModel()
{
    const GeoLocation location(kMunichLat, kMunichLon, 519.0);
    const Vector3 expected =
        location.earthModel().toEcef(kMunichLat, kMunichLon, 519.0);
    const Vector3 actual = location.toEcef();
    CHECK_NEAR(actual.x, expected.x, 1e-9);
    CHECK_NEAR(actual.y, expected.y, 1e-9);
    CHECK_NEAR(actual.z, expected.z, 1e-9);
}

/// east/north/up must form a right handed orthonormal frame.
void testLocalFrameIsOrthonormal()
{
    const double latitudes[] = {-60.0, -12.0, 0.0, 33.3, 48.1372, 71.0};
    const double longitudes[] = {-150.0, -20.0, 0.0, 11.5756, 100.0, 179.0};

    for (double lat : latitudes) {
        for (double lon : longitudes) {
            const GeoLocation location(lat, lon);
            const Vector3 e = location.east();
            const Vector3 n = location.north();
            const Vector3 u = location.up();

            CHECK_NEAR(e.length(), 1.0, 1e-12);
            CHECK_NEAR(n.length(), 1.0, 1e-12);
            CHECK_NEAR(u.length(), 1.0, 1e-12);

            CHECK_NEAR(e.dot(n), 0.0, 1e-12);
            CHECK_NEAR(e.dot(u), 0.0, 1e-12);
            CHECK_NEAR(n.dot(u), 0.0, 1e-12);

            // Right handed: east x north = up.
            const Vector3 cross = e.cross(n);
            CHECK_NEAR(cross.x, u.x, 1e-12);
            CHECK_NEAR(cross.y, u.y, 1e-12);
            CHECK_NEAR(cross.z, u.z, 1e-12);
        }
    }
}

void testLocalFrameOrientation()
{
    // At latitude 0 / longitude 0 the frame has a simple closed form.
    const GeoLocation origin(0.0, 0.0);

    const Vector3 up = origin.up();
    CHECK_NEAR(up.x, 1.0, 1e-12);
    CHECK_NEAR(up.y, 0.0, 1e-12);
    CHECK_NEAR(up.z, 0.0, 1e-12);

    const Vector3 east = origin.east();
    CHECK_NEAR(east.x, 0.0, 1e-12);
    CHECK_NEAR(east.y, 1.0, 1e-12);
    CHECK_NEAR(east.z, 0.0, 1e-12);

    const Vector3 north = origin.north();
    CHECK_NEAR(north.x, 0.0, 1e-12);
    CHECK_NEAR(north.y, 0.0, 1e-12);
    CHECK_NEAR(north.z, 1.0, 1e-12);

    // At the north pole "up" points along +z.
    const Vector3 poleUp = GeoLocation(90.0, 0.0).up();
    CHECK_NEAR(poleUp.z, 1.0, 1e-9);
}

/// "Up" must be the outward normal, i.e. roughly parallel to the position.
void testUpPointsOutward()
{
    for (double lat : {-70.0, -20.0, 0.0, 25.0, 48.1372, 85.0}) {
        const GeoLocation location(lat, 42.0);
        const Vector3 position = location.toEcef();
        CHECK_TRUE(location.up().dot(position.normalized()) > 0.99);
    }
}

void testAltitudeMovesAlongUp()
{
    const GeoLocation ground(kMunichLat, kMunichLon, 0.0);
    const GeoLocation raised(kMunichLat, kMunichLon, 250.0);
    const Vector3 delta = raised.toEcef() - ground.toEcef();

    CHECK_NEAR(delta.length(), 250.0, 1e-6);
    // The offset is parallel to the local vertical.
    CHECK_NEAR(delta.normalized().dot(ground.up()), 1.0, 1e-9);
}

void testDistanceToKnownPairs()
{
    const GeoLocation munich(kMunichLat, kMunichLon);
    const GeoLocation berlin(52.5200, 13.4050);
    const GeoLocation paris(48.8566, 2.3522);

    // Munich - Berlin is about 504 km, Munich - Paris about 684 km.
    CHECK_NEAR(munich.distanceTo(berlin), 504000.0, 8000.0);
    CHECK_NEAR(munich.distanceTo(paris), 684000.0, 10000.0);

    // Symmetry and identity.
    CHECK_NEAR(munich.distanceTo(berlin), berlin.distanceTo(munich), 1e-6);
    CHECK_NEAR(munich.distanceTo(munich), 0.0, 1e-6);
}

void testDistanceAlongEquatorAndMeridian()
{
    // A quarter of the way around the equator.
    const double quarter = GeoLocation(0.0, 0.0).distanceTo(GeoLocation(0.0, 90.0));
    CHECK_NEAR(quarter, 10018000.0, 60000.0);

    // One degree of latitude is roughly 111 km.
    const double oneDegree = GeoLocation(0.0, 0.0).distanceTo(GeoLocation(1.0, 0.0));
    CHECK_NEAR(oneDegree, 111000.0, 1500.0);

    // Pole to pole is half the circumference.
    const double poleToPole = GeoLocation(-90.0, 0.0).distanceTo(GeoLocation(90.0, 0.0));
    CHECK_NEAR(poleToPole, 20015000.0, 120000.0);
}

void testDistanceTriangleInequality()
{
    const GeoLocation a(48.1372, 11.5756);
    const GeoLocation b(52.5200, 13.4050);
    const GeoLocation c(41.9028, 12.4964);
    CHECK_TRUE(a.distanceTo(c) <= a.distanceTo(b) + b.distanceTo(c) + 1e-6);
}

void testGroundPlaneFactory()
{
    const GeoLocation location(kMunichLat, kMunichLon, 519.0);
    const GroundPlane plane = location.groundPlane();

    CHECK_NEAR(plane.origin().latitude(), kMunichLat, 1e-12);
    CHECK_NEAR(plane.origin().longitude(), kMunichLon, 1e-12);

    // The plane axes match the local frame of the location.
    CHECK_NEAR(plane.normal().dot(location.up()), 1.0, 1e-12);
    CHECK_NEAR(plane.east().dot(location.east()), 1.0, 1e-12);
    CHECK_NEAR(plane.north().dot(location.north()), 1.0, 1e-12);
}

} // namespace

int main()
{
    testAccessors();
    testDefaultModelIsWgs84();
    testExplicitModel();
    testToEcefMatchesModel();
    testLocalFrameIsOrthonormal();
    testLocalFrameOrientation();
    testUpPointsOutward();
    testAltitudeMovesAlongUp();
    testDistanceToKnownPairs();
    testDistanceAlongEquatorAndMeridian();
    testDistanceTriangleInequality();
    testGroundPlaneFactory();
    return geotest::summarize("GeoLocationTests");
}
