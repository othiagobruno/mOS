#!/bin/bash
# Mede o corpus inteiro, um app por vez, em primeiro plano. Regras de RAM do PC
# (/home/thiago/OpenBot/Shared/mos-regras-de-ram.md): nada em background; extração e machocheck de arquivo com
# 200 MB ou mais correm pelo mos-pesado (trava única do PC e limite de 2 GB); os menores, só com nice. Cada app só
# começa com RAM disponível >= 4000 MB. O binário é apagado depois de medido.
# Não roda o build: usa o libSystem já compilado em build/. Uso: tools/corpus/varrer-tudo.sh [ids...]
# Sem ids mede um lote de LOTE apps (padrão 20) e sai 3 se ainda faltar.
set -uo pipefail
cd "$(dirname "$0")/../.."
H=tools/corpus/harness.py
# libSystem: o do build/ local (o build final pela fila renova; build/libSystem.origem diz de onde veio)
LIBS=(--lib build/libSystem.mos --disco build/disco-x86 --lib-arm64 build/libSystem-arm64.mos --disco-arm64 build/disco-arm64)
echo "varrer-tudo: libSystem local ($(cat build/libSystem.origem 2>/dev/null))"
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
LOTE=${LOTE:-20}
if [ $# -eq 0 ] && [ ${#ids[@]} -gt "$LOTE" ]; then
    # um lote por chamada: a cada lote o libSystem é conferido de novo (sai 3 quando ainda há apps)
    ids=("${ids[@]:0:$LOTE}")
    mais=1
fi
echo "varrer-tudo: ${#ids[@]} apps a medir neste lote"
PESADO=/home/thiago/OpenBot/Shared/mos-tools/bin/mos-pesado
espera_ram() { while [ "$(free -m | awk '/^Mem/{print $7}')" -lt 4000 ]; do sleep 60; done; }
mede() {
    timeout 1800 python3 "$H" scan --tolerante --limpar --fixar --continuar \
        "${LIBS[@]}" --summary "build/corpus/$1.summary.json" "$1" 2>&1 | grep -v '^baixando' | tail -2
}
adiados=()
for id in "${ids[@]}"; do
    python3 "$H" baixar --fixar "$id" 2>&1 | grep -v '^baixando'
    espera_ram
    # arquivo pequeno (< 200 MB) não é descompactação grande: mede fora da trava; o resto vai pelo mos-pesado
    tam=$(du -sm "build/corpus/$id" 2>/dev/null | cut -f1)
    if [ "${tam:-999}" -lt 200 ]; then nice -n 10 bash -c "$(declare -p H LIBS); $(declare -f mede); mede $id"; continue; fi
    if ! flock -n /tmp/mos-pesado.lock true; then
        # trava ocupada e arquivo grande: vai para o fim do lote (o download fica no disco) e os pequenos seguem
        adiados+=("$id")
        echo "varrer-tudo: $id ($tam MB) adiado para o fim do lote"
        continue
    fi
    "$PESADO" bash -c "$(declare -p H LIBS); $(declare -f mede); mede $id"
done
for id in "${adiados[@]}"; do
    espera_ram
    "$PESADO" bash -c "$(declare -p H LIBS); $(declare -f mede); mede $id"
done
[ $mais = 1 ] && exit 3
exit 0
