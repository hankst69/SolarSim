#pragma once

#include "geolib/DateTimeUtc.h"
#include "geolib/GeoLocation.h"
#include "geolib/HorizonDome.h"
#include "geolib/SunPosition.h"
#include "geolib/Vector3.h"

#include <vector>

namespace geo {

/// One sampled point of the sun path.
struct SunPathSample {
    DateTimeUtc time;      ///< UTC time of the sample.
    SunPosition position; ///< Sun position at that time.
    Vector3 domePoint;     ///< Projection on the dome (local east/north/up).
};

/// Samples the SunPosition over one UTC day and provides the resulting arc
/// on a HorizonDome.
class SunPath {
public:
    /// Default number of samples per day (one every 10 minutes).
    static constexpr int kDefaultSampleCount = 144;

    /// Samples the given UTC date from 00:00 to 24:00.
    /// \param sampleCount number of intervals; sampleCount + 1 samples are taken.
    SunPath(const HorizonDome& dome, int year, int month, int day,
            int sampleCount = kDefaultSampleCount);

    const HorizonDome& dome() const { return m_dome; }
    const GeoLocation& location() const { return m_dome.standpoint(); }

    int year() const { return m_year; }
    int month() const { return m_month; }
    int day() const { return m_day; }

    /// All samples of the day, ordered by time.
    const std::vector<SunPathSample>& samples() const { return m_samples; }

    /// Samples with the sun above the horizon, i.e. the visible arc on the dome.
    std::vector<SunPathSample> visibleSamples() const;

    /// Dome points of the visible arc (local east/north/up).
    std::vector<Vector3> arcPoints() const;

    /// Sun position at an arbitrary UTC time of this day.
    SunPosition at(int hour, int minute = 0, double second = 0.0) const;

    /// True if the sun rises above the horizon at all during the day.
    bool hasSunrise() const;

    /// Approximate UTC sunrise / sunset, refined by bisection between the two
    /// samples that bracket the horizon crossing. Returns false for polar day
    /// or polar night.
    bool sunrise(DateTimeUtc& utc) const;
    bool sunset(DateTimeUtc& utc) const;

    /// Sample with the highest elevation (solar noon).
    const SunPathSample& highestSample() const;

    /// Sum of the relative irradiance of all samples multiplied by the sample
    /// interval in hours: a simple relative daily energy measure.
    double relativeDailyEnergy() const;

private:
    DateTimeUtc timeAtMinutes(double minutes) const;
    double findHorizonCrossing(double minutesA, double minutesB) const;

    HorizonDome m_dome;
    int m_year{2000};
    int m_month{1};
    int m_day{1};
    double m_intervalMinutes{10.0};
    std::vector<SunPathSample> m_samples;
};

} // namespace geo
