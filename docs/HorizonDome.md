# HorizonDome

`HorizonDome` places a half sphere on the ground plane, centred on the
standpoint. Its radius is the distance from the standpoint to the visible
horizon: from the observer eye at height `h` above the earth surface a tangent
to the earth sphere of radius `R` is drawn and intersected with the ground
plane, which yields

```
radius = R * sqrt(h * (2R + h)) / (2R + h)
```

The observer height `h = terrainHeight() + viewHeight()` combines the terrain
height of the standpoint above sea level with the eye height above the terrain
(default 1.6 m). Standing on flat ground at sea level this is roughly 4.5 km,
on a 500 m hill already about 80 km.

## Terrain height

The terrain height can be passed explicitly or fetched from elevation data:

```cpp
// Explicit terrain height.
HorizonDome dome(home, 1.6, 520.0);

// Terrain height from a specific height data source.
HorizonDome dome = HorizonDome::fromHeightDataSource(home, source, 1.6);

// Terrain height from the best source in the HeightDataSourceRegistry.
HorizonDome dome = HorizonDome::fromHeightDataSourceRegistry(home, 1.6);
```

If no source is given, or the source has no value for the location, the
altitude of the `GeoLocation` is used as terrain height.

## Further queries

The class also exposes the line of sight distance to the tangent point, the
geocentric horizon angle, the arc distance along the surface, the earth
curvature drop and `pointOnDome(azimuth, elevation)` to place points on the
dome surface.

## Related

- [GroundPlane.md](GroundPlane.md) - the plane the dome stands on.
- [SunPosition.md](SunPosition.md) - `projectOnDome()` puts the sun on the dome.
- [SunPath.md](SunPath.md) - the sun arc drawn on the dome.
