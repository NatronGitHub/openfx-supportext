Contributing to openfx-supportext
=================================

The project rules (pull requests, reviews, issues, tests, CI) are in the
[Natron Maintainer Guide](https://natron.readthedocs.io/en/rb-2.6/maintainers/guidelines.html);
this file holds what is specific to SupportExt. The exceptions for this
repository are in the guide's
[Differences between the repositories](https://natron.readthedocs.io/en/rb-2.6/maintainers/guidelines.html#differences-between-the-repositories).

What this repository is
-----------------------

Header-mostly helpers on the OpenFX C++ Support library, mostly in
`namespace OFX`. openfx-misc, openfx-io and openfx-arena use it as their
`SupportExt` submodule and compile the sources they need into each plug-in,
through `cmake/SupportExt.cmake` (see [README.md](README.md)). Several plug-ins
depend on the exact rendered output, so a change here can alter existing
projects in all three repositories.

- **Transform3x3** (`ofxsTransform3x3*`): the base class of the 2D-matrix
  plug-ins (Transform, CornerPin, Reformat, Card3D, DirBlur…). Subclasses
  implement `getInverseTransformCanonical()`; the base handles the RoD and RoI,
  isIdentity, motion blur (`ofxsShutter`), directional blur and masking.
  Filters are in `ofxsFilter.h`, matrices in `ofxsMatrix2D.h`.
- **Overlays**: `ofxsOverlayDraw.h` (one drawing routine for OpenGL V1 and
  DrawSuite V2 overlays; read its header comment), `ofxsTransformInteract`,
  `ofxsRectangleInteract`, `ofxsPositionInteract.h`, `ofxsTracking`,
  `ofxsRamp`. Pen and overlay coordinates are canonical.
- **Pixel processing**: `ofxsPixelProcessor.h`, `ofxsMaskMix.h`,
  `ofxsMerging.h`, `ofxsCopier.h`, `ofxsImageBlenderMasked.h`,
  `ofxsLut.{h,cpp}` (colour LUTs, luminance), `ofxsNonFinite.h`,
  `ofxsRGBAProcessing.h`, `ofxsCurves.h`.
- **Plug-in scaffolding**: `ofxsGenerator`, `ofxsMultiPlane`, `ofxsCoords.h`,
  `ofxsNormalisedDefaults.h`, `ofxsAnalysisScope.h`, `ofxsMacros.h`.
- **OpenGL**: `glad/` is generated; `ofxsOGLFunctions*` (see
  [ofxsOGLFunctions-README.md](ofxsOGLFunctions-README.md)),
  `ofxsOGLTextRenderer`.
- **Threads**: `ofxsThreadSuite` (a fallback multithread suite with C++11
  threads); `tinythread` is a C++11 alias.

Requirements
------------

- C++17 minimum; the consumers build SupportExt as C++17 to C++23.
- The [NatronGitHub fork of openfx](https://github.com/NatronGitHub/openfx)
  (the Natron extensions), commit a61684fe or later for `ofxsOverlayDraw.h`.
- Hosts that implement only OFX 1.4 must keep working: OFX 1.5 and Natron
  features are optional, with a fallback (overlays keep an OpenGL V1 path).

Checking a change
-----------------

The top-level `CMakeLists.txt` compile-checks every source; `tests/cmake`
checks the CMake helper and that the modules link. With an openfx checkout
`$O` (the NatronGitHub fork):

```sh
cmake -S . -B /tmp/sx-check -DOPENFX_DIR=$O -DCMAKE_CXX_STANDARD=17 \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wno-unused-parameter" -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
cmake --build /tmp/sx-check -j4
cmake -S tests/cmake -B /tmp/sx-tests -DOPENFX_DIR=$O && cmake --build /tmp/sx-tests -j4
```

- Headers must pass `-std=c++17 -fsyntax-only` with `-DOFX_EXTENSIONS_VEGAS
  -DOFX_EXTENSIONS_NUKE -DOFX_EXTENSIONS_NATRON -DOFX_EXTENSIONS_TUTTLE
  -DOFX_SUPPORTS_OPENGLRENDER`.
- Search the consumers (openfx-misc, openfx-io and openfx-arena, at a fresh
  `origin/master`) for every symbol you change.
- CI (`ci.yml`: Linux and MSVC, then macOS) also builds openfx-misc with the
  branch and renders its plug-ins; `extended.yml` runs weekly.
- Check the [OpenFX specification](https://openfx.readthedocs.io) (its
  `include/` headers and `Documentation/sources/Reference/`) before writing or
  reviewing code that uses the OFX API: properties, coordinates, render scale,
  pixel aspect ratio, RoD and RoI, isIdentity, frames needed, interacts, clip
  preferences, multi-plane. Quote it when a decision depends on it.

Changing rendered output
------------------------

A fix that changes the plausible output of existing projects keeps the old
behaviour behind a flag, which the plug-in enables only for its new versions.
Crash, NaN and
garbage fixes need no flag, and the maintainer decides borderline cases.
Before "fixing" output that looks odd, ask whether it is intended: for
example, DirBlur's reversed fading with a negative Amount is intended (#20).

SupportExt has no changelog file: the lines are in the `### CHANGELOG` section
of each pull request's description.
