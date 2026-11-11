#!/bin/bash
# Copia para esp/mOS/Aplicativos os apps oficiais citados em docs/l22-qemu-arm64.md,
# o OpenBot de docs/compat-open-usage.md e Antigravity, Postico e Setapp (docs/meta-compat.md).
# O SHA-256 do pacote é conferido antes de extrair. Cada .app Electron entra pelo
# tools/stage-image.sh --sem-imagem (a mesma lista de arquivos do stage de teste); os AppKit
# (WhatsApp, Postico, Setapp) entram inteiros. O Codex CLI não é um .app: o binário Mach-O vai
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

# download com SHA-256 conferido (curl, wget ou python3: tools/baixar.sh)
. "$ROOT/tools/baixar.sh"
fetch() {
    baixar "$1" "$2" "$3" || exit 1
    echo "ok $(basename "$2")"
}

# Versões e hashes: docs/l22-qemu-arm64.md, exceto onde o comentário diz o contrário.
# Cursor: o SHA f78da77b… do doc não é o zip arm64, x64 nem universal deste commit no CDN.
# Entra o zip darwin-arm64 oficial da 3.29.15 (o mesmo URL do cask), com o SHA medido aqui.
fetch "https://downloads.claude.ai/releases/darwin/universal/4.23118.1/Claude-e071e72f72fe1f56124e3c8a0096f4d75510060a.zip" \
    "$DL/Claude.zip" a1b69ab28237a34b9894c7fe370492107e3e3b6a6cccb98a258bde27ce98a575
fetch "https://downloads.cursor.com/production/86776fcb52ca505783155631089b65d646f9b03f/darwin/arm64/Cursor-darwin-arm64.zip" \
    "$DL/Cursor.zip" 8fa25e50a63b04e8d8a8645834e7471d0d102b229b8c16bf9e06dd623e694986
# Slack e Grok: x64, os builds que o machocheck do host lê. O DMG do Slack
# começa com bloco bzip2 (UDIF). O zip universal do Grok 0.79.4 responde 403.
fetch "https://downloads.slack-edge.com/desktop-releases/mac/x64/4.53.287/Slack-4.53.287-macOS.dmg" \
    "$DL/Slack.dmg" 3053e29516857c31d55b79fc0dc288c590a9742a1e7aec0e6e29419206449660
fetch "https://downloads.cursor.com/sand/stable/darwin-x64/0.79.4/Grok_Bot_0.79.4_x64.dmg" \
    "$DL/Grok.dmg" e756dda191013c4042b3ceb886d699f0fb07b83c93ecbe4c9181888737d9d0b6
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
# Antigravity, Postico e Setapp (docs/meta-compat.md, apps 4, 9 e 12): SHA-256 medido no download oficial
# de 10 nov. 2026. O Postico é o 2.3.5 (9817), para onde releases.eggerapps.at/postico2/download aponta.
fetch "https://storage.googleapis.com/antigravity-public/antigravity-hub/2.31.0-6379060755038208/darwin-x64/Antigravity.dmg" \
    "$DL/Antigravity.dmg" f9d2699fd1307a44eae396f7463fc118f04554dc84d4ae56d95c487f1f050c56
fetch "https://downloads.eggerapps.at/postico/postico-9817.dmg" \
    "$DL/Postico.dmg" 34ec7730de0796c008e0caf9f366629a3043c5813c79f749e47d22084afb6e0f
fetch "https://dl.devmate.com/com.setapp.DesktopClient/Setapp.zip" \
    "$DL/Setapp.zip" a124004ba0c6555e8bb05904a65e78b7d32a8eaa069b304f8e9a78e4e859735a

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
        "$ROOT/kernel/fs/hfsplus_wr.c" "$ROOT/kernel/fs/pcache.c" \
        "$ROOT/kernel/fs/inflate.c" "$ROOT/kernel/fs/lzfse.c" "$ROOT/kernel/fs/xz.c" \
        "$ROOT/kernel/fs/fat.c" -o "$DMG"
fi

stage_one() {
    local app=$1
    shift
    bash "$ROOT/tools/stage-image.sh" --out "$OUT" --sem-imagem --app "$app" --efi "$OUT/BOOTX64.EFI" "$@"
}

# DMG UDIF (o Slack aparece como bzip2 no file(1); o Grok x64, como zlib).
# Se o leitor não montar e o arquivo for um bzip2 único, descompacta e procura o .app.
extrair_dmg() {
    local dmg=$1 name=$2
    local dest=$DL/x-$name
    rm -rf "$dest"
    mkdir -p "$dest"
    if "$DMG" "$dmg" "$dest" >/tmp/$name-dmg.txt 2>&1; then
        return 0
    fi
    if file "$dmg" | grep -q 'bzip2'; then
        echo "stage-apps: $name é bzip2; descompactando"
        bzip2 -dc "$dmg" > "$dest/payload" || true
        if file "$dest/payload" | grep -q 'Zip archive'; then
            python3 - "$dest/payload" "$dest" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as zf:
    zf.extractall(sys.argv[2])
PY
            rm -f "$dest/payload"
            return 0
        fi
        if file "$dest/payload" | grep -q 'bzip2\|zlib\|UDIF\|Macintosh'; then
            if "$DMG" "$dest/payload" "$dest" >>/tmp/$name-dmg.txt 2>&1; then
                return 0
            fi
        fi
    fi
    echo "stage-apps: não extraí o .app de $dmg" >&2
    tail -20 /tmp/$name-dmg.txt >&2 || true
    return 1
}

APP=$(unzip_app "$DL/Claude.zip" claude)
stage_one "$APP"
APP=$(unzip_app "$DL/Cursor.zip" cursor)
# git: o do build (tools/build-git.sh, /usr/bin/git Mach-O). Sem ele, o bottle Intel do Homebrew (sha256 do
# manifesto do ghcr.io conferido pelo fetch-bottles.sh) com pcre2 e gettext.
if [ "$(head -c 4 "$MOS/usr/bin/git" 2>/dev/null | od -An -tx1 | tr -d ' \n')" = cffaedfe ]; then
    stage_one "$APP" --cursor-extras
else
    bash "$ROOT/tools/fetch-bottles.sh" git gettext -d "$DL/brew"
    stage_one "$APP" --cursor-extras --bottles "$DL/brew"
fi
extrair_dmg "$DL/Grok.dmg" grok
APP=$(find "$DL/x-grok" -name 'Grok Bot.app' -type d | head -1)
if [ -z "$APP" ]; then echo "stage-apps: Grok Bot.app não saiu do dmg" >&2; exit 1; fi
stage_one "$APP"
APP=$(unzip_app "$DL/ChatGPT.zip" chatgpt)
stage_one "$APP"
APP=$(unzip_app "$DL/VSCode.zip" vscode)
stage_one "$APP"
APP=$(unzip_app "$DL/WhatsApp.zip" whatsapp)
stage_one "$APP"
APP=$(unzip_app "$DL/OpenBot.zip" openbot)
stage_one "$APP"

extrair_dmg "$DL/Slack.dmg" slack
APP=$(find "$DL/x-slack" -name 'Slack.app' -type d | head -1)
if [ -z "$APP" ]; then echo "stage-apps: Slack.app não saiu do dmg" >&2; tail -20 /tmp/slack-dmg.txt >&2; exit 1; fi
stage_one "$APP"

extrair_dmg "$DL/Antigravity.dmg" antigravity
APP=$(find "$DL/x-antigravity" -name 'Antigravity.app' -type d | head -1)
if [ -z "$APP" ]; then echo "stage-apps: Antigravity.app não saiu do dmg" >&2; exit 1; fi
stage_one "$APP"

# Postico e Setapp são AppKit, não Electron: o bundle entra inteiro (frameworks, plug-ins, agentes de
# launchd do Setapp), como o do WhatsApp abaixo.
copiar_bundle() {
    local app=$1
    local dest="$MOS/Aplicativos/$(basename "$app")"
    rm -rf "$dest"
    cp -a "$app" "$dest"
    python3 "$ROOT/tools/extrair-framework.py" --reparar "$dest" >/dev/null || true
}
extrair_dmg "$DL/Postico.dmg" postico
APP=$(find "$DL/x-postico" -name 'Postico 2.app' -type d | head -1)
if [ -z "$APP" ]; then echo "stage-apps: Postico 2.app não saiu do dmg" >&2; exit 1; fi
copiar_bundle "$APP"
APP=$(unzip_app "$DL/Setapp.zip" setapp)
copiar_bundle "$APP"

mkdir -p "$DL/x-codex" "$MOS/Aplicativos/Codex CLI"
tar -xzf "$DL/codex-cli.tar.gz" -C "$DL/x-codex"
CODEX=$(find "$DL/x-codex" -type f \( -name codex -o -name 'codex-*apple-darwin' \) | head -1)
if [ -z "$CODEX" ]; then echo "stage-apps: binário codex ausente no tar" >&2; exit 1; fi
cp -f "$CODEX" "$MOS/Aplicativos/Codex CLI/codex"
chmod +x "$MOS/Aplicativos/Codex CLI/codex"
# O stage-image.sh só leva o executável e dois frameworks do WhatsApp.
# O bundle nativo (Resources, PlugIns) entra inteiro; o reparo copia o alvo do link.
WA=$(find "$DL/x-whatsapp" -name 'WhatsApp.app' -type d | head -1)
if [ -n "$WA" ]; then
    rm -rf "$MOS/Aplicativos/WhatsApp.app"
    cp -a "$WA" "$MOS/Aplicativos/WhatsApp.app"
    python3 "$ROOT/tools/extrair-framework.py" --reparar "$MOS/Aplicativos/WhatsApp.app" >/dev/null || true
fi
echo "OK: apps em $MOS/Aplicativos"
