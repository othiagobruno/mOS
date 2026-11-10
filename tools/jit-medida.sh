#!/bin/bash
# Mede o JIT arm64 no harness Linux. Não altera o produto.
# Uso: tools/jit-medida.sh build/a64host build/a64tests [repetições]
# Grava um TSV em stdout: bench n insns amostras mediana_ns mad_ns min_ns max_ns mips
set -u
HOST=${1:?a64host}
DIR=${2:?dir dos elf}
REPS=${3:-11}
CPUSET=${CPUSET:-1}

benches=(
  "jit_bench_call|"
  "jit_bench_int|"
  "jit_bench_mem|"
  "jit_bench_leaf|"
  "jit_bench_br|"
)

task() {
  if command -v taskset >/dev/null 2>&1; then taskset -c "$CPUSET" "$@"; else "$@"; fi
}

echo -e "bench\tn\tinsns\tnreps\tmediana_ns\tmad_ns\tmin_ns\tmax_ns\tmips"
for spec in "${benches[@]}"; do
  name=${spec%%|*}
  args=${spec#*|}
  elf="$DIR/$name.elf"
  if [[ ! -f $elf ]]; then echo "falta $elf" >&2; exit 1; fi
  insns=$(task "$HOST" --count --argv0 "$name" "$elf" $args 2>&1 >/dev/null | awk '/^insns /{print $2}')
    mode=(--jit)
  if [[ ${INTERP:-0} == 1 ]]; then mode=(); fi
  # aquecimento (descartado)
  task "$HOST" "${mode[@]}" --tempo --argv0 "$name" "$elf" $args >/dev/null 2>&1
  times=()
  for ((i = 0; i < REPS; i++)); do
    ns=$(task "$HOST" "${mode[@]}" --tempo --argv0 "$name" "$elf" $args 2>&1 >/dev/null | awk '/^tempo_ns /{print $2}')
    times+=("$ns")
  done
  python3 - "$name" "$args" "$insns" "${times[@]}" << 'PY'
import sys
name, args, insns, *ts = sys.argv[1:]
xs = sorted(int(t) for t in ts)
n = len(xs)
med = xs[n // 2]
devs = sorted(abs(x - med) for x in xs)
mad = devs[n // 2]
mips = (int(insns) / (med / 1e9)) / 1e6 if med else 0
print(f"{name}\t{args or '-'}\t{insns}\t{n}\t{med}\t{mad}\t{xs[0]}\t{xs[-1]}\t{mips:.1f}")
PY
done
