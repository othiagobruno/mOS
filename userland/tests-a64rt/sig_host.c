/*
 * mOS — sinais do processo ARM64 traduzido, no hospedeiro x86_64 (Linux).
 *
 * Compila o motor (interpretador + JIT) e userland/a64rt/sig.c contra um kernel falso: as syscalls
 * que o runtime pede são respondidas aqui. A pilha do host é alinhada a A64RT_HSTACK para o
 * rt_self() do tratador achar a thread, como no mOS. Não é o boot do mOS e não executa um Mach-O.
 *
 *   gcc -O1 -g -std=c11 -Wall -Wno-unused-function -fno-strict-aliasing -mcx16 \
 *       -I userland/a64 -I userland/a64rt -I kernel/include -DA64_HAVE_JIT -D_GNU_SOURCE \
 *       userland/a64/arquivos.c userland/a64rt/sig.c userland/tests-a64rt/sig_host.c -pthread -o a64sig
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "rt.h"

/* signal.h do Linux transforma si_addr/si_pid/si_status em macros. Estes acessos ficam antes desse include. */
static void mos_si_set_addr(mos_siginfo_t *s, unsigned long long a) { s->si_addr = a; }
static unsigned long long mos_si_addr(const mos_siginfo_t *s) { return s->si_addr; }
static void mos_si_set_pid(mos_siginfo_t *s, int p) { s->si_pid = p; }
static int mos_si_pid(const mos_siginfo_t *s) { return s->si_pid; }
static void mos_si_set_status(mos_siginfo_t *s, int st) { s->si_status = st; }
static int mos_si_status(const mos_siginfo_t *s) { return s->si_status; }

#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>

int rt_engine, rt_debug;
u64 rt_text_lo, rt_text_hi;

static int falhas;
#define CHECK(nome, cond) do { \
    if (cond) printf("ok   %s\n", nome); \
    else { printf("FALHA %s (linha %d)\n", nome, __LINE__); falhas++; } \
    fflush(stdout); \
} while (0)

/* ---------------------------------------------------------------- stubs do hospedeiro */
static u32 g_mask;
static mos_sigaction_t g_host[MOS_NSIG];
static int g_tkill_captura;
static long g_tkill_tid, g_tkill_sig;

long rt_sys(long n, long a, long b, long c, long d, long e, long f) {
    (void)e; (void)f;
    if (n == SYS_WRITE) {
        if (b && c > 0) {
            ssize_t w = write(2, (const void *)b, (size_t)c);
            (void)w;
        }
        return c;
    }
    if (n == SYS_EXIT) _exit((int)a);
    if (n == SYS_YIELD || n == SYS_TIME) return 0;
    if (n == SYS_GETPID || n == SYS_GETTID) return 1;
    if (n == SYS_SIG && a == MOS_SIGOP_ACTION) {
        int sig = (int)b;
        if (sig <= 0 || sig >= MOS_NSIG) return -MOS_EINVAL;
        if (d) memcpy((void *)d, &g_host[sig], sizeof g_host[sig]);
        if (c) memcpy(&g_host[sig], (const void *)c, sizeof g_host[sig]);
        return 0;
    }
    if (n == SYS_SIG && a == MOS_SIGOP_MASK) {
        u32 old = g_mask, set = 0;
        if (c) memcpy(&set, (const void *)c, 4);
        if (b == MOS_SIG_BLOCK) g_mask |= set;
        else if (b == MOS_SIG_UNBLOCK) g_mask &= ~set;
        else if (b == MOS_SIG_SETMASK) g_mask = c ? set : 0;
        else return -MOS_EINVAL;
        if (d) memcpy((void *)d, &old, 4);
        return 0;
    }
    if (n == SYS_SIG && a == MOS_SIGOP_PENDING) {
        u32 z = 0;
        if (b) memcpy((void *)b, &z, 4);
        return 0;
    }
    if (n == SYS_SIG && a == MOS_SIGOP_TKILL) {
        g_tkill_tid = b;
        g_tkill_sig = c;
        if (g_tkill_captura) return 0;
        fprintf(stderr, "tkill inesperado tid %ld sig %ld\n", b, c);
        _exit(140 + (int)c);
    }
    fprintf(stderr, "syscall %ld op %ld não tratada no teste\n", n, a);
    return -MOS_EINVAL;
}

void rt_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
void rt_die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    _exit(97);
}
void rt_fbstat_final(const char *quando) { (void)quando; }
void rt_host_tramp(void) {}
/* rt.c não entra neste link: nenhuma página é MAP_JIT, então a falta segue o caminho normal */
int rt_jit_contains(u64 addr) { (void)addr; return 0; }
u64 rt_jit_alias_page(u64 addr) { (void)addr; return 0; }
void rt_jit_note_write(rt_thread_t *T, u64 addr) { (void)T; (void)addr; }

void *a64_host_jit_alloc(a64_u64 len) {
    void *p = mmap(0, (size_t)len, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? 0 : p;
}
int a64_host_jit_wprotect(a64_u64 page, int protect) { (void)page; (void)protect; return 0; }
void a64_host_icache_inval(a64_cpu *c, a64_u64 addr, a64_u64 len) { (void)c; (void)addr; (void)len; }
void a64_host_yield(a64_cpu *c) { (void)c; }
a64_u64 a64_host_cntvct(void) { return 0; }

__asm__(
    ".text\n"
    ".globl rt_setjmp\n"
    "rt_setjmp:\n"
    "mov %rbx, (%rdi)\n"
    "mov %rbp, 8(%rdi)\n"
    "mov %r12, 16(%rdi)\n"
    "mov %r13, 24(%rdi)\n"
    "mov %r14, 32(%rdi)\n"
    "mov %r15, 40(%rdi)\n"
    "lea 8(%rsp), %rax\n"
    "mov %rax, 48(%rdi)\n"
    "mov (%rsp), %rax\n"
    "mov %rax, 56(%rdi)\n"
    "xor %eax, %eax\n"
    "ret\n"
);

/* ---------------------------------------------------------------- falta real do Linux → runtime */
static volatile sig_atomic_t g_captura;

static void preenche_host(mos_mcontext_t *mc, ucontext_t *uc) {
    greg_t *g = uc->uc_mcontext.gregs;
    memset(mc, 0, sizeof *mc);
    mc->ss.rax = (unsigned long long)g[REG_RAX];
    mc->ss.rcx = (unsigned long long)g[REG_RCX];
    mc->ss.rdx = (unsigned long long)g[REG_RDX];
    mc->ss.rbx = (unsigned long long)g[REG_RBX];
    mc->ss.rsp = (unsigned long long)g[REG_RSP];
    mc->ss.rbp = (unsigned long long)g[REG_RBP];
    mc->ss.rsi = (unsigned long long)g[REG_RSI];
    mc->ss.rdi = (unsigned long long)g[REG_RDI];
    mc->ss.r8 = (unsigned long long)g[REG_R8];
    mc->ss.r9 = (unsigned long long)g[REG_R9];
    mc->ss.r10 = (unsigned long long)g[REG_R10];
    mc->ss.r11 = (unsigned long long)g[REG_R11];
    mc->ss.r12 = (unsigned long long)g[REG_R12];
    mc->ss.r13 = (unsigned long long)g[REG_R13];
    mc->ss.r14 = (unsigned long long)g[REG_R14];
    mc->ss.r15 = (unsigned long long)g[REG_R15];
    mc->ss.rip = (unsigned long long)g[REG_RIP];
    mc->ss.rflags = (unsigned long long)g[REG_EFL];
    mc->err = (unsigned)g[REG_ERR];
}

static void aplica_host(ucontext_t *uc, const mos_mcontext_t *mc) {
    greg_t *g = uc->uc_mcontext.gregs;
    g[REG_RAX] = (greg_t)mc->ss.rax;
    g[REG_RCX] = (greg_t)mc->ss.rcx;
    g[REG_RDX] = (greg_t)mc->ss.rdx;
    g[REG_RBX] = (greg_t)mc->ss.rbx;
    g[REG_RSP] = (greg_t)mc->ss.rsp;
    g[REG_RBP] = (greg_t)mc->ss.rbp;
    g[REG_RSI] = (greg_t)mc->ss.rsi;
    g[REG_RDI] = (greg_t)mc->ss.rdi;
    g[REG_R8] = (greg_t)mc->ss.r8;
    g[REG_R9] = (greg_t)mc->ss.r9;
    g[REG_R10] = (greg_t)mc->ss.r10;
    g[REG_R11] = (greg_t)mc->ss.r11;
    g[REG_R12] = (greg_t)mc->ss.r12;
    g[REG_R13] = (greg_t)mc->ss.r13;
    g[REG_R14] = (greg_t)mc->ss.r14;
    g[REG_R15] = (greg_t)mc->ss.r15;
    g[REG_RIP] = (greg_t)mc->ss.rip;
    g[REG_EFL] = (greg_t)mc->ss.rflags;
}

static void ao_segv_linux(int sig, siginfo_t *si, void *vuc) {
    ucontext_t *uc = vuc;
    if (sig != SIGSEGV || !g_captura) {
        fprintf(stderr, "SIGSEGV inesperado rip=%llx addr=%p\n",
                (unsigned long long)uc->uc_mcontext.gregs[REG_RIP], si->si_addr);
        _exit(90);
    }
    mos_mcontext_t mc;
    mos_siginfo_t msi;
    mos_ucontext_t muc;
    preenche_host(&mc, uc);
    memset(&msi, 0, sizeof msi);
    msi.si_signo = MOS_SIGSEGV;
    msi.si_code = MOS_SEGV_MAPERR;
    mos_si_set_addr(&msi, (unsigned long long)(uintptr_t)si->si_addr);
    memset(&muc, 0, sizeof muc);
    muc.uc_mcontext = (unsigned long long)(uintptr_t)&mc;
    muc.uc_sigmask = g_mask;
    unsigned long long rip0 = mc.ss.rip, rsp0 = mc.ss.rsp;
    rt_host_signal(MOS_SIGSEGV, &msi, &muc);
    if (mc.ss.rip == rip0 && mc.ss.rsp == rsp0) {
        fprintf(stderr, "rt_host_signal não reescreveu o contexto (rip %llx)\n", rip0);
        _exit(91);
    }
    aplica_host(uc, &mc);
}

static void ao_alarme(int sig) { (void)sig; fprintf(stderr, "alarme: o laço não parou\n"); _exit(92); }

/* ---------------------------------------------------------------- arranjo do guest */
#define TRAMP 0x4000A640ULL
#define HAND  0x4000A648ULL
#define BIT(s) (1u << (s))

static u8 *gstk;
static u64 gsp;
static u32 *codigo;
static u8 *altstk;

static void recursos(void) {
    gstk = mmap(0, 65536, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    altstk = mmap(0, 65536, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    codigo = mmap(0, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (gstk == MAP_FAILED || altstk == MAP_FAILED || codigo == MAP_FAILED) {
        perror("mmap");
        _exit(93);
    }
    gsp = ((uintptr_t)gstk + 65536) & ~15ULL;
}

static void zera(rt_thread_t *T) {
    memset(&T->cpu, 0, sizeof T->cpu);
    T->cpu.x[31] = gsp;
    T->pend = 0;
    T->intr_sig = 0;
    T->in_guest = 0;
    T->in_svc = 0;
    T->fault_pending = 0;
    T->sig_depth = 0;
    T->ss_sp = 0;
    T->ss_size = 0;
    u32 z = 0;
    rt_sig_syscall(T, MOS_SIGOP_MASK, MOS_SIG_SETMASK, (long)&z, 0);
}

static void acao(rt_thread_t *T, int sig, u32 flags, u32 mask) {
    mos_sigaction_t a;
    memset(&a, 0, sizeof a);
    a.handler = HAND;
    a.tramp = TRAMP;
    a.mask = mask;
    a.flags = flags;
    long r = rt_sig_syscall(T, MOS_SIGOP_ACTION, sig, (long)&a, 0);
    if (r != 0) {
        printf("FALHA sigaction do sinal %d devolveu %ld\n", sig, r);
        falhas++;
    }
}

static u32 mascara(rt_thread_t *T) {
    u32 m = 0;
    rt_sig_syscall(T, MOS_SIGOP_MASK, MOS_SIG_BLOCK, 0, (long)&m);
    return m;
}

static a64_darwin_mcontext_t *mc_tramp(rt_thread_t *T) {
    a64_darwin_ucontext_t *uc = (a64_darwin_ucontext_t *)T->cpu.x[4];
    return (a64_darwin_mcontext_t *)uc->uc_mcontext;
}

static mos_siginfo_t user_si(int sig) {
    mos_siginfo_t si;
    memset(&si, 0, sizeof si);
    si.si_signo = sig;
    si.si_code = MOS_SI_USER;
    return si;
}

static void host_async(rt_thread_t *T, int sig) {
    mos_siginfo_t si = user_si(sig);
    mos_mcontext_t mc;
    mos_ucontext_t uc;
    memset(&mc, 0, sizeof mc);
    memset(&uc, 0, sizeof uc);
    uc.uc_mcontext = (u64)(uintptr_t)&mc;
    uc.uc_sigmask = mascara(T);
    (void)T;
    rt_host_signal(sig, &si, &uc);
}

static u64 planta(const u32 *w, int n) {
    a64_invalidate((u64)(uintptr_t)codigo, 4096);
    memcpy(codigo, w, (size_t)n * 4);
    return (u64)(uintptr_t)codigo;
}

/* ---------------------------------------------------------------- casos */
static void teste_esr(void) {
    u32 ill = rt_sig_esr_host(MOS_SIGILL, 0, 0, 0);
    u32 tr = rt_sig_esr_host(MOS_SIGTRAP, 0, 0, 0);
    u32 fpe = rt_sig_esr_host(MOS_SIGFPE, 0, 0, 0);
    u32 seg = rt_sig_esr_host(MOS_SIGSEGV, 0x4, 0x10, 0x1000);          /* leitura, página ausente, usuário */
    u32 wr = rt_sig_esr_host(MOS_SIGSEGV, 0x6, 0x10, 0x1000);           /* escrita */
    u32 ins = rt_sig_esr_host(MOS_SIGSEGV, 0x14, 0x2000, 0x2000);       /* busca de instrução */
    CHECK("ESR de SIGILL é EC unknown", (ill >> 26) == A64_ESR_EC_UNKNOWN && (ill & A64_ESR_IL));
    CHECK("ESR de SIGTRAP é EC brk", (tr >> 26) == A64_ESR_EC_BRK);
    CHECK("ESR de SIGFPE é EC de ponto flutuante", (fpe >> 26) == A64_ESR_EC_FP);
    CHECK("ESR de SIGSEGV de leitura é DABT de tradução sem WnR",
          (seg >> 26) == A64_ESR_EC_DABT_LO && (seg & 0x3F) == A64_ESR_DFSC_TRANS && !(seg & A64_ESR_WNR));
    CHECK("ESR de SIGSEGV de escrita tem WnR", (wr & A64_ESR_WNR) != 0 && (wr & 0x3F) == A64_ESR_DFSC_TRANS);
    CHECK("ESR de falta de instrução é IABT", (ins >> 26) == A64_ESR_EC_IABT_LO);
}

static void teste_quadro(rt_thread_t *T) {
    zera(T);
    acao(T, MOS_SIGUSR1, MOS_SA_SIGINFO, BIT(MOS_SIGUSR2));
    T->cpu.pc = 0x1000;
    T->cpu.x[0] = 0x111;
    T->cpu.x[1] = 0x222;
    T->cpu.x[29] = 0x29;
    T->cpu.x[30] = 0x30;
    T->cpu.nzcv = A64_N | A64_Z;
    T->cpu.v[3].d[0] = 0x0123456789ABCDEFULL;
    T->cpu.v[3].d[1] = 0xFEDCBA9876543210ULL;
    T->cpu.fpsr = 1;
    T->cpu.fpcr = 1u << 8;                 /* IOE: o sigreturn descarta (A64_FPCR_MASK não tem armadilha) */
    T->cpu.excl_valid = 1;
    T->cpu.excl_addr = 0xABC;
    rt_sig_raise_sync(T, MOS_SIGUSR1, MOS_SI_USER, 0, 0);
    CHECK("trampolim recebe handler, estilo UC_FLAVOR e o sinal",
          T->cpu.pc == TRAMP && T->cpu.x[0] == HAND && T->cpu.x[1] == 30 && T->cpu.x[2] == MOS_SIGUSR1);
    a64_darwin_ucontext_t *uc = (a64_darwin_ucontext_t *)T->cpu.x[4];
    a64_darwin_mcontext_t *mc = (a64_darwin_mcontext_t *)uc->uc_mcontext;
    mos_siginfo_t *si = (mos_siginfo_t *)T->cpu.x[3];
    CHECK("ucontext e siginfo apontam para o quadro na pilha do guest",
          (u8 *)uc >= gstk && (u8 *)uc < gstk + 65536 && si->si_signo == MOS_SIGUSR1 && si->si_code == MOS_SI_USER);
    CHECK("mcontext guarda pc, x0, x1, fp, lr, sp e NZCV",
          mc->ss.pc == 0x1000 && mc->ss.x[0] == 0x111 && mc->ss.x[1] == 0x222 &&
          mc->ss.fp == 0x29 && mc->ss.lr == 0x30 && mc->ss.sp == gsp &&
          (mc->ss.cpsr & 0xF0000000u) == (A64_N | A64_Z));
    CHECK("neon v3, fpsr e o tamanho do mcontext",
          mc->ns.v[3][0] == 0x0123456789ABCDEFULL && mc->ns.v[3][1] == 0xFEDCBA9876543210ULL &&
          mc->ns.fpsr == 1 && uc->uc_mcsize == sizeof(a64_darwin_mcontext_t));
    CHECK("uc_mcsize e offsets iguais ao __darwin_mcontext64 (816, fpsr em 800)",
          sizeof(a64_darwin_neon_state_t) == 528 && sizeof(a64_darwin_mcontext_t) == 816 &&
          uc->uc_mcsize == 816 && __builtin_offsetof(a64_darwin_mcontext_t, ns.fpsr) == 800 &&
          __builtin_offsetof(a64_darwin_mcontext_t, ns.fpcr) == 804);
    CHECK("a máscara do tratador bloqueia o sinal e o conjunto da ação",
          (mascara(T) & BIT(MOS_SIGUSR1)) && (mascara(T) & BIT(MOS_SIGUSR2)));
    CHECK("entregar o sinal zera o monitor exclusivo", T->cpu.excl_valid == 0);
    mc->ss.pc = 0x2000;
    mc->ss.x[0] = 0x42;
    mc->ss.cpsr = (mc->ss.cpsr & ~0xF0000000u) | A64_C;
    mc->ns.v[3][0] = 0x99;
    mc->ns.fpcr = 1u << 8;
    rt_sig_syscall(T, MOS_SIGOP_RETURN, (long)uc, 0, 0);
    CHECK("sigreturn aplica pc, x0, NZCV e v3 escritos pelo tratador",
          T->cpu.pc == 0x2000 && T->cpu.x[0] == 0x42 && T->cpu.nzcv == A64_C && T->cpu.v[3].d[0] == 0x99);
    CHECK("sigreturn restaura a máscara anterior", mascara(T) == 0);
    CHECK("sigreturn descarta o bit de armadilha do FPCR", T->cpu.fpcr == 0);
}

static void teste_um_por_vez(rt_thread_t *T) {
    zera(T);
    acao(T, MOS_SIGUSR1, MOS_SA_SIGINFO, 0);
    acao(T, MOS_SIGUSR2, MOS_SA_SIGINFO, 0);
    T->cpu.pc = 0x3000;
    host_async(T, MOS_SIGUSR1);
    host_async(T, MOS_SIGUSR2);
    host_async(T, MOS_SIGUSR1);            /* o mesmo sinal não empilha outro quadro */
    rt_sig_deliver_pending(T, 0);
    CHECK("o primeiro pendente é o de menor número e o segundo continua na fila",
          T->cpu.pc == TRAMP && T->cpu.x[2] == MOS_SIGUSR1 && (T->pend & BIT(MOS_SIGUSR2)) && !(T->pend & BIT(MOS_SIGUSR1)));
    u64 pc_salvo = mc_tramp(T)->ss.pc;
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);
    CHECK("sigreturn volta ao pc de antes dos dois sinais", T->cpu.pc == pc_salvo && pc_salvo == 0x3000);
    rt_sig_deliver_pending(T, 0);
    CHECK("o segundo sinal só sai depois do sigreturn", T->cpu.pc == TRAMP && T->cpu.x[2] == MOS_SIGUSR2 && T->pend == 0);
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);
}

static void teste_flags(rt_thread_t *T) {
    zera(T);
    acao(T, MOS_SIGUSR1, 0, 0);            /* sem SA_SIGINFO */
    T->cpu.pc = 0x1000;
    rt_sig_raise_sync(T, MOS_SIGUSR1, MOS_SI_USER, 0, 0);
    CHECK("sem SA_SIGINFO o estilo é UC_TRAD", T->cpu.x[1] == 1);
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);

    zera(T);
    acao(T, MOS_SIGUSR1, MOS_SA_NODEFER | MOS_SA_SIGINFO, 0);
    T->cpu.pc = 0x1000;
    rt_sig_raise_sync(T, MOS_SIGUSR1, MOS_SI_USER, 0, 0);
    CHECK("SA_NODEFER não bloqueia o próprio sinal", (mascara(T) & BIT(MOS_SIGUSR1)) == 0);
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);

    zera(T);
    acao(T, MOS_SIGUSR1, MOS_SA_RESETHAND | MOS_SA_SIGINFO, 0);
    T->cpu.pc = 0x1000;
    rt_sig_raise_sync(T, MOS_SIGUSR1, MOS_SI_USER, 0, 0);
    mos_sigaction_t agora;
    memset(&agora, 0, sizeof agora);
    rt_sig_syscall(T, MOS_SIGOP_ACTION, MOS_SIGUSR1, 0, (long)&agora);
    CHECK("SA_RESETHAND volta o tratador para SIG_DFL", agora.handler == MOS_SIG_DFL);
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);

    zera(T);
    acao(T, MOS_SIGILL, MOS_SA_RESETHAND | MOS_SA_SIGINFO, 0);
    T->cpu.pc = 0x1000;
    rt_sig_raise_sync(T, MOS_SIGILL, MOS_ILL_ILLOPC, 0x1000, 0);
    memset(&agora, 0, sizeof agora);
    rt_sig_syscall(T, MOS_SIGOP_ACTION, MOS_SIGILL, 0, (long)&agora);
    CHECK("SA_RESETHAND não zera SIGILL", agora.handler == HAND);
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);

    zera(T);
    acao(T, MOS_SIGPIPE, MOS_SA_SIGINFO, 0);
    acao(T, MOS_SIGCHLD, MOS_SA_SIGINFO, 0);
    acao(T, MOS_SIGALRM, MOS_SA_SIGINFO, 0);
    T->cpu.pc = 0x1000;
    mos_siginfo_t si = user_si(MOS_SIGCHLD);
    mos_si_set_pid(&si, 44);
    si.si_code = MOS_CLD_EXITED;
    mos_si_set_status(&si, 3);
    mos_mcontext_t hmc;
    mos_ucontext_t huc;
    memset(&hmc, 0, sizeof hmc);
    memset(&huc, 0, sizeof huc);
    huc.uc_mcontext = (u64)(uintptr_t)&hmc;
    rt_host_signal(MOS_SIGCHLD, &si, &huc);
    rt_sig_deliver_pending(T, 0);
    mos_siginfo_t *gsi = (mos_siginfo_t *)T->cpu.x[3];
    CHECK("SIGCHLD assíncrono conserva si_pid e si_code",
          T->cpu.x[2] == MOS_SIGCHLD && mos_si_pid(gsi) == 44 && gsi->si_code == MOS_CLD_EXITED && mos_si_status(gsi) == 3);
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);
    host_async(T, MOS_SIGPIPE);
    host_async(T, MOS_SIGALRM);
    rt_sig_deliver_pending(T, 0);
    CHECK("SIGPIPE assíncrono chega ao trampolim", T->cpu.x[2] == MOS_SIGPIPE);
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);
    rt_sig_deliver_pending(T, 0);
    CHECK("SIGALRM assíncrono chega ao trampolim", T->cpu.x[2] == MOS_SIGALRM);
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);

    g_tkill_captura = 1;
    long tk = rt_sig_syscall(T, MOS_SIGOP_TKILL, 7, MOS_SIGUSR2, 0);
    g_tkill_captura = 0;
    CHECK("pthread_kill é repassado ao kernel com tid e sinal", tk == 0 && g_tkill_tid == 7 && g_tkill_sig == MOS_SIGUSR2);
}

static void teste_altstack(rt_thread_t *T) {
    zera(T);
    mos_sigstack_t pequena = {(u64)(uintptr_t)altstk, 8192, 0, 0};
    long r = rt_sig_syscall(T, MOS_SIGOP_ALTSTACK, (long)&pequena, 0, 0);
    CHECK("sigaltstack abaixo de 32 KiB devolve ENOMEM", r == -MOS_ENOMEM && T->ss_size == 0);
    mos_sigstack_t boa = {(u64)(uintptr_t)altstk, 32768, 0, 0};
    r = rt_sig_syscall(T, MOS_SIGOP_ALTSTACK, (long)&boa, 0, 0);
    CHECK("sigaltstack de 32 KiB é aceita", r == 0 && T->ss_size == 32768);
    acao(T, MOS_SIGUSR2, MOS_SA_ONSTACK | MOS_SA_SIGINFO, 0);
    T->cpu.pc = 0x1000;
    u64 sp_antes = T->cpu.x[31];
    rt_sig_raise_sync(T, MOS_SIGUSR2, MOS_SI_USER, 0, 0);
    a64_darwin_ucontext_t *uc = (a64_darwin_ucontext_t *)T->cpu.x[4];
    CHECK("SA_ONSTACK monta o quadro na sigaltstack",
          T->cpu.x[31] >= T->ss_sp && T->cpu.x[31] < T->ss_sp + T->ss_size &&
          uc->uc_onstack == 1 && (T->cpu.x[31] < sp_antes - 128 || T->cpu.x[31] >= sp_antes));
    T->cpu.x[31] = T->ss_sp + 64;
    mos_sigstack_t outra = boa;
    r = rt_sig_syscall(T, MOS_SIGOP_ALTSTACK, (long)&outra, 0, 0);
    CHECK("sigaltstack com o sp já na pilha alternativa é EPERM", r == -MOS_EPERM);
    rt_sig_syscall(T, MOS_SIGOP_RETURN, (long)uc, 0, 0);
    mos_sigstack_t off = {0, 0, MOS_SS_DISABLE, 0};
    r = rt_sig_syscall(T, MOS_SIGOP_ALTSTACK, (long)&off, 0, 0);
    CHECK("SS_DISABLE remove a sigaltstack", r == 0 && T->ss_size == 0);
}

static void teste_restart(rt_thread_t *T) {
    zera(T);
    acao(T, MOS_SIGALRM, MOS_SA_RESTART | MOS_SA_SIGINFO, 0);
    acao(T, MOS_SIGUSR1, MOS_SA_SIGINFO, 0);     /* sem SA_RESTART */
    T->in_svc = 1;
    host_async(T, MOS_SIGALRM);
    T->in_svc = 0;
    CHECK("SIGALRM com SA_RESTART refaz read", rt_sig_should_restart(T, SYS_READ) == true);
    CHECK("a decisão consome o sinal que interrompeu", rt_sig_should_restart(T, SYS_READ) == false);

    T->pend = 0;
    T->in_svc = 1;
    host_async(T, MOS_SIGALRM);
    T->in_svc = 0;
    CHECK("sleep não é refazível mesmo com SA_RESTART", rt_sig_should_restart(T, SYS_SLEEP) == false);

    T->pend = 0;
    host_async(T, MOS_SIGUSR1);                  /* pendente antes, sem restart */
    T->in_svc = 1;
    host_async(T, MOS_SIGALRM);                  /* este é o que acordou a syscall */
    T->in_svc = 0;
    CHECK("reinicia pelo sinal que interrompeu, não pelo de menor número", rt_sig_should_restart(T, SYS_READ) == true);

    T->pend = 0;
    T->in_svc = 1;
    host_async(T, MOS_SIGUSR1);                  /* primeiro a interromper: sem restart */
    host_async(T, MOS_SIGALRM);
    T->in_svc = 0;
    CHECK("se o primeiro a interromper não tem SA_RESTART, a syscall não é refeita",
          rt_sig_should_restart(T, SYS_WRITE) == false);
}

static int motor(rt_thread_t *T, const u32 *w, int n) {
    u64 pc = planta(w, n);
    zera(T);
    T->cpu.pc = pc;
    return a64_interp_run(&T->cpu, 1);
}

static void teste_falta_do_motor(rt_thread_t *T) {
    _Alignas(16) unsigned char buf[32];
    memset(buf, 0xAB, sizeof buf);
    u32 ldar = 0xC8DFFC20, stlr = 0xC89FFC20, br = 0xD61F0000, ldrsp = 0xF94003E0; /* ldar/stlr x0, [x1] */
    u32 brk = 0xD4200EE0, udf = 0;

    zera(T);
    T->cpu.pc = planta(&ldar, 1);
    T->cpu.x[1] = (u64)(uintptr_t)buf;
    int r = a64_interp_run(&T->cpu, 1);
    CHECK("LDAR alinhado executa", r == A64_EXIT_STOP && T->cpu.x[0] == 0xABABABABABABABABULL);

    T->cpu.pc = (u64)(uintptr_t)codigo;
    T->cpu.x[0] = 0;
    T->cpu.x[1] = (u64)(uintptr_t)buf + 12;
    r = a64_interp_run(&T->cpu, 1);
    int sig = 0, code = 0;
    u32 esr = 0;
    rt_sig_classify_fault(&T->cpu, &sig, &code, &esr);
    CHECK("LDAR que cruza 16 bytes é SIGBUS de alinhamento, load, FAR no endereço",
          r == A64_EXIT_FAULT && sig == MOS_SIGBUS && code == MOS_BUS_ADRALN &&
          (esr >> 26) == A64_ESR_EC_DABT_LO && (esr & 0x3F) == A64_ESR_DFSC_ALIGN &&
          !(esr & A64_ESR_WNR) && T->cpu.exit_info == T->cpu.x[1] && T->cpu.pc == (u64)(uintptr_t)codigo);

    T->cpu.pc = (u64)(uintptr_t)codigo;
    codigo[0] = stlr;
    T->cpu.x[0] = 0x11;
    T->cpu.x[1] = (u64)(uintptr_t)buf + 12;
    r = a64_interp_run(&T->cpu, 1);
    rt_sig_classify_fault(&T->cpu, &sig, &code, &esr);
    CHECK("STLR que cruza 16 bytes é SIGBUS com WnR",
          r == A64_EXIT_FAULT && (esr & A64_ESR_WNR) && (esr >> 26) == A64_ESR_EC_DABT_LO);

    T->cpu.x[31] = ((u64)(uintptr_t)buf + 8);   /* 8 mod 16 */
    T->cpu.pc = planta(&ldrsp, 1);
    r = a64_interp_run(&T->cpu, 1);
    rt_sig_classify_fault(&T->cpu, &sig, &code, &esr);
    CHECK("SP desalinhado é EC_SPALIGN",
          r == A64_EXIT_FAULT && (esr >> 26) == A64_ESR_EC_SPALIGN && T->cpu.exit_info == T->cpu.x[31]);

    T->cpu.x[31] = gsp;
    T->cpu.x[0] = 2;
    T->cpu.pc = planta(&br, 1);
    r = a64_interp_run(&T->cpu, 1);
    rt_sig_classify_fault(&T->cpu, &sig, &code, &esr);
    CHECK("BR para pc não múltiplo de 4 é EC_PCALIGN com FAR = pc",
          r == A64_EXIT_FAULT && T->cpu.pc == 2 && T->cpu.exit_info == 2 && (esr >> 26) == A64_ESR_EC_PCALIGN);

    acao(T, MOS_SIGTRAP, MOS_SA_SIGINFO, 0);
    T->cpu.x[31] = gsp;
    T->cpu.pc = planta(&brk, 1);
    r = a64_interp_run(&T->cpu, 1);
    u64 pc_brk = T->cpu.pc;
    rt_sig_from_motor(T, r);
    mc_tramp(T);
    CHECK("BRK vira SIGTRAP com o imediato no ESR e o pc na instrução",
          r == A64_EXIT_BRK && T->cpu.pc == TRAMP && T->cpu.x[2] == MOS_SIGTRAP &&
          (mc_tramp(T)->es.esr >> 26) == A64_ESR_EC_BRK && (mc_tramp(T)->es.esr & 0xFFFF) == 0x77 &&
          mc_tramp(T)->ss.pc == pc_brk);
    mc_tramp(T)->ss.pc += 4;
    mc_tramp(T)->ss.x[0] = 0x77;
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);
    CHECK("tratador do BRK altera pc e x0", T->cpu.pc == pc_brk + 4 && T->cpu.x[0] == 0x77);

    acao(T, MOS_SIGILL, MOS_SA_SIGINFO, 0);
    T->cpu.x[31] = gsp;
    T->cpu.pc = planta(&udf, 1);
    r = a64_interp_run(&T->cpu, 1);
    rt_sig_from_motor(T, r);
    CHECK("UDF vira SIGILL com EC unknown e o pc na instrução",
          r == A64_EXIT_UDF && T->cpu.x[2] == MOS_SIGILL &&
          (mc_tramp(T)->es.esr >> 26) == A64_ESR_EC_UNKNOWN && mc_tramp(T)->ss.pc == (u64)(uintptr_t)codigo);
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);
}

static int g_viu;
static u64 g_far, g_pc_salvo, g_esr;
static int g_sig;

static int ao_segv_guest(rt_thread_t *T) {
    a64_darwin_mcontext_t *mc = mc_tramp(T);
    mos_siginfo_t *si = (mos_siginfo_t *)T->cpu.x[3];
    g_viu = 1;
    g_sig = (int)T->cpu.x[2];
    g_far = mos_si_addr(si);
    g_pc_salvo = mc->ss.pc;
    g_esr = mc->es.esr;
    mc->ss.pc += 4;
    mc->ss.x[0] = 0x55;
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);
    return 0;
}

static int rodar(rt_thread_t *T, int jit, int (*no_tramp)(rt_thread_t *)) {
    g_captura = 1;
    int r = 0;
    for (int n = 0; n < 100000; n++) {
        if (T->cpu.pc == TRAMP) {
            no_tramp(T);
            continue;
        }
        if (rt_setjmp(&T->jb)) {
            T->in_guest = 0;
            rt_sig_raise_sync(T, T->fault_sig, T->fault_code, T->fault_addr, T->fault_esr);
            continue;
        }
        T->in_guest = 1;
        r = jit ? a64_jit_run(&T->cpu) : a64_interp_run(&T->cpu, 0);
        T->in_guest = 0;
        if (T->fault_pending) {
            T->fault_pending = 0;
            rt_sig_raise_sync(T, T->fault_sig, T->fault_code, T->fault_addr, T->fault_esr);
            continue;
        }
        break;
    }
    g_captura = 0;
    T->in_guest = 0;
    return r;
}

static void teste_segv_real(rt_thread_t *T, int jit) {
    u32 w[2] = {0x39400020, 0xD4200020};       /* ldrb w0, [x1]; brk #1 */
    u64 pc = planta(w, 2);
    zera(T);
    acao(T, MOS_SIGSEGV, MOS_SA_SIGINFO, 0);
    T->cpu.pc = pc;
    T->cpu.x[1] = 0x10;
    T->cpu.x[0] = 0;
    g_viu = 0;
    alarm(8);
    int r = rodar(T, jit, ao_segv_guest);
    alarm(0);
    const char *nome = jit ? "JIT" : "interpretador";
    char buf[160];
    snprintf(buf, sizeof buf, "SIGSEGV no %s: sinal, si_addr e pc do guest na instrução", nome);
    CHECK(buf, g_viu && g_sig == MOS_SIGSEGV && g_far == 0x10 && g_pc_salvo == pc);
    snprintf(buf, sizeof buf, "SIGSEGV no %s: ESR é DABT de página ausente", nome);
    CHECK(buf, (g_esr >> 26) == A64_ESR_EC_DABT_LO && (g_esr & 0x3F) == A64_ESR_DFSC_TRANS && !(g_esr & A64_ESR_WNR));
    if (r != A64_EXIT_BRK || T->cpu.x[0] != 0x55 || T->cpu.pc != pc + 4)
        printf("  %s r=%d x0=%llx pc=%llx (ldrb %llx)\n", nome, r, (unsigned long long)T->cpu.x[0],
               (unsigned long long)T->cpu.pc, (unsigned long long)pc);
    snprintf(buf, sizeof buf, "tratador no %s pula o load, grava x0 e o brk seguinte para", nome);
    CHECK(buf, r == A64_EXIT_BRK && T->cpu.x[0] == 0x55 && T->cpu.pc == pc + 4);
}

static volatile int g_solta, g_fim;
static rt_thread_t *g_alvo;

static void *parador(void *arg) {
    (void)arg;
    while (!g_solta && !g_fim) usleep(1000);
    while (!g_fim && __atomic_load_n(&g_alvo->cpu.x[0], __ATOMIC_RELAXED) < 1000) usleep(200);
    if (!g_fim) a64_request_stop(&g_alvo->cpu);
    return 0;
}

static void teste_parada(rt_thread_t *T, int jit) {
    u32 w[5] = {0x91000400, 0x91000400, 0x91000400, 0x91000400, 0x17FFFFFC}; /* 4× add x0,x0,#1; b .-16 */
    u64 pc = planta(w, 5);
    zera(T);
    T->cpu.pc = pc;
    T->cpu.x[0] = 0;
    g_alvo = T;
    g_fim = 0;
    g_solta = 0;
    pthread_t th;
    if (pthread_create(&th, 0, parador, 0) != 0) { perror("pthread"); _exit(94); }
    alarm(5);
    g_solta = 1;
    T->in_guest = 1;
    int r = jit ? a64_jit_run(&T->cpu) : a64_interp_run(&T->cpu, 0);
    T->in_guest = 0;
    alarm(0);
    g_fim = 1;
    pthread_join(th, 0);
    const char *nome = jit ? "JIT" : "interpretador";
    char buf[160];
    snprintf(buf, sizeof buf, "parada no %s sai com STOP e x0 andou", nome);
    CHECK(buf, r == A64_EXIT_STOP && T->cpu.x[0] >= 1000);
    if (!jit) {
        u64 off = T->cpu.pc - pc;
        int idx = (int)(off / 4);
        int esp = idx == 4 ? 0 : idx;
        snprintf(buf, sizeof buf, "parada no interpretador cai entre instruções com x0 coerente");
        CHECK(buf, off <= 16 && (off % 4) == 0 && ((T->cpu.x[0] & 3) == (u64)esp));
    } else {
        snprintf(buf, sizeof buf, "parada no JIT cai na cabeça do laço com x0 múltiplo de 4");
        CHECK(buf, T->cpu.pc == pc && (T->cpu.x[0] % 4) == 0);
    }
}

static int ao_segue(rt_thread_t *T) {
    a64_darwin_mcontext_t *mc = mc_tramp(T);
    mc->ss.x[0] = 100;
    rt_sig_syscall(T, MOS_SIGOP_RETURN, T->cpu.x[4], 0, 0);
    return 0;
}

static void teste_handler_segue(rt_thread_t *T) {
    u32 w[2] = {0x91000400, 0xD4200020};       /* add x0, x0, #1; brk #1 */
    u64 pc = planta(w, 2);
    zera(T);
    acao(T, MOS_SIGUSR1, MOS_SA_SIGINFO, 0);
    T->cpu.pc = pc;
    T->cpu.x[0] = 7;
    host_async(T, MOS_SIGUSR1);
    int r = 0;
    for (int n = 0; n < 8; n++) {
        if (T->pend) rt_sig_deliver_pending(T, 0);
        if (T->cpu.pc == TRAMP) { ao_segue(T); continue; }
        r = a64_interp_run(&T->cpu, 1);
        if (r == A64_EXIT_BRK) break;
    }
    CHECK("depois do sigreturn o add vê o x0 que o tratador escreveu", r == A64_EXIT_BRK && T->cpu.x[0] == 101);
}

static uintptr_t g_base;

static int todos(void) {
    rt_thread_t *T = rt_self();
    if ((uintptr_t)T != g_base) {
        printf("FALHA rt_self não achou a base da pilha\n");
        return 1;
    }
    memset(T, 0, sizeof *T);
    T->magic = RT_THREAD_MAGIC;
    T->self = T;
    T->tid = 1;
    T->state = 1;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = ao_segv_linux;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, 0);
    signal(SIGALRM, ao_alarme);
    rt_sig_init();
    recursos();
    teste_esr();
    teste_quadro(T);
    teste_um_por_vez(T);
    teste_flags(T);
    teste_altstack(T);
    teste_restart(T);
    teste_falta_do_motor(T);
    teste_handler_segue(T);
    teste_segv_real(T, 0);
    teste_parada(T, 0);
    teste_parada(T, 1);
    teste_segv_real(T, 1);
    return falhas;
}

int main(void) {
    size_t hs = (size_t)A64RT_HSTACK;
    void *map = mmap(0, hs * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (map == MAP_FAILED) { perror("mmap pilha"); return 1; }
    uintptr_t raw = (uintptr_t)map;
    uintptr_t base = (raw + hs - 1) & ~(hs - 1);
    if (base < raw || base + hs > raw + 3 * hs) {
        fprintf(stderr, "pilha alinhada não coube\n");
        return 1;
    }
    g_base = base;
    ucontext_t aqui, la;
    getcontext(&la);
    la.uc_stack.ss_sp = (void *)base;
    la.uc_stack.ss_size = hs;
    la.uc_link = &aqui;
    makecontext(&la, (void (*)(void))todos, 0);
    int rc = swapcontext(&aqui, &la);
    if (rc != 0) { perror("swapcontext"); return 1; }
    printf("a64sig: %d falhas\n", falhas);
    return falhas ? 1 : 0;
}
