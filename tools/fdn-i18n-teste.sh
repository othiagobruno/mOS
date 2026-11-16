#!/bin/bash
# Testes de host da Foundation sobre a ICU: locale, fuso, calendário (fdn_i18n_test.c) e formatters (fdn_fmt_test.c).
# Usam a ICU do Linux (libicui18n/libicuuc) com a mesma tabela de fdn_icu.c; sem ela no host, avisa e sai 0.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-/tmp/mos-fdn-i18n}
mkdir -p "$OUT"
if ! compgen -G "/usr/lib/x86_64-linux-gnu/libicui18n.so.*" >/dev/null && ! compgen -G "/usr/lib64/libicui18n.so.*" >/dev/null \
   && ! compgen -G "/usr/lib/libicui18n.so.*" >/dev/null; then
    echo "fdn-i18n: sem ICU no host, testes pulados" >&2
    exit 0
fi
L="$ROOT/userland/libsystem"
gcc -O1 -Wall -DFDN_ICU_HOST -I"$L" "$L/fdn_icu.c" "$L/fdn_i18n_core.c" "$ROOT/tools/fdn_i18n_test.c" -ldl -lm -o "$OUT/fdn_i18n_test"
gcc -O1 -Wall -Wno-format-truncation -DFDN_ICU_HOST -I"$L" "$L/fdn_icu.c" "$L/fdn_i18n_core.c" "$L/fdn_fmt_core.c" \
    "$ROOT/tools/fdn_fmt_test.c" -ldl -lm -o "$OUT/fdn_fmt_test"
"$OUT/fdn_i18n_test"
"$OUT/fdn_fmt_test"
gcc -O2 -Wall -DFDN_CONSTS_HOST "$L/fdn_consts.c" "$ROOT/tools/fdn_consts_test.c" -o "$OUT/fdn_consts_test"
"$OUT/fdn_consts_test"
