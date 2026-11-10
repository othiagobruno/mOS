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
gcc -O2 -Wall -Wextra -I kernel/include tools/cred_host.c kernel/proc/cred.c -o /tmp/cred_host
/tmp/cred_host

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

echo "==> libc++ (LLVM, dylib Mach-O)"
export ZIG
bash "$ROOT/tools/build-libcxx.sh"

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
# x86_64: _Unwind_* fica em cxx_unwind.c. UnwindLevel1.c repete esses nomes; o resto do libunwind fica.
LS_SRC=()
for f in "$ROOT"/userland/libsystem/*.c "$ROOT"/userland/libsystem/*.S "$ROOT"/userland/third_party/h264bsd/*.c; do
    [ -f "$f" ] || continue
    # sqlite3.c pede assert.h/stdio.h. Compila com o shim, fora do -I do resto.
    case "$f" in
        */sqlite3_mos.c) continue ;;
    esac
    LS_SRC+=("$f")
done
# audiodec.c e o SQLite usam o shim de host-audio, fora do -I do resto.
# Raise/Resume/GetIP, os seis _Unwind extras e __unw_getcontext ficam em cxx_unwind.c.
# Sem UnwindLevel1.c, sem UnwindLevel1-gcc-ext.c e sem UnwindRegistersSave.S.
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
    -I"$ROOT/userland/host-audio/shim" -I"$ROOT/userland/libsystem" \
    -c "$ROOT/userland/host-audio/audiodec.c" -o "$OUT/audiodec.o"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
    -Wno-unused-function -Wno-unused-variable -Wno-unreachable-code \
    -I"$ROOT/userland/host-audio/shim" -I"$ROOT/userland/libsystem" \
    -c "$ROOT/userland/libsystem/sqlite3_mos.c" -o "$OUT/sqlite3_mos.o"
# UnwindLevel1.c, UnwindLevel1-gcc-ext.c, UnwindRegistersSave.S e libunwind.cpp repetem cxx_unwind.c.
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie \
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
    -I"$ROOT/userland/libsystem/kshim" -I"$ROOT/userland/libsystem/fsinc" -I"$ROOT/kernel/include" -I"$ROOT/userland/third_party/h264bsd" \
    -isystem "$ZIGDIR/lib/include" \
    -Wl,--image-base=0x400000100000 -Wl,-e,cx_start -Wl,--no-gc-sections \
    "${LS_SRC[@]}" "$OUT/audiodec.o" "$OUT/sqlite3_mos.o" \
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

echo "==> a64rt (tradutor dos binários darwin-arm64)"
A64SRC=()
while IFS= read -r f; do A64SRC+=("$f"); done < <(find "$ROOT/userland/a64" "$ROOT/userland/a64rt" -type f \( -name '*.c' -o -name '*.S' \) | sort)
mkdir -p "$ESP/mOS/Sistema/Biblioteca/a64"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -nostdlib -static -no-pie -fpie \
    -mcx16 -Wall -Wno-unused-function -Wno-unused-parameter -fno-strict-aliasing \
    -DA64_RUNTIME_MOS -DA64_HAVE_JIT -I"$ROOT/userland/a64" -I"$ROOT/userland/a64rt" \
    -Wl,--image-base=0x400041000000 -Wl,-e,_start \
    "${A64SRC[@]}" -o "$ESP/mOS/Sistema/Biblioteca/a64/a64rt.mos"

echo "==> libswift"
# Runtime real (swift-6.4.2) quando a toolchain e o fonte estão em .tools/. Sem isso, o stage completa com stub.
if [ -f "$ROOT/tools/build-libswift-real.sh" ]; then
    sh "$ROOT/tools/build-libswift-real.sh" || echo "aviso: build-libswift-real.sh falhou; o que faltar continua stub" >&2
fi
if [ -f "$ROOT/build/universal-swift/usr/lib/swift/libswiftCore.dylib" ]; then
    cp -a "$ROOT/build/universal-swift/." "$ESP/mOS/"
elif [ -f "$ROOT/build/swift/objc/dylib/usr/lib/swift/libswiftCore.dylib" ]; then
    cp -a "$ROOT/build/swift/objc/dylib/." "$ESP/mOS/"
fi
sh "$ROOT/tools/stage-libswift.sh" "$ESP/mOS/usr/lib/swift"

echo "==> Virtualization.framework"
VZSRC="$ROOT/userland/frameworks/Virtualization/Virtualization.m"
VZOUT="$OUT/vz"
VZDIR="$ESP/mOS/System/Library/Frameworks/Virtualization.framework/Versions/A"
mkdir -p "$VZOUT" "$VZDIR"
vz_one() {
    "$ZIG" cc -target "$1" -O2 -shared -x objective-c -I"$ROOT/userland/sdk/include" -I"$ROOT/userland/frameworks/Virtualization" \
        -Wno-objc-root-class -Wno-objc-method-access "$VZSRC" -o "$2" \
        -Wl,-install_name,/System/Library/Frameworks/Virtualization.framework/Versions/A/Virtualization \
        -Wl,-undefined,dynamic_lookup
}
if vz_one x86_64-macos "$VZOUT/Virtualization.x86" && vz_one aarch64-macos "$VZOUT/Virtualization.arm"; then
    python3 - "$VZDIR/Virtualization" "$VZOUT/Virtualization.x86" "$VZOUT/Virtualization.arm" <<'PY'
import pathlib, struct, sys
out, *srcs = sys.argv[1:]
slices = []
for p in srcs:
    b = pathlib.Path(p).read_bytes()
    if struct.unpack_from("<I", b, 0)[0] != 0xFEEDFACF:
        raise SystemExit(p + " nao e Mach-O fino")
    cpu, sub = struct.unpack_from("<II", b, 4)
    align = 14 if cpu == 0x0100000C else 12
    slices.append((cpu, sub, align, b))
off = 8 + 20 * len(slices)
parts = []
for cpu, sub, align, b in slices:
    a = 1 << align
    if off % a:
        off += a - (off % a)
    parts.append((cpu, sub, align, off, b))
    off += len(b)
buf = bytearray()
def be(v):
    buf.extend(struct.pack(">I", v & 0xFFFFFFFF))
be(0xCAFEBABE); be(len(parts))
for cpu, sub, align, o, b in parts:
    be(cpu); be(sub); be(o); be(len(b)); be(align)
for cpu, sub, align, o, b in parts:
    if len(buf) < o:
        buf.extend(b"\0" * (o - len(buf)))
    buf.extend(b)
pathlib.Path(out).write_bytes(buf)
PY
else
    echo "aviso: Virtualization.framework não compilou" >&2
fi

echo "==> apps oficiais em /Aplicativos"
bash "$ROOT/tools/stage-apps-oficiais.sh" "$OUT"
mkdir -p "$ESP/mOS/usr/lib"
cp -f "$ROOT/build/libcxx/libc++.1.dylib" "$ESP/mOS/usr/lib/libc++.1.dylib"

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
