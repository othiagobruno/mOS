#!/bin/bash
# Mede o corpus inteiro, um app por vez. Regras de RAM do PC (/home/thiago/OpenBot/Shared/mos-regras-de-ram.md):
# o download (rede, pouca RAM) corre fora da trava e já adianta o próximo app; extração e machocheck de arquivo com
# 200 MB ou mais correm dentro de `flock /tmp/mos-pesado.lock nice -n 10` (descompactação grande); os menores, só com
# `nice`. Tudo só com RAM disponível >= 3000 MB. O binário é apagado depois de medido.
# Não roda o build: usa o libSystem da main do CI ou o já compilado em build/. Uso: tools/corpus/varrer-tudo.sh [ids...]
# Sem ids mede um lote de 20 e sai 3 se ainda faltar: while tools/corpus/varrer-tudo.sh; [ $? = 3 ]; do :; done
set -uo pipefail
cd "$(dirname "$0")/../.."
H=tools/corpus/harness.py
# libSystem: o da main compilado pelo CI (harness.py libsystem-ci), senão o build/ local já copiado
python3 "$H" libsystem-ci >/dev/null 2>&1 || true
if [ -f build/ci-lib/libSystem.mos ]; then
    LIBS=(--lib build/ci-lib/libSystem.mos --disco build/ci-lib/disco-x86 --disco build/disco-x86)
    [ -f build/ci-lib/libSystem-arm64.mos ] && LIBS+=(--lib-arm64 build/ci-lib/libSystem-arm64.mos --disco-arm64 build/disco-arm64)
    echo "varrer-tudo: libSystem da main $(cat build/ci-lib/commit.txt 2>/dev/null | cut -c1-10)"
else
    LIBS=(--lib build/libSystem.mos --disco build/disco-x86 --lib-arm64 build/libSystem-arm64.mos --disco-arm64 build/disco-arm64)
    echo "varrer-tudo: libSystem local ($(cat build/libSystem.origem 2>/dev/null))"
fi
# libSystem diferente do da última medida: tudo o que foi medido com o outro é refeito
atual=$(sha256sum "${LIBS[1]}" | cut -c1-16)
if [ "$(cat build/corpus/libsystem-medido.txt 2>/dev/null)" != "$atual" ]; then
    rm -f build/corpus/*/medido.sha256
    mkdir -p build/corpus && echo "$atual" > build/corpus/libsystem-medido.txt
fi
ids=("$@")
if [ ${#ids[@]} -eq 0 ]; then
    # sem ids: os que ainda não têm medida com o hash atual do manifesto
    mapfile -t ids < <(python3 -c "
import json, pathlib
for a in json.load(open('tools/corpus/manifest.json'))['apps']:
    m = pathlib.Path('build/corpus') / a['id'] / 'medido.sha256'
    if not (a.get('sha256') and m.is_file() and m.read_text().strip() == a['sha256']):
        print(a['id'])")
fi
mais=0
if [ $# -eq 0 ] && [ ${#ids[@]} -gt 20 ]; then
    # lotes de 20: a cada lote o libSystem da main é conferido de novo (sai 3 quando ainda há apps)
    ids=("${ids[@]:0:20}")
    mais=1
fi
echo "varrer-tudo: ${#ids[@]} apps a medir neste lote"
pre=""
for i in "${!ids[@]}"; do
    id=${ids[$i]}
    [ -n "$pre" ] && wait "$pre" 2>/dev/null
    python3 "$H" baixar --fixar "$id" 2>&1 | grep -v '^baixando'
    if [ $((i + 1)) -lt ${#ids[@]} ]; then
        python3 "$H" baixar --fixar "${ids[$((i + 1))]}" > /dev/null 2>&1 &
        pre=$!
    else
        pre=""
    fi
    while [ "$(free -m | awk '/^Mem/{print $7}')" -lt 3000 ]; do sleep 60; done
    # arquivo pequeno (< 200 MB) não é descompactação grande: mede fora da trava; o resto espera a vez nela
    tam=$(du -sm "build/corpus/$id" 2>/dev/null | cut -f1)
    trava=(flock /tmp/mos-pesado.lock)
    [ "${tam:-999}" -lt 200 ] && trava=()
    "${trava[@]}" nice -n 10 timeout 1800 python3 "$H" scan --tolerante --limpar --fixar --continuar \
        "${LIBS[@]}" \
        --summary "build/corpus/$id.summary.json" "$id" 2>&1 | grep -v '^baixando' | tail -2
done
[ -n "$pre" ] && wait "$pre"
[ $mais = 1 ] && exit 3
exit 0
