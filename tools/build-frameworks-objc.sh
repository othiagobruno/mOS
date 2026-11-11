#!/bin/bash
# Linux: o mesmo que tools/build-frameworks-objc.ps1. Cada userland/frameworks/<Nome> (*.m, *.c) vira
# /System/Library/Frameworks/<Nome>.framework/Versions/A/<Nome> (ou o caminho do arquivo install-path)
# dentro de <destino>, a mesma árvore que o machocheck --disco e o machotest leem.
# Overlays *.swift precisam do swift-frontend (SWIFTC); sem ele o framework sai só com a parte ObjC/C,
# e o aviso diz isso.
#   tools/build-frameworks-objc.sh [destino] [Nome,...]     destino padrão: build/swift/objc/dylib
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DST=${1:-$ROOT/build/swift/objc/dylib}
ONLY=${2:-}
ZIG=${MOS_ZIG:-$(command -v zig || true)}
if [ -z "$ZIG" ] || [ ! -x "$ZIG" ]; then echo "build-frameworks-objc: falta zig (MOS_ZIG ou PATH)" >&2; exit 1; fi
TARGET=${MOS_ZIG_TARGET:-x86_64-macos}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fails=0
for fw in "$ROOT"/userland/frameworks/*/; do
    name=$(basename "$fw")
    if [ -n "$ONLY" ] && ! printf ',%s,' "$ONLY" | grep -q ",$name,"; then continue; fi
    objs=()
    bad=0
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
    if ls "$fw"*.swift >/dev/null 2>&1; then
        echo "$name: aviso: overlay Swift não compilado (falta swift-frontend); só a parte ObjC/C" >&2
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
    for a in "${alias[@]}"; do
        mkdir -p "$(dirname "$DST$a")"
        cp -f "$out" "$DST$a"
    done
    echo "$name: ok ($(( $(stat -c %s "$out") / 1024 )) KB) -> $install"
done
exit $fails
