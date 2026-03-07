# Build CharCoverage tool (Windows, Visual Studio)
# Run from tools/CharCoverage: .\build.ps1
# Output: build\Release\char_coverage.exe (or build\Debug\char_coverage.exe with -Debug)

param(
    [switch]$Debug
)

$ErrorActionPreference = "Stop"
$config = if ($Debug) { "Debug" } else { "Release" }

if (-not (Test-Path ".\char_coverage.cpp")) {
    Write-Error "Run this script from the CharCoverage folder (tools/CharCoverage)."
    exit 1
}

$generator = "Visual Studio 18 2026"
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "CMake not found. Add CMake to PATH or run from a Visual Studio Developer shell."
    exit 1
}

Write-Host "Configuring CharCoverage ($config)..."
cmake -B build -G $generator -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Building $config..."
cmake --build build --config $config
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = "build\$config\char_coverage.exe"
if (Test-Path $exe) {
    Write-Host "Done: $exe"
} else {
    Write-Host "Build completed; executable expected at $exe"
}
