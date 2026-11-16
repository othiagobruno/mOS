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
# a casca C cx_h264_* (a mesma que vai no libSystem) e o teste dela
g++ -O1 -w -fno-exceptions -fno-rtti $I -I"$R/userland/host-audio" -c "$R/userland/host-audio/h264_glue.cpp" -o "$T/glue.o" || exit 1
gcc -O1 -w -I"$R/userland/host-audio" -c "$R/tools/h264_glue_test.c" -o "$T/gt.o" && g++ "$T/gt.o" "$T/glue.o" "$T"/o/*.o -lpthread -o "$T/gt" || exit 1
F="-loglevel error -y"
A="-f lavfi -i testsrc2=size=352x288:rate=25:duration=3"
B="-f lavfi -i mandelbrot=size=320x240:rate=25,trim=duration=3"
C="-f lavfi -i testsrc=size=426x238:rate=30:duration=2"
L="-f lavfi -i life=size=320x240:rate=25:mold=10:ratio=0.5,trim=duration=3"
H="-f lavfi -i testsrc2=size=1280x720:rate=30:duration=1"
ok=0; n=0
t() { # nome entrada parâmetros do x264... (COR=faixa,matriz,primárias,transferência para conferir o VUI)
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
    local gl=FALHA
    "$T/gt" "$T/$nm.264" "$T/$nm.ref" ${COR:-} > "$T/$nm.gl" && gl=ok
    echo "$nm: exibição $ex, ordem de decodificação $dord, casca cx_h264 $gl $(grep -v '^glue ok' "$T/$nm.gl" | head -1)"
    [ "$ex" = bit-exato ] && [ "$dord" = ok ] && [ "$gl" = ok ] && ok=$((ok+1))
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
COR=1,1,1,1 t full709 "$A" -profile:v high -x264-params fullrange=on:colormatrix=bt709:colorprim=bt709:transfer=bt709
COR=0,2,2,2 t semcor "$A" -profile:v main
# VTDecompressionSession (vscode_media.c de verdade; o runtime em volta substituído em tools/vt_test.c), amostras de MP4
gcc -std=gnu11 -O1 -w -fno-builtin -I"$R/userland/libsystem/kshim" -I"$R/userland/libsystem" -I"$R/kernel/include" \
    -c "$R/userland/libsystem/vscode_media.c" -o "$T/vm.o" || exit 1
gcc -O1 -w -I"$R/userland/libsystem" -c "$R/userland/libsystem/mp4demux.c" -o "$T/mp4d.o" && gcc -O1 -w -I"$R/userland/libsystem" \
    -c "$R/tools/vt_test.c" -o "$T/vtt.o" || exit 1
g++ "$T/vtt.o" "$T/vm.o" "$T/mp4d.o" "$T/glue.o" "$T"/o/*.o -lm -lpthread -Wl,--unresolved-symbols=ignore-all -o "$T/vtt" || exit 1
v() { # nome entrada [full] parâmetros do x264...
    local nm=$1 src=$2 full=$3; shift 3
    n=$((n+1))
    ffmpeg $F $src -c:v libx264 -pix_fmt yuv420p "$@" "$T/$nm.mp4" && ffmpeg $F -i "$T/$nm.mp4" -fps_mode passthrough -f rawvideo -pix_fmt yuv420p "$T/$nm.vref"
    local r; r=$("$T/vtt" "$T/$nm.mp4" "$T/$nm.vref" $full 2>&1); echo "vt $nm: $r"
    case "$r" in "vt ok"*) ok=$((ok+1));; esac
}
v vt_high "$A" "" -profile:v high -x264-params bframes=3:b-pyramid=normal
v vt_720 "$H" "" -profile:v main
v vt_full709 "$A" full -profile:v high -x264-params fullrange=on:colormatrix=bt709:colorprim=bt709:transfer=bt709:bframes=2
v vt_baseline "$A" "" -profile:v baseline
# núcleo de vídeo do AVFoundation (avvideo.c): quadro no tempo t de MP4/MOV, BGRA, rotação e redução
gcc -O1 -w -I"$R/userland/libsystem" -c "$R/userland/libsystem/avvideo.c" -o "$T/avv.o" && gcc -O1 -w -I"$R/userland/libsystem" \
    -c "$R/tools/avvideo_test.c" -o "$T/avt.o" && g++ "$T/avt.o" "$T/avv.o" "$T/mp4d.o" "$T/glue.o" "$T"/o/*.o -lpthread -o "$T/avt" || exit 1
av() { # nome extensão entrada parâmetros...
    local nm=$1 ext=$2 src=$3; shift 3
    n=$((n+1))
    ffmpeg $F $src "$@" "$T/$nm.$ext" && ffmpeg $F -i "$T/$nm.$ext" -map 0:v -fps_mode passthrough -f rawvideo -pix_fmt yuv420p "$T/$nm.aref"
    local r; r=$("$T/avt" "$T/$nm.$ext" "$T/$nm.aref" 2>/dev/null); echo "avvideo $nm: $r"
    case "$r" in "avvideo ok"*) ok=$((ok+1));; esac
}
av av_gop12 mp4 "$A" -c:v libx264 -profile:v high -pix_fmt yuv420p -g 12 -x264-params bframes=3:b-pyramid=normal
av av_mov_audio mov "-f lavfi -i testsrc=size=320x240:rate=30:duration=3 -f lavfi -i sine=d=3" -c:v libx264 -profile:v main -g 30 -c:a aac -pix_fmt yuv420p
av av_720 mp4 "$H" -c:v libx264 -profile:v high -pix_fmt yuv420p -g 15
# codificador I_PCM (h264sw.c) com ida e volta pelo mesmo decodificador e pelo ffmpeg
gcc -std=c11 -O2 -w -c "$R/tools/vtcodec_test.c" -o "$T/vct.o" && gcc -std=gnu11 -O2 -w -I"$R/userland/libsystem" \
    -c "$R/userland/libsystem/h264sw.c" -o "$T/hsw.o" && g++ "$T/vct.o" "$T/hsw.o" "$T/glue.o" "$T"/o/*.o -lm -lpthread -o "$T/vct" || exit 1
n=$((n+1)); r=$(cd "$T" && ./vct 2>/dev/null | tail -1); echo "codificador I_PCM: $r"; [ "$r" = ok ] && ok=$((ok+1))
echo "h264: $ok/$n ok"
[ "$ok" = "$n" ]
