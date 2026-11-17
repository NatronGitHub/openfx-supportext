openfx-supportext
=================

Extensions for the OpenFX Support library

Building with CMake
-------------------

SupportExt is compiled into each plug-in target, with the plug-in's own
compile definitions: `OFX_EXTENSIONS_*`, `OFX_SUPPORTS_OPENGLRENDER` and the
DrawSuite support change class layouts, so SupportExt must not be built
separately with other values. Linking `openfx::Support` (openfx's CMake) sets
them, as well as the openfx include directories:

```cmake
add_subdirectory(openfx)                     # or find_package(openfx), before the call
include(SupportExt/cmake/SupportExt.cmake)   # or add_subdirectory(SupportExt)
add_library(MyPlugin MODULE ${MY_SOURCES})
supportext_add_sources(MyPlugin COMPONENTS THREADS TRANSFORM3X3 TRANSFORM_INTERACT)
```

`supportext_add_sources(<target> [ALL] [OPENGL] [OSMESA] [EGL] [COMPONENTS <name>...])`
adds the selected sources (and their dependencies) and SupportExt's include
directories to `<target>`. The arguments may come in any order, and `OPENGL`,
`EGL` and `OSMESA` may be listed among the components. It links:

- `OpenGL::GL` for the components that draw (`TEXT`, the overlays and
  interacts, `OPENGL`, `OGL_FUNCTIONS`), `${CMAKE_DL_LIBS}` for the GL and EGL
  loaders, `Threads::Threads` for `THREADS`;
- `openfx::Support`, if that target exists when the function is called (so
  add or find openfx first), otherwise the consumer compiles the openfx Support
  library itself.

The links also reach the final link when `<target>` is a STATIC or OBJECT
library. The function can be called several times, and from another directory than the
target's. The imported targets it links (`OpenGL::GL`, `Threads::Threads`,
`OSMesa::OSMesa`, `openfx::Support`) must then be visible from the target's directory.
Those the helper finds itself are made global, and with CMake 3.21 or later, so are those
created in the calling directory (e.g. by `find_package`).

Plug-ins whose own code draws text with the header-only overlay code
(`OverlayDraw::bitmapString`, `ofxsPositionInteract.h`) need the `OVERLAY` (or
`POSITION_INTERACT`) component, which brings `TEXT`. `OPENGL` brings the EGL loader, which is used on
Linux and the BSDs. `OSMESA` adds the OSMesa function table, defines
`HAVE_OSMESA` for the whole target (the plug-in's own sources too) and links
an imported target `OSMesa::OSMesa`, which the consumer must define. Every
component is documented in `cmake/SupportExt.cmake`. If a source tree contains
several copies of SupportExt, the first one included is used everywhere.

For example, the Natron plug-in repositories use:

- openfx-misc: `THREADS FILEOPEN GENERATOR LUT MULTIPLANE TEXT RAMP SHUTTER
  TRACKING TRANSFORM3X3 TRANSFORM_INTERACT RECTANGLE_INTERACT` (Misc), `THREADS
  LUT` (CImg), `THREADS` (Shadertoy, whose OSMesa setup stays in openfx-misc),
  `THREADS TRANSFORM3X3` (Test);
- openfx-io: `THREADS FILEOPEN MULTIPLANE OPENGL` for every plug-in, plus
  `TEXT` (OIIOText), `LUT` (PNG), `RAMP RECTANGLE_INTERACT TRANSFORM3X3
  TRANSFORM_INTERACT` (SeExpr);
- openfx-arena: `TEXT TRANSFORM3X3 TRANSFORM_INTERACT MULTIPLANE`.

Configured as a top-level project (`cmake -S . -B build -DOPENFX_DIR=<openfx>`),
SupportExt builds a compile check of every source (with OpenGL and the OSMesa
function table, without linking OSMesa). `tests/cmake` is a consumer project
that checks the uses above: calls from another directory, sibling includes, a
STATIC library linked into a module, and modules linked without undefined
symbols.

License
-------

<!-- BEGIN LICENSE BLOCK -->
Copyright (C) 2013-2018 INRIA

openfx-supportext is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

openfx-supportext is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with openfx-supportext.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
<!-- END LICENSE BLOCK -->
