#!/bin/sh
# libswiftCore: runtime, metadados emitidos pelo swift-frontend e o que sobra de Data.
# libswiftFoundation: só os stubs de Data (o agente de Foundation substitui).
# A libc++ é tools/build-libcxx.sh.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
FE=${SWIFT_FRONTEND:-}
if [ -z "$FE" ]; then
  echo "defina SWIFT_FRONTEND" >&2
  exit 1
fi
OUT=${1:-$ROOT/build/vscode-macho-stubs}
SRC=$ROOT/tools/vscode-macho-stubs
mkdir -p "$OUT"
"$FE" -frontend -c -target x86_64-apple-macosx14.0 -nostdimport -parse-stdlib -parse-as-library \
  -module-name Swift -o "$OUT/SwiftMetadata.o" "$SRC/SwiftMetadata.swift"
"$ZIG" cc -target x86_64-macos-none -c -o "$OUT/libswift-metadata.o" "$SRC/libswift-metadata.c"
"$ZIG" cc -target x86_64-macos-none -c -o "$OUT/error-method-alias.o" "$SRC/error-method-alias.S"
"$ZIG" cc -target x86_64-macos-none -c -o "$OUT/libswift-runtime.o" "$SRC/libswift-runtime.c"
"$ZIG" cc -target x86_64-macos-none -c -o "$OUT/libswift-missing.o" "$SRC/libswift-missing.c"
"$ZIG" cc -target x86_64-macos-none -shared -o "$OUT/libswiftCore.dylib" \
  "$OUT/libswift-runtime.o" "$OUT/libswift-metadata.o" "$OUT/SwiftMetadata.o" \
  "$OUT/error-method-alias.o" "$OUT/libswift-missing.o" \
  -Wl,-install_name,/usr/lib/swift/libswiftCore.dylib -Wl,-undefined,dynamic_lookup
"$ZIG" cc -target x86_64-macos-none -shared -o "$OUT/libswiftFoundation.dylib" \
  "$OUT/libswift-missing.o" \
  -Wl,-install_name,/usr/lib/swift/libswiftFoundation.dylib -Wl,-undefined,dynamic_lookup
echo "$OUT"
