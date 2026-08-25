# Earth model

All computations of `geolib` go through the abstract `EarthModel` interface, so
the shape of the earth is a plug-in and never hard coded in the algorithms.

## Implementations

- `WGS84EarthModel` - the WGS84 reference ellipsoid (semi major axis
  a = 6378137 m, inverse flattening 1/f = 298.257223563). This is the model
  returned by `EarthModel::defaultModel()` and therefore used by every
  `GeoLocation` that is created without an explicit model.
- `SphericalEarthModel` - earth as a perfect sphere (mean radius 6371008.8 m),
  available via `EarthModel::sphericalModel()` and useful for simple estimates
  and for comparing the effect of the ellipsoid.

## Interface

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

## Related

- [GroundPlane.md](GroundPlane.md) - the tangential plane and the local radii of
  curvature it caches from the model.
- [HorizonDome.md](HorizonDome.md) - the horizon distance derived from the local
  radius.
