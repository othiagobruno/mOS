#!/bin/bash
# Teste de host do /usr/bin/tccutil (userland/cli-macos/tccutil.c) sobre o arquivo de decisões do TCC.
#   bash tools/tccutiltest.sh        -> "tccutil: N/N ok"
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
gcc -std=c11 -O1 -Wall -Wextra -Wno-unused-function "$ROOT/userland/cli-macos/tccutil.c" -o "$T/tccutil" || exit 1
export HOME="$T/home"
F="$HOME/Library/Application Support/com.mos.tcc/decisions.txt"
ok=0; total=0
check() { total=$((total + 1)); if eval "$2"; then ok=$((ok + 1)); else echo "FALHA: $1"; fi; }
seed() {
    mkdir -p "$(dirname "$F")"
    cat > "$F" << 'EOF'
com.a.app kTCCServiceCamera granted
com.b.app kTCCServiceCamera denied
com.a.app kTCCServiceMicrophone granted
com.a.app kTCCServiceAppleEvents+com.apple.finder granted
com.a.app kTCCServiceAppleEvents+com.apple.mail denied
com.b.app userNotifications granted
EOF
}
seed
out=$("$T/tccutil" reset Camera com.a.app)
check "reset por app" '[ "$out" = "Successfully reset Camera approval status for com.a.app" ] && ! grep -q "com.a.app kTCCServiceCamera" "$F" && grep -q "com.b.app kTCCServiceCamera denied" "$F"'
out=$("$T/tccutil" reset AppleEvents)
check "AppleEvents apaga os pares por alvo" '[ "$out" = "Successfully reset AppleEvents" ] && ! grep -q AppleEvents "$F" && grep -q Microphone "$F"'
out=$("$T/tccutil" reset Notifications)
check "notificações" '! grep -q userNotifications "$F"'
"$T/tccutil" reset Inventado 2> "$T/err"; rc=$?
check "serviço desconhecido" '[ $rc = 1 ] && grep -q "Unknown service: Inventado" "$T/err"'
"$T/tccutil" listar 2> "$T/err"; rc=$?
check "uso" '[ $rc = 1 ] && grep -q "Usage: tccutil reset SERVICE" "$T/err"'
seed
out=$("$T/tccutil" reset All com.b.app)
check "All de um app" '! grep -q com.b.app "$F" && [ $(grep -c com.a.app "$F") = 4 ]'
out=$("$T/tccutil" reset kTCCServiceMicrophone)
check "nome kTCCService inteiro" '! grep -q Microphone "$F"'
out=$("$T/tccutil" reset All)
check "All" '[ ! -s "$F" ]'
echo "tccutil: $ok/$total $([ $ok = $total ] && echo ok || echo FALHOU)"
[ $ok = $total ]
