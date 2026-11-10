/*
 * Mini-JIT arm64 no host x86_64: gera código, executa, reescreve, executa de novo, com várias threads.
 * Não é o V8. É o protocolo que o V8 usa (escrever, invalidar, publicar com release; a outra thread
 * adquire e executa) mais a rede de falta de escrita e o chute de uma thread que já está num laço.
 *
 * O harness oficial (tools/build-a64test.ps1) é PowerShell + zig e produz um binário Windows. Este
 * arquivo compila no Linux:
 *   clang -O1 -g -pthread -mcx16 -fno-strict-aliasing -std=gnu11 -DA64_HAVE_JIT \
 *     -I userland/a64 tools/jit_v8_mt.c userland/a64/a64_fp.c userland/a64/engine.c \
 *     userland/a64/interp.c userland/a64/interp_fp.c userland/a64/interp_int.c \
 *     userland/a64/interp_int_br.c userland/a64/interp_int_ls.c userland/a64/interp_simd.c \
 *     userland/a64/interp_simd_crypto.c userland/a64/interp_simd_fp.c userland/a64/interp_simd_int.c \
 *     userland/a64/jit_core.c userland/a64/jit_tr.c -o /tmp/jit_v8_mt -lm
 */
#include "a64.h"

#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static volatile a64_u32 *g_code;

static void *host_page(a64_u64 page) { return (void *)(uintptr_t)(page & ~(a64_u64)0xfff); }

#define HOST_PAGES 64
static void *hp_base[HOST_PAGES];
static int hp_rx[HOST_PAGES];
static int hp_n;

static int hp_slot(void *p) {
    for (int i = 0; i < hp_n; i++) if (hp_base[i] == p) return i;
    if (hp_n == HOST_PAGES) return -1;
    hp_base[hp_n] = p;
    hp_rx[hp_n] = 0;
    return hp_n++;
}

void *a64_host_jit_alloc(a64_u64 len) {
    void *p = mmap(NULL, (size_t)len, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}

int a64_host_jit_wprotect(a64_u64 page, int protect) {
    void *p = host_page(page);
    int i = hp_slot(p);
    if (i < 0) return 0;
    if (protect == 2) return hp_rx[i] ? 0 : 1;
    int prot = protect ? (PROT_READ | PROT_EXEC) : (PROT_READ | PROT_WRITE | PROT_EXEC);
    if (mprotect(p, 4096, prot) != 0) return 0;
    hp_rx[i] = protect ? 1 : 0;
    return 1;
}

a64_u64 a64_host_cntvct(void) {
    unsigned lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((a64_u64)hi << 32) | lo;
}
void a64_host_icache_inval(a64_cpu *c, a64_u64 addr, a64_u64 len) { (void)c; a64_invalidate(addr, len); }
void a64_host_yield(a64_cpu *c) { (void)c; sched_yield(); }

static void on_fault(int sig, siginfo_t *si, void *ctx) {
    (void)sig;
    (void)ctx;
    if (si && a64_jit_write_fault((a64_u64)(uintptr_t)si->si_addr)) return;
    const char msg[] = "jit_v8_mt: falta que o JIT nao assumiu\n";
    if (write(2, msg, sizeof msg - 1) < 0) { }
    _exit(2);
}

static a64_u32 movz_x0(unsigned imm) { return 0xD2800000u | ((imm & 0xffffu) << 5); }
static const a64_u32 BRK1 = 0xD4200020u;
static const a64_u32 B_BACK = 0x17FFFFFFu;

static int run_at(a64_cpu *c, a64_u64 pc, unsigned expect) {
    c->pc = pc;
    c->x[0] = 0xdead;
    int rc = a64_run(c, A64_ENGINE_JIT);
    if (rc != A64_EXIT_BRK) {
        fprintf(stderr, "jit_v8_mt: saida %d em vez de BRK (x0=%llu, esperado %u)\n", rc,
                (unsigned long long)c->x[0], expect);
        return 1;
    }
    if (c->x[0] != expect) {
        fprintf(stderr, "jit_v8_mt: x0=%llu, esperado %u\n", (unsigned long long)c->x[0], expect);
        return 1;
    }
    return 0;
}

static int single_thread(void) {
    a64_cpu c;
    memset(&c, 0, sizeof c);
    g_code[0] = movz_x0(7);
    g_code[1] = BRK1;
    a64_invalidate((a64_u64)(uintptr_t)g_code, 64);
    if (run_at(&c, (a64_u64)(uintptr_t)g_code, 7)) return 1;
    g_code[0] = movz_x0(9);
    a64_invalidate((a64_u64)(uintptr_t)g_code, 64);
    if (run_at(&c, (a64_u64)(uintptr_t)g_code, 9)) return 1;
    /* a página já foi executada: a escrita falta, o tratador devolve a escrita e a próxima
     * entrada no despachante aplica a invalidação. Sem a64_invalidate explícito. */
    g_code[0] = movz_x0(11);
    if (run_at(&c, (a64_u64)(uintptr_t)g_code, 11)) return 1;
    printf("jit_v8_mt: uma thread ok (7, 9, 11 pela falta)\n");
    return 0;
}

static atomic_uint g_seq, g_pub, g_stop, g_bad, g_ok;

static int run_value(a64_cpu *c) {
    c->pc = (a64_u64)(uintptr_t)g_code;
    c->x[0] = 0xdead;
    return a64_run(c, A64_ENGINE_JIT);
}

static void *reader(void *arg) {
    unsigned slot = (unsigned)(uintptr_t)arg;
    a64_cpu c;
    memset(&c, 0, sizeof c);
    unsigned got = 0;
    while (got < 40) {
        unsigned s1 = atomic_load_explicit(&g_seq, memory_order_acquire);
        if ((s1 & 1u) || s1 == 0) {
            if (atomic_load_explicit(&g_stop, memory_order_acquire) && got > 0) break;
            continue;
        }
        unsigned v = atomic_load_explicit(&g_pub, memory_order_acquire);
        int rc = run_value(&c);
        unsigned s2 = atomic_load_explicit(&g_seq, memory_order_acquire);
        if (s1 != s2) continue;
        if (rc != A64_EXIT_BRK || c.x[0] != v) {
            fprintf(stderr, "jit_v8_mt: leitor %u viu x0=%llu rc=%d, publicado %u\n", slot,
                    (unsigned long long)c.x[0], rc, v);
            atomic_fetch_add_explicit(&g_bad, 1, memory_order_relaxed);
            return (void *)1;
        }
        got++;
        atomic_fetch_add_explicit(&g_ok, 1, memory_order_relaxed);
    }
    return got >= 40 ? NULL : (void *)1;
}

static int many_threads(void) {
    atomic_store(&g_seq, 0);
    atomic_store(&g_pub, 0);
    atomic_store(&g_stop, 0);
    atomic_store(&g_bad, 0);
    atomic_store(&g_ok, 0);
    pthread_t th[2];
    for (int i = 0; i < 2; i++) {
        if (pthread_create(&th[i], NULL, reader, (void *)(uintptr_t)i) != 0) {
            perror("pthread_create");
            return 1;
        }
    }
    for (unsigned k = 1; k <= 200; k++) {
        atomic_store_explicit(&g_seq, 2 * k - 1, memory_order_release);
        g_code[0] = movz_x0(k);
        g_code[1] = BRK1;
        a64_invalidate((a64_u64)(uintptr_t)g_code, 64);
        atomic_store_explicit(&g_pub, k, memory_order_release);
        atomic_store_explicit(&g_seq, 2 * k, memory_order_release);
        if (atomic_load_explicit(&g_bad, memory_order_relaxed)) break;
    }
    atomic_store_explicit(&g_stop, 1, memory_order_release);
    int fail = 0;
    for (int i = 0; i < 2; i++) {
        void *ret = 0;
        pthread_join(th[i], &ret);
        if (ret) fail = 1;
    }
    unsigned ok = atomic_load(&g_ok), bad = atomic_load(&g_bad);
    if (fail || bad || ok < 80) {
        fprintf(stderr, "jit_v8_mt: varias threads falhou (ok=%u bad=%u)\n", ok, bad);
        return 1;
    }
    printf("jit_v8_mt: duas threads ok (%u leituras estaveis)\n", ok);
    return 0;
}

static atomic_uint g_started, g_done;
static a64_u64 g_loop_x0;

static void *loop_reader(void *arg) {
    (void)arg;
    a64_cpu c;
    memset(&c, 0, sizeof c);
    atomic_store_explicit(&g_started, 1, memory_order_release);
    c.pc = (a64_u64)(uintptr_t)g_code;
    int rc = a64_run(&c, A64_ENGINE_JIT);
    g_loop_x0 = c.x[0];
    if (rc != A64_EXIT_BRK) g_loop_x0 = ~0ull;
    atomic_store_explicit(&g_done, 1, memory_order_release);
    return NULL;
}

static int kick_loop(void) {
    for (unsigned k = 1; k <= 12; k++) {
        g_code[0] = movz_x0(1);
        g_code[1] = B_BACK;
        a64_invalidate((a64_u64)(uintptr_t)g_code, 64);
        atomic_store(&g_started, 0);
        atomic_store(&g_done, 0);
        g_loop_x0 = 0;
        pthread_t th;
        if (pthread_create(&th, NULL, loop_reader, NULL) != 0) return 1;
        while (!atomic_load_explicit(&g_started, memory_order_acquire)) sched_yield();
        usleep(20000);
        g_code[0] = movz_x0(k + 20);
        g_code[1] = BRK1;
        a64_invalidate((a64_u64)(uintptr_t)g_code, 64);
        for (int n = 0; n < 2000 && !atomic_load_explicit(&g_done, memory_order_acquire); n++) usleep(1000);
        if (!atomic_load_explicit(&g_done, memory_order_acquire)) {
            fprintf(stderr, "jit_v8_mt: laço da thread nao saiu no passo %u\n", k);
            return 1;
        }
        pthread_join(th, NULL);
        if (g_loop_x0 != k + 20) {
            fprintf(stderr, "jit_v8_mt: laço devolveu %llu, esperado %u\n",
                    (unsigned long long)g_loop_x0, k + 20);
            return 1;
        }
    }
    printf("jit_v8_mt: chute de laço em outra thread ok (12 passos)\n");
    return 0;
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO;
    if (sigaction(SIGSEGV, &sa, NULL) != 0 || sigaction(SIGBUS, &sa, NULL) != 0) {
        perror("sigaction");
        return 1;
    }
    g_code = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_code == MAP_FAILED) { perror("mmap"); return 1; }
    alarm(30);
    if (single_thread() || many_threads() || kick_loop()) return 1;
    printf("jit_v8_mt: passou\n");
    return 0;
}
