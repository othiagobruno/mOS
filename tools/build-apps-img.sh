#!/bin/bash
# mOS — disco de dados com os apps de terceiros (WhatsApp, ChatGPT, Claude, Cursor, VS Code, Setapp, OpenBot...).
# Eles não vão na imagem do sistema (build/mOS.img só leva o sistema e os apps nativos do mOS). Este script, rodado
# só quando pedido, baixa e prepara os apps (tools/stage-apps-oficiais.sh) numa árvore à parte e grava
# build/apps.img: GPT com uma partição de dados FAT32 rotulada MOS-APPS, com os apps em /Aplicativos.
# No mOS o disco aparece em /Volumes/MOS-APPS/Aplicativos (kernel/dev/storage.c monta todo disco extra).
#   tools/build-apps-img.sh [--out build]          (precisa de build/mkimg e build/BOOTX64.EFI do tools/build.sh)
# No QEMU, como segundo disco:  -drive file=build/apps.img,format=raw,if=none,id=apps -device nvme,drive=apps,serial=apps
# (o kernel vê NVMe, SATA/AHCI e USB; virtio não). Nada é gravado no disco do PC.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/build
while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT=$(cd "$2" && pwd); shift 2 ;;
        *) echo "uso: tools/build-apps-img.sh [--out build]" >&2; exit 2 ;;
    esac
done
[ -x "$OUT/mkimg" ] || cc -O2 "$ROOT/tools/mkimg.c" -o "$OUT/mkimg"
STG=$OUT/apps-stage
rm -rf "$STG"
mkdir -p "$STG" "$OUT/apps-dl"
ln -s "$OUT/apps-dl" "$STG/apps-dl"                  # downloads com hash conferido ficam em build/apps-dl
[ -f "$OUT/BOOTX64.EFI" ] && ln -s "$OUT/BOOTX64.EFI" "$STG/BOOTX64.EFI"
[ -x "$OUT/dmgtest" ] && ln -s "$OUT/dmgtest" "$STG/dmgtest"
# o stage-image.sh monta cada app em <out>/esp/mOS/Aplicativos e alguns (Cursor) precisam de programas do sistema
# (/usr/bin/security, git): a árvore de preparo começa como CÓPIA da do build (cópia real, não hardlink, porque o
# stage-image reescreve arquivos do sistema e isso não pode vazar para build/esp). Só /Aplicativos sai daqui.
[ -d "$OUT/esp/mOS/usr/bin" ] || { echo "build-apps-img: falta $OUT/esp do tools/build.sh" >&2; exit 1; }
cp -a "$OUT/esp" "$STG/esp"
rm -rf "$STG/esp/mOS/Aplicativos"
bash "$ROOT/tools/stage-apps-oficiais.sh" "$STG"
DADOS=$STG/disco
mkdir -p "$DADOS"
mv "$STG/esp/mOS/Aplicativos" "$DADOS/Aplicativos"
python3 "$ROOT/tools/verificar-imagem-apps.py" --apps "$DADOS"
BYTES=$(find "$DADOS" -type f -printf '%s\n' | awk '{s+=$1} END {print s+0}')
MIB=$(( (BYTES * 9 / 8 + 1048575) / 1048576 + 64 ))
MIB=$(( (MIB + 15) / 16 * 16 ))
echo "==> apps.img ${MIB} MiB"
"$OUT/mkimg" "$OUT/apps.img" "$DADOS" "$MIB" --dados MOS-APPS
rm -rf "$STG"
[ -s "$OUT/apps.img" ] || { echo "build-apps-img: o mkimg não gerou $OUT/apps.img" >&2; exit 1; }
echo "OK: $OUT/apps.img (segundo disco; /Volumes/MOS-APPS/Aplicativos no mOS)"
