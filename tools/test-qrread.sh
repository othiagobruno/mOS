#!/bin/bash
# Teste de host do leitor de QR do CIDetector (qrread.c), com códigos gerados pelo qrgen.c.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
S="$ROOT/userland/frameworks/CoreImage"
gcc -std=gnu11 -O1 -Wall -o /tmp/test-qrread "$ROOT/tools/test-qrread.c" "$S/qrread.c" "$S/qrgen.c" -lm
/tmp/test-qrread
rm -f /tmp/test-qrread
