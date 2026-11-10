#!/bin/bash
# Linux: o mesmo teste de tools/test-uikit-swift.ps1.
# As asserções ficam em userland/tests-swiftobjc/uikit/main.swift (o .ps1 compila esse arquivo).
# Este script não reimplementa a ligação Swift: no Linux falta o toolchain que o .ps1 acha em
# .tools/swift/LocalApp/Programs/Swift/Toolchains/*/usr/bin/swiftc.exe e o build/swift/full.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
swiftc=""
if command -v swiftc >/dev/null 2>&1; then swiftc=$(command -v swiftc); fi
shopt -s nullglob
cand=("$root"/.tools/swift/LocalApp/Programs/Swift/Toolchains/*/usr/bin/swiftc "$root"/.tools/swift/LocalApp/Programs/Swift/Toolchains/*/usr/bin/swiftc.exe)
shopt -u nullglob
if [[ -z "$swiftc" && ${#cand[@]} -gt 0 && -x "${cand[0]}" ]]; then swiftc=${cand[0]}; fi
if [[ -z "$swiftc" || ! -d "$root/build/swift/full" || ! -d "$root/userland/tests-swiftobjc/uikit" ]]; then
    echo "test-uikit-swift não rodou: falta o toolchain Swift do mOS (swiftc) ou build/swift/full." >&2
    echo "Fonte das asserções: userland/tests-swiftobjc/uikit/main.swift (a mesma do test-uikit-swift.ps1)." >&2
    exit 2
fi
echo "swiftc encontrado em $swiftc, mas a ligação do overlay (link-swift-objc.ps1) continua no PowerShell." >&2
exit 2
