#pragma once

#include "geolib/GeoLocation.h"
#include "geolib/GroundPlane.h"
#include "geolib/Vector3.h"

namespace geo {

/// Half sphere ("dome") placed on the GroundPlane of a standpoint.
/// Its centre is the standpoint, its radius is the distance from the standpoint
/// to the visible horizon as seen from a viewer eye height above ground.
class HorizonDome {
public:
    /// Default eye height of an observer above the ground plane in metres.
    static constexpr double kDefaultViewHeightM = 1.6;

    explicit HorizonDome(const GeoLocation& standpoint,
                         double viewHeightM = kDefaultViewHeightM);

    const GeoLocation& standpoint() const { return m_groundPlane.origin(); }
    const GroundPlane& groundPlane() const { return m_groundPlane; }

    double viewHeight() const { return m_viewHeightM; }

    /// Radius of the dome = distance standpoint -> horizon measured in the
    /// ground plane (metres).
    double radius() const { return m_radiusM; }

    /// Straight line of sight distance from the viewpoint to the tangent point
    /// on the earth surface (metres).
    double lineOfSightDistance() const;

    /// Angle between the standpoint and the tangent point seen from the earth
    /// centre (radians).
    double geocentricHorizonAngle() const;

    /// Distance to the horizon measured along the earth surface (metres).
    double arcDistanceToHorizon() const;

    /// How far the ground plane is above the earth surface at the horizon
    /// (metres), i.e. the "earth curvature drop".
    double curvatureDrop() const;

    /// Point on the dome surface for the given azimuth (degrees, clockwise from
    /// north) and elevation (degrees above the ground plane), expressed in the
    /// local east/north/up frame of the ground plane.
    Vector3 pointOnDome(double azimuthDeg, double elevationDeg) const;

    /// True if the local east/north/up point lies inside the dome.
    bool contains(const Vector3& localPoint) const;

private:
    GroundPlane m_groundPlane;
    double m_viewHeightM{kDefaultViewHeightM};
    double m_earthRadiusM{0.0};
    double m_radiusM{0.0};
};

} // namespace geo
