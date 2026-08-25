# Height data sources

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

## Source selection

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

## Bavaria DGM1

Concrete data set implementations live in their own subdirectory,
`geolib/include/geolib/data_sources` and `geolib/src/data_sources`, so that the
generic height data interfaces stay separated from the region specific
adapters.

The DGM1 support consists of three cooperating classes (the UTM projection
itself is part of the core library, see [Projections.md](Projections.md)):

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

## Germany: BKG DGM5

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

## World: Copernicus DEM GLO-30

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

## United Kingdom: Environment Agency LIDAR 1 m

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

## USA: USGS 3DEP 1 m

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

## Other data sets

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

## Related

- [Projections.md](Projections.md) - the grids the tiles are addressed in.
- [TerrainModel.md](TerrainModel.md) - turning the heights into a 3D mesh.
