#!/bin/bash
# Coalescência de definições weak do carregador (kernel/compat/macho.c, coalesced_export/run_weak_bind) nos dois
# slices, no machotest do Linux: userland/tests-weakdef (executável + @rpath/libweak.dylib). Critério:
# "weakdef-teste: N/N ok" com N == N em x86_64, arm64 interp e arm64 jit.
#   tools/weakdef.sh [x86_64|arm64|ambos]    (o arm64 usa o runtime de tools/a64rt-testes.sh)
# Trabalho pesado neste PC: rode dentro de `flock /tmp/mos-pesado.lock nice -n 10` (/home/thiago/OpenBot/Shared/mos-regras-de-ram.md).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-$(command -v zig || true)}
[ -n "$ZIG" ] || { echo "zig: defina MOS_ZIG" >&2; exit 1; }
QUAIS=${1:-ambos}
SRC=$ROOT/userland/tests-weakdef
LIBX86=${MOS_LIBSYSTEM:-$ROOT/build/libSystem.mos}
[ -f "$LIBX86" ] || { echo "falta $LIBX86 (tools/build.sh, ou MOS_LIBSYSTEM=<libSystem.mos>)" >&2; exit 1; }
W=$ROOT/build/weakdef
mkdir -p "$W"
MT=$W/machotest
"$ZIG" cc -target x86_64-linux-gnu -O2 -std=gnu11 -DMOS_HOST_TEST -fno-strict-aliasing \
    -I"$ROOT/kernel/include" -I"$ROOT/tools" "$ROOT/tools/machotest.c" "$ROOT/tools/host_unix.c" "$ROOT/tools/host_gpu.c" \
    "$ROOT/tools/host_qos.c" "$ROOT/tools/host_id.c" "$ROOT/tools/host_bootstrap.c" "$ROOT/tools/host_enter.S" \
    "$ROOT/kernel/compat/macho.c" -o "$MT" -pthread || { echo "machotest: build FALHOU"; exit 1; }

rc=0
caso() {  # caso <triplo> <rótulo> <vfs> [VAR=valor]...
    local t=$1 r=$2 v=$3
    shift 3
    mkdir -p "$v/bin"
    "$ZIG" cc -target "$t" -O1 -dynamiclib "$SRC/libweak.c" -o "$v/bin/libweak.dylib" -Wl,-install_name,@rpath/libweak.dylib &&
        "$ZIG" cc -target "$t" -O1 "$SRC/main.c" "$v/bin/libweak.dylib" -Wl,-rpath,@executable_path -o "$v/bin/weakdef" ||
        { echo "weakdef $r: build FALHOU"; rc=1; return; }
    env "$@" MACHOTEST_PATH=/bin/weakdef timeout 120 "$MT" "$LIBX86" "$v" "$v/bin/weakdef" > "$W/$r.log" 2>&1
    local linha
    linha=$(grep -aE 'weakdef-teste: [0-9]+/[0-9]+ ok' "$W/$r.log" | tail -1)
    if [[ $linha =~ ([0-9]+)/([0-9]+) ]] && [ "${BASH_REMATCH[1]}" = "${BASH_REMATCH[2]}" ]; then echo "ok   $r: $linha"
    else echo "FALHA $r: ${linha:-sem resumo}"; grep -aE 'FALHOU|macho' "$W/$r.log" | head -6; rc=1; fi
}

if [ "$QUAIS" != arm64 ]; then V=$W/vfs-x86; rm -rf "$V"; caso x86_64-macos x86_64 "$V"; fi
if [ "$QUAIS" != x86_64 ]; then
    A=$ROOT/build/a64rt-test/vfs/Sistema/Biblioteca/a64
    [ -f "$A/a64rt.mos" ] && [ -f "$A/libSystem.mos" ] || { echo "falta o runtime arm64: rode tools/a64rt-testes.sh" >&2; exit 1; }
    V=$W/vfs-arm64; rm -rf "$V"; mkdir -p "$V/Sistema/Biblioteca"; cp -r "$A" "$V/Sistema/Biblioteca/"
    for e in interp jit; do caso aarch64-macos "arm64-$e" "$V" MOS_A64_ENGINE=$e; done
fi
exit $rc
