#!/bin/sh
# libc++ e libc++abi do LLVM que vêm com o zig, como dylib Mach-O.
# O mesmo conjunto de tools/build-libcxx.ps1. Não é stub.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-zig}
ZIGDIR=$(CDPATH= cd -- "$(dirname "$ZIG")" && pwd)
if [ ! -d "$ZIGDIR/lib/libcxx/src" ]; then
  ZIGDIR=$(CDPATH= cd -- "$(dirname "$ZIG")/.." && pwd)
fi
if [ ! -d "$ZIGDIR/lib/libcxx/src" ]; then
  echo "libcxx do zig nao encontrada a partir de MOS_ZIG=$ZIG" >&2
  exit 1
fi
OUT=${1:-$ROOT/build/libcxx}
L="$ZIGDIR/lib/libcxx"
A="$ZIGDIR/lib/libcxxabi"
mkdir -p "$OUT/obj"
DEFS="-D_LIBCPP_ABI_VERSION=1 -D_LIBCPP_ABI_NAMESPACE=__1 -D_LIBCPP_HAS_THREADS=1 -D_LIBCPP_HAS_MONOTONIC_CLOCK=1 \
 -D_LIBCPP_HAS_TERMINAL=1 -D_LIBCPP_HAS_MUSL_LIBC=0 -D_LIBCPP_HAS_THREAD_API_PTHREAD=1 -D_LIBCPP_HAS_THREAD_API_EXTERNAL=0 \
 -D_LIBCPP_HAS_THREAD_API_WIN32=0 -D_LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS=0 -D_LIBCPP_HAS_FILESYSTEM=1 \
 -D_LIBCPP_HAS_RANDOM_DEVICE=1 -D_LIBCPP_HAS_LOCALIZATION=1 -D_LIBCPP_HAS_UNICODE=1 -D_LIBCPP_HAS_WIDE_CHARACTERS=1 \
 -D_LIBCPP_HAS_TIME_ZONE_DATABASE=0 -D_LIBCPP_INSTRUMENTED_WITH_ASAN=0 -D_LIBCPP_PSTL_BACKEND_SERIAL=1 \
 -D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE -D_LIBCPP_ASSERTION_SEMANTIC_DEFAULT=_LIBCPP_ASSERTION_SEMANTIC_IGNORE \
 -D_LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS -D_LIBCPP_BUILDING_LIBRARY -DLIBCXX_BUILDING_LIBCXXABI \
 -D_LIBCXXABI_BUILDING_LIBRARY -D_LIBCPP_DISABLE_NEW_DELETE_DEFINITIONS"
COMMON="-target x86_64-macos.12.0 -std=c++23 -O2 -fPIC -fvisibility=default -nostdinc++ -I$L/include -I$A/include -I$L/src -I$L/libc $DEFS"
OBJS=""
for s in "$L"/src/*.cpp "$L"/src/filesystem/*.cpp "$L"/src/ryu/*.cpp "$A"/src/*.cpp \
         "$L"/src/experimental/keep.cpp "$L"/src/experimental/log_hardening_failure.cpp; do
  base=$(basename "$s")
  case "$base" in
    cxa_noexception.cpp|stdlib_new_delete.cpp|int128_builtins.cpp) continue ;;
  esac
  tag=$(echo "$s" | sed "s|$ZIGDIR/lib/||; s|[\\/]|_|g")
  o="$OUT/obj/$tag.o"
  echo "cc $base"
  # shellcheck disable=SC2086
  "$ZIG" c++ $COMMON -c "$s" -o "$o"
  OBJS="$OBJS $o"
done
# shellcheck disable=SC2086
"$ZIG" c++ -target x86_64-macos.12.0 -shared -nostdlib++ -o "$OUT/libc++.1.dylib" $OBJS \
  -Wl,-install_name,/usr/lib/libc++.1.dylib
echo "$OUT/libc++.1.dylib"
