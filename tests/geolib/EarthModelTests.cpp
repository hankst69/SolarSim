#include "geolib/EarthModel.h"

#include "TestSupport.h"

#include "geolib/Angle.h"

#include <cmath>

using namespace geo;

namespace {

void testSphereParameters()
{
    const SphericalEarthModel sphere;
    CHECK_NEAR(sphere.equatorialRadius(), SphericalEarthModel::kMeanRadiusM, 1e-9);
    CHECK_NEAR(sphere.polarRadius(), SphericalEarthModel::kMeanRadiusM, 1e-9);
    CHECK_NEAR(sphere.flattening(), 0.0, 1e-15);

    // All curvature radii are constant on a sphere.
    for (double lat : {-90.0, -45.0, 0.0, 23.5, 48.1372, 90.0}) {
        CHECK_NEAR(sphere.localRadius(lat), SphericalEarthModel::kMeanRadiusM, 1e-6);
        CHECK_NEAR(sphere.meridionalRadius(lat), SphericalEarthModel::kMeanRadiusM, 1e-6);
        CHECK_NEAR(sphere.primeVerticalRadius(lat), SphericalEarthModel::kMeanRadiusM, 1e-6);
        CHECK_NEAR(sphere.geocentricRadius(lat), SphericalEarthModel::kMeanRadiusM, 1e-6);
        // Geodetic and geocentric latitude coincide on a sphere.
        CHECK_NEAR(sphere.geocentricLatitude(lat), lat, 1e-9);
    }
}

void testCustomSphereRadius()
{
    const SphericalEarthModel sphere(1000.0);
    CHECK_NEAR(sphere.equatorialRadius(), 1000.0, 1e-9);
    CHECK_NEAR(sphere.localRadius(45.0), 1000.0, 1e-9);
}

void testWgs84Parameters()
{
    const WGS84EarthModel wgs84;
    CHECK_NEAR(wgs84.equatorialRadius(), 6378137.0, 1e-6);
    CHECK_NEAR(wgs84.flattening(), 1.0 / 298.257223563, 1e-15);
    // Semi minor axis b = a * (1 - f) = 6356752.314245...
    CHECK_NEAR(wgs84.polarRadius(), 6356752.314245, 1e-5);
    CHECK_TRUE(wgs84.polarRadius() < wgs84.equatorialRadius());
}

/// Published WGS84 curvature radii at the equator and the pole.
void testWgs84CurvatureRadii()
{
    const WGS84EarthModel wgs84;

    // At the equator: N = a, M = a * (1 - e^2) = b^2 / a.
    CHECK_NEAR(wgs84.primeVerticalRadius(0.0), 6378137.0, 1e-3);
    CHECK_NEAR(wgs84.meridionalRadius(0.0), 6335439.327, 1e-2);

    // At the pole both equal a^2 / b = 6399593.626.
    CHECK_NEAR(wgs84.primeVerticalRadius(90.0), 6399593.626, 1e-2);
    CHECK_NEAR(wgs84.meridionalRadius(90.0), 6399593.626, 1e-2);

    // The prime vertical radius always exceeds the meridional one.
    for (double lat : {0.0, 15.0, 45.0, 60.0, 89.0}) {
        CHECK_TRUE(wgs84.primeVerticalRadius(lat) >= wgs84.meridionalRadius(lat));
    }

    // Gaussian mean radius sqrt(M * N) lies between the two.
    for (double lat : {0.0, 45.0, 80.0}) {
        const double m = wgs84.meridionalRadius(lat);
        const double n = wgs84.primeVerticalRadius(lat);
        const double local = wgs84.localRadius(lat);
        CHECK_NEAR(local, std::sqrt(m * n), 1e-6);
        CHECK_TRUE(local >= m && local <= n);
    }
}

void testWgs84GeocentricRadius()
{
    const WGS84EarthModel wgs84;
    // At the equator the geocentric radius is the semi major axis, at the pole
    // the semi minor axis.
    CHECK_NEAR(wgs84.geocentricRadius(0.0), wgs84.equatorialRadius(), 1e-3);
    CHECK_NEAR(wgs84.geocentricRadius(90.0), wgs84.polarRadius(), 1e-3);

    // Monotonically decreasing from equator to pole.
    double previous = wgs84.geocentricRadius(0.0);
    for (double lat = 10.0; lat <= 90.0; lat += 10.0) {
        const double current = wgs84.geocentricRadius(lat);
        CHECK_TRUE(current < previous);
        previous = current;
    }
}

void testWgs84GeocentricLatitude()
{
    const WGS84EarthModel wgs84;
    // Coincides with the geodetic latitude at the equator and the poles.
    CHECK_NEAR(wgs84.geocentricLatitude(0.0), 0.0, 1e-9);
    CHECK_NEAR(wgs84.geocentricLatitude(90.0), 90.0, 1e-6);
    CHECK_NEAR(wgs84.geocentricLatitude(-90.0), -90.0, 1e-6);

    // In between the geocentric latitude is smaller in magnitude; the maximum
    // difference of about 0.19 degrees occurs near 45 degrees.
    CHECK_TRUE(wgs84.geocentricLatitude(45.0) < 45.0);
    CHECK_NEAR(wgs84.geocentricLatitude(45.0), 44.8076, 1e-3);
    CHECK_TRUE(wgs84.geocentricLatitude(-45.0) > -45.0);

    // Odd symmetry.
    CHECK_NEAR(wgs84.geocentricLatitude(30.0), -wgs84.geocentricLatitude(-30.0), 1e-12);
}

void testToEcefAxes()
{
    const WGS84EarthModel wgs84;

    // Latitude 0, longitude 0 lies on the x axis at distance a.
    const Vector3 origin = wgs84.toEcef(0.0, 0.0, 0.0);
    CHECK_NEAR(origin.x, wgs84.equatorialRadius(), 1e-3);
    CHECK_NEAR(origin.y, 0.0, 1e-6);
    CHECK_NEAR(origin.z, 0.0, 1e-6);

    // Longitude 90 rotates onto the y axis.
    const Vector3 east = wgs84.toEcef(0.0, 90.0, 0.0);
    CHECK_NEAR(east.x, 0.0, 1e-3);
    CHECK_NEAR(east.y, wgs84.equatorialRadius(), 1e-3);
    CHECK_NEAR(east.z, 0.0, 1e-6);

    // The north pole lies on the z axis at distance b.
    const Vector3 pole = wgs84.toEcef(90.0, 0.0, 0.0);
    CHECK_NEAR(pole.x, 0.0, 1e-3);
    CHECK_NEAR(pole.y, 0.0, 1e-3);
    CHECK_NEAR(pole.z, wgs84.polarRadius(), 1e-3);
}

void testToEcefAltitude()
{
    const WGS84EarthModel wgs84;
    // At the equator the altitude adds directly to the x coordinate.
    const Vector3 ground = wgs84.toEcef(0.0, 0.0, 0.0);
    const Vector3 raised = wgs84.toEcef(0.0, 0.0, 1000.0);
    CHECK_NEAR(raised.x - ground.x, 1000.0, 1e-6);

    // In general the altitude moves the point along the local vertical, so the
    // distance from the centre grows by exactly the altitude at the pole too.
    const Vector3 polePlus = wgs84.toEcef(90.0, 0.0, 500.0);
    CHECK_NEAR(polePlus.z - wgs84.polarRadius(), 500.0, 1e-6);
}

/// Distance from the centre must agree with geocentricRadius().
void testEcefMatchesGeocentricRadius()
{
    const WGS84EarthModel wgs84;
    for (double lat : {-60.0, -30.0, 0.0, 12.5, 48.1372, 70.0}) {
        const Vector3 p = wgs84.toEcef(lat, 25.0, 0.0);
        CHECK_NEAR(p.length(), wgs84.geocentricRadius(lat), 1e-3);
    }
}

/// The ECEF latitude implied by z / |p| is the geocentric latitude.
void testEcefImpliesGeocentricLatitude()
{
    const WGS84EarthModel wgs84;
    for (double lat : {10.0, 45.0, 48.1372, 75.0}) {
        const Vector3 p = wgs84.toEcef(lat, 0.0, 0.0);
        const double implied = radToDeg(std::asin(p.z / p.length()));
        CHECK_NEAR(implied, wgs84.geocentricLatitude(lat), 1e-6);
    }
}

void testSphereEcef()
{
    const SphericalEarthModel sphere;
    for (double lat : {-45.0, 0.0, 30.0, 80.0}) {
        for (double lon : {-120.0, 0.0, 45.0, 179.0}) {
            const Vector3 p = sphere.toEcef(lat, lon, 0.0);
            CHECK_NEAR(p.length(), SphericalEarthModel::kMeanRadiusM, 1e-6);
        }
    }
}

void testSharedModels()
{
    // The default model is the WGS84 ellipsoid.
    CHECK_NEAR(EarthModel::defaultModel().flattening(), 1.0 / 298.257223563, 1e-15);
    CHECK_NEAR(EarthModel::defaultModel().equatorialRadius(), 6378137.0, 1e-6);

    // The spherical model has no flattening.
    CHECK_NEAR(EarthModel::sphericalModel().flattening(), 0.0, 1e-15);

    // Both accessors return the same instance every time.
    CHECK_TRUE(&EarthModel::defaultModel() == &EarthModel::defaultModel());
    CHECK_TRUE(&EarthModel::sphericalModel() == &EarthModel::sphericalModel());
    CHECK_TRUE(&EarthModel::defaultModel() != &EarthModel::sphericalModel());
}

/// Sphere and ellipsoid must stay within a few kilometres of each other.
void testModelsAreComparable()
{
    const double diff = std::fabs(EarthModel::defaultModel().geocentricRadius(45.0) -
                                  EarthModel::sphericalModel().geocentricRadius(45.0));
    CHECK_TRUE(diff < 15000.0);
}

} // namespace

int main()
{
    testSphereParameters();
    testCustomSphereRadius();
    testWgs84Parameters();
    testWgs84CurvatureRadii();
    testWgs84GeocentricRadius();
    testWgs84GeocentricLatitude();
    testToEcefAxes();
    testToEcefAltitude();
    testEcefMatchesGeocentricRadius();
    testEcefImpliesGeocentricLatitude();
    testSphereEcef();
    testSharedModels();
    testModelsAreComparable();
    return geotest::summarize("EarthModelTests");
}
