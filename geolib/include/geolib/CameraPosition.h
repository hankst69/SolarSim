#pragma once

#include "geolib/DateTimeUtc.h"
#include "geolib/GeoLocation.h"
#include "geolib/HorizonDome.h"
#include "geolib/SunPosition.h"
#include "geolib/Vector3.h"

namespace geo {

/// Position of the virtual camera that looks at the standpoint of a HorizonDome.
///
/// The camera is placed above the terrain and offset horizontally from the dome
/// centre. Without a date/time the offset points towards the equator, i.e. to
/// the south on the northern hemisphere and to the north on the southern one,
/// so that the sun path is always in front of the camera. With a date/time the
/// camera is placed on the line between the standpoint and the sun.
class CameraPosition {
public:
    /// Default camera height above the terrain ground level in metres.
    static constexpr double kDefaultHeightM = 30.0;

    /// Default horizontal distance from the dome centre in metres.
    static constexpr double kDefaultDistanceM = 50.0;

    /// Initial camera for the standpoint of the given dome.
    static CameraPosition initial(const HorizonDome& dome,
                                  double heightM = kDefaultHeightM,
                                  double distanceM = kDefaultDistanceM);

    /// Initial camera for the given location; the dome is built from the height
    /// data source registry.
    static CameraPosition initial(const GeoLocation& location,
                                  double heightM = kDefaultHeightM,
                                  double distanceM = kDefaultDistanceM);

    /// Camera placed on the line between the standpoint and the sun at the
    /// given UTC time. The horizontal distance to the dome centre is
    /// distanceM, the height follows the sun elevation but never drops below
    /// heightM. For a sun at or below the horizon only its azimuth is used.
    static CameraPosition forDateTime(const HorizonDome& dome, const DateTimeUtc& utc,
                                      double heightM = kDefaultHeightM,
                                      double distanceM = kDefaultDistanceM);

    /// Same as above for a location; the dome is built from the height data
    /// source registry.
    static CameraPosition forDateTime(const GeoLocation& location, const DateTimeUtc& utc,
                                      double heightM = kDefaultHeightM,
                                      double distanceM = kDefaultDistanceM);

    /// Standpoint the camera is looking at (centre of the dome).
    const GeoLocation& target() const { return m_target; }

    /// Camera position in the local east/north/up frame of the dome ground
    /// plane (metres).
    const Vector3& localPosition() const { return m_local; }

    /// Camera position in earth centred earth fixed coordinates (metres).
    Vector3 ecefPosition() const;

    /// Geodetic location of the camera.
    GeoLocation geoLocation() const;

    /// Height of the camera above the terrain ground level (metres).
    double heightAboveTerrain() const { return m_heightM; }

    /// Horizontal distance of the camera from the dome centre (metres).
    double horizontalDistance() const;

    /// Azimuth of the camera as seen from the dome centre, in degrees
    /// clockwise from north.
    double azimuth() const;

    /// Elevation of the camera as seen from the dome centre, in degrees above
    /// the ground plane.
    double elevation() const;

    /// Unit view direction from the camera towards the dome centre, in the
    /// local east/north/up frame.
    Vector3 viewDirection() const;

private:
    CameraPosition(const GroundPlane& groundPlane, const Vector3& local, double heightM);

    GeoLocation m_target;
    GroundPlane m_groundPlane;
    Vector3 m_local;
    double m_heightM{kDefaultHeightM};
};

} // namespace geo
