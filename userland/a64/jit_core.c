/*
 * mOS — tradutor arm64 (frente JIT): núcleo do tradutor dinâmico.
 *   - regiões de código (RWX) e de metadados, stubs comuns, tabela de helpers
 *   - despachante (a64_jit_run): cache de saltos por thread → hash global → tradução
 *   - encadeamento direto de blocos (remendo atômico do rel32), desencadeamento na invalidação
 *   - invalidação por intervalo, proteção de escrita das páginas traduzidas (rede de segurança)
 *     com páginas "voláteis" passando a blocos que conferem o próprio código
 *   - esvaziamento total com quiescência das threads quando o cache enche
 *   - recuperação exata do estado do guest numa falha dentro do código gerado
 * Multithread: cache COMPARTILHADO; uma trava global para tradução/encadeamento/invalidação,
 * leituras do cache por thread sem trava. Código nunca é liberado enquanto alguma thread pode
 * estar dentro dele (só no esvaziamento total, com todas as threads fora).
 */
#include "jit.h"
#include "a64_mem.h"

#ifdef _WIN32
#include <windows.h>
#endif

jit_globals jit_g;

/* ---------------------------------------------------------------------------------------------
 * Host: memória executável e proteção de páginas
 * ------------------------------------------------------------------------------------------- */
#ifdef _WIN32
static void *host_alloc(a64_u64 n) { return VirtualAlloc(NULL, (SIZE_T)n, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE); }
/* protect: 1 = tirar escrita se gravável (1 se fez); 0 = devolver escrita; 2 = consultar se gravável */
static int host_wprotect(a64_u64 page, int protect) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((void *)(uintptr_t)page, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT) return 0;
    DWORD p = mbi.Protect & 0xFF, np, old;
    if (protect == 2) return p == PAGE_READWRITE || p == PAGE_EXECUTE_READWRITE || p == PAGE_WRITECOPY || p == PAGE_EXECUTE_WRITECOPY;
    if (protect) {
        if (p == PAGE_READWRITE) np = PAGE_READONLY;
        else if (p == PAGE_EXECUTE_READWRITE) np = PAGE_EXECUTE_READ;
        else return 0;
    } else {
        if (p == PAGE_READONLY) np = PAGE_READWRITE;
        else if (p == PAGE_EXECUTE_READ) np = PAGE_EXECUTE_READWRITE;
        else return 1;
    }
    return VirtualProtect((void *)(uintptr_t)page, (SIZE_T)JIT_PAGE, np, &old) ? 1 : 0;
}
#else
static void *host_alloc(a64_u64 n) { return a64_host_jit_alloc(n); }
static int host_wprotect(a64_u64 page, int protect) { return a64_host_jit_wprotect(page, protect); }
#endif

/* ---------------------------------------------------------------------------------------------
 * Travas (spin; seções curtas)
 * ------------------------------------------------------------------------------------------- */
typedef volatile int jlock;
static inline void lk(jlock *l) {
    while (__atomic_exchange_n(l, 1, __ATOMIC_ACQUIRE)) while (*l) __builtin_ia32_pause();
}
static inline void ulk(jlock *l) { __atomic_store_n(l, 0, __ATOMIC_RELEASE); }

/* ---------------------------------------------------------------------------------------------
 * Estado global
 * ------------------------------------------------------------------------------------------- */
/* tamanhos: no Windows (harness) a reserva sai barata; no mOS o mmap materializa as páginas na
 * hora, então regiões menores (cache cheio = esvaziamento total e recomeço). A64_JIT_CODE_MIB
 * na build muda o tamanho do código (dados = metade, blocos = código/128 B). */
#ifndef A64_JIT_CODE_MIB
#ifdef _WIN32
#define A64_JIT_CODE_MIB 64
#else
#define A64_JIT_CODE_MIB 8
#endif
#endif
#ifdef A64_JIT_CODE_KIB                 /* só para testar o esvaziamento */
#define CODE_SIZE   ((a64_u64)A64_JIT_CODE_KIB << 10)
#else
#define CODE_SIZE   ((a64_u64)A64_JIT_CODE_MIB << 20)
#endif
#define DATA_SIZE   (CODE_SIZE / 2)
#define MAX_BLKS    ((a64_u32)(CODE_SIZE / 128))
#define HASH_BITS   16
#define PGH_BITS    14
#define VOLATILE_AFTER 4            /* invalidações por escrita até a página virar volátil */

typedef struct jpage {
    a64_u64 page;
    jblk *blocks;
    struct jpage *next;
    int wp, wp_ever, ninval, vol, pending;
} jpage;

static struct {
    volatile int inited;
    jlock lock;                     /* tradução, hash, encadeamento, invalidação, esvaziamento */
    jlock wplock;                   /* páginas (proteção) — tomada também pelo handler de falhas */
    a64_u8 *code_base, *code_start, *code_ptr, *code_end;
    a64_u8 *data_base, *data_ptr, *data_end;
    jblk **hash;
    jpage **pgh;
    jblk **order; volatile a64_u32 nblk;
    jthr *volatile threads;
    volatile int flush_active, pending;
    int (*enter)(a64_cpu *, void *);
    a64_u64 nflush, ntrans, nwfault, nstale, ninval, nvol;
} J;

void *jit_data_alloc(a64_u64 n) {
    n = (n + 15) & ~15ull;
    if (J.data_ptr + n > J.data_end) return NULL;
    void *r = J.data_ptr;
    J.data_ptr += n;
    return r;
}
void jit_code_space(a64_u8 **p, a64_u8 **lim) { *p = J.code_ptr; *lim = J.code_end; }
void jit_code_commit(a64_u8 *end) { J.code_ptr = (a64_u8 *)(((uintptr_t)end + 15) & ~(uintptr_t)15); }

static inline a64_u32 hpc(a64_u64 pc) { return (a64_u32)((pc >> 2) ^ (pc >> (2 + HASH_BITS))) & ((1u << HASH_BITS) - 1); }
static inline a64_u32 hpg(a64_u64 pg) { return (a64_u32)((pg >> 12) ^ (pg >> (12 + PGH_BITS))) & ((1u << PGH_BITS) - 1); }
static inline a64_u32 hjc(a64_u64 pc) { return (a64_u32)(pc >> 2) & ((1u << JIT_JC_BITS) - 1); }

static jblk *hash_find(a64_u64 pc) {
    for (jblk *b = J.hash[hpc(pc)]; b; b = b->hnext) if (b->pc == pc && !b->dead) return b;
    return NULL;
}
static void hash_remove(jblk *b) {
    jblk **pp = &J.hash[hpc(b->pc)];
    while (*pp && *pp != b) pp = &(*pp)->hnext;
    if (*pp) *pp = b->hnext;
}
static jpage *page_find(a64_u64 pg) {
    for (jpage *p = J.pgh[hpg(pg)]; p; p = p->next) if (p->page == pg) return p;
    return NULL;
}
static jpage *page_get(a64_u64 pg) {        /* wplock tomada */
    jpage *p = page_find(pg);
    if (p) return p;
    p = jit_data_alloc(sizeof *p);
    if (!p) return NULL;
    __builtin_memset(p, 0, sizeof *p);
    p->page = pg;
    p->next = J.pgh[hpg(pg)];
    J.pgh[hpg(pg)] = p;
    return p;
}

/* bloco que contém um endereço do host (código gerado) */
static jblk *block_by_host(const a64_u8 *a) {
    a64_u32 n = __atomic_load_n(&J.nblk, __ATOMIC_ACQUIRE);
    a64_u32 lo = 0, hi = n;
    while (lo < hi) {
        a64_u32 m = (lo + hi) / 2;
        if (J.order[m]->code <= a) lo = m + 1; else hi = m;
    }
    if (!lo) return NULL;
    jblk *b = J.order[lo - 1];
    return (a >= b->code && a < b->code + b->len) ? b : NULL;
}

/* ---------------------------------------------------------------------------------------------
 * Threads
 * ------------------------------------------------------------------------------------------- */
static void jc_clear(jthr *t) { for (unsigned i = 0; i < (1u << JIT_JC_BITS); i++) t->jc[i].pc = ~0ull; }

static jthr *thr_get(a64_cpu *c) {
    jthr *t = (jthr *)c->jit;
    if (t) return t;
    for (t = J.threads; t; t = t->next)        /* a mesma CPU (quem chamou zerou o a64_cpu) */
        if (t->live && t->cpu == c) break;
    if (!t) for (t = J.threads; t; t = t->next) /* reaproveita uma estrutura de thread encerrada */
        if (!t->live && __atomic_exchange_n(&t->live, 1, __ATOMIC_ACQ_REL) == 0) break;
    if (!t) {
        t = host_alloc(sizeof *t);
        if (!t) return NULL;
        __builtin_memset(t, 0, sizeof *t);
        t->live = 1;
        jthr *h;
        do { h = J.threads; t->next = h; } while (!__atomic_compare_exchange_n(&J.threads, &h, t, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
    }
    lk(&J.lock);                                /* jc_clear não pode correr junto com kill() */
    jc_clear(t);
    t->cpu = c;
    t->in_jit = 0;
    t->saved_rsp = 0;
    ulk(&J.lock);
    c->jit = t;
    c->reserved[13] = (a64_u64)(uintptr_t)&t->jc[0];
    return t;
}

void a64_jit_thread_exit(a64_cpu *c) {
    jthr *t = (jthr *)c->jit;
    if (!t) return;
    lk(&J.lock);
    t->cpu = NULL;
    t->in_jit = 0;
    ulk(&J.lock);
    c->jit = NULL;
    __atomic_store_n(&t->live, 0, __ATOMIC_RELEASE);
}

static inline volatile a64_u32 *stop_lo(a64_cpu *c) { return (volatile a64_u32 *)&c->reserved[15]; }
static inline volatile a64_u32 *stop_hi(a64_cpu *c) { return (volatile a64_u32 *)&c->reserved[15] + 1; }

static void kick_all(void) {
    for (jthr *t = J.threads; t; t = t->next) {
        a64_cpu *c = t->cpu;
        if (c) __atomic_store_n(stop_hi(c), 1, __ATOMIC_SEQ_CST);
    }
}

/* ---------------------------------------------------------------------------------------------
 * Invalidação
 * ------------------------------------------------------------------------------------------- */
static void kill_block(jblk *b) {           /* J.lock tomada */
    if (b->dead) return;
    b->dead = 1;
    J.ninval++;
    hash_remove(b);
    for (jlink *l = b->in; l; l = l->next) __atomic_store_n((a64_u32 *)l->site, 0u, __ATOMIC_RELEASE);
    b->in = NULL;
    for (jthr *t = J.threads; t; t = t->next) {
        jc_ent *e = &t->jc[hjc(b->pc)];
        if (e->pc == b->pc) __atomic_store_n(&e->pc, ~0ull, __ATOMIC_RELEASE);
    }
}

static void unprotect_if_empty(jpage *p) {  /* wplock tomada */
    if (!p->wp) return;
    for (jblk *b = p->blocks; b; b = b->pgnext[0]) if (!b->dead) return;
    host_wprotect(p->page, 0);
    p->wp = 0;
}

static void inval_page(jpage *p, a64_u64 lo, a64_u64 hi) {   /* J.lock e wplock tomadas */
    jblk **pp = &p->blocks;
    while (*pp) {
        jblk *b = *pp;
        if (!b->dead && b->pc < hi && b->end > lo) kill_block(b);
        if (b->dead) *pp = b->pgnext[0]; else pp = &b->pgnext[0];
    }
}

static void process_pending(void) {         /* J.lock tomada */
    if (!__atomic_exchange_n(&J.pending, 0, __ATOMIC_ACQ_REL)) return;
    lk(&J.wplock);
    for (unsigned i = 0; i < (1u << PGH_BITS); i++)
        for (jpage *p = J.pgh[i]; p; p = p->next)
            if (p->pending) {
                p->pending = 0;
                inval_page(p, p->page, p->page + JIT_PAGE);
                if (++p->ninval >= VOLATILE_AFTER && !p->vol) { p->vol = 1; J.nvol++; }
            }
    ulk(&J.wplock);
}

void a64_jit_invalidate(a64_u64 addr, a64_u64 len) {
    if (!J.inited || !len) return;
    a64_u64 lo = addr, hi = addr + len;
    if (hi < lo) hi = ~0ull;
    lk(&J.lock);
    lk(&J.wplock);
    a64_u64 p0 = lo & ~(JIT_PAGE - 1), p1 = (hi - 1) & ~(JIT_PAGE - 1);
    if ((p1 - p0) / JIT_PAGE > (1u << PGH_BITS)) {
        for (unsigned i = 0; i < (1u << PGH_BITS); i++)
            for (jpage *p = J.pgh[i]; p; p = p->next)
                if (p->page < hi && p->page + JIT_PAGE > lo) { inval_page(p, lo, hi); unprotect_if_empty(p); }
    } else {
        for (a64_u64 pg = p0;; pg += JIT_PAGE) {
            jpage *p = page_find(pg);
            if (p) { inval_page(p, lo, hi); unprotect_if_empty(p); }
            if (pg == p1) break;
        }
    }
    ulk(&J.wplock);
    ulk(&J.lock);
}

/* escrita numa página que o JIT protegeu: devolve true se a falha era nossa (refazer a escrita).
 * Não toma J.lock (pode vir de dentro do código gerado durante um esvaziamento): só devolve a
 * escrita e agenda a invalidação, que o despachante faz; as threads são expulsas do código. */
bool a64_jit_write_fault(a64_u64 addr) {
    if (!J.inited) return false;
    a64_u64 pg = addr & ~(JIT_PAGE - 1);
    lk(&J.wplock);
    jpage *p = page_find(pg);
    bool ours = false;
    if (p && p->wp) {
        host_wprotect(pg, 0);
        p->wp = 0;
        p->pending = 1;
        __atomic_store_n(&J.pending, 1, __ATOMIC_RELEASE);
        J.nwfault++;
        ours = true;
    } else if (p && p->wp_ever && host_wprotect(pg, 2)) {
        ours = true;                            /* outra thread já devolveu a escrita */
    }
    ulk(&J.wplock);
    if (ours) kick_all();
    return ours;
}

/* ---------------------------------------------------------------------------------------------
 * Esvaziamento total (cache cheio). J.lock tomada; a thread que chama está fora do código.
 * ------------------------------------------------------------------------------------------- */
static void reset_tables(void);
static void flush_all(jthr *self) {
    __atomic_store_n(&J.flush_active, 1, __ATOMIC_SEQ_CST);
    kick_all();
    for (jthr *t = J.threads; t; t = t->next)
        if (t != self) while (__atomic_load_n(&t->in_jit, __ATOMIC_SEQ_CST)) __builtin_ia32_pause();
    lk(&J.wplock);
    for (unsigned i = 0; i < (1u << PGH_BITS); i++)
        for (jpage *p = J.pgh[i]; p; p = p->next) if (p->wp) host_wprotect(p->page, 0);
    reset_tables();
    ulk(&J.wplock);
    for (jthr *t = J.threads; t; t = t->next) {
        jc_clear(t);
        if (t->cpu) __atomic_store_n(stop_hi(t->cpu), 0, __ATOMIC_SEQ_CST);
    }
    J.nflush++;
    __atomic_store_n(&J.flush_active, 0, __ATOMIC_SEQ_CST);
}

static void reset_tables(void) {
    J.code_ptr = J.code_start;
    J.data_ptr = J.data_base;
    __builtin_memset(J.hash, 0, sizeof(jblk *) << HASH_BITS);
    __builtin_memset(J.pgh, 0, sizeof(jpage *) << PGH_BITS);
    __atomic_store_n(&J.nblk, 0, __ATOMIC_RELEASE);
    J.pending = 0;
}

/* ---------------------------------------------------------------------------------------------
 * Stubs comuns e inicialização
 * ------------------------------------------------------------------------------------------- */
static void gen_stubs(xb *b) {
    a64_u8 *enter = b->p;
#ifdef _WIN32
    static const int sv[] = { XBX, XBP, XDI, XSI, X12, X13, X14, X15 };
    const int nsv = 8, frame = 40, a0 = XCX, a1 = XDX;
#else
    static const int sv[] = { XBX, XBP, X12, X13, X14, X15 };
    const int nsv = 6, frame = 8, a0 = XDI, a1 = XSI;
#endif
    for (int i = 0; i < nsv; i++) x_push(b, sv[i]);
    x_alu_ri(b, 1, A_SUB, XR(XSP), frame);
    x_mov_rr(b, 1, RCPU, a0);
    x_mov_rm(b, 1, XAX, XM(a0, OFF_JIT));
    x_mov_mr(b, 1, XM(XAX, OFF_THR_SAVED_RSP), XSP);
    x_jmp_r(b, a1);

    /* g_exit: eax = código */
    jit_g.g_exit = b->p;
    x_alu_ri(b, 1, A_ADD, XR(XSP), frame);
    for (int i = nsv - 1; i >= 0; i--) x_pop(b, sv[i]);
    x_ret(b);

    /* g_link: rdx = campo rel32 do salto a encadear; c->pc = alvo */
    jit_g.g_link = b->p;
    x_mov_rm(b, 1, XCX, XM(RCPU, OFF_JIT));
    x_mov_mr(b, 1, XM(XCX, OFF_THR_LINK), XDX);
    x_mov_ri(b, XAX, JX_LINK);
    x_jmp_to(b, jit_g.g_exit);

    /* g_ind_pc: alvo em c->pc; g_ind: alvo em rax */
    jit_g.g_ind_pc = b->p;
    x_mov_rm(b, 1, XAX, XM(RCPU, OFF_PC));
    jit_g.g_ind = b->p;
    x_alu_ri(b, 1, A_CMP, XM(RCPU, OFF_STOP), 0);
    a64_u8 *j1 = x_jcc32(b, CC_NE);
    x_mov_rm(b, 1, XCX, XM(RCPU, OFF_JC));
    x_mov_rr(b, 0, XDX, XAX);
    x_shift_ri(b, 0, S_SHR, XDX, 2);
    x_alu_ri(b, 0, A_AND, XR(XDX), (1 << JIT_JC_BITS) - 1);
    x_shift_ri(b, 0, S_SHL, XDX, 4);
    x_alu_rm(b, 1, A_CMP, XAX, XMI(XCX, XDX, 0, 0));
    a64_u8 *j2 = x_jcc32(b, CC_NE);
    x_jmp_m(b, XMI(XCX, XDX, 0, 8));
    x_patch32(j1, b->p);
    x_patch32(j2, b->p);
    x_mov_mr(b, 1, XM(RCPU, OFF_PC), XAX);
    x_mov_ri(b, XAX, JX_IND);
    x_jmp_to(b, jit_g.g_exit);

    /* g_hexit: eax = retorno != 0 do helper do interpretador (1 = desvio, >= 2 = saída) */
    jit_g.g_hexit = b->p;
    x_alu_ri(b, 0, A_CMP, XR(XAX), 1);
    x_jcc_to(b, CC_E, jit_g.g_ind_pc);
    x_jmp_to(b, jit_g.g_exit);

    J.enter = (int (*)(a64_cpu *, void *))(void *)enter;
}

#ifdef _WIN32
static LONG CALLBACK jit_veh(EXCEPTION_POINTERS *e) {
    EXCEPTION_RECORD *r = e->ExceptionRecord;
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2 && r->ExceptionInformation[0] == 1
        && a64_jit_write_fault((a64_u64)r->ExceptionInformation[1]))
        return EXCEPTION_CONTINUE_EXECUTION;
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
static void jit_atexit(void) {
    fprintf(stderr, "[jit] blocos=%llu esvaziamentos=%llu codigo=%llu bytes falhas_escrita=%llu invalidados=%llu volateis=%llu stale=%llu\n",
            (unsigned long long)J.ntrans, (unsigned long long)J.nflush, (unsigned long long)(J.code_ptr - J.code_start),
            (unsigned long long)J.nwfault, (unsigned long long)J.ninval, (unsigned long long)J.nvol, (unsigned long long)J.nstale);
}
static void jit_env(void) {
    const char *s;
    if ((s = getenv("A64JIT_NONATIVE")) && *s == '1') jit_g.nonative = 1;
    if ((s = getenv("A64JIT_MAXINSNS"))) jit_g.maxinsns = atoi(s);
    if ((s = getenv("A64JIT_NOWP")) && *s == '1') jit_g.nowp = 1;
    if ((s = getenv("A64JIT_FBLOG")) && *s == '1') jit_g.fblog = 1;
    if ((s = getenv("A64JIT_STATS")) && *s == '1') atexit(jit_atexit);
}
#else
static void jit_env(void) {}
#endif

static void jit_cpuid(void) {
    unsigned a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
    jit_g.ssse3 = (c >> 9) & 1;
    jit_g.sse41 = (c >> 19) & 1;
    jit_g.sse42 = (c >> 20) & 1;
    (void)a; (void)b; (void)d;
}

static jlock init_lock;
static void jit_init(void) {
    if (J.inited) return;
    lk(&init_lock);
    if (!J.inited) {
        J.code_base = host_alloc(CODE_SIZE);
        J.data_base = host_alloc(DATA_SIZE);
        a64_u8 *fixed = host_alloc((sizeof(jblk *) << HASH_BITS) + (sizeof(jpage *) << PGH_BITS) + sizeof(jblk *) * (a64_u64)MAX_BLKS);
        if (!J.code_base || !J.data_base || !fixed) { ulk(&init_lock); return; }
        J.hash = (jblk **)fixed;
        J.pgh = (jpage **)(fixed + (sizeof(jblk *) << HASH_BITS));
        J.order = (jblk **)(fixed + (sizeof(jblk *) << HASH_BITS) + (sizeof(jpage *) << PGH_BITS));
        J.data_end = J.data_base + DATA_SIZE;
        J.code_end = J.code_base + CODE_SIZE;
        jit_g.helpers = (void **)J.code_base;
        jit_g.helpers[JH_INTERP] = (void *)jit_h_interp;
        xb b = { J.code_base + 8 * JH_COUNT, J.code_end, 0 };
        jit_env();
        jit_cpuid();
        gen_stubs(&b);
        J.code_start = (a64_u8 *)(((uintptr_t)b.p + 63) & ~(uintptr_t)63);
        reset_tables();
#ifdef _WIN32
        AddVectoredExceptionHandler(1, jit_veh);
#endif
        __atomic_store_n(&J.inited, 1, __ATOMIC_RELEASE);
    }
    ulk(&init_lock);
}

/* ---------------------------------------------------------------------------------------------
 * Helper do fallback: executa UMA instrução no interpretador com o estado sincronizado.
 * Devolve 0 = seguir, 1 = desviou (c->pc), >= 2 = saída A64_EXIT_* (já com o pc do contrato).
 * ------------------------------------------------------------------------------------------- */
int jit_h_interp(a64_cpu *c, a64_u32 insn) {
    c->nzcv = jit_x86_to_nzcv(JIT_F(c));
    int r = a64_interp_exec(c, insn);
    JIT_F(c) = jit_nzcv_to_x86(c->nzcv);
    if (r == A64_NEXT) return 0;
    if (r == A64_BRANCH) return 1;
    c->exit_reason = (a64_u32)r;
    c->exit_insn = insn;
    if (r == A64_EXIT_SVC || r == A64_EXIT_YIELD || r == A64_EXIT_ICACHE) c->pc += 4;
    return r;
}

/* ---------------------------------------------------------------------------------------------
 * Tradução sob a trava
 * ------------------------------------------------------------------------------------------- */
static jblk *get_block(jthr *self, a64_u64 pc) {   /* J.lock tomada */
    jblk *b = hash_find(pc);
    if (b) return b;
    a64_u64 pg = pc & ~(JIT_PAGE - 1);
    lk(&J.wplock);
    jpage *p = page_get(pg);
    int vol = p ? p->vol : 0;
    ulk(&J.wplock);
    b = (p && J.nblk < MAX_BLKS) ? jit_translate(pc, vol) : NULL;
    if (!b) {
        flush_all(self);
        lk(&J.wplock);
        p = page_get(pg);
        ulk(&J.wplock);
        b = p ? jit_translate(pc, 0) : NULL;
        if (!b) return NULL;
        vol = 0;
    }
    J.ntrans++;
    b->hnext = J.hash[hpc(pc)];
    J.hash[hpc(pc)] = b;
    J.order[J.nblk] = b;
    __atomic_store_n(&J.nblk, J.nblk + 1, __ATOMIC_RELEASE);
    lk(&J.wplock);
    b->npg = 1;
    b->pg[0] = pg;
    b->pgnext[0] = p->blocks;
    p->blocks = b;
    if (!vol && !p->wp && !jit_g.nowp) {
        p->wp = host_wprotect(pg, 1);
        if (p->wp) p->wp_ever = 1;
    }
    ulk(&J.wplock);
    return b;
}

/* ---------------------------------------------------------------------------------------------
 * Despachante
 * ------------------------------------------------------------------------------------------- */
static int jit_loop(a64_cpu *c, jthr *t);
int a64_jit_run(a64_cpu *c) {
    jit_init();
    jthr *t = J.inited ? thr_get(c) : NULL;
    if (!t) return a64_interp_run(c, 0);
    JIT_F(c) = jit_nzcv_to_x86(c->nzcv);
    t->in_run = 1;
    int r = jit_loop(c, t);
    t->in_run = 0;
    return r;
}

static int jit_loop(a64_cpu *c, jthr *t) {
    for (;;) {
        if (__builtin_expect(*stop_lo(c), 0)) {
            *stop_lo(c) = 0;
            c->nzcv = jit_x86_to_nzcv(JIT_F(c));
            c->exit_reason = A64_EXIT_STOP;
            return A64_EXIT_STOP;
        }
        __atomic_store_n(&t->in_jit, 1, __ATOMIC_SEQ_CST);
        if (__builtin_expect(J.flush_active || J.pending || *stop_hi(c), 0)) {
            __atomic_store_n(&t->in_jit, 0, __ATOMIC_SEQ_CST);
            while (__atomic_load_n(&J.flush_active, __ATOMIC_SEQ_CST)) __builtin_ia32_pause();
            if (J.pending) { lk(&J.lock); process_pending(); ulk(&J.lock); }
            if (!J.flush_active) __atomic_store_n(stop_hi(c), 0, __ATOMIC_SEQ_CST);
            continue;
        }
        jc_ent *e = &t->jc[hjc(c->pc)];
        void *code = (e->pc == c->pc) ? e->code : NULL;
        if (!code) {
            __atomic_store_n(&t->in_jit, 0, __ATOMIC_SEQ_CST);
            a64_u64 pc = c->pc;
            (void)a64_ld32(pc);                 /* falha de busca aqui: estado já consistente (pc = alvo) */
            lk(&J.lock);
            jblk *b = get_block(t, pc);
            if (b) { e->code = b->code; __atomic_store_n(&e->pc, pc, __ATOMIC_RELEASE); }
            ulk(&J.lock);
            if (!b) { c->nzcv = jit_x86_to_nzcv(JIT_F(c)); return a64_interp_run(c, 0); }
            continue;                           /* volta ao topo: in_jit e pedidos conferidos de novo */
        }
        int r = J.enter(c, code);
        __atomic_store_n(&t->in_jit, 0, __ATOMIC_SEQ_CST);
        switch (r) {
        case JX_LINK: {
            lk(&J.lock);
            a64_u8 *site = (a64_u8 *)(uintptr_t)t->link_site;
            jblk *src = block_by_host(site);
            a64_u64 gen = J.nflush;
            jblk *dst = get_block(t, c->pc);
            if (dst && src && gen == J.nflush && !src->dead && !dst->dead) {
                jlink *l = jit_data_alloc(sizeof *l);
                if (l) {
                    l->site = site;
                    l->next = dst->in;
                    dst->in = l;
                    __atomic_store_n((int32_t *)site, (int32_t)(dst->code - (site + 4)), __ATOMIC_RELEASE);
                }
            }
            if (dst) { jc_ent *f = &t->jc[hjc(dst->pc)]; f->code = dst->code; __atomic_store_n(&f->pc, dst->pc, __ATOMIC_RELEASE); }
            ulk(&J.lock);
            break;
        }
        case JX_IND:
            break;
        case JX_STALE:
            J.nstale++;
            a64_jit_invalidate(c->pc, 4);
            break;
        case JX_UNWIND:
            JIT_F(c) = jit_nzcv_to_x86(c->nzcv);
            if (t->unwind_code == A64_NEXT) break;
            c->exit_reason = (a64_u32)t->unwind_code;
            return t->unwind_code;
        case A64_EXIT_ICACHE:
            a64_jit_invalidate(c->exit_info & ~63ull, 64);
            break;
        case A64_EXIT_YIELD:
            a64_host_yield(c);
            break;
        default:
            c->nzcv = jit_x86_to_nzcv(JIT_F(c));
            return r;
        }
    }
}

/* ---------------------------------------------------------------------------------------------
 * Recuperação de estado numa falha dentro do código gerado
 * ------------------------------------------------------------------------------------------- */
bool a64_jit_recover(a64_cpu *c, const a64_hostctx *h) {
    if (!J.inited) return false;
    jthr *t = (jthr *)c->jit;
    const a64_u8 *rip = (const a64_u8 *)(uintptr_t)h->rip;
    jblk *b = (rip >= J.code_start && rip < J.code_end) ? block_by_host(rip) : NULL;
    if (!b) {
        /* fora dos blocos: helper do interpretador (estado já sincronizado em c) ou despachante */
        if (t && t->in_run) c->nzcv = jit_x86_to_nzcv(JIT_F(c));
        if (t) t->in_jit = 0;
        return false;
    }
    a64_u32 off = (a64_u32)(rip - b->code);
    const jfp *f = NULL;
    for (a64_u32 i = 0; i < b->nfp && b->fp[i].off <= off; i++) f = &b->fp[i];
    if (!f) {                                   /* entrada do bloco (autoconferência): tudo na memória */
        c->pc = b->pc;
        c->nzcv = jit_x86_to_nzcv(JIT_F(c));
    } else {
        c->pc = b->pc + 4ull * f->idx;
        for (int r = 0; r < 16; r++) if (f->map[r] != 0xFF) c->x[f->map[r]] = h->gpr[r];
        int fe = f->fl & 3;
        if (fe) c->nzcv = jit_rflags_to_nzcv(h->rflags, fe == 2);
        else c->nzcv = jit_x86_to_nzcv(JIT_F(c));
    }
    JIT_F(c) = jit_nzcv_to_x86(c->nzcv);
    if (t) t->in_jit = 0;
    return true;
}

bool a64_jit_unwind(a64_cpu *c, a64_hostctx *h, int code) {
    jthr *t = (jthr *)c->jit;
    if (!J.inited || !t || !t->saved_rsp) return false;
    h->gpr[XSP] = t->saved_rsp;
    h->gpr[XAX] = JX_UNWIND;
    h->rip = (a64_u64)(uintptr_t)jit_g.g_exit;
    t->unwind_code = code;
    return true;
}

void a64_jit_stats(a64_u64 *ntrans, a64_u64 *nflush, a64_u64 *code_bytes) {
    if (ntrans) *ntrans = J.ntrans;
    if (nflush) *nflush = J.nflush;
    if (code_bytes) *code_bytes = J.inited ? (a64_u64)(J.code_ptr - J.code_start) : 0;
}

/* o runtime abandonou a64_jit_run por longjmp (depois de a64_jit_recover): limpa o estado por
 * thread. Nenhuma trava do JIT fica tomada numa falha do código traduzido nem do helper. */
void a64_jit_abort(a64_cpu *c) {
    jthr *t = (jthr *)c->jit;
    if (!t) return;
    __atomic_store_n(&t->in_jit, 0, __ATOMIC_SEQ_CST);
    t->in_run = 0;
    t->saved_rsp = 0;
}
