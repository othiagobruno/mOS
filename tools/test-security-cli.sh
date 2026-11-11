#!/bin/bash
# mOS — teste de host do /usr/bin/security (userland/cli-macos/security.c) no machotest, contra o libSystem.mos.
#   tools/test-security-cli.sh <libSystem.mos> <machotest>
# Confere: find-certificate -a -p devolve as mesmas raízes (DER a DER) do assets/certs/cacert.pem que o kernel
# embute, também com keychain nomeado; e o ciclo add/find/-U/delete de senha genérica no Keychain do mOS.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LIBSYS=${1:?uso: test-security-cli.sh <libSystem.mos> <machotest>}
MT=${2:?uso: test-security-cli.sh <libSystem.mos> <machotest>}
ZIG=${MOS_ZIG:-zig}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/vfs/usr/bin" "$T/vfs/Users/Visitante"   # Keychains: o libSystem cria com 0700
"$ZIG" cc -target x86_64-macos -O2 "$ROOT/userland/cli-macos/security.c" -o "$T/vfs/usr/bin/security" -Wl,-undefined,dynamic_lookup
fails=0
ok() { echo "ok   $1"; }
bad() { echo "FALHOU $1"; fails=$((fails + 1)); }
sec() {   # sec <saida> <args...>; devolve o código de saída do security
    local out=$1; shift
    local rc=0
    (cd "$ROOT" && MACHOTEST_PATH=/usr/bin/security HOME=/Users/Visitante timeout 60 "$MT" "$LIBSYS" "$T/vfs" "$T/vfs/usr/bin/security" "$@") \
        > "$out" 2> "$out.err" || rc=$?
    return $rc
}
rc_of() { local rc=0; sec "$@" || rc=$?; echo $rc; }

# 1. raízes: o mesmo conjunto DER do cacert.pem
rc=$(rc_of "$T/roots.pem" find-certificate -a -p)
[ "$rc" = 0 ] && ok "find-certificate -a -p sai 0" || bad "find-certificate -a -p saiu $rc"
python3 -I - "$T/roots.pem" "$ROOT/assets/certs/cacert.pem" <<'PY' && ok "PEM igual ao cacert.pem (DER a DER)" || bad "PEM difere do cacert.pem"
import base64, re, sys
def ders(p):
    t = open(p, encoding="ascii", errors="replace").read()
    return [base64.b64decode("".join(b.split())) for b in re.findall(r"-----BEGIN CERTIFICATE-----(.*?)-----END CERTIFICATE-----", t, re.S)]
a, b = ders(sys.argv[1]), ders(sys.argv[2])
print("     raízes: %d no security, %d no cacert.pem" % (len(a), len(b)))
long_lines = [l for l in open(sys.argv[1]).read().splitlines() if len(l) > 64]
sys.exit(0 if a and sorted(a) == sorted(b) and not long_lines else 1)
PY
rc=$(rc_of "$T/sys.pem" find-certificate -a -p /System/Library/Keychains/SystemRootCertificates.keychain)
[ "$rc" = 0 ] && cmp -s "$T/sys.pem" "$T/roots.pem" && ok "SystemRootCertificates.keychain = mesma lista" || bad "SystemRootCertificates.keychain (rc $rc)"
# o mOS tem um Keychain só: o nome na linha de comando é aceito e ignorado (userland/cli-macos/security.c)
rc=$(rc_of "$T/login.pem" find-certificate -a -p /Users/Visitante/Library/Keychains/login.keychain-db)
[ "$rc" = 0 ] && cmp -s "$T/login.pem" "$T/roots.pem" && ok "keychain nomeado: a mesma lista" || bad "login.keychain saiu $rc"
rc=$(rc_of "$T/c1.pem" find-certificate -c "ISRG Root X1" -p)
n=$(grep -c 'BEGIN CERTIFICATE' "$T/c1.pem" || true)
[ "$rc" = 0 ] && [ "$n" = 1 ] && ok "find-certificate -c 'ISRG Root X1' acha 1" || bad "-c ISRG Root X1 (rc $rc, $n certificados)"

# 2. senha genérica no Keychain do mOS (arquivo em $HOME/Library/Keychains do VFS)
rc=$(rc_of "$T/f0" find-generic-password -s mos-teste -a visitante -w)
[ "$rc" = 44 ] && ok "senha ausente sai 44" || bad "senha ausente saiu $rc"
rc=$(rc_of "$T/a1" add-generic-password -s mos-teste -a visitante -w segredo-1)
[ "$rc" = 0 ] && ok "add-generic-password" || bad "add-generic-password saiu $rc"
rc=$(rc_of "$T/f1" find-generic-password -s mos-teste -a visitante -w)
[ "$rc" = 0 ] && [ "$(cat "$T/f1")" = segredo-1 ] && ok "find-generic-password -w devolve a senha (outro processo)" || bad "find -w: rc $rc, '$(cat "$T/f1")'"
rc=$(rc_of "$T/a2" add-generic-password -s mos-teste -a visitante -w segredo-2)
[ "$rc" = 45 ] && ok "duplicado sem -U sai 45" || bad "duplicado saiu $rc"
rc=$(rc_of "$T/a3" add-generic-password -U -s mos-teste -a visitante -w segredo-2)
rc2=$(rc_of "$T/f2" find-generic-password -s mos-teste -a visitante -w)
[ "$rc" = 0 ] && [ "$rc2" = 0 ] && [ "$(cat "$T/f2")" = segredo-2 ] && ok "-U troca a senha" || bad "-U: rc $rc/$rc2, '$(cat "$T/f2")'"
rc=$(rc_of "$T/d1" delete-generic-password -s mos-teste -a visitante)
rc2=$(rc_of "$T/f3" find-generic-password -s mos-teste -a visitante -w)
[ "$rc" = 0 ] && [ "$rc2" = 44 ] && ok "delete-generic-password apaga" || bad "delete: rc $rc, depois $rc2"
rc=$(rc_of "$T/u" comando-que-nao-existe)
[ "$rc" = 2 ] && ok "comando desconhecido é erro de uso (2)" || bad "comando desconhecido saiu $rc"

if [ "$fails" = 0 ]; then echo "security-cli: tudo ok"; else echo "security-cli: $fails falha(s)"; exit 1; fi
