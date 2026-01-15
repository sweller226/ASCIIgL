# vendor/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# If the header is vendor/FastNoiseLite/FastNoiseLite.h
add_library(FastNoiseLite INTERFACE)
target_include_directories(FastNoiseLite INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/FastNoiseLite
)

# EnTT (header-only)
add_library(entt INTERFACE)
target_include_directories(entt INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/entt/single_include
)
