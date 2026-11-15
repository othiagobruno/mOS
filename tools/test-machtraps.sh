#!/bin/bash
# mOS — traps Mach de tempo e de portas no harness do Linux (docs/testes-linux.md). Compila userland/tests-machtraps/main.c
# como Mach-O x86_64 e roda no build/machotest com o build/libSystem.mos. O placar é "machtraps: N/N ok".
# O mesmo build/machtrapsteste roda no Darling (darling shell ./machtrapsteste) para comparar as linhas.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
OUT="$ROOT/build"
"$ZIG" cc -target x86_64-macos -O1 -Wno-deprecated-declarations "$ROOT/userland/tests-machtraps/main.c" -o "$OUT/machtrapsteste"
RAIZ=$(mktemp -d)
trap 'rm -rf "$RAIZ"' EXIT
timeout 60 "$OUT/machotest" "$OUT/libSystem.mos" "$RAIZ" "$OUT/machtrapsteste"
