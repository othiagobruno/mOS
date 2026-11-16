#!/bin/bash
# mOS - porte para Linux de tools/build-icu.ps1: ICU4C 74.2 (Unicode License, permissiva) como a libicucore do macOS.
# Saída: build/icu/libicucore.A.dylib (Mach-O x86_64, install name /usr/lib/libicucore.A.dylib), build/icu/icudt74l.dat
# (dados, que o tools/build.sh põe em /usr/share/icu) e build/icu/LICENSE.
#
# A libicucore da Apple exporta a API C do ICU sem sufixo de versão (uloc_open, não uloc_open_74): U_DISABLE_RENAMING.
# Uma biblioteca com common + i18n (U_COMBINED_IMPLEMENTATION) e o stubdata; os dados vêm do .dat em /usr/share/icu.
# A Foundation do libSystem (NSLocale, NSCalendar, formatters, transformações de NSString) chama esta biblioteca.
#
#   tools/build-icu.sh [--force] [--jobs N]     (padrão: 2 jobs, regra de RAM do time)
# MOS_ARCH=arm64: a fatia arm64 em build/icu-arm64 (processo arm64 do tradutor, docs/runtime-arm64-plano.md), ligada na
# libc++ arm64 de build/libcxx-arm64; o .dat é o mesmo (little-endian nas duas). O tools/build.sh junta com o lipo-mos.py.
# Fonte: .tools/icu-dl/icu4c-74_2-src.tgz (baixa com SHA-256 conferido) ou MOS_ICU_TGZ.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
force=0; jobs=2
while [ $# -gt 0 ]; do
    case "$1" in --force) force=1 ;; --jobs) shift; jobs=$1 ;; esac
    shift
done
zig=${MOS_ZIG:-}
if [ -z "$zig" ]; then for z in "$root"/.tools/zig-*/zig; do [ -x "$z" ] && { zig=$z; break; }; done; fi
[ -z "$zig" ] && zig=$(command -v zig || true)
[ -x "${zig:-/}" ] || { echo "build-icu: falta o zig" >&2; exit 1; }
case "${MOS_ARCH:-x86_64}" in
    x86_64) tgt=x86_64-macos.12.0; out="$root/build/icu"; cxx="$root/build/libcxx/libc++.1.dylib" ;;
    arm64) tgt=aarch64-macos.12.0; out="$root/build/icu-arm64"; cxx="$root/build/libcxx-arm64/libc++.1.dylib" ;;
    *) echo "MOS_ARCH=$MOS_ARCH: use x86_64 ou arm64" >&2; exit 1 ;;
esac
dylib="$out/libicucore.A.dylib"
dat="$out/icudt74l.dat"
if [ -s "$dylib" ] && [ -s "$dat" ] && [ $force = 0 ]; then exit 0; fi

ICU_URL=https://github.com/unicode-org/icu/releases/download/release-74-2/icu4c-74_2-src.tgz
ICU_SHA=68db082212a96d6f53e35d60f47d38b962e9f9d207a74cfac78029ae8ff5e08c
dl="$root/.tools/icu-dl"
tgz=${MOS_ICU_TGZ:-$dl/icu4c-74_2-src.tgz}
src="$dl/icu/source"
if [ ! -f "$src/common/uloc.cpp" ]; then
    # shellcheck source=tools/baixar.sh
    . "$root/tools/baixar.sh"
    mkdir -p "$dl"
    if [ ! -f "$tgz" ] || [ "$(sha256_de "$tgz")" != "$ICU_SHA" ]; then
        baixar "$ICU_URL" "$tgz" "$ICU_SHA" || { echo "build-icu: download do ICU falhou" >&2; exit 1; }
    fi
    tar xzf "$tgz" -C "$dl" || exit 1
fi
if [ ! -f "$cxx" ]; then MOS_ZIG="$zig" bash "$root/tools/build-libcxx.sh" "$root/build/libcxx" || exit 1; fi
mkdir -p "$out/obj" "$out/inc"
cfg="$out/icu_mos_config.h"
cat > "$cfg" <<'H'
/* mOS: o ICU como a libicucore do macOS (tools/build-icu.sh) */
#define U_COMBINED_IMPLEMENTATION 1      /* common + i18n numa biblioteca, tudo exportado */
#define U_DISABLE_RENAMING 1             /* uloc_open, não uloc_open_74 (como a libicucore da Apple) */
#define U_USING_ICU_NAMESPACE 0
#define U_HAVE_STRTOD_L 0                /* sem strtod_l/newlocale do libSystem */
#define U_ENABLE_DYLOAD 0
#define ICU_DATA_DIR "/usr/share/icu"    /* icudt74l.dat, mapeado com mmap */
H
# o putil.cpp inclui <tzfile.h> no macOS (TZDIR e TZDEFAULT); o zig não traz esse cabeçalho
printf '#define TZDIR "/usr/share/zoneinfo"\n#define TZDEFAULT "/etc/localtime"\n' > "$out/inc/tzfile.h"

# lista "fonte objeto define", um por linha; recompila só o que mudou
list="$out/fontes.txt"
: > "$list"
for s in "$src"/common/*.cpp "$src"/i18n/*.cpp "$src"/stubdata/stubdata.cpp; do
    d=$(basename "$(dirname "$s")")
    o="$out/obj/${d}_$(basename "$s" .cpp).o"
    impl=-DU_COMMON_IMPLEMENTATION; [ "$d" = i18n ] && impl=-DU_I18N_IMPLEMENTATION
    if [ $force = 1 ] || [ ! -s "$o" ] || [ "$s" -nt "$o" ]; then printf '%s %s %s\n' "$s" "$o" "$impl" >> "$list"; fi
done
export zig tgt cfg out src
compile_one() {
    "$zig" c++ -target "$tgt" -std=c++17 -O2 -g0 -fPIC -fvisibility=default -Wno-deprecated-declarations \
        -include "$cfg" "-I$out/inc" "-I$src/common" "-I$src/i18n" "$3" -c "$1" -o "$2" 2> "$2.err" || { cat "$2.err" | head -5; rm -f "$2"; return 1; }
    rm -f "$2.err"
}
export -f compile_one
n=$(wc -l < "$list")
echo "==> libicucore.A.dylib (ICU4C 74.2): $n fontes, $jobs jobs"
if [ "$n" -gt 0 ]; then
    xargs -P "$jobs" -L 1 bash -c 'compile_one "$0" "$1" "$2"' < "$list"
fi
objs=()
for s in "$src"/common/*.cpp "$src"/i18n/*.cpp "$src"/stubdata/stubdata.cpp; do
    d=$(basename "$(dirname "$s")")
    o="$out/obj/${d}_$(basename "$s" .cpp).o"
    [ -s "$o" ] || { echo "build-icu: falhou $o" >&2; exit 1; }
    objs+=("$o")
done
printf '"%s"\n' "${objs[@]}" > "$out/link.rsp"
# a libc++ do disco (o mesmo arquivo que vai para /usr/lib/libc++.1.dylib), não a estática do zig
"$zig" c++ -target "$tgt" -shared -nostdlib++ -o "$dylib" "@$out/link.rsp" "$cxx" \
    -Wl,-install_name,/usr/lib/libicucore.A.dylib -Wl,-compatibility_version,1.0.0 -Wl,-current_version,74.2.0 || exit 1
[ -s "$dylib" ] || { echo "build-icu: link falhou" >&2; exit 1; }
cp -f "$src/data/in/icudt74l.dat" "$dat"
cp -f "$dl/icu/LICENSE" "$out/LICENSE" 2>/dev/null || true
echo "OK: $dylib ($(( $(stat -c %s "$dylib") / 1024 )) KiB) e icudt74l.dat ($(( $(stat -c %s "$dat") / 1024 )) KiB)"
