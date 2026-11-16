#!/bin/bash
# Teste de host de userland/libsystem/multibyte.c contra a libc do host. Uso: tools/multibyte_hosttest.sh [dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/multibyte-hosttest}
ZIG=${ZIG:-$(command -v zig)}
ZIGDIR=$(dirname "$(readlink -f "$ZIG")"); [ -d "$ZIGDIR/lib/include" ] || ZIGDIR=$(dirname "$ZIGDIR")
NM=$(command -v llvm-nm || command -v llvm-nm-21)
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -Wall -I"$ROOT/userland/libsystem/kshim" -isystem "$ZIGDIR/lib/include" \
    -include "$ROOT/userland/libsystem/kshim/math.h" -DMOS_LIBSYSTEM \
    -c "$ROOT/userland/libsystem/multibyte.c" -o "$OUT/multibyte.o"
"$NM" --defined-only -g "$OUT/multibyte.o" | awk '{print $NF" mos_"$NF}' > "$OUT/redef.txt"
objcopy --redefine-syms="$OUT/redef.txt" "$OUT/multibyte.o" "$OUT/multibyte_mos.o"
cc -O1 -o "$OUT/multibyte_hosttest" "$ROOT/tools/multibyte_hosttest.c" "$OUT/multibyte_mos.o"
"$OUT/multibyte_hosttest"
