/* mOS — teste do modo usuário: syscalls, memória, arquivos, processos filhos, isolamento de falhas
 * e preempção. Rodado no boot quando a imagem é gerada com build.ps1 -Tests (marcador .autoteste). */
#include "mos.h"

static int fails;
#define CHECK(c, ...) do { if (c) print("  ok  " __VA_ARGS__); else { fails++; print("  FALHOU  " __VA_ARGS__); } print("\n"); } while (0)

static u64 fib(u64 n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

static volatile int got_sig, got_segv;
static volatile u64 counters[2];
static void on_usr1(int sig) { got_sig = sig; }
static void on_segv(int sig) { got_segv = sig; exit(77); }        /* tratador que encerra com código próprio */
static int worker(void *arg) {
    int k = (int)(u64)arg;
    for (int i = 0; i < 200000; i++) counters[k]++;
    sleep_ms(20);
    return 100 + k;
}

void _start(const char *arg) {
    print("teste: pid %d, argumento \"%s\", %d núcleo(s)\n", (i64)getpid(), arg ? arg : "(nenhum)", (i64)cpus());

    /* memória: heap via brk, pilha funda (recursão) */
    u64 base = brk(0);
    CHECK(brk(base + (1 << 20)) == base + (1 << 20), "brk: +1 MiB");
    unsigned char *heap = (unsigned char *)base;
    for (int i = 0; i < (1 << 20); i++) heap[i] = (unsigned char)(i * 7);
    int good = 1;
    for (int i = 0; i < (1 << 20); i += 4093) if (heap[i] != (unsigned char)(i * 7)) good = 0;
    CHECK(good, "heap escrita e relida");
    CHECK(fib(24) == 46368, "recursão (fib 24 = %u)", fib(24));

    /* tempo e sono: o processo dorme sem ocupar a CPU */
    u64 t0 = time_ms();
    sleep_ms(100);
    u64 dt = time_ms() - t0;
    CHECK(dt >= 100 && dt < 400, "sleep 100 ms (dormiu %u ms)", dt);

    /* arquivos pelo VFS (e pela persistência) */
    static const char msg[] = "escrito por um processo em ring 3\n";
    CHECK(write_file("/tmp/proc-teste.txt", msg, sizeof msg - 1) == 0, "write_file /tmp/proc-teste.txt");
    char buf[128];
    i64 n = read_file("/tmp/proc-teste.txt", buf, sizeof buf);
    CHECK(n == (i64)sizeof msg - 1 && !memcmp(buf, msg, n), "read_file devolve o mesmo conteúdo (%d bytes)", n);
    CHECK(file_size("/nao/existe") == -MOS_ENOENT, "arquivo inexistente devolve -ENOENT");
    CHECK(read_file((const char *)0x1000, buf, 10) == -MOS_EFAULT, "ponteiro do kernel é recusado (-EFAULT)");

    /* descritores: open/write/lseek/read/fstat, append, pastas, readdir, rename, unlink */
    CHECK(mkdir("/tmp/dir-teste") == 0, "mkdir /tmp/dir-teste");
    int fd = open("/tmp/dir-teste/a.txt", MOS_O_RDWR | MOS_O_CREAT | MOS_O_TRUNC);
    CHECK(fd >= 3, "open O_CREAT (fd %d)", (i64)fd);
    CHECK(write(fd, "0123456789", 10) == 10, "write 10 bytes no arquivo");
    CHECK(lseek(fd, 3, MOS_SEEK_SET) == 3 && write(fd, "abc", 3) == 3, "lseek + sobrescrever no meio");
    CHECK(lseek(fd, 0, MOS_SEEK_SET) == 0 && read(fd, buf, sizeof buf) == 10 && !memcmp(buf, "012abc6789", 10), "reler: 012abc6789");
    mos_stat_t st;
    CHECK(fstat(fd, &st) == 0 && st.size == 10 && !st.is_dir, "fstat: 10 bytes");
    CHECK(close(fd) == 0 && close(fd) == -MOS_EBADF, "close (e fechar de novo dá -EBADF)");
    fd = open("/tmp/dir-teste/a.txt", MOS_O_WRONLY | MOS_O_APPEND);
    CHECK(fd >= 3 && write(fd, "XY", 2) == 2 && close(fd) == 0, "O_APPEND");
    CHECK(file_size("/tmp/dir-teste/a.txt") == 12, "tamanho após append = 12");
    CHECK(open("/tmp/nao-existe.txt", MOS_O_RDONLY) == -MOS_ENOENT, "open sem O_CREAT em arquivo inexistente");
    CHECK(rename("/tmp/dir-teste/a.txt", "/tmp/dir-teste/b.txt") == 0 && stat("/tmp/dir-teste/b.txt", &st) == 0, "rename");
    char name[64];
    CHECK(readdir("/tmp/dir-teste", 0, name, sizeof name) == 1 && !memcmp(name, "b.txt", 6) &&
          readdir("/tmp/dir-teste", 1, name, sizeof name) == 0, "readdir lista b.txt");
    CHECK(unlink("/tmp/dir-teste") == 0 && stat("/tmp/dir-teste", &st) == -MOS_ENOENT, "unlink da pasta (recursivo)");

    /* memória anônima */
    unsigned char *m = mmap(3 << 20);
    int mok = m != 0;
    for (int i = 0; mok && i < (3 << 20); i += 4096) { if (m[i]) mok = 0; m[i] = (unsigned char)i; }
    CHECK(mok && m[4096 * 7] == (unsigned char)(4096 * 7), "mmap 3 MiB zerada e gravável");
    CHECK(munmap(m, 3 << 20) == 0, "munmap");
    i64 now = unixtime();
    CHECK(now > 1700000000 && now < 4102444800, "unixtime plausível (%d)", now);

    /* threads no mesmo processo (memória compartilhada) */
    int t1 = thread_create(worker, (void *)0), t2 = thread_create(worker, (void *)1);
    int j1 = thread_join(t1), j2 = thread_join(t2);
    CHECK(t1 > 0 && t2 > 0 && j1 == 100 && j2 == 101 && counters[0] == 200000 && counters[1] == 200000,
          "2 threads no mesmo processo (join %d/%d, contadores %u/%u)", (i64)j1, (i64)j2, counters[0], counters[1]);
    CHECK(gettid() > 0 && thread_join(gettid()) < 0, "gettid e join de si mesmo recusado");

    /* fork com cópia na escrita: pai e filho escrevem na mesma variável sem se ver */
    static volatile u64 shared = 1234;
    heap[0] = 11;
    int fk = fork();
    if (fk == 0) {                                                  /* filho */
        int okc = shared == 1234 && heap[0] == 11;
        shared = 5555;
        heap[0] = 22;
        exit(okc && shared == 5555 && heap[0] == 22 ? 33 : 1);
    }
    int fc = wait(fk);
    CHECK(fk > 0 && fc == 33 && shared == 1234 && heap[0] == 11, "fork + COW (filho %d, pai manteve %u/%u)", (i64)fc, shared, (u64)heap[0]);

    /* sinais: tratador no próprio processo, SIGTERM padrão e SIGSEGV com tratador */
    signal(MOS_SIGUSR1, on_usr1);
    sigkill(getpid(), MOS_SIGUSR1);
    CHECK(got_sig == MOS_SIGUSR1, "tratador de SIGUSR1 rodou e voltou");
    int sp = spawn("spin", "0");
    sleep_ms(30);
    CHECK(sigkill(sp, MOS_SIGTERM) == 0 && wait(sp) == -MOS_SIGTERM, "SIGTERM (ação padrão) encerra um processo em laço");
    int sf = fork();
    if (sf == 0) { signal(MOS_SIGSEGV, on_segv); *(volatile int *)0x100000 = 1; exit(5); }
    CHECK(wait(sf) == 77, "SIGSEGV entregue ao tratador do programa");

    /* processos filhos: código de saída, falha isolada, preempção */
    int c = spawn("filho", "olá do pai");
    CHECK(c > 0 && wait(c) == 42, "filho termina com código 42");
    int k = spawn("crash", 0);
    CHECK(k > 0 && wait(k) == -11, "filho que acessa memória do kernel é encerrado (sem derrubar o sistema)");
    int s1 = spawn("spin", "300"), s2 = spawn("spin", "300");
    t0 = time_ms();
    int r1 = wait(s1), r2 = wait(s2);
    dt = time_ms() - t0;
    CHECK(r1 == 0 && r2 == 0, "dois processos em laço infinito de CPU dividem o processador (%u ms)", dt);
    int inf = spawn("spin", "0");                      /* nunca termina sozinho */
    sleep_ms(50);
    CHECK(kill(inf) == 0 && wait(inf) == -9, "kill encerra um processo em laço");

    print(fails ? "teste: %d FALHA(S)\n" : "teste: TODOS OS TESTES OK\n", (i64)fails);
    exit(fails);
}
