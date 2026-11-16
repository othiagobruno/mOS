#!/bin/bash
# CoreImage.framework Mach-O para a imagem do Linux (tools/build.sh).
# O .ps1 continua valendo no Windows. Sem este arquivo o loader fica na fachada
# vazia do libSystem (vscode_fwclasses.c).
# Uso: tools/build-coreimage.sh <zig> <raiz mOS da imagem> [x86_64|arm64]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${1:?zig}
DEST=${2:?raiz}
case "${3:-x86_64}" in x86_64) ZT=x86_64-macos ;; arm64) ZT=aarch64-macos ;; *) echo "arquitetura: x86_64 ou arm64" >&2; exit 1 ;; esac
SRC="$ROOT/userland/frameworks/CoreImage"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
"$ZIG" cc -target $ZT -O2 -c -fvisibility=hidden \
    -I"$SRC" \
    "$SRC/coreimage_cpu.c" -o "$TMP/cpu.o"
"$ZIG" cc -target $ZT -O2 -c -x objective-c -fblocks \
    -I"$ROOT/userland/sdk/include" -I"$SRC" -Wno-objc-root-class \
    "$SRC/CoreImage.m" -o "$TMP/ci.o"
install=/System/Library/Frameworks/CoreImage.framework/Versions/A/CoreImage
dir="$DEST$install"
mkdir -p "$(dirname "$dir")"
"$ZIG" cc -target $ZT -shared -o "$dir" \
    -Wl,-install_name,"$install" -Wl,-undefined,dynamic_lookup \
    "$TMP/ci.o" "$TMP/cpu.o"
echo "CoreImage: $dir"
