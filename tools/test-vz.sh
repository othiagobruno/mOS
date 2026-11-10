#!/bin/sh
# Teste de host do Virtualization.framework. Sem QEMU.
# 1) o comportamento (validate/start/disco/MAC) que os dois lados chamam
# 2) os dois slices exportam cada classe de tools/testdata/vz-imports.txt
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
OUT=${1:-/tmp/vz-host}
mkdir -p "$OUT"

cc -std=c11 -Wall -Wextra -O1 tools/vztest.c -o "$OUT/vztest"
"$OUT/vztest"

ZIG=${MOS_ZIG:-}
if [ -z "$ZIG" ]; then
    if command -v zig >/dev/null 2>&1; then ZIG=$(command -v zig); fi
fi
if [ -z "$ZIG" ] && [ -x /opt/zig-x86_64-linux-0.17.1/zig ]; then
    ZIG=/opt/zig-x86_64-linux-0.17.1/zig
fi
if [ -z "$ZIG" ]; then
    echo "test-vz: sem zig; o comportamento passou, os exports do Mach-O não foram conferidos" >&2
    exit 1
fi

one() {
    "$ZIG" cc -target "$1" -O2 -shared -x objective-c \
        -I"$ROOT/userland/sdk/include" -I"$ROOT/userland/frameworks/Virtualization" \
        -Wno-objc-root-class -Wno-objc-method-access \
        "$ROOT/userland/frameworks/Virtualization/Virtualization.m" -o "$2" \
        -Wl,-install_name,/System/Library/Frameworks/Virtualization.framework/Versions/A/Virtualization \
        -Wl,-undefined,dynamic_lookup
}
one x86_64-macos "$OUT/Virtualization.x86"
one aarch64-macos "$OUT/Virtualization.arm"

python3 - "$ROOT/tools/testdata/vz-imports.txt" "$OUT/Virtualization.x86" "$OUT/Virtualization.arm" <<'PY'
import pathlib, struct, sys
want = set()
for line in pathlib.Path(sys.argv[1]).read_text().splitlines():
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    if line.startswith("_"):
        line = line[1:]
    want.add(line)

def exports(path):
    b = pathlib.Path(path).read_bytes()
    def u32(o):
        return struct.unpack_from("<I", b, o)[0]
    if u32(0) != 0xFEEDFACF:
        raise SystemExit(path + " nao e Mach-O fino")
    ncmds, c = u32(16), 32
    symoff = nsyms = stroff = 0
    for _ in range(ncmds):
        cmd, csz = u32(c), u32(c + 4)
        if cmd == 0x2:
            symoff, nsyms, stroff = u32(c + 8), u32(c + 12), u32(c + 16)
        c += csz
    names = set()
    for i in range(nsyms):
        o = symoff + i * 16
        n_strx, typ, sect = struct.unpack_from("<IBB", b, o)
        if sect == 0 or (typ & 0x0E) == 0:
            continue
        end = b.find(b"\0", stroff + n_strx)
        name = b[stroff + n_strx:end].decode()
        if name.startswith("_"):
            name = name[1:]
        names.add(name)
    return names

rc = 0
for p in sys.argv[2:]:
    miss = sorted(want - exports(p))
    print("%s: %d classes, faltam %d" % (p, len(want), len(miss)))
    for m in miss:
        print(" ", m)
    if miss:
        rc = 1
sys.exit(rc)
PY
echo "test-vz: ok"
