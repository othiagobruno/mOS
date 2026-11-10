/*
 * mOS — a64rt: sinais do processo arm64.
 *
 * O runtime guarda as ações do guest e instala no kernel as suas (rt_host_tramp → rt_host_signal):
 *   - guest SIG_DFL/SIG_IGN → mesma disposição no host (o kernel encerra/ignora sozinho), exceto SEGV/BUS/
 *     ILL/TRAP/FPE, sempre com tratador do runtime (faltas dentro do código traduzido precisam virar o estado
 *     do guest).
 *   - guest com tratador → tratador do host, sem SA_RESTART (o runtime decide refazer a syscall do guest).
 * Sinal assíncrono: o tratador do host anota o sinal na thread, já aplica a máscara que valerá no tratador do
 * guest (uc_sigmask do host) e pede a64_request_stop; o laço da thread monta UM quadro arm64 por vez (siginfo +
 * ucontext + mcontext64 do Darwin) na pilha do guest e desvia para o _sigtramp do libSystem arm64. O próximo
 * pendente só sai depois do sigreturn. No JIT o pedido de parada é lido nas arestas para trás e nos saltos
 * indiretos (estado do guest já na memória); no interpretador, entre instruções.
 * Falta síncrona no código do guest: estado do guest reconstruído (a64_jit_recover no JIT; no interpretador
 * o a64_cpu já está no início da instrução, pelo contrato do motor) e "longjmp" para o laço, que entrega
 * SIGSEGV/SIGBUS/SIGILL/SIGFPE/SIGTRAP com far/esr e pc do guest. Falta de alinhamento do motor (A64_EXIT_FAULT)
 * é SIGBUS, não SIGSEGV. Máscara e sigsuspend/sigwait/tkill ficam no kernel (a máscara do host é a do guest).
 * A syscall interrompida olha o sinal que a acordou (intr_sig), não o de menor número que estiver pendente.
 */
#include "rt.h"

void rt_host_tramp(void);

typedef struct { u64 handler, tramp; u32 mask, flags; } gact_t;
static gact_t g_act[MOS_NSIG];
static volatile int g_alock;
static u64 g_restorer;                  /* SYS_SIGACTION antigo */

typedef struct {
    mos_siginfo_t info;                 /* 104 */
    u32 pad0[2];
    a64_darwin_ucontext_t uc;           /* 56 */
    u64 pad1;
    a64_darwin_mcontext_t mc;           /* 816, 16n */
} gframe_t;
_Static_assert(__builtin_offsetof(gframe_t, mc) % 16 == 0, "mcontext alinhado");

#define BIT(s) (1u << (s))
#define SYNC_SET (BIT(MOS_SIGSEGV) | BIT(MOS_SIGBUS) | BIT(MOS_SIGILL) | BIT(MOS_SIGTRAP) | BIT(MOS_SIGFPE))
/* ação padrão "ignorar" */
#define DFL_IGN_SET (BIT(MOS_SIGCHLD) | BIT(MOS_SIGURG) | BIT(MOS_SIGWINCH) | BIT(MOS_SIGINFO) | BIT(MOS_SIGCONT) | BIT(MOS_SIGIO))

static void alock(void) { while (__atomic_exchange_n(&g_alock, 1, __ATOMIC_ACQUIRE)) RS0(SYS_YIELD); }
static void aunlock(void) { __atomic_store_n(&g_alock, 0, __ATOMIC_RELEASE); }

static long sigop(long op, long a, long b, long c) { return RS4(SYS_SIG, op, a, b, c); }

static void host_install(int sig) {
    gact_t a = g_act[sig];
    mos_sigaction_t h = {0, 0, 0, 0};
    bool ours = (SYNC_SET & BIT(sig)) || a.handler > MOS_SIG_IGN;
    if (ours) {
        h.handler = (u64)rt_host_signal;
        h.tramp = (u64)rt_host_tramp;
        h.mask = ~0u;                                   /* o tratador do host roda curto e sem ser interrompido */
        h.flags = MOS_SA_SIGINFO;
    } else h.handler = a.handler;                       /* DFL ou IGN */
    h.flags |= a.flags & (MOS_SA_NOCLDSTOP | MOS_SA_NOCLDWAIT);
    sigop(MOS_SIGOP_ACTION, sig, (long)&h, 0);
}

void rt_sig_init(void) {
    for (int s = 1; s < MOS_NSIG; s++) {
        mos_sigaction_t o;
        memset(&o, 0, sizeof o);
        if (sigop(MOS_SIGOP_ACTION, s, 0, (long)&o) == 0 && o.handler == MOS_SIG_IGN) g_act[s].handler = MOS_SIG_IGN;   /* herdado do exec */
        if (SYNC_SET & BIT(s)) host_install(s);
    }
}

void rt_sig_fork_child(void) { g_alock = 0; }

bool rt_sig_restart_ok(int sig) { return g_act[sig].handler > MOS_SIG_IGN && (g_act[sig].flags & MOS_SA_RESTART); }

static u32 cur_mask(void) { u32 m = 0; sigop(MOS_SIGOP_MASK, MOS_SIG_BLOCK, 0, (long)&m); return m; }
static void set_mask(u32 m) { sigop(MOS_SIGOP_MASK, MOS_SIG_SETMASK, (long)&m, 0); }

/* morre (ou para) pelo sinal com a ação padrão do kernel: disposição DFL, desbloqueia e manda para si mesma */
static __attribute__((noreturn)) void reraise_default(rt_thread_t *T, int sig) {
    if (rt_debug && !(DFL_IGN_SET & BIT(sig))) rt_fbstat_final("morte por sinal");
    mos_sigaction_t d = {0, 0, 0, 0};
    sigop(MOS_SIGOP_ACTION, sig, (long)&d, 0);
    u32 m = BIT(sig);
    sigop(MOS_SIGOP_MASK, MOS_SIG_UNBLOCK, (long)&m, 0);
    for (;;) {
        sigop(MOS_SIGOP_TKILL, T->tid, sig, 0);
        RS0(SYS_YIELD);
        if (DFL_IGN_SET & BIT(sig)) RS1(SYS_EXIT, 128 + sig);  /* não devia: a ação padrão não encerra */
    }
}

/* ---------------------------------------------------------------- entrega ao guest */
/* 1 = o pc do guest agora é o tratador (o laço tem que deixá-lo rodar); 0 = sinal descartado */
static int deliver(rt_thread_t *T, int sig, const mos_siginfo_t *si, u32 oldmask, u32 esr, bool fault) {
    if (rt_debug) rt_log("a64rt[%d]: entrega do sinal %d (falta %d, pc %lx, tratador %lx, tramp %lx)\n", (int)T->tid, sig, (int)fault, T->cpu.pc, g_act[sig].handler, g_act[sig].tramp);
    a64_cpu *c = &T->cpu;
    gact_t a = g_act[sig];
    if (a.handler <= MOS_SIG_IGN) {
        if (a.handler == MOS_SIG_IGN || ((DFL_IGN_SET & BIT(sig)) && !fault)) { set_mask(oldmask); return 0; }
        reraise_default(T, sig);
    }
    u64 gsp = c->x[31];
    bool onalt = T->ss_size && gsp > T->ss_sp && gsp <= T->ss_sp + T->ss_size;
    bool usealt = (a.flags & MOS_SA_ONSTACK) && T->ss_size && !onalt;
    u64 top = usealt ? T->ss_sp + T->ss_size : gsp - 128;           /* red zone do arm64 da Apple */
    u64 fa = (top - sizeof(gframe_t)) & ~15ULL;
    gframe_t f;
    memset(&f, 0, sizeof f);
    f.info = *si;
    f.info.si_signo = sig;
    f.uc.uc_onstack = onalt || usealt;
    f.uc.uc_sigmask = oldmask;
    f.uc.ss_sp = T->ss_sp; f.uc.ss_size = T->ss_size;
    f.uc.ss_flags = !T->ss_size ? MOS_SS_DISABLE : (onalt || usealt) ? MOS_SS_ONSTACK : 0;
    f.uc.uc_mcsize = sizeof(a64_darwin_mcontext_t);
    f.uc.uc_mcontext = fa + __builtin_offsetof(gframe_t, mc);
    a64_darwin_mcontext_t *mc = &f.mc;
    if (fault || esr) { mc->es.far = si->si_addr; mc->es.esr = esr; mc->es.exception = esr >> 26; }
    for (int i = 0; i < 29; i++) mc->ss.x[i] = c->x[i];
    mc->ss.fp = c->x[29]; mc->ss.lr = c->x[30]; mc->ss.sp = c->x[31]; mc->ss.pc = c->pc;
    mc->ss.cpsr = (c->nzcv & 0xF0000000u) | (c->pstate & (A64_PS_DIT | A64_PS_SSBS));
    for (int i = 0; i < 32; i++) { mc->ns.v[i][0] = c->v[i].d[0]; mc->ns.v[i][1] = c->v[i].d[1]; }
    mc->ns.fpsr = c->fpsr; mc->ns.fpcr = c->fpcr;
    T->sig_depth++;
    memcpy((void *)fa, &f, sizeof f);                  /* pilha do guest estourada: falta fora do guest → morre (macOS) */
    T->sig_depth--;
    set_mask(oldmask | a.mask | ((a.flags & MOS_SA_NODEFER) ? 0 : BIT(sig)));
    if ((a.flags & MOS_SA_RESETHAND) && sig != MOS_SIGILL && sig != MOS_SIGTRAP) {
        alock();
        g_act[sig].handler = MOS_SIG_DFL; g_act[sig].tramp = 0; g_act[sig].flags &= ~MOS_SA_SIGINFO;
        host_install(sig);
        aunlock();
    }
    c->x[31] = fa;
    c->excl_valid = 0;                                 /* exceção zera o monitor local antes do tratador */
    c->pstate &= ~(3u << A64_PS_BTYPE_SHIFT);
    if (a.tramp) {                                     /* _sigtramp(handler, estilo, sinal, siginfo*, ucontext*, token) */
        c->pc = a.tramp;
        c->x[0] = a.handler;
        c->x[1] = (a.flags & MOS_SA_SIGINFO) ? 30 : 1;
        c->x[2] = (u64)sig;
        c->x[3] = fa + __builtin_offsetof(gframe_t, info);
        c->x[4] = fa + __builtin_offsetof(gframe_t, uc);
        c->x[5] = 0;
        c->x[30] = 0;
    } else {                                           /* handler(sinal, siginfo*, ucontext*), volta pelo restorer */
        c->pc = a.handler;
        c->x[0] = (u64)sig;
        c->x[1] = fa + __builtin_offsetof(gframe_t, info);
        c->x[2] = fa + __builtin_offsetof(gframe_t, uc);
        c->x[30] = g_restorer;
    }
    return 1;
}

void rt_sig_deliver_pending(rt_thread_t *T, int unused) {
    (void)unused;
    while (T->pend) {
        u32 p = __atomic_load_n(&T->pend, __ATOMIC_ACQUIRE);
        int sig = __builtin_ctz(p);
        mos_siginfo_t si = T->pinfo[sig];
        u32 om = T->pend_oldmask[sig], esr = T->pesr[sig];
        __atomic_and_fetch(&T->pend, ~BIT(sig), __ATOMIC_ACQ_REL);
        if (deliver(T, sig, &si, om, esr, false)) return;   /* um por vez: o sigreturn volta ao laço, que entrega o próximo */
    }
}

void rt_sig_raise_sync(rt_thread_t *T, int sig, int code, u64 addr, u32 esr) {
    u32 m = cur_mask();
    if ((m & BIT(sig)) || g_act[sig].handler <= MOS_SIG_IGN) {
        if (rt_debug) {
            rt_log("a64rt: sinal %d (código %d) em pc=%lx endereço %lx sem tratador: encerrando\n", sig, code, T->cpu.pc, addr);
            rt_log("  lr=%lx sp=%lx x0=%lx x1=%lx x2=%lx\n", T->cpu.x[30], T->cpu.x[31], T->cpu.x[0], T->cpu.x[1], T->cpu.x[2]);
            u64 fp = T->cpu.x[29];   /* cadeia de quadros (x29, x30) do guest */
            for (int i = 0; i < 16 && fp && !(fp & 7) && fp >= 0x400000000000ULL && fp < 0x400100000000ULL; i++) {
                rt_log("  #%d %lx\n", i, ((const u64 *)fp)[1]);
                fp = ((const u64 *)fp)[0];
            }
        }
        reraise_default(T, sig);                       /* bloqueado ou sem tratador: o macOS mata */
    }
    mos_siginfo_t si;
    memset(&si, 0, sizeof si);
    si.si_signo = sig; si.si_code = code; si.si_addr = addr; si.si_pid = (int)RS0(SYS_GETPID); si.si_uid = 501;
    deliver(T, sig, &si, m, esr, true);
}

/* volta do tratador do guest: registradores do mcontext (quem o alterou retoma noutro ponto) e a máscara */
static void sig_return(rt_thread_t *T, u64 ucva) {
    a64_cpu *c = &T->cpu;
    const a64_darwin_ucontext_t *uc = (const a64_darwin_ucontext_t *)ucva;
    const a64_darwin_mcontext_t *mc = (const a64_darwin_mcontext_t *)uc->uc_mcontext;
    if (!ucva || !uc->uc_mcontext) rt_die("sigreturn com contexto nulo");
    for (int i = 0; i < 29; i++) c->x[i] = mc->ss.x[i];
    c->x[29] = mc->ss.fp; c->x[30] = mc->ss.lr; c->x[31] = mc->ss.sp; c->pc = mc->ss.pc;
    c->nzcv = mc->ss.cpsr & 0xF0000000u;
    c->pstate = (c->pstate & ~(A64_PS_DIT | A64_PS_SSBS)) | (mc->ss.cpsr & (A64_PS_DIT | A64_PS_SSBS));
    for (int i = 0; i < 32; i++) { c->v[i].d[0] = mc->ns.v[i][0]; c->v[i].d[1] = mc->ns.v[i][1]; }
    c->fpsr = mc->ns.fpsr & A64_FPSR_MASK;
    c->fpcr = mc->ns.fpcr & A64_FPCR_MASK;
    c->excl_valid = 0;                                  /* exceção limpa o monitor local */
    set_mask(uc->uc_sigmask);
}

void rt_sig_return_old(rt_thread_t *T) { sig_return(T, T->cpu.x[31] + __builtin_offsetof(gframe_t, uc)); }

long rt_sigaction_old(rt_thread_t *T, long sig, long handler, long restorer) {
    (void)T;
    if (sig <= 0 || sig >= MOS_NSIG || sig == MOS_SIGKILL || sig == MOS_SIGSTOP) return -MOS_EINVAL;
    alock();
    long old = (long)g_act[sig].handler;
    g_act[sig].handler = (u64)handler;
    g_act[sig].tramp = 0;
    g_act[sig].mask = 0;
    g_act[sig].flags = MOS_SA_SIGINFO;
    if (restorer) g_restorer = (u64)restorer;
    host_install((int)sig);
    aunlock();
    return old;
}

long rt_sig_syscall(rt_thread_t *T, long op, long a, long b, long c) {
    if (rt_debug > 1) rt_log("a64rt[%d]: SYS_SIG op %ld (%lx, %lx, %lx)\n", (int)T->tid, op, a, b, c);
    switch (op) {
    case MOS_SIGOP_ACTION: {
        int sig = (int)a;
        if (sig <= 0 || sig >= MOS_NSIG || (b && (sig == MOS_SIGKILL || sig == MOS_SIGSTOP))) return -MOS_EINVAL;
        mos_sigaction_t n;
        if (b) memcpy(&n, (const void *)b, sizeof n);
        alock();
        gact_t o = g_act[sig];
        if (b) {
            g_act[sig].handler = n.handler;
            g_act[sig].tramp = n.handler > MOS_SIG_IGN ? n.tramp : 0;
            g_act[sig].mask = n.mask;
            g_act[sig].flags = n.flags;
            host_install(sig);
            if (n.handler == MOS_SIG_IGN || (n.handler == MOS_SIG_DFL && (DFL_IGN_SET & BIT(sig))))
                __atomic_and_fetch(&T->pend, ~BIT(sig), __ATOMIC_ACQ_REL);
        }
        aunlock();
        if (c) { mos_sigaction_t ov = {o.handler, o.tramp, o.mask, o.flags}; memcpy((void *)c, &ov, sizeof ov); }
        return 0;
    }
    case MOS_SIGOP_PENDING: {
        u32 s = 0;
        long r = sigop(MOS_SIGOP_PENDING, (long)&s, 0, 0);
        if (r < 0) return r;
        s |= T->pend;
        memcpy((void *)a, &s, 4);
        return 0;
    }
    case MOS_SIGOP_ALTSTACK: {
        u64 gsp = T->cpu.x[31];
        bool onalt = T->ss_size && gsp > T->ss_sp && gsp <= T->ss_sp + T->ss_size;
        mos_sigstack_t o = {T->ss_sp, T->ss_size, !T->ss_size ? MOS_SS_DISABLE : onalt ? MOS_SS_ONSTACK : 0, 0}, n;
        if (a) memcpy(&n, (const void *)a, sizeof n);
        if (b) memcpy((void *)b, &o, sizeof o);
        if (!a) return 0;
        if (onalt) return -MOS_EPERM;
        if (n.flags & MOS_SS_DISABLE) { T->ss_sp = 0; T->ss_size = 0; return 0; }
        if (n.flags & ~MOS_SS_DISABLE) return -MOS_EINVAL;
        if (n.size < 32768) return -MOS_ENOMEM;         /* MINSIGSTKSZ do Darwin arm64 (e do kernel) = 32 KiB */
        T->ss_sp = n.sp; T->ss_size = n.size;
        return 0;
    }
    case MOS_SIGOP_RETURN: sig_return(T, (u64)a); return (long)T->cpu.x[0];
    default:                                            /* MASK, SUSPEND, TKILL, WAIT: no kernel */
        return sigop(op, a, b, c);
    }
}

/* A64_EXIT_FAULT é sempre alinhamento (o interpretador não devolve FAULT para página ausente: isso vira
 * falta do host). PC desalinhado: o desvio já se completou e c->pc == FAR. SP desalinhado com FAR == SP: SA0. */
static int fault_is_store(u32 i) {
    if (((i >> 26) & 1) == 0 && ((i >> 28) & 3) == 0 && ((i >> 24) & 1) == 0) {
        unsigned o2 = (i >> 23) & 1, L = (i >> 22) & 1, o1 = (i >> 21) & 1;
        if (o2 && o1) return 1;                         /* CAS* escreve */
        return !L;                                      /* LDXR/LDAR = load, STXR/STLR = store */
    }
    /* LSE (LDADD, SWP, ...): bit 21 e bits 11:10 == 0 no grupo de load/store com registrador */
    if (((i >> 26) & 1) == 0 && ((i >> 21) & 1) && ((i >> 10) & 3) == 0) return 1;
    return 0;
}

void rt_sig_classify_fault(const a64_cpu *c, int *sig, int *code, u32 *esr) {
    u64 far = c->exit_info;
    *sig = MOS_SIGBUS;
    *code = MOS_BUS_ADRALN;
    if (c->pc & 3) {
        *esr = A64_ESR_IL | (A64_ESR_EC_PCALIGN << 26);
        return;
    }
    if ((c->x[31] & 15) && far == c->x[31]) {
        *esr = A64_ESR_IL | (A64_ESR_EC_SPALIGN << 26);
        return;
    }
    *esr = A64_ESR_IL | (A64_ESR_EC_DABT_LO << 26) | (fault_is_store(c->exit_insn) ? A64_ESR_WNR : 0) | A64_ESR_DFSC_ALIGN;
}

u32 rt_sig_esr_host(int sig, u32 err, u64 addr, u64 guest_pc) {
    if (sig == MOS_SIGILL) return A64_ESR_IL | (A64_ESR_EC_UNKNOWN << 26);
    if (sig == MOS_SIGTRAP) return A64_ESR_IL | (A64_ESR_EC_BRK << 26);
    if (sig == MOS_SIGFPE) return A64_ESR_IL | (A64_ESR_EC_FP << 26);
    int present = (err & 1) != 0;
    if (addr == guest_pc || (err & 0x10))
        return A64_ESR_IL | (A64_ESR_EC_IABT_LO << 26) | (present ? A64_ESR_DFSC_PERM : A64_ESR_DFSC_TRANS);
    return A64_ESR_IL | (A64_ESR_EC_DABT_LO << 26) | ((err & 2) ? A64_ESR_WNR : 0) | (present ? A64_ESR_DFSC_PERM : A64_ESR_DFSC_TRANS);
}

void rt_sig_from_motor(rt_thread_t *T, int reason) {
    a64_cpu *c = &T->cpu;
    switch (reason) {
    case A64_EXIT_BRK:
        rt_sig_raise_sync(T, MOS_SIGTRAP, MOS_TRAP_BRKPT, c->pc,
                          A64_ESR_IL | (A64_ESR_EC_BRK << 26) | (u32)(c->exit_info & 0xFFFF));
        break;
    case A64_EXIT_UDF:
    case A64_EXIT_HLT:
    case A64_EXIT_HVC:
        rt_sig_raise_sync(T, MOS_SIGILL, MOS_ILL_ILLOPC, c->pc, A64_ESR_IL | (A64_ESR_EC_UNKNOWN << 26));
        break;
    case A64_EXIT_FAULT: {
        int sig, code;
        u32 esr;
        rt_sig_classify_fault(c, &sig, &code, &esr);
        rt_sig_raise_sync(T, sig, code, c->exit_info, esr);
        break;
    }
    default:
        break;
    }
}

static int syscall_restartable(long n) {
    return n == SYS_READ || n == SYS_WRITE || n == SYS_WAITPID || n == SYS_WAIT || n == SYS_WAITANY || n == SYS_FUTEX_WAIT;
}

bool rt_sig_should_restart(rt_thread_t *T, long nr) {
    int sig = T->intr_sig;
    T->intr_sig = 0;
    if (sig <= 0 || sig >= MOS_NSIG) return false;
    if (!(__atomic_load_n(&T->pend, __ATOMIC_ACQUIRE) & BIT(sig))) return false;
    if (!syscall_restartable(nr)) return false;
    return rt_sig_restart_ok(sig);
}

/* ---------------------------------------------------------------- tratador do host */
static void hostctx_from(a64_hostctx *h, const mos_mcontext_t *mc) {
    const mos_thread_state_t *s = &mc->ss;
    h->gpr[0] = s->rax; h->gpr[1] = s->rcx; h->gpr[2] = s->rdx; h->gpr[3] = s->rbx;
    h->gpr[4] = s->rsp; h->gpr[5] = s->rbp; h->gpr[6] = s->rsi; h->gpr[7] = s->rdi;
    h->gpr[8] = s->r8; h->gpr[9] = s->r9; h->gpr[10] = s->r10; h->gpr[11] = s->r11;
    h->gpr[12] = s->r12; h->gpr[13] = s->r13; h->gpr[14] = s->r14; h->gpr[15] = s->r15;
    h->rip = s->rip; h->rflags = s->rflags;
    memcpy(h->xmm, mc->fx + 160, 16 * 16);              /* FXSAVE: xmm0 em +160 */
}

void rt_host_signal(int sig, mos_siginfo_t *si, mos_ucontext_t *uc) {
    rt_thread_t *T = rt_self();
    if (T->magic != RT_THREAD_MAGIC || T->self != T) return;   /* fora de uma pilha do runtime: impossível */
    mos_mcontext_t *mc = (mos_mcontext_t *)uc->uc_mcontext;
    bool sync = (SYNC_SET & BIT(sig)) && si->si_code != MOS_SI_USER && si->si_code != MOS_SI_QUEUE;
    if (sync) {
        a64_cpu *c = &T->cpu;
        bool mem = sig == MOS_SIGSEGV || sig == MOS_SIGBUS;
        bool in_translated = T->in_guest && !T->in_svc;
#ifdef A64_HAVE_JIT
        /* escrita numa página que o JIT protegeu: só retomar (a escrita é refeita). Vale também fora do guest: o próprio
         * runtime escreve na memória do guest (quadros de sinal, structs de syscall emuladas) */
        if ((sig == MOS_SIGSEGV || sig == MOS_SIGBUS) && (mc->err & 2) && a64_jit_write_fault(si->si_addr)) return;
        a64_hostctx h;
        memset(&h, 0, sizeof h);
        bool jit_pc = false;
        if (c->jit && in_translated) {
            hostctx_from(&h, mc);
            jit_pc = a64_jit_recover(c, &h);            /* false: falta no interpretador, o a64_cpu já vale */
        }
#else
        bool jit_pc = false;
        (void)hostctx_from;
#endif
        /* SEGV/BUS: qualquer falta com in_guest (o interpretador desreferencia ponteiro do guest).
         * ILL/FPE/TRAP do host: só se o RIP está no código gerado — UDF/BRK do guest saem pelo motor, e um
         * ud2 no meio do runtime não é instrução do guest. */
        if ((mem && !in_translated) || (!mem && !jit_pc)) {
            rt_log("a64rt: %s %d no próprio runtime: rip=%lx endereço=%lx (pc do guest %lx, tid %d%s)\n",
                   T->sig_depth ? "sem pilha do guest para o sinal; falta" : "falta", sig, mc->ss.rip, si->si_addr,
                   c->pc, (int)T->tid, T->in_svc ? ", numa syscall" : "");
            mos_sigaction_t d = {0, 0, 0, 0};
            sigop(MOS_SIGOP_ACTION, sig, (long)&d, 0);   /* volta e repete a instrução: o kernel encerra pelo sinal */
            return;
        }
        T->fault_sig = sig;
        T->fault_code = si->si_code;
        T->fault_addr = mem ? si->si_addr : c->pc;      /* ILL/FPE/TRAP: si_addr do host é o RIP x86; o guest vê o pc */
        T->fault_esr = rt_sig_esr_host(sig, mc->err, T->fault_addr, c->pc);
        mos_thread_state_t *s = &mc->ss;
#ifdef A64_HAVE_JIT
        /* dentro do a64_jit_run (código gerado ou o interpretador chamado por ele): o JIT reconstrói o estado do guest
         * (pc na instrução que falhou) e faz o a64_jit_run devolver; o laço entrega o sinal (fault_pending) */
        if (c->jit) {
            if (a64_jit_unwind(c, &h, A64_EXIT_STOP)) {
                T->fault_pending = 1;
                s->rsp = h.gpr[4]; s->rax = h.gpr[0]; s->rip = h.rip;
                return;
            }
            a64_jit_abort(c);                           /* longjmp para fora do a64_jit_run: solta o estado da thread */
        }
#endif
        /* interpretador: "longjmp" para o rt_setjmp do laço; o kernel restaura estes registradores na volta */
        s->rbx = T->jb.rbx; s->rbp = T->jb.rbp; s->r12 = T->jb.r12; s->r13 = T->jb.r13;
        s->r14 = T->jb.r14; s->r15 = T->jb.r15; s->rsp = T->jb.rsp; s->rip = T->jb.rip; s->rax = 1;
        s->rflags &= ~0x400ULL;                         /* DF limpo */
        return;
    }
    /* assíncrono: anota e para o motor; a máscara do tratador do guest já vale na volta deste */
    gact_t a = g_act[sig];
    if (a.handler == MOS_SIG_IGN) return;
    if (!(T->pend & BIT(sig))) {
        T->pinfo[sig] = *si;
        T->pend_oldmask[sig] = uc->uc_sigmask;
        T->pesr[sig] = 0;
        __atomic_or_fetch(&T->pend, BIT(sig), __ATOMIC_ACQ_REL);
    }
    if (a.handler > MOS_SIG_IGN) uc->uc_sigmask |= a.mask | ((a.flags & MOS_SA_NODEFER) ? 0 : BIT(sig));
    if (T->in_svc && !T->intr_sig) T->intr_sig = sig;   /* o que interrompeu a syscall, mesmo que outro já estivesse pendente */
    a64_request_stop(&T->cpu);
}
