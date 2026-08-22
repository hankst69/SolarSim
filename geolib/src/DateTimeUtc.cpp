#include "geolib/DateTimeUtc.h"

#include <cmath>

namespace geo {

double DateTimeUtc::julianDay() const
{
    int y = year;
    int m = month;
    if (m <= 2) {
        y -= 1;
        m += 12;
    }

    const int a = static_cast<int>(std::floor(y / 100.0));
    const int b = 2 - a + static_cast<int>(std::floor(a / 4.0));

    const double dayFraction = (hour + minute / 60.0 + second / 3600.0) / 24.0;

    return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (m + 1)) + day + dayFraction
           + b - 1524.5;
}

double DateTimeUtc::julianCentury() const
{
    return (julianDay() - 2451545.0) / 36525.0;
}

} // namespace geo
