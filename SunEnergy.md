# SunEnergy - irradiance model

`geo::SunEnergy` turns the geometry of `SunPosition` into a power per area, in
watt per square metre. It answers two questions:

1. **What is physically available on that date?** The theoretical maximum, i.e.
   the irradiance on a surface that is perpendicular to the sun rays, at sea
   level, with no atmosphere at all. It depends on the date only, through the
   earth - sun distance of the elliptical orbit.
2. **What actually arrives at a given place and time?** The realistic value for
   a `GeoLocation` at a UTC time, which subtracts the damping of the atmosphere
   (the light path through the air grows towards the horizon) and the cosine
   loss caused by the inclination of the receiving plane.

This document describes the model as implemented in `geolib/src/SunEnergy.cpp`.

## 1. The theoretical maximum: inverse square law

The **solar constant** `E0 = 1361 W/m^2` (`SunEnergy::kSolarConstant`, CODATA /
WMO value) is the irradiance perpendicular to the rays at a distance of exactly
one astronomical unit, outside the atmosphere.

The earth orbit is an ellipse with an eccentricity of about 0.0167, so the
distance `d` varies over the year. `SunPosition::sunDistanceAu()` provides `d`
in astronomical units for the given date (radius vector of the Keplerian
orbit), and the irradiance follows the inverse square law:

```
theoreticalIrradiance = E0 / d^2
```

| Date | Event | `d` [AU] | `theoreticalIrradiance` [W/m^2] |
| --- | --- | --- | --- |
| ~3 January | perihelion (closest) | 0.9833 | ~1408 |
| ~4 April / ~5 October | mean distance | 1.0000 | 1361 |
| ~5 July | aphelion (farthest) | 1.0167 | ~1316 |

The seasonal swing is therefore about **+/- 3.4 %** - notably the northern
hemisphere winter is the time with the *strongest* available radiation, the
seasons are caused by the sun elevation, not by the distance.

This value needs no location, so it is available from the date-only
constructor:

```cpp
SunEnergy january(DateTimeUtc(2026, 1, 3));
double max = january.theoreticalIrradiance();   // ~1408 W/m^2
```

## 2. Atmospheric damping: optical air mass

With a `GeoLocation` the class also holds a `SunPosition` and can model the
path the light has to take through the atmosphere. The measure for that path is
the **relative optical air mass** `AM`: the ratio between the actual path
length and the path length for a sun in the zenith. `AM = 1` in the zenith,
`AM ~ 2` at 30 degrees elevation, `AM ~ 38` at the horizon.

A plane parallel atmosphere would give `AM = 1 / sin(elevation)`, which diverges
at the horizon. The implementation uses the **Kasten and Young (1989)** formula
instead, which accounts for the curvature of the atmosphere:

```
AM = 1 / ( sin(h) + 0.50572 * (h + 6.07995)^-1.6364 )       h in degrees
```

`h` is the **refracted** elevation (`SunPosition::refractedElevation()`),
because the refracted ray is the path the light really takes. For a sun below
the horizon `AM` is reported as 0 and all irradiances become 0.

The fraction of the radiation that survives the path is modelled by
**Beer-Lambert** with a clear sky transmittance per air mass
`tau = 0.7` (`SunEnergy::kZenithTransmittance`, a common clear sky value for
sea level):

```
atmosphericTransmittance = tau ^ AM
```

so about 0.70 in the zenith, 0.49 at 30 degrees elevation and practically zero
at the horizon. This is the **damping due to the length of the light path**
through the atmosphere; it grows exactly where the ground plane is strongly
inclined relative to the sun rays.

The result is the **direct normal irradiance** (DNI), the power on a surface
that is tracking the sun at sea level:

```
directNormalIrradiance = theoreticalIrradiance * atmosphericTransmittance
```

## 3. Inclination of the receiving plane: cosine law

A plane that is not perpendicular to the rays spreads the same beam over a
larger area. With `n` the unit normal of the plane and `s` the unit direction
to the sun (both in the local east/north/up frame of the location):

```
incidenceFactor = max(0, n . s)
irradiance      = directNormalIrradiance * incidenceFactor
```

The clamp at zero covers a sun behind the plane; a sun below the horizon is
zero anyway through the transmittance.

Three convenience forms are offered:

| Method | Plane |
| --- | --- |
| `groundIrradiance()` | the horizontal plane of the location (`n = up`) |
| `irradianceOnPlane(n)` | arbitrary normal in the local ENU frame |
| `irradianceOnTiltedPlane(tilt, azimuth)` | tilt against the horizontal and facing azimuth in degrees, clockwise from north (same convention as `SunPosition::azimuth()`) |
| `irradianceOnGroundPlane(plane)` | normal of a `GroundPlane` (given in ECEF), converted into the local frame |

For a tilted plane the normal is built as

```
n = ( sin(tilt) * sin(azimuth), sin(tilt) * cos(azimuth), cos(tilt) )
```

with the axes being east, north and up. Setting `tilt = zenithAngle` and
`azimuth = sun azimuth` makes the plane face the sun, and the result is exactly
the direct normal irradiance - this identity is checked by the unit tests.

## 4. Putting it together

```
                     E0
   theoretical  =  ------      (date, elliptical orbit)
                    d^2

   DNI          =  theoretical * tau^AM       (atmospheric path length)

   on a plane   =  DNI * max(0, n . s)        (plane inclination)
```

```cpp
GeoLocation munich(48.1372, 11.5756);
SunEnergy energy(munich, DateTimeUtc(2026, 6, 21, 11, 0, 0.0));

energy.theoreticalIrradiance();          // ~1322 W/m^2, date only
energy.airMass();                        // ~1.1
energy.atmosphericTransmittance();        // ~0.68
energy.directNormalIrradiance();          // ~900 W/m^2 facing the sun
energy.groundIrradiance();                // ~870 W/m^2 on flat ground
energy.irradianceOnTiltedPlane(30, 180);  // south facing 30 deg roof
```

## Scope and limitations

The model is deliberately analytic and dependency free, matching the rest of
`geolib`:

- **Direct beam only.** Diffuse sky radiation and ground reflection (albedo)
  are not included, so the value for an inclined or shaded plane is a lower
  bound. On a clear day the diffuse part is roughly 10-20 % of the global
  irradiance, on an overcast day it is everything.
- **Clear sky, sea level.** `tau = 0.7` is a fixed average; real turbidity,
  water vapour and clouds are not modelled. Altitude is not taken into account
  in the transmittance (thinner air above a mountain would raise it).
- **No terrain shading.** Whether the standpoint actually sees the sun is a
  question for `HorizonDome` / `TerrainModel`, not for this class.
- The geometry inherits the accuracy of `SunPosition` (about one arc minute),
  see [SunPosition.md](SunPosition.md).

The class is therefore a good measure for the *potential* of a location, date
and orientation, and for comparing them against each other, but it is not a
replacement for a full clear sky model such as Bird or Ineichen.
