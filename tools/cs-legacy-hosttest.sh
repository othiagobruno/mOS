#!/bin/sh
# Teste de host de userland/libsystem/cs_legacy_core.inc (UCCollator, TEC, Memory Manager) com a ICU do Linux.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
L=$ROOT/userland/libsystem
T=$(mktemp -d "${TMPDIR:-/tmp}/cs-legacy.XXXXXX")
trap 'rm -rf "$T"' EXIT
gcc -O1 -Wall -Wno-unused-function -DFDN_ICU_HOST -I"$L" "$L/fdn_icu.c" "$L/fdn_encoding_core.c" "$ROOT/tools/cs_legacy_host.c" -ldl -o "$T/t"
"$T/t"
