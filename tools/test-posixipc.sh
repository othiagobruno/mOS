#!/bin/bash
# mOS — IPC POSIX nomeado e chamadas BSD no harness do Linux (docs/testes-linux.md). Compila userland/tests-posixipc/main.c
# como Mach-O x86_64 e roda no build/machotest com o build/libSystem.mos. O placar é "posixipc: N/N ok".
# O mesmo build/posixipcteste roda no Darling (darling shell ./posixipcteste) para comparar as linhas.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
OUT="$ROOT/build"
"$ZIG" cc -target x86_64-macos -O1 -Wno-deprecated-declarations "$ROOT/userland/tests-posixipc/main.c" -o "$OUT/posixipcteste"
RAIZ=$(mktemp -d)
trap 'rm -rf "$RAIZ"' EXIT
timeout 60 "$OUT/machotest" "$OUT/libSystem.mos" "$RAIZ" "$OUT/posixipcteste"
