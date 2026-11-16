#!/bin/sh
# teste de host da ACL do libSystem: compila userland/libsystem/acl.c ao lado dos shims de tools/acl_host
set -e
cd "$(dirname "$0")/.."
D=$(mktemp -d)
cp userland/libsystem/acl.c tools/acl_host/sys.h tools/acl_host/libc.h "$D"/
gcc -O1 -g -fsanitize=address,undefined -I"$D" -Ikernel/include tools/acl_test.c "$D/acl.c" -o "$D/t"
"$D/t"; r=$?
rm -rf "$D"
exit $r
