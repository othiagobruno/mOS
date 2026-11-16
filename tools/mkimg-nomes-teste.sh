#!/bin/bash
# Teste de host dos nomes longos do mkimg: o que entra no staging tem de sair igual pelo mtools (leitor FAT de referência).
# Uso: tools/mkimg-nomes-teste.sh   (precisa de cc e mtools)
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d /tmp/mos-mkimg-nomes-XXXX); trap 'rm -rf "$T"' EXIT
cc -O2 -o "$T/mkimg" "$R/tools/mkimg.c" || exit 1
S=$T/stg; mkdir -p "$S/EFI/BOOT" "$S/mOS/Aplicativos"
echo k > "$S/EFI/BOOT/BOOTX64.EFI"
nomes=("Calculadora Científica.app" "Ação e Reação.txt" "日本語のメモ.txt" "Notas 📝 rápidas.txt"
       "Um nome bem comprido que passa de vinte e seis caracteres e usa várias entradas LFN.txt" "MAIUSC.TXT" "minusc.txt")
for n in "${nomes[@]}"; do echo "$n" > "$S/mOS/Aplicativos/$n"; done
"$T/mkimg" "$T/a.img" "$S" 64 >/dev/null || { echo "FALHA: mkimg"; exit 1; }
off=$((2048 * 512))
mkdir "$T/out"
MTOOLS_SKIP_CHECK=1 LC_ALL=C.UTF-8 mcopy -s -n -i "$T/a.img@@$off" ::/mOS "$T/out" || { echo "FALHA: mcopy"; exit 1; }
# nomes longos lidos direto das entradas LFN (o mtools não junta pares substitutos; o kernel/fs/fat.c junta)
python3 - "$T/a.img" > "$T/lfn.txt" <<'P'
import sys
d = open(sys.argv[1], "rb").read()
P = (1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30)
partes = {}
for i in range(0, len(d) - 32, 32):
    e = d[i:i + 32]
    if e[11] == 0x0F and e[0] != 0xE5 and 1 <= (e[0] & 0x3F) <= 20:
        u = [e[k] | e[k + 1] << 8 for k in P]
        partes.setdefault((i // 512, e[13]), {})[e[0] & 0x3F] = u
for (_, _), seqs in partes.items():
    w = [c for k in sorted(seqs) for c in seqs[k]]
    w = w[: w.index(0)] if 0 in w else [c for c in w if c != 0xFFFF]
    print(b"".join(c.to_bytes(2, "little") for c in w).decode("utf-16-le", "surrogatepass"))
P
ok=0
for n in "${nomes[@]}"; do
    if grep -qxF "$n" "$T/lfn.txt" || [ "$n" = MAIUSC.TXT ]; then ok=$((ok + 1)); else echo "FALHA LFN: $n"; fi
    case "$n" in *📝*) continue ;; esac                      # o mtools não lê par substituto
    if [ "$(cat "$T/out/mOS/Aplicativos/$n" 2>/dev/null)" = "$n" ]; then ok=$((ok + 1)); else echo "FALHA mtools: $n"; fi
done
total=$(( ${#nomes[@]} * 2 - 1 ))
echo "mkimg-nomes: $ok/$total ok"
[ $ok = $total ]
