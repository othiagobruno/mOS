#!/bin/bash
# mOS — lista de arquivos da imagem de teste no Linux (docs/staging-linux.md).
# No Windows a lista é tools/stage-electron.ps1. No Linux esse .ps1 chama este script,
# para as duas entradas não divergirem. O mkimg é o binário nativo (cc/gcc), sem .exe.
# O prefixo \\?\ não existe aqui.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=build-electron
SIZE=768
APP=""
FATIA=x86_64
MAXMIB=16
ARGS0="--in-process-gpu --disable-gpu --no-sandbox --enable-logging --v=0"
AUTOSTART=0
DIAG=0
CALOG=0
TRACE=""
SUB=""
COMSWIFT=0
ABRIR=""
ABRIRMS=120000
ABRIR2=""
ABRIR2MS=180000
COMAPP=""
NAV=""
MOTOR=""
CURSOR=0
BOTTLES=""
EFI=""
SEMIMG=0
ARGSAPP=()

while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT=$2; shift 2 ;;
        --size) SIZE=$2; shift 2 ;;
        --app) APP=$2; shift 2 ;;
        --fatia) FATIA=$2; shift 2 ;;
        --max-modulo-mib) MAXMIB=$2; shift 2 ;;
        --args0) ARGS0=$2; shift 2 ;;
        --autostart) AUTOSTART=1; shift ;;
        --diagnostico) DIAG=1; shift ;;
        --ca-log) CALOG=1; shift ;;
        --tracepoints) TRACE=$2; shift 2 ;;
        --substituir-app) SUB=$2; shift 2 ;;
        --com-swift) COMSWIFT=1; shift ;;
        --abrir-url) ABRIR=$2; shift 2 ;;
        --abrir-apos-ms) ABRIRMS=$2; shift 2 ;;
        --abrir-url2) ABRIR2=$2; shift 2 ;;
        --abrir-apos2-ms) ABRIR2MS=$2; shift 2 ;;
        --com-app) COMAPP=$2; shift 2 ;;
        --navegador-padrao) NAV=$2; shift 2 ;;
        --argumentos-app) ARGSAPP+=("$2"); shift 2 ;;
        --motor-horizonte) MOTOR=$2; shift 2 ;;
        --cursor-extras) CURSOR=1; shift ;;
        --bottles) BOTTLES=$2; shift 2 ;;
        --efi) EFI=$2; shift 2 ;;
        --sem-imagem) SEMIMG=1; shift ;;
        *) echo "argumento desconhecido: $1" >&2; exit 2 ;;
    esac
done
if [ -z "$APP" ]; then echo "stage-image.sh: falta --app" >&2; exit 2; fi
APP=$(cd "$(dirname "$APP")" && pwd)/$(basename "$APP")
case "$OUT" in
    /*) OUTABS=$OUT ;;
    *) OUTABS=$ROOT/$OUT ;;
esac
mkdir -p "$OUTABS"
ESP=$OUTABS/esp
MOS=$ESP/mOS
if [ ! -d "$MOS" ]; then
    mkdir -p "$MOS"
    if [ -d "$ROOT/root/mOS" ]; then
        cp -a "$ROOT/root/mOS/." "$MOS/"
        echo "esp/mOS criado e semeado com root/mOS"
    fi
fi
rm -rf "$MOS/mOS"
if [ -n "$EFI" ] && [ -f "$EFI" ]; then
    mkdir -p "$ESP/EFI/BOOT"
    cp -f "$EFI" "$ESP/EFI/BOOT/BOOTX64.EFI"
elif [ ! -f "$ESP/EFI/BOOT/BOOTX64.EFI" ] && [ -f "$OUTABS/BOOTX64.EFI" ]; then
    mkdir -p "$ESP/EFI/BOOT"
    cp -f "$OUTABS/BOOTX64.EFI" "$ESP/EFI/BOOT/BOOTX64.EFI"
fi
if [ ! -f "$ESP/EFI/BOOT/BOOTX64.EFI" ]; then
    echo "aviso: esp sem EFI/BOOT/BOOTX64.EFI. O kernel sai de tools/build.sh ou build.ps1 (zig). A imagem ainda é gerada." >&2
fi

SRC=$APP/Contents
EXE=$(python3 - "$SRC/Info.plist" <<'PY'
import re, sys
t = open(sys.argv[1], encoding="utf-8", errors="replace").read()
m = re.search(r"<key>CFBundleExecutable</key>\s*<string>([^<]+)</string>", t)
print(m.group(1) if m else "")
PY
)
if [ -z "$EXE" ]; then EXE=$(basename "$(find "$SRC/MacOS" -maxdepth 1 -type f | head -1)"); fi
NAME=$(basename "$APP" .app)
DSTAPP=$MOS/Aplicativos/$NAME.app
DST=$DSTAPP/Contents
rm -rf "$DSTAPP"
mkdir -p "$DST/MacOS" "$DST/Resources"
cp -f "$SRC/Info.plist" "$DST/Info.plist"
python3 "$ROOT/tools/extrair-framework.py" --copiar "$SRC/MacOS/$EXE" "$DST/MacOS/$EXE"
if [ -d "$SRC/Frameworks" ]; then
    for fwdir in "$SRC/Frameworks"/*.framework; do
        [ -d "$fwdir" ] || continue
        fw=$(basename "$fwdir" .framework)
        bin=""
        if [ -e "$fwdir/Versions/A/$fw" ]; then bin="$fwdir/Versions/A/$fw"; fi
        if [ -z "$bin" ] && [ -e "$fwdir/$fw" ]; then bin="$fwdir/$fw"; fi
        if [ -n "$bin" ]; then
            mkdir -p "$DST/Frameworks/$fw.framework/Versions/A"
            python3 "$ROOT/tools/extrair-framework.py" --copiar "$bin" "$DST/Frameworks/$fw.framework/Versions/A/$fw"
        fi
    done
    for h in "$SRC/Frameworks"/*Helper*.app; do
        [ -d "$h" ] || continue
        bn=$(basename "$h")
        mkdir -p "$DST/Frameworks/$bn/Contents/MacOS"
        cp -f "$h/Contents/Info.plist" "$DST/Frameworks/$bn/Contents/Info.plist" 2>/dev/null || true
        if [ -d "$h/Contents/MacOS" ]; then
            for b in "$h/Contents/MacOS"/*; do
                [ -f "$b" ] || continue
                python3 "$ROOT/tools/extrair-framework.py" --copiar "$b" "$DST/Frameworks/$bn/Contents/MacOS/$(basename "$b")"
            done
        fi
    done
fi
RES="Frameworks/Electron Framework.framework/Resources"
VERRES="Frameworks/Electron Framework.framework/Versions/A/Resources"
mkdir -p "$DST/$RES" "$DST/$VERRES"
RESSRC="$SRC/Frameworks/Electron Framework.framework/Versions/A/Resources"
[ -d "$RESSRC" ] || RESSRC="$SRC/Frameworks/Electron Framework.framework/Resources"
if [ -d "$RESSRC" ]; then
    # O binário fica em Versions/A. O Electron abre Versions/A/Resources/icudtl.dat
    # (DIR_MODULE + Resources). Só a Resources do topo deixa o arquivo fora do caminho.
    find "$RESSRC" -maxdepth 1 -type f -exec cp -f {} "$DST/$RES/" \;
    find "$RESSRC" -maxdepth 1 -type f -exec cp -f {} "$DST/$VERRES/" \;
    for l in en.lproj pt_BR.lproj; do
        [ -d "$RESSRC/$l" ] && cp -a "$RESSRC/$l" "$DST/$RES/" && cp -a "$RESSRC/$l" "$DST/$VERRES/"
    done
fi
LIB="$SRC/Frameworks/Electron Framework.framework/Versions/A/Libraries"
if [ -d "$LIB" ]; then
    mkdir -p "$DST/Frameworks/Electron Framework.framework/Versions/A/Libraries"
    for b in "$LIB"/*; do
        [ -f "$b" ] || continue
        python3 "$ROOT/tools/extrair-framework.py" --copiar "$b" "$DST/Frameworks/Electron Framework.framework/Versions/A/Libraries/$(basename "$b")"
    done
fi
HLP="$SRC/Frameworks/Electron Framework.framework/Versions/A/Helpers"
if [ -d "$HLP" ]; then
    mkdir -p "$DST/Frameworks/Electron Framework.framework/Helpers"
    for b in "$HLP"/*; do
        [ -f "$b" ] || continue
        python3 "$ROOT/tools/extrair-framework.py" --copiar "$b" "$DST/Frameworks/Electron Framework.framework/Helpers/$(basename "$b")"
    done
fi
for f in app.asar electron.icns; do
    [ -f "$SRC/Resources/$f" ] && cp -f "$SRC/Resources/$f" "$DST/Resources/"
done
if [ -d "$SRC/Resources" ]; then
    for a in "$SRC/Resources"/*.asar; do
        [ -f "$a" ] || continue
        base=$(basename "$a")
        [ "$base" = app.asar ] && continue
        cp -f "$a" "$DST/Resources/"
    done
fi
[ -d "$SRC/Resources/fonts" ] && cp -a "$SRC/Resources/fonts" "$DST/Resources/"
python3 "$ROOT/tools/extrair-framework.py" --reparar "$DSTAPP" >/dev/null || true

SWIFT=$MOS/usr/lib/swift/libswiftCore.dylib
if [ ! -f "$SWIFT" ]; then
    if [ -d "$ROOT/build/universal-swift" ]; then cp -a "$ROOT/build/universal-swift/." "$MOS/"; fi
    if [ ! -f "$SWIFT" ] && [ -d "$ROOT/build/swift/objc/dylib" ]; then cp -a "$ROOT/build/swift/objc/dylib/." "$MOS/"; fi
fi
TEMSWIFT=0
[ -f "$SWIFT" ] || [ "$COMSWIFT" = 1 ] && TEMSWIFT=1
copy_tree_limited() {
    local from=$1 rel=$2
    [ -d "$from" ] || return 0
    find "$from" -type f | while read -r f; do
        sz=$(stat -c %s "$f")
        if [ "$sz" -gt $((MAXMIB * 1048576)) ]; then continue; fi
        if [ "$TEMSWIFT" = 0 ] && [ "${f##*.}" = node ]; then
            if python3 -c 'import sys; d=open(sys.argv[1],"rb").read(262144); raise SystemExit(0 if b"/usr/lib/swift/libswiftCore.dylib" in d else 1)' "$f"; then
                echo "  sem $(basename "$f") (Swift)"
                continue
            fi
        fi
        relp=${f#"$from"}
        mkdir -p "$(dirname "$DST/Resources/$rel$relp")"
        cp -f "$f" "$DST/Resources/$rel$relp"
    done
}
if [ -z "$SUB" ]; then
    if [ -d "$SRC/Resources/app.asar.unpacked" ]; then copy_tree_limited "$SRC/Resources/app.asar.unpacked" app.asar.unpacked; fi
    if [ ! -f "$SRC/Resources/app.asar" ] && [ -d "$SRC/Resources/app" ]; then copy_tree_limited "$SRC/Resources/app" app; fi
else
    rm -rf "$DST/Resources/app" "$DST/Resources/app.asar" "$DST/Resources/app.asar.unpacked"
    mkdir -p "$DST/Resources/app"
    cp -a "$SUB/." "$DST/Resources/app/"
fi

mkdir -p "$MOS/Sistema/Biblioteca/LaunchDaemons"
printf '%s\n' "$NAME" > "$MOS/Sistema/Biblioteca/dyld-experimental"
SVC=$MOS/Sistema/Biblioteca/LaunchDaemons/com.mos.electron-teste.servico
if [ "$AUTOSTART" = 1 ]; then
    printf 'Programa=/Aplicativos/%s.app/Contents/MacOS/%s\nArgumentos=%s\nAtrasoMs=3000\n' "$NAME" "$EXE" "$ARGS0" > "$SVC"
else
    rm -f "$SVC"
fi
if [ -n "$ABRIR" ]; then
    printf 'Programa=/usr/bin/open\nArgumentos=%s\nAtrasoMs=%s\n' "$ABRIR" "$ABRIRMS" > "$MOS/Sistema/Biblioteca/LaunchDaemons/com.mos.zz-abrir-url.servico"
fi
if [ -n "$ABRIR2" ]; then
    printf 'Programa=/usr/bin/open\nArgumentos=%s\nAtrasoMs=%s\n' "$ABRIR2" "$ABRIR2MS" > "$MOS/Sistema/Biblioteca/LaunchDaemons/com.mos.zz-abrir-url2.servico"
fi
if [ -n "$COMAPP" ]; then cp -a "$COMAPP" "$MOS/Aplicativos/$(basename "$COMAPP")"; fi
if [ -n "$MOTOR" ]; then
    mkdir -p "$MOS/Sistema/Biblioteca/Horizonte"
    rm -rf "$MOS/Sistema/Biblioteca/Horizonte/Chromium.app"
    cp -a "$MOTOR" "$MOS/Sistema/Biblioteca/Horizonte/Chromium.app"
fi
if [ "$DIAG" = 1 ]; then
    for f in dyld-registrar stderr-serial despejar-threads; do printf '1\n' > "$MOS/Sistema/Biblioteca/$f"; done
fi
if [ "$CALOG" = 1 ]; then printf '1\n' > "$MOS/Sistema/Biblioteca/ca-log"; else rm -f "$MOS/Sistema/Biblioteca/ca-log"; fi
if [ -n "$TRACE" ]; then printf '%s\n' "${TRACE//;/$'\n'}" > "$MOS/Sistema/Biblioteca/diag-tracepoints"; else rm -f "$MOS/Sistema/Biblioteca/diag-tracepoints"; fi
printf '%s\n' 'mos-stage-linux-ok' > "$MOS/Sistema/Biblioteca/mos-stage-linux.txt"

if [ "$CURSOR" = 1 ]; then
    BIN=$MOS/usr/bin
    OPT=$MOS/usr/local/opt/git
    mkdir -p "$BIN" "$MOS/usr/local/bin" "$OPT/bin"
    cp -f "$ROOT/tools/brew-shims/which" "$BIN/which"
    cp -f "$ROOT/tools/brew-shims/security" "$BIN/security"
    chmod +x "$BIN/which" "$BIN/security"
    if [ -n "$BOTTLES" ] && [ -f "$BOTTLES/git/bin/git" ]; then
        cp -f "$BOTTLES/git/bin/git" "$OPT/bin/git"
        chmod +x "$OPT/bin/git"
        if [ -d "$BOTTLES/git/libexec/git-core" ]; then
            mkdir -p "$OPT/libexec/git-core"
            main=$(stat -c %s "$BOTTLES/git/bin/git")
            for f in "$BOTTLES/git/libexec/git-core"/*; do
                [ -f "$f" ] || continue
                base=$(basename "$f")
                sz=$(stat -c %s "$f")
                if [ "$base" != git ] && [ "$sz" = "$main" ]; then
                    printf '%s' '#mOS-link:/usr/local/opt/git/bin/git' > "$OPT/libexec/git-core/$base"
                else
                    cp -f "$f" "$OPT/libexec/git-core/$base"
                fi
            done
        fi
    else
        cp -f "$ROOT/tools/brew-shims/git" "$OPT/bin/git"
        chmod +x "$OPT/bin/git"
    fi
    printf '%s' '#mOS-link:/usr/local/opt/git/bin/git' > "$MOS/usr/local/bin/git"
    printf '%s' '#mOS-link:/usr/local/opt/git/bin/git' > "$BIN/git"
fi

OVT=$OUTABS/overlay-teste
rm -rf "$OVT"
mkdir -p "$OVT/mOS/Sistema/Biblioteca/Preferencias"
printf '%s\n' 'imagem de teste: o live abre na sessao do Visitante, sem o assistente' > "$OVT/mOS/Sistema/Biblioteca/Preferencias/com.mos.teste.sem-assistente"
if [ -n "$NAV" ]; then
    pref=$OVT/mOS/Users/Visitante/Library/Preferences/com.apple.LaunchServices
    mkdir -p "$pref"
    python3 - "$pref/com.apple.launchservices.secure.plist" "$NAV" <<'PY'
import sys
nav = sys.argv[2]
body = []
for s in ("http", "https"):
    body.append("\t\t<dict>\n\t\t\t<key>LSHandlerURLScheme</key>\n\t\t\t<string>%s</string>\n\t\t\t<key>LSHandlerRoleAll</key>\n\t\t\t<string>%s</string>\n\t\t</dict>" % (s, nav))
open(sys.argv[1], "w").write('<?xml version="1.0" encoding="UTF-8"?>\n<plist version="1.0">\n<dict>\n\t<key>LSHandlers</key>\n\t<array>\n' + "\n".join(body) + "\n\t</array>\n</dict>\n</plist>\n")
PY
fi
if [ ${#ARGSAPP[@]} -gt 0 ]; then
    printf '%s\n' "${ARGSAPP[@]}" > "$OVT/mOS/Sistema/Biblioteca/Preferencias/com.mos.argumentos.cfg"
fi

if [ "$SEMIMG" = 1 ]; then
    echo "OK: app em $DSTAPP (sem mkimg)"
    exit 0
fi

MK=$OUTABS/mkimg
if [ ! -x "$MK" ] || [ "$ROOT/tools/mkimg.c" -nt "$MK" ] || [ "$ROOT/kernel/include/ed25519_impl.h" -nt "$MK" ]; then
    CC=${CC:-}
    if [ -z "$CC" ]; then
        for c in cc gcc clang; do command -v "$c" >/dev/null 2>&1 && CC=$c && break; done
    fi
    if [ -z "$CC" ]; then echo "mkimg: não há cc, gcc nem clang para compilar tools/mkimg.c" >&2; exit 1; fi
    echo "==> Building mkimg (Linux, $CC)"
    "$CC" -O2 "$ROOT/tools/mkimg.c" -o "$MK" || { echo "mkimg FAILED: $CC não gerou $MK" >&2; exit 1; }
fi
KEY=${MOS_CHAVE_ATUALIZACAO:-$ROOT/tools/chave-atualizacao.semente}
"$MK" "$OUTABS/mOS.img" "$ESP" "$SIZE" --test-overlay "$OVT" --key "$KEY"
echo "OK: $OUTABS/mOS.img"
