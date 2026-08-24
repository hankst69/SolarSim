#include "geolib/SolarPosition.h"

#include "geolib/Angle.h"

// The calculation below passes through several reference frames: geocentric
// ecliptic, geocentric equatorial, hour angle and finally the topocentric
// horizontal frame of the ground plane. See SolarPosition.md in the repository
// root for a detailed description of these coordinate systems and their
// origins.

namespace geo {
namespace {

/// Normalize an angle in degrees into [0, 360).
double wrap360(double deg)
{
    double d = std::fmod(deg, 360.0);
    if (d < 0.0) {
        d += 360.0;
    }
    return d;
}

} // namespace

SolarPosition::SolarPosition(const GeoLocation& location, const DateTimeUtc& utc)
    : m_location(location)
    , m_utc(utc)
{
    compute();
}

void SolarPosition::compute()
{
    // NOAA solar position algorithm (accuracy of about one arc minute).
    const double t = m_utc.julianCentury();

    const double geomMeanLong = wrap360(280.46646 + t * (36000.76983 + 0.0003032 * t));
    const double geomMeanAnom = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    const double eccent = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);

    const double manomRad = degToRad(geomMeanAnom);
    const double sunEqOfCentre = std::sin(manomRad) * (1.914602 - t * (0.004817 + 0.000014 * t))
                                 + std::sin(2.0 * manomRad) * (0.019993 - 0.000101 * t)
                                 + std::sin(3.0 * manomRad) * 0.000289;

    const double sunTrueLong = geomMeanLong + sunEqOfCentre;
    const double omega = 125.04 - 1934.136 * t;
    const double sunAppLong = sunTrueLong - 0.00569 - 0.00478 * std::sin(degToRad(omega));

    const double meanObliq = 23.0
                             + (26.0
                                + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0)
                                   / 60.0;
    const double obliqCorr = meanObliq + 0.00256 * std::cos(degToRad(omega));

    m_declinationDeg = radToDeg(
        std::asin(std::sin(degToRad(obliqCorr)) * std::sin(degToRad(sunAppLong))));

    const double y = std::tan(degToRad(obliqCorr / 2.0)) * std::tan(degToRad(obliqCorr / 2.0));
    const double l0Rad = degToRad(geomMeanLong);
    m_equationOfTimeMin = 4.0
                          * radToDeg(y * std::sin(2.0 * l0Rad)
                                     - 2.0 * eccent * std::sin(manomRad)
                                     + 4.0 * eccent * y * std::sin(manomRad) * std::cos(2.0 * l0Rad)
                                     - 0.5 * y * y * std::sin(4.0 * l0Rad)
                                     - 1.25 * eccent * eccent * std::sin(2.0 * manomRad));

    const double minutesUtc = m_utc.hour * 60.0 + m_utc.minute + m_utc.second / 60.0;
    const double trueSolarTime =
        std::fmod(minutesUtc + m_equationOfTimeMin + 4.0 * m_location.longitude(), 1440.0);

    m_hourAngleDeg = trueSolarTime / 4.0 - 180.0;
    if (m_hourAngleDeg < -180.0) {
        m_hourAngleDeg += 360.0;
    }

    const double latRad = degToRad(m_location.latitude());
    const double declRad = degToRad(m_declinationDeg);
    const double haRad = degToRad(m_hourAngleDeg);

    double cosZenith = std::sin(latRad) * std::sin(declRad)
                       + std::cos(latRad) * std::cos(declRad) * std::cos(haRad);
    cosZenith = std::fmax(-1.0, std::fmin(1.0, cosZenith));

    const double zenithRad = std::acos(cosZenith);
    m_elevationDeg = 90.0 - radToDeg(zenithRad);

    const double denom = std::cos(latRad) * std::sin(zenithRad);
    double azimuth = 180.0;
    if (std::fabs(denom) > 1e-12) {
        double cosAz = (std::sin(latRad) * cosZenith - std::sin(declRad)) / denom;
        cosAz = std::fmax(-1.0, std::fmin(1.0, cosAz));
        azimuth = radToDeg(std::acos(cosAz));
        if (m_hourAngleDeg > 0.0) {
            azimuth = 360.0 - azimuth;
        }
    }
    m_azimuthDeg = wrap360(azimuth);
}

double SolarPosition::refractedElevation() const
{
    // Approximate atmospheric refraction (NOAA), valid near the horizon.
    const double e = m_elevationDeg;
    if (e > 85.0) {
        return e;
    }

    const double te = std::tan(degToRad(e));
    double refractionArcSec = 0.0;
    if (e > 5.0) {
        refractionArcSec = 58.1 / te - 0.07 / (te * te * te) + 0.000086 / (te * te * te * te * te);
    } else if (e > -0.575) {
        refractionArcSec = 1735.0 + e * (-518.2 + e * (103.4 + e * (-12.79 + e * 0.711)));
    } else {
        refractionArcSec = -20.772 / te;
    }

    return e + refractionArcSec / 3600.0;
}

Vector3 SolarPosition::direction() const
{
    const double az = degToRad(m_azimuthDeg);
    const double el = degToRad(m_elevationDeg);
    const double horizontal = std::cos(el);

    return {horizontal * std::sin(az), horizontal * std::cos(az), std::sin(el)};
}

Vector3 SolarPosition::projectOnDome(const HorizonDome& dome) const
{
    const double elevation = std::fmax(0.0, m_elevationDeg);
    return dome.pointOnDome(m_azimuthDeg, elevation);
}

double SolarPosition::relativeIrradiance() const
{
    if (!isAboveHorizon()) {
        return 0.0;
    }
    return std::cos(degToRad(zenithAngle()));
}

} // namespace geo
