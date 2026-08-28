#pragma once

#include "geolib/DateTimeUtc.h"
#include "geolib/GeoLocation.h"
#include "geolib/GroundPlane.h"
#include "geolib/SunPosition.h"
#include "geolib/Vector3.h"

namespace geo {

/// Solar irradiance (power per area) arriving at the earth.
///
/// Two levels of detail are provided:
///  * the theoretical maximum for a date, i.e. the irradiance on a surface
///    perpendicular to the sun rays at the top of the atmosphere. It only
///    depends on the earth - sun distance of that date (elliptical orbit) and
///    assumes no atmospheric damping at all.
///  * a realistic value for a concrete geolocation and UTC time, which adds
///    the damping by the atmosphere (the light path grows with the zenith
///    angle) and the cosine loss caused by the inclination of the receiving
///    ground plane relative to the sun direction.
class SunEnergy {
public:
    /// Solar constant: mean irradiance at one astronomical unit in W/m^2.
    static constexpr double kSolarConstant = 1361.0;

    /// Clear sky transmittance of the atmosphere at zenith (air mass 1).
    static constexpr double kZenithTransmittance = 0.7;

    /// Theoretical values for a date only (no location needed).
    explicit SunEnergy(const DateTimeUtc& utc);

    /// Theoretical and realistic values for a location at a UTC date/time.
    SunEnergy(const GeoLocation& location, const DateTimeUtc& utc);

    const DateTimeUtc& time() const { return m_utc; }

    /// True when a geolocation was given, i.e. the realistic values are valid.
    bool hasLocation() const { return m_hasLocation; }

    const GeoLocation& location() const { return m_location; }

    /// Sun position of the location and time (only valid with a location).
    const SunPosition& sunPosition() const { return m_sun; }

    /// Distance earth - sun of that date in astronomical units.
    double sunDistanceAu() const { return m_sunDistanceAu; }

    /// Theoretical maximum irradiance in W/m^2: perpendicular irradiation,
    /// sea level, no atmospheric damping. This is the solar constant scaled by
    /// the inverse square of the current earth - sun distance.
    double theoreticalIrradiance() const { return m_theoreticalIrradiance; }

    /// Relative optical air mass (length of the light path through the
    /// atmosphere, 1.0 for the sun in the zenith). Returns 0 when the sun is
    /// below the horizon.
    double airMass() const { return m_airMass; }

    /// Fraction of the radiation that survives the atmospheric path,
    /// in [0, 1]. Zero for a sun below the horizon.
    double atmosphericTransmittance() const { return m_transmittance; }

    /// Direct irradiance in W/m^2 on a surface facing the sun at sea level,
    /// i.e. the theoretical value reduced by the atmospheric damping.
    double directNormalIrradiance() const;

    /// Realistic irradiance in W/m^2 on the horizontal ground plane of the
    /// location: direct normal irradiance times the cosine of the angle
    /// between the sun and the plane normal. Zero for a sun below the horizon.
    double groundIrradiance() const;

    /// Realistic irradiance in W/m^2 on a plane with the given normal, which
    /// is expressed in the local east/north/up frame of the location. The
    /// normal does not need to be normalized.
    double irradianceOnPlane(const Vector3& planeNormalEnu) const;

    /// Realistic irradiance in W/m^2 on an inclined plane given by its tilt
    /// against the horizontal and the azimuth its normal is facing (degrees,
    /// clockwise from north, as used by SunPosition).
    double irradianceOnTiltedPlane(double tiltDeg, double azimuthDeg) const;

    /// Realistic irradiance in W/m^2 on the given ground plane. Its normal is
    /// converted into the local frame of this location.
    double irradianceOnGroundPlane(const GroundPlane& plane) const;

    /// Cosine of the angle between the sun direction and the given plane
    /// normal (local east/north/up), clamped to zero for a sun behind the
    /// plane or below the horizon.
    double incidenceFactor(const Vector3& planeNormalEnu) const;

private:
    void computeDistance();
    void computeAtmosphere();

    DateTimeUtc m_utc;
    GeoLocation m_location;
    SunPosition m_sun;
    bool m_hasLocation{false};

    double m_sunDistanceAu{1.0};
    double m_theoreticalIrradiance{kSolarConstant};
    double m_airMass{0.0};
    double m_transmittance{0.0};
};

} // namespace geo
