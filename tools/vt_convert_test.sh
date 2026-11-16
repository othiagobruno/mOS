#!/bin/bash
# Teste de host da saída com escala e formatos extras do VTDecompressionSession (sem OpenH264 nem ffmpeg).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
L="$ROOT/userland/libsystem"
T=$(mktemp -d)
gcc -std=gnu11 -O1 -w -fno-builtin -I "$L/kshim" -I "$L" -I "$ROOT/kernel/include" -c "$L/vscode_media.c" -o "$T/vm.o"
gcc -std=gnu11 -O1 -w -c "$L/sck_core.c" -o "$T/k.o"
gcc -std=gnu11 -O1 -w -fno-builtin -c "$L/avcap_conv.c" -o "$T/c.o"
gcc -std=gnu11 -O1 -w -c "$ROOT/tools/vt_convert_test.c" -o "$T/t.o"
gcc -o "$T/vt" "$T/t.o" "$T/vm.o" "$T/k.o" "$T/c.o" -lm -lpthread -Wl,--unresolved-symbols=ignore-all
"$T/vt"
rm -rf "$T"
