#!/bin/bash
# Mede o corpus inteiro, um app por vez, cada um dentro da trava de trabalho pesado do PC
# (/home/thiago/OpenBot/Shared/mos-regras-de-ram.md): espera RAM disponível >= 3000 MB, baixa, extrai, mede, apaga o binário.
# Não roda o build: usa o libSystem.mos já compilado em build/. Uso: tools/corpus/varrer-tudo.sh [ids...]
set -uo pipefail
cd "$(dirname "$0")/../.."
H=tools/corpus/harness.py
ids=("$@")
[ ${#ids[@]} -eq 0 ] && mapfile -t ids < <(python3 -c "import json;[print(a['id']) for a in json.load(open('tools/corpus/manifest.json'))['apps']]")
for id in "${ids[@]}"; do
    while [ "$(free -m | awk '/^Mem/{print $7}')" -lt 3000 ]; do sleep 60; done
    flock /tmp/mos-pesado.lock nice -n 10 timeout 1800 python3 "$H" scan --tolerante --limpar --fixar --continuar \
        --lib build/libSystem.mos --disco build/disco-x86 --lib-arm64 build/libSystem-arm64.mos --disco-arm64 build/disco-arm64 \
        --summary "build/corpus/$id.summary.json" "$id" 2>&1 | grep -v '^baixando' | tail -2
done
