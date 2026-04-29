cmake_minimum_required(VERSION 3.16)
project(ASCIICraft VERSION 1.0.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Project Configuration
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "FastDebug;Release" CACHE STRING "Build configurations" FORCE)
endif()

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /VERBOSE")
endif()

# ---------------------------------------------------------------------------
# Consumer vendor libraries (defines entt, FastNoiseLite, stb, fmt, etc.)
# ---------------------------------------------------------------------------
add_subdirectory(vendor)

# nlohmann/json (header-only, shipped in sibling ASCIIgL project vendor/)
set(ASCIIgL_NLOHMANN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ASCIIgL/vendor/nlohmann/include")
if(NOT TARGET nlohmann_json)
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE
        "${ASCIIgL_NLOHMANN_DIR}"
    )
endif()

# ---------------------------------------------------------------------------
# oneTBB (job queue for chunk generation / meshing)
# ---------------------------------------------------------------------------
set(TBB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/oneTBB/oneapi-tbb-2022.3.0-win/oneapi-tbb-2022.3.0")
list(APPEND CMAKE_PREFIX_PATH "${TBB_ROOT}")
find_package(TBB CONFIG REQUIRED COMPONENTS tbb)

# ---------------------------------------------------------------------------
# ASCIIgL Engine paths (using distributed library)
# ---------------------------------------------------------------------------
set(ASCIIgL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/ASCIIgL-v1.0.0")
set(ASCIIgL_LIB_DIR "${ASCIIgL_ROOT}/lib")
set(ASCIIgL_INCLUDE_DIR "${ASCIIgL_ROOT}/include")
set(ASCIIgL_VENDOR_DIR "${ASCIIgL_ROOT}/vendor")

# Make ASCIIgL's shipped vendor targets available (should only expose glm::glm)
# Ensure lib/ASCIIgL-v1.0.0/vendor/CMakeLists.txt creates only glm::glm (or prefers system glm)
add_subdirectory("${ASCIIgL_VENDOR_DIR}" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Find the ASCIIgL library (try both release and fastdebug versions)
# ---------------------------------------------------------------------------
find_library(ASCIIgL_LIBRARY_RELEASE
    NAMES ASCIIgL
    PATHS "${ASCIIgL_LIB_DIR}"
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(ASCIIgL_LIBRARY_FASTDEBUG
    NAMES ASCIIgL_FastDebug ASCIIgL
    PATHS "${ASCIIgL_LIB_DIR}"
    NO_DEFAULT_PATH
)

if(NOT ASCIIgL_LIBRARY_FASTDEBUG)
    set(ASCIIgL_LIBRARY_FASTDEBUG ${ASCIIgL_LIBRARY_RELEASE})
endif()

# Create imported target for ASCIIgL
add_library(ASCIIgL STATIC IMPORTED)

set_target_properties(ASCIIgL PROPERTIES
    IMPORTED_LOCATION_RELEASE "${ASCIIgL_LIBRARY_RELEASE}"
    IMPORTED_LOCATION_FASTDEBUG "${ASCIIgL_LIBRARY_FASTDEBUG}"
    INTERFACE_INCLUDE_DIRECTORIES "${ASCIIgL_INCLUDE_DIR}"
    INTERFACE_LINK_DIRECTORIES "${ASCIIgL_LIB_DIR}"
    # Match ASCIIgL's public link interface so consumers of the packaged static lib
    # inherit the required system libraries without relying on header pragmas.
    INTERFACE_LINK_LIBRARIES "glm::glm;winmm;dwrite;d3d11;d3dcompiler;dxgi"
    IMPORTED_LOCATION_MINSIZEREL     "${ASCIIgL_LIBRARY_RELEASE}"
    IMPORTED_LOCATION_RELWITHDEBINFO "${ASCIIgL_LIBRARY_RELEASE}"
)

# Provide a namespaced alias for consumers
add_library(ASCIIgL::ASCIIgL ALIAS ASCIIgL)

# ---------------------------------------------------------------------------
# Source Files
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ASCIICraft_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE ASCIICraft_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# ---------------------------------------------------------------------------
# Game Executable
# ---------------------------------------------------------------------------
add_executable(ASCIICraft
    ${ASCIICraft_SOURCES}
    ${ASCIICraft_HEADERS}
    "${CMAKE_CURRENT_SOURCE_DIR}/../ASCIIgL/vendor/tracy/public/TracyClient.cpp"
)

# ---------------------------------------------------------------------------
# Include Directories
# ---------------------------------------------------------------------------
target_include_directories(ASCIICraft PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ASCIIgL/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ASCIIgL/vendor/tracy/public"
)

# ---------------------------------------------------------------------------
# Linking
# ---------------------------------------------------------------------------
# Link ASCIIgL (which transitively provides glm::glm)
target_link_libraries(ASCIICraft PRIVATE ASCIIgL::ASCIIgL)

# Link oneTBB (job queue)
target_link_libraries(ASCIICraft PRIVATE TBB::tbb)

# Link vendor header-only targets from top-level vendor/ if present
if(TARGET entt)
    target_link_libraries(ASCIICraft PRIVATE entt)
endif()

if(TARGET FastNoiseLite)
    target_link_libraries(ASCIICraft PRIVATE FastNoiseLite)
endif()

if(TARGET nlohmann_json)
    target_link_libraries(ASCIICraft PRIVATE nlohmann_json)
endif()

# Platform-specific: prevent Windows min/max macros from breaking std::min/std::max
if(WIN32)
    target_compile_definitions(ASCIICraft PRIVATE NOMINMAX TRACY_ENABLE)
    target_link_libraries(ASCIICraft PRIVATE winmm)
endif()

# ---------------------------------------------------------------------------
# Output Configuration
# ---------------------------------------------------------------------------
set_target_properties(ASCIICraft PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_FASTDEBUG   "${CMAKE_BINARY_DIR}/bin/FastDebug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# ---------------------------------------------------------------------------
# Post-Build Steps
# ---------------------------------------------------------------------------
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

# Copy oneTBB DLLs to exe dir on Windows (required at runtime)
if(WIN32)
    add_custom_command(TARGET ASCIICraft POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying oneTBB DLLs"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TBB_ROOT}/redist/intel64/vc14/tbb12.dll"
            "$<TARGET_FILE_DIR:ASCIICraft>/tbb12.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TBB_ROOT}/redist/intel64/vc14/tbb12_debug.dll"
            "$<TARGET_FILE_DIR:ASCIICraft>/tbb12_debug.dll"
        COMMENT "Copying oneTBB runtime DLLs"
    )
endif()