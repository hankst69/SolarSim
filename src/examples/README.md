# `geolib/samples`

## `HeightDataSourcesSample`

This sample console application shows how to:

- create concrete tile downloaders for `BavariaDgm1` and `WorldCopernicusDem30`
- create matching `HeightDataSource` instances
- register them in `HeightDataSourceRegistry`
- let the registry prefer the finer Bavarian source over the global Copernicus source

The sample source file is:

- `HeightDataSourcesSample.cpp`

## What it registers

The sample registers these sources in this order:

1. `FlatHeightDataSource` as a fallback
2. `WorldCopernicusDem30HeightDataSource`
3. `BavariaDgm1HeightDataSource`

Because the registry selects the best available resolution for a location, Bavaria should use `BavariaDgm1`, while locations outside Bavaria can fall back to `WorldCopernicusDem30`.

## Download implementation

The sample uses `WinHTTP` on Windows for real downloads.

Configured caches:

- `height_data_cache/bavaria_dgm1`
- `height_data_cache/world_copernicus_dem30`

## Copernicus credentials

`WorldCopernicusDem30` may require an account depending on the endpoint being used.
The sample currently contains placeholders in `HeightDataSourcesSample.cpp`:

- `kCopernicusUserName`
- `kCopernicusPassword`

Replace them with real values if required by your download endpoint.

## Build

The sample is added from `geolib/CMakeLists.txt` and builds as:

- `HeightDataSourcesSample`

Example:

- `cmake --build build/Debug --target HeightDataSourcesSample`

## Run

Example output flow:

- print the registered sources
- sample a point in Bamberg, Germany
- sample a point in Paris, France

If no tile is cached yet, the sample attempts to download it.
