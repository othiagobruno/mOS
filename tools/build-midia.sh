#!/bin/bash
# Codecs de mídia do libSystem: libopus 1.5.2 (C), FDK AAC 2.0.3 (C++ sem exceções/RTTI/libc++) e as cascas
# opus_glue.c / aac_glue.cpp. Gera objetos freestanding em <saída>/midia e a lista em <saída>/midia/objs.txt.
# Os símbolos internos dos codecs saem com visibilidade hidden: o lld os torna locais e o carregador do
# kernel (que só exporta GLOBAL) não os mostra aos apps. Só cx_opus_* e cx_aac_* ficam visíveis.
# uso: tools/build-midia.sh <saída> <zig> <zigdir> [jobs] [alvo: x86_64|aarch64]
set -eu
OUT=$1; ZIG=$2; ZIGDIR=$3; JOBS=${4:-2}; ARCH=${5:-x86_64}
R=$(cd "$(dirname "$0")/.." && pwd)
D="$OUT/midia-$ARCH"; mkdir -p "$D"
SH="$R/userland/host-audio/shim"; OP="$R/userland/third_party/opus"; FD="$R/userland/third_party/fdk-aac"
CF=(-target "$ARCH-freestanding-none" -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -fpie -w)
[ "$ARCH" = x86_64 ] && CF+=(-mno-red-zone)
OPI=(-DHAVE_CONFIG_H -I"$OP" -I"$OP/include" -I"$OP/celt" -I"$OP/silk" -I"$OP/silk/fixed")
FDI=(); for d in "$FD"/*/include "$FD"/*/src; do FDI+=(-I"$d"); done
jobs_list=()
for f in $(cat "$OP/SOURCES.txt"); do
    jobs_list+=("c|$OP/$f|$D/op_$(basename "$f" .c).o")
done
while IFS= read -r f; do
    jobs_list+=("cpp|$FD/$f|$D/fdk_$(echo "$f" | tr / _).o")
done < <(cd "$FD" && find . -mindepth 3 -maxdepth 3 -path './*/src/*.cpp' | sed 's|^\./||' | sort)
one() {
    IFS='|' read -r kind src obj <<< "$1"
    if [ "$kind" = c ]; then
        "$ZIG" cc "${CF[@]}" -fvisibility=hidden "${OPI[@]}" -I"$SH" -isystem "$ZIGDIR/lib/include" -c "$src" -o "$obj"
    else
        "$ZIG" c++ "${CF[@]}" -std=c++11 -fno-exceptions -fno-rtti -nostdinc++ -fno-threadsafe-statics -fvisibility=hidden \
            "${FDI[@]}" -I"$SH" -isystem "$ZIGDIR/lib/include" -c "$src" -o "$obj"
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
"$ZIG" cc "${CF[@]}" -Wall "${OPI[@]}" -I"$R/userland/libsystem" -I"$SH" -isystem "$ZIGDIR/lib/include" \
    -c "$R/userland/host-audio/opus_glue.c" -o "$D/opus_glue.o"
"$ZIG" c++ "${CF[@]}" -std=c++11 -fno-exceptions -fno-rtti -nostdinc++ -fno-threadsafe-statics \
    "${FDI[@]}" -I"$R/userland/host-audio" -I"$SH" -isystem "$ZIGDIR/lib/include" \
    -c "$R/userland/host-audio/aac_glue.cpp" -o "$D/aac_glue.o"
ls "$D"/*.o > "$D/objs.txt"
echo "build-midia: $(wc -l < "$D/objs.txt") objetos ($ARCH)"
