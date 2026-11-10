#!/bin/bash
# Roda no host o comportamento de String, Array e Dictionary do Foundation.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${1:-$(command -v zig)}
OUT=${2:-/tmp/mos-string-fdn}
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-linux-gnu -O0 -DFDN_HOST -I"$ROOT/userland/libsystem" \
    -c "$ROOT/userland/libsystem/foundation_swift_data.c" -o "$OUT/foundation_swift_data.o"
"$ZIG" cc -target x86_64-linux-gnu -O0 -I"$ROOT/userland/libsystem" \
    -c "$ROOT/userland/libsystem/string_fdn.c" -o "$OUT/string_fdn.o"
"$ZIG" cc -target x86_64-linux-gnu -O0 \
    "$OUT/foundation_swift_data.o" "$OUT/string_fdn.o" "$ROOT/tools/string_fdn_test.c" \
    -o "$OUT/string_fdn_test"
"$OUT/string_fdn_test"
"$ZIG" cc -target x86_64-macos -O0 -c "$ROOT/userland/libsystem/string_fdn.c" -o "$OUT/string_fdn_x64.o"
if ! strings "$OUT/string_fdn_x64.o" | grep -q '\$sSS10FoundationE10contentsOfSSAA3URLVh_tKcfC'; then
    echo "string-fdn: objeto x86_64 sem contentsOf" >&2
    exit 1
fi
echo "string-fdn: símbolo no objeto x86_64"
