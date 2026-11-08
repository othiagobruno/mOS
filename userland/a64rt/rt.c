/*
 * mOS — a64rt: partida, laço das threads guest e ponte de syscalls.
 * Ver rt.h e a64rt_abi.h (contrato com o kernel e com o libSystem arm64).
 */
#include "rt.h"

_Static_assert(SYS_THREAD_EXIT == 35 && SYS_SIG == 58 && MOS_SIGOP_RETURN == 6, "números literais em start.S");
_Static_assert(sizeof(rt_thread_t) < 64 * 1024, "rt_thread_t na base da pilha do host");

#ifdef A64_HAVE_JIT
int rt_engine = A64_ENGINE_JIT;
#else
int rt_engine = A64_ENGINE_INTERP;
#endif
u64 rt_text_lo, rt_text_hi;
static const a64rt_boot_t *g_boot;
int rt_debug;

void rt_thread_start(void);
void rt_thread_exit_now(volatile int *state, long code) __attribute__((noreturn));

/* ---------------------------------------------------------------- memória do host */
void *rt_alloc(u64 len) {
    long r = RS1(SYS_MMAP, (len + 0xFFF) & ~0xFFFULL);
    if (r < 0 && r > -4096) return 0;
    return (void *)r;
}
void rt_free(void *p, u64 len) { if (p) RS2(SYS_MUNMAP, p, (len + 0xFFF) & ~0xFFFULL); }

/* pilha do host alinhada ao próprio tamanho (rt_self) */
static void *alloc_hstack(void) {
    u64 a = (u64)rt_alloc(2 * A64RT_HSTACK);
    if (!a) return 0;
    u64 lo = (a + A64RT_HSTACK - 1) & ~(A64RT_HSTACK - 1);
    if (lo > a) RS2(SYS_MUNMAP, a, lo - a);
    if (a + 2 * A64RT_HSTACK > lo + A64RT_HSTACK) RS2(SYS_MUNMAP, lo + A64RT_HSTACK, a + 2 * A64RT_HSTACK - (lo + A64RT_HSTACK));
    return (void *)lo;
}

/* ---------------------------------------------------------------- tabela de threads */
static rt_thread_t *g_threads[RT_MAX_THREADS];
static volatile int g_tlock;
static void tlock(void) { while (__atomic_exchange_n(&g_tlock, 1, __ATOMIC_ACQUIRE)) RS0(SYS_YIELD); }
static void tunlock(void) { __atomic_store_n(&g_tlock, 0, __ATOMIC_RELEASE); }

rt_thread_t *rt_thread_find(long tid) {
    rt_thread_t *r = 0;
    tlock();
    for (int i = 0; i < RT_MAX_THREADS && !r; i++)
        if (g_threads[i] && g_threads[i]->state == 1 && g_threads[i]->tid == tid) r = g_threads[i];
    tunlock();
    return r;
}

/* um bloco livre (thread que já saiu) ou uma pilha nova; devolve marcado como vivo */
/* Um bloco livre só é reusado RT_REUSE_MS depois da saída: a thread que sai marca o bloco e chama SYS_THREAD_EXIT sem
 * tocar mais na pilha no mOS, mas no harness do Windows o int 0x80 vira uma exceção despachada NESSA pilha. */
#define RT_REUSE_MS 200
static rt_thread_t *thread_take(void) {
    u64 now = (u64)RS0(SYS_TIME);
    tlock();
    for (int i = 0; i < RT_MAX_THREADS; i++)
        if (g_threads[i] && __atomic_load_n(&g_threads[i]->state, __ATOMIC_ACQUIRE) == 0 && now - g_threads[i]->freed_ms >= RT_REUSE_MS) {
            rt_thread_t *T = g_threads[i];
            T->state = 1;
            tunlock();
            return T;
        }
    tunlock();
    rt_thread_t *T = alloc_hstack();
    if (!T) return 0;
    T->state = 1;
    tlock();
    int ok = 0;
    for (int i = 0; i < RT_MAX_THREADS; i++) if (!g_threads[i]) { g_threads[i] = T; ok = 1; break; }
    tunlock();
    if (!ok) { rt_free(T, A64RT_HSTACK); return 0; }
    return T;
}

static void thread_init(rt_thread_t *T) {
    u64 lo = (u64)T;
    memset(T, 0, sizeof *T);
    T->magic = RT_THREAD_MAGIC;
    T->self = T;
    T->hstack_lo = lo;
    T->state = 1;
    T->cpu.host = T;
}

/* ---------------------------------------------------------------- serviços do hospedeiro para o motor */
static u64 tsc_hz;
static u64 tsc0, cnt0;
static inline u64 rdtsc(void) { u32 lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return (u64)hi << 32 | lo; }
static void tsc_calibrate(void) {
    /* CPUID 0x15 (cristal) quando informa; senão mede contra o relógio do kernel (ms) */
    u32 a = 0, b = 0, c = 0, d = 0;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    if (a >= 0x15) {
        u32 den, num, cr, x;
        __asm__ volatile("cpuid" : "=a"(den), "=b"(num), "=c"(cr), "=d"(x) : "a"(0x15), "c"(0));
        if (den && num && cr) tsc_hz = (u64)cr * num / den;
    }
    if (!tsc_hz) {
        long m0 = RS0(SYS_TIME);
        while (RS0(SYS_TIME) == m0) {}
        long m1 = RS0(SYS_TIME);
        u64 t1 = rdtsc();
        while (RS0(SYS_TIME) < m1 + 20) RS1(SYS_SLEEP, 1);
        long m2 = RS0(SYS_TIME);
        u64 t2 = rdtsc();
        tsc_hz = m2 > m1 ? (t2 - t1) * 1000 / (u64)(m2 - m1) : 2000000000ULL;
    }
    tsc0 = rdtsc();
    cnt0 = (u64)RS0(SYS_TIME) * (A64_CNTFRQ / 1000);
}
a64_u64 a64_host_cntvct(void) {
    if (!tsc_hz) tsc_calibrate();
    u64 dt = rdtsc() - tsc0;
    return cnt0 + dt / tsc_hz * A64_CNTFRQ + dt % tsc_hz * A64_CNTFRQ / tsc_hz;   /* sem divisão de 128 bits */
}
void a64_host_icache_inval(a64_cpu *c, a64_u64 addr, a64_u64 len) { (void)c; a64_invalidate(addr, len); }
void a64_host_yield(a64_cpu *c) { (void)c; RS0(SYS_YIELD); }
/* JIT: regiões RWX (todo mmap do mOS é leitura+escrita+execução) */
void *a64_host_jit_alloc(a64_u64 len) { return rt_alloc(len); }
/* O kernel do mOS ainda não aplica proteção por página (mprotect só registra, machipc.c): sem tirar a escrita das
 * páginas traduzidas, a rede de segurança do JIT fica desligada e vale o IC IVAU / sys_icache_invalidate, que é o que
 * a arquitetura exige de quem gera código. protect=2 (consulta): a página é gravável. */
int a64_host_jit_wprotect(a64_u64 page, int protect) { (void)page; (void)protect; return 0; }

/* ---------------------------------------------------------------- syscalls */
static bool restartable(long n) {
    return n == SYS_READ || n == SYS_WRITE || n == SYS_WAITPID || n == SYS_WAIT || n == SYS_WAITANY || n == SYS_FUTEX_WAIT;
}

static long thread_create(u64 entry, u64 sp, u64 arg) {
    rt_thread_t *T = thread_take();
    if (!T) return -MOS_ENOMEM;
    thread_init(T);
    T->cpu.pc = entry;
    T->cpu.x[0] = arg;
    T->cpu.x[31] = sp;
    long tid = RS3(SYS_THREAD_CREATE, rt_thread_start, (u64)T + A64RT_HSTACK - 8, T);
    if (tid < 0) { __atomic_store_n(&T->state, 0, __ATOMIC_RELEASE); return tid; }
    T->tid = tid;
    return tid;
}

void rt_thread_main(rt_thread_t *T) {
    T->tid = RS0(SYS_GETTID);
    rt_run_thread(T);
}

static __attribute__((noreturn)) void thread_exit(rt_thread_t *T, long code) {
    u32 all = ~0u;
    RS4(SYS_SIG, MOS_SIGOP_MASK, MOS_SIG_SETMASK, &all, 0);   /* nada entra nesta pilha depois de liberada */
#ifdef A64_HAVE_JIT
    if (T->cpu.jit) a64_jit_thread_exit(&T->cpu);
#endif
    T->freed_ms = (u64)RS0(SYS_TIME);
    rt_thread_exit_now(&T->state, code);
}

/* thread_get_state do guest: registradores arm64 de uma thread suspensa */
static long threadctl_getstate(long tid, u64 out) {
    rt_thread_t *O = rt_thread_find(tid);
    if (!O) return -MOS_ESRCH;
    u64 regs[21];
    long r = RS3(SYS_THREADCTL, MOS_THR_GETSTATE, tid, regs);
    if (r < 0) return r;
    a64_cpu tmp = O->cpu;
#ifdef A64_HAVE_JIT
    if (O->in_guest && !O->in_svc) {
        static const int map[16] = {0, 2, 3, 1, 7, 6, 5, 4, 8, 9, 10, 11, 12, 13, 14, 15};   /* a64_hostctx ← GETSTATE */
        a64_hostctx h;
        memset(&h, 0, sizeof h);
        for (int i = 0; i < 16; i++) h.gpr[i] = regs[map[i]];
        h.rip = regs[16]; h.rflags = regs[17];
        a64_cpu t2 = O->cpu;
        if (a64_jit_recover(&t2, &h)) tmp = t2;
    }
#endif
    a64_darwin_thread_state_t s;
    memset(&s, 0, sizeof s);
    for (int i = 0; i < 29; i++) s.x[i] = tmp.x[i];
    s.fp = tmp.x[29]; s.lr = tmp.x[30]; s.sp = tmp.x[31]; s.pc = tmp.pc; s.cpsr = tmp.nzcv | (tmp.pstate & (A64_PS_DIT | A64_PS_SSBS));
    memcpy((void *)out, &s, sizeof s);
    return 0;
}

static void fork_child(rt_thread_t *T) {
    g_tlock = 0;
    for (int i = 0; i < RT_MAX_THREADS; i++)
        if (g_threads[i] && g_threads[i] != T) g_threads[i]->state = 0;   /* as outras threads não existem no filho */
    T->tid = RS0(SYS_GETTID);
    rt_sig_fork_child();
}

static void mos_svc(rt_thread_t *T) {
    a64_cpu *c = &T->cpu;
    long n = (long)c->x[8];
    u64 a = c->x[0], b = c->x[1], d = c->x[2], e = c->x[3], f = c->x[4], g = c->x[5];
    long r;
    switch (n) {
    case A64RT_SYS_SET_TSD: c->tpidrro_el0 = a; c->x[0] = 0; return;
    case A64RT_SYS_GET_TSD: c->x[0] = c->tpidrro_el0; return;
    case A64RT_SYS_ICACHE_INVAL: a64_invalidate(a, b); c->x[0] = 0; return;
    case A64RT_SYS_JIT_WP: c->x[0] = 0; return;
    case A64RT_SYS_INFO:
        c->x[0] = a == A64RT_INFO_ENGINE ? (u64)rt_engine : a == A64RT_INFO_VERSION ? 1 : (u64)-MOS_EINVAL;
        return;
    case SYS_THREAD_CREATE: c->x[0] = (u64)thread_create(a, b, d); return;
    case SYS_THREAD_EXIT: thread_exit(T, (long)a);
    case SYS_SIG: c->x[0] = (u64)rt_sig_syscall(T, (long)a, (long)b, (long)d, (long)e); return;
    case SYS_SIGACTION: c->x[0] = (u64)rt_sigaction_old(T, (long)a, (long)b, (long)d); return;
    case SYS_SIGRETURN: rt_sig_return_old(T); return;
    case SYS_THREADCTL:
        if (a == MOS_THR_GETSTATE) { c->x[0] = (u64)threadctl_getstate((long)b, d); return; }
        break;
    case SYS_FORK:
        r = RS0(SYS_FORK);
        if (r == 0) fork_child(T);
        c->x[0] = (u64)r;
        return;
    }
    if (rt_debug > 1) rt_log("a64rt[%d]: svc %ld (%lx, %lx, %lx, %lx) pc=%lx\n", (int)T->tid, n, a, b, d, e, c->pc - 4);
    T->syscall_nr = n;
    T->in_svc = 1;
    r = rt_sys(n, (long)a, (long)b, (long)d, (long)e, (long)f, (long)g);
    T->in_svc = 0;
    /* memória que deixou de ser o que era (munmap, MAP_FIXED por cima, mprotect): traduções dela não valem mais */
    if (n == SYS_MUNMAP && r == 0) a64_invalidate(a, b);
    if (n == SYS_MACH_VM && b && r == 0 && a == MACH_VM_PROTECT) a64_invalidate(((const u64 *)b)[0], ((const u64 *)b)[1]);
    if (r == -MOS_EINTR && T->pend && restartable(n)) {
        int sig = __builtin_ctz(T->pend);
        if (rt_sig_restart_ok(sig)) { c->pc -= 4; return; }   /* a syscall é refeita depois do tratador */
    }
    c->x[0] = (u64)r;
}

/* svc #0x80 do Darwin: x16 = número; erro = bit C com x0 = errno */
static int sig_darwin_to_mos(int s) {
    switch (s) { case 10: return 30; case 12: return 31; case 30: return 10; case 31: return 12; case 20: return 17; case 17: return 20; }
    return s;
}
static void darwin_svc(rt_thread_t *T) {
    a64_cpu *c = &T->cpu;
    i64 n = (i64)c->x[16];
    long r;
    if ((u64)n == 0x80000000ULL) {                     /* platform_syscall */
        if (c->x[3] == 2) { c->tpidrro_el0 = c->x[0]; c->x[0] = 0; }
        else if (c->x[3] == 3) c->x[0] = c->tpidrro_el0;
        else { rt_sig_raise_sync(T, MOS_SIGSYS, 0, c->pc - 4, A64_ESR_IL | (A64_ESR_EC_SVC << 26) | 0x80); return; }
        c->nzcv &= ~A64_C;
        return;
    }
    switch (n) {
    case 1: RS1(SYS_EXIT, c->x[0]); for (;;) {}
    case 3: r = RS3(SYS_READ, c->x[0], c->x[1], c->x[2]); break;
    case 4: r = RS3(SYS_WRITE, c->x[0], c->x[1], c->x[2]); break;
    case 20: r = RS0(SYS_GETPID); break;
    case 37: r = RS2(SYS_SIGKILL, c->x[0], sig_darwin_to_mos((int)c->x[1])); break;
    case 372: r = RS0(SYS_GETTID); break;              /* thread_selfid */
    default:
        rt_sig_raise_sync(T, MOS_SIGSYS, 0, c->pc - 4, A64_ESR_IL | (A64_ESR_EC_SVC << 26) | 0x80);
        return;
    }
    if (r < 0 && r > -4096) { c->x[0] = (u64)-r; c->nzcv |= A64_C; }
    else { c->x[0] = (u64)r; c->nzcv &= ~A64_C; }
}

/* ---------------------------------------------------------------- laço de uma thread guest */
void rt_run_thread(rt_thread_t *T) {
    a64_cpu *c = &T->cpu;
    for (;;) {
        if (T->pend) rt_sig_deliver_pending(T, 0);
        if (rt_setjmp(&T->jb)) {                       /* falta no código do guest (rt_host_signal) */
            T = rt_self();
            c = &T->cpu;
            T->in_guest = 0;
            rt_sig_raise_sync(T, T->fault_sig, T->fault_code, T->fault_addr, T->fault_esr);
            continue;
        }
        T->in_guest = 1;
        int r = a64_run(c, rt_engine);
        T->in_guest = 0;
        if (T->fault_pending) {                        /* falta dentro do JIT (rt_host_signal + a64_jit_unwind) */
            T->fault_pending = 0;
            rt_sig_raise_sync(T, T->fault_sig, T->fault_code, T->fault_addr, T->fault_esr);
            continue;
        }
        switch (r) {
        case A64_EXIT_SVC: {
            u32 imm = (u32)(c->exit_info & 0xFFFF);
            if (imm == A64RT_SVC_MOS) mos_svc(T); else darwin_svc(T);
            break;
        }
        case A64_EXIT_BRK:
            rt_sig_raise_sync(T, MOS_SIGTRAP, MOS_TRAP_BRKPT, c->pc, A64_ESR_IL | (A64_ESR_EC_BRK << 26) | (u32)(c->exit_info & 0xFFFF));
            break;
        case A64_EXIT_UDF: case A64_EXIT_HLT: case A64_EXIT_HVC:
            if (rt_debug) rt_log("a64rt[%d]: instrução %08x indefinida (saída %d) em pc=%lx\n", (int)T->tid, c->exit_insn, r, c->pc);
            rt_sig_raise_sync(T, MOS_SIGILL, MOS_ILL_ILLOPC, c->pc, A64_ESR_IL | (A64_ESR_EC_UNKNOWN << 26));
            break;
        case A64_EXIT_FAULT:
            rt_sig_raise_sync(T, MOS_SIGSEGV, MOS_SEGV_MAPERR, c->exit_info, A64_ESR_IL | (A64_ESR_EC_DABT_LO << 26) | A64_ESR_DFSC_TRANS);
            break;
        case A64_EXIT_ICACHE: a64_invalidate(c->exit_info & ~63ull, 64); break;
        case A64_EXIT_YIELD: RS0(SYS_YIELD); break;
        case A64_EXIT_STOP: break;                     /* sinal pendente: entregue no topo do laço */
        default: rt_die("saída desconhecida do motor %d em pc=%lx", r, c->pc);
        }
    }
}

/* ---------------------------------------------------------------- partida */
static void pick_engine(u64 cxs) {
    const u64 *blk = (const u64 *)cxs;
    const char *const *envp = blk ? (const char *const *)blk[8] : 0;
    for (int i = 0; envp && envp[i]; i++) {
        if (!strncmp(envp[i], "MOS_A64_DEBUG=", 14)) rt_debug = 1;
        if (strncmp(envp[i], "MOS_A64_ENGINE=", 15)) continue;
        const char *v = envp[i] + 15;
        if (!strcmp(v, "interp")) rt_engine = A64_ENGINE_INTERP;
#ifdef A64_HAVE_JIT
        else if (!strcmp(v, "jit")) rt_engine = A64_ENGINE_JIT;
#endif
        else rt_log("a64rt: MOS_A64_ENGINE=%s não disponível nesta build\n", v);
    }
}

void rt_main(const a64rt_boot_t *b) {
    if (!b || b->magic != A64RT_BOOT_MAGIC) rt_die("descritor de partida inválido");
    g_boot = b;
    rt_text_lo = b->rt_base;
    rt_text_hi = b->rt_base + b->rt_size;
    rt_thread_t *T = (rt_thread_t *)b->hstack_lo;
    if ((u64)T != (u64)rt_self()) rt_die("pilha do host desalinhada (%p, rsp em %p)", (void *)T, (void *)rt_self());
    thread_init(T);
    T->tid = RS0(SYS_GETTID);
    g_threads[0] = T;
    if (b->flags & A64RT_BOOT_DEBUG) rt_debug = b->flags & A64RT_BOOT_TRACE ? 2 : 1;
    if (b->flags & A64RT_BOOT_INTERP) rt_engine = A64_ENGINE_INTERP;
#ifdef A64_HAVE_JIT
    if (b->flags & A64RT_BOOT_JIT) rt_engine = A64_ENGINE_JIT;
#endif
    pick_engine(b->guest_x0);
    rt_sig_init();
    a64_cpu *c = &T->cpu;
    c->pc = b->guest_entry;
    c->x[0] = b->guest_x0;
    c->x[31] = b->guest_sp;
    c->x[30] = 0;
    if (rt_debug) rt_log("a64rt: entrada %lx sp %lx x0 %lx motor %d\n", c->pc, c->x[31], c->x[0], rt_engine);
    rt_run_thread(T);
}
