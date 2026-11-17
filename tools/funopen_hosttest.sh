#!/bin/bash
# Teste de host do funopen de userland/libsystem/stdio.c. Uso: tools/funopen_hosttest.sh [dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/funopen-hosttest}
ZIG=${ZIG:-$(command -v zig)}
ZIGDIR=$(dirname "$(readlink -f "$ZIG")"); [ -d "$ZIGDIR/lib/include" ] || ZIGDIR=$(dirname "$ZIGDIR")
NM=$(command -v llvm-nm || command -v llvm-nm-21)
mkdir -p "$OUT"
# -ffunction-sections + --gc-sections: só o caminho do funopen/fwrite/fread é linkado (o resto do stdio é descartado)
"$ZIG" cc -target x86_64-freestanding-none -O1 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -w -ffunction-sections -fdata-sections -I"$ROOT/userland/libsystem/kshim" \
    -isystem "$ZIGDIR/lib/include" -include "$ROOT/userland/libsystem/kshim/math.h" -DMOS_LIBSYSTEM \
    -c "$ROOT/userland/libsystem/stdio.c" -o "$OUT/stdio.o"
"$NM" --defined-only -g "$OUT/stdio.o" | awk '{print $NF" mos_"$NF}' > "$OUT/redef.txt"
objcopy --redefine-syms="$OUT/redef.txt" "$OUT/stdio.o" "$OUT/stdio_mos.o"
cc -O1 -ffunction-sections -Wl,--gc-sections -o "$OUT/funopen_hosttest" "$ROOT/tools/funopen_hosttest.c" "$OUT/stdio_mos.o"
"$OUT/funopen_hosttest"
