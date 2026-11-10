/*
 * mOS — a64rt: partida, laço das threads guest e ponte de syscalls.
 * Ver rt.h e a64rt_abi.h (contrato com o kernel e com o libSystem arm64).
 */
#include "rt.h"
#include "writes.h"
#include "a64_xmon.h"

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
/* memória do runtime (estado do JIT, cache de código, pilhas do host): sob demanda (SYS_MMAP modo 3, kernel/mm/lazy.c),
 * cada página vira RAM só no primeiro toque; o kernel alinha faixas de 2 MiB ou mais à maior potência de 2. Sem o modo
 * (harness antigo), cai para o mmap comum. */
void *rt_alloc(u64 len) {
    len = (len + 0xFFF) & ~0xFFFULL;
    long r = RS3(SYS_MMAP, len, 3, 0);
    if (r < 0 && r > -4096) r = RS1(SYS_MMAP, len);
    if (r < 0 && r > -4096) return 0;
    return (void *)r;
}
void rt_free(void *p, u64 len) { if (p) RS2(SYS_MUNMAP, p, (len + 0xFFF) & ~0xFFFULL); }

/* pilha do host alinhada ao próprio tamanho (rt_self). 2 MiB sob demanda já vêm alinhados a 2 MiB: usa a
 * primeira metade e, no caminho feliz, não devolve a sobra (munmap no meio da faixa deixa um buraco e o
 * mmap seguinte pode cair nele). Se a tabela de threads estiver cheia, solta o bloco inteiro a partir da
 * base crua — no harness, sem o modo 3, o mmap vem alinhado a 4 KiB e o ponteiro usado não é essa base. */
static void *alloc_hstack(u64 *free_base, u64 *free_len) {
    u64 raw = (u64)rt_alloc(2 * A64RT_HSTACK);
    a64rt_hstack plan;
    if (!raw) return 0;
    if (!a64rt_hstack_plan(raw, A64RT_HSTACK, &plan)) {
        rt_free((void *)raw, 2 * A64RT_HSTACK);
        return 0;
    }
    *free_base = plan.free_base;
    *free_len = plan.free_len;
    return (void *)plan.use;
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
    u64 free_base = 0, free_len = 0;
    rt_thread_t *T = alloc_hstack(&free_base, &free_len);
    if (!T) return 0;
    T->state = 1;
    tlock();
    int ok = 0;
    for (int i = 0; i < RT_MAX_THREADS; i++) if (!g_threads[i]) { g_threads[i] = T; ok = 1; break; }
    tunlock();
    if (!ok) { rt_free((void *)free_base, free_len); return 0; }
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
    T->jit_wp = 1;                     /* Apple: thread nova nasce sem poder escrever MAP_JIT */
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
/* MAP_JIT e apelidos (vm_remap das mesmas páginas). O JIT chama a64_host_jit_wprotect com J.lock tomado:
 * a leitura é seqlock, sem a trava de escrita, para não inverter a ordem com J.lock. */
#define RT_JIT_MAX   48
#define RT_ALIAS_MAX 32
typedef struct { u64 lo, hi; } rt_span_t;
typedef struct { u64 a, b, len; } rt_alias_t;
static rt_span_t g_jitmap[RT_JIT_MAX];
static rt_alias_t g_alias[RT_ALIAS_MAX];
static int g_jitmap_n, g_alias_n;
static volatile u32 g_jseq;
static volatile int g_jw;

static void jitmap_wlock(void) {
    while (__atomic_exchange_n(&g_jw, 1, __ATOMIC_ACQUIRE)) RS0(SYS_YIELD);
    __atomic_fetch_add(&g_jseq, 1, __ATOMIC_ACQ_REL);       /* par → ímpar: o leitor tenta de novo */
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
}
static void jitmap_wunlock(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    __atomic_fetch_add(&g_jseq, 1, __ATOMIC_RELEASE);       /* ímpar → par */
    __atomic_store_n(&g_jw, 0, __ATOMIC_RELEASE);
}
static u64 ld64(u64 *p) { return __atomic_load_n(p, __ATOMIC_RELAXED); }
static int ldint(int *p) { return __atomic_load_n(p, __ATOMIC_RELAXED); }

static void mirror_range(u64 addr, u64 end, u64 base, u64 other, u64 n, u64 *blo, u64 *bhi, int *any) {
    if (!n || other + n < other) return;
    u64 be = base + n;
    u64 lo = addr > base ? addr : base;
    u64 hi = end < be ? end : be;
    if (lo >= hi) return;
    u64 mlo = other + (lo - base), mhi = other + (hi - base);
    if (!*any || mlo < *blo) *blo = mlo;
    if (!*any || mhi > *bhi) *bhi = mhi;
    *any = 1;
}

int a64_jit_alias_span(a64_u64 addr, a64_u64 len, a64_u64 *lo, a64_u64 *hi) {
    if (!len || !lo || !hi) return 0;
    a64_u64 end = addr + len;
    if (end < addr) end = ~0ull;
    for (;;) {
        u32 s = __atomic_load_n(&g_jseq, __ATOMIC_ACQUIRE);
        if (s & 1) { __builtin_ia32_pause(); continue; }
        int n = ldint(&g_alias_n), any = 0;
        u64 blo = ~0ull, bhi = 0;
        if (n > RT_ALIAS_MAX) n = RT_ALIAS_MAX;
        for (int i = 0; i < n; i++) {
            u64 a = ld64(&g_alias[i].a), b = ld64(&g_alias[i].b), ln = ld64(&g_alias[i].len);
            mirror_range(addr, end, a, b, ln, &blo, &bhi, &any);
            mirror_range(addr, end, b, a, ln, &blo, &bhi, &any);
        }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_jseq, __ATOMIC_ACQUIRE) != s) continue;
        if (!any || bhi <= blo) return 0;
        *lo = blo;
        *hi = bhi;
        return 1;
    }
}

u64 rt_jit_alias_page(u64 addr) {
    a64_u64 lo, hi;
    addr &= ~0xfffULL;
    if (!a64_jit_alias_span(addr, 4096, &lo, &hi) || hi < lo + 4096) return 0;
    return lo & ~0xfffULL;
}

int rt_jit_contains(u64 addr) {
    for (;;) {
        u32 s = __atomic_load_n(&g_jseq, __ATOMIC_ACQUIRE);
        if (s & 1) { __builtin_ia32_pause(); continue; }
        int yes = 0, n = ldint(&g_jitmap_n);
        if (n > RT_JIT_MAX) n = RT_JIT_MAX;
        for (int i = 0; i < n && !yes; i++) {
            u64 lo = ld64(&g_jitmap[i].lo), hi = ld64(&g_jitmap[i].hi);
            if (addr >= lo && addr < hi) yes = 1;
        }
        n = ldint(&g_alias_n);
        if (n > RT_ALIAS_MAX) n = RT_ALIAS_MAX;
        for (int i = 0; i < n && !yes; i++) {
            u64 a = ld64(&g_alias[i].a), b = ld64(&g_alias[i].b), ln = ld64(&g_alias[i].len);
            if ((addr >= a && addr < a + ln) || (addr >= b && addr < b + ln)) yes = 1;
        }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_jseq, __ATOMIC_ACQUIRE) == s) return yes;
    }
}

void rt_jit_note_write(rt_thread_t *T, u64 addr) {
    if (!T) return;
    addr &= ~0xfffULL;
    for (int i = 0; i < T->jit_dirty_n; i++) if (T->jit_dirty[i] == addr) return;
    if (T->jit_dirty_n >= RT_JIT_DIRTY) { T->jit_dirty_all = 1; return; }
    T->jit_dirty[T->jit_dirty_n++] = addr;
}

static void rt_jit_publish(rt_thread_t *T) {
    if (T->jit_dirty_all) {
        rt_span_t spans[RT_JIT_MAX];
        rt_alias_t als[RT_ALIAS_MAX];
        int ns, na;
        jitmap_wlock();
        ns = g_jitmap_n; if (ns > RT_JIT_MAX) ns = RT_JIT_MAX;
        for (int i = 0; i < ns; i++) spans[i] = g_jitmap[i];
        na = g_alias_n; if (na > RT_ALIAS_MAX) na = RT_ALIAS_MAX;
        for (int i = 0; i < na; i++) als[i] = g_alias[i];
        jitmap_wunlock();
        for (int i = 0; i < ns; i++)
            if (spans[i].hi > spans[i].lo) a64_invalidate(spans[i].lo, spans[i].hi - spans[i].lo);
        for (int i = 0; i < na; i++) {
            a64_invalidate(als[i].a, als[i].len);
            a64_invalidate(als[i].b, als[i].len);
        }
    } else {
        for (int i = 0; i < T->jit_dirty_n; i++) a64_invalidate(T->jit_dirty[i], 4096);
    }
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    T->jit_dirty_n = 0;
    T->jit_dirty_all = 0;
}

static void rt_jit_add_region(u64 addr, u64 len) {
    if (!addr || !len) return;
    u64 lo = addr & ~0xfffULL;
    u64 hi = addr + len;
    if (hi < addr) return;
    hi = (hi + 0xfffULL) & ~0xfffULL;
    jitmap_wlock();
    for (int i = 0; i < g_jitmap_n; i++) {
        if (hi < g_jitmap[i].lo || lo > g_jitmap[i].hi) continue;
        if (lo < g_jitmap[i].lo) g_jitmap[i].lo = lo;
        if (hi > g_jitmap[i].hi) g_jitmap[i].hi = hi;
        jitmap_wunlock();
        return;
    }
    if (g_jitmap_n < RT_JIT_MAX) g_jitmap[g_jitmap_n++] = (rt_span_t){lo, hi};
    /* cheia: a faixa nova fica só com a rede PTE_JWP (a escrita depois de executar invalida) */
    jitmap_wunlock();
}

/* Rede de segurança do JIT contra código que se reescreve sem IC IVAU: proteção de escrita de verdade na PTE
 * (MACH_VM_JIT_WPROTECT, kernel/proc/process.c uvm_jit_wprotect). protect=1 tira a escrita (1 se a página era
 * gravável e não COW), 0 devolve, 2 consulta. Sem o op no kernel (kernel antigo), devolve 0: a rede fica desligada e
 * vale o IC IVAU / sys_icache_invalidate, que é o que a arquitetura exige de quem gera código.
 * A outra vista do mesmo físico recebe o mesmo op, senão a escrita pelo apelido não falta. */
static int wp_off;
static int wp_call(u64 page, int protect) {
    if (wp_off) return 0;
    u64 a[8] = {page, (u64)protect, 0, 0, 0, 0, 0, 0};
    long r = RS2(SYS_MACH_VM, MACH_VM_JIT_WPROTECT, a);
    if (r != 0) { wp_off = 1; return 0; }
    return (int)a[2];
}
int a64_host_jit_wprotect(a64_u64 page, int protect) {
    int r = wp_call(page, protect);
    u64 o = rt_jit_alias_page(page);
    if (o && o != (page & ~0xfffULL)) wp_call(o, protect);
    return r;
}

static void rt_jit_add_alias(u64 dst, u64 src, u64 len) {
    dst &= ~0xfffULL;
    src &= ~0xfffULL;
    if (len & 0xfff) len = (len + 0xfffULL) & ~0xfffULL;
    if (!dst || !src || !len || len > (1ull << 30) || dst + len < dst || src + len < src) return;
    jitmap_wlock();
    if (g_alias_n < RT_ALIAS_MAX) g_alias[g_alias_n++] = (rt_alias_t){dst, src, len};
    jitmap_wunlock();
    a64_invalidate(src, len);
    a64_invalidate(dst, len);
    /* se a vista de origem já não é gravável, a nova também deixa de ser (senão a escrita fura o JWP) */
    for (u64 p = 0; p < len; p += 4096)
        if (!a64_host_jit_wprotect(src + p, 2)) a64_host_jit_wprotect(dst + p, 1);
}

/* ---------------------------------------------------------------- syscalls */
/* a lista de syscalls refazíveis mora em rt_sig_should_restart (sig.c): o sinal que interrompeu
 * (não o de menor número pendente) decide o SA_RESTART */

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
    a64_xmon_invalidate(&T->cpu);                             /* solta a reserva e a vaga no contador global */
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
    g_jw = 0;
    /* a cópia pode ter pego o seqlock no meio da escrita: ímpar faria o leitor girar para sempre */
    if (__atomic_load_n(&g_jseq, __ATOMIC_RELAXED) & 1)
        __atomic_fetch_add(&g_jseq, 1, __ATOMIC_RELAXED);
    u64 now = (u64)RS0(SYS_TIME);
    for (int i = 0; i < RT_MAX_THREADS; i++) {
        rt_thread_t *O = g_threads[i];
        if (!O || O == T || !O->state) continue;
        /* só a thread que chamou fork existe no filho; a pilha do host das outras pode ser reusada depois da carência */
        O->state = 0;
        O->pend = 0;
        O->fault_pending = 0;
        O->cpu.excl_valid = 0;
        O->freed_ms = now;
    }
    T->tid = RS0(SYS_GETTID);
    T->pend = 0;                       /* POSIX: o filho não herda sinal pendente */
    T->fault_pending = 0;
    T->in_guest = 0;
    a64_xmon_fork_child(&T->cpu);      /* a cópia trouxe gates e travas das threads que morreram */
    rt_sig_fork_child();               /* disposição e sigaltstack já vieram na cópia; solta a trava */
#ifdef A64_HAVE_JIT
    /* cache JIT copiado (COW). O kernel mantém PTE_JWP e entrega SIGBUS na primeira escrita de cada lado,
     * então a rede de SMC continua sem esvaziar o código já traduzido. */
    a64_jit_fork_child(&T->cpu);
#endif
}

/* fork e vfork. O vfork fica um fork com espaço próprio: um vfork de verdade teria de
 * suspender o pai até o exec ou o _exit, compartilhar o pml4 (não COW) e o _exit do filho
 * não pode soltar essa memória. O exec monta um espaço novo e libera o antigo — com o pml4
 * compartilhado, levaria as páginas do pai. O a64rt, as pilhas do host e o JIT vivem nesse
 * mesmo espaço; o acerto de fork_child (outras threads mortas, tid, travas) corromperia o pai
 * se a memória fosse a mesma. POSIX deixa vfork com mais de uma thread indefinido. O monitor
 * exclusivo sai antes da syscall, para o pai e a cópia do filho nascerem limpos. */
static long rt_do_fork(rt_thread_t *T) {
    a64_xmon_invalidate(&T->cpu);      /* a exceção do fork limpa o monitor dos dois lados */
    long r = RS0(SYS_FORK);
    if (r == 0) fork_child(T);
    T->intr_sig = 0;                                   /* fork não é refazível; o filho não herda o sinal que acordou o pai */
    return r;
}

/* diagnóstico para a frente JIT (com MOS_A64_DEBUG / A64RT_DIR/depurar): as instruções que o código gerado manda ao
 * interpretador, por frequência (chave = palavra sem os campos de registrador), na saída e a cada 60 s */
#ifdef A64_HAVE_JIT
static int g_fb_n;
static void fbstat_out(a64_u32 key, a64_u64 count, void *arg) {
    (void)arg;
    rt_log("a64rt: interp %08x x %lu\n", key, count);
    g_fb_n++;
}
void fbstat_dump(const char *quando) {
    if (rt_engine != A64_ENGINE_JIT) return;
    rt_log("a64rt: instruções no interpretador (%s, pid %d):\n", quando, (int)RS0(SYS_GETPID));
    g_fb_n = 0;
    a64_jit_fbstat_dump(fbstat_out, 0, 80);
}
static volatile u64 g_fb_next;
static void fbstat_tick(void) {
    u64 now = (u64)RS0(SYS_TIME), nx = g_fb_next;
    if (!nx) { __atomic_compare_exchange_n(&g_fb_next, &nx, now + 60000, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE); return; }
    if (now >= nx && __atomic_compare_exchange_n(&g_fb_next, &nx, now + 60000, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        fbstat_dump("periódico");
}
#endif

/* escrita do kernel (ou memcpy do runtime no lugar dele) na memória do guest: não passa pela PTE, então a
 * proteção do JIT não vê. has_code evita a trava no caso comum (buffer sem tradução). */
static int peek_guest(void *ctx, u64 addr, void *dst, u64 n) {
    (void)ctx;
    if (!addr || !dst || !n) return 0;
    memcpy(dst, (const void *)addr, (size_t)n);
    return 1;
}
static void touch_code(u64 addr, u64 len) {
    if (!addr || !len) return;
#ifdef A64_HAVE_JIT
    if (!a64_jit_has_code(addr, len)) return;   /* o caso comum (buffer sem tradução) não toma a trava */
#endif
    a64_invalidate(addr, len);
}
static void touch_writes(long nr, u64 x0, u64 x1, u64 x2, u64 x3, long ret) {
    a64rt_span sp[4];
    int n = a64rt_guest_writes(nr, x0, x1, x2, x3, ret, peek_guest, 0, sp, 4);
    for (int i = 0; i < n; i++) touch_code(sp[i].addr, sp[i].len);
}

static void mos_svc(rt_thread_t *T) {
    a64_cpu *c = &T->cpu;
    long n = (long)c->x[8];
    u64 a = c->x[0], b = c->x[1], d = c->x[2], e = c->x[3], f = c->x[4], g = c->x[5];
    long r;
#ifdef A64_HAVE_JIT
    if (rt_debug) fbstat_tick();
#endif
    switch (n) {
    case A64RT_SYS_SET_TSD: c->tpidrro_el0 = a; c->x[0] = 0; return;
    case A64RT_SYS_GET_TSD: c->x[0] = c->tpidrro_el0; return;
    case A64RT_SYS_ICACHE_INVAL: a64_invalidate(a, b); c->x[0] = 0; return;
    case A64RT_SYS_JIT_WP: {
        int prev = T->jit_wp, en = a ? 1 : 0;
        if (prev && !en) { T->jit_dirty_n = 0; T->jit_dirty_all = 0; }
        if (!prev && en) rt_jit_publish(T);          /* a janela de escrita fechou: o que faltou sai do cache */
        T->jit_wp = en;
        c->x[0] = (u64)prev;
        return;
    }
    case A64RT_SYS_MAP_JIT: rt_jit_add_region(a, b); c->x[0] = 0; return;
    case A64RT_SYS_JIT_ALIAS: rt_jit_add_alias(a, b, d); c->x[0] = 0; return;
    case A64RT_SYS_INFO:
        c->x[0] = a == A64RT_INFO_ENGINE ? (u64)rt_engine : a == A64RT_INFO_VERSION ? 1 : (u64)-MOS_EINVAL;
        return;
    case SYS_THREAD_CREATE: c->x[0] = (u64)thread_create(a, b, d); return;
    case SYS_THREAD_EXIT: thread_exit(T, (long)a);
    case SYS_EXIT:
#ifdef A64_HAVE_JIT
        if (rt_debug) {                                /* diagnóstico: traduções, esvaziamentos e cache em uso */
            a64_u64 nt = 0, nf = 0, cb = 0;
            a64_jit_stats(&nt, &nf, &cb);
            rt_log("a64rt: saída %ld; JIT: %lu blocos traduzidos, %lu esvaziamentos, %lu KiB de código\n", (long)a, nt, nf, cb >> 10);
            fbstat_dump("saída");
        }
#endif
        break;
    case SYS_SIG:
        r = rt_sig_syscall(T, (long)a, (long)b, (long)d, (long)e);
        c->x[0] = (u64)r;
        touch_writes(SYS_SIG, a, b, d, e, r);
        return;
    case SYS_SIGACTION: c->x[0] = (u64)rt_sigaction_old(T, (long)a, (long)b, (long)d); return;
    case SYS_SIGRETURN: rt_sig_return_old(T); return;
    case SYS_THREADCTL:
        if (a == MOS_THR_GETSTATE) {
            r = threadctl_getstate((long)b, d);
            c->x[0] = (u64)r;
            touch_writes(SYS_THREADCTL, a, b, d, e, r);
            return;
        }
        break;
    case SYS_FORK:
        c->x[0] = (u64)rt_do_fork(T);
        return;
    }
    if (rt_debug > 1) rt_log("a64rt[%d]: svc %ld (%lx, %lx, %lx, %lx) pc=%lx\n", (int)T->tid, n, a, b, d, e, c->pc - 4);
    T->syscall_nr = n;
    T->in_svc = 1;
    r = rt_sys(n, (long)a, (long)b, (long)d, (long)e, (long)f, (long)g);
    T->in_svc = 0;
    /* munmap (também KEEP e DISCARD) tira ou zera a faixa: a tradução dela não vale mais */
    if (n == SYS_MUNMAP && r == 0) a64_invalidate(a, b);
    /* o kernel escreve na memória do usuário sem passar pela PTE. read/pread e o resto (rede, stat, mach, dyld, ...) */
    touch_writes(n, a, b, d, e, r);
    if (r == -MOS_EINTR && rt_sig_should_restart(T, n)) { c->pc -= 4; return; }   /* a syscall é refeita depois do tratador */
    T->intr_sig = 0;
    c->x[0] = (u64)r;
}

/* svc #0x80 do Darwin: x16 = número; erro = bit C com x0 = errno.
 *
 * 59 (execve) e 244 (posix_spawn) reusam SYS_EXECVE e SYS_SPAWNVEC. O layout é o do XNU
 * (xnu-10002, LP64): _posix_spawn_args_desc começa com (attr_size, attrp, file_actions_size,
 * file_actions). A attr começa com psa_flags (short). Cada file action tem 1040 bytes:
 * tipo em +0, fd em +4, open (oflag +8, mode +12, path +14), dup2 (novo fd +8), chdir (path +8).
 * O contêiner é { int alloc; int count; ações... }. FILEPORT_DUP2 não tem porta Mach aqui.
 * SETSIGDEF, SETSIGMASK, RESETIDS e START_SUSPENDED são lidos e ignorados (START_SUSPENDED
 * cruza a suspensão de processo, outra frente). Não há commpage: o endereço do Darwin arm64
 * (0xFFFFFC000) cai no mapa de identidade do kernel, abaixo de USER_BASE. */
#define PSFA_OPEN 0
#define PSFA_CLOSE 1
#define PSFA_DUP2 2
#define PSFA_INHERIT 3
#define PSFA_FILEPORT_DUP2 4
#define PSFA_CHDIR 5
#define PSFA_FCHDIR 6
#define PSFA_BYTES 1040
#define DARWIN_ENAMETOOLONG 63

static int sig_darwin_to_mos(int s) {
    switch (s) { case 10: return 30; case 12: return 31; case 30: return 10; case 31: return 12; case 20: return 17; case 17: return 20; }
    return s;
}
static int darwin_oflags(int oflag) {
    int k = 0, acc = oflag & 3;
    if (acc == 1) k |= MOS_O_WRONLY;
    if (acc == 2) k |= MOS_O_RDWR;
    if (oflag & 0x0008) k |= MOS_O_APPEND;
    if (oflag & 0x0200) k |= MOS_O_CREAT;
    if (oflag & 0x0400) k |= MOS_O_TRUNC;
    if (oflag & 0x1000000) k |= MOS_O_CLOEXEC;
    return k;
}

static long copy_guest_path(char *dst, const char *src, int cap) {
    int i;
    if (!src || cap < 2) return -MOS_EINVAL;
    for (i = 0; i < cap - 1; i++) {
        dst[i] = src[i];
        if (!src[i]) return dst[0] ? 0 : -MOS_EINVAL;
    }
    return -DARWIN_ENAMETOOLONG;
}

static long apply_setexec(const mos_spawn_fa_t *acts, unsigned n) {
    unsigned i;
    for (i = 0; i < n; i++) {
        const mos_spawn_fa_t *a = &acts[i];
        long r = 0;
        if (a->kind == MOS_SPAWN_FA_CLOSE) r = rt_sys(SYS_CLOSE, a->a, 0, 0, 0, 0, 0);
        else if (a->kind == MOS_SPAWN_FA_DUP2) r = rt_sys(SYS_DUP2, a->a, a->b, 0, 0, 0, 0);
        else if (a->kind == MOS_SPAWN_FA_CHDIR) r = rt_sys(SYS_CHDIR, (long)a->path, 0, 0, 0, 0, 0);
        else if (a->kind == MOS_SPAWN_FA_OPEN) {
            r = rt_sys(SYS_OPEN, (long)a->path, a->flags, 0, 0, 0, 0);
            if (r >= 0 && r != a->a) {
                long got = r;
                r = rt_sys(SYS_DUP2, got, a->a, 0, 0, 0, 0);
                rt_sys(SYS_CLOSE, got, 0, 0, 0, 0, 0);
            }
        } else r = -MOS_EINVAL;
        if (r < 0) return r;
    }
    return 0;
}

/* Monta o vetor do kernel a partir do args_desc. setexec: POSIX_SPAWN_SETEXEC (0x40),
 * as ações valem neste processo e quem troca a imagem é o SYS_EXECVE. */
static long spawn_fill(const void *adesc, u64 argv, u64 envp, mos_spawn_vec_t *v, mos_spawn_fa_t *acts, int *setexec) {
    memset(v, 0, sizeof *v);
    v->argv = argv;
    v->envp = envp;
    *setexec = 0;
    if (!adesc) return 0;
    unsigned long long hdr[4];
    memcpy(hdr, adesc, sizeof hdr);
    if (hdr[1] && hdr[0] >= 2) {
        unsigned short ps = 0;
        unsigned flags;
        memcpy(&ps, (const void *)hdr[1], 2);
        flags = ps;
        if (flags & 0x0002) v->flags |= MOS_SPAWN_SETPGROUP;
        if (flags & 0x0400) v->flags |= MOS_SPAWN_SETSID;
        if (flags & 0x4000) v->flags |= MOS_SPAWN_CLOEXEC_DEFAULT;
        if (flags & 0x0040) *setexec = 1;
    }
    if (!hdr[3] || hdr[2] < 8) return 0;
    {
        int count = 0;
        const unsigned char *base;
        int i, n = 0;
        memcpy(&count, (const char *)hdr[3] + 4, 4);
        if (count < 0 || count > MOS_SPAWN_FA_MAX) return -MOS_EINVAL;
        if (hdr[2] < 8ull + (unsigned long long)count * PSFA_BYTES) return -MOS_EINVAL;
        base = (const unsigned char *)hdr[3] + 8;
        for (i = 0; i < count; i++) {
            const unsigned char *a = base + (unsigned long long)i * PSFA_BYTES;
            int type = 0, filedes = 0;
            mos_spawn_fa_t *fa = &acts[n];
            long rc;
            memcpy(&type, a, 4);
            memcpy(&filedes, a + 4, 4);
            memset(fa, 0, sizeof *fa);
            if (type == PSFA_OPEN) {
                int oflag = 0;
                unsigned short mode = 0;
                memcpy(&oflag, a + 8, 4);
                memcpy(&mode, a + 12, 2);
                rc = copy_guest_path(fa->path, (const char *)(a + 14), (int)sizeof fa->path);
                if (rc) return rc;
                fa->kind = MOS_SPAWN_FA_OPEN;
                fa->flags = darwin_oflags(oflag);
                fa->mode = mode ? mode : 0644;
                fa->a = filedes;
            } else if (type == PSFA_CLOSE) {
                fa->kind = MOS_SPAWN_FA_CLOSE;
                fa->a = filedes;
            } else if (type == PSFA_DUP2 || type == PSFA_INHERIT) {
                int newfd = filedes;
                if (type == PSFA_DUP2) memcpy(&newfd, a + 8, 4);
                fa->kind = MOS_SPAWN_FA_DUP2;
                fa->a = filedes;
                fa->b = newfd;
            } else if (type == PSFA_FILEPORT_DUP2) {
                return -MOS_EINVAL;
            } else if (type == PSFA_CHDIR) {
                rc = copy_guest_path(fa->path, (const char *)(a + 8), (int)sizeof fa->path);
                if (rc) return rc;
                fa->kind = MOS_SPAWN_FA_CHDIR;
            } else if (type == PSFA_FCHDIR) {
                char pth[256];
                rc = rt_sys(SYS_FDPATH, filedes, (long)pth, (int)sizeof pth, 0, 0, 0);
                if (rc < 0) return rc;
                memcpy(fa->path, pth, sizeof fa->path);
                fa->kind = MOS_SPAWN_FA_CHDIR;
            } else return -MOS_EINVAL;
            n++;
        }
        v->acts = (unsigned long long)acts;
        v->nacts = (unsigned)n;
    }
    return 0;
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
    case 3: case 4: {                                  /* read/write: EINTR e SA_RESTART iguais ao svc do mOS */
        T->in_svc = 1;
        r = n == 3 ? RS3(SYS_READ, c->x[0], c->x[1], c->x[2]) : RS3(SYS_WRITE, c->x[0], c->x[1], c->x[2]);
        T->in_svc = 0;
        if (r > 0 && n == 3) touch_writes(SYS_READ, c->x[0], c->x[1], c->x[2], 0, r);
        if (r == -MOS_EINTR && rt_sig_should_restart(T, n == 3 ? SYS_READ : SYS_WRITE)) { c->pc -= 4; return; }
        T->intr_sig = 0;
        break;
    }
    case 2: case 66: r = rt_do_fork(T); break;         /* fork; vfork continua fork (espaço próprio, ver rt_do_fork) */
    case 59: {                                         /* execve(path, argv, envp) */
        mos_spawn_vec_t v;
        if (!c->x[0]) { r = -MOS_EINVAL; break; }
        memset(&v, 0, sizeof v);
        v.argv = c->x[1];
        v.envp = c->x[2];
        r = rt_sys(SYS_EXECVE, (long)c->x[0], (long)&v, 0, 0, 0, 0);
        break;
    }
    case 244: {                                        /* posix_spawn(pid*, path, desc, argv, envp) */
        mos_spawn_vec_t v;
        mos_spawn_fa_t acts[MOS_SPAWN_FA_MAX];
        int setexec = 0;
        if (!c->x[1]) { r = -MOS_EINVAL; break; }
        r = spawn_fill((const void *)c->x[2], c->x[3], c->x[4], &v, acts, &setexec);
        if (r < 0) break;
        if (setexec) {
            r = apply_setexec(acts, v.nacts);
            if (r < 0) break;
            v.acts = 0;
            v.nacts = 0;
            r = rt_sys(SYS_EXECVE, (long)c->x[1], (long)&v, 0, 0, 0, 0);
            break;
        }
        r = rt_sys(SYS_SPAWNVEC, (long)c->x[1], (long)&v, 0, 0, 0, 0);
        if (r > 0) {
            if (c->x[0]) {
                int pid = (int)r;
                memcpy((void *)c->x[0], &pid, 4);
                touch_code(c->x[0], 4);
            }
            r = 0;
        }
        break;
    }
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
            a64_xmon_invalidate(c);
            rt_sig_raise_sync(T, T->fault_sig, T->fault_code, T->fault_addr, T->fault_esr);
            continue;
        }
        T->in_guest = 1;
        int r = a64_run(c, rt_engine);
        T->in_guest = 0;
        if (T->fault_pending) {                        /* falta dentro do JIT (rt_host_signal + a64_jit_unwind) */
            T->fault_pending = 0;
            a64_xmon_invalidate(c);
            rt_sig_raise_sync(T, T->fault_sig, T->fault_code, T->fault_addr, T->fault_esr);
            continue;
        }
        switch (r) {
        case A64_EXIT_SVC: {
            a64_xmon_invalidate(c);                    /* SVC é exceção: a reserva não sobrevive */
            u32 imm = (u32)(c->exit_info & 0xFFFF);
            if (imm == A64RT_SVC_MOS) mos_svc(T); else darwin_svc(T);
            break;
        }
        case A64_EXIT_BRK:
        case A64_EXIT_UDF: case A64_EXIT_HLT: case A64_EXIT_HVC:
        case A64_EXIT_FAULT:
            a64_xmon_invalidate(c);                /* BRK, indefinida e falta são exceção: a reserva não sobrevive */
            if (rt_debug && r != A64_EXIT_BRK && r != A64_EXIT_FAULT)
                rt_log("a64rt[%d]: instrução %08x indefinida (saída %d) em pc=%lx\n", (int)T->tid, c->exit_insn, r, c->pc);
            rt_sig_from_motor(T, r);
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
#ifdef A64_HAVE_JIT
    if (rt_debug && rt_engine == A64_ENGINE_JIT) a64_jit_fbstat(1);
#endif
    a64_cpu *c = &T->cpu;
    c->pc = b->guest_entry;
    c->x[0] = b->guest_x0;
    c->x[31] = b->guest_sp;
    c->x[30] = 0;
    if (rt_debug) rt_log("a64rt: entrada %lx sp %lx x0 %lx motor %d\n", c->pc, c->x[31], c->x[0], rt_engine);
    rt_run_thread(T);
}

/* sig.c: o processo vai morrer por um sinal (sem SYS_EXIT): o diagnóstico do JIT sai antes */
void rt_fbstat_final(const char *quando) {
#ifdef A64_HAVE_JIT
    fbstat_dump(quando);
#else
    (void)quando;
#endif
}
