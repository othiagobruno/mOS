#!/bin/bash
# Testes de host do ScreenCaptureKit: núcleo (sck_core.c) e as classes (screencapturekit.c) com compositor falso.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
T=$(mktemp -d)
L="$ROOT/userland/libsystem"
gcc -std=gnu11 -O1 -Wall -o "$T/core" "$ROOT/tools/sck_core_test.c" "$L/sck_core.c"
"$T/core"
gcc -std=gnu11 -O1 -w -c -o "$T/t.o" "$ROOT/tools/sck_host_test.c"
gcc -std=gnu11 -O1 -w -DMOS_LIBSYS_SYSCALL -I "$L/kshim" -I "$L" -I "$ROOT/kernel/include" -fno-builtin -c -o "$T/s.o" "$L/screencapturekit.c"
gcc -std=gnu11 -O1 -w -fno-builtin -c -o "$T/k.o" "$L/sck_core.c"
gcc -std=gnu11 -O1 -w -fno-builtin -c -o "$T/c.o" "$L/avcap_conv.c"
gcc -o "$T/sck" "$T/t.o" "$T/s.o" "$T/k.o" "$T/c.o" -lpthread
timeout 60 "$T/sck"
rm -rf "$T"
