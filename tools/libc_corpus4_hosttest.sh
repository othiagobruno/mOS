#!/bin/bash
# Teste de host de userland/libsystem/libc_corpus4.c. Uso: tools/libc_corpus4_hosttest.sh [dir]
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
[ -n "$INC" ] || { echo "libc_corpus4_hosttest: sem lib/include do zig perto de $ZD" >&2; exit 1; }
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
    -c "$ROOT/userland/libsystem/libc_corpus4.c" -o "$OUT/libc_corpus4.o"
"$NM" --defined-only -g "$OUT/libc_corpus4.o" | awk -v p="$PFX" '{s=$NF; print s" "p"mos_"substr(s, length(p)+1)}' > "$OUT/redef.txt"
"$OBJCOPY" --redefine-syms="$OUT/redef.txt" "$OUT/libc_corpus4.o" "$OUT/libc_corpus4_mos.o"
cc -O1 -ffunction-sections $GC -o "$OUT/libc_corpus4_hosttest" "$ROOT/tools/libc_corpus4_hosttest.c" "$OUT/libc_corpus4_mos.o" -lm
"$OUT/libc_corpus4_hosttest" "$OUT"
