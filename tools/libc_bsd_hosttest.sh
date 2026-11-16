#!/bin/bash
# Teste de host de userland/libsystem/libc_bsd.c contra a libc do host. Uso: tools/libc_bsd_hosttest.sh [dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/libc-bsd-hosttest}
ZIG=${ZIG:-$(command -v zig)}
ZIGDIR=$(dirname "$(readlink -f "$ZIG")"); [ -d "$ZIGDIR/lib/include" ] || ZIGDIR=$(dirname "$ZIGDIR")
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
    -I"$ROOT/userland/libsystem/kshim" -I"$ROOT/userland/libsystem/fsinc" -I"$ROOT/kernel/include" \
    -isystem "$ZIGDIR/lib/include" -include "$ROOT/userland/libsystem/kshim/math.h" -DMOS_LIBSYSTEM \
    -c "$ROOT/userland/libsystem/libc_bsd.c" -o "$OUT/libc_bsd.o"
# prefixa mos_ só nos símbolos DEFINIDOS pelo arquivo (os indefinidos vêm da libc do host)
llvm-nm-21 --defined-only -g "$OUT/libc_bsd.o" | awk '{print $NF" mos_"$NF}' > "$OUT/redef.txt"
objcopy --redefine-syms="$OUT/redef.txt" "$OUT/libc_bsd.o" "$OUT/libc_bsd_mos.o"
cc -O1 -fno-builtin -o "$OUT/libc_bsd_hosttest" "$ROOT/tools/libc_bsd_hosttest.c" "$OUT/libc_bsd_mos.o"
"$OUT/libc_bsd_hosttest"
