/* mOS — filas de futex por chave (SYS_FUTEX_WAIT / SYS_FUTEX_WAKE).
 *
 * Antes o FUTEX_WAKE percorria todas as threads do sistema. Agora cada thread em espera está em até duas listas
 * de uma tabela de hash de 512 baldes:
 *   lista 0: chave (processo, endereço virtual) — sempre;
 *   lista 1: chave física (página compartilhada de verdade, futex_shared_key em process.c) — quando há.
 * O wake procura na lista 0 de (p, va) quem é do mesmo processo e endereço, e na lista 1 da chave física quem tem a
 * mesma chave: a mesma regra de antes (mesmo processo e endereço, ou a mesma chave física), sem a varredura.
 *
 * A entrada é tirada no wake, no prazo (sched.c), na próxima espera da mesma thread e no reap. Quem saiu da espera
 * por outro caminho (sinal, morte) fica na lista até um wake do mesmo balde a ver fora da espera e a soltar: a
 * conferência é pelo estado (TH_WAITING com wait_pid = -8), nunca pela presença na lista.
 * Tudo roda com a trava grande (syscalls e escalonador).
 */
#ifdef MOS_HOST_TEST
#include "futexq_host.h"
#else
#include "proc.h"
#endif

#define FQ_BITS 9
#define FQ_N (1u << FQ_BITS)

static thread_t *fq_head[2][FQ_N];

static inline u32 fq_hash(u64 a, u64 b) {
    u64 x = a * 0x9E3779B97F4A7C15ULL ^ (b + 0x632BE59BD9B4E019ULL) * 0xC2B2AE3D27D4EB4FULL;
    return (u32)(x >> (64 - FQ_BITS));
}
static inline u32 fq_bucket_pv(const process_t *p, u64 va) { return fq_hash((u64)p, va); }
static inline u32 fq_bucket_ph(u64 key) { return fq_hash(key, 0x5048); }

static void fq_link(thread_t *t, int l, u32 b) {
    t->fq_prev[l] = NULL;
    t->fq_next[l] = fq_head[l][b];
    if (t->fq_next[l]) t->fq_next[l]->fq_prev[l] = t;
    fq_head[l][b] = t;
    t->fq_b[l] = (u16)b;
    t->fq_on |= (u8)(1u << l);
}
static void fq_unlink_one(thread_t *t, int l) {
    if (!(t->fq_on & (1u << l))) return;
    if (t->fq_prev[l]) t->fq_prev[l]->fq_next[l] = t->fq_next[l];
    else fq_head[l][t->fq_b[l]] = t->fq_next[l];
    if (t->fq_next[l]) t->fq_next[l]->fq_prev[l] = t->fq_prev[l];
    t->fq_next[l] = t->fq_prev[l] = NULL;
    t->fq_on &= (u8)~(1u << l);
}

void futexq_remove(thread_t *t) {
    if (!t) return;
    fq_unlink_one(t, 0);
    fq_unlink_one(t, 1);
}

/* t já tem futex_va, futex_phys e proc preenchidos */
void futexq_add(thread_t *t) {
    futexq_remove(t);
    fq_link(t, 0, fq_bucket_pv(t->proc, t->futex_va));
    if (t->futex_phys) fq_link(t, 1, fq_bucket_ph(t->futex_phys));
}

static inline bool fq_waiting(const thread_t *t) { return t->state == TH_WAITING && t->wait_pid == -8 && t->futex_va; }

/* acorda até n (n = 0 vale 1). wake(w) faz rax = 0 e o põe pronto. Devolve quantas acordou. */
int futexq_wake(process_t *p, u64 va, u64 key, int n, void (*wake)(thread_t *)) {
    int done = 0;
    if (n <= 0) n = 1;
    for (int l = 0; l < 2 && done < n; l++) {
        if (l == 1 && !key) break;
        u32 b = l ? fq_bucket_ph(key) : fq_bucket_pv(p, va);
        thread_t *w = fq_head[l][b];
        while (w && done < n) {
            thread_t *nx = w->fq_next[l];
            if (!fq_waiting(w)) { futexq_remove(w); w = nx; continue; }   /* saiu da espera por outro caminho */
            bool hit = l ? w->futex_phys == key : (w->proc == p && w->futex_va == va);
            if (hit) {
                futexq_remove(w);
                w->futex_va = 0;
                w->futex_phys = 0;
                w->wake_at = 0;
                wake(w);
                done++;
            }
            w = nx;
        }
    }
    return done;
}

/* diagnóstico e teste: quantas entradas há na tabela */
u32 futexq_count(void) {
    u32 n = 0;
    for (int l = 0; l < 2; l++)
        for (u32 b = 0; b < FQ_N; b++)
            for (thread_t *t = fq_head[l][b]; t; t = t->fq_next[l]) n++;
    return n;
}
