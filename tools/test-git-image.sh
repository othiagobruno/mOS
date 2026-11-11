#!/bin/bash
# mOS — teste de host do git da imagem (tools/stage-git.sh) no machotest, contra o libSystem.mos.
#   tools/test-git-image.sh <libSystem.mos> <machotest> <bottles>
# <bottles>: o -d do tools/fetch-bottles.sh git gettext. Monta uma raiz só com o git, a pcre2, a libintl e a libz
# do build (build/zlib-x86_64), e roda o Mach-O x86_64 do Homebrew: --version, init, add, commit, log, status, diff.
# O harness não emula fork(): a manutenção automática depois do commit fica desligada (-c maintenance.auto=false).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LIBSYS=${1:?uso: test-git-image.sh <libSystem.mos> <machotest> <bottles>}
MT=${2:?uso: test-git-image.sh <libSystem.mos> <machotest> <bottles>}
B=${3:?uso: test-git-image.sh <libSystem.mos> <machotest> <bottles>}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
V=$T/vfs
mkdir -p "$V/usr/lib" "$V/tmp" "$V/Users/Visitante"
bash "$ROOT/tools/stage-git.sh" "$B" "$V"
if [ -f "$ROOT/build/zlib-x86_64/libz.1.dylib" ]; then cp -f "$ROOT/build/zlib-x86_64/libz.1.dylib" "$V/usr/lib/"
else echo "test-git-image: falta build/zlib-x86_64/libz.1.dylib (tools/build-libz.sh)" >&2; exit 1; fi
echo "raiz do git: $(du -sh "$V" | cut -f1) (o bottle copiado sem links tem 619 MiB)"
fails=0
ok() { echo "ok   $1"; }
bad() { echo "FALHOU $1"; fails=$((fails + 1)); }
G() {
    MACHOTEST_PATH=/usr/bin/git HOME=/Users/Visitante timeout 120 "$MT" "$LIBSYS" "$V" "$V/usr/local/opt/git/bin/git" \
        -c maintenance.auto=false -c gc.auto=0 -c user.name=mOS -c user.email=teste@mos.invalid "$@" 2> "$T/err" |
        tr -d '\r'
}
[ "$(head -c 11 "$V/usr/bin/git")" = '#mOS-link:/' ] && ok "/usr/bin/git é link do mOS para /usr/local/opt/git/bin/git" || bad "/usr/bin/git não é link"
[ -f "$V/usr/local/opt/pcre2/lib/libpcre2-8.0.dylib" ] && [ -f "$V/usr/local/opt/gettext/lib/libintl.8.dylib" ] &&
    ok "pcre2 e libintl em /usr/local/opt" || bad "dylibs do git ausentes"
v=$(G --version)
[[ "$v" == "git version "* ]] && ok "$v" || { bad "--version: '$v'"; cat "$T/err"; }
G init -q -b main /tmp/repo >/dev/null
[ -f "$V/tmp/repo/.git/HEAD" ] && ok "git init cria .git" || { bad "git init"; cat "$T/err"; }
printf 'ola mOS\n' > "$V/tmp/repo/a.txt"
G -C /tmp/repo add a.txt >/dev/null
G -C /tmp/repo commit -q -m primeiro >/dev/null
l=$(G -C /tmp/repo log --format=%s)
[ "$l" = primeiro ] && ok "add + commit + log ('$l')" || { bad "log: '$l'"; cat "$T/err"; }
h=$(G -C /tmp/repo rev-parse HEAD:a.txt)
want=$(python3 -I -c 'import hashlib; d=b"ola mOS\n"; print(hashlib.sha1(b"blob %d\0" % len(d) + d).hexdigest())')
[ "$h" = "$want" ] && ok "blob a.txt = $h (SHA-1 do objeto conferido)" || bad "hash do blob $h, esperado $want"
s=$(G -C /tmp/repo status --short)
[ -z "$s" ] && ok "status limpo depois do commit" || bad "status: '$s'"
echo mudou >> "$V/tmp/repo/a.txt"
s=$(G -C /tmp/repo status --short)
[ "$s" = " M a.txt" ] && ok "status vê a mudança ('$s')" || bad "status: '$s'"
d=$(G -C /tmp/repo diff --numstat)
[ "$(printf '%s' "$d" | awk '{print $1, $2, $3}')" = "1 0 a.txt" ] && ok "diff --numstat: $d" || bad "diff: '$d'"
G -C /tmp/repo checkout -q -b ramo >/dev/null
G -C /tmp/repo commit -q -am segundo >/dev/null
b=$(G -C /tmp/repo log --format=%s main..ramo)
[ "$b" = segundo ] && ok "branch + commit -a ('$b')" || { bad "branch: '$b'"; cat "$T/err"; }
if [ "$fails" = 0 ]; then echo "git-image: tudo ok"; else echo "git-image: $fails falha(s)"; exit 1; fi
