#pragma once

namespace geo {

/// Simple UTC date/time (proleptic Gregorian calendar).
struct DateTimeUtc {
    int year{2000};
    int month{1};
    int day{1};
    int hour{0};
    int minute{0};
    double second{0.0};

    DateTimeUtc() = default;
    DateTimeUtc(int year_, int month_, int day_, int hour_ = 0, int minute_ = 0, double second_ = 0.0)
        : year(year_), month(month_), day(day_), hour(hour_), minute(minute_), second(second_)
    {
    }

    /// Julian day number including the fractional part of the day.
    double julianDay() const;

    /// Julian centuries since the J2000.0 epoch.
    double julianCentury() const;
};

} // namespace geo
