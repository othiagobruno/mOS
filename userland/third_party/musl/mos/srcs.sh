#!/bin/bash
# Lista as fontes da libm do musl para uma arquitetura (x86_64 ou aarch64), como o Makefile do musl:
# o arquivo de src/math/<arch>/ com o mesmo nome-base substitui o genérico. Uso: srcs.sh <dir-musl> <arch>
M=$1; A=$2
declare -A seen
for f in "$M"/src/math/"$A"/*.c "$M"/src/math/"$A"/*.s; do
    [ -f "$f" ] || continue
    b=$(basename "$f"); seen[${b%.*}]=1; echo "$f"
done
for f in "$M"/src/math/*.c; do
    b=$(basename "$f" .c)
    [ -n "${seen[$b]:-}" ] && continue
    echo "$f"
done
echo "$M/mos/aliases.c"
echo "$M/mos/apple.c"
