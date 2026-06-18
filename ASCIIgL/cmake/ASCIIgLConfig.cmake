# ============================================================================
# ASCIIgLConfig.cmake
# ============================================================================
# Usage in consumer project:
#
#   list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/ASCIIgL-vX.X.X/cmake")
#   find_package(ASCIIgL REQUIRED)
#   target_link_libraries(MyTarget PRIVATE ASCIIgL::ASCIIgL)
#
# ============================================================================

cmake_minimum_required(VERSION 3.16)

# Root of the installed package (folder containing /cmake, /include, /lib)
get_filename_component(ASCIIgL_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# ============================================================================
# Paths
# ============================================================================
set(ASCIIgL_INCLUDE_DIR "${ASCIIgL_ROOT}/include")
set(ASCIIgL_LIB_DIR     "${ASCIIgL_ROOT}/lib")
set(ASCIIgL_VENDOR_DIR  "${ASCIIgL_ROOT}/vendor")

# ============================================================================
# Locate libraries
# ============================================================================
find_library(ASCIIgL_LIB_RELEASE
    NAMES ASCIIgL
    PATHS "${ASCIIgL_LIB_DIR}"
    NO_DEFAULT_PATH
    REQUIRED
)

find_library(ASCIIgL_LIB_FASTDEBUG
    NAMES ASCIIgL_FastDebug
    PATHS "${ASCIIgL_LIB_DIR}"
    NO_DEFAULT_PATH
)
if(NOT ASCIIgL_LIB_FASTDEBUG)
    message(WARNING "ASCIIgL FastDebug library (ASCIIgL_FastDebug.lib) not found; "
                    "FastDebug builds will link the Release library and RenderDoc Present will be disabled.")
    set(ASCIIgL_LIB_FASTDEBUG "${ASCIIgL_LIB_RELEASE}")
elseif(ASCIIgL_LIB_FASTDEBUG STREQUAL ASCIIgL_LIB_RELEASE)
    message(WARNING "ASCIIgL FastDebug library resolves to the Release .lib; "
                    "RenderDoc Present will be disabled.")
endif()

# ============================================================================
# GLM (required transitive dependency)
# ============================================================================
find_package(glm QUIET)
if(NOT TARGET glm::glm)
    # Fall back to the vendored copy shipped inside the package
    if(EXISTS "${ASCIIgL_VENDOR_DIR}/CMakeLists.txt")
        add_subdirectory("${ASCIIgL_VENDOR_DIR}" "${CMAKE_BINARY_DIR}/ASCIIgL_vendor" EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR "ASCIIgL: glm::glm not found and no vendored fallback available.")
    endif()
endif()

# ============================================================================
# SIMD compile definitions (match ASCIIgL build settings)
# ============================================================================
set(ASCIIgL_COMPILE_DEFINITIONS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    TRACY_ENABLE
    GLM_FORCE_SIMD_AVX2
    GLM_FORCE_ALIGNED_GENTYPES
    GLM_FORCE_INTRINSICS
)

# ============================================================================
# Imported target
# ============================================================================
if(NOT TARGET ASCIIgL::ASCIIgL)
    add_library(ASCIIgL::ASCIIgL STATIC IMPORTED)
    set_target_properties(ASCIIgL::ASCIIgL PROPERTIES
        # Multi-config generators (Visual Studio) require IMPORTED_CONFIGURATIONS
        # or they ignore IMPORTED_LOCATION_FASTDEBUG and fall back to Release .lib.
        IMPORTED_CONFIGURATIONS "RELEASE;FASTDEBUG"
        IMPORTED_LOCATION_RELEASE "${ASCIIgL_LIB_RELEASE}"
        IMPORTED_LOCATION_FASTDEBUG "${ASCIIgL_LIB_FASTDEBUG}"
        IMPORTED_LOCATION_MINSIZEREL "${ASCIIgL_LIB_RELEASE}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${ASCIIgL_LIB_RELEASE}"

        INTERFACE_INCLUDE_DIRECTORIES "${ASCIIgL_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "glm::glm;winmm;dwrite;d3d11;d3dcompiler;dxgi"
        INTERFACE_COMPILE_DEFINITIONS "${ASCIIgL_COMPILE_DEFINITIONS}"
    )
endif()

message(STATUS "Found ASCIIgL Release: ${ASCIIgL_LIB_RELEASE}")
message(STATUS "Found ASCIIgL FastDebug: ${ASCIIgL_LIB_FASTDEBUG}")