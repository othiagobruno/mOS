#!/bin/bash
# expat 2.6.4 (licença MIT) como /usr/lib/libexpat.1.dylib Mach-O, como a libxml2/zlib/SQLite da imagem.
# Vários apps do corpus ligam XML_ParserCreate/XML_Parse/XML_Set*Handler/XML_GetErrorCode nela (docs/lacunas/leonardo.md).
# Só 3 fontes (xmltok.c inclui xmltok_impl.c e xmltok_ns.c). Entropia por arc4random_buf do libSystem.
# O fonte é o tarball oficial do GitHub, conferido pelo SHA-256, em .tools/ (fora do git).
#   tools/build-expat.sh [destino-da-dylib]     padrão: build/expat/libexpat.1.dylib
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-}
if [ -z "$ZIG" ]; then for z in "$ROOT"/.tools/zig-*/zig; do [ -x "$z" ] && { ZIG=$z; break; }; done; fi
[ -n "$ZIG" ] || ZIG=$(command -v zig || true)
[ -n "$ZIG" ] && [ -x "$ZIG" ] || { echo "build-expat: sem zig (MOS_ZIG)" >&2; exit 1; }
VER=2.6.4
SHA=a695629dae047055b37d50a0ff4776d1d45d0a4c842cf4ccee158441f55ff7ee
DL="$ROOT/.tools/dl/expat-$VER.tar.xz"
SRC="$ROOT/.tools/expat-$VER"
ARCH=${MOS_ARCH:-x86_64}   # x86_64 ou arm64
case "$ARCH" in x86_64) ZT=x86_64 ;; arm64) ZT=aarch64 ;; *) echo "MOS_ARCH=$ARCH: use x86_64 ou arm64" >&2; exit 1 ;; esac
OUT=${1:-$ROOT/build/expat/libexpat.1.dylib}
mkdir -p "$ROOT/.tools/dl" "$(dirname "$OUT")"
. "$ROOT/tools/baixar.sh"
baixar "https://github.com/libexpat/libexpat/releases/download/R_2_6_4/expat-$VER.tar.xz" "$DL" "$SHA" \
    || { echo "build-expat: download ou SHA-256 falhou" >&2; exit 1; }
if [ ! -f "$SRC/lib/xmlparse.c" ]; then
    rm -rf "$SRC"; mkdir -p "$SRC"
    tar xJf "$DL" -C "$SRC" --strip-components=1
fi
# o expat_config.h do mOS (nossa pasta tem prioridade; o include "expat_config.h" dos .c acha o nosso)
CFG="$ROOT/userland/third_party/expat"
OBJ=$(mktemp -d)
trap 'rm -rf "$OBJ"' EXIT
objs=()
for f in xmlparse.c xmlrole.c xmltok.c; do
    "$ZIG" cc -target $ZT-macos -O2 -g0 -fPIC -DHAVE_EXPAT_CONFIG_H -D_REENTRANT -DXML_ENABLE_VISIBILITY=0 \
        -I"$CFG" -I"$SRC/lib" -w -c "$SRC/lib/$f" -o "$OBJ/${f%.c}.o"
    objs+=("$OBJ/${f%.c}.o")
done
"$ZIG" cc -target $ZT-macos -shared -o "$OUT" -Wl,-install_name,/usr/lib/libexpat.1.dylib \
    -Wl,-compatibility_version,1.0.0 -Wl,-current_version,9.1.0 -Wl,-undefined,dynamic_lookup "${objs[@]}"
echo "expat $VER ($ARCH): $OUT ($(wc -c < "$OUT") bytes)"
