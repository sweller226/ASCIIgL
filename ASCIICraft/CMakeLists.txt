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
# Test options
#
# ASAN must be declared before any target so ASCIICraft_core is instrumented too
# (an uninstrumented library linked into an instrumented exe reports garbage).
# Configure into a SEPARATE build dir - do not mix ASan and non-ASan objects:
#   cmake -S ASCIICraft -B ASCIICraft/build-asan -DASCIICRAFT_TESTS_ASAN=ON
# ---------------------------------------------------------------------------
option(ASCIICRAFT_BUILD_TESTS "Build the ASCIICraft test suite" ON)
option(ASCIICRAFT_TESTS_ASAN  "Build with MSVC AddressSanitizer"  OFF)

if(ASCIICRAFT_TESTS_ASAN)
    if(NOT MSVC)
        message(FATAL_ERROR "ASCIICRAFT_TESTS_ASAN is MSVC-only.")
    endif()
    message(STATUS "ASCIICraft: AddressSanitizer ENABLED")

    # /RTC is incompatible with /fsanitize=address; strip it if a config set it.
    foreach(cfgFlags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_FASTDEBUG CMAKE_CXX_FLAGS_RELEASE)
        string(REGEX REPLACE "/RTC[1csu]+" "" ${cfgFlags} "${${cfgFlags}}")
    endforeach()

    add_compile_options(/fsanitize=address /Zi)
    add_link_options(/INCREMENTAL:NO /DEBUG)

    # ASan changes the ABI of std::string and std::vector by adding container
    # annotations, so instrumented objects cannot link against uninstrumented ones:
    #   ASCIIgL_FastDebug.lib(Logger.obj) : error LNK2038: mismatch detected for
    #   'annotate_string': value '0' doesn't match value '1'
    #
    # ASCIICraft links a PREBUILT ASCIIgL distribution that is not instrumented, so
    # the annotations must be switched off for the link to succeed.
    #
    # Nothing of value is lost. Annotations only add intra-container overflow
    # detection, which is already disabled at runtime via
    # ASAN_OPTIONS=detect_container_overflow=0 because oneTBB is an uninstrumented
    # DLL. Heap use-after-free - the reason ASan is here at all, for defect A -
    # is unaffected.
    add_compile_definitions(_DISABLE_STRING_ANNOTATION=1 _DISABLE_VECTOR_ANNOTATION=1)
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
# CONFIGURE_DEPENDS so a new .cpp under src/ is picked up by a plain build,
# matching the resource and test-suite globs. Without it CMake reuses the cached
# file list and a newly added source is silently left out of the library - which
# only surfaces later as an unresolved-symbol error at exe link time.
file(GLOB_RECURSE ASCIICraft_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE ASCIICraft_HEADERS
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

# main.cpp belongs to the executable only, so ASCIICraft_core can be linked by
# other executables (the test suite) without dragging in a second entry point.
set(ASCIICraft_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
list(REMOVE_ITEM ASCIICraft_SOURCES "${ASCIICraft_MAIN}")

# ---------------------------------------------------------------------------
# Tracy (TracyClient.cpp #includes .cpp files from public/common and public/client)
# ---------------------------------------------------------------------------
set(ASCIICraft_TRACY_PUBLIC "${ASCIIgL_SOURCE_VENDOR_DIR}/tracy/public")

# ---------------------------------------------------------------------------
# Core Library (everything except main.cpp)
#
# Linked by the game executable and by the test suite. STATIC rather than OBJECT
# so the linker pulls archive members lazily: a test that only touches the world
# code never pulls Game.obj, the GUI, or the sound system.
#
# NOTE: TracyClient.cpp is deliberately NOT in this archive. ASCIIgL.lib already
# contains a copy, and a second copy here risks LNK2005 when both get pulled.
# Each *executable* compiles it directly instead - which is what the game already
# did, and why its own .obj currently wins over the archive member.
# ---------------------------------------------------------------------------
add_library(ASCIICraft_core STATIC
    ${ASCIICraft_SOURCES}
    ${ASCIICraft_HEADERS}
)

# PUBLIC throughout: these are usage requirements the test executable must inherit.
target_include_directories(ASCIICraft_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${ASCIICraft_TRACY_PUBLIC}"          # ASCIIgLConfig forces TRACY_ENABLE but does not export this
)

# ASCIIgL::ASCIIgL carries glm::glm, the GLM_FORCE_* defines and TRACY_ENABLE via
# INTERFACE_COMPILE_DEFINITIONS. Do not restate the glm defines or add /arch:AVX2
# here - that would create the ABI mismatch it looks like it is preventing.
target_link_libraries(ASCIICraft_core PUBLIC ASCIIgL::ASCIIgL)
target_link_libraries(ASCIICraft_core PUBLIC TBB::tbb)          # not in ASCIIgL's link interface
target_link_libraries(ASCIICraft_core PUBLIC OpenAL::OpenAL)
target_link_libraries(ASCIICraft_core PUBLIC stb)

if(TARGET entt)
    target_link_libraries(ASCIICraft_core PUBLIC entt)
endif()

if(TARGET FastNoiseLite)
    target_link_libraries(ASCIICraft_core PUBLIC FastNoiseLite)
endif()

if(TARGET nlohmann_json)
    target_link_libraries(ASCIICraft_core PUBLIC nlohmann_json)
endif()

# Platform-specific
if(WIN32)
    target_compile_definitions(ASCIICraft_core PUBLIC NOMINMAX TRACY_ENABLE)
    target_link_libraries(ASCIICraft_core PUBLIC winmm)
endif()

set_target_properties(ASCIICraft_core PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_FASTDEBUG "${CMAKE_BINARY_DIR}/lib/FastDebug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE   "${CMAKE_BINARY_DIR}/lib/Release"
)

# ---------------------------------------------------------------------------
# Game Executable
# ---------------------------------------------------------------------------
add_executable(ASCIICraft
    "${ASCIICraft_MAIN}"
    "${ASCIICraft_TRACY_PUBLIC}/TracyClient.cpp"
)

target_link_libraries(ASCIICraft PRIVATE ASCIICraft_core)

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

# ---------------------------------------------------------------------------
# Tests
#
# enable_testing() must be called in this directory (or above) for ctest to
# pick up the tests registered in tests/CMakeLists.txt.
# ---------------------------------------------------------------------------
if(ASCIICRAFT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()