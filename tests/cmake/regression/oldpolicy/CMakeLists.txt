# Regression test (#46, baf86b0): a consumer that includes SupportExt.cmake under old
# policies (cmake_minimum_required(VERSION 3.1): CMP0057 OLD, no if(IN_LIST)), as openfx-io
# and openfx-arena did then (both declare 3.21...3.31 now). The helper's cmake_policy(PUSH)
# and VERSION 3.16...3.31 make it work under any consumer policies.
# Configure only, with CMake < 4 (CMake 4 rejects policy versions below 3.5):
#   cmake -S tests/cmake/regression/oldpolicy -B build
cmake_minimum_required(VERSION 3.1)
project(SupportExtOldPolicyTest CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/SupportExt.cmake")
add_library(sx_oldpolicy MODULE ../../common/text.cpp)
supportext_add_sources(sx_oldpolicy OPENGL COMPONENTS THREADS TEXT TRANSFORM3X3 TRANSFORM_INTERACT MULTIPLANE)
