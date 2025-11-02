#!/usr/bin/env pwsh
# Christmas Game Build Script
# This script builds the ASCIIgL library and updates the Christmas game with the new distribution

param(
    [string]$Version = "1.0.0"
)

Write-Host "Starting Christmas Game Build Process..." -ForegroundColor Green
Write-Host "Version: $Version" -ForegroundColor Cyan

# Set error action preference to stop on errors
$ErrorActionPreference = "Stop"

try {
    # Get the base directory (should be ASCIIgL root)
    $BaseDir = Split-Path -Parent $PSScriptRoot
    $ASCIIgLDir = Join-Path $BaseDir "ASCIIgL"
    $ChristmasGameDir = Join-Path $BaseDir "I_Dont_Wanna_Run_For_Christmas"
    
    Write-Host "Base Directory: $BaseDir" -ForegroundColor Yellow
    Write-Host "ASCIIgL Directory: $ASCIIgLDir" -ForegroundColor Yellow
    Write-Host "Christmas Game Directory: $ChristmasGameDir" -ForegroundColor Yellow
    
    # Verify directories exist
    if (-not (Test-Path $ASCIIgLDir)) {
        throw "ASCIIgL directory not found: $ASCIIgLDir"
    }
    
    if (-not (Test-Path $ChristmasGameDir)) {
        throw "Christmas Game directory not found: $ChristmasGameDir"
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
    
    Write-Host "`n=== Step 4: Updating Christmas Game Libraries ===" -ForegroundColor Magenta
    
    $SourceDistribution = Join-Path $ASCIIgLDir "dist\ASCIIgL-v$Version"
    $ChristmasLibsDir = Join-Path $ChristmasGameDir "libs"
    $TargetLibrary = Join-Path $ChristmasLibsDir "ASCIIgL-v$Version"
    
    Write-Host "Source Distribution: $SourceDistribution" -ForegroundColor Yellow
    Write-Host "Target Library: $TargetLibrary" -ForegroundColor Yellow
    
    # Verify source distribution exists
    if (-not (Test-Path $SourceDistribution)) {
        throw "Source distribution not found: $SourceDistribution"
    }
    
    # Create libs directory if it doesn't exist
    if (-not (Test-Path $ChristmasLibsDir)) {
        New-Item -ItemType Directory -Path $ChristmasLibsDir -Force | Out-Null
        Write-Host "Created libs directory: $ChristmasLibsDir" -ForegroundColor Green
    }
    
    # Remove existing library if it exists
    if (Test-Path $TargetLibrary) {
        Write-Host "Removing existing library: $TargetLibrary" -ForegroundColor Yellow
        Remove-Item -Path $TargetLibrary -Recurse -Force
    }
    
    # Copy new distribution
    Write-Host "Copying new distribution..." -ForegroundColor Yellow
    Copy-Item -Path $SourceDistribution -Destination $TargetLibrary -Recurse -Force
    
    # Copy oneTBB library files
    Write-Host "Copying oneTBB library files..." -ForegroundColor Yellow
    $TBBSourceLibDir = Join-Path $ASCIIgLDir "lib\oneTBB\oneapi-tbb-2022.3.0-win\oneapi-tbb-2022.3.0\lib\intel64\vc14"
    $TBBSourceDllDir = Join-Path $ASCIIgLDir "lib\oneTBB\oneapi-tbb-2022.3.0-win\oneapi-tbb-2022.3.0\redist\intel64\vc14"
    $TBBTargetLibDir = Join-Path $TargetLibrary "lib"
    $TBBTargetBinDir = Join-Path $TargetLibrary "bin"
    
    if (Test-Path $TBBSourceLibDir) {
        # Copy release and debug TBB import libraries
        Copy-Item -Path (Join-Path $TBBSourceLibDir "tbb12.lib") -Destination $TBBTargetLibDir -Force
        Copy-Item -Path (Join-Path $TBBSourceLibDir "tbb12_debug.lib") -Destination $TBBTargetLibDir -Force
        Write-Host "  - Copied tbb12.lib" -ForegroundColor Cyan
        Write-Host "  - Copied tbb12_debug.lib" -ForegroundColor Cyan
    } else {
        Write-Host "  Warning: oneTBB lib directory not found, skipping TBB library copy" -ForegroundColor Yellow
    }
    
    if (Test-Path $TBBSourceDllDir) {
        # Create bin directory if it doesn't exist
        if (-not (Test-Path $TBBTargetBinDir)) {
            New-Item -ItemType Directory -Path $TBBTargetBinDir -Force | Out-Null
        }
        
        # Copy release and debug TBB DLLs
        Copy-Item -Path (Join-Path $TBBSourceDllDir "tbb12.dll") -Destination $TBBTargetBinDir -Force
        Copy-Item -Path (Join-Path $TBBSourceDllDir "tbb12_debug.dll") -Destination $TBBTargetBinDir -Force
        Write-Host "  - Copied tbb12.dll" -ForegroundColor Cyan
        Write-Host "  - Copied tbb12_debug.dll" -ForegroundColor Cyan
    } else {
        Write-Host "  Warning: oneTBB DLL directory not found, skipping TBB DLL copy" -ForegroundColor Yellow
    }
    
    # Verify the copy was successful
    if (Test-Path $TargetLibrary) {
        Write-Host "Library updated successfully!" -ForegroundColor Green
        
        # List contents to verify
        $IncludeDir = Join-Path $TargetLibrary "include"
        $LibDir = Join-Path $TargetLibrary "lib"
        $BinDir = Join-Path $TargetLibrary "bin"
        
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
        
        if (Test-Path $BinDir) {
            $DllFiles = Get-ChildItem -Path $BinDir -Filter "*.dll"
            Write-Host "DLL files copied:" -ForegroundColor Cyan
            foreach ($dll in $DllFiles) {
                Write-Host "  - $($dll.Name)" -ForegroundColor Cyan
            }
        }
    } else {
        throw "Failed to copy library to target location"
    }
    
    Write-Host "`n=== Christmas Game Build Process Completed Successfully! ===" -ForegroundColor Green
    Write-Host "ASCIIgL library version $Version has been built and deployed to the Christmas game." -ForegroundColor Green
    
} catch {
    Write-Host "`nERROR: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Build process failed!" -ForegroundColor Red
    exit 1
} finally {
    # Make sure we return to original location
    Pop-Location -ErrorAction SilentlyContinue
}
