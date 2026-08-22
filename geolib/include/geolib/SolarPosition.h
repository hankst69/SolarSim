#pragma once

#include "geolib/DateTimeUtc.h"
#include "geolib/GeoLocation.h"
#include "geolib/HorizonDome.h"
#include "geolib/Vector3.h"

namespace geo {

/// Position of the sun as seen from a location at a given UTC date/time.
/// The angles follow the usual horizontal (topocentric) convention:
/// azimuth is measured clockwise from north, elevation above the ground plane.
class SolarPosition {
public:
    SolarPosition(const GeoLocation& location, const DateTimeUtc& utc);

    const GeoLocation& location() const { return m_location; }
    const DateTimeUtc& time() const { return m_utc; }

    /// Azimuth in degrees, clockwise from north (0 = north, 90 = east).
    double azimuth() const { return m_azimuthDeg; }

    /// Geometric elevation in degrees above the ground plane (negative = below).
    double elevation() const { return m_elevationDeg; }

    /// Elevation in degrees corrected for atmospheric refraction.
    double refractedElevation() const;

    /// Zenith angle in degrees (90 - elevation).
    double zenithAngle() const { return 90.0 - m_elevationDeg; }

    /// Declination of the sun in degrees.
    double declination() const { return m_declinationDeg; }

    /// Local hour angle of the sun in degrees.
    double hourAngle() const { return m_hourAngleDeg; }

    /// Equation of time in minutes.
    double equationOfTime() const { return m_equationOfTimeMin; }

    /// True when the sun centre is above the ground plane.
    bool isAboveHorizon() const { return m_elevationDeg > 0.0; }

    /// Unit direction to the sun in the local east/north/up frame.
    Vector3 direction() const;

    /// Sun position projected onto the given dome, in the local east/north/up
    /// frame of its ground plane. For a sun below the horizon the point is
    /// clamped to the dome rim (elevation 0).
    Vector3 projectOnDome(const HorizonDome& dome) const;

    /// Relative irradiance factor (cosine of the zenith angle), 0 when the sun
    /// is below the horizon.
    double relativeIrradiance() const;

private:
    void compute();

    GeoLocation m_location;
    DateTimeUtc m_utc;

    double m_azimuthDeg{0.0};
    double m_elevationDeg{0.0};
    double m_declinationDeg{0.0};
    double m_hourAngleDeg{0.0};
    double m_equationOfTimeMin{0.0};
};

} // namespace geo
