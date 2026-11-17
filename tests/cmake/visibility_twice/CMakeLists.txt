# A source directory added twice: the target is defined in the first instance, and an
# imported target is created (through an ALIAS) and the helper called in the second,
# which has the same SOURCE_DIR but another BINARY_DIR: the helper must make it global
# and link the aliased target (CMake 3.21 or later). Configure and generate only:
#   cmake -S tests/cmake/visibility_twice -B build
cmake_minimum_required(VERSION 3.16)
project(SupportExtVisibilityTwiceTest CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/SupportExt.cmake")
set(SX_TEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../text.cpp")
if(CMAKE_VERSION VERSION_LESS 3.21)
  return()
endif()
set(SX_VIS_TWICE_DEFINE ON)
add_subdirectory(twice twice1)
set(SX_VIS_TWICE_DEFINE OFF)
add_subdirectory(twice twice2)
