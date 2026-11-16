#!/bin/sh
# hdiutil do mOS no host: create em todos os formatos, verify, imageinfo, convert, checksum corrompido; as imagens
# lidas pelo leitor de .dmg do kernel e o volume conferido pelo volimg check
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d)
S="kernel/fs/apfs.c kernel/fs/blkdev.c kernel/fs/bzip2.c kernel/fs/dfs.c kernel/fs/dmg.c kernel/fs/hfsplus.c kernel/fs/hfsplus_wr.c kernel/fs/inflate.c kernel/fs/lzfse.c kernel/fs/xz.c kernel/fs/fat.c"
gcc -std=gnu11 -O1 -g -fsanitize=address,undefined -DUDIF_HOST -w -Iuserland/pkg userland/cli-macos/hdiutil.c -o "$D/hdiutil" -lz -llzma
gcc -O1 -w -D_FILE_OFFSET_BITS=64 -DMOS_HOST_TEST -Ikernel/include -Itools tools/hdiutil_kread.c $S -o "$D/kread"
gcc -O1 -w -D_FILE_OFFSET_BITS=64 tools/volimg.c -o "$D/volimg"
export ASAN_OPTIONS=detect_leaks=0
ok=0; bad=0
check() { if eval "$2"; then ok=$((ok+1)); else bad=$((bad+1)); echo "FALHA: $1"; fi; }
H="$D/hdiutil"
for fmt in UDRW UDRO UDZO ULFO ULMO; do
  for fs in HFS+ JHFS+ HFSX MS-DOS; do
    $H create "$D/i_${fmt}_$fs" -size 48m -fs "$fs" -volname "Vol $fmt" -format $fmt -ov -quiet
    check "verify $fmt $fs" "$H verify '$D/i_${fmt}_$fs.dmg' >/dev/null"
    out=$("$D/kread" "$D/i_${fmt}_$fs.dmg" 2>/dev/null | tail -1)
    case "$fs" in MS-DOS) want="FAT|VOL $fmt";; *) want="HFS+|Vol $fmt";; esac
    check "kernel lê $fmt $fs ($out)" "echo '$out' | grep -qi '^$want'"
  done
done
check "imageinfo UDZO" "$H imageinfo -format '$D/i_UDZO_HFS+.dmg' | grep -qx UDZO"
check "imageinfo ULMO" "$H imageinfo -format '$D/i_ULMO_HFS+.dmg' | grep -qx ULMO"
check "imageinfo partições GPT" "$H imageinfo '$D/i_UDZO_HFS+.dmg' | grep -q 'disk image (Apple_HFS : 4)'"
check "comprime" "[ \$(stat -c %s '$D/i_UDZO_HFS+.dmg') -lt 1000000 ]"
# convert UDZO -> ULMO -> UDTO (cru) e o volume pelo verificador independente
$H convert "$D/i_UDZO_JHFS+.dmg" -format ULMO -o "$D/c1" -quiet
check "convert ULMO verify" "$H verify '$D/c1.dmg' >/dev/null"
$H convert "$D/c1.dmg" -format UDTO -o "$D/c2" -quiet
check "UDTO é o disco cru com GPT" "$D/volimg check '$D/c2.cdr' | grep -q '0 erro(s), 0 aviso(s)'"
$H convert "$D/i_UDRW_MS-DOS.dmg" -format UDTO -o "$D/c3" -quiet
check "FAT32 cru limpo" "$D/volimg check '$D/c3.cdr' | grep -q '0 erro(s), 0 aviso(s)'"
check "kernel lê o cru" "$D/kread '$D/c2.cdr' 2>/dev/null | grep -q '^HFS+|Vol UDZO'"
# convert preserva os dados: UDTO de UDZO e de ULMO idênticos
$H convert "$D/i_UDZO_HFSX.dmg" -format UDTO -o "$D/a" -quiet
$H convert "$D/i_UDZO_HFSX.dmg" -format ULMO -o "$D/b" -quiet
$H convert "$D/b.dmg" -format UDTO -o "$D/b2" -quiet
check "ida e volta sem perda" "cmp -s '$D/a.cdr' '$D/b2.cdr'"
# checksum corrompido: verify acusa
cp "$D/i_UDZO_HFS+.dmg" "$D/x.dmg"
python3 -I -c "import sys; p=sys.argv[1]; d=bytearray(open(p,'rb').read()); d[100]^=0xff; open(p,'wb').write(d)" "$D/x.dmg"
check "verify pega o byte trocado" "! $H verify '$D/x.dmg' >/dev/null 2>&1"
check "existente sem -ov" "! $H create '$D/i_UDZO_HFS+' -size 48m -format UDZO -quiet 2>/dev/null"
check "fs desconhecido" "! $H create '$D/y' -size 48m -fs ZFS -quiet 2>/dev/null"
echo "hdiutil: $((ok+bad)) verificações, $bad falha(s)"
rm -rf "$D"
[ $bad -eq 0 ]
