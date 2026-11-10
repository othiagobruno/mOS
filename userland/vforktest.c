/* vfork: o pai dorme, o filho escreve na mesma memória e o exec não apaga o pai. */
#include "mos.h"

static int fails;
static void line(const char *s) { puts(s); puts("\n"); }
#define CHECK(c, s) do { if (c) line("  ok  " s); else { fails++; line("  FALHOU  " s); } } while (0)

static volatile int shared = 1;
static volatile int alive = 42;

void _start(void) {
    int pid = (int)sys3(SYS_VFORK, 0, 0, 0);
    if (pid == 0) {
        shared = 2;
        exit(7);
    }
    CHECK(pid > 0, "vfork devolveu o pid ao pai");
    CHECK(shared == 2, "o filho escreveu na memória do pai");
    CHECK(wait(pid) == 7, "o _exit do filho acordou o pai");

    pid = (int)sys3(SYS_VFORK, 0, 0, 0);
    if (pid == 0) {
        mos_spawn_vec_t v;
        for (unsigned i = 0; i < sizeof v; i++) ((unsigned char *)&v)[i] = 0;
        sys3(SYS_EXECVE, (u64)"/Sistema/Programas/vforkchild", (u64)&v, 0);
        exit(8);
    }
    CHECK(pid > 0 && alive == 42, "depois do exec o pai ainda está no espaço antigo");
    CHECK(wait(pid) == 9, "o exec do filho rodou o vforkchild");
    puts(fails ? "vforktest: FALHOU\n" : "vforktest: TODOS OS TESTES OK\n");
    exit(fails ? 1 : 0);
}
