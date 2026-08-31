<#
.SYNOPSIS
    Configures and builds the GeoSolar solution (geolib + unit tests + app) as
    WebAssembly using Emscripten.

.DESCRIPTION
    Wraps "emcmake cmake" / "emmake cmake --build" so the project can be built
    for the web with a single command. Requires the Emscripten SDK to be
    installed and activated (emsdk_env.ps1 dot-sourced, so emcc/emcmake/emmake
    are on PATH). The build tree is kept in the ignored "build" directory next
    to the sources.

.PARAMETER Configuration
    CMake build type, e.g. Debug or Release. Defaults to Release.

.PARAMETER BuildDir
    Build tree location. Defaults to <repo>/build/wasm-<Configuration>.

.PARAMETER NoTests
    Configure with BUILD_GEOLIB_TESTS=OFF, i.e. build the library only.

.PARAMETER WebGpu
    Configure with SOLARSIM_USE_WEBGPU=ON (else the software rasterizer is
    used).

.PARAMETER Clean
    Delete the build tree before configuring.

.EXAMPLE
    ./scripts/build-wasm.ps1

.EXAMPLE
    ./scripts/build-wasm.ps1 -Configuration Debug -Clean
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Release',
    [string]$BuildDir,
    [switch]$NoTests,
    [switch]$WebGpu,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot "build/wasm-$Configuration"
}

if (-not (Get-Command emcmake -ErrorAction SilentlyContinue) -or
    -not (Get-Command emmake -ErrorAction SilentlyContinue)) {
    throw 'emcmake/emmake were not found on PATH. Install the Emscripten SDK ' +
          '(https://emscripten.org/docs/getting_started/downloads.html) and ' +
          'dot-source emsdk_env.ps1 first.'
}

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Removing build tree $BuildDir" -ForegroundColor Cyan
    Remove-Item -Recurse -Force $BuildDir
}

$configureArgs = @('-S', $repoRoot, '-B', $BuildDir, "-DCMAKE_BUILD_TYPE=$Configuration")
if ($NoTests) {
    $configureArgs += '-DBUILD_GEOLIB_TESTS=OFF'
}
if ($WebGpu) {
    $configureArgs += '-DSOLARSIM_USE_WEBGPU=ON'
}
# Let CTest run the resulting .js tests directly through Node.
$node = Get-Command node -ErrorAction SilentlyContinue
if ($node) {
    $configureArgs += "-DCMAKE_CROSSCOMPILING_EMULATOR=$($node.Source)"
}

Write-Host "Configuring wasm ($Configuration) in $BuildDir" -ForegroundColor Cyan
emcmake cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE."
}

Write-Host "Building wasm ($Configuration)" -ForegroundColor Cyan
emmake cmake --build $BuildDir --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

Write-Host "Build succeeded: $BuildDir" -ForegroundColor Green
