/* mOS — teste POSIX de um programa Mach-O (libSystem): fork/execvp/waitpid, dup2 + pipe, symlink,
 * lstat/stat e "#!". É a base do bash e do Homebrew (docs/porte-brew.md).
 * Compilado por build.ps1 -Tests como /Sistema/Programas/posixteste; o "teste" nativo o chama no fim. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("  ok  " __VA_ARGS__); else { fails++; printf("  FALHOU  " __VA_ARGS__); } printf("\n"); } while (0)

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

    printf(fails ? "posixteste: %d FALHA(S)\n" : "posixteste: TODOS OS TESTES OK\n", fails);
    return fails ? 1 : 0;
}
