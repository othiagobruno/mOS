#!/usr/bin/env bash
# mOS — teste de host do PAC (userland/libsystem/cfpac.c) com o motor JS do JavaScriptCore.framework do mOS
# (kernel/web/js_*.c + jsc_api.c, montados como em tools/jsc_host.sh). uso: tools/pac_host.sh [pasta]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-/tmp/pac_host}"
mkdir -p "$OUT/obj"
CC="${CC:-gcc}"
rm -rf "$OUT/inc" && cp -r "$ROOT/kernel/include" "$OUT/inc"
sed -i -e 's/__asm__ volatile("cli")/(void)0/' -e 's/__asm__ volatile("sti")/(void)0/' \
       -e 's/pushfq; popq %0; cli/pushfq; popq %0/' -e 's/__asm__ volatile("str %0" : "=r"(s))/s = 0/' "$OUT/inc/kernel.h"
KF=(-std=gnu11 -O1 -g -w -fno-builtin -fno-pie -I"$OUT/inc" -I"$ROOT")
objs=()
for f in $(ls "$ROOT"/kernel/web/js_*.c | grep -v /js_dom.c) "$ROOT/kernel/web/util.c" "$ROOT/userland/frameworks/JavaScriptCore/jsc_api.c"; do
    o="$OUT/obj/$(basename "$f" .c).o"
    "$CC" "${KF[@]}" -c "$f" -o "$o"
    objs+=("$o")
done
for f in "$ROOT"/userland/libsystem/mnet_*.c "$ROOT/userland/libsystem/cfpac.c" "$ROOT/tools/pac_host_test.c"; do
    o="$OUT/obj/m_$(basename "$f" .c).o"
    "$CC" -std=gnu11 -O1 -g -Wall -fno-pie -DMNET_HOST -DPAC_HOST -I"$ROOT/userland/libsystem" -c "$f" -o "$o"
    objs+=("$o")
done
"$CC" -no-pie -o "$OUT/pac_host" "${objs[@]}" -lm -lpthread -lz -l:libbrotlidec.so.1
"$OUT/pac_host"
