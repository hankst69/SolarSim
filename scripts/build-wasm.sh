#!/usr/bin/env bash
#
# Configures and builds the GeoSolar solution (geolib + unit tests + app) as
# WebAssembly using Emscripten.
#
# Wraps "emcmake cmake" / "emmake cmake --build" so the project can be built
# for the web with a single command. Requires the Emscripten SDK to be
# installed and activated (emsdk_env.sh sourced, so emcc/emcmake/emmake are on
# PATH). The build tree is kept in the ignored "build" directory next to the
# sources.
#
# Usage:
#   ./scripts/build-wasm.sh [options]
#
# Options:
#   -c, --configuration <cfg>  CMake build type (Debug, Release, RelWithDebInfo,
#                              MinSizeRel). Default: Release.
#   -b, --build-dir <dir>      Build tree location. Default: <repo>/build/wasm-<cfg>.
#   -j, --jobs <n>             Parallel build jobs. Default: number of CPUs.
#       --no-tests             Configure with BUILD_GEOLIB_TESTS=OFF.
#       --webgpu                Configure with SOLARSIM_USE_WEBGPU=ON (else the
#                              software rasterizer is used).
#       --clean                Delete the build tree before configuring.
#   -h, --help                 Show this help.
#
# Examples:
#   ./scripts/build-wasm.sh
#   ./scripts/build-wasm.sh --configuration Debug --clean

set -euo pipefail

configuration="Release"
build_dir=""
jobs=""
no_tests=0
webgpu=0
clean=0

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"

usage() {
    sed -n '3,27p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--configuration) configuration="$2"; shift 2 ;;
        -b|--build-dir)     build_dir="$2";     shift 2 ;;
        -j|--jobs)          jobs="$2";          shift 2 ;;
        --no-tests)         no_tests=1;         shift ;;
        --webgpu)           webgpu=1;           shift ;;
        --clean)            clean=1;            shift ;;
        -h|--help)          usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

case "$configuration" in
    Debug|Release|RelWithDebInfo|MinSizeRel) ;;
    *) echo "Invalid configuration: $configuration" >&2; exit 2 ;;
esac

if [[ -z "$build_dir" ]]; then
    build_dir="$repo_root/build/wasm-$configuration"
fi

if ! command -v emcmake >/dev/null 2>&1 || ! command -v emmake >/dev/null 2>&1; then
    echo "emcmake/emmake were not found on PATH. Install the Emscripten SDK" >&2
    echo "(https://emscripten.org/docs/getting_started/downloads.html) and" >&2
    echo "source emsdk_env.sh first." >&2
    exit 1
fi

if [[ -z "$jobs" ]]; then
    jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)"
fi

if [[ $clean -eq 1 && -d "$build_dir" ]]; then
    echo "Removing build tree $build_dir"
    rm -rf "$build_dir"
fi

configure_args=(-S "$repo_root" -B "$build_dir" "-DCMAKE_BUILD_TYPE=$configuration")
if [[ $no_tests -eq 1 ]]; then
    configure_args+=(-DBUILD_GEOLIB_TESTS=OFF)
fi
if [[ $webgpu -eq 1 ]]; then
    configure_args+=(-DSOLARSIM_USE_WEBGPU=ON)
fi
# Let CTest run the resulting .js tests directly through Node.
if command -v node >/dev/null 2>&1; then
    configure_args+=(-DCMAKE_CROSSCOMPILING_EMULATOR="$(command -v node)")
fi

echo "Configuring wasm ($configuration) in $build_dir"
emcmake cmake "${configure_args[@]}"

echo "Building wasm ($configuration)"
emmake cmake --build "$build_dir" --config "$configuration" --parallel "$jobs"

echo "Build succeeded: $build_dir"
