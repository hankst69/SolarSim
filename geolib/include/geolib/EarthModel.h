#pragma once

#include "geolib/Vector3.h"

#include <memory>

namespace geo {

class GeoLocation;

/// Abstract earth shape model. The spherical model is used for the simple math,
/// an WGS84 ellipsoidal model can be added later without touching client code.
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

    /// Convert geodetic coordinates to earth centred earth fixed coordinates.
    virtual Vector3 toEcef(double latitudeDeg, double longitudeDeg, double altitudeM) const = 0;

    /// Shared default model (sphere) used when no explicit model is given.
    static const EarthModel& defaultModel();
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
    Vector3 toEcef(double latitudeDeg, double longitudeDeg, double altitudeM) const override;

private:
    double m_radiusM;
};

} // namespace geo
