# CMake helper to compile openfx-supportext into an OpenFX plug-in target.
#
#   include(<SupportExt dir>/cmake/SupportExt.cmake)   # or add_subdirectory(<SupportExt dir>)
#   supportext_add_sources(<target> [ALL] [OPENGL] [OSMESA] [EGL]
#                          [COMPONENTS <name>...])
#
# The selected SupportExt sources are added to <target> (target_sources PRIVATE),
# so they are compiled with the target's own compile definitions. This matters:
# OFX_EXTENSIONS_*, OFX_SUPPORTS_OPENGLRENDER, HAVE_OSMESA and the DrawSuite
# support (OFXS_HAVE_DRAWSUITE / OFXS_SUPPORT_HAS_DRAWCONTEXT) change class
# layouts, so SupportExt must not be compiled separately with other values
# (One Definition Rule). Set those definitions on <target>, as for the plug-in's
# own sources.
#
# Components (dependencies are added automatically):
#   THREADS             tinythread.cpp ofxsThreadSuite.cpp
#   FILEOPEN            ofxsFileOpen.cpp
#   LUT                 ofxsLut.cpp
#   MULTIPLANE          ofxsMultiPlane.cpp
#   SHUTTER             ofxsShutter.cpp
#   TRANSFORM3X3        ofxsTransform3x3.cpp (+ SHUTTER)
#   TRANSFORM_INTERACT  ofxsTransformInteract.cpp
#   RECTANGLE_INTERACT  ofxsRectangleInteract.cpp
#   GENERATOR           ofxsGenerator.cpp (+ RECTANGLE_INTERACT)
#   TEXT                ofxsOGLTextRenderer.cpp ofxsOGLFontData.cpp
#   RAMP                ofxsRamp.cpp (+ TEXT)
#   TRACKING            ofxsTracking.cpp (+ TEXT)
#   OGL_FUNCTIONS       ofxsOGLFunctions_gl.cpp (the GL_GPU function table, + glad)
#   EGL                 glad/gladegl.cpp (the EGL loader)
# Options:
#   ALL     every component above except EGL, plus OPENGL (not OSMESA)
#   OPENGL  the OpenGL loader and helpers: glad/glad.cpp ofxsOGLUtilities.cpp,
#           plus TEXT and OGL_FUNCTIONS
#   EGL     same as COMPONENTS EGL
#   OSMESA  ofxsOGLFunctions_mesa.cpp (the GL_CPU function table), and HAVE_OSMESA on
#           <target>, which is linked with the imported target OSMesa::OSMesa. The
#           consumer must define OSMesa::OSMesa (include directories, the OSMesa
#           library, and its dependencies, e.g. a mangled OSMesa and LLVM, see
#           openfx-misc's Shadertoy); it is an error if it does not exist.
#
# The overlay interacts (RECTANGLE_INTERACT, GENERATOR, TRANSFORM_INTERACT, RAMP,
# TRACKING), TEXT, OGL_FUNCTIONS and OPENGL call OpenGL: for those, <target> is
# linked with OpenGL::GL (find_package(OpenGL)).
#
# If the target openfx::Support exists (openfx's CMake: add_subdirectory(openfx) or
# find_package(openfx)), <target> is linked with it, which also brings the openfx
# include directories and OFX_* definitions. Otherwise the consumer must compile the
# openfx Support library and set its include directories itself.
#
# Linking is done through the LINK_LIBRARIES target property, so it works whichever
# target_link_libraries signature (plain or PRIVATE/PUBLIC) the consumer uses.
#
# Calling supportext_add_sources() several times for the same target is fine: each
# source is added once.

if(COMMAND supportext_add_sources)
  return()
endif()

# the SupportExt directory (the first SupportExt included wins, see return() above)
get_filename_component(_supportext_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set_property(GLOBAL PROPERTY SUPPORTEXT_DIR "${_supportext_dir}")
unset(_supportext_dir)

if(CMAKE_VERSION VERSION_LESS 3.16)
  message(FATAL_ERROR "SupportExt.cmake needs CMake 3.16 or later")
endif()

# component -> sources (relative to SUPPORTEXT_DIR) and dependencies
set(_SUPPORTEXT_THREADS_SOURCES            tinythread.cpp ofxsThreadSuite.cpp)
set(_SUPPORTEXT_FILEOPEN_SOURCES           ofxsFileOpen.cpp)
set(_SUPPORTEXT_LUT_SOURCES                ofxsLut.cpp)
set(_SUPPORTEXT_MULTIPLANE_SOURCES         ofxsMultiPlane.cpp)
set(_SUPPORTEXT_SHUTTER_SOURCES            ofxsShutter.cpp)
set(_SUPPORTEXT_TRANSFORM3X3_SOURCES       ofxsTransform3x3.cpp)
set(_SUPPORTEXT_TRANSFORM3X3_DEPENDS       SHUTTER)
set(_SUPPORTEXT_TRANSFORM_INTERACT_SOURCES ofxsTransformInteract.cpp)
set(_SUPPORTEXT_RECTANGLE_INTERACT_SOURCES ofxsRectangleInteract.cpp)
set(_SUPPORTEXT_GENERATOR_SOURCES          ofxsGenerator.cpp)
set(_SUPPORTEXT_GENERATOR_DEPENDS          RECTANGLE_INTERACT)
set(_SUPPORTEXT_TEXT_SOURCES               ofxsOGLTextRenderer.cpp ofxsOGLFontData.cpp)
set(_SUPPORTEXT_RAMP_SOURCES               ofxsRamp.cpp)
set(_SUPPORTEXT_RAMP_DEPENDS               TEXT)
set(_SUPPORTEXT_TRACKING_SOURCES           ofxsTracking.cpp)
set(_SUPPORTEXT_TRACKING_DEPENDS           TEXT)
set(_SUPPORTEXT_GLAD_SOURCES               glad/glad.cpp)
set(_SUPPORTEXT_OGL_FUNCTIONS_SOURCES      ofxsOGLFunctions_gl.cpp)
set(_SUPPORTEXT_OGL_FUNCTIONS_DEPENDS      GLAD)
set(_SUPPORTEXT_OPENGL_SOURCES             ofxsOGLUtilities.cpp)
set(_SUPPORTEXT_OPENGL_DEPENDS             GLAD TEXT OGL_FUNCTIONS)
set(_SUPPORTEXT_EGL_SOURCES                glad/gladegl.cpp)
set(_SUPPORTEXT_OSMESA_SOURCES             ofxsOGLFunctions_mesa.cpp)
set(_SUPPORTEXT_COMPONENTS THREADS FILEOPEN LUT MULTIPLANE SHUTTER TRANSFORM3X3
    TRANSFORM_INTERACT RECTANGLE_INTERACT GENERATOR TEXT RAMP TRACKING OGL_FUNCTIONS EGL)
# components that call OpenGL
set(_SUPPORTEXT_GL_COMPONENTS TRANSFORM_INTERACT RECTANGLE_INTERACT GENERATOR TEXT RAMP
    TRACKING GLAD OGL_FUNCTIONS OPENGL)

function(supportext_add_sources TARGET)
  cmake_parse_arguments(SX "ALL;OPENGL;OSMESA;EGL" "" "COMPONENTS" ${ARGN})
  get_property(SUPPORTEXT_DIR GLOBAL PROPERTY SUPPORTEXT_DIR)
  if(NOT TARGET ${TARGET})
    message(FATAL_ERROR "supportext_add_sources: ${TARGET} is not a target")
  endif()
  if(SX_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "supportext_add_sources: unknown arguments: ${SX_UNPARSED_ARGUMENTS}")
  endif()

  set(_todo ${SX_COMPONENTS})
  if(SX_ALL)
    list(APPEND _todo ${_SUPPORTEXT_COMPONENTS} OPENGL)
    list(REMOVE_ITEM _todo EGL)
    if(SX_EGL OR "EGL" IN_LIST SX_COMPONENTS)
      list(APPEND _todo EGL)
    endif()
  endif()
  foreach(_opt OPENGL OSMESA EGL)
    if(SX_${_opt})
      list(APPEND _todo ${_opt})
    endif()
  endforeach()

  # resolve the dependencies
  set(_done "")
  while(_todo)
    list(GET _todo 0 _c)
    list(REMOVE_AT _todo 0)
    string(TOUPPER "${_c}" _c)
    if(NOT DEFINED _SUPPORTEXT_${_c}_SOURCES)
      message(FATAL_ERROR "supportext_add_sources: unknown component ${_c} "
        "(known: ${_SUPPORTEXT_COMPONENTS}; options: ALL OPENGL EGL OSMESA)")
    endif()
    list(FIND _done ${_c} _i)
    if(_i EQUAL -1)
      list(APPEND _done ${_c})
      list(APPEND _todo ${_SUPPORTEXT_${_c}_DEPENDS})
    endif()
  endwhile()

  # sources, once per target
  get_property(_added TARGET ${TARGET} PROPERTY SUPPORTEXT_COMPONENTS)
  set(_sources "")
  set(_need_gl OFF)
  foreach(_c ${_done})
    list(FIND _added ${_c} _i)
    if(_i EQUAL -1)
      foreach(_s ${_SUPPORTEXT_${_c}_SOURCES})
        list(APPEND _sources "${SUPPORTEXT_DIR}/${_s}")
      endforeach()
      set_property(TARGET ${TARGET} APPEND PROPERTY SUPPORTEXT_COMPONENTS ${_c})
    endif()
    list(FIND _SUPPORTEXT_GL_COMPONENTS ${_c} _i)
    if(NOT _i EQUAL -1)
      set(_need_gl ON)
    endif()
  endforeach()
  if(_sources)
    target_sources(${TARGET} PRIVATE ${_sources})
  endif()

  # include directories and definitions, once per target
  get_property(_setup TARGET ${TARGET} PROPERTY SUPPORTEXT_SETUP)
  if(NOT _setup)
    set_property(TARGET ${TARGET} PROPERTY SUPPORTEXT_SETUP ON)
    target_include_directories(${TARGET} PRIVATE "${SUPPORTEXT_DIR}" "${SUPPORTEXT_DIR}/glad")
    if(TARGET openfx::Support)
      set_property(TARGET ${TARGET} APPEND PROPERTY LINK_LIBRARIES openfx::Support)
    endif()
  endif()
  if(SX_OSMESA)
    if(NOT TARGET OSMesa::OSMesa)
      message(FATAL_ERROR "supportext_add_sources(${TARGET} OSMESA): define the imported target "
        "OSMesa::OSMesa first (OSMesa include directories and libraries)")
    endif()
    target_compile_definitions(${TARGET} PRIVATE HAVE_OSMESA)
    get_property(_mesa TARGET ${TARGET} PROPERTY SUPPORTEXT_OSMESA_LINKED)
    if(NOT _mesa)
      set_property(TARGET ${TARGET} PROPERTY SUPPORTEXT_OSMESA_LINKED ON)
      set_property(TARGET ${TARGET} APPEND PROPERTY LINK_LIBRARIES OSMesa::OSMesa)
    endif()
  endif()
  get_property(_gl TARGET ${TARGET} PROPERTY SUPPORTEXT_OPENGL_LINKED)
  if(_need_gl AND NOT _gl)
    find_package(OpenGL REQUIRED)
    set_property(TARGET ${TARGET} PROPERTY SUPPORTEXT_OPENGL_LINKED ON)
    set_property(TARGET ${TARGET} APPEND PROPERTY LINK_LIBRARIES OpenGL::GL)
  endif()
endfunction()
