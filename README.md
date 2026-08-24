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
| `tests/geolib/AngleTests.cpp` | `degToRad` / `radToDeg` / `kPi` |
| `tests/geolib/Vector3Tests.cpp` | `Vector3` |
| `tests/geolib/DateTimeUtcTests.cpp` | `DateTimeUtc` |
| `tests/geolib/EarthModelTests.cpp` | `EarthModel`, `SphericalEarthModel`, `WGS84EarthModel` |
| `tests/geolib/GeoLocationTests.cpp` | `GeoLocation` |
| `tests/geolib/GroundPlaneTests.cpp` | `GroundPlane` |
| `tests/geolib/HorizonDomeTests.cpp` | `HorizonDome` |
| `tests/geolib/UtmProjectionTests.cpp` | `UtmProjection` / `Utm32Projection` |
| `tests/geolib/BritishNationalGridProjectionTests.cpp` | `BritishNationalGridProjection` |
| `tests/geolib/SolarPositionTests.cpp` | `SolarPosition` |
| `tests/geolib/SunPathTests.cpp` | `SunPath` |
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

### British National Grid projection

Not every national elevation model uses UTM: the British data products are
published on the British National Grid (OSGB36, EPSG:27700), which uses the Airy
1830 ellipsoid instead of WGS84. `BritishNationalGridProjection` therefore
combines the transverse Mercator projection of the grid with the Helmert datum
shift WGS84 -> OSGB36, so its interface takes and returns WGS84 coordinates just
like `UtmProjection`. The Helmert parameters are the standard OS values, giving
an accuracy of a few metres, well below the 1 m raster spacing of the LIDAR
data. In addition it converts between coordinates and the two letter codes of
the 100 km squares (`squareFor()` / `squareOrigin()`), which the tile names of
the British data sets are built from.

```cpp
double easting = 0.0, northing = 0.0;
BritishNationalGridProjection::forward(51.5074, -0.1278, easting, northing);  // London
const std::string square = BritishNationalGridProjection::squareFor(easting, northing); // "TQ"
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

#### Germany: BKG DGM5

The DGM1 only covers Bavaria, so the nation wide "Digitales Geländemodell
Gitterweite 5 m (DGM5)" of the Bundesamt für Kartographie und Geodäsie (BKG,
dl-de/by-2-0) fills the gaps for the rest of Germany. It uses the same UTM32
tiling as the DGM1 and therefore reuses `Utm32GridTile`; only the grid spacing
differs (5 m instead of 1 m):

- `GermanyDgm5TileReader` - parses the official `XYZ` delivery format and ESRI
  `ASC` ASCII grids. In addition to the DGM1 reader it normalises German number
  formatting: a comma is treated as a decimal comma when the line already
  consists of three tokens and as a column separator otherwise.
- `GermanyDgm5TileDownloader` - builds the tile file name (`dgm5_32_690_5334_2.xyz`)
  from the 1 km square, resolves it against a base URL and a local cache
  directory and hands the parsed result to the data source via `tileLoader()`.
  As for all sources the HTTP GET is an injected `FetchFunction`.
- `GermanyDgm5HeightDataSource` - projects the query to UTM32, derives the tile key
  of the containing 1 km square and caches loaded tiles including negative
  results. The border fallback uses the 5 m grid spacing as the width of the
  strip in which the interpolation stencil reaches into the neighbour.

```cpp
#include "geolib/data_sources/GermanyDgm5TileDownloader.h"

GermanyDgm5TileDownloader::Config config;
config.cacheDirectory = "C:/data/dgm5";

static GermanyDgm5TileDownloader downloader(config,
    [](const std::string& url, const std::string& target) {
        return myHttpGet(url, target);
    });

auto dgm5 = std::make_shared<GermanyDgm5HeightDataSource>(downloader.tileLoader());
HeightDataSourceRegistry::instance().addSource(dgm5);
```

Because the registry ranks by resolution, registering both German sources gives
the 1 m DGM1 inside Bavaria and the 5 m DGM5 everywhere else in Germany
automatically.

#### World: Copernicus DEM GLO-30

The global counterpart follows exactly the same three class pattern, but works
on the geographic 1 deg x 1 deg tiles of the Copernicus DEM GLO-30 data set
(ESA/Airbus, ~30 m ground sample distance):

- `WorldCopernicusDem30TileReader` - parses a tile into a `GridHeightDataSource`.
  Two formats are supported: the plain `HGT` raster many GLO-30 mirrors provide
  (big endian signed 16 bit samples, northernmost row first, square, without
  any georeference, so the bounds are supplied by the caller) and ESRI `ASC`
  ASCII grids in degrees. Both formats already store the northernmost row
  first, which matches the row order of `GridHeightDataSource`.
- `WorldCopernicusDem30TileDownloader` - builds the official tile name
  (`Copernicus_DSM_COG_10_N48_00_E011_00_DEM`), resolves it against a base URL
  and a local cache directory and hands the parsed tile to the data source via
  `tileLoader()`. As above the HTTP GET is an injected `FetchFunction`.
- `WorldCopernicusDem30HeightDataSource` - derives the tile key of the containing
  degree square (`tileKeyFor()`), caches loaded tiles including negative
  results (tiles simply do not exist over open water) and falls back to the
  neighbouring tiles for samples exactly on a tile border.

```cpp
#include "geolib/data_sources/WorldCopernicusDem30TileDownloader.h"

WorldCopernicusDem30TileDownloader::Config config;
config.cacheDirectory = "C:/data/glo30";

static WorldCopernicusDem30TileDownloader downloader(config,
    [](const std::string& url, const std::string& target) {
        return myHttpGet(url, target);
    });

auto glo30 = std::make_shared<WorldCopernicusDem30HeightDataSource>(downloader.tileLoader());
HeightDataSourceRegistry::instance().addSource(glo30);
```

Its coverage is the whole world at a 30 m resolution, so the registry prefers
the finer regional sources (such as DGM1) wherever they are available and uses
GLO-30 as the global fallback.

#### United Kingdom: Environment Agency LIDAR 1 m

The English open data set "LIDAR Composite DTM 1 m" of the Environment Agency
(published under the Open Government Licence) follows the same pattern, but on
the British National Grid:

- `BngGridTile` - one elevation raster tile kept in its **native** National Grid
  coordinates. Geodetic queries are projected with
  `BritishNationalGridProjection` before the bilinear interpolation, so the 1 m
  raster is never resampled. Row 0 is the southernmost row.
- `UkEaLidarTileReader` - parses the tile files. The official delivery format is
  an ESRI `ASC` ASCII grid (`ncols`/`nrows`/`xllcorner`/`yllcorner`/`cellsize`/
  `NODATA_value`, with `xllcenter`/`yllcenter` accepted as well); plain `XYZ`
  triples as produced by conversion tools are supported too. ASCII grids store
  the northernmost row first, so the reader flips them to the south-up ordering
  of `BngGridTile`.
- `UkEaLidarTileDownloader` - builds the official tile file name from the 5 km
  block name (`sp50ne` -> `LIDARCOMP-DTM-1M-sp50ne.asc`), resolves it against a
  base URL and a local cache directory and hands the parsed result to the data
  source via `tileLoader()`. The HTTP GET is again an injected `FetchFunction`.

```cpp
#include "geolib/data_sources/UkEaLidarTileDownloader.h"

UkEaLidarTileDownloader::Config config;
config.cacheDirectory = "C:/data/ea_lidar";

static UkEaLidarTileDownloader downloader(config,
    [](const std::string& url, const std::string& target) {
        return myHttpGet(url, target);
    });

auto lidar = std::make_shared<UkEaLidarHeightDataSource>(downloader.tileLoader());
HeightDataSourceRegistry::instance().addSource(lidar);
```

`UkEaLidarHeightDataSource` projects the query to the National Grid, snaps it to
the containing 5 km block (`tileKeyFor()`) and asks the `TileLoader` for the
tile. As with the other sources loaded tiles are cached including negative
results, and tile borders fall back to the neighbouring tiles. Note that the
composite does not cover the whole bounding box of England and Wales; locations
without data are reported as unavailable so the registry can fall back to a
coarser source such as GLO-30.

#### USA: USGS 3DEP 1 m

The US "3D Elevation Program" 1 meter DEM of the USGS (public domain, published
via The National Map) follows the same pattern, but on the UTM grid of the
project area. Because the conterminous US spans the zones 10 to 19, the zone is
part of the tile and of the tile key:

- `UtmGridTile` - one elevation raster tile kept in its **native** UTM grid,
  including the zone it belongs to. Geodetic queries are projected with
  `UtmProjection` of that zone before the bilinear interpolation, so the 1 m
  raster is never resampled. Row 0 is the southernmost row.
- `UsaUsgs3Dep1mTileReader` - parses the tile files: ESRI `ASC` ASCII grids
  (`ncols`/`nrows`/`xllcorner`/`yllcorner`/`cellsize`/`NODATA_value`, with
  `xllcenter`/`yllcenter` accepted as well) and plain `XYZ` triples as produced
  by the common GeoTIFF conversion tools. Neither format carries the UTM zone,
  so it is supplied by the caller. ASCII grids store the northernmost row
  first, so the reader flips them to the south-up ordering of `UtmGridTile`.
- `UsaUsgs3Dep1mTileDownloader` - builds the official tile file name from the
  zone and the 10 km block (`USGS_1M_16_x54y4400.asc`), resolves it against a
  base URL and a local cache directory and hands the parsed result to the data
  source via `tileLoader()`. The HTTP GET is again an injected `FetchFunction`.

```cpp
#include "geolib/data_sources/UsaUsgs3Dep1mTileDownloader.h"

UsaUsgs3Dep1mTileDownloader::Config config;
config.cacheDirectory = "C:/data/usgs_3dep";

static UsaUsgs3Dep1mTileDownloader downloader(config,
    [](const std::string& url, const std::string& target) {
        return myHttpGet(url, target);
    });

auto dep3 = std::make_shared<UsaUsgs3Dep1mHeightDataSource>(downloader.tileLoader());
HeightDataSourceRegistry::instance().addSource(dep3);
```

`UsaUsgs3Dep1mHeightDataSource` determines the UTM zone of the query
(`toUtm()`), snaps the projected position to the containing 10 km block
(`tileKeyFor()`) and asks the `TileLoader` for the tile. Loaded tiles are cached
including negative results, and tile borders fall back to the neighbouring tiles
of the same zone. 3DEP 1 m coverage is not complete; locations without data are
reported as unavailable so the registry can fall back to a coarser source such
as GLO-30.

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

GeoLocation home(49.56255, 11.14493);  // Bavaria, Germany
HorizonDome dome(home);                // eye height 1.6 m

SunPath path(dome, 2026, 8, 23);
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
- Qt GUI application.
