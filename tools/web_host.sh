#!/usr/bin/env bash
# mOS — teste de host do motor web (kernel/web + kernel/gfx) no Linux, sem imagem nem QEMU.
# uso: tools/web_host.sh [pasta de saída]   (WEB_HOST_OUT=<pasta> grava os quadros em PPM)
# XCFLAGS="-fsanitize=address,undefined" tools/web_host.sh /tmp/x   roda com os sanitizadores
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-/tmp/web_host}"
mkdir -p "$OUT/obj"
CC="${CC:-gcc}"
# cópia dos headers do kernel em que cli/sti/str (privilegiadas em ring 3) viram nada: no host há um núcleo só
rm -rf "$OUT/inc" && cp -r "$ROOT/kernel/include" "$OUT/inc"
sed -i -e 's/__asm__ volatile("cli")/(void)0/' -e 's/__asm__ volatile("sti")/(void)0/' \
       -e 's/pushfq; popq %0; cli/pushfq; popq %0/' -e 's/__asm__ volatile("str %0" : "=r"(s))/s = 0/' "$OUT/inc/kernel.h"
KF=(-std=gnu11 -O1 -g -w -ffreestanding -fno-builtin -mno-red-zone -fno-pie -I"$OUT/inc" -I"$ROOT")
objs=()
for f in $(ls "$ROOT"/kernel/web/*.c | grep -v /webview.c) \
         "$ROOT"/kernel/gfx/gfx.c "$ROOT"/kernel/gfx/font.c "$ROOT/tools/web_host.c" $(ls "$ROOT"/tools/web_host_*.c 2>/dev/null | grep -v shim); do
    o="$OUT/obj/$(basename "$f" .c).o"
    if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ -n "$(find "$ROOT/kernel/include" "$ROOT/kernel/web" -newer "$o" -name '*.h' | head -1)" ]; then
        "$CC" ${XCFLAGS:-} "${KF[@]}" -c "$f" -o "$o"
    fi
    objs+=("$o")
done
# fontes Inter embutidas como no kernel (kernel/arch/assets.S)
sed 's/\.section \.rdata,"dr"/.section .rodata/' "$ROOT/kernel/arch/assets.S" > "$OUT/obj/assets.S"
( cd "$ROOT" && "$CC" -c "$OUT/obj/assets.S" -o "$OUT/obj/assets.o" )
"$CC" ${XCFLAGS:-} -std=gnu11 -O1 -g -fno-pie -c "$ROOT/tools/web_host_shim.c" -o "$OUT/obj/shim.o"
"$CC" ${XCFLAGS:-} -no-pie -o "$OUT/web_host" "${objs[@]}" "$OUT/obj/assets.o" "$OUT/obj/shim.o" -lm
# referências WebP decodificadas pelo libwebp do Pillow (sem Pillow, os casos WebP avisam e não contam)
python3 -I "$ROOT/tools/testdata/web/webp/ref.py" "$ROOT/tools/testdata/web/webp" "$OUT/webp-ref" 2>/dev/null || echo "aviso: sem Pillow, sem referências WebP"
WEB_HOST_DIR="$ROOT/tools/testdata/web" WEB_HOST_REF="$OUT/webp-ref" "$OUT/web_host"
