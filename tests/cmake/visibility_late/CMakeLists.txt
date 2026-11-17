# supportext_add_sources() called from a parent directory for a target defined in a
# subdirectory that was added BEFORE the parent created the imported target: the
# subdirectory cannot see it, so the helper must make it global (CMake 3.21 or later).
# A separate project, so that no other case has already made the target global.
# Configure and generate only:
#   cmake -S tests/cmake/visibility_late -B build
cmake_minimum_required(VERSION 3.16)
project(SupportExtVisibilityLateTest CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/SupportExt.cmake")
set(SX_TEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../text.cpp")
if(CMAKE_VERSION VERSION_LESS 3.21)
  return()
endif()
add_subdirectory(t)
# created after add_subdirectory(t): not visible from t
find_package(Threads REQUIRED)
add_library(OSMesa::OSMesa INTERFACE IMPORTED)
supportext_add_sources(vis_late OSMESA COMPONENTS THREADS TEXT)
