#include "geolib/SunPath.h"

#include <algorithm>
#include <cmath>

namespace geo {

SunPath::SunPath(const HorizonDome& dome, int year, int month, int day, int sampleCount)
    : m_dome(dome)
    , m_year(year)
    , m_month(month)
    , m_day(day)
{
    const int count = std::max(1, sampleCount);
    m_intervalMinutes = 1440.0 / count;

    m_samples.reserve(static_cast<std::size_t>(count) + 1);
    for (int i = 0; i <= count; ++i) {
        const DateTimeUtc utc = timeAtMinutes(i * m_intervalMinutes);
        const SunPosition position(m_dome.standpoint(), utc);
        m_samples.push_back({utc, position, position.projectOnDome(m_dome)});
    }
}

DateTimeUtc SunPath::timeAtMinutes(double minutes) const
{
    const int hour = static_cast<int>(minutes / 60.0);
    const double rest = minutes - hour * 60.0;
    const int minute = static_cast<int>(rest);
    const double second = (rest - minute) * 60.0;

    return DateTimeUtc(m_year, m_month, m_day, hour, minute, second);
}

std::vector<SunPathSample> SunPath::visibleSamples() const
{
    std::vector<SunPathSample> visible;
    for (const SunPathSample& sample : m_samples) {
        if (sample.position.isAboveHorizon()) {
            visible.push_back(sample);
        }
    }
    return visible;
}

std::vector<Vector3> SunPath::arcPoints() const
{
    std::vector<Vector3> points;
    for (const SunPathSample& sample : m_samples) {
        if (sample.position.isAboveHorizon()) {
            points.push_back(sample.domePoint);
        }
    }
    return points;
}

SunPosition SunPath::at(int hour, int minute, double second) const
{
    return SunPosition(m_dome.standpoint(), DateTimeUtc(m_year, m_month, m_day, hour, minute, second));
}

bool SunPath::hasSunrise() const
{
    return std::any_of(m_samples.begin(), m_samples.end(),
                       [](const SunPathSample& s) { return s.position.isAboveHorizon(); });
}

double SunPath::findHorizonCrossing(double minutesA, double minutesB) const
{
    // Bisection on the elevation between a below and an above horizon time.
    double lo = minutesA;
    double hi = minutesB;
    for (int i = 0; i < 40; ++i) {
        const double mid = 0.5 * (lo + hi);
        const SunPosition position(m_dome.standpoint(), timeAtMinutes(mid));
        if (position.elevation() < 0.0) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return 0.5 * (lo + hi);
}

bool SunPath::sunrise(DateTimeUtc& utc) const
{
    for (std::size_t i = 1; i < m_samples.size(); ++i) {
        if (!m_samples[i - 1].position.isAboveHorizon() && m_samples[i].position.isAboveHorizon()) {
            const double crossing =
                findHorizonCrossing((i - 1) * m_intervalMinutes, i * m_intervalMinutes);
            utc = timeAtMinutes(crossing);
            return true;
        }
    }
    return false;
}

bool SunPath::sunset(DateTimeUtc& utc) const
{
    for (std::size_t i = 1; i < m_samples.size(); ++i) {
        if (m_samples[i - 1].position.isAboveHorizon() && !m_samples[i].position.isAboveHorizon()) {
            const double crossing =
                findHorizonCrossing(i * m_intervalMinutes, (i - 1) * m_intervalMinutes);
            utc = timeAtMinutes(crossing);
            return true;
        }
    }
    return false;
}

const SunPathSample& SunPath::highestSample() const
{
    return *std::max_element(m_samples.begin(), m_samples.end(),
                             [](const SunPathSample& a, const SunPathSample& b) {
                                 return a.position.elevation() < b.position.elevation();
                             });
}

double SunPath::relativeDailyEnergy() const
{
    double sum = 0.0;
    for (const SunPathSample& sample : m_samples) {
        sum += sample.position.relativeIrradiance();
    }
    return sum * (m_intervalMinutes / 60.0);
}

} // namespace geo
