// mOS — teste do runtime do tradutor arm64 (frente RUNTIME, docs/tradutor-arm64.md): um Mach-O arm64 com threads,
// sinais (ucontext arm64 do Darwin: leitura e alteração do mcontext, falta de dados, brk, SIGUSR1 entre threads,
// máscara, sigaltstack), TLS (thread_local) e exceções C++. Compilado com zig c++ -target aarch64-macos.
// Imprime "ok <nome>" por verificação e "a64rt-teste: N falhas" no fim; código de saída = falhas.
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <sys/ucontext.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
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

static volatile int bus_ok, bus_load, bus_far;
static volatile void *bus_addr;
static void h_bus(int s, siginfo_t *si, void *ucv) {
    ucontext_t *uc = (ucontext_t *)ucv;
    unsigned esr = uc->uc_mcontext->__es.__esr;
    bus_ok = s == SIGBUS && si->si_code == BUS_ADRALN && (esr >> 26) == 0x24 && (esr & 0x3F) == 0x21;
    bus_load = (esr & (1u << 6)) == 0;
    bus_far = si->si_addr == bus_addr && (void *)(uintptr_t)uc->uc_mcontext->__es.__far == bus_addr;
    uc->uc_mcontext->__ss.__pc += 4;
    uc->uc_mcontext->__ss.__x[0] = 0x66;
}

#endif
static volatile int rst_vezes, nd_depth, nd_max, pipe_vezes, chld_ok, alrm_vezes;
static volatile pid_t chld_pid;
static void h_rst(int) { rst_vezes++; }
static void h_nd(int) {
    int d = ++nd_depth;
    if (d > nd_max) nd_max = d;
    if (d == 1) raise(SIGURG);
    nd_depth--;
}
static void h_pipe(int s) { if (s == SIGPIPE) pipe_vezes++; }
static void h_alrm(int s) { if (s == SIGALRM) alrm_vezes++; }
static void h_chld(int s, siginfo_t *si, void *) {
    chld_ok = s == SIGCHLD && si->si_code == CLD_EXITED;
    chld_pid = si->si_pid;
}
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
    sigpending(&pend);
    CHECK("pthread_kill não deixa o sinal pendente na thread principal", !sigismember(&pend, SIGUSR1));
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

    // LDAR de 8 bytes que cruza uma fronteira de 16: SIGBUS, não SIGSEGV; o tratador pula a instrução
    alignas(16) unsigned char cruza[32];
    memset(cruza, 0xAB, sizeof cruza);
    void *meio = cruza + 12;
    bus_addr = meio;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = h_bus; sa.sa_flags = SA_SIGINFO;
    sigaction(SIGBUS, &sa, 0);
    long xbus = 0;
    __asm__ volatile("mov x1, %1\n ldar x0, [x1]\n mov %0, x0" : "=r"(xbus) : "r"(meio) : "x0", "x1", "memory");
    CHECK("ldar que cruza 16 bytes é SIGBUS ADRALN e o tratador grava x0", bus_ok && bus_load && bus_far && xbus == 0x66);

#endif
    // SA_RESETHAND, SA_NODEFER, SIGPIPE, SIGCHLD, EINTR e SA_RESTART
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = h_rst; sa.sa_flags = SA_RESETHAND;
    sigaction(SIGWINCH, &sa, 0);
    raise(SIGWINCH);
    struct sigaction agora;
    sigaction(SIGWINCH, 0, &agora);
    CHECK("SA_RESETHAND volta o tratador para SIG_DFL", rst_vezes == 1 && agora.sa_handler == SIG_DFL);

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = h_nd; sa.sa_flags = SA_NODEFER;
    sigaction(SIGURG, &sa, 0);
    raise(SIGURG);
    CHECK("SA_NODEFER entrega o sinal de novo dentro do tratador", nd_max == 2);

    int fds[2];
    CHECK("pipe para SIGPIPE", pipe(fds) == 0);
    close(fds[0]);
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = h_pipe;
    sigaction(SIGPIPE, &sa, 0);
    errno = 0;
    ssize_t nw = write(fds[1], "z", 1);
    CHECK("SIGPIPE no write e errno EPIPE", nw == -1 && errno == EPIPE && pipe_vezes == 1);
    close(fds[1]);

    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = h_chld; sa.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD, &sa, 0);
    pid_t filho = fork();
    if (filho == 0) _exit(0);
    for (int i = 0; i < 100 && !chld_ok; i++) usleep(10000);
    CHECK("SIGCHLD com si_pid do filho", filho > 0 && chld_ok && chld_pid == filho);
    int st = 0;
    if (filho > 0) waitpid(filho, &st, 0);

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = h_alrm;
    sigaction(SIGALRM, &sa, 0);
    int pr[2];
    CHECK("pipe para o read bloqueante", pipe(pr) == 0);
    char byte = 0;
    alarm(1);
    errno = 0;
    ssize_t nr = read(pr[0], &byte, 1);
    int er = errno;
    CHECK("SIGALRM interrompe read com EINTR", nr == -1 && er == EINTR && alrm_vezes == 1);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, 0);
    pid_t escreve = fork();
    if (escreve == 0) { usleep(1500000); write(pr[1], "x", 1); _exit(0); }
    alarm(1);
    nr = read(pr[0], &byte, 1);
    CHECK("SA_RESTART refaz o read e entrega o byte", nr == 1 && byte == 'x' && alrm_vezes == 2);
    if (escreve > 0) waitpid(escreve, 0, 0);
    close(pr[0]); close(pr[1]);
    // sigaltstack + SA_ONSTACK. Darwin arm64 (e o kernel) exigem 32 KiB.
    stack_t pequena; pequena.ss_sp = altpilha; pequena.ss_size = 8192; pequena.ss_flags = 0;
    errno = 0;
    int alt_rc = sigaltstack(&pequena, 0);
    CHECK("sigaltstack menor que 32 KiB é ENOMEM", alt_rc == -1 && errno == ENOMEM);
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

// ---- página de 16 KiB vista pelo processo arm64 (docs/pagesize-arm64.md)
// O kernel continua em 4 KiB. Este teste só roda no mOS (zig c++ -target aarch64-macos + machotest/VM).
#ifdef __aarch64__
extern "C" unsigned long long vm_page_size, vm_kernel_page_size;
extern "C" int vm_page_shift, vm_kernel_page_shift;
extern "C" int host_page_size(unsigned, unsigned long long *);
extern "C" int mach_vm_deallocate(unsigned, unsigned long long, unsigned long long);

static volatile char *volatile pag_alvo;
static volatile int pag_falta;
static void h_pagina(int s, siginfo_t *si, void *ucv) {
    ucontext_t *uc = (ucontext_t *)ucv;
    pag_falta = s == SIGSEGV && si && si->si_addr == (void *)pag_alvo;
    if (uc && uc->uc_mcontext) uc->uc_mcontext->__ss.__pc += 4;
}
/* 1 se o load faltou e a instrução foi pulada. Instala o tratador só em volta do load. */
static int load_falta(volatile char *p) {
    struct sigaction sa, old;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = h_pagina;
    sa.sa_flags = SA_SIGINFO;
    pag_alvo = p;
    pag_falta = 0;
    if (sigaction(SIGSEGV, &sa, &old) != 0) return 0;
    unsigned char b = 0xA5;
    __asm__ volatile("ldrb %w0, [%1]" : "+r"(b) : "r"(p) : "memory");
    int falta = pag_falta && b == 0xA5;
    sigaction(SIGSEGV, &old, 0);
    return falta;
}
struct pag_pilha { int base_ok; int falta; size_t sz; uintptr_t top; };
static void *olha_pilha(void *arg) {
    pag_pilha *i = (pag_pilha *)arg;
    char *top = (char *)pthread_get_stackaddr_np(pthread_self());
    size_t sz = pthread_get_stacksize_np(pthread_self());
    i->top = (uintptr_t)top;
    i->sz = sz;
    char *base = top - sz;
    base[0] = 1;
    i->base_ok = base[0] == 1;
    i->falta = load_falta(base - 1);
    return 0;
}
static void teste_pagina(void) {
    unsigned long long hostpg = 0;
    CHECK("vm_page_size e vm_kernel_page_size = 16 KiB, shift 14",
          vm_page_size == 16384 && vm_kernel_page_size == 16384 &&
          vm_page_shift == 14 && vm_kernel_page_shift == 14);
    CHECK("getpagesize e sysconf(_SC_PAGESIZE) = 16 KiB",
          getpagesize() == 16384 && sysconf(29) == 16384 &&
          sysconf(_SC_PAGESIZE) == 16384 && _SC_PAGESIZE == 29);
    CHECK("host_page_size = 16 KiB", host_page_size(0, &hostpg) == 0 && hostpg == 16384);

    unsigned long long pg = 0; size_t n = sizeof pg;
    CHECK("sysctl hw.pagesize é u64 16384",
          sysctlbyname("hw.pagesize", &pg, &n, 0, 0) == 0 && n == 8 && pg == 16384);
    int p32 = 0; n = sizeof p32;
    CHECK("sysctl hw.pagesize32 é int 16384",
          sysctlbyname("hw.pagesize32", &p32, &n, 0, 0) == 0 && n == 4 && p32 == 16384);
    int vp = 0; n = sizeof vp;
    CHECK("sysctl vm.pagesize é int 16384",
          sysctlbyname("vm.pagesize", &vp, &n, 0, 0) == 0 && n == 4 && vp == 16384);
    int mib[2] = {6, 7}, mb = 0; n = sizeof mb;
    CHECK("sysctl MIB {6,7} (hw.pagesize) = 16384",
          sysctl(mib, 2, &mb, &n, 0, 0) == 0 && n == 4 && mb == 16384);

    void *p = mmap(0, 100000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    size_t arred = (100000u + 16383u) & ~16383u;   /* 114688 */
    int alinhado = p != MAP_FAILED && ((uintptr_t)p & 16383) == 0;
    if (alinhado) {
        ((volatile char *)p)[0] = 1;
        ((volatile char *)p)[99999] = 2;
        ((volatile char *)p)[arred - 1] = 3;
    }
    CHECK("mmap(100000) alinha a 16 KiB e o pedido cabe na faixa",
          alinhado && ((volatile char *)p)[99999] == 2 && ((volatile char *)p)[arred - 1] == 3);
    if (alinhado) {
        CHECK("o byte depois da página guest falta", load_falta((volatile char *)p + arred));
        errno = 0;
        void *ruim = mmap((char *)p + 4096, 16384, PROT_READ | PROT_WRITE,
                          MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
        CHECK("MAP_FIXED desalinhado é EINVAL", ruim == MAP_FAILED && errno == EINVAL);

        void *meio = mmap((char *)p + 16384, 1, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
        CHECK("MAP_FIXED de 1 byte troca a página guest do meio",
              meio == (char *)p + 16384 && ((volatile char *)p)[0] == 1 &&
              load_falta((volatile char *)p + 16384) && ((volatile char *)p)[32768] == 0);

        errno = 0;
        int pr = mprotect((char *)p + 32768, 1, PROT_NONE);
        int vizinho = ((volatile char *)p)[49152] == 0;
        CHECK("mprotect parcial PROT_NONE cobre 16 KiB e poupa o vizinho",
              pr == 0 && load_falta((volatile char *)p + 32768) && vizinho);
        errno = 0;
        CHECK("mprotect no meio da página guest é EINVAL",
              mprotect((char *)p + 32768 + 4096, 4096, PROT_READ | PROT_WRITE) == -1 && errno == EINVAL &&
              load_falta((volatile char *)p + 32768));
        CHECK("mprotect de volta a RW religa a página",
              mprotect((char *)p + 32768, 1, PROT_READ | PROT_WRITE) == 0 &&
              (((volatile char *)p)[32768] = 4) == 4);

        /* a página em +16384 continua solta pelo MAP_FIXED; o vizinho em +49152 segue mapeado */
        CHECK("munmap de 100 bytes solta a página guest",
              munmap(p, 100) == 0 && load_falta((volatile char *)p) && ((volatile char *)p)[49152] == 0);
        errno = 0;
        CHECK("munmap desalinhado é EINVAL", munmap((char *)p + 8, 16384) == -1 && errno == EINVAL);
        CHECK("munmap de comprimento 0 é sucesso", munmap(p, 0) == 0);
        munmap((char *)p + 16384, arred - 16384);
    }

    void *g = mmap(0, 65536, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (g != MAP_FAILED) ((volatile char *)g)[16384] = 9;
    CHECK("mach_vm_deallocate no meio da página solta só essa página guest",
          g != MAP_FAILED && ((volatile char *)g)[16384] == 9 &&
          mach_vm_deallocate(0, (unsigned long long)(uintptr_t)g + 4096, 16) == 0 &&
          load_falta((volatile char *)g) && ((volatile char *)g)[16384] == 9);
    if (g != MAP_FAILED) munmap(g, 65536);

    pthread_attr_t at;
    size_t gs = 0;
    CHECK("guarda padrão da pthread é 16 KiB",
          pthread_attr_init(&at) == 0 && pthread_attr_getguardsize(&at, &gs) == 0 && gs == 16384);
    CHECK("setguardsize(100) sobe a 16 KiB",
          pthread_attr_setguardsize(&at, 100) == 0 && pthread_attr_getguardsize(&at, &gs) == 0 && gs == 16384);
    CHECK("setguardsize(0) tira a guarda",
          pthread_attr_setguardsize(&at, 0) == 0 && pthread_attr_getguardsize(&at, &gs) == 0 && gs == 0);
    CHECK("setguardsize(NULL) é EINVAL", pthread_attr_setguardsize(0, 16384) == 22);
    CHECK("getguardsize(NULL) é EINVAL", pthread_attr_getguardsize(&at, 0) == 22);

    pthread_attr_setguardsize(&at, 16384);
    pthread_attr_setstacksize(&at, 64 << 10);
    pag_pilha info = {0, 0, 0, 0};
    pthread_t th;
    int cr = pthread_create(&th, &at, olha_pilha, &info);
    if (!cr) pthread_join(th, 0);
    CHECK("pilha da thread: base usável e a página de guarda abaixo falta",
          cr == 0 && info.base_ok && info.falta && info.sz == (64u << 10) && (info.top & 16383) == 0 &&
          (info.sz & 16383) == 0);
    pthread_attr_destroy(&at);
}
#else
static void teste_pagina(void) { printf("pula pagina (não é arm64)\n"); }
#endif
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

/* svc #0x80 do Darwin, sem o libSystem: 59 = execve, 244 = posix_spawn.
 * O args_desc é o do XNU (LP64): 32 bytes iniciais e file action de 1040 bytes
 * (tipo +0, fd +4, dup2 novo fd +8, chdir path +8). O contêiner é {alloc, count, ações}. */
#ifdef __aarch64__
static long svc80(long nr, long a0, long a1, long a2, long a3, long a4, int *carry) {
    register long r0 __asm__("x0") = a0;
    register long r1 __asm__("x1") = a1;
    register long r2 __asm__("x2") = a2;
    register long r3 __asm__("x3") = a3;
    register long r4 __asm__("x4") = a4;
    register long r16 __asm__("x16") = nr;
    unsigned long nzcv = 0;
    __asm__ volatile("svc #0x80\n\tmrs %1, nzcv"
                     : "+r"(r0), "=r"(nzcv)
                     : "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r16)
                     : "memory");
    if (carry) *carry = (int)((nzcv >> 29) & 1);
    return r0;
}
struct fa_box { int alloc, count; unsigned char act[1040]; };
struct spawn_desc { unsigned long long attr_size, attrp, fa_size, fap; };
struct spawn_attr { unsigned short flags, pad; };
static void fa_set(fa_box *b, int type, int fd, int extra, const char *path) {
    memset(b, 0, sizeof *b);
    b->alloc = 1;
    b->count = 1;
    memcpy(b->act, &type, 4);
    memcpy(b->act + 4, &fd, 4);
    if (type == 2) memcpy(b->act + 8, &extra, 4);
    if (path) memcpy(b->act + 8, path, strlen(path) + 1);
}
#endif
extern "C" int mach_vm_map(unsigned task, unsigned long long *addr, unsigned long long size, unsigned long long mask,
                           int flags, unsigned object, unsigned long long offset, int copy, int cur, int max, unsigned inherit);
extern "C" int mach_make_memory_entry_64(unsigned task, unsigned long long *size, unsigned long long offset,
                                         int perm, unsigned *handle, unsigned parent);
static void teste_svc(void) {
#ifdef __aarch64__
    pid_t pid = fork();
    if (pid == 0) {
        char *av[] = {argv_fork, (char *)"--filho-exec", 0};
        int carry = 0;
        svc80(59, (long)argv_fork, (long)av, (long)environ, 0, 0, &carry);
        _exit(127);
    }
    CHECK("svc 59 execve executa o filho e devolve o status", pid > 0 && filho_ok(pid, 42));

    pid = fork();
    if (pid == 0) {
        char *av[] = {argv_fork, (char *)"--filho-exec", 0};
        int carry = 0;
        long r = svc80(59, (long)"/nao/existe-a64", (long)av, 0, 0, 0, &carry);
        _exit(carry && r == ENOENT ? 7 : 8);
    }
    CHECK("svc 59 de caminho ausente volta com ENOENT", pid > 0 && filho_ok(pid, 7));

    int spid = -1, carry = 0;
    char *av[] = {argv_fork, (char *)"--filho-spawn", 0};
    long r = svc80(244, (long)&spid, (long)argv_fork, 0, (long)av, (long)environ, &carry);
    CHECK("svc 244 posix_spawn executa o filho e devolve o status", carry == 0 && r == 0 && spid > 0 && filho_ok(spid, 43));

    int fds[2];
    int pp = pipe(fds);
    fa_box dup;
    fa_set(&dup, 2, pp == 0 ? fds[1] : -1, 1, 0);
    spawn_desc desc;
    memset(&desc, 0, sizeof desc);
    desc.fa_size = 8 + 1040;
    desc.fap = (unsigned long long)&dup;
    char *ioav[] = {argv_fork, (char *)"--filho-io", 0};
    spid = -1; carry = 0;
    r = pp == 0 ? svc80(244, (long)&spid, (long)argv_fork, (long)&desc, (long)ioav, (long)environ, &carry) : -1;
    char z = 0;
    int n = (r == 0 && spid > 0 && filho_ok(spid, 0)) ? (int)read(fds[0], &z, 1) : -1;
    if (pp == 0) { close(fds[0]); close(fds[1]); }
    CHECK("svc 244 com file action dup2: o filho escreve no pipe", pp == 0 && carry == 0 && r == 0 && n == 1 && z == 'Z');

    mkdir("/tmp/a64cwd", 0755);
    fa_box ch;
    fa_set(&ch, 5, 0, 0, "/tmp/a64cwd");
    memset(&desc, 0, sizeof desc);
    desc.fa_size = 8 + 1040;
    desc.fap = (unsigned long long)&ch;
    char *cdav[] = {argv_fork, (char *)"--filho-cwd", 0};
    spid = -1; carry = 0;
    r = svc80(244, (long)&spid, (long)argv_fork, (long)&desc, (long)cdav, (long)environ, &carry);
    CHECK("svc 244 com file action chdir", carry == 0 && r == 0 && spid > 0 && filho_ok(spid, 0));

    int cfd = open("/tmp/a64-cloexec.bin", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (cfd >= 0) { if (write(cfd, "Q", 1) == 1) lseek(cfd, 0, SEEK_SET); }
    spawn_attr attr;
    attr.flags = 0x4000; /* POSIX_SPAWN_CLOEXEC_DEFAULT */
    attr.pad = 0;
    memset(&desc, 0, sizeof desc);
    desc.attr_size = 4;
    desc.attrp = (unsigned long long)&attr;
    char num[16];
    snprintf(num, sizeof num, "%d", cfd);
    char *fdav[] = {argv_fork, (char *)"--filho-fd", num, 0};
    spid = -1; carry = 0;
    r = cfd >= 0 ? svc80(244, (long)&spid, (long)argv_fork, (long)&desc, (long)fdav, (long)environ, &carry) : -1;
    int fechou = cfd >= 0 && r == 0 && spid > 0 && filho_ok(spid, 0);
    if (cfd >= 0) close(cfd);
    CHECK("svc 244 com POSIX_SPAWN_CLOEXEC_DEFAULT não entrega o fd do pai", fechou && carry == 0);
#else
    printf("pula svc (sem aarch64)\n");
#endif
}

static void teste_fixo(void) {
    const long page = 16384;
    char img[32768];
    memset(img, 'A', page);
    memset(img + page, 'B', page);
    int fd = open("/tmp/a64-fixed.bin", O_RDWR | O_CREAT | O_TRUNC, 0644);
    int wr = fd >= 0 ? (int)write(fd, img, (size_t)page * 2) : -1;
    void *slot = mmap(0, (size_t)page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    void *p = (fd >= 0 && slot != MAP_FAILED)
                  ? mmap(slot, (size_t)page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, fd, page)
                  : MAP_FAILED;
    int conteudo = p == slot && p != MAP_FAILED && ((char *)p)[0] == 'B';
    CHECK("mmap MAP_FIXED de arquivo devolve o endereço e o conteúdo", fd >= 0 && wr == page * 2 && conteudo);

    unsigned long long sz = (unsigned long long)page;
    unsigned handle = 0;
    int kr = conteudo ? mach_make_memory_entry_64(0, &sz, (unsigned long long)p, PROT_READ | PROT_WRITE, &handle, 0) : -1;
    void *hole = mmap(0, (size_t)page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (hole != MAP_FAILED) munmap(hole, (size_t)page);
    unsigned long long at = hole != MAP_FAILED ? (unsigned long long)hole : 0;
    int kr2 = (kr == 0 && handle && at) ? mach_vm_map(0, &at, (unsigned long long)page, 0, 0, handle, 0, 0, 3, 3, 0) : -1;
    int viu = kr2 == 0 && at == (unsigned long long)hole && ((char *)at)[0] == 'B';
    CHECK("mach_vm_map fixo devolve o endereço e o conteúdo do arquivo", viu);
    unsigned long long busy = (unsigned long long)p;
    int kr3 = viu ? mach_vm_map(0, &busy, (unsigned long long)page, 0, 0, handle, 0, 0, 3, 3, 0) : -1;
    CHECK("mach_vm_map sem OVERWRITE em faixa ocupada é KERN_NO_SPACE", kr3 == 3 && ((char *)p)[0] == 'B');
    if (fd >= 0) close(fd);
    if (slot != MAP_FAILED) munmap(slot, (size_t)page * 2);
    if (viu) munmap((void *)at, (size_t)page);
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--filho-exec")) return 42;
    if (argc > 1 && !strcmp(argv[1], "--filho-spawn")) return 43;
    if (argc > 1 && !strcmp(argv[1], "--filho-io")) { char z = 'Z'; return write(1, &z, 1) == 1 ? 0 : 2; }
    if (argc > 1 && !strcmp(argv[1], "--filho-cwd")) {
        char b[256];
        if (!getcwd(b, sizeof b)) return 2;
        return strcmp(b, "/tmp/a64cwd") == 0 ? 0 : 1;
    }
    if (argc > 1 && !strcmp(argv[1], "--filho-fd")) {
        int fd = argc > 2 ? atoi(argv[2]) : -1;
        char b;
        return read(fd, &b, 1) < 0 ? 0 : 1;
    }
    argv_fork = argv[0];
#ifdef __aarch64__
    {   // svc #0x4d4f com x8 = A64RT_SYS_INFO (userland/a64rt/a64rt_abi.h): 0 interpretador, 1 JIT
        register long x0 __asm__("x0") = 1, x8 __asm__("x8") = 0x4D03;
        __asm__ volatile("svc #0x4d4f" : "+r"(x0) : "r"(x8) : "memory");
        motor = x0 == 1 ? "[jit] " : x0 == 0 ? "[interp] " : "[?] ";
    }
#endif
    printf("%sa64rt-teste: %s (argc %d)\n", motor, argv[0], argc);
    const char *so = argc > 1 ? argv[1] : 0;   // um teste só: threads, mutex, sinais, cxx, sistema, pagina, smc, fork, spawn, svc, fixo
#define RODA(n, f) if (!so || !strcmp(so, n)) f()
    RODA("threads", teste_threads);
    RODA("mutex", teste_mutex);
    RODA("sinais", teste_sinais);
    RODA("sistema", teste_sistema);
    RODA("pagina", teste_pagina);
    RODA("smc", teste_smc);
    RODA("fork", teste_fork);
    RODA("spawn", teste_spawn);
    RODA("svc", teste_svc);
    RODA("fixo", teste_fixo);
    RODA("cxx", teste_cxx);                     // por último: sem unwinder no libSystem vira std::terminate
    printf("%sa64rt-teste: %d falhas\n", motor, falhas);
    return falhas;
}
