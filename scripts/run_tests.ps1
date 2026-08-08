#!/usr/bin/env pwsh
# ASCIICraft Test Runner
# Configures, builds and runs the ASCIICraft test suite via CTest.
#
# ASCIICraft consumes the packaged ASCIIgL distribution (never the sibling
# source tree), so if the distribution is missing this script builds and
# deploys it first via build_ASCIIgL_ASCIICraft.ps1. That makes
# `git clone --recursive` followed by this script a complete bootstrap.

param(
    [ValidateSet("FastDebug", "Release")]
    [string]$Config = "FastDebug",

    # ctest -R : run only tests matching this regex
    [string]$Filter = "",

    # ctest -L : run only tests carrying this label (world, tier1, tier2, ...)
    [string]$Label = "",

    # Build with MSVC AddressSanitizer into a separate build dir
    [switch]$Asan,

    # Include the stress suite (excluded by default)
    [switch]$Stress,

    # Skip configure+build and go straight to ctest
    [switch]$NoBuild,

    # Force a rebuild+redeploy of the ASCIIgL distribution before testing
    [switch]$RebuildEngine,

    [string]$Generator = "Visual Studio 18 2026",

    # ctest -j : parallel test jobs. 0 = let ctest decide
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"

$BaseDir       = Split-Path -Parent $PSScriptRoot
$GameDir       = Join-Path $BaseDir "ASCIICraft"
$ASCIIgLConfig = Join-Path $GameDir "lib\ASCIIgL-v1.0.0\cmake\ASCIIgLConfig.cmake"

if ($Asan) {
    $BuildDir = Join-Path $GameDir "build-asan"
} else {
    $BuildDir = Join-Path $GameDir "build"
}

Write-Host "ASCIICraft Test Runner" -ForegroundColor Green
Write-Host "  Config:    $Config"    -ForegroundColor Cyan
Write-Host "  Build dir: $BuildDir"  -ForegroundColor Cyan
if ($Asan) { Write-Host "  ASan:      ENABLED" -ForegroundColor Yellow }

try {
    # -----------------------------------------------------------------------
    # Step 1: Submodules (doctest, entt, oneTBB, tracy, ...)
    # -----------------------------------------------------------------------
    Write-Host "`n=== Step 1: Submodules ===" -ForegroundColor Magenta
    git -C $BaseDir submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw "git submodule update failed with exit code $LASTEXITCODE" }

    # -----------------------------------------------------------------------
    # Step 2: ASCIIgL distribution
    #
    # The dist is gitignored, so a clean clone has none and CMake's
    # find_package(ASCIIgL REQUIRED) would fail at configure time.
    # -----------------------------------------------------------------------
    Write-Host "`n=== Step 2: ASCIIgL distribution ===" -ForegroundColor Magenta
    if ($RebuildEngine -or -not (Test-Path $ASCIIgLConfig)) {
        if ($RebuildEngine) {
            Write-Host "Rebuild requested - building and deploying ASCIIgL..." -ForegroundColor Yellow
        } else {
            Write-Host "Distribution not found - building and deploying ASCIIgL..." -ForegroundColor Yellow
        }
        & (Join-Path $PSScriptRoot "build_ASCIIgL_ASCIICraft.ps1")
        if ($LASTEXITCODE -ne 0) { throw "ASCIIgL distribution build failed with exit code $LASTEXITCODE" }
        if (-not (Test-Path $ASCIIgLConfig)) { throw "Distribution build reported success but $ASCIIgLConfig is still missing" }
    } else {
        Write-Host "Distribution present. Use -RebuildEngine after changing engine code." -ForegroundColor Cyan
    }

    # -----------------------------------------------------------------------
    # Step 3: Configure + build
    # -----------------------------------------------------------------------
    if (-not $NoBuild) {
        Write-Host "`n=== Step 3: Configure ===" -ForegroundColor Magenta
        $cmakeArgs = @("-S", $GameDir, "-B", $BuildDir, "-G", $Generator, "-DASCIICRAFT_BUILD_TESTS=ON")
        if ($Asan) { $cmakeArgs += "-DASCIICRAFT_TESTS_ASAN=ON" }
        & cmake @cmakeArgs
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }

        Write-Host "`n=== Step 4: Build ASCIICraft_tests ===" -ForegroundColor Magenta
        & cmake --build $BuildDir --config $Config --target ASCIICraft_tests
        if ($LASTEXITCODE -ne 0) { throw "Test build failed with exit code $LASTEXITCODE" }
    } else {
        Write-Host "`n=== Steps 3-4 skipped (-NoBuild) ===" -ForegroundColor Yellow
    }

    # -----------------------------------------------------------------------
    # Step 5: ASan runtime
    #
    # The exe fails to start with 0xC0000135 unless clang_rt.asan_dynamic is on
    # PATH. oneTBB is an uninstrumented DLL, so container-overflow checks throw
    # false positives at the instrumented/uninstrumented boundary.
    # -----------------------------------------------------------------------
    if ($Asan) {
        Write-Host "`n=== Step 5: ASan runtime ===" -ForegroundColor Magenta
        if ($env:VCToolsInstallDir) {
            $asanDir = Join-Path $env:VCToolsInstallDir "bin\Hostx64\x64"
            if (Test-Path $asanDir) {
                $env:PATH = "$asanDir;$env:PATH"
                Write-Host "Added to PATH: $asanDir" -ForegroundColor Cyan
            } else {
                Write-Host "WARNING: $asanDir not found; the test exe may fail to start." -ForegroundColor Yellow
            }
        } else {
            Write-Host "WARNING: VCToolsInstallDir not set. Run from a Developer PowerShell so the ASan runtime resolves." -ForegroundColor Yellow
        }
        $env:ASAN_OPTIONS = "detect_container_overflow=0:abort_on_error=1:print_stacktrace=1"
        Write-Host "ASAN_OPTIONS=$env:ASAN_OPTIONS" -ForegroundColor Cyan
    }

    # -----------------------------------------------------------------------
    # Step 6: Run
    # -----------------------------------------------------------------------
    Write-Host "`n=== Step 6: CTest ===" -ForegroundColor Magenta
    $ctestArgs = @("--test-dir", $BuildDir, "-C", $Config, "--output-on-failure")
    if ($Jobs -gt 0)  { $ctestArgs += @("-j", $Jobs) }
    if ($Filter)      { $ctestArgs += @("-R", $Filter) }
    if ($Label)       { $ctestArgs += @("-L", $Label) }
    if (-not $Stress) { $ctestArgs += @("-LE", "stress") }

    & ctest @ctestArgs
    $testExit = $LASTEXITCODE

    if ($testExit -eq 0) {
        Write-Host "`n=== Tests passed ===" -ForegroundColor Green
    } else {
        Write-Host "`n=== Tests FAILED (exit code $testExit) ===" -ForegroundColor Red
    }
    exit $testExit

} catch {
    Write-Host "`nERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
