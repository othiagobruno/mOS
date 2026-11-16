#!/bin/bash
# AudioFileStream contra o ffmpeg: o arquivo entra em pedaços aleatórios (3 sementes) e os pacotes têm de sair iguais
# ao "ffmpeg -c copy -f data" (no ADTS, sem os cabeçalhos, como no macOS). MP4 com moov no fim tem de dar 'optm'.
# Saída: "audiofilestream: N/N ok"
set -u
R=$(cd "$(dirname "$0")/.." && pwd); T=$(mktemp -d)
gcc -O1 -Wall -I"$R/userland/libsystem" "$R/tools/audiofilestream_test.c" "$R/userland/libsystem/audiofilestream.c" \
    "$R/userland/libsystem/mp4demux.c" -o "$T/afs" || exit 1
S="-f lavfi -i aevalsrc=0.4*sin(2*PI*(200+900*t)*t)|0.4*sin(2*PI*(3000-700*t)*t):s=44100:d=2"; F="-loglevel error -y"
ffmpeg $F $S -c:a libmp3lame -b:a 128k "$T/a.mp3"; ffmpeg $F $S -c:a libmp3lame -q:a 4 "$T/vbr.mp3"
ffmpeg $F $S -c:a mp2 "$T/a.mp2"; ffmpeg $F $S -c:a aac "$T/a.aac"; ffmpeg $F $S -c:a aac -movflags +faststart "$T/fs.m4a"
ffmpeg $F $S -c:a aac "$T/late.m4a"; ffmpeg $F $S -c:a pcm_s16le "$T/a.wav"; ffmpeg $F $S -c:a pcm_f32le "$T/f.wav"
ffmpeg $F $S -c:a pcm_s24be "$T/a.aiff"; ffmpeg $F $S -c:a pcm_s16be -f caf "$T/a.caf"
ffmpeg $F $S -c:a alac -movflags +faststart "$T/al.m4a"; ffmpeg $F $S -c:a libopus -movflags +faststart "$T/op.mp4"
ffmpeg $F $S -c:a libmp3lame -b:a 128k -id3v2_version 3 -metadata title=mOS "$T/id3.mp3"
ok=0; n=0
for f in a.mp3 vbr.mp3 a.mp2 id3.mp3 a.aac fs.m4a a.wav f.wav a.aiff a.caf al.m4a op.mp4; do
    ffmpeg $F -i "$T/$f" -map 0:a -c copy -f data "$T/ref.bin"
    if [ "$f" = a.aac ]; then
        python3 -I -c "
import sys; b=open(sys.argv[1],'rb').read(); o=bytearray(); i=0
while i<len(b):
    h=7 if b[i+1]&1 else 9; L=((b[i+3]&3)<<11)|(b[i+4]<<3)|(b[i+5]>>5); o+=b[i+h:i+L]; i+=L
open(sys.argv[1],'wb').write(o)" "$T/ref.bin"
    fi
    for s in 1 2 3; do
        n=$((n+1)); r=$("$T/afs" "$T/$f" "$T/ours.bin" $s)
        if cmp -s "$T/ours.bin" "$T/ref.bin"; then ok=$((ok+1)); [ $s = 1 ] && echo "ok $f: $r"; else echo "DIFERE $f semente $s: $r"; fi
    done
done
n=$((n+1)); r=$("$T/afs" "$T/late.m4a" "$T/ours.bin" 1); case "$r" in "erro optm") ok=$((ok+1)); echo "ok late.m4a: $r";; *) echo "FALHA late.m4a: $r";; esac
echo "audiofilestream: $ok/$n ok"
rm -rf "$T"; [ "$ok" = "$n" ]
