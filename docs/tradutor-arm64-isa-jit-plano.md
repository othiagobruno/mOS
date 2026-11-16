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

## Andamento (15 nov. 2026)

### Bugs do JIT achados e corrigidos (todos com regressão)

| PR | Bug | Como apareceu |
|---|---|---|
| #271 | FCVTAS/FCVTAU somavam ±0,5 e truncavam: erro em 0,49999997f, 2^52 + 1, 2^23 + 1 | fuzz interp × JIT, `fp.int`; `jit_cvt_borda` falha no código antigo |
| #271 | UCVTF de u64 com bit 63 sem sticky: arredondamento duplo (2^63 + 1025) | fuzz, `simdv.2misc` |
| #301 | remendo de salto (`x_patch8/32`) gravava 1–4 bytes depois do fim do cache de código quando o bloco estourava o buffer | FAULT do JIT em endereço do host, 3 em 45 000 casos; `a64-linux.sh jitcache` (cache de 16 KiB) |

O terceiro corrompe a memória logo depois do cache de código no processo real; avisei o Sergio e a Paula
(pode ter relação com a queda do Claude arm64 na VM, `tradutor-arm64-runtime-falta.md` item 2).

### Novos nativos no JIT (antes iam pela chamada leve ao interpretador)

| PR | Instruções |
|---|---|
| #271 | SQSHL/UQSHL/SQSHLU por imediato (vetor e escalar) |
| #277 | SHADD/UHADD/SRHADD/URHADD/SHSUB/UHSUB, SABD/UABD/SABA/UABA, MUL/MLA/MLS 8..32 bits, ADD/SUB/CMxx/NEG de D escalar |
| #286 | FCMEQ/FCMGE/FCMGT/FACGE/FACGT e FCMxx #0 (S/D, vetor e escalar), FCVTZS/FCVTZU de S/D |
| #290 | SMULL/UMULL/SMLAL/UMLAL/SMLSL/UMLSL por elemento, SABDL/UABDL/SABAL/UABAL |
| #293 | SDOT/UDOT/USDOT/SUDOT (vetor e por elemento) |
| #301 | SQDMULH/SQRDMULH de S e por elemento, SQDMULL/SQDMLAL/SQDMLSL |
| #304 | SQABS/SQNEG, SQADD/UQADD/SQSUB/UQSUB escalares |
| #308 | FCVTN/P/M/A (S e U) vetoriais e escalares de S/D |
| #311 | DUP elemento → escalar, FMUL/FMLA/FMLS escalares por elemento, FADDP escalar e 2S |
| #314 | FMAX/FMIN/FMAXNM/FMINNM, FABD, FMULX vetoriais de S/D |
| #343 #398 | FP16: comparações, FMAX/FMIN/FMAXNM/FMINNM, FABD, FMULX; FMAXP/FMINP/FMAXNMP/FMINNMP de S/D |
| #405 | FRECPS/FRSQRTS vetoriais (FMA), FDIV 2S (teste `jit_fstep`) |
| #410 #413 | ADDV H/S, SADDLV/UADDLV, SADDLP/UADDLP/SADALP/UADALP; FMAXV/FMINV/FMAXNMV/FMINNMV de 4S |
| #417 #420 #424 | FCVTL/FCVTN (H↔S, S↔D); FSQRT/FRINTX vetoriais, SHLL; CLZ/CLS |
| #433 | SRI/SLI; SQ(R)SHRN/UQ(R)SHRN/SQ(R)SHRUN 16→8 e 32→16; SCVTF/UCVTF/FCVTZS/FCVTZU #fbits vetoriais (teste `jit_fixcvt`) |
| #439 #442 | FRINTA e FRINT32Z/32X/64Z/64X vetoriais; RBIT |

Cada lote: fuzz interp × JIT (`a64fuzz --sem-qemu`) com 3–4 sementes, de 75 000 a 250 000 casos,
0 divergências. O interpretador é o oráculo verificado contra o QEMU.

### Verificação

- Testes ELF × QEMU 10.2 (`a64-linux.sh cmp`): 62 iguais, 0 diferentes (inclui `jit_qshl` e
  `jit_cvt_borda`).
- Fuzz SIMD/FP × QEMU, semente 20261115, 100 000 casos: JIT = interpretador em todos; 245 casos de
  load/store SIMD divergiam do QEMU por causa do harness Linux (o `a64host` mapeava 64 KiB de rascunho
  e o monitor 16 KiB). Corrigido no `a64host` (#328).
- Placar completo × QEMU 10.2 (16 nov., harness corrigido): todos os grupos, semente 20261116,
  139 500 casos de 1 instrução, e semente 20261117, 18 600 sequências de 8 instruções:
  0 divergências e 0 JIT ≠ interpretador.
- Modelo de memória (`tools/a64litmus_host.c`, `a64-linux.sh litmus`): duas threads, 500 000 iterações
  por teste, JIT e interpretador. SB com STLR/LDAR, DMB ISH e SWPAL e MP com STLR/LDAR: 0 resultados
  proibidos. Controle sem barreira: 57 285 reordenações no JIT (o teste enxerga a reordenação do x86).
- Desempenho (`tools/jit-medida.sh`, carga 7,3 no host): chamadas 632 MIPS, inteiros 1045, memória 586,
  folha 1839, desvios 1954.

### O que ainda passa pela chamada leve (correto, sem aceleração)

Medido com `A64JIT_FBLOG=1` no fuzz de SIMD/FP (distribuição uniforme por codificação, não por uso real):
- FP16 (formas `h` de quase tudo: aritmética por elemento, FMLA/FMLS, comparações, conversões, pareadas);
- SHA1/SHA256 quando a codificação não cai no caminho nativo (investigar: o host tem SHA-NI);
- estimativas FRECPE/FRSQRTE/FRECPS/FRSQRTS/FRECPX, FCMLA/FCADD, BF16 (BFDOT/BFMMLA/BFMLAL/BFCVT), I8MM;
- SQRDMLAH/SQRDMLSH, SQRSHL/UQRSHL/SQSHL/UQSHL por registrador, SUQADD/USQADD, SQ*SHRN/SQXTN escalares;
- conversões de ponto fixo (`#fbits`) e FP↔GPR fora de FCVTZ, FP pareadas (FMAXP/FMINP/...NMP), FMAXV etc.;
- SADDLP/UADALP, CLZ/CLS, RBIT, SRI/SLI, URSHL/SRSHL, PMUL/PMULL de 8 bits.

### Próximos passos

1. Repetir o placar a cada lote novo do JIT.
2. Desempenho: medir com a máquina livre; superblocos e cache de registradores entre saídas encadeadas
   (`jit_bench_call`).
3. FP16 nativo via F16C (exato para add/sub/mul/div/sqrt: f32 tem precisão ≥ 2·11 + 2) e o resto da
   lista acima, por ordem de uso nos apps (`A64JIT_FBSTAT` nos binários reais com o Sergio).
