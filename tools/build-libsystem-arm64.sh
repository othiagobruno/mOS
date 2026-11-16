#!/bin/bash
# mOS — libSystem para processos arm64 no Linux: build/libSystem-arm64.mos. Mesmo build do
# tools/build-libsystem-arm64.ps1 (triplo arm64-apple-macos11-elf, compiler-rt e libunwind do zig, ld.lld, símbolos
# fracos promovidos a GLOBAL, deslocamento do .eh_frame gravado no marcador cx_ls_ehframe). Ver docs/tradutor-arm64.md.
#   tools/build-libsystem-arm64.sh [jobs]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-$(command -v zig || true)}
[ -n "$ZIG" ] || { echo "zig: defina MOS_ZIG" >&2; exit 1; }
ZIGDIR=$(dirname "$(readlink -f "$ZIG")")
[ -f "$ZIGDIR/lib/compiler_rt.zig" ] || ZIGDIR=$("$ZIG" env | python3 -I -c 'import json,sys,os; print(os.path.dirname(json.load(sys.stdin)["lib_dir"]))')
INC=$ZIGDIR/lib/include
LS=$ROOT/userland/libsystem
TP=$ROOT/userland/third_party
OBJ=$ROOT/build/libsystem-arm64
OUT=$ROOT/build/libSystem-arm64.mos
JOBS=${1:-2}   # regras de RAM do time: no máximo 2 jobs
mkdir -p "$OBJ"
TRIPLE=arm64-apple-macos11-elf
# Os grupos e os -I são os do libSystem x86 em tools/build.sh: o math.h do kshim só entra na compressão (nos .S ele
# quebra o montador), o SQLite e o audiodec usam o shim de host-audio. -nostdlibinc: os cabeçalhos do Linux
# (/usr/include) nunca entram; no Windows eles nem existem.
BASE=(--target=$TRIPLE -mcpu=apple-m1 -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlibinc
    -fPIC -ffixed-x18 -fsigned-char -fno-common -funwind-tables
    -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header
    -Wno-unknown-warning-option -Wno-unused-function)
F_LS=(-I"$LS/kshim" -I"$LS/fsinc" -I"$ROOT/kernel/include" -isystem "$INC")
# SQLite e ALAC escolhem cabeçalhos da Apple por __APPLE__/TARGET_OS_MAC (o x86 compila com x86_64-freestanding-none, sem ele);
# a ABI continua a do triplo Darwin, só a macro sai
F_SHIM=(-U__APPLE__ -U__MACH__ -UTARGET_OS_MAC -UTARGET_OS_OSX -DTARGET_OS_MAC=0 -DTARGET_OS_OSX=0 -Wno-unused-variable -Wno-unreachable-code -I"$ROOT/userland/host-audio/shim" -I"$LS" -isystem "$INC")
F_COMP=(-Wno-unused-variable -DHAVE_CONFIG_H -DTUKLIB_SYMBOL_PREFIX=lzma_
    -I"$LS/kshim" -I"$LS/kshim/xz-config" -I"$LS" -I"$TP/zlib" -I"$TP/lz4" -I"$TP/lzfse"
    -I"$TP/xz" -I"$TP/xz/src/liblzma/api" -I"$TP/xz/src/liblzma/common" -I"$TP/xz/src/liblzma/check"
    -I"$TP/xz/src/liblzma/lz" -I"$TP/xz/src/liblzma/rangecoder" -I"$TP/xz/src/liblzma/lzma"
    -I"$TP/xz/src/liblzma/delta" -I"$TP/xz/src/liblzma/simple" -I"$TP/xz/src/common"
    -I"$TP/brotli/c/include" -include "$LS/kshim/math.h" -isystem "$INC")

# Fontes com a convenção Swift do x86_64 escrita à mão (self em r13, erro em r12, String em rax/rdx, registradores
# xmm): no processo arm64 esses tipos vêm das bibliotecas Swift arm64 de verdade (tools/build-swift-arm64.ps1), não
# do libSystem. Ficam fora até terem a versão arm64 (x20/x21/x8, docs/tradutor-arm64.md).
X86_SWIFTCC='swiftui_*.c combine_link.c combine_meta.c combine_meta.S'

SRC=(); GRP=()
add() { SRC+=("$2"); GRP+=("$1"); }
for f in "$LS"/*.c "$LS"/*.S; do
    b=$(basename "$f")
    case "$b" in objc_msg.S|objc_tramp.S|sigjmp.S|start.S|tlv.S) continue;; esac   # versões *_arm64.S
    skip=0; for p in $X86_SWIFTCC; do [[ $b == $p ]] && skip=1; done; [ $skip = 1 ] && continue
    if [ "$b" = sqlite3_mos.c ]; then add shim "$f"; else add ls "$f"; fi
done
add shim "$ROOT/userland/host-audio/audiodec.c"
add compls "$ROOT/userland/frameworks/libcompression/compression.c"   # MOS_LIBSYSTEM: usa o cz_raw_* de compression_z.c
for f in lz4.c lz4hc.c lz4frame.c xxhash.c; do add comp "$TP/lz4/$f"; done
for f in lzfse_decode.c lzfse_decode_base.c lzfse_encode.c lzfse_encode_base.c lzfse_fse.c lzvn_decode_base.c lzvn_encode_base.c; do add comp "$TP/lzfse/$f"; done
while IFS= read -r f; do
    case "$(basename "$f")" in crc_clmul_consts_gen.c|crc32_small.c|crc64_small.c|tuklib_exit.c|tuklib_mbstr_fw.c|tuklib_mbstr_nonprint.c|tuklib_mbstr_width.c|tuklib_mbstr_wrap.c|tuklib_open_stdxxx.c|tuklib_progname.c|stream_decoder_mt.c|stream_encoder_mt.c) continue;; esac
    add comp "$f"
done < <(find "$TP/xz/src/liblzma" "$TP/xz/src/common" -name '*.c' | sort)
for d in common dec enc; do for f in "$TP"/brotli/c/$d/*.c; do add comp "$f"; done; done

# os .o levam o nome do fonte; dois fontes com o mesmo nome se sobreporiam
dup=$(for f in "${SRC[@]}"; do basename "$f"; done | sort | uniq -d)
[ -z "$dup" ] || { echo "libSystem-arm64: nomes repetidos: $dup" >&2; exit 1; }

rm -f "$OBJ"/*.o "$OBJ"/*.log
export ZIG OBJ
# arrays não são exportados: vão como texto citado (printf %q) e voltam com eval
export Q_BASE=$(printf '%q ' "${BASE[@]}") Q_LS=$(printf '%q ' "${F_LS[@]}") Q_SHIM=$(printf '%q ' "${F_SHIM[@]}") Q_COMP=$(printf '%q ' "${F_COMP[@]}")
for i in "${!SRC[@]}"; do printf '%s\0%s\0' "${GRP[$i]}" "${SRC[$i]}"; done | xargs -0 -n 2 -P "$JOBS" bash -c '
    g=$1; f=$2; o="$OBJ/$(basename "$f").o"
    case $g in ls) ex=$Q_LS;; shim) ex=$Q_SHIM;; comp) ex=$Q_COMP;; compls) ex="$Q_COMP -DMOS_LIBSYSTEM";; esac
    eval "flags=($Q_BASE $ex)"
    "$ZIG" clang "${flags[@]}" -c "$f" -o "$o" 2>"$o.log" || { echo "--- $(basename "$f")"; grep -m3 "error" "$o.log"; rm -f "$o"; exit 1; }
    exit 0' _ || { echo "libSystem-arm64: erro de compilação" >&2; exit 1; }

# símbolos hidden -> default (compiler-rt) e fracos -> GLOBAL (o carregador só indexa GLOBAL)
symtab() {  # symtab <arquivo> hidden|weak
    python3 -I - "$1" "$2" <<'PY'
import struct, sys
p, mode = sys.argv[1], sys.argv[2]
b = bytearray(open(p, 'rb').read())
shoff, = struct.unpack_from('<Q', b, 40); shentsize, shnum = struct.unpack_from('<HH', b, 58)
n = 0
for i in range(shnum):
    sh = shoff + i * shentsize
    if struct.unpack_from('<I', b, sh + 4)[0] != 2: continue
    off, size = struct.unpack_from('<QQ', b, sh + 24); ent, = struct.unpack_from('<Q', b, sh + 56)
    for k in range(1, size // ent):
        s = off + k * ent
        if mode == 'hidden' and b[s + 5] & 3:
            b[s + 5] &= 0xFC; n += 1
        elif mode == 'weak' and (b[s + 4] >> 4) == 2 and struct.unpack_from('<H', b, s + 6)[0] != 0:
            b[s + 4] = (1 << 4) | (b[s + 4] & 15); n += 1
open(p, 'wb').write(b)
print(n)
PY
}

CRT=$OBJ/compiler_rt.o
ZV=$("$ZIG" version)
if [ ! -f "$CRT" ] || [ "$(cat "$OBJ/compiler_rt.zigver" 2>/dev/null)" != "$ZV" ]; then
    (cd "$OBJ" && "$ZIG" build-obj -target aarch64-macos.11.0 -mcpu apple_m1 -O ReleaseFast -fPIC -fno-stack-check \
        "$ZIGDIR/lib/compiler_rt.zig" -fno-emit-bin -femit-llvm-bc="$OBJ/compiler_rt.bc")
    "$ZIG" clang --target=$TRIPLE -mcpu=apple-m1 -O2 -fPIC -Wno-override-module -c "$OBJ/compiler_rt.bc" -o "$CRT"
    echo "compiler-rt arm64: $(symtab "$CRT" hidden) símbolo(s) hidden -> default"
    echo "$ZV" > "$OBJ/compiler_rt.zigver"
fi

UW=$ZIGDIR/lib/libunwind
UWFLAGS=(--target=$TRIPLE -mcpu=apple-m1 -O2 -g0 -fPIC -ffixed-x18 -fno-common -funwind-tables
    -D_LIBUNWIND_IS_NATIVE_ONLY -DNDEBUG -w -I"$UW/include" -isystem "$INC" -isystem "$ZIGDIR/lib/libc/include/any-darwin-any")
for f in libunwind.cpp UnwindLevel1.c UnwindLevel1-gcc-ext.c UnwindRegistersSave.S UnwindRegistersRestore.S; do
    case $f in
        *.cpp) x=(-std=c++17 -fno-exceptions -fno-rtti -nostdinc++);;
        *.S) x=(-U__APPLE__ -U__MACH__);;
        *) x=(-std=c11);;
    esac
    "$ZIG" clang "${UWFLAGS[@]}" "${x[@]}" -c "$UW/src/$f" -o "$OBJ/uw_$f.o"
done

OBJS=()
for f in "${SRC[@]}"; do OBJS+=("$OBJ/$(basename "$f").o"); done
OBJS+=("$CRT")
# o mesmo que o libSystem x86 liga (tools/build.sh): libm do musl (pow, atan2, ldexp, exp10...) e os codecs de mídia
# (cx_opus_*, cx_aac_* do audiodec.c), agora em arm64 com a ABI Darwin
bash "$ROOT/tools/build-libm-musl.sh" "$ZIG" aarch64 "$ROOT/build/libm-arm64" "$JOBS" || { echo "libSystem-arm64: libm falhou" >&2; exit 1; }
mapfile -t LIBM_OBJS < "$ROOT/build/libm-arm64/objs.txt"
bash "$ROOT/tools/build-midia.sh" "$ROOT/build" "$ZIG" "$ZIGDIR" "$JOBS" aarch64 || { echo "libSystem-arm64: mídia falhou" >&2; exit 1; }
mapfile -t MIDIA_OBJS < "$ROOT/build/midia-aarch64/objs.txt"
OBJS+=("${LIBM_OBJS[@]}" "${MIDIA_OBJS[@]}")
for f in libunwind.cpp UnwindLevel1.c UnwindLevel1-gcc-ext.c UnwindRegistersSave.S UnwindRegistersRestore.S; do OBJS+=("$OBJ/uw_$f.o"); done
printf '"%s"\n' "${OBJS[@]}" > "$OBJ/link.rsp"
"$ZIG" ld.lld -m aarch64elf -pie --no-dynamic-linker -z notext -e cx_start --no-gc-sections --hash-style=sysv \
    --pack-dyn-relocs=none -z max-page-size=0x4000 -z separate-loadable-segments "@$OBJ/link.rsp" -o "$OUT"
NWEAK=$(symtab "$OUT" weak)

# .eh_frame do blob -> cx_ls_ehframe[2..3] (deslocamento a partir do cabeçalho ELF e tamanho), achado pelo marcador
python3 -I - "$OUT" <<'PY'
import struct, sys
p = sys.argv[1]
b = bytearray(open(p, 'rb').read())
shoff, = struct.unpack_from('<Q', b, 40); shentsize, shnum, shstr = struct.unpack_from('<HHH', b, 58)
stroff, = struct.unpack_from('<Q', b, shoff + shstr * shentsize + 24)
eh_addr = eh_size = 0
for i in range(shnum):
    sh = shoff + i * shentsize
    no = stroff + struct.unpack_from('<I', b, sh)[0]
    name = bytes(b[no:b.index(0, no)])
    if name == b'.eh_frame':
        eh_addr, = struct.unpack_from('<Q', b, sh + 16); eh_size, = struct.unpack_from('<Q', b, sh + 32)
magic = struct.pack('<QQ', 0x4d4f5345485f4652, 0x4152344d34365f41)
hit = -1
for k in range(0, len(b) - 32, 8):
    if b[k:k + 16] == magic: hit = k; break
if hit < 0 or not eh_size:
    sys.exit('libSystem-arm64: .eh_frame ou marcador cx_ls_ehframe não encontrado')
struct.pack_into('<QQ', b, hit + 16, eh_addr, eh_size)
open(p, 'wb').write(b)
print('eh_frame: +0x%X (%d bytes)' % (eh_addr, eh_size))
PY

# formato que o carregador espera: só R_AARCH64_RELATIVE, nenhum símbolo indefinido
RO=$(command -v llvm-readobj || ls /usr/bin/llvm-readobj-* 2>/dev/null | head -1 || true)
NM=$(command -v llvm-nm || ls /usr/bin/llvm-nm-* 2>/dev/null | head -1 || true)
if [ -n "$RO" ] && [ -n "$NM" ]; then
    rels=$("$RO" -r "$OUT" | grep -o 'R_AARCH64_[A-Z0-9_]*' | sort | uniq -c | tr -s ' ' | tr '\n' ' ')
    und=$("$NM" -u "$OUT" | wc -l)
    echo "relocações: $rels símbolos fracos promovidos: $NWEAK  indefinidos: $und"
    [ "$und" -eq 0 ] || { "$NM" -u "$OUT"; exit 1; }
    "$RO" -r "$OUT" | grep -o 'R_AARCH64_[A-Z0-9_]*' | grep -qv '^R_AARCH64_RELATIVE$' && { echo "relocação fora de R_AARCH64_RELATIVE" >&2; exit 1; }
fi
echo "build/libSystem-arm64.mos: $(( $(stat -c %s "$OUT") / 1024 )) KiB"
