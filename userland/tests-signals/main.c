/* mOS — teste de sinais POSIX num programa Mach-O (libSystem + kernel/proc/process.c, SYS_SIG):
 * tratadores de SIGUSR1/SIGALRM/SIGSEGV/SIGBUS/SIGFPE que retomam, SA_SIGINFO com siginfo/ucontext do Darwin,
 * sigprocmask + sigpending, sigsuspend, sigaltstack, SA_RESETHAND, EINTR e SA_RESTART num read bloqueante,
 * SIGCHLD ao filho morrer, pthread_kill e o status de um filho morto por sinal.
 * Compilado por build.ps1 -Tests como /Sistema/Programas/sinaisteste; o "teste" nativo o chama no fim. */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/ucontext.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("  ok  " __VA_ARGS__); else { fails++; printf("  FALHOU  " __VA_ARGS__); } printf("\n"); } while (0)

static volatile sig_atomic_t got_usr1, got_alrm, got_chld, got_usr2, chld_pid, info_ok;
static volatile void *segv_addr;
static volatile int segv_code, alt_ok;
static char altstack[64 << 10];

static void on_usr1(int s) { got_usr1 = s; }
static void on_alrm(int s) { got_alrm++; (void)s; }
static void on_usr2(int s) { got_usr2++; (void)s; }
static void on_chld(int s, siginfo_t *si, void *uc) { (void)s; (void)uc; got_chld++; chld_pid = si->si_pid; }

/* SIGSEGV: anota o endereço e pula a instrução que falhou (mov %eax, (%rdx): 2 bytes) pelo mcontext */
static void on_segv(int s, siginfo_t *si, void *ucv) {
    ucontext_t *uc = ucv;
    segv_addr = si->si_addr;
    segv_code = si->si_code;
    char here;
    alt_ok = (char *)&here >= altstack && (char *)&here < altstack + sizeof altstack;
    info_ok = s == SIGSEGV && si->si_signo == SIGSEGV && uc->uc_mcontext->__es.__faultvaddr == (unsigned long)si->si_addr;
    uc->uc_mcontext->__ss.__rip += 2;
}
static void __attribute__((noinline)) poke(volatile int *p) { __asm__ volatile("movl %%eax, (%%rdx)" :: "d"(p), "a"(7) : "memory"); }

static sigjmp_buf jb;
static void on_fpe(int s) { (void)s; siglongjmp(jb, 1); }
static volatile int zero;

static void *thread_wait_usr2(void *arg) {
    (void)arg;
    sigset_t m; sigemptyset(&m); sigaddset(&m, SIGUSR2);
    int sig = 0;
    sigwait(&m, &sig);
    return (void *)(long)sig;
}

int main(void) {
    /* 1. SIGUSR1 por raise: tratador roda antes do raise voltar */
    signal(SIGUSR1, on_usr1);
    raise(SIGUSR1);
    CHECK(got_usr1 == SIGUSR1, "SIGUSR1 entregue ao tratador (raise), número do macOS %d", (int)got_usr1);

    /* 2. kill(getpid()) também */
    got_usr1 = 0;
    kill(getpid(), SIGUSR1);
    for (int i = 0; i < 100 && !got_usr1; i++) usleep(1000);
    CHECK(got_usr1 == SIGUSR1, "SIGUSR1 por kill(getpid())");

    /* 3. máscara: bloqueado fica pendente e sai no desbloqueio */
    sigset_t m, old, pend;
    sigemptyset(&m); sigaddset(&m, SIGUSR1);
    got_usr1 = 0;
    sigprocmask(SIG_BLOCK, &m, &old);
    raise(SIGUSR1);
    sigpending(&pend);
    CHECK(!got_usr1 && sigismember(&pend, SIGUSR1) == 1, "SIGUSR1 bloqueado fica pendente (sigpending)");
    sigprocmask(SIG_SETMASK, &old, 0);
    CHECK(got_usr1 == SIGUSR1, "desbloquear entrega o pendente");

    /* 4. SIGALRM (alarm) acorda um read bloqueante com EINTR (sigaction sem SA_RESTART) */
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_alrm;
    sigaction(SIGALRM, &sa, 0);
    int p[2];
    pipe(p);
    char c;
    alarm(1);
    errno = 0;
    ssize_t n = read(p[0], &c, 1);
    int e = errno;
    CHECK(n == -1 && e == EINTR && got_alrm == 1, "SIGALRM interrompe read bloqueante: EINTR (n %ld, errno %d, alarmes %d)", (long)n, e, (int)got_alrm);

    /* 5. SA_RESTART: o read é refeito e recebe o byte que o filho manda depois do sinal */
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, 0);
    pid_t w = fork();
    if (w == 0) { usleep(1500000); write(p[1], "x", 1); _exit(0); }
    alarm(1);
    n = read(p[0], &c, 1);
    CHECK(n == 1 && c == 'x' && got_alrm == 2, "SA_RESTART refaz o read interrompido (n %ld, alarmes %d)", (long)n, (int)got_alrm);
    waitpid(w, 0, 0);

    /* 6. SIGSEGV com SA_SIGINFO numa sigaltstack; o tratador ajusta o rip e o programa retoma */
    stack_t ss = { .ss_sp = altstack, .ss_size = sizeof altstack, .ss_flags = 0 };
    CHECK(sigaltstack(&ss, 0) == 0, "sigaltstack");
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_segv;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &sa, 0);
    volatile int *bad = (volatile int *)0x100000;
    poke(bad);
    CHECK(segv_addr == (void *)bad && segv_code == SEGV_MAPERR && info_ok, "SIGSEGV entregue com si_addr %p, si_code %d e __es.__faultvaddr", (void *)segv_addr, segv_code);
    CHECK(alt_ok, "tratador rodou na sigaltstack");
    poke(bad);
    CHECK(1, "retomou depois de duas faltas seguidas");

    /* 7. SIGFPE (divisão por zero) com siglongjmp para fora do tratador */
    signal(SIGFPE, on_fpe);
    int jumped = 0;
    if (sigsetjmp(jb, 1) == 0) { volatile int x = 10 / zero; (void)x; }
    else jumped = 1;
    CHECK(jumped, "SIGFPE e siglongjmp saindo do tratador");

    /* 8. SA_RESETHAND: a segunda vez vale a ação padrão (aqui SIG_IGN para não morrer: testa só a troca) */
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_usr2;
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGUSR2, &sa, 0);
    raise(SIGUSR2);
    struct sigaction now;
    sigaction(SIGUSR2, 0, &now);
    CHECK(got_usr2 == 1 && now.sa_handler == SIG_DFL, "SA_RESETHAND volta ao padrão depois da entrega");

    /* 9. SIGCHLD com siginfo ao filho terminar; sigsuspend espera por ele */
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_chld;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGCHLD, &sa, 0);
    sigemptyset(&m); sigaddset(&m, SIGCHLD);
    sigprocmask(SIG_BLOCK, &m, &old);
    pid_t k = fork();
    if (k == 0) { usleep(100000); _exit(3); }
    sigset_t waitmask = old;
    sigdelset(&waitmask, SIGCHLD);
    int r = sigsuspend(&waitmask);
    e = errno;
    sigprocmask(SIG_SETMASK, &old, 0);
    CHECK(r == -1 && e == EINTR && got_chld == 1 && chld_pid == k, "SIGCHLD (si_pid %d) acorda sigsuspend", (int)chld_pid);
    int st = 0;
    waitpid(k, &st, 0);
    CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 3, "o filho é colhido depois do SIGCHLD");

    /* 10. filho morto por SIGTERM: WTERMSIG com o número do macOS */
    signal(SIGCHLD, SIG_DFL);
    pid_t z = fork();
    if (z == 0) { for (;;) pause(); }
    usleep(50000);
    kill(z, SIGUSR1);                                    /* SIGUSR1 sem tratador no filho? herdou on_usr1: não morre */
    usleep(50000);
    kill(z, SIGTERM);
    waitpid(z, &st, 0);
    CHECK(WIFSIGNALED(st) && WTERMSIG(st) == SIGTERM, "filho morto por SIGTERM (WTERMSIG %d)", WIFSIGNALED(st) ? WTERMSIG(st) : -1);

    /* 11. pthread_kill + sigwait numa thread */
    sigemptyset(&m); sigaddset(&m, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &m, 0);                   /* a thread nova herda o bloqueio */
    pthread_t th;
    pthread_create(&th, 0, thread_wait_usr2, 0);
    usleep(50000);
    pthread_kill(th, SIGUSR2);
    void *ret = 0;
    pthread_join(th, &ret);
    CHECK((long)ret == SIGUSR2, "sigwait numa thread recebe o pthread_kill (%ld)", (long)ret);

    printf(fails ? "sinaisteste: %d FALHA(S)\n" : "sinaisteste: TODOS OK\n", fails);
    return fails;
}
