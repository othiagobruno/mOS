#!/bin/sh
# Teste de host do runtime de blocos do libSystem (objc.c, parte "blocks") com a libdispatch (dispatch*.c), no Linux,
# com blocos de verdade (clang -fblocks). Repete os cenários de userland/tests-dispatch que dependem de __block
# copiada de dentro de outro bloco.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
L="$root/userland/libsystem"
out=${TMPDIR:-/tmp}/mos-blocks-hosttest
mkdir -p "$out"
python3 - "$L/objc.c" "$out/blocks_rt.c" <<'PY'
import sys
s = open(sys.argv[1]).read()
a = s.index("/* ================================================================ blocks */")
b = s.index("static id block_copy_m(")
hdr = """#include <stdlib.h>
#include <string.h>
typedef unsigned long long u64;
typedef struct { void *p[8]; } cx_class_t;
typedef void *id;
id objc_retain(id o) { return o; }
void objc_release(id o) { (void)o; }
"""
open(sys.argv[2], "w").write(hdr + s[a:b])
PY
cc_=${CC:-clang}
$cc_ -fblocks -DMOS_HOST_TEST -O1 -g -w -pthread -I"$L" \
    "$out/blocks_rt.c" "$L/dispatch.c" "$L/dispatch_source.c" "$L/dispatch_data.c" "$L/dispatch_io.c" "$L/dispatch_block.c" \
    "$root/tools/blocks_hosttest.c" -o "$out/blocks_hosttest"
exec timeout 60 "$out/blocks_hosttest"
