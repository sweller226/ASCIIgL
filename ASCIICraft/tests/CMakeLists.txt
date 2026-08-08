# ---------------------------------------------------------------------------
# ASCIICraft test suite
#
# One executable linking ASCIICraft_core, so tests drive the exact objects the
# game ships. Runs fully headless: nothing here initializes D3D11 or a window.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# doctest
# ---------------------------------------------------------------------------
set(DOCTEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/doctest")

if(NOT EXISTS "${DOCTEST_DIR}/doctest/doctest.h")
    message(FATAL_ERROR
        "doctest submodule not found at ${DOCTEST_DIR}. "
        "Run: git submodule update --init --recursive")
endif()

if(NOT TARGET doctest)
    add_library(doctest INTERFACE)
    target_include_directories(doctest INTERFACE "${DOCTEST_DIR}")
endif()

# ---------------------------------------------------------------------------
# Sources
#
# Globbed recursively so adding a new test area is just a new directory with
# .cpp files in it - no CMake edit required. CONFIGURE_DEPENDS makes the build
# system re-glob when files appear or disappear.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE ASCIICraft_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

file(GLOB_RECURSE ASCIICraft_TEST_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

# ---------------------------------------------------------------------------
# Test executable
#
# TracyClient.cpp is compiled here rather than inherited from ASCIICraft_core -
# see the note in the parent CMakeLists. ASCIIgLConfig forces TRACY_ENABLE
# unconditionally, so the symbols must come from somewhere in every executable.
# ---------------------------------------------------------------------------
add_executable(ASCIICraft_tests
    ${ASCIICraft_TEST_SOURCES}
    ${ASCIICraft_TEST_HEADERS}
    "${ASCIICraft_TRACY_PUBLIC}/TracyClient.cpp"
)

target_link_libraries(ASCIICraft_tests PRIVATE ASCIICraft_core doctest)

# Lets test files include "support/TestPaths.hpp" etc. relative to tests/.
target_include_directories(ASCIICraft_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# Emit alongside the game so res/ and the runtime DLLs are already staged there.
set_target_properties(ASCIICraft_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_FASTDEBUG "${CMAKE_BINARY_DIR}/bin/FastDebug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE   "${CMAKE_BINARY_DIR}/bin/Release"
)

# ---------------------------------------------------------------------------
# Runtime staging
#
# Block registration reads res/blockstates and res/models. The game target
# stages those, but the test exe must not depend on the game having been built.
# ---------------------------------------------------------------------------
add_dependencies(ASCIICraft_tests ASCIICraftResources)

if(WIN32)
    add_custom_command(TARGET ASCIICraft_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../res"
            "$<TARGET_FILE_DIR:ASCIICraft_tests>/res"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:tbb>"
            "$<TARGET_FILE_DIR:ASCIICraft_tests>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:OpenAL::OpenAL>"
            "$<TARGET_FILE_DIR:ASCIICraft_tests>/OpenAL32.dll"
        COMMENT "Staging test resources and runtime DLLs"
    )
endif()

# ---------------------------------------------------------------------------
# CTest registration
#
# doctest_discover_tests runs the built exe with --list-test-cases and registers
# one CTest entry per TEST_CASE. ADD_LABELS derives labels from the TEST_SUITE
# name, so the "<area>.<tier>.<topic>" convention gives us `ctest -L world`,
# `ctest -L tier1`, and so on for free.
# ---------------------------------------------------------------------------
include("${DOCTEST_DIR}/scripts/cmake/doctest.cmake")

doctest_discover_tests(ASCIICraft_tests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:ASCIICraft_tests>"
    ADD_LABELS 1
)
