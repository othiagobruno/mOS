#!/bin/bash
# Teste de host do núcleo UAC (kernel/dev/uac_core.c): descritores, faixas de taxa e conversão para 48 kHz estéreo.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
gcc -std=gnu11 -O1 -Wall -o /tmp/uac_core_test "$ROOT/tools/uac_core_test.c" "$ROOT/kernel/dev/uac_core.c"
/tmp/uac_core_test
rm -f /tmp/uac_core_test
