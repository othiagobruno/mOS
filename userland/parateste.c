/* mOS — SIGSTOP/SIGCONT e POSIX_SPAWN_START_SUSPENDED.
 *
 * ELF de ring 3. O autoteste (userland/teste.c) o dispara quando a imagem o traz, junto de
 * userland/preso.c. Fala com o kernel por int 0x80.
 *
 *   1. fork: o filho bloqueia num read e uma segunda thread conta numa página MAP_SHARED.
 *      SIGSTOP para as duas; waitpid(WUNTRACED) informa SIGSTOP; o contador não anda.
 *      SIGCONT: waitpid(WCONTINUED) informa SIGCONT, o contador anda, o filho sai 42.
 *   2. SYS_SPAWNVEC de "preso" com MOS_SPAWN_START_SUSPENDED (0x80). O filho não grava
 *      /tmp/preso.saiu enquanto está parado. SIGCONT o solta e ele sai 42.
 *
 * Status de parada no kernel: (sinal << 8) | 0x7f, com o número do kernel (SIGSTOP 20, SIGCONT 19).
 */
#include "mos.h"

static int fails;
#define CHECK(c, ...) do { if (c) print("  ok  " __VA_ARGS__); else { fails++; print("  FALHOU  " __VA_ARGS__); } print("\n"); } while (0)

static inline i64 sys5(u64 n, u64 a, u64 b, u64 c, u64 d, u64 e) {
    i64 r;
    register u64 r10 __asm__("r10") = d;
    register u64 r8 __asm__("r8") = e;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8) : "memory", "rcx", "r9", "r11");
    return r;
}

static int waitpid(int pid, int *st, int opt) {
    return (int)sys3(SYS_WAITPID, (u64)pid, (u64)st, (u64)opt);
}

static volatile u64 *tick;
static int spinner(void *arg) {
    (void)arg;
    for (;;) tick[0]++;
    return 0;
}

static void teste_sinais(void) {
    tick = (volatile u64 *)sys5(SYS_MMAP, 4096, 6, 0, 0, 3);   /* MAP_SHARED|MAP_ANON, leitura+escrita */
    int mapok = (i64)tick > 0;
    if (mapok) tick[0] = 0;
    int down[2], up[2];
    int pok = mapok && pipe(down) == 0 && pipe(up) == 0;
    int fk = pok ? fork() : -1;
    if (fk == 0) {
        close(down[1]);
        close(up[0]);
        thread_create(spinner, 0);
        char a = 'A';
        write(up[1], &a, 1);
        char z = 0;
        read(down[0], &z, 1);
        exit(z == 'Z' ? 42 : 1);
    }
    if (pok) { close(down[0]); close(up[1]); }
    char got = 0;
    if (fk > 0) read(up[0], &got, 1);
    CHECK(fk > 0 && got == 'A', "filho bloqueado no read");
    CHECK(fk > 0 && sigkill(fk, MOS_SIGSTOP) == 0, "SIGSTOP enviado");
    /* o SIGSTOP já gravou process.stopped antes de voltar; a amostra de baixo é o piso */
    u64 antes = mapok ? tick[0] : 0;
    sleep_ms(80);
    u64 durante = mapok ? tick[0] : 1;
    int st = 0;
    int w = fk > 0 ? waitpid(fk, &st, MOS_WUNTRACED) : -1;
    CHECK(w == fk && (st & 0xff) == 0x7f && ((st >> 8) & 0xff) == MOS_SIGSTOP,
          "waitpid WUNTRACED ve SIGSTOP (status 0x%x)", (i64)(u64)(unsigned)st);
    CHECK(durante == antes, "contador parado sob SIGSTOP (%u)", durante);
    int w0 = fk > 0 ? waitpid(fk, &st, MOS_WNOHANG | MOS_WUNTRACED) : -1;
    CHECK(w0 == 0, "a parada nao se repete no waitpid");
    CHECK(fk > 0 && sigkill(fk, MOS_SIGCONT) == 0, "SIGCONT enviado");
    w = fk > 0 ? waitpid(fk, &st, MOS_WCONTINUED) : -1;
    CHECK(w == fk && (st & 0xff) == 0x7f && ((st >> 8) & 0xff) == MOS_SIGCONT,
          "waitpid WCONTINUED ve SIGCONT (status 0x%x)", (i64)(u64)(unsigned)st);
    sleep_ms(40);
    u64 depois = mapok ? tick[0] : 0;
    CHECK(depois != durante, "contador andou depois do SIGCONT (%u -> %u)", durante, depois);
    if (fk > 0) write(down[1], "Z", 1);
    w = fk > 0 ? waitpid(fk, &st, 0) : -1;
    CHECK(w == fk && st == (42 << 8), "filho sai 42 depois de continuar (status 0x%x)", (i64)(u64)(unsigned)st);
}

static void teste_spawn_suspenso(void) {
    unlink("/tmp/preso.saiu");
    char *av[] = { "preso", 0 };
    mos_spawn_vec_t v;
    memset(&v, 0, sizeof v);
    v.argv = (u64)av;
    v.flags = MOS_SPAWN_START_SUSPENDED;
    int pid = (int)sys3(SYS_SPAWNVEC, (u64)"preso", (u64)&v, 0);
    sleep_ms(150);
    i64 marca = file_size("/tmp/preso.saiu");
    int st = 0;
    int w = pid > 0 ? waitpid(pid, &st, MOS_WNOHANG | MOS_WUNTRACED) : -1;
    CHECK(pid > 0 && marca < 0 && w == pid && (st & 0xff) == 0x7f && ((st >> 8) & 0xff) == MOS_SIGSTOP,
          "spawn suspenso nasce parado e nao roda (pid %d, marca %d, status 0x%x)",
          (i64)pid, marca, (i64)(u64)(unsigned)st);
    if (pid > 0) sigkill(pid, MOS_SIGCONT);
    w = pid > 0 ? waitpid(pid, &st, 0) : -1;
    marca = file_size("/tmp/preso.saiu");
    CHECK(w == pid && st == (42 << 8) && marca > 0, "SIGCONT solta o spawn suspenso (status 0x%x, marca %d)",
          (i64)(u64)(unsigned)st, marca);
}

void _start(const char *arg) {
    (void)arg;
    print("parateste: pid %d\n", (i64)getpid());
    teste_sinais();
    teste_spawn_suspenso();
    print(fails ? "parateste: %d FALHA(S)\n" : "parateste: TODOS OS TESTES OK\n", (i64)fails);
    exit(fails ? 1 : 0);
}
