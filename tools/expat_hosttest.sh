#!/bin/bash
# Teste de host da expat vendorizada (fonte real + userland/third_party/expat/expat_config.h).
# Compila os 3 .c da expat para o host e roda tools/expat_hosttest.c. Uso: tools/expat_hosttest.sh [dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/expat-hosttest}
VER=2.6.4
SRC="$ROOT/.tools/expat-$VER"
CFG="$ROOT/userland/third_party/expat"
DL="$ROOT/.tools/dl/expat-$VER.tar.xz"
SHA=a695629dae047055b37d50a0ff4776d1d45d0a4c842cf4ccee158441f55ff7ee
mkdir -p "$OUT"
. "$ROOT/tools/baixar.sh"
baixar "https://github.com/libexpat/libexpat/releases/download/R_2_6_4/expat-$VER.tar.xz" "$DL" "$SHA" \
    || { echo "expat-hosttest: download ou SHA-256 falhou" >&2; exit 1; }
[ -f "$SRC/lib/xmlparse.c" ] || { rm -rf "$SRC"; mkdir -p "$SRC"; tar xJf "$DL" -C "$SRC" --strip-components=1; }
# mesmo config de produção (HAVE_ARC4RANDOM_BUF); o arc4random_buf do host vem do próprio expat_hosttest.c
cc -O1 -w -DHAVE_EXPAT_CONFIG_H -D_REENTRANT \
    -I"$CFG" -I"$SRC/lib" \
    "$ROOT/tools/expat_hosttest.c" "$SRC/lib/xmlparse.c" "$SRC/lib/xmlrole.c" "$SRC/lib/xmltok.c" \
    -o "$OUT/expat_hosttest"
"$OUT/expat_hosttest"
