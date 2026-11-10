#!/bin/bash
# Host: mkimg em streaming equivale ao mkimg que fazia calloc da imagem (7b65273),
# o pico de RSS fica abaixo do tamanho da imagem, e o Launch Services (parser real
# de plist + as pastas que ls_rescan vigia) lista os bundles gravados.
# Não sobe QEMU. Não baixa os apps oficiais.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cd "$TMP"

if ! git -C "$ROOT" cat-file -e 7b65273:tools/mkimg.c 2>/dev/null; then
    # checkout do Actions é raso; 7b65273 é commit, não um ref.
    git -C "$ROOT" fetch --depth=80 origin main
fi
mkdir -p "$TMP/ref/tools"
git -C "$ROOT" show 7b65273:tools/mkimg.c > "$TMP/ref/tools/mkimg.c"
ln -s "$ROOT/kernel" "$TMP/ref/kernel"
cc -O2 -o "$TMP/mkimg-ram" "$TMP/ref/tools/mkimg.c"
cc -O2 -Wall -Wextra -o "$TMP/mkimg" "$ROOT/tools/mkimg.c"
cc -O2 -Wno-incompatible-library-redeclaration -I"$ROOT/kernel/include" -o "$TMP/ls_lista_host" \
    "$ROOT/tools/ls_lista_host.c" "$ROOT/kernel/compat/plist.c"

cat > "$TMP/libtime.c" <<'EOF'
#include <time.h>
time_t time(time_t *t) {
    time_t v = 1700000000;
    if (t) *t = v;
    return v;
}
EOF
cc -shared -fPIC -o "$TMP/libtime.so" "$TMP/libtime.c"

mkdir -p "$TMP/esp/EFI/BOOT" "$TMP/esp/mOS/Aplicativos"
printf 'EFI' > "$TMP/esp/EFI/BOOT/BOOTX64.EFI"
plist() {
    local dir="$TMP/esp/mOS/Aplicativos/$1.app/Contents"
    mkdir -p "$dir/MacOS"
    printf 'MZ' > "$dir/MacOS/$2"
    cat > "$dir/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>$3</string>
<key>CFBundleExecutable</key><string>$2</string>
<key>CFBundleName</key><string>$1</string>
</dict></plist>
EOF
}
plist Claude Claude com.anthropic.claudefordesktop
plist Cursor Cursor com.todesktop.230313mzl4w4u92
plist Slack Slack com.tinyspeck.slackmacgap
plist "Grok Bot" "Grok Bot" com.xai.grokbot
plist ChatGPT ChatGPT com.openai.chat
plist "Visual Studio Code" Code com.microsoft.VSCode
plist WhatsApp WhatsApp net.whatsapp.WhatsApp
plist OpenBot OpenBot com.nightlylabs.openbot
mkdir -p "$TMP/esp/mOS/Aplicativos/Codex CLI"
printf '\xcf\xfa\xed\xfe' > "$TMP/esp/mOS/Aplicativos/Codex CLI/codex"

export LD_PRELOAD="$TMP/libtime.so"
"$TMP/mkimg-ram" "$TMP/old.img" "$TMP/esp" 48
"$TMP/mkimg" "$TMP/new.img" "$TMP/esp" 48
unset LD_PRELOAD
cmp "$TMP/old.img" "$TMP/new.img"
echo "mkimg: imagem de 48 MiB idêntica à versão com calloc"

if [ ! -x /usr/bin/time ]; then
    sudo apt-get update -qq && sudo apt-get install -y time
fi
# 1024 MiB de imagem, payload de poucos KiB. O calloc antigo segurava os 1024 MiB.
set +e
/usr/bin/time -v "$TMP/mkimg" "$TMP/big.img" "$TMP/esp" 1024 >"$TMP/mkimg-big.txt" 2>"$TMP/time.txt"
rc=$?
set -e
if [ "$rc" != 0 ]; then
    echo "mkimg da imagem grande falhou ($rc)" >&2
    cat "$TMP/time.txt" >&2
    exit 1
fi
rss=$(awk '/Maximum resident set size/ {print $6}' "$TMP/time.txt")
echo "mkimg: RSS máximo ${rss} KiB para imagem de 1048576 KiB"
if [ -z "$rss" ] || [ "$rss" -ge 262144 ]; then
    echo "RSS ${rss:-?} KiB não ficou bem abaixo da imagem (limite 256 MiB)" >&2
    cat "$TMP/time.txt" >&2
    exit 1
fi

"$TMP/ls_lista_host" "$TMP/new.img" | tee "$TMP/ls.txt"
for id in \
    com.anthropic.claudefordesktop \
    com.todesktop.230313mzl4w4u92 \
    com.tinyspeck.slackmacgap \
    com.xai.grokbot \
    com.openai.chat \
    com.microsoft.VSCode \
    net.whatsapp.WhatsApp \
    com.nightlylabs.openbot
do
    grep -q "($id)" "$TMP/ls.txt" || { echo "Launch Services não listou $id" >&2; exit 1; }
done
if grep -q 'Codex' "$TMP/ls.txt"; then
    echo "Codex CLI não é bundle; não pode entrar na lista do Launch Services" >&2
    exit 1
fi
grep -q 'ls_lista_host: 8 bundle' "$TMP/ls.txt"
echo "test-mkimg-ls: ok"
