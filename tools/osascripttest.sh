#!/bin/bash
# Teste de host do osascript(1) (userland/cli-macos/osascript.c) sobre tools/osascript_shim.c (interpretador real).
#   bash tools/osascripttest.sh   -> "osascript: N/N ok"
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
gcc -std=gnu11 -O1 -Wall -Wno-unused-function -I"$ROOT/userland/libsystem" "$ROOT/userland/cli-macos/osascript.c" "$ROOT/tools/osascript_shim.c" -o "$T/osascript" || exit 1
O="$T/osascript"
ok=0; total=0
check() { total=$((total + 1)); if eval "$2"; then ok=$((ok + 1)); else echo "FALHA: $1"; fi; }
out=$("$O" -e 'set x to 2' -e 'x * 21'); check "-e em várias linhas" '[ "$out" = 42 ]'
out=$("$O" -e '"olá"'); check "texto legível" '[ "$out" = "olá" ]'
out=$("$O" -s s -e '{1, "a"}'); check "-s s" '[ "$out" = "{1, \"a\"}" ]'
out=$("$O" -e 'on run argv' -e 'return item 2 of argv & (count of argv)' -e 'end run' um dois tres); check "argv" '[ "$out" = "dois3" ]'
printf 'return "da entrada"\n' > "$T/s.applescript"
out=$("$O" "$T/s.applescript"); check "arquivo" '[ "$out" = "da entrada" ]'
out=$(printf '1 + 1\n' | "$O"); check "entrada padrão" '[ "$out" = 2 ]'
out=$(printf '#!/usr/bin/osascript\nreturn 5\n' | "$O" -); check "shebang" '[ "$out" = 5 ]'
out=$("$O" -e 'do shell script "echo oi; echo tchau"'); check "do shell script" '[ "$out" = "$(printf "oi\ntchau")" ]'
"$O" -e 'do shell script "echo ruim >&2; exit 3"' 2> "$T/err"; rc=$?
check "do shell script com erro" '[ $rc = 1 ] && grep -q "ruim (3)" "$T/err"'
"$O" -e 'error "deu errado" number 77' 2> "$T/err"; rc=$?
check "erro de execução" '[ $rc = 1 ] && grep -q "execution error: deu errado (77)" "$T/err"'
out=$("$O" -s o -e 'error "x" number 5' 2>/dev/null); check "-s o" 'printf "%s" "$out" | grep -q "execution error: x (5)"'
"$O" -e 'set x to (' 2> "$T/err"; rc=$?
check "erro de sintaxe" '[ $rc = 1 ] && grep -q "syntax error" "$T/err"'
"$O" -e 'tell application "Nada" to activate' 2> "$T/err"; rc=$?
check "app inexistente" '[ $rc = 1 ] && grep -q "(-10814)" "$T/err"'
"$O" -l JavaScript -e '1' 2> "$T/err"; rc=$?
check "-l JavaScript" '[ $rc = 1 ] && grep -q "não disponível" "$T/err"'
"$O" -x 2> "$T/err"; rc=$?
check "uso" '[ $rc = 1 ] && grep -q usage "$T/err"'
"$O" "$T/nao-existe" 2> "$T/err"; rc=$?
check "arquivo inexistente" '[ $rc = 1 ] && grep -q "No such file" "$T/err"'
out=$("$O" -e 'log "no log"' -e 'return' 2>&1); check "log no stderr" '[ "$out" = "(*no log*)" ]'
out=$(printf '2 + 3\n"x" & "y"\n' | "$O" -i); check "-i" 'printf "%s" "$out" | grep -q ">> 5" && printf "%s" "$out" | grep -q ">> xy"'
echo "osascript: $ok/$total $([ $ok = $total ] && echo ok || echo FALHOU)"
[ $ok = $total ]
