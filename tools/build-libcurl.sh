#!/bin/bash
# mOS — libcurl 8.23.0 (licença curl, COPYING) como /usr/lib/libcurl.4.dylib. O mesmo de tools/build-libcurl.ps1.
# Fonte: https://curl.se/download/curl-8.23.0.tar.gz em .tools/curl-dl.
# tools/libcurl/curl_config.h liga HTTP, arquivo local, zlib e mbed TLS 3.6.4 (Apache-2.0, .tools/mbedtls-dl).
# A verificação de certificado fica ligada. O bundle é /usr/share/curl/cacert.pem (assets/certs/cacert.pem).
# Fora: LDAP, HTTP/2, HTTP/3, SSH, brotli, zstd, libidn e libpsl.
# A libz é a de tools/build-libz.sh (build/zlib-x86_64).
# Saída em build/curl-x86_64: libcurl.4.dylib, COPYING, LICENSE.mbedtls e certs/cacert.pem.
#   MOS_ZIG=/caminho/do/zig bash tools/build-libcurl.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
TGT=x86_64-macos.11.0
OUT=$ROOT/build/curl-x86_64
DY=$OUT/libcurl.4.dylib
JOBS=$(nproc 2>/dev/null || echo 4)

VER=8.23.0
SHA=e3de77cfa6367083c776a26647fc9f633b538c06c3baa73d931a252ac8f72b49
DL=$ROOT/.tools/curl-dl
SRC=$DL/curl-$VER
MVER=3.6.4
MSHA=ec35b18a6c593cf98c3e30db8b98ff93e8940a8c4e690e66b41dfc011d678110
MDL=$ROOT/.tools/mbedtls-dl
MSRC=$MDL/mbedtls-$MVER

. "$ROOT/tools/baixar.sh"
baixar "https://curl.se/download/curl-$VER.tar.gz" "$DL/curl-$VER.tar.gz" "$SHA" || exit 1
baixar "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$MVER/mbedtls-$MVER.tar.bz2" \
    "$MDL/mbedtls-$MVER.tar.bz2" "$MSHA" || exit 1

# tar por python3 (tarfile): o Linux de build pode não ter bzip2. O filtro "data" recusa caminho absoluto e "..".
extrair() {
    python3 -I -c 'import sys, tarfile
with tarfile.open(sys.argv[1]) as t:
    if hasattr(tarfile, "data_filter"):
        t.extractall(sys.argv[2], filter="data")
    else:
        for m in t.getmembers():
            if m.name.startswith("/") or ".." in m.name.split("/"):
                sys.exit("tar: caminho suspeito: " + m.name)
        t.extractall(sys.argv[2])' "$1" "$2"
}
if [ ! -f "$SRC/lib/easy.c" ]; then
    extrair "$DL/curl-$VER.tar.gz" "$DL"
    [ -f "$SRC/lib/easy.c" ] || { echo "curl: extrair o fonte falhou" >&2; exit 1; }
fi
if [ ! -f "$MSRC/include/mbedtls/ssl.h" ]; then
    extrair "$MDL/mbedtls-$MVER.tar.bz2" "$MDL"
    [ -f "$MSRC/include/mbedtls/ssl.h" ] || { echo "mbedtls: extrair o fonte falhou" >&2; exit 1; }
fi

MOS_ZIG="$ZIG" sh "$ROOT/tools/build-libz.sh"
ZLIB=$ROOT/build/zlib-x86_64/libz.1.dylib
ZSRC=$ROOT/.tools/zlib-dl/zlib-1.3.2
if [ ! -f "$ZLIB" ] || [ ! -f "$ZSRC/zlib.h" ]; then
    echo "libcurl: falta a libz (build/zlib-x86_64, .tools/zlib-dl)" >&2
    exit 1
fi

if [ -f "$DY" ] && [ "$DY" -nt "$0" ] && [ "$DY" -nt "$ROOT/tools/libcurl/curl_config.h" ] && [ "$DY" -nt "$ZLIB" ]; then
    echo "OK: $DY (cache)"
    exit 0
fi

# compila em paralelo. A lista tem pares "fonte\0objeto\0" (caminhos com espaço e acento); o xargs acrescenta
# cada par ao fim do comando do compilador. O primeiro erro para o build.
compilar() {
    local lista=$1
    shift
    xargs -0 -n 2 -P "$JOBS" bash -c '
        f=${@: -2:1}; o=${@: -1}; set -- "${@:1:$#-2}"
        "$@" -c "$f" -o "$o" 2>"$o.err" || { echo "FALHOU: $f" >&2; grep "error:" "$o.err" | head -8 >&2; exit 255; }
    ' _ "$@" < "$lista"
}

MOBJ=$OUT/mbed
OBJ=$OUT/obj
rm -rf "$MOBJ" "$OBJ"
mkdir -p "$MOBJ" "$OBJ"

: > "$OUT/mbed.lista"
for f in "$MSRC"/library/*.c; do
    case "$(basename "$f")" in
        *net_sockets*) continue ;;
    esac
    printf '%s\0%s\0' "$f" "$MOBJ/$(basename "$f").o" >> "$OUT/mbed.lista"
done
: > "$OUT/curl.lista"
while IFS= read -r f; do
    tag=${f#"$SRC/lib/"}
    tag=${tag//\//_}
    printf '%s\0%s\0' "$f" "$OBJ/$tag.o" >> "$OUT/curl.lista"
done < <(find "$SRC/lib" -name '*.c' | sort)

echo "mbed TLS $MVER: $(( $(tr -cd '\0' < "$OUT/mbed.lista" | wc -c) / 2 )) arquivos"
compilar "$OUT/mbed.lista" "$ZIG" cc -target "$TGT" -O2 -g0 -fPIC -I "$MSRC/include" -I "$MSRC/library" \
    '-DMBEDTLS_CONFIG_FILE="mbedtls/mbedtls_config.h"' || { echo "mbedtls FALHOU" >&2; exit 1; }
echo "curl $VER: $(( $(tr -cd '\0' < "$OUT/curl.lista" | wc -c) / 2 )) arquivos"
compilar "$OUT/curl.lista" "$ZIG" cc -target "$TGT" -O2 -g0 -fPIC -fvisibility=hidden \
    -DBUILDING_LIBCURL -DHAVE_CONFIG_H -DCURL_HIDDEN_SYMBOLS -DUSE_MBEDTLS \
    -I "$ROOT/tools/libcurl" -I "$SRC/lib" -I "$SRC/include" -I "$ZSRC" -I "$MSRC/include" || { echo "libcurl FALHOU" >&2; exit 1; }

# lista de objetos num arquivo de resposta (o caminho da raiz tem espaço e acento)
RSP=$OUT/link.rsp
find "$OBJ" "$MOBJ" -name '*.o' | sort | sed 's/.*/"&"/' > "$RSP"
"$ZIG" cc -target "$TGT" -shared -fvisibility=hidden -o "$DY" "@$RSP" "$ZLIB" \
    -Wl,-install_name,/usr/lib/libcurl.4.dylib -Wl,-compatibility_version,7.0.0 -Wl,-current_version,8.23.0 \
    -Wl,-undefined,dynamic_lookup
cp -f "$SRC/COPYING" "$OUT/COPYING"
cp -f "$MSRC/LICENSE" "$OUT/LICENSE.mbedtls"
mkdir -p "$OUT/certs"
cp -f "$ROOT/assets/certs/cacert.pem" "$OUT/certs/cacert.pem"
echo "OK: $DY ($(( $(wc -c < "$DY") / 1024 )) KiB, curl $VER, mbed TLS $MVER, verificação ligada)"
