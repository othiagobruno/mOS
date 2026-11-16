#!/bin/bash
# Teste de host do lsregister (userland/cli-macos/lsregister.c) sobre tools/lsregister_shim.c.
#   bash tools/lsregistertest.sh   -> "lsregister: N/N ok"
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
gcc -std=gnu11 -O1 -Wall "$ROOT/userland/cli-macos/lsregister.c" "$ROOT/tools/lsregister_shim.c" -o "$T/lsregister" || exit 1
L="$T/lsregister"
export LSREG_LOG="$T/log"
ok=0; total=0
check() { total=$((total + 1)); if eval "$2"; then ok=$((ok + 1)); else echo "FALHA: $1"; fi; }
mkdir -p "$T/apps/Um.app/Contents" "$T/apps/sub/Dois.app/Contents" "$T/apps/Ruim.app"
touch "$T/apps/Um.app/Contents/Info.plist" "$T/apps/sub/Dois.app/Contents/Info.plist"
out=$("$L" -v "$T/apps/Um.app/"); check "registra com -v" '[ "$out" = "registrado: $T/apps/Um.app (com.teste.Um)" ]'
"$L" "$T/apps/Ruim.app" 2> "$T/err"; rc=$?
check "sem Info.plist" '[ $rc = 1 ] && grep -q "não é um bundle" "$T/err"'
"$L" "$T/nada.app" 2> "$T/err"; rc=$?
check "caminho inexistente" '[ $rc = 1 ] && grep -q "No such file" "$T/err"'
: > "$LSREG_LOG"; "$L" -R "$T/apps" 2>/dev/null; rc=$?
check "-R acha os .app nas subpastas (e reporta o ruim)" '[ $rc = 1 ] && grep -q "sub/Dois.app" "$LSREG_LOG" && grep -q "/Um.app" "$LSREG_LOG"'
out=$(cd "$T/apps" && "$L" -v Um.app); check "caminho relativo" 'printf "%s" "$out" | grep -q "$T/apps/Um.app"'
out=$("$L" "$T/apps/Um.app" -dump); check "-dump" 'printf "%s" "$out" | grep -q "path:           $T/apps/Um.app" && printf "%s" "$out" | tail -1 | grep -q "1 apps"'
out=$("$L" "$T/apps/Um.app" -v -u "$T/apps/Um.app"); check "-u" '[ "$out" = "desregistrado: $T/apps/Um.app" ]'
"$L" -u "$T/apps/Um.app" 2> "$T/err"; rc=$?
check "-u do que não está" '[ $rc = 1 ] && grep -q "não estava registrado" "$T/err"'
out=$("$L" -v -kill -r -domain local -domain user); check "-kill -r -domain" '[ "$(printf "%s\n" "$out" | head -1)" = "7 apps registrados" ]'
"$L" 2> "$T/err"; rc=$?
check "sem argumentos" '[ $rc = 1 ] && grep -q usage "$T/err"'
"$L" -zz 2> /dev/null; rc=$?
check "opção desconhecida" '[ $rc = 1 ]'
echo "lsregister: $ok/$total $([ $ok = $total ] && echo ok || echo FALHOU)"
[ $ok = $total ]
