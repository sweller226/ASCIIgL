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
# ASCIIgL Engine (using distributed library + CMake config)
# ---------------------------------------------------------------------------
set(ASCIIgL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/ASCIIgL-v1.0.0")
list(APPEND CMAKE_PREFIX_PATH "${ASCIIgL_ROOT}/cmake")
find_package(ASCIIgL REQUIRED)

# ---------------------------------------------------------------------------
# stb (header-only, in vendor/stb)
# ---------------------------------------------------------------------------
if(NOT TARGET stb)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb"
    )
endif()

# ---------------------------------------------------------------------------
# OpenAL Soft
# ---------------------------------------------------------------------------
set(OPENAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/OpenAL-Soft/openal-soft-1.25.1-bin")
find_library(OPENAL_LIB
    NAMES OpenAL32 openal
    PATHS "${OPENAL_ROOT}/libs/Win64"
    NO_DEFAULT_PATH
    REQUIRED
)

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
# Tracy (TracyClient.cpp #includes .cpp files from public/common and public/client)
# ---------------------------------------------------------------------------
set(ASCIICraft_TRACY_PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../ASCIIgL/vendor/tracy/public")

# ---------------------------------------------------------------------------
# Game Executable
# ---------------------------------------------------------------------------
add_executable(ASCIICraft
    ${ASCIICraft_SOURCES}
    ${ASCIICraft_HEADERS}
    "${ASCIICraft_TRACY_PUBLIC}/TracyClient.cpp"
)

# ---------------------------------------------------------------------------
# Include Directories
# ---------------------------------------------------------------------------
target_include_directories(ASCIICraft PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${OPENAL_ROOT}/include"
    "${ASCIICraft_TRACY_PUBLIC}"
)


# ---------------------------------------------------------------------------
# Linking
# ---------------------------------------------------------------------------
target_link_libraries(ASCIICraft PRIVATE ASCIIgL::ASCIIgL)
target_link_libraries(ASCIICraft PRIVATE TBB::tbb)
target_link_libraries(ASCIICraft PRIVATE ${OPENAL_LIB})
target_link_libraries(ASCIICraft PRIVATE stb)

if(TARGET entt)
    target_link_libraries(ASCIICraft PRIVATE entt)
endif()

if(TARGET FastNoiseLite)
    target_link_libraries(ASCIICraft PRIVATE FastNoiseLite)
endif()

if(TARGET nlohmann_json)
    target_link_libraries(ASCIICraft PRIVATE nlohmann_json)
endif()

# Platform-specific
if(WIN32)
    target_compile_definitions(ASCIICraft PRIVATE NOMINMAX TRACY_ENABLE)
    target_link_libraries(ASCIICraft PRIVATE winmm)
endif()

# ---------------------------------------------------------------------------
# Output Configuration
# ---------------------------------------------------------------------------
set_target_properties(ASCIICraft PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_FASTDEBUG "${CMAKE_BINARY_DIR}/bin/FastDebug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE   "${CMAKE_BINARY_DIR}/bin/Release"
)

# ---------------------------------------------------------------------------
# Resource Copy
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ASCIICraft_RESOURCE_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/res/*"
    "${ASCIIgL_ROOT}/res/*"
)

set(ASCIICraft_RESOURCE_STAMP "${CMAKE_BINARY_DIR}/resources.stamp")

add_custom_command(
    OUTPUT "${ASCIICraft_RESOURCE_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E echo "Copying resources to build directories"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res"
        "${CMAKE_BINARY_DIR}/bin/FastDebug/res"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res"
        "${CMAKE_BINARY_DIR}/bin/Release/res"
    COMMAND ${CMAKE_COMMAND} -E echo "Copying ASCIIgL library resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASCIIgL_ROOT}/res"
        "${CMAKE_BINARY_DIR}/bin/FastDebug/res/ASCIIgL"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${ASCIIgL_ROOT}/res"
        "${CMAKE_BINARY_DIR}/bin/Release/res/ASCIIgL"
    COMMAND ${CMAKE_COMMAND} -E touch "${ASCIICraft_RESOURCE_STAMP}"
    DEPENDS ${ASCIICraft_RESOURCE_FILES}
    COMMENT "Copying game and library resources"
)

add_custom_target(ASCIICraftResources
    DEPENDS "${ASCIICraft_RESOURCE_STAMP}"
)

add_dependencies(ASCIICraft ASCIICraftResources)

# ---------------------------------------------------------------------------
# Post-Build Steps
# ---------------------------------------------------------------------------
if(WIN32)
    add_custom_command(TARGET ASCIICraft POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/res"
            "$<TARGET_FILE_DIR:ASCIICraft>/res"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASCIIgL_ROOT}/res"
            "$<TARGET_FILE_DIR:ASCIICraft>/res/ASCIIgL"
        COMMENT "Copying game and library resources"
    )

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

    add_custom_command(TARGET ASCIICraft POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENAL_ROOT}/bin/Win64/soft_oal.dll"
            "$<TARGET_FILE_DIR:ASCIICraft>/OpenAL32.dll"
        COMMENT "Copying OpenAL32.dll"
    )
endif()