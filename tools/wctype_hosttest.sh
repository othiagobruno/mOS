#!/bin/bash
# Teste de host de userland/libsystem/wctype_wide.c. Uso: tools/wctype_hosttest.sh [dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/wctype-hosttest}
ZIG=${ZIG:-$(command -v zig)}
ZIGDIR=$(dirname "$(readlink -f "$ZIG")"); [ -d "$ZIGDIR/lib/include" ] || ZIGDIR=$(dirname "$ZIGDIR")
NM=$(command -v llvm-nm || command -v llvm-nm-21)
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -w -I"$ROOT/userland/libsystem/kshim" -isystem "$ZIGDIR/lib/include" \
    -include "$ROOT/userland/libsystem/kshim/math.h" -DMOS_LIBSYSTEM \
    -c "$ROOT/userland/libsystem/wctype_wide.c" -o "$OUT/wctype.o"
"$NM" --defined-only -g "$OUT/wctype.o" | awk '{print $NF" mos_"$NF}' > "$OUT/redef.txt"
objcopy --redefine-syms="$OUT/redef.txt" "$OUT/wctype.o" "$OUT/wctype_mos.o"
cc -O1 -o "$OUT/wctype_hosttest" "$ROOT/tools/wctype_hosttest.c" "$OUT/wctype_mos.o"
"$OUT/wctype_hosttest"
