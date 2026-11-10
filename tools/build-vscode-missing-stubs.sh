#!/bin/sh
# libswiftCore.dylib e libswiftFoundation.dylib com o que o VS Code 1.145.2 importa.
# A libc++ não entra aqui: é a do LLVM, tools/build-libcxx.sh.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
OUT=${1:-$ROOT/build/vscode-macho-stubs}
SRC=$ROOT/tools/vscode-macho-stubs
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-macos-none -shared -o "$OUT/libswiftCore.dylib" "$SRC/libswift-missing.c" \
  -Wl,-install_name,/usr/lib/swift/libswiftCore.dylib -Wl,-undefined,dynamic_lookup
"$ZIG" cc -target x86_64-macos-none -shared -o "$OUT/libswiftFoundation.dylib" "$SRC/libswift-missing.c" \
  -Wl,-install_name,/usr/lib/swift/libswiftFoundation.dylib -Wl,-undefined,dynamic_lookup
echo "$OUT"
