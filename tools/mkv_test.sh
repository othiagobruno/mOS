#!/bin/bash
# Teste de host do Matroska/WebM (mkvdemux.c) e do Vorbis (vorbis_glue.c, stb_vorbis) contra o ffprobe/ffmpeg.
# Leve: só arquivos de áudio no ffmpeg e só os arquivos do mOS compilados. Rode pelo Shared/mos-tools/bin/mos-pesado.
# Saída: "mkv: N/N ok"
set -u
R=$(cd "$(dirname "$0")/.." && pwd); T=${MKV_TMP:-/tmp/mkv-test}; mkdir -p "$T"
gcc -O1 -w -I"$R/userland/libsystem" "$R/tools/mkvdemux_test.c" "$R/userland/libsystem/mkvdemux.c" -o "$T/mkt" || exit 1
gcc -O1 -w -I"$R/userland/libsystem" -I"$R/userland/host-audio" "$R/tools/vorbis_test.c" "$R/userland/host-audio/vorbis_glue.c" \
    "$R/userland/libsystem/mkvdemux.c" -lm -o "$T/vbt" || exit 1
F="-loglevel error -y"; S="-f lavfi -i sine=f=440:d=3"
ok=0; n=0
n=$((n+1)); r=$("$T/mkt" --lacing); echo "$r"; [ "$r" = "mkvdemux lacing ok" ] && ok=$((ok+1))
ffmpeg $F $S -c:a libopus "$T/a.webm"
ffmpeg $F $S -c:a libvorbis "$T/b.mka"
ffmpeg $F $S -c:a aac "$T/c.mka"
ffmpeg $F $S -c:a libmp3lame "$T/d.mka"
ffmpeg $F $S -c:a libopus -f webm -live 1 "$T/e.webm"
ffmpeg $F $S -f lavfi -i sine=f=880:d=3 -map 0 -map 1 -c:a libopus "$T/f.webm"
for x in a.webm b.mka c.mka d.mka e.webm f.webm; do
    n=$((n+1))
    "$T/mkt" "$T/$x" > "$T/ours.txt"
    ffprobe -v error -show_entries packet=stream_index,pts,size,flags -of csv=p=0 "$T/$x" 2>/dev/null | awk -F, '{print $1, $2, $3, (index($4,"K")?1:0)}' > "$T/ref.txt"
    if cmp -s "$T/ours.txt" "$T/ref.txt"; then echo "$x: $(wc -l < "$T/ref.txt") pacotes iguais ao ffprobe"; ok=$((ok+1)); else echo "$x: DIFERE do ffprobe"; fi
done
ffmpeg $F -f lavfi -i sine=f=440:d=3 -f lavfi -i sine=f=1000:d=3 -filter_complex amerge -c:a libvorbis -q:a 5 "$T/st.ogg"
ffmpeg $F -i "$T/st.ogg" -c copy "$T/st.mka"
for x in b.mka st.ogg st.mka; do
    n=$((n+1))
    ffmpeg $F -i "$T/$x" -f s16le "$T/r.raw"
    r=$("$T/vbt" "$T/$x" "$T/r.raw"); echo "$x: $r"
    case "$r" in "vorbis ok"*) ok=$((ok+1));; esac
done
echo "mkv: $ok/$n ok"
[ "$ok" = "$n" ]
