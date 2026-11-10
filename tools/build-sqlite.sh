#!/bin/sh
# mOS — SQLite (amálgama oficial, domínio público) como a libsqlite3 do macOS: build/sqlite/libsqlite3.dylib
# (Mach-O x86_64, install name /usr/lib/libsqlite3.dylib), que o tools/build.sh põe na imagem.
# O mesmo de tools/build-sqlite.ps1: mesmas opções (FTS3/FTS5, R*Tree, metadados de coluna, threadsafe, sem
# extensões carregáveis) e as versões de compatibilidade da libsqlite3 da Apple (9.0.0 / 357.0.0).
# Fonte: https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip (SQLite 3.50.4). O caminho com o ano é o
# endereço permanente do sqlite.org para cada versão. SHA-256 medido aqui (wget, 10/11/2026):
#   1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032
# O .ps1 usa o amálgama que estiver em .tools/sqlite-dl; este baixa esse mesmo diretório.
#   MOS_ZIG=/caminho/do/zig sh tools/build-sqlite.sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
VER=3500400
SHA=1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032
DL=$ROOT/.tools/sqlite-dl
ZIP=$DL/sqlite-amalgamation-$VER.zip
SRC=$DL/sqlite-amalgamation-$VER
OUT=$ROOT/build/sqlite
DY=$OUT/libsqlite3.dylib
mkdir -p "$DL" "$OUT"
. "$ROOT/tools/baixar.sh"
baixar "https://www.sqlite.org/2025/sqlite-amalgamation-$VER.zip" "$ZIP" "$SHA" || exit 1
if [ ! -f "$SRC/sqlite3.c" ]; then
    # o zip traz uma pasta sqlite-amalgamation-<ver>/ com sqlite3.c, sqlite3.h, sqlite3ext.h e shell.c
    python3 -I -c 'import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    for n in z.namelist():
        if n.startswith("/") or ".." in n.split("/"):
            sys.exit("sqlite: caminho suspeito no zip: " + n)
    z.extractall(sys.argv[2])' "$ZIP" "$DL"
fi
if [ ! -f "$SRC/sqlite3.c" ]; then
    echo "sqlite: falta $SRC/sqlite3.c depois de extrair" >&2
    exit 1
fi
if [ -f "$DY" ] && [ "$DY" -nt "$SRC/sqlite3.c" ] && [ "$DY" -nt "$0" ]; then
    echo "OK: $DY (cache)"
    exit 0
fi
"$ZIG" cc -target x86_64-macos.11.0 -O2 -g0 -fPIC -shared \
    -DSQLITE_THREADSAFE=2 -DSQLITE_ENABLE_FTS3 -DSQLITE_ENABLE_FTS3_PARENTHESIS -DSQLITE_ENABLE_FTS4 \
    -DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_RTREE -DSQLITE_ENABLE_COLUMN_METADATA -DSQLITE_ENABLE_DBSTAT_VTAB \
    -DSQLITE_ENABLE_PREUPDATE_HOOK -DSQLITE_ENABLE_SESSION -DSQLITE_ENABLE_UNLOCK_NOTIFY \
    -DSQLITE_ENABLE_API_ARMOR -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1 \
    -DHAVE_USLEEP=1 -DSQLITE_MAX_MMAP_SIZE=0 \
    "$SRC/sqlite3.c" -o "$DY" \
    -Wl,-install_name,/usr/lib/libsqlite3.dylib -Wl,-compatibility_version,9.0.0 -Wl,-current_version,357.0.0
echo "OK: $DY ($(( $(wc -c < "$DY") / 1024 )) KiB, sqlite $VER)"
