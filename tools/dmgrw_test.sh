#!/bin/sh
# .dmg gravável no kernel: UDRW HFS+ e imagem crua FAT32 montadas pelo VFS, gravadas e relidas; o .dmg resultante
# passa no verify do hdiutil (CRC32 recalculados) e o volume, no verificador independente (volimg check)
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d)
S="kernel/fs/apfs.c kernel/fs/blkdev.c kernel/fs/bzip2.c kernel/fs/dfs.c kernel/fs/dmg.c kernel/fs/hfsplus.c kernel/fs/hfsplus_wr.c kernel/fs/inflate.c kernel/fs/lzfse.c kernel/fs/xz.c kernel/fs/fat.c kernel/fs/vfs_bridge.c kernel/lib/vfs.c kernel/lib/vfsmeta.c kernel/lib/vfsperm.c kernel/lib/vfsevents.c kernel/lib/vfsops.c kernel/proc/cred.c"
gcc -std=gnu11 -O1 -g -fsanitize=address,undefined -DUDIF_HOST -w -Iuserland/pkg userland/cli-macos/hdiutil.c -o "$D/hdiutil" -lz -llzma
gcc -O1 -g -fsanitize=address,undefined -D_FILE_OFFSET_BITS=64 -DMOS_HOST_TEST -Itools/vfsmeta_host -Ikernel/include -Itools tools/dmgrw_test.c $S -o "$D/t"
gcc -O1 -w -D_FILE_OFFSET_BITS=64 tools/volimg.c -o "$D/volimg"
export ASAN_OPTIONS=detect_leaks=0
H="$D/hdiutil"
$H create "$D/rw" -size 24m -fs JHFS+ -volname Grava -format UDRW -quiet
$H create "$D/z" -size 24m -fs HFS+ -volname Comprimida -format UDZO -quiet
$H create "$D/c" -size 40m -fs MS-DOS -volname CRUA -format UDTO -layout NONE -quiet
"$D/t" "$D/rw.dmg" "$D/z.dmg" "$D/c.cdr" "$D/out.dmg" "$D/out.cdr" | grep -v "^dfs\|^hfs+\|^fat\|^VFS\|^vol:\|^dmg:"
r=0
$H verify "$D/out.dmg" >/dev/null || { echo "FALHA: verify do .dmg gravado"; r=1; }
$H convert "$D/out.dmg" -format UDTO -o "$D/flat" -quiet
"$D/volimg" check "$D/flat.cdr" | tail -1
"$D/volimg" check "$D/flat.cdr" >/dev/null || r=1
"$D/volimg" check "$D/out.cdr" | tail -1
"$D/volimg" check "$D/out.cdr" >/dev/null || r=1
# corromper um byte de dados: o verify tem de recusar (prova que o CRC é conferido e foi recalculado)
cp "$D/out.dmg" "$D/bad.dmg"
printf '\377' | dd of="$D/bad.dmg" bs=1 seek=300000 conv=notrunc 2>/dev/null
if $H verify "$D/bad.dmg" >/dev/null 2>&1; then echo "FALHA: verify aceitou imagem corrompida"; r=1; fi
rm -rf "$D"
exit $r
