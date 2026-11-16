#!/bin/sh
# xattr no HFS+ com os drivers reais e o volimg check (verificador independente) nas duas imagens
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d)
S="kernel/fs/apfs.c kernel/fs/blkdev.c kernel/fs/bzip2.c kernel/fs/dfs.c kernel/fs/dmg.c kernel/fs/hfsplus.c kernel/fs/hfsplus_wr.c kernel/fs/inflate.c kernel/fs/lzfse.c kernel/fs/xz.c kernel/fs/fat.c"
gcc -O1 -D_FILE_OFFSET_BITS=64 tools/volimg.c -o "$D/volimg"
gcc -O1 -g -fsanitize=address,undefined -D_FILE_OFFSET_BITS=64 -DMOS_HOST_TEST -Ikernel/include -Itools tools/hfsxattr_test.c $S -o "$D/t"
"$D/volimg" mkdisk "$D/a.img" 64 hfsj:Mac:40 >/dev/null
"$D/volimg" mkdisk "$D/b.img" 64 hfs:Sem:40 >/dev/null
# segunda imagem: tira a árvore de atributos do cabeçalho do volume (como volumes antigos sem ela)
python3 -I - "$D/b.img" <<'PY'
import sys, struct
p = sys.argv[1]; d = bytearray(open(p, 'rb').read())
# GPT: 1ª partição em LBA da entrada 0 (cabeçalho em 512, entradas em 1024)
start = struct.unpack_from('<Q', d, 1024 + 32)[0] * 512
vh = start + 1024
bs = struct.unpack_from('>I', d, vh + 40)[0]
fork = vh + 352
blk = struct.unpack_from('>II', d, fork + 16)
# solta os blocos no bitmap (fork de alocação em vh+112)
astart = struct.unpack_from('>I', d, vh + 112 + 16)[0]
bm = start + astart * bs
for b in range(blk[0], blk[0] + blk[1]):
    d[bm + b // 8] &= ~(0x80 >> (b % 8)) & 0xff
free = struct.unpack_from('>I', d, vh + 48)[0] + blk[1]
struct.pack_into('>I', d, vh + 48, free)
d[fork:fork + 80] = b'\0' * 80
alt = start + (struct.unpack_from('>I', d, vh + 44)[0]) * bs - 1024
d[alt:alt + 512] = d[vh:vh + 512]
open(p, 'wb').write(d)
PY
ASAN_OPTIONS=detect_leaks=0 "$D/t" "$D/a.img" "$D/b.img" | grep -v "^dfs\|^hfs+\|^fat"
"$D/volimg" check "$D/a.img" | tail -1
"$D/volimg" check "$D/b.img" | tail -1
r=0
"$D/volimg" check "$D/a.img" >/dev/null && "$D/volimg" check "$D/b.img" >/dev/null || r=1
rm -rf "$D"
exit $r
