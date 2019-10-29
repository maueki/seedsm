cmake_minimum_required(VERSION 2.8)
project(unit_test)

# Find includes in corresponding build directories
SET(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
SET(CMAKE_AUTOMOC ON)

FIND_PACKAGE(Qt5Core)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Core_EXECUTABLE_COMPILE_FLAGS}")

ADD_DEFINITIONS(
    ${Qt5Core_DEFINITIONS}
)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${Qt5Core_INCLUDE_DIRS}
)

file(GLOB SOURCES
  ./*.cpp)

set(CMAKE_CXX_FLAGS "-std=c++11")

add_executable(unit_test ${SOURCES})

target_link_libraries(unit_test
  ${Qt5Core_LIBRARIES}
  -lgtest -lgtest_main -lpthread)
