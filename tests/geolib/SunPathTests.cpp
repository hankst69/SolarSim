#include "geolib/SunPath.h"

#include "TestSupport.h"

#include <cmath>

using namespace geo;

namespace {

constexpr double kMunichLat = 48.1372;
constexpr double kMunichLon = 11.5756;

HorizonDome munichDome()
{
    return HorizonDome(GeoLocation(kMunichLat, kMunichLon));
}

double toMinutes(const DateTimeUtc& t)
{
    return t.hour * 60.0 + t.minute + t.second / 60.0;
}

void testSampleCountAndOrder()
{
    const SunPath path(munichDome(), 2024, 6, 21);
    // sampleCount intervals produce sampleCount + 1 samples.
    CHECK_EQ_INT(static_cast<long long>(path.samples().size()),
                 SunPath::kDefaultSampleCount + 1);

    CHECK_EQ_INT(path.year(), 2024);
    CHECK_EQ_INT(path.month(), 6);
    CHECK_EQ_INT(path.day(), 21);
    CHECK_NEAR(path.location().latitude(), kMunichLat, 1e-12);

    // Samples are strictly ordered in time and span the full day.
    const auto& samples = path.samples();
    for (std::size_t i = 1; i < samples.size(); ++i) {
        CHECK_TRUE(samples[i].time.julianDay() > samples[i - 1].time.julianDay());
    }
    CHECK_NEAR(toMinutes(samples.front().time), 0.0, 1e-6);
    CHECK_NEAR(samples.back().time.julianDay() - samples.front().time.julianDay(), 1.0, 1e-9);
}

void testCustomSampleCount()
{
    const SunPath path(munichDome(), 2024, 6, 21, 24);
    CHECK_EQ_INT(static_cast<long long>(path.samples().size()), 25);
    // One sample per hour.
    CHECK_NEAR(toMinutes(path.samples()[1].time), 60.0, 1e-6);
}

void testVisibleSamplesAndArc()
{
    const SunPath path(munichDome(), 2024, 6, 21);
    const auto visible = path.visibleSamples();

    CHECK_TRUE(!visible.empty());
    CHECK_TRUE(visible.size() < path.samples().size());
    for (const auto& sample : visible) {
        CHECK_TRUE(sample.position.elevation() > 0.0);
    }

    // Midsummer in Munich has roughly 16 hours of daylight.
    const double hours = static_cast<double>(visible.size()) * 10.0 / 60.0;
    CHECK_NEAR(hours, 16.2, 1.0);

    // The arc points correspond one to one with the visible samples.
    const auto arc = path.arcPoints();
    CHECK_EQ_INT(static_cast<long long>(arc.size()),
                 static_cast<long long>(visible.size()));
    for (const auto& point : arc) {
        CHECK_NEAR(point.length(), path.dome().radius(), 1e-6);
        CHECK_TRUE(point.z > -1e-6);
    }
}

void testWinterHasShorterDay()
{
    const SunPath summer(munichDome(), 2024, 6, 21);
    const SunPath winter(munichDome(), 2024, 12, 21);
    CHECK_TRUE(winter.visibleSamples().size() < summer.visibleSamples().size());

    // Around 8.3 hours of daylight at midwinter.
    const double hours = static_cast<double>(winter.visibleSamples().size()) * 10.0 / 60.0;
    CHECK_NEAR(hours, 8.3, 1.0);
}

void testAtMatchesDirectComputation()
{
    const SunPath path(munichDome(), 2024, 6, 21);
    const SunPosition viaPath = path.at(9, 30, 0.0);
    const SunPosition direct(GeoLocation(kMunichLat, kMunichLon),
                               DateTimeUtc(2024, 6, 21, 9, 30, 0.0));
    CHECK_NEAR(viaPath.elevation(), direct.elevation(), 1e-9);
    CHECK_NEAR(viaPath.azimuth(), direct.azimuth(), 1e-9);
}

void testSunriseAndSunset()
{
    const SunPath path(munichDome(), 2024, 6, 21);
    CHECK_TRUE(path.hasSunrise());

    DateTimeUtc rise;
    DateTimeUtc set;
    CHECK_TRUE(path.sunrise(rise));
    CHECK_TRUE(path.sunset(set));

    // Midsummer in Munich: sunrise about 03:15 UTC, sunset about 19:17 UTC.
    CHECK_NEAR(toMinutes(rise), 3.0 * 60.0 + 15.0, 20.0);
    CHECK_NEAR(toMinutes(set), 19.0 * 60.0 + 17.0, 20.0);
    CHECK_TRUE(toMinutes(rise) < toMinutes(set));

    // The date is preserved.
    CHECK_EQ_INT(rise.year, 2024);
    CHECK_EQ_INT(rise.month, 6);
    CHECK_EQ_INT(rise.day, 21);

    // At the crossing the elevation is essentially zero.
    const SunPosition atRise(GeoLocation(kMunichLat, kMunichLon), rise);
    CHECK_NEAR(atRise.elevation(), 0.0, 0.2);
    const SunPosition atSet(GeoLocation(kMunichLat, kMunichLon), set);
    CHECK_NEAR(atSet.elevation(), 0.0, 0.2);
}

void testWinterSunriseIsLater()
{
    const SunPath summer(munichDome(), 2024, 6, 21);
    const SunPath winter(munichDome(), 2024, 12, 21);

    DateTimeUtc summerRise;
    DateTimeUtc winterRise;
    CHECK_TRUE(summer.sunrise(summerRise));
    CHECK_TRUE(winter.sunrise(winterRise));
    CHECK_TRUE(toMinutes(winterRise) > toMinutes(summerRise));
}

/// Polar day and polar night have no horizon crossing at all.
void testPolarDayAndNight()
{
    const HorizonDome tromso(GeoLocation(69.6496, 18.9560));

    const SunPath polarDay(tromso, 2024, 6, 21);
    CHECK_TRUE(polarDay.hasSunrise());
    DateTimeUtc unused;
    CHECK_FALSE(polarDay.sunrise(unused));
    CHECK_FALSE(polarDay.sunset(unused));
    // Every sample is above the horizon.
    CHECK_EQ_INT(static_cast<long long>(polarDay.visibleSamples().size()),
                 static_cast<long long>(polarDay.samples().size()));

    const SunPath polarNight(tromso, 2024, 12, 21);
    CHECK_FALSE(polarNight.hasSunrise());
    CHECK_FALSE(polarNight.sunrise(unused));
    CHECK_TRUE(polarNight.visibleSamples().empty());
    CHECK_TRUE(polarNight.arcPoints().empty());
    CHECK_NEAR(polarNight.relativeDailyEnergy(), 0.0, 1e-12);
}

void testHighestSample()
{
    const SunPath path(munichDome(), 2024, 6, 21);
    const SunPathSample& highest = path.highestSample();

    // No other sample may be higher.
    for (const auto& sample : path.samples()) {
        CHECK_TRUE(sample.position.elevation() <= highest.position.elevation() + 1e-12);
    }

    // Solar noon in Munich is near 11:07 UTC.
    CHECK_NEAR(toMinutes(highest.time), 11.0 * 60.0 + 7.0, 15.0);
    CHECK_NEAR(highest.position.elevation(), 90.0 - kMunichLat + 23.44, 0.6);
}

void testRelativeDailyEnergy()
{
    const SunPath summer(munichDome(), 2024, 6, 21);
    const SunPath winter(munichDome(), 2024, 12, 21);

    CHECK_TRUE(summer.relativeDailyEnergy() > 0.0);
    CHECK_TRUE(winter.relativeDailyEnergy() > 0.0);
    // Far more energy in summer than in winter.
    CHECK_TRUE(summer.relativeDailyEnergy() > 3.0 * winter.relativeDailyEnergy());

    // The value is the irradiance sum times the sample interval in hours.
    double expected = 0.0;
    for (const auto& sample : summer.samples()) {
        expected += sample.position.relativeIrradiance();
    }
    expected *= 10.0 / 60.0;
    CHECK_NEAR(summer.relativeDailyEnergy(), expected, 1e-9);
}

void testDomePointsMatchPositions()
{
    const SunPath path(munichDome(), 2024, 6, 21, 24);
    for (const auto& sample : path.samples()) {
        const Vector3 expected = sample.position.projectOnDome(path.dome());
        CHECK_NEAR(sample.domePoint.x, expected.x, 1e-6);
        CHECK_NEAR(sample.domePoint.y, expected.y, 1e-6);
        CHECK_NEAR(sample.domePoint.z, expected.z, 1e-6);
    }
}

} // namespace

int main()
{
    testSampleCountAndOrder();
    testCustomSampleCount();
    testVisibleSamplesAndArc();
    testWinterHasShorterDay();
    testAtMatchesDirectComputation();
    testSunriseAndSunset();
    testWinterSunriseIsLater();
    testPolarDayAndNight();
    testHighestSample();
    testRelativeDailyEnergy();
    testDomePointsMatchPositions();
    return geotest::summarize("SunPathTests");
}
