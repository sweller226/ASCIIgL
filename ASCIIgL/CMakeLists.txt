# ASCIIgL - A Console 3D Rendering Engine

cmake_minimum_required(VERSION 3.16)
project(ASCIIgL VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# Project Configuration
# ============================================================================
# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "FastDebug;Release" CACHE STRING "Build configurations" FORCE)
endif()

# ============================================================================
# Vendor subdirectory (registers glm::glm and other vendor interface targets)
# ============================================================================
add_subdirectory(vendor)

# ============================================================================
# SIMD and Performance Optimizations
# ============================================================================
# Enable SIMD optimizations for maximum performance
if(MSVC)
    # Visual Studio compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2 /fp:fast")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /Oi /Ot /DNDEBUG")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # GCC/Clang compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma -ffast-math")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto -DNDEBUG")
endif()

# Force GLM to use SIMD optimizations (kept as global compile defs for compatibility)
add_compile_definitions(
    GLM_FORCE_SIMD_AVX2
    GLM_FORCE_ALIGNED_GENTYPES
    GLM_FORCE_INTRINSICS
)

# ============================================================================
# Source Files
# ============================================================================
# Automatically find all source files
file(GLOB_RECURSE ASCIIgL_SOURCES
    "src/renderer/*.cpp"
    "src/engine/*.cpp"
    "src/util/*.cpp"
    "vendor/tinyobjloader/tiny_obj_loader.cpp"
    "vendor/tracy/public/TracyClient.cpp"
)

# ============================================================================
# Library Target
# ============================================================================
# Create the main library
add_library(ASCIIgL ${ASCIIgL_SOURCES})

# Provide a namespaced alias for consumers
add_library(ASCIIgL::ASCIIgL ALIAS ASCIIgL)

# ============================================================================
# Include Directories
# ============================================================================
# Public includes (available to consumers)
target_include_directories(ASCIIgL PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vendor/tracy/public>
    $<INSTALL_INTERFACE:include>
)

# Private includes (internal use only: e.g. renderer/RendererImpl.hpp)
target_include_directories(ASCIIgL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/nlohmann/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# Linking
# ============================================================================
# Public dependencies (exposed to consumers)
# Use the namespaced glm target provided by vendor/CMakeLists.txt (or system glm)
target_link_libraries(ASCIIgL PUBLIC glm::glm)

# Private dependencies (internal use only - hidden from consumers)
# tinyobjloader is compiled directly into ASCIIgL.lib - no external linking needed

# Platform-specific libraries
if(WIN32)
    target_link_libraries(ASCIIgL PUBLIC
        winmm
        dwrite
        d3d11
        d3dcompiler
        dxgi
    )
endif()

# ============================================================================
# Compiler Definitions
# ============================================================================
target_compile_definitions(ASCIIgL PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    TRACY_ENABLE
    $<$<CONFIG:FastDebug>:ASCIIGL_FASTDEBUG>
)

# ============================================================================
# Output Directories
# ============================================================================
set_target_properties(ASCIIgL PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_FASTDEBUG   "${CMAKE_BINARY_DIR}/lib/FastDebug"
    LIBRARY_OUTPUT_DIRECTORY_FASTDEBUG   "${CMAKE_BINARY_DIR}/lib/FastDebug"
    RUNTIME_OUTPUT_DIRECTORY_FASTDEBUG   "${CMAKE_BINARY_DIR}/bin/FastDebug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# ============================================================================
# Development Asset Packaging
# ============================================================================
# Copy entire res folder to build directories for development builds
add_custom_target(copy_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res"
        "${CMAKE_BINARY_DIR}/bin/FastDebug/res"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res"
        "${CMAKE_BINARY_DIR}/bin/Release/res"
    COMMENT "Copying res folder to build directories"
    VERBATIM
)

# Make the library depend on resource copying
add_dependencies(ASCIIgL copy_resources)