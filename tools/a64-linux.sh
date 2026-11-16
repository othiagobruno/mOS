#!/bin/bash
# mOS — verificação do tradutor arm64 no Linux (equivalente a build-a64test.ps1 + a64cmp.ps1).
# Não roda o build do projeto: só o harness de host, o monitor do oráculo e os ELF de teste.
#
#   tools/a64-linux.sh build            # build/a64l/{a64host,a64host-jit,mon.elf,tests/*.elf}
#   tools/a64-linux.sh cmp [nome...]    # interp × JIT × QEMU em cada ELF (ou só nos nomes dados)
#   tools/a64-linux.sh oracle x.elf     # saída do QEMU (mesmo formato do a64host)
#   tools/a64-linux.sh fuzz [args...]   # build/a64l/a64fuzz (mesmos argumentos do a64fuzz.c)
#
# Trabalho pesado (compilação, QEMU, fuzz) entra na trava compartilhada /tmp/mos-pesado.lock com
# nice 10 e espera ter 3000 MB livres (regras de RAM do time).
#
# QEMU: MOS_QEMU_A64 ou qemu-system-aarch64 no PATH. Zig: MOS_ZIG ou zig no PATH.
set -u
R=$(cd "$(dirname "$0")/.." && pwd)
O=${A64L_OUT:-$R/build/a64l}
ZIG=${MOS_ZIG:-zig}
CC=${CC_HOST:-clang}
QEMU=${MOS_QEMU_A64:-qemu-system-aarch64}
TMO=${A64L_TIMEOUT:-60}
LLVM_OBJDUMP=${MOS_LLVM_OBJDUMP:-$(command -v llvm-objdump || ls /usr/lib/llvm-*/bin/llvm-objdump 2>/dev/null | tail -1)}

pesado() {
    while [[ $(free -m | awk '/^Mem/{print $7}') -lt 3000 ]]; do sleep 30; done
    flock /tmp/mos-pesado.lock nice -n 10 "$@"
}

build() {
    mkdir -p "$O/tests"
    local base=(userland/a64/a64_fp.c userland/a64/a64_xmon.c userland/a64/engine.c userland/a64/interp*.c)
    (cd "$R" && $CC -O2 -g -mcx16 -Wall -Wno-unused-function -fno-strict-aliasing -pthread \
        tools/a64host.c "${base[@]}" -o "$O/a64host" -lm) || { echo "a64host: falhou"; return 1; }
    (cd "$R" && $CC -O2 -g -mcx16 -Wall -Wno-unused-function -fno-strict-aliasing -pthread -DA64_HAVE_JIT -DA64_HOST_TOOL \
        tools/a64host.c "${base[@]}" userland/a64/jit*.c -o "$O/a64host-jit" -lm) || { echo "a64host-jit: falhou"; return 1; }
    $ZIG cc -target aarch64-freestanding-none -mcpu=cortex_a76 -O2 -g0 -ffreestanding -nostdlib -static -fno-pic \
        -fno-stack-protector -mgeneral-regs-only -fno-builtin -Wall -Wno-unused-function \
        -Wl,-T,"$R/tools/a64oracle/mon.ld" "$R/tools/a64oracle/mon.S" "$R/tools/a64oracle/mon.c" \
        -o "$O/mon.elf" || { echo "mon.elf: falhou"; return 1; }
    $CC -O2 -Wall -Wno-unused-function "$R/tools/a64fuzz.c" -o "$O/a64fuzz" || { echo "a64fuzz: falhou"; return 1; }
    local fails=0 n=0 t b lay
    for t in "$R"/tools/a64tests/*.c "$R"/tools/a64tests/*.S; do
        b=$(basename "${t%.*}")
        [[ $b == crt ]] && continue
        if [[ $# -gt 0 && " $* " != *" $b "* ]]; then continue; fi
        if [[ $b == regress_* ]]; then lay=(-Wl,-T,"$R/tools/a64oracle/regress.ld" -Wl,-e,_start)
        else lay=(-Wl,--image-base=0x200000000 -Wl,-e,_start); fi
        n=$((n + 1))
        $ZIG cc -target aarch64-freestanding-none -O1 -g0 -ffreestanding -nostdlib -static -fno-pic -fno-stack-protector \
            -mcpu=apple_m2 -I"$R/tools/a64tests" "${lay[@]}" "$R/tools/a64tests/crt.S" "$t" -o "$O/tests/$b.elf" \
            || { echo "teste $b: não compilou"; fails=$((fails + 1)); }
    done
    [[ $fails -eq 0 ]] || return 1
    echo "ok: a64host, a64host-jit, mon.elf + $n teste(s)"
}

oracle() {
    case "$1" in *,*) echo "caminho com vírgula"; return 2;; esac
    timeout "$TMO" "$QEMU" -M virt -cpu max -m 2048 -display none -serial none -monitor none -nodefaults \
        -semihosting-config enable=on,target=native -kernel "$O/mon.elf" \
        -device loader,file="$1",addr=0x44000000,force-raw=on
}

run3() { # elf → grava $T/{q,i,j}.{out,err,code}
    local elf=$1
    oracle "$elf" >"$T/q.out" 2>"$T/q.err"; echo $? >"$T/q.code"
    timeout "$TMO" "$O/a64host" --argv0 prog "$elf" >"$T/i.out" 2>"$T/i.err"; echo $? >"$T/i.code"
    timeout "$TMO" "$O/a64host-jit" --jit --argv0 prog "$elf" >"$T/j.out" 2>"$T/j.err"; echo $? >"$T/j.code"
}

same() { cmp -s "$T/$1.out" "$T/$2.out" && cmp -s "$T/$1.err" "$T/$2.err" && cmp -s "$T/$1.code" "$T/$2.code"; }

cmp_all() {
    T=$(mktemp -d); trap 'rm -rf "$T"' RETURN
    local ok=0 bad=0 e b src nc
    for e in "$O"/tests/*.elf; do
        b=$(basename "${e%.elf}")
        if [[ $# -gt 0 && " $* " != *" $b "* ]]; then continue; fi
        src=$(ls "$R"/tools/a64tests/"$b".[cS] 2>/dev/null | head -1)
        nc=0; grep -q 'a64cmp: nao-comparavel' "$src" 2>/dev/null && nc=1
        run3 "$e"
        if [[ $nc == 1 ]]; then
            if same i j; then echo "IGUAL  $b (autoverificado, interp = JIT, saída $(cat "$T/i.code"))"; ok=$((ok + 1))
            else echo "DIFERE $b (autoverificado: interp=$(cat "$T/i.code") jit=$(cat "$T/j.code"))"; bad=$((bad + 1)); fi
            continue
        fi
        local r=""
        same i q || r+=" interp≠qemu"
        same j q || r+=" jit≠qemu"
        if [[ -z $r ]]; then echo "IGUAL  $b (saída $(cat "$T/q.code"))"; ok=$((ok + 1))
        else
            echo "DIFERE $b:$r (códigos q=$(cat "$T/q.code") i=$(cat "$T/i.code") j=$(cat "$T/j.code"))"
            diff <(cat "$T/q.out" "$T/q.err") <(cat "$T/i.out" "$T/i.err") | head -8 | sed 's/^/    /'
            bad=$((bad + 1))
        fi
    done
    echo "a64-linux cmp: $ok iguais, $bad diferentes"
    [[ $bad -eq 0 ]]
}

cmd=${1:-}; shift || true
case "$cmd" in
    build) pesado bash "$0" _build "$@" ;;
    _build) build "$@" ;;
    cmp) pesado bash "$0" _cmp "$@" ;;
    _cmp) cmp_all "$@" ;;
    oracle) oracle "$@" ;;
    fuzz) MOS_QEMU_A64=$QEMU MOS_LLVM_OBJDUMP=$LLVM_OBJDUMP pesado timeout 3000 "$O/a64fuzz" "$@" ;;
    *) sed -n 2,9p "$0"; exit 2 ;;
esac
