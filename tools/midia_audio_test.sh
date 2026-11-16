#!/bin/bash
# Teste de host do áudio comprimido (Opus, AAC, MP3 em MP4) contra o ffmpeg. Saída: "midia-audio: N/N ok".
# Pesado (compila libopus e fdk-aac no host): rode dentro de flock /tmp/mos-pesado.lock.
set -u
R=$(cd "$(dirname "$0")/.." && pwd); T=${MIDIA_TMP:-/tmp/midia-audio-test}; mkdir -p "$T/o"
OP="$R/userland/third_party/opus"; FD="$R/userland/third_party/fdk-aac"
I="-I$OP -I$OP/include -I$OP/celt -I$OP/silk -I$OP/silk/fixed"
for f in $(cat "$OP/SOURCES.txt"); do
    o="$T/o/op_$(basename "$f" .c).o"
    gcc -O2 -w -DHAVE_CONFIG_H $I -c "$OP/$f" -o "$o" || exit 1
done
FI=""; for d in "$FD"/*/include "$FD"/*/src; do FI="$FI -I$d"; done
for f in $(cd "$FD" && find . -mindepth 3 -maxdepth 3 -path './*/src/*.cpp' | sort); do
    o="$T/o/fdk_$(echo "$f" | tr / _ | sed 's/^\._//').o"
    g++ -O2 -w -fno-exceptions -fno-rtti $FI -c "$FD/$f" -o "$o" || exit 1
done
g++ -O1 -w -fno-exceptions -fno-rtti $FI -I"$R/userland/host-audio" -c "$R/userland/host-audio/aac_glue.cpp" -o "$T/o/aac_glue.o" || exit 1
gcc -O1 -Wall -Wno-unused-function -I"$R/userland/libsystem" $I -c "$R/userland/host-audio/opus_glue.c" -o "$T/o/opus_glue.o" || exit 1
gcc -O1 -w -I"$R/userland/host-audio/shim" -I"$R/userland/libsystem" -c "$R/userland/host-audio/audiodec.c" -o "$T/o/audiodec.o" || exit 1
gcc -O1 -w -I"$R/userland/libsystem" -c "$R/userland/libsystem/mp4demux.c" -o "$T/o/mp4demux.o" || exit 1
g++ -O1 "$R/tools/midia_audio_cmp.c" -x none "$T"/o/*.o -lm -o "$T/cmp" 2>/dev/null || \
    { gcc -O1 -c "$R/tools/midia_audio_cmp.c" -o "$T/cmp.o" && g++ "$T/cmp.o" "$T"/o/*.o -lm -o "$T/cmp"; } || exit 1
F="-loglevel error -y"
S="-f lavfi -i aevalsrc=0.4*sin(2*PI*(200+900*t)*t)|0.4*sin(2*PI*(3000-700*t)*t):s=48000:d=3"
M="-f lavfi -i aevalsrc=0.4*sin(2*PI*(300+700*t)*t):s=44100:d=3"
ok=0; n=0
run() { # nome arquivo taxa canais tolerância (quadros a menos que o ffmpeg, que não corta o fim pela edit list)
    n=$((n+1))
    ffmpeg $F -i "$2" -f s16le -acodec pcm_s16le "$T/ref.raw" || { echo "FALHA ref $1"; return; }
    r=$("$T/cmp" "$2" "$T/ref.raw" "$3" "$4" "$5"); echo "$1: $r" | tr '\n' ' '; echo
    case "$r" in *ok) ok=$((ok+1));; esac
}
ffmpeg $F $S -c:a libopus -b:a 96k "$T/a.opus" && run "ogg opus estéreo" "$T/a.opus" 48000 2 0
ffmpeg $F $M -c:a libopus -b:a 48k "$T/m.opus" && run "ogg opus mono" "$T/m.opus" 48000 1 0
ffmpeg $F $S -c:a libopus -b:a 96k "$T/a_opus.mp4" && run "mp4 opus" "$T/a_opus.mp4" 48000 2 1152
ffmpeg $F $S -c:a aac -b:a 128k "$T/a.aac" && run "adts aac-lc" "$T/a.aac" 48000 2 0
ffmpeg $F $S -c:a aac -b:a 128k "$T/a.m4a" && run "m4a aac-lc" "$T/a.m4a" 48000 2 1152
ffmpeg $F $M -c:a aac -b:a 64k "$T/m.m4a" && run "m4a aac mono 44.1k" "$T/m.m4a" 44100 1 1152
ffmpeg $F $S -c:a aac -b:a 128k "$T/a.mov" && run "mov aac" "$T/a.mov" 48000 2 1152
# AAC em CAF: o ffmpeg não grava (só o afconvert do macOS); fica fora deste teste.
ffmpeg $F $S -c:a libopus -b:a 96k -f caf "$T/o.caf" 2>/dev/null && run "caf opus" "$T/o.caf" 48000 2 1152
ffmpeg $F $S -c:a libmp3lame -b:a 192k "$T/mp3.mp4" && run "mp4 mp3" "$T/mp3.mp4" 48000 2 1152
# decodificação por pacote (AudioConverter) contra o arquivo inteiro
gcc -O1 -w -I"$R/userland/libsystem" -c "$R/tools/midia_pdec_test.c" -o "$T/pdec.o" && g++ "$T/pdec.o" "$T"/o/*.o -lm -o "$T/pdec" || exit 1
ffmpeg $F $S -c:a alac "$T/alac.m4a" && run "m4a alac 24 bits" "$T/alac.m4a" 48000 2 0   # a fonte é double: s32p/24
ffmpeg $F $S -c:a alac -sample_fmt s16p "$T/alac16.m4a" && run "m4a alac 16 bits" "$T/alac16.m4a" 48000 2 0
for f in "$T/a.m4a" "$T/m.m4a" "$T/a.mov" "$T/a_opus.mp4" "$T/mp3.mp4" "$T/alac.m4a" "$T/alac16.m4a"; do
    [ -s "$f" ] || continue
    n=$((n+1)); r=$("$T/pdec" "$f"); echo "pdec $(basename "$f"): $r"
    case "$r" in "pdec ok"*) ok=$((ok+1));; esac
done
# AudioConverter decodificando (coreaudio.c de verdade; stubs só para o que o conversor não chama)
gcc -std=gnu11 -O1 -w -fno-builtin -I"$R/userland/libsystem/kshim" -I"$R/userland/libsystem" -I"$R/kernel/include" \
    -c "$R/userland/libsystem/coreaudio.c" -o "$T/ca.o" || exit 1
gcc -O1 -w -c "$R/tools/audioconverter_host_stubs.c" -o "$T/stubs.o" && gcc -O1 -w -I"$R/userland/libsystem" \
    -c "$R/tools/audioconverter_test.c" -o "$T/act.o" && g++ "$T/act.o" "$T/ca.o" "$T/stubs.o" "$T"/o/*.o -lm -lpthread -o "$T/act" || exit 1
for c in "a.m4a" "a.m4a 0 esds" "m.m4a" "a_opus.mp4" "mp3.mp4" "alac.m4a" "a.m4a 44100" "m.m4a 48000"; do
    set -- $c; f=$1; shift
    [ -s "$T/$f" ] || continue
    n=$((n+1)); r=$("$T/act" "$T/$f" "$@"); echo "audioconverter $c: $r"
    case "$r" in "ac ok"*) ok=$((ok+1));; esac
done
# AUGraph e AUMultiChannelMixer (o mesmo coreaudio.c)
gcc -O1 -w -c "$R/tools/augraph_test.c" -o "$T/agt.o" && g++ "$T/agt.o" "$T/ca.o" "$T/stubs.o" "$T"/o/*.o -lm -lpthread -o "$T/agt" || exit 1
n=$((n+1)); "$T/agt" && ok=$((ok+1))
# CoreMIDI (fontes/destinos virtuais, UMP 1.0/2.0)
gcc -O1 -w -I"$R/userland/libsystem" "$R/tools/coremidi_test.c" "$R/userland/libsystem/coremidi.c" -lpthread -o "$T/cmt" || exit 1
n=$((n+1)); "$T/cmt" && ok=$((ok+1))
echo "midia-audio: $ok/$n ok"
[ "$ok" = "$n" ]
