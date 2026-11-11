#!/bin/bash
# libxml2 2.13.8 (MIT) como /usr/lib/libxml2.2.dylib Mach-O x86_64, como o SQLite e a zlib da imagem.
# O WhatsApp liga htmlReadMemory, htmlParseChunk, xmlReadMemory, xmlXPath* e xmlFindCharEncodingHandler
# nela (machocheck, 10 nov. 2026). iconv e pthread vêm do libSystem; sem zlib, lzma, ICU, HTTP e FTP.
# O fonte é o tarball oficial do GNOME, conferido pelo SHA-256, em .tools/ (fora do git).
#   tools/build-libxml2.sh [destino-da-dylib]     padrão: build/libxml2/libxml2.2.dylib
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-}
if [ -z "$ZIG" ]; then for z in "$ROOT"/.tools/zig-*/zig; do [ -x "$z" ] && { ZIG=$z; break; }; done; fi
[ -n "$ZIG" ] && [ -x "$ZIG" ] || { echo "build-libxml2: sem zig (MOS_ZIG)" >&2; exit 1; }
VER=2.13.8
SHA=277294cb33119ab71b2bc81f2f445e9bc9435b893ad15bb2cd2b0e859a0ee84a
DL="$ROOT/.tools/dl/libxml2-$VER.tar.xz"
SRC="$ROOT/.tools/libxml2-$VER"
OUT=${1:-$ROOT/build/libxml2/libxml2.2.dylib}
mkdir -p "$ROOT/.tools/dl" "$(dirname "$OUT")"
if [ ! -f "$DL" ] || [ "$(sha256sum "$DL" | cut -d' ' -f1)" != "$SHA" ]; then
    curl -fL --retry 3 -o "$DL.partial" "https://download.gnome.org/sources/libxml2/2.13/libxml2-$VER.tar.xz"
    mv "$DL.partial" "$DL"
fi
[ "$(sha256sum "$DL" | cut -d' ' -f1)" = "$SHA" ] || { echo "build-libxml2: SHA-256 não confere" >&2; exit 1; }
WRAP=$(mktemp -d)
trap 'rm -rf "$WRAP"' EXIT
printf '#!/bin/sh\nexec "%s" cc -target x86_64-macos "$@"\n' "$ZIG" > "$WRAP/cc"
chmod +x "$WRAP/cc"
if [ ! -f "$SRC/config.h" ]; then
    rm -rf "$SRC"; mkdir -p "$SRC"
    tar xJf "$DL" -C "$SRC" --strip-components=1
    # configure em modo cruzado (não roda nada do alvo); só gera config.h e xmlversion.h
    (cd "$SRC" && ./configure --host=x86_64-apple-darwin CC="$WRAP/cc" --disable-dependency-tracking \
        --without-python --without-lzma --without-zlib --without-icu --without-http --without-ftp \
        --without-debug --without-catalog --without-modules --disable-static --enable-shared > configure.log 2>&1) \
        || { tail -20 "$SRC/configure.log" >&2; exit 1; }
fi
# A lista é a am_libxml2_la_OBJECTS do Makefile gerado com essas opções.
FILES="buf.c chvalid.c dict.c entities.c encoding.c error.c globals.c hash.c list.c parser.c parserInternals.c
SAX2.c threads.c tree.c uri.c valid.c xmlIO.c xmlmemory.c xmlstring.c c14n.c HTMLparser.c HTMLtree.c xmlsave.c
pattern.c xmlreader.c xmlregexp.c xmlunicode.c relaxng.c xmlschemas.c xmlschemastypes.c schematron.c xmlwriter.c
xinclude.c xpath.c xlink.c xpointer.c"
OBJ=$(mktemp -d)
objs=()
for f in $FILES; do
    "$ZIG" cc -target x86_64-macos -O2 -fPIC -DHAVE_CONFIG_H -D_REENTRANT -I"$SRC" -I"$SRC/include" \
        -w -c "$SRC/$f" -o "$OBJ/${f%.c}.o"
    objs+=("$OBJ/${f%.c}.o")
done
"$ZIG" cc -target x86_64-macos -shared -o "$OUT" -Wl,-install_name,/usr/lib/libxml2.2.dylib \
    -Wl,-undefined,dynamic_lookup "${objs[@]}"
rm -rf "$OBJ"
echo "libxml2 $VER: $OUT ($(wc -c < "$OUT") bytes)"
