#!/bin/bash
# Teste de host dos algoritmos de Vision sem modelo (userland/libsystem/vision_core2.c).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
gcc -std=gnu11 -O1 -Wall -o /tmp/vision_core2_test "$ROOT/tools/vision_core2_test.c" "$ROOT/userland/libsystem/vision_core2.c" -lm
/tmp/vision_core2_test
rm -f /tmp/vision_core2_test
