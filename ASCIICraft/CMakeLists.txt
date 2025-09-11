# ============================================================================
# ASCIICraft - Voxel Game
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ASCIICraft VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# Project Configuration
# ============================================================================

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

# Debug configuration for MSVC
if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /VERBOSE")
endif()

# ============================================================================
# Engine Integration
# ============================================================================

# ASCIIgL Engine paths (using distributed library)
set(ASCIIgL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libs/ASCIIgL-v1.0.0")
set(ASCIIgL_LIB_DIR "${ASCIIgL_ROOT}/lib")
set(ASCIIgL_INCLUDE_DIR "${ASCIIgL_ROOT}/include")
set(ASCIIgL_VENDOR_DIR "${ASCIIgL_ROOT}/vendor")

# Set up GLM as interface library (header-only, needed for engine API)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE "${ASCIIgL_VENDOR_DIR}")

# Find the ASCIIgL library (try both release and debug versions)
find_library(ASCIIgL_LIBRARY_RELEASE
    NAMES ASCIIgL
    PATHS "${ASCIIgL_LIB_DIR}"
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(ASCIIgL_LIBRARY_DEBUG
    NAMES ASCIIgL_Debug
    PATHS "${ASCIIgL_LIB_DIR}"
    NO_DEFAULT_PATH
)

# Use release library as fallback if debug not found
if(NOT ASCIIgL_LIBRARY_DEBUG)
    set(ASCIIgL_LIBRARY_DEBUG ${ASCIIgL_LIBRARY_RELEASE})
endif()

# Create imported target for ASCIIgL
add_library(ASCIIgL STATIC IMPORTED)

# Set configuration-specific library locations
set_target_properties(ASCIIgL PROPERTIES
    IMPORTED_LOCATION_RELEASE "${ASCIIgL_LIBRARY_RELEASE}"
    IMPORTED_LOCATION_DEBUG "${ASCIIgL_LIBRARY_DEBUG}"
    INTERFACE_INCLUDE_DIRECTORIES "${ASCIIgL_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "glm"
)

# ============================================================================
# Source Files
# ============================================================================

# Collect all source files
file(GLOB_RECURSE ASCIICraft_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Collect all header files
file(GLOB_RECURSE ASCIICraft_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# ============================================================================
# Game Executable
# ============================================================================

# Create the game executable
add_executable(ASCIICraft
    ${ASCIICraft_SOURCES}
    ${ASCIICraft_HEADERS}
)

# ============================================================================
# Include Directories
# ============================================================================

# Add ASCIICraft's own include directory
target_include_directories(ASCIICraft PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# ============================================================================
# Linking
# ============================================================================

# Link to ASCIIgL engine (automatically provides all dependencies)
target_link_libraries(ASCIICraft PRIVATE ASCIIgL)

# Platform-specific libraries (required by ASCIIgL)
if(WIN32)
    target_link_libraries(ASCIICraft PRIVATE winmm)
endif()

# ============================================================================
# Output Configuration
# ============================================================================

# Set output directories for different build configurations
set_target_properties(ASCIICraft PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# ============================================================================
# Post-Build Steps
# ============================================================================

# Copy resources to build directories
add_custom_command(TARGET ASCIICraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying resources to build directories"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res"
        "$<TARGET_FILE_DIR:ASCIICraft>/res"
    COMMAND ${CMAKE_COMMAND} -E echo "Copying ASCIIgL library resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASCIIgL_ROOT}/res"
        "$<TARGET_FILE_DIR:ASCIICraft>/res/ASCIIgL"
    COMMENT "Copying game and library resources"
)