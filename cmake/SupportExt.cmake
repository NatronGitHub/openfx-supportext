# CMake helper to compile openfx-supportext into an OpenFX plug-in target.
#
#   include(<SupportExt dir>/cmake/SupportExt.cmake)   # or add_subdirectory(<SupportExt dir>)
#   supportext_add_sources(<target> [ALL] [OPENGL] [OSMESA] [EGL]
#                          [COMPONENTS <name>...])
#
# The arguments may come in any order, and OPENGL, OSMESA and EGL may also be
# listed among the COMPONENTS.
#
# The selected SupportExt sources are added to <target> (target_sources PRIVATE),
# so they are compiled with the target's own compile definitions. This matters:
# OFX_EXTENSIONS_*, OFX_SUPPORTS_OPENGLRENDER and the DrawSuite support
# (OFXS_HAVE_DRAWSUITE) change class layouts, so
# SupportExt must not be compiled separately with other values (One Definition
# Rule). Set those definitions on <target>, as for the plug-in's own sources (or
# link openfx::Support, whose usage requirements carry them).
#
# Components (dependencies are added automatically):
#   THREADS             ofxsThreadSuite.cpp (links Threads::Threads)
#   FILEOPEN            ofxsFileOpen.cpp
#   LUT                 ofxsLut.cpp
#   MULTIPLANE          ofxsMultiPlane.cpp
#   SHUTTER             ofxsShutter.cpp
#   TRANSFORM3X3        ofxsTransform3x3.cpp (+ SHUTTER)
#   TEXT                ofxsOGLTextRenderer.cpp ofxsOGLFontData.cpp
#   OVERLAY             no source, for plug-ins whose own code draws text with the
#                       header-only overlay code (OverlayDraw::bitmapString,
#                       ofxsPositionInteract.h): links OpenGL::GL (+ TEXT)
#   POSITION_INTERACT   same as OVERLAY, for plug-ins using ofxsPositionInteract.h
#                       (PositionInteract, PointsInteract)
#   TRANSFORM_INTERACT  ofxsTransformInteract.cpp
#   RECTANGLE_INTERACT  ofxsRectangleInteract.cpp
#   GENERATOR           ofxsGenerator.cpp (+ RECTANGLE_INTERACT)
#   RAMP                ofxsRamp.cpp (+ TEXT)
#   TRACKING            ofxsTracking.cpp (+ TEXT)
#   OGL_FUNCTIONS       ofxsOGLFunctions_gl.cpp (the GL_GPU function table, + glad)
#   EGL                 glad/gladegl.cpp, the EGL loader (links ${CMAKE_DL_LIBS}); it is
#                       an empty file on Windows, Cygwin, macOS and Haiku
#   OPENMP              ofxsOpenMP.cpp, OFX::ofxsPinOpenMPRuntime() (links ${CMAKE_DL_LIBS});
#                       compiled with <target>'s OpenMP flags, a no-op without OpenMP
# Options:
#   ALL     every component above, plus OPENGL (not OSMESA)
#   OPENGL  the OpenGL loader: glad/glad.cpp ofxsOGLUtilities.cpp, plus EGL, which
#           ofxsOGLUtilities uses on Linux and the BSDs (Wayland)
#   EGL     same as COMPONENTS EGL
#   OSMESA  ofxsOGLFunctions_mesa.cpp (the GL_CPU function table), and HAVE_OSMESA
#           (a PRIVATE definition of the whole <target>, which the plug-in's own
#           sources see too), and links <target> with the imported target
#           OSMesa::OSMesa. The consumer must define OSMesa::OSMesa (include
#           directories, the OSMesa library, and its dependencies, e.g. a mangled
#           OSMesa and LLVM, see openfx-misc's Shadertoy); it is an error if it
#           does not exist.
#
# Linking:
# - the components that call OpenGL (TEXT, the overlays and interacts, OPENGL,
#   OGL_FUNCTIONS) link OpenGL::GL (find_package(OpenGL));
# - glad, EGL and OPENMP link ${CMAKE_DL_LIBS}, THREADS links Threads::Threads;
# - if the target openfx::Support exists when supportext_add_sources() is called
#   (openfx's CMake: add_subdirectory(openfx) or find_package(openfx) before the
#   call), <target> is linked with it, which also brings the openfx include
#   directories and OFX_* definitions; otherwise the consumer must compile the
#   openfx Support library and set its include directories itself.
# Links are added to the LINK_LIBRARIES target property (and, for STATIC and OBJECT
# libraries, as $<LINK_ONLY:...> to INTERFACE_LINK_LIBRARIES so that they reach the
# final link), so this works whichever target_link_libraries signature (plain or
# PRIVATE/PUBLIC) the consumer uses, and a library the consumer already linked
# (e.g. openfx::Support) is not added twice.
#
# Calling supportext_add_sources() several times for the same target is fine: each
# source is added once. It can also be called from another directory than the
# target's: the imported targets it links (OpenGL::GL, Threads::Threads, OSMesa::OSMesa,
# openfx::Support) must then be visible from the target's directory. Those the helper
# finds itself are made global. With CMake 3.21 or later, so are those the consumer
# created in the calling directory (e.g. with find_package), with the other imported
# targets of their namespace (their export set); an ALIAS of one is linked by its
# aliased name. An imported target created in a third directory that the target's
# directory cannot see is reported by CMake at generate time.
#
# If a source tree contains several copies of SupportExt (e.g. openfx-arena's
# SupportExt and OpenFX-IO/SupportExt), the first one included is used by every
# supportext_add_sources() call.

if(CMAKE_VERSION VERSION_LESS 3.16)
  message(FATAL_ERROR "SupportExt.cmake needs CMake 3.16 or later")
endif()

# the SupportExt directory (the first SupportExt included wins)
get_property(_supportext_dir_set GLOBAL PROPERTY SUPPORTEXT_DIR SET)
if(NOT _supportext_dir_set)
  get_filename_component(_supportext_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
  set_property(GLOBAL PROPERTY SUPPORTEXT_DIR "${_supportext_dir}")
  unset(_supportext_dir)
endif()
unset(_supportext_dir_set)

if(COMMAND supportext_add_sources)
  return()
endif()

# The functions below use if(IN_LIST) (policy CMP0057) and keep the policy settings in
# force where they are defined: use CMake 3.16+ policies here, whatever the consumer's
# cmake_minimum_required (tests/cmake/regression/oldpolicy checks a 3.1 consumer). Note that the
# helper's own find_package(OpenGL) then uses CMP0072 NEW (GLVND on Linux).
cmake_policy(PUSH)
cmake_policy(VERSION 3.16...3.31)

# Add <item> to the link libraries of <target>, once, without using
# target_link_libraries (whose plain and keyword signatures cannot be mixed).
function(_supportext_link TARGET ITEM)
  if(NOT ITEM)
    return()
  endif()
  # A non-global imported target is visible in the directory that created it and in
  # the subdirectories added after it. If the consumer created ITEM (e.g. with
  # find_package) in the calling directory and TARGET is defined in another directory,
  # TARGET's directory may not see it: make it global, which only the creating
  # directory may do (and is harmless if it was visible). This needs the
  # IMPORTED_TARGETS directory property (CMake 3.21); otherwise, and for an ITEM created
  # in a third directory that TARGET's directory cannot see, CMake reports the missing
  # target at generate time. An ALIAS of a non-global imported target is local too: link
  # the aliased target instead.
  if(TARGET ${ITEM} AND NOT CMAKE_VERSION VERSION_LESS 3.21)
    get_property(_real TARGET ${ITEM} PROPERTY ALIASED_TARGET)
    if(NOT _real)
      set(_real "${ITEM}")
    endif()
    get_property(_imported TARGET ${_real} PROPERTY IMPORTED)
    get_property(_global TARGET ${_real} PROPERTY IMPORTED_GLOBAL)
    get_property(_here DIRECTORY PROPERTY IMPORTED_TARGETS)
    # BINARY_DIR, not SOURCE_DIR: a source directory may be added twice
    get_property(_tdir TARGET ${TARGET} PROPERTY BINARY_DIR)
    if(_imported AND NOT _global)
      # the ALIAS is local to the directory that created it, the aliased target may not be
      set(ITEM "${_real}")
    endif()
    if(_imported AND NOT _global AND _real IN_LIST _here
       AND NOT _tdir STREQUAL CMAKE_CURRENT_BINARY_DIR)
      set_property(TARGET ${_real} PROPERTY IMPORTED_GLOBAL TRUE)
      # A package's exported targets (e.g. openfx::Api, openfx::HostSupport and
      # openfx::Support) form one export set: if only some were global, a later
      # find_package() of that package in another directory would fail with "Some (but
      # not all) targets in this export set were already defined". Promote the other
      # imported targets of the calling directory with the same namespace too (an export
      # set installed without a NAMESPACE is not detected).
      if(_real MATCHES "^([^:]+)::")
        set(_ns "${CMAKE_MATCH_1}::")
        foreach(_t ${_here})
          string(FIND "${_t}" "${_ns}" _pos)
          if(_pos EQUAL 0)
            get_property(_g TARGET ${_t} PROPERTY IMPORTED_GLOBAL)
            if(NOT _g)
              set_property(TARGET ${_t} PROPERTY IMPORTED_GLOBAL TRUE)
            endif()
          endif()
        endforeach()
      endif()
    endif()
  endif()
  get_property(_libs TARGET ${TARGET} PROPERTY LINK_LIBRARIES)
  if(NOT ITEM IN_LIST _libs)
    set_property(TARGET ${TARGET} APPEND PROPERTY LINK_LIBRARIES "${ITEM}")
  endif()
  get_property(_type TARGET ${TARGET} PROPERTY TYPE)
  if(_type STREQUAL "STATIC_LIBRARY" OR _type STREQUAL "OBJECT_LIBRARY")
    get_property(_ilibs TARGET ${TARGET} PROPERTY INTERFACE_LINK_LIBRARIES)
    if(NOT "$<LINK_ONLY:${ITEM}>" IN_LIST _ilibs AND NOT ITEM IN_LIST _ilibs)
      set_property(TARGET ${TARGET} APPEND PROPERTY INTERFACE_LINK_LIBRARIES "$<LINK_ONLY:${ITEM}>")
    endif()
  endif()
endfunction()

# find_package() creates imported targets visible only in the current directory and
# below: make them global so that a target defined in another directory can use them.
macro(_supportext_find_global PACKAGE IMPORTED)
  if(NOT TARGET ${IMPORTED})
    find_package(${PACKAGE} REQUIRED)
    get_property(_supportext_global TARGET ${IMPORTED} PROPERTY IMPORTED_GLOBAL)
    if(NOT _supportext_global)
      set_property(TARGET ${IMPORTED} PROPERTY IMPORTED_GLOBAL TRUE)
    endif()
    unset(_supportext_global)
  endif()
endmacro()

function(supportext_add_sources TARGET)
  if(NOT TARGET ${TARGET})
    message(FATAL_ERROR "supportext_add_sources: ${TARGET} is not a target")
  endif()
  get_property(_dir GLOBAL PROPERTY SUPPORTEXT_DIR)
  # The arguments are ALL, the options OPENGL, OSMESA and EGL, and component names,
  # in any order: COMPONENTS is optional, and the options are accepted inside the
  # COMPONENTS list too (cmake_parse_arguments would end the list at them).
  set(SX_ALL OFF)
  set(SX_OSMESA OFF)
  set(_todo "")
  foreach(_a ${ARGN})
    string(TOUPPER "${_a}" _a)
    if(_a STREQUAL "COMPONENTS")
      continue()
    elseif(_a STREQUAL "ALL")
      set(SX_ALL ON)
    else()
      if(_a STREQUAL "OSMESA")
        set(SX_OSMESA ON)
      endif()
      list(APPEND _todo ${_a})
    endif()
  endforeach()

  # component -> sources (relative to the SupportExt directory) and dependencies.
  # These are function variables, so the function works from any directory
  # (see above for the imported targets it links).
  set(_components THREADS FILEOPEN LUT MULTIPLANE SHUTTER TRANSFORM3X3 TEXT OVERLAY
      POSITION_INTERACT TRANSFORM_INTERACT RECTANGLE_INTERACT GENERATOR RAMP TRACKING
      OGL_FUNCTIONS EGL OPENMP)
  set(_internal GLAD OPENGL OSMESA)
  set(_THREADS_SOURCES            ofxsThreadSuite.cpp)
  set(_FILEOPEN_SOURCES           ofxsFileOpen.cpp)
  set(_LUT_SOURCES                ofxsLut.cpp)
  set(_MULTIPLANE_SOURCES         ofxsMultiPlane.cpp)
  set(_SHUTTER_SOURCES            ofxsShutter.cpp)
  set(_TRANSFORM3X3_SOURCES       ofxsTransform3x3.cpp)
  set(_TRANSFORM3X3_DEPENDS       SHUTTER)
  set(_TEXT_SOURCES               ofxsOGLTextRenderer.cpp ofxsOGLFontData.cpp)
  set(_OVERLAY_DEPENDS            TEXT)
  set(_POSITION_INTERACT_DEPENDS  OVERLAY)
  set(_TRANSFORM_INTERACT_SOURCES ofxsTransformInteract.cpp)
  set(_RECTANGLE_INTERACT_SOURCES ofxsRectangleInteract.cpp)
  set(_GENERATOR_SOURCES          ofxsGenerator.cpp)
  set(_GENERATOR_DEPENDS          RECTANGLE_INTERACT)
  set(_RAMP_SOURCES               ofxsRamp.cpp)
  set(_RAMP_DEPENDS               TEXT)
  set(_TRACKING_SOURCES           ofxsTracking.cpp)
  set(_TRACKING_DEPENDS           TEXT)
  set(_GLAD_SOURCES               glad/glad.cpp)
  set(_OGL_FUNCTIONS_SOURCES      ofxsOGLFunctions_gl.cpp)
  set(_OGL_FUNCTIONS_DEPENDS      GLAD)
  set(_OPENGL_SOURCES             ofxsOGLUtilities.cpp)
  set(_OPENGL_DEPENDS             GLAD EGL)
  set(_EGL_SOURCES                glad/gladegl.cpp)
  set(_OSMESA_SOURCES             ofxsOGLFunctions_mesa.cpp)
  set(_OPENMP_SOURCES             ofxsOpenMP.cpp)
  # components that call OpenGL, or use dlopen (glad, EGL)
  set(_gl_components TEXT OVERLAY POSITION_INTERACT TRANSFORM_INTERACT RECTANGLE_INTERACT
      GENERATOR RAMP TRACKING GLAD OGL_FUNCTIONS OPENGL)
  set(_dl_components GLAD EGL OPENMP)

  if(SX_ALL)
    list(APPEND _todo ${_components} OPENGL)
  endif()

  # resolve the dependencies
  set(_done "")
  while(NOT "${_todo}" STREQUAL "")
    list(GET _todo 0 _c)
    list(REMOVE_AT _todo 0)
    string(TOUPPER "${_c}" _c)
    if(NOT _c IN_LIST _components AND NOT _c IN_LIST _internal)
      message(FATAL_ERROR "supportext_add_sources(${TARGET}): unknown component or option ${_c} "
        "(components: ${_components}; options: ALL OPENGL EGL OSMESA)")
    endif()
    if(NOT _c IN_LIST _done)
      list(APPEND _done ${_c})
      list(APPEND _todo ${_${_c}_DEPENDS})
    endif()
  endwhile()

  # sources, once per target
  get_property(_added TARGET ${TARGET} PROPERTY SUPPORTEXT_COMPONENTS)
  set(_sources "")
  set(_need_gl OFF)
  set(_need_dl OFF)
  foreach(_c ${_done})
    if(NOT _c IN_LIST _added)
      foreach(_s ${_${_c}_SOURCES})
        list(APPEND _sources "${_dir}/${_s}")
      endforeach()
      set_property(TARGET ${TARGET} APPEND PROPERTY SUPPORTEXT_COMPONENTS ${_c})
    endif()
    if(_c IN_LIST _gl_components)
      set(_need_gl ON)
    endif()
    if(_c IN_LIST _dl_components)
      set(_need_dl ON)
    endif()
  endforeach()
  if(_sources)
    target_sources(${TARGET} PRIVATE ${_sources})
  endif()

  # include directories, once per target
  get_property(_setup TARGET ${TARGET} PROPERTY SUPPORTEXT_SETUP)
  if(NOT _setup)
    set_property(TARGET ${TARGET} PROPERTY SUPPORTEXT_SETUP ON)
    target_include_directories(${TARGET} PRIVATE "${_dir}" "${_dir}/glad")
  endif()

  # libraries (each is added once, see _supportext_link)
  if(TARGET openfx::Support)
    _supportext_link(${TARGET} openfx::Support)
  endif()
  if(SX_OSMESA)
    if(NOT TARGET OSMesa::OSMesa)
      message(FATAL_ERROR "supportext_add_sources(${TARGET} OSMESA): define the imported target "
        "OSMesa::OSMesa first (OSMesa include directories and libraries)")
    endif()
    target_compile_definitions(${TARGET} PRIVATE HAVE_OSMESA)
    _supportext_link(${TARGET} OSMesa::OSMesa)
  endif()
  if(_need_gl)
    _supportext_find_global(OpenGL OpenGL::GL)
    _supportext_link(${TARGET} OpenGL::GL)
  endif()
  if(_need_dl)
    foreach(_l ${CMAKE_DL_LIBS})
      _supportext_link(${TARGET} ${_l})
    endforeach()
  endif()
  if("THREADS" IN_LIST _done)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    _supportext_find_global(Threads Threads::Threads)
    _supportext_link(${TARGET} Threads::Threads)
  endif()
endfunction()

cmake_policy(POP)
