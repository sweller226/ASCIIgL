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

# Regenerate build files with CMake for Visual Studio 2022
cmake -B build -G "Visual Studio 17 2022"

# Build all targets in Release configuration
cmake --build build --config Release