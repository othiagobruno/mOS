/* mOS — os casos dos testes do Darling (tests/src: fcntl, fork_wait, libproc, tls1, selfexec), reescritos com o
 * mesmo comportamento esperado do macOS: F_GETPATH, morte por sinal com e sem tratador, proc_name, proc_listpids,
 * proc_listpidspath, variável __thread por thread e execl de si mesmo. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <libproc.h>
#include <sys/param.h>
#include <sys/wait.h>

static int fails, total;
#define EXPECT(got, want, what) do { long long g_ = (long long)(got), w_ = (long long)(want); total++; \
    if (g_ == w_) printf("  ok  %s = %lld\n", what, g_); \
    else { fails++; printf("  FALHOU  %s = %lld (esperado %lld)\n", what, g_, w_); } } while (0)

static void on_sys(int s) { (void)s; _exit(0); }
/* filho espera um SIGSYS: com tratador sai com 0; com SIG_DFL morre pelo sinal */
static int signal_child(int handle) {
    pid_t c = fork();
    if (c == 0) {
        struct sigaction a;
        memset(&a, 0, sizeof a);
        a.sa_handler = handle ? on_sys : SIG_DFL;
        sigaction(SIGSYS, &a, 0);
        sleep(10);
        _exit(1);
    }
    usleep(300000);
    kill(c, SIGSYS);
    int st = 0;
    waitpid(c, &st, 0);
    return st;
}

static __thread int tv = 123;
static void *tls_thread(void *p) { *(int *)p = tv; tv = 7; return 0; }

int main(int argc, char **argv) {
    setbuf(stdout, 0);
    if (argc > 1 && !strcmp(argv[1], "--eco")) { printf("eco: %s\n", argc > 2 ? argv[2] : ""); return 42; }

    printf("fcntl F_GETPATH\n");
    char tmpl[] = "/tmp/darwinmisc.XXXXXX", fpath[MAXPATHLEN] = "", r1[MAXPATHLEN], r2[MAXPATHLEN];
    int fd = mkstemp(tmpl);
    EXPECT(fd >= 0, 1, "mkstemp");
    EXPECT(fcntl(fd, F_GETPATH, fpath), 0, "F_GETPATH");
    EXPECT(realpath(tmpl, r1) && realpath(fpath, r2) && !strcmp(r1, r2), 1, "F_GETPATH é o caminho do arquivo");

    printf("libproc\n");
    char name[1024] = "";
    EXPECT(proc_name(getpid(), name, sizeof name) > 0, 1, "proc_name");
    EXPECT(strstr(name, "darwinmisc") != 0, 1, "proc_name tem o nome do programa");
    pid_t pids[2048];
    int n = proc_listpids(PROC_UID_ONLY, getuid(), pids, sizeof pids) / (int)sizeof(pid_t), found = 0;
    for (int i = 0; i < n; i++) if (pids[i] == getpid()) found = 1;
    EXPECT(n > 0 && found, 1, "proc_listpids PROC_UID_ONLY tem este processo");
    n = proc_listpidspath(PROC_ALL_PIDS, 0, tmpl, 0, pids, sizeof pids);
    EXPECT(n > 0, 1, "proc_listpidspath acha quem abriu o arquivo");
    EXPECT(n > 0 && pids[0] == getpid(), 1, "e é este processo");
    close(fd);
    unlink(tmpl);

    printf("sinais\n");
    int st = signal_child(1);
    EXPECT(WIFEXITED(st) && WEXITSTATUS(st) == 0, 1, "com tratador o filho sai normalmente");
    st = signal_child(0);
    EXPECT(WIFSIGNALED(st) && WTERMSIG(st) == SIGSYS, 1, "com SIG_DFL o filho morre por SIGSYS");

    printf("__thread\n");
    EXPECT(tv, 123, "valor inicial na thread principal");
    tv = 321;
    int seen = -1;
    pthread_t t;
    pthread_create(&t, 0, tls_thread, &seen);
    pthread_join(t, 0);
    EXPECT(seen, 123, "a outra thread vê o valor inicial");
    EXPECT(tv, 321, "a thread principal não mudou");

    printf("execl de si mesmo\n");
    pid_t c = fork();
    if (c == 0) { execl(argv[0], argv[0], "--eco", "Hello world", (char *)0); _exit(1); }
    waitpid(c, &st, 0);
    EXPECT(WIFEXITED(st) && WEXITSTATUS(st) == 42, 1, "o filho rodou a si mesmo com o argumento");

    printf("darwinmisc: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
