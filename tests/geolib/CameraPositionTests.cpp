#include "geolib/CameraPosition.h"

#include "TestSupport.h"

#include "geolib/Angle.h"

#include <cmath>

using namespace geo;

namespace {

constexpr double kMunichLat = 48.1372;
constexpr double kMunichLon = 11.5756;

GeoLocation munich()
{
    return GeoLocation(kMunichLat, kMunichLon);
}

GeoLocation sydney()
{
    return GeoLocation(-33.8688, 151.2093);
}

/// On the northern hemisphere the camera stands 50 m to the south, 30 m up.
void testInitialNorthernHemisphere()
{
    const HorizonDome dome(munich());
    const CameraPosition camera = CameraPosition::initial(dome);

    CHECK_NEAR(camera.localPosition().x, 0.0, 1e-6);
    CHECK_NEAR(camera.localPosition().y, -CameraPosition::kDefaultDistanceM, 1e-6);
    CHECK_NEAR(camera.heightAboveTerrain(), CameraPosition::kDefaultHeightM, 1e-12);
    CHECK_NEAR(camera.horizontalDistance(), CameraPosition::kDefaultDistanceM, 1e-6);
    CHECK_NEAR(camera.azimuth(), 180.0, 1e-6);

    // The curvature drop over 50 m is negligible but not exactly zero.
    CHECK_NEAR(camera.localPosition().z, CameraPosition::kDefaultHeightM, 1e-3);
    CHECK_TRUE(camera.localPosition().z <= CameraPosition::kDefaultHeightM);
}

/// On the southern hemisphere the camera stands to the north.
void testInitialSouthernHemisphere()
{
    const CameraPosition camera = CameraPosition::initial(HorizonDome(sydney()));

    CHECK_NEAR(camera.localPosition().x, 0.0, 1e-6);
    CHECK_NEAR(camera.localPosition().y, CameraPosition::kDefaultDistanceM, 1e-6);
    CHECK_NEAR(camera.azimuth(), 0.0, 1e-6);
}

/// The defaults are 30 m height and 50 m distance, both are configurable.
void testInitialCustomHeightAndDistance()
{
    CHECK_NEAR(CameraPosition::kDefaultHeightM, 30.0, 1e-12);
    CHECK_NEAR(CameraPosition::kDefaultDistanceM, 50.0, 1e-12);

    const CameraPosition camera = CameraPosition::initial(HorizonDome(munich()), 10.0, 100.0);
    CHECK_NEAR(camera.horizontalDistance(), 100.0, 1e-6);
    CHECK_NEAR(camera.heightAboveTerrain(), 10.0, 1e-12);
    CHECK_NEAR(camera.localPosition().z, 10.0, 1e-2);
}

/// The camera height is measured above the terrain, not above sea level.
void testHeightAboveTerrain()
{
    const HorizonDome dome(GeoLocation(kMunichLat, kMunichLon), 1.6, 520.0);
    const CameraPosition camera = CameraPosition::initial(dome);

    CHECK_NEAR(camera.heightAboveTerrain(), CameraPosition::kDefaultHeightM, 1e-12);
    // Ground plane origin is at sea level here, so the local height contains
    // the terrain height as well.
    CHECK_NEAR(camera.localPosition().z, 520.0 + CameraPosition::kDefaultHeightM, 1e-2);
}

/// The camera looks back at the dome centre.
void testViewDirection()
{
    const CameraPosition camera = CameraPosition::initial(HorizonDome(munich()));
    const Vector3 view = camera.viewDirection();

    CHECK_NEAR(view.length(), 1.0, 1e-12);
    CHECK_TRUE(view.y > 0.0); // towards north, i.e. towards the centre
    CHECK_TRUE(view.z < 0.0); // looking down
    CHECK_NEAR(camera.elevation(), radToDeg(std::asin(-view.z)), 1e-6);
}

/// The target of the camera is the standpoint of the dome and the geodetic
/// position of the camera is offset from it.
void testTargetAndGeoLocation()
{
    const CameraPosition camera = CameraPosition::initial(HorizonDome(munich()));

    CHECK_NEAR(camera.target().latitude(), kMunichLat, 1e-12);
    CHECK_NEAR(camera.target().longitude(), kMunichLon, 1e-12);

    const GeoLocation position = camera.geoLocation();
    CHECK_TRUE(position.latitude() < kMunichLat); // 50 m to the south
    CHECK_NEAR(position.longitude(), kMunichLon, 1e-9);
    CHECK_NEAR(camera.target().distanceTo(position), CameraPosition::kDefaultDistanceM, 0.1);

    // The ECEF position is the local point transformed by the ground plane.
    const Vector3 ecef = camera.ecefPosition();
    CHECK_NEAR(camera.target().groundPlane().toLocal(ecef).y, camera.localPosition().y, 1e-6);
}

/// For a date/time the camera sits on the line standpoint -> sun.
void testForDateTimeUsesSunAzimuth()
{
    const HorizonDome dome(munich());
    const DateTimeUtc noon(2024, 6, 21, 10, 0, 0); // ~solar noon in Munich
    const SunPosition sun(dome.standpoint(), noon);

    const CameraPosition camera = CameraPosition::forDateTime(dome, noon);
    CHECK_NEAR(camera.azimuth(), sun.azimuth(), 1e-6);
    CHECK_NEAR(camera.horizontalDistance(), CameraPosition::kDefaultDistanceM, 1e-6);

    // High sun: the camera follows the elevation and is well above 30 m.
    CHECK_TRUE(sun.elevation() > 45.0);
    CHECK_NEAR(camera.elevation(), sun.elevation(), 1e-3);
    CHECK_TRUE(camera.heightAboveTerrain() > CameraPosition::kDefaultHeightM);
}

/// A low or invisible sun keeps the default camera height.
void testForDateTimeLowSun()
{
    const HorizonDome dome(munich());
    const DateTimeUtc night(2024, 12, 21, 23, 0, 0);
    const SunPosition sun(dome.standpoint(), night);
    CHECK_FALSE(sun.isAboveHorizon());

    const CameraPosition camera = CameraPosition::forDateTime(dome, night);
    CHECK_NEAR(camera.azimuth(), sun.azimuth(), 1e-6);
    CHECK_NEAR(camera.heightAboveTerrain(), CameraPosition::kDefaultHeightM, 1e-12);
    CHECK_TRUE(camera.localPosition().z > 0.0);
}

/// fromOrbit places the camera at the requested azimuth/elevation/range.
/// The curvature drop shifts the camera down by a few millimetres, so the
/// angles and the range are only reproduced approximately.
void testFromOrbit()
{
    const HorizonDome dome(munich());
    const CameraPosition camera = CameraPosition::fromOrbit(dome, 135.0, 30.0, 200.0);

    CHECK_NEAR(camera.azimuth(), 135.0, 1e-6);
    CHECK_NEAR(camera.elevation(), 30.0, 0.01);
    CHECK_NEAR(camera.range(), 200.0, 0.05);
}

/// Orbiting rotates around the target without changing the distance.
void testOrbitedKeepsRange()
{
    const HorizonDome dome(munich());
    const CameraPosition start = CameraPosition::fromOrbit(dome, 180.0, 30.0, 300.0);
    const CameraPosition moved = start.orbited(90.0, 10.0);

    CHECK_NEAR(moved.azimuth(), 270.0, 1e-3);
    CHECK_NEAR(moved.elevation(), 40.0, 0.01);
    CHECK_NEAR(moved.range(), start.range(), 0.05);

    // The azimuth wraps around into [0, 360).
    const CameraPosition wrapped = start.orbited(240.0, 0.0);
    CHECK_NEAR(wrapped.azimuth(), 60.0, 1e-3);
}

/// The elevation stays inside the usable range.
void testOrbitedClampsElevation()
{
    const HorizonDome dome(munich());
    const CameraPosition start = CameraPosition::fromOrbit(dome, 180.0, 30.0, 300.0);

    CHECK_NEAR(start.orbited(0.0, 500.0).elevation(), CameraPosition::kMaxElevationDeg, 0.01);
    CHECK_NEAR(start.orbited(0.0, -500.0).elevation(), CameraPosition::kMinElevationDeg, 0.01);
}

/// Zooming scales the distance and keeps the viewing angles.
void testZoomed()
{
    const HorizonDome dome(munich());
    const CameraPosition start = CameraPosition::fromOrbit(dome, 210.0, 25.0, 400.0);
    const CameraPosition closer = start.zoomed(0.5);

    CHECK_NEAR(closer.range(), 200.0, 0.05);
    CHECK_NEAR(closer.azimuth(), start.azimuth(), 1e-3);
    CHECK_NEAR(closer.elevation(), start.elevation(), 0.01);

    // The range never drops below the minimum.
    CHECK_NEAR(start.withRange(0.0).range(), CameraPosition::kMinRangeM, 0.01);
}

} // namespace

int main()
{
    testInitialNorthernHemisphere();
    testInitialSouthernHemisphere();
    testInitialCustomHeightAndDistance();
    testHeightAboveTerrain();
    testViewDirection();
    testTargetAndGeoLocation();
    testForDateTimeUsesSunAzimuth();
    testForDateTimeLowSun();
    testFromOrbit();
    testOrbitedKeepsRange();
    testOrbitedClampsElevation();
    testZoomed();
    return geotest::summarize("CameraPositionTests");
}
