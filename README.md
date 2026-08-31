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

On Windows `scripts/build.ps1` wraps both steps (run it from a Visual Studio
developer prompt so the compiler and Ninja are on `PATH`):

```
./scripts/build.ps1                          # Debug build in build/Debug
./scripts/build.ps1 -Configuration Release   # Release build in build/Release
./scripts/build.ps1 -Clean                   # rebuild from scratch
./scripts/build.ps1 -NoTests                 # library only
```

On Linux `scripts/build.sh` does the same:

```
./scripts/build.sh                         # Debug build in build/Debug
./scripts/build.sh -c Release              # Release build in build/Release
./scripts/build.sh --clean                 # rebuild from scratch
./scripts/build.sh --no-tests              # library only
```

## Tests

The unit tests are built by default and registered with CTest:

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

`scripts/test.ps1` (Windows) and `scripts/test.sh` (Linux) build and
run the suites in one step:

```
./scripts/test.ps1                           # build + run all tests
./scripts/test.ps1 -Filter UsaUsgs3Dep1m     # run a subset (ctest -R)
./scripts/test.ps1 -SkipBuild                # reuse the existing build tree
```

```
./scripts/test.sh                          # build + run all tests
./scripts/test.sh -f UsaUsgs3Dep1m         # run a subset (ctest -R)
./scripts/test.sh --skip-build             # reuse the existing build tree
```

Both exit with a non zero code if any suite fails, so they can be used in CI.
Pass `-h` / `--help` to either script for the full option list.

Set `-DBUILD_GEOLIB_TESTS=OFF` to skip them.


## geolib

[`geolib` is a dependency free C++17 static library.](docs/README_geolib.md)

All types live in the `geo` namespace, headers are located under `geolib/include/geolib`.

Find detailed documentation in [docs/README_geolib.md](docs/README_geolib.md).


## SolarSim GUI application

`app` contains the Qt Widgets GUI on top of `geolib`. It renders the terrain
around a location and lights it with the simulated sun.

The application is built when Qt (Widgets, Qt6 preferred, Qt5 as fallback) is
found by CMake; otherwise it is skipped with a status message and only the
library and the tests are built. Set `-DBUILD_SOLARSIM_APP=OFF` to skip it
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

### Hardware accelerated rendering (WebGPU, optional)

By default the scene is rendered with a small software rasterizer
(`SceneView`'s painter's algorithm), which needs no extra dependencies. For
larger terrains an optional GPU backend (`GpuSceneRenderer`) is available,
built on the [WebGPU](https://www.w3.org/TR/webgpu/) API via its C header
`webgpu.h`. Because WebGPU itself is implemented on top of Vulkan/Metal/D3D12
on desktop and natively by the browser on the web, the same rendering code
works unchanged for both a native build and a WebAssembly build (via
Emscripten), without depending on OS specific graphics APIs.

Enable it with:

```
cmake -S . -B build -DSOLARSIM_USE_WEBGPU=ON
```

- **Native desktop**: requires a native WebGPU implementation such as
  [wgpu-native](https://github.com/gfx-rs/wgpu-native) or
  [Dawn](https://dawn.googlesource.com/dawn) installed/built with CMake
  package support, discoverable via `find_package(webgpu)` (point
  `CMAKE_PREFIX_PATH` at its install directory). Windows (HWND) surface
  creation is implemented; Linux/macOS surface creation is a follow-up.
- **WebAssembly**: configure with the Emscripten toolchain
  (`emcmake cmake -S . -B build-wasm -DSOLARSIM_USE_WEBGPU=ON`) and build with
  `emmake cmake --build build-wasm`; Emscripten's built-in WebGPU support is
  used automatically, rendering into the page's `#canvas` element.

When `SOLARSIM_USE_WEBGPU` is `OFF` (the default), none of this code is
compiled and the application behaves exactly as before.

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
