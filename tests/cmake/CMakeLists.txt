# Consumer tests of cmake/SupportExt.cmake (supportext_add_sources), run by the CI
# (.github/workflows/build-one.yml, "CMake helper (consumer tests)"):
#   cmake -S tests/cmake -B build -DOPENFX_DIR=<openfx checkout> && cmake --build build
# Each test target is a MODULE linked with undefined symbols forbidden, so that a missing
# SupportExt component or library (OpenGL, dl, threads, TEXT...) fails the build.
#
#   common/      shared code: sx_test_module() and sx_expect_components()
#                (SxTest.cmake), and the sources used by several tests
#   functional/  this build: the helper's contracts that the plug-in repositories rely
#                on, and compile and link checks of SupportExt's header APIs
#                (targets functional_*)
#   regression/  contrived layouts that reproduced a specific failure of a past version
#                of the helper (each names the commit it failed with): separate projects,
#                each configured on its own by the CI (cmake -S tests/cmake/regression/<name>)
#
# The compile check of every SupportExt source is the top-level CMakeLists.txt.
cmake_minimum_required(VERSION 3.16)
# macOS 14.0 deployment target, as the plug-in repositories (VFX Reference Platform CY2027,
# openfx#160); a toolchain file, a cross build or MACOSX_DEPLOYMENT_TARGET take precedence
if(CMAKE_HOST_APPLE AND NOT CMAKE_TOOLCHAIN_FILE AND (NOT CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Darwin") AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
  set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "Minimum macOS version")
endif()
project(SupportExtConsumerTests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OPENFX_DIR "" CACHE PATH "openfx checkout (NatronGitHub/openfx)")
if(NOT OPENFX_DIR)
  message(FATAL_ERROR "set -DOPENFX_DIR=<openfx checkout>")
endif()

# openfx first, so that supportext_add_sources() links openfx::Support
add_subdirectory("${OPENFX_DIR}" openfx EXCLUDE_FROM_ALL)

include("${CMAKE_CURRENT_SOURCE_DIR}/common/SxTest.cmake")

add_subdirectory(functional)
