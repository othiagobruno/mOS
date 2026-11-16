#!/bin/bash
# Teste de host do decodificador H.264 (OpenH264 2.6.0 com as correções do mOS, userland/third_party/openh264) contra o
# ffmpeg: cada fluxo do x264 tem de sair bit-exato (I420 inteiro). No modo "ordem de decodificação" (o do VideoToolbox)
# os mesmos quadros têm de sair, um por quadro de entrada. Saída: "h264: N/N ok".
# Pesado (compila o decodificador e codifica os fluxos): rode dentro de flock /tmp/mos-pesado.lock.
set -u
R=$(cd "$(dirname "$0")/.." && pwd); T=${H264_TMP:-/tmp/h264-test}; mkdir -p "$T/o"
O="$R/userland/third_party/openh264/codec"
I="-I$O/api/wels -I$O/common/inc -I$O/decoder/core/inc -I$O/decoder/plus/inc"
ls "$O"/common/src/*.cpp "$O"/decoder/core/src/*.cpp "$O"/decoder/plus/src/*.cpp | \
    xargs -P2 -I{} sh -c 'g++ -O2 -w '"$I"' -c {} -o '"$T"'/o/$(basename {} .cpp).o' || exit 1
g++ -O1 -w $I "$R/tools/h264_test_dec.cpp" "$T"/o/*.o -lpthread -o "$T/dec" || exit 1
F="-loglevel error -y"
A="-f lavfi -i testsrc2=size=352x288:rate=25:duration=3"
B="-f lavfi -i mandelbrot=size=320x240:rate=25,trim=duration=3"
C="-f lavfi -i testsrc=size=426x238:rate=30:duration=2"
L="-f lavfi -i life=size=320x240:rate=25:mold=10:ratio=0.5,trim=duration=3"
H="-f lavfi -i testsrc2=size=1280x720:rate=30:duration=1"
ok=0; n=0
t() { # nome entrada parâmetros do x264...
    local nm=$1 src=$2; shift 2
    n=$((n+1))
    ffmpeg $F $src -c:v libx264 -pix_fmt yuv420p "$@" -f h264 "$T/$nm.264" || { echo "$nm: FALHA ao codificar"; return; }
    ffmpeg $F -i "$T/$nm.264" -f rawvideo -pix_fmt yuv420p "$T/$nm.ref"
    "$T/dec" "$T/$nm.264" "$T/$nm.yuv" 2>/dev/null
    "$T/dec" "$T/$nm.264" "$T/$nm.dec.yuv" decode 2>/dev/null
    local ex=FALHA dord=FALHA
    cmp -s "$T/$nm.ref" "$T/$nm.yuv" && ex=bit-exato
    python3 - "$T/$nm.ref" "$T/$nm.dec.yuv" "$(ffprobe -v error -show_entries stream=width,height -of csv=p=0 "$T/$nm.264")" <<'P' && dord=ok
import sys, hashlib
w, h = map(int, sys.argv[3].split(',')[:2]); fs = w * h * 3 // 2
a = open(sys.argv[1], 'rb').read(); b = open(sys.argv[2], 'rb').read()
fa = sorted(hashlib.md5(a[i:i + fs]).digest() for i in range(0, len(a), fs))
fb = sorted(hashlib.md5(b[i:i + fs]).digest() for i in range(0, len(b), fs))
sys.exit(0 if fa == fb and len(a) == len(b) else 1)
P
    echo "$nm: exibição $ex, ordem de decodificação $dord"
    [ "$ex" = bit-exato ] && [ "$dord" = ok ] && ok=$((ok+1))
}
t baseline "$A" -profile:v baseline
t main "$A" -profile:v main
t high "$A" -profile:v high
t high_b3_pyramid "$A" -profile:v high -x264-params bframes=3:b-pyramid=normal:weightb=1:8x8dct=1
t veryslow "$A" -profile:v high -preset veryslow -tune film
t cavlc_main "$A" -profile:v main -x264-params cabac=0:bframes=3
t cavlc_high "$A" -profile:v high -x264-params cabac=0:bframes=3:8x8dct=1
t direct_temporal "$A" -profile:v high -x264-params direct=temporal:bframes=3
t weightb "$B" -profile:v high -x264-params weightb=1:bframes=3:direct=auto
t weightp "$B" -profile:v high -x264-params weightp=2:bframes=2
t refs6_strict "$A" -profile:v high -x264-params ref=6:bframes=4:b-pyramid=strict
t crop "$C" -profile:v high -x264-params bframes=3
t life "$L" -profile:v main -x264-params bframes=3:ref=3
t badapt2 "$L" -profile:v high -x264-params b-adapt=2:bframes=8:direct=spatial
t nodeblock "$A" -profile:v high -x264-params no-deblock=1:bframes=2
t qp10 "$B" -profile:v high -qp 10 -x264-params bframes=2
t qp45 "$B" -profile:v high -qp 45 -x264-params bframes=2
t slices "$A" -profile:v high -x264-params slices=4:bframes=2
t zerolatency "$A" -profile:v main -preset ultrafast -tune zerolatency
t hd720 "$H" -profile:v high -x264-params bframes=3:ref=4
echo "h264: $ok/$n ok"
[ "$ok" = "$n" ]
