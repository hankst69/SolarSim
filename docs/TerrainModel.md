# TerrainModel and shadows

`TerrainModel` turns a `HeightDataSource` into the 3D topology of the ground
plane. It samples the source on a regular grid in the local ENU frame and
builds a triangle mesh from it. The modelled area is limited to the extent of
the ground plane covered by the `HorizonDome`: the half size of the grid never
exceeds `HorizonDome::radius()`, and with `clipToDomeCircle` the area is cut to
the circular dome footprint instead of a square. `Config` further controls the
grid spacing, an upper limit of samples per axis (the spacing is coarsened
automatically if needed) and whether the earth `curvatureDrop()` is subtracted
from the sampled heights. Grid points without data are skipped, so holes in the
data set simply produce holes in the mesh.

Heights are stored relative to the ground plane. `heightAt(east, north)`
interpolates the grid bilinearly and `surfacePoint()` returns the corresponding
ENU point.

## Building model

On top of the topology an optional detailed building model can be placed in the
centre of the ground plane. `setBuildingModel(mesh, east, north)` takes a
`TriangleMesh` in its own local ENU coordinates and lifts it onto the terrain
height of the given footprint centre; `setBuildingBox()` is a shortcut for a
simple box shaped placeholder. Terrain and building are merged into
`sceneMesh()`, ready for rendering and ray casting.

## Shadow casting

Shadow casting traces a ray from a surface point towards the sun against that
scene mesh: `isInShadow(point, sunDirection)` and
`isSurfaceInShadow(east, north, sunDirection)` take the unit sun vector of
`SunPosition::direction()` and report whether the terrain or the building
blocks it. A sun below the ground plane always counts as shadowed.

```cpp
HeightDataSourcePtr source =
    HeightDataSourceRegistry::instance().selectSource(home);

TerrainModel::Config config;
config.extentM = 500.0;        // 500 m around the standpoint
config.gridSpacingM = 1.0;     // DGM1 native resolution

TerrainModel terrain(dome, source, config);
terrain.setBuildingBox(12.0, 8.0, 9.0);   // 12 x 8 m house, 9 m high

const Vector3 sun = noon.direction();
bool shadowed = terrain.isSurfaceInShadow(20.0, -15.0, sun);
```

## TriangleMesh

`TriangleMesh` itself is a small indexed mesh with `addVertex()`/`addTriangle()`,
`append()` for merging (optionally translated), an axis aligned `bounds()`
query, the `createBox()` helper and a Möller-Trumbore based `intersect()` /
`isOccluded()`. The intersection test is a linear scan over all triangles, which
is fine for interactive queries but should be replaced by a spatial acceleration
structure before computing dense shadow maps over a full day.

## Related

- [HeightDataSources.md](HeightDataSources.md) - where the heights come from.
- [HorizonDome.md](HorizonDome.md) - the limit of the modelled area.
- [SunPosition.md](SunPosition.md) - the sun direction used for shadow rays.
