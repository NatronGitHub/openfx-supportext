# A consumer that includes SupportExt.cmake under old policies, as openfx-io and
# openfx-arena do (cmake_minimum_required(VERSION 3.1): CMP0057 OLD, no if(IN_LIST)).
# Configure only, with CMake < 4 (CMake 4 rejects policy versions below 3.5):
#   cmake -S tests/cmake/oldpolicy -B build
cmake_minimum_required(VERSION 3.1)
project(SupportExtOldPolicyTest CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/SupportExt.cmake")
add_library(sx_oldpolicy MODULE ../text.cpp)
supportext_add_sources(sx_oldpolicy OPENGL COMPONENTS THREADS TEXT TRANSFORM3X3 TRANSFORM_INTERACT MULTIPLANE)
