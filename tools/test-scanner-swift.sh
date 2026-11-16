#!/bin/bash
# Linux: teste do Scanner do módulo Foundation (asserções em userland/tests-swiftobjc/scanner/main.swift).
# Liga o teste como um binário da Apple contra as dylibs de build/swift/objc/dylib (libswiftCore real, libswiftFoundation,
# libswiftDispatch) e roda no build/machotest com essa árvore como raiz (só leitura).
# Placar: "scanner: N/N ok". Antes: tools/build-swift-foundation-objc.sh e o libSystem.mos com fdn_scanner.c.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
zig=${MOS_ZIG:-$(command -v zig || true)}
fe=${SWIFTC:-$root/.tools/swift-6.4.2/usr/bin/swift-frontend}
objc="$root/build/swift/objc"
dy="$objc/dylib"
sw="$dy/usr/lib/swift"
t="$root/build/swiftobjctest/scanner"
res=/tmp/mos-swift-resdir
for f in "$root/build/machotest" "$root/build/libSystem.mos" "$fe" "$sw/libswiftCore.dylib" "$sw/libswiftFoundation.dylib"; do
    [ -e "$f" ] || { echo "test-scanner: falta $f" >&2; exit 2; }
done
mkdir -p "$t"
rm -f "$t/ScanTest"
rm -rf "$t/mc"
"$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" -module-name ScanTest -swift-version 5 -O \
    -I "$objc" -I "$objc/overlays" -I "$root/.tools/swift-src/stdlib/public/SwiftShims/swift/shims" \
    -module-cache-path "$t/mc" -Xcc "-isystem$objc/sdk/include" -Xcc -nostdlibinc -Xcc -DMOS_SWIFT_OBJC=1 \
    -Xcc -Wno-nullability-completeness "$root/userland/tests-swiftobjc/scanner/main.swift" -o "$t/ScanTest.o" \
    > "$t/compilacao.txt" 2>&1 || { grep -m20 ' error: ' "$t/compilacao.txt" >&2; echo "compilação FALHOU" >&2; exit 1; }
libs=()
for b in libswiftCore libswift_Concurrency libswiftDarwin libswiftObjectiveC libswiftCoreFoundation libswiftCoreGraphics \
         libswiftDispatch libswiftFoundation libswift_StringProcessing; do
    [ -f "$sw/$b.dylib" ] && libs+=("$sw/$b.dylib")
done
"$zig" cc -target x86_64-macos -o "$t/ScanTest" "$t/ScanTest.o" "${libs[@]}" \
    -Wl,-undefined,dynamic_lookup >> "$t/compilacao.txt" 2>&1 || { tail -20 "$t/compilacao.txt" >&2; echo "link FALHOU" >&2; exit 1; }
export MACHOTEST_WATCHDOG="${MACHOTEST_WATCHDOG:-30000}"
"$root/build/machotest" "$root/build/libSystem.mos" "$dy" "$t/ScanTest" > "$t/saida.txt" 2>&1
code=$?
grep -E 'scanner|  ok   |FALHA|exceção|não implementado|macho_load' "$t/saida.txt" | head -60
if grep -E -q 'scanner: ([0-9]+)/\1 ok' "$t/saida.txt"; then exit 0; fi
echo "scanner FALHOU (machotest saiu $code); saída em $t/saida.txt" >&2
exit 1
