#!/bin/bash
# Teste de host dos filtros do CoreImage (coreimage_cpu.c) e do teste antigo (tools/test-coreimage.c). Leve.
set -u
R=$(cd "$(dirname "$0")/.." && pwd); T=${CI_TMP:-/tmp/ci-test}; mkdir -p "$T"
C="$R/userland/frameworks/CoreImage"
gcc -O1 -Wall -I"$C" "$R/tools/test-coreimage-filtros.c" "$C/coreimage_cpu.c" -lm -Wl,--unresolved-symbols=ignore-all -o "$T/f" || exit 1
gcc -O1 -w -I"$C" -DIIO_HOST -I"$R/userland/libsystem" "$R/tools/test-coreimage.c" "$C/coreimage_cpu.c" "$R/userland/libsystem/imageio.c" -lz -lm -Wl,--unresolved-symbols=ignore-all -o "$T/o"   # sem o WebP (o teste não usa) || exit 1
"$T/f"; a=$?
"$T/o" > "$T/o.txt" 2>&1; b=$?; grep -c "^ok" "$T/o.txt" | sed 's/^/teste antigo: /; s/$/ ok/'; grep FALHOU "$T/o.txt"
exit $((a | b))
