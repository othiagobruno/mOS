#!/bin/bash
# mOS — semáforos Mach no harness do Linux (docs/testes-linux.md). Compila userland/tests-machsem/main.c
# como Mach-O x86_64 e roda no build/machotest com o build/libSystem.mos. O placar é "machsem: N/N ok".
# O mesmo build/machsemteste roda no Darling (darling shell ./machsemteste) para comparar as linhas.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
OUT="$ROOT/build"
"$ZIG" cc -target x86_64-macos -O1 "$ROOT/userland/tests-machsem/main.c" -o "$OUT/machsemteste"
RAIZ=$(mktemp -d)
trap 'rm -rf "$RAIZ"' EXIT
timeout 60 "$OUT/machotest" "$OUT/libSystem.mos" "$RAIZ" "$OUT/machsemteste"
