#!/bin/bash
# Mede o corpus inteiro, um app por vez, em primeiro plano. Regras de RAM do PC
# (/home/thiago/OpenBot/Shared/mos-regras-de-ram.md): nada em background; extração e machocheck de arquivo com
# 200 MB ou mais correm pelo mos-pesado (trava única do PC e limite de 2 GB); os menores, só com nice. Cada app só
# começa com RAM disponível >= 4000 MB. O binário é apagado depois de medido.
# Não roda o build: usa o libSystem já compilado em build/. Uso: tools/corpus/varrer-tudo.sh [ids...]
# Sem ids mede um lote de LOTE apps (padrão 20) e sai 3 se ainda faltar. PEQUENOS=1: só os que não precisam do
# mos-pesado (download < 200 MB); os grandes ficam para quando a trava vagar (build/corpus/grandes.txt guarda os que
# passaram do teto sem baixar; sem PEQUENOS eles voltam ao lote normal).
set -uo pipefail
cd "$(dirname "$0")/../.."
H=tools/corpus/harness.py
# libSystem e disco: os do build/ local. Depois do tools/build.sh, o disco é a própria árvore da imagem (build/esp/mOS:
# /usr/lib/swift com libswiftFoundation, libswiftAppKit..., os frameworks do mOS como CryptoKit, CoreImage e CloudKit, e
# /usr/lib); sem ela, os discos copiados de antes (build/disco-x86). build/libSystem.origem diz de onde veio.
if [ -d build/esp/mOS/usr/lib/swift ]; then
    [ -d build/esp/mOS/Aplicativos ] && { echo "varrer-tudo: build/esp/mOS tem /Aplicativos (apps de terceiros contariam como sistema); apague" >&2; exit 1; }
    [ -f build/libSystem.origem ] || echo "build final $(git log -1 --format='%h %cs' 2>/dev/null)" > build/libSystem.origem
    LIBS=(--lib build/libSystem.mos --disco build/esp/mOS --lib-arm64 build/libSystem-arm64.mos --disco-arm64 build/esp/mOS)
else
    LIBS=(--lib build/libSystem.mos --disco build/disco-x86 --lib-arm64 build/libSystem-arm64.mos --disco-arm64 build/disco-arm64)
fi
echo "varrer-tudo: libSystem local ($(cat build/libSystem.origem 2>/dev/null)), disco ${LIBS[3]}"
# libSystem diferente do da última medida: tudo o que foi medido com o outro é refeito
atual=$(sha256sum "${LIBS[1]}" | cut -c1-16)
if [ "$(cat build/corpus/libsystem-medido.txt 2>/dev/null)" != "$atual" ]; then
    rm -f build/corpus/*/medido.sha256 build/corpus/falhas.json
    mkdir -p build/corpus && echo "$atual" > build/corpus/libsystem-medido.txt
fi
ids=("$@")
if [ ${#ids[@]} -eq 0 ]; then
    # sem ids: os que ainda não têm medida com o hash atual do manifesto. Os de build/corpus/falhas.json ficam de fora
    # (senão voltam em todo lote e o laço não termina); refazer um deles: varrer-tudo.sh <id>
    mapfile -t ids < <(python3 -c "
import json, pathlib
f = pathlib.Path('build/corpus/falhas.json')
falhas = json.loads(f.read_text()) if f.is_file() else {}
for a in json.load(open('tools/corpus/manifest.json'))['apps']:
    m = pathlib.Path('build/corpus') / a['id'] / 'medido.sha256'
    if a['id'] not in falhas and not (a.get('sha256') and m.is_file() and m.read_text().strip() == a['sha256']):
        print(a['id'])")
fi
mais=0
if [ $# -eq 0 ] && [ -n "${PEQUENOS:-}" ]; then
    leves=()
    for id in "${ids[@]}"; do
        tam=$(du -sm "build/corpus/$id" 2>/dev/null | cut -f1)
        grep -qxF "$id" build/corpus/grandes.txt 2>/dev/null && continue    # passou do teto num lote anterior
        [ "${tam:-0}" -lt 200 ] && leves+=("$id")
    done
    echo "varrer-tudo: $(( ${#ids[@]} - ${#leves[@]} )) grandes já baixados ficam para a trava"
    ids=("${leves[@]}")
fi
LOTE=${LOTE:-20}
if [ $# -eq 0 ] && [ ${#ids[@]} -gt "$LOTE" ]; then
    # um lote por chamada: a cada lote o libSystem é conferido de novo (sai 3 quando ainda há apps)
    ids=("${ids[@]:0:$LOTE}")
    mais=1
fi
echo "varrer-tudo: ${#ids[@]} apps a medir neste lote"
PESADO=/home/thiago/OpenBot/Shared/mos-tools/bin/mos-pesado
espera_ram() { while [ "$(free -m | awk '/^Mem/{print $7}')" -lt 4000 ]; do sleep 60; done; }
adiados=()
for id in "${ids[@]}"; do
    if [ -n "${PEQUENOS:-}" ]; then
        saida=$(MOS_CORPUS_MAX_MB=200 python3 "$H" baixar --fixar "$id" 2>&1 | grep -v '^baixando')
        if [[ "$saida" == *"adiado:"* ]]; then echo "varrer-tudo: $id passa de 200 MB, fica para a trava (não baixado)"; echo "$id" >> build/corpus/grandes.txt; continue; fi
        [ -n "$saida" ] && echo "$saida"
    else
        python3 "$H" baixar --fixar "$id" 2>&1 | grep -v '^baixando'
    fi
    espera_ram
    # arquivo pequeno (< 200 MB) não é descompactação grande: mede fora da trava; o resto vai pelo mos-pesado
    tam=$(du -sm "build/corpus/$id" 2>/dev/null | cut -f1)
    if [ "${tam:-999}" -lt 200 ]; then nice -n 10 tools/corpus/medir-um.sh "$id" "${LIBS[@]}"; continue; fi
    if [ -n "${PEQUENOS:-}" ]; then echo "varrer-tudo: $id ($tam MB) fica para a trava"; continue; fi
    if ! flock -n /tmp/mos-pesado.lock true; then
        # trava ocupada e arquivo grande: vai para o fim do lote (o download fica no disco) e os pequenos seguem
        adiados+=("$id")
        echo "varrer-tudo: $id ($tam MB) adiado para o fim do lote"
        continue
    fi
    "$PESADO" tools/corpus/medir-um.sh "$id" "${LIBS[@]}"
done
for id in "${adiados[@]}"; do
    espera_ram
    "$PESADO" tools/corpus/medir-um.sh "$id" "${LIBS[@]}"
done
[ $mais = 1 ] && exit 3
exit 0
