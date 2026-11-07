/* mOS — teste POSIX de um programa Mach-O (libSystem): fork/execvp/waitpid, dup2 + pipe, symlink,
 * lstat/stat e "#!". É a base do bash e do Homebrew (docs/porte-brew.md).
 * Compilado por build.ps1 -Tests como /Sistema/Programas/posixteste; o "teste" nativo o chama no fim. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("  ok  " __VA_ARGS__); else { fails++; printf("  FALHOU  " __VA_ARGS__); } printf("\n"); } while (0)


/* pthread_rwlock e pthread_once com threads em núcleos diferentes (SMP): eram no-op/não atômicos no libSystem */
static pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
static volatile long rw_a, rw_b, rw_bad;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static volatile int once_runs, once_val, once_bad;
static void once_init(void) {
    __atomic_add_fetch(&once_runs, 1, __ATOMIC_SEQ_CST);
    for (volatile int i = 0; i < 2000000; i++) {}             /* devagar: as outras threads chegam no meio */
    once_val = 1234;
}
static void *rw_worker(void *arg) {
    (void)arg;
    pthread_once(&once, once_init);
    if (once_val != 1234) __atomic_add_fetch(&once_bad, 1, __ATOMIC_SEQ_CST);
    for (int i = 0; i < 20000; i++) {
        if (i % 4) {
            pthread_rwlock_rdlock(&rw);
            long a = rw_a;
            for (volatile int k = 0; k < 20; k++) {}
            if (rw_b != a) rw_bad++;
            pthread_rwlock_unlock(&rw);
        } else {
            pthread_rwlock_wrlock(&rw);
            long a = rw_a;
            for (volatile int k = 0; k < 20; k++) {}
            rw_a = a + 1;
            rw_b = a + 1;
            pthread_rwlock_unlock(&rw);
        }
    }
    return 0;
}
static void rwlock_test(void) {
    pthread_t th[4];
    for (int i = 0; i < 4; i++) pthread_create(&th[i], 0, rw_worker, 0);
    for (int i = 0; i < 4; i++) pthread_join(th[i], 0);
    CHECK(once_runs == 1 && !once_bad, "pthread_once com 4 threads (rodou %d vez(es), %d viram antes do fim)", once_runs, once_bad);
    CHECK(rw_a == 4 * 5000 && rw_b == rw_a && !rw_bad, "pthread_rwlock com 4 threads (%ld escritas, %ld leituras inconsistentes)", rw_a, rw_bad);
}
static int run_wait(const char *path, char *const av[]) {
    pid_t pid = fork();
    if (pid == 0) { execvp(path, av); _exit(99); }
    int st = -1;
    if (waitpid(pid, &st, 0) != pid) return -1;
    return st;
}

int main(void) {
    char *av[] = {"filho", "posix", 0};
    int st = run_wait("filho", av);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 42, "fork + execvp + waitpid (WEXITSTATUS %d)", WEXITSTATUS(st));

    pid_t q = fork();
    if (q == 0) _exit(5);
    int s2 = 0;
    pid_t w = wait(&s2);
    CHECK(w == q && WIFEXITED(s2) && WEXITSTATUS(s2) == 5, "wait() devolve o filho e o código 5");
    CHECK(waitpid(-1, &s2, WNOHANG) == -1, "waitpid sem filhos falha (ECHILD)");

    int p[2];
    CHECK(pipe(p) == 0, "pipe");
    pid_t c = fork();
    if (c == 0) {
        dup2(p[1], 1);
        close(p[0]);
        char *a2[] = {"filho", "pelo-pipe", 0};
        execv("/Sistema/Programas/filho", a2);
        _exit(99);
    }
    close(p[1]);
    char buf[256] = {0};
    long n = 0, k;
    while (n < (long)sizeof buf - 1 && (k = read(p[0], buf + n, sizeof buf - 1 - n)) > 0) n += k;
    close(p[0]);
    waitpid(c, &st, 0);
    CHECK(strstr(buf, "pelo-pipe") != 0, "dup2 leva o stdout do execv para o pipe (%ld bytes)", n);

    unlink("/tmp-posix-ln");
    CHECK(symlink("/Sistema/Programas/filho", "/tmp-posix-ln") == 0, "symlink");
    struct stat ls, ss;
    CHECK(lstat("/tmp-posix-ln", &ls) == 0 && S_ISLNK(ls.st_mode), "lstat vê o link (modo %o)", (unsigned)ls.st_mode);
    CHECK(stat("/tmp-posix-ln", &ss) == 0 && S_ISREG(ss.st_mode) && (ss.st_mode & 0111), "stat segue o link até um executável (modo %o)", (unsigned)ss.st_mode);
    char tgt[256] = {0};
    long tl = readlink("/tmp-posix-ln", tgt, sizeof tgt - 1);
    CHECK(tl > 0 && !strcmp(tgt, "/Sistema/Programas/filho"), "readlink: %s", tgt);
    char *a3[] = {"/tmp-posix-ln", 0};
    st = run_wait("/tmp-posix-ln", a3);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 42, "execvp por link simbólico");

    int fd = open("/tmp-posix.sh", O_WRONLY | O_CREAT | O_TRUNC, 0755);
    const char sh[] = "#!/Sistema/Programas/filho\n";
    write(fd, sh, sizeof sh - 1);
    close(fd);
    char *a4[] = {"/tmp-posix.sh", "arg", 0};
    st = run_wait("/tmp-posix.sh", a4);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 42, "execvp de script com #!");

    rwlock_test();
    printf(fails ? "posixteste: %d FALHA(S)\n" : "posixteste: TODOS OS TESTES OK\n", fails);
    return fails ? 1 : 0;
}
