#!/bin/bash
# Teste de host de userland/libsystem/libc_corpus2.c. Uso: tools/libc_corpus2_hosttest.sh [dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/libc-corpus2-hosttest}
ZIG=${ZIG:-$(command -v zig)}
ZIGDIR=$(dirname "$(readlink -f "$ZIG")"); [ -d "$ZIGDIR/lib/include" ] || ZIGDIR=$(dirname "$ZIGDIR")
NM=$(command -v llvm-nm || command -v llvm-nm-21)
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -w -I"$ROOT/userland/libsystem/kshim" -isystem "$ZIGDIR/lib/include" \
    -include "$ROOT/userland/libsystem/kshim/math.h" -DMOS_LIBSYSTEM \
    -c "$ROOT/userland/libsystem/libc_corpus2.c" -o "$OUT/libc_corpus2.o"
# prefixa mos_ só nos símbolos definidos (exp/log/cos/... e getpid vêm da libc/libm do host)
"$NM" --defined-only -g "$OUT/libc_corpus2.o" | awk '{print $NF" mos_"$NF}' > "$OUT/redef.txt"
objcopy --redefine-syms="$OUT/redef.txt" "$OUT/libc_corpus2.o" "$OUT/libc_corpus2_mos.o"
cc -O1 -o "$OUT/libc_corpus2_hosttest" "$ROOT/tools/libc_corpus2_hosttest.c" "$OUT/libc_corpus2_mos.o" -lm
"$OUT/libc_corpus2_hosttest"
