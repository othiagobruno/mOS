#!/bin/bash
# Roda no host os tipos-valor do overlay Swift do Foundation (fdn_tipos.c, fdn_meta.S).
# -O2 como no libSystem: o teste pega o que o otimizador fizer com a convenção swiftcc.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${1:-$(command -v zig)}
OUT=${2:-/tmp/mos-fdn-tipos}
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-linux-gnu -O2 -Wall -Wno-unused-parameter -I"$ROOT/userland/libsystem" \
    "$ROOT/userland/libsystem/fdn_tipos.c" "$ROOT/userland/libsystem/fdn_url.c" "$ROOT/userland/libsystem/fdn_local.c" "$ROOT/userland/libsystem/fdn_cset.c" "$ROOT/userland/libsystem/fdn_valores.c" "$ROOT/userland/libsystem/fdn_urlcomp.c" "$ROOT/userland/libsystem/fdn_meta.S" "$ROOT/tools/fdn_tipos_test.c" \
    -o "$OUT/fdn_tipos_test"
"$OUT/fdn_tipos_test"
