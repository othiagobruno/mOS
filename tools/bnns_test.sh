#!/bin/bash
# Teste de host do BNNS BroadcastMatMul (userland/libsystem/bnns_matmul.c).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
gcc -std=gnu11 -O1 -Wall -DBNNS_HOST -o /tmp/bnns_test "$ROOT/tools/bnns_test.c" "$ROOT/userland/libsystem/bnns_matmul.c" -lm
/tmp/bnns_test
rm -f /tmp/bnns_test
