#!/bin/bash
# Baixa o que tools/build-libswift-real.sh, build-swift-combine.sh e build-frameworks-objc.sh procuram em .tools/:
#   .tools/swift-6.4.2   toolchain oficial swift-6.4.2-RELEASE para Ubuntu 24.04 (o swift-frontend que emite Mach-O)
#   .tools/swift-src     fonte swift-6.4.2-RELEASE (stdlib, runtime, SwiftShims)
# Sem os dois, a imagem sai com as libswift* stub (marcador "STUB mOS libswift"), sem Combine.framework e sem o
# overlay Swift do CoreData; o placar (tools/meta/placar.py) não conta stub, então o Setapp e o WhatsApp ficavam
# com centenas de ausentes de Combine, libswiftCore e libswiftDispatch.
#
# SHA-256 medidos no download de 10 nov. 2026 (download.swift.org e o tarball da tag no GitHub).
# Transporte: tools/baixar.sh (curl, wget ou python3). ~1,1 GB de download, ~4 GB extraído.
#   tools/fetch-swift-toolchain.sh
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$root/tools/baixar.sh"
VER=6.4.2
TC_URL="https://download.swift.org/swift-$VER-release/ubuntu2404/swift-$VER-RELEASE/swift-$VER-RELEASE-ubuntu24.04.tar.gz"
TC_SHA=2e9ebec040ad459dd89a6d093a02d8314939135104850fae4d5c0f174fb3ba8c
SRC_URL="https://github.com/swiftlang/swift/archive/refs/tags/swift-$VER-RELEASE.tar.gz"
SRC_SHA=c9c0c37995aa69994807eeb62c1e7e89bdd9f1a366dd8fc98a4bf65753dc6cbb
dl="$root/.tools/dl"
tc="$root/.tools/swift-$VER"
src="$root/.tools/swift-src"
mkdir -p "$dl"

if [ -x "$tc/usr/bin/swift-frontend" ]; then
    echo "fetch-swift-toolchain: toolchain já está em $tc"
else
    baixar "$TC_URL" "$dl/swift-$VER-RELEASE-ubuntu24.04.tar.gz" "$TC_SHA"
    rm -rf "$tc.partial"
    mkdir -p "$tc.partial"
    tar xzf "$dl/swift-$VER-RELEASE-ubuntu24.04.tar.gz" -C "$tc.partial" --strip-components=1
    [ -x "$tc.partial/usr/bin/swift-frontend" ] || { echo "fetch-swift-toolchain: tarball sem usr/bin/swift-frontend" >&2; exit 1; }
    rm -rf "$tc"
    mv "$tc.partial" "$tc"
    echo "fetch-swift-toolchain: toolchain em $tc"
fi

if [ -f "$src/stdlib/public/core/CMakeLists.txt" ]; then
    echo "fetch-swift-toolchain: fonte já está em $src"
else
    baixar "$SRC_URL" "$dl/swift-$VER-RELEASE.tar.gz" "$SRC_SHA"
    rm -rf "$src.partial"
    mkdir -p "$src.partial"
    tar xzf "$dl/swift-$VER-RELEASE.tar.gz" -C "$src.partial" --strip-components=1
    [ -f "$src.partial/stdlib/public/core/CMakeLists.txt" ] || { echo "fetch-swift-toolchain: fonte sem stdlib/public/core" >&2; exit 1; }
    rm -rf "$src"
    mv "$src.partial" "$src"
    echo "fetch-swift-toolchain: fonte em $src"
fi
# o frontend do Linux precisa rodar nesta máquina (Ubuntu 24.04 ou mais novo)
"$tc/usr/bin/swift-frontend" -version | head -1
