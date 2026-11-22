#!/bin/bash
# Teste de host de userland/libsystem/accelerate_corpus2.c (com accelerate_lapack.c e accelerate_vimage.c). Uso: tools/accelerate_corpus2_test.sh [dir]
# Linux: objeto ELF x86_64 freestanding, símbolos com prefixo mos_ (objcopy). macOS: o mesmo com o alvo do host
# (Mach-O), llvm-objcopy e -dead_strip.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/libc-corpus4-hosttest}
ZIG=${ZIG:-$(command -v zig)}
# lib/include ao lado do binário (tarball do zig) ou lib/zig/include (Homebrew)
ZD=$(dirname "$(readlink -f "$ZIG")")
INC=
for c in "$ZD/lib/include" "$ZD/../lib/include" "$ZD/../lib/zig/include"; do [ -d "$c" ] && { INC=$c; break; }; done
[ -n "$INC" ] || { echo "accelerate_corpus2_test: sem lib/include do zig perto de $ZD" >&2; exit 1; }
mkdir -p "$OUT"
if [ "$(uname)" = Darwin ]; then
    TGT=$(uname -m | sed 's/arm64/aarch64/')-macos
    NM=$(command -v llvm-nm || echo /opt/homebrew/opt/llvm/bin/llvm-nm)
    OBJCOPY=$(command -v llvm-objcopy || echo /opt/homebrew/opt/llvm/bin/llvm-objcopy)
    PFX=_
    GC=-Wl,-dead_strip
    LD=ld
else
    TGT=x86_64-freestanding-none
    NM=$(command -v llvm-nm || command -v llvm-nm-21 || command -v nm)
    OBJCOPY=objcopy
    PFX=
    GC=-Wl,--gc-sections
    LD=ld
fi
OBJS=
for f in accelerate_corpus2 accelerate_corpus accelerate_vimage accelerate_mais accelerate_wa accelerate_lapack; do
    "$ZIG" cc -target "$TGT" -O1 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -fpie -w \
        -ffunction-sections -fdata-sections -I"$ROOT/userland/libsystem/kshim" -isystem "$INC" \
        -c "$ROOT/userland/libsystem/$f.c" -o "$OUT/$f.o"
    OBJS="$OBJS $OUT/$f.o"
done
"$LD" -r $OBJS -o "$OUT/accel2_all.o"
"$NM" --defined-only -g "$OUT/accel2_all.o" | awk -v p="$PFX" '{s=$NF; print s" "p"mos_"substr(s, length(p)+1)}' > "$OUT/redef.txt"
"$OBJCOPY" --redefine-syms="$OUT/redef.txt" "$OUT/accel2_all.o" "$OUT/accelerate_corpus2_mos.o"
cc -O1 -ffunction-sections $GC -o "$OUT/accelerate_corpus2_test" "$ROOT/tools/accelerate_corpus2_test.c" "$OUT/accelerate_corpus2_mos.o" -lm
"$OUT/accelerate_corpus2_test" "$OUT"
