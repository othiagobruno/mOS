#!/bin/bash
# Tradutor arm64 no QEMU do mOS a partir do Linux: o mesmo do tools/build-a64rt-teste.ps1 (userland/tests-a64rt e
# userland/tests-a64libsys/abi.mm com os dois motores, por jobs do launchd no boot), sobre a ESP de tools/build.sh.
#   tools/build.sh --out build/linux-a64
#   tools/a64rt-teste-vm.sh build/linux-a64 [segundos [binário arm64 [argumentos...]]]   # padrão 900 s de QEMU
# Com um binário (Mach-O arm64 ou universal), ele também roda forçado em arm64 (MOS_ARCH=arm64), como o -Real do .ps1;
# a saída dele fica no serial entre as linhas do launchd de com.mos.a64teste.real.
# Saída: build/linux-a64/a64-serial.log; o script resume as linhas "a64rt-teste: N falhas" e "libsys-abi: N falhas".
# KVM quando /dev/kvm abre para o usuário; senão TCG (bem mais lento).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(cd "${1:?uso: tools/a64rt-teste-vm.sh <dir do tools/build.sh> [segundos]}" && pwd)
SECS=${2:-900}
REAL=${3:-}
REALARGS=("${@:4}")
ZIG=${MOS_ZIG:-$(command -v zig || true)}
[ -n "$ZIG" ] || { echo "zig: defina MOS_ZIG" >&2; exit 1; }
[ -d "$OUT/esp/mOS" ] && [ -x "$OUT/mkimg" ] || { echo "rode antes: tools/build.sh --out $1" >&2; exit 1; }
# ESP enxuta: o boot copia \mOS\ para a RAM, e os apps oficiais (/Aplicativos, ~7 GB) não cabem na VM do teste.
# Hardlinks da ESP do build.sh, sem /Aplicativos; os programas e os jobs do teste só existem nesta cópia.
ESP=$OUT/esp-a64
rm -rf "$ESP"
cp -al "$OUT/esp" "$ESP"
rm -rf "$ESP/mOS/Aplicativos"
A64=$ESP/mOS/Sistema/Biblioteca/a64
[ -f "$A64/a64rt.mos" ] && [ -f "$A64/libSystem.mos" ] || { echo "a ESP não tem o a64rt.mos e o libSystem arm64" >&2; exit 1; }

# @try/@catch do arm64: o __objc_personality_v0 do libSystem é o da libc++abi do disco (objc_exc.c). O tools/build.sh
# põe só a fatia x86_64 em /usr/lib/libc++.1.dylib; aqui vai o universal (o build.ps1 faz o mesmo com o lipo-mos).
LIPO=$(command -v llvm-lipo || ls /usr/bin/llvm-lipo-* 2>/dev/null | head -1 || true)
CXX=$ESP/mOS/usr/lib/libc++.1.dylib
if [ -n "$LIPO" ] && [ -f "$ROOT/build/libcxx-arm64/libc++.1.dylib" ] && [ -f "$CXX" ]; then
    "$LIPO" -create "$CXX" "$ROOT/build/libcxx-arm64/libc++.1.dylib" -output "$OUT/libc++.universal.dylib" &&
        rm -f "$CXX" && cp "$OUT/libc++.universal.dylib" "$CXX"      # rm antes: o arquivo é hardlink da ESP do build.sh
else echo "aviso: sem libc++ universal (llvm-lipo e build/libcxx-arm64); o abi.mm aborta no @try/@catch"; fi

T=$ESP/mOS/Sistema/Programas/a64teste
mkdir -p "$T"
printf '#include <stdio.h>\nint main(int c, char **v) { printf("hello %%s: argc=%%d\\n", v[0], c); return 7; }\n' > "$OUT/a64rt-hello.c"
cc_mac() { "$ZIG" "$@" -Wl,-undefined,dynamic_lookup; }
cc_mac cc -target aarch64-macos -O1 "$OUT/a64rt-hello.c" -o "$T/hello-arm64" &&
cc_mac cc -target x86_64-macos -O1 "$OUT/a64rt-hello.c" -o "$T/hello-x86" &&
cc_mac cc -target x86_64-macos -O1 "$ROOT/userland/tests-a64rt/exec-from-x86.c" -o "$T/exec-from-x86" &&
cc_mac c++ -target aarch64-macos -O1 "$ROOT/userland/tests-a64rt/main.cpp" -o "$T/a64rt-teste" 2>/dev/null &&
cc_mac c++ -target aarch64-macos.12.0 -O1 -fblocks -fobjc-exceptions -std=c++17 -x objective-c++ \
    "$ROOT/userland/tests-a64libsys/abi.mm" -o "$T/libsys-abi-arm64" || { echo "testes: build FALHOU"; exit 1; }

LD=$ESP/mOS/Sistema/Biblioteca/LaunchDaemons
mkdir -p "$LD"
rm -f "$LD"/com.mos.a64teste.*
job() {  # job <label> <programa> [VAR=valor]...
    local label=$1 prog=$2 e="" kv
    shift 2
    for kv in "$@"; do e+="<key>${kv%%=*}</key><string>${kv#*=}</string>"; done
    cat > "$LD/$label.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>Label</key><string>$label</string>
<key>ProgramArguments</key><array><string>$prog</string></array>
<key>EnvironmentVariables</key><dict>$e</dict>
<key>RunAtLoad</key><true/>
</dict></plist>
EOF
}
# posix_spawn arm64->x86, arm64->arm64 e x86 que faz exec do arm64
SPAWN='/Sistema/Programas/a64teste/hello-x86:7;/Sistema/Programas/a64teste/hello-arm64:7;/Sistema/Programas/a64teste/exec-from-x86:7'
for m in interp jit; do
    job "com.mos.a64teste.$m" /Sistema/Programas/a64teste/a64rt-teste "MOS_A64_ENGINE=$m" "A64RT_TESTE_SPAWN=$SPAWN"
    job "com.mos.a64teste.abi-$m" /Sistema/Programas/a64teste/libsys-abi-arm64 "MOS_A64_ENGINE=$m"
done
if [ -n "$REAL" ]; then
    cp -f "$REAL" "$T/real"
    args=""; for x in "${REALARGS[@]}"; do args+="<string>$x</string>"; done
    job com.mos.a64teste.real /Sistema/Programas/a64teste/real MOS_ARCH=arm64 HOME=/Users/Visitante
    sed -i "s|<array><string>/Sistema/Programas/a64teste/real</string></array>|<array><string>/Sistema/Programas/a64teste/real</string>$args</array>|" "$LD/com.mos.a64teste.real.plist"
fi

OVT=$OUT/overlay-teste
rm -rf "$OVT"
mkdir -p "$OVT/mOS/Sistema/Biblioteca/Preferencias"
printf 'imagem de teste: o live abre na sessao do Visitante, sem o assistente\n' > "$OVT/mOS/Sistema/Biblioteca/Preferencias/com.mos.teste.sem-assistente"
BYTES=$(find "$ESP" -type f -printf '%s\n' | awk '{s+=$1} END {print s+0}')
MIB=$(( (BYTES * 5 / 4 + 1048575) / 1048576 )); MIB=$(( (MIB + 15) / 16 * 16 ))
IMG=$OUT/mOS-a64.img
rm -f "$IMG"
"$OUT/mkimg" "$IMG" "$ESP" "$MIB" --test-overlay "$OVT" > "$OUT/mkimg-a64.log" 2>&1 || { tail -5 "$OUT/mkimg-a64.log"; echo "mkimg FALHOU"; exit 1; }

SER=$OUT/a64-serial.log
rm -f "$SER"
ACC=(-accel tcg); [ -r /dev/kvm ] && [ -w /dev/kvm ] && ACC=(-accel kvm)
CODE=/usr/share/OVMF/OVMF_CODE_4M.fd
cp -f /usr/share/OVMF/OVMF_VARS_4M.fd "$OUT/ovmf-vars-a64.fd"
# sem VMX/SVM no convidado: com -cpu max o OVMF configura o IA32_FEATURE_CONTROL em cada núcleo e trava (run.ps1)
echo "QEMU (${ACC[1]}, até ${SECS}s): $SER"
conta() { local n; n=$(grep -ac "$1" "$SER" 2>/dev/null); echo "${n:-0}"; }
fim() { [ "$(conta 'a64rt-teste: [0-9]* falhas')" -ge 2 ] && [ "$(conta 'libsys-abi: [0-9]* falhas')" -ge 2 ] &&
        { [ -z "$REAL" ] || [ "$(conta 'com.mos.a64teste.real (pid [0-9]*) terminou')" -ge 1 ]; }; }
# Neste host o QEMU com KVM às vezes para no OVMF (o serial fica nos ~113 bytes do firmware) ou cai com SIGSEGV antes
# do mOS: sem 1 KiB de serial em 120 s, a partida é refeita (até 3 vezes).
for tent in 1 2 3; do
    rm -f "$SER"
    cp -f /usr/share/OVMF/OVMF_VARS_4M.fd "$OUT/ovmf-vars-a64.fd"
    timeout "$SECS" qemu-system-x86_64 -machine q35 -m 1536 -cpu max,-vmx,-svm -smp 4 "${ACC[@]}" \
        -drive if=pflash,format=raw,readonly=on,file="$CODE" -drive if=pflash,format=raw,file="$OUT/ovmf-vars-a64.fd" \
        -drive format=raw,file="$IMG",if=ide,index=0 -snapshot -vga std -display none -nic none -no-reboot \
        -serial file:"$SER" > "$OUT/qemu-a64.log" 2>&1 &
    QP=$!
    t0=$SECONDS; subiu=0
    while kill -0 $QP 2>/dev/null; do
        fim && break
        [ $subiu = 0 ] && [ "$(stat -c %s "$SER" 2>/dev/null || echo 0)" -gt 1024 ] && subiu=1
        [ $subiu = 0 ] && [ $((SECONDS - t0)) -gt 120 ] && { echo "partida $tent: o mOS não subiu em 120 s; refazendo"; break; }
        sleep 5
    done
    kill $QP 2>/dev/null; wait $QP 2>/dev/null
    [ $subiu = 1 ] && break
done
grep -aE '(a64rt-teste|libsys-abi): [0-9]+ falhas|FALHA' "$SER" | head -40
if [ -n "$REAL" ]; then
    echo "--- $(basename "$REAL") ${REALARGS[*]} (arm64):"
    tr -d '\000' < "$SER" | awk '/com.mos.a64teste.real iniciado/{f=1} f{print} /com.mos.a64teste.real \(pid [0-9]*\) terminou/{exit}' | grep -av '^proc: \|^mach: \|^macho: ' | head -30
fi
n=$(grep -aEc '(a64rt-teste|libsys-abi): 0 falhas' "$SER")
echo "a64rt-teste-vm: $n de 4 execuções sem falha"
[ "$n" -eq 4 ]
