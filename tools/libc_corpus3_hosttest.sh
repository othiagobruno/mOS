#!/bin/bash
# Teste de host de userland/libsystem/libc_corpus3.c. Uso: tools/libc_corpus3_hosttest.sh [dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/libc-corpus3-hosttest}
ZIG=${ZIG:-$(command -v zig)}
ZIGDIR=$(dirname "$(readlink -f "$ZIG")"); [ -d "$ZIGDIR/lib/include" ] || ZIGDIR=$(dirname "$ZIGDIR")
NM=$(command -v llvm-nm || command -v llvm-nm-21)
mkdir -p "$OUT"
# -ffunction-sections + --gc-sections: wprintf/vwprintf (que usam __stdoutp) são descartados; só swprintf/readpassphrase entram
"$ZIG" cc -target x86_64-freestanding-none -O1 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -w -ffunction-sections -fdata-sections -I"$ROOT/userland/libsystem/kshim" \
    -isystem "$ZIGDIR/lib/include" -include "$ROOT/userland/libsystem/kshim/math.h" -DMOS_LIBSYSTEM \
    -c "$ROOT/userland/libsystem/libc_corpus3.c" -o "$OUT/libc_corpus3.o"
"$NM" --defined-only -g "$OUT/libc_corpus3.o" | awk '{print $NF" mos_"$NF}' > "$OUT/redef.txt"
objcopy --redefine-syms="$OUT/redef.txt" "$OUT/libc_corpus3.o" "$OUT/libc_corpus3_mos.o"
cc -O1 -ffunction-sections -Wl,--gc-sections -o "$OUT/libc_corpus3_hosttest" \
    "$ROOT/tools/libc_corpus3_hosttest.c" "$OUT/libc_corpus3_mos.o"
printf 's3cr3t\n' | "$OUT/libc_corpus3_hosttest"
