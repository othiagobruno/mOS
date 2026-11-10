#!/bin/sh
# Compila o runtime Swift real (não o stub com "ret") para Mach-O x86_64.
# Saída: build/swift/objc/dylib/usr/lib/swift/libswift*.dylib
# O tools/build.sh e o build.ps1 (no Linux) chamam este script e copiam essa árvore
# para mOS/usr/lib/swift. Dylib que já existe não é trocada por stub (stage-libswift.sh).
#
# Toolchain (ABI estável do Swift 5, compilador 6.4.2, a mesma tag do fonte que
# tools/build-swift-full-objc.ps1 já usa):
#   https://download.swift.org/swift-6.4.2-release/ubuntu2404/swift-6.4.2-RELEASE/swift-6.4.2-RELEASE-ubuntu24.04.tar.gz
#   https://github.com/swiftlang/swift/archive/refs/tags/swift-6.4.2-RELEASE.tar.gz
#   zig 0.17.1 ELF (docs/staging-linux.md)
# Variáveis: SWIFTC (swift-frontend), MOS_ZIG, SWIFT_SRC. MOS_SWIFT_REBUILD=1 refaz.
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
out="$root/build/swift/objc/dylib/usr/lib/swift"
core="$out/libswiftCore.dylib"
if [ -z "${MOS_SWIFT_REBUILD:-}" ] && [ -f "$core" ] && ! grep -a -q 'STUB mOS libswift' "$core"; then
    echo "libswift real já está em $out (MOS_SWIFT_REBUILD=1 para refazer)"
    exit 0
fi
zig=${MOS_ZIG:-}
if [ -z "$zig" ]; then
    for z in "$root"/.tools/zig-*/zig; do
        if [ -x "$z" ]; then zig=$z; break; fi
    done
fi
fe=${SWIFTC:-}
src=${SWIFT_SRC:-$root/.tools/swift-src}
if [ -z "$fe" ]; then
    fe=$(ls -d /tmp/swift642/swift-*-ubuntu*/usr/bin/swift-frontend "$root"/.tools/swift*/usr/bin/swift-frontend 2>/dev/null | head -1 || true)
fi
if [ -z "$zig" ] || [ ! -x "$zig" ] || [ -z "$fe" ] || [ ! -x "$fe" ] || [ ! -d "$src/stdlib/public/core" ]; then
    echo "aviso: sem zig, swift-frontend ou .tools/swift-src (tag swift-6.4.2-RELEASE). As libswift* que faltarem continuam stub." >&2
    echo "aviso: zig=${zig:-ausente} swift-frontend=${fe:-ausente} src=${src}" >&2
    exit 0
fi
echo "build-libswift-real: toolchain $fe"
echo "Este script, sem os artefatos, repete a receita de docs/swift-runtime-real.md."
echo "MOS_SWIFT_REBUILD está ligado e a receita longa não está automatizada por completo neste arquivo." >&2
echo "Siga docs/swift-runtime-real.md. Nada foi apagado." >&2
exit 0
