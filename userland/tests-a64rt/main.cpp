// mOS — teste do runtime do tradutor arm64 (frente RUNTIME, docs/tradutor-arm64.md): um Mach-O arm64 com threads,
// sinais (ucontext arm64 do Darwin: leitura e alteração do mcontext, falta de dados, brk, SIGUSR1 entre threads,
// máscara, sigaltstack), TLS (thread_local) e exceções C++. Compilado com zig c++ -target aarch64-macos.
// Imprime "ok <nome>" por verificação e "a64rt-teste: N falhas" no fim; código de saída = falhas.
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <sys/ucontext.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <spawn.h>
#include <setjmp.h>
#include <stdexcept>
#include <string>
#include <vector>

static int falhas;
static const char *motor = "";   // [interp]/[jit]: nome do motor do tradutor nas linhas (vários processos no mesmo log)
#define CHECK(nome, cond) do { if (cond) printf("%sok   %s\n", motor, nome); else { printf("%sFALHA %s (linha %d)\n", motor, nome, __LINE__); falhas++; } fflush(stdout); } while (0)

// ---- threads + thread_local
static thread_local long tls_val = 5;
static void *trabalho(void *arg) {
    long id = (long)arg, s = 0;
    tls_val = 1000 + id;
    for (long i = 1; i <= 100000; i++) s += i % (id + 3);
    if (tls_val != 1000 + id) return (void *)-1;
    return (void *)s;
}
static long esperado(long id) { long s = 0; for (long i = 1; i <= 100000; i++) s += i % (id + 3); return s; }

static void teste_threads(void) {
    pthread_t t[6];
    for (long i = 0; i < 6; i++) pthread_create(&t[i], 0, trabalho, (void *)i);
    int ok = 1;
    for (long i = 0; i < 6; i++) { void *r; pthread_join(t[i], &r); if ((long)r != esperado(i)) ok = 0; }
    CHECK("threads (6, join e resultado)", ok);
    CHECK("thread_local da thread principal intacto", tls_val == 5);
}

// ---- mutex/cond entre threads
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cnd = PTHREAD_COND_INITIALIZER;
static long contador;
static void *somador(void *) {
    for (int i = 0; i < 20000; i++) { pthread_mutex_lock(&mtx); contador++; pthread_mutex_unlock(&mtx); }
    pthread_mutex_lock(&mtx); pthread_cond_signal(&cnd); pthread_mutex_unlock(&mtx);
    return 0;
}
static void teste_mutex(void) {
    pthread_t t[4];
    for (int i = 0; i < 4; i++) pthread_create(&t[i], 0, somador, 0);
    for (int i = 0; i < 4; i++) pthread_join(t[i], 0);
    CHECK("mutex entre 4 threads (80000)", contador == 80000);
}

// ---- sinais
static volatile sig_atomic_t usr1_vezes, usr1_sinal;
static volatile pthread_t usr1_thread;
static void h_usr1(int s) { usr1_vezes++; usr1_sinal = s; usr1_thread = pthread_self(); }

#ifdef __aarch64__   // os testes de falta/brk/udf/ucontext são do arm64; no x86_64 (comparação) ficam de fora
static volatile int segv_ok, segv_far_ok;
static volatile char *volatile ptr_ruim = (volatile char *)0x10;
static void h_segv(int s, siginfo_t *si, void *ucv) {
    ucontext_t *uc = (ucontext_t *)ucv;
    segv_ok = s == SIGSEGV;
    segv_far_ok = si->si_addr == (void *)0x10 && uc->uc_mcontext->__es.__far == 0x10 &&
                  ((uc->uc_mcontext->__es.__esr >> 26) == 0x24);
    uc->uc_mcontext->__ss.__pc += 4;            // pula o load que falhou
    uc->uc_mcontext->__ss.__x[0] = 0x55;        // e muda um registrador visível depois
}

static volatile int trap_ok, trap_imm;
static void h_trap(int s, siginfo_t *, void *ucv) {
    ucontext_t *uc = (ucontext_t *)ucv;
    unsigned esr = uc->uc_mcontext->__es.__esr;
    trap_ok = s == SIGTRAP && (esr >> 26) == 0x3C;
    trap_imm = esr & 0xFFFF;
    uc->uc_mcontext->__ss.__pc += 4;
}

static volatile int ill_ok;
static void h_ill(int s, siginfo_t *, void *ucv) {
    ucontext_t *uc = (ucontext_t *)ucv;
    ill_ok = s == SIGILL;
    uc->uc_mcontext->__ss.__pc += 4;
}

#endif
static volatile int alt_ok;
static char altpilha[65536];
static void h_alt(int) {
    char marca;
    alt_ok = &marca >= altpilha && &marca < altpilha + sizeof altpilha;
}

static void *recebe_usr1(void *) {
    sigset_t m; sigemptyset(&m); sigaddset(&m, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, &m, 0);
    for (int i = 0; i < 2000 && usr1_vezes < 3; i++) usleep(1000);   // 2 já entregues antes
    return 0;
}

static void teste_sinais(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = h_usr1;
    sigaction(SIGUSR1, &sa, 0);
    raise(SIGUSR1);
    CHECK("raise(SIGUSR1) chama o tratador", usr1_vezes == 1 && usr1_sinal == SIGUSR1);

    // bloqueado: fica pendente até desbloquear
    sigset_t m, old; sigemptyset(&m); sigaddset(&m, SIGUSR1);
    sigprocmask(SIG_BLOCK, &m, &old);
    raise(SIGUSR1);
    sigset_t pend; sigpending(&pend);
    CHECK("sinal bloqueado fica pendente", usr1_vezes == 1 && sigismember(&pend, SIGUSR1));
    sigprocmask(SIG_SETMASK, &old, 0);
    CHECK("desbloquear entrega o pendente", usr1_vezes == 2);

    // pthread_kill para outra thread (a principal bloqueia, a outra desbloqueia)
    sigprocmask(SIG_BLOCK, &m, &old);
    pthread_t t;
    pthread_create(&t, 0, recebe_usr1, 0);
    usleep(20000);
    pthread_kill(t, SIGUSR1);
    pthread_join(t, 0);
    sigprocmask(SIG_SETMASK, &old, 0);
    CHECK("pthread_kill entrega na outra thread", usr1_vezes == 3);

#ifdef __aarch64__
    // falta de dados: far/esr no mcontext, pc alterado pelo tratador, x0 alterado
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = h_segv; sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, 0);
    long x0;
    __asm__ volatile("mov x0, %1\n ldrb w0, [x0]\n mov %0, x0" : "=r"(x0) : "r"(ptr_ruim) : "x0", "memory");
    CHECK("SIGSEGV entregue com si_addr/far/esr do arm64", segv_ok && segv_far_ok);
    CHECK("tratador muda pc e x0 no mcontext", x0 == 0x55);

    // brk #0x77 → SIGTRAP com ESR.EC = 0x3C e o imediato
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = h_trap; sa.sa_flags = SA_SIGINFO;
    sigaction(SIGTRAP, &sa, 0);
    __asm__ volatile("brk #0x77");
    CHECK("brk vira SIGTRAP com o imediato no ESR", trap_ok && trap_imm == 0x77);

    // udf → SIGILL
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = h_ill; sa.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &sa, 0);
    __asm__ volatile(".inst 0x00000000");
    CHECK("udf vira SIGILL", ill_ok);

#endif
    // sigaltstack + SA_ONSTACK
    stack_t ss; ss.ss_sp = altpilha; ss.ss_size = sizeof altpilha; ss.ss_flags = 0;
    sigaltstack(&ss, 0);
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = h_alt; sa.sa_flags = SA_ONSTACK;
    sigaction(SIGUSR2, &sa, 0);
    raise(SIGUSR2);
    CHECK("SA_ONSTACK roda na sigaltstack", alt_ok);
    signal(SIGSEGV, SIG_DFL); signal(SIGTRAP, SIG_DFL); signal(SIGILL, SIG_DFL);   // falhas depois daqui não são engolidas
}

// ---- exceções C++
struct Erro : std::runtime_error { int cod; Erro(int c) : std::runtime_error("erro " + std::to_string(c)), cod(c) {} };
static int __attribute__((noinline)) profundo(int n) {
    std::vector<std::string> lixo(3, "abc");
    if (n == 0) throw Erro(42);
    return profundo(n - 1) + (int)lixo.size();
}
static void *lanca_na_thread(void *) {
    try { profundo(5); } catch (const Erro &e) { return (void *)(long)e.cod; }
    return 0;
}
static void teste_cxx(void) {
    int cod = 0; std::string msg;
    try { profundo(10); } catch (const Erro &e) { cod = e.cod; msg = e.what(); }
    CHECK("throw/catch atravessando 10 quadros", cod == 42 && msg == "erro 42");
    int pegou = 0;
    try { throw 7; } catch (int v) { pegou = v; }
    CHECK("throw de int", pegou == 7);
    pthread_t t; void *r;
    pthread_create(&t, 0, lanca_na_thread, 0);
    pthread_join(t, &r);
    CHECK("exceção dentro de uma thread", (long)r == 42);
}

// ---- identidade da máquina
static void teste_sistema(void) {
    struct utsname u; uname(&u);
    CHECK("uname -m = arm64", !strcmp(u.machine, "arm64"));
    int v = 0; size_t n = sizeof v;
    CHECK("hw.optional.arm64 = 1", sysctlbyname("hw.optional.arm64", &v, &n, 0, 0) == 0 && v == 1);
    v = 0; n = sizeof v;
    CHECK("hw.optional.arm.FEAT_LSE = 1", sysctlbyname("hw.optional.arm.FEAT_LSE", &v, &n, 0, 0) == 0 && v == 1);
    int ct = 0; n = sizeof ct;
    CHECK("hw.cputype = arm64", sysctlbyname("hw.cputype", &ct, &n, 0, 0) == 0 && ct == 0x0100000C);
}

// ---- código gerado em tempo de execução (MAP_JIT): com sys_icache_invalidate e sem (rede de segurança do JIT)
#include <sys/mman.h>
#include <libkern/OSCacheControl.h>
static void teste_smc(void) {
#ifdef __aarch64__
    unsigned *p = (unsigned *)mmap(0, 16384, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
    if (p == (unsigned *)MAP_FAILED) { CHECK("mmap MAP_JIT", 0); return; }
    typedef int (*fn_t)(void);
    fn_t f = (fn_t)p;
    int soma = 0;
    for (int i = 1; i <= 50; i++) {                    // reescreve o mesmo código 50 vezes, com invalidação
        p[0] = 0x52800000u | ((unsigned)i << 5);       // mov w0, #i
        p[1] = 0xd65f03c0u;                            // ret
        sys_icache_invalidate(p, 8);
        soma += f();
    }
    CHECK("código reescrito com sys_icache_invalidate", soma == 50 * 51 / 2);
    int ok = 1;
    for (int i = 1; i <= 20; i++) {                    // sem invalidação: só a rede de segurança do JIT (ou o interpretador)
        for (int k = 0; k < 3; k++) if (f() != (k ? i : i - 1 ? i - 1 : 50)) {}
        p[0] = 0x52800000u | ((unsigned)(100 + i) << 5);
        if (f() != 100 + i) ok = 0;
    }
    CHECK("código reescrito sem invalidar (proteção de página)", ok);
    munmap(p, 16384);
#endif
}

// ---- fork / vfork / posix_spawn / exec a partir do processo traduzido
extern char **environ;
static char *argv_fork;
static int ordem_atfork[8], n_ordem;
static int atfork_chi, atfork_par;
static void atfork_parent(void) { atfork_par++; }
static void atfork_child(void) { atfork_chi++; }
static void marca_atfork(int n) { if (n_ordem < 8) ordem_atfork[n_ordem++] = n; }
static void pre1(void) { marca_atfork(1); }
static void pre2(void) { marca_atfork(2); }
static int filho_ok(pid_t pid, int want) {
    int st = 0;
    if (pid <= 0 || waitpid(pid, &st, 0) != pid) return 0;
    return WIFEXITED(st) && WEXITSTATUS(st) == want;
}
static volatile int pulsos_fork, gira_fork = 1;
static void *pulsa_fork(void *) {
    while (gira_fork) {
        void *p = malloc(64);
        if (p) free(p);
        pulsos_fork++;
        usleep(1000);
    }
    return 0;
}
static int quente(int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += i * 3 + 1;
    return s;
}
static void teste_fork(void) {
    pid_t pid = fork();
    if (pid == 0) _exit(getpid() > 0 ? 0 : 1);
    CHECK("fork devolve no pai e o filho sai 0", pid > 0 && pid != getpid() && filho_ok(pid, 0));

    tls_val = 77;
    volatile int pilha = 41;
    volatile int global_local = 41;
    pid = fork();
    if (pid == 0) {
        int ok = tls_val == 77 && pilha == 41 && global_local == 41;
        tls_val = 1;
        pilha = 2;
        _exit(ok ? 0 : 1);
    }
    int herdou = pid > 0 && filho_ok(pid, 0);
    CHECK("filho herda TLS e pilha; a escrita dele não volta", herdou && tls_val == 77 && pilha == 41);

    int esp = quente(1000);
    for (int i = 0; i < 2000; i++) if (quente(1000) != esp) esp = -1;
    pid = fork();
    if (pid == 0) _exit(quente(1000) == esp ? 0 : 1);
    CHECK("fork depois do código aquecido (JIT ou interpretador)", esp > 0 && pid > 0 && filho_ok(pid, 0));

#ifdef __aarch64__
    unsigned long long celula __attribute__((aligned(16))) = 0x1111, lida = 0;
    __asm__ volatile("ldxr %0, [%1]" : "=r"(lida) : "r"(&celula) : "memory");
    pid = fork();
    if (pid == 0) {
        unsigned st = 99;
        __asm__ volatile("stxr %w0, %1, [%2]" : "=&r"(st) : "r"(lida), "r"(&celula) : "memory");
        _exit(st == 1 ? 0 : 1);
    }
    unsigned st_pai = 99;
    if (pid > 0) __asm__ volatile("stxr %w0, %1, [%2]" : "=&r"(st_pai) : "r"(lida), "r"(&celula) : "memory");
    CHECK("monitor exclusivo limpo no pai e no filho", pid > 0 && st_pai == 1 && filho_ok(pid, 0) && celula == 0x1111);
#endif

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = h_usr1;
    sigaction(SIGUSR1, &sa, 0);
    int usr_antes = usr1_vezes;
    pid = fork();
    if (pid == 0) {
        usr1_vezes = 0;
        raise(SIGUSR1);
        _exit(usr1_vezes == 1 ? 0 : 1);
    }
    CHECK("filho herda o tratador de sinal", pid > 0 && filho_ok(pid, 0) && usr1_vezes == usr_antes);

    stack_t ss;
    ss.ss_sp = altpilha;
    ss.ss_size = sizeof altpilha;
    ss.ss_flags = 0;
    int alt = sigaltstack(&ss, 0);
    pid = alt == 0 ? fork() : -1;
    if (pid == 0) {
        stack_t cur;
        memset(&cur, 0, sizeof cur);
        sigaltstack(0, &cur);
        _exit(cur.ss_sp == altpilha && cur.ss_size == sizeof altpilha ? 0 : 1);
    }
    stack_t off;
    memset(&off, 0, sizeof off);
    off.ss_flags = SS_DISABLE;
    sigaltstack(&off, 0);
    CHECK("filho herda sigaltstack", alt == 0 && pid > 0 && filho_ok(pid, 0));

    pthread_atfork(pre1, 0, 0);
    pthread_atfork(pre2, 0, 0);
    n_ordem = 0;
    atfork_chi = atfork_par = 0;
    pthread_atfork(0, atfork_parent, atfork_child);
    pid = fork();
    if (pid == 0) _exit(atfork_chi == 1 && atfork_par == 0 ? 0 : 1);
    CHECK("pthread_atfork: prepare LIFO, parent no pai, child no filho",
          pid > 0 && n_ordem >= 2 && ordem_atfork[0] == 2 && ordem_atfork[1] == 1 && atfork_par == 1 && filho_ok(pid, 0));

    int pre_v = n_ordem, par_v = atfork_par;
    pid = vfork();
    if (pid == 0) _exit(0);
    CHECK("vfork devolve e não dispara pthread_atfork", pid > 0 && filho_ok(pid, 0) && n_ordem == pre_v && atfork_par == par_v);

    pthread_t outra;
    gira_fork = 1;
    pulsos_fork = 0;
    int tc = pthread_create(&outra, 0, pulsa_fork, 0);
    for (int i = 0; tc == 0 && i < 1000 && pulsos_fork < 2; i++) usleep(1000);
    pid = tc == 0 ? fork() : -1;
    if (pid == 0) {
        int a = pulsos_fork;
        usleep(20000);
        void *b = malloc(128);
        int e = pthread_kill(outra, 0);
        pthread_t nova;
        int rc = pthread_create(&nova, 0, trabalho, (void *)3);
        void *r = 0;
        if (rc == 0) pthread_join(nova, &r);
        int ok = b && e != 0 && pulsos_fork == a && rc == 0 && (long)r == esperado(3);
        _exit(ok ? 0 : 1);
    }
    gira_fork = 0;
    if (tc == 0) pthread_join(outra, 0);
    CHECK("fork com outra thread: filho fica só com quem chamou e cria thread nova", tc == 0 && pid > 0 && filho_ok(pid, 0));

#ifdef __aarch64__
    unsigned *pag = (unsigned *)mmap(0, 16384, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
    if (pag == (unsigned *)MAP_FAILED) CHECK("mmap para o fork com código reescrito", 0);
    else {
        pag[0] = 0x52800000u | (7u << 5);   // mov w0, #7
        pag[1] = 0xd65f03c0u;               // ret
        int (*fn)(void) = (int (*)(void))pag;
        int warm = fn() == 7;
        for (int i = 0; i < 500 && warm; i++) if (fn() != 7) warm = 0;
        pid = warm ? fork() : -1;
        if (pid == 0) {
            pag[0] = 0x52800000u | (9u << 5);   // sem sys_icache_invalidate: a proteção da página tem de pegar
            _exit(fn() == 9 ? 0 : 1);
        }
        int vivo = pid > 0 && filho_ok(pid, 0);
        int pai_antigo = fn() == 7;
        pag[0] = 0x52800000u | (4u << 5);
        int pai_novo = fn() == 4;
        munmap(pag, 16384);
        CHECK("fork com código aquecido: o filho não vaza a escrita e os dois lados vêem o código novo",
              warm && vivo && pai_antigo && pai_novo);
    }
#endif

    pid = fork();
    if (pid == 0) {
        char *av[] = {argv_fork, (char *)"--filho-exec", 0};
        execv(argv_fork, av);
        _exit(127);
    }
    CHECK("exec de binário arm64 a partir de processo arm64", pid > 0 && filho_ok(pid, 42));

    char *av[] = {argv_fork, (char *)"--filho-spawn", 0};
    int sp = posix_spawn(&pid, argv_fork, 0, 0, av, environ);
    CHECK("posix_spawn de filho arm64", sp == 0 && filho_ok(pid, 43));

    if (access("/Sistema/Programas/a64teste/hello-x86", 0) == 0) {
        pid = fork();
        if (pid == 0) {
            char *xv[] = {(char *)"/Sistema/Programas/a64teste/hello-x86", 0};
            execv(xv[0], xv);
            _exit(127);
        }
        CHECK("exec de binário x86_64 a partir de processo arm64", pid > 0 && filho_ok(pid, 7));
    } else printf("pula exec x86_64 (sem hello-x86)\n");
}

// ---- posix_spawn de outro Mach-O (A64RT_TESTE_SPAWN="caminho:código esperado;..."): arm64 → x86_64 e arm64 → arm64
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
static void teste_spawn(void) {
    const char *lista = getenv("A64RT_TESTE_SPAWN");
    if (!lista) { printf("pula spawn (sem A64RT_TESTE_SPAWN)\n"); return; }
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", lista);
    for (char *it = strtok(buf, ";"); it; it = strtok(0, ";")) {
        char *dp = strrchr(it, ':');
        if (!dp) continue;
        *dp = 0;
        int want = atoi(dp + 1);
        pid_t pid;
        char *av[] = {it, 0};
        int r = posix_spawn(&pid, it, 0, 0, av, environ);
        int st = 0;
        if (!r) waitpid(pid, &st, 0);
        char nome[300];
        snprintf(nome, sizeof nome, "posix_spawn %s sai com %d", it, want);
        CHECK(nome, r == 0 && WIFEXITED(st) && WEXITSTATUS(st) == want);
    }
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--filho-exec")) return 42;
    if (argc > 1 && !strcmp(argv[1], "--filho-spawn")) return 43;
    argv_fork = argv[0];
#ifdef __aarch64__
    {   // svc #0x4d4f com x8 = A64RT_SYS_INFO (userland/a64rt/a64rt_abi.h): 0 interpretador, 1 JIT
        register long x0 __asm__("x0") = 1, x8 __asm__("x8") = 0x4D03;
        __asm__ volatile("svc #0x4d4f" : "+r"(x0) : "r"(x8) : "memory");
        motor = x0 == 1 ? "[jit] " : x0 == 0 ? "[interp] " : "[?] ";
    }
#endif
    printf("%sa64rt-teste: %s (argc %d)\n", motor, argv[0], argc);
    const char *so = argc > 1 ? argv[1] : 0;   // um teste só: threads, mutex, sinais, cxx, sistema, smc, fork, spawn
#define RODA(n, f) if (!so || !strcmp(so, n)) f()
    RODA("threads", teste_threads);
    RODA("mutex", teste_mutex);
    RODA("sinais", teste_sinais);
    RODA("sistema", teste_sistema);
    RODA("smc", teste_smc);
    RODA("fork", teste_fork);
    RODA("spawn", teste_spawn);
    RODA("cxx", teste_cxx);                     // por último: sem unwinder no libSystem vira std::terminate
    printf("%sa64rt-teste: %d falhas\n", motor, falhas);
    return falhas;
}
