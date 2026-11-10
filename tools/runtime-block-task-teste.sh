#!/bin/bash
# Compila dispatch_block.c e swift_task.c no host e confere o "_" único do Mach-O.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${1:-$(command -v zig)}
OUT=${2:-/tmp/mos-runtime-block-task}
mkdir -p "$OUT"
"$ZIG" cc -target x86_64-linux-gnu -O2 -DMOS_HOST_TEST \
    -c "$ROOT/userland/libsystem/dispatch_block.c" -o "$OUT/dispatch_block.o"
"$ZIG" cc -target x86_64-linux-gnu -O2 -DMOS_HOST_TEST \
    -c "$ROOT/userland/libsystem/swift_task.c" -o "$OUT/swift_task.o"
"$ZIG" cc -target x86_64-linux-gnu -O2 \
    "$OUT/dispatch_block.o" "$OUT/swift_task.o" "$ROOT/tools/runtime_block_task_test.c" \
    -o "$OUT/runtime_block_task_test"
"$OUT/runtime_block_task_test"

"$ZIG" cc -target x86_64-macos -O2 -c "$ROOT/userland/libsystem/swift_task.c" -o "$OUT/swift_task_x64.o"
"$ZIG" cc -target aarch64-macos -O2 -c "$ROOT/userland/libsystem/swift_task.c" -o "$OUT/swift_task_arm64.o"
"$ZIG" cc -target x86_64-macos -O2 -c "$ROOT/userland/libsystem/dispatch_block.c" -o "$OUT/dispatch_block_x64.o"
python3 - "$OUT" <<'PY'
import struct, sys, pathlib
out = pathlib.Path(sys.argv[1])
want = [
    "_swift_task_isCancelled",
    "_swift_task_cancel",
    "_swift_task_getCurrent",
    "_swift_task_getMainExecutor",
    "_swift_task_enqueueMainExecutor",
    "_swift_job_run",
    "_swift_task_isCurrentExecutor",
    "_dispatch_block_create",
    "_dispatch_block_wait",
    "_dispatch_block_cancel",
    "_dispatch_block_notify",
    "_dispatch_block_perform",
    "_dispatch_block_testcancel",
]
def syms(path):
    data = path.read_bytes()
    magic = struct.unpack_from("<I", data, 0)[0]
    if magic != 0xFEEDFACF:
        raise SystemExit(f"{path} não é Mach-O 64")
    ncmds, = struct.unpack_from("<I", data, 16)
    off = 32
    symoff = stroff = 0
    nsyms = 0
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, off)
        if cmd == 0x2:
            symoff, nsyms, stroff = struct.unpack_from("<III", data, off + 8)
        off += cmdsize
    names = []
    for i in range(nsyms):
        n_strx = struct.unpack_from("<I", data, symoff + i * 16)[0]
        end = data.index(b"\0", stroff + n_strx)
        names.append(data[stroff + n_strx:end].decode())
    return names
for obj, label in ((out / "swift_task_x64.o", "x64"), (out / "swift_task_arm64.o", "arm64"), (out / "dispatch_block_x64.o", "bloco")):
    got = set(syms(obj))
    if label == "bloco":
        need = [s for s in want if s.startswith("_dispatch_block_")]
    else:
        need = [s for s in want if s.startswith("_swift_")]
    missing = [s for s in need if s not in got]
    double = [s for s in got if s.startswith("__swift_") or s.startswith("__dispatch_block_")]
    if missing or double:
        raise SystemExit(f"{obj.name}: falta {missing} extra {double}")
print("mach-o: um underscore nos símbolos de swift_task e dispatch_block")
PY
echo "runtime bloco/task: símbolos em arm64 e x86_64"
