/*
 * mOS — tradutor arm64: monitor exclusivo (ver a64_xmon.h).
 * Uma trava de ticket global só protege a tabela. O dado do guest é lido e escrito fora dela,
 * exceto o store do STXR, que confirma a época com a trava segurada. A trava é solta se a
 * mesma thread entrar de novo por sinal ou falta (longjmp deixaria o ticket preso).
 */
#include "a64_xmon.h"

#if !defined(A64_RUNTIME_MOS)
_Thread_local a64_cpu *a64_xmon_tls;
#endif

a64_u32 a64_xmon_live;
static a64_u32 gate_used;
static a64_u32 lk_head, lk_tail;
static a64_xmon_gate gates[A64_XMON_MAX];

_Static_assert(offsetof(a64_cpu, excl_valid) == 844, "excl_valid");
_Static_assert(offsetof(a64_cpu, reserved[8]) == 944, "geração de preempção");
_Static_assert(offsetof(a64_cpu, reserved[11]) == 968, "gate");

/* espelho do prefixo de rt_thread_t (magic, self, cpu): o kernel usa estes deslocamentos */
struct a64_xmon_rtprefix { a64_u64 magic, self; a64_cpu cpu; };
_Static_assert(offsetof(struct a64_xmon_rtprefix, cpu.excl_valid) == 860, "excl no host");
_Static_assert(offsetof(struct a64_xmon_rtprefix, cpu.reserved[8]) == 960, "sw no host");

static int slot_of(const a64_cpu *c) {
    a64_u64 s = c->reserved[A64_XMON_SLOT];
    if (!s || s > A64_XMON_MAX) return -1;
    return (int)s - 1;
}

__attribute__((target("general-regs-only")))
void a64_xmon_bind(a64_cpu *c) {
    if (!c) return;
#if !defined(A64_RUNTIME_MOS)
    a64_xmon_tls = c;
#endif
    if (c->reserved[A64_XMON_GATE]) return;
    a64_u32 i = __atomic_fetch_add(&gate_used, 1, __ATOMIC_RELAXED);
    if (i >= A64_XMON_MAX) return;
    a64_xmon_gate *g = &gates[i];
    g->addr = 0; g->len = 0; g->inflight = 0; g->held = 0; g->suppressed = 0;
    g->armed = 0; g->size = 0; g->epoch = 0; g->sw = 0; g->raddr = 0;
    c->reserved[A64_XMON_SLOT] = (a64_u64)i + 1;
    __atomic_store_n(&c->reserved[A64_XMON_GATE], (a64_u64)(uintptr_t)g, __ATOMIC_RELEASE);
}

static void xmon_lock(a64_xmon_gate *g) {
    if (g->held) { g->held++; return; }
    a64_u32 t = __atomic_fetch_add(&lk_tail, 1, __ATOMIC_RELAXED);
    while (__atomic_load_n(&lk_head, __ATOMIC_ACQUIRE) != t)
        __builtin_ia32_pause();
    g->held = 1;
}

static void xmon_unlock(a64_xmon_gate *g) {
    if (g->held > 1) { g->held--; return; }
    g->held = 0;
    __atomic_fetch_add(&lk_head, 1, __ATOMIC_RELEASE);
}

static int ranges_overlap(a64_u64 a, a64_u32 an, a64_u64 b, a64_u32 bn) {
    if (!an || !bn) return 0;
    a64_u64 ae = a + an, be = b + bn;
    if (ae < a) ae = ~(a64_u64)0;
    if (be < b) be = ~(a64_u64)0;
    return a < be && b < ae;
}

static a64_u32 gate_n(void) {
    a64_u32 n = __atomic_load_n(&gate_used, __ATOMIC_ACQUIRE);
    return n > A64_XMON_MAX ? A64_XMON_MAX : n;
}

static void reap_locked(a64_cpu *c) {
    int s = slot_of(c);
    if (s < 0) return;
    a64_xmon_gate *g = &gates[s];
    if (g->armed && !c->excl_valid) {
        g->armed = 0;
        __atomic_fetch_sub(&a64_xmon_live, 1, __ATOMIC_RELEASE);
    }
}

static void bump_locked(a64_u64 addr, a64_u32 len) {
    a64_u32 n = gate_n();
    for (a64_u32 i = 0; i < n; i++) {
        a64_xmon_gate *s = &gates[i];
        if (s->armed && ranges_overlap(s->raddr, s->size, addr, len))
            s->epoch++;
    }
}

__attribute__((target("general-regs-only")))
void a64_xmon_bump(a64_cpu *c) {
    a64_xmon_gate *g = a64_xmon_gate_of(c);
    if (!c || !g || g->held) return;
    if (__atomic_load_n(&a64_xmon_live, __ATOMIC_ACQUIRE) == 0) return;
    xmon_lock(g);
    reap_locked(c);
    if (__atomic_load_n(&a64_xmon_live, __ATOMIC_RELAXED))
        bump_locked(g->addr, g->len);
    xmon_unlock(g);
}

__attribute__((target("general-regs-only")))
void a64_xmon_prepare(a64_cpu *c, a64_u64 addr, a64_u32 len) {
    a64_xmon_bind(c);
    a64_xmon_gate *g = a64_xmon_gate_of(c);
    if (!g) return;
    if (g->held) { g->suppressed++; return; }
    g->addr = addr;
    g->len = len;
    __atomic_store_n(&g->inflight, 1, __ATOMIC_RELEASE);
    if (__atomic_load_n(&a64_xmon_live, __ATOMIC_ACQUIRE) == 0) return;
    xmon_lock(g);
    reap_locked(c);
    bump_locked(addr, len);
    xmon_unlock(g);
}

static void wait_quiet(int self) {
    a64_u32 n = gate_n();
    for (a64_u32 i = 0; i < n; i++) {
        if ((int)i == self) continue;
        while (__atomic_load_n(&gates[i].inflight, __ATOMIC_ACQUIRE))
            __builtin_ia32_pause();
    }
}

static a64_u64 raw_ld(a64_u64 a, a64_u32 n) {
    uintptr_t p = (uintptr_t)a;
    switch (n) {
    case 1: return *(volatile a64_u8 *)p;
    case 2: return *(volatile a64_u16 *)p;
    case 4: return *(volatile a64_u32 *)p;
    default: return *(volatile a64_u64 *)p;
    }
}

typedef long long a64x_v2di __attribute__((vector_size(16)));

static void raw_ld16(a64_u64 a, a64_u64 *lo, a64_u64 *hi) {
    if ((a & 15) == 0) {
        a64x_v2di v;
        __asm__ volatile("movdqa (%1), %0" : "=x"(v) : "r"((uintptr_t)a) : "memory");
        __builtin_memcpy(lo, &v, 8);
        __builtin_memcpy(hi, (const char *)&v + 8, 8);
    } else {
        *lo = raw_ld(a, 8);
        *hi = raw_ld(a + 8, 8);
    }
}

static void raw_st(a64_u64 a, a64_u32 n, a64_u64 lo, a64_u64 hi) {
    uintptr_t p = (uintptr_t)a;
    switch (n) {
    case 1: *(volatile a64_u8 *)p = (a64_u8)lo; break;
    case 2: *(volatile a64_u16 *)p = (a64_u16)lo; break;
    case 4: *(volatile a64_u32 *)p = (a64_u32)lo; break;
    case 8: *(volatile a64_u64 *)p = lo; break;
    default:
        if ((a & 15) == 0) {
            a64x_v2di v;
            __builtin_memcpy(&v, &lo, 8);
            __builtin_memcpy((char *)&v + 8, &hi, 8);
            __asm__ volatile("movdqa %0, (%1)" :: "x"(v), "r"(p) : "memory");
        } else {
            *(volatile a64_u64 *)p = lo;
            *(volatile a64_u64 *)(p + 8) = hi;
        }
        break;
    }
}

static void read_mem(a64_u64 addr, a64_u32 n, a64_u64 *lo, a64_u64 *hi) {
    if (n == 16) raw_ld16(addr, lo, hi);
    else { *lo = raw_ld(addr, n); *hi = 0; }
}

void a64_xmon_load(a64_cpu *c, a64_u64 addr, a64_u32 n, a64_u64 *lo, a64_u64 *hi) {
    a64_xmon_bind(c);
    a64_xmon_gate *g = a64_xmon_gate_of(c);
    a64_u64 v0 = 0, v1 = 0;
    if (!g) {
        read_mem(addr, n, &v0, &v1);
        c->excl_valid = 0;
        *lo = v0; *hi = v1;
        return;
    }
    int self = slot_of(c);
    __atomic_store_n(&g->inflight, 0, __ATOMIC_RELAXED);
    a64_u64 sw = c->reserved[A64_XMON_SW];
    xmon_lock(g);
    reap_locked(c);
    int was = g->armed;
    g->raddr = addr;
    g->size = n;
    g->armed = 1;
    if (!was) __atomic_fetch_add(&a64_xmon_live, 1, __ATOMIC_RELEASE);
    a64_u64 ep = g->epoch;
    xmon_unlock(g);
    wait_quiet(self);
    read_mem(addr, n, &v0, &v1);
    xmon_lock(g);
    if (g->epoch != ep) {
        read_mem(addr, n, &v0, &v1);
        ep = g->epoch;
    }
    if (c->reserved[A64_XMON_SW] != sw || !g->armed) {
        if (g->armed) {
            g->armed = 0;
            __atomic_fetch_sub(&a64_xmon_live, 1, __ATOMIC_RELEASE);
        }
        c->excl_valid = 0;
    } else {
        g->sw = sw;
        c->excl_addr = addr;
        c->excl_size = n;
        c->excl_val[0] = v0;
        c->excl_val[1] = v1;
        c->reserved[A64_XMON_EPOCH] = ep;
        /* publica e confere de novo: a preempção no meio limpa e soma a geração */
        __atomic_store_n(&c->excl_valid, 1, __ATOMIC_RELEASE);
        if (__atomic_load_n(&c->reserved[A64_XMON_SW], __ATOMIC_ACQUIRE) != sw) {
            g->armed = 0;
            __atomic_fetch_sub(&a64_xmon_live, 1, __ATOMIC_RELEASE);
            c->excl_valid = 0;
        }
    }
    xmon_unlock(g);
    *lo = v0;
    *hi = v1;
}

int a64_xmon_store(a64_cpu *c, a64_u64 addr, a64_u32 n, a64_u64 lo, a64_u64 hi) {
    a64_xmon_bind(c);
    a64_xmon_gate *g = a64_xmon_gate_of(c);
    if (!g) { c->excl_valid = 0; return 1; }
    (void)*(volatile a64_u8 *)(uintptr_t)addr;          /* traz a página antes da trava */
    xmon_lock(g);
    int ok = c->excl_valid && g->armed && g->raddr == addr && g->size == n
             && g->epoch == c->reserved[A64_XMON_EPOCH]
             && __atomic_load_n(&c->reserved[A64_XMON_SW], __ATOMIC_ACQUIRE) == g->sw;
    int status = 1;
    if (ok && c->excl_valid && __atomic_load_n(&c->reserved[A64_XMON_SW], __ATOMIC_ACQUIRE) == g->sw) {
        raw_st(addr, n, lo, hi);
        bump_locked(addr, n);
        status = 0;
    }
    if (g->armed) {
        g->armed = 0;
        __atomic_fetch_sub(&a64_xmon_live, 1, __ATOMIC_RELEASE);
    }
    c->excl_valid = 0;
    xmon_unlock(g);
    return status;
}

static void drop_locked(a64_cpu *c, a64_xmon_gate *g) {
    if (g->armed) {
        g->armed = 0;
        __atomic_fetch_sub(&a64_xmon_live, 1, __ATOMIC_RELEASE);
    }
    g->inflight = 0;
    g->suppressed = 0;
    c->excl_valid = 0;
}

void a64_xmon_invalidate(a64_cpu *c) {
    if (!c) return;
    a64_xmon_gate *g = a64_xmon_gate_of(c);
    if (!g) { c->excl_valid = 0; c->reserved[A64_XMON_SW]++; return; }
    if (g->held) {
        g->held = 0;
        __atomic_fetch_add(&lk_head, 1, __ATOMIC_RELEASE);
    }
    xmon_lock(g);
    drop_locked(c, g);
    xmon_unlock(g);
    c->reserved[A64_XMON_SW]++;
}

void a64_xmon_clrex(a64_cpu *c) { a64_xmon_invalidate(c); }

void a64_xmon_fork_child(a64_cpu *self) {
    __builtin_memset(gates, 0, sizeof gates);
    __atomic_store_n(&gate_used, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&a64_xmon_live, 0, __ATOMIC_RELAXED);
    lk_head = lk_tail = 0;
    if (!self) return;
    self->excl_valid = 0;
    self->reserved[A64_XMON_GATE] = 0;
    self->reserved[A64_XMON_SLOT] = 0;
    self->reserved[A64_XMON_SW]++;
#if !defined(A64_RUNTIME_MOS)
    a64_xmon_tls = self;
#endif
}
