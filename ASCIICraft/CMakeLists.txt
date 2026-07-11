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

# Sibling ASCIIgL source vendor tree (nlohmann, tracy, oneTBB submodule, etc.)
set(ASCIIgL_SOURCE_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ASCIIgL/vendor")

# nlohmann/json (header-only, shipped in sibling ASCIIgL project vendor/)
set(ASCIIgL_NLOHMANN_DIR "${ASCIIgL_SOURCE_VENDOR_DIR}/nlohmann/include")
if(NOT TARGET nlohmann_json)
    add_library(nlohmann_json INTERFACE)
    target_include_directories(nlohmann_json INTERFACE
        "${ASCIIgL_NLOHMANN_DIR}"
    )
endif()

# ---------------------------------------------------------------------------
# oneTBB (shared submodule with ASCIIgL; job queue + engine LUT precompute)
# ---------------------------------------------------------------------------
if(NOT TARGET TBB::tbb)
    if(NOT EXISTS "${ASCIIgL_SOURCE_VENDOR_DIR}/oneTBB/CMakeLists.txt")
        message(FATAL_ERROR
            "ASCIIgL oneTBB submodule not found at ${ASCIIgL_SOURCE_VENDOR_DIR}/oneTBB. "
            "Run: git submodule update --init --recursive")
    endif()
    set(TBB_TEST OFF CACHE BOOL "" FORCE)
    set(TBB_STRICT OFF CACHE BOOL "" FORCE)
    set(TBB_BUILD_TBBMALLOC OFF CACHE BOOL "" FORCE)
    add_subdirectory("${ASCIIgL_SOURCE_VENDOR_DIR}/oneTBB" "${CMAKE_BINARY_DIR}/oneTBB" EXCLUDE_FROM_ALL)
endif()

# ---------------------------------------------------------------------------
# ASCIIgL Engine (using distributed library + CMake config)
# ---------------------------------------------------------------------------
set(ASCIIgL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/ASCIIgL-v1.0.0")
list(APPEND CMAKE_PREFIX_PATH "${ASCIIgL_ROOT}/cmake")
find_package(ASCIIgL REQUIRED)

# ---------------------------------------------------------------------------
# OpenAL Soft (shared submodule; built from source)
# ---------------------------------------------------------------------------
if(NOT TARGET OpenAL::OpenAL)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/openal-soft/CMakeLists.txt")
        message(FATAL_ERROR
            "openal-soft submodule not found at ASCIICraft/vendor/openal-soft. "
            "Run: git submodule update --init --recursive")
    endif()
    set(LIBTYPE SHARED CACHE STRING "" FORCE)
    set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
    set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
    set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
    set(ALSOFT_UPDATE_BUILD_VERSION OFF CACHE BOOL "" FORCE)
    set(ALSOFT_ENABLE_MODULES OFF CACHE BOOL "" FORCE)
    add_subdirectory(vendor/openal-soft "${CMAKE_BINARY_DIR}/openal-soft" EXCLUDE_FROM_ALL)
endif()

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
set(ASCIICraft_TRACY_PUBLIC "${ASCIIgL_SOURCE_VENDOR_DIR}/tracy/public")

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
    "${ASCIICraft_TRACY_PUBLIC}"
)


# ---------------------------------------------------------------------------
# Linking
# ---------------------------------------------------------------------------
target_link_libraries(ASCIICraft PRIVATE ASCIIgL::ASCIIgL)
target_link_libraries(ASCIICraft PRIVATE TBB::tbb)
target_link_libraries(ASCIICraft PRIVATE OpenAL::OpenAL)
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
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:tbb>"
            "$<TARGET_FILE_DIR:ASCIICraft>"
        COMMENT "Copying oneTBB runtime DLL"
    )

    add_custom_command(TARGET ASCIICraft POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:OpenAL::OpenAL>"
            "$<TARGET_FILE_DIR:ASCIICraft>/OpenAL32.dll"
        COMMENT "Copying OpenAL32.dll"
    )
endif()