#!/bin/sh
# DiskArbitration do libSystem no host (CF e volumes falsos; a lógica é a do diskarb.c)
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d)
cp userland/libsystem/diskarb.c tools/libsys_host/sys.h tools/libsys_host/libc.h "$D"/
gcc -O1 -g -fsanitize=address,undefined -w -DCX_HOST_TEST -I"$D" -Ikernel/include -c "$D/diskarb.c" -o "$D/d.o"
gcc -O1 -g -fsanitize=address,undefined -w -c tools/diskarb_test.c -o "$D/t.o"
gcc -fsanitize=address,undefined "$D/d.o" "$D/t.o" -o "$D/t"
ASAN_OPTIONS=detect_leaks=0 "$D/t"; r=$?
rm -rf "$D"
exit $r
