#!/bin/sh
# libswiftCore leva o runtime real e os stubs que sobram.
# libswiftFoundation leva só os stubs (metadado e Data).
# A libc++ é tools/build-libcxx.sh.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
OUT=${1:-$ROOT/build/vscode-macho-stubs}
SRC=$ROOT/tools/vscode-macho-stubs
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-macos-none -shared -o "$OUT/libswiftCore.dylib" \
  "$SRC/libswift-runtime.c" "$SRC/libswift-missing.c" \
  -Wl,-install_name,/usr/lib/swift/libswiftCore.dylib -Wl,-undefined,dynamic_lookup
"$ZIG" cc -target x86_64-macos-none -shared -o "$OUT/libswiftFoundation.dylib" \
  "$SRC/libswift-missing.c" \
  -Wl,-install_name,/usr/lib/swift/libswiftFoundation.dylib -Wl,-undefined,dynamic_lookup
echo "$OUT"
