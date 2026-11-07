/* mOS — autoteste de SMP: processos e threads de usuário rodando ao mesmo tempo em vários núcleos.
 *
 *   smpteste        roda tudo e imprime "smpteste: OK" (ou FALHOU) no console
 *   smpteste w      operário: laço de CPU sem syscalls; o resultado vai no código de saída
 *
 * 1) um operário sozinho (referência de tempo e de resultado);
 * 2) N operários (processos) ao mesmo tempo: todos têm de devolver o mesmo resultado (registradores e FPU
 *    salvos direito em cada troca de núcleo) e, com mais de um núcleo, terminar bem antes de N vezes o tempo;
 * 3) N threads do mesmo processo ao mesmo tempo, cada uma calculando e fazendo mmap/escreve/confere/munmap
 *    (TLB shootdown entre núcleos: uma página desmapeada por uma thread não pode continuar visível a outra). */
#include "mos.h"

#define ITERS 60000000ULL
#define NW    3

static u64 work(u64 seed, u64 iters) {
    u64 x = seed;
    double d = 1.0;
    for (u64 i = 0; i < iters; i++) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        x ^= x >> 29;
        if ((i & 1023) == 0) d = d * 1.0000001 + (double)(x & 7);   /* SSE no meio: estado salvo por núcleo */
    }
    return x ^ (u64)d;
}

static volatile u64 tres[NW];
static volatile int tbad[NW];

static int tworker(void *arg) {
    int k = (int)(u64)arg;
    tres[k] = work(1, ITERS / 2);
    for (int r = 0; r < 64; r++) {                 /* mapeia, usa e devolve memória enquanto as outras rodam */
        u32 *m = mmap(65536);
        if (!m) { tbad[k]++; continue; }
        for (int i = 0; i < 16384; i += 64) m[i] = (u32)(k * 100000 + r * 1000 + i);
        for (int i = 0; i < 16384; i += 64) if (m[i] != (u32)(k * 100000 + r * 1000 + i)) tbad[k]++;
        munmap(m, 65536);
    }
    return 0;
}

void _start(const char *arg) {
    if (arg && arg[0] == 'w') exit((int)(work(1, ITERS) & 0x7FFF));

    int nc = cpus(), fails = 0;
    print("smpteste: %d núcleo(s) ativos\n", (i64)nc);

    u64 t0 = time_ms();
    int p = spawn("smpteste", "w");
    int ref = p > 0 ? wait(p) : -1;
    u64 one = time_ms() - t0;
    print("smpteste: 1 operário em %u ms (resultado %d)\n", one, (i64)ref);
    if (ref < 0) { print("smpteste: FALHOU (não consegui criar o operário)\n"); exit(1); }

    int pid[NW];
    t0 = time_ms();
    for (int i = 0; i < NW; i++) pid[i] = spawn("smpteste", "w");
    for (int i = 0; i < NW; i++) {
        int c = pid[i] > 0 ? wait(pid[i]) : -1;
        if (c != ref) { print("smpteste: operário %d devolveu %d (esperado %d)\n", (i64)i, (i64)c, (i64)ref); fails++; }
    }
    u64 par = time_ms() - t0;
    u64 x10 = par ? NW * one * 10 / par : 0;
    print("smpteste: %d operários juntos em %u ms (aceleração %u.%ux)\n", (i64)NW, par, x10 / 10, x10 % 10);
    if (nc > 1 && x10 < 15) { print("smpteste: processos não rodaram em paralelo\n"); fails++; }

    int tid[NW];
    t0 = time_ms();
    for (int i = 0; i < NW; i++) tid[i] = thread_create(tworker, (void *)(u64)i);
    for (int i = 0; i < NW; i++) if (tid[i] > 0) thread_join(tid[i]); else fails++;
    u64 tt = time_ms() - t0;
    u64 want = work(1, ITERS / 2);
    for (int i = 0; i < NW; i++) {
        if (tres[i] != want) { print("smpteste: thread %d calculou errado\n", (i64)i); fails++; }
        if (tbad[i]) { print("smpteste: thread %d viu %d palavra(s) erradas na memória\n", (i64)i, (i64)tbad[i]); fails++; }
    }
    print("smpteste: %d threads (cálculo + 64 mmap/munmap cada) em %u ms\n", (i64)NW, tt);

    print("smpteste: %s\n", fails ? "FALHOU" : "OK");
    exit(fails);
}
