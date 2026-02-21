#!/usr/bin/env pwsh
# Copies coverage_cleartype.json from CharCoverage build output into ASCIIgL/res.
# Optionally runs the char_coverage tool first to regenerate the file.

param(
    [switch]$Regenerate  # If set, run char_coverage --scan --cleartype before copying
)

$ErrorActionPreference = "Stop"

$BaseDir = Split-Path -Parent $PSScriptRoot
$SourcePath = Join-Path $BaseDir "tools\CharCoverage\build\Release\coverage_cleartype.json"
$DestDir = Join-Path $BaseDir "ASCIIgL\res"
$DestPath = Join-Path $DestDir "coverage_cleartype.json"
$CharCoverageExe = Join-Path $BaseDir "tools\CharCoverage\build\Release\char_coverage.exe"

if ($Regenerate) {
    if (-not (Test-Path $CharCoverageExe)) {
        Write-Error "char_coverage.exe not found at $CharCoverageExe. Build the CharCoverage tool first."
    }
    $ReleaseDir = Split-Path -Parent $SourcePath
    if (-not (Test-Path $ReleaseDir)) {
        New-Item -ItemType Directory -Path $ReleaseDir -Force | Out-Null
    }
    Write-Host "Regenerating coverage with char_coverage --scan --cleartype ..." -ForegroundColor Cyan
    & $CharCoverageExe --scan --cleartype --output $SourcePath
    if ($LASTEXITCODE -ne 0) {
        Write-Error "char_coverage failed with exit code $LASTEXITCODE"
    }
}

if (-not (Test-Path $SourcePath)) {
    Write-Error "Source not found: $SourcePath. Build CharCoverage, run with -Regenerate, or generate coverage_cleartype.json manually."
}

if (-not (Test-Path $DestDir)) {
    New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
}

Copy-Item -Path $SourcePath -Destination $DestPath -Force
Write-Host "Copied coverage_cleartype.json to ASCIIgL/res" -ForegroundColor Green
Write-Host "  $DestPath" -ForegroundColor Gray
