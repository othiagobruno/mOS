#!/bin/sh
# Teste no host do SecAsn1 (userland/libsystem/secasn1.c): compila com um libc.h de mentira e roda tools/secasn1_host.c.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
printf '#pragma once\n#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\ntypedef uint8_t u8;\ntypedef uint32_t u32;\n' > "$T/libc.h"
cp "$ROOT/userland/libsystem/secasn1.c" "$T/"
cc -O1 -Wall -Wno-unused-parameter -I"$T" "$T/secasn1.c" "$ROOT/tools/secasn1_host.c" -o "$T/t"
"$T/t"
