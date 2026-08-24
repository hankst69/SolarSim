<#
.SYNOPSIS
    Configures and builds the GeoSolar solution (geolib + unit tests).

.DESCRIPTION
    Wraps the CMake configure and build steps so the project can be built with a
    single command. The build tree is kept in the ignored "build" directory next
    to the sources.

    Run this from a Visual Studio developer prompt (or any shell that has the
    compiler and Ninja on PATH).

.PARAMETER Configuration
    CMake build type, e.g. Debug or Release. Defaults to Debug.

.PARAMETER Generator
    CMake generator to use. Defaults to Ninja; falls back to the platform
    default generator if Ninja is not available.

.PARAMETER BuildDir
    Build tree location. Defaults to <repo>/build/<Configuration>.

.PARAMETER NoTests
    Configure with GEOSOLAR_BUILD_TESTS=OFF, i.e. build the library only.

.PARAMETER Clean
    Delete the build tree before configuring.

.EXAMPLE
    ./scripts/win_build.ps1

.EXAMPLE
    ./scripts/win_build.ps1 -Configuration Release -Clean
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Debug',
    [string]$Generator = 'Ninja',
    [string]$BuildDir,
    [switch]$NoTests,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot "build/$Configuration"
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'cmake was not found on PATH. Run this from a Visual Studio developer prompt.'
}

# Ninja is the generator used by the CMakePresets/VS integration; fall back to
# the platform default if it is not installed.
if ($Generator -eq 'Ninja' -and -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    Write-Warning 'Ninja was not found on PATH, using the default CMake generator instead.'
    $Generator = ''
}

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Removing build tree $BuildDir" -ForegroundColor Cyan
    Remove-Item -Recurse -Force $BuildDir
}

$configureArgs = @('-S', $repoRoot, '-B', $BuildDir, "-DCMAKE_BUILD_TYPE=$Configuration")
if ($Generator) {
    $configureArgs += @('-G', $Generator)
}
if ($NoTests) {
    $configureArgs += '-DGEOSOLAR_BUILD_TESTS=OFF'
}

Write-Host "Configuring ($Configuration) in $BuildDir" -ForegroundColor Cyan
cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE."
}

Write-Host "Building ($Configuration)" -ForegroundColor Cyan
cmake --build $BuildDir --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

Write-Host "Build succeeded: $BuildDir" -ForegroundColor Green
