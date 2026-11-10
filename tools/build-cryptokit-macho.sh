#!/bin/sh
# CryptoKit como Mach-O x86_64 que o carregador abre.
# Usa o swift-frontend oficial com -target x86_64-apple-macosx14.0,
# os stubs em tools/cryptokit-macho e o CryptoKit.swift do repo.
# Não copia SDK da Apple.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
FE=${SWIFT_FRONTEND:-}
if [ -z "$FE" ]; then
  echo "defina SWIFT_FRONTEND com o swift-frontend da toolchain oficial" >&2
  exit 1
fi
ZIG=${MOS_ZIG:-zig}
OUT=${1:-$ROOT/build/cryptokit-macho}
STUB=$ROOT/tools/cryptokit-macho
mkdir -p "$OUT"
"$FE" -frontend -emit-module -target x86_64-apple-macosx14.0 -nostdimport -parse-stdlib -parse-as-library \
  -module-name Swift -o "$OUT/Swift.swiftmodule" "$STUB/Swift.swift"
"$FE" -frontend -emit-module -c -target x86_64-apple-macosx14.0 -nostdimport -parse-as-library \
  -module-name Foundation -module-abi-name Foundation -I "$OUT" \
  -emit-module-path "$OUT/Foundation.swiftmodule" -o "$OUT/Foundation.o" "$STUB/Foundation.swift"
"$FE" -frontend -c -target x86_64-apple-macosx14.0 -nostdimport -parse-as-library \
  -module-name CryptoKit -enable-library-evolution -import-underlying-module \
  -I "$ROOT/userland/frameworks/CryptoKit/include" -I "$OUT" \
  -o "$OUT/CryptoKit.o" "$ROOT/userland/frameworks/CryptoKit/CryptoKit.swift"
"$ZIG" cc -target x86_64-macos-none -shared -o "$OUT/CryptoKit" "$OUT/CryptoKit.o" "$OUT/Foundation.o" \
  -Wl,-install_name,/System/Library/Frameworks/CryptoKit.framework/Versions/A/CryptoKit \
  -Wl,-undefined,dynamic_lookup
echo "$OUT/CryptoKit"
