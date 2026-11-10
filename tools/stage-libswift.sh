#!/bin/sh
# Equivalente Linux do trecho de libswift* do build.ps1.
# Copia a cadeia compilada se ela existir e completa com stubs o que faltar.
# Não sobrescreve uma libswift*.dylib que já está no destino.
#   tools/stage-libswift.sh [destino]     destino padrão: build/esp/mOS/usr/lib/swift
set -e
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
dest=${1:-"$root/build/esp/mOS/usr/lib/swift"}
mkdir -p "$dest"
if [ -f "$root/build/universal-swift/usr/lib/swift/libswiftCore.dylib" ]; then
    cp -n "$root/build/universal-swift/usr/lib/swift/"*.dylib "$dest/" 2>/dev/null || true
elif [ -f "$root/build/swift/objc/dylib/usr/lib/swift/libswiftCore.dylib" ]; then
    cp -n "$root/build/swift/objc/dylib/usr/lib/swift/"*.dylib "$dest/" 2>/dev/null || true
fi
cc=$(command -v gcc || command -v cc)
"$cc" -O2 -o /tmp/gen-libswift-stubs "$root/tools/gen-libswift-stubs.c"
tmp=$(mktemp -d)
/tmp/gen-libswift-stubs "$tmp" "$root/tools/libswift-stub-exports.txt"
n=0
for f in "$tmp"/*.dylib; do
    base=$(basename "$f")
    if [ ! -f "$dest/$base" ]; then
        cp "$f" "$dest/$base"
        n=$((n + 1))
    fi
done
rm -rf "$tmp"
echo "libswift em $dest ($n stub(s) novos; dylib que já existia foi mantida)"
