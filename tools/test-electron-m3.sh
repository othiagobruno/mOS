#!/bin/bash
# mOS — teste de host dos seletores do marco M3 dos apps Electron (tools/testdata/electron-m3/main.c).
#   tools/test-electron-m3.sh <libSystem.mos> <machotest>
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LIBSYS=${1:?uso: test-electron-m3.sh <libSystem.mos> <machotest>}
MT=${2:?uso: test-electron-m3.sh <libSystem.mos> <machotest>}
ZIG=${MOS_ZIG:-zig}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/vfs/bin" "$T/vfs/Users/Visitante"
"$ZIG" cc -target x86_64-macos -O1 "$ROOT/tools/testdata/electron-m3/main.c" -o "$T/vfs/bin/m3" -Wl,-undefined,dynamic_lookup
MACHOTEST_PATH=/bin/m3 HOME=/Users/Visitante timeout 60 "$MT" "$LIBSYS" "$T/vfs" "$T/vfs/bin/m3" 2>"$T/err" | tee "$T/out"
grep -q '^electron-m3: \([0-9]*\)/\1 ok$' "$T/out" || { echo "--- stderr"; tail -20 "$T/err"; exit 1; }
