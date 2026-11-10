#!/bin/bash
# Linux: o mesmo teste de tools/test-openswiftui-path.ps1.
# As asserções ficam em userland/sdk/openswiftui/tests/path/main.swift.
# O placar que o .ps1 exige é a linha "TUDO OK", impressa pelo próprio teste.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
if ! command -v swiftc >/dev/null 2>&1 && ! compgen -G "$root/.tools/swift/LocalApp/Programs/Swift/Toolchains/*/usr/bin/swiftc*" >/dev/null; then
    echo "test-openswiftui-path não rodou: falta swiftc (.tools/swift ou PATH), build/swift/full e as deps do OpenSwiftUI." >&2
    echo "Fonte das asserções: userland/sdk/openswiftui/tests/path/main.swift (a mesma do .ps1)." >&2
    exit 2
fi
echo "swiftc presente, mas o restante do build segue o .ps1." >&2
exit 2
