# CameraPosition

`CameraPosition` computes where a virtual camera is placed to look at the
standpoint of a [`HorizonDome`](HorizonDome.md). The result is expressed in the
local east/north/up frame of the dome ground plane, so it can be handed to a
renderer directly, and can also be converted to ECEF or geodetic coordinates.

## Initial camera

Without a date/time the camera is placed by the hemisphere rule:

- 30 m above the terrain ground level of the standpoint,
- 50 m horizontally away from the dome centre,
- towards the **south** (azimuth 180 deg) on the northern hemisphere,
- towards the **north** (azimuth 0 deg) on the southern hemisphere.

That way the sun path is always in front of the camera. Locations exactly on the
equator use the northern hemisphere convention.

```cpp
const HorizonDome dome = HorizonDome::fromHeightDataSourceRegistry(home);
const CameraPosition camera = CameraPosition::initial(dome);

const Vector3 eye = camera.localPosition();  // east/north/up, metres
const Vector3 view = camera.viewDirection(); // unit vector towards the centre
```

Height and distance are parameters, the defaults are the constants
`kDefaultHeightM` (30 m) and `kDefaultDistanceM` (50 m):

```cpp
const CameraPosition wide = CameraPosition::initial(dome, 60.0, 200.0);
```

An overload takes a `GeoLocation` and builds the dome from the
[height data source registry](HeightDataSources.md):

```cpp
const CameraPosition camera = CameraPosition::initial(home);
```

## Camera for a date/time

For a given `DateTimeUtc` the camera is placed on the line between the
standpoint and the sun, i.e. it looks along the sun rays towards the ground:

```cpp
const CameraPosition camera =
    CameraPosition::forDateTime(dome, DateTimeUtc(2024, 6, 21, 10, 0, 0));
```

The azimuth of the camera equals the [sun azimuth](SunPosition.md). The
horizontal distance stays at `distanceM`, the height follows the sun elevation
(`distance * tan(elevation)`) but never drops below `heightM`. For a sun at or
below the horizon only its azimuth is used and the camera keeps the default
height, so the view stays usable at night.

## Height and earth curvature

`heightAboveTerrain()` is always measured from the terrain ground level of the
standpoint, not from sea level. The local `z` coordinate therefore contains

```
z = (terrainHeight - standpointAltitude) + heightAboveTerrain - curvatureDrop
```

where `curvatureDrop` is the drop of the curved earth surface below the
tangential plane at the camera offset (see [GroundPlane.md](GroundPlane.md)).
Over 50 m the drop is well below a millimetre, but it keeps the camera
consistent with large distances too.

## Queries

| Member | Meaning |
| --- | --- |
| `target()` | Standpoint the camera looks at (dome centre). |
| `localPosition()` | Camera position in the local east/north/up frame (metres). |
| `ecefPosition()` | Camera position in earth centred earth fixed coordinates. |
| `geoLocation()` | Geodetic location of the camera. |
| `heightAboveTerrain()` | Camera height above terrain ground level (metres). |
| `horizontalDistance()` | Distance from the dome centre in the ground plane. |
| `azimuth()` | Camera azimuth seen from the centre, degrees clockwise from north. |
| `elevation()` | Camera elevation seen from the centre, degrees above the plane. |
| `viewDirection()` | Unit direction from the camera towards the centre. |

## Related

- [HorizonDome.md](HorizonDome.md) - the dome whose centre the camera targets.
- [GroundPlane.md](GroundPlane.md) - local frame and curvature drop.
- [SunPosition.md](SunPosition.md) - azimuth/elevation used by `forDateTime()`.
