#!/bin/bash
# mOS — build no Linux (docs/staging-linux.md). Mesma ordem do build.ps1 para o que o
# boot precisa: libSystem, kernel UEFI, programas de userland, mkimg nativo e a imagem.
# O zig é o ELF do host (MOS_ZIG, PATH ou .tools/zig-*), nunca a primeira pasta zig-*.
# pwsh ausente: este script é o caminho. Com pwsh, build.ps1 chama a mesma escolha de zig.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build"
JOBS=$(nproc 2>/dev/null || echo 4)

while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT=$2; shift 2 ;;
        --jobs) JOBS=$2; shift 2 ;;
        *) echo "uso: tools/build.sh [--out DIR] [--jobs N]" >&2; exit 2 ;;
    esac
done
mkdir -p "$OUT"
cd "$ROOT"

kind_of() {
    local b
    b=$(od -An -t x1 -N 4 "$1" 2>/dev/null | tr -d ' \n')
    case "$b" in
        4d5a*|4D5A*) echo pe ;;
        7f454c46) echo elf ;;
        feedface|feedfacf|cefaedfe|cffaedfe|cafebabe|bebafeca) echo macho ;;
        *) echo other ;;
    esac
}

ver_of() {
    local v
    v=$("$1" version 2>/dev/null | head -1 || true)
    if printf '%s' "$v" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+'; then
        printf '%s\n' "$v" | head -1
    else
        echo 0.0.0
    fi
}

# maior versão semver (a.b.c)
newer() {
    printf '%s\n%s\n' "$1" "$2" | sort -t. -k1,1n -k2,2n -k3,3n | tail -1
}

pick_zig() {
    local want=elf best="" bestv="0.0.0" c k v
    case "$(uname -s)" in
        Linux) want=elf ;;
        Darwin) want=macho ;;
        MINGW*|MSYS*|CYGWIN*) want=pe ;;
        *) want=elf ;;
    esac
    consider() {
        [ -n "$1" ] && [ -f "$1" ] || return 0
        k=$(kind_of "$1")
        if [ "$k" != "$want" ]; then
            echo "aviso: $1 é $k; este host precisa de $want" >&2
            return 0
        fi
        v=$(ver_of "$1")
        if [ -z "$best" ] || [ "$(newer "$bestv" "$v")" = "$v" ]; then
            best=$1
            bestv=$v
        fi
    }
    if [ -n "${MOS_ZIG:-}" ]; then
        consider "$MOS_ZIG"
        if [ -n "$best" ]; then
            echo "$best"
            return 0
        fi
    fi
    if command -v zig >/dev/null 2>&1; then consider "$(command -v zig)"; fi
    if [ -d "$ROOT/.tools" ]; then
        local d name cand
        for d in "$ROOT"/.tools/zig-*; do
            [ -d "$d" ] || continue
            for name in zig.exe zig; do
                cand="$d/$name"
                [ -f "$cand" ] || continue
                consider "$cand"
            done
        done
    fi
    if [ -z "$best" ]; then
        echo "zig ELF não encontrado. MOS_ZIG, PATH ou .tools/zig-* (binário ELF, não a primeira pasta)." >&2
        return 1
    fi
    echo "zig: $best (elf, $bestv)" >&2
    echo "$best"
}

ZIG=$(pick_zig)
ZIGDIR=$(dirname "$ZIG")
if [ ! -d "$ZIGDIR/lib/libunwind" ]; then
    ZIGDIR=$(dirname "$ZIGDIR")
fi
UW="$ZIGDIR/lib/libunwind"
if [ -d "$ZIGDIR/lib/libc/include/any-darwin-any" ]; then
    SYSINC="$ZIGDIR/lib/libc/include/any-darwin-any"
elif [ -d "$ZIGDIR/lib/libc/include/any-macos-any" ]; then
    SYSINC="$ZIGDIR/lib/libc/include/any-macos-any"
else
    echo "libunwind: falta any-darwin-any e any-macos-any em $ZIGDIR" >&2
    exit 1
fi

echo "==> libunwind ($ZIG)"
UWO="$OUT/libunwind-x86"
mkdir -p "$UWO"
APPLE=(--target=x86_64-apple-macos11-elf)
FALL=(--target=x86_64-unknown-none-elf -D__APPLE__ -D__MACH__ -D__ELF__)
COMMON=(-O2 -g0 -fPIC -mno-red-zone -fno-stack-protector -funwind-tables -D_LIBUNWIND_IS_NATIVE_ONLY -DNDEBUG -w -I"$UW/include" -isystem "$ZIGDIR/lib/include" -isystem "$SYSINC")
probe=$("$ZIG" clang "${APPLE[@]}" "${COMMON[@]}" -std=c11 -c "$UW/src/UnwindLevel1.c" -o "$UWO/probe.o" 2>&1) && probe_ok=1 || probe_ok=0
CTARGET=("${APPLE[@]}")
CDEF=()
if [ "$probe_ok" != 1 ]; then
    printf '%s\n' "$probe" | tee "$OUT/libunwind-alvo-erro.txt" >&2
    echo "libunwind: x86_64-apple-macos11-elf falhou neste zig. C/C++ usam x86_64-unknown-none-elf com -D__APPLE__ -D__MACH__ -D__ELF__." >&2
    if ! printf '%s' "$probe" | grep -Eq 'data layout|UnknownOperatingSystem|wordsize.h'; then
        echo "libunwind: erro fora do contorno conhecido" >&2
        exit 1
    fi
    CTARGET=("${FALL[@]}")
    rm -f "$UWO/probe.o"
fi
compile_uw() {
    local f=$1 x=()
    case "$f" in
        *.cpp) x=(-std=c++17 -fno-exceptions -fno-rtti -nostdinc++) ;;
        *.S) x=(-U__APPLE__ -U__MACH__) ;;
        *) x=(-std=c11) ;;
    esac
    local tgt=("${CTARGET[@]}")
    if [ "${f##*.}" = S ]; then tgt=("${APPLE[@]}"); fi
    if ! "$ZIG" clang "${tgt[@]}" "${COMMON[@]}" "${x[@]}" -c "$UW/src/$f" -o "$UWO/$f.o" 2>"$UWO/$f.err"; then
        if [ "${f##*.}" = S ]; then
            "$ZIG" clang --target=x86_64-unknown-none-elf "${COMMON[@]}" "${x[@]}" -c "$UW/src/$f" -o "$UWO/$f.o"
        else
            cat "$UWO/$f.err" >&2
            return 1
        fi
    fi
}
for f in libunwind.cpp UnwindLevel1.c UnwindLevel1-gcc-ext.c UnwindRegistersSave.S UnwindRegistersRestore.S; do
    compile_uw "$f"
done

echo "==> libSystem.mos"
LS_SRC=()
for f in "$ROOT"/userland/libsystem/*.c "$ROOT"/userland/libsystem/*.S; do
    [ -f "$f" ] || continue
    # sqlite3.c pede assert.h/stdio.h. Compila com o shim, fora do -I do resto.
    case "$f" in
        */sqlite3_mos.c) continue ;;
    esac
    LS_SRC+=("$f")
done
# audiodec.c inclui stdlib/math do decodificador de terceiros. Fica fora do glob
# e entra com cabeçalhos mínimos, para o -I não trocar o stdint do resto do libSystem.
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
    -I"$ROOT/userland/host-audio/shim" \
    -c "$ROOT/userland/host-audio/audiodec.c" -o "$OUT/audiodec.o"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
    -Wno-unused-function -Wno-unused-variable -Wno-unreachable-code \
    -I"$ROOT/userland/host-audio/shim" -I"$ROOT/userland/libsystem" \
    -c "$ROOT/userland/libsystem/sqlite3_mos.c" -o "$OUT/sqlite3_mos.o"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie \
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
    -I"$ROOT/userland/libsystem/kshim" \
    -Wl,--image-base=0x400000100000 -Wl,-e,cx_start -Wl,--no-gc-sections \
    "${LS_SRC[@]}" "$OUT/audiodec.o" "$OUT/sqlite3_mos.o" "$UWO"/libunwind.cpp.o "$UWO"/UnwindLevel1-gcc-ext.c.o \
    "$UWO"/UnwindRegistersSave.S.o "$UWO"/UnwindRegistersRestore.S.o \
    -o "$OUT/libSystem.mos"
python3 "$ROOT/tools/patch-ehframe.py" "$OUT/libSystem.mos"
HASH=$(sha256sum "$OUT/libSystem.mos" | cut -c1-16)
BLOB="$ROOT/kernel/compat/libsystem_blob.S"
python3 - "$BLOB" "$HASH" <<'PY'
import pathlib, re, sys
p, h = pathlib.Path(sys.argv[1]), sys.argv[2]
t = p.read_text(encoding="utf-8", errors="surrogateescape")
line = "/* libSystem.mos sha256: %s */\n" % h
t = re.sub(r"(?m)^/\* libSystem\.mos sha256: [0-9A-Fa-f]+ \*/\n?", "", t)
if not t.startswith(line):
    p.write_text(line + t, encoding="utf-8")
PY
mkdir -p "$ROOT/build"
if [ "$(readlink -f "$OUT/libSystem.mos")" != "$(readlink -f "$ROOT/build/libSystem.mos")" ]; then
    cp -f "$OUT/libSystem.mos" "$ROOT/build/libSystem.mos"
fi

echo "==> kernel"
mapfile -t KSRC < <(find "$ROOT/kernel" -type f \( -name '*.c' -o -name '*.S' \) | sort)
"$ZIG" cc -target x86_64-uefi-msvc -O2 -g0 -ffreestanding -fno-stack-protector -fno-stack-check -mno-stack-arg-probe \
    -mno-red-zone -fshort-wchar -fno-sanitize=all -fno-strict-aliasing -nostdlib -I"$ROOT/kernel/include" -I"$ROOT" -std=gnu11 \
    -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
    -Wno-missing-field-initializers -Wno-unused-command-line-argument \
    "${KSRC[@]}" -o "$OUT/BOOTX64.EFI"

echo "==> mkimg (cc do host, sem .exe)"
cc -O2 "$ROOT/tools/mkimg.c" -o "$OUT/mkimg"

echo "==> programas de usuário"
ESP="$OUT/esp"
rm -rf "$ESP"
mkdir -p "$ESP/EFI/BOOT" "$ESP/mOS/Sistema/Programas" "$ESP/mOS/usr/bin"
cp -f "$OUT/BOOTX64.EFI" "$ESP/EFI/BOOT/BOOTX64.EFI"
cp -a "$ROOT/root/mOS/." "$ESP/mOS/"
rm -rf "$ESP/mOS/mOS"
if [ -d "$ESP/mOS/Users" ]; then
    if ! find "$ESP/mOS/Users" -type f | grep -q .; then
        rm -rf "$ESP/mOS/Users"
    fi
fi
for p in "$ROOT"/userland/*.c; do
    name=$(basename "$p" .c)
    if ! "$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -nostdlib -fno-stack-protector -fno-pic -mcmodel=large -static \
        -Wl,--image-base=0x400000000000 -Wl,-e,_start -Wno-unused-function -I"$ROOT/userland" -I"$ROOT/kernel/include" \
        "$p" -o "$ESP/mOS/Sistema/Programas/$name"; then
        echo "aviso: userland/$name.c não entrou na imagem" >&2
    fi
done
if [ -d "$ROOT/userland/cli-macos" ]; then
    for c in "$ROOT"/userland/cli-macos/*.c; do
        [ -f "$c" ] || continue
        name=$(basename "$c" .c)
        if ! "$ZIG" cc -target x86_64-macos -O2 "$c" -o "$ESP/mOS/usr/bin/$name" -Wl,-undefined,dynamic_lookup; then
            echo "aviso: /usr/bin/$name não compilou" >&2
        fi
    done
fi

echo "==> libswift"
if [ -f "$ROOT/build/universal-swift/usr/lib/swift/libswiftCore.dylib" ]; then
    cp -a "$ROOT/build/universal-swift/." "$ESP/mOS/"
elif [ -f "$ROOT/build/swift/objc/dylib/usr/lib/swift/libswiftCore.dylib" ]; then
    cp -a "$ROOT/build/swift/objc/dylib/." "$ESP/mOS/"
fi
sh "$ROOT/tools/stage-libswift.sh" "$ESP/mOS/usr/lib/swift"

KEY=${MOS_CHAVE_ATUALIZACAO:-$ROOT/tools/chave-atualizacao.semente}
REV=$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo local)
VER="0.1-$(date +%Y%m%d.%H%M)-$REV"
BYTES=$(find "$ESP" "$ROOT/root-live" -type f -printf '%s\n' 2>/dev/null | awk '{s+=$1} END {print s+0}')
MIB=$(( (BYTES * 5 / 4 + 1048575) / 1048576 ))
if [ "$MIB" -lt 128 ]; then MIB=128; fi
MIB=$(( (MIB + 15) / 16 * 16 ))
echo "==> imagem ${MIB} MiB"
rm -f "$OUT/mOS.img"
"$OUT/mkimg" "$OUT/mOS.img" "$ESP" "$MIB" --live-overlay "$ROOT/root-live" --key "$KEY" --manifest-out "$OUT/manifesto.txt" --versao "$VER"
echo "OK: $OUT/mOS.img"
ls -lh "$OUT/BOOTX64.EFI" "$OUT/mOS.img"
