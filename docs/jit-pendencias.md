# JIT arm64 → x86_64: o que ainda falta

Estado em 08/11. O JIT executa toda instrução (nativa ou via interpretador) e, até a última
rodada validada, não tinha nenhuma divergência interp × JIT no fuzzing (todos os 93 grupos, K=8)
nem na suíte (39 testes ELF no placar do TESTES). Nada está commitado.

## 1. Em andamento: código escrito, ainda NÃO compilado nem testado

A última build foi interrompida. Estas mudanças estão no disco sem verificação:

- `jit_core.c`:
  - a chave do `fbstat` mantém os bits 20:16 nos grupos em que eles são opcode (FP↔inteiro, FP de
    1 fonte, 2misc/across). A linha duplicada `| 1u; /* ... */` já foi corrigida.
  - o CPUID agora só liga FMA3/F16C/AVX2 se o SO habilitou o estado AVX (OSXSAVE + XCR0);
    campo novo `jit_g.avx2` em `jit.h`.
- `jit_tr.c`:
  - PAC/AUT/XPAC de 1 fonte como identidade (sem código), com a mesma validação do INT.
  - CRC32C (B/H/W/X) via `crc32` do SSE4.2.
  - LDAPUR/STLUR e variantes com sinal (`tr_rcpc2`), com SA0 e a falha de quem atravessa 16 bytes.

**Próximo passo:** rodar `mk.sh` e `cmp.sh` (scratchpad), escrever um teste dirigido
(`jit_rcpc2.S`: LDAPUR/STLUR de todos os tamanhos, CRC32C, PACIA/XPACI/AUTIA) e rodar o fuzzing
de `ls.rcpc_unscaled`, `dp.1src` e `dp.2src`.

## 2. Pela lista real dos apps (`jit-fbstat-*.txt` na pasta de notas)

Ainda não feitos, em ordem de contagem (WhatsApp; os apps Electron só pararam na partida):

| chave | instrução | plano |
|---|---|---|
| `4f400400` ×916 | SSHR/SSRA de 64 bits | emular: `psrlq` + máscara de sinal (`psrad 31` + `pshufd 0xF5`) deslocada com `psllq`; tirar os `return 0` de `s_shimm`/`s_shift`/`s_rshr` para lanes de 64 com sinal |
| `6ee04400` ×215, `6ea04400` ×8 | USHL 2D/4S | AVX2: `vpsllvq/d(x, b)` OR `vpsrlvq/d(x, 256 - b)`, com b = byte baixo de cada lane (contagens >= largura dão 0) |
| `5e60d800` ×74, `7e60d800` | provavelmente SCVTF/UCVTF escalares no SIMD (Dd, Dn) | confirmar com a chave nova; `cvtsi2sd` em RN, UCVTF com bit alto → interp |
| `0e205800` ×65 | CNT 8B/16B | `pshufb` com tabela de nibbles |
| `0e200800` ×22 | REV64 (e REV32/REV16 vetoriais) | `pshufb` com máscara constante |
| `9e200000` ×57 em todos os apps | conversão FP→inteiro que não é FCVTZ* | a chave nova vai dizer qual; provável FCVTNS/FCVTMS/FCVTPS/FCVTAS: NS em RN com `cvtsd2si`, MS/PS com `roundsd` (SSE4.1) + `cvtt`, AS (empate longe do zero) → interp |
| `2ea0a400` ×11 | UMAXP/UMINP/SMAXP... | combinar as lanes de n e m com `pshufb` e aplicar `pmax*` |
| `5ee0b800`, `5e202800`, `5e200800` | ABS escalar e 2misc escalares | baixa prioridade |

A lista do Electron na VM (com mais tempo de execução) ainda vai ser mandada pelo RUNTIME. Quando chegar, reordenar.

## 3. Ainda pela chamada leve ao interpretador (corretos, sem aceleração)

- SHA1/SHA256/SHA512: SHA-NI divide a rodada de outro jeito (estado ABEF/CDGH); só emitir com
  prova de equivalência.
- SQSHL/UQSHL/SQRSHL/UQRSHL, SQDMULH/SQRDMULH de 32 bits e por elemento, SQDMLAL e afins.
- Estimativas FP (FRECPE/FRSQRTE/FRECPX/FRECPS/FRSQRTS): tabelas do ARM, o `rcpps` não reproduz.
- FP half: FMLA/FMLS (o produto não é half, então o teorema do arredondamento duplo não vale) e
  FCVT D→H (dois arredondamentos).
- FMAX/FMIN/FMAXNM/FMINNM, FRINT*, FABD, FP pareado além de FADDP.
- LDXP/STXP e CASP já são nativos; PRFM e hints já são NOP.

## 4. Desempenho

- `jit_bench_call` (fib) está em ~430 MIPS (4,4×): a transição entre blocos curtos domina.
  Próximo passo seria superblocos (seguir B/BL diretos dentro do mesmo bloco) e manter o cache de
  registradores através de saídas encadeadas.
- Medir de novo com a máquina livre (as outras sessões carregam a CPU) e atualizar a linha JIT do
  Estado em `docs/tradutor-arm64.md`.

## 5. Validação a repetir depois de cada mudança

- `cmp.sh` (todos os testes de `tools/a64tests`, interp × JIT) com o harness privado (`mk.sh`);
  o `build/a64test.exe` compartilhado pode estar sem `-Jit`.
- `a64fuzz --grupo all --casos 300 --k 8 --jit` com duas sementes novas, mais os grupos tocados com
  1000 casos e 3 sementes.
- Avisar o TESTES para refazer o placar com interp × JIT × QEMU.
