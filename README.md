# SolarSim

Simulation of solar light, shadows and energy levels for a given location on
earth and a given day.

The project is split into a reusable C++ class library (`geolib`) that contains
all geo-geometrical and astronomical math, and a Qt GUI application (planned)
that visualizes the results.

## Building

The project uses CMake (minimum 3.16) and C++17.

```
cmake -S . -B build
cmake --build build
```

## geolib

`geolib` is a dependency free C++17 static library. All types live in the
`geo` namespace, headers are located under `geolib/include/geolib`.

### Core types

| Type | Header | Purpose |
| --- | --- | --- |
| `Vector3` | `Vector3.h` | Minimal 3D vector math used for ECEF and local ENU coordinates (metres). |
| `degToRad` / `radToDeg` | `Angle.h` | Angle conversion helpers and `kPi`. |
| `EarthModel` | `EarthModel.h` | Abstract earth shape: radii, flattening, radii of curvature, geocentric radius/latitude, geodetic to ECEF conversion. |
| `SphericalEarthModel` | `EarthModel.h` | Earth as a perfect sphere (mean radius 6371008.8 m). |
| `WGS84EarthModel` | `EarthModel.h` | Earth as the WGS84 reference ellipsoid (a = 6378137 m, 1/f = 298.257223563). The default model. |
| `GeoLocation` | `GeoLocation.h` | A point on earth given by latitude/longitude in degrees and altitude in metres. |
| `GroundPlane` | `GroundPlane.h` | Tangential plane on the earth surface at a `GeoLocation`. |
| `HorizonDome` | `HorizonDome.h` | Half sphere standing on the ground plane, reaching to the visible horizon. |
| `DateTimeUtc` | `DateTimeUtc.h` | UTC date/time with Julian day and Julian century conversion. |
| `SolarPosition` | `SolarPosition.h` | Sun position for a location and UTC time, projected onto the dome. |
| `SunPath` | `SunPath.h` | Samples `SolarPosition` across a day to produce the sun arc on the dome. |

### Earth model

All computations go through the abstract `EarthModel` interface. Two
implementations are available:

- `WGS84EarthModel` - the WGS84 reference ellipsoid (semi major axis
  a = 6378137 m, inverse flattening 1/f = 298.257223563). This is the model
  returned by `EarthModel::defaultModel()` and therefore used by every
  `GeoLocation` that is created without an explicit model.
- `SphericalEarthModel` - earth as a perfect sphere (mean radius 6371008.8 m),
  available via `EarthModel::sphericalModel()` and useful for simple estimates
  and for comparing the effect of the ellipsoid.

Besides the equatorial and polar radius and the flattening, the interface
exposes the meridional radius of curvature `M`, the prime vertical radius of
curvature `N`, the Gaussian mean radius `sqrt(M * N)` as `localRadius()`, the
geocentric radius of the surface and the geocentric latitude belonging to a
geodetic latitude. `toEcef()` performs the exact geodetic to ECEF conversion of
the respective model.

A different model can be passed explicitly:

```cpp
GeoLocation ellipsoidal(48.1372, 11.5756);                              // WGS84 (default)
GeoLocation spherical(48.1372, 11.5756, 0.0, EarthModel::sphericalModel());
```

### Ground plane

`GeoLocation::groundPlane()` returns the tangential plane touching the earth
surface below the location. Its origin is the standpoint and its axes are the
local east/north/up (ENU) frame. `GroundPlane` converts between ECEF and this
local frame and reports the signed height of a point above the plane.

The plane also caches the local radii of curvature of the earth model at its
origin (`meridionalRadius()`, `primeVerticalRadius()`). Based on them,
`curvatureDrop(east, north)` returns how far the curved surface falls below the
tangential plane at a local offset, and `toGeoLocation(local)` maps a local ENU
coordinate back to latitude/longitude/altitude with the correct north/south and
east/west scaling of the ellipsoid.

### Horizon dome

`HorizonDome` places a half sphere on the ground plane, centred on the
standpoint. Its radius is the distance from the standpoint to the visible
horizon: from a viewpoint at eye height `h` above the standpoint (default
1.6 m) a tangent to the earth sphere of radius `R` is drawn and intersected
with the ground plane, which yields

```
radius = R * sqrt(h * (2R + h)) / (2R + h)
```

For `h = 1.6 m` this is roughly 4.5 km. The class also exposes the line of
sight distance to the tangent point, the geocentric horizon angle, the arc
distance along the surface, the earth curvature drop and
`pointOnDome(azimuth, elevation)` to place points on the dome surface.

### Sun position

`SolarPosition` implements the NOAA solar position algorithm (accuracy of about
one arc minute). It provides azimuth (clockwise from north), geometric and
refraction corrected elevation, zenith angle, declination, hour angle, the
equation of time and the sun distance in astronomical units. The geocentric
result is converted to the topocentric frame by a diurnal parallax correction
that uses the ellipsoid terms of the earth model and the observer altitude.
`direction()` returns the unit vector to the sun in the local ENU frame,
`projectOnDome()` returns the corresponding point on the dome, and
`relativeIrradiance()` returns `cos(zenith)` as a first energy measure.

The calculation passes through several reference frames (geocentric ecliptic,
geocentric equatorial, hour angle and finally the topocentric horizontal frame
of the ground plane). See [SolarPosition.md](SolarPosition.md) for a detailed
description of these coordinate systems, their origins and the accuracy
trade-offs.

### Sun path

`SunPath` samples a whole UTC day (by default every 10 minutes) and stores the
time, the `SolarPosition` and the dome point of every sample. It offers the
visible arc as a point list for rendering, sunrise and sunset times refined by
bisection, the solar noon sample and a relative daily energy value.

### Example

```cpp
#include "geolib/SunPath.h"

using namespace geo;

GeoLocation home(48.1372, 11.5756);        // Munich
HorizonDome dome(home);                     // eye height 1.6 m

SunPath path(dome, 2024, 6, 21);
std::vector<Vector3> arc = path.arcPoints();

DateTimeUtc rise;
if (path.sunrise(rise)) {
    // rise holds the UTC sunrise time
}

SolarPosition noon = path.highestSample().position;
double elevation = noon.elevation();
```

## GUI application (planned)

A Qt based desktop application will be added as a second CMake subdirectory on
top of `geolib`. Planned functionality:

- Input of the location (latitude/longitude or map picking) and of the date.
- 3D view of the ground plane and the horizon dome with the sun path arc of the
  selected day and a draggable time slider.
- Placement of simple obstacle geometry (buildings, trees) on the ground plane.
- Shadow casting from `SolarPosition::direction()` onto the ground plane and
  onto the placed geometry.
- Energy level diagram over the day, based on the sun elevation and the shading
  of a configurable surface (for example a solar panel with a given tilt and
  orientation).
- Export of the computed curves.

## Roadmap

- Shadow casting helpers in `geolib`.
- Surface/panel model with tilt and azimuth for irradiance calculation.
- Local time and timezone handling on top of `DateTimeUtc`.
- Unit tests for the geometric and astronomical math.
- Qt GUI application.
