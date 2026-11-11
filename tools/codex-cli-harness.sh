#!/bin/bash
# Codex CLI oficial (0.167.0, x86_64) no harness tools/machotest.c, sem VM e sem conta.
#   tools/codex-cli-harness.sh <libSystem.mos> [codex-x86_64-apple-darwin]
# Sem o segundo argumento, baixa o tar.gz da release rust-v0.167.0 e confere o SHA-256 (o "digest" do GitHub).
# Confere, em ordem:
#   1. codex --version, sem o aviso "could not create PATH aliases" (~/.codex/tmp/arg0 e os links apply_patch);
#   2. apply_patch de ponta a ponta (--codex-run-as-apply-patch): edita um arquivo e cria outro;
#   3. login --with-api-key com uma chave falsa pelo stdin, e login status lendo ~/.codex/auth.json;
#   4. a segunda partida limpa os diretórios arg0 da primeira sem "os error 66" (readdir com retrato).
# Rede, login com ChatGPT, codex exec e o sandbox-exec ficam para a VM: o harness não tem SYS_NET nem processo filho.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LIB=${1:?uso: tools/codex-cli-harness.sh <libSystem.mos> [codex]}
WORK=${CODEX_HARNESS_DIR:-$ROOT/build/codex-harness}
mkdir -p "$WORK"
BIN=${2:-}
if [ -z "$BIN" ]; then
    TGZ=$WORK/codex-x86_64-apple-darwin.tar.gz
    WANT=d8eb4a865a0b17319cde10347523e5bab0978d01725b39f4a639f40905e01221
    if [ ! -f "$TGZ" ] || [ "$(sha256sum "$TGZ" | cut -d' ' -f1)" != "$WANT" ]; then
        curl -fsSL -o "$TGZ.part" https://github.com/openai/codex/releases/download/rust-v0.167.0/codex-x86_64-apple-darwin.tar.gz
        mv "$TGZ.part" "$TGZ"
    fi
    [ "$(sha256sum "$TGZ" | cut -d' ' -f1)" = "$WANT" ] || { echo "codex: SHA-256 diferente do da release" >&2; exit 1; }
    tar -xzf "$TGZ" -C "$WORK"
    BIN=$WORK/codex-x86_64-apple-darwin
fi

ZIG=${MOS_ZIG:-$(command -v zig || true)}
[ -n "$ZIG" ] || { echo "zig: defina MOS_ZIG" >&2; exit 1; }
MT=$WORK/machotest
"$ZIG" cc -target x86_64-linux-gnu -O2 -std=gnu11 -DMOS_HOST_TEST -fno-strict-aliasing \
    -I"$ROOT/kernel/include" -I"$ROOT/tools" "$ROOT/tools/machotest.c" "$ROOT/tools/host_unix.c" "$ROOT/tools/host_gpu.c" \
    "$ROOT/tools/host_qos.c" "$ROOT/tools/host_id.c" "$ROOT/tools/host_bootstrap.c" "$ROOT/tools/host_enter.S" \
    "$ROOT/kernel/compat/macho.c" -o "$MT" -pthread

R=$WORK/raiz
rm -rf "$R"
mkdir -p "$R/bin" "$R/Users/Visitante" "$R/tmp"
cp "$BIN" "$R/bin/codex"
run() { MACHOTEST_PATH=/bin/codex timeout 300 "$MT" "$LIB" "$R" "$R/bin/codex" "$@" 2>&1; }
falhou=0
ok() { echo "ok   $1"; }
ruim() { echo "FALHA $1"; falhou=1; }

out=$(run --version)
echo "$out" | grep -q '^codex-cli 0.167.0$' && ok "codex --version" || ruim "codex --version: $out"
echo "$out" | grep -q 'could not create PATH aliases' && ruim "aliases do arg0: $(echo "$out" | grep WARNING)" || ok "aliases apply_patch em ~/.codex/tmp/arg0"
ls "$R"/Users/Visitante/.codex/tmp/arg0-macos/codex-arg0*/apply_patch >/dev/null 2>&1 && ok "link apply_patch gravado" || ruim "sem link apply_patch"

printf 'linha um\nlinha dois\n' > "$R/Users/Visitante/nota.txt"
out=$(run --codex-run-as-apply-patch "*** Begin Patch
*** Update File: nota.txt
@@
 linha um
-linha dois
+linha dois, corrigida
*** Add File: novo.txt
+criado pelo apply_patch
*** End Patch")
echo "$out" | grep -q '^Success. Updated the following files:' && ok "apply_patch respondeu Success" || ruim "apply_patch: $out"
[ "$(cat "$R/Users/Visitante/nota.txt")" = "$(printf 'linha um\nlinha dois, corrigida')" ] && ok "nota.txt editado" || ruim "nota.txt: $(cat "$R/Users/Visitante/nota.txt")"
[ "$(cat "$R/Users/Visitante/novo.txt" 2>/dev/null)" = "criado pelo apply_patch" ] && ok "novo.txt criado" || ruim "novo.txt ausente"
echo "$out" | grep -q 'os error 66' && ruim "limpeza do arg0: $(echo "$out" | grep WARNING)" || ok "segunda partida limpou o arg0 da primeira"

out=$(run login status || true)
echo "$out" | grep -q '^Not logged in$' && ok "login status sem credencial" || ruim "login status: $out"
out=$(printf 'sk-mos-harness-falsa-0000\n' | MACHOTEST_STDIN=1 run login --with-api-key || true)
echo "$out" | grep -q '^API key saved' && ok "login --with-api-key" || ruim "login --with-api-key: $out"
grep -q '"OPENAI_API_KEY"' "$R/Users/Visitante/.codex/auth.json" 2>/dev/null && ok "~/.codex/auth.json gravado" || ruim "sem auth.json"
out=$(run login status || true)
echo "$out" | grep -q '^API key configured - sk-mos-h\*\*\*-0000$' && ok "login status lê a chave" || ruim "login status: $out"

# cada partida apaga os diretórios arg0 das anteriores (apply_patch, applypatch e .lock, com remove_dir_all);
# com o readdir por índice ao vivo, a ordem da pasta decide se o .lock é pulado e o rmdir dá ENOTEMPTY
erros=0
for i in 1 2 3 4 5 6; do
    out=$(run --version)
    echo "$out" | grep -q 'os error 66' && erros=$((erros + 1))
done
[ $erros = 0 ] && ok "seis partidas sem 'os error 66'" || ruim "limpeza do arg0 falhou em $erros de 6 partidas"
n=$(ls -d "$R"/Users/Visitante/.codex/tmp/arg0-macos/codex-arg0* 2>/dev/null | wc -l)
[ "$n" -le 2 ] && ok "diretórios arg0 vivos: $n" || ruim "diretórios arg0 acumulados: $n"

[ $falhou = 0 ] && echo "codex-cli-harness: tudo ok" || { echo "codex-cli-harness: houve falha"; exit 1; }
