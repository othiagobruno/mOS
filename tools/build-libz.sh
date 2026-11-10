#!/bin/sh
# zlib 1.3.2 como /usr/lib/libz.1.dylib (Mach-O). O mesmo fonte de tools/build-zlib.ps1.
# Não é stub: deflate/inflate são o código do zlib.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
VER=1.3.2
SHA=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
DL=$ROOT/.tools/zlib-dl
TGZ=$DL/zlib-$VER.tar.gz
OUT=$ROOT/build/zlib-x86_64
DY=$OUT/libz.1.dylib
mkdir -p "$DL" "$OUT"
. "$ROOT/tools/baixar.sh"
baixar "https://github.com/madler/zlib/releases/download/v$VER/zlib-$VER.tar.gz" "$TGZ" "$SHA" || exit 1
if [ ! -f "$DL/zlib-$VER/zlib.h" ]; then
    tar -xzf "$TGZ" -C "$DL"
fi
SRC=$DL/zlib-$VER
if [ -f "$DY" ] && [ "$DY" -nt "$SRC/zlib.h" ]; then
    echo "OK: $DY (cache)"
    exit 0
fi
set --
for c in adler32 compress crc32 deflate gzclose gzlib gzread gzwrite infback inffast inflate inftrees trees uncompr zutil; do
    set -- "$@" "$SRC/$c.c"
done
"$ZIG" cc -target x86_64-macos.11.0 -O2 -g0 -fPIC -shared -DHAVE_UNISTD_H -DHAVE_STDARG_H -D_LARGEFILE64_SOURCE=1 \
    -I"$SRC" "$@" -o "$DY" \
    -Wl,-install_name,/usr/lib/libz.1.dylib -Wl,-compatibility_version,1.0.0 -Wl,-current_version,1.3.2
cp -f "$SRC/LICENSE" "$OUT/LICENSE"
echo "OK: $DY"
