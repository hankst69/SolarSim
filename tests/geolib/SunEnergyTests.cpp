#include "geolib/SunEnergy.h"

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

/// The earth is closest to the sun in early January and farthest in early July,
/// which modulates the theoretical maximum by about +-3.3%.
void testTheoreticalIrradianceOverTheYear()
{
    const SunEnergy perihelion(DateTimeUtc(2024, 1, 3));
    const SunEnergy aphelion(DateTimeUtc(2024, 7, 5));

    CHECK_NEAR(perihelion.sunDistanceAu(), 0.9833, 0.001);
    CHECK_NEAR(aphelion.sunDistanceAu(), 1.0167, 0.001);

    CHECK_NEAR(perihelion.theoreticalIrradiance(), 1408.0, 6.0);
    CHECK_NEAR(aphelion.theoreticalIrradiance(), 1316.0, 6.0);
    CHECK_TRUE(perihelion.theoreticalIrradiance() > aphelion.theoreticalIrradiance());

    // Inverse square law of the distance for that date.
    const double d = perihelion.sunDistanceAu();
    CHECK_NEAR(perihelion.theoreticalIrradiance(), SunEnergy::kSolarConstant / (d * d), 1e-9);
}

void testDateOnlyHasNoLocationValues()
{
    const SunEnergy energy(DateTimeUtc(2024, 6, 21, 12, 0, 0.0));
    CHECK_FALSE(energy.hasLocation());
    CHECK_NEAR(energy.directNormalIrradiance(), 0.0, 1e-12);
    CHECK_NEAR(energy.groundIrradiance(), 0.0, 1e-12);
}

/// With a location the theoretical value is unchanged, the realistic values are
/// damped by the atmosphere and the cosine of the incidence angle.
void testRealisticValuesAreBelowTheoretical()
{
    const SunEnergy energy(munich(), DateTimeUtc(2024, 6, 21, 11, 0, 0.0));
    CHECK_TRUE(energy.hasLocation());
    CHECK_NEAR(energy.theoreticalIrradiance(), SunEnergy::kSolarConstant
                   / (energy.sunDistanceAu() * energy.sunDistanceAu()), 1e-9);

    CHECK_TRUE(energy.directNormalIrradiance() > 0.0);
    CHECK_TRUE(energy.directNormalIrradiance() < energy.theoreticalIrradiance());
    CHECK_TRUE(energy.groundIrradiance() > 0.0);
    CHECK_TRUE(energy.groundIrradiance() < energy.directNormalIrradiance());
}

/// The air mass is 1 for a sun in the zenith and grows towards the horizon.
void testAirMassAndTransmittance()
{
    const SunEnergy noon(munich(), DateTimeUtc(2024, 6, 21, 11, 0, 0.0));
    const SunEnergy evening(munich(), DateTimeUtc(2024, 6, 21, 18, 30, 0.0));

    CHECK_TRUE(noon.airMass() >= 1.0);
    CHECK_TRUE(noon.airMass() < 1.5);
    CHECK_TRUE(evening.airMass() > noon.airMass());
    CHECK_TRUE(evening.atmosphericTransmittance() < noon.atmosphericTransmittance());
    CHECK_TRUE(noon.atmosphericTransmittance() < 1.0);

    // Sun in the zenith: air mass 1 and the plain zenith transmittance.
    const SunEnergy tropic(GeoLocation(23.44, 0.0), DateTimeUtc(2024, 6, 21, 12, 0, 0.0));
    CHECK_NEAR(tropic.airMass(), 1.0, 0.02);
    CHECK_NEAR(tropic.atmosphericTransmittance(), SunEnergy::kZenithTransmittance, 0.02);
}

void testNightHasNoEnergy()
{
    const SunEnergy night(munich(), DateTimeUtc(2024, 12, 21, 2, 0, 0.0));
    CHECK_FALSE(night.sunPosition().isAboveHorizon());
    CHECK_NEAR(night.airMass(), 0.0, 1e-12);
    CHECK_NEAR(night.atmosphericTransmittance(), 0.0, 1e-12);
    CHECK_NEAR(night.directNormalIrradiance(), 0.0, 1e-12);
    CHECK_NEAR(night.groundIrradiance(), 0.0, 1e-12);
}

/// A plane facing the sun collects the full direct normal irradiance, a plane
/// turned away collects nothing.
void testInclinedPlane()
{
    const SunEnergy energy(munich(), DateTimeUtc(2024, 6, 21, 11, 0, 0.0));
    const SunPosition& sun = energy.sunPosition();

    const double facing =
        energy.irradianceOnTiltedPlane(sun.zenithAngle(), sun.azimuth());
    CHECK_NEAR(facing, energy.directNormalIrradiance(), 1e-6);

    const double away = energy.irradianceOnTiltedPlane(90.0, sun.azimuth() + 180.0);
    CHECK_NEAR(away, 0.0, 1e-9);

    // A horizontal plane matches the generic ground irradiance.
    CHECK_NEAR(energy.irradianceOnTiltedPlane(0.0, 0.0), energy.groundIrradiance(), 1e-9);

    // A south facing 30 degree roof beats the horizontal plane in winter.
    const SunEnergy winter(munich(), DateTimeUtc(2024, 12, 21, 11, 0, 0.0));
    CHECK_TRUE(winter.irradianceOnTiltedPlane(30.0, 180.0) > winter.groundIrradiance());
}

void testGroundPlaneOverload()
{
    const SunEnergy energy(munich(), DateTimeUtc(2024, 6, 21, 11, 0, 0.0));
    const GroundPlane plane = munich().groundPlane();
    CHECK_NEAR(energy.irradianceOnGroundPlane(plane), energy.groundIrradiance(), 1e-6);
}

} // namespace

int main()
{
    testTheoreticalIrradianceOverTheYear();
    testDateOnlyHasNoLocationValues();
    testRealisticValuesAreBelowTheoretical();
    testAirMassAndTransmittance();
    testNightHasNoEnergy();
    testInclinedPlane();
    testGroundPlaneOverload();
    return geotest::summarize("SunEnergyTests");
}
