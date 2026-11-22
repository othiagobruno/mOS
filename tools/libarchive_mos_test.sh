#!/bin/bash
# Teste de host de userland/libsystem/libarchive_mos.c. Uso: tools/libarchive_mos_hosttest.sh [dir]
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
[ -n "$INC" ] || { echo "libarchive_mos_hosttest: sem lib/include do zig perto de $ZD" >&2; exit 1; }
mkdir -p "$OUT"
if [ "$(uname)" = Darwin ]; then
    TGT=$(uname -m | sed 's/arm64/aarch64/')-macos
    NM=$(command -v llvm-nm || echo /opt/homebrew/opt/llvm/bin/llvm-nm)
    OBJCOPY=$(command -v llvm-objcopy || echo /opt/homebrew/opt/llvm/bin/llvm-objcopy)
    PFX=_
    GC=-Wl,-dead_strip
else
    TGT=x86_64-freestanding-none
    NM=$(command -v llvm-nm || command -v llvm-nm-21 || command -v nm)
    OBJCOPY=objcopy
    PFX=
    GC=-Wl,--gc-sections
fi
"$ZIG" cc -target "$TGT" -O1 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -fpie -w \
    -ffunction-sections -fdata-sections -I"$ROOT/userland/libsystem/kshim" -isystem "$INC" \
    -c "$ROOT/userland/libsystem/libarchive_mos.c" -o "$OUT/libarchive_mos.o"
"$NM" --defined-only -g "$OUT/libarchive_mos.o" | awk -v p="$PFX" '{s=$NF; print s" "p"mos_"substr(s, length(p)+1)}' > "$OUT/redef.txt"
"$OBJCOPY" --redefine-syms="$OUT/redef.txt" "$OUT/libarchive_mos.o" "$OUT/libarchive_mos_mos.o"
cc -O1 -ffunction-sections $GC -o "$OUT/libarchive_mos_test" "$ROOT/tools/libarchive_mos_test.c" "$OUT/libarchive_mos_mos.o" -lm -lz -lbz2
"$OUT/libarchive_mos_test" "$OUT"
