#!/bin/bash
# mOS — abre a imagem no QEMU do macOS com janela (Apple silicon ou Intel), afinado para o TCG (não há KVM no Mac).
#   tools/qemu-mac.sh [--compat] [--img build/mOS.img] [--apps build/apps.img] [--smp N] [--mem MiB] [--sem-janela]
# Perfil padrão (rápido): -accel tcg,thread=multi (um thread do host por núcleo; o QEMU desliga por padrão porque o
# arm64 tem ordem de memória mais fraca que o x86, mas o autoteste inteiro passa) e -cpu Nehalem-v2,+fsgsbase: sem AVX o
# kernel salva a FPU com FXSAVE em vez de XSAVE a cada interrupção e nada roda AVX emulado. Medido no autoteste (M-series,
# 4 núcleos): desktop pronto em 1,35 s contra 4,5 s do -cpu max, autoteste em 58 s contra 79 s, os mesmos 443 ok.
# --compat: -cpu max (AVX/AVX2/AVX-512), para apps que usam AVX sem checar o CPUID. Mais lento.
# Segundo disco de apps (tools/build-apps-img.sh): /Volumes/MOS-APPS/Aplicativos no mOS. Log serial: <img>.serial.log
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
IMG=$ROOT/build/mOS.img
APPS=$ROOT/build/apps.img
CPU="Nehalem-v2,+fsgsbase,-spec-ctrl"
SMP=$(( $(sysctl -n hw.perflevel0.physicalcpu 2>/dev/null || sysctl -n hw.ncpu) ))
[ "$SMP" -gt 8 ] && SMP=8
[ "$SMP" -lt 2 ] && SMP=2
MEM=4096
DISPLAY_ARGS=(-display cocoa,zoom-to-fit=on)
while [ $# -gt 0 ]; do
    case "$1" in
        --compat) CPU=max; shift ;;
        --img) IMG=$2; shift 2 ;;
        --apps) APPS=$2; shift 2 ;;
        --smp) SMP=$2; shift 2 ;;
        --mem) MEM=$2; shift 2 ;;
        --sem-janela) DISPLAY_ARGS=(-display none); shift ;;
        *) echo "uso: tools/qemu-mac.sh [--compat] [--img X] [--apps X] [--smp N] [--mem MiB] [--sem-janela]" >&2; exit 2 ;;
    esac
done
[ -f "$IMG" ] || { echo "qemu-mac: falta $IMG (tools/build.sh ou tools/build-mac.sh)" >&2; exit 1; }
BREW=$(command -v brew >/dev/null 2>&1 && brew --prefix || echo /opt/homebrew)
CODE=$BREW/share/qemu/edk2-x86_64-code.fd
VARS=${IMG%.img}.vars.fd
[ -f "$VARS" ] || cp "$BREW/share/qemu/edk2-i386-vars.fd" "$VARS"
EXTRA=()
# MOS_QEMU_EXTRA: argumentos a mais para o QEMU (ex.: -monitor unix:/tmp/m.sock,server,nowait para screendump)
[ -n "${MOS_QEMU_EXTRA:-}" ] && read -r -a EXTRA <<< "$MOS_QEMU_EXTRA"
if [ -f "$APPS" ]; then
    EXTRA+=(-drive "file=$APPS,format=raw,if=none,id=apps" -device nvme,drive=apps,serial=apps)
    echo "qemu-mac: disco de apps $APPS (/Volumes/MOS-APPS/Aplicativos)"
fi
echo "qemu-mac: $IMG, cpu $CPU, $SMP núcleos, $MEM MiB; log em ${IMG%.img}.serial.log"
exec qemu-system-x86_64 -name mOS -machine q35 -accel tcg,thread=multi,tb-size=2048 -cpu "$CPU" -smp "$SMP" -m "$MEM" \
    -drive "if=pflash,format=raw,readonly=on,file=$CODE" -drive "if=pflash,format=raw,file=$VARS" \
    -drive "file=$IMG,format=raw,if=ide,index=0" "${EXTRA[@]}" \
    -vga std "${DISPLAY_ARGS[@]}" -device qemu-xhci -device usb-tablet -device usb-kbd -nic user,model=e1000 \
    -serial "file:${IMG%.img}.serial.log" -no-reboot
