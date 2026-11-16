openfx-supportext
=================

Extensions for the OpenFX Support library

Building with CMake
-------------------

SupportExt is compiled into each plug-in target, with the plug-in's own
compile definitions (`OFX_EXTENSIONS_*`, `OFX_SUPPORTS_OPENGLRENDER`,
`HAVE_OSMESA` and the DrawSuite support change class layouts, so SupportExt
must not be built separately with other values):

```cmake
include(SupportExt/cmake/SupportExt.cmake)   # or add_subdirectory(SupportExt)
add_library(MyPlugin MODULE ${MY_SOURCES})
target_compile_definitions(MyPlugin PRIVATE OFX_EXTENSIONS_NATRON OFX_EXTENSIONS_NUKE
                           OFX_EXTENSIONS_VEGAS OFX_EXTENSIONS_TUTTLE OFX_SUPPORTS_OPENGLRENDER)
supportext_add_sources(MyPlugin COMPONENTS THREADS TRANSFORM3X3 TRANSFORM_INTERACT)
```

`supportext_add_sources(<target> [ALL] [OPENGL] [OSMESA] [EGL] [COMPONENTS <name>...])`
adds the selected sources and SupportExt's include directories, links
`OpenGL::GL` for the components that draw, links `openfx::Support` if that
target exists, and can be called several times. The components and options
are documented in `cmake/SupportExt.cmake`. `OSMESA` needs an imported target
`OSMesa::OSMesa` defined by the consumer.

Configured as a top-level project (`cmake -S . -B build -DOPENFX_DIR=<openfx>`),
SupportExt builds a compile check of every source.

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
