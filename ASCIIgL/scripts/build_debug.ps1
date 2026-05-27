# Remove only the FastDebug configuration folder if it exists
if (Test-Path -Path ".\build\FastDebug") {
    Remove-Item -Recurse -Force ".\build\FastDebug"
}
if (Test-Path -Path ".\build\lib\FastDebug") {
    Remove-Item -Recurse -Force ".\build\lib\FastDebug"
}
if (Test-Path -Path ".\build\bin\FastDebug") {
    Remove-Item -Recurse -Force ".\build\bin\FastDebug"
}

# Regenerate build files with CMake for Visual Studio 2026
cmake -B build -G "Visual Studio 18 2026"

# Build all targets in FastDebug configuration
cmake --build build --config FastDebug