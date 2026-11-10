#!/bin/sh
# Dylibs Mach-O com os símbolos que o machocheck ainda marca em libc++ e Swift
# no VS Code 1.145.2. São stubs: cada função retorna na hora.
# Saída: libc++.1.dylib, libswiftCore.dylib, libswiftFoundation.dylib
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
OUT=${1:-$ROOT/build/vscode-macho-stubs}
SRC=$ROOT/tools/vscode-macho-stubs
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-macos-none -shared -o "$OUT/libc++.1.dylib" "$SRC/libcxx-missing.c" \
  -Wl,-install_name,/usr/lib/libc++.1.dylib -Wl,-undefined,dynamic_lookup
"$ZIG" cc -target x86_64-macos-none -shared -o "$OUT/libswiftCore.dylib" "$SRC/libswift-missing.c" \
  -Wl,-install_name,/usr/lib/swift/libswiftCore.dylib -Wl,-undefined,dynamic_lookup
"$ZIG" cc -target x86_64-macos-none -shared -o "$OUT/libswiftFoundation.dylib" "$SRC/libswift-missing.c" \
  -Wl,-install_name,/usr/lib/swift/libswiftFoundation.dylib -Wl,-undefined,dynamic_lookup
echo "$OUT"
