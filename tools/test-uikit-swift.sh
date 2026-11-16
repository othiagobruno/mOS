#!/bin/bash
# Linux: o teste do overlay Swift do UIKit do Catalyst (as asserções ficam em userland/tests-swiftobjc/uikit/main.swift,
# as mesmas do test-uikit-swift.ps1). Liga o teste como um binário da Apple, contra as dylibs de build/swift/objc/dylib
# (libswiftCore real, libswiftFoundation, libswiftUIKit e o UIKit.framework do Catalyst), e roda no build/machotest com
# essa árvore como raiz do VFS. Placar: "uikit: N/N ok".
# Antes: tools/build-libswift-real.sh, tools/build-swift-foundation-objc.sh, tools/build-frameworks-objc.sh
# (UIKit) e tools/build-swift-uikit-objc.sh; build/machotest e build/libSystem.mos (docs/testes-linux.md).
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
zig=${MOS_ZIG:-$(command -v zig || true)}
fe=${SWIFTC:-$root/.tools/swift-6.4.2/usr/bin/swift-frontend}
objc="$root/build/swift/objc"
dy="$objc/dylib"
sw="$dy/usr/lib/swift"
fw="$dy/System/iOSSupport/System/Library/Frameworks/UIKit.framework/Versions/A/UIKit"
uk="$dy/System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib"
t="$root/build/swiftobjctest/uikit"
res=/tmp/mos-swift-resdir
for f in "$root/build/machotest" "$root/build/libSystem.mos" "$fe" "$sw/libswiftCore.dylib" "$sw/libswiftFoundation.dylib" "$fw" "$uk"; do
    [ -e "$f" ] || { echo "test-uikit-swift: falta $f" >&2; exit 2; }
done
mkdir -p "$t"
rm -f "$t/UIKitTest"
rm -rf "$t/mc"
"$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" -module-name UIKitTest -swift-version 5 -O \
    -I "$objc" -I "$objc/overlays" -I "$root/.tools/swift-src/stdlib/public/SwiftShims/swift/shims" \
    -module-cache-path "$t/mc" -Xcc "-isystem$objc/sdk/include" -Xcc -nostdlibinc -Xcc -DMOS_SWIFT_OBJC=1 \
    -Xcc -Wno-nullability-completeness "$root/userland/tests-swiftobjc/uikit/main.swift" -o "$t/UIKitTest.o" \
    > "$t/compilacao.txt" 2>&1 || { grep -m20 ' error: ' "$t/compilacao.txt" >&2; echo "compilação FALHOU" >&2; exit 1; }
ln -sfn "$fw" "$t/UIKit-link.dylib"
libs=()
for b in libswiftCore libswift_Concurrency libswiftDarwin libswiftObjectiveC libswiftCoreFoundation libswiftCoreGraphics \
         libswiftDispatch libswiftFoundation libswift_StringProcessing; do
    [ -f "$sw/$b.dylib" ] && libs+=("$sw/$b.dylib")
done
"$zig" cc -target x86_64-macos -o "$t/UIKitTest" "$t/UIKitTest.o" "$t/UIKit-link.dylib" "$uk" "${libs[@]}" \
    -Wl,-undefined,dynamic_lookup >> "$t/compilacao.txt" 2>&1 || { tail -20 "$t/compilacao.txt" >&2; echo "link FALHOU" >&2; exit 1; }
export MACHOTEST_WATCHDOG="${MACHOTEST_WATCHDOG:-20000}"
"$root/build/machotest" "$root/build/libSystem.mos" "$dy" "$t/UIKitTest" > "$t/saida.txt" 2>&1
code=$?
grep -E 'uikit|  ok   |FALHA|exceção|não implementado|macho_load' "$t/saida.txt" | head -60
if grep -E -q 'uikit: ([0-9]+)/\1 ok' "$t/saida.txt"; then exit 0; fi
echo "uikit FALHOU (machotest saiu $code); saída em $t/saida.txt" >&2
exit 1
