# ============================================================================
# ASCIIgL Library Distribution Script
# ============================================================================
# This script creates distribution packages for the ASCIIgL library
# Run from the ASCIIgL library root directory

param(
    [string]$Version = "1.0.0",
    [switch]$Clean = $false
)

# Colors for output
$ColorInfo = "Cyan"
$ColorSuccess = "Green"
$ColorWarning = "Yellow"
$ColorError = "Red"

function Write-Info($Message) {
    Write-Host "[INFO] $Message" -ForegroundColor $ColorInfo
}

function Write-Success($Message) {
    Write-Host "[SUCCESS] $Message" -ForegroundColor $ColorSuccess
}

function Write-Warning($Message) {
    Write-Host "[WARNING] $Message" -ForegroundColor $ColorWarning
}

function Write-Error($Message) {
    Write-Host "[ERROR] $Message" -ForegroundColor $ColorError
}

# ============================================================================
# Validation
# ============================================================================

Write-Info "ASCIIgL Distribution Builder v$Version"
Write-Info "========================================="

# Check if we're in the right directory
if (-not (Test-Path "CMakeLists.txt") -or -not (Test-Path "include/ASCIIgL")) {
    Write-Error "This script must be run from the ASCIIgL root directory!"
    exit 1
}

# Check if library has been built
if (-not (Test-Path "build/lib/Release/ASCIIgL.lib")) {
    Write-Error "Release library not found! Please run './scripts/build_release.ps1' first."
    exit 1
}

# ============================================================================
# Distribution Function
# ============================================================================

function Copy-TbbBuildArtifacts {
    param([string]$OutputDir)

    New-Item -Path "$OutputDir/lib" -ItemType Directory -Force | Out-Null
    New-Item -Path "$OutputDir/bin" -ItemType Directory -Force | Out-Null

    $releaseLib = Get-ChildItem -Path "build" -Recurse -Filter "tbb12.lib" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "release" -and $_.FullName -notmatch "fastdebug" } |
        Select-Object -First 1
    $releaseDll = Get-ChildItem -Path "build" -Recurse -Filter "tbb12.dll" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "release" -and $_.FullName -notmatch "fastdebug" } |
        Select-Object -First 1
    $fastDebugLib = Get-ChildItem -Path "build" -Recurse -Filter "tbb12.lib" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "fastdebug" } |
        Select-Object -First 1
    $fastDebugDll = Get-ChildItem -Path "build" -Recurse -Filter "tbb12.dll" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "fastdebug" } |
        Select-Object -First 1

    if ($releaseLib) {
        Copy-Item $releaseLib.FullName "$OutputDir/lib/tbb12.lib" -Force
        Write-Success "Copied Release oneTBB import library"
    } else {
        Write-Warning "Release oneTBB import library not found in build output"
    }

    if ($releaseDll) {
        Copy-Item $releaseDll.FullName "$OutputDir/bin/tbb12.dll" -Force
        Write-Success "Copied Release oneTBB runtime DLL"
    } else {
        Write-Warning "Release oneTBB runtime DLL not found in build output"
    }

    if ($fastDebugLib) {
        Copy-Item $fastDebugLib.FullName "$OutputDir/lib/tbb12_debug.lib" -Force
        Write-Success "Copied FastDebug oneTBB import library"
    } else {
        Write-Warning "FastDebug oneTBB import library not found in build output"
    }

    if ($fastDebugDll) {
        Copy-Item $fastDebugDll.FullName "$OutputDir/bin/tbb12_debug.dll" -Force
        Write-Success "Copied FastDebug oneTBB runtime DLL"
    } else {
        Write-Warning "FastDebug oneTBB runtime DLL not found in build output"
    }
}

function New-Distribution {
    param([string]$OutputDir)
    
    Write-Info "Creating complete library distribution..."
    
    # Create directory structure
    New-Item -Path "$OutputDir/lib" -ItemType Directory -Force | Out-Null
    
    # Copy Release library
    Copy-Item "build/lib/Release/ASCIIgL.lib" "$OutputDir/lib/" -Force
    Write-Success "Copied Release library"

    # Copy FastDebug library if available
    if (Test-Path "build/lib/FastDebug/ASCIIgL.lib") {
        Copy-Item "build/lib/FastDebug/ASCIIgL.lib" "$OutputDir/lib/ASCIIgL_FastDebug.lib" -Force
        Write-Success "Copied FastDebug library"
    } else {
        Write-Warning "FastDebug library not found, run build_fastdebug.ps1 first for complete package"
    }
    
    # Copy headers
    Copy-Item "include" "$OutputDir/" -Recurse -Force
    Write-Success "Copied headers"
    
    # Copy entire vendor folder (headers only — exclude .cpp files)
    robocopy "vendor" "$OutputDir/vendor" /E /XF *.cpp | Out-Null
    # Robocopy exit codes: 0-3 are success, 4+ are errors
    if ($LASTEXITCODE -le 3) {
        Write-Success "Copied vendor dependencies (headers only)"
        $global:LASTEXITCODE = 0  # Reset exit code to 0 for success
    } else {
        Write-Warning "Issue copying vendor folder, exit code: $LASTEXITCODE"
    }

    # Tracy: TracyClient.cpp #includes other .cpp under public/common and public/client.
    # robocopy /XF *.cpp strips those; copy the full public tree for consumer builds.
    Copy-Item "vendor/tracy/public" "$OutputDir/vendor/tracy/public" -Recurse -Force
    Write-Success "Copied Tracy public sources (TracyClient.cpp + included .cpp files)"

    # Copy CMake config
    if (-not (Test-Path "cmake/ASCIIgLConfig.cmake")) {
        Write-Error "cmake/ASCIIgLConfig.cmake not found! Please create it first."
        exit 1
    }
    New-Item -Path "$OutputDir/cmake" -ItemType Directory -Force | Out-Null
    Copy-Item "cmake/ASCIIgLConfig.cmake" "$OutputDir/cmake/" -Force
    Write-Success "Copied CMake config"

    Copy-TbbBuildArtifacts $OutputDir
        
    # Copy resources (fonts, etc.)
    if (Test-Path "res") {
        Copy-Item "res" "$OutputDir/" -Recurse -Force
        Write-Success "Copied library resources"
    }
    
    # Copy examples if they exist
    if (Test-Path "examples") {
        Copy-Item "examples" "$OutputDir/" -Recurse -Force
        Write-Success "Copied examples"
    }
    
    # Copy documentation if it exists
    if (Test-Path "docs") {
        Copy-Item "docs" "$OutputDir/docs/" -Recurse -Force
        Write-Success "Copied documentation"
    }
}

# ============================================================================
# Main Distribution Logic
# ============================================================================

$timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
$distDir = "dist"

# Clean previous distributions if requested
if ($Clean) {
    if (Test-Path $distDir) {
        Remove-Item $distDir -Recurse -Force
        Write-Info "Cleaned previous distributions"
    }
}

# Create base distribution directory
New-Item -Path $distDir -ItemType Directory -Force | Out-Null

# Create the distribution
$outputDir = "$distDir/ASCIIgL-v$Version"
New-Distribution $outputDir
Write-Success "Distribution created: $outputDir"

# Ensure we exit with success code
exit 0