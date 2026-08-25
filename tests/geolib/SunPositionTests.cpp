#include "geolib/SunPosition.h"

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

void testAccessors()
{
    const DateTimeUtc time(2024, 6, 21, 11, 0, 0.0);
    const SunPosition sun(munich(), time);
    CHECK_NEAR(sun.location().latitude(), kMunichLat, 1e-12);
    CHECK_EQ_INT(sun.time().year, 2024);
    CHECK_EQ_INT(sun.time().month, 6);
    CHECK_EQ_INT(sun.time().day, 21);
}

/// The declination follows the seasons: +23.44 at the June solstice,
/// -23.44 at the December solstice and about zero at the equinoxes.
void testDeclinationOverTheYear()
{
    CHECK_NEAR(SunPosition(munich(), DateTimeUtc(2024, 6, 21, 12, 0, 0.0)).declination(), 23.44,
               0.1);
    CHECK_NEAR(SunPosition(munich(), DateTimeUtc(2024, 12, 21, 12, 0, 0.0)).declination(), -23.44,
               0.1);
    CHECK_NEAR(SunPosition(munich(), DateTimeUtc(2024, 3, 20, 12, 0, 0.0)).declination(), 0.0,
               0.5);
    CHECK_NEAR(SunPosition(munich(), DateTimeUtc(2024, 9, 22, 12, 0, 0.0)).declination(), 0.0,
               0.5);
}

/// At local solar noon the elevation reaches 90 - lat + declination.
void testNoonElevation()
{
    // Munich is at UTC+1 in winter terms; solar noon is near 11:07 UTC.
    const SunPosition summer(munich(), DateTimeUtc(2024, 6, 21, 11, 15, 0.0));
    CHECK_NEAR(summer.elevation(), 90.0 - kMunichLat + 23.44, 0.5);

    const SunPosition winter(munich(), DateTimeUtc(2024, 12, 21, 11, 15, 0.0));
    CHECK_NEAR(winter.elevation(), 90.0 - kMunichLat - 23.44, 0.5);

    // The sun is due south around noon in the northern hemisphere.
    CHECK_NEAR(summer.azimuth(), 180.0, 3.0);
}

void testEquatorNoon()
{
    // At the equator on an equinox the sun passes almost through the zenith.
    const SunPosition sun(GeoLocation(0.0, 0.0), DateTimeUtc(2024, 3, 20, 12, 7, 0.0));
    CHECK_TRUE(sun.elevation() > 88.0);
}

void testNightIsBelowHorizon()
{
    const SunPosition midnight(munich(), DateTimeUtc(2024, 6, 21, 23, 0, 0.0));
    CHECK_TRUE(midnight.elevation() < 0.0);
    CHECK_FALSE(midnight.isAboveHorizon());
    CHECK_NEAR(midnight.relativeIrradiance(), 0.0, 1e-12);

    const SunPosition noon(munich(), DateTimeUtc(2024, 6, 21, 11, 15, 0.0));
    CHECK_TRUE(noon.isAboveHorizon());
    CHECK_TRUE(noon.relativeIrradiance() > 0.0);
}

/// Polar day and polar night above the arctic circle.
void testPolarDayAndNight()
{
    const GeoLocation tromso(69.6496, 18.9560);
    // Midsummer midnight sun.
    for (int hour = 0; hour < 24; hour += 4) {
        const SunPosition sun(tromso, DateTimeUtc(2024, 6, 21, hour, 0, 0.0));
        CHECK_TRUE(sun.elevation() > 0.0);
    }
    // Midwinter polar night.
    for (int hour = 0; hour < 24; hour += 4) {
        const SunPosition sun(tromso, DateTimeUtc(2024, 12, 21, hour, 0, 0.0));
        CHECK_TRUE(sun.elevation() < 0.0);
    }
}

void testZenithAngle()
{
    const SunPosition sun(munich(), DateTimeUtc(2024, 6, 21, 11, 15, 0.0));
    CHECK_NEAR(sun.zenithAngle(), 90.0 - sun.elevation(), 1e-12);
}

void testAzimuthRange()
{
    for (int hour = 0; hour < 24; ++hour) {
        const SunPosition sun(munich(), DateTimeUtc(2024, 6, 21, hour, 0, 0.0));
        CHECK_TRUE(sun.azimuth() >= 0.0 && sun.azimuth() < 360.0 + 1e-9);
        CHECK_TRUE(sun.elevation() >= -90.0 && sun.elevation() <= 90.0);
    }
}

/// The sun moves from east through south to west during the day.
void testAzimuthProgression()
{
    const SunPosition morning(munich(), DateTimeUtc(2024, 6, 21, 6, 0, 0.0));
    const SunPosition noon(munich(), DateTimeUtc(2024, 6, 21, 11, 15, 0.0));
    const SunPosition evening(munich(), DateTimeUtc(2024, 6, 21, 17, 0, 0.0));

    CHECK_TRUE(morning.azimuth() < noon.azimuth());
    CHECK_TRUE(noon.azimuth() < evening.azimuth());
    CHECK_TRUE(morning.azimuth() > 45.0 && morning.azimuth() < 120.0);  // east-ish
    CHECK_TRUE(evening.azimuth() > 240.0 && evening.azimuth() < 315.0); // west-ish
}

void testRefraction()
{
    // Refraction lifts the apparent position; the effect is largest near the
    // horizon (about 0.5 degrees) and negligible near the zenith.
    const SunPosition low(munich(), DateTimeUtc(2024, 3, 20, 5, 30, 0.0));
    CHECK_TRUE(low.refractedElevation() > low.elevation());
    CHECK_TRUE(low.refractedElevation() - low.elevation() < 1.0);

    const SunPosition high(munich(), DateTimeUtc(2024, 6, 21, 11, 15, 0.0));
    CHECK_TRUE(high.refractedElevation() - high.elevation() < 0.05);
}

void testEquationOfTime()
{
    // The equation of time stays within about +/- 17 minutes and has its well
    // known extremes in February (about -14) and November (about +16).
    for (int month = 1; month <= 12; ++month) {
        const SunPosition sun(munich(), DateTimeUtc(2024, month, 15, 12, 0, 0.0));
        CHECK_TRUE(std::fabs(sun.equationOfTime()) < 18.0);
    }
    CHECK_NEAR(SunPosition(munich(), DateTimeUtc(2024, 2, 11, 12, 0, 0.0)).equationOfTime(),
               -14.2, 1.5);
    CHECK_NEAR(SunPosition(munich(), DateTimeUtc(2024, 11, 3, 12, 0, 0.0)).equationOfTime(), 16.4,
               1.5);
}

void testSunDistance()
{
    // The earth is closest in early January and furthest in early July.
    const double perihelion =
        SunPosition(munich(), DateTimeUtc(2024, 1, 3, 12, 0, 0.0)).sunDistanceAu();
    const double aphelion =
        SunPosition(munich(), DateTimeUtc(2024, 7, 5, 12, 0, 0.0)).sunDistanceAu();

    CHECK_NEAR(perihelion, 0.9833, 0.002);
    CHECK_NEAR(aphelion, 1.0167, 0.002);
    CHECK_TRUE(perihelion < aphelion);
}

void testHourAngle()
{
    // The hour angle passes through zero at solar noon and grows westwards.
    const SunPosition noon(munich(), DateTimeUtc(2024, 6, 21, 11, 15, 0.0));
    CHECK_NEAR(noon.hourAngle(), 0.0, 3.0);

    const SunPosition afternoon(munich(), DateTimeUtc(2024, 6, 21, 14, 15, 0.0));
    CHECK_TRUE(afternoon.hourAngle() > noon.hourAngle());
    // Three hours correspond to about 45 degrees.
    CHECK_NEAR(afternoon.hourAngle() - noon.hourAngle(), 45.0, 1.5);
}

/// direction() must be a unit vector consistent with azimuth and elevation.
void testDirection()
{
    const SunPosition sun(munich(), DateTimeUtc(2024, 6, 21, 9, 0, 0.0));
    const Vector3 direction = sun.direction();
    CHECK_NEAR(direction.length(), 1.0, 1e-9);

    const double elevationRad = std::asin(direction.z);
    CHECK_NEAR(radToDeg(elevationRad), sun.elevation(), 1e-6);

    // Azimuth clockwise from north: x = east, y = north.
    double azimuth = radToDeg(std::atan2(direction.x, direction.y));
    if (azimuth < 0.0) {
        azimuth += 360.0;
    }
    CHECK_NEAR(azimuth, sun.azimuth(), 1e-6);
}

void testRelativeIrradiance()
{
    const SunPosition noon(munich(), DateTimeUtc(2024, 6, 21, 11, 15, 0.0));
    const double expected = std::cos(degToRad(noon.zenithAngle()));
    CHECK_NEAR(noon.relativeIrradiance(), expected, 1e-9);
    CHECK_TRUE(noon.relativeIrradiance() > 0.0 && noon.relativeIrradiance() <= 1.0);

    // Summer noon must beat winter noon.
    const SunPosition winter(munich(), DateTimeUtc(2024, 12, 21, 11, 15, 0.0));
    CHECK_TRUE(noon.relativeIrradiance() > winter.relativeIrradiance());
}

void testProjectOnDome()
{
    const HorizonDome dome(munich());

    // Sun above the horizon: the point lies on the dome surface.
    const SunPosition noon(munich(), DateTimeUtc(2024, 6, 21, 11, 15, 0.0));
    const Vector3 point = noon.projectOnDome(dome);
    CHECK_NEAR(point.length(), dome.radius(), 1e-6);
    CHECK_TRUE(point.z > 0.0);

    // Sun below the horizon: clamped to the rim.
    const SunPosition midnight(munich(), DateTimeUtc(2024, 6, 21, 23, 30, 0.0));
    const Vector3 rim = midnight.projectOnDome(dome);
    CHECK_NEAR(rim.length(), dome.radius(), 1e-6);
    CHECK_NEAR(rim.z, 0.0, 1e-6);
}

/// The southern hemisphere sees the sun in the north at noon.
void testSouthernHemisphere()
{
    const GeoLocation sydney(-33.8688, 151.2093);
    // Solar noon in Sydney is around 02:00 UTC.
    const SunPosition sun(sydney, DateTimeUtc(2024, 12, 21, 1, 0, 0.0));
    CHECK_TRUE(sun.elevation() > 0.0);
    // Azimuth near north (0/360) rather than south.
    CHECK_TRUE(sun.azimuth() < 90.0 || sun.azimuth() > 270.0);
}

} // namespace

int main()
{
    testAccessors();
    testDeclinationOverTheYear();
    testNoonElevation();
    testEquatorNoon();
    testNightIsBelowHorizon();
    testPolarDayAndNight();
    testZenithAngle();
    testAzimuthRange();
    testAzimuthProgression();
    testRefraction();
    testEquationOfTime();
    testSunDistance();
    testHourAngle();
    testDirection();
    testRelativeIrradiance();
    testProjectOnDome();
    testSouthernHemisphere();
    return geotest::summarize("SunPositionTests");
}
