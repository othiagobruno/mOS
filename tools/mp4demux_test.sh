#!/bin/bash
# Compara o demuxer do mOS com o ffprobe em arquivos gerados pelo ffmpeg. Saída: "mp4demux: N/N ok".
set -u
R=$(cd "$(dirname "$0")/.." && pwd); T=$(mktemp -d)
gcc -O1 -Wall -I"$R/userland/libsystem" "$R/tools/mp4demux_test.c" "$R/userland/libsystem/mp4demux.c" -o "$T/t" || exit 1
F="-loglevel error -y"
V="-f lavfi -i testsrc2=size=320x240:rate=30:duration=2"
A="-f lavfi -i sine=frequency=440:duration=2:sample_rate=44100"
ffmpeg $F $V $A -c:v libx264 -bf 2 -c:a aac "$T/h264_aac.mp4"
ffmpeg $F $V $A -c:v libx264 -bf 0 -c:a aac -movflags +faststart "$T/faststart.mp4"
ffmpeg $F $V $A -c:v libx264 -c:a aac -movflags +frag_keyframe+empty_moov+default_base_moof "$T/frag.mp4"
ffmpeg $F $V -c:v libx265 -tag:v hvc1 "$T/hevc.mp4" 2>/dev/null || true
ffmpeg $F $V -c:v libvpx-vp9 -b:v 200k "$T/vp9.mp4" 2>/dev/null || true
ffmpeg $F $V -c:v libaom-av1 -cpu-used 8 -b:v 200k "$T/av1.mp4" 2>/dev/null || true
ffmpeg $F $A -c:a libopus "$T/opus.mp4" 2>/dev/null || true
ffmpeg $F $A -c:a alac "$T/alac.m4a"
ffmpeg $F $A -c:a aac "$T/aac.m4a"
ffmpeg $F $V $A -c:v libx264 -c:a pcm_s16le "$T/pcm.mov"
ffmpeg $F $V $A -c:v mjpeg -c:a aac "$T/mjpeg.mov"
ok=0; n=0
for f in "$T"/*.mp4 "$T"/*.m4a "$T"/*.mov; do
    [ -s "$f" ] || continue
    n=$((n+1))
    "$T/t" "$f" > "$T/nosso.csv" 2> "$T/info.txt" || { echo "FALHOU $(basename "$f"): $(cat "$T/info.txt")"; continue; }
    ffprobe -v error -show_entries packet=stream_index,pos,size,pts,flags -of csv=p=0 "$f" \
        | awk -F, '{k=substr($5,1,1)=="K"?"K":"_"; print $1","$4","$3","$2","k}' > "$T/ref.csv"
    sort -t, -k1,1n -k2,2n "$T/ref.csv" > "$T/a"; sort -t, -k1,1n -k2,2n "$T/nosso.csv" > "$T/b"
    if [ "$(basename "$f")" = pcm.mov ]; then
        # o ffmpeg reparte o PCM em pacotes próprios; confere só a faixa de vídeo
        grep '^0,' "$T/a" > "$T/a2"; grep '^0,' "$T/b" > "$T/b2"; mv "$T/a2" "$T/a"; mv "$T/b2" "$T/b"
    fi
    if cmp -s "$T/a" "$T/b"; then ok=$((ok+1)); echo "ok $(basename "$f") $(wc -l < "$T/b") amostras"
    else echo "DIFERE $(basename "$f")"; diff "$T/a" "$T/b" | head -5; cat "$T/info.txt"; fi
done
echo "mp4demux: $ok/$n ok"
rm -rf "$T"
[ "$ok" = "$n" ]
