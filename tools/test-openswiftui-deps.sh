#!/bin/bash
# Linux: o mesmo teste de tools/test-openswiftui-deps.ps1.
# As asserções ficam em userland/sdk/openswiftui/tests/main.swift (e essentials.swift com -Essentials).
# O .ps1 é quem compila esses fontes. Aqui só falta o toolchain Swift e as bibliotecas já construídas.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
if ! command -v swiftc >/dev/null 2>&1 && ! compgen -G "$root/.tools/swift/LocalApp/Programs/Swift/Toolchains/*/usr/bin/swiftc*" >/dev/null; then
    echo "test-openswiftui-deps não rodou: falta swiftc (.tools/swift ou PATH) e build/swift/full." >&2
    echo "Fonte das asserções: userland/sdk/openswiftui/tests/main.swift (a mesma do .ps1)." >&2
    exit 2
fi
echo "swiftc presente, mas o restante do build (Foundation, OpenCoreGraphics, runtime) segue o .ps1." >&2
exit 2
