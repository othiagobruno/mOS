#!/usr/bin/env bash
# mOS — teste de host do JavaScriptCore.framework (userland/frameworks/JavaScriptCore): o motor do mOS e a API C
# compilados para o Linux, com um programa que usa a API como um app usaria.
# uso: tools/jsc_host.sh [pasta de saída]   (XCFLAGS="-fsanitize=address,undefined" para os sanitizadores)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-/tmp/jsc_host}"
mkdir -p "$OUT/obj"
CC="${CC:-gcc}"
rm -rf "$OUT/inc" && cp -r "$ROOT/kernel/include" "$OUT/inc"
sed -i -e 's/__asm__ volatile("cli")/(void)0/' -e 's/__asm__ volatile("sti")/(void)0/' \
       -e 's/pushfq; popq %0; cli/pushfq; popq %0/' -e 's/__asm__ volatile("str %0" : "=r"(s))/s = 0/' "$OUT/inc/kernel.h"
KF=(-std=gnu11 -O1 -g -w -fno-builtin -fno-pie -I"$OUT/inc" -I"$ROOT")
objs=()
for f in $(ls "$ROOT"/kernel/web/js_*.c | grep -v /js_dom.c) "$ROOT/kernel/web/util.c" "$ROOT/userland/frameworks/JavaScriptCore/jsc_api.c"; do
    o="$OUT/obj/$(basename "$f" .c).o"
    "$CC" ${XCFLAGS:-} "${KF[@]}" -c "$f" -o "$o"
    objs+=("$o")
done
"$CC" ${XCFLAGS:-} -std=gnu11 -O1 -g -fno-pie -I"$ROOT/userland/frameworks/JavaScriptCore" -c "$ROOT/tools/jsc_host_test.c" -o "$OUT/obj/test.o"
"$CC" ${XCFLAGS:-} -no-pie -o "$OUT/jsc_host" "$OUT/obj/test.o" "${objs[@]}" -lm -lpthread
"$OUT/jsc_host"
