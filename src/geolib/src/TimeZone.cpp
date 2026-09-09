#include "geolib/TimeZone.h"

#include <algorithm>
#include <cmath>

namespace geo {
namespace {

long long daysFromCivil(int year, int month, int day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153u * static_cast<unsigned>(month + (month > 2 ? -3 : 9)) + 2u) / 5u
                         + static_cast<unsigned>(day) - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return static_cast<long long>(era) * 146097LL + static_cast<long long>(doe) - 719468LL;
}

void civilFromDays(long long z, int& year, int& month, int& day)
{
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    year = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    month = static_cast<int>(mp + (mp < 10 ? 3 : -9));
    year += month <= 2;
}

DateTimeUtc addSeconds(const DateTimeUtc& t, long long deltaSeconds)
{
    const long long day = daysFromCivil(t.year, t.month, t.day);
    const long long secondsOfDay = static_cast<long long>(t.hour) * 3600LL
                                   + static_cast<long long>(t.minute) * 60LL
                                   + static_cast<long long>(std::floor(t.second));

    long long total = day * 86400LL + secondsOfDay + deltaSeconds;
    const long long wholeDays = (total >= 0) ? (total / 86400LL) : ((total - 86399LL) / 86400LL);
    long long rem = total - wholeDays * 86400LL;
    if (rem < 0) {
        rem += 86400LL;
    }

    int year = 2000;
    int month = 1;
    int dayOfMonth = 1;
    civilFromDays(wholeDays, year, month, dayOfMonth);

    DateTimeUtc out;
    out.year = year;
    out.month = month;
    out.day = dayOfMonth;
    out.hour = static_cast<int>(rem / 3600LL);
    rem %= 3600LL;
    out.minute = static_cast<int>(rem / 60LL);
    out.second = static_cast<double>(rem % 60LL) + (t.second - std::floor(t.second));
    return out;
}

int weekdaySunday0(int year, int month, int day)
{
    const long long days = daysFromCivil(year, month, day);
    const int weekdayMonday0 = static_cast<int>((days + 3) % 7);
    const int monday0 = weekdayMonday0 < 0 ? weekdayMonday0 + 7 : weekdayMonday0;
    return (monday0 + 1) % 7;
}

int nthSundayOfMonth(int year, int month, int n)
{
    const int firstWeekday = weekdaySunday0(year, month, 1);
    const int firstSunday = 1 + ((7 - firstWeekday) % 7);
    return firstSunday + 7 * (n - 1);
}

int lastSundayOfMonth(int year, int month)
{
    int monthLength = 31;
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        monthLength = 30;
    } else if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        monthLength = leap ? 29 : 28;
    }

    const int lastWeekday = weekdaySunday0(year, month, monthLength);
    return monthLength - lastWeekday;
}

bool supportsEuDst(int timeZoneCode)
{
    return timeZoneCode >= -1 && timeZoneCode <= 3;
}

bool supportsNorthAmericaDst(int timeZoneCode)
{
    return timeZoneCode >= -9 && timeZoneCode <= -4;
}

} // namespace

int TimeZone::codeForLocation(const GeoLocation& location)
{
    const int offset = static_cast<int>(std::round(location.longitude() / 15.0));
    return std::clamp(offset, -12, 14);
}

bool TimeZone::isDaylightSavingTime(int timeZoneCode, const DateTimeUtc& utc)
{
    if (supportsEuDst(timeZoneCode)) {
        const int startDay = lastSundayOfMonth(utc.year, 3);
        const int endDay = lastSundayOfMonth(utc.year, 10);
        const DateTimeUtc start(utc.year, 3, startDay, 1, 0, 0.0);
        const DateTimeUtc end(utc.year, 10, endDay, 1, 0, 0.0);
        return utc.julianDay() >= start.julianDay() && utc.julianDay() < end.julianDay();
    }

    if (supportsNorthAmericaDst(timeZoneCode)) {
        const int startDay = nthSundayOfMonth(utc.year, 3, 2);
        const int endDay = nthSundayOfMonth(utc.year, 11, 1);

        DateTimeUtc startLocal(utc.year, 3, startDay, 2, 0, 0.0);
        DateTimeUtc endLocal(utc.year, 11, endDay, 2, 0, 0.0);

        const DateTimeUtc startUtc = addSeconds(startLocal, static_cast<long long>(-timeZoneCode) * 3600LL);
        const DateTimeUtc endUtc = addSeconds(endLocal, static_cast<long long>(-(timeZoneCode + 1)) * 3600LL);

        return utc.julianDay() >= startUtc.julianDay() && utc.julianDay() < endUtc.julianDay();
    }

    return false;
}

int TimeZone::gmtOffsetHours(int timeZoneCode, bool daylightSavingTime)
{
    return timeZoneCode + (daylightSavingTime ? 1 : 0);
}

DateTimeUtc TimeZone::toLocalTime(const DateTimeUtc& utc, int timeZoneCode, bool daylightSavingTime)
{
    const int offsetHours = gmtOffsetHours(timeZoneCode, daylightSavingTime);
    return addSeconds(utc, static_cast<long long>(offsetHours) * 3600LL);
}

} // namespace geo
