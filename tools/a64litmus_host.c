/*
 * mOS — litmus do modelo de memória do tradutor arm64 no hospedeiro x86_64 (Linux).
 * Duas threads rodam código arm64 de verdade pelo motor (JIT ou interpretador) e marcham por
 * vetores de células em linhas de cache separadas (método "array" dos testes de litmus).
 *
 * SB (store buffering): A: x = 1; r0 = y   B: y = 1; r1 = x.  r0 = r1 = 0 é proibido com
 *   STLR + LDAR (RCsc), com STR; DMB ISH; LDR e com SWPAL; LDR. Sem barreira é permitido; no x86 a
 *   variante sem barreira mostra que o teste enxerga a reordenação store→load do hospedeiro.
 * MP (message passing): A: data = 1; STLR flag = 1   B: LDAR flag; LDR data. flag = 1, data = 0 é
 *   proibido.
 *
 *   clang -O2 -pthread -DA64_HAVE_JIT -Iuserland/a64 tools/a64litmus_host.c userland/a64/a64_fp.c \
 *     userland/a64/a64_xmon.c userland/a64/engine.c userland/a64/interp*.c userland/a64/jit*.c -lm \
 *     -o build/a64l/a64litmus && build/a64l/a64litmus [iterações]
 * Saída 0 = nenhum resultado proibido; imprime as contagens de cada teste.
 */
#define _GNU_SOURCE
#include "../userland/a64/a64.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

void *a64_host_jit_alloc(a64_u64 len) {
    void *p = mmap(NULL, (size_t)len, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
int a64_host_jit_wprotect(a64_u64 page, int protect) { (void)page; (void)protect; return 0; }
void a64_host_icache_inval(a64_cpu *c, a64_u64 addr, a64_u64 len) { (void)c; a64_invalidate(addr, len); }
void a64_host_yield(a64_cpu *c) { (void)c; }
a64_u64 a64_host_cntvct(void) { return 0; }

/* Programas (montados com llvm-mc de tools/a64litmus_host.c, comentário no fim). Cada iteração
 * sincroniza as duas threads: grava o próprio contador (x7) e espera o do outro (x8) chegar ao
 * mesmo número, com LDR/STR simples (não ordenam nada a mais no x86). Depois o corpo do teste.
 * SB: x0 = célula escrita, x1 = lida, x2 = resultados, x3 = iterações. MP: x0 = data, x1 = flag,
 * x2 = resultados (leitor). Células em passos de 64 bytes. */
static const uint32_t LIT[] = {0xd2800024, 0xd2800009, 0x91000529, 0xf90000e9, 0xf940010a, 0xeb09015f, 0x54ffffc3, 0xc89ffc04, 0xc8dffc25, 0xf9000045, 0x91010000, 0x91010021, 0x91002042, 0xf1000463, 0x54fffe81, 0xd4200000, 0xd2800024, 0xd2800009, 0x91000529, 0xf90000e9, 0xf940010a, 0xeb09015f, 0x54ffffc3, 0xf9000004, 0xd5033bbf, 0xf9400025, 0xf9000045, 0x91010000, 0x91010021, 0x91002042, 0xf1000463, 0x54fffe61, 0xd4200000, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd503201f, 0xd2800024, 0xd2800009, 0x91000529, 0xf90000e9, 0xf940010a, 0xeb09015f, 0x54ffffc3, 0xf8e48006, 0xf9400025, 0xf9000045, 0x91010000, 0x91010021, 0x91002042, 0xf1000463, 0x54fffe81, 0xd4200000, 0xd2800024, 0xd2800009, 0x91000529, 0xf90000e9, 0xf940010a, 0xeb09015f, 0x54ffffc3, 0xf9000004, 0xf9400025, 0xf9000045, 0x91010000, 0x91010021, 0x91002042, 0xf1000463, 0x54fffe81, 0xd4200000, 0xd2800024, 0xd2800009, 0x91000529, 0xf90000e9, 0xf940010a, 0xeb09015f, 0x54ffffc3, 0xf9000004, 0xc89ffc24, 0x91010000, 0x91010021, 0xf1000463, 0x54fffec1, 0xd4200000, 0xd503201f, 0xd503201f, 0xd2800009, 0x91000529, 0xf90000e9, 0xf940010a, 0xeb09015f, 0x54ffffc3, 0xc8dffc25, 0xf9400006, 0xa9001845, 0x91010000, 0x91010021, 0x91004042, 0xf1000463, 0x54fffe81, 0xd4200000};
enum { OFF_SB_REL = 0x00, OFF_SB_DMB = 0x40, OFF_SB_LSE = 0xC0, OFF_SB_NONE = 0x100, OFF_MP_W = 0x140, OFF_MP_R = 0x180 };

typedef struct { const uint32_t *code; a64_u64 x0, x1, x2, n, mine, other; int engine; } job;
static _Atomic int go, ready;

static void *run_thr(void *arg) {
    job *j = arg;
    a64_cpu *c = calloc(1, sizeof *c);
    c->pc = (a64_u64)(uintptr_t)j->code;
    c->x[0] = j->x0; c->x[1] = j->x1; c->x[2] = j->x2; c->x[3] = j->n; c->x[7] = j->mine; c->x[8] = j->other;
    atomic_fetch_add(&ready, 1);
    while (!atomic_load(&go)) { }
    int r = a64_run(c, j->engine);
    if (r != A64_EXIT_BRK) { fprintf(stderr, "a64litmus: saída %d em pc %llx\n", r, (unsigned long long)c->pc); exit(2); }
#ifdef A64_HAVE_JIT
    a64_jit_thread_exit(c);
#endif
    free(c);
    return NULL;
}

static uint32_t *g_code;
static a64_u64 *g_ctr;                  /* dois contadores, em linhas de cache diferentes */
static const uint32_t *at(int off) { return g_code + off / 4; }

static void pair(job *a, job *b) {
    pthread_t ta, tb;
    atomic_store(&go, 0); atomic_store(&ready, 0);
    pthread_create(&ta, NULL, run_thr, a);
    pthread_create(&tb, NULL, run_thr, b);
    while (atomic_load(&ready) < 2) { }
    atomic_store(&go, 1);
    pthread_join(ta, NULL); pthread_join(tb, NULL);
}

/* devolve o número de iterações com r0 = r1 = 0 */
static long sb(int off, long n, int rounds, int engine) {
    const uint32_t *ca = at(off), *cb = at(off);
    size_t cells = (size_t)n * 64;
    a64_u64 *X = mmap(NULL, cells, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    a64_u64 *Y = mmap(NULL, cells, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    a64_u64 *RA = calloc((size_t)n, 8), *RB = calloc((size_t)n, 8);
    long both0 = 0;
    for (int k = 0; k < rounds; k++) {
        memset(X, 0, cells); memset(Y, 0, cells); g_ctr[0] = g_ctr[8] = 0;
        a64_u64 c0 = (a64_u64)(uintptr_t)&g_ctr[0], c1 = (a64_u64)(uintptr_t)&g_ctr[8];
        job a = { ca, (a64_u64)(uintptr_t)X, (a64_u64)(uintptr_t)Y, (a64_u64)(uintptr_t)RA, (a64_u64)n, c0, c1, engine };
        job b = { cb, (a64_u64)(uintptr_t)Y, (a64_u64)(uintptr_t)X, (a64_u64)(uintptr_t)RB, (a64_u64)n, c1, c0, engine };
        pair(&a, &b);
        for (long i = 0; i < n; i++) both0 += RA[i] == 0 && RB[i] == 0;
    }
    munmap(X, cells); munmap(Y, cells); free(RA); free(RB);
    return both0;
}

static long mp(long n, int rounds, int engine) {
    const uint32_t *cw = at(OFF_MP_W), *cr = at(OFF_MP_R);
    size_t cells = (size_t)n * 64;
    a64_u64 *D = mmap(NULL, cells, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    a64_u64 *F = mmap(NULL, cells, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    a64_u64 *R = calloc((size_t)n * 2, 8);
    long bad = 0;
    for (int k = 0; k < rounds; k++) {
        memset(D, 0, cells); memset(F, 0, cells); g_ctr[0] = g_ctr[8] = 0;
        a64_u64 c0 = (a64_u64)(uintptr_t)&g_ctr[0], c1 = (a64_u64)(uintptr_t)&g_ctr[8];
        job a = { cw, (a64_u64)(uintptr_t)D, (a64_u64)(uintptr_t)F, 0, (a64_u64)n, c0, c1, engine };
        job b = { cr, (a64_u64)(uintptr_t)D, (a64_u64)(uintptr_t)F, (a64_u64)(uintptr_t)R, (a64_u64)n, c1, c0, engine };
        pair(&a, &b);
        for (long i = 0; i < n; i++) bad += R[2 * i] == 1 && R[2 * i + 1] == 0;
    }
    munmap(D, cells); munmap(F, cells); free(R);
    return bad;
}

int main(int argc, char **argv) {
    long n = argc > 1 ? atol(argv[1]) : 200000;
    int rounds = 5, fails = 0;
    g_code = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    memcpy(g_code, LIT, sizeof LIT);
    g_ctr = aligned_alloc(64, 128);
    static const struct { const char *nome; int engine; } eng[] = {
#ifdef A64_HAVE_JIT
        { "jit", A64_ENGINE_JIT },
#endif
        { "interp", A64_ENGINE_INTERP } };
    for (unsigned e = 0; e < sizeof eng / sizeof eng[0]; e++) {
        int en = eng[e].engine;
        long r1 = sb(OFF_SB_REL, n, rounds, en);
        long r2 = sb(OFF_SB_DMB, n, rounds, en);
        long r3 = sb(OFF_SB_LSE, n, rounds, en);
        long r4 = sb(OFF_SB_NONE, n, rounds, en);
        long r5 = mp(n, rounds, en);
        printf("%-6s SB stlr/ldar %ld | SB dmb ish %ld | SB swpal %ld | SB sem barreira (permitido) %ld | MP stlr/ldar %ld  (de %ld)\n",
               eng[e].nome, r1, r2, r3, r4, r5, n * rounds);
        fails += r1 != 0 || r2 != 0 || r3 != 0 || r5 != 0;
    }
    printf(fails ? "a64litmus: RESULTADO PROIBIDO\n" : "a64litmus: ok\n");
    return fails ? 1 : 0;
}

/* Fonte dos programas (llvm-mc -triple=aarch64 -mattr=+lse,+rcpc; .p2align 6 entre eles):
 * sb_X:  mov x4,#1; mov x9,#0
 *     1: add x9,x9,#1; str x9,[x7]
 *     2: ldr x10,[x8]; cmp x10,x9; b.lo 2b
 *        <corpo>; str x5,[x2]; add x0,x0,#64; add x1,x1,#64; add x2,x2,#8; subs x3,x3,#1; b.ne 1b; brk #0
 *   rel: stlr x4,[x0]; ldar x5,[x1]        dmb: str x4,[x0]; dmb ish; ldr x5,[x1]
 *   lse: swpal x4,x6,[x0]; ldr x5,[x1]     none: str x4,[x0]; ldr x5,[x1]
 * mp_w: (mesma sincronização) str x4,[x0]; stlr x4,[x1]; avança x0/x1 64
 * mp_r: (mesma sincronização) ldar x5,[x1]; ldr x6,[x0]; stp x5,x6,[x2]; avança x0/x1 64, x2 16 */
