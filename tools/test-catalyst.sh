#!/bin/bash
# Linux: a mesma amostra de tools/test-catalyst.ps1.
# A lista de asserções não está aqui: cada CHECK mora em userland/tests-catalyst/main.m,
# e os dois scripts compilam esse arquivo. O placar impresso é "catalyst: N/N ok".
# Precisa de zig no PATH (ou MOS_ZIG) e de build/libSystem.mos + build/machotest
# (comandos em docs/testes-linux.md).
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
zig=${MOS_ZIG:-zig}
t="$root/build/catalysttest"
install="/System/iOSSupport/System/Library/Frameworks/UIKit.framework/Versions/A/UIKit"
fw="$t$install"
if [[ ! -x "$root/build/machotest" || ! -f "$root/build/libSystem.mos" ]]; then
    echo "falta build/machotest ou build/libSystem.mos (docs/testes-linux.md)" >&2
    exit 1
fi
if ! command -v "$zig" >/dev/null 2>&1; then
    echo "zig não está no PATH" >&2
    exit 1
fi
mkdir -p "$t/obj" "$(dirname "$fw")" "$t/System/iOSSupport/System/Library/Frameworks/UIKit.framework"
objs=()
: >"$t/compilacao.txt"
for f in "$root"/userland/frameworks/UIKit/*.m; do
    b=$(basename "$f" .m)
    o="$t/obj/$b.o"
    if ! "$zig" cc -target x86_64-macos -O1 -c -x objective-c \
        -I"$root/userland/sdk/include" -I"$root/userland/frameworks/UIKit" \
        -Wno-objc-root-class "$f" -o "$o" >>"$t/compilacao.txt" 2>&1; then
        echo "compilação FALHOU ($b); ver $t/compilacao.txt" >&2
        exit 1
    fi
    objs+=("$o")
done
if ! "$zig" cc -target x86_64-macos -shared -o "$fw" \
    -Wl,-install_name,"$install" -Wl,-undefined,dynamic_lookup "${objs[@]}" >>"$t/compilacao.txt" 2>&1; then
    echo "link do UIKit FALHOU; ver $t/compilacao.txt" >&2
    exit 1
fi
cp -f "$fw" "$t/System/iOSSupport/System/Library/Frameworks/UIKit.framework/UIKit"
cp -f "$fw" "$t/UIKit-link.dylib"
if ! "$zig" cc -target x86_64-macos -O1 -fobjc-arc -x objective-c \
    -I"$root/userland/sdk/include" "$root/userland/tests-catalyst/main.m" -x none \
    "$t/UIKit-link.dylib" -o "$t/Catalyst" -Wl,-undefined,dynamic_lookup >>"$t/compilacao.txt" 2>&1; then
    echo "compilação do teste FALHOU; ver $t/compilacao.txt" >&2
    exit 1
fi
export MACHOTEST_WATCHDOG="${MACHOTEST_WATCHDOG:-8000}"
export MACHOTEST_DUMP="$t/tela.bmp"
set +e
"$root/build/machotest" "$root/build/libSystem.mos" "$t" "$t/Catalyst" >"$t/saida.txt" 2>&1
code=$?
set -e
grep -E 'catalyst:|FALHA |^  ok|exceção|não emulada|macho_load' "$t/saida.txt" || true
if grep -E -q 'catalyst: ([0-9]+)/\1 ok' "$t/saida.txt"; then
    exit 0
fi
echo "catalyst FALHOU (machotest saiu $code); saída em $t/saida.txt" >&2
exit 1
