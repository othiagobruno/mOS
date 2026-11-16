/* Host: filas de futex (kernel/proc/futexq.c) contra a varredura antiga, e a prioridade do escalonador (smp_perf.h).
 * gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/include -I tools tools/futexq_host.c kernel/proc/futexq.c \
 *     -o /tmp/futexq_host && /tmp/futexq_host */
#include "futexq_host.h"
#include "smp_perf.h"
#include <stdio.h>
#include <string.h>

void futexq_add(thread_t *t);
void futexq_remove(thread_t *t);
int futexq_wake(process_t *p, u64 va, u64 key, int n, void (*wake)(thread_t *));
u32 futexq_count(void);

static int falhas;
static void check(int c, const char *m) { if (!c) { printf("FALHA: %s\n", m); falhas++; } }
static u64 rng = 0x1234567;
static u64 xr(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

enum { NT = 600, NP = 4 };
static thread_t th[NT];
static process_t pr[NP];
static int woken_new[NT];

static void wake_cb(thread_t *w) { w->state = TH_READY; woken_new[w - th]++; }

/* regra antiga (process.c antes deste ramo): mesmo processo e va, ou a mesma chave física; na ordem da lista */
static int ref_wake(thread_t *ref, process_t *p, u64 va, u64 key, int n, int *woke) {
    int done = 0;
    for (int i = 0; i < NT && done < n; i++) {
        thread_t *w = &ref[i];
        if (w->state != TH_WAITING || w->wait_pid != -8) continue;
        if (!(w->proc == &pr[p - pr] && w->futex_va == va) && !(key && w->futex_phys == key)) continue;
        w->state = TH_READY; w->futex_va = 0; woke[i]++; done++;
    }
    return done;
}

int main(void) {
    static thread_t ref[NT];
    static int woken_ref[NT];
    u64 vas[6] = {0x400000001000, 0x400000001004, 0x400000002000, 0x400000003000, 0x400000003008, 0x7000};
    u64 keys[3] = {0, 0x12345000, 0x22222008};
    for (int i = 0; i < NT; i++) { th[i].id = i; th[i].state = TH_RUNNING; th[i].proc = &pr[i % NP]; }
    for (int i = 0; i < NP; i++) pr[i].pid = i + 1;
    long wakes = 0;
    for (int step = 0; step < 200000; step++) {
        int i = (int)(xr() % NT);
        thread_t *t = &th[i];
        u64 r = xr() % 10;
        if (r < 5 && t->state != TH_WAITING) {                       /* espera */
            t->futex_va = vas[xr() % 6];
            t->futex_phys = keys[xr() % 3];
            t->wait_pid = -8;
            t->state = TH_WAITING;
            futexq_add(t);
        } else if (r < 6 && t->state == TH_WAITING) {                /* sinal ou prazo: sai sem passar pelo wake */
            t->state = TH_READY;
            t->futex_va = 0;
            if (xr() & 1) futexq_remove(t);
        } else {                                                      /* wake de alguém */
            process_t *p = &pr[xr() % NP];
            u64 va = vas[xr() % 6], key = keys[xr() % 3];
            int n = (int)(xr() % 4);
            /* a referência: as mesmas threads, mas sem ordem de lista; com n = "todas" a contagem é igual */
            memcpy(ref, th, sizeof th);
            memset(woken_ref, 0, sizeof woken_ref);
            memset(woken_new, 0, sizeof woken_new);
            int a = ref_wake(ref, p, va, key, 1 << 30, woken_ref);
            int b = futexq_wake(p, va, key, 1 << 30, wake_cb);
            if (a != b || memcmp(woken_ref, woken_new, sizeof woken_ref)) { check(0, "wake de todas acorda as mesmas threads da varredura"); break; }
            (void)n;
            wakes++;
            /* volta a deixar algumas esperando para o próximo passo */
        }
        if (step % 5000 == 0) {
            u32 inq = futexq_count();
            int waiting = 0;
            for (int k = 0; k < NT; k++) waiting += th[k].state == TH_WAITING;
            if ((int)inq < waiting) { check(0, "toda thread em espera está na tabela"); break; }
        }
    }
    /* n limitado: no máximo n, e só quem casa */
    for (int i = 0; i < NT; i++) { th[i].state = TH_RUNNING; futexq_remove(&th[i]); }
    check(futexq_count() == 0, "remove esvazia a tabela");
    for (int i = 0; i < 10; i++) {
        th[i].proc = &pr[0]; th[i].futex_va = vas[0]; th[i].futex_phys = 0; th[i].wait_pid = -8; th[i].state = TH_WAITING;
        futexq_add(&th[i]);
    }
    memset(woken_new, 0, sizeof woken_new);
    check(futexq_wake(&pr[0], vas[0], 0, 3, wake_cb) == 3, "wake(3) acorda 3");
    check(futexq_wake(&pr[1], vas[0], 0, 100, wake_cb) == 0, "outro processo, mesmo va: nenhuma");
    check(futexq_wake(&pr[0], vas[0], 0, 0, wake_cb) == 1, "n = 0 vale 1");
    check(futexq_wake(&pr[0], vas[0], 0, 100, wake_cb) == 6, "o resto");
    check(futexq_count() == 0, "acordadas saem da tabela");
    /* chave física: processo diferente, va diferente, mesma página compartilhada */
    th[20].proc = &pr[1]; th[20].futex_va = 0x500000000010; th[20].futex_phys = 0xABC010; th[20].wait_pid = -8; th[20].state = TH_WAITING;
    futexq_add(&th[20]);
    check(futexq_wake(&pr[2], 0x600000000010, 0xABC010, 1, wake_cb) == 1, "chave física acorda o outro processo");
    printf("  %ld wakes conferidos contra a varredura\n", wakes);

    /* prioridade */
    check(smp_qos_base_pri(smp_qos_from_darwin(0x21)) == 47 && smp_qos_base_pri(smp_qos_from_darwin(0x09)) == 4, "QoS -> prioridade Mach");
    check(smp_eff_pri(31, 0, 0) == 31 && smp_eff_pri(31, 80, 0) == 21 && smp_eff_pri(31, 10000, 0) == 15, "decaimento até 16");
    check(smp_eff_pri(31, 10000, 1) == 31, "fixa não decai");
    check(smp_eff_pri(2, 10000, 0) == 0 && smp_eff_pri(200, 0, 1) == 127, "faixa 0..127");
    check(smp_usage_decay(800, 1000, 1099) == 800 && smp_usage_decay(800, 1000, 1100) == 400 && smp_usage_decay(800, 1000, 1350) == 100,
          "meia-vida de 100 ms");
    check(smp_usage_decay(800, 1000, 900) == 800 && smp_usage_decay(~0u, 0, 1ULL << 40) == 0, "relógio para trás e uso antigo");
    /* uma thread de CPU cheia (interativa) cai abaixo de uma padrão ociosa, mas não abaixo de utilidade ociosa */
    int busy = smp_eff_pri(47, 200, 0), idle_def = smp_eff_pri(31, 0, 0), idle_ut = smp_eff_pri(20, 0, 0);
    check(busy == 31 && busy >= idle_def && busy > idle_ut, "interativa ocupada empata com padrão ociosa, acima de utilidade");

    if (falhas) { printf("futexq_host: %d falhas\n", falhas); return 1; }
    printf("futexq_host: ok\n");
    return 0;
}
