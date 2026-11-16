#!/bin/bash
# Teste de host de userland/libsystem/rand48.c contra a glibc. Uso: tools/rand48_hosttest.sh [dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/rand48-hosttest}
ZIG=${ZIG:-$(command -v zig)}
ZIGDIR=$(dirname "$(readlink -f "$ZIG")"); [ -d "$ZIGDIR/lib/include" ] || ZIGDIR=$(dirname "$ZIGDIR")
NM=$(command -v llvm-nm || command -v llvm-nm-21)
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -w -I"$ROOT/userland/libsystem/kshim" -isystem "$ZIGDIR/lib/include" \
    -include "$ROOT/userland/libsystem/kshim/math.h" -DMOS_LIBSYSTEM \
    -c "$ROOT/userland/libsystem/rand48.c" -o "$OUT/rand48.o"
"$NM" --defined-only -g "$OUT/rand48.o" | awk '{print $NF" mos_"$NF}' > "$OUT/redef.txt"
objcopy --redefine-syms="$OUT/redef.txt" "$OUT/rand48.o" "$OUT/rand48_mos.o"
cc -O1 -o "$OUT/rand48_hosttest" "$ROOT/tools/rand48_hosttest.c" "$OUT/rand48_mos.o"
"$OUT/rand48_hosttest"
