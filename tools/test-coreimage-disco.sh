#!/bin/bash
# Confere que mos_coreimage_real não está no libSystem e resolve no Mach-O do disco.
# Precisa de zig e de build/libSystem.mos (tools/build.sh). Não sobe a VM.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
LS=${1:-$ROOT/build/libSystem.mos}
if [[ ! -f "$LS" ]]; then
    echo "falta $LS" >&2
    exit 1
fi
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
bash "$ROOT/tools/build-coreimage.sh" "$ZIG" "$T"
FW="$T/System/Library/Frameworks/CoreImage.framework/Versions/A/CoreImage"
cp "$FW" "$T/libCoreImage.dylib"
"$ZIG" cc -target x86_64-macos -O2 "$ROOT/tools/ci-probe.c" -L"$T" -lCoreImage -o "$T/probe"
cc -O2 "$ROOT/tools/machocheck.c" -o "$T/machocheck"
mkdir -p "$T/Probe.app"
cp "$T/probe" "$T/Probe.app/probe"
"$T/machocheck" "$LS" "$T/Probe.app" "$T/sem-rel.txt" | tee "$T/sem.txt"
"$T/machocheck" --disco "$FW" "$LS" "$T/Probe.app" "$T/com-rel.txt" | tee "$T/com.txt"
echo "--- sem disco ---"
grep mos_coreimage_real "$T/sem-rel.txt"
echo "--- com disco ---"
grep -E 'CoreImage|Total:' "$T/com.txt"
grep -q 'CoreImage.framework .*1 / *0' "$T/com.txt"
if grep -q mos_coreimage_real "$T/com-rel.txt"; then
    echo "o disco não cobriu mos_coreimage_real" >&2
    exit 1
fi
echo "ok: o símbolo real resolve no Mach-O do CoreImage"
