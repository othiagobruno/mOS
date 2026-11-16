#!/bin/bash
# Compila a libm do musl 1.2.5 (MIT, userland/third_party/musl) para o libSystem do mOS.
# Uso: tools/build-libm-musl.sh <zig> <x86_64|aarch64> <dir-saida> [jobs]
# x86_64: alvo x86_64-freestanding-none (o mesmo do libSystem.mos); long double de 80 bits, como no macOS x86_64.
# aarch64: triplo arm64-apple-macos11-elf (ABI Darwin: long double = double), flags de tools/build-libsystem-arm64.ps1.
# Os .o saem em <dir-saida>; a lista vai em <dir-saida>/objs.txt.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=$1; ARCH=$2; OUTD=$3; J=${4:-2}
ZIGDIR=$(dirname "$(readlink -f "$ZIG")")
[ -d "$ZIGDIR/lib/include" ] || ZIGDIR=$(dirname "$ZIGDIR")
MU="$ROOT/userland/third_party/musl"
INC=(-w -std=c99 -nostdinc -I"$MU/mos/include" -I"$MU/src/internal")
case "$ARCH" in
    x86_64) FL=(-target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -fpie -mno-red-zone
                "${INC[@]}" -I"$MU/arch/generic" -isystem "$ZIGDIR/lib/include"); CC=(cc) ;;
    aarch64) FL=(--target=arm64-apple-macos11-elf -mcpu=apple-m1 -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -fPIC
                 -ffixed-x18 -fsigned-char -fno-common -funwind-tables "${INC[@]}" -I"$MU/arch/aarch64" -I"$MU/arch/generic"
                 -isystem "$ZIGDIR/lib/include"); CC=(clang) ;;
    *) echo "build-libm-musl: arquitetura $ARCH" >&2; exit 2 ;;
esac
mkdir -p "$OUTD"
bash "$MU/mos/srcs.sh" "$MU" "$ARCH" > "$OUTD/srcs.txt"
: > "$OUTD/objs.txt"
while IFS= read -r s; do echo "$OUTD/$(basename "$s").o" >> "$OUTD/objs.txt"; done < "$OUTD/srcs.txt"
export ZIG OUTD
printf '%s\n' "${CC[@]}" "${FL[@]}" > "$OUTD/flags.txt"
# um processo por fonte, J de cada vez
xargs -a "$OUTD/srcs.txt" -P "$J" -I{} bash -c 'mapfile -t F < "$OUTD/flags.txt"; o="$OUTD/$(basename "$1").o"; "$ZIG" "${F[@]}" -c "$1" -o "$o" || { echo "libm: falhou $1" >&2; exit 255; }' _ {}
echo "libm (musl, $ARCH): $(wc -l < "$OUTD/objs.txt") objetos"
