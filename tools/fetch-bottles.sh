#!/usr/bin/env bash
# mOS — baixa bottles Intel do Homebrew (tag "<versão>.sonoma") do ghcr.io com as dependências de execução.
#   tools/fetch-bottles.sh <fórmula>... [-d build/brew]
# As versões novas das fórmulas não têm mais bottle Intel macOS: procura a tag mais nova que ainda tem.
# As dependências vêm do "sh.brew.tab" da entrada .sonoma do manifesto (runtime_dependencies), não da API atual.
# Cada bottle é conferido pelo sha256 do manifesto e extraído em <dir>/x/<fórmula>/<versão>.
set -u
dir=build/brew
args=()
while [ $# -gt 0 ]; do
    case "$1" in -d) dir=$2; shift 2;; *) args+=("$1"); shift;; esac
done
mkdir -p "$dir/dl" "$dir/x"
declare -A done_
fetch() {
    local f=$1 repo=${1//@//} tok v d
    [ -n "${done_[$f]:-}" ] && return 0
    done_[$f]=1
    if ls -d "$dir/x/$f"/*/ >/dev/null 2>&1; then echo "= $f (já extraído)"
    else
        tok=$(curl -s "https://ghcr.io/token?scope=repository:homebrew/core/$repo:pull" | sed 's/.*"token":"\([^"]*\)".*/\1/')
        d=""
        for v in $(curl -s -H "Authorization: Bearer $tok" "https://ghcr.io/v2/homebrew/core/$repo/tags/list" | tr ',' '\n' | tr -d '"]}' | grep -v name | tail -25 | tac); do
            curl -s -H "Authorization: Bearer $tok" -H "Accept: application/vnd.oci.image.index.v1+json" \
                 "https://ghcr.io/v2/homebrew/core/$repo/manifests/$v" -o "$dir/dl/$f.manifest.json"
            # bottle Intel "sonoma" ou, para fórmulas sem binário (ex.: ca-certificates), o "all"
            d=$(grep -oE '"org.opencontainers.image.ref.name": "[^"]*\.(sonoma|all)"|"sh.brew.bottle.digest": "[^"]*"' "$dir/dl/$f.manifest.json" |
                grep -E -A1 '\.(sonoma|all)"' | tail -1 | sed 's/.*: "\(.*\)"/\1/')
            if [ -n "$d" ] && ! echo "$d" | grep -qE 'sonoma|all'; then break; fi
            d=""
        done
        if [ -z "$d" ]; then echo "! $f: nenhuma tag com bottle sonoma"; return 1; fi
        local tgz="$dir/dl/$f-$v.sonoma.tar.gz"
        curl -sL -H "Authorization: Bearer $tok" "https://ghcr.io/v2/homebrew/core/$repo/blobs/sha256:$d" -o "$tgz"
        if ! echo "$d *$tgz" | sha256sum -c --quiet; then echo "! $f: sha256 não confere"; return 1; fi
        tar -xzf "$tgz" -C "$dir/x" 2>/dev/null   # symlinks de man pages falham no Windows; os binários não
        echo "+ $f $v"
    fi
    # dependências: "sh.brew.tab" da entrada .sonoma do manifesto (o receipt só nasce na instalação)
    local man="$dir/dl/$f.manifest.json"
    if [ ! -s "$man" ]; then
        local ver
        ver=$(basename "$(ls -d "$dir/x/$f"/*/ | head -1)")
        tok=$(curl -s "https://ghcr.io/token?scope=repository:homebrew/core/$repo:pull" | sed 's/.*"token":"\([^"]*\)".*/\1/')
        curl -s -H "Authorization: Bearer $tok" -H "Accept: application/vnd.oci.image.index.v1+json" \
             "https://ghcr.io/v2/homebrew/core/$repo/manifests/$ver" -o "$man"
    fi
    local deps
    deps=$(grep -E -A12 '\.(sonoma|all)"' "$man" | grep -m1 'sh.brew.tab' | grep -o 'full_name\\":\\"[^\\]*' | sed 's/.*"//')
    for dep in $deps; do fetch "$dep"; done
}
for f in "${args[@]}"; do fetch "$f"; done
