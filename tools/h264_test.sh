#!/bin/bash
# Teste de host do decodificador H.264 (OpenH264 2.6.0 com as correções do mOS, userland/third_party/openh264) contra o
# ffmpeg: cada fluxo do x264 tem de sair bit-exato (I420 inteiro). No modo "ordem de decodificação" (o do VideoToolbox)
# os mesmos quadros têm de sair, um por quadro de entrada. Saída: "h264: N/N ok".
# Pesado (compila o OpenH264 inteiro e usa o ffmpeg): só pelo Shared/mos-tools/bin/mos-pesado, nunca em rotina
# (mos-regras-de-ram.md, item 10). Rodado por inteiro em 16/11 (44/44) antes da regra.
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
# AVPlayerItemVideoOutput e AVPlayerLayer (avplayervideo.c de verdade, runtime em volta substituído)
gcc -std=gnu11 -O1 -w -fno-builtin -I"$R/userland/libsystem/kshim" -I"$R/userland/libsystem" -I"$R/kernel/include" \
    -c "$R/userland/libsystem/avplayervideo.c" -o "$T/apv.o" && gcc -O1 -w -I"$R/userland/libsystem" -c "$R/tools/avplayervideo_test.c" -o "$T/apvt.o" && \
    g++ "$T/apvt.o" "$T/apv.o" "$T/avv.o" "$T/mp4d.o" "$T/glue.o" "$T"/o/*.o -lpthread -Wl,--unresolved-symbols=ignore-all -o "$T/apvt" || exit 1
n=$((n+1))
ffmpeg $F $A -c:v libx264 -profile:v main -bf 0 -pix_fmt yuv420p "$T/pv.mp4" && ffmpeg $F -i "$T/pv.mp4" -fps_mode passthrough -f rawvideo -pix_fmt yuv420p "$T/pv.ref"
r=$("$T/apvt" "$T/pv.mp4" "$T/pv.ref" 2>/dev/null); echo "$r"; case "$r" in "avplayervideo ok"*) ok=$((ok+1));; esac
# codificador H.264 (OpenH264 pela casca cx_h264enc_*): perfil, taxa, IDRs, PSNR e o nosso decodificador bit-exato ao ffmpeg
OE="-I$O/api/wels -I$O/common/inc -I$O/encoder/core/inc -I$O/encoder/plus/inc -I$O/processing/interface"
for d in "$O"/processing/src/*/; do OE="$OE -I$d"; done
mkdir -p "$T/oe"
ls "$O"/encoder/core/src/*.cpp "$O"/encoder/plus/src/*.cpp "$O"/processing/src/*/*.cpp | \
    xargs -P2 -I{} sh -c 'g++ -O2 -w '"$OE"' -c {} -o '"$T"'/oe/e_$(basename {} .cpp).o' || exit 1
g++ -O1 -w -fno-exceptions -fno-rtti $OE -I"$R/userland/host-audio" -c "$R/userland/host-audio/h264enc_glue.cpp" -o "$T/eglue.o" || exit 1
gcc -O1 -w -I"$R/userland/host-audio" -c "$R/tools/h264enc_test.c" -o "$T/et.o" || exit 1
COMMON_O=$(ls "$T"/o/*.o | grep -E "/(WelsThread|WelsTaskThread|WelsThreadLib|WelsThreadPool|common_tables|copy_mb|cpu|crt_util_safe_x|deblocking_common|expand_pic|intra_pred_common|mc|memory_align|sad_common|utils|welsCodecTrace)\.o")
g++ "$T/et.o" "$T/eglue.o" "$T"/oe/*.o $COMMON_O -lpthread -lm -o "$T/et" || exit 1
ffmpeg $F $A -f rawvideo -pix_fmt yuv420p "$T/esrc.yuv"
enc() { # perfil bitrate keyint forçar perfil-esperado idrs-esperados
    n=$((n+1))
    local f="$T/enc_$1_$2_$4.264" r ps
    r=$("$T/et" "$T/esrc.yuv" 352 288 75 25 $1 $2 $3 $4 "$f" 2>/dev/null)
    local prof; prof=$(ffprobe -v error -show_entries stream=profile -of csv=p=0 "$f")
    ffmpeg $F -i "$f" -f rawvideo -pix_fmt yuv420p "$T/enc.ref"
    ps=$(ffmpeg -hide_banner -f rawvideo -pix_fmt yuv420p -s 352x288 -i "$T/esrc.yuv" -f rawvideo -pix_fmt yuv420p -s 352x288 -i "$T/enc.ref" -lavfi '[0][1]psnr' -f null - 2>&1 | grep -o 'average:[0-9.]*' | cut -d: -f2)
    "$T/dec" "$f" "$T/enc.yuv" 2>/dev/null
    local kb; kb=$(echo "$r" | awk '{print $4}')
    local ok_=1
    case "$r" in "h264enc ok"*) ;; *) ok_=0;; esac
    [ "$prof" = "$5" ] || ok_=0
    echo "$r" | grep -q "idr:$6\$" || ok_=0
    awk -v p="$ps" 'BEGIN{exit !(p > 38)}' || ok_=0
    if [ "$2" -gt 0 ]; then awk -v k="$kb" -v t="$2" 'BEGIN{exit !(k > t/1000*0.8 && k < t/1000*1.2)}' || ok_=0; fi
    cmp -s "$T/enc.ref" "$T/enc.yuv" || ok_=0
    echo "codificador $5 $2 bit/s: $r, PSNR $ps dB, nosso decodificador $(cmp -s "$T/enc.ref" "$T/enc.yuv" && echo bit-exato || echo DIFERE) -> $([ $ok_ = 1 ] && echo ok || echo FALHA)"
    [ $ok_ = 1 ] && ok=$((ok+1))
}
enc 66 600000 25 -1 "Constrained Baseline" "0,25,50"
enc 77 600000 25 -1 "Main" "0,25,50"
enc 100 600000 25 37 "High" "0,25,37,62"
enc 100 0 0 -1 "High" "0"
# VTCompressionSession real (vscode_media.c, runtime substituído em tools/vtenc_test.c)
gcc -O1 -w -I"$R/userland/libsystem" -c "$R/userland/libsystem/h264sw.c" -o "$T/hsw.o" && gcc -O1 -w -c "$R/tools/vtenc_test.c" -o "$T/vet.o" || exit 1
DEC_O=$(ls "$T"/o/*.o | grep -vE "/(WelsThread|WelsTaskThread|WelsThreadLib|WelsThreadPool|common_tables|copy_mb|cpu|crt_util_safe_x|deblocking_common|expand_pic|intra_pred_common|mc|memory_align|sad_common|utils|welsCodecTrace)\.o")
g++ "$T/vet.o" "$T/vm.o" "$T/hsw.o" "$T/eglue.o" "$T/glue.o" $DEC_O $COMMON_O "$T"/oe/*.o -lpthread -lm -Wl,--unresolved-symbols=ignore-all -o "$T/vet" || exit 1
vte() { # perfil bitrate forçar perfil-esperado idrs
    n=$((n+1))
    local f="$T/vte_$1.264" r ps kb
    r=$("$T/vet" "$T/esrc.yuv" "$f" $1 $2 $3 2>/dev/null)
    ffmpeg $F -i "$f" -f rawvideo -pix_fmt yuv420p "$T/vte.yuv"
    ps=$(ffmpeg -hide_banner -f rawvideo -pix_fmt yuv420p -s 352x288 -i "$T/esrc.yuv" -f rawvideo -pix_fmt yuv420p -s 352x288 -i "$T/vte.yuv" -lavfi '[0][1]psnr' -f null - 2>&1 | grep -o 'average:[0-9.]*' | cut -d: -f2)
    kb=$(stat -c%s "$f" | awk '{printf "%.0f", $1*8/3/1000}')
    local prof; prof=$(ffprobe -v error -show_entries stream=profile -of csv=p=0 "$f")
    local ok_=0
    case "$r" in "vtenc ok amostras=75 idr=$5 "*) [ "$prof" = "$4" ] && awk -v p="$ps" -v k="$kb" -v t="$2" 'BEGIN{exit !(p > 38 && k > t/1000*0.8 && k < t/1000*1.2)}' && ok_=1;; esac
    echo "VTCompressionSession $4: $r, PSNR $ps dB, $kb kbit/s -> $([ $ok_ = 1 ] && echo ok || echo FALHA)"
    [ $ok_ = 1 ] && ok=$((ok+1))
}
vte High 600000 37 High "0,25,37,62"
vte Baseline 400000 -1 "Constrained Baseline" "0,25,50"
n=$((n+1))
r=$("$T/vet" --bgra "$T/vte_bgra.264" 2>/dev/null)
c=$(ffprobe -v error -show_entries stream=color_range,color_space -of csv=p=0 "$T/vte_bgra.264")
ffmpeg $F -i "$T/vte_bgra.264" -frames:v 1 -f rawvideo -pix_fmt rgb24 "$T/vte.rgb"
rgb=$(python3 -c "d=open('$T/vte.rgb','rb').read(); p=(144*352+176)*3; print(d[p], d[p+1], d[p+2])")
echo "VTCompressionSession BGRA: $r, VUI $c, RGB $rgb (origem 200 100 50)"
python3 -c "import sys; v=list(map(int,'$rgb'.split())); sys.exit(0 if '$c'=='tv,smpte170m' and all(abs(a-b)<=3 for a,b in zip(v,[200,100,50])) else 1)" && case "$r" in "vtenc ok"*) ok=$((ok+1));; esac
# dimensões do SPS (h264sw.c, usadas pela CMVideoFormatDescription) contra o ffprobe
gcc -O1 -w -I"$R/userland/libsystem" "$R/tools/h264sps_test.c" "$R/userland/libsystem/h264sw.c" -o "$T/sps" || exit 1
ffmpeg $F -f lavfi -i testsrc2=size=352x288:rate=25:duration=1 -c:v libx264 -flags +ildct+ilme -x264-params tff=1 -pix_fmt yuv420p -f h264 "$T/inter.264"
for f in baseline main high_b3_pyramid crop hd720 cavlc_high inter; do
    n=$((n+1))
    e=$(ffprobe -v error -show_entries stream=width,height -of csv=s=x:p=0 "$T/$f.264")
    g=$("$T/sps" "$T/$f.264")
    echo "SPS $f: $g (ffprobe $e)"
    [ "$g" = "$e" ] && ok=$((ok+1))
done
# gravação MP4/MOV (mp4mux.c): regravar faixas de verdade e o ffmpeg decodificar igual
gcc -O1 -w -I"$R/userland/libsystem" "$R/tools/mp4mux_test.c" "$R/userland/libsystem/mp4mux.c" "$R/userland/libsystem/mp4demux.c" -o "$T/mxt" || exit 1
ffmpeg $F -f lavfi -i testsrc2=size=320x240:rate=25:duration=3 -f lavfi -i "sine=f=440:d=3" -c:v libx264 -x264-params bframes=3 -pix_fmt yuv420p -c:a aac -b:a 128k "$T/mx_in.mp4"
ffmpeg $F -f lavfi -i "sine=f=440:d=3" -c:a libopus "$T/mx_op.mp4"
for c in "mx_in.mp4 mx_a.mp4 mp4 0" "mx_in.mp4 mx_b.mp4 mp4 1" "mx_in.mp4 mx_c.mov mov 0" "mx_in.mp4 mx_d.mov mov 1" "mx_op.mp4 mx_e.mp4 mp4 1"; do
    set -- $c
    n=$((n+1))
    r=$("$T/mxt" "$T/$1" "$T/$2" $3 $4)
    ffmpeg -v error -y -i "$T/$1" -f framemd5 "$T/mx1.md5" </dev/null; ffmpeg -v error -y -i "$T/$2" -f framemd5 "$T/mx2.md5" </dev/null
    if cmp -s <(grep -v "^#" "$T/mx1.md5") <(grep -v "^#" "$T/mx2.md5"); then d=igual; else d=DIFERE; fi
    echo "$r; ffmpeg decodifica $d"
    case "$r" in "mp4mux ok"*) [ $d = igual ] && ok=$((ok+1));; esac
done
echo "h264: $ok/$n ok"
[ "$ok" = "$n" ]
