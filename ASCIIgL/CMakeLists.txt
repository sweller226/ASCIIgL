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

# ============================================================================
# SIMD and Performance Optimizations
# ============================================================================

# Enable SIMD optimizations for maximum performance
if(MSVC)
    # Visual Studio compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2 /fp:fast")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /Oi /Ot /GL /DNDEBUG")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # GCC/Clang compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma -ffast-math")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto -DNDEBUG")
endif()

# Force GLM to use SIMD optimizations
add_compile_definitions(
    GLM_FORCE_SIMD_AVX2
    GLM_FORCE_ALIGNED_GENTYPES
    GLM_FORCE_INTRINSICS
)

# ============================================================================
# Dependencies Setup
# ============================================================================

# GLM (Header-only math library)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE 
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm"
)

# Make GLM available to parent projects when using add_subdirectory
set_target_properties(glm PROPERTIES EXPORT_NAME glm)
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # We're being built as a subdirectory, create an alias
    add_library(ASCIIgL::glm ALIAS glm)
endif()

# tinyobjloader (Lightweight 3D model loading library)
# Implementation is included in vendor/tinyobjloader/tiny_obj_loader.cpp
# No external dependencies needed!

# oneTBB (Intel Threading Building Blocks)
set(TBB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/oneTBB/oneapi-tbb-2022.3.0-win/oneapi-tbb-2022.3.0")
set(TBB_INCLUDE_DIR "${TBB_ROOT}/include")
set(TBB_LIB_DIR "${TBB_ROOT}/lib/intel64/vc14")

# ============================================================================
# Source Files
# ============================================================================

# Automatically find all source files
file(GLOB_RECURSE ASCIIgL_SOURCES
    "src/renderer/*.cpp"
    "src/engine/*.cpp"
    "src/util/*.cpp"
    "vendor/stb_image/stb_image.cpp"
    "vendor/tinyobjloader/tiny_obj_loader.cpp"
)

# ============================================================================
# Library Target
# ============================================================================

# Create the main library
add_library(ASCIIgL ${ASCIIgL_SOURCES})

# ============================================================================
# Include Directories
# ============================================================================

# Public includes (available to consumers)
target_include_directories(ASCIIgL PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Private includes (internal use only)
target_include_directories(ASCIIgL PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
    ${TBB_INCLUDE_DIR}
)

# ============================================================================
# Linking
# ============================================================================

# Public dependencies (exposed to consumers)
target_link_libraries(ASCIIgL PUBLIC 
    glm
)

# Private dependencies (internal use only - hidden from consumers)
# tinyobjloader is compiled directly into ASCIIgL.lib - no external linking needed!
# oneTBB is linked privately - consumers don't need to know about it
target_link_directories(ASCIIgL PRIVATE ${TBB_LIB_DIR})
target_link_libraries(ASCIIgL PRIVATE
    optimized tbb12
    debug tbb12_debug
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(ASCIIgL PUBLIC winmm)
endif()

# ============================================================================
# Compiler Definitions
# ============================================================================

target_compile_definitions(ASCIIgL PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ============================================================================
# Output Directories
# ============================================================================

set_target_properties(ASCIIgL PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/lib/Debug"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/lib/Debug"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
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
        "${CMAKE_BINARY_DIR}/bin/Debug/res"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res"
        "${CMAKE_BINARY_DIR}/bin/Release/res"
    COMMENT "Copying res folder to build directories"
    VERBATIM
)

# Make the library depend on resource copying
add_dependencies(ASCIIgL copy_resources)