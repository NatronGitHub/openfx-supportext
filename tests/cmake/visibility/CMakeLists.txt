# supportext_add_sources() called from another directory than the target's, with
# non-global imported targets created by the consumer. Configure and generate only:
#   cmake -S tests/cmake/visibility -B build
# A missing link target is an error at generate time.
cmake_minimum_required(VERSION 3.16)
project(SupportExtVisibilityTest CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/SupportExt.cmake")
set(SX_TEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../text.cpp")

# what find_package(openfx) and find_package(OpenGL) create: non-global imported targets,
# visible here and in the subdirectories added below
add_library(openfx::Support INTERFACE IMPORTED)
find_package(OpenGL REQUIRED)

# A. target here, helper called from a subdirectory (the imported targets are visible
#    from both)
add_library(vis_a MODULE ${SX_TEST_SRC})
add_subdirectory(a)

# F. target in a subdirectory, helper called from here
add_subdirectory(f)
supportext_add_sources(vis_f COMPONENTS TEXT)

# P. target in one subdirectory, Threads found and the helper called in a sibling:
#    Threads::Threads must be made global (CMake 3.21 or later)
if(NOT CMAKE_VERSION VERSION_LESS 3.21)
  add_subdirectory(p1)
  add_subdirectory(p2)
endif()
