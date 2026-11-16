#!/bin/bash
# Teste de host do núcleo do AVVideoComposition (userland/libsystem/avvcomp_core.c): rampas e composição.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
gcc -std=gnu11 -O1 -Wall -o /tmp/avvcomp_test "$ROOT/tools/avvcomp_test.c" "$ROOT/userland/libsystem/avvcomp_core.c"
/tmp/avvcomp_test
rm -f /tmp/avvcomp_test
