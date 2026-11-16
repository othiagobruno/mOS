#!/bin/bash
# Teste de host de tools/lipo-mos.py e tools/verificar-imagem-arm64.py (docs/runtime-arm64-plano.md).
# Compila dylibs Mach-O de verdade com o zig (x86_64-macos e aarch64-macos), junta com o lipo-mos.py, monta uma ESP
# mínima e confere que o verificador acusa exatamente o que falta. Não sobe QEMU, não roda o build.
#   tools/arm64-imagem-teste.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-$(command -v zig)}
T=$(mktemp -d /tmp/arm64-imagem-teste.XXXXXX)
trap 'rm -rf "$T"' EXIT
LIPO=(python3 -I "$ROOT/tools/lipo-mos.py")
VER=(python3 -I "$ROOT/tools/verificar-imagem-arm64.py")
falhas=0
ok() { echo "ok: $1"; }
falha() { echo "FALHA: $1"; falhas=$((falhas + 1)); }

printf 'int fa(void){return 1;}\n' > "$T/a.c"
printf 'int fa(void); int fb(void){return fa()+1;}\n' > "$T/b.c"
for arch in x86_64 aarch64; do
    "$ZIG" cc -target $arch-macos -shared "$T/a.c" -o "$T/liba-$arch.dylib" -Wl,-install_name,/usr/lib/libmosa.dylib
    "$ZIG" cc -target $arch-macos -shared "$T/b.c" -o "$T/libb-$arch.dylib" -Wl,-install_name,/usr/lib/libmosb.dylib \
        "$T/liba-$arch.dylib"
done

# lipo: cria, lista, extrai e recusa fatia repetida
"${LIPO[@]}" -o "$T/liba.u" "$T/liba-x86_64.dylib" "$T/liba-aarch64.dylib" > /dev/null
[ "$("${LIPO[@]}" --archs "$T/liba.u")" = "x86_64 arm64" ] && ok "universal x86_64 + arm64" || falha "--archs"
"${LIPO[@]}" --thin arm64 -o "$T/liba.thin" "$T/liba.u"
cmp -s "$T/liba.thin" "$T/liba-aarch64.dylib" && ok "--thin devolve a fatia byte a byte" || falha "--thin"
off=$(python3 -I -c 'import struct,sys; b=open(sys.argv[1],"rb").read(); print(struct.unpack_from(">I",b,8+20+8)[0] % 16384)' "$T/liba.u")
[ "$off" = 0 ] && ok "fatia arm64 alinhada a 16 KiB" || falha "alinhamento arm64 ($off)"
if "${LIPO[@]}" -o "$T/dup" "$T/liba.u" "$T/liba-aarch64.dylib" 2>/dev/null; then falha "aceitou duas fatias arm64"; else ok "recusa fatia repetida"; fi
# universal de universal: as fatias de entrada universal entram uma vez só
"${LIPO[@]}" -o "$T/liba.u2" "$T/liba.u" > /dev/null
cmp -s "$T/liba.u" "$T/liba.u2" && ok "reempacotar um universal dá o mesmo arquivo" || falha "reempacotar"

# ESP mínima
esp() {
    rm -rf "$T/esp"; mkdir -p "$T/esp/mOS/Sistema/Biblioteca/a64" "$T/esp/mOS/usr/lib" "$T/esp/mOS/Aplicativos/X.app"
    printf '\177ELF\002\001\001\000\000\000\000\000\000\000\000\000\003\000\076\000' > "$T/esp/mOS/Sistema/Biblioteca/a64/a64rt.mos"
    printf '\177ELF\002\001\001\000\000\000\000\000\000\000\000\000\003\000\267\000' > "$T/esp/mOS/Sistema/Biblioteca/a64/libSystem.mos"
    # bibliotecas de app de terceiros não entram na conta
    cp "$T/liba-x86_64.dylib" "$T/esp/mOS/Aplicativos/X.app/libapp.dylib"
}
run() { "${VER[@]}" "$T/esp" > "$T/out" 2>&1; }

esp
"${LIPO[@]}" -o "$T/esp/mOS/usr/lib/libmosa.dylib" "$T/liba-x86_64.dylib" "$T/liba-aarch64.dylib" > /dev/null
"${LIPO[@]}" -o "$T/esp/mOS/usr/lib/libmosb.dylib" "$T/libb-x86_64.dylib" "$T/libb-aarch64.dylib" > /dev/null
if run; then ok "ESP completa passa"; else falha "ESP completa"; cat "$T/out"; fi

esp
cp "$T/liba-x86_64.dylib" "$T/esp/mOS/usr/lib/libmosa.dylib"
"${LIPO[@]}" -o "$T/esp/mOS/usr/lib/libmosb.dylib" "$T/libb-x86_64.dylib" "$T/libb-aarch64.dylib" > /dev/null
if run; then falha "não acusou libmosa só x86_64"; else
    grep -q 'usr/lib/libmosa.dylib: sem fatia arm64' "$T/out" && grep -q 'libmosb.dylib (arm64): depende de /usr/lib/libmosa.dylib, que não tem fatia arm64' "$T/out" \
        && ok "acusa a biblioteca só x86_64 e quem depende dela" || { falha "mensagens"; cat "$T/out"; }
fi
grep -q libapp "$T/out" && falha "contou biblioteca de /Aplicativos" || ok "ignora /Aplicativos"

esp
"${LIPO[@]}" -o "$T/esp/mOS/usr/lib/libmosb.dylib" "$T/libb-x86_64.dylib" "$T/libb-aarch64.dylib" > /dev/null
if run; then falha "não acusou dependência ausente"; else
    grep -q 'depende de /usr/lib/libmosa.dylib, que não está na imagem' "$T/out" && ok "acusa dependência ausente" || { falha "ausente"; cat "$T/out"; }
fi

esp
rm "$T/esp/mOS/Sistema/Biblioteca/a64/libSystem.mos"
if run; then falha "não acusou libSystem arm64 ausente"; else
    grep -q 'libSystem.mos: não está na imagem' "$T/out" && ok "acusa libSystem arm64 ausente" || { falha "libSystem"; cat "$T/out"; }
fi

# nomes que o libSystem substitui (lidos de kernel/compat/macho.c) não precisam de arquivo
python3 -I - "$ROOT" <<'PY' && ok "libsystem_dylib_ok lida do macho.c" || falha "libsystem_dylib_ok"
import importlib.util, sys
s = importlib.util.spec_from_file_location("v", sys.argv[1] + "/tools/verificar-imagem-arm64.py")
v = importlib.util.module_from_spec(s); s.loader.exec_module(v)
e, p = v.libsystem_substitui(sys.argv[1])
assert "/usr/lib/libSystem.B.dylib" in e and "/usr/lib/libc++.1.dylib" in e, e
assert v.substituido("/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit", e, p)
assert v.substituido("/System/iOSSupport/System/Library/Frameworks/Foundation.framework/Foundation", e, p)
assert not v.substituido("/usr/lib/libmosa.dylib", e, p)
PY

if [ $falhas = 0 ]; then echo "arm64-imagem-teste: tudo ok"; else echo "arm64-imagem-teste: $falhas falha(s)"; exit 1; fi
