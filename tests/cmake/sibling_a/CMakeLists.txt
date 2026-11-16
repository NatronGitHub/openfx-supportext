include("${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/SupportExt.cmake")
sx_test_module(sx_sibling_a ../text.cpp)
supportext_add_sources(sx_sibling_a COMPONENTS TEXT LUT)
