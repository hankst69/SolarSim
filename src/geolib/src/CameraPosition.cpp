#include "geolib/CameraPosition.h"

#include "geolib/Angle.h"

namespace geo {

namespace {

/// Local east/north/up point at the given azimuth (degrees clockwise from
/// north), horizontal distance and height above the ground plane.
Vector3 localPointAt(const HorizonDome& dome, double azimuthDeg, double distanceM,
                     double heightM)
{
    const double az = degToRad(azimuthDeg);
    const double east = distanceM * std::sin(az);
    const double north = distanceM * std::cos(az);

    // Height of the camera above the ground plane: terrain height of the
    // standpoint relative to the plane origin, plus the camera height above the
    // terrain, minus the drop of the curved earth surface at that offset.
    const GeoLocation& standpoint = dome.standpoint();
    const double terrainOffsetM = dome.terrainHeight() - standpoint.altitude();
    const double dropM = dome.groundPlane().curvatureDrop(east, north);

    return {east, north, terrainOffsetM + heightM - dropM};
}

} // namespace

CameraPosition::CameraPosition(const HorizonDome& dome, const Vector3& local, double heightM)
    : m_dome(dome)
    , m_target(dome.groundPlane().origin())
    , m_groundPlane(dome.groundPlane())
    , m_local(local)
    , m_heightM(heightM)
{
}

CameraPosition CameraPosition::initial(const HorizonDome& dome, double heightM,
                                       double distanceM)
{
    // Northern hemisphere: look from the south, southern hemisphere: from the
    // north, so that the sun path is in front of the camera. On the equator we
    // keep the northern hemisphere convention.
    const double azimuthDeg = (dome.standpoint().latitude() >= 0.0) ? 180.0 : 0.0;
    return CameraPosition(dome, localPointAt(dome, azimuthDeg, distanceM, heightM), heightM);
}

CameraPosition CameraPosition::initial(const GeoLocation& location, double heightM,
                                       double distanceM)
{
    return initial(HorizonDome::fromHeightDataSourceRegistry(location), heightM, distanceM);
}

CameraPosition CameraPosition::forDateTime(const HorizonDome& dome, const DateTimeUtc& utc,
                                           double heightM, double distanceM)
{
    const SunPosition sun(dome.standpoint(), utc);

    // Place the camera on the line standpoint -> sun. Its height follows the
    // sun elevation, but the camera never gets closer to the terrain than the
    // requested height.
    double cameraHeightM = heightM;
    if (sun.elevation() > 0.0) {
        const double byElevationM = distanceM * std::tan(degToRad(sun.elevation()));
        if (byElevationM > cameraHeightM) {
            cameraHeightM = byElevationM;
        }
    }

    return CameraPosition(dome, localPointAt(dome, sun.azimuth(), distanceM, cameraHeightM),
                          cameraHeightM);
}

CameraPosition CameraPosition::forDateTime(const GeoLocation& location, const DateTimeUtc& utc,
                                           double heightM, double distanceM)
{
    return forDateTime(HorizonDome::fromHeightDataSourceRegistry(location), utc, heightM,
                       distanceM);
}

CameraPosition CameraPosition::fromOrbit(const HorizonDome& dome, double azimuthDeg,
                                         double elevationDeg, double rangeM)
{
    if (elevationDeg < kMinElevationDeg) {
        elevationDeg = kMinElevationDeg;
    } else if (elevationDeg > kMaxElevationDeg) {
        elevationDeg = kMaxElevationDeg;
    }
    if (rangeM < kMinRangeM) {
        rangeM = kMinRangeM;
    }

    const double elevation = degToRad(elevationDeg);
    const double distanceM = rangeM * std::cos(elevation);
    const double heightM = rangeM * std::sin(elevation);

    return CameraPosition(dome, localPointAt(dome, azimuthDeg, distanceM, heightM), heightM);
}

CameraPosition CameraPosition::orbited(double deltaAzimuthDeg, double deltaElevationDeg) const
{
    double azimuthDeg = std::fmod(azimuth() + deltaAzimuthDeg, 360.0);
    if (azimuthDeg < 0.0) {
        azimuthDeg += 360.0;
    }
    return fromOrbit(m_dome, azimuthDeg, elevation() + deltaElevationDeg, range());
}

CameraPosition CameraPosition::zoomed(double factor) const
{
    if (factor <= 0.0) {
        factor = 1.0;
    }
    return withRange(range() * factor);
}

CameraPosition CameraPosition::withRange(double rangeM) const
{
    return fromOrbit(m_dome, azimuth(), elevation(), rangeM);
}

Vector3 CameraPosition::ecefPosition() const
{
    return m_groundPlane.toEcef(m_local);
}

GeoLocation CameraPosition::geoLocation() const
{
    return m_groundPlane.toGeoLocation(m_local);
}

double CameraPosition::horizontalDistance() const
{
    return std::sqrt(m_local.x * m_local.x + m_local.y * m_local.y);
}

double CameraPosition::azimuth() const
{
    double azimuthDeg = radToDeg(std::atan2(m_local.x, m_local.y));
    if (azimuthDeg < 0.0) {
        azimuthDeg += 360.0;
    }
    return azimuthDeg;
}

double CameraPosition::elevation() const
{
    const double horizontalM = horizontalDistance();
    if (horizontalM <= 0.0) {
        return m_local.z >= 0.0 ? 90.0 : -90.0;
    }
    return radToDeg(std::atan2(m_local.z, horizontalM));
}

double CameraPosition::range() const
{
    return m_local.length();
}

Vector3 CameraPosition::viewDirection() const
{
    return (Vector3{0.0, 0.0, 0.0} - m_local).normalized();
}

} // namespace geo
