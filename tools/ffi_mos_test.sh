#!/bin/bash
# Teste de host da libffi do mOS (userland/libsystem/ffi_mos.c). Uso: tools/ffi_mos_test.sh [dir]
# Só roda num host arm64 da Apple (macOS em Apple Silicon): é o PCS do libSystem arm64 do mOS. No x86_64 o teste
# não roda (o host Linux tem outra ABI de variádicas e de long double); sai 0 com o aviso.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/ffi-mos-test}
if [ "$(uname)-$(uname -m)" != Darwin-arm64 ]; then echo "ffi-mos: pulado (precisa de macOS arm64)"; exit 0; fi
ZIG=${ZIG:-$(command -v zig)}
NM=$(command -v llvm-nm || echo /opt/homebrew/opt/llvm/bin/llvm-nm)
OBJCOPY=$(command -v llvm-objcopy || echo /opt/homebrew/opt/llvm/bin/llvm-objcopy)
mkdir -p "$OUT"
"$ZIG" cc -target aarch64-macos -O1 -g0 -fno-stack-protector -fno-builtin -w -c "$ROOT/userland/libsystem/ffi_mos.c" -o "$OUT/ffi_mos.o"
"$ZIG" cc -target aarch64-macos -c "$ROOT/userland/libsystem/ffi_arm64.S" -o "$OUT/ffi_arm64.o"
ld -r "$OUT/ffi_mos.o" "$OUT/ffi_arm64.o" -o "$OUT/ffi_all.o"
"$NM" --defined-only -g "$OUT/ffi_all.o" | awk '{s=$NF; print s" _mos_"substr(s, 2)}' > "$OUT/redef.txt"
"$OBJCOPY" --redefine-syms="$OUT/redef.txt" "$OUT/ffi_all.o" "$OUT/ffi_mos_ren.o"
cc -O1 -o "$OUT/ffi_mos_test" "$ROOT/tools/ffi_mos_test.c" "$OUT/ffi_mos_ren.o"
"$OUT/ffi_mos_test"
