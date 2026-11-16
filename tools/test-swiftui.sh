#!/bin/bash
# Linux: teste do SwiftUI.framework (as asserções ficam em userland/tests-swiftobjc/swiftui/main.swift). Compila o teste
# com as mesmas opções do módulo OpenSwiftUI (o .rsp que tools/build-swiftui-objc.py grava), liga como um binário da
# Apple contra /System/Library/Frameworks/SwiftUI.framework de build/swift/objc/dylib e roda no build/machotest com essa
# árvore como raiz do VFS. Placar: "swiftui: N/N ok".
# Antes: tools/build-swiftui-objc.py (e o que ele pede), build/machotest e build/libSystem.mos (docs/testes-linux.md).
# MOS_ARCH=arm64: compila e liga o mesmo teste arm64 contra build/swift/objc-arm64 e para aí; ele roda na VM pelo
# tradutor: tools/a64rt-teste-vm.sh <dir do build.sh> 900 build/swiftobjctest/swiftui-arm64/SwiftUITest
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
zig=${MOS_ZIG:-$(command -v zig || true)}
fe=${SWIFTC:-$root/.tools/swift-6.4.2/usr/bin/swift-frontend}
arch=${MOS_ARCH:-x86_64}
case "$arch" in x86_64) sfx=""; zt=x86_64-macos ;; arm64) sfx="-arm64"; zt=aarch64-macos ;; *) echo "MOS_ARCH: x86_64 ou arm64" >&2; exit 2 ;; esac
objc="$root/build/swift/objc$sfx"
dy="$objc/dylib"
sw="$dy/usr/lib/swift"
sui="$objc/swiftui"
fw="$dy/System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI"
cmb="$dy/System/Library/Frameworks/Combine.framework/Versions/A/Combine"
t="$root/build/swiftobjctest/swiftui$sfx"
host=("$root/build/machotest" "$root/build/libSystem.mos")
[ "$arch" = arm64 ] && host=()
for f in "${host[@]}" "$fe" "$sw/libswiftCore.dylib" "$sw/libswiftFoundation.dylib" "$fw" \
         "$sui/OpenSwiftUI/OpenSwiftUI.rsp"; do
    [ -e "$f" ] || { echo "test-swiftui: falta $f" >&2; exit 2; }
done
mkdir -p "$t"
rm -f "$t/SwiftUITest" "$t/SwiftUITest.o"
rm -rf "$t/mc"
# as opções do módulo OpenSwiftUI, sem as que só valem para compilar o próprio framework
python3 - "$sui/OpenSwiftUI/OpenSwiftUI.rsp" "$t/SwiftUITest.rsp" "$sui" "$t" "$root/userland/tests-swiftobjc/swiftui/main.swift" <<'EOF'
import sys
src, dst, sui, t, main = sys.argv[1:6]
drop_with_arg = {'-emit-module-path', '-o', '-module-name', '-module-abi-name', '-package-name', '-module-cache-path'}
drop = {'-c', '-parse-as-library', '-enable-library-evolution', '-experimental-spi-only-imports', '-typecheck'}
out, skip = [], False
for line in open(src).read().split('\n'):
    x = line.strip()
    if skip:
        skip = False
        continue
    if not x or x in drop or x.strip('"').endswith('.swift'):
        continue
    if x in drop_with_arg:
        skip = True
        continue
    out.append(x)
out += ['-c', '-I', sui + '/OpenSwiftUI', '-module-name', 'SwiftUITest', '-module-cache-path', t + '/mc', main, '-o', t + '/SwiftUITest.o']
open(dst, 'w').write('\n'.join(out) + '\n')
EOF
"$fe" -frontend @"$t/SwiftUITest.rsp" > "$t/compilacao.txt" 2>&1 \
    || { grep -m20 ' error: ' "$t/compilacao.txt" >&2; echo "compilação FALHOU" >&2; exit 1; }
ln -sfn "$fw" "$t/SwiftUI-link.dylib"
libs=("$t/SwiftUI-link.dylib")
[ -f "$cmb" ] && { ln -sfn "$cmb" "$t/Combine-link.dylib"; libs+=("$t/Combine-link.dylib"); }
for b in libswiftCore libswift_Concurrency libswiftDarwin libswiftObjectiveC libswiftCoreFoundation libswiftCoreGraphics \
         libswiftDispatch libswiftFoundation libswift_StringProcessing libswiftObservation; do
    [ -f "$sw/$b.dylib" ] && libs+=("$sw/$b.dylib")
done
"$zig" cc -target "$zt" -o "$t/SwiftUITest" "$t/SwiftUITest.o" "${libs[@]}" -Wl,-undefined,dynamic_lookup \
    >> "$t/compilacao.txt" 2>&1 || { tail -20 "$t/compilacao.txt" >&2; echo "link FALHOU" >&2; exit 1; }
if [ "$arch" = arm64 ]; then
    file "$t/SwiftUITest" 2>/dev/null || ls -l "$t/SwiftUITest"
    echo "test-swiftui arm64: ligado em $t/SwiftUITest (rode na VM com tools/a64rt-teste-vm.sh)"
    exit 0
fi
export MACHOTEST_WATCHDOG="${MACHOTEST_WATCHDOG:-30000}"
"$root/build/machotest" "$root/build/libSystem.mos" "$dy" "$t/SwiftUITest" > "$t/saida.txt" 2>&1
code=$?
grep -E 'swiftui|  ok   |FALHA|exceção|não implementado|macho_load' "$t/saida.txt" | head -60
if grep -E -q 'swiftui: ([0-9]+)/\1 ok' "$t/saida.txt"; then exit 0; fi
echo "swiftui FALHOU (machotest saiu $code); saída em $t/saida.txt" >&2
exit 1
