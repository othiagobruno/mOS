/*
 * Estresse do monitor exclusivo, das barreiras e do LSE no hospedeiro x86_64.
 * Não precisa do kernel do mOS: o interpretador e o JIT rodam direto.
 *
 *   clang -O2 -pthread -I userland/a64 -I userland/a64rt \
 *     tools/a64xmon_host.c userland/a64/a64_xmon.c userland/a64/interp.c ... -o /tmp/a64xmon
 */
#define _GNU_SOURCE
#include "a64.h"
#include "a64_xmon.h"
#include "a64rt_abi.h"

#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

static int g_fail;

static void fail(const char *what) {
    fprintf(stderr, "FALHA: %s\n", what);
    g_fail++;
}

static void on_alarm(int sig) {
    (void)sig;
    fprintf(stderr, "FALHA: tempo esgotado (livelock ou espera sem fim)\n");
    _exit(2);
}

a64_u64 a64_host_cntvct(void) { return 0; }
void a64_host_yield(a64_cpu *c) { (void)c; sched_yield(); }

void *a64_host_jit_alloc(a64_u64 len) {
    void *p = mmap(0, (size_t)len, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? 0 : p;
}
int a64_host_jit_wprotect(a64_u64 page, int protect) {
    (void)page;
    (void)protect;
    return 0;
}

static int step(a64_cpu *c, a64_u32 insn) {
    int r = a64_interp_exec(c, insn);
    if (r == A64_NEXT) c->pc += 4;
    return r;
}

enum {
    I_LDXR = 0xc85f7c20,   /* ldxr x0, [x1] */
    I_STXR = 0xc8027c23,   /* stxr w2, x3, [x1] */
    I_STR  = 0xf9000023,   /* str  x3, [x1] */
    I_STRW = 0xb9000023,   /* str  w3, [x1] */
    I_CLREX = 0xd5033f5f,
    I_DMB_SY = 0xd5033fbf,
    I_DMB_ST = 0xd5033ebf,
    I_DMB_LD = 0xd5033dbf,
    I_DSB_SY = 0xd5033f9f,
    I_DSB_ST = 0xd5033e9f,
    I_ISB = 0xd5033fdf,
    I_SB = 0xd50330ff,
    I_STLR = 0xc89ffc01,   /* stlr x1, [x0] */
    I_LDAR = 0xc8dffc02,   /* ldar x2, [x0] */
    I_STLLR = 0xc89f7c01,  /* stllr x1, [x0] */
    I_LDAPR = 0xf8bfc002,  /* ldapr x2, [x0] */
    I_LDXP = 0xc87f0440,   /* ldxp x0, x1, [x2] */
    I_STXP = 0xc8231444,   /* stxp w3, x4, x5, [x2] */
    I_STP  = 0xa9001023,   /* stp x3, x4, [x1] */
    I_LDADD = 0xf8210002,  /* ldadd x1, x2, [x0] */
    I_SWP = 0xf8218002,    /* swp x1, x2, [x0] */
    I_CAS = 0xc8a17c02,    /* cas x1, x2, [x0] */
    I_LDXR_X2 = 0xc85f7c02, /* ldxr x2, [x0] */
    I_ADD1 = 0x91000442,   /* add x2, x2, #1 */
    I_STXR_W3 = 0xc8037c02, /* stxr w3, x2, [x0] */
    I_CBNZ = 0x35ffffa3,   /* cbnz w3, .-12 */
    I_SUBS = 0xf1000421,   /* subs x1, x1, #1 */
    I_BNE = 0x54ffff61,    /* b.ne .-20 */
    I_BRK = 0xd4200000,
    I_STR_X1 = 0xf9000001  /* str x1, [x0] */
};

static void cpu_clear(a64_cpu *c) { memset(c, 0, sizeof *c); }

static void test_same_thread(void) {
    _Alignas(16) a64_u64 cell[4];
    a64_cpu c;
    cpu_clear(&c);
    cell[0] = 0x1111111111111111ull;
    c.x[1] = (a64_u64)(uintptr_t)&cell[0];
    c.x[3] = cell[0];
    if (step(&c, I_LDXR) != A64_NEXT || c.x[0] != cell[0]) fail("ldxr valor");
    if (step(&c, I_STR) != A64_NEXT) fail("str mesmo valor");
    c.x[2] = 0;
    c.x[3] = 0x99;
    if (step(&c, I_STXR) != A64_NEXT || c.x[2] != 1 || cell[0] != 0x1111111111111111ull)
        fail("aba do mesmo valor na mesma thread");

    cpu_clear(&c);
    c.x[1] = (a64_u64)(uintptr_t)&cell[0];
    if (step(&c, I_LDXR) != A64_NEXT) fail("ldxr 2");
    c.x[3] = 0x55;
    c.x[1] = (a64_u64)(uintptr_t)&cell[1];
    if (step(&c, I_STR) != A64_NEXT) fail("str ao lado");
    c.x[1] = (a64_u64)(uintptr_t)&cell[0];
    c.x[2] = 0;
    c.x[3] = 0x99;
    if (step(&c, I_STXR) != A64_NEXT || c.x[2] != 0 || cell[0] != 0x99)
        fail("store em outro endereço derrubou o monitor");

    cell[0] = 0x1111111111111111ull;
    cpu_clear(&c);
    c.x[1] = (a64_u64)(uintptr_t)&cell[0];
    if (step(&c, I_LDXR) != A64_NEXT) fail("ldxr 3");
    c.x[3] = 0x11111111;
    c.x[1] = (a64_u64)(uintptr_t)&cell[0] + 4;
    if (step(&c, I_STRW) != A64_NEXT) fail("strw sobreposto");
    c.x[1] = (a64_u64)(uintptr_t)&cell[0];
    c.x[2] = 0;
    c.x[3] = 0x99;
    if (step(&c, I_STXR) != A64_NEXT || c.x[2] != 1) fail("sobreposição parcial não derrubou");

    cpu_clear(&c);
    c.x[1] = (a64_u64)(uintptr_t)&cell[0];
    if (step(&c, I_LDXR) != A64_NEXT) fail("ldxr 4");
    if (step(&c, I_CLREX) != A64_NEXT) fail("clrex");
    c.x[2] = 0;
    c.x[3] = 1;
    if (step(&c, I_STXR) != A64_NEXT || c.x[2] != 1) fail("stxr depois de clrex");

    cpu_clear(&c);
    c.x[2] = 0;
    c.x[1] = (a64_u64)(uintptr_t)&cell[0];
    c.x[3] = 7;
    if (step(&c, I_STXR) != A64_NEXT || c.x[2] != 1) fail("stxr sem ldxr");
}

static void test_pair(void) {
    _Alignas(16) a64_u64 cell[2];
    a64_cpu c;
    cpu_clear(&c);
    cell[0] = 0x11;
    cell[1] = 0x22;
    c.x[2] = (a64_u64)(uintptr_t)cell;
    if (step(&c, I_LDXP) != A64_NEXT || c.x[0] != 0x11 || c.x[1] != 0x22) fail("ldxp");
    c.x[1] = (a64_u64)(uintptr_t)cell;
    c.x[3] = 0x11;
    c.x[4] = 0x22;
    if (step(&c, I_STP) != A64_NEXT) fail("stp");
    c.x[2] = (a64_u64)(uintptr_t)cell;
    c.x[3] = 0;
    c.x[4] = 1;
    c.x[5] = 2;
    if (step(&c, I_STXP) != A64_NEXT || c.x[3] != 1 || cell[0] != 0x11) fail("stxp aba");
}

static void test_invalidate(void) {
    _Alignas(16) a64_u64 cell = 1;
    a64_cpu c;
    cpu_clear(&c);
    c.x[1] = (a64_u64)(uintptr_t)&cell;
    if (step(&c, I_LDXR) != A64_NEXT) fail("ldxr inv");
    a64_xmon_invalidate(&c);
    c.x[2] = 0;
    c.x[3] = 2;
    if (step(&c, I_STXR) != A64_NEXT || c.x[2] != 1 || cell != 1) fail("invalidate não derrubou");
}

static void test_lse_values(void) {
    _Alignas(16) a64_u64 cell = 10;
    a64_cpu c;
    cpu_clear(&c);
    c.x[0] = (a64_u64)(uintptr_t)&cell;
    c.x[1] = 5;
    if (step(&c, I_LDADD) != A64_NEXT || c.x[2] != 10 || cell != 15) fail("ldadd");
    c.x[1] = 15;
    c.x[2] = 40;
    if (step(&c, I_CAS) != A64_NEXT || cell != 40) fail("cas troca");
    c.x[1] = 1;
    c.x[2] = 9;
    if (step(&c, I_CAS) != A64_NEXT || cell != 40 || c.x[1] != 40) fail("cas não troca");
    c.x[1] = 7;
    if (step(&c, I_SWP) != A64_NEXT || cell != 7 || c.x[2] != 40) fail("swp");
}

static void test_barriers_decode(void) {
    a64_cpu c;
    cpu_clear(&c);
    a64_u32 ops[] = { I_DMB_SY, I_DMB_ST, I_DMB_LD, I_DSB_SY, I_DSB_ST, I_ISB, I_SB };
    for (unsigned i = 0; i < sizeof ops / sizeof ops[0]; i++)
        if (step(&c, ops[i]) != A64_NEXT) fail("barreira indefinida");
}

static _Atomic int phase;
static _Atomic int aba_status;
static a64_u64 aba_cell;

static void *aba_peer(void *arg) {
    (void)arg;
    while (atomic_load(&phase) == 0) sched_yield();
    a64_cpu c;
    cpu_clear(&c);
    c.x[1] = (a64_u64)(uintptr_t)&aba_cell;
    c.x[3] = 0x3333;
    step(&c, I_STR);
    c.x[3] = 0x1111;
    step(&c, I_STR);
    atomic_store(&phase, 2);
    return 0;
}

static void test_aba_threads(void) {
    aba_cell = 0x1111;
    atomic_store(&phase, 0);
    pthread_t th;
    if (pthread_create(&th, 0, aba_peer, 0) != 0) fail("pthread aba");
    a64_cpu c;
    cpu_clear(&c);
    c.x[1] = (a64_u64)(uintptr_t)&aba_cell;
    if (step(&c, I_LDXR) != A64_NEXT) fail("ldxr aba");
    atomic_store(&phase, 1);
    while (atomic_load(&phase) != 2) sched_yield();
    c.x[2] = 0;
    c.x[3] = 0x2222;
    if (step(&c, I_STXR) != A64_NEXT) fail("stxr aba");
    aba_status = (int)c.x[2];
    pthread_join(th, 0);
    if (aba_status != 1 || aba_cell != 0x1111) fail("aba entre threads");
}

enum { NTHR = 4, NITER = 4000 };

static void *counter_thr(void *arg) {
    a64_u64 *cell = arg;
    a64_cpu c;
    cpu_clear(&c);
    c.x[0] = (a64_u64)(uintptr_t)cell;
    for (int i = 0; i < NITER; i++) {
        for (;;) {
            if (step(&c, I_LDXR_X2) != A64_NEXT) { fail("ldxr contador"); return 0; }
            c.x[2]++;
            c.x[3] = 0;
            if (step(&c, I_STXR_W3) != A64_NEXT) { fail("stxr contador"); return 0; }
            if (c.x[3] == 0) break;
        }
    }
    return 0;
}

static void test_counter(void) {
    a64_u64 cell = 0;
    pthread_t th[NTHR];
    for (int i = 0; i < NTHR; i++) pthread_create(&th[i], 0, counter_thr, &cell);
    for (int i = 0; i < NTHR; i++) pthread_join(th[i], 0);
    if (cell != (a64_u64)NTHR * NITER) fail("contador exclusivo perdeu incremento");
}

static a64_u64 lock_word, crit;

static void *lock_thr(void *arg) {
    (void)arg;
    a64_cpu c;
    cpu_clear(&c);
    c.x[0] = (a64_u64)(uintptr_t)&lock_word;
    for (int i = 0; i < NITER; i++) {
        for (;;) {
            if (step(&c, I_LDXR_X2) != A64_NEXT) { fail("ldxr trava"); return 0; }
            if (c.x[2] != 0) {
                step(&c, I_CLREX);
                sched_yield();
                continue;
            }
            c.x[2] = 1;
            c.x[3] = 0;
            if (step(&c, I_STXR_W3) != A64_NEXT) { fail("stxr trava"); return 0; }
            if (c.x[3] == 0) break;
        }
        crit++;
        c.x[1] = (a64_u64)(uintptr_t)&lock_word;
        c.x[3] = 0;
        if (step(&c, I_STR) != A64_NEXT) fail("unlock");
        c.x[0] = (a64_u64)(uintptr_t)&lock_word;
    }
    return 0;
}

static void test_lock(void) {
    lock_word = 0;
    crit = 0;
    pthread_t th[NTHR];
    for (int i = 0; i < NTHR; i++) pthread_create(&th[i], 0, lock_thr, 0);
    for (int i = 0; i < NTHR; i++) pthread_join(th[i], 0);
    if (crit != (a64_u64)NTHR * NITER || lock_word != 0) fail("spinlock exclusivo");
}

static void *add_thr(void *arg) {
    a64_u64 *cell = arg;
    a64_cpu c;
    cpu_clear(&c);
    c.x[0] = (a64_u64)(uintptr_t)cell;
    c.x[1] = 1;
    for (int i = 0; i < NITER; i++)
        if (step(&c, I_LDADD) != A64_NEXT) { fail("ldadd mt"); return 0; }
    return 0;
}

static void test_ldadd_mt(void) {
    a64_u64 cell = 0;
    pthread_t th[NTHR];
    for (int i = 0; i < NTHR; i++) pthread_create(&th[i], 0, add_thr, &cell);
    for (int i = 0; i < NTHR; i++) pthread_join(th[i], 0);
    if (cell != (a64_u64)NTHR * NITER) fail("ldadd multithread");
}

static a64_u64 mp_data, mp_flag;

static void *mp_writer(void *arg) {
    (void)arg;
    while (atomic_load(&phase) == 0) sched_yield();
    a64_cpu c;
    cpu_clear(&c);
    c.x[0] = (a64_u64)(uintptr_t)&mp_data;
    c.x[1] = 0xabcd;
    step(&c, I_STR_X1);
    c.x[0] = (a64_u64)(uintptr_t)&mp_flag;
    c.x[1] = 1;
    step(&c, I_STLR);
    return 0;
}

static void test_message(void) {
    mp_data = 0;
    mp_flag = 0;
    atomic_store(&phase, 0);
    pthread_t th;
    pthread_create(&th, 0, mp_writer, 0);
    atomic_store(&phase, 1);
    a64_cpu c;
    cpu_clear(&c);
    c.x[0] = (a64_u64)(uintptr_t)&mp_flag;
    for (;;) {
        if (step(&c, I_LDAR) != A64_NEXT) { fail("ldar"); break; }
        if (c.x[2] == 1) break;
    }
    c.x[0] = (a64_u64)(uintptr_t)&mp_data;
    if (step(&c, 0xf9400002) != A64_NEXT || c.x[2] != 0xabcd) fail("mensagem stlr/ldar");
    pthread_join(th, 0);
}

/* 0xf9400002 = ldr x2, [x0] */

typedef struct {
    int id;
    int mode; /* 0 STLR/LDAR, 1 STLLR, 2 DMB SY, 3 DMB ST */
    _Atomic unsigned *epoch;
    _Atomic unsigned *fin;
    _Atomic int *result;
    _Atomic int *stop;
    a64_u64 *mine;
    a64_u64 *other;
} sb_arg;

static void *sb_thr(void *arg) {
    sb_arg *a = arg;
    unsigned seen = 0;
    a64_cpu c;
    cpu_clear(&c);
    while (!atomic_load(a->stop)) {
        unsigned e = atomic_load(a->epoch);
        if (e == seen) { sched_yield(); continue; }
        seen = e;
        c.x[0] = (a64_u64)(uintptr_t)a->mine;
        c.x[1] = 1;
        if (a->mode == 0) step(&c, I_STLR);
        else if (a->mode == 1) step(&c, I_STLLR);
        else {
            step(&c, I_STR_X1);
            step(&c, a->mode == 2 ? I_DMB_SY : I_DMB_ST);
        }
        c.x[0] = (a64_u64)(uintptr_t)a->other;
        step(&c, a->mode <= 1 ? I_LDAR : 0xf9400002);
        atomic_store(&a->result[a->id], (int)c.x[2]);
        atomic_store(&a->fin[a->id], e);
    }
    return 0;
}

static int sb_run(int mode, int n, int *relaxed) {
    _Alignas(64) static a64_u64 x, y;
    _Atomic unsigned epoch = 0, fin[2];
    _Atomic int result[2], stop = 0;
    fin[0] = fin[1] = 0;
    sb_arg a0 = { 0, mode, &epoch, fin, result, &stop, &x, &y };
    sb_arg a1 = { 1, mode, &epoch, fin, result, &stop, &y, &x };
    pthread_t t0, t1;
    pthread_create(&t0, 0, sb_thr, &a0);
    pthread_create(&t1, 0, sb_thr, &a1);
    int bad = 0;
    for (int i = 1; i <= n; i++) {
        x = y = 0;
        atomic_thread_fence(memory_order_seq_cst);
        atomic_store(&fin[0], 0);
        atomic_store(&fin[1], 0);
        atomic_store(&epoch, (unsigned)i);
        while (atomic_load(&fin[0]) != (unsigned)i || atomic_load(&fin[1]) != (unsigned)i)
            sched_yield();
        if (atomic_load(&result[0]) == 0 && atomic_load(&result[1]) == 0) bad++;
    }
    atomic_store(&stop, 1);
    atomic_fetch_add(&epoch, 1);
    pthread_join(t0, 0);
    pthread_join(t1, 0);
    if (relaxed) *relaxed = bad;
    return bad;
}

static void test_sysctl(void) {
#define X(n, v) + (!strcmp(n, "hw.optional.armv8_1_atomics") ? (v) : 0)
    int atomics = 0 A64RT_SYSCTL_INTS(X);
#undef X
#define X(n, v) + (!strcmp(n, "hw.optional.arm.FEAT_LSE128") ? (v) + 1 : 0)
    int lse128 = 0 A64RT_SYSCTL_INTS(X);
#undef X
    if (atomics != 1) fail("hw.optional.armv8_1_atomics");
    if (lse128 != 1) fail("FEAT_LSE128 anunciado");
#define F(f) + (!strcmp(#f, "LSE")) + (!strcmp(#f, "LRCPC") ? 2 : 0) \
    + (!strcmp(#f, "LRCPC2") ? 4 : 0) + (!strcmp(#f, "LOR") ? 8 : 0)
    int mask = 0 A64_FEATURES(F);
#undef F
    if ((mask & 7) != 7) fail("A64_FEATURES sem LSE ou LRCPC");
    if (mask & 8) fail("FEAT_LOR não devia estar anunciado");
}

static void test_fork(void) {
    _Alignas(16) a64_u64 cell = 0x42;
    a64_cpu c;
    cpu_clear(&c);
    c.x[1] = (a64_u64)(uintptr_t)&cell;
    if (step(&c, I_LDXR) != A64_NEXT) fail("ldxr fork");
    pid_t pid = fork();
    if (pid < 0) { fail("fork"); return; }
    if (pid == 0) {
        a64_xmon_fork_child(&c);
        c.x[2] = 0;
        c.x[3] = 9;
        int r = step(&c, I_STXR);
        if (r != A64_NEXT || c.x[2] != 1) _exit(3);
        if (step(&c, I_LDXR) != A64_NEXT) _exit(4);
        c.x[2] = 0;
        c.x[3] = 9;
        if (step(&c, I_STXR) != A64_NEXT || c.x[2] != 0 || cell != 9) _exit(5);
        _exit(0);
    }
    int st = 0;
    if (waitpid(pid, &st, 0) != pid || !WIFEXITED(st) || WEXITSTATUS(st) != 0)
        fail("filho do fork");
    c.x[2] = 0;
    c.x[3] = 8;
    if (step(&c, I_STXR) != A64_NEXT || c.x[2] != 0 || cell != 8)
        fail("o pai perdeu a reserva no fork");
}

static a64_u8 *rwx_base;
static size_t rwx_off;
static a64_u32 *rwx(size_t n) {
    if (!rwx_base) {
        rwx_base = mmap(0, 1 << 20, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (rwx_base == MAP_FAILED) return 0;
    }
    n = (n + 4095) & ~(size_t)4095;
    if (rwx_off + n > (1 << 20)) return 0;
    a64_u32 *p = (a64_u32 *)(rwx_base + rwx_off);
    rwx_off += n;
    return p;
}

static void test_jit(void) {
    a64_u32 *code = rwx(4096);
    if (!code) { fail("mmap jit"); return; }
    _Alignas(16) a64_u64 cell = 0x42;
    /* ldxr x3, [x0]; str x1, [x0]; stxr w4, x2, [x0]; brk */
    code[0] = 0xc85f7c03; /* ldxr x3, [x0] */
    code[1] = I_STR_X1;
    code[2] = 0xc8047c02; /* stxr w4, x2, [x0] */
    code[3] = I_BRK;
    a64_cpu c;
    cpu_clear(&c);
    c.pc = (a64_u64)(uintptr_t)code;
    c.x[0] = (a64_u64)(uintptr_t)&cell;
    c.x[1] = 0x42;
    c.x[2] = 0x99;
    int r = a64_jit_run(&c);
    if (r != A64_EXIT_BRK || c.x[4] != 1 || cell != 0x42) fail("jit aba do str nativo");

    cell = 0x42;
    a64_u32 *code2 = rwx(4096);
    if (!code2) { fail("mmap jit 2"); return; }
    code2[0] = 0xc85f7c03;
    code2[1] = 0xc8047c02; /* stxr w4, x2, [x0] */
    code2[2] = I_BRK;
    cpu_clear(&c);
    c.pc = (a64_u64)(uintptr_t)code2;
    c.x[0] = (a64_u64)(uintptr_t)&cell;
    c.x[2] = 0x99;
    r = a64_jit_run(&c);
    if (r != A64_EXIT_BRK || c.x[4] != 0 || cell != 0x99) {
        fprintf(stderr, "  jit sucesso: r=%d w4=%llx cell=%llx pc=%llx\n", r, (unsigned long long)c.x[4], (unsigned long long)cell, (unsigned long long)c.pc);
        fail("jit stxr sem interferência");
    }

    /* laço numa página nova: ldxr/add/stxr, N vezes. x1 acaba em 0 se o laço rodou. */
    a64_u32 *code3 = rwx(4096);
    if (!code3) { fail("mmap jit 3"); return; }
    code3[0] = I_LDXR_X2;
    code3[1] = I_ADD1;
    code3[2] = I_STXR_W3;
    code3[3] = I_CBNZ;
    code3[4] = I_SUBS;
    code3[5] = I_BNE;
    code3[6] = I_BRK;
    cell = 0;
    const int n = 500;
    cpu_clear(&c);
    c.pc = (a64_u64)(uintptr_t)code3;
    c.x[0] = (a64_u64)(uintptr_t)&cell;
    c.x[1] = n;
    r = a64_jit_run(&c);
    if (r != A64_EXIT_BRK || cell != (a64_u64)n || c.x[1] != 0) {
        fprintf(stderr, "  jit laço: r=%d cell=%llx x1=%llx\n", r, (unsigned long long)cell, (unsigned long long)c.x[1]);
        fail("jit laço exclusivo");
    }
}

static a64_u64 jit_cell;
static a64_u32 *jit_code;

static void *jit_counter_thr(void *arg) {
    int n = (int)(intptr_t)arg;
    a64_cpu c;
    cpu_clear(&c);
    c.pc = (a64_u64)(uintptr_t)jit_code;
    c.x[0] = (a64_u64)(uintptr_t)&jit_cell;
    c.x[1] = (a64_u64)n;
    int r = a64_jit_run(&c);
    if (r != A64_EXIT_BRK) fail("jit thread não chegou no brk");
    return 0;
}

static void test_jit_mt(void) {
    jit_code = rwx(4096);
    if (!jit_code) { fail("mmap jit mt"); return; }
    jit_code[0] = I_LDXR_X2;
    jit_code[1] = I_ADD1;
    jit_code[2] = I_STXR_W3;
    jit_code[3] = I_CBNZ;
    jit_code[4] = I_SUBS;
    jit_code[5] = I_BNE;
    jit_code[6] = I_BRK;
    jit_cell = 0;
    const int n = 300;
    pthread_t t0, t1;
    pthread_create(&t0, 0, jit_counter_thr, (void *)(intptr_t)n);
    pthread_create(&t1, 0, jit_counter_thr, (void *)(intptr_t)n);
    pthread_join(t0, 0);
    pthread_join(t1, 0);
    if (jit_cell != (a64_u64)n * 2) fail("jit contador com duas threads");
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alarm;
    sigaction(SIGALRM, &sa, 0);
    alarm(60);
    test_sysctl();
    test_barriers_decode();
    test_same_thread();
    test_pair();
    test_invalidate();
    test_lse_values();
    test_fork();
    test_aba_threads();
    test_message();
    test_counter();
    test_lock();
    test_ldadd_mt();
    int relaxed = 0;
    if (sb_run(0, 20000, &relaxed) != 0) fail("stlr/ldar permitiu store-load");
    if (sb_run(1, 20000, &relaxed) != 0) fail("stllr não teve a barreira do stlr");
    if (sb_run(2, 20000, &relaxed) != 0) fail("dmb sy permitiu store-load");
    sb_run(3, 8000, &relaxed);
    fprintf(stderr, "dmb st: %d resultados relaxados em 8000 (o TSO pode mostrar)\n", relaxed);
    test_jit();
    test_jit_mt();
    alarm(0);
    if (g_fail) {
        fprintf(stderr, "%d falhas\n", g_fail);
        return 1;
    }
    printf("a64xmon: ok\n");
    return 0;
}
