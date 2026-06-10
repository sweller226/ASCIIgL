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

    # Copy TracyClient.cpp explicitly — excluded above by /XF *.cpp but required by consumers
    $tracyDest = "$OutputDir/vendor/tracy/public"
    New-Item -Path $tracyDest -ItemType Directory -Force | Out-Null
    Copy-Item "vendor/tracy/public/TracyClient.cpp" "$tracyDest/TracyClient.cpp" -Force
    Write-Success "Copied TracyClient.cpp"

    # Copy CMake config
    if (-not (Test-Path "cmake/ASCIIgLConfig.cmake")) {
        Write-Error "cmake/ASCIIgLConfig.cmake not found! Please create it first."
        exit 1
    }
    New-Item -Path "$OutputDir/cmake" -ItemType Directory -Force | Out-Null
    Copy-Item "cmake/ASCIIgLConfig.cmake" "$OutputDir/cmake/" -Force
    Write-Success "Copied CMake config"
        
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