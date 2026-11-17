#!/bin/bash
# Teste de host de userland/libsystem/libc_corpus.c. Uso: tools/libc_corpus_hosttest.sh [dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/libc-corpus-hosttest}
ZIG=${ZIG:-$(command -v zig)}
ZIGDIR=$(dirname "$(readlink -f "$ZIG")"); [ -d "$ZIGDIR/lib/include" ] || ZIGDIR=$(dirname "$ZIGDIR")
NM=$(command -v llvm-nm || command -v llvm-nm-21)
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -w -I"$ROOT/userland/libsystem/kshim" -isystem "$ZIGDIR/lib/include" \
    -include "$ROOT/userland/libsystem/kshim/math.h" -DMOS_LIBSYSTEM \
    -c "$ROOT/userland/libsystem/libc_corpus.c" -o "$OUT/libc_corpus.o"
# prefixa mos_ só nos símbolos definidos (os externos — wcslen/towlower — vêm da libc do host)
"$NM" --defined-only -g "$OUT/libc_corpus.o" | awk '{print $NF" mos_"$NF}' > "$OUT/redef.txt"
objcopy --redefine-syms="$OUT/redef.txt" "$OUT/libc_corpus.o" "$OUT/libc_corpus_mos.o"
cc -O1 -o "$OUT/libc_corpus_hosttest" "$ROOT/tools/libc_corpus_hosttest.c" "$OUT/libc_corpus_mos.o"
"$OUT/libc_corpus_hosttest"
