

# Layout is vendor/glm/glm/glm.hpp so include dir should be vendor/glm
set(ASCIIgL_GLM_DIR "${CMAKE_CURRENT_LIST_DIR}/glm")

if(NOT TARGET glm::glm)
  add_library(glm INTERFACE)
  target_include_directories(glm INTERFACE
    "$<BUILD_INTERFACE:${ASCIIgL_GLM_DIR}>"
    "$<INSTALL_INTERFACE:include>"
  )
  add_library(glm::glm ALIAS glm)
  message(STATUS "Created local glm::glm INTERFACE target with include dir: ${ASCIIgL_GLM_DIR}")
endif()

# oneTBB (parallel LUT precompute and other engine workloads)
if(NOT TARGET TBB::tbb)
  set(TBB_TEST OFF CACHE BOOL "" FORCE)
  set(TBB_STRICT OFF CACHE BOOL "" FORCE)
  set(TBB_BUILD_TBBMALLOC OFF CACHE BOOL "" FORCE)
  add_subdirectory(oneTBB EXCLUDE_FROM_ALL)
endif()