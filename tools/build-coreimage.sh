#!/bin/bash
# CoreImage.framework Mach-O para a imagem do Linux (tools/build.sh).
# O .ps1 continua valendo no Windows. Sem este arquivo o loader fica na fachada
# vazia do libSystem (vscode_fwclasses.c).
# Uso: tools/build-coreimage.sh <zig> <raiz mOS da imagem>
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${1:?zig}
DEST=${2:?raiz}
SRC="$ROOT/userland/frameworks/CoreImage"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
"$ZIG" cc -target x86_64-macos -O2 -c -fvisibility=hidden \
    -I"$SRC" \
    "$SRC/coreimage_cpu.c" -o "$TMP/cpu.o"
"$ZIG" cc -target x86_64-macos -O2 -c -x objective-c -fblocks \
    -I"$ROOT/userland/sdk/include" -I"$SRC" -Wno-objc-root-class \
    "$SRC/CoreImage.m" -o "$TMP/ci.o"
install=/System/Library/Frameworks/CoreImage.framework/Versions/A/CoreImage
dir="$DEST$install"
mkdir -p "$(dirname "$dir")"
"$ZIG" cc -target x86_64-macos -shared -o "$dir" \
    -Wl,-install_name,"$install" -Wl,-undefined,dynamic_lookup \
    "$TMP/ci.o" "$TMP/cpu.o"
echo "CoreImage: $dir"
