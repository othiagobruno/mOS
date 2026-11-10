#!/bin/bash
# Copia para esp/mOS/Aplicativos os apps oficiais citados em docs/l22-qemu-arm64.md
# e o OpenBot de docs/compat-open-usage.md. O SHA-256 do pacote é conferido antes
# de extrair. Cada .app entra pelo tools/stage-image.sh --sem-imagem (a mesma lista
# de arquivos do stage de teste). O Codex CLI não é um .app: o binário Mach-O vai
# para /Aplicativos/Codex CLI/codex.
# Uso: tools/stage-apps-oficiais.sh <OutDir>     OutDir já tem esp/ (tools/build.sh)
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build}
case "$OUT" in
    /*) ;;
    *) OUT=$ROOT/$OUT ;;
esac
ESP=$OUT/esp
MOS=$ESP/mOS
if [ ! -d "$MOS" ]; then
    echo "stage-apps: falta $MOS (rode tools/build.sh até criar o esp)" >&2
    exit 1
fi
DL=$OUT/apps-dl
mkdir -p "$DL" "$MOS/Aplicativos" "$MOS/usr/lib" "$MOS/usr/lib/swift"

fetch() {
    local url=$1 dest=$2 want=$3
    if [ -f "$dest" ] && [ "$(sha256sum "$dest" | awk '{print $1}')" = "$want" ]; then
        echo "cache ok $(basename "$dest")"
        return 0
    fi
    echo "baixando $(basename "$dest")"
    curl -fL --retry 3 -o "$dest.partial" "$url"
    mv "$dest.partial" "$dest"
    local got
    got=$(sha256sum "$dest" | awk '{print $1}')
    if [ "$got" != "$want" ]; then
        echo "stage-apps: SHA-256 de $(basename "$dest") é $got, esperado $want" >&2
        exit 1
    fi
}

# Versões e hashes: docs/l22-qemu-arm64.md, exceto onde o comentário diz o contrário.
# Cursor: o SHA f78da77b… do doc não é o zip arm64, x64 nem universal deste commit no CDN.
# Entra o zip darwin-arm64 oficial da 3.29.15 (o mesmo URL do cask), com o SHA medido aqui.
fetch "https://downloads.claude.ai/releases/darwin/universal/4.23118.1/Claude-e071e72f72fe1f56124e3c8a0096f4d75510060a.zip" \
    "$DL/Claude.zip" a1b69ab28237a34b9894c7fe370492107e3e3b6a6cccb98a258bde27ce98a575
fetch "https://downloads.cursor.com/production/86776fcb52ca505783155631089b65d646f9b03f/darwin/arm64/Cursor-darwin-arm64.zip" \
    "$DL/Cursor.zip" 8fa25e50a63b04e8d8a8645834e7471d0d102b229b8c16bf9e06dd623e694986
fetch "https://downloads.slack-edge.com/desktop-releases/mac/arm64/4.53.287/Slack-4.53.287-macOS.dmg" \
    "$DL/Slack.dmg" 4d4b794cfca076d438122693889179ef6c59066a239413b76df9b51c6dd14082
fetch "https://downloads.cursor.com/sand/stable/darwin-arm64/0.79.4/Grok_Bot_0.79.4.zip" \
    "$DL/Grok.zip" fdbe280ff9bf23ad95efb518141a8892fc39f677bef5d6540a73010993ad2ac4
fetch "https://persistent.oaistatic.com/codex-app-prod/ChatGPT-darwin-arm64-26.1103.60933.zip" \
    "$DL/ChatGPT.zip" 1d671e7784e0e379f7293ebcfeac826c2a297524ea806cd55e994068d019e388
fetch "https://update.code.visualstudio.com/1.145.3/darwin-arm64/stable" \
    "$DL/VSCode.zip" c81d07705484b6017f34f2b1c106d2817b38e35cbd98adc9dec5c562aa945167
fetch "https://web.whatsapp.com/desktop/mac_native/release/?version=2.26.45.13&extension=zip&configuration=Release&branch=master&is_buck=true" \
    "$DL/WhatsApp.zip" 29c12f980448668107782f6f1b52fe28f60e8a6729032177f6002eedd917cb3f
fetch "https://github.com/openai/codex/releases/download/rust-v0.167.0/codex-aarch64-apple-darwin.tar.gz" \
    "$DL/codex-cli.tar.gz" 4b22a2a971be2791c3e31f912f3e77a88ebd64a65812a7289ccba65dd6425fe9
fetch "https://github.com/nightly-labs/openbot/releases/download/v0.66.0/OpenBot-0.66.0-x64.zip" \
    "$DL/OpenBot.zip" 473091aac28ccab16be5921c9477f6deef8f2d9523d6c87962c99581821f817f

unzip_app() {
    local zip=$1 name=$2
    local dest=$DL/x-$name
    rm -rf "$dest"
    mkdir -p "$dest"
    python3 - "$zip" "$dest" <<'PY'
import sys, zipfile
z, dest = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(z) as zf:
    zf.extractall(dest)
PY
    local app
    app=$(find "$dest" -name '*.app' -type d | head -1)
    if [ -z "$app" ]; then echo "stage-apps: sem .app em $zip" >&2; exit 1; fi
    printf '%s\n' "$app"
}

DMG=$OUT/dmgtest
if [ ! -x "$DMG" ]; then
    cat > "$OUT/dmghost.c" <<'EOF'
#include <stdint.h>
typedef struct { int year, month, day, hour, minute, second, weekday; } datetime_t;
void rtc_now(datetime_t *dt) { if (dt) { dt->year=2026; dt->month=1; dt->day=1; } }
void rtc_now_utc(datetime_t *dt) { rtc_now(dt); }
uint32_t rand32(void) { return 1; }
EOF
    cc -O2 -DMOS_HOST_TEST -I"$ROOT/kernel/include" -I"$ROOT/tools" \
        "$ROOT/tools/dmgtest.c" "$OUT/dmghost.c" \
        "$ROOT/kernel/fs/apfs.c" "$ROOT/kernel/fs/blkdev.c" "$ROOT/kernel/fs/bzip2.c" \
        "$ROOT/kernel/fs/dfs.c" "$ROOT/kernel/fs/dmg.c" "$ROOT/kernel/fs/hfsplus.c" \
        "$ROOT/kernel/fs/hfsplus_wr.c" \
        "$ROOT/kernel/fs/inflate.c" "$ROOT/kernel/fs/lzfse.c" "$ROOT/kernel/fs/xz.c" \
        "$ROOT/kernel/fs/fat.c" "$ROOT/kernel/fs/pcache.c" -o "$DMG"
fi

stage_one() {
    local app=$1
    shift
    bash "$ROOT/tools/stage-image.sh" --out "$OUT" --sem-imagem --app "$app" --efi "$OUT/BOOTX64.EFI" "$@"
}

APP=$(unzip_app "$DL/Claude.zip" claude)
stage_one "$APP"
APP=$(unzip_app "$DL/Cursor.zip" cursor)
stage_one "$APP" --cursor-extras
APP=$(unzip_app "$DL/Grok.zip" grok)
stage_one "$APP"
APP=$(unzip_app "$DL/ChatGPT.zip" chatgpt)
stage_one "$APP"
APP=$(unzip_app "$DL/VSCode.zip" vscode)
stage_one "$APP"
APP=$(unzip_app "$DL/WhatsApp.zip" whatsapp)
stage_one "$APP"
APP=$(unzip_app "$DL/OpenBot.zip" openbot)
stage_one "$APP"

rm -rf "$DL/x-slack"
mkdir -p "$DL/x-slack"
"$DMG" "$DL/Slack.dmg" "$DL/x-slack" >/tmp/slack-dmg.txt
APP=$(find "$DL/x-slack" -name 'Slack.app' -type d | head -1)
if [ -z "$APP" ]; then echo "stage-apps: Slack.app não saiu do dmg" >&2; tail -20 /tmp/slack-dmg.txt >&2; exit 1; fi
stage_one "$APP"

mkdir -p "$DL/x-codex" "$MOS/Aplicativos/Codex CLI"
tar -xzf "$DL/codex-cli.tar.gz" -C "$DL/x-codex"
CODEX=$(find "$DL/x-codex" -type f \( -name codex -o -name 'codex-*apple-darwin' \) | head -1)
if [ -z "$CODEX" ]; then echo "stage-apps: binário codex ausente no tar" >&2; exit 1; fi
cp -f "$CODEX" "$MOS/Aplicativos/Codex CLI/codex"
chmod +x "$MOS/Aplicativos/Codex CLI/codex"
echo "OK: apps em $MOS/Aplicativos"
