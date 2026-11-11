#!/usr/bin/env bash
# Teste de host do /usr/bin/security (userland/cli-macos/security.c): compila para o host com o Keychain falso de
# tools/security_cli_test.c e confere os subcomandos que VS Code/Cursor e scripts de credencial chamam.
# Não testa o Keychain real (userland/libsystem/security.c) nem roda no mOS.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CC=${CC:-cc}
T=$(mktemp -d "${TMPDIR:-/tmp}/security-teste.XXXXXX")
trap 'rm -rf "$T"' EXIT
export SECURITY_TESTE_KC=$T/kc SECURITY_TESTE_DER=$T/raiz.der
$CC -O1 -Wall -Wno-unused-function "$ROOT/userland/cli-macos/security.c" "$ROOT/tools/security_cli_test.c" -o "$T/security" || exit 1
# a primeira raiz do bundle da imagem, em DER
python3 -I - "$ROOT/assets/certs/cacert.pem" "$T/raiz.der" "$T/raiz.pem" <<'EOF' || exit 1
import base64, sys
t = open(sys.argv[1]).read()
i = t.index("-----BEGIN CERTIFICATE-----")
j = t.index("-----END CERTIFICATE-----", i) + len("-----END CERTIFICATE-----")
b = "".join(t[i:j].splitlines()[1:-1])
open(sys.argv[2], "wb").write(base64.b64decode(b))
open(sys.argv[3], "w").write(t[i:j] + "\n")
EOF

S=$T/security
falhas=0
ok() { echo "ok  $1"; }
ruim() { echo "FALHA $1"; falhas=$((falhas + 1)); }
espera() {   # espera <nome> <código esperado> <saída esperada (stdout) ou -> comando...
    local nome=$1 cod=$2 out=$3; shift 3
    local got rc
    got=$("$@" 2>"$T/err"); rc=$?
    if [ "$rc" != "$cod" ]; then ruim "$nome: código $rc, esperado $cod ($(head -c 200 "$T/err"))"; return; fi
    if [ "$out" != "-" ] && [ "$got" != "$out" ]; then ruim "$nome: saída '$got', esperado '$out'"; return; fi
    ok "$nome"
}

espera "find ausente -> 44" 44 "" "$S" find-generic-password -s svc.teste -a ana -w
espera "add genérica" 0 "" "$S" add-generic-password -s svc.teste -a ana -w s3nha
espera "find -w" 0 "s3nha" "$S" find-generic-password -s svc.teste -a ana -w
espera "add duplicada -> 45" 45 "" "$S" add-generic-password -s svc.teste -a ana -w outra
espera "add -U atualiza" 0 "" "$S" add-generic-password -U -s svc.teste -a ana -w nova
espera "find depois do -U" 0 "nova" "$S" find-generic-password -s svc.teste -a ana -w
"$S" find-generic-password -s svc.teste -a ana -g >"$T/out" 2>"$T/err"
if grep -q '^class: "genp"' "$T/out" && grep -q '"acct"<blob>="ana"' "$T/out" && grep -q '^password: "nova"' "$T/err"; then
    ok "find -g (atributos e password em stderr)"
else
    ruim "find -g: $(cat "$T/out" "$T/err" | head -c 400)"
fi
espera "delete genérica" 0 - "$S" delete-generic-password -s svc.teste -a ana
espera "find depois do delete -> 44" 44 "" "$S" find-generic-password -s svc.teste -a ana -w
espera "add internet" 0 "" "$S" add-internet-password -s github.com -a ana -w tok
espera "find internet -w" 0 "tok" "$S" find-internet-password -s github.com -a ana -w
espera "genérica não vê internet" 44 "" "$S" find-generic-password -s github.com -a ana -w
espera "delete internet" 0 - "$S" delete-internet-password -s github.com -a ana
espera "comando desconhecido -> 2" 2 "" "$S" nao-existe
"$S" find-certificate -a -p >"$T/certs" 2>"$T/err"
rc=$?
# mesmo DER, base64 em linhas de 64 colunas como o security da Apple
if [ $rc -eq 0 ] && python3 -I -c '
import base64, sys
l = open(sys.argv[1]).read().splitlines()
assert l[0] == "-----BEGIN CERTIFICATE-----" and l[-1] == "-----END CERTIFICATE-----"
assert all(len(x) == 64 for x in l[1:-2]) and 0 < len(l[-2]) <= 64
assert base64.b64decode("".join(l[1:-1])) == open(sys.argv[2], "rb").read()
' "$T/certs" "$T/raiz.der"; then
    ok "find-certificate -a -p (mesmo DER da primeira raiz do bundle)"
else
    ruim "find-certificate -a -p: $(diff "$T/certs" "$T/raiz.pem" | head -5) $(head -c 200 "$T/err")"
fi
espera "default-keychain" 0 '    "/Users/teste/Library/Keychains/login.keychain-db"' "$S" default-keychain

if [ $falhas -ne 0 ]; then echo "$falhas falha(s)"; exit 1; fi
echo "security-cli-teste: tudo ok"
