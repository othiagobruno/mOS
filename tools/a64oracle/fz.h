/*
 * mOS — verificação do tradutor arm64: definições comuns ao harness (tools/a64test.c), ao oráculo
 * bare-metal no QEMU (tools/a64oracle/mon.c) e ao fuzzer (tools/a64fuzz.c).
 * C11 freestanding (só <stdint.h>): compila para x86_64 (Windows) e aarch64 (monitor).
 *
 * Tudo o que aparece no estado final de um teste (endereços em registradores, sp, argv) é
 * determinístico: os três lados usam os mesmos endereços virtuais fixos abaixo.
 */
#pragma once
#include <stdint.h>

/* ----- layout fixo do processo de teste (modo ELF) ----- */
#define FZ_STACK_VA     0x2F0000000ull           /* pilha de 8 MiB: [FZ_STACK_VA, FZ_STACK_VA+FZ_STACK_SZ) */
#define FZ_STACK_SZ     (8ull << 20)
#define FZ_IMG_LO       0x200000000ull           /* faixa permitida para PT_LOAD dos testes */
#define FZ_IMG_HI       0x2E0000000ull
/* sp e argv: mesma aritmética no a64test e no monitor (ver fz_stack_layout) */

/* ----- fuzzing: página de código e memória de rascunho ----- */
#define FZ_CODE_VA      0x2E0000000ull           /* 64 KiB preenchidos com FZ_TRAP_INSN */
#define FZ_CODE_SZ      0x10000ull
#define FZ_CASE_OFF     0x8000ull                /* as instruções do caso começam aqui */
#define FZ_SCR_VA       0x2E0100000ull           /* rascunho: 16 KiB, o resto do bloco de 64 KiB não é mapeado */
#define FZ_SCR_SZ       0x4000ull
#define FZ_TRAP_INSN    0xD4214C80u              /* brk #0xA64: alvo de desvio que caiu fora do caso */
#define FZ_END_INSN     0xD4080021u              /* svc #0x4001: fim do caso (e "dump" no modo ELF) */
#define FZ_EXIT0_INSN   0xD4080041u              /* svc #0x4002: exit(0) sem tocar registradores */
#define FZ_MAXINSN      16

/* syscalls do harness (svc #0x80 com x16): 1 exit, 4 write, 20 getpid, 0x4000 dump.
 * Pelo imediato (sem usar x16, para não sujar o estado): svc #0x4001 = dump, svc #0x4002 = exit(0). */
#define FZ_SVC_DUMP     0x4001
#define FZ_SVC_EXIT0    0x4002

/* ----- lote de casos (arquivo binário, little-endian) ----- */
#define FZ_BATCH_MAGIC  0x3130425A46343641ull    /* "A64FZB01" */
typedef struct fz_batch_hdr {
    uint64_t magic;
    uint32_t ncases;
    uint32_t flags;
    uint32_t case_size;                          /* sizeof(fz_case), conferido nos dois lados */
    uint32_t res_size;                           /* sizeof(fz_result) */
    uint64_t budget;                             /* interpretador: máx. de instruções por caso */
    char     outpath[224];                       /* QEMU: arquivo de saída (semihosting) */
} fz_batch_hdr;                                  /* 256 bytes */

typedef struct fz_state {
    uint64_t x[31];
    uint64_t sp;
    uint32_t nzcv, fpcr, fpsr, pstate;           /* pstate: só DIT (bit 24) e SSBS (bit 12) */
    uint64_t tpidr;                              /* TPIDR_EL0 (TPIDRRO_EL0 é sempre 0) */
    uint64_t pad;
    uint8_t  v[32][16];
} fz_state;                                      /* 800 bytes */

typedef struct fz_case {
    fz_state st;
    uint64_t scr_seed;                           /* conteúdo inicial do rascunho = fz_fill(seed) */
    uint32_t ninsn, pad;
    uint32_t insn[FZ_MAXINSN];
} fz_case;                                       /* 880 bytes */

enum { FZK_END = 0, FZK_SVC = 1, FZK_BRK = 2, FZK_UDF = 3, FZK_FAULT = 4, FZK_BUDGET = 5,
       FZK_HLT = 6, FZK_OTHER = 7 };

typedef struct fz_result {
    uint32_t kind;
    uint32_t insn;                               /* palavra da instrução que causou a saída */
    uint64_t pc;                                 /* SVC: instrução seguinte; resto: a própria */
    uint64_t info;                               /* SVC/BRK/HLT: imm16; FAULT: endereço; OTHER: ESR */
    uint64_t scr_hash;                           /* FNV-1a 64 do rascunho inteiro no fim */
    fz_state st;
} fz_result;                                     /* 832 bytes */

_Static_assert(sizeof(fz_batch_hdr) == 256, "fz_batch_hdr");
_Static_assert(sizeof(fz_state) == 800, "fz_state");
_Static_assert(sizeof(fz_case) == 880, "fz_case");
_Static_assert(sizeof(fz_result) == 832, "fz_result");

/* conteúdo determinístico do rascunho */
static inline uint64_t fz_rng(uint64_t *s) {     /* xorshift64* */
    uint64_t x = *s; x ^= x >> 12; x ^= x << 25; x ^= x >> 27; *s = x; return x * 0x2545F4914F6CDD1Dull;
}
static inline void fz_fill(volatile uint64_t *p, uint64_t seed) {
    uint64_t s = seed | 1;
    for (unsigned i = 0; i < FZ_SCR_SZ / 8; i++) p[i] = fz_rng(&s);
}
static inline uint64_t fz_hash(const volatile uint8_t *p, uint64_t n) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint64_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ull; }
    return h;
}

/* pilha inicial do modo ELF: devolve sp; grava argv (argc ponteiros + NULL) e as strings */
static inline uint64_t fz_stack_layout(uint64_t stk, int argc, const char *const *argv,
                                       uint64_t *argv_out) {
    uint64_t sp = stk + FZ_STACK_SZ - 4096;
    uint64_t *gargv = (uint64_t *)(uintptr_t)(sp - 8 * (uint64_t)(argc + 1) - 64);
    char *strs = (char *)(uintptr_t)(stk + 4096);
    for (int i = 0; i < argc; i++) {
        const char *s = argv[i];
        gargv[i] = (uint64_t)(uintptr_t)strs;
        do { *strs++ = *s; } while (*s++);
    }
    gargv[argc] = 0;
    *argv_out = (uint64_t)(uintptr_t)gargv;
    return ((uint64_t)(uintptr_t)gargv - 64) & ~15ull;
}
