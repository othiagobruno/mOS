#!/bin/bash
# Compila a libc++ e a libc++abi que vêm com o zig (LLVM, Apache-2.0 com exceção LLVM)
# como dylibs Mach-O: build/libcxx/libc++.1.dylib (x86_64) e build/libcxx-arm64 (arm64).
# Mesmas macros de tools/build-libcxx.ps1: ABI v1, namespace std::__1, filesystem ligado.
# No Linux também liga as mesmas fontes no host e roda tools/libcxx-host-test.cpp.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${ZIG:-${MOS_ZIG:-}}
if [ -z "$ZIG" ]; then
    ZIG=$(command -v zig || true)
fi
if [ -z "$ZIG" ] || [ ! -x "$ZIG" ]; then
    echo "build-libcxx: zig não encontrado" >&2
    exit 1
fi
ZIGDIR=$(dirname "$ZIG")
if [ ! -d "$ZIGDIR/lib/libcxx/src" ]; then
    ZIGDIR=$(dirname "$ZIGDIR")
fi
L="$ZIGDIR/lib/libcxx"
A="$ZIGDIR/lib/libcxxabi"
if [ ! -d "$L/src" ] || [ ! -d "$A/src" ]; then
    echo "build-libcxx: libcxx do zig ausente em $ZIGDIR" >&2
    exit 1
fi
JOBS=$(nproc 2>/dev/null || echo 4)

defs=(
    -D_LIBCPP_ABI_VERSION=1 -D_LIBCPP_ABI_NAMESPACE=__1 -D_LIBCPP_HAS_THREADS=1
    -D_LIBCPP_HAS_MONOTONIC_CLOCK=1 -D_LIBCPP_HAS_TERMINAL=1 -D_LIBCPP_HAS_MUSL_LIBC=0
    -D_LIBCPP_HAS_THREAD_API_PTHREAD=1 -D_LIBCPP_HAS_THREAD_API_EXTERNAL=0
    -D_LIBCPP_HAS_THREAD_API_WIN32=0 -D_LIBCPP_HAS_VENDOR_AVAILABILITY_ANNOTATIONS=0
    -D_LIBCPP_HAS_FILESYSTEM=1 -D_LIBCPP_HAS_RANDOM_DEVICE=1 -D_LIBCPP_HAS_LOCALIZATION=1
    -D_LIBCPP_HAS_UNICODE=1 -D_LIBCPP_HAS_WIDE_CHARACTERS=1 -D_LIBCPP_HAS_TIME_ZONE_DATABASE=0
    -D_LIBCPP_INSTRUMENTED_WITH_ASAN=0 -D_LIBCPP_PSTL_BACKEND_SERIAL=1
    -D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE
    -D_LIBCPP_ASSERTION_SEMANTIC_DEFAULT=_LIBCPP_ASSERTION_SEMANTIC_IGNORE
    -D_LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS
    -D_LIBCPP_BUILDING_LIBRARY -DLIBCXX_BUILDING_LIBCXXABI -D_LIBCXXABI_BUILDING_LIBRARY
)

collect() {
    local f b
    SRCS=()
    for f in "$L"/src/*.cpp "$L"/src/filesystem/*.cpp "$L"/src/ryu/*.cpp "$A"/src/*.cpp \
             "$L"/src/experimental/keep.cpp "$L"/src/experimental/log_hardening_failure.cpp; do
        [ -f "$f" ] || continue
        b=$(basename "$f")
        case "$b" in
            cxa_noexception.cpp|int128_builtins.cpp) continue ;;
            stdlib_new_delete.cpp)
                [ "${SKIP_NEW:-}" = 1 ] && continue
                ;;
        esac
        SRCS+=("$f")
    done
}

compile_arch() {
    local arch=$1 tgt=$2 out=$3
    local extra=()
    if [ "$arch" = macos ]; then
        extra=(-D_LIBCPP_DISABLE_NEW_DELETE_DEFINITIONS)
        SKIP_NEW=1
    else
        SKIP_NEW=0
    fi
    collect
    mkdir -p "$out"
    local running=0 s tag o
    for s in "${SRCS[@]}"; do
        tag=$(printf '%s' "${s#"$ZIGDIR/lib/"}" | tr '/' '_')
        o="$out/$tag.o"
        "$ZIG" c++ -target "$tgt" -std=c++23 -O2 -fPIC -fvisibility=default -nostdinc++ \
            -I"$L/include" -I"$A/include" -I"$L/src" -I"$L/libc" \
            "${defs[@]}" "${extra[@]}" -c "$s" -o "$o" 2>"$o.err" &
        running=$((running + 1))
        if [ "$running" -ge "$JOBS" ]; then
            wait
            running=0
        fi
    done
    wait
    local bad=0
    for s in "${SRCS[@]}"; do
        tag=$(printf '%s' "${s#"$ZIGDIR/lib/"}" | tr '/' '_')
        o="$out/$tag.o"
        if [ ! -s "$o" ]; then
            echo "build-libcxx: falhou $tag" >&2
            head -8 "$o.err" >&2 || true
            bad=1
        fi
    done
    [ "$bad" = 0 ]
    rm -f "$out"/*.err
}

echo "==> libc++.1.dylib x86_64"
compile_arch macos x86_64-macos.12.0 "$ROOT/build/libcxx"
"$ZIG" c++ -target x86_64-macos.12.0 -shared -nostdlib++ \
    -o "$ROOT/build/libcxx/libc++.1.dylib" "$ROOT/build/libcxx"/*.o \
    -Wl,-install_name,/usr/lib/libc++.1.dylib

echo "==> libc++.1.dylib arm64"
compile_arch macos aarch64-macos.12.0 "$ROOT/build/libcxx-arm64"
"$ZIG" c++ -target aarch64-macos.12.0 -shared -nostdlib++ \
    -o "$ROOT/build/libcxx-arm64/libc++.1.dylib" "$ROOT/build/libcxx-arm64"/*.o \
    -Wl,-install_name,/usr/lib/libc++.1.dylib

if [ "$(uname -s)" = Linux ]; then
    echo "==> teste de host da libc++"
    compile_arch host x86_64-linux-gnu "$ROOT/build/libcxx-host"
    "$ZIG" c++ -target x86_64-linux-gnu -std=c++23 -O2 -nostdinc++ -nostdlib++ \
        -I"$L/include" -I"$A/include" \
        "${defs[@]}" \
        "$ROOT/tools/libcxx-host-test.cpp" "$ROOT/build/libcxx-host"/*.o \
        -o "$ROOT/build/libcxx-host/libcxx-host-test" \
        -lc -lpthread -ldl -lgcc_s
    "$ROOT/build/libcxx-host/libcxx-host-test"
fi
echo "OK: libc++.1.dylib"
