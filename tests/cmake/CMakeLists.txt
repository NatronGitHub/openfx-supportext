# Consumer tests for cmake/SupportExt.cmake, run by the CI:
#   cmake -S tests/cmake -B build -DOPENFX_DIR=<openfx checkout> && cmake --build build
# Each case links a MODULE with undefined symbols forbidden, so that a missing
# SupportExt component or library (OpenGL, dl, threads, TEXT...) fails the build.
cmake_minimum_required(VERSION 3.16)
project(SupportExtConsumerTests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OPENFX_DIR "" CACHE PATH "openfx checkout (NatronGitHub/openfx)")
if(NOT OPENFX_DIR)
  message(FATAL_ERROR "set -DOPENFX_DIR=<openfx checkout>")
endif()

# openfx first, so that supportext_add_sources() links openfx::Support
add_subdirectory("${OPENFX_DIR}" openfx EXCLUDE_FROM_ALL)
# SupportExt with add_subdirectory; the helper is then called from this directory
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." supportext)

# forbid undefined symbols in the modules
if(APPLE)
  set(SX_NO_UNDEFINED "") # undefined symbols are an error in macOS bundles
elseif(UNIX)
  set(SX_NO_UNDEFINED "-Wl,--no-undefined")
else()
  set(SX_NO_UNDEFINED "") # Windows DLLs cannot have undefined symbols
endif()

function(sx_test_module NAME)
  add_library(${NAME} MODULE ${ARGN})
  # Support/Plugins/include (ofxsProcessing.H...) is not in openfx::Support's usage requirements
  target_include_directories(${NAME} PRIVATE "${OPENFX_DIR}/Support/Plugins/include")
  target_link_options(${NAME} PRIVATE ${SX_NO_UNDEFINED})
endfunction()

# 1. called from the parent of add_subdirectory(SupportExt); header-only overlay
#    (ofxsPositionInteract.h needs TEXT through OverlayDraw::bitmapString)
sx_test_module(sx_parent position.cpp)
supportext_add_sources(sx_parent COMPONENTS POSITION_INTERACT THREADS TRANSFORM3X3
                       TRANSFORM_INTERACT GENERATOR RAMP TRACKING)
# calling it again adds nothing twice
get_property(_sources_before TARGET sx_parent PROPERTY SOURCES)
get_property(_libs_before TARGET sx_parent PROPERTY LINK_LIBRARIES)
supportext_add_sources(sx_parent COMPONENTS THREADS POSITION_INTERACT)
get_property(_sources_after TARGET sx_parent PROPERTY SOURCES)
get_property(_libs_after TARGET sx_parent PROPERTY LINK_LIBRARIES)
if(NOT _sources_after STREQUAL _sources_before OR NOT _libs_after STREQUAL _libs_before)
  message(FATAL_ERROR "a second supportext_add_sources() call added sources or links again")
endif()

# 1b. the header-only overlay alone must pull in TEXT
sx_test_module(sx_position position.cpp)
supportext_add_sources(sx_position COMPONENTS POSITION_INTERACT)

# 2. OPENGL (glad, EGL on Linux, dl), every component
sx_test_module(sx_all opengl.cpp)
supportext_add_sources(sx_all ALL)

# 2b. options among the COMPONENTS, in any order (OPENGL and EGL must not end the
#     list: FILEOPEN after them must still be added)
sx_test_module(sx_mixed opengl.cpp text.cpp)
supportext_add_sources(sx_mixed COMPONENTS THREADS OPENGL EGL TEXT FILEOPEN)
get_property(_sx_mixed TARGET sx_mixed PROPERTY SUPPORTEXT_COMPONENTS)
foreach(_c THREADS OPENGL EGL TEXT FILEOPEN GLAD)
  if(NOT _c IN_LIST _sx_mixed)
    message(FATAL_ERROR "sx_mixed: component ${_c} missing (got ${_sx_mixed})")
  endif()
endforeach()
sx_test_module(sx_mixed2 opengl.cpp)
supportext_add_sources(sx_mixed2 OPENGL COMPONENTS LUT)
supportext_add_sources(sx_mixed2 COMPONENTS MULTIPLANE EGL)

# 3. a STATIC library with TEXT, linked into a MODULE: OpenGL must propagate
add_library(sx_static STATIC text.cpp)
set_target_properties(sx_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
supportext_add_sources(sx_static COMPONENTS TEXT)
sx_test_module(sx_uses_static empty.cpp)
target_link_libraries(sx_uses_static PRIVATE sx_static)

# 4. the helper included from two sibling directories
add_subdirectory(sibling_a)
add_subdirectory(sibling_b)

# 5. a target defined here, the helper called from another directory
sx_test_module(sx_otherdir text.cpp)
add_subdirectory(otherdir)

# 6. separate projects, configured by the CI: tests/cmake/visibility (calls from
#    another directory with consumer-created imported targets), tests/cmake/visibility_late
#    and tests/cmake/oldpolicy
#    (a consumer with 3.1 policies, with CMake < 4)
