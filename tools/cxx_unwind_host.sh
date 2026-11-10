#!/bin/sh
# Casos de exceção no host, em -O0 -O1 -O2. x86_64. Não usa QEMU.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CC=${CC:-clang}
SRC="$ROOT/userland/libsystem/cxx_unwind.c"
TMP=${TMPDIR:-/tmp}/cxx-unwind-host
mkdir -p "$TMP"
"$CC" -c "$ROOT/tools/cxx_unwind_ti.c" -o "$TMP/ti.o"
for O in 0 1 2; do
    "$CC" -O"$O" -fno-exceptions -c "$SRC" -o "$TMP/uw.o"
    "$CC"++ -O"$O" -fexceptions -fcxx-exceptions -fomit-frame-pointer \
        "$TMP/uw.o" "$TMP/ti.o" "$ROOT/tools/cxx_unwind_host.cpp" -nostdlib++ -o "$TMP/host" -lc
    "$TMP/host"
    "$CC" -O"$O" "$TMP/uw.o" "$ROOT/tools/cxx_unwind_bt.c" -o "$TMP/bt"
    "$TMP/bt"
    "$CC" -O"$O" -fomit-frame-pointer "$TMP/uw.o" "$ROOT/tools/cxx_unwind_compact_host.c" "$ROOT/tools/cxx_unwind_frames.S" -o "$TMP/compact"
    "$TMP/compact"
done
echo "cxx_unwind_host: -O0 -O1 -O2 ok"
