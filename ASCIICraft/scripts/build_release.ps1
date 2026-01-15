# Remove only the Release configuration folder if it exists
if (Test-Path -Path ".\build\Release") {
    Remove-Item -Recurse -Force ".\build\Release"
}
if (Test-Path -Path ".\build\lib\Release") {
    Remove-Item -Recurse -Force ".\build\lib\Release"
}
if (Test-Path -Path ".\build\bin\Release") {
    Remove-Item -Recurse -Force ".\build\bin\Release"
}

# Regenerate build files with CMake for Visual Studio 2026
cmake -B build -G "Visual Studio 18 2026"

# Build all targets in Release configuration
cmake --build build --config Release