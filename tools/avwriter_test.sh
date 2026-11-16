#!/bin/bash
# Teste de host do núcleo do AVAssetWriter (avwcore.c + mp4mux.c): os arquivos gravados pelo tools/avwcore_test.c (H.264
# sintético I_PCM/P-skip e LPCM, sem codificador) conferidos com o ffprobe (pacotes, início, rotação, duração) e
# decodificados pelo ffmpeg (vídeo bit a bit contra a imagem gerada, PCM byte a byte). Leve; rode pelo
# Shared/mos-tools/bin/mos-pesado. Saída: "avwriter: N/N ok"
set -u
R=$(cd "$(dirname "$0")/.." && pwd); T=${AVW_TMP:-/tmp/avw-test}; rm -rf "$T"; mkdir -p "$T"
L="$R/userland/libsystem"
gcc -O1 -w -I"$L" "$R/tools/avwcore_test.c" "$L/avwcore.c" "$L/mp4mux.c" "$L/mp4demux.c" -lm -o "$T/t" || exit 1
ok=0; n=0
chk() { n=$((n+1)); if [ "$2" = 0 ]; then ok=$((ok+1)); echo "$1: ok"; else echo "$1: FALHA"; fi; }
r=$("$T/t" "$T"); echo "$r"; [ "$r" = "avwcore ok" ]; chk "núcleo (mp4demux do mOS)" $?
pk() { ffprobe -v error -select_streams v -show_entries packet=pts,dts,size,flags -of csv=p=0 "$1" | awk -F, '{print $1, $2, $3, (index($4,"K")?1:0)}'; }
pk "$T/a.mov" > "$T/a_p.txt"; cmp -s "$T/a_p.txt" "$T/a_v.txt"; chk "a.mov: pacotes de vídeo iguais ao ffprobe (vazio de 0,5 s)" $?
[ "$(ffprobe -v error -select_streams v -show_entries stream_side_data=rotation -of csv=p=0 "$T/a.mov")" = "-90" ]; chk "a.mov: rotação -90 (matriz do tkhd)" $?
ffmpeg -v error -noautorotate -i "$T/a.mov" -map 0:v -fps_mode passthrough -f rawvideo -pix_fmt yuv420p - > "$T/a_dec.yuv"; cmp -s "$T/a_dec.yuv" "$T/a.yuv"; chk "a.mov: 25 quadros decodificados bit a bit" $?
ffmpeg -v error -i "$T/a.mov" -map 0:a -f s16le - > "$T/a_dec.pcm"; cmp -s "$T/a_dec.pcm" "$T/a.pcm"; chk "a.mov: PCM idêntico" $?
d=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$T/b.mp4"); [ "$d" = "0.400000" ]; chk "b.mp4: sessão [1,2 s, 1,6 s) dura $d s" $?
pk "$T/d.mp4" > "$T/d_p.txt"; cmp -s "$T/d_p.txt" "$T/d_v.txt"; chk "d.mp4: quadros B (ctts, edit pelo menor pts) iguais ao ffprobe" $?
# a casca ObjC (avwriter.c) com o runtime, a CoreMedia e o VT substituídos (tools/avwriter_host_test.c)
K="-I$R/userland/libsystem/kshim -I$L -I$R/kernel/include"
gcc -std=gnu11 -O1 -w -fno-builtin $K -c "$L/avwriter.c" -o "$T/aw.o" && gcc -std=gnu11 -O1 -w -fno-builtin $K -c "$L/avf_core.c" -o "$T/avfc.o" && \
    gcc -std=gnu11 -O1 -w -I"$L" "$R/tools/avwriter_host_test.c" "$T/aw.o" "$T/avfc.o" "$L/avwcore.c" "$L/mp4mux.c" "$L/mp4demux.c" -lm -Wl,--unresolved-symbols=ignore-all -o "$T/ht" || exit 1
ffmpeg -v error -y -f lavfi -i sine=f=660:d=1.5 -c:a aac -b:a 96k "$T/in.m4a"
mkdir -p "$T/h"; r=$("$T/ht" "$T/h" "$T/in.m4a"); echo "$r"; [ "$r" = "avwriter_host ok" ]; chk "casca ObjC (estado, erros, adaptador, requestMediaData)" $?
H="$T/h"
[ "$(ffprobe -v error -select_streams v -show_entries stream_side_data=rotation -of csv=p=0 "$H/a.mov")" = "-90" ]; chk "a.mov: setTransform: vira rotação -90" $?
ffmpeg -v error -noautorotate -i "$H/a.mov" -map 0:v -fps_mode passthrough -f rawvideo -pix_fmt yuv420p - > "$H/a.yuv"; cmp -s "$H/a.yuv" "$H/a_ref.yuv"; chk "a.mov: adaptador + VTCompressionSession: 12 quadros bit a bit" $?
ffmpeg -v error -i "$H/a.mov" -map 0:a -f s16le - > "$H/a.pcm"; cmp -s "$H/a.pcm" "$H/a_ref.pcm"; chk "a.mov: float32 mono não intercalado -> LPCM 16 estéreo" $?
ffmpeg -v error -i "$H/b.mp4" -map 0:v -fps_mode passthrough -f rawvideo -pix_fmt yuv420p - > "$H/b.yuv"; cmp -s "$H/b.yuv" "$H/b_ref.yuv"; chk "b.mp4: H.264 comprimido (passthrough) bit a bit" $?
ffmpeg -v error -i "$H/b.mp4" -map 0:a -f s16le - > "$H/b.pcm"; [ -s "$H/b.pcm" ]; chk "b.mp4: AAC (passthrough) decodifica" $?
ffmpeg -v error -i "$H/c.wav" -f s16le - > "$H/c.pcm"; cmp -s "$H/c.pcm" "$H/c_ref.pcm"; chk "c.wav: estéreo -> mono pelo requestMediaDataWhenReadyOnQueue" $?
echo "avwriter: $ok/$n ok"
[ "$ok" = "$n" ]
