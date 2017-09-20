cmake_minimum_required(VERSION 2.8)
project(unit_test)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src)

file(GLOB SOURCES
  ./*.cpp)

add_executable(unit_test ${SOURCES})

target_link_libraries(unit_test -lev -lpthread -lgtest -lgtest_main)
