#pragma once

#include "geolib/DateTimeUtc.h"
#include "geolib/GeoLocation.h"

namespace geo {

/// Lightweight time zone helper based on longitude derived standard offsets.
///
/// The time zone code is the base GMT offset in hours, typically in [-12, +14].
/// Daylight saving rules are approximated for European (UTC-1..UTC+3) and
/// North American (UTC-9..UTC-4) zones.
class TimeZone {
public:
    /// Returns the time zone code for a geolocation by rounding longitude to
    /// the nearest 15 degree meridian.
    static int codeForLocation(const GeoLocation& location);

    /// Detects if daylight saving time is active for the given UTC date/time in
    /// the specified time zone code.
    static bool isDaylightSavingTime(int timeZoneCode, const DateTimeUtc& utc);

    /// Converts time zone code and DST mode to a GMT offset in hours.
    static int gmtOffsetHours(int timeZoneCode, bool daylightSavingTime);

    /// Converts UTC date/time to local date/time for the provided time zone and
    /// DST mode.
    static DateTimeUtc toLocalTime(const DateTimeUtc& utc, int timeZoneCode, bool daylightSavingTime);
};

} // namespace geo
