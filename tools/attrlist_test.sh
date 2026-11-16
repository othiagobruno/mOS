#!/bin/sh
# getattrlist/setattrlist/bulk/statfs/renamex/clonefile do libSystem sobre o VFS real do kernel, no host
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d)
F="-O1 -g -fsanitize=address,undefined"
for f in kernel/lib/vfs.c kernel/lib/vfsmeta.c kernel/lib/vfsperm.c kernel/lib/vfsevents.c kernel/lib/vfsops.c kernel/proc/cred.c tools/attrlist_kglue.c; do
    gcc $F -DMOS_HOST_TEST -Itools/vfsmeta_host -Ikernel/include -Itools -c "$f" -o "$D/k_$(basename "$f").o" 2>/dev/null
done
mkdir "$D/u"
cp userland/libsystem/attrlist.c userland/libsystem/fsmeta.c userland/libsystem/bookmark.c userland/libsystem/quarantine.c tools/libsys_host/sys.h tools/libsys_host/libc.h "$D/u/"
for f in attrlist.c fsmeta.c bookmark.c quarantine.c; do gcc $F -w -DCX_HOST_TEST -Dopen=t_open -Dread=t_read -Dwrite=t_write -Dclose=t_close -Dtime=t_time -Darc4random_buf=t_rand -I"$D/u" -Ikernel/include -c "$D/u/$f" -o "$D/u_$f.o"; done
gcc $F -w -I"$D/u" -Ikernel/include -c tools/attrlist_test.c -o "$D/t.o"
gcc -fsanitize=address,undefined "$D"/*.o -o "$D/t"
ASAN_OPTIONS=detect_leaks=0 "$D/t" | grep -v "^VFS:"; r=$?
rm -rf "$D"
exit $r
