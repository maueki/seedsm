cmake_minimum_required(VERSION 2.8)
project(toggle)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

add_executable(toggle main.cpp)

target_link_libraries(toggle -lev -lpthread)
