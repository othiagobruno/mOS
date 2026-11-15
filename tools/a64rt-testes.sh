#!/bin/bash
# Processo arm64 de ponta a ponta no harness Linux (tools/machotest.c), sem VM: o carregador do kernel monta o
# processo, o a64rt.mos traduz e o libSystem arm64 atende. Espelha tools/build-libsys-arm64-test.ps1 e a parte de
# userland/tests-a64rt do tools/build-a64rt-teste.ps1 (que no Windows roda na VM).
#   tools/a64rt-testes.sh                 # compila tudo e roda com MOS_A64_ENGINE=interp e =jit
# Precisa de build/libSystem.mos (x86_64, referência e processo x86 do posix_spawn) e gera build/libSystem-arm64.mos
# com tools/build-libsystem-arm64.sh. O harness não emula porta Mach, socketpair nem futex (docs/tradutor-arm64.md).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-$(command -v zig || true)}
[ -n "$ZIG" ] || { echo "zig: defina MOS_ZIG" >&2; exit 1; }
W=$ROOT/build/a64rt-test
VFS=$W/vfs
A64=$VFS/Sistema/Biblioteca/a64
T=$VFS/Sistema/Programas/a64teste
mkdir -p "$A64" "$T" "$VFS/tmp" "$VFS/Users/Visitante"
LIBX86=$ROOT/build/libSystem.mos
[ -f "$LIBX86" ] || { echo "falta build/libSystem.mos (tools/build.sh)" >&2; exit 1; }

"$ZIG" cc -target x86_64-linux-gnu -O2 -std=gnu11 -DMOS_HOST_TEST -fno-strict-aliasing \
    -I"$ROOT/kernel/include" -I"$ROOT/tools" "$ROOT/tools/machotest.c" "$ROOT/tools/host_unix.c" "$ROOT/tools/host_gpu.c" \
    "$ROOT/tools/host_qos.c" "$ROOT/tools/host_id.c" "$ROOT/tools/host_bootstrap.c" "$ROOT/tools/host_enter.S" \
    "$ROOT/kernel/compat/macho.c" -o "$W/machotest" -pthread || { echo "machotest: build FALHOU"; exit 1; }
mapfile -t A64SRC < <(find "$ROOT/userland/a64" "$ROOT/userland/a64rt" -type f \( -name '*.c' -o -name '*.S' \) | sort)
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -nostdlib -static -no-pie -fpie \
    -mcx16 -Wall -Wno-unused-function -Wno-unused-parameter -fno-strict-aliasing \
    -DA64_RUNTIME_MOS -DA64_HAVE_JIT -I"$ROOT/userland/a64" -I"$ROOT/userland/a64rt" \
    -Wl,--image-base=0x400041000000 -Wl,-e,_start "${A64SRC[@]}" -o "$A64/a64rt.mos" || { echo "a64rt: build FALHOU"; exit 1; }
"$ROOT/tools/build-libsystem-arm64.sh" > "$W/libsystem-arm64.log" 2>&1 || { tail -20 "$W/libsystem-arm64.log"; echo "libSystem-arm64: build FALHOU"; exit 1; }
cp -f "$ROOT/build/libSystem-arm64.mos" "$A64/libSystem.mos"
# @try/@catch: o __objc_personality_v0 do libSystem é o da libc++abi do disco (userland/libsystem/objc_exc.c)
if [ -f "$ROOT/build/libcxx-arm64/libc++.1.dylib" ]; then mkdir -p "$VFS/usr/lib"; cp -f "$ROOT/build/libcxx-arm64/libc++.1.dylib" "$VFS/usr/lib/"
else echo "aviso: sem build/libcxx-arm64/libc++.1.dylib (tools/build-libcxx.sh): os casos de @try/@catch abortam"; fi

printf '#include <stdio.h>\nint main(int c, char **v) { printf("hello %%s: argc=%%d\\n", v[0], c); return 7; }\n' > "$W/hello.c"
cc_mac() { "$ZIG" "$@" -Wl,-undefined,dynamic_lookup; }
cc_mac cc -target aarch64-macos -O1 "$W/hello.c" -o "$T/hello-arm64" &&
cc_mac cc -target x86_64-macos -O1 "$W/hello.c" -o "$T/hello-x86" &&
cc_mac cc -target x86_64-macos -O1 "$ROOT/userland/tests-a64rt/exec-from-x86.c" -o "$T/exec-from-x86" &&
cc_mac c++ -target aarch64-macos -O1 "$ROOT/userland/tests-a64rt/main.cpp" -o "$T/a64rt-teste" &&
cc_mac c++ -target aarch64-macos.12.0 -O1 -fblocks -fobjc-exceptions -std=c++17 -x objective-c++ \
    "$ROOT/userland/tests-a64libsys/abi.mm" -o "$T/libsys-abi-arm64" || { echo "testes: build FALHOU"; exit 1; }

rc=0
run() {  # run <rótulo> <motor> <binário no VFS> <padrão de sucesso> <tempo>
    local log=$W/$1-$2.log
    MOS_A64_ENGINE=$2 A64RT_TESTE_SPAWN="/Sistema/Programas/a64teste/hello-x86:7;/Sistema/Programas/a64teste/hello-arm64:7;/Sistema/Programas/a64teste/exec-from-x86:7" \
        MACHOTEST_PATH=$3 timeout "$5" "$W/machotest" "$LIBX86" "$VFS" "$VFS$3" > "$log" 2>&1
    local c=$?
    if grep -Eq "$4" "$log"; then echo "ok   $1 [$2] ($(grep -c 'ok ' "$log") verificações)"; return 0; fi
    echo "FALHA $1 [$2] (saída $c, $(grep -c 'ok ' "$log") ok)"; grep -E 'FALHA|FATAL|exceção sinal|não emulada' "$log" | head -8
    return 1
}
for e in interp jit; do
    run libsys-abi $e /Sistema/Programas/a64teste/libsys-abi-arm64 'libsys-abi: 0 falhas' 300 || rc=1
    # Informativo: o machotest Linux não emula alarm, fork, SIGPIPE nem a entrega do pendente ao desbloquear; o
    # teste para no read que espera o SIGALRM. A prova inteira (21/21) é a da VM (tools/build-a64rt-teste.ps1).
    run a64rt-teste $e /Sistema/Programas/a64teste/a64rt-teste 'a64rt-teste: 0 falhas' 60 || echo "     (informativo: limites do harness Linux; ver comentário no script)"
done
exit $rc
