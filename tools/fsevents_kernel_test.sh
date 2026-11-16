#!/bin/sh
# FSEvents do mOS (modo journal) sobre o VFS e o journal reais do kernel, no host
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d)
F="-O1 -g -fsanitize=address,undefined"
for f in kernel/lib/vfs.c kernel/lib/vfsmeta.c kernel/lib/vfsperm.c kernel/lib/vfsevents.c kernel/proc/cred.c tools/fsevents_kernel_glue.c; do
    gcc $F -DMOS_HOST_TEST -Itools/vfsmeta_host -Ikernel/include -Itools -c "$f" -o "$D/$(basename "$f").o"
done
gcc $F -Wno-builtin-declaration-mismatch -Ikernel/include -Iuserland/libsystem -c tools/fsevents_kernel_test.c -o "$D/t.o"
gcc -fsanitize=address,undefined "$D"/*.o -o "$D/t"
ASAN_OPTIONS=detect_leaks=0 "$D/t"; r=$?
rm -rf "$D"
exit $r
