#!/bin/bash
# Compile every SupportExt source file against an openfx checkout.
# Usage: compile.sh <openfx dir> [extra compiler flags...]
# Environment: CXX (default c++), CXXSTD (default -std=c++17).
# Fails on any compiler error or warning.
set -u
OFX=$1
shift
CXX=${CXX:-c++}
CXXSTD=${CXXSTD:--std=c++17}
DEFS="-DOFX_EXTENSIONS_VEGAS -DOFX_EXTENSIONS_NUKE -DOFX_EXTENSIONS_NATRON -DOFX_EXTENSIONS_TUTTLE -DOFX_SUPPORTS_OPENGLRENDER"
INCS="-I. -Iglad -I$OFX/include -I$OFX/Support/include -I$OFX/Support/Plugins/include"
WARN="-Wall -Wextra -Wno-unused-parameter"
OUT=$(mktemp -d)
status=0
for f in *.cpp glad/*.cpp; do
    log=$OUT/$(basename "$f").log
    # shellcheck disable=SC2086
    if ! $CXX $CXXSTD $WARN $DEFS $INCS "$@" -c "$f" -o "$OUT/$(basename "$f" .cpp).o" > "$log" 2>&1; then
        echo "::error file=$f::compilation failed"
        cat "$log"
        status=1
    elif grep -q "warning:" "$log"; then
        echo "::error file=$f::compiler warnings"
        cat "$log"
        status=1
    else
        echo "ok $f"
    fi
done
rm -rf "$OUT"
exit $status
