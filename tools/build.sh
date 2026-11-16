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
gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/include -I tools tools/resto_host.c \
    kernel/proc/cred.c kernel/proc/fdshare.c kernel/proc/fileport.c kernel/mm/commpage.c kernel/mm/swap.c \
    kernel/mm/vmcomp.c -o /tmp/resto_host
/tmp/resto_host
# compressor de memória e swap comprimido (docs/nucleo-plano.md)
gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/include -I tools tools/vmcomp_host.c kernel/mm/vmcomp.c kernel/mm/swap.c \
    -o /tmp/vmcomp_host
# filas de futex e prioridade do escalonador (docs/nucleo-plano.md)
gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/include -I tools tools/futexq_host.c kernel/proc/futexq.c -o /tmp/futexq_host
gcc -O2 -Wall -Wextra -I kernel/include tools/vmpolicy_host.c -o /tmp/vmpolicy_host
/tmp/vmpolicy_host
/tmp/futexq_host
/tmp/vmcomp_host
# o que o ChatGPT desktop pedia (Spotlight, chave P-256 em PEM, inversa de CATransform3D)
gcc -std=c11 -Wall -Wextra -I userland/libsystem -DCK_PEM_HOST userland/libsystem/mdq_core.c userland/libsystem/ck_pem.c \
    userland/libsystem/catransform_inv.c tools/agentes_host.c -o /tmp/agentes_host
/tmp/agentes_host > /tmp/agentes_host.log || { cat /tmp/agentes_host.log; exit 1; }
tail -1 /tmp/agentes_host.log
gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/include -I tools tools/r8169_host.c kernel/dev/r8169.c \
    -o /tmp/r8169_host
/tmp/r8169_host
# RTL8821CE: tabelas da Realtek contra o estado real do chip, energia, eFuse e firmware (docs/rede-hardware.md)
gcc -O1 -Wall -Wextra -DMOS_HOST_TEST -I tools -I kernel/include -I kernel/wifi/rtw88 tools/rtw88_host.c \
    kernel/wifi/rtw88/rtw_core.c kernel/wifi/rtw88/rtw_pwr.c kernel/wifi/rtw88/rtw8821c_table.c -o /tmp/rtw88_host
/tmp/rtw88_host firmware/rtw88/rtw8821c_fw.bin tools/rtw88-fixture/bb_8821ce_linux.txt | tail -1
gcc -O1 -Wall -Wextra -DMOS_HOST_TEST -I tools -I kernel/include -I kernel/wifi/rtw88 tools/rtw88_chip_host.c \
    kernel/wifi/rtw88/rtw8821c.c kernel/wifi/rtw88/rtw_core.c kernel/wifi/rtw88/rtw_pwr.c \
    kernel/wifi/rtw88/rtw8821c_table.c -o /tmp/rtw88_chip
/tmp/rtw88_chip firmware/rtw88/rtw8821c_fw.bin 2>/dev/null | tail -1
# Bluetooth: servidor SDP e AVRCP (botões do fone)
gcc -O1 -Wall -Wextra -DMOS_HOST_TEST -I tools -I kernel/include tools/avrcp_host.c kernel/bt/avrcp.c -o /tmp/avrcp_host
/tmp/avrcp_host | tail -1
# WPA3-SAE: vetores do IEEE 802.11-2020 anexo J.10 (caçar-e-bicar e H2E)
gcc -O1 -Wall -Wextra -DMOS_HOST_TEST -I tools -I kernel/include tools/sae_host.c kernel/wifi/sae.c \
    kernel/bt/p256.c kernel/net/crypto.c kernel/bt/smp_crypto.c kernel/wifi/wpa.c -o /tmp/sae_host
/tmp/sae_host 2>/dev/null | tail -1
# GPU AMD (Raven/Picasso/Raven2): EDID real do monitor do Thiago e cabeçalhos do firmware/amdgpu
gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu tools/amdgpu_host.c kernel/dev/amdgpu/edid.c \
    kernel/dev/amdgpu/ucode.c -o /tmp/amdgpu_host
/tmp/amdgpu_host > /tmp/amdgpu_host.log || { cat /tmp/amdgpu_host.log; exit 1; }
tail -1 /tmp/amdgpu_host.log
# GPU AMD: validador dos IBs PM4 do userland (ABI revisão 3)
gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu tools/amdgpu_pm4_host.c kernel/dev/amdgpu/pm4check.c -o /tmp/amdgpu_pm4_host
/tmp/amdgpu_pm4_host > /tmp/amdgpu_pm4_host.log || { cat /tmp/amdgpu_pm4_host.log; exit 1; }
tail -1 /tmp/amdgpu_pm4_host.log
# sysctl do libSystem (docs/darwin-bsd-mach.md, F1): kernel falso de SYS_DARWIN e o CPUID real do host
gcc -O1 -Wall -DMOS_LIBSYS_SYSCALL -I userland/libsystem -c userland/libsystem/sysctl.c -o /tmp/sysctl_ls.o
gcc -O1 -Wall tools/sysctl_host.c /tmp/sysctl_ls.o -o /tmp/sysctl_host
/tmp/sysctl_host | tail -1
# libproc do libSystem (F2): proc_pidinfo/pidfdinfo/rusage/listpids/terminate contra um kernel falso
gcc -O1 -Wall -DMOS_LIBSYS_SYSCALL -I userland/libsystem -c userland/libsystem/procinfo.c -o /tmp/procinfo_ls.o
gcc -O1 -Wall tools/procinfo_host.c /tmp/procinfo_ls.o -o /tmp/procinfo_host
/tmp/procinfo_host | tail -1
# IPC System V do libSystem (F7): semáforos, shm e filas entre processos (fork) sobre MAP_SHARED + futex
gcc -O1 -Wall -Wno-builtin-declaration-mismatch -DMOS_LIBSYS_SYSCALL -Dopen=h_open -I userland/libsystem -c userland/libsystem/sysv_ipc.c -o /tmp/sysv_ls.o
gcc -O1 -Wall tools/sysv_ipc_host.c /tmp/sysv_ls.o -o /tmp/sysv_ipc_host
timeout 60 /tmp/sysv_ipc_host | tail -1
# os_unfair_lock (futex), recursivo e os_sync_wait_on_address (F7), com 4 threads disputando
gcc -O1 -Wall -Wno-builtin-declaration-mismatch -DMOS_LIBSYS_SYSCALL -I userland/libsystem -c userland/libsystem/oslock.c -o /tmp/oslock_ls.o
gcc -O1 -Wall -pthread tools/oslock_host.c /tmp/oslock_ls.o -o /tmp/oslock_host
timeout 120 /tmp/oslock_host | tail -1
# kqueue/kevent (F6): todos os filtros contra um kernel falso (journal do VFS, processos, pressão de memória)
gcc -O1 -Wall -Wno-builtin-declaration-mismatch -DMOS_LIBSYS_SYSCALL -I userland/libsystem -c userland/libsystem/kqueue.c -o /tmp/kqueue_ls.o
gcc -O1 -Wall tools/kqueue_host.c /tmp/kqueue_ls.o -o /tmp/kqueue_host
timeout 30 /tmp/kqueue_host | tail -1
# API cliente do launchd (F9): launch_data_*, launch_msg CheckIn, vproc_*
gcc -O1 -Wall -Wno-builtin-declaration-mismatch -DMOS_LIBSYS_SYSCALL -I userland/libsystem -c userland/libsystem/launchdata.c -o /tmp/launchdata_ls.o
gcc -O1 -Wall tools/launchdata_host.c /tmp/launchdata_ls.o -o /tmp/launchdata_host
/tmp/launchdata_host | tail -1
# sandbox do libSystem (F10): checagens, extensões com token, contêiner e spawnattrs
gcc -O1 -w -DMOS_LIBSYS_SYSCALL -I userland/libsystem -c userland/libsystem/darwin.c -o /tmp/darwin_ls.o
gcc -O1 tools/sandbox_host.c /tmp/darwin_ls.o -Wl,--unresolved-symbols=ignore-all -o /tmp/sandbox_host
/tmp/sandbox_host | tail -1
# rotinas de portas Mach (F3): mach_port_names/space_info/get_srights/get_set_status/kobject
gcc -O1 -Wall -Wno-builtin-declaration-mismatch -DMOS_LIBSYS_SYSCALL -I userland/libsystem -c userland/libsystem/machports.c -o /tmp/machports_ls.o
gcc -O1 -Wall tools/machports_host.c /tmp/machports_ls.o -o /tmp/machports_host
/tmp/machports_host | tail -1
# os_activity, bootstrap_* antigos e _dyld_bind_fully (lacunas do corpus da Renata)
gcc -O1 -Wall -Wno-builtin-declaration-mismatch -DMOS_LIBSYS_SYSCALL -I userland/libsystem -c userland/libsystem/osactivity.c -o /tmp/osactivity_ls.o
gcc -O1 -w -pthread tools/osactivity_host.c /tmp/osactivity_ls.o -o /tmp/osactivity_host
/tmp/osactivity_host | tail -1
# GPU AMD: display, vsync, troca no vblank, buffer duplo, VRAM, SMU, GART, SDMA e IH contra um Picasso simulado
gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu -I tools tools/amdgpu_hw_host.c kernel/dev/amdgpu/amdgpu.c kernel/dev/amdgpu/engine.c kernel/dev/amdgpu/irq.c kernel/dev/amdgpu/gfx.c kernel/dev/amdgpu/compute.c kernel/dev/amdgpu/vm.c kernel/dev/amdgpu/abi.c kernel/dev/amdgpu/pm4check.c kernel/dev/amdgpu/pm.c kernel/dev/amdgpu/ddc.c \
    kernel/dev/amdgpu/edid.c kernel/dev/amdgpu/ucode.c -o /tmp/amdgpu_hw_host
/tmp/amdgpu_hw_host > /tmp/amdgpu_hw_host.log || { cat /tmp/amdgpu_hw_host.log; exit 1; }
tail -1 /tmp/amdgpu_hw_host.log
# URL longa no cliente HTTP do kernel (OAuth do Google no Antigravity: invalid_scope com o scope cortado)
gcc -std=gnu11 -DMOS_HOST_TEST -I kernel/include -I tools tools/http_url_longa_host.c kernel/net/http.c -o /tmp/http_url_longa
/tmp/http_url_longa > /tmp/http_url_longa.log || { cat /tmp/http_url_longa.log; exit 1; }
tail -1 /tmp/http_url_longa.log
# computer use: AX entre processos (reserva AF_UNIX, páginas, elemento único) e CGEventPostToPid só no pid alvo
gcc -std=gnu11 -Wall -Wextra -pthread -I userland/libsystem userland/libsystem/axproto.c userland/libsystem/axtransport.c \
    userland/libsystem/axunix.c userland/libsystem/axremote.c tools/axremote_host.c -o /tmp/axremote_host
/tmp/axremote_host > /tmp/axremote_host.log || { cat /tmp/axremote_host.log; exit 1; }
tail -1 /tmp/axremote_host.log
gcc -std=c11 -Wall -Wextra -I userland/libsystem userland/libsystem/cgpid.c tools/cgpid_host.c -o /tmp/cgpid_host
/tmp/cgpid_host > /tmp/cgpid_host.log || { cat /tmp/cgpid_host.log; exit 1; }
tail -1 /tmp/cgpid_host.log

# OpenGL por software: fixed function, GLSL (core e compat), texturas, FBO, UBO, transform feedback
L=userland/libsystem
gcc -std=gnu11 -O1 -w -fno-builtin -I tools/hostinc -I $L -o /tmp/gl_host tools/gl_host.c tools/gl_host_glsl.c \
    $L/gl_ctx.c $L/gl_buf.c $L/gl_tex.c $L/gl_fbo.c $L/gl_raster.c $L/gl_ffp.c $L/gl_misc.c $L/gl_eval.c $L/gl_arbprog.c $L/gl_debug.c $L/gl_glsl.c $L/gl_glsl_tr.c \
    $L/gl_glsl_pre.c $L/mtltex.c $L/mtlshader.c $L/mtlexec.c $L/mslspirv.c -lm -lpthread
/tmp/gl_host > /tmp/gl_host.log || { cat /tmp/gl_host.log; exit 1; }
tail -1 /tmp/gl_host.log

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
# o zig do PATH pode ser um link (~/.local/bin/zig -> .../zig-x86_64-linux-0.17.1/zig): lib/ fica ao lado do binário real
ZIGDIR=$(dirname "$(readlink -f "$ZIG")")
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

echo "==> unw_hosttest (API pública da libunwind no host)"
cc -O1 -fno-omit-frame-pointer -funwind-tables -rdynamic \
    -I"$ROOT/userland/libsystem" \
    "$ROOT/tools/unw_hosttest.c" "$ROOT/userland/libsystem/cxx_unwind.c" "$ROOT/userland/libsystem/unw_alias.S" \
    -ldl -o "$OUT/unw_hosttest"
"$OUT/unw_hosttest"

echo "==> libdispatch, runtime de blocos e Unicode do libSystem (testes de host)"
sh "$ROOT/tools/dispatch_hosttest.sh"
sh "$ROOT/tools/blocks_hosttest.sh"
cc -DMOS_HOST_TEST -O1 "$ROOT/userland/libsystem/swiftrt.c" "$ROOT/tools/swiftrt_hosttest.c" -o "$OUT/swiftrt_hosttest"
"$OUT/swiftrt_hosttest"

echo "==> runtime Swift do Grok (teste de host)"
bash "$ROOT/tools/swift-rt-grok-teste.sh" "$ZIG" "$OUT/swift-rt-grok"

echo "==> libSystem.mos"
# x86_64: _Unwind_* fica em cxx_unwind.c. UnwindLevel1.c repete esses nomes; o resto do libunwind fica.
LS_SRC=()
for f in "$ROOT"/userland/libsystem/*.c "$ROOT"/userland/libsystem/*.S; do
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
# Opus (libopus) e AAC (FDK AAC) com as cascas: docs/midia-plano.md
bash "$ROOT/tools/build-midia.sh" "$OUT" "$ZIG" "$ZIGDIR" "$(( JOBS < 2 ? JOBS : 2 ))" x86_64
mapfile -t MIDIA_OBJS < "$OUT/midia-x86_64/objs.txt"
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
    -Wno-unused-function -Wno-unused-variable -Wno-unreachable-code \
    -I"$ROOT/userland/host-audio/shim" -I"$ROOT/userland/libsystem" \
    -c "$ROOT/userland/libsystem/sqlite3_mos.c" -o "$OUT/sqlite3_mos.o"
# compression.c entra no ELF. MOS_LIBSYSTEM usa o cz_raw_* de compression_z.c
# (já na lista) em vez de incluir o .c de novo. liblzma (0BSD) e Brotli (MIT)
# compilam no mesmo link; o kshim cobre o que o freestanding não traz.
XZ="$ROOT/userland/third_party/xz"
BR="$ROOT/userland/third_party/brotli"
comp_one() {
    "$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib \
        -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
        -Wno-unused-function -Wno-unused-variable \
        -DHAVE_CONFIG_H -DTUKLIB_SYMBOL_PREFIX=lzma_ \
        -I"$ROOT/userland/libsystem/kshim" -I"$ROOT/userland/libsystem/kshim/xz-config" \
        -I"$ROOT/userland/libsystem" -I"$ROOT/userland/third_party/zlib" \
        -I"$ROOT/userland/third_party/lz4" -I"$ROOT/userland/third_party/lzfse" \
        -I"$XZ" -I"$XZ/src/liblzma/api" -I"$XZ/src/liblzma/common" -I"$XZ/src/liblzma/check" \
        -I"$XZ/src/liblzma/lz" -I"$XZ/src/liblzma/rangecoder" -I"$XZ/src/liblzma/lzma" \
        -I"$XZ/src/liblzma/delta" -I"$XZ/src/liblzma/simple" -I"$XZ/src/common" \
        -I"$BR/c/include" \
        -include "$ROOT/userland/libsystem/kshim/math.h" \
        -isystem "$ZIGDIR/lib/include" \
        "$@" || return 1
}
comp_one -DMOS_LIBSYSTEM -c "$ROOT/userland/frameworks/libcompression/compression.c" -o "$OUT/compression.o"
for pair in \
    lz4.c:lz4.o lz4hc.c:lz4hc.o lz4frame.c:lz4frame.o xxhash.c:xxhash.o
do
    src="${pair%%:*}"; obj="${pair##*:}"
    comp_one -c "$ROOT/userland/third_party/lz4/$src" -o "$OUT/$obj"
done
for pair in \
    lzfse_decode.c:lzfse_decode.o lzfse_decode_base.c:lzfse_decode_base.o \
    lzfse_encode.c:lzfse_encode.o lzfse_encode_base.c:lzfse_encode_base.o lzfse_fse.c:lzfse_fse.o \
    lzvn_decode_base.c:lzvn_decode_base.o lzvn_encode_base.c:lzvn_encode_base.o
do
    src="${pair%%:*}"; obj="${pair##*:}"
    comp_one -c "$ROOT/userland/third_party/lzfse/$src" -o "$OUT/$obj"
done
LZMA_OBJS=()
while IFS= read -r src; do
    base=$(basename "$src" .c)
    case "$base" in
        crc_clmul_consts_gen|crc32_small|crc64_small|tuklib_exit|tuklib_mbstr_fw|tuklib_mbstr_nonprint|tuklib_mbstr_width|tuklib_mbstr_wrap|tuklib_open_stdxxx|tuklib_progname|stream_decoder_mt|stream_encoder_mt) continue ;;
    esac
    comp_one -c "$src" -o "$OUT/lzma_${base}.o"
    LZMA_OBJS+=("$OUT/lzma_${base}.o")
done < <(find "$XZ/src/liblzma" "$XZ/src/common" -name '*.c' | sort)
BR_OBJS=()
while IFS= read -r src; do
    base=$(basename "$src" .c)
    comp_one -c "$src" -o "$OUT/br_${base}.o"
    BR_OBJS+=("$OUT/br_${base}.o")
done < <(find "$BR/c/common" "$BR/c/dec" "$BR/c/enc" -name '*.c' | sort)
comp_one -c "$ROOT/userland/libsystem/compression_z.c" -o "$OUT/compression_z.o"
echo "==> compression no ELF (ida e volta pelos símbolos)"
cc -O2 -I"$ROOT/userland/third_party/zlib" \
    "$ROOT/tools/compression_libsystem_host.c" \
    "$OUT/compression.o" "$OUT/compression_z.o" \
    "$OUT/lz4.o" "$OUT/lz4hc.o" "$OUT/lz4frame.o" "$OUT/xxhash.o" \
    "$OUT/lzfse_decode.o" "$OUT/lzfse_decode_base.o" "$OUT/lzfse_encode.o" \
    "$OUT/lzfse_encode_base.o" "$OUT/lzfse_fse.o" "$OUT/lzvn_decode_base.o" "$OUT/lzvn_encode_base.o" \
    "${LZMA_OBJS[@]}" "${BR_OBJS[@]}" \
    -lm -o "$OUT/compression_libsystem_host"
"$OUT/compression_libsystem_host"
echo "==> libm (musl 1.2.5, MIT; teste de host contra a glibc)"
bash "$ROOT/tools/build-libm-musl.sh" "$ZIG" x86_64 "$OUT/libm-x86" "$JOBS"
mapfile -t LIBM_OBJS < "$OUT/libm-x86/objs.txt"
ZIG="$ZIG" bash "$ROOT/tools/libm_hosttest.sh" "$OUT/libm-hosttest" "$OUT/libm-x86"
echo "==> libc_bsd (random BSD, ffs/fls, lldiv, NXArchInfo, backtrace; teste de host)"
ZIG="$ZIG" bash "$ROOT/tools/libc_bsd_hosttest.sh" "$OUT/libc-bsd-hosttest"
echo "==> multibyte (UTF-8 estrito, estado parcial; teste de host)"
ZIG="$ZIG" bash "$ROOT/tools/multibyte_hosttest.sh" "$OUT/multibyte-hosttest"
echo "==> wctype largo (tabelas Unicode do musl; teste de host)"
ZIG="$ZIG" bash "$ROOT/tools/wctype_hosttest.sh" "$OUT/wctype-hosttest"
echo "==> rand48 (família drand48 vs glibc; teste de host)"
ZIG="$ZIG" bash "$ROOT/tools/rand48_hosttest.sh" "$OUT/rand48-hosttest"
echo "==> libc de apps (swab/putenv/getproto/getloadavg...; teste de host)"
ZIG="$ZIG" bash "$ROOT/tools/libc_apps_hosttest.sh" "$OUT/libc-apps-hosttest"
# UnwindLevel1.c, UnwindLevel1-gcc-ext.c, UnwindRegistersSave.S e libunwind.cpp repetem cxx_unwind.c.
"$ZIG" cc -target x86_64-freestanding-none -O2 -g0 -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -static -no-pie \
    -fpie -mno-red-zone -Wall -Wno-unused-parameter -Wno-incompatible-library-redeclaration -Wno-builtin-requires-header \
    -I"$ROOT/userland/libsystem/kshim" -I"$ROOT/userland/libsystem/fsinc" -I"$ROOT/kernel/include" \
    -isystem "$ZIGDIR/lib/include" \
    -Wl,--image-base=0x400000100000 -Wl,-e,cx_start -Wl,--no-gc-sections \
    "${LS_SRC[@]}" "$OUT/audiodec.o" "$OUT/sqlite3_mos.o" "${MIDIA_OBJS[@]}" \
    "$OUT/compression.o" "$OUT/lz4.o" "$OUT/lz4hc.o" "$OUT/lz4frame.o" "$OUT/xxhash.o" \
    "$OUT/lzfse_decode.o" "$OUT/lzfse_decode_base.o" "$OUT/lzfse_encode.o" \
    "$OUT/lzfse_encode_base.o" "$OUT/lzfse_fse.o" "$OUT/lzvn_decode_base.o" "$OUT/lzvn_encode_base.o" \
    "${LZMA_OBJS[@]}" "${BR_OBJS[@]}" "${LIBM_OBJS[@]}" \
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

echo "==> overlay Darwin (teste de host)"
bash "$ROOT/tools/darwin-swift-teste.sh" "$ZIG" "$OUT/darwin-swift"

echo "==> dispatch_block e swift_task (teste de host)"
bash "$ROOT/tools/runtime-block-task-teste.sh" "$ZIG" "$OUT/runtime-block-task"

echo "==> Foundation sobre a ICU: locale, fuso, calendário e formatters (teste de host)"
bash "$ROOT/tools/fdn-i18n-teste.sh" "$OUT/fdn-i18n"

echo "==> compositor: sombra em cache igual à direta e tempo por quadro (teste de host)"
"$ZIG" cc -O2 -ffreestanding -fno-builtin -mno-red-zone -fshort-wchar -I"$ROOT/kernel/include" -I"$ROOT" -std=gnu11 -w \
    -c "$ROOT/kernel/gfx/gfx.c" -o "$OUT/gfx_host.o"
"$ZIG" cc -O2 -I"$ROOT/kernel/include" -I"$ROOT" -std=gnu11 -w "$ROOT/tools/compositor_bench_host.c" "$OUT/gfx_host.o" -lm \
    -o "$OUT/compositor_bench_host"
"$OUT/compositor_bench_host" 10

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
# firmware de dispositivos (Intel e Realtek Wi-Fi/Bluetooth, do linux-firmware, redistribuível; licenças junto)
if [ -d "$ROOT/firmware" ]; then
    mkdir -p "$ESP/mOS/Sistema/Firmware"
    cp -a "$ROOT/firmware/." "$ESP/mOS/Sistema/Firmware/"
fi
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
        bindir=usr/bin
        case "$name" in installer|pkgutil) bindir=usr/sbin ;; esac   # como no macOS: /usr/sbin/installer
        mkdir -p "$ESP/mOS/$bindir"
        if ! "$ZIG" cc -target x86_64-macos -O2 -I"$ROOT/userland/pkg" "$c" -o "$ESP/mOS/$bindir/$name" -Wl,-undefined,dynamic_lookup; then
            echo "aviso: /$bindir/$name não compilou" >&2
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
echo "==> libSystem arm64 (tools/build-libsystem-arm64.sh)"
"$ROOT/tools/build-libsystem-arm64.sh"
cp -f "$ROOT/build/libSystem-arm64.mos" "$ESP/mOS/Sistema/Biblioteca/a64/libSystem.mos"

echo "==> /usr/lib (libc++ do LLVM, zlib 1.3.2, SQLite e libcurl 8.23.0; não são stub)"
# O mesmo do build.ps1: libsqlite3 (WhatsApp, Core Data), libz e libcurl (/usr/lib/libcurl.4.dylib, sem
# libcurl.dylib) e o bundle de CAs em /usr/share/curl/cacert.pem (CURL_CA_BUNDLE de tools/libcurl/curl_config.h).
MOS_ZIG="$ZIG" bash "$ROOT/tools/build-libcxx.sh" "$ROOT/build/libcxx"
MOS_ZIG="$ZIG" sh "$ROOT/tools/build-libz.sh"
MOS_ZIG="$ZIG" sh "$ROOT/tools/build-sqlite.sh"
MOS_ZIG="$ZIG" bash "$ROOT/tools/build-libcurl.sh"
mkdir -p "$ESP/mOS/usr/lib" "$ESP/mOS/usr/share/zlib" "$ESP/mOS/usr/share/curl"
cp -f "$ROOT/build/libcxx/libc++.1.dylib" "$ESP/mOS/usr/lib/libc++.1.dylib"
cp -f "$ROOT/build/zlib-x86_64/libz.1.dylib" "$ESP/mOS/usr/lib/libz.1.dylib"
cp -f "$ROOT/build/sqlite/libsqlite3.dylib" "$ESP/mOS/usr/lib/libsqlite3.dylib"
# libxml2 2.13.8 (MIT): o WhatsApp liga htmlReadMemory, xmlReadMemory e xmlXPath* em /usr/lib/libxml2.2.dylib.
if MOS_ZIG="$ZIG" bash "$ROOT/tools/build-libxml2.sh" "$ROOT/build/libxml2/libxml2.2.dylib"; then
    mkdir -p "$ESP/mOS/usr/share/libxml2"
    cp -f "$ROOT/build/libxml2/libxml2.2.dylib" "$ESP/mOS/usr/lib/libxml2.2.dylib"
    cp -f "$ROOT/.tools/libxml2-2.13.8/Copyright" "$ESP/mOS/usr/share/libxml2/Copyright"
else
    echo "aviso: libxml2 não compilou; /usr/lib/libxml2.2.dylib fica de fora" >&2
fi
cp -f "$ROOT/build/curl-x86_64/libcurl.4.dylib" "$ESP/mOS/usr/lib/libcurl.4.dylib"
cp -f "$ROOT/build/zlib-x86_64/LICENSE" "$ESP/mOS/usr/share/zlib/LICENSE"
cp -f "$ROOT/build/curl-x86_64/COPYING" "$ESP/mOS/usr/share/curl/COPYING"
cp -f "$ROOT/build/curl-x86_64/LICENSE.mbedtls" "$ESP/mOS/usr/share/curl/LICENSE.mbedtls"
cp -f "$ROOT/assets/certs/cacert.pem" "$ESP/mOS/usr/share/curl/cacert.pem"
# ICU4C 74.2 como /usr/lib/libicucore.A.dylib e os dados em /usr/share/icu (tools/build-icu.sh, porte do build-icu.ps1):
# NSLocale, NSCalendar, formatters e transformações de NSString do libSystem, e o Intl do Claude Code.
if MOS_ZIG="$ZIG" bash "$ROOT/tools/build-icu.sh" --jobs 2; then
    mkdir -p "$ESP/mOS/usr/share/icu"
    cp -f "$ROOT/build/icu/libicucore.A.dylib" "$ESP/mOS/usr/lib/libicucore.A.dylib"
    cp -f "$ROOT/build/icu/icudt74l.dat" "$ESP/mOS/usr/share/icu/icudt74l.dat"
    cp -f "$ROOT/build/icu/LICENSE" "$ESP/mOS/usr/share/icu/LICENSE" 2>/dev/null || true
else
    echo "build: libicucore não compilou" >&2; exit 1
fi

echo "==> git (/usr/bin/git, /usr/libexec/git-core; do fonte, com a libz e a libcurl acima)"
MOS_ZIG="$ZIG" bash "$ROOT/tools/build-git.sh"
cp -a "$ROOT/build/git-x86_64/mos/." "$ESP/mOS/"

echo "==> libswift"
# Runtime real (swift-6.4.2) quando a toolchain e o fonte estão em .tools/. Sem isso, o stage completa com stub.
# Fora do CI, baixa a toolchain e o fonte (SHA-256 conferido) se faltarem: sem eles a imagem saía com as
# libswift* stub e sem Combine.framework. MOS_SWIFT_FETCH=0 desliga; no CI só com MOS_SWIFT_FETCH=1.
if [ "${MOS_SWIFT_FETCH:-}" = 1 ] || { [ -z "${MOS_SWIFT_FETCH:-}" ] && [ -z "${CI:-}" ]; }; then
    bash "$ROOT/tools/fetch-swift-toolchain.sh" || echo "aviso: toolchain Swift 6.4.2 não baixou; as libswift* continuam stub" >&2
fi
if [ -f "$ROOT/tools/build-libswift-real.sh" ]; then
    MOS_ZIG="$ZIG" bash "$ROOT/tools/build-libswift-real.sh" || echo "aviso: build-libswift-real.sh falhou; o que faltar continua stub" >&2
fi
# Frameworks ObjC do mOS (UIKit do Catalyst, CoreData, ImageIO...) e o Combine (OpenCombine como módulo
# "Combine") na mesma raiz das dylibs Swift; o cp -a abaixo leva tudo para a imagem.
MOS_ZIG="$ZIG" bash "$ROOT/tools/build-frameworks-objc.sh" || echo "aviso: algum framework ObjC não compilou" >&2
if [ -f "$ROOT/build/swift/objc/dylib/usr/lib/swift/libswiftCore.dylib" ]; then
    MOS_ZIG="$ZIG" bash "$ROOT/tools/build-swift-combine.sh" || echo "aviso: Combine.framework não compilou" >&2
fi
# Foundation Swift real (swift-foundation; libswiftFoundation, _StringProcessing, UniformTypeIdentifiers, Observation):
# sem ele URL, Data, Date... dos apps caem nos stubs zerados do libSystem. Depois do Combine: com o módulo Combine o
# swift-foundation conforma JSONEncoder/JSONDecoder a TopLevelEncoder/Decoder, como na Apple, e liga o Combine.framework
if [ -f "$ROOT/build/swift/objc/dylib/usr/lib/swift/libswiftCore.dylib" ]; then
    MOS_ZIG="$ZIG" bash "$ROOT/tools/build-swift-foundation-objc.sh" || echo "aviso: Foundation Swift não compilou; continua o stub" >&2
fi
# frameworks cujo overlay Swift importa o Foundation acima (arquivo swift-needs, ex.: CloudKit): segunda passada
if [ -e "$ROOT/build/swift/objc/overlays/Foundation.swiftmodule" ]; then
    precisa=$(cd "$ROOT/userland/frameworks" && grep -l . */swift-needs 2>/dev/null | cut -d/ -f1 | paste -sd, -)
    [ -n "$precisa" ] && { MOS_ZIG="$ZIG" bash "$ROOT/tools/build-frameworks-objc.sh" "" "$precisa" || echo "aviso: overlay Swift de $precisa não compilou" >&2; }
fi
# overlay Swift do UIKit do Catalyst (/System/iOSSupport/usr/lib/swift/libswiftUIKit.dylib): importa o Foundation acima
if [ -e "$ROOT/build/swift/objc/overlays/Foundation.swiftmodule" ]; then
    MOS_ZIG="$ZIG" bash "$ROOT/tools/build-swift-uikit-objc.sh" || echo "aviso: libswiftUIKit não compilou" >&2
fi
# SwiftUI.framework (OpenSwiftUI e dependências fixados por commit e SHA-256): depois do Foundation e do Combine acima
if [ -e "$ROOT/build/swift/objc/overlays/Foundation.swiftmodule" ]; then
    MOS_ZIG="$ZIG" python3 "$ROOT/tools/build-swiftui-objc.py" || echo "aviso: SwiftUI.framework não compilou" >&2
fi
if [ -f "$ROOT/build/universal-swift/usr/lib/swift/libswiftCore.dylib" ]; then
    cp -a "$ROOT/build/universal-swift/." "$ESP/mOS/"
elif [ -d "$ROOT/build/swift/objc/dylib" ]; then
    # os frameworks ObjC (UIKit, WebKit, CoreData, ImageIO...) entram mesmo sem o libswiftCore real;
    # antes só iam junto com ele e, sem toolchain Swift, a imagem ficava sem nenhum
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

echo "==> StoreKit.framework"
SKSRC="$ROOT/userland/frameworks/StoreKit/StoreKit.m"
SKOUT="$OUT/storekit"
SKDIR="$ESP/mOS/System/Library/Frameworks/StoreKit.framework/Versions/A"
mkdir -p "$SKOUT" "$SKDIR"
sk_one() {
    "$ZIG" cc -target "$1" -O2 -shared -x objective-c -I"$ROOT/userland/sdk/include" \
        -Wno-objc-root-class -Wno-objc-method-access "$SKSRC" -o "$2" \
        -Wl,-install_name,/System/Library/Frameworks/StoreKit.framework/Versions/A/StoreKit \
        -Wl,-undefined,dynamic_lookup
}
if sk_one x86_64-macos "$SKOUT/StoreKit.x86" && sk_one aarch64-macos "$SKOUT/StoreKit.arm"; then
    python3 - "$SKDIR/StoreKit" "$SKOUT/StoreKit.x86" "$SKOUT/StoreKit.arm" <<'PY'
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
parts_n = len(parts)
buf.extend(struct.pack(">II", 0xCAFEBABE, parts_n))
for cpu, sub, align, o, b in parts:
    buf.extend(struct.pack(">IIIII", cpu & 0xFFFFFFFF, sub & 0xFFFFFFFF, o & 0xFFFFFFFF, len(b) & 0xFFFFFFFF, align & 0xFFFFFFFF))
for cpu, sub, align, o, b in parts:
    if len(buf) < o:
        buf.extend(b"\0" * (o - len(buf)))
    buf.extend(b)
pathlib.Path(out).write_bytes(buf)
PY
    echo "StoreKit.framework ok"
else
    echo "StoreKit.framework não compilou" >&2
    exit 1
fi

echo "==> CoreImage.framework"
bash "$ROOT/tools/build-coreimage.sh" "$ZIG" "$ESP/mOS"

# WebKit.framework do mOS (userland/frameworks/WebKit): WKWebView sobre o motor web do sistema e o WebKit
# legado (WebView, WebPreferences, WebArchive...). O mesmo que o build.ps1 põe na imagem.
echo "==> WebKit.framework"
WKDIR="$ESP/mOS/System/Library/Frameworks/WebKit.framework/Versions/A"
mkdir -p "$WKDIR"
"$ZIG" cc -target x86_64-macos -O2 -shared -x objective-c -I"$ROOT/userland/sdk/include" \
    -Wno-objc-root-class -Wno-objc-method-access "$ROOT/userland/frameworks/WebKit/WebKit.m" -o "$WKDIR/WebKit" \
    -Wl,-install_name,/System/Library/Frameworks/WebKit.framework/Versions/A/WebKit \
    -Wl,-undefined,dynamic_lookup
echo "WebKit.framework ok"

# JavaScriptCore.framework do mOS (userland/frameworks/JavaScriptCore): o motor JS do sistema (kernel/web/js_*.c, sem o
# DOM) no processo do app, com a API C (jsc_api.c) e a ObjC (JSC.m) do JavaScriptCore por cima
echo "==> JavaScriptCore.framework"
JSCDIR="$ESP/mOS/System/Library/Frameworks/JavaScriptCore.framework/Versions/A"
JSCOBJ="$OUT/jsc-obj"
mkdir -p "$JSCDIR" "$JSCOBJ"
for f in $(ls "$ROOT"/kernel/web/js_*.c | grep -v /js_dom.c) "$ROOT/kernel/web/util.c" "$ROOT/userland/frameworks/JavaScriptCore/jsc_api.c"; do
    "$ZIG" cc -target x86_64-macos -O2 -std=gnu11 -w -fno-builtin -fno-stack-protector -fvisibility=hidden \
        -I"$ROOT/kernel/include" -I"$ROOT" -c "$f" -o "$JSCOBJ/$(basename "$f" .c).o"
done
"$ZIG" cc -target x86_64-macos -O2 -c -x objective-c -I"$ROOT/userland/sdk/include" -Wno-objc-root-class -Wno-objc-method-access \
    "$ROOT/userland/frameworks/JavaScriptCore/JSC.m" -o "$JSCOBJ/JSC.o"
"$ZIG" cc -target x86_64-macos -shared "$JSCOBJ"/*.o -o "$JSCDIR/JavaScriptCore" \
    -Wl,-install_name,/System/Library/Frameworks/JavaScriptCore.framework/Versions/A/JavaScriptCore \
    -Wl,-undefined,dynamic_lookup
echo "JavaScriptCore.framework ok"

# SafariServices.framework (userland/frameworks/SafariServices): a API das extensões do Safari num sistema sem Safari
echo "==> SafariServices.framework"
SSDIR="$ESP/mOS/System/Library/Frameworks/SafariServices.framework/Versions/A"
mkdir -p "$SSDIR"
"$ZIG" cc -target x86_64-macos -O2 -shared -x objective-c -I"$ROOT/userland/sdk/include" -Wno-objc-root-class -Wno-objc-method-access \
    "$ROOT/userland/frameworks/SafariServices/SafariServices.m" -o "$SSDIR/SafariServices" \
    -Wl,-install_name,/System/Library/Frameworks/SafariServices.framework/Versions/A/SafariServices -Wl,-undefined,dynamic_lookup
echo "SafariServices.framework ok"

# CryptoKit.framework: o Swift de userland/frameworks/CryptoKit compilado com evolução de biblioteca
# (tools/build-cryptokit-macho.sh), como o build.ps1 faz. Precisa do swift-frontend 6.4.2 oficial
# (SWIFT_FRONTEND, SWIFTC ou .tools/swift-6.4.2); sem ele o framework fica de fora desta imagem.
echo "==> CryptoKit.framework"
CKFE=${SWIFT_FRONTEND:-${SWIFTC:-}}
if [ -z "$CKFE" ]; then
    for c in "$ROOT/.tools/swift-6.4.2/usr/bin/swift-frontend" "$ROOT"/.tools/swift/*/usr/bin/swift-frontend; do
        if [ -x "$c" ]; then CKFE=$c; break; fi
    done
fi
if [ -n "$CKFE" ] && [ -x "$CKFE" ]; then
    if SWIFT_FRONTEND="$CKFE" MOS_ZIG="$ZIG" sh "$ROOT/tools/build-cryptokit-macho.sh" "$OUT/cryptokit-macho" \
            > "$OUT/cryptokit-macho.log" 2>&1; then
        CKDIR="$ESP/mOS/System/Library/Frameworks/CryptoKit.framework/Versions/A"
        mkdir -p "$CKDIR"
        cp -f "$OUT/cryptokit-macho/CryptoKit" "$CKDIR/CryptoKit"
        echo "CryptoKit.framework na imagem"
    else
        grep -m8 -E 'error:' "$OUT/cryptokit-macho.log" >&2 || true
        echo "CryptoKit.framework não compilou (log em $OUT/cryptokit-macho.log)" >&2
        exit 1
    fi
else
    echo "aviso: sem swift-frontend 6.4.2; CryptoKit.framework fica de fora desta imagem" >&2
fi

echo "==> apps oficiais em /Aplicativos"
bash "$ROOT/tools/stage-apps-oficiais.sh" "$OUT"
mkdir -p "$ESP/mOS/usr/lib"
cp -f "$ROOT/build/libcxx/libc++.1.dylib" "$ESP/mOS/usr/lib/libc++.1.dylib"

# Processo arm64 do tradutor (docs/runtime-arm64-plano.md): o processo é inteiro arm64, então cada dylib de /usr/lib
# vai universal (x86_64 + arm64) e o carregador escolhe a fatia. As fatias arm64 saem dos mesmos scripts com
# MOS_ARCH=arm64; a libc++ arm64 já sai do build-libcxx.sh (build/libcxx-arm64).
echo "==> /usr/lib universal (x86_64 + arm64)"
MOS_ZIG="$ZIG" MOS_ARCH=arm64 sh "$ROOT/tools/build-libz.sh"
MOS_ZIG="$ZIG" MOS_ARCH=arm64 sh "$ROOT/tools/build-sqlite.sh"
MOS_ZIG="$ZIG" MOS_ARCH=arm64 bash "$ROOT/tools/build-libcurl.sh"
LIPO=(python3 -I "$ROOT/tools/lipo-mos.py")
"${LIPO[@]}" -o "$ESP/mOS/usr/lib/libc++.1.dylib" "$ROOT/build/libcxx/libc++.1.dylib" "$ROOT/build/libcxx-arm64/libc++.1.dylib"
"${LIPO[@]}" -o "$ESP/mOS/usr/lib/libz.1.dylib" "$ROOT/build/zlib-x86_64/libz.1.dylib" "$ROOT/build/zlib-arm64/libz.1.dylib"
"${LIPO[@]}" -o "$ESP/mOS/usr/lib/libsqlite3.dylib" "$ROOT/build/sqlite/libsqlite3.dylib" "$ROOT/build/sqlite-arm64/libsqlite3.dylib"
"${LIPO[@]}" -o "$ESP/mOS/usr/lib/libcurl.4.dylib" "$ROOT/build/curl-x86_64/libcurl.4.dylib" "$ROOT/build/curl-arm64/libcurl.4.dylib"
MOS_ZIG="$ZIG" MOS_ARCH=arm64 bash "$ROOT/tools/build-icu.sh" --jobs 2
"${LIPO[@]}" -o "$ESP/mOS/usr/lib/libicucore.A.dylib" "$ROOT/build/icu/libicucore.A.dylib" "$ROOT/build/icu-arm64/libicucore.A.dylib"
if [ -f "$ESP/mOS/usr/lib/libxml2.2.dylib" ]; then
    MOS_ZIG="$ZIG" MOS_ARCH=arm64 bash "$ROOT/tools/build-libxml2.sh" "$ROOT/build/libxml2-arm64/libxml2.2.dylib"
    "${LIPO[@]}" -o "$ESP/mOS/usr/lib/libxml2.2.dylib" "$ROOT/build/libxml2/libxml2.2.dylib" "$ROOT/build/libxml2-arm64/libxml2.2.dylib"
fi
# Frameworks do mOS (userland/frameworks: UIKit, CoreData, WebKit, CoreImage...) com a fatia arm64: a mesma árvore do
# x86_64, compilada com MOS_ARCH=arm64, entra arquivo por arquivo nos Mach-O que já estão na ESP.
echo "==> frameworks universais (x86_64 + arm64)"
A64FW="$ROOT/build/swift/objc-arm64/dylib"
# runtime Swift, overlays, Foundation Swift e Combine arm64 (as mesmas etapas do x86_64 acima, MOS_ARCH=arm64)
MOS_ZIG="$ZIG" MOS_ARCH=arm64 bash "$ROOT/tools/build-libswift-real.sh" || echo "aviso: runtime Swift arm64 falhou" >&2
rm -rf "$A64FW/System"
if [ -f "$A64FW/usr/lib/swift/libswiftCore.dylib" ]; then
    MOS_ZIG="$ZIG" MOS_ARCH=arm64 bash "$ROOT/tools/build-swift-combine.sh" || echo "aviso: Combine arm64 não compilou" >&2
    MOS_ZIG="$ZIG" MOS_ARCH=arm64 bash "$ROOT/tools/build-swift-foundation-objc.sh" || echo "aviso: Foundation Swift arm64 não compilou" >&2
    MOS_ZIG="$ZIG" MOS_ARCH=arm64 python3 "$ROOT/tools/build-swiftui-objc.py" || echo "aviso: SwiftUI.framework arm64 não compilou" >&2
fi
MOS_ZIG="$ZIG" MOS_ARCH=arm64 bash "$ROOT/tools/build-frameworks-objc.sh" "$A64FW" || echo "aviso: algum framework arm64 não compilou" >&2
bash "$ROOT/tools/build-coreimage.sh" "$ZIG" "$A64FW" arm64
"${LIPO[@]}" --arvore "$ESP/mOS" "$A64FW" || echo "aviso: fatia arm64 sem par x86_64 na ESP (verificar-imagem-arm64.py lista)" >&2

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
python3 "$ROOT/tools/verificar-imagem-apps.py" "$ESP"
# o que ainda falta para o processo arm64 (docs/runtime-arm64-plano.md); vira obrigatório quando a lista zerar
python3 -I "$ROOT/tools/verificar-imagem-arm64.py" --relatorio "$ESP"
