# test 5 of ../CMakeLists.txt: the helper included here and in sibling_a
include("${SX_SUPPORTEXT_DIR}/cmake/SupportExt.cmake")
sx_test_module(functional_sibling_b "${SX_COMMON_DIR}/text.cpp")
supportext_add_sources(functional_sibling_b COMPONENTS TEXT MULTIPLANE)
