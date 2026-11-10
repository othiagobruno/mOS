/*
 * mOS — harness do tradutor arm64 no Linux (medição e regressão interp × JIT).
 * Mesmo contrato de tools/a64test.c: ELF64 AArch64 estático, endereços de tools/a64oracle/fz.h,
 * syscalls svc #0x80 (x16: 1 exit, 4 write, 20 getpid, 0x4000 dump) e svc #0x4001/#0x4002.
 *
 *   a64host [--jit] [--count] [--tempo] [--budget N] [--argv0 NOME] programa.elf [args...]
 *
 * --tempo grava em stderr "tempo_ns <n>" só do a64_run (sem carga do ELF).
 * --count executa no interpretador e grava "insns <n>" (instruções guest arquiteturais).
 * Não é flag de produto: o JIT no binário continua o de sempre.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/mman.h>
#include "../userland/a64/a64.h"
#include "a64oracle/fz.h"

static int g_trace, g_count, g_tempo;
static a64_cpu *g_cpu;
static sigjmp_buf g_jb;
static volatile sig_atomic_t g_in_guest;
static volatile sig_atomic_t g_faulted;
static volatile a64_u64 g_fault_addr;
static volatile int g_fault_sig;

a64_u64 a64_host_cntvct(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long double s = (long double)ts.tv_sec + (long double)ts.tv_nsec / 1e9L;
    return (a64_u64)(s * (long double)A64_CNTFRQ);
}
void a64_host_icache_inval(a64_cpu *c, a64_u64 addr, a64_u64 len) { (void)c; a64_invalidate(addr, len); }
void a64_host_yield(a64_cpu *c) { (void)c; sched_yield(); }

static int page_prot(a64_u64 page, char *perm) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return 0;
    char line[384];
    int ok = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned long lo, hi;
        char p[8];
        if (sscanf(line, "%lx-%lx %7s", &lo, &hi, p) == 3 && page >= lo && page < hi) {
            if (perm) memcpy(perm, p, 5);
            ok = 1;
            break;
        }
    }
    fclose(f);
    return ok;
}

void *a64_host_jit_alloc(a64_u64 len) {
    void *p = mmap(NULL, (size_t)len, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
int a64_host_jit_wprotect(a64_u64 page, int protect) {
    char perm[8];
    if (!page_prot(page, perm)) return 0;
    int wr = perm[1] == 'w';
    if (protect == 2) return wr;
    if (protect) {
        if (!wr) return 0;
        return mprotect((void *)(uintptr_t)page, 4096, PROT_READ) == 0;
    }
    if (wr) return 1;
    return mprotect((void *)(uintptr_t)page, 4096, PROT_READ | PROT_WRITE) == 0;
}

static void dump(const a64_cpu *c) {
    for (int i = 0; i < 31; i++) printf("x%-2d %016llx%s", i, (unsigned long long)c->x[i], (i % 4 == 3) ? "\n" : "  ");
    printf("sp  %016llx\n", (unsigned long long)c->x[31]);
    printf("nzcv %08x fpcr %08x fpsr %08x\n", c->nzcv, c->fpcr, c->fpsr);
    for (int i = 0; i < 32; i++)
        printf("v%-2d %016llx%016llx%s", i, (unsigned long long)c->v[i].d[1], (unsigned long long)c->v[i].d[0], (i % 2 == 1) ? "\n" : "  ");
    fflush(stdout);
}

static void hostctx_from(a64_hostctx *h, ucontext_t *uc) {
    mcontext_t *mc = &uc->uc_mcontext;
    h->gpr[0] = (a64_u64)mc->gregs[REG_RAX];
    h->gpr[1] = (a64_u64)mc->gregs[REG_RCX];
    h->gpr[2] = (a64_u64)mc->gregs[REG_RDX];
    h->gpr[3] = (a64_u64)mc->gregs[REG_RBX];
    h->gpr[4] = (a64_u64)mc->gregs[REG_RSP];
    h->gpr[5] = (a64_u64)mc->gregs[REG_RBP];
    h->gpr[6] = (a64_u64)mc->gregs[REG_RSI];
    h->gpr[7] = (a64_u64)mc->gregs[REG_RDI];
    h->gpr[8] = (a64_u64)mc->gregs[REG_R8];
    h->gpr[9] = (a64_u64)mc->gregs[REG_R9];
    h->gpr[10] = (a64_u64)mc->gregs[REG_R10];
    h->gpr[11] = (a64_u64)mc->gregs[REG_R11];
    h->gpr[12] = (a64_u64)mc->gregs[REG_R12];
    h->gpr[13] = (a64_u64)mc->gregs[REG_R13];
    h->gpr[14] = (a64_u64)mc->gregs[REG_R14];
    h->gpr[15] = (a64_u64)mc->gregs[REG_R15];
    h->rip = (a64_u64)mc->gregs[REG_RIP];
    h->rflags = (a64_u64)mc->gregs[REG_EFL];
    memset(h->xmm, 0, sizeof h->xmm);
}

static void on_fault(int sig, siginfo_t *si, void *ud) {
    ucontext_t *uc = ud;
    unsigned long err = (unsigned long)uc->uc_mcontext.gregs[REG_ERR];
    a64_u64 addr = si && si->si_addr ? (a64_u64)(uintptr_t)si->si_addr : 0;
#ifdef A64_HAVE_JIT
    if ((sig == SIGSEGV || sig == SIGBUS) && (err & 2) && a64_jit_write_fault(addr)) return;
#endif
    if (!g_in_guest) {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = SIG_DFL;
        sigaction(sig, &sa, NULL);
        raise(sig);
        return;
    }
    if (g_cpu) {
        a64_hostctx h;
        memset(&h, 0, sizeof h);
        hostctx_from(&h, uc);
#ifdef A64_HAVE_JIT
        a64_jit_recover(g_cpu, &h);
        a64_jit_abort(g_cpu);
#endif
    }
    g_fault_sig = sig;
    g_fault_addr = addr;
    g_faulted = 1;
    g_in_guest = 0;
    siglongjmp(g_jb, 1);
}

static int map_span(a64_u64 va, a64_u64 len) {
    a64_u64 lo = va & ~0xFFFFull, hi = (va + len + 0xFFFF) & ~0xFFFFull;
    if (hi <= lo) return 0;
    void *p = mmap((void *)(uintptr_t)lo, (size_t)(hi - lo), PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p != MAP_FAILED) return 1;
    /* já mapeado por um PT_LOAD anterior: confere que cobre o intervalo */
    char perm[8];
    for (a64_u64 a = lo; a < hi; a += 0x10000)
        if (!page_prot(a, perm)) {
            fprintf(stderr, "a64host: mmap %llx falhou (%s)\n", (unsigned long long)lo, strerror(errno));
            return 0;
        }
    return 1;
}

static int load_elf(const char *path, a64_u64 *entry) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "a64host: não abriu %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 64) { fclose(f); return 0; }
    unsigned char *b = malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return 0; }
    fclose(f);
    if (memcmp(b, "\x7f" "ELF", 4) || b[4] != 2 || *(a64_u16 *)(b + 18) != 183) {
        fprintf(stderr, "a64host: %s não é ELF64 AArch64\n", path); free(b); return 0;
    }
    *entry = *(a64_u64 *)(b + 24);
    a64_u64 phoff = *(a64_u64 *)(b + 32);
    a64_u16 phent = *(a64_u16 *)(b + 54), phnum = *(a64_u16 *)(b + 56);
    for (int i = 0; i < phnum; i++) {
        unsigned char *ph = b + phoff + (a64_u64)i * phent;
        if (*(a64_u32 *)ph != 1) continue;
        a64_u64 off = *(a64_u64 *)(ph + 8), va = *(a64_u64 *)(ph + 16);
        a64_u64 fsz = *(a64_u64 *)(ph + 32), msz = *(a64_u64 *)(ph + 40);
        if (va < FZ_IMG_LO || va + msz > FZ_STACK_VA + FZ_STACK_SZ) {
            fprintf(stderr, "a64host: PT_LOAD em %llx fora da faixa dos testes\n", (unsigned long long)va);
            free(b); return 0;
        }
        if (!map_span(va, msz)) { free(b); return 0; }
        memcpy((void *)(uintptr_t)va, b + off, (size_t)fsz);
        if (msz > fsz) memset((void *)(uintptr_t)(va + fsz), 0, (size_t)(msz - fsz));
    }
    free(b);
    return 1;
}

static int handle_svc(a64_cpu *c, int *done) {
    if (c->exit_info == FZ_SVC_DUMP) { dump(c); return 0; }
    if (c->exit_info == FZ_SVC_EXIT0) { *done = 1; fflush(stdout); return 0; }
    switch (c->x[16]) {
    case 1: *done = 1; fflush(stdout); return (int)c->x[0];
    case 4: {
        fflush(stdout);
        const void *buf = (const void *)(uintptr_t)c->x[1];
        size_t len = (size_t)c->x[2];
        int fd = c->x[0] == 2 ? 2 : 1;
        ssize_t w = write(fd, buf, len);
        c->x[0] = w < 0 ? 0 : (a64_u64)w;
        c->nzcv &= ~A64_C;
        return 0;
    }
    case 20: c->x[0] = 1; c->nzcv &= ~A64_C; return 0;
    case 0x4000: dump(c); return 0;
    default:
        fflush(stdout);
        fprintf(stderr, "a64host: syscall %llu não suportada (pc=%llx)\n",
                (unsigned long long)c->x[16], (unsigned long long)c->pc);
        c->x[0] = 78; c->nzcv |= A64_C;
        return 0;
    }
}

static a64_u64 nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (a64_u64)ts.tv_sec * 1000000000ull + (a64_u64)ts.tv_nsec;
}

int main(int argc, char **argv) {
    int engine = A64_ENGINE_INTERP, ai = 1;
    a64_u64 budget = 0;
    const char *argv0 = NULL;
    for (; ai < argc && argv[ai][0] == '-'; ai++) {
        if (!strcmp(argv[ai], "--jit")) engine = A64_ENGINE_JIT;
        else if (!strcmp(argv[ai], "--trace")) g_trace = 1;
        else if (!strcmp(argv[ai], "--count")) g_count = 1;
        else if (!strcmp(argv[ai], "--tempo")) g_tempo = 1;
        else if (!strcmp(argv[ai], "--budget") && ai + 1 < argc) budget = strtoull(argv[++ai], NULL, 0);
        else if (!strcmp(argv[ai], "--argv0") && ai + 1 < argc) argv0 = argv[++ai];
        else { fprintf(stderr, "a64host: opção desconhecida %s\n", argv[ai]); return 2; }
    }
    if (ai >= argc) {
        fprintf(stderr, "uso: a64host [--jit] [--count] [--tempo] [--budget N] [--argv0 NOME] programa.elf [args]\n");
        return 2;
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);

    a64_u64 entry;
    if (!load_elf(argv[ai], &entry)) return 2;
    if (!map_span(FZ_STACK_VA, FZ_STACK_SZ)) return 2;

    int gargc = argc - ai;
    const char **gv = calloc((size_t)gargc + 1, sizeof(char *));
    for (int i = 0; i < gargc; i++) gv[i] = argv[ai + i];
    if (argv0) gv[0] = argv0;
    a64_u64 gargv;
    a64_u64 sp = fz_stack_layout(FZ_STACK_VA, gargc, gv, &gargv);

    size_t csz = (sizeof(a64_cpu) + 63u) & ~(size_t)63u;
    a64_cpu *c = aligned_alloc(64, csz);
    if (!c) return 2;
    memset(c, 0, sizeof *c);
    c->pc = entry;
    c->x[31] = sp;
    c->x[0] = (a64_u64)gargc;
    c->x[1] = gargv;
    c->x[30] = 0;
    g_cpu = c;

    int done = 0, status = 0;
    a64_u64 steps = 0, acc_ns = 0;
    if (sigsetjmp(g_jb, 1) != 0) {
        fflush(stdout);
        int sig = g_fault_sig;
        if (sig != SIGSEGV && sig != SIGBUS) {
            fprintf(stderr, "HOSTEXC %d pc=%016llx\n", sig, (unsigned long long)c->pc);
            dump(c);
            return 140;
        }
        fprintf(stderr, "FAULT pc=%016llx addr=%016llx\n", (unsigned long long)c->pc, (unsigned long long)g_fault_addr);
        dump(c);
        return 139;
    }
    while (!done) {
        int r;
        g_in_guest = 1;
        if (g_count || g_trace || budget) {
            if (g_trace) fprintf(stderr, "%016llx: %08x\n", (unsigned long long)c->pc, *(a64_u32 *)(uintptr_t)c->pc);
            r = a64_interp_step(c);
            g_in_guest = 0;
            if (r <= A64_BRANCH) {
                steps++;
                if (budget && steps >= budget) {
                    fprintf(stderr, "a64host: orçamento esgotado\n");
                    dump(c);
                    return 3;
                }
                continue;
            }
            if (r == A64_EXIT_ICACHE) { a64_invalidate(c->exit_info & ~63ull, 64); continue; }
            if (r == A64_EXIT_YIELD) continue;
            steps++;
        } else {
            a64_u64 t0 = nsec_now();
            r = a64_run(c, engine);
            acc_ns += nsec_now() - t0;
            g_in_guest = 0;
        }
        if (r != A64_EXIT_SVC) fflush(stdout);
        switch (r) {
        case A64_EXIT_SVC: status = handle_svc(c, &done); break;
        case A64_EXIT_BRK:
            fprintf(stderr, "BRK #%llx pc=%016llx\n", (unsigned long long)c->exit_info, (unsigned long long)c->pc);
            dump(c); return 133;
        case A64_EXIT_UDF:
            fprintf(stderr, "UDF %08x pc=%016llx\n", c->exit_insn, (unsigned long long)c->pc);
            dump(c); return 132;
        case A64_EXIT_HLT:
            fprintf(stderr, "HLT pc=%016llx\n", (unsigned long long)c->pc); return 132;
        case A64_EXIT_FAULT:
            fprintf(stderr, "FAULT pc=%016llx addr=%016llx\n", (unsigned long long)c->pc, (unsigned long long)c->exit_info);
            dump(c); return 139;
        case A64_EXIT_STOP: break;
        case A64_EXIT_ICACHE: a64_invalidate(c->exit_info & ~63ull, 64); break;
        case A64_EXIT_YIELD: break;
        default:
            fprintf(stderr, "a64host: saída %d inesperada\n", r); return 4;
        }
    }
    if (g_count) fprintf(stderr, "insns %llu\n", (unsigned long long)steps);
    if (g_tempo) fprintf(stderr, "tempo_ns %llu\n", (unsigned long long)acc_ns);
    return status;
}
