# Functional tests: the contracts of supportext_add_sources() that the plug-in
# repositories rely on (README.md and the header of cmake/SupportExt.cmake: components and
# their dependencies, argument order, linking, calls from other directories), and compile
# and link checks of SupportExt's header APIs. Target names start with functional_.
# Several cases were first written for a bug of the helper's first version, fixed in #46.

# SupportExt with add_subdirectory, as the plug-in repositories do; the helper is then
# called from this directory, the parent of SupportExt's
add_subdirectory("${SX_SUPPORTEXT_DIR}" supportext)

# 1. A call from the parent of add_subdirectory(SupportExt) (it failed with "unknown
#    component THREADS", #46), with the components of openfx-misc's Misc module;
#    helpers.cpp instantiates the helpers shared with openfx-misc (#238, #239, #231), and
#    position.cpp the header-only overlays, in one binary with those components.
sx_test_module(functional_parent helpers.cpp position.cpp)
supportext_add_sources(functional_parent COMPONENTS POSITION_INTERACT THREADS TRANSFORM3X3
                       TRANSFORM_INTERACT GENERATOR RAMP TRACKING)
# 2. Calling it again adds no source or link twice (#46).
get_property(_sources_before TARGET functional_parent PROPERTY SOURCES)
get_property(_libs_before TARGET functional_parent PROPERTY LINK_LIBRARIES)
supportext_add_sources(functional_parent COMPONENTS THREADS POSITION_INTERACT)
get_property(_sources_after TARGET functional_parent PROPERTY SOURCES)
get_property(_libs_after TARGET functional_parent PROPERTY LINK_LIBRARIES)
if(NOT _sources_after STREQUAL _sources_before OR NOT _libs_after STREQUAL _libs_before)
  message(FATAL_ERROR "a second supportext_add_sources() call added sources or links again")
endif()

# 3. Components, options and dependencies: each case is a module functional_<name> built
#    from <sources> (in this directory or common/), with one supportext_add_sources() call
#    per comma-separated argument list; the module must link and record the <expected>
#    components (SUPPORTEXT_COMPONENTS).
#    - position_interact_brings_text: the header-only overlays need TEXT (through OVERLAY,
#      as ofxsOverlayDraw.h's bitmapString calls TextRenderer), and no other component
#      pulls it in here (#46); position.cpp instantiates PositionInteract, a
#      PointsInteract subclass and both V2 overlay descriptors (#228, #223).
#    - all_with_opengl: ALL is every component and OPENGL (glad, EGL on Linux, dl) (#46).
#    - options_inside_components: OPENGL and EGL inside COMPONENTS don't end the list:
#      TEXT and FILEOPEN after them are still added (#46, openfx-io#102).
#    - option_before_components_two_calls: an option before COMPONENTS, then a second
#      call that adds new components and repeats one already added (EGL, through OPENGL)
#      (#46).
#    - openmp: ofxsOpenMP.cpp (a no-op without OpenMP) is added (#222).
#    <name>|<sources>|<calls>|<expected>
set(_cases
  "position_interact_brings_text|position.cpp|COMPONENTS POSITION_INTERACT|POSITION_INTERACT OVERLAY TEXT"
  "all_with_opengl|opengl.cpp|ALL|THREADS FILEOPEN LUT MULTIPLANE SHUTTER TRANSFORM3X3 TEXT OVERLAY POSITION_INTERACT TRANSFORM_INTERACT RECTANGLE_INTERACT GENERATOR RAMP TRACKING OGL_FUNCTIONS EGL OPENMP OPENGL GLAD"
  "options_inside_components|opengl.cpp text.cpp|COMPONENTS THREADS OPENGL EGL TEXT FILEOPEN|THREADS OPENGL EGL TEXT FILEOPEN GLAD"
  "option_before_components_two_calls|opengl.cpp|OPENGL COMPONENTS LUT,COMPONENTS MULTIPLANE EGL|OPENGL GLAD EGL LUT MULTIPLANE"
  "openmp|openmp.cpp|COMPONENTS OPENMP|OPENMP")
foreach(_case IN LISTS _cases)
  string(REPLACE "|" ";" _fields "${_case}")
  list(GET _fields 0 _name)
  list(GET _fields 1 _srcnames)
  list(GET _fields 2 _calls)
  list(GET _fields 3 _expected)
  set(_sources "")
  string(REPLACE " " ";" _srcnames "${_srcnames}")
  foreach(_s IN LISTS _srcnames)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_s}")
      list(APPEND _sources "${CMAKE_CURRENT_SOURCE_DIR}/${_s}")
    else()
      list(APPEND _sources "${SX_COMMON_DIR}/${_s}")
    endif()
  endforeach()
  sx_test_module(functional_${_name} ${_sources})
  string(REPLACE "," ";" _calls "${_calls}")
  foreach(_call IN LISTS _calls)
    string(REPLACE " " ";" _args "${_call}")
    supportext_add_sources(functional_${_name} ${_args})
  endforeach()
  string(REPLACE " " ";" _expected "${_expected}")
  sx_expect_components(functional_${_name} ${_expected})
endforeach()

# 4. A STATIC library with TEXT, linked into a MODULE: its links (OpenGL) must reach the
#    module's link, as $<LINK_ONLY:...> (#46). uses_static.cpp calls the library, so that
#    its archive members are pulled in.
add_library(functional_static STATIC "${SX_COMMON_DIR}/text.cpp")
set_target_properties(functional_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
supportext_add_sources(functional_static COMPONENTS TEXT)
sx_test_module(functional_uses_static uses_static.cpp)
target_link_libraries(functional_uses_static PRIVATE functional_static)

# 5. The helper include()d from two sibling directories, as with several SupportExt copies
#    in one tree (failed with "unknown component", #46).
add_subdirectory(sibling_a)
add_subdirectory(sibling_b)

# 6. A target defined here, the helper called from a subdirectory: the imported targets it
#    links (OpenGL::GL) must be visible from here (#46).
sx_test_module(functional_otherdir "${SX_COMMON_DIR}/text.cpp")
add_subdirectory(otherdir)
