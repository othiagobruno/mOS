#!/bin/bash
# Teste de host do núcleo UVC (kernel/dev/uvc_core.c): descritores, PROBE/COMMIT, alt e remontagem.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
gcc -std=gnu11 -O1 -Wall -o /tmp/uvc_core_test "$ROOT/tools/uvc_core_test.c" "$ROOT/kernel/dev/uvc_core.c"
/tmp/uvc_core_test
