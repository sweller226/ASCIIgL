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

function New-Distribution {
    param([string]$OutputDir)
    
    Write-Info "Creating complete library distribution..."
    
    # Create directory structure
    New-Item -Path "$OutputDir/lib" -ItemType Directory -Force | Out-Null
    
    # Copy Release library
    Copy-Item "build/lib/Release/ASCIIgL.lib" "$OutputDir/lib/" -Force
    Write-Success "Copied Release library"
    
    # Copy Debug library if available
    if (Test-Path "build/lib/Debug/ASCIIgL.lib") {
        Copy-Item "build/lib/Debug/ASCIIgL.lib" "$OutputDir/lib/ASCIIgL_Debug.lib" -Force
        Write-Success "Copied Debug library"
    } else {
        Write-Warning "Debug library not found, run build_debug.ps1 first for complete package"
    }
    
    # Copy headers
    Copy-Item "include" "$OutputDir/" -Recurse -Force
    Write-Success "Copied headers"
    
    # Copy entire vendor folder (includes GLM, stb_image headers needed by public API)
    # Exclude .cpp files since this is a static library distribution
    robocopy "vendor" "$OutputDir/vendor" /E /XF *.cpp | Out-Null
    if ($LASTEXITCODE -le 1) { # robocopy success codes are 0 or 1
        Write-Success "Copied vendor dependencies (headers only)"
    } else {
        Write-Warning "Issue copying vendor folder, exit code: $LASTEXITCODE"
    }
    
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