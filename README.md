# SolarSim

Simulation of solar light, shadows and energy levels for a given location on
earth and a given day.

The project is split into a reusable C++ class library (`geolib`) that contains
all geo-geometrical and astronomical math, and a Qt GUI application (`app`)
that visualizes the results.

## Building

The project uses CMake (minimum 3.16) and C++17.

```
cmake -S . -B build
cmake --build build
```

On Windows `scripts/win_build.ps1` wraps both steps (run it from a Visual Studio
developer prompt so the compiler and Ninja are on `PATH`):

```
./scripts/win_build.ps1                          # Debug build in build/Debug
./scripts/win_build.ps1 -Configuration Release   # Release build in build/Release
./scripts/win_build.ps1 -Clean                   # rebuild from scratch
./scripts/win_build.ps1 -NoTests                 # library only
```

On Linux `scripts/linux_build.sh` does the same:

```
./scripts/linux_build.sh                         # Debug build in build/Debug
./scripts/linux_build.sh -c Release              # Release build in build/Release
./scripts/linux_build.sh --clean                 # rebuild from scratch
./scripts/linux_build.sh --no-tests              # library only
```

## Tests

The unit tests are built by default and registered with CTest:

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

`scripts/win_test.ps1` (Windows) and `scripts/linux_test.sh` (Linux) build and
run the suites in one step:

```
./scripts/win_test.ps1                           # build + run all tests
./scripts/win_test.ps1 -Filter UsaUsgs3Dep1m     # run a subset (ctest -R)
./scripts/win_test.ps1 -SkipBuild                # reuse the existing build tree
```

```
./scripts/linux_test.sh                          # build + run all tests
./scripts/linux_test.sh -f UsaUsgs3Dep1m         # run a subset (ctest -R)
./scripts/linux_test.sh --skip-build             # reuse the existing build tree
```

Both exit with a non zero code if any suite fails, so they can be used in CI.
Pass `-h` / `--help` to either script for the full option list.

Set `-DGEOSOLAR_BUILD_TESTS=OFF` to skip them. The test tree mirrors the
library layout, so a test sits in the same relative place as the code it
covers:

| Test suite | Covers |
| --- | --- |
| `tests/geolib/AngleTests.cpp` | `degToRad` / `radToDeg` / `kPi` |
| `tests/geolib/Vector3Tests.cpp` | `Vector3` |
| `tests/geolib/DateTimeUtcTests.cpp` | `DateTimeUtc` |
| `tests/geolib/EarthModelTests.cpp` | `EarthModel`, `SphericalEarthModel`, `WGS84EarthModel` |
| `tests/geolib/GeoLocationTests.cpp` | `GeoLocation` |
| `tests/geolib/GroundPlaneTests.cpp` | `GroundPlane` |
| `tests/geolib/HorizonDomeTests.cpp` | `HorizonDome` |
| `tests/geolib/UtmProjectionTests.cpp` | `UtmProjection` / `Utm32Projection` |
| `tests/geolib/BritishNationalGridProjectionTests.cpp` | `BritishNationalGridProjection` |
| `tests/geolib/SunPositionTests.cpp` | `SunPosition` |
| `tests/geolib/SunPathTests.cpp` | `SunPath` |
| `tests/geolib/SunEnergyTests.cpp` | `SunEnergy` |
| `tests/geolib/SunLightTests.cpp` | `SunLight` |
| `tests/geolib/TriangleMeshTests.cpp` | `TriangleMesh` |
| `tests/geolib/HeightDataSourceTests.cpp` | `GeoBounds`, `FlatHeightDataSource`, `GridHeightDataSource`, `HeightDataSourceRegistry` |
| `tests/geolib/TerrainModelTests.cpp` | `TerrainModel` |
| `tests/geolib/data_sources/Utm32GridTileTests.cpp` | `Utm32GridTile` |
| `tests/geolib/data_sources/BavariaDgm1TileReaderTests.cpp` | `BavariaDgm1TileReader` |
| `tests/geolib/data_sources/BavariaDgm1HeightDataSourceTests.cpp` | `BavariaDgm1HeightDataSource` |
| `tests/geolib/data_sources/BavariaDgm1TileDownloaderTests.cpp` | `BavariaDgm1TileDownloader` |
| `tests/geolib/data_sources/GermanyDgm5TileReaderTests.cpp` | `GermanyDgm5TileReader` |
| `tests/geolib/data_sources/GermanyDgm5HeightDataSourceTests.cpp` | `GermanyDgm5HeightDataSource` |
| `tests/geolib/data_sources/GermanyDgm5TileDownloaderTests.cpp` | `GermanyDgm5TileDownloader` |
| `tests/geolib/data_sources/WorldCopernicusDem30TileReaderTests.cpp` | `WorldCopernicusDem30TileReader` |
| `tests/geolib/data_sources/WorldCopernicusDem30HeightDataSourceTests.cpp` | `WorldCopernicusDem30HeightDataSource` |
| `tests/geolib/data_sources/WorldCopernicusDem30TileDownloaderTests.cpp` | `WorldCopernicusDem30TileDownloader` |
| `tests/geolib/data_sources/BngGridTileTests.cpp` | `BngGridTile` |
| `tests/geolib/data_sources/UkEaLidarTileReaderTests.cpp` | `UkEaLidarTileReader` |
| `tests/geolib/data_sources/UkEaLidarHeightDataSourceTests.cpp` | `UkEaLidarHeightDataSource` |
| `tests/geolib/data_sources/UkEaLidarTileDownloaderTests.cpp` | `UkEaLidarTileDownloader` |
| `tests/geolib/data_sources/UtmGridTileTests.cpp` | `UtmGridTile` |
| `tests/geolib/data_sources/UsaUsgs3Dep1mTileReaderTests.cpp` | `UsaUsgs3Dep1mTileReader` |
| `tests/geolib/data_sources/UsaUsgs3Dep1mHeightDataSourceTests.cpp` | `UsaUsgs3Dep1mHeightDataSource` |
| `tests/geolib/data_sources/UsaUsgs3Dep1mTileDownloaderTests.cpp` | `UsaUsgs3Dep1mTileDownloader` |

Each suite is a small standalone executable that prints its failures and
returns a non zero exit code. `tests/TestSupport.h` provides the `CHECK_*`
assertion macros, so the tests stay as dependency free as the library itself.
The astronomical suites check against published reference values (solstice
declinations, the equation of time, sunrise/sunset times, perihelion and
aphelion distances), and the terrain suites use analytic height sources plus
injected loader/fetch callbacks, so nothing ever touches the network.

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
| `UtmProjection` | `UtmProjection.h` | Forward/inverse UTM (transverse Mercator) projection for any zone. |
| `Utm32Projection` | `UtmProjection.h` | Convenience accessor for UTM zone 32N (EPSG:25832). |
| `BritishNationalGridProjection` | `BritishNationalGridProjection.h` | British National Grid (OSGB36, EPSG:27700) incl. datum shift and square codes. |
| `HorizonDome` | `HorizonDome.h` | Half sphere standing on the ground plane, reaching to the visible horizon. |
| `DateTimeUtc` | `DateTimeUtc.h` | UTC date/time with Julian day and Julian century conversion. |
| `SunPosition` | `SunPosition.h` | Sun position for a location and UTC time, projected onto the dome. |
| `SunPath` | `SunPath.h` | Samples `SunPosition` across a day to produce the sun arc on the dome. |
| `SunEnergy` | `SunEnergy.h` | Solar irradiance in W/m^2: theoretical maximum for a date and realistic value for a location, time and plane inclination. |
| `SunLight` | `SunLight.h` | Directional light source approximating the sun as a flat area with parallel rays covering the scene. |
| `TriangleMesh` | `TriangleMesh.h` | Indexed triangle mesh in the local ENU frame with ray/triangle intersection. |
| `TerrainModel` | `TerrainModel.h` | Height field of the ground plane built from height data, plus shadow queries. |
| `GeoBounds` | `HeightDataSource.h` | Latitude/longitude bounding box describing the coverage of a data source. |
| `HeightDataSource` | `HeightDataSource.h` | Abstract provider of terrain heights (digital elevation/terrain model). |
| `FlatHeightDataSource` | `GridHeightDataSource.h` | World wide fallback source returning a constant height. |
| `GridHeightDataSource` | `GridHeightDataSource.h` | Height source backed by an in-memory latitude/longitude raster. |
| `HeightDataSourceRegistry` | `HeightDataSourceRegistry.h` | Registry of data sources with location based source selection. |
| `BavariaDgm1HeightDataSource` | `data_sources/BavariaDgm1HeightDataSource.h` | Bavarian open data DGM1 (1 m) source with UTM32 tiling. |
| `BavariaDgm1TileReader` | `data_sources/BavariaDgm1TileReader.h` | Parser for the DGM1 tile files (XYZ and ESRI ASCII grid). |
| `BavariaDgm1TileDownloader` | `data_sources/BavariaDgm1TileDownloader.h` | Tile naming, local cache and download of DGM1 tiles. |
| `GermanyDgm5HeightDataSource` | `data_sources/GermanyDgm5HeightDataSource.h` | Nation wide German BKG DGM5 (5 m) source with UTM32 tiling. |
| `GermanyDgm5TileReader` | `data_sources/GermanyDgm5TileReader.h` | Parser for the DGM5 tile files (XYZ incl. decimal comma, and ESRI ASCII grid). |
| `GermanyDgm5TileDownloader` | `data_sources/GermanyDgm5TileDownloader.h` | Tile naming, local cache and download of DGM5 tiles. |
| `WorldCopernicusDem30HeightDataSource` | `data_sources/WorldCopernicusDem30HeightDataSource.h` | Global Copernicus DEM GLO-30 (30 m) source with 1 deg tiling. |
| `WorldCopernicusDem30TileReader` | `data_sources/WorldCopernicusDem30TileReader.h` | Parser for the GLO-30 tile files (HGT and ESRI ASCII grid). |
| `WorldCopernicusDem30TileDownloader` | `data_sources/WorldCopernicusDem30TileDownloader.h` | Tile naming, local cache and download of GLO-30 tiles. |
| `UkEaLidarHeightDataSource` | `data_sources/UkEaLidarHeightDataSource.h` | Environment Agency LIDAR Composite DTM (1 m) source with BNG tiling. |
| `UkEaLidarTileReader` | `data_sources/UkEaLidarTileReader.h` | Parser for the EA LIDAR tile files (ESRI ASCII grid and XYZ). |
| `UkEaLidarTileDownloader` | `data_sources/UkEaLidarTileDownloader.h` | Tile naming, local cache and download of EA LIDAR tiles. |
| `UsaUsgs3Dep1mHeightDataSource` | `data_sources/UsaUsgs3Dep1mHeightDataSource.h` | USGS 3DEP 1 m DEM source with 10 km tiling in the local UTM zone. |
| `UsaUsgs3Dep1mTileReader` | `data_sources/UsaUsgs3Dep1mTileReader.h` | Parser for the 3DEP 1 m tile files (ESRI ASCII grid and XYZ). |
| `UsaUsgs3Dep1mTileDownloader` | `data_sources/UsaUsgs3Dep1mTileDownloader.h` | Tile naming, local cache and download of 3DEP 1 m tiles. |
| `BngGridTile` | `data_sources/BngGridTile.h` | Elevation raster tile in its native British National Grid. |
| `Utm32GridTile` | `data_sources/Utm32GridTile.h` | Elevation raster tile in its native UTM32 grid. |
| `UtmGridTile` | `data_sources/UtmGridTile.h` | Elevation raster tile in its native UTM grid, zone stored per tile. |

### Documentation

Detailed documentation of the individual classes lives in the [`docs`](docs)
folder:

| Document | Content |
| --- | --- |
| [docs/EarthModel.md](docs/EarthModel.md) | Earth shape interface, WGS84 ellipsoid and sphere. |
| [docs/GroundPlane.md](docs/GroundPlane.md) | Tangential plane, local ENU frame, curvature drop. |
| [docs/HorizonDome.md](docs/HorizonDome.md) | Horizon distance and the dome the sun path is drawn on. |
| [docs/CameraPosition.md](docs/CameraPosition.md) | Initial camera placement for a location or a date/time. |
| [docs/Projections.md](docs/Projections.md) | UTM and British National Grid projections. |
| [docs/SunPosition.md](docs/SunPosition.md) | NOAA solar position algorithm and its coordinate systems. |
| [docs/SunPath.md](docs/SunPath.md) | Sampling the sun over a day, sunrise/sunset, solar noon. |
| [docs/SunEnergy.md](docs/SunEnergy.md) | Irradiance in W/m^2: orbit distance, air mass, plane inclination. |
| [docs/SunLight.md](docs/SunLight.md) | Sun as a directional area light for rendering. |
| [docs/HeightDataSources.md](docs/HeightDataSources.md) | Height data interface, registry and the concrete data sets. |
| [docs/TerrainModel.md](docs/TerrainModel.md) | Terrain mesh, building model, shadow casting, `TriangleMesh`. |

### Overview

- **Earth model** - all math goes through the abstract `EarthModel`;
  `WGS84EarthModel` is the default, `SphericalEarthModel` an alternative.
  See [docs/EarthModel.md](docs/EarthModel.md).
- **Ground plane** - `GeoLocation::groundPlane()` returns the tangential plane
  at the standpoint with its local east/north/up frame.
  See [docs/GroundPlane.md](docs/GroundPlane.md).
- **Horizon dome** - `HorizonDome` is the half sphere on that plane reaching to
  the visible horizon. See [docs/HorizonDome.md](docs/HorizonDome.md).
- **Camera** - `CameraPosition` places the viewer above the terrain, either by
  the hemisphere rule or on the line to the sun. `fromOrbit()`, `orbited()` and
  `zoomed()` move it interactively around the standpoint.
  See [docs/CameraPosition.md](docs/CameraPosition.md).
- **Projections** - `UtmProjection` and `BritishNationalGridProjection` address
  the tiles of the national elevation data sets.
  See [docs/Projections.md](docs/Projections.md).
- **Sun** - `SunPosition` computes azimuth and elevation, `SunPath` samples a
  whole day, `SunEnergy` turns the geometry into W/m^2 and `SunLight` turns it
  into a directional light for the renderer.
  See [docs/SunPosition.md](docs/SunPosition.md),
  [docs/SunPath.md](docs/SunPath.md), [docs/SunEnergy.md](docs/SunEnergy.md)
  and [docs/SunLight.md](docs/SunLight.md).
- **Height data** - `HeightDataSource`, `HeightDataSourceRegistry` and the
  region specific readers/downloaders provide terrain elevation.
  See [docs/HeightDataSources.md](docs/HeightDataSources.md).
- **Terrain and shadows** - `TerrainModel` builds the mesh of the ground plane
  and answers shadow queries with `TriangleMesh`.
  See [docs/TerrainModel.md](docs/TerrainModel.md).

### Example

```cpp
#include "geolib/SunEnergy.h"
#include "geolib/SunPath.h"

using namespace geo;

GeoLocation home(49.56255, 11.14493);  // Bavaria, Germany
HorizonDome dome(home);                // eye height 1.6 m

SunPath path(dome, 2026, 8, 23);
std::vector<Vector3> arc = path.arcPoints();

DateTimeUtc rise;
if (path.sunrise(rise)) {
    // rise holds the UTC sunrise time
}

SunPosition noon = path.highestSample().position;
double elevation = noon.elevation();

SunEnergy energy(home, noon.time());
double watts = energy.groundIrradiance();   // W/m^2 on flat ground
```

Terrain topology and shadow casting:

```cpp
#include "geolib/HeightDataSourceRegistry.h"
#include "geolib/TerrainModel.h"

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

## SolarSim GUI application

`app` contains the Qt Widgets GUI on top of `geolib`. It renders the terrain
around a location and lights it with the simulated sun.

The application is built when Qt (Widgets, Qt6 preferred, Qt5 as fallback) is
found by CMake; otherwise it is skipped with a status message and only the
library and the tests are built. Set `-DGEOSOLAR_BUILD_APP=OFF` to skip it
explicitly.

On start it shows the location of the example above (49.56255, 11.14493) for
the current date:

- The scene is the `TerrainModel` of the standpoint, sampled from the best
  `HeightDataSource` covering it (the flat fallback if nothing else is
  registered).
- Lighting combines a weak ambient/sky term with the `SunLight` of the current
  time, including terrain cast shadows from `TerrainModel::isInShadow()`.
- The **time slider** scrolls from sunrise to sunset of the selected date, both
  taken from `SunPath`. The date can be changed with the date picker.
- The **camera** is a `CameraPosition` orbiting the standpoint: drag with the
  left mouse button to rotate, use the mouse wheel to zoom, or enter azimuth,
  elevation and distance directly.

Planned additions:

- Input of the location (latitude/longitude or map picking).
- Drawing of the sun path arc on the horizon dome.
- Placement of simple obstacle geometry (buildings, trees) on the ground plane.
- Energy level diagram over the day, based on the sun elevation and the shading
  of a configurable surface (for example a solar panel with a given tilt and
  orientation), and export of the computed curves.

## Roadmap

- Binary tile formats (GeoTIFF, SRTM HGT) for the existing tile readers.
- Readers/downloaders for further height data sets.
- Spatial acceleration structure (BVH) for the mesh ray casting.
- Import of detailed building models (for example CityGML LoD2 or OBJ).
- Surface/panel model with tilt and azimuth for irradiance calculation.
- Local time and timezone handling on top of `DateTimeUtc`.
- Qt GUI application.
