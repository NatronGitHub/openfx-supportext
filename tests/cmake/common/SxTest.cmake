# Shared code of the consumer tests (included by tests/cmake/CMakeLists.txt).

# the SupportExt checkout and these shared sources (text.cpp, opengl.cpp)
get_filename_component(SX_SUPPORTEXT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(SX_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}")

# forbid undefined symbols in the modules
if(APPLE)
  set(SX_NO_UNDEFINED "") # undefined symbols are an error in macOS bundles
elseif(UNIX)
  set(SX_NO_UNDEFINED "-Wl,--no-undefined")
else()
  set(SX_NO_UNDEFINED "") # Windows DLLs cannot have undefined symbols
endif()

# sx_test_module(<name> <sources>...): a MODULE that must link without undefined symbols
function(sx_test_module NAME)
  add_library(${NAME} MODULE ${ARGN})
  # Support/Plugins/include (ofxsProcessing.H...) is not in openfx::Support's usage requirements
  target_include_directories(${NAME} PRIVATE "${OPENFX_DIR}/Support/Plugins/include")
  target_link_options(${NAME} PRIVATE ${SX_NO_UNDEFINED})
endfunction()

# sx_expect_components(<target> <component>...): fail unless supportext_add_sources()
# recorded each component in the target's SUPPORTEXT_COMPONENTS
function(sx_expect_components TARGET)
  get_property(_got TARGET ${TARGET} PROPERTY SUPPORTEXT_COMPONENTS)
  foreach(_c ${ARGN})
    if(NOT _c IN_LIST _got)
      message(FATAL_ERROR "${TARGET}: component ${_c} missing (got ${_got})")
    endif()
  endforeach()
endfunction()
