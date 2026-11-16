#!/bin/sh
# Teste de host da libdispatch do mOS (Linux): compila userland/libsystem/dispatch*.c com -DMOS_HOST_TEST e roda.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/mos-dispatch-hosttest
L="$root/userland/libsystem"
${CC:-cc} -DMOS_HOST_TEST -O1 -g -Wall -Wno-unused-function -pthread -I"$L" \
    "$L/dispatch.c" "$L/dispatch_source.c" "$L/dispatch_data.c" "$L/dispatch_io.c" "$L/dispatch_block.c" \
    "$root/tools/dispatch_hosttest.c" -o "$out"
exec timeout 120 "$out"
