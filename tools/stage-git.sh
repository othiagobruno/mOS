#!/bin/bash
# mOS — põe o git do Homebrew (bottle Intel, tools/fetch-bottles.sh git) numa raiz do mOS.
#   tools/stage-git.sh <bottles> <raiz-mOS>
# <bottles> é o -d do fetch-bottles.sh (com x/<fórmula>/<versão>/) ou uma pasta com <fórmula>/bin/git.
# O bottle tem ~65 MiB, mas libexec/git-core são ~180 hardlinks do mesmo binário: copiados, viram 619 MiB.
# Cada arquivo de libexec idêntico ao bin/git vira um link do mOS (#mOS-link:), que o loader segue.
# Entram as dylibs que o git liga por @@HOMEBREW_PREFIX@@ (o loader troca por /usr/local):
#   opt/pcre2/lib/libpcre2-8.0.dylib e opt/gettext/lib/libintl.8.dylib.
# Saída: /usr/local/opt/git/{bin,libexec,share/git-core}, /usr/local/bin/git e /usr/bin/git (links).
set -euo pipefail
B=${1:?uso: stage-git.sh <bottles> <raiz-mOS>}
MOS=${2:?uso: stage-git.sh <bottles> <raiz-mOS>}

# x/<f>/<versão>/ (fetch-bottles.sh) ou <f>/ direto
formula_dir() {
    local f=$1 d
    for d in "$B/x/$f"/*/ "$B/$f"/*/ "$B/$f/"; do
        d=${d%/}
        if [ -d "$d/bin" ] || [ -d "$d/lib" ]; then printf '%s\n' "$d"; return 0; fi
    done
    return 1
}

GIT=$(formula_dir git) || { echo "stage-git: bottle do git ausente em $B (tools/fetch-bottles.sh git gettext -d $B)" >&2; exit 1; }
[ -f "$GIT/bin/git" ] || { echo "stage-git: $GIT/bin/git ausente" >&2; exit 1; }

OPT=$MOS/usr/local/opt/git
rm -rf "$OPT"
mkdir -p "$OPT/bin" "$OPT/libexec/git-core" "$MOS/usr/local/bin" "$MOS/usr/bin"
LINK='#mOS-link:/usr/local/opt/git/bin/git'
cp -f "$GIT/bin/git" "$OPT/bin/git"
chmod +x "$OPT/bin/git"
for f in "$GIT/bin"/*; do
    b=$(basename "$f")
    [ "$b" = git ] && continue
    if [ -f "$f" ] && cmp -s "$f" "$GIT/bin/git"; then printf '%s' "$LINK" > "$OPT/bin/$b"
    elif [ -f "$f" ]; then cp -f "$f" "$OPT/bin/$b"; fi
done
nlink=0
ncopy=0
for f in "$GIT/libexec/git-core"/*; do
    b=$(basename "$f")
    if [ -L "$f" ]; then             # git-remote-https -> git-remote-http
        t=$(readlink "$f")
        case "$t" in /*) ;; *) t=/usr/local/opt/git/libexec/git-core/$t ;; esac
        [ "$(basename "$t")" = git ] && t=/usr/local/opt/git/bin/git
        printf '#mOS-link:%s' "$t" > "$OPT/libexec/git-core/$b"
        nlink=$((nlink + 1))
        continue
    fi
    if [ -d "$f" ]; then cp -a "$f" "$OPT/libexec/git-core/$b"; continue; fi
    [ -f "$f" ] || continue
    if cmp -s "$f" "$GIT/bin/git"; then printf '%s' "$LINK" > "$OPT/libexec/git-core/$b"; nlink=$((nlink + 1))
    else cp -f "$f" "$OPT/libexec/git-core/$b"; ncopy=$((ncopy + 1)); fi
done
# modelos do git init e o resto de share/git-core
if [ -d "$GIT/share/git-core" ]; then mkdir -p "$OPT/share"; cp -a "$GIT/share/git-core" "$OPT/share/"; fi
printf '%s' "$LINK" > "$MOS/usr/local/bin/git"
printf '%s' "$LINK" > "$MOS/usr/bin/git"

dep() {
    local f=$1 lib=$2 d
    d=$(formula_dir "$f") || { echo "stage-git: falta o bottle $f ($lib)" >&2; exit 1; }
    [ -f "$d/lib/$lib" ] || { echo "stage-git: $d/lib/$lib ausente" >&2; exit 1; }
    mkdir -p "$MOS/usr/local/opt/$f/lib"
    cp -fL "$d/lib/$lib" "$MOS/usr/local/opt/$f/lib/$lib"
}
dep pcre2 libpcre2-8.0.dylib
dep gettext libintl.8.dylib
echo "stage-git: $(basename "$GIT") em /usr/local/opt/git ($nlink links, $ncopy arquivos próprios em libexec), pcre2 e libintl"
