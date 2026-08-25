
`SunLight` turns a [`SunPosition`](SunPosition.md) into a light source that a
renderer can use directly. It is the light model of the SolarSim GUI
application.

## Parallel rays

The sun is roughly 150 million kilometres away, so for a scene that spans a few
hundred metres its rays are parallel for all practical purposes. `SunLight`
therefore does not model a point light but a **flat, square area** whose normal
is the direction to the sun:

- `directionToSun()` is the unit vector from the scene towards the sun; it is
  exactly `SunPosition::direction()` in the local east/north/up frame.
- `rayDirection()` is its negation, i.e. the direction the light travels in.
- Every ray leaving the rectangle has that same direction - there is no
  divergence and no falloff with distance.

## Size and placement

The rectangle is sized so that it covers the whole visible scene. When it is
built from a [`TerrainModel`](TerrainModel.md), `coveredRadius()` computes the
radius of the bounding sphere of the scene mesh around the local origin and the
half size becomes that radius times `kCoverageMargin` (5 % extra):

```cpp
const TerrainModel terrain(dome, source, config);
const SunLight light(terrain, DateTimeUtc(2026, 8, 23, 10, 0, 0));

const Vector3 toSun = light.directionToSun();
const double edgeLength = light.size();       // metres
```

The centre sits at `distance()` metres from the scene centre along
`directionToSun()`, far enough that the rectangle never intersects the terrain
even for a very low sun. `axisU()` and `axisV()` span the rectangle plane and
form an orthonormal frame together with the sun direction, so `cornerAt(u, v)`
with `u`, `v` in `[-1, 1]` walks the whole light area.

An explicit sun position and scene radius can be used as well:

```cpp
const SunPosition sun(home, utc);
const SunLight light(sun, 400.0);   // 400 m scene radius
```

## Using it in a renderer

| Member | Use |
| --- | --- |
| `rayDirection()` | Direction of a directional light. |
| `intensity()` | Relative irradiance (cosine of the zenith angle), 0 at night. |
| `isAboveHorizon()` | False while only the ambient term should be applied. |
| `rayOriginFor(p)` | Emission point on the rectangle for a scene point `p`. |
| `nearPlane()` / `farPlane()` | Depth range of an orthographic shadow projection. |
| `axisU()` / `axisV()` / `halfSize()` | Extent of that shadow projection. |

Shadows themselves come from the terrain: `TerrainModel::isInShadow(point,
light.directionToSun())` traces the ray back to the sun and reports whether the
terrain or the building model blocks it.

## Related

- [SunPosition.md](SunPosition.md) - azimuth, elevation and the direction vector.
- [SunPath.md](SunPath.md) - the sun over a whole day.
- [TerrainModel.md](TerrainModel.md) - scene mesh and shadow queries.
- [CameraPosition.md](CameraPosition.md) - the viewer of the same scene.
