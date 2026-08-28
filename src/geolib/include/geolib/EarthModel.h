#pragma once

#include "geolib/Vector3.h"

#include <memory>

namespace geo {

class GeoLocation;

/// Abstract earth shape model. The spherical model is used for simple math, the
/// WGS84 ellipsoidal model provides the higher precision used by default.
class EarthModel {
public:
    virtual ~EarthModel() = default;

    /// Equatorial (semi major) radius in metres.
    virtual double equatorialRadius() const = 0;

    /// Polar (semi minor) radius in metres.
    virtual double polarRadius() const = 0;

    /// Flattening f = (a - b) / a. Zero for a sphere.
    virtual double flattening() const = 0;

    /// Radius of curvature used for local (tangential) computations at the given
    /// latitude. For a sphere this is constant.
    virtual double localRadius(double latitudeDeg) const = 0;

    /// Radius of curvature in the meridian (north/south) direction.
    virtual double meridionalRadius(double latitudeDeg) const = 0;

    /// Radius of curvature in the prime vertical (east/west) direction.
    virtual double primeVerticalRadius(double latitudeDeg) const = 0;

    /// Distance from the earth centre to the reference surface at the given
    /// geodetic latitude.
    virtual double geocentricRadius(double latitudeDeg) const = 0;

    /// Geocentric latitude belonging to the given geodetic latitude.
    virtual double geocentricLatitude(double latitudeDeg) const = 0;

    /// Convert geodetic coordinates to earth centred earth fixed coordinates.
    virtual Vector3 toEcef(double latitudeDeg, double longitudeDeg, double altitudeM) const = 0;

    /// Shared default model (WGS84 ellipsoid) used when no explicit model is given.
    static const EarthModel& defaultModel();

    /// Shared spherical model, useful for comparisons and simple estimates.
    static const EarthModel& sphericalModel();
};

/// Earth modelled as a perfect sphere (mean earth radius).
class SphericalEarthModel : public EarthModel {
public:
    static constexpr double kMeanRadiusM = 6371008.8;

    explicit SphericalEarthModel(double radiusM = kMeanRadiusM);

    double equatorialRadius() const override { return m_radiusM; }
    double polarRadius() const override { return m_radiusM; }
    double flattening() const override { return 0.0; }
    double localRadius(double latitudeDeg) const override;
    double meridionalRadius(double latitudeDeg) const override;
    double primeVerticalRadius(double latitudeDeg) const override;
    double geocentricRadius(double latitudeDeg) const override;
    double geocentricLatitude(double latitudeDeg) const override;
    Vector3 toEcef(double latitudeDeg, double longitudeDeg, double altitudeM) const override;

private:
    double m_radiusM;
};

/// Earth modelled as the WGS84 reference ellipsoid (rotational ellipsoid with
/// the semi major axis in the equatorial plane).
class WGS84EarthModel : public EarthModel {
public:
    static constexpr double kSemiMajorAxisM = 6378137.0;
    static constexpr double kInverseFlattening = 298.257223563;

    WGS84EarthModel();

    double equatorialRadius() const override { return m_a; }
    double polarRadius() const override { return m_b; }
    double flattening() const override { return m_f; }

    /// First eccentricity squared e^2 = 2f - f^2.
    double eccentricitySquared() const { return m_e2; }

    /// Gaussian mean radius of curvature, the best local sphere approximation.
    double localRadius(double latitudeDeg) const override;
    double meridionalRadius(double latitudeDeg) const override;
    double primeVerticalRadius(double latitudeDeg) const override;
    double geocentricRadius(double latitudeDeg) const override;
    double geocentricLatitude(double latitudeDeg) const override;
    Vector3 toEcef(double latitudeDeg, double longitudeDeg, double altitudeM) const override;

private:
    double m_a;
    double m_f;
    double m_b;
    double m_e2;
};

} // namespace geo
