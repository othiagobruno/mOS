#!/bin/bash
# Linux: o mesmo que tools/build-frameworks-objc.ps1. Cada userland/frameworks/<Nome> (*.m, *.c) vira
# /System/Library/Frameworks/<Nome>.framework/Versions/A/<Nome> (ou o caminho do arquivo install-path)
# dentro de <destino>, a mesma árvore que o machocheck --disco e o machotest leem.
# Overlays *.swift (CoreData, CryptoKit) usam o swift-frontend 6.4.2 (SWIFTC ou .tools/swift-6.4.2) e o
# runtime de tools/build-libswift-real.sh (Swift.swiftmodule, overlays e /tmp/mos-swift-resdir); sem eles o
# framework sai só com a parte ObjC/C, e o aviso diz isso.
#   tools/build-frameworks-objc.sh [destino] [Nome,...]     destino padrão: build/swift/objc/dylib
# MOS_ARCH=arm64: a fatia arm64 (processo arm64 do tradutor, docs/runtime-arm64-plano.md) em build/swift/objc-arm64/dylib,
# com o runtime Swift arm64 de build/swift/objc-arm64; o tools/build.sh junta as duas árvores com o lipo-mos.py --arvore.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ARCH=${MOS_ARCH:-x86_64}
case "$ARCH" in
    x86_64) ZT=x86_64-macos; SWT=x86_64-apple-macos26; OBJC="$ROOT/build/swift/objc" ;;
    arm64) ZT=aarch64-macos; SWT=arm64-apple-macos26; OBJC="$ROOT/build/swift/objc-arm64" ;;
    *) echo "MOS_ARCH=$ARCH: use x86_64 ou arm64" >&2; exit 1 ;;
esac
DST=${1:-$OBJC/dylib}
ONLY=${2:-}
ZIG=${MOS_ZIG:-$(command -v zig || true)}
if [ -z "$ZIG" ] || [ ! -x "$ZIG" ]; then echo "build-frameworks-objc: falta zig (MOS_ZIG ou PATH)" >&2; exit 1; fi
TARGET=${MOS_ZIG_TARGET:-$ZT}
FE=${SWIFTC:-$ROOT/.tools/swift-6.4.2/usr/bin/swift-frontend}
RES=/tmp/mos-swift-resdir
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fails=0
for fw in "$ROOT"/userland/frameworks/*/; do
    name=$(basename "$fw")
    # no Linux o libcompression entra no libSystem.mos (tools/build.sh, com os -I do lz4/lzfse/xz)
    [ "$name" = libcompression ] && continue
    # o CryptoKit tem build próprio (tools/build-cryptokit-macho.sh, módulos Swift mínimos)
    [ "$name" = CryptoKit ] && continue
    # o JavaScriptCore compila com o motor kernel/web/js_*.c (tools/build.sh, "==> JavaScriptCore.framework")
    [ "$name" = JavaScriptCore ] && continue
    if [ -n "$ONLY" ] && ! printf ',%s,' "$ONLY" | grep -q ",$name,"; then continue; fi
    objs=()
    swobjs=()
    bad=0
    # swift-overlay-path: o overlay Swift vai numa dylib à parte (ex.: /usr/lib/swift/libswiftNaturalLanguage.dylib,
    # como no macOS), não dentro do framework
    swdylib=
    [ -f "$fw/swift-overlay-path" ] && swdylib=$(grep -m1 '^/' "$fw/swift-overlay-path" | tr -d '\r')
    for f in "$fw"*.m "$fw"*.c; do
        [ -f "$f" ] || continue
        base=$(basename "$f")
        o="$TMP/${name}_${base%.*}.o"
        lang=()
        [ "${f##*.}" = m ] && lang=(-x objective-c)
        if ! "$ZIG" cc -target "$TARGET" -O2 -c "${lang[@]}" -I"$ROOT/userland/sdk/include" -Wno-objc-root-class "$f" -o "$o" 2>"$o.err"; then
            grep -m8 error "$o.err" | sed 's/^/  /' >&2
            bad=1
        else
            objs+=("$o")
        fi
    done
    # swift-needs: módulos Swift que o overlay importa e que saem de outro passo do build (ex.: o Foundation de
    # build-swift-foundation-objc.sh, que roda depois deste). Sem eles o framework sai só com a parte ObjC/C;
    # tools/build.sh chama este script de novo, só com esses frameworks, depois do Foundation.
    missing=
    if [ -f "$fw/swift-needs" ]; then
        for m in $(grep -v '^#' "$fw/swift-needs" | tr -d '\r'); do
            [ -e "$OBJC/overlays/$m.swiftmodule" ] || missing="$missing $m"
        done
    fi
    if ls "$fw"*.swift >/dev/null 2>&1 && [ -n "$missing" ]; then
        echo "$name: aviso: overlay Swift adiado (falta o módulo$missing); só a parte ObjC/C por enquanto" >&2
    elif ls "$fw"*.swift >/dev/null 2>&1; then
        if [ -x "$FE" ] && [ -d "$OBJC/Swift.swiftmodule" ] && [ -d "$RES" ] && [ "$bad" = 0 ]; then
            so="$TMP/${name}_swift.o"
            # iface/X.swift: só a interface de um módulo X que o overlay importa e que não existe aqui (ex.: o View do
            # SwiftUI para o ExtensionKit); vira um .swiftmodule temporário e não é ligado
            inc=(-I "$fw/include")
            for d in "$ROOT"/userland/frameworks/*/include; do [ "$d" = "$fw/include" ] || inc+=(-I "$d"); done
            if ls "$fw"iface/*.swift >/dev/null 2>&1; then
                mkdir -p "$TMP/iface-$name"
                for i in "$fw"iface/*.swift; do
                    "$FE" -frontend -emit-module -target $SWT -resource-dir "$RES" -parse-as-library -O \
                        -module-name "$(basename "$i" .swift)" -enable-library-evolution -I "$OBJC" -I "$OBJC/overlays" \
                        -I "$ROOT/.tools/swift-src/stdlib/public/SwiftShims/swift/shims" -module-cache-path "$OBJC/modcache" -o "$TMP/iface-$name/$(basename "$i" .swift).swiftmodule" "$i" 2>>"$so.err"
                done
                inc+=(-I "$TMP/iface-$name")
            fi
            if "$FE" -frontend -c -target $SWT -resource-dir "$RES" -module-name "$name" -parse-as-library -O \
                    -enable-library-evolution -import-underlying-module "${inc[@]}" -I "$OBJC" -I "$OBJC/overlays" \
                    -I "$ROOT/.tools/swift-src/stdlib/public/SwiftShims/swift/shims" -module-cache-path "$OBJC/modcache" \
                    -Xcc "-isystem$OBJC/sdk/include" -Xcc -Wno-nullability-completeness -Xcc -DMOS_SWIFT_OBJC=1 \
                    -emit-module-path "$TMP/$name.swiftmodule" -o "$so" "$fw"*.swift 2>"$so.err"; then
                if [ -n "$swdylib" ]; then swobjs+=("$so"); else objs+=("$so"); fi
                # o módulo fica visível para os frameworks seguintes (o ExtensionKit importa o ExtensionFoundation)
                cp -f "$TMP/$name.swiftmodule" "$OBJC/overlays/$name.swiftmodule" 2>/dev/null || true
            else
                grep -m8 -E ':[0-9]+:[0-9]+: error: |<unknown>:0: error: ' "$so.err" | sed 's/^/  /' >&2
                bad=1
            fi
        else
            echo "$name: aviso: overlay Swift não compilado (falta swift-frontend ou o runtime real); só a parte ObjC/C" >&2
        fi
    fi
    if [ "$bad" = 1 ] || [ ${#objs[@]} -eq 0 ]; then echo "$name: FALHOU" >&2; fails=$((fails + 1)); continue; fi
    case "$name" in
        lib*) install="/usr/lib/$name.dylib" ;;
        *) install="/System/Library/Frameworks/$name.framework/Versions/A/$name" ;;
    esac
    alias=()
    if [ -f "$fw/install-path" ]; then
        mapfile -t ip < <(grep '^/' "$fw/install-path" | tr -d '\r')
        install=${ip[0]}
        alias=("${ip[@]:1}")
    fi
    out="$DST$install"
    mkdir -p "$(dirname "$out")"
    if ! "$ZIG" cc -target "$TARGET" -shared -o "$out" "-Wl,-install_name,$install" -Wl,-undefined,dynamic_lookup "${objs[@]}" 2>"$TMP/$name.link"; then
        head -8 "$TMP/$name.link" | sed 's/^/  /' >&2
        echo "$name: link FALHOU" >&2
        fails=$((fails + 1))
        continue
    fi
    if [ ${#swobjs[@]} -gt 0 ]; then
        mkdir -p "$(dirname "$DST$swdylib")"
        # o libswiftCore real em dois níveis, quando existe: em busca plana os swift_* iam para os do libSystem
        swcore=()
        [ -f "$OBJC/dylib/usr/lib/swift/libswiftCore.dylib" ] && \
            ! grep -a -q 'STUB mOS libswift' "$OBJC/dylib/usr/lib/swift/libswiftCore.dylib" && \
            swcore=("$OBJC/dylib/usr/lib/swift/libswiftCore.dylib")
        if ! "$ZIG" cc -target "$TARGET" -shared -o "$DST$swdylib" "-Wl,-install_name,$swdylib" -Wl,-undefined,dynamic_lookup \
                "${swobjs[@]}" "${swcore[@]}" 2>"$TMP/$name.swlink"; then
            head -8 "$TMP/$name.swlink" | sed 's/^/  /' >&2
            echo "$name: link do overlay Swift FALHOU" >&2
            fails=$((fails + 1))
        else
            echo "$name: overlay Swift -> $swdylib"
        fi
    fi
    for a in "${alias[@]}"; do
        mkdir -p "$(dirname "$DST$a")"
        cp -f "$out" "$DST$a"
    done
    echo "$name: ok ($(( $(stat -c %s "$out") / 1024 )) KB) -> $install"
done
exit $fails
