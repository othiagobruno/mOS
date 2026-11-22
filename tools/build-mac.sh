#!/bin/bash
# mOS — build no macOS (Apple silicon ou Intel), sem Docker nem VM. Prepara o ambiente e roda o mesmo tools/build.sh:
#   - ferramentas GNU na frente do PATH (coreutils, findutils, sed, tar, grep, xargs e timeout do Homebrew): o build.sh
#     e os scripts que ele chama usam sed -i, stat -c, readlink -f, sha256sum, xargs -a/-P...;
#   - o zig do Homebrew (Mach-O, o que o pick_zig do build.sh pede no Darwin) copiado para build/zig-mac com lib/ ao
#     lado, o leiaute que os scripts esperam (o Homebrew guarda a lib em lib/zig);
#   - MOS_HOST_MAC=1: o build.sh pula os poucos testes de host que dependem de chamadas do Linux.
# Uso: tools/build-mac.sh [--out DIR] [--jobs N]. Pré-requisitos: brew install zig coreutils findutils gnu-sed gnu-tar
#      qemu python (grep do GNU é opcional) (e Xcode Command Line Tools para o cc/gcc do host).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
[ "$(uname -s)" = Darwin ] || { echo "build-mac: este script é só para macOS; no Linux use tools/build.sh" >&2; exit 2; }
BREW=$(command -v brew >/dev/null 2>&1 && brew --prefix || echo /opt/homebrew)

# ---- ferramentas GNU
GNU="$ROOT/build/gnubin-mac"
mkdir -p "$GNU"
falta=()
for f in coreutils findutils gnu-sed gnu-tar; do
    d="$BREW/opt/$f/libexec/gnubin"
    [ -d "$d" ] || falta+=("$f")
done
for t in gxargs gtimeout zig python3 qemu-system-x86_64; do command -v "$t" >/dev/null 2>&1 || [ -x "$BREW/bin/$t" ] || falta+=("$t"); done
if [ ${#falta[@]} -gt 0 ]; then
    echo "build-mac: faltam ${falta[*]}. Instale: brew install zig coreutils findutils gnu-sed gnu-tar qemu python" >&2
    exit 1
fi
ln -sf "$BREW/bin/gxargs" "$GNU/xargs"
ln -sf "$BREW/bin/gtimeout" "$GNU/timeout"
export PATH="$GNU:$BREW/opt/coreutils/libexec/gnubin:$BREW/opt/findutils/libexec/gnubin:$BREW/opt/gnu-sed/libexec/gnubin:$BREW/opt/gnu-tar/libexec/gnubin:$BREW/bin:$PATH"
[ -d "$BREW/opt/grep/libexec/gnubin" ] && PATH="$BREW/opt/grep/libexec/gnubin:$PATH"    # opcional: o grep do macOS serve

# ---- zig: o binário do Homebrew com lib/ do lado (zig acha a lib a partir do próprio executável)
ZSRC=$(readlink -f "$(command -v zig)")
ZLIB=
for c in "$(dirname "$ZSRC")/../lib/zig" "$(dirname "$ZSRC")/../lib" "$(dirname "$ZSRC")/lib"; do
    [ -f "$c/compiler_rt.zig" ] && { ZLIB=$(cd "$c" && pwd); break; }
done
[ -n "$ZLIB" ] || { echo "build-mac: não achei a lib do zig perto de $ZSRC" >&2; exit 1; }
ZD="$ROOT/build/zig-mac"
mkdir -p "$ZD"
if ! cmp -s "$ZSRC" "$ZD/zig"; then cp -f "$ZSRC" "$ZD/zig"; fi
ln -sfn "$ZLIB" "$ZD/lib"
export MOS_ZIG="$ZD/zig" ZIG="$ZD/zig"
echo "build-mac: zig $("$MOS_ZIG" version) em $ZD (lib -> $ZLIB)"

# ---- Swift: o swift-frontend do Xcode, se for o 6.4.2 que o build pede (o toolchain baixado é o do Linux)
if [ -z "${SWIFTC:-}" ] && FE=$(xcrun --find swift-frontend 2>/dev/null); then
    if "$FE" -version 2>&1 | grep -q "Swift version 6\.4\.2"; then
        export SWIFTC="$FE"
        echo "build-mac: swift-frontend do Xcode ($FE)"
    else
        echo "build-mac: aviso: o Swift do Xcode não é o 6.4.2; as libswift* podem sair stub" >&2
    fi
fi

export MOS_HOST_MAC=1
JOBS=$(sysctl -n hw.ncpu)
exec bash "$ROOT/tools/build.sh" --jobs "$JOBS" "$@"
