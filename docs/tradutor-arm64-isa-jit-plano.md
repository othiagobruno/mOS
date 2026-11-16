# Tradutor arm64 — frente ISA e JIT (Carolina): levantamento e plano

Dona: Carolina (frente 15 de `TIME.md`), junto com a Paula (liderança do tradutor). Runtime de processo
(dyld arm64, 16 KiB, frameworks, `MAP_JIT` do lado do processo) é do Sergio, frente 16.
Contrato e arquitetura: [tradutor-arm64.md](tradutor-arm64.md). Pendências antigas:
[tradutor-arm64-pendencias.md](tradutor-arm64-pendencias.md), [jit-pendencias.md](jit-pendencias.md).

## O que já existe na `main` (medido por mim no Linux em 15 nov.)

- Interpretador (`userland/a64/interp*.c`, `a64_fp.c`) com os recursos de `A64_FEATURES`
  (NEON/ASIMD, FP16, FHM, AES/PMULL/SHA1/SHA256/SHA512/SHA3, CRC32, LSE/LSE2, RDM, DotProd, FCMA,
  JSCVT, RCpc 1/2, FlagM 1/2, FRINTTS, SB, SSBS, DPB/DPB2, DIT, PAuth "identidade", BTI, BF16, I8MM).
- JIT (`userland/a64/jit*.c`, `jit_simd.h`) com chamada leve ao interpretador para o que não tem forma
  nativa; monitor exclusivo sem ABA (`a64_xmon.c`); rede de SMC com proteção de página.
- Harness Linux novo desta frente: `tools/a64-linux.sh` (build, `cmp` interp × JIT × QEMU, `fuzz`) e
  `tools/a64fuzz.c` compilando também em POSIX. O QEMU usado é o `qemu-system-aarch64` 10.2.1 do
  pacote Ubuntu, extraído sem instalar (`MOS_QEMU_A64`).
- Primeira rodada `tools/a64-linux.sh cmp`: 49 de 55 ELF de `tools/a64tests` iguais ao QEMU nos dois
  motores; os 5 autoverificados já marcados davam diferença só porque o script exigia saída 0 (corrigido:
  autoverificado = JIT igual ao interpretador). `int_xmon` diverge do QEMU por definição (store comum do
  mesmo PE derrubar o monitor é IMPLEMENTATION DEFINED; o QEMU compara valor) e passou a
  `nao-comparavel`.

## O que falta (levantamento)

| # | Item | Situação | Critério de pronto |
|---|---|---|---|
| 1 | Placar completo no Linux (fuzz de todos os grupos × QEMU, interp e JIT) | harness portado, rodada pendente (fila de RAM) | 0 divergências nas sementes do placar, tabela no `tradutor-arm64.md` |
| 2 | Lista do que o JIT ainda manda ao interpretador | levantar com `A64JIT_FBSTAT=1` em todos os grupos SIMD/FP do fuzzer | lista no fim deste doc, por classe |
| 3 | Nativos SIMD que faltam: SQSHL/UQSHL/SQRSHL/UQRSHL (reg e imediato), SQSHLU, SQDMULH/SQRDMULH 32 bits e por elemento, SQDMLAL/SQDMLSL/SQDMULL, FP pareado além de FADDP, FABD | pelo interpretador (corretos) | nativo + fuzz do grupo com 3 sementes, JIT = interp = QEMU |
| 4 | Estimativas FP (FRECPE/FRSQRTE/FRECPX/FRECPS/FRSQRTS) | interpretador | FRECPS/FRSQRTS nativos (FMA exato); FRECPE/FRSQRTE por tabela de 256/512 entradas do ARM ARM emitida inline |
| 5 | FP16: FMLA/FMLS half, FCVT D→H | interpretador | nativo só com prova (arredondamento duplo); senão fica documentado como chamada leve |
| 6 | Desempenho: superblocos (seguir B/BL diretos), cache de registradores entre saídas encadeadas | `jit_bench_call` ~430 MIPS | medir antes/depois com `tools/jit-medida.sh`, máquina livre |
| 7 | Modelo de memória: teste de litmus multi-thread (MP, SB, LB, IRIW com DMB/LDAR/STLR) no host | só testes de uma thread | harness `tools/a64litmus_host.c` sem resultado proibido em 10⁶ iterações |
| 8 | SMC/exceções: testes com fork real, `read()` sobre código, falha no meio de bloco | cobertos no host Linux; QEMU do mOS pendente | prova no build final (fila `build-final.sh`) |

Itens 1, 2 e 7 dependem da trava `/tmp/mos-pesado.lock` (QEMU e fuzz são trabalho pesado). Item 8 só roda
no build final, que é uma vez só no fim da frente.

## Fluxo de verificação

```
export MOS_QEMU_A64=<qemu-system-aarch64>        # 10.2+ com -cpu max
tools/a64-linux.sh build
tools/a64-linux.sh cmp                            # ELF de tools/a64tests
tools/a64-linux.sh fuzz --grupo all --casos 300 --k 8 --jit --semente N
```

## Andamento

Atualizado a cada PR desta frente.
