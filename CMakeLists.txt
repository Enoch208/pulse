cmake_minimum_required(VERSION 3.25)

project(pulse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(pulse_warnings INTERFACE)
target_compile_options(pulse_warnings INTERFACE
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wsign-conversion
  -Wshadow
  -Wold-style-cast
  -Wnon-virtual-dtor
  -Woverloaded-virtual
  -Wcast-align
  -Wnull-dereference
  -Wdouble-promotion
  -Wformat=2
  -Wimplicit-fallthrough)

add_subdirectory(src)
add_subdirectory(apps)
add_subdirectory(bench)

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
  if(BUILD_TESTING)
    add_subdirectory(tests)
  endif()
endif()
