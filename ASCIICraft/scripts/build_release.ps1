# Regenerate build files with CMake for Visual Studio 2026
cmake -B build -G "Visual Studio 18 2026"

# Build all targets in Release configuration
cmake --build build --config Release