# Regression test (#46): supportext_add_sources() called from another directory than the
# target's, with non-global imported targets created by the consumer, in the layouts that
# failed with a past version of the helper (named per case). One project, so that later
# cases see what earlier ones made global, in this order. Configure and generate only:
#   cmake -S tests/cmake/regression/visibility -B build
# A missing link target is an error at generate time.
# It also covers a contract that README.md and cmake/SupportExt.cmake promise (calls from
# another directory, with imported targets the consumer created): never remove it as a bug that
# can't come back.
cmake_minimum_required(VERSION 3.16)
project(SupportExtVisibilityTest CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/SupportExt.cmake")
set(SX_TEST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../common/text.cpp")

# what find_package(openfx) and find_package(OpenGL) create: non-global imported targets,
# visible here and in the subdirectories added below
add_library(openfx::Support INTERFACE IMPORTED)
find_package(OpenGL REQUIRED)

# A. target here, helper called from a subdirectory (the imported targets are visible
#    from both): rejected by baf86b0's check, fixed by 07257cc
add_library(vis_a MODULE ${SX_TEST_SRC})
add_subdirectory(a)

# F. target in a subdirectory, helper called from here: rejected by baf86b0, fixed by 07257cc
add_subdirectory(f)
supportext_add_sources(vis_f COMPONENTS TEXT)

# P. target in one subdirectory, Threads found and the helper called in a sibling:
#    Threads::Threads must be made global (CMake 3.21 or later); failed with a5764ed,
#    fixed by 07257cc
if(NOT CMAKE_VERSION VERSION_LESS 3.21)
  add_subdirectory(p1)
  add_subdirectory(p2)
endif()

# E. target in a subdirectory added before the imported target is created here, and
#    an ALIAS of a non-global imported target (CMake 3.21 or later; after P, which
#    must find Threads in its own directory); failed with 07257cc, fixed by ba8631b
if(NOT CMAKE_VERSION VERSION_LESS 3.21)
  add_subdirectory(e)
  add_library(visE::OSMesa INTERFACE IMPORTED)
  add_library(OSMesa::OSMesa ALIAS visE::OSMesa)
  find_package(Threads REQUIRED)
  supportext_add_sources(vis_e OSMESA COMPONENTS THREADS TEXT)
endif()

# X. an export set (what find_package(openfx) creates) in the calling directory: the
#    helper makes openfx2::Support global, so it must make openfx2::Api global too, or a
#    later find_package(openfx2) elsewhere fails ("Some (but not all) targets in this
#    export set were already defined"), as the check in x3 does (CMake 3.21 or later);
#    failed with ba8631b, fixed by 428aa58
if(NOT CMAKE_VERSION VERSION_LESS 3.21)
  add_subdirectory(x1)
  add_subdirectory(x2)
  add_subdirectory(x3)
endif()
