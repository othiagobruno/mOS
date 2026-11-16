#!/bin/bash
# Erros de carga do carregador (kernel/compat/macho.c) nos dois slices, no machotest do Linux:
#   1. dependência forte ausente  -> recusa "requer a, b, c, d e mais N, que o mOS ainda não tem" (macho_load, rc 3)
#   2. dependência fraca ausente   -> o processo abre e o símbolo weak_import vale NULL
#   3. função que ninguém exporta  -> stub cx_unresolved: "símbolo não implementado no libSystem do mOS: <nome>", saída 127
#   4. constante que ninguém exporta -> GOT ligado à área zerada: lê 0/nil
#   tools/carga-erros.sh [x86_64|arm64|ambos]    (padrão: ambos; o arm64 usa o runtime de tools/a64rt-testes.sh)
# Trabalho pesado neste PC: rode dentro de `flock /tmp/mos-pesado.lock nice -n 10` (/home/thiago/OpenBot/Shared/mos-regras-de-ram.md).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-$(command -v zig || true)}
[ -n "$ZIG" ] || { echo "zig: defina MOS_ZIG" >&2; exit 1; }
QUAIS=${1:-ambos}
SRC=$ROOT/userland/tests-carga-erros
LIBX86=${MOS_LIBSYSTEM:-$ROOT/build/libSystem.mos}
[ -f "$LIBX86" ] || { echo "falta $LIBX86 (tools/build.sh, ou MOS_LIBSYSTEM=<libSystem.mos>)" >&2; exit 1; }
W=$ROOT/build/carga-erros
mkdir -p "$W"
MT=$W/machotest
"$ZIG" cc -target x86_64-linux-gnu -O2 -std=gnu11 -DMOS_HOST_TEST -fno-strict-aliasing \
    -I"$ROOT/kernel/include" -I"$ROOT/tools" "$ROOT/tools/machotest.c" "$ROOT/tools/host_unix.c" "$ROOT/tools/host_gpu.c" \
    "$ROOT/tools/host_qos.c" "$ROOT/tools/host_id.c" "$ROOT/tools/host_bootstrap.c" "$ROOT/tools/host_enter.S" \
    "$ROOT/kernel/compat/macho.c" -o "$MT" -pthread || { echo "machotest: build FALHOU"; exit 1; }

ok=0 total=0
confere() {  # confere <rótulo> <condição verdadeira?> [detalhe]
    total=$((total + 1))
    if [ "$2" = 1 ]; then ok=$((ok + 1)); echo "ok    $1"; else echo "FALHA $1${3:+: $3}"; fi
}

progs() {  # progs <triplo> <dir>: compila os programas do teste
    local t=$1 d=$2 libs=() i
    mkdir -p "$d/bin" "$d/libs"
    for i in 1 2 3 4 5; do
        "$ZIG" cc -target "$t" -O1 -dynamiclib "$SRC/lib.c" -o "$d/libs/libausente$i.dylib" \
            -Wl,-install_name,/opt/mos-ausente/libausente$i.dylib || return 1
        libs+=("$d/libs/libausente$i.dylib")
    done
    "$ZIG" cc -target "$t" -O1 "$SRC/dylib.c" "${libs[@]}" -o "$d/bin/dylib-forte" || return 1
    "$ZIG" cc -target "$t" -O1 "$SRC/dylib.c" -L"$d/libs" -Wl,-weak-lausente1 -o "$d/bin/dylib-fraca" || return 1
    "$ZIG" cc -target "$t" -O1 "$SRC/funcao.c" -o "$d/bin/funcao" -Wl,-undefined,dynamic_lookup || return 1
    "$ZIG" cc -target "$t" -O1 "$SRC/constante.c" -o "$d/bin/constante" -Wl,-undefined,dynamic_lookup || return 1
}

roda() {  # roda <vfs> <programa> <log> [VAR=valor]...: devolve o código de saída
    local vfs=$1 p=$2 log=$3
    shift 3
    env "$@" MACHOTEST_PATH=/bin/$p timeout 120 "$MT" "$LIBX86" "$vfs" "$vfs/bin/$p" > "$log" 2>&1
}

suite() {  # suite <rótulo> <vfs> [VAR=valor]...
    local r=$1 v=$2 rc
    shift 2
    roda "$v" dylib-forte "$W/$r-dylib-forte.log" "$@"; rc=$?
    confere "$r dependência forte ausente recusa a carga (rc $rc)" \
        "$([ $rc = 3 ] && grep -aq 'requer libausente1.dylib, libausente2.dylib, libausente3.dylib, libausente4.dylib e mais 1, que o mOS ainda não tem' "$W/$r-dylib-forte.log" && echo 1)" \
        "$(grep -a 'macho_load' "$W/$r-dylib-forte.log" | head -1)"
    roda "$v" dylib-fraca "$W/$r-dylib-fraca.log" "$@"; rc=$?
    confere "$r dependência fraca ausente abre com símbolo NULL (rc $rc)" \
        "$([ $rc = 0 ] && grep -aq 'dylib-teste: aberto, simbolo=NULL' "$W/$r-dylib-fraca.log" && echo 1)" \
        "$(head -3 "$W/$r-dylib-fraca.log" | tr '\n' ' ')"
    roda "$v" funcao "$W/$r-funcao.log" "$@"; rc=$?
    confere "$r função ausente imprime o nome e sai com 127 (rc $rc)" \
        "$([ $rc = 127 ] && grep -aq 'antes da chamada' "$W/$r-funcao.log" \
            && grep -aq 'símbolo não implementado no libSystem do mOS: _mos_funcao_inexistente' "$W/$r-funcao.log" \
            && ! grep -aq 'não devia voltar' "$W/$r-funcao.log" && echo 1)" \
        "$(head -3 "$W/$r-funcao.log" | tr '\n' ' ')"
    roda "$v" constante "$W/$r-constante.log" "$@"; rc=$?
    confere "$r constante ausente lê 0/nil (rc $rc)" \
        "$([ $rc = 0 ] && grep -aqE 'constante-teste: k=(0x0|\(nil\)|0) notif=(0x0|\(nil\)|0)' "$W/$r-constante.log" && echo 1)" \
        "$(head -3 "$W/$r-constante.log" | tr '\n' ' ')"
}

if [ "$QUAIS" != arm64 ]; then
    V=$W/vfs-x86; rm -rf "$V"; mkdir -p "$V"
    progs x86_64-macos "$V" || { echo "carga-erros x86_64: build FALHOU"; exit 1; }
    rm -rf "$V/libs"
    suite x86_64 "$V"
fi
if [ "$QUAIS" != x86_64 ]; then
    A=$ROOT/build/a64rt-test/vfs/Sistema/Biblioteca/a64
    [ -f "$A/a64rt.mos" ] && [ -f "$A/libSystem.mos" ] || { echo "falta o runtime arm64: rode tools/a64rt-testes.sh" >&2; exit 1; }
    V=$W/vfs-arm64; rm -rf "$V"; mkdir -p "$V/Sistema/Biblioteca"
    cp -r "$A" "$V/Sistema/Biblioteca/"
    progs aarch64-macos "$V" || { echo "carga-erros arm64: build FALHOU"; exit 1; }
    rm -rf "$V/libs"
    for e in interp jit; do suite "arm64-$e" "$V" MOS_A64_ENGINE=$e; done
fi
echo "carga-erros: $ok/$total ok"
[ $ok = $total ]
