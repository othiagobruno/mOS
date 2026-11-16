#!/bin/bash
# Teste de host do mdfind(1), mdls(1) e mdutil(1) (userland/cli-macos). O mdutil roda como está (POSIX); o mdfind
# e o mdls rodam sobre tools/md_cli_shim.c (CF/MD mínimos com o predicado de verdade, mdq_core.c).
#   bash tools/mdclitest.sh        -> "mdcli: N/N ok"
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
CF="-std=gnu11 -O1 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -I$ROOT/userland/libsystem"
gcc $CF "$ROOT/userland/cli-macos/mdutil.c" -o "$T/mdutil" || exit 1
gcc $CF "$ROOT/userland/cli-macos/mdfind.c" "$ROOT/tools/md_cli_shim.c" "$ROOT/userland/libsystem/mdq_core.c" -o "$T/mdfind" || exit 1
gcc $CF "$ROOT/userland/cli-macos/mdls.c" "$ROOT/tools/md_cli_shim.c" "$ROOT/userland/libsystem/mdq_core.c" -o "$T/mdls" || exit 1
ok=0; total=0
check() { total=$((total + 1)); if eval "$2"; then ok=$((ok + 1)); else echo "FALHA: $1"; fi; }
H="$T/casa"; mkdir -p "$H/Documentos/sub" "$H/Fotos"
printf 'relatorio' > "$H/Documentos/Relatório Final.txt"; printf 'x' > "$H/Documentos/sub/notas.md"
head -c 5000 /dev/zero > "$H/Fotos/grande.bin"; touch -d '2020-01-02 03:04:05 UTC' "$H/Fotos/grande.bin"
export MDCLI_HOME="$H"
out=$("$T/mdfind" -name relat)
check "-name sem caixa e sem acento" '[ "$out" = "$H/Documentos/Relatório Final.txt" ]'
out=$("$T/mdfind" 'kMDItemFSSize > 1000')
check "consulta numérica" '[ "$out" = "$H/Fotos/grande.bin" ]'
out=$("$T/mdfind" -count 'kMDItemContentTypeTree == "public.text"')
check "-count" '[ "$out" = 6 ]'
out=$("$T/mdfind" -onlyin "$H/Documentos" -name '.')
check "-onlyin" '[ $(printf "%s\n" "$out" | grep -c Fotos) = 0 ] && printf "%s\n" "$out" | grep -q notas.md'
out=$("$T/mdfind" -0 -name notas | tr "\0" "#")
check "-0 separa por NUL" '[ "$out" = "$H/Documentos/sub/notas.md#" ]'
out=$("$T/mdfind" 'kMDItemContentModificationDate < $time.iso(2021-01-01T00:00:00Z)')
check "\$time.iso no mdfind" '[ "$out" = "$H/Fotos/grande.bin" ]'
"$T/mdfind" 'kMDItemFSName ==' 2> "$T/err"; rc=$?
check "consulta inválida" '[ $rc = 1 ] && grep -q "consulta inválida" "$T/err"'
out=$("$T/mdls" "$H/Fotos/grande.bin")
check "mdls alinha os nomes" 'printf "%s\n" "$out" | grep -q "^kMDItemFSSize                  = 5000$"'
check "mdls data" 'printf "%s\n" "$out" | grep -q "^kMDItemContentModificationDate = 2020-01-02 03:04:05 +0000$"'
check "mdls lista" 'printf "%s\n" "$out" | grep -q "^    \"public.text\",$" && printf "%s\n" "$out" | grep -q "^kMDItemContentTypeTree         = ($"'
out=$(cd "$H/Fotos" && "$T/mdls" -name kMDItemFSName -name kMDItemFSSize grande.bin)
check "mdls -name e caminho relativo" '[ "$out" = "$(printf "kMDItemFSName = \"grande.bin\"\nkMDItemFSSize = 5000")" ]'
out=$("$T/mdls" -raw -name kMDItemFSName -name kMDItemXYZ -nullMarker NADA "$H/Fotos/grande.bin" | tr "\0" "#")
check "mdls -raw -nullMarker" '[ "$out" = "grande.bin#NADA#" ]'
"$T/mdls" "$H/naoexiste" 2> "$T/err"; rc=$?
check "mdls arquivo inexistente" '[ $rc = 1 ] && grep -q "could not find" "$T/err"'
V="$T/vol"; mkdir -p "$V"
out=$("$T/mdutil" -s "$V")
check "mdutil -s" '[ "$out" = "$(printf "%s:\n\tIndexing enabled." "$V")" ]'
out=$("$T/mdutil" -i off "$V")
check "mdutil -i off" '[ -e "$V/.metadata_never_index" ] && printf "%s" "$out" | grep -q "Indexing disabled."'
out=$("$T/mdutil" -i on "$V")
check "mdutil -i on" '[ ! -e "$V/.metadata_never_index" ] && printf "%s" "$out" | grep -q "Indexing enabled."'
"$T/mdutil" -s "$T/nada" 2> "$T/err"; rc=$?
check "mdutil volume desconhecido" '[ $rc = 1 ] && grep -q "unknown volume" "$T/err"'
"$T/mdutil" -X 2> "$T/err"; rc=$?
check "mdutil uso" '[ $rc = 1 ] && grep -q Usage "$T/err"'
echo "mdcli: $ok/$total $([ $ok = $total ] && echo ok || echo FALHOU)"
[ $ok = $total ]
