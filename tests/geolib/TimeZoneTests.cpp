#include "geolib/TimeZone.h"

#include "TestSupport.h"

using namespace geo;

namespace {

void testCodeForLocation()
{
    CHECK_EQ_INT(TimeZone::codeForLocation(GeoLocation(51.5, 0.0)), 0);
    CHECK_EQ_INT(TimeZone::codeForLocation(GeoLocation(52.5, 13.4)), 1);
    CHECK_EQ_INT(TimeZone::codeForLocation(GeoLocation(40.7, -74.0)), -5);
    CHECK_EQ_INT(TimeZone::codeForLocation(GeoLocation(35.7, 139.7)), 9);
}

void testDaylightSavingDetection()
{
    CHECK_TRUE(TimeZone::isDaylightSavingTime(1, DateTimeUtc(2024, 6, 15, 12, 0, 0.0)));
    CHECK_FALSE(TimeZone::isDaylightSavingTime(1, DateTimeUtc(2024, 1, 15, 12, 0, 0.0)));

    CHECK_TRUE(TimeZone::isDaylightSavingTime(-5, DateTimeUtc(2024, 7, 1, 12, 0, 0.0)));
    CHECK_FALSE(TimeZone::isDaylightSavingTime(-5, DateTimeUtc(2024, 1, 1, 12, 0, 0.0)));

    CHECK_FALSE(TimeZone::isDaylightSavingTime(9, DateTimeUtc(2024, 7, 1, 12, 0, 0.0)));
}

void testGmtOffset()
{
    CHECK_EQ_INT(TimeZone::gmtOffsetHours(1, false), 1);
    CHECK_EQ_INT(TimeZone::gmtOffsetHours(1, true), 2);
    CHECK_EQ_INT(TimeZone::gmtOffsetHours(-5, false), -5);
    CHECK_EQ_INT(TimeZone::gmtOffsetHours(-5, true), -4);
}

void testUtcToLocalTime()
{
    DateTimeUtc local = TimeZone::toLocalTime(DateTimeUtc(2024, 1, 1, 23, 30, 0.0), 2, false);
    CHECK_EQ_INT(local.year, 2024);
    CHECK_EQ_INT(local.month, 1);
    CHECK_EQ_INT(local.day, 2);
    CHECK_EQ_INT(local.hour, 1);
    CHECK_EQ_INT(local.minute, 30);

    local = TimeZone::toLocalTime(DateTimeUtc(2024, 1, 1, 1, 0, 0.0), -8, false);
    CHECK_EQ_INT(local.year, 2023);
    CHECK_EQ_INT(local.month, 12);
    CHECK_EQ_INT(local.day, 31);
    CHECK_EQ_INT(local.hour, 17);
    CHECK_EQ_INT(local.minute, 0);

    local = TimeZone::toLocalTime(DateTimeUtc(2024, 7, 1, 12, 15, 0.0), -5, true);
    CHECK_EQ_INT(local.year, 2024);
    CHECK_EQ_INT(local.month, 7);
    CHECK_EQ_INT(local.day, 1);
    CHECK_EQ_INT(local.hour, 8);
    CHECK_EQ_INT(local.minute, 15);
}

} // namespace

int main()
{
    testCodeForLocation();
    testDaylightSavingDetection();
    testGmtOffset();
    testUtcToLocalTime();
    return geotest::summarize("TimeZoneTests");
}
