#!/bin/bash
# mOS — leva um app Electron x86_64 até a janela no machotest (docs/testes-linux.md, "Electron até a janela").
#   tools/test-electron-janela.sh <libSystem.mos> <machotest> <App.app> [segundos]
# Monta uma raiz com o app em /Aplicativos, a libc++ e a libz do build (build/libcxx, build/zlib-x86_64) e, se
# existir, o CryptoKit de tools/build-cryptokit-macho.sh (MOS_CRYPTOKIT=<arquivo>). Passa se aparecer
# "[gui] janela 1". O harness é um processo só: o renderer (Helper) não sobe, então o conteúdo da janela fica vazio
# e a janela que o app só mostra depois do ready-to-show do renderer (o Slack) não aparece aqui.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LIBSYS=${1:?uso: test-electron-janela.sh <libSystem.mos> <machotest> <App.app> [segundos]}
MT=${2:?uso}
APP=${3:?uso}
SECS=${4:-90}
NAME=$(basename "$APP")
EXE=$(python3 -I -c 'import plistlib,sys; print(plistlib.load(open(sys.argv[1],"rb"))["CFBundleExecutable"])' "$APP/Contents/Info.plist")
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
V=$T/vfs
mkdir -p "$V/Aplicativos" "$V/usr/lib" "$V/tmp" "$V/Users/Visitante"
cp -a "$APP" "$V/Aplicativos/$NAME"
cp -f "$ROOT/build/libcxx/libc++.1.dylib" "$ROOT/build/zlib-x86_64/libz.1.dylib" "$V/usr/lib/"
if [ -n "${MOS_CRYPTOKIT:-}" ]; then
    mkdir -p "$V/System/Library/Frameworks/CryptoKit.framework/Versions/A"
    cp -f "$MOS_CRYPTOKIT" "$V/System/Library/Frameworks/CryptoKit.framework/Versions/A/CryptoKit"
fi
P=/Aplicativos/$NAME/Contents/MacOS/$EXE
MACHOTEST_PATH=$P HOME=/Users/Visitante timeout "$SECS" "$MT" "$LIBSYS" "$V" "$V$P" \
    --no-sandbox --disable-gpu --in-process-gpu --enable-features=NetworkServiceInProcess 2>&1 |
    grep -v 'fim do roteiro' | head -c 2000000 > "$T/log" || true
grep -E 'dyld: dlopen|\[gui\] janela|exceção|ainda não implementado|SQLITE_|não emulada' "$T/log" | sort | uniq -c | sort -rn | head -40
if grep -q '\[gui\] janela 1 ' "$T/log"; then echo "electron-janela: $NAME chegou à janela"; else echo "electron-janela: $NAME não chegou à janela"; exit 1; fi
