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

## Tests

The unit tests are built by default and registered with CTest:

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Set `-DGEOSOLAR_BUILD_TESTS=OFF` to skip them. The test tree mirrors the
library layout, so a test sits in the same relative place as the code it
covers:

| Test suite | Covers |
| --- | --- |
| `tests/geolib/UtmProjectionTests.cpp` | `UtmProjection` / `Utm32Projection` |
| `tests/geolib/data_sources/Utm32GridTileTests.cpp` | `Utm32GridTile` |
| `tests/geolib/data_sources/BavariaDgm1TileReaderTests.cpp` | `BavariaDgm1TileReader` |
| `tests/geolib/data_sources/BavariaDgm1HeightDataSourceTests.cpp` | `BavariaDgm1HeightDataSource` |
| `tests/geolib/data_sources/BavariaDgm1TileDownloaderTests.cpp` | `BavariaDgm1TileDownloader` |

Each suite is a small standalone executable that prints its failures and
returns a non zero exit code. `tests/TestSupport.h` provides the `CHECK_*`
assertion macros, so the tests stay as dependency free as the library itself.
The data source and downloader tests use injected loader/fetch callbacks and
therefore never touch the network.

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
| `HorizonDome` | `HorizonDome.h` | Half sphere standing on the ground plane, reaching to the visible horizon. |
| `DateTimeUtc` | `DateTimeUtc.h` | UTC date/time with Julian day and Julian century conversion. |
| `SolarPosition` | `SolarPosition.h` | Sun position for a location and UTC time, projected onto the dome. |
| `SunPath` | `SunPath.h` | Samples `SolarPosition` across a day to produce the sun arc on the dome. |
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
| `Utm32GridTile` | `data_sources/Utm32GridTile.h` | Elevation raster tile in its native UTM32 grid. |

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

### UTM projection

`UtmProjection` implements the transverse Mercator projection used by UTM on the
WGS84/ETRS89 ellipsoid. The zone enters only through its central meridian
(`6 * zone - 183`), so one implementation serves all 60 zones;
`zoneForLongitude()` picks the right one. `forward()` maps latitude/longitude to
easting/northing, `inverse()` maps back, with a round trip accurate to roughly
1e-9 degrees.

The projection lives in the core of `geolib` rather than next to a single data
set, because many national elevation models are published in UTM: zone 32N
(EPSG:25832) for Germany, Austria and Denmark, zone 31N for France, zones 32-35
for the Nordics and zones 10-19 for the USA. `Utm32Projection` is a static
convenience wrapper for zone 32N.

```cpp
double easting = 0.0, northing = 0.0;
Utm32Projection::forward(48.1372, 11.5756, easting, northing);   // Munich

UtmProjection zone31(31);
zone31.forward(48.8566, 2.3522, easting, northing);              // Paris
```

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

### Height data sources

Terrain heights are read through the abstract `HeightDataSource` interface. A
source reports its `name()`, the geographic area it can deliver values for
(`coverage()` as a `GeoBounds` box), its nominal ground sample distance
(`resolutionM()`) and answers `sampleHeight(lat, lon, height)` with the height
in metres above the reference surface. Returning `false` marks a data gap, a
missing tile or a location outside the coverage.

Two generic implementations are provided:

- `FlatHeightDataSource` - constant height for the whole world, used as a
  fallback when no real elevation data is available.
- `GridHeightDataSource` - a regular latitude/longitude raster held in memory
  with bilinear interpolation and no-data handling. Row 0 is the northernmost
  row, which matches the layout of most DEM raster formats. Decoding and
  downloading of a data set is deliberately outside of `geolib`: an application
  specific reader (GeoTIFF, XYZ, HGT, ...) produces the raster and hands it to
  this class.

#### Source selection

`HeightDataSourceRegistry` holds the available sources and selects one for a
given standpoint. `sourcesFor(lat, lon)` returns all sources covering the
location sorted by resolution (finest first), `selectSource()` returns the best
one or `nullptr`. The registry returned by `instance()` is pre-filled with the
flat fallback source; applications add their own sources on top:

```cpp
#include "geolib/data_sources/BavariaDgm1HeightDataSource.h"

auto& registry = HeightDataSourceRegistry::instance();
registry.addSource(std::make_shared<BavariaDgm1HeightDataSource>(tileLoader));

HeightDataSourcePtr source = registry.selectSource(home);
```

#### Bavaria DGM1

Concrete data set implementations live in their own subdirectory,
`geolib/include/geolib/data_sources` and `geolib/src/data_sources`, so that the
generic height data interfaces stay separated from the region specific
adapters.

The DGM1 support consists of three cooperating classes (the UTM projection
itself is part of the core library, see above):

- `Utm32GridTile` - one elevation raster tile kept in its **native** projected
  grid. Incoming geodetic coordinates are projected to UTM (via the core
  `Utm32Projection`) before the bilinear interpolation, so the 1 m grid is never
  resampled. Row 0 is the southernmost row.
- `BavariaDgm1TileReader` - parses the tile files. Two text formats are
  supported: the official `XYZ` delivery format (one `easting northing height`
  triple per line; the grid geometry is derived from the sample spacing) and
  ESRI `ASC` ASCII grids as produced by common conversion tools
  (`ncols`/`nrows`/`xllcorner`/`yllcorner`/`cellsize`/`NODATA_value`, with
  `xllcenter`/`yllcenter` accepted as well). ASCII grids store the northernmost
  row first, so the reader flips them to the south-up ordering of
  `Utm32GridTile`.
- `BavariaDgm1TileDownloader` - builds the official file name of a tile
  (`dgm1_32_690_5334_1_by.tif`), resolves it against a base URL and a local
  cache directory, downloads missing tiles and hands the parsed result to the
  data source via `tileLoader()`.

`geolib` stays dependency free, so the downloader does not implement HTTP
itself: the actual GET is injected as a `FetchFunction` callback that the
application implements with libcurl, Qt Network, WinHTTP or similar. With
`allowDownload = false` (or no callback at all) the downloader works purely on
an already populated local tile directory.

```cpp
#include "geolib/data_sources/BavariaDgm1TileDownloader.h"

BavariaDgm1TileDownloader::Config config;
config.cacheDirectory = "C:/data/dgm1";
config.fileExtension = ".xyz";

static BavariaDgm1TileDownloader downloader(config,
    [](const std::string& url, const std::string& target) {
        return myHttpGet(url, target);   // libcurl, Qt, WinHTTP, ...
    });

auto dgm1 = std::make_shared<BavariaDgm1HeightDataSource>(downloader.tileLoader());
HeightDataSourceRegistry::instance().addSource(dgm1);
```

`BavariaDgm1HeightDataSource` itself projects the query to UTM32, derives the
tile key of the containing 1 km square (`tileKeyFor()`, e.g. `690_5334`) and
asks the `TileLoader` for the tile. Loaded tiles are cached, including negative
results, so a missing tile is not requested twice. Near a tile border the
interpolation stencil reaches across the edge; in that case the neighbouring
tiles are consulted before the sample is reported as unavailable.

The downloader owns the tile loader it returns, so it has to outlive the data
source (hence the `static` in the example above).

#### Other data sets

The same pattern can be used for other regions. Suggested open data sets:

| Area | Data set | Resolution |
| --- | --- | --- |
| Bavaria (DE) | LDBV openData DGM1 | 1 m |
| Germany | BKG DGM5 / DGM10 | 5 m / 10 m |
| Austria | data.gv.at ALS DGM | 1 m |
| Switzerland | swissALTI3D | 0.5 m |
| France | IGN RGE ALTI | 1 m |
| Netherlands | AHN | 0.5 m |
| United Kingdom | Environment Agency LIDAR | 1 m |
| Sweden / Norway / Finland | Lantmäteriet, Kartverket, NLS LiDAR | 1-2 m |
| USA | USGS 3DEP | 1 m / 10 m |
| Canada | HRDEM | 1-2 m |
| Europe | EU-DEM (Copernicus Land Monitoring Service) | 25 m |
| World | Copernicus DEM GLO-30, NASADEM / SRTM, ALOS AW3D30, ASTER GDEM | 30 m |

### Terrain model and shadows

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

On top of the topology an optional detailed building model can be placed in the
centre of the ground plane. `setBuildingModel(mesh, east, north)` takes a
`TriangleMesh` in its own local ENU coordinates and lifts it onto the terrain
height of the given footprint centre; `setBuildingBox()` is a shortcut for a
simple box shaped placeholder. Terrain and building are merged into
`sceneMesh()`, ready for rendering and ray casting.

Shadow casting traces a ray from a surface point towards the sun against that
scene mesh: `isInShadow(point, sunDirection)` and
`isSurfaceInShadow(east, north, sunDirection)` take the unit sun vector of
`SolarPosition::direction()` and report whether the terrain or the building
blocks it. A sun below the ground plane always counts as shadowed.

`TriangleMesh` itself is a small indexed mesh with `addVertex()`/`addTriangle()`,
`append()` for merging (optionally translated), an axis aligned `bounds()`
query, the `createBox()` helper and a Möller-Trumbore based `intersect()` /
`isOccluded()`. The intersection test is a linear scan over all triangles, which
is fine for interactive queries but should be replaced by a spatial acceleration
structure before computing dense shadow maps over a full day.

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

## GUI application (planned)

A Qt based desktop application will be added as a second CMake subdirectory on
top of `geolib`. Planned functionality:

- Input of the location (latitude/longitude or map picking) and of the date.
- 3D view of the ground plane and the horizon dome with the sun path arc of the
  selected day and a draggable time slider.
- Rendering of the `TerrainModel` mesh and of the building model.
- Placement of simple obstacle geometry (buildings, trees) on the ground plane.
- Energy level diagram over the day, based on the sun elevation and the shading
  of a configurable surface (for example a solar panel with a given tilt and
  orientation).
- Export of the computed curves.

## Roadmap

- Binary tile formats (GeoTIFF, SRTM HGT) for the existing tile readers.
- Readers/downloaders for further height data sets.
- Spatial acceleration structure (BVH) for the mesh ray casting.
- Import of detailed building models (for example CityGML LoD2 or OBJ).
- Surface/panel model with tilt and azimuth for irradiance calculation.
- Local time and timezone handling on top of `DateTimeUtc`.
- Unit tests for the geometric and astronomical math.
- Qt GUI application.
