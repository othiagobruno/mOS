/* limiteteste — limites de memória do kernel (SYS_VMCTL, kernel/include/vmpolicy.h, proc/process.c).
 *
 * 1. rlimit: NOFILE com o teto do kernel no cur e max infinito; subir o max de volta sem root dá EPERM; RLIMIT_DATA
 *    baixo faz o brk recusar; cur > max dá EINVAL.
 * 2. limite de footprint (MOS_VM_SETLIMIT, como o memorystatus): o filho põe 16 MiB e toca 64 MiB; o kernel o encerra
 *    com SIGKILL na falta de página que passa do limite.
 * 3. jetsam: um filho com prioridade de jetsam 0 enche a RAM com páginas que não comprimem até o nível crítico e fica
 *    parado lá. O vm_pageout não consegue liberar nada e, depois de 3 s, encerra o de menor prioridade e maior
 *    footprint — este filho. O pai confere o SIGKILL e o contador jetsam_kills.
 *
 * Saída: "limiteteste: TODOS OS TESTES OK" ou "limiteteste: N FALHA(S)".
 */
#include "mos.h"

static int fails;
#define CHECK(c, ...) do { if (c) print("  ok  " __VA_ARGS__); else { fails++; print("  FALHOU  " __VA_ARGS__); } print("\n"); } while (0)

static i64 vmctl(u64 op, u64 a, u64 b) { return sys3(SYS_VMCTL, op, a, b); }
static int stats(mos_vmstats_t *s) { return (int)vmctl(MOS_VM_STATS, (u64)s, 0); }

static u64 rng = 0x9E3779B97F4A7C15ULL;
static u64 xr(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

static void child_footprint(void) {
    if (vmctl(MOS_VM_SETLIMIT, 0, 16) != 0) exit(2);
    u64 len = 64ull << 20;
    u64 *m = (u64 *)sys3(SYS_MMAP, len, 3, 0);
    if ((i64)(u64)m <= 0) exit(3);
    for (u64 i = 0; i < len / 8; i += 512) m[i] = i;   /* uma escrita por página */
    exit(0);                                            /* não devia chegar aqui */
}

static void child_jetsam(void) {
    if (vmctl(MOS_VM_SETJPRIO, 0, 0) != 0) exit(2);
    mos_vmstats_t s;
    if (stats(&s) != 0) exit(3);
    u64 total = s.total_pages, floor = total / 16 < 256 ? 256 : total / 16;
    u64 chunk = 256 * 4096;                              /* 1 MiB por vez */
    /* até 25 s: enche até o crítico e volta a encher sempre que o compressor liberar algo (páginas dos outros) */
    for (int tick = 0; tick < 250; tick++) {
        for (int k = 0; k < 64; k++) {
            stats(&s);
            if (s.free_pages < floor / 2 - floor / 8) break;      /* bem dentro do crítico */
            u64 *m = (u64 *)sys3(SYS_MMAP, chunk, 3, 0);
            if ((i64)(u64)m <= 0) break;
            for (u64 i = 0; i < chunk / 8; i++) m[i] = xr();     /* aleatório: o compressor recusa */
        }
        sys3(SYS_SLEEP, 100, 0, 0);
    }
    exit(0);
}

static void run(void) {
    mos_rlim_t r;
    CHECK(vmctl(MOS_VM_GETRLIM, 8, (u64)&r) == 0 && r.cur == 256 && r.max == 0x7FFFFFFFFFFFFFFFull,
          "RLIMIT_NOFILE: cur 256 (teto do kernel), max infinito");
    mos_rlim_t big = {10240, 0x7FFFFFFFFFFFFFFFull};
    CHECK(vmctl(MOS_VM_SETRLIM, 8, (u64)&big) == 0 && vmctl(MOS_VM_GETRLIM, 8, (u64)&r) == 0 && r.cur == 256,
          "pedir 10240 fds fica no teto de 256, sem erro");
    mos_rlim_t bad = {10, 5};
    CHECK(vmctl(MOS_VM_SETRLIM, 2, (u64)&bad) == -MOS_EINVAL, "cur > max: EINVAL");

    int c = fork();
    if (c == 0) {
        mos_rlim_t low = {1 << 20, 1 << 20};
        if (vmctl(MOS_VM_SETRLIM, 2, (u64)&low) != 0) exit(2);                 /* RLIMIT_DATA 1 MiB */
        mos_rlim_t up = {1 << 20, 1 << 22};
        if (vmctl(MOS_VM_SETRLIM, 2, (u64)&up) != -MOS_EPERM) exit(3);          /* uid 501: não sobe o max */
        i64 b0 = sys3(SYS_BRK, 0, 0, 0);
        if (sys3(SYS_BRK, (u64)b0 + (4u << 20), 0, 0) >= 0) exit(4);            /* 4 MiB além do brk: recusado */
        if (sys3(SYS_BRK, (u64)b0 + (64u << 10), 0, 0) < 0) exit(5);            /* 64 KiB: cabe */
        exit(0);
    }
    CHECK(c > 0 && wait(c) == 0, "RLIMIT_DATA no brk e EPERM ao subir o max sem root (filho)");
    CHECK(vmctl(MOS_VM_FOOTPRINT, 0, 0) > 0, "MOS_VM_FOOTPRINT do próprio processo");

    c = fork();
    if (c == 0) child_footprint();
    int st = c > 0 ? wait(c) : 1;
    CHECK(st == -MOS_SIGKILL, "passar do limite de footprint encerra com SIGKILL (saída %d)", (i64)st);

    mos_vmstats_t a, b;
    stats(&a);
    if (!a.total_pages || a.total_pages > (4ull << 30) / 4096) {
        print("limiteteste: jetsam pulado (RAM grande demais para encher no teste)\n");
        return;
    }
    c = fork();
    if (c == 0) child_jetsam();
    st = c > 0 ? wait(c) : 1;
    stats(&b);
    CHECK(st == -MOS_SIGKILL, "pressão crítica parada: o jetsam encerra o de prioridade 0 (saída %d)", (i64)st);
    CHECK(b.jetsam_kills > a.jetsam_kills, "contador jetsam_kills subiu (%d)", (i64)(b.jetsam_kills - a.jetsam_kills));
    CHECK(b.free_pages > a.free_pages / 2, "a memória do encerrado voltou");
}

void _start(const char *arg) {
    (void)arg;
    run();
    if (fails) { print("limiteteste: %d FALHA(S)\n", (i64)fails); exit(1); }
    print("limiteteste: TODOS OS TESTES OK\n");
    exit(0);
}
