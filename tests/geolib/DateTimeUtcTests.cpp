#include "geolib/DateTimeUtc.h"

#include "TestSupport.h"

using namespace geo;

namespace {

void testDefaults()
{
    const DateTimeUtc t;
    CHECK_EQ_INT(t.year, 2000);
    CHECK_EQ_INT(t.month, 1);
    CHECK_EQ_INT(t.day, 1);
    CHECK_EQ_INT(t.hour, 0);
    CHECK_EQ_INT(t.minute, 0);
    CHECK_NEAR(t.second, 0.0, 1e-12);
}

/// Reference Julian days from the standard astronomical tables.
void testKnownJulianDays()
{
    // J2000.0 epoch: 2000-01-01 12:00 UTC.
    CHECK_NEAR(DateTimeUtc(2000, 1, 1, 12, 0, 0.0).julianDay(), 2451545.0, 1e-9);

    // Midnight before it is half a day earlier.
    CHECK_NEAR(DateTimeUtc(2000, 1, 1, 0, 0, 0.0).julianDay(), 2451544.5, 1e-9);

    // Start of the Gregorian calendar reform epoch used by the algorithm.
    CHECK_NEAR(DateTimeUtc(1858, 11, 17, 0, 0, 0.0).julianDay(), 2400000.5, 1e-6);

    // A few further well known dates.
    CHECK_NEAR(DateTimeUtc(1987, 1, 27, 0, 0, 0.0).julianDay(), 2446822.5, 1e-6);
    CHECK_NEAR(DateTimeUtc(2024, 6, 21, 12, 0, 0.0).julianDay(), 2460483.0, 1e-6);
}

void testFractionOfDay()
{
    const double midnight = DateTimeUtc(2024, 3, 15, 0, 0, 0.0).julianDay();
    CHECK_NEAR(DateTimeUtc(2024, 3, 15, 6, 0, 0.0).julianDay() - midnight, 0.25, 1e-9);
    CHECK_NEAR(DateTimeUtc(2024, 3, 15, 12, 0, 0.0).julianDay() - midnight, 0.5, 1e-9);
    CHECK_NEAR(DateTimeUtc(2024, 3, 15, 18, 0, 0.0).julianDay() - midnight, 0.75, 1e-9);

    // One minute and one second expressed as a day fraction. The Julian day is
    // about 2.4e6, so differences are only resolvable to roughly 1e-9.
    CHECK_NEAR(DateTimeUtc(2024, 3, 15, 0, 1, 0.0).julianDay() - midnight, 1.0 / 1440.0, 1e-9);
    CHECK_NEAR(DateTimeUtc(2024, 3, 15, 0, 0, 1.0).julianDay() - midnight, 1.0 / 86400.0, 1e-9);
}

void testConsecutiveDaysDifferByOne()
{
    CHECK_NEAR(DateTimeUtc(2024, 1, 2).julianDay() - DateTimeUtc(2024, 1, 1).julianDay(), 1.0,
               1e-9);
    // Across a month boundary.
    CHECK_NEAR(DateTimeUtc(2024, 2, 1).julianDay() - DateTimeUtc(2024, 1, 31).julianDay(), 1.0,
               1e-9);
    // Across a year boundary.
    CHECK_NEAR(DateTimeUtc(2025, 1, 1).julianDay() - DateTimeUtc(2024, 12, 31).julianDay(), 1.0,
               1e-9);
}

/// 2024 is a leap year, 2023 and 1900 are not, 2000 is.
void testLeapYears()
{
    CHECK_NEAR(DateTimeUtc(2024, 3, 1).julianDay() - DateTimeUtc(2024, 2, 28).julianDay(), 2.0,
               1e-9);
    CHECK_NEAR(DateTimeUtc(2023, 3, 1).julianDay() - DateTimeUtc(2023, 2, 28).julianDay(), 1.0,
               1e-9);
    CHECK_NEAR(DateTimeUtc(2000, 3, 1).julianDay() - DateTimeUtc(2000, 2, 28).julianDay(), 2.0,
               1e-9);
    CHECK_NEAR(DateTimeUtc(1900, 3, 1).julianDay() - DateTimeUtc(1900, 2, 28).julianDay(), 1.0,
               1e-9);

    // Length of a full year.
    CHECK_NEAR(DateTimeUtc(2025, 1, 1).julianDay() - DateTimeUtc(2024, 1, 1).julianDay(), 366.0,
               1e-9);
    CHECK_NEAR(DateTimeUtc(2024, 1, 1).julianDay() - DateTimeUtc(2023, 1, 1).julianDay(), 365.0,
               1e-9);
}

void testJulianCentury()
{
    // The J2000.0 epoch is the zero point of the Julian century count.
    CHECK_NEAR(DateTimeUtc(2000, 1, 1, 12, 0, 0.0).julianCentury(), 0.0, 1e-12);

    // A Julian century is 36525 days.
    CHECK_NEAR(DateTimeUtc(2100, 1, 1, 12, 0, 0.0).julianCentury(), 1.0, 1e-6);

    // Dates before the epoch are negative.
    CHECK_TRUE(DateTimeUtc(1990, 1, 1).julianCentury() < 0.0);

    // Consistency with julianDay().
    const DateTimeUtc t(2024, 6, 21, 12, 0, 0.0);
    CHECK_NEAR(t.julianCentury(), (t.julianDay() - 2451545.0) / 36525.0, 1e-12);
}

void testMonotonic()
{
    const DateTimeUtc times[] = {
        DateTimeUtc(2024, 1, 1, 0, 0, 0.0),  DateTimeUtc(2024, 1, 1, 0, 0, 30.0),
        DateTimeUtc(2024, 1, 1, 0, 30, 0.0), DateTimeUtc(2024, 1, 1, 12, 0, 0.0),
        DateTimeUtc(2024, 6, 21, 0, 0, 0.0), DateTimeUtc(2024, 12, 31, 23, 59, 59.0),
    };
    for (std::size_t i = 1; i < sizeof(times) / sizeof(times[0]); ++i) {
        CHECK_TRUE(times[i].julianDay() > times[i - 1].julianDay());
    }
}

} // namespace

int main()
{
    testDefaults();
    testKnownJulianDays();
    testFractionOfDay();
    testConsecutiveDaysDifferByOne();
    testLeapYears();
    testJulianCentury();
    testMonotonic();
    return geotest::summarize("DateTimeUtcTests");
}
