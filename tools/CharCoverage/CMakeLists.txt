cmake_minimum_required(VERSION 3.10)
project(CharCoverage LANGUAGES CXX)

add_executable(char_coverage char_coverage.cpp)
target_compile_features(char_coverage PRIVATE cxx_std_17)
target_link_libraries(char_coverage PRIVATE dwrite)
