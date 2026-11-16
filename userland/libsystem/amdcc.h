/* mOS libSystem — compilador de kernels de compute do Metal (a árvore do mtlshader.c) para a ISA GCN do GFX9.
 *
 * Contrato com o despacho (amdws.c):
 *   s[0:1]  dado de usuário: endereço da tabela de argumentos (amdcc_args_t, alinhada a 256 bytes)
 *   s2..s4  id do grupo x, y, z (COMPUTE_PGM_RSRC2.TGID_X/Y/Z_EN)
 *   v0..v2  id da thread no grupo x, y, z (TIDIG_COMP_CNT = 2)
 * A tabela traz o endereço de cada [[buffer(n)]] e o tamanho do grupo e da grade.
 *
 * O que a primeira versão compila: parâmetros [[buffer(n)]] (ponteiro device/constant e referência constant),
 * thread_position_in_grid/threadgroup, threadgroup_position_in_grid, threads_per_threadgroup/grid; escalares e
 * vetores int/uint/float/bool; + - * / (float), & | ^ << >>, comparações, !, ~, -; casts; variáveis locais;
 * if/else, for e while com EXEC (divergência), return em qualquer ponto; loads e stores de 32 bits (e vetores) em
 * buffers, inclusive membros de struct; min/max/abs/clamp/sqrt/rsqrt/fma/mix/select/floor/ceil/trunc/fract/rint/
 * exp2/log2/dot; threadgroup_barrier. O resto (divisão inteira, break/continue, atomics, threadgroup memory,
 * texturas, tipos de 8/16 bits) devolve 0: o kernel roda no interpretador. */
#pragma once
#include "amdisa.h"

typedef struct msl_fn msl_fn_t;

#define AMDCC_MAX_BUF 31
typedef struct {
    unsigned long long buf[AMDCC_MAX_BUF + 1];   /* [[buffer(n)]]: endereço na GPU */
    unsigned local[4];                           /* threads por grupo x, y, z */
    unsigned grid[4];                            /* threads na grade x, y, z */
} amdcc_args_t;
#define AMDCC_ARGS_LOCAL 256u
#define AMDCC_ARGS_GRID 272u

typedef struct {
    amdisa_u32 *code;
    unsigned ncode;
    amdisa_u32 rsrc1, rsrc2;
    unsigned nuser;            /* dados de usuário (2: a tabela) */
    unsigned used_buf;         /* bit n = usa [[buffer(n)]] */
    char why[96];              /* por que não compilou (0 = compilou) */
} amdcc_kernel_t;

/* 1 = compilou; 0 = não compilável (k->why diz o motivo) */
int amdcc_compile(msl_fn_t *f, amdcc_kernel_t *k);
void amdcc_free(amdcc_kernel_t *k);
