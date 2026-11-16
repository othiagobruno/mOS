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
ffmpeg -v error -y -f lavfi -i sine=f=660:d=1.5 -c:a aac -b:a 96k "$T/in.m4a"
# AVAssetExportSession (avexport.c): passthrough inteiro, cortado e só áudio, contra a decodificação da origem
gcc -std=gnu11 -O1 -w -I"$L" "$R/tools/avexport_test.c" "$L/avexport.c" "$L/avwcore.c" "$L/mp4mux.c" "$L/mp4demux.c" -o "$T/xt" || exit 1
X="$T/x"; mkdir -p "$X"; r=$("$T/xt" "$X" "$T/in.m4a"); echo "$r"; [ "$r" = "avexport ok" ]; chk "export: núcleo (corte no quadro-chave, cancelamento, intervalo vazio)" $?
dv() { ffmpeg -v error -i "$1" -map 0:v -fps_mode passthrough -f rawvideo -pix_fmt yuv420p - > "$2"; }
da() { ffmpeg -v error -i "$1" -map 0:a -f s16le - > "$2"; }
dv "$X/src.mp4" "$X/src.yuv"; da "$X/src.mp4" "$X/src.pcm"
dv "$X/out1.mov" "$X/o1.yuv"; da "$X/out1.mov" "$X/o1.pcm"
cmp -s "$X/src.yuv" "$X/o1.yuv" && cmp -s "$X/src.pcm" "$X/o1.pcm"; chk "export inteiro (MOV): vídeo e áudio idênticos à origem" $?
dv "$X/out2.mp4" "$X/o2.yuv"; da "$X/out2.mp4" "$X/o2.pcm"
python3 - "$X" "$(ffprobe -v error -select_streams a -show_entries stream=sample_rate,channels -of csv=p=0 "$X/src.mp4")" <<'P'; chk "export [0,5 s, 1,3 s): 20 quadros 13..32 e o áudio do trecho" $?
import sys, math
d = sys.argv[1]; rate, ch = map(int, sys.argv[2].split(','))
fs = 64 * 48 * 3 // 2
src = open(d + '/src.yuv', 'rb').read(); o = open(d + '/o2.yuv', 'rb').read()
okv = o == src[13 * fs:33 * fs]
a = open(d + '/src.pcm', 'rb').read(); b = open(d + '/o2.pcm', 'rb').read()
i0 = int(0.5 * rate) * 2 * ch; want = a[i0:i0 + int(0.8 * rate) * 2 * ch]
import array
x = array.array('h', want); y = array.array('h', b[:len(want)])
err = sum((p - q) ** 2 for p, q in zip(x, y)); sig = sum(p * p for p in x) or 1
snr = 10 * math.log10(sig / err) if err else 999
print('  vídeo', 'igual' if okv else 'DIFERE (%d bytes)' % len(o), '| áudio %d de %d amostras, SNR %.1f dB' % (len(b) // 2, len(want) // 2, snr))
sys.exit(0 if okv and abs(len(b) - len(want)) <= 1024 * 2 * ch and snr > 60 else 1)   # o ffmpeg não corta o fim do AAC no exato
P
[ "$(ffprobe -v error -show_entries stream=duration_ts -of csv=p=0 "$X/out2.mp4" | tr '\n' ' ')" = "20 35280 " ]; chk "export [0,5 s, 1,3 s): edit list de 0,8 s exata nas duas faixas" $?
[ -z "$(ffprobe -v error -select_streams v -show_entries stream=index -of csv=p=0 "$X/out3.m4a")" ] && da "$X/out3.m4a" "$X/o3.pcm" && cmp -s "$X/src.pcm" "$X/o3.pcm"; chk "export só áudio (M4A): sem vídeo, áudio idêntico" $?
# a casca ObjC (avwriter.c) com o runtime, a CoreMedia e o VT substituídos (tools/avwriter_host_test.c)
K="-I$R/userland/libsystem/kshim -I$L -I$R/kernel/include"
gcc -std=gnu11 -O1 -w -fno-builtin $K -c "$L/avwriter.c" -o "$T/aw.o" && gcc -std=gnu11 -O1 -w -fno-builtin $K -c "$L/avexport_objc.c" -o "$T/axo.o" && gcc -std=gnu11 -O1 -w -fno-builtin $K -c "$L/avf_core.c" -o "$T/avfc.o" && \
    gcc -std=gnu11 -O1 -w -I"$L" "$R/tools/avwriter_host_test.c" "$T/aw.o" "$T/axo.o" "$T/avfc.o" "$L/avexport.c" "$L/avwcore.c" "$L/mp4mux.c" "$L/mp4demux.c" -lm -lpthread -Wl,--unresolved-symbols=ignore-all -o "$T/ht" || exit 1
mkdir -p "$T/h"; r=$("$T/ht" "$T/h" "$T/in.m4a" "$X/src.mp4"); echo "$r"; [ "$r" = "avwriter_host ok" ]; chk "casca ObjC: AVAssetWriter (estado, erros, adaptador, requestMediaData) e AVAssetExportSession (presets, timeRange, thread, erros)" $?
H="$T/h"
[ "$(ffprobe -v error -select_streams v -show_entries stream_side_data=rotation -of csv=p=0 "$H/a.mov")" = "-90" ]; chk "a.mov: setTransform: vira rotação -90" $?
ffmpeg -v error -noautorotate -i "$H/a.mov" -map 0:v -fps_mode passthrough -f rawvideo -pix_fmt yuv420p - > "$H/a.yuv"; cmp -s "$H/a.yuv" "$H/a_ref.yuv"; chk "a.mov: adaptador + VTCompressionSession: 12 quadros bit a bit" $?
ffmpeg -v error -i "$H/a.mov" -map 0:a -f s16le - > "$H/a.pcm"; cmp -s "$H/a.pcm" "$H/a_ref.pcm"; chk "a.mov: float32 mono não intercalado -> LPCM 16 estéreo" $?
ffmpeg -v error -i "$H/b.mp4" -map 0:v -fps_mode passthrough -f rawvideo -pix_fmt yuv420p - > "$H/b.yuv"; cmp -s "$H/b.yuv" "$H/b_ref.yuv"; chk "b.mp4: H.264 comprimido (passthrough) bit a bit" $?
ffmpeg -v error -i "$H/b.mp4" -map 0:a -f s16le - > "$H/b.pcm"; [ -s "$H/b.pcm" ]; chk "b.mp4: AAC (passthrough) decodifica" $?
ffmpeg -v error -i "$H/c.wav" -f s16le - > "$H/c.pcm"; cmp -s "$H/c.pcm" "$H/c_ref.pcm"; chk "c.wav: estéreo -> mono pelo requestMediaDataWhenReadyOnQueue" $?
echo "avwriter: $ok/$n ok"
[ "$ok" = "$n" ]
