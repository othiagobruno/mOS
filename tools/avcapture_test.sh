#!/bin/bash
# Testes de host da captura: conversão de pixels (avcap_conv.c) e AVCapture com câmera UVC falsa (avcapture.c).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
T=$(mktemp -d)
gcc -std=gnu11 -O1 -Wall -o "$T/conv" "$ROOT/tools/avcap_conv_test.c" "$ROOT/userland/libsystem/avcap_conv.c"
"$T/conv"
gcc -std=gnu11 -O1 -w -c -o "$T/t.o" "$ROOT/tools/avcapture_host_test.c"
gcc -std=gnu11 -O1 -w -DMOS_LIBSYS_SYSCALL -I "$ROOT/userland/libsystem/kshim" -I "$ROOT/userland/libsystem" -I "$ROOT/kernel/include" \
    -fno-builtin -c -o "$T/a.o" "$ROOT/userland/libsystem/avcapture.c"
gcc -std=gnu11 -O1 -w -fno-builtin -c -o "$T/c.o" "$ROOT/userland/libsystem/avcap_conv.c"
gcc -o "$T/avcap" "$T/t.o" "$T/a.o" "$T/c.o" -lpthread
timeout 60 "$T/avcap"
rm -rf "$T"
