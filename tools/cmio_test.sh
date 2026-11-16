#!/bin/bash
# Teste de host do CoreMediaIO (userland/libsystem/cmio.c), com câmera UVC falsa.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
T=$(mktemp -d)
gcc -std=gnu11 -O1 -w -c -o "$T/t.o" "$ROOT/tools/cmio_host_test.c"
gcc -std=gnu11 -O1 -w -DMOS_LIBSYS_SYSCALL -I "$ROOT/userland/libsystem/kshim" -I "$ROOT/userland/libsystem" -I "$ROOT/kernel/include" \
    -fno-builtin -c -o "$T/c.o" "$ROOT/userland/libsystem/cmio.c"
gcc -o "$T/cmio" "$T/t.o" "$T/c.o" -lpthread -Wl,--unresolved-symbols=ignore-all
timeout 60 "$T/cmio"
rm -rf "$T"
