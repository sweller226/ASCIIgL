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

# Runs a native command with ErrorActionPreference relaxed, then checks its exit code.
#
# Necessary because PowerShell 5.1 wraps a native executable's stderr in
# NativeCommandError records; under "Stop" that makes any warning on stderr a
# terminating error. cmake warns on stderr routinely (oneTBB emits one), so without
# this a successful configure aborts the script.
function Invoke-Native {
    param([string]$What, [scriptblock]$Command)
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $Command
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($LASTEXITCODE -ne 0) { throw "$What failed with exit code $LASTEXITCODE" }
}

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
    Invoke-Native 'git submodule update' { git -C $BaseDir submodule update --init --recursive }

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
        Invoke-Native 'ASCIIgL distribution build' { & (Join-Path $PSScriptRoot "build_ASCIIgL_ASCIICraft.ps1") }
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
        Invoke-Native 'CMake configure' { & cmake @cmakeArgs }

        Write-Host "`n=== Step 4: Build ASCIICraft_tests ===" -ForegroundColor Magenta
        Invoke-Native 'Test build' { & cmake --build $BuildDir --config $Config --target ASCIICraft_tests }
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

        # clang_rt.asan_dynamic must be on PATH or the exe dies at startup with
        # 0xC0000135 (DLL not found). VCToolsInstallDir is only set inside a Developer
        # PowerShell, so fall back to locating the toolset with vswhere.
        $toolsDir = $env:VCToolsInstallDir
        if (-not $toolsDir) {
            $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
            if (Test-Path $vswhere) {
                $vsPath = & $vswhere -latest -property installationPath
                $verFile = Join-Path $vsPath "VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt"
                if ($vsPath -and (Test-Path $verFile)) {
                    $toolsVer = (Get-Content $verFile -Raw).Trim()
                    $toolsDir = Join-Path $vsPath "VC\Tools\MSVC\$toolsVer"
                    Write-Host "Located toolset via vswhere: $toolsDir" -ForegroundColor Cyan
                }
            }
        }

        if ($toolsDir) {
            $asanDir = Join-Path $toolsDir "bin\Hostx64\x64"
            if (Test-Path (Join-Path $asanDir "clang_rt.asan_dynamic-x86_64.dll")) {
                $env:PATH = "$asanDir;$env:PATH"
                Write-Host "ASan runtime on PATH: $asanDir" -ForegroundColor Cyan
            } else {
                Write-Host "WARNING: clang_rt.asan_dynamic-x86_64.dll not found under $asanDir." -ForegroundColor Yellow
                Write-Host "         Install the 'C++ AddressSanitizer' VS component." -ForegroundColor Yellow
            }
        } else {
            Write-Host "WARNING: could not locate the MSVC toolset; the test exe may fail to start." -ForegroundColor Yellow
        }

        # detect_container_overflow must be off: oneTBB is an uninstrumented DLL, and
        # the instrumented/uninstrumented boundary produces false positives.
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

    $ErrorActionPreference = 'Continue'
    & ctest @ctestArgs
    $testExit = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'

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
