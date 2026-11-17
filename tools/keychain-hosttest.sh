#!/bin/sh
# Teste de host do Keychain (userland/libsystem/security.c): tools/keychain_hosttest.c. Imprime "keychain: N/N ok"
# três vezes: a execução principal, outra que relê o arquivo cifrado gravado por ela e uma com um arquivo da versão 1.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
CC=${CC:-cc}
T=$(mktemp -d "${TMPDIR:-/tmp}/keychain-teste.XXXXXX")
trap 'rm -rf "$T"' EXIT
$CC -O1 -w -fno-builtin -I"$ROOT/userland/libsystem" "$ROOT/tools/keychain_hosttest.c" "$ROOT/tools/keychain_hostshim.c" -o "$T/kc"
mkdir -p "$T/home" "$T/antigo/Library/Keychains"
HOME=$T/home "$T/kc"
HOME=$T/home "$T/kc" reler
HOME=$T/antigo "$T/kc" antigo
