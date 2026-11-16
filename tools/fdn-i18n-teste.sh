#!/bin/bash
# Testes de host da Foundation sobre a ICU: locale, fuso, calendário (fdn_i18n_test.c) e formatters (fdn_fmt_test.c).
# Usam a ICU do Linux (libicui18n/libicuuc) com a mesma tabela de fdn_icu.c; sem ela no host, avisa e sai 0.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-/tmp/mos-fdn-i18n}
mkdir -p "$OUT"
# NSDecimal não usa a ICU: roda sempre
gcc -O1 -Wall -I"$ROOT/userland/libsystem" "$ROOT/userland/libsystem/fdn_decimal_core.c" "$ROOT/tools/fdn_dec_test.c" -o "$OUT/fdn_dec_test"
# trechos de atributos do NSAttributedString (sem ICU)
gcc -O1 -Wall -I"$ROOT/userland/libsystem" "$ROOT/userland/libsystem/fdn_astr_core.c" "$ROOT/tools/fdn_astr_test.c" -o "$OUT/fdn_astr_test"
"$OUT/fdn_astr_test"
# tabela hash do NSSet/CFSet: com os cabeçalhos do libSystem e o include do zig
ZIG_INC=$(dirname "$(readlink -f "$(command -v zig 2>/dev/null || echo /nao/existe)")")/lib/include
if [ -d "$ZIG_INC" ]; then
    gcc -O1 -w -fno-builtin -nostdinc -isystem "$ZIG_INC" -I"$ROOT/userland/libsystem" -I"$ROOT/userland/libsystem/kshim" \
        "$ROOT/tools/fdn_set_test.c" -o "$OUT/fdn_set_test"
    "$OUT/fdn_set_test"
    gcc -O1 -w -fno-builtin -nostdinc -isystem "$ZIG_INC" -I"$ROOT/userland/libsystem" -I"$ROOT/userland/libsystem/kshim" \
        "$ROOT/tools/fdn_indexset_test.c" -o "$OUT/fdn_indexset_test"
    "$OUT/fdn_indexset_test"
fi
"$OUT/fdn_dec_test"
if ! compgen -G "/usr/lib/x86_64-linux-gnu/libicui18n.so.*" >/dev/null && ! compgen -G "/usr/lib64/libicui18n.so.*" >/dev/null \
   && ! compgen -G "/usr/lib/libicui18n.so.*" >/dev/null; then
    echo "fdn-i18n: sem ICU no host, testes pulados" >&2
    exit 0
fi
L="$ROOT/userland/libsystem"
gcc -O1 -Wall -DFDN_ICU_HOST -I"$L" "$L/fdn_icu.c" "$L/fdn_i18n_core.c" "$ROOT/tools/fdn_i18n_test.c" -ldl -lm -o "$OUT/fdn_i18n_test"
gcc -O1 -Wall -DFDN_ICU_HOST -I"$L" "$L/fdn_icu.c" "$L/fdn_charset_core.c" "$ROOT/tools/fdn_charset_test.c" -ldl -o "$OUT/fdn_charset_test"
# NSString (busca, comparação, caixa) e NSScanner, em UTF-16 sobre a ICU
gcc -O1 -Wall -DFDN_ICU_HOST -I"$L" "$L/fdn_icu.c" "$L/fdn_text_core.c" "$ROOT/tools/fdn_text_test.c" -ldl -o "$OUT/fdn_text_test"
"$OUT/fdn_text_test"
gcc -O1 -Wall -DFDN_ICU_HOST -I"$L" "$L/fdn_icu.c" "$L/fdn_text_core.c" "$L/fdn_scanner_core.c" "$ROOT/tools/fdn_scanner_test.c" \
    -ldl -lm -o "$OUT/fdn_scanner_test"
gcc -O1 -Wall -DFDN_ICU_HOST -I"$L" "$L/fdn_icu.c" "$L/fdn_encoding_core.c" "$ROOT/tools/fdn_encoding_test.c" -ldl -o "$OUT/fdn_encoding_test"
"$OUT/fdn_encoding_test"
"$OUT/fdn_scanner_test"
"$OUT/fdn_charset_test"
gcc -O1 -Wall -Wno-format-truncation -DFDN_ICU_HOST -I"$L" "$L/fdn_icu.c" "$L/fdn_i18n_core.c" "$L/fdn_fmt_core.c" \
    "$ROOT/tools/fdn_fmt_test.c" -ldl -lm -o "$OUT/fdn_fmt_test"
"$OUT/fdn_i18n_test"
"$OUT/fdn_fmt_test"
gcc -O2 -Wall -DFDN_CONSTS_HOST "$L/fdn_consts.c" "$ROOT/tools/fdn_consts_test.c" -o "$OUT/fdn_consts_test"
"$OUT/fdn_consts_test"
