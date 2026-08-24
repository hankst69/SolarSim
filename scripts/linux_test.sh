#!/usr/bin/env bash
#
# Builds the GeoSolar solution and runs the geolib unit tests with CTest.
#
# Calls scripts/linux_build.sh and then executes the test suites registered in
# tests/CMakeLists.txt. Each suite is a standalone executable that returns a
# non zero exit code on failure, so a non zero exit code of this script means
# at least one test failed.
#
# Usage:
#   ./scripts/linux_test.sh [options]
#
# Options:
#   -c, --configuration <cfg>  CMake build type (Debug, Release, RelWithDebInfo,
#                              MinSizeRel). Default: Debug.
#   -b, --build-dir <dir>      Build tree location. Default: <repo>/build/<cfg>.
#   -f, --filter <regex>       Regular expression selecting the tests to run,
#                              passed to "ctest -R". Runs all tests if omitted.
#   -j, --jobs <n>             Parallel build jobs. Default: number of CPUs.
#       --clean                Delete the build tree before configuring.
#       --skip-build           Run the tests against the existing build tree.
#   -h, --help                 Show this help.
#
# Examples:
#   ./scripts/linux_test.sh
#   ./scripts/linux_test.sh --filter UsaUsgs3Dep1m
#   ./scripts/linux_test.sh --configuration Release --clean

set -euo pipefail

configuration="Debug"
build_dir=""
filter=""
jobs=""
clean=0
skip_build=0

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(dirname "$script_dir")"

usage() {
    sed -n '3,28p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -c|--configuration) configuration="$2"; shift 2 ;;
        -b|--build-dir)     build_dir="$2";     shift 2 ;;
        -f|--filter)        filter="$2";        shift 2 ;;
        -j|--jobs)          jobs="$2";          shift 2 ;;
        --clean)            clean=1;            shift ;;
        --skip-build)       skip_build=1;       shift ;;
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

if [[ $skip_build -eq 0 ]]; then
    build_args=(--configuration "$configuration" --build-dir "$build_dir")
    if [[ $clean -eq 1 ]]; then
        build_args+=(--clean)
    fi
    if [[ -n "$jobs" ]]; then
        build_args+=(--jobs "$jobs")
    fi
    "$script_dir/linux_build.sh" "${build_args[@]}"
fi

if [[ ! -d "$build_dir" ]]; then
    echo "Build tree $build_dir does not exist. Run without --skip-build first." >&2
    exit 1
fi

if ! command -v ctest >/dev/null 2>&1; then
    echo "ctest was not found on PATH." >&2
    exit 1
fi

ctest_args=(--test-dir "$build_dir" --build-config "$configuration" --output-on-failure)
if [[ -n "$filter" ]]; then
    ctest_args+=(-R "$filter")
fi

echo "Running tests ($configuration)"
if ! ctest "${ctest_args[@]}"; then
    echo "Some tests failed."
    exit 1
fi

echo "All tests passed."
