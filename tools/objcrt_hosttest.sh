#!/bin/sh
# Teste de host do runtime ObjC do mOS (Linux): compila userland/libsystem/objc.c e objcrt.c para o host e roda
# tools/objcrt_hosttest.c. As dependências de fora do runtime (Foundation, dispatch, dyld) vêm de objcrt_hosttest_shims.c.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
out=${TMPDIR:-/tmp}/mos-objcrt-hosttest
L="$root/userland/libsystem"
mkdir -p "$out"
${CC:-cc} -w -O1 -fno-builtin -I"$L" -I"$L/kshim" -c "$L/objc.c" -o "$out/objc.o"
${CC:-cc} -w -O1 -fno-builtin -I"$L" -I"$L/kshim" -c "$L/objcrt.c" -o "$out/objcrt.o"
${CC:-cc} -w "$root/tools/objcrt_hosttest.c" "$root/tools/objcrt_hosttest_shims.c" "$out/objcrt.o" "$out/objc.o" -o "$out/t"
exec timeout 60 "$out/t"
