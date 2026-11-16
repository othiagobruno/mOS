#!/usr/bin/env bash
# Confere o libSystem antes de um PR: símbolo forte duplicado entre os .c (quebra o link) e símbolo indefinido
# nos arquivos dados. Compila cada .c em objeto (cache em $CACHE; só recompila o que mudou), sem montar o
# libSystem.mos. Trabalho pesado: rode dentro da trava do time.
#   flock /tmp/mos-pesado.lock nice -n 10 tools/libsystem-symcheck.sh [arquivo.c ...]
# Sai 1 se houver duplicata, ou indefinido nos arquivos dados (fora libm, objc_msgSend dos .S e os objetos
# de terceiros que o build.sh liga à parte: compressão, sqlite, áudio).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CACHE=${CACHE:-/tmp/libsystem-symcheck-$(id -u)}
ZIG=${MOS_ZIG:-$(command -v zig)}
ZIGDIR=$(dirname "$(readlink -f "$ZIG")")
export LC_ALL=C ZIG_LOCAL_CACHE_DIR="$CACHE/zc" ZIG_GLOBAL_CACHE_DIR="$CACHE/zc"
mkdir -p "$CACHE/o"
build_one() {
    local f=$1 b o
    b=$(basename "$f" .c); o="$CACHE/o/$b.o"
    [ "$o" -nt "$f" ] && [ -z "$(find "$ROOT/userland/libsystem" -name '*.inc' -newer "$o" -print -quit)" ] && return 0
    "$ZIG" cc -target x86_64-freestanding-none -O2 -c -ffreestanding -fno-builtin -nostdlib -fpie -mno-red-zone -w \
        -I "$ROOT/userland/libsystem/kshim" -I "$ROOT/userland/libsystem/fsinc" -I "$ROOT/kernel/include" \
        -I "$ROOT/userland/third_party/h264bsd" -isystem "$ZIGDIR/lib/include" "$f" -o "$o" || echo "não compilou: $f" >&2
}
export -f build_one; export ROOT CACHE ZIG ZIGDIR
ls "$ROOT"/userland/libsystem/*.c | grep -v sqlite3_mos.c | xargs -P2 -I{} bash -c 'build_one {}'
for o in "$CACHE"/o/*.o; do nm -g --defined-only "$o" | awk -v f="$(basename "$o")" 'NF==3 && $2!~/^[WwVv]$/ {print $3" "f}'; done | sort > "$CACHE/defs.txt"
rc=0
dups=$(awk '{print $1}' "$CACHE/defs.txt" | uniq -d)
if [ -n "$dups" ]; then echo "símbolos fortes duplicados:"; echo "$dups" | while read -r s; do grep "^$s " "$CACHE/defs.txt"; done; rc=1; fi
awk '{print $1}' "$CACHE/defs.txt" | sort -u > "$CACHE/alldefs.txt"
for f in "$@"; do
    b=$(basename "$f" .c)
    u=$(nm -u "$CACHE/o/$b.o" | awk '{print $2}' | sort -u | comm -23 - "$CACHE/alldefs.txt" |
        grep -vxE '(sin|cos|tan|atan2?|asin|acos|exp|exp2|log|log2|log10|pow|sqrt|cbrt|floor|ceil|round|trunc|fabs|fmod|hypot|ldexp|frexp|modf|sinh|cosh|tanh|expm1|log1p|rint|lrint|nearbyint|copysign|fmin|fmax)f?|objc_msgSend.*|_objc_msgSend.*')
    [ -n "$u" ] && { echo "indefinidos em $b:"; echo "$u"; rc=1; }
done
[ $rc = 0 ] && echo "libsystem-symcheck: ok ($(wc -l < "$CACHE/alldefs.txt") símbolos, nenhum duplicado)"
exit $rc
