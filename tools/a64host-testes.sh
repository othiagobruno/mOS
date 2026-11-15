#!/bin/bash
# Testes ELF do tradutor arm64 (tools/a64tests) no harness Linux tools/a64host.c, interpretador × JIT.
#   tools/a64host-testes.sh [nome-do-teste...]
# Compila build/a64host (com JIT) e build/a64tests/*.elf com as mesmas flags do build-a64test.ps1. Para cada teste,
# roda no interpretador e no JIT e exige a mesma saída e o mesmo código de saída. O código esperado é 0
# (autoverificação), salvo os testes de FAULT (139) e UDF (132) listados em ESPERA abaixo; um teste com a marca
# "a64cmp: nao-comparavel" só precisa bater entre os motores. Os jit_bench_* só rodam com BENCH=1.
# Não é o oráculo QEMU (a64cmp.ps1 no Windows): aqui não há comparação com o ARM de referência.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ZIG=${MOS_ZIG:-$(command -v zig || true)}
[ -n "$ZIG" ] || { echo "zig: defina MOS_ZIG" >&2; exit 1; }
OUT=$ROOT/build/a64tests
mkdir -p "$OUT"
HOST=$ROOT/build/a64host
"$ZIG" cc -target x86_64-linux-gnu -O2 -g -mcx16 -Wall -Wno-unused-function -fno-strict-aliasing -DA64_HAVE_JIT \
    "$ROOT/tools/a64host.c" "$ROOT"/userland/a64/*.c -o "$HOST" -pthread -lm || { echo "a64host: build FALHOU"; exit 1; }

if [ $# -gt 0 ]; then names=("$@"); else
    names=()
    for f in "$ROOT"/tools/a64tests/*.c "$ROOT"/tools/a64tests/*.S; do
        b=$(basename "$f"); b=${b%.*}
        [ "$b" = crt ] && continue
        [[ $b == jit_bench_* && ${BENCH:-0} != 1 ]] && continue
        names+=("$b")
    done
fi

declare -A ESPERA=(
    [jit_fault_ld1]=139 [jit_fault_ldp]=139 [jit_fault_mid]=139 [jit_fault_pair]=139 [jit_fault_q]=139
    [jit_fault_sub]=139 [jit_ldar]=139 [jit_rcpc2_fault]=139 [jit_sa0]=139 [jit_badpc]=139 [jit_sb_fault]=139
    [regress_br_reg_d65f0160]=139
    [jit_udf_first]=132 [jit_3same_extra_udf]=132 [jit_udf_escalar_shift]=132 [jit_udf_escalar_elem]=132
    [regress_br_reg_503c5df0]=132 [regress_dp_csel_33e9fc55]=132
)
pass=0; fail=0
for n in "${names[@]}"; do
    src=$(ls "$ROOT/tools/a64tests/$n".[cS] 2>/dev/null | head -1)
    [ -n "$src" ] || { echo "FALHA $n: fonte ausente"; fail=$((fail + 1)); continue; }
    if [[ $n == regress_* ]]; then lay=(-Wl,-T,"$ROOT/tools/a64oracle/regress.ld" -Wl,-e,_start)
    else lay=(-Wl,--image-base=0x200000000 -Wl,-e,_start); fi
    if ! "$ZIG" cc -target aarch64-freestanding-none -O1 -g0 -ffreestanding -nostdlib -static -fno-pic -fno-stack-protector \
        -mcpu=apple_m2 -I"$ROOT/tools/a64tests" "${lay[@]}" "$ROOT/tools/a64tests/crt.S" "$src" -o "$OUT/$n.elf" 2>"$OUT/$n.cc.txt"; then
        echo "FALHA $n: não compilou ($(head -1 "$OUT/$n.cc.txt"))"; fail=$((fail + 1)); continue
    fi
    oi=$(timeout 120 "$HOST" --argv0 prog "$OUT/$n.elf" 2>&1); ci=$?
    oj=$(timeout 120 "$HOST" --jit --argv0 prog "$OUT/$n.elf" 2>&1); cj=$?
    selfchk=1; grep -q 'a64cmp: nao-comparavel' "$src" && selfchk=0
    esp=${ESPERA[$n]:-0}
    if [ "$oi" != "$oj" ] || [ $ci -ne $cj ]; then
        echo "FALHA $n: JIT ≠ interpretador (saída $ci × $cj)"
        diff <(echo "$oi") <(echo "$oj") | head -8
        fail=$((fail + 1))
    elif [ $selfchk = 1 ] && [ $ci -ne "$esp" ]; then
        echo "FALHA $n: saída $ci, esperada $esp"; echo "$oi" | tail -5; fail=$((fail + 1))
    else
        echo "ok   $n (saída $ci)"; pass=$((pass + 1))
    fi
done
echo "a64host-testes: $pass/$((pass + fail)) ok"
[ $fail -eq 0 ]
