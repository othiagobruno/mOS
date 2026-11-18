#!/bin/bash
# Linux: testes dos frameworks Swift só-Swift (ExtensionFoundation, Translation, FoundationModels, SwiftData, ...).
#   tools/test-frameworks-swift.sh <Framework> [<Framework> ...]
# Para cada um: compila userland/tests-swiftobjc/<framework em minúsculas>/main.swift como um binário da Apple, liga
# contra o framework de build/swift/objc/dylib (saída de tools/build-frameworks-objc.sh) e o runtime Swift real, e roda
# no build/machotest. A raiz do processo é build/swiftobjctest/<fw>/raiz: usr e System apontam para a árvore do build
# e, se existir userland/tests-swiftobjc/<fw>/raiz.sh, ele monta ali os arquivos do teste (ex.: apps em /Applications).
# Placar: "<fw>: N/N ok". Antes: build/machotest, build/libSystem.mos e os frameworks compilados.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
zig=${MOS_ZIG:-$(command -v zig || true)}
fe=${SWIFTC:-$root/.tools/swift-6.4.2/usr/bin/swift-frontend}
objc="$root/build/swift/objc"
dy="$objc/dylib"
sw="$dy/usr/lib/swift"
res=/tmp/mos-swift-resdir
[ $# -gt 0 ] || { echo "uso: $0 <Framework> [...]" >&2; exit 2; }
for f in "$root/build/machotest" "$root/build/libSystem.mos" "$fe" "$sw/libswiftCore.dylib"; do
    [ -e "$f" ] || { echo "test-frameworks-swift: falta $f" >&2; exit 2; }
done
falhas=0
for fw in "$@"; do
    nome=$(echo "$fw" | tr '[:upper:]' '[:lower:]')
    src="$root/userland/tests-swiftobjc/$nome"
    bin="$dy/System/Library/Frameworks/${FWBIN:-$fw}.framework/Versions/A/${FWBIN:-$fw}"
    t="$root/build/swiftobjctest/$nome"
    [ -f "$src/main.swift" ] || { echo "$fw: falta $src/main.swift" >&2; falhas=$((falhas + 1)); continue; }
    [ -f "$bin" ] || { echo "$fw: falta $bin (tools/build-frameworks-objc.sh)" >&2; falhas=$((falhas + 1)); continue; }
    rm -rf "$t"
    mkdir -p "$t/raiz"
    ln -s "$dy/usr" "$t/raiz/usr"
    ln -s "$dy/System" "$t/raiz/System"
    [ -f "$src/raiz.sh" ] && bash "$src/raiz.sh" "$t/raiz"
    inc=()
    for d in "$root"/userland/frameworks/*/include; do inc+=(-I "$d"); done
    # framework sobre o SwiftUI (swiftui-needs): compila com as opções do OpenSwiftUI.rsp (como tools/test-swiftui.sh)
    suilib=()
    fwdir="$root/userland/frameworks/${FWBIN:-$fw}"
    if [ -f "$fwdir/swiftui-needs" ]; then
        python3 - "$objc/swiftui/OpenSwiftUI/OpenSwiftUI.rsp" "$t/swiftui.rsp" <<'PY'
import sys
src, dst = sys.argv[1:3]
drop_arg = {'-emit-module-path', '-o', '-module-name', '-module-abi-name', '-package-name', '-module-cache-path'}
drop = {'-c', '-parse-as-library', '-enable-library-evolution', '-experimental-spi-only-imports', '-typecheck'}
out, skip = [], False
for line in open(src).read().split('\n'):
    x = line.strip()
    if skip:
        skip = False
        continue
    if not x or x in drop or x.strip('"').endswith('.swift'):
        continue
    if x in drop_arg:
        skip = True
        continue
    out.append(x)
open(dst, 'w').write('\n'.join(out) + '\n')
PY
        ln -sf "$dy/System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI" "$t/SwiftUI.dylib"
        suilib=("$t/SwiftUI.dylib")
        cmb="$dy/System/Library/Frameworks/Combine.framework/Versions/A/Combine"
        [ -f "$cmb" ] && { ln -sf "$cmb" "$t/Combine.dylib"; suilib+=("$t/Combine.dylib"); }
        "$fe" -frontend @"$t/swiftui.rsp" -c -module-name Teste -I "$fwdir/include" -I "$objc/swiftui/OpenSwiftUI" \
            -I "$objc/overlays" -module-alias SwiftUI=OpenSwiftUI -module-cache-path "$t/mc" "$src/main.swift" \
            -o "$t/Teste.o" > "$t/compilacao.txt" 2>&1 ||
            { grep -m20 ' error: ' "$t/compilacao.txt" >&2; echo "$fw: compilação FALHOU" >&2; falhas=$((falhas + 1)); continue; }
    else
    "$fe" -frontend -c -target x86_64-apple-macos26 -resource-dir "$res" -module-name Teste -swift-version 5 -O \
        -I "$objc" -I "$objc/overlays" "${inc[@]}" -I "$root/.tools/swift-src/stdlib/public/SwiftShims/swift/shims" \
        -module-cache-path "$t/mc" -Xcc "-isystem$objc/sdk/include" -Xcc -nostdlibinc -Xcc -DMOS_SWIFT_OBJC=1 \
        -Xcc -Wno-nullability-completeness "$src/main.swift" -o "$t/Teste.o" > "$t/compilacao.txt" 2>&1 ||
        { grep -m20 ' error: ' "$t/compilacao.txt" >&2; echo "$fw: compilação FALHOU" >&2; falhas=$((falhas + 1)); continue; }
    fi
    # o zig não aceita o binário do framework sem extensão: liga por um link .dylib
    ln -sf "$bin" "$t/$fw.dylib"
    libs=("$t/$fw.dylib" "${suilib[@]}")
    for b in libswiftCore libswift_Concurrency libswiftDarwin libswiftObjectiveC libswiftCoreFoundation \
             libswiftDispatch libswiftFoundation libswiftObservation libswift_StringProcessing libswiftCoreGraphics; do
        [ -f "$sw/$b.dylib" ] && libs+=("$sw/$b.dylib")
    done
    "$zig" cc -target x86_64-macos -o "$t/Teste" "$t/Teste.o" "${libs[@]}" -Wl,-undefined,dynamic_lookup \
        >> "$t/compilacao.txt" 2>&1 ||
        { tail -20 "$t/compilacao.txt" >&2; echo "$fw: link FALHOU" >&2; falhas=$((falhas + 1)); continue; }
    MACHOTEST_WATCHDOG="${MACHOTEST_WATCHDOG:-60000}" \
        "$root/build/machotest" "$root/build/libSystem.mos" "$t/raiz" "$t/Teste" > "$t/saida.txt" 2>&1
    code=$?
    grep -E "^$nome:|  ok   |FALHA|exceção|não implementado|macho_load" "$t/saida.txt" | head -80
    if grep -E -q "^$nome: ([0-9]+)/\\1 ok" "$t/saida.txt"; then continue; fi
    echo "$fw FALHOU (machotest saiu $code); saída em $t/saida.txt" >&2
    falhas=$((falhas + 1))
done
exit $falhas
