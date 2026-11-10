/*
 * mOS — tradutor arm64: monitor exclusivo local/global sobre o TSO do x86.
 *
 * O x86 já ordena load-load, store-store e load-store. Só store-load (buffer de store)
 * reordena. Por isso mfence aparece só onde o ARM exige essa ordem: DMB/DSB com (CRm & 3) == 3
 * (SY/ISH/NSH/OSH) e o store RCsc (STLR/STLLR). DMB/DSB LD e ST não levam mfence.
 *
 * LDXR/STXR não podem ser compare-exchange do valor: A→B→A (e o STR do mesmo valor na mesma
 * thread) fariam o STXR passar. Cada reserva tem uma época que só cresce. Qualquer store que
 * sobreponha os bytes marca a época. O grânulo é o intervalo acessado, não a linha de 64 B do
 * ERG: dois exclusivos em palavras diferentes não compartilham época e não entram em livelock.
 * Falha espúria continua permitida. Sucesso falso, não.
 *
 * O store comum publica inflight no gate da thread antes de olhar o contador global. Quem arma
 * um LDXR espera inflight == 0 e só então lê. No TSO esse store não ultrapassa o load seguinte,
 * então não há mfence no caminho rápido (nenhum exclusivo armado).
 *
 * reserved[8]  geração de preempção (o kernel soma 1 ao trocar de contexto)
 * reserved[9]  índice do gate + 1 (0 = nenhum)
 * reserved[10] época observada pelo LDXR
 * reserved[11] ponteiro do gate
 * reserved[12..15] continuam do JIT.
 */
#pragma once
#include "a64.h"

#define A64_XMON_MAX 4096

typedef struct a64_xmon_gate {
    a64_u64 addr;        /* 0: bytes do store em curso, ou da reserva */
    a64_u32 len;         /* 8 */
    a64_u32 inflight;    /* 12: 1 enquanto o store desta thread ainda não aconteceu */
    a64_u32 held;        /* 16: esta thread segura a trava global (recursivo) */
    a64_u32 suppressed;  /* 20: notes aninhados que não publicaram inflight */
    a64_u32 armed;       /* 24 */
    a64_u32 size;        /* 28: bytes da reserva */
    a64_u64 epoch;       /* 32: só cresce */
    a64_u64 sw;          /* 40: reserved[8] visto no LDXR */
    a64_u64 raddr;       /* 48: endereço da reserva; addr/len são o store em curso */
} a64_xmon_gate;

_Static_assert(offsetof(a64_xmon_gate, len) == 8, "gate");
_Static_assert(offsetof(a64_xmon_gate, inflight) == 12, "gate");
_Static_assert(offsetof(a64_xmon_gate, held) == 16, "gate");
_Static_assert(offsetof(a64_xmon_gate, suppressed) == 20, "gate");
_Static_assert(offsetof(a64_xmon_gate, armed) == 24, "gate");
_Static_assert(offsetof(a64_xmon_gate, size) == 28, "gate");
_Static_assert(offsetof(a64_xmon_gate, epoch) == 32, "gate");
_Static_assert(offsetof(a64_xmon_gate, sw) == 40, "gate");
_Static_assert(offsetof(a64_xmon_gate, raddr) == 48, "gate");

enum {
    A64_XMON_SW = 8,
    A64_XMON_SLOT = 9,
    A64_XMON_EPOCH = 10,
    A64_XMON_GATE = 11
};

/* contador de reservas armadas; o JIT lê com um cmp simples (u32 alinhado é atômico no x86) */
extern a64_u32 a64_xmon_live;

void a64_xmon_bind(a64_cpu *c);
void a64_xmon_bump(a64_cpu *c);
void a64_xmon_prepare(a64_cpu *c, a64_u64 addr, a64_u32 len);
void a64_xmon_load(a64_cpu *c, a64_u64 addr, a64_u32 n, a64_u64 *lo, a64_u64 *hi);
int  a64_xmon_store(a64_cpu *c, a64_u64 addr, a64_u32 n, a64_u64 lo, a64_u64 hi);
void a64_xmon_clrex(a64_cpu *c);
void a64_xmon_invalidate(a64_cpu *c);
void a64_xmon_fork_child(a64_cpu *self);

#if defined(A64_RUNTIME_MOS)
/* a pilha do host é A64RT_HSTACK e a rt_thread_t está na base; sem TLS de C no freestanding */
static inline a64_cpu *a64_xmon_self(void) {
    unsigned long sp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(sp));
    unsigned long base = sp & ~(0x100000ul - 1);
    if (*(unsigned long *)base != 0x4441455254343641ul) return 0;
    if (*(unsigned long *)(base + 8) != base) return 0;
    return (a64_cpu *)(base + 16);
}
#else
extern _Thread_local a64_cpu *a64_xmon_tls;
static inline a64_cpu *a64_xmon_self(void) { return a64_xmon_tls; }
#endif

static inline a64_xmon_gate *a64_xmon_gate_of(a64_cpu *c) {
    return c ? (a64_xmon_gate *)(uintptr_t)c->reserved[A64_XMON_GATE] : 0;
}

/* publica o store antes dos dados. Caminho rápido: dois stores no gate e um load de a64_xmon_live. */
static inline void a64_xmon_note(a64_u64 addr, a64_u32 len) {
    a64_cpu *c = a64_xmon_self();
    if (!c || !len) return;
    a64_xmon_gate *g = a64_xmon_gate_of(c);
    if (!g) { a64_xmon_prepare(c, addr, len); return; }
    if (g->held) { g->suppressed++; return; }
    g->addr = addr;
    g->len = len;
    __atomic_store_n(&g->inflight, 1, __ATOMIC_RELEASE);
    if (__atomic_load_n(&a64_xmon_live, __ATOMIC_ACQUIRE) == 0) return;
    a64_xmon_bump(c);
}

static inline void a64_xmon_note_end(void) {
    a64_cpu *c = a64_xmon_self();
    if (!c) return;
    a64_xmon_gate *g = a64_xmon_gate_of(c);
    if (!g) return;
    if (g->suppressed) { g->suppressed--; return; }
    __atomic_store_n(&g->inflight, 0, __ATOMIC_RELEASE);
}
