#!/usr/bin/env bash
#
# Configures and builds the GeoSolar solution (geolib + unit tests).
#
# Wraps the CMake configure and build steps so the project can be built with a
# single command. The build tree is kept in the ignored "build" directory next
# to the sources.
#
# Usage:
#   ./scripts/build.sh [options]
#
# Options:
#   -c, --configuration <cfg>  CMake build type (Debug, Release, RelWithDebInfo,
#                              MinSizeRel). Default: Debug.
#   -g, --generator <gen>      CMake generator. Default: Ninja; falls back to the
#                              platform default generator if Ninja is missing.
#   -b, --build-dir <dir>      Build tree location. Default: <repo>/build/<cfg>.
#   -j, --jobs <n>             Parallel build jobs. Default: number of CPUs.
#       --no-tests             Configure with BUILD_GEOLIB_TESTS=OFF.
#       --clean                Delete the build tree before configuring.
#   -h, --help                 Show this help.
#
# Examples:
#   ./scripts/build.sh
#   ./scripts/build.sh --configuration Release --clean

set -euo pipefail

configuration="Debug"
generator="Ninja"
build_dir=""
jobs=""
no_tests=0
clean=0

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"

usage() {
    sed -n '3,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--configuration) configuration="$2"; shift 2 ;;
        -g|--generator)     generator="$2";     shift 2 ;;
        -b|--build-dir)     build_dir="$2";     shift 2 ;;
        -j|--jobs)          jobs="$2";          shift 2 ;;
        --no-tests)         no_tests=1;         shift ;;
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
    build_dir="$repo_root/build/$configuration"
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake was not found on PATH." >&2
    exit 1
fi

# Ninja is the generator used by the Windows/VS integration; fall back to the
# platform default if it is not installed.
if [[ "$generator" == "Ninja" ]] && ! command -v ninja >/dev/null 2>&1; then
    echo "Warning: Ninja was not found on PATH, using the default CMake generator instead." >&2
    generator=""
fi

if [[ -z "$jobs" ]]; then
    jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)"
fi

if [[ $clean -eq 1 && -d "$build_dir" ]]; then
    echo "Removing build tree $build_dir"
    rm -rf "$build_dir"
fi

configure_args=(-S "$repo_root" -B "$build_dir" "-DCMAKE_BUILD_TYPE=$configuration")
if [[ -n "$generator" ]]; then
    configure_args+=(-G "$generator")
fi
if [[ $no_tests -eq 1 ]]; then
    configure_args+=(-DBUILD_GEOLIB_TESTS=OFF)
fi

echo "Configuring ($configuration) in $build_dir"
cmake "${configure_args[@]}"

echo "Building ($configuration)"
cmake --build "$build_dir" --config "$configuration" --parallel "$jobs"

echo "Build succeeded: $build_dir"
