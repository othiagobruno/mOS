#!/bin/bash
# Compila o overlay Darwin com swiftcall em x86_64 e arm64 e roda o teste no host.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${1:-$(command -v zig)}
OUT=${2:-/tmp/mos-darwin-swift}
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-linux-gnu -O2 -DMOS_HOST_TEST \
    -c "$ROOT/userland/libsystem/darwin_swift.c" -o "$OUT/darwin_swift.o"
"$ZIG" cc -target x86_64-linux-gnu -O2 \
    "$OUT/darwin_swift.o" "$ROOT/tools/darwin_swift_test.c" -o "$OUT/darwin_swift_test" -pthread
"$OUT/darwin_swift_test"
"$ZIG" cc -target aarch64-macos -O2 -c "$ROOT/userland/libsystem/darwin_swift.c" -o "$OUT/darwin_swift_arm64.o"
"$ZIG" cc -target x86_64-macos -O2 -c "$ROOT/userland/libsystem/darwin_swift.c" -o "$OUT/darwin_swift_x64.o"
for obj in "$OUT/darwin_swift_arm64.o" "$OUT/darwin_swift_x64.o"; do
    if ! strings "$obj" | grep -q '\$s6Darwin6openatys5Int32VAD_SPys4Int8VGADs6UInt16VtF'; then
        echo "darwin-swift: $obj sem o openat com mode" >&2
        exit 1
    fi
done
echo "darwin-swift: símbolos em arm64 e x86_64"
