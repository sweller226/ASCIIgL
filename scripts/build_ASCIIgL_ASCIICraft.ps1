#!/usr/bin/env pwsh
# ASCIICraft Build Script
# This script builds the ASCIIgL library and updates the ASCIICraft game with the new distribution

param(
    [string]$Version = "1.0.0"
)

Write-Host "Starting ASCIICraft Build Process..." -ForegroundColor Green
Write-Host "Version: $Version" -ForegroundColor Cyan

# Set error action preference to stop on errors
$ErrorActionPreference = "Stop"

try {
    # Get the base directory (should be ASCIIgL root)
    $BaseDir = Split-Path -Parent $PSScriptRoot
    $ASCIIgLDir = Join-Path $BaseDir "ASCIIgL"
    $ASCIICraftDir = Join-Path $BaseDir "ASCIICraft"
    
    Write-Host "Base Directory: $BaseDir" -ForegroundColor Yellow
    Write-Host "ASCIIgL Directory: $ASCIIgLDir" -ForegroundColor Yellow
    Write-Host "ASCIICraft Directory: $ASCIICraftDir" -ForegroundColor Yellow
    
    # Verify directories exist
    if (-not (Test-Path $ASCIIgLDir)) {
        throw "ASCIIgL directory not found: $ASCIIgLDir"
    }
    
    if (-not (Test-Path $ASCIICraftDir)) {
        throw "ASCIICraft directory not found: $ASCIICraftDir"
    }
    
    # Change to ASCIIgL directory
    Push-Location $ASCIIgLDir
    
    Write-Host "`n=== Step 1: Building ASCIIgL Release ===" -ForegroundColor Magenta
    & ".\scripts\build_release.ps1"
    if ($LASTEXITCODE -ne 0) {
        throw "Release build failed with exit code $LASTEXITCODE"
    }
    Write-Host "Release build completed successfully!" -ForegroundColor Green
    
    Write-Host "`n=== Step 2: Building ASCIIgL Debug ===" -ForegroundColor Magenta
    & ".\scripts\build_debug.ps1"
    if ($LASTEXITCODE -ne 0) {
        throw "Debug build failed with exit code $LASTEXITCODE"
    }
    Write-Host "Debug build completed successfully!" -ForegroundColor Green
    
    Write-Host "`n=== Step 3: Creating Distribution ===" -ForegroundColor Magenta
    & ".\scripts\create_distribution.ps1" -Clean -Version $Version
    if ($LASTEXITCODE -ne 0) {
        throw "Distribution creation failed with exit code $LASTEXITCODE"
    }
    Write-Host "Distribution created successfully!" -ForegroundColor Green
    
    # Pop back to original location
    Pop-Location
    
    Write-Host "`n=== Step 4: Updating ASCIICraft Libraries ===" -ForegroundColor Magenta
    
    $SourceDistribution = Join-Path $ASCIIgLDir "dist\ASCIIgL-v$Version"
    $ASCIICraftLibsDir = Join-Path $ASCIICraftDir "libs"
    $TargetLibrary = Join-Path $ASCIICraftLibsDir "ASCIIgL-v$Version"
    
    Write-Host "Source Distribution: $SourceDistribution" -ForegroundColor Yellow
    Write-Host "Target Library: $TargetLibrary" -ForegroundColor Yellow
    
    # Verify source distribution exists
    if (-not (Test-Path $SourceDistribution)) {
        throw "Source distribution not found: $SourceDistribution"
    }
    
    # Create libs directory if it doesn't exist
    if (-not (Test-Path $ASCIICraftLibsDir)) {
        New-Item -ItemType Directory -Path $ASCIICraftLibsDir -Force | Out-Null
        Write-Host "Created libs directory: $ASCIICraftLibsDir" -ForegroundColor Green
    }
    
    # Remove existing library if it exists
    if (Test-Path $TargetLibrary) {
        Write-Host "Removing existing library: $TargetLibrary" -ForegroundColor Yellow
        Remove-Item -Path $TargetLibrary -Recurse -Force
    }
    
    # Copy new distribution
    Write-Host "Copying new distribution..." -ForegroundColor Yellow
    Copy-Item -Path $SourceDistribution -Destination $TargetLibrary -Recurse -Force
    
    # Verify the copy was successful
    if (Test-Path $TargetLibrary) {
        Write-Host "Library updated successfully!" -ForegroundColor Green
        
        # List contents to verify
        $IncludeDir = Join-Path $TargetLibrary "include"
        $LibDir = Join-Path $TargetLibrary "lib"
        
        if (Test-Path $IncludeDir) {
            $HeaderCount = (Get-ChildItem -Path $IncludeDir -Recurse -Filter "*.hpp" | Measure-Object).Count
            Write-Host "Headers copied: $HeaderCount .hpp files" -ForegroundColor Cyan
        }
        
        if (Test-Path $LibDir) {
            $LibFiles = Get-ChildItem -Path $LibDir -Filter "*.lib"
            Write-Host "Library files copied:" -ForegroundColor Cyan
            foreach ($lib in $LibFiles) {
                Write-Host "  - $($lib.Name)" -ForegroundColor Cyan
            }
        }
    } else {
        throw "Failed to copy library to target location"
    }
    
    Write-Host "`n=== ASCIICraft Build Process Completed Successfully! ===" -ForegroundColor Green
    Write-Host "ASCIIgL library version $Version has been built and deployed to ASCIICraft." -ForegroundColor Green
    
} catch {
    Write-Host "`nERROR: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Build process failed!" -ForegroundColor Red
    exit 1
} finally {
    # Make sure we return to original location
    Pop-Location -ErrorAction SilentlyContinue
}
