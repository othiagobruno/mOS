#!/bin/bash
# Codecs de mídia do libSystem: libopus 1.5.2 (C), FDK AAC 2.0.3 (C++ sem exceções/RTTI/libc++) e as cascas
# opus_glue.c / aac_glue.cpp; o decodificador e o codificador H.264 (OpenH264 2.6.0, C++) com h264_glue.cpp e h264enc_glue.cpp.
# Gera objetos freestanding em <saída>/midia e a lista em <saída>/midia/objs.txt.
# Os símbolos internos dos codecs saem com visibilidade hidden: o lld os torna locais e o carregador do
# kernel (que só exporta GLOBAL) não os mostra aos apps. Só cx_opus_*, cx_aac_*, cx_h264_* e cx_h264enc_* ficam visíveis.
# uso: tools/build-midia.sh <saída> <zig> <zigdir> [jobs] [alvo: x86_64|aarch64]
set -eu
OUT=$1; ZIG=$2; ZIGDIR=$3; JOBS=${4:-2}; ARCH=${5:-x86_64}
R=$(cd "$(dirname "$0")/.." && pwd)
D="$OUT/midia-$ARCH"; mkdir -p "$D"
SH="$R/userland/host-audio/shim"; SHM="$R/userland/host-audio/shim-midia"; OP="$R/userland/third_party/opus"; FD="$R/userland/third_party/fdk-aac"
if [ "$ARCH" = aarch64 ]; then
    # libSystem arm64 (tools/build-libsystem-arm64.sh): a ABI Darwin do triplo arm64-apple-macos11-elf (variádicos na
    # pilha, char com sinal, x18 reservado). Sem __APPLE__ (os codecs escolheriam cabeçalhos da Apple) e sem /usr/include.
    CF=(--target=arm64-apple-macos11-elf -mcpu=apple-m1 -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib
        -nostdlibinc -fPIC -ffixed-x18 -fsigned-char -fno-common -funwind-tables -U__APPLE__ -U__MACH__ -w)
    CC=(clang); CXX=(clang -x c++)   # o "zig cc" não aceita o triplo Darwin com contêiner ELF; o clang do zig aceita
else
    CF=(-target "$ARCH-freestanding-none" -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -fpie -w)
    [ "$ARCH" = x86_64 ] && CF+=(-mno-red-zone)
    CC=(cc); CXX=(c++)
fi
OPI=(-DHAVE_CONFIG_H -I"$OP" -I"$OP/include" -I"$OP/celt" -I"$OP/silk" -I"$OP/silk/fixed")
FDI=(); for d in "$FD"/*/include "$FD"/*/src; do FDI+=(-I"$d"); done
# H.264: OpenH264 corrigido (só o decodificador, sem assembly), cabeçalhos de sistema em shim-h264 (ABI Darwin)
OH="$R/userland/third_party/openh264/codec"; SHH="$R/userland/host-audio/shim-h264"
OHI=(-I"$OH/api/wels" -I"$OH/common/inc" -I"$OH/decoder/core/inc" -I"$OH/decoder/plus/inc")
# codificador (e o pré-processamento que ele usa): os nomes de arquivo repetem os do decodificador, daí ohe_/ohp_
OHE=(-I"$OH/api/wels" -I"$OH/common/inc" -I"$OH/encoder/core/inc" -I"$OH/encoder/plus/inc" -I"$OH/processing/interface")
for d in "$OH"/processing/src/*/; do OHE+=(-I"$d"); done
jobs_list=()
for f in $(cat "$OP/SOURCES.txt"); do
    jobs_list+=("c|$OP/$f|$D/op_$(basename "$f" .c).o")
done
while IFS= read -r f; do
    jobs_list+=("cpp|$FD/$f|$D/fdk_$(echo "$f" | tr / _).o")
done < <(cd "$FD" && find . -mindepth 3 -maxdepth 3 -path './*/src/*.cpp' | sed 's|^\./||' | sort)
for f in "$OH"/common/src/*.cpp "$OH"/decoder/core/src/*.cpp "$OH"/decoder/plus/src/*.cpp; do
    jobs_list+=("h264|$f|$D/oh_$(basename "$f" .cpp).o")
done
for f in "$OH"/encoder/core/src/*.cpp "$OH"/encoder/plus/src/*.cpp; do jobs_list+=("h264e|$f|$D/ohe_$(basename "$f" .cpp).o"); done
for f in "$OH"/processing/src/*/*.cpp; do jobs_list+=("h264e|$f|$D/ohp_$(basename "$f" .cpp).o"); done
one() {
    IFS='|' read -r kind src obj <<< "$1"
    if [ "$kind" = c ]; then
        "$ZIG" "${CC[@]}" "${CF[@]}" -fvisibility=hidden "${OPI[@]}" -I"$SH" -isystem "$ZIGDIR/lib/include" -c "$src" -o "$obj"
    elif [ "$kind" = h264e ]; then
        "$ZIG" "${CXX[@]}" "${CF[@]}" -std=c++11 -fno-exceptions -fno-rtti -nostdinc++ -fno-threadsafe-statics -fvisibility=hidden \
            "${OHE[@]}" -I"$SHH" -I"$SHM" -I"$SH" -isystem "$ZIGDIR/lib/include" -c "$src" -o "$obj"
    elif [ "$kind" = h264 ]; then
        "$ZIG" "${CXX[@]}" "${CF[@]}" -std=c++11 -fno-exceptions -fno-rtti -nostdinc++ -fno-threadsafe-statics -fvisibility=hidden \
            "${OHI[@]}" -I"$SHH" -I"$SHM" -I"$SH" -isystem "$ZIGDIR/lib/include" -c "$src" -o "$obj"
    else
        "$ZIG" "${CXX[@]}" "${CF[@]}" -std=c++11 -fno-exceptions -fno-rtti -nostdinc++ -fno-threadsafe-statics -fvisibility=hidden \
            "${FDI[@]}" -I"$SHM" -I"$SH" -isystem "$ZIGDIR/lib/include" -c "$src" -o "$obj"
    fi
}
fail=0
printf '%s\n' "${jobs_list[@]}" | while IFS= read -r j; do echo "$j"; done > "$D/jobs.txt"
# xargs com função: reexporta os arrays como texto
while IFS= read -r j; do
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n || fail=1; done
    one "$j" &
done < "$D/jobs.txt"
while [ "$(jobs -rp | wc -l)" -gt 0 ]; do wait -n || fail=1; done
[ "$fail" = 0 ] || { echo "build-midia: codec falhou" >&2; exit 1; }
"$ZIG" "${CC[@]}" "${CF[@]}" -Wall "${OPI[@]}" -I"$R/userland/libsystem" -I"$SH" -isystem "$ZIGDIR/lib/include" \
    -c "$R/userland/host-audio/opus_glue.c" -o "$D/opus_glue.o"
"$ZIG" "${CXX[@]}" "${CF[@]}" -std=c++11 -fno-exceptions -fno-rtti -nostdinc++ -fno-threadsafe-statics \
    "${FDI[@]}" -I"$R/userland/host-audio" -I"$SHM" -I"$SH" -isystem "$ZIGDIR/lib/include" \
    -c "$R/userland/host-audio/aac_glue.cpp" -o "$D/aac_glue.o"
"$ZIG" "${CXX[@]}" "${CF[@]}" -std=c++11 -fno-exceptions -fno-rtti -nostdinc++ -fno-threadsafe-statics \
    "${OHI[@]}" -I"$R/userland/host-audio" -I"$SHH" -I"$SHM" -I"$SH" -isystem "$ZIGDIR/lib/include" \
    -c "$R/userland/host-audio/h264_glue.cpp" -o "$D/h264_glue.o"
"$ZIG" "${CXX[@]}" "${CF[@]}" -std=c++11 -fno-exceptions -fno-rtti -nostdinc++ -fno-threadsafe-statics \
    "${OHE[@]}" -I"$R/userland/host-audio" -I"$SHH" -I"$SHM" -I"$SH" -isystem "$ZIGDIR/lib/include" \
    -c "$R/userland/host-audio/h264enc_glue.cpp" -o "$D/h264enc_glue.o"
ls "$D"/*.o > "$D/objs.txt"
echo "build-midia: $(wc -l < "$D/objs.txt") objetos ($ARCH)"
