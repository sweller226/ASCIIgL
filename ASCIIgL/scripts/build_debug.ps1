# Remove only the Debug configuration folder if it exists
if (Test-Path -Path ".\build\Debug") {
    Remove-Item -Recurse -Force ".\build\Debug"
}
if (Test-Path -Path ".\build\lib\Debug") {
    Remove-Item -Recurse -Force ".\build\lib\Debug"
}
if (Test-Path -Path ".\build\bin\Debug") {
    Remove-Item -Recurse -Force ".\build\bin\Debug"
}

# Regenerate build files with CMake for Visual Studio 2026
cmake -B build -G "Visual Studio 18 2026"

# Build all targets in Debug configuration
cmake --build build --config Debug