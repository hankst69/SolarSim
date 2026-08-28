<#
.SYNOPSIS
    Builds the GeoSolar solution and runs the geolib unit tests with CTest.

.DESCRIPTION
    Calls scripts/build.ps1 and then executes the test suites registered in
    tests/CMakeLists.txt. Each suite is a standalone executable that returns a
    non zero exit code on failure, so a non zero exit code of this script means
    at least one test failed.

    Run this from a Visual Studio developer prompt (or any shell that has the
    compiler and Ninja on PATH).

.PARAMETER Configuration
    CMake build type, e.g. Debug or Release. Defaults to Debug.

.PARAMETER BuildDir
    Build tree location. Defaults to <repo>/build/<Configuration>.

.PARAMETER Filter
    Regular expression selecting the tests to run, passed to "ctest -R".
    Runs all tests if omitted.

.PARAMETER Clean
    Delete the build tree before configuring.

.PARAMETER SkipBuild
    Run the tests against the existing build tree without rebuilding.

.EXAMPLE
    ./scripts/test.ps1

.EXAMPLE
    ./scripts/test.ps1 -Filter UsaUsgs3Dep1m

.EXAMPLE
    ./scripts/test.ps1 -Configuration Release -Clean
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Configuration = 'Debug',
    [string]$BuildDir,
    [string]$Filter,
    [switch]$Clean,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot "build/$Configuration"
}

if (-not $SkipBuild) {
    $buildScript = Join-Path $PSScriptRoot 'build.ps1'
    & $buildScript -Configuration $Configuration -BuildDir $BuildDir -Clean:$Clean
}

$TestsDir = Join-Path $BuildDir "tests"

if (-not (Test-Path $TestsDir)) {
    throw "Build tree $BuildDir does not exist. Run without -SkipBuild first."
}

if (-not (Get-Command ctest -ErrorAction SilentlyContinue)) {
    throw 'ctest was not found on PATH. Run this from a Visual Studio developer prompt.'
}

$ctestArgs = @('--test-dir', $TestsDir, '--build-config', $Configuration, '--output-on-failure')
if ($Filter) {
    $ctestArgs += @('-R', $Filter)
}

Write-Host
Write-Host "Running tests ($Configuration)" -ForegroundColor Cyan
Write-Host "ctest $ctestArgs"

ctest @ctestArgs
$testExitCode = $LASTEXITCODE

if ($testExitCode -ne 0) {
    Write-Host 'Some tests failed.' -ForegroundColor Red
    exit $testExitCode
}

Write-Host 'All tests passed.' -ForegroundColor Green
