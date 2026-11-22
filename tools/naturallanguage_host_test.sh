#!/bin/bash
# Teste de host do NaturalLanguage.framework do mOS contra o Foundation do macOS. Só roda no macOS (precisa do
# Foundation de verdade); em outro host sai 0 com o aviso. Uso: tools/naturallanguage_host_test.sh [dir]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/build/naturallanguage-host-test}
if [ "$(uname)" != Darwin ]; then echo "naturallanguage: pulado (precisa do Foundation do macOS)"; exit 0; fi
mkdir -p "$OUT"
clang -fno-objc-arc -w -c -x objective-c "$ROOT/userland/frameworks/NaturalLanguage/NaturalLanguage.m" -o "$OUT/nl.o"
clang -fobjc-arc -w -o "$OUT/t" "$ROOT/tools/naturallanguage_host_test.m" "$OUT/nl.o" -framework Foundation -framework AppKit
"$OUT/t"
