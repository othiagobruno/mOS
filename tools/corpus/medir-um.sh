#!/bin/bash
# Mede um app do corpus (extração + machocheck), em primeiro plano. Chamado pelo varrer-tudo.sh, direto (arquivo
# pequeno) ou pelo mos-pesado (arquivo grande). Uso: tools/corpus/medir-um.sh <id> <opções de libSystem do harness...>
set -uo pipefail
cd "$(dirname "$0")/../.."
id=$1
shift
timeout 1800 python3 tools/corpus/harness.py scan --tolerante --limpar --fixar --continuar "$@" \
    --summary "build/corpus/$id.summary.json" "$id" 2>&1 | grep -v '^baixando' | tail -2
