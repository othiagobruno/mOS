#!/bin/bash
# dlopen/dlsym/dlclose do carregador nos dois slices, no machotest do Linux (o mesmo userland/tests-dlopen do
# tools/build-dltest.ps1): DLTest.app/Contents/MacOS/{DLTest, Plugin.bundle (MH_BUNDLE, @rpath/libdep.dylib), libdep.dylib}.
#   tools/dltest.sh [x86_64|arm64|ambos]      (padrão: ambos)
# O x86_64 roda direto no machotest; o arm64 pelo tradutor (a64rt.mos e libSystem arm64 em /Sistema/Biblioteca/a64,
# montados por tools/a64rt-testes.sh em build/a64rt-test). Critério: "dlopen-teste: N/N ok" com N == N.
# Trabalho pesado neste PC: rode dentro de `flock /tmp/mos-pesado.lock nice -n 10` (/home/thiago/OpenBot/Shared/mos-regras-de-ram.md).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-$(command -v zig || true)}
[ -n "$ZIG" ] || { echo "zig: defina MOS_ZIG" >&2; exit 1; }
QUAIS=${1:-ambos}
SRC=$ROOT/userland/tests-dlopen
INC=-I$ROOT/userland/sdk/include
LIBX86=${MOS_LIBSYSTEM:-$ROOT/build/libSystem.mos}
[ -f "$LIBX86" ] || { echo "falta $LIBX86 (tools/build.sh, ou MOS_LIBSYSTEM=<libSystem.mos>)" >&2; exit 1; }
W=$ROOT/build/dltest
mkdir -p "$W"
MT=$W/machotest
"$ZIG" cc -target x86_64-linux-gnu -O2 -std=gnu11 -DMOS_HOST_TEST -fno-strict-aliasing \
    -I"$ROOT/kernel/include" -I"$ROOT/tools" "$ROOT/tools/machotest.c" "$ROOT/tools/host_unix.c" "$ROOT/tools/host_gpu.c" \
    "$ROOT/tools/host_qos.c" "$ROOT/tools/host_id.c" "$ROOT/tools/host_bootstrap.c" "$ROOT/tools/host_enter.S" \
    "$ROOT/kernel/compat/macho.c" -o "$MT" -pthread || { echo "machotest: build FALHOU"; exit 1; }

app() {  # app <triplo zig> <raiz do VFS>
    local t=$1 vfs=$2 out=$2/DLTest.app/Contents/MacOS
    mkdir -p "$out"
    "$ZIG" cc -target "$t" -O1 -dynamiclib "$SRC/libdep.c" -o "$out/libdep.dylib" -Wl,-install_name,@rpath/libdep.dylib || return 1
    # o linker do zig não tem -bundle: liga como dylib e troca o filetype para MH_BUNDLE (8), como os .node do ld64
    "$ZIG" cc -target "$t" -O1 -shared $INC -x objective-c "$SRC/plugin.m" -x none "$out/libdep.dylib" -o "$out/Plugin.bundle" \
        -Wl,-rpath,@loader_path -Wl,-undefined,dynamic_lookup || return 1
    printf '\x08' | dd of="$out/Plugin.bundle" bs=1 seek=12 count=1 conv=notrunc status=none
    "$ZIG" cc -target "$t" -O1 $INC -x objective-c "$SRC/main.m" -o "$out/DLTest" -Wl,-undefined,dynamic_lookup || return 1
    printf '<?xml version="1.0" encoding="UTF-8"?>\n<plist version="1.0"><dict><key>CFBundleName</key><string>DLTest</string><key>CFBundleIdentifier</key><string>com.mos.dltest</string><key>CFBundleExecutable</key><string>DLTest</string></dict></plist>\n' \
        > "$vfs/DLTest.app/Contents/Info.plist"
}

rc=0
roda() {  # roda <rótulo> <raiz do VFS> [VAR=valor]...
    local nome=$1 vfs=$2
    shift 2
    env "$@" MACHOTEST_PATH=/DLTest.app/Contents/MacOS/DLTest timeout 300 "$MT" "$LIBX86" "$vfs" "$vfs/DLTest.app/Contents/MacOS/DLTest" \
        > "$W/$nome.log" 2>&1
    local linha
    linha=$(grep -aE 'dlopen-teste: [0-9]+/[0-9]+ ok' "$W/$nome.log" | tail -1)
    if [[ $linha =~ ([0-9]+)/([0-9]+) ]] && [ "${BASH_REMATCH[1]}" = "${BASH_REMATCH[2]}" ]; then echo "ok   $nome: $linha"
    else echo "FALHA $nome: ${linha:-sem resumo}"; grep -aE 'FALHOU|FATAL|macho|exceção' "$W/$nome.log" | head -8; rc=1; fi
}

if [ "$QUAIS" != arm64 ]; then
    V=$W/vfs-x86; rm -rf "$V"; mkdir -p "$V"
    app x86_64-macos "$V" || { echo "dltest x86_64: build FALHOU"; exit 1; }
    roda dltest-x86_64 "$V"
fi
if [ "$QUAIS" != x86_64 ]; then
    A=$ROOT/build/a64rt-test/vfs/Sistema/Biblioteca/a64
    [ -f "$A/a64rt.mos" ] && [ -f "$A/libSystem.mos" ] || { echo "falta o runtime arm64: rode tools/a64rt-testes.sh" >&2; exit 1; }
    V=$W/vfs-arm64; rm -rf "$V"; mkdir -p "$V/Sistema/Biblioteca"
    cp -r "$ROOT/build/a64rt-test/vfs/Sistema/Biblioteca/a64" "$V/Sistema/Biblioteca/"
    app aarch64-macos "$V" || { echo "dltest arm64: build FALHOU"; exit 1; }
    for e in interp jit; do roda "dltest-arm64-$e" "$V" MOS_A64_ENGINE=$e; done
fi
exit $rc
