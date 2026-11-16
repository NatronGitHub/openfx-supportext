#!/bin/bash
# Load and render SupportExt-based plug-ins with openfx's hostDemo.
#
# Builds:
# - hostDemo, from an openfx checkout (CMake, bundled expat);
# - a test bundle, SupportExtTest.ofx.bundle, made of a few openfx-misc
#   plug-ins that use SupportExt, compiled against THIS SupportExt and the
#   openfx Support library, and linked like the consumers' bundles
#   (hidden visibility, only OfxGetPlugin/OfxGetNumberOfPlugins exported);
# then renders each plug-in with hostDemo and fails on any load, describe,
# instance creation or render error, or if an output frame is missing.
#
# Usage: tests/hostdemo/run.sh <openfx dir> <openfx-misc dir> [work dir]
# Environment: CXX (default c++), CXXSTD (default -std=c++17), JOBS.
# Runs on macOS, Linux and MSYS2 (MinGW).
set -eo pipefail

OFX=$(cd "$1" && pwd)
MISC=$(cd "$2" && pwd)
SX=$(cd "$(dirname "$0")/../.." && pwd)
WORK=${3:-${RUNNER_TEMP:-/tmp}/supportext-hostdemo}
# MSYS2: RUNNER_TEMP is a Windows path (D:\a\_temp)
command -v cygpath > /dev/null && WORK=$(cygpath -u "$WORK")
CXX=${CXX:-c++}
CXXSTD=${CXXSTD:--std=c++17}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)

case "$(uname -s)" in
    Darwin)
        OS=macos; ARCH=MacOS
        PIC=""
        LINK=(-bundle -fvisibility=hidden -exported_symbols_list "$OFX/Support/include/osxSymbols" -framework OpenGL)
        HOSTDEMO=hostDemo
        ;;
    Linux)
        OS=linux
        case "$(uname -m)" in x86_64) ARCH=Linux-x86-64 ;; i?86) ARCH=Linux-x86 ;; *) ARCH="Linux-$(uname -m)" ;; esac
        PIC=-fPIC
        LINK=(-shared -fvisibility=hidden -Xlinker --version-script="$OFX/Support/include/linuxSymbols" -Wl,--no-undefined -lGL)
        HOSTDEMO=hostDemo
        ;;
    MINGW*|MSYS*)
        OS=mingw; ARCH=Win64
        PIC=""
        LINK=(-shared -fvisibility=hidden -Xlinker --version-script="$OFX/Support/include/linuxSymbols" -static-libgcc -static-libstdc++ -lopengl32)
        HOSTDEMO=hostDemo.exe
        ;;
    *) echo "unsupported OS: $(uname -s)"; exit 1 ;;
esac

# --- hostDemo ---------------------------------------------------------------
GEN=()
command -v ninja > /dev/null && GEN=(-G Ninja)
cmake -S "$OFX" -B "$WORK/host" "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_EXAMPLE_HOSTS=ON -DBUILD_EXAMPLE_PLUGINS=OFF \
      -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON -DCMAKE_CXX_EXTENSIONS=OFF > "$WORK/host-configure.log" 2>&1 \
    || { cat "$WORK/host-configure.log"; exit 1; }
cmake --build "$WORK/host" --target hostDemo --parallel "$JOBS" > "$WORK/host-build.log" 2>&1 \
    || { cat "$WORK/host-build.log"; exit 1; }
HD=$(find "$WORK/host" -name "$HOSTDEMO" -type f | head -1)
test -x "$HD" || { echo "hostDemo not built"; exit 1; }
echo "hostDemo: $HD"

# --- test bundle ------------------------------------------------------------
# openfx-misc plug-ins exercising SupportExt: Transform3x3/TransformInteract,
# RectangleInteract (Crop), the generator and RectangleInteract with an angle
# (Constant, Rectangle), PositionInteract (Position).
PLUGINS="Transform/Transform.cpp Crop/Crop.cpp Constant/Constant.cpp Rectangle/Rectangle.cpp Position/Position.cpp"
SUPPORT="ofxsCore ofxsImageEffect ofxsInteract ofxsLog ofxsMultiThread ofxsParams ofxsProperty ofxsPropertyValidation"
SUPPORTEXT="ofxsThreadSuite ofxsTransform3x3 ofxsTransformInteract ofxsShutter ofxsRectangleInteract ofxsGenerator ofxsOGLTextRenderer ofxsOGLFontData tinythread"
DEFS="-DOFX_EXTENSIONS_VEGAS -DOFX_EXTENSIONS_NUKE -DOFX_EXTENSIONS_NATRON -DOFX_EXTENSIONS_TUTTLE -DOFX_SUPPORTS_OPENGLRENDER"
CXXFLAGS="$CXXSTD -O1 $PIC -fvisibility=hidden $DEFS -I$SX -I$SX/glad -I$MISC/Misc -I$OFX/include -I$OFX/Support/include -I$OFX/Support/Plugins/include -I$OFX/Support/Library"
[ "$OS" = macos ] && CXXFLAGS="$CXXFLAGS -DGL_SILENCE_DEPRECATION"

OBJ="$WORK/obj"
rm -rf "$OBJ"; mkdir -p "$OBJ"
srcs=()
for s in $SUPPORT; do srcs+=("$OFX/Support/Library/$s.cpp"); done
for s in $SUPPORTEXT; do srcs+=("$SX/$s.cpp"); done
for s in $PLUGINS; do srcs+=("$MISC/$s"); done
export CXX CXXFLAGS OBJ
# shellcheck disable=SC2016
printf '%s\n' "${srcs[@]}" | xargs -P "$JOBS" -I{} bash -c 'o="$OBJ/$(basename "{}" .cpp).o"; $CXX $CXXFLAGS -c "{}" -o "$o" > "$o.log" 2>&1 || { cat "$o.log"; exit 255; }'

BUNDLE="$WORK/plugins/SupportExtTest.ofx.bundle"
rm -rf "$WORK/plugins"; mkdir -p "$BUNDLE/Contents/$ARCH"
$CXX "$OBJ"/*.o "${LINK[@]}" -o "$BUNDLE/Contents/$ARCH/SupportExtTest.ofx"
echo "bundle: $BUNDLE/Contents/$ARCH/SupportExtTest.ofx"

# --- render with hostDemo ---------------------------------------------------
PLUGPATH="$WORK/plugins"
[ "$OS" = mingw ] && PLUGPATH=$(cygpath -w "$PLUGPATH")
OUT="$WORK/out"
rm -rf "$OUT"; mkdir -p "$OUT"
# hostDemo writes <prefix>.<t>.ppm, or <prefix>.<t>l.ppm and <prefix>.<t>r.ppm
# for multi-view plug-ins; it renders t = 0 and 1
frames_ok() {
    local t
    for t in 0 1; do
        [ -s "$1.$t.ppm" ] || { [ -s "$1.${t}l.ppm" ] && [ -s "$1.${t}r.ppm" ]; } || return 1
    done
}
fail=0
# plug-in id, context, parameters
while read -r id ctx params; do
    [ -z "$id" ] && continue
    name=${id##*.}
    oprefix="$OUT/$name"
    [ "$OS" = mingw ] && oprefix=$(cygpath -w "$oprefix")
    # shellcheck disable=SC2086
    if (cd "$OUT" && OFX_PLUGIN_PATH="$PLUGPATH" "$HD" -p "$id" -c "$ctx" -o "$oprefix" $params) > "$OUT/$name.log" 2>&1 \
       && frames_ok "$OUT/$name"; then
        echo "ok   $id ($ctx)"
    else
        echo "FAIL $id ($ctx)"; cat "$OUT/$name.log"; fail=1
    fi
done <<EOF
net.sf.openfx.TransformPlugin filter rotate=30 scale=0.5,0.5
net.sf.openfx.CropPlugin filter
net.sf.openfx.ConstantPlugin general
net.sf.openfx.Rectangle general
net.sf.openfx.Position filter translate=10,5
EOF
exit $fail
