#!/bin/bash
# Compila swift_rt_grok.c como o libSystem (sem wchar_t do hosted) e roda o once no host.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${1:-$(command -v zig)}
OUT=${2:-/tmp/mos-swift-rt-grok}
ZIGDIR=$(cd "$(dirname "$ZIG")" && pwd)
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
    -I"$ROOT/userland/libsystem/kshim" -I"$ROOT/userland/libsystem/fsinc" -I"$ROOT/kernel/include" \
    -isystem "$ZIGDIR/lib/include" \
    -c "$ROOT/userland/libsystem/swift_rt_grok.c" -o "$OUT/swift_rt_grok_fs.o"
"$ZIG" cc -target x86_64-linux-gnu -O2 \
    "$ROOT/userland/libsystem/swift_rt_grok.c" "$ROOT/userland/libsystem/swift_refcount.c" \
    "$ROOT/tools/swift_rt_grok_test.c" -o "$OUT/swift_rt_grok_test"
"$OUT/swift_rt_grok_test"
echo "swift-rt-grok: freestanding compilou"
