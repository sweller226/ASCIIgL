# vendor/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/entt/src/entt/entt.hpp")
    message(FATAL_ERROR
        "entt submodule not found at ${CMAKE_CURRENT_LIST_DIR}/entt. "
        "Run: git submodule update --init --recursive")
endif()
add_library(entt INTERFACE)
target_include_directories(entt INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/entt/src
)

if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/FastNoiseLite/Cpp/FastNoiseLite.h")
    message(FATAL_ERROR
        "FastNoiseLite submodule not found at ${CMAKE_CURRENT_LIST_DIR}/FastNoiseLite. "
        "Run: git submodule update --init --recursive")
endif()
add_library(FastNoiseLite INTERFACE)
target_include_directories(FastNoiseLite INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/FastNoiseLite/Cpp
)

if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/stb/stb_vorbis.c")
    message(FATAL_ERROR
        "stb submodule not found at ${CMAKE_CURRENT_LIST_DIR}/stb. "
        "Run: git submodule update --init --recursive")
endif()
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/stb
)
