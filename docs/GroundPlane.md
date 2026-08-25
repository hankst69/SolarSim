# GroundPlane

`GeoLocation::groundPlane()` returns the tangential plane touching the earth
surface below the location. Its origin is the standpoint and its axes are the
local east/north/up (ENU) frame. `GroundPlane` converts between ECEF and this
local frame and reports the signed height of a point above the plane.

## Curvature

The plane also caches the local radii of curvature of the earth model at its
origin (`meridionalRadius()`, `primeVerticalRadius()`). Based on them,
`curvatureDrop(east, north)` returns how far the curved surface falls below the
tangential plane at a local offset, and `toGeoLocation(local)` maps a local ENU
coordinate back to latitude/longitude/altitude with the correct north/south and
east/west scaling of the ellipsoid.

```cpp
GeoLocation home(49.56255, 11.14493);
GroundPlane plane = home.groundPlane();

Vector3 local = plane.toLocal(other);          // ENU coordinates of another location
double drop = plane.curvatureDrop(5000.0, 0.0); // ~2 m at 5 km distance
GeoLocation back = plane.toGeoLocation(local);
```

## Related

- [EarthModel.md](EarthModel.md) - the radii of curvature the plane is built on.
- [HorizonDome.md](HorizonDome.md) - the half sphere standing on this plane.
- [TerrainModel.md](TerrainModel.md) - the height field sampled in this frame.
