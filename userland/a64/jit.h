/*
 * mOS — tradutor arm64 (frente JIT): estruturas internas compartilhadas entre jit_core.c
 * (cache, despachante, threads, invalidação, recuperação) e jit_tr*.c (tradução A64 → x86_64).
 * Desenho e convenções: docs/tradutor-arm64.md, seção "JIT".
 */
#pragma once
#include "a64.h"
#include "jit_x86.h"

#define JIT_MAX_INSNS 64            /* instruções guest por bloco */
#define JIT_JC_BITS   12            /* cache de saltos indiretos por thread: 4096 entradas */
#define JIT_PAGE      4096ull

/* códigos devolvidos pelo código gerado ao despachante (além de A64_EXIT_*) */
enum {
    JX_LINK = 100,                  /* saída direta não encadeada: c->pc = alvo, t->link_site = campo rel32 */
    JX_IND  = 101,                  /* salto indireto sem acerto no cache: c->pc = alvo */
    JX_STALE = 102,                 /* bloco de página volátil viu o código guest mudar: c->pc = início */
    JX_UNWIND = 103,                /* a64_jit_unwind: código em t->unwind_code */
};

/* offsets no a64_cpu usados pelo código gerado */
#define OFF_X(r)    ((int32_t)(8 * (r)))
#define OFF_PC      ((int32_t)offsetof(a64_cpu, pc))
#define OFF_V(r)    ((int32_t)(offsetof(a64_cpu, v) + 16 * (r)))
#define OFF_STOP    ((int32_t)offsetof(a64_cpu, reserved[15]))
#define OFF_FLAGS   ((int32_t)offsetof(a64_cpu, reserved[14]))   /* NZCV em formato x86: AH=lahf, AL=OF */
#define OFF_JC      ((int32_t)offsetof(a64_cpu, reserved[13]))
#define OFF_JIT     ((int32_t)offsetof(a64_cpu, jit))
#define RCPU X15                    /* registrador fixo do host com o a64_cpu* */

/* conversões NZCV (ARM) ↔ formato x86 guardado em reserved[14] */
static inline a64_u16 jit_nzcv_to_x86(a64_u32 n) {
    unsigned ah = ((n >> 31) & 1) << 7 | ((n >> 30) & 1) << 6 | 2 | ((n >> 29) & 1);
    return (a64_u16)(ah << 8 | ((n >> 28) & 1));
}
static inline a64_u32 jit_x86_to_nzcv(a64_u16 f) {
    return ((a64_u32)(f >> 15) & 1) << 31 | ((a64_u32)(f >> 14) & 1) << 30 | ((a64_u32)(f >> 8) & 1) << 29 | ((a64_u32)f & 1) << 28;
}
static inline a64_u32 jit_rflags_to_nzcv(a64_u64 rf, int sub) {
    return (a64_u32)((rf >> 7) & 1) << 31 | (a64_u32)((rf >> 6) & 1) << 30 |
           (a64_u32)(((rf & 1) ^ (sub ? 1 : 0)) & 1) << 29 | (a64_u32)((rf >> 11) & 1) << 28;
}
#define JIT_F(c) (*(a64_u16 *)&(c)->reserved[14])

/* ponto de falha: instrução x86 que acessa memória do guest */
typedef struct jfp {
    a64_u32 off;                    /* início da instrução x86, relativo a jblk.code */
    a64_u16 idx;                    /* índice da instrução guest no bloco */
    a64_u8  fl;                     /* bits 1:0 = flags em EFLAGS (0 não, 1 modo ADD, 2 modo SUB); bit 2 = reserved[14] válido */
    a64_u8  pad;
    a64_u8  map[16];                /* registrador do host → registrador guest sujo nele (0xFF = nenhum) */
} jfp;

struct jblk;
typedef struct jlink { a64_u8 *site; struct jlink *next; } jlink;

typedef struct jblk {
    a64_u64 pc, end;                /* intervalo guest [pc, end) */
    a64_u8 *code;
    a64_u32 len, nfp;
    jfp *fp;
    jlink *in;                      /* saídas de outros blocos encadeadas para este */
    struct jblk *hnext;             /* hash pc → bloco */
    struct jblk *pgnext[2];         /* lista de blocos por página guest */
    a64_u64 pg[2];
    int npg, dead;
} jblk;

typedef struct jc_ent { a64_u64 pc; void *code; } jc_ent;

typedef struct jthr {
    jc_ent jc[1 << JIT_JC_BITS];
    a64_u64 saved_rsp;              /* rsp do código gerado (para a64_jit_unwind) */
    a64_u64 link_site;
    a64_cpu *cpu;
    struct jthr *next;
    volatile int in_jit;            /* 1 enquanto pode estar executando código traduzido */
    int unwind_code;
    int live;
    int in_run;                     /* dentro de a64_jit_run: flags vivas em reserved[14] */
} jthr;
#define OFF_THR_SAVED_RSP ((int32_t)offsetof(jthr, saved_rsp))
#define OFF_THR_LINK      ((int32_t)offsetof(jthr, link_site))

/* serviços do núcleo para o tradutor */
typedef struct jit_globals {
    a64_u8 *g_exit, *g_link, *g_ind, *g_ind_pc, *g_hexit;   /* stubs comuns */
    void **helpers;                 /* tabela de ponteiros chamada via call [rip+x] */
    int nonative;                   /* depuração: tudo pelo interpretador */
    int maxinsns;                   /* depuração: instruções por bloco (0 = JIT_MAX_INSNS) */
    int nowp;                       /* depuração: sem proteção de escrita */
    int fblog;                      /* depuração: lista instruções que vão ao interpretador */
    int ssse3, sse41, sse42;        /* recursos do host (CPUID) */
} jit_globals;
extern jit_globals jit_g;
enum { JH_INTERP = 0, JH_COUNT = 16 };

/* aloca no arena de dados (bloqueio já tomado); NULL = cheio */
void *jit_data_alloc(a64_u64 n);
/* traduz o bloco que começa em pc (bloqueio tomado). NULL = sem espaço (o núcleo esvazia e repete). */
jblk *jit_translate(a64_u64 pc, int selfcheck);
/* espaço de código: [*p, lim) */
void jit_code_space(a64_u8 **p, a64_u8 **lim);
void jit_code_commit(a64_u8 *end);
/* helper do fallback (chamado pelo código gerado) */
int jit_h_interp(a64_cpu *c, a64_u32 insn);
