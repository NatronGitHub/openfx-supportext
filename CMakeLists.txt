# openfx-supportext is compiled into each plug-in target, with the plug-in's own
# compile definitions: see cmake/SupportExt.cmake (supportext_add_sources).
#
#   add_subdirectory(SupportExt)                  # or
#   include(SupportExt/cmake/SupportExt.cmake)
#   supportext_add_sources(MyPlugin COMPONENTS THREADS TRANSFORM3X3 TRANSFORM_INTERACT)
#
# Used as the top-level project, it builds a compile check of every source
# (requires -DOPENFX_DIR=<openfx checkout>), which the CI's "Compile" steps run; the
# consumer tests are in tests/cmake.
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  # standalone compile check: the CMake range of openfx's cmake/OpenFX.cmake (openfx#196);
  # add_subdirectory(SupportExt) from a consumer keeps 3.16
  cmake_minimum_required(VERSION 3.21...3.31)
  # macOS 14.0 deployment target, as the plug-in repositories (VFX Reference Platform CY2027,
  # openfx#160); a toolchain file, a cross build or MACOSX_DEPLOYMENT_TARGET take precedence
  if(CMAKE_HOST_APPLE AND NOT CMAKE_TOOLCHAIN_FILE AND (NOT CMAKE_SYSTEM_NAME OR CMAKE_SYSTEM_NAME STREQUAL "Darwin") AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0 CACHE STRING "Minimum macOS version")
  endif()
  project(SupportExt CXX)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/cmake/SupportExt.cmake")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(OPENFX_DIR "" CACHE PATH "openfx checkout (NatronGitHub/openfx), for the compile check")
  if(NOT OPENFX_DIR)
    message(FATAL_ERROR "Building SupportExt on its own is a compile check: set -DOPENFX_DIR=<openfx checkout>")
  endif()
  if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
  endif()
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  add_library(SupportExtCheck OBJECT)
  target_include_directories(SupportExtCheck PRIVATE
    "${OPENFX_DIR}/include" "${OPENFX_DIR}/Support/include" "${OPENFX_DIR}/Support/Plugins/include")
  target_compile_definitions(SupportExtCheck PRIVATE
    OFX_EXTENSIONS_VEGAS OFX_EXTENSIONS_NUKE OFX_EXTENSIONS_NATRON OFX_EXTENSIONS_TUTTLE
    OFX_SUPPORTS_OPENGLRENDER $<$<BOOL:${APPLE}>:GL_SILENCE_DEPRECATION>)
  supportext_add_sources(SupportExtCheck ALL)
  # ofxsOGLFunctions_mesa.cpp (the OSMESA option) needs an OSMesa::OSMesa target:
  # compile it without HAVE_OSMESA, which at least checks it is a valid file
  target_sources(SupportExtCheck PRIVATE "${CMAKE_CURRENT_LIST_DIR}/ofxsOGLFunctions_mesa.cpp")
  # ofxsOpenMP.h only does something with OpenMP: compile it with OpenMP where CMake finds it
  find_package(OpenMP COMPONENTS CXX QUIET)
  if(OpenMP_CXX_FOUND)
    add_library(SupportExtOpenMPCheck OBJECT "${CMAKE_CURRENT_LIST_DIR}/ofxsOpenMP.cpp")
    target_include_directories(SupportExtOpenMPCheck PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(SupportExtOpenMPCheck PRIVATE OpenMP::OpenMP_CXX)
  endif()
endif()
