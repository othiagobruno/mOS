#!/bin/bash
# Teste de host da libm x86_64 do libSystem (musl + mos/apple.c + fenv.c) contra a glibc.
# Uso: tools/libm_hosttest.sh [dir] [dir-dos-objetos-já-compilados]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/libm-hosttest}
ZIG=${ZIG:-$(command -v zig)}
mkdir -p "$OUT"
OBJD=${2:-$OUT/obj}
[ -n "${2:-}" ] || bash "$ROOT/tools/build-libm-musl.sh" "$ZIG" x86_64 "$OBJD" 2
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-builtin -fpie -mno-red-zone -Wall \
    -c "$ROOT/userland/libsystem/fenv.c" -o "$OUT/fenv.o"
mapfile -t O < "$OBJD/objs.txt"
ld -r -o "$OUT/libm.o" "${O[@]}" "$OUT/fenv.o"
if [ -n "$(nm -u "$OUT/libm.o")" ]; then echo "libm: símbolos indefinidos:"; nm -u "$OUT/libm.o"; exit 1; fi
objcopy --prefix-symbols=mos_ "$OUT/libm.o" "$OUT/libm_mos.o"
cc -O1 -fno-builtin -o "$OUT/libm_hosttest" "$ROOT/tools/libm_hosttest.c" "$OUT/libm_mos.o" -lm
"$OUT/libm_hosttest"
