#!/bin/sh
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d)
S="kernel/fs/apfs.c kernel/fs/blkdev.c kernel/fs/bzip2.c kernel/fs/dfs.c kernel/fs/dmg.c kernel/fs/hfsplus.c kernel/fs/hfsplus_wr.c kernel/fs/inflate.c kernel/fs/lzfse.c kernel/fs/xz.c kernel/fs/fat.c kernel/fs/vfs_bridge.c kernel/lib/vfs.c kernel/lib/vfsmeta.c kernel/lib/vfsperm.c kernel/lib/vfsevents.c kernel/lib/vfsops.c kernel/proc/cred.c"
gcc -O1 -D_FILE_OFFSET_BITS=64 tools/volimg.c -o "$D/volimg"
gcc -O1 -g -fsanitize=address,undefined -D_FILE_OFFSET_BITS=64 -DMOS_HOST_TEST -Itools/vfsmeta_host -Ikernel/include -Itools tools/volxattr_test.c $S -o "$D/t"
"$D/volimg" mkdisk "$D/a.img" 64 hfs:Mac:30 fat32:F:30 >/dev/null
ASAN_OPTIONS=detect_leaks=0 "$D/t" "$D/a.img" | grep -v "^dfs\|^hfs+\|^fat\|^VFS\|^vol:"
"$D/volimg" check "$D/a.img" | tail -1
"$D/volimg" check "$D/a.img" >/dev/null; r=$?
rm -rf "$D"
exit $r
