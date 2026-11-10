/*
 * mOS libSystem — complementos: libm estendida, pthreads de um processo com uma thread,
 * memmem, dirname/basename, realpath, strptime, pathconf.
 */
#include "sys.h"
#include "libc.h"
#if defined(__aarch64__)
#include "a64page.h"
#endif

double sqrt(double), exp(double), log(double), pow(double, double), fabs(double), floor(double);
double sin(double), cos(double), atan(double), frexp(double, int *), ldexp(double, int), trunc(double);
double fmod(double, double), round(double), rint(double);

/* ================================================================ libm */
double __exp10(double x) { return pow(10.0, x); }
double exp10(double x) { return pow(10.0, x); }
double acosh(double x) { return log(x + sqrt(x * x - 1)); }
double asinh(double x) { return x < 0 ? -log(-x + sqrt(x * x + 1)) : log(x + sqrt(x * x + 1)); }
double atanh(double x) { return 0.5 * log((1 + x) / (1 - x)); }
double fdim(double a, double b) { return a > b ? a - b : 0; }
double fma(double a, double b, double c) { return a * b + c; }
double copysign(double a, double b) { u64 x, y; memcpy(&x, &a, 8); memcpy(&y, &b, 8); x = (x & ~(1ull << 63)) | (y & (1ull << 63)); memcpy(&a, &x, 8); return a; }
double logb(double x) { int e; if (x == 0) return -__builtin_inf(); frexp(x, &e); return e - 1; }
int ilogb(double x) { return (int)logb(x); }
double scalbn(double x, int n) { return ldexp(x, n); }
double scalbln(double x, long n) { return ldexp(x, (int)n); }
double scalb(double x, double n) { return ldexp(x, (int)n); }
double remainder(double a, double b) { double q = rint(a / b); return a - q * b; }
double drem(double a, double b) { return remainder(a, b); }
double significand(double x) { int e; return frexp(x, &e) * 2; }
double nextafter(double x, double y) {
    if (x != x || y != y) return x + y;
    if (x == y) return y;
    if (x == 0) { u64 b = 1; double r; memcpy(&r, &b, 8); return y > 0 ? r : -r; }
    u64 b;
    memcpy(&b, &x, 8);
    if ((y > x) == (x > 0)) b++; else b--;
    memcpy(&x, &b, 8);
    return x;
}
double nexttoward(double x, long double y) { return nextafter(x, (double)y); }
double log2(double);

/* erf/erfc: aproximação de Abramowitz & Stegun 7.1.26 refinada (erro < 1.5e-7) */
double erf(double x) {
    double s = x < 0 ? -1 : 1;
    x = fabs(x);
    double t = 1 / (1 + 0.3275911 * x);
    double y = 1 - (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t * exp(-x * x);
    return s * y;
}
double erfc(double x) { return 1 - erf(x); }

/* gamma de Lanczos (g = 7) */
static const double lanczos[9] = {0.99999999999980993, 676.5203681218851, -1259.1392167224028, 771.32342877765313,
                                  -176.61502916214059, 12.507343278686905, -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7};
double tgamma(double x) {
    if (x < 0.5) return 3.14159265358979323846 / (sin(3.14159265358979323846 * x) * tgamma(1 - x));
    x -= 1;
    double a = lanczos[0], t = x + 7.5;
    for (int i = 1; i < 9; i++) a += lanczos[i] / (x + i);
    return 2.5066282746310002 * pow(t, x + 0.5) * exp(-t) * a;
}
int signgam;
double lgamma_r(double x, int *sign) {
    if (x < 0.5) {
        double s = sin(3.14159265358979323846 * x);
        *sign = s < 0 ? -1 : 1;
        return log(3.14159265358979323846 / fabs(s)) - lgamma_r(1 - x, sign);
    }
    x -= 1;
    double a = lanczos[0], t = x + 7.5;
    for (int i = 1; i < 9; i++) a += lanczos[i] / (x + i);
    *sign = 1;
    return 0.91893853320467274178 + (x + 0.5) * log(t) - t + log(a);
}
double lgamma(double x) { return lgamma_r(x, &signgam); }

/* Bessel: série de potências para |x| < 12, assintótica acima */
static double bessel_j(int n, double x) {
    if (fabs(x) < 12) {
        double term = 1, sum = 0, hx = x / 2;
        for (int k = 1; k <= n; k++) term *= hx / k;
        for (int k = 0; k < 60; k++) {
            sum += term;
            term *= -(hx * hx) / ((k + 1.0) * (k + 1.0 + n));
        }
        return sum;
    }
    double w = x - (2 * n + 1) * 3.14159265358979323846 / 4;
    return sqrt(2 / (3.14159265358979323846 * x)) * cos(w);
}
double j0(double x) { return bessel_j(0, x); }
double j1(double x) { return bessel_j(1, x); }
double jn(int n, double x) { return n < 0 ? ((n & 1) ? -bessel_j(-n, x) : bessel_j(-n, x)) : bessel_j(n, x); }
static double bessel_y(int n, double x) {
    if (x <= 0) return -__builtin_inf();
    /* Y_n por integração numérica de Simpson da forma integral (precisão ~1e-6) */
    const double PI = 3.14159265358979323846;
    int N = 400;
    double h = PI / N, s1 = 0;
    for (int i = 0; i <= N; i++) {
        double th = i * h, f = sin(x * sin(th) - n * th);
        s1 += (i == 0 || i == N) ? f : (i & 1) ? 4 * f : 2 * f;
    }
    s1 *= h / 3 / PI;
    double s2 = 0, T = 20.0 / (x > 1 ? 1 : x) + 20;
    int M = 2000;
    double ht = T / M;
    for (int i = 0; i <= M; i++) {
        double t = i * ht, f = (exp(n * t) + ((n & 1) ? -1 : 1) * exp(-n * t)) * exp(-x * (exp(t) - exp(-t)) / 2);
        s2 += (i == 0 || i == M) ? f : (i & 1) ? 4 * f : 2 * f;
    }
    s2 *= ht / 3 / PI;
    return s1 - s2;
}
double y0(double x) { return bessel_y(0, x); }
double y1(double x) { return bessel_y(1, x); }
double yn(int n, double x) { return bessel_y(n, x); }

/* ================================================================ pthreads (1 thread) */
typedef unsigned long pthread_key_t;
/* Dados específicos de thread no formato do macOS: cada thread tem uma tabela de 512 entradas
 * apontada pelo GS (gs:[chave*8]); o Chromium lê essa tabela direto, sem chamar getspecific. As
 * chaves de pthread_key_create vão de KEY_FIRST a 511 (as de baixo são slots fixos do sistema;
 * slot 0 = a própria thread). Sem FSGSBASE na CPU, cai numa tabela global (uma thread). */
#define TSD_SLOTS 512
#define KEY_FIRST 128
static void *tsd_global[TSD_SLOTS];
static int key_used[TSD_SLOTS];
static int gs_ok = -1;
void *pthread_self(void);
#if defined(__aarch64__)
/* arm64: o TSD fica no TPIDRRO_EL0, como no macOS arm64 (o código da Apple lê com mrs e faz & ~7); quem o grava é o
 * runtime do tradutor (chamada privada A64RT_SET_TSD, x8 = 0x4d00). Sem o runtime (harness), a tabela global. */
#define A64RT_SET_TSD 0x4d00
static int cpu_has_fsgsbase(void) { return 1; }
static inline u64 rd_tsd_base(void) { u64 v; __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(v)); return v & ~7ULL; }
static inline int wr_tsd_base(u64 v) { SYS1(A64RT_SET_TSD, v); return rd_tsd_base() == v; }
#else
static int cpu_has_fsgsbase(void) {
    u32 a = 7, b = 0, c = 0, d = 0;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    return b & 1;
}
static inline u64 rd_tsd_base(void) { u64 v; __asm__ volatile("rdgsbase %0" : "=r"(v)); return v; }
static inline int wr_tsd_base(u64 v) { __asm__ volatile("wrgsbase %0" :: "r"(v) : "memory"); return 1; }
#endif
/* pthread_t no formato do macOS (struct _pthread do x86_64): cada thread tem o seu bloco, o TSD começa em +0xe0 (768 slots:
 * 512 externos + 256 internos), o GS aponta para o TSD e tsd[0] é o próprio bloco, que é o pthread_self(). Código compilado
 * para o macOS lê dados da thread a partir do pthread_self() (o mimalloc do Bun no Claude Code guarda o heap da thread em
 * pthread_self()+0x300, o tsd[68]). Antes o pthread_self() era main_thread + tid, endereços a um byte uns dos outros: a
 * escrita de uma thread aparecia deslocada na leitura de outra, e o Bun caía num ponteiro de heap pela metade. */
#define PT_SIG 0x54485244L                      /* "THRD", o _PTHREAD_SIG do macOS */
#define PT_TSD_OFS 0xe0
#define PT_TSD_N 768
typedef struct {
    long sig;                                   /* 0x00 */
    void *cleanup;                              /* 0x08: __cleanup_stack */
    long tid;                                   /* 0x10: o tid do kernel (mOS) */
    char pad[PT_TSD_OFS - 0x18];
    void *tsd[PT_TSD_N];                        /* 0xe0 */
} cx_pthread_t;
_Static_assert(__builtin_offsetof(cx_pthread_t, tsd) == PT_TSD_OFS, "tsd em +0xe0, como no macOS");
/* tabela da thread atual (cria o bloco e liga ao GS na primeira vez) */
void **cx_tsd(void) {
    if (gs_ok < 0) gs_ok = cpu_has_fsgsbase();
    if (!gs_ok) return tsd_global;
    u64 base;
    base = rd_tsd_base();
    if (!base) {
        cx_pthread_t *b = calloc(1, sizeof *b);
        if (!b) return tsd_global;
        b->sig = PT_SIG;
        b->tid = SYS0(SYS_GETTID);
        b->tsd[0] = b;
        base = (u64)b->tsd;
        if (!wr_tsd_base(base)) { free(b); gs_ok = 0; return tsd_global; }
    }
    return (void **)base;
}
static void (*key_dtor[TSD_SLOTS])(void *);
int pthread_key_create(pthread_key_t *k, void (*dtor)(void *)) {
    for (int i = KEY_FIRST; i < TSD_SLOTS; i++)
        if (!__sync_lock_test_and_set(&key_used[i], 1)) { key_dtor[i] = dtor; cx_tsd()[i] = 0; *k = (pthread_key_t)i; return 0; }
    return 35; /* EAGAIN */
}
int pthread_key_delete(pthread_key_t k) { if (k < TSD_SLOTS) { key_dtor[k] = 0; key_used[k] = 0; } return 0; }
/* fim de uma thread (cx_thread_entry, pthread_exit): os destrutores das chaves que têm valor, como o libpthread do macOS,
 * em até 4 voltas (PTHREAD_DESTRUCTOR_ITERATIONS: um destrutor pode gravar outra chave). O mimalloc abandona aqui o heap
 * da thread que acaba (sem isso as páginas dela ficavam presas a uma thread morta); o TLS do Chromium e o
 * __thread_struct da libc++ também liberam o que é da thread. Sem FSGSBASE a tabela é global e nada roda. */
void cx_thread_run_dtors(void) {
    void **tsd = cx_tsd();
    if (tsd == tsd_global) return;
    for (int round = 0; round < 4; round++) {
        int any = 0;
        for (int i = KEY_FIRST; i < TSD_SLOTS; i++) {
            void (*d)(void *) = key_dtor[i];
            void *v = tsd[i];
            if (!d || !v || !key_used[i]) continue;
            tsd[i] = 0;
            d(v);
            any = 1;
        }
        if (!any) break;
    }
}
void *pthread_getspecific(pthread_key_t k) { return k < TSD_SLOTS ? cx_tsd()[k] : 0; }
int pthread_setspecific(pthread_key_t k, const void *v) { if (k >= TSD_SLOTS) return CX_EINVAL; cx_tsd()[k] = (void *)v; return 0; }
typedef struct { long sig; char opaque[8]; } pthread_once_t;
/* int em +8 (o opaque zerado de PTHREAD_ONCE_INIT): 0 = nunca rodou, 1 = rodando, 2 = pronto. Quem chega com 1 dorme
 * até o 2: antes marcava "pronto" antes de chamar fn, e uma segunda thread (em outro núcleo, desde o SMP) voltava na
 * hora e usava o estado ainda não iniciado. */
int cx_futex_wait(volatile int *w, int expected, long ms);
int cx_futex_wake(volatile int *w, int n);
int pthread_once(pthread_once_t *o, void (*fn)(void)) {
    volatile int *st = (volatile int *)o->opaque;
    if (__atomic_load_n(st, __ATOMIC_ACQUIRE) == 2) return 0;
    int z = 0;
    if (__atomic_compare_exchange_n(st, &z, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        fn();
        __atomic_store_n(st, 2, __ATOMIC_RELEASE);
        cx_futex_wake(st, 0x7FFFFFFF);
        return 0;
    }
    while (__atomic_load_n(st, __ATOMIC_ACQUIRE) != 2) cx_futex_wait(st, 1, -1);
    return 0;
}
/* threads de verdade: SYS_THREAD_CREATE com pilha própria (mmap) */
typedef struct { void *(*fn)(void *); void *arg; u64 stack_top, stack_size; } cx_tstart_t;
/* pilha de cada thread criada aqui, pela mesma chave do pthread_self (tid & 0xFFF): pthread_get_stackaddr_np devolvia o
 * topo da pilha da thread principal para todas, e o V8 de um Web Worker do renderer do VS Code via a própria pilha
 * acima do "início" (CHECK numa verificação de pilha, int3). A própria thread registra antes de rodar o código dela. */
static struct { u64 top, size; } thread_stacks[4096];
static char main_thread[8192];                  /* pthread_self() = main_thread + (tid & 0xFFF) */
static __attribute__((noreturn)) void cx_thread_entry(cx_tstart_t *ts) {
    void *(*fn)(void *) = ts->fn;
    void *arg = ts->arg;
    long me = SYS0(SYS_GETTID);
    thread_stacks[me & 0xFFF].top = ts->stack_top;
    thread_stacks[me & 0xFFF].size = ts->stack_size;
    free(ts);
    cx_tsd();                                   /* tabela TSD própria desta thread (GS) */
    void *r = fn(arg);
    cx_thread_run_dtors();                      /* destrutores das chaves (pthread_key_create), como no macOS */
    SYS1(SYS_THREAD_EXIT, (long)r);
    for (;;) {}
}
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void *a, size_t len);
int mprotect(void *a, size_t len, int prot);
size_t cx_attr_getguard(const void *a);
/* pthread_attr_t do macOS: {long sig; char opaque[56]}. Guardamos o tamanho de pilha pedido em opaque[0..8)
 * (sig = ATTR_SIG); sem attr, ou attr de antes, 512 KiB como sempre. O Codex CLI (Rust) pede pilhas grandes para a
 * thread principal do tokio e estourava os 512 KiB. Limite de 64 MiB (o mmap do kernel aloca na hora). */
#define ATTR_SIG 0x5448524541445341L
#define DEF_STACK (512 << 10)
static size_t attr_stack(const void *a) {
    if (!a || ((const long *)a)[0] != ATTR_SIG) return DEF_STACK;
    size_t s = ((const size_t *)a)[1];
    return s < (64 << 10) ? DEF_STACK : s > (64UL << 20) ? (64UL << 20) : s;
}
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg) {
    size_t sz = (attr_stack(attr) + 0xFFF) & ~(size_t)0xFFF;
    size_t guard = 0;
#if defined(__aarch64__)
    /* página de guarda abaixo da pilha, do tamanho anunciado (16 KiB por padrão). PROT_NONE
     * desmapeia essa página na arena de mmap: o estouro gera falta de verdade. */
    guard = cx_attr_getguard(attr);
    sz = (size_t)a64_guard_round(sz ? sz : A64_GUEST_PAGE);
#endif
    char *stack = mmap(0, sz + guard, 3, 0x1002, -1, 0);
    if (stack == (char *)-1) { cx_fdprintf(2, "[thread] pthread_create: sem pilha de %lu KiB\n", (unsigned long)(sz >> 10)); return 35; }
    if (guard && mprotect(stack, guard, 0) != 0) { munmap(stack, sz + guard); return 35; }
    cx_tstart_t *ts = malloc(sizeof *ts);
    if (!ts) { munmap(stack, sz + guard); return 35; }
    ts->fn = fn;
    ts->arg = arg;
    ts->stack_top = (u64)stack + guard + sz;
    ts->stack_size = sz;                        /* a guarda não entra no tamanho, como no macOS */
    /* a guarda fica no começo da faixa: o sp nasce no topo da parte usável, não dentro dela */
    u64 top = ((u64)stack + guard + sz) & ~15ULL;
#if !defined(__aarch64__)   /* arm64: sp alinhado a 16, sem endereço de volta na pilha (lr = 0) */
    top -= 8;                                   /* rsp = 16n - 8, como logo após um call */
    *(u64 *)top = 0;
#endif
    long tid = SYS3(SYS_THREAD_CREATE, cx_thread_entry, top, ts);
    if (tid < 0) { munmap(stack, sz + guard); free(ts); cx_fdprintf(2, "[thread] pthread_create: o kernel recusou a thread (%ld)\n", tid); return 35; }
    { char *getenv(const char *); if (getenv("CX_TRACE_THREADS")) cx_fdprintf(2, "[thread] %ld: entrada %p, arg %p\n", tid, (void *)fn, arg); }   /* diagnóstico (porte do VS Code): CX_TRACE_THREADS=1 */
    *t = (unsigned long)tid;
    return 0;
}
int pthread_join(unsigned long t, void **ret) {
    long r = SYS1(SYS_THREAD_JOIN, t);
    if (ret) *ret = (void *)r;
    return 0;
}
int pthread_detach(unsigned long t) { (void)t; return 0; }
/* o bloco da thread (tsd[0]); sem FSGSBASE, como antes: main_thread + (tid & 0xFFF) */
void *pthread_self(void) {
    void **t = cx_tsd();
    if (t != tsd_global) return t[0];
    long tid = SYS0(SYS_GETTID);
    return tid > 1 ? (void *)(main_thread + (tid & 0xFFF)) : main_thread;
}
/* tid do kernel de um pthread_t: o bloco do pthread_self, o tid que o pthread_create devolve, ou main_thread + tid */
long cx_pthread_tid(void *t) {
    u64 v = (u64)t;
    if (!t) return SYS0(SYS_GETTID);
    if (v < 0x100000ULL) return (long)v;
    if ((char *)t >= main_thread && (char *)t < main_thread + sizeof main_thread) return (long)((char *)t - main_thread);
    const cx_pthread_t *b = t;
    return b->sig == PT_SIG ? b->tid : SYS0(SYS_GETTID);
}
int pthread_equal(void *a, void *b) { return a == b; }
/* thread principal = a que rodou cx_darwin_init (cx_main_tid, dispatch.c); antes disso, ou em apps que
 * não passam por ele, toda thread responde 1 como antes (feito pela sessão do VS Code, com o ok da DMG) */
extern int cx_main_tid;
int pthread_main_np(void) { return !cx_main_tid || (int)SYS0(SYS_GETTID) == cx_main_tid; }
/* futex do kernel (SYS_FUTEX_WAIT/WAKE, kernel/proc/process.c): a thread que espera fica parada no kernel em vez de
 * girar com SYS_YIELD. ms < 0 = sem prazo. Devolve 0 (acordada), -35 (o valor já tinha mudado), -60 (prazo) ou -4
 * (sinal). O harness também emula estas duas syscalls. Se alguma devolver -EINVAL, cede a CPU e o chamador confere de novo. */
static int futex_ok = 1;
int cx_futex_wait(volatile int *w, int expected, long ms) {
    if (futex_ok) {
        long r = SYS3(SYS_FUTEX_WAIT, w, expected, ms < 0 ? ~0UL : (unsigned long)ms);
        if (r != -22) return (int)r;
        futex_ok = 0;
    }
    SYS0(SYS_YIELD);
    return 0;
}
int cx_futex_wake(volatile int *w, int n) { return futex_ok ? (int)SYS2(SYS_FUTEX_WAKE, w, n) : 0; }

/* pthread_mutex_t do macOS tem 64 bytes ({long sig; char opaque[56]}): usamos o primeiro int do opaque.
 * 0 = livre, 1 = travado, 2 = travado com alguém esperando (o mutex de "Futexes Are Tricky", Drepper): sem disputa,
 * travar e destravar não entram no kernel; com disputa, quem espera dorme no futex até o destravar acordá-lo. */
static volatile int *mx(void *m) { return (volatile int *)((char *)m + 8); }
static void mutex_acquire(volatile int *w) {
    int c = __sync_val_compare_and_swap(w, 0, 1);
    if (!c) return;
    if (c != 2) c = __sync_lock_test_and_set(w, 2);
    while (c) { cx_futex_wait(w, 2, -1); c = __sync_lock_test_and_set(w, 2); }
}
static void mutex_release(volatile int *w) { if (__sync_fetch_and_sub(w, 1) != 1) { *w = 0; cx_futex_wake(w, 1); } }

/* tipos de mutex: PTHREAD_MUTEX_NORMAL 0, ERRORCHECK 1, RECURSIVE 2. O SQLite (no Chromium e no vscode-sqlite3.node)
 * cria os mutexes das conexões como recursivos e entra de novo neles na mesma thread; ignorando o tipo, a thread
 * travava em si mesma (no VS Code, sql::Database::OpenInternal no processo principal, para sempre).
 * pthread_mutexattr_t do macOS tem 16 bytes ({long sig; char opaque[8]}): o tipo fica no int em +8.
 * No pthread_mutex_t: +8 palavra do futex, +12 tipo + 1 (0 = não informado: vale o sig dos inicializadores
 * estáticos), +16 dono (tid), +20 contagem — o mesmo layout do std::recursive_mutex de libcxx.c. */
#define MTX_SIG_ERRORCHECK 0x32AAABA1L      /* PTHREAD_ERRORCHECK_MUTEX_INITIALIZER */
#define MTX_SIG_RECURSIVE  0x32AAABA2L      /* PTHREAD_RECURSIVE_MUTEX_INITIALIZER */
static volatile int *mtype(void *m) { return (volatile int *)((char *)m + 12); }
static volatile int *mowner(void *m) { return (volatile int *)((char *)m + 16); }
static volatile int *mcount(void *m) { return (volatile int *)((char *)m + 20); }
static int mutex_type(void *m) {
    int t = *mtype(m);
    if (t >= 1 && t <= 3) return t - 1;
    long sig = *(volatile long *)m;
    return sig == MTX_SIG_RECURSIVE ? 2 : sig == MTX_SIG_ERRORCHECK ? 1 : 0;
}
int pthread_mutex_init(void *m, const void *a) {
    int t = a ? ((const int *)a)[2] : 0;
    if (t < 0 || t > 2) t = 0;
    *mx(m) = 0; *mowner(m) = 0; *mcount(m) = 0;
    *mtype(m) = t + 1;
    return 0;
}
int pthread_mutex_destroy(void *m) { (void)m; return 0; }
int pthread_mutex_lock(void *m) {
    int t = mutex_type(m);
    if (!t) { mutex_acquire(mx(m)); return 0; }
    int me = (int)SYS0(SYS_GETTID);
    if (*mowner(m) == me) {                  /* só o dono vê o próprio tid aqui */
        if (t == 2) { (*mcount(m))++; return 0; }
        return 11;                           /* EDEADLK */
    }
    mutex_acquire(mx(m));
    *mowner(m) = me;
    *mcount(m) = 1;
    return 0;
}
int pthread_mutex_trylock(void *m) {
    int t = mutex_type(m);
    int me = t ? (int)SYS0(SYS_GETTID) : 0;
    if (t == 2 && *mowner(m) == me) { (*mcount(m))++; return 0; }
    if (__sync_val_compare_and_swap(mx(m), 0, 1)) return 16;   /* EBUSY */
    if (t) { *mowner(m) = me; *mcount(m) = 1; }
    return 0;
}
int pthread_mutex_unlock(void *m) {
    int t = mutex_type(m);
    if (t) {
        if (*mowner(m) != (int)SYS0(SYS_GETTID)) return 1;      /* EPERM */
        if (t == 2 && --*mcount(m) > 0) return 0;
        *mowner(m) = 0;
        *mcount(m) = 0;
    }
    mutex_release(mx(m));
    return 0;
}
int pthread_mutexattr_init(void *a) { ((long *)a)[0] = 0x4D545841L; ((int *)a)[2] = 0; return 0; }   /* _PTHREAD_MUTEX_ATTR_SIG */
int pthread_mutexattr_settype(void *a, int t) { if (t < 0 || t > 2) return 22; ((int *)a)[2] = t; return 0; }
int pthread_mutexattr_gettype(const void *a, int *t) { if (t) *t = ((const int *)a)[2]; return 0; }
int pthread_mutexattr_destroy(void *a) { (void)a; return 0; }
int pthread_cond_init(void *c, const void *a) { (void)a; ((volatile int *)((char *)c + 8))[0] = 0; ((volatile int *)((char *)c + 8))[1] = 0; return 0; }
int pthread_cond_destroy(void *c) { (void)c; return 0; }
/* pthread_cond_t do macOS tem 48 bytes ({long sig; char opaque[40]}): int em +8 = sequência (cada signal/broadcast
 * soma 1; o futex espera ela mudar), int em +12 = quantos esperam (signal sem ninguém esperando não entra no kernel).
 * Quem espera lê a sequência ainda com o mutex travado: um signal depois disso muda o valor e o FUTEX_WAIT volta na
 * hora (EAGAIN) se ainda não tinha dormido. Acordar sem sinal é permitido pelo POSIX; o chamador confere a condição. */
static volatile int *cv(void *c) { return (volatile int *)((char *)c + 8); }
static volatile int *cvw(void *c) { return (volatile int *)((char *)c + 12); }
int pthread_cond_signal(void *c) { __sync_fetch_and_add(cv(c), 1); if (*cvw(c)) cx_futex_wake(cv(c), 1); return 0; }
int pthread_cond_broadcast(void *c) { __sync_fetch_and_add(cv(c), 1); if (*cvw(c)) cx_futex_wake(cv(c), 0x7FFFFFFF); return 0; }
/* espera com prazo: rel_ms < 0 = sem prazo; devolve ETIMEDOUT (60) se o prazo vencer */
static int cond_wait_ms(void *c, void *m, long rel_ms) {
    __sync_fetch_and_add(cvw(c), 1);
    int seq = *cv(c);
    u64 end = (u64)SYS0(SYS_TIME) + (u64)(rel_ms < 0 ? 0 : rel_ms);
    pthread_mutex_unlock(m);
    int r = 0;
    while (*cv(c) == seq) {
        long left = -1;
        if (rel_ms >= 0) {
            u64 now = (u64)SYS0(SYS_TIME);
            if (now >= end) { r = 60; break; }
            left = (long)(end - now);
        }
        cx_futex_wait(cv(c), seq, left);
    }
    __sync_fetch_and_sub(cvw(c), 1);
    pthread_mutex_lock(m);
    return r;
}
int pthread_cond_wait(void *c, void *m) { cond_wait_ms(c, m, -1); return 0; }
/* ts: struct timespec {long sec; long nsec} absoluto (tempo Unix) */
int gettimeofday(void *tv, void *tz);
int pthread_cond_timedwait(void *c, void *m, const void *ts) {
    if (!ts) return pthread_cond_wait(c, m);
    const long *t = ts;
    struct { long sec; int usec; } now;
    gettimeofday(&now, 0);
    long rel = (t[0] - now.sec) * 1000 + (t[1] / 1000000 - now.usec / 1000);
    return cond_wait_ms(c, m, rel < 0 ? 0 : rel);
}
/* ts relativo (macOS: pthread_cond_timedwait_relative_np) */
int pthread_cond_timedwait_relative_np(void *c, void *m, const void *ts) {
    if (!ts) return pthread_cond_wait(c, m);
    const long *t = ts;
    return cond_wait_ms(c, m, t[0] * 1000 + t[1] / 1000000);
}
/* pthread_rwlock_t do macOS tem 200 bytes ({long sig; char opaque[192]}); PTHREAD_RWLOCK_INITIALIZER zera o opaque.
 * int em +8 = estado (0 livre, n > 0 leitores, -1 escritor), int em +12 = quantos dormem no futex do estado.
 * Antes era tudo no-op (época de um núcleo): o V8 (base::SharedMutex) e o Chromium ficavam sem exclusão nenhuma
 * com threads rodando em núcleos diferentes. */
static volatile int *rws(void *l) { return (volatile int *)((char *)l + 8); }
static volatile int *rww(void *l) { return (volatile int *)((char *)l + 12); }
static void rw_sleep(void *l, int seen) {
    __atomic_add_fetch(rww(l), 1, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(rws(l), __ATOMIC_SEQ_CST) == seen) cx_futex_wait(rws(l), seen, -1);
    __atomic_sub_fetch(rww(l), 1, __ATOMIC_SEQ_CST);
}
int pthread_rwlock_tryrdlock(void *l) {
    int s = __atomic_load_n(rws(l), __ATOMIC_RELAXED);
    while (s >= 0)
        if (__atomic_compare_exchange_n(rws(l), &s, s + 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return 0;
    return 16;                                                   /* EBUSY */
}
int pthread_rwlock_trywrlock(void *l) {
    int z = 0;
    return __atomic_compare_exchange_n(rws(l), &z, -1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED) ? 0 : 16;
}
int pthread_rwlock_rdlock(void *l) {
    for (;;) {
        if (!pthread_rwlock_tryrdlock(l)) return 0;
        rw_sleep(l, -1);
    }
}
int pthread_rwlock_wrlock(void *l) {
    for (;;) {
        if (!pthread_rwlock_trywrlock(l)) return 0;
        int s = __atomic_load_n(rws(l), __ATOMIC_RELAXED);
        if (s) rw_sleep(l, s);
    }
}
int pthread_rwlock_unlock(void *l) {
    int s = __atomic_load_n(rws(l), __ATOMIC_RELAXED), n;
    if (s == -1) { __atomic_store_n(rws(l), 0, __ATOMIC_SEQ_CST); n = 0; }
    else if (s > 0) n = __atomic_sub_fetch(rws(l), 1, __ATOMIC_SEQ_CST);
    else return 1;                                               /* EPERM: não estava travado */
    if (!n && __atomic_load_n(rww(l), __ATOMIC_SEQ_CST)) cx_futex_wake(rws(l), 0x7FFFFFFF);
    return 0;
}
int pthread_attr_init(void *a) {
    if (a) {
        ((long *)a)[0] = ATTR_SIG;
        ((size_t *)a)[1] = DEF_STACK;
#if defined(__aarch64__)
        ((size_t *)a)[2] = A64_GUEST_PAGE;   /* guarda padrão: uma página guest */
        ((size_t *)a)[3] = 1;
#endif
    }
    return 0;
}
/* Guarda da pilha. No arm64 o slot [3] == 1 significa que [2] foi pedido (0 = sem guarda).
 * Sem o pedido, a guarda é uma página de 16 KiB. No x86_64 a resposta continua 4096 e o
 * pthread_create não reserva guarda (o comportamento anterior). */
size_t cx_attr_getguard(const void *a) {
#if defined(__aarch64__)
    if (a && ((const long *)a)[0] == ATTR_SIG && ((const size_t *)a)[3] == 1)
        return (size_t)((const size_t *)a)[2];
    return A64_GUEST_PAGE;
#else
    (void)a;
    return 4096;
#endif
}
int cx_attr_setguard(void *a, size_t s) {
#if defined(__aarch64__)
    if (!a) return 22;
    ((long *)a)[0] = ATTR_SIG;
    ((size_t *)a)[2] = (size_t)a64_guard_round(s);
    ((size_t *)a)[3] = 1;
#else
    (void)a;
    (void)s;
#endif
    return 0;
}
int pthread_attr_destroy(void *a) { (void)a; return 0; }
int pthread_attr_setstacksize(void *a, size_t s) {
    if (!a || s < 16384) return 22;                              /* EINVAL, como o macOS abaixo de PTHREAD_STACK_MIN */
    ((long *)a)[0] = ATTR_SIG; ((size_t *)a)[1] = s;
    return 0;
}
/* pilha da thread t (pthread_self() = main_thread + (tid & 0xFFF)): a de uma thread do pthread_create, ou a da thread
 * principal do processo (USER_STACK_TOP, 2 MiB: kernel/include/proc.h) */
static u64 stack_index(void *t) { return (u64)cx_pthread_tid(t) & 0xFFF; }
size_t pthread_get_stacksize_np(void *t) {
    u64 i = stack_index(t);
    return i < 4096 && thread_stacks[i].top ? (size_t)thread_stacks[i].size : (size_t)(2048 << 10);
}
void *pthread_get_stackaddr_np(void *t) {
    u64 i = stack_index(t);
    return i < 4096 && thread_stacks[i].top ? (void *)thread_stacks[i].top : (void *)0x400040000000ULL;
}
int pthread_setname_np(const char *n) { char *getenv(const char *); if (getenv("CX_TRACE_THREADS")) cx_fdprintf(2, "[thread] %ld chama-se \"%s\"\n", SYS0(SYS_GETTID), n ? n : ""); return 0; }   /* diagnóstico temporário */
/* id da thread: o tid do kernel (o Chromium usa como PlatformThread::CurrentId; antes era sempre 1).
 * Para t != a atual ainda devolve o tid de quem chamou (pthread_t do mOS não guarda o tid). */
int pthread_threadid_np(void *t, u64 *id) { if (id) *id = (u64)cx_pthread_tid(t); return 0; }
/* os_unfair_lock (u32): 0 = livre, senão o tid do dono. O PartitionAlloc do Chromium protege o heap com
 * ele; como no-op (época de uma thread por processo), a freelist corrompia com várias threads
 * (CHECK "slotsize"/"entrypos"). Feito pela sessão do VS Code. */
void os_unfair_lock_lock(void *l) {
    u32 me = (u32)SYS0(SYS_GETTID);
    while (!__sync_bool_compare_and_swap((volatile u32 *)l, 0u, me)) SYS0(SYS_YIELD);
}
void os_unfair_lock_lock_with_options(void *l, u32 opts) { (void)opts; os_unfair_lock_lock(l); }
void os_unfair_lock_unlock(void *l) { __sync_lock_release((volatile u32 *)l); }
int os_unfair_lock_trylock(void *l) { return __sync_bool_compare_and_swap((volatile u32 *)l, 0u, (u32)SYS0(SYS_GETTID)); }
void os_unfair_lock_assert_owner(void *l) { (void)l; }

/* ================================================================ strings e caminhos */
void *memmem(const void *h, size_t hl, const void *n, size_t nl) {
    const u8 *a = h;
    if (!nl) return (void *)h;
    for (size_t i = 0; i + nl <= hl; i++) if (a[i] == *(const u8 *)n && !memcmp(a + i, n, nl)) return (void *)(a + i);
    return 0;
}

char *dirname_r(const char *p, char *out) {
    size_t n = p ? strlen(p) : 0;
    while (n > 1 && p[n - 1] == '/') n--;
    while (n > 0 && p[n - 1] != '/') n--;
    while (n > 1 && p[n - 1] == '/') n--;
    if (!n) { strcpy(out, p && p[0] == '/' ? "/" : "."); return out; }
    memcpy(out, p, n);
    out[n] = 0;
    return out;
}
char *dirname(char *p) { static char b[1024]; return dirname_r(p, b); }
char *basename_r(const char *p, char *out) {
    if (!p || !*p) { strcpy(out, "."); return out; }
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') n--;
    size_t s = n;
    while (s > 0 && p[s - 1] != '/') s--;
    memcpy(out, p + s, n - s);
    out[n - s] = 0;
    if (!out[0]) strcpy(out, "/");
    return out;
}
char *basename(char *p) { static char b[1024]; return basename_r(p, b); }

int stat(const char *p, void *st);
long readlink(const char *p, char *b, size_t n);
int snprintf(char *b, size_t n, const char *f, ...);
char *strncpy(char *d, const char *s, size_t n);
char *realpath(const char *path, char *out) {
    char tmp[1024], res[1024];
    int hops = 0;
    cx_path(path, tmp, sizeof tmp);
    size_t o = 0;
    res[0] = 0;
    for (char *s = tmp; *s;) {
        while (*s == '/') s++;
        if (!*s) break;
        char *e = s;
        while (*e && *e != '/') e++;
        size_t n = (size_t)(e - s);
        if (n == 1 && s[0] == '.') {}
        else if (n == 2 && s[0] == '.' && s[1] == '.') { while (o > 0 && res[o - 1] != '/') o--; if (o > 0) o--; }
        else if (o + n + 2 < sizeof res) {
            res[o++] = '/'; memcpy(res + o, s, n); o += n;
            res[o] = 0;
            /* componente que é link (arquivo "#mOS-link:"): troca pelo alvo e recomeça com o resto do caminho.
             * O Homebrew compara realpaths ("/usr/local/bin/tree" -> "/usr/local/Cellar/tree/2.3.2/bin/tree"). */
            char tgt[1024];
            long tl = hops < 32 ? readlink(res, tgt, sizeof tgt - 1) : -1;
            if (tl > 0) {
                tgt[tl] = 0;
                char next[1024];
                if (tgt[0] == '/') snprintf(next, sizeof next, "%s%s", tgt, e);
                else {
                    while (o > 0 && res[o - 1] != '/') o--;     /* pasta do link */
                    res[o] = 0;
                    snprintf(next, sizeof next, "%s%s%s", res, tgt, e);
                }
                strncpy(tmp, next, sizeof tmp - 1); tmp[sizeof tmp - 1] = 0;
                hops++;
                o = 0; res[0] = 0;
                s = tmp;
                continue;
            }
        }
        s = e;
    }
    if (!o) res[o++] = '/';
    res[o] = 0;
    char st[160];
    if (stat(res, st) < 0) return 0;
    if (!out) return strdup(res);
    return strcpy(out, res);
}

long pathconf(const char *p, int name) { (void)p; return name == 5 ? 1024 : name == 4 ? 255 : -1; }  /* PATH_MAX, NAME_MAX */
long fpathconf(int fd, int name) { (void)fd; return pathconf(0, name); }

/* strptime: %Y %m %d %H %M %S %y %j %e %T %D %F %z %Z %a %A %b %B %h %n %t %% */
typedef struct { int sec, min, hour, mday, mon, year, wday, yday, isdst; long gmtoff; const char *zone; } mac_tm_t;
static const char *MON[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
static const char *DAY[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
static const char *num(const char *s, int maxd, int *out) {
    int v = 0, d = 0, neg = 0;
    if (*s == '-' || *s == '+') { neg = *s == '-'; s++; }
    while (d < maxd && isdigit((u8)*s)) { v = v * 10 + (*s++ - '0'); d++; }
    if (!d) return 0;
    *out = neg ? -v : v;
    return s;
}
char *strptime(const char *s, const char *f, mac_tm_t *tm) {
    for (; *f; f++) {
        if (isspace((u8)*f)) { while (isspace((u8)*s)) s++; continue; }
        if (*f != '%') { if (*s++ != *f) return 0; continue; }
        f++;
        int v;
        switch (*f) {
        case 'Y': if (!(s = num(s, 4, &v))) return 0; tm->year = v - 1900; break;
        case 'y': if (!(s = num(s, 2, &v))) return 0; tm->year = v < 69 ? v + 100 : v; break;
        case 'm': if (!(s = num(s, 2, &v))) return 0; tm->mon = v - 1; break;
        case 'd': case 'e': while (*s == ' ') s++; if (!(s = num(s, 2, &v))) return 0; tm->mday = v; break;
        case 'H': if (!(s = num(s, 2, &v))) return 0; tm->hour = v; break;
        case 'M': if (!(s = num(s, 2, &v))) return 0; tm->min = v; break;
        case 'S': if (!(s = num(s, 2, &v))) return 0; tm->sec = v; break;
        case 'j': if (!(s = num(s, 3, &v))) return 0; tm->yday = v - 1; break;
        case 'T': if (!(s = strptime(s, "%H:%M:%S", tm))) return 0; break;
        case 'D': if (!(s = strptime(s, "%m/%d/%y", tm))) return 0; break;
        case 'F': if (!(s = strptime(s, "%Y-%m-%d", tm))) return 0; break;
        case 'z': {
            if (*s == 'Z') { s++; tm->gmtoff = 0; break; }
            int sign = *s == '-' ? -1 : 1;
            if (*s == '+' || *s == '-') s++;
            int hh = 0, mm = 0;
            if (!(s = num(s, 2, &hh))) return 0;
            if (*s == ':') s++;
            if (isdigit((u8)*s)) s = num(s, 2, &mm);
            tm->gmtoff = sign * (hh * 3600L + mm * 60L);
            break;
        }
        case 'Z': while (isalpha((u8)*s)) s++; break;
        case 'a': case 'A': {
            int k = 0;
            for (; k < 7; k++) if (!strncasecmp(s, DAY[k], 3)) break;
            if (k == 7) return 0;
            tm->wday = k;
            while (isalpha((u8)*s)) s++;
            break;
        }
        case 'b': case 'B': case 'h': {
            int k = 0;
            for (; k < 12; k++) if (!strncasecmp(s, MON[k], 3)) break;
            if (k == 12) return 0;
            tm->mon = k;
            while (isalpha((u8)*s)) s++;
            break;
        }
        case 'n': case 't': while (isspace((u8)*s)) s++; break;
        case '%': if (*s++ != '%') return 0; break;
        default: return 0;
        }
    }
    return (char *)s;
}

/* ================================================================ suporte a runtimes (Rust, C++) */
extern u64 cx_image_base;
extern char **NXArgv;
char *getenv(const char *);
u32 arc4random(void);
void arc4random_buf(void *buf, size_t n);

/* dyld: _dyld_image_count/_dyld_get_image_*, dlopen/dlsym/dlclose/dlerror ficam em dlfcn.c (sessão do dlopen) */
int _NSGetExecutablePath(char *buf, u32 *size) {
    const char *p = NXArgv && NXArgv[0] ? NXArgv[0] : "/app";
    u32 n = (u32)strlen(p) + 1;
    if (*size < n) { *size = n; return -1; }
    memcpy(buf, p, n);
    return 0;
}

/* destrutores de thread_local (C++/Rust): rodam no exit */
int __cxa_atexit(void (*fn)(void *), void *arg, void *dso);
void _tlv_atexit(void (*fn)(void *), void *obj) { __cxa_atexit(fn, obj, 0); }

/* memória virtual */
#define CX_MMAP_BASE 0x400100000000ULL   /* USER_MMAP_BASE do kernel */

/* Grava o prot na PTE (SYS_MMAP b = 7). Sem PROT_EXEC a página fica NX. PROT_NONE não passa por aqui:
 * no guest arm64 o PR #11 desmapeia a página de 16 KiB inteira (a64_span + MOS_MUNMAP_KEEP); no x86 a
 * reserva continua. O comprimento já é o span do chamador — 4 KiB no x86, 16 KiB no arm64 quando o
 * a64_span do PR #11 arredondou antes. No x86 a página presente segue legível (não há PROT_NONE no PTE). */
static int cx_pte_prot(u64 addr, u64 len, int prot) {
    if (!prot || !len || addr < CX_MMAP_BASE) return 0;
    long r = SYS5(SYS_MMAP, (long)len, 7, (long)addr, 0, prot);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    return 0;
}

/* MAP_SHARED de arquivo com escrita: o mapeamento é uma cópia em memória (lida com read). Para o que se escreve nele
 * chegar ao arquivo, cada mapeamento fica registrado com um dup do fd (no macOS o mapeamento sobrevive ao close) e
 * volta ao arquivo com pwrite no msync e no munmap, até o tamanho atual do arquivo (além do fim seria SIGBUS no macOS).
 * O Zig copia a saída do cache assim (ftruncate do destino, mmap MAP_SHARED, memcpy, munmap): sem isto, os objetos e
 * executáveis do `zig cc` saíam com o tamanho certo e só zeros. */
int dup(int fd);
int fstat(int fd, void *st);
int close(int fd);
ssize_t pwrite(int fd, const void *b, size_t n, off_t off);
typedef struct { u64 addr, len; int fd; off_t off; } cx_shm_t;
static cx_shm_t shm_tab[64];
static volatile int shm_lock;
static void shm_take(void) { while (__atomic_exchange_n(&shm_lock, 1, __ATOMIC_ACQUIRE)) __builtin_ia32_pause(); }
static void shm_give(void) { __atomic_store_n(&shm_lock, 0, __ATOMIC_RELEASE); }
static void shm_track(u64 addr, size_t len, int fd, off_t off) {
    int d = dup(fd);
    if (d < 0) return;
    shm_take();
    for (int i = 0; i < 64; i++) if (!shm_tab[i].len) {
        shm_tab[i] = (cx_shm_t){addr, len, d, off};
        shm_give();
        return;
    }
    shm_give();
    close(d);   /* tabela cheia: fica como antes (cópia sem volta) */
}
/* grava de volta os mapeamentos que cruzam [a, a+n); drop = 1 (munmap) tira da tabela os que o trecho cobre inteiros */
static void shm_flush(u64 a, size_t n, int drop) {
    if (!n) return;
    shm_take();
    for (int i = 0; i < 64; i++) {
        cx_shm_t *m = &shm_tab[i];
        if (!m->len || a >= m->addr + m->len || a + n <= m->addr) continue;
        u64 lo = a > m->addr ? a : m->addr, hi = a + n < m->addr + m->len ? a + n : m->addr + m->len;
        u64 stb[18] = {0};                                      /* stat do macOS: st_size em +96 */
        long size = fstat(m->fd, stb) == 0 ? (long)stb[12] : 0;
        long fo = (long)m->off + (long)(lo - m->addr);
        if (size > fo) {
            u64 cnt = hi - lo;
            if ((long)cnt > size - fo) cnt = (u64)(size - fo);
            for (u64 w = 0; w < cnt;) { ssize_t k = pwrite(m->fd, (const char *)(lo + w), cnt - w, fo + (long)w); if (k <= 0) break; w += (u64)k; }
        }
        if (drop && a <= m->addr && a + n >= m->addr + m->len) { close(m->fd); m->len = 0; }
    }
    shm_give();
}
#if defined(__aarch64__)
static long a64_sys(long n, long a, long b, long c, long d, void *ctx) {
    (void)ctx;
    return cx_syscall(n, a, b, c, d, 0);
}
void a64rt_map_jit_register(void *addr, unsigned long long len);
static void note_map_jit(int flags, void *p, unsigned long long n) {
    if ((flags & 0x800) && p && p != (void *)-1) a64rt_map_jit_register(p, n);
}
#endif
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    /* MAP_JIT (0x800): o V8 pede uma região anônima executável. Com PROT_EXEC a PTE não leva NX. */
    if (flags & 0x800) flags |= 0x1000;
#if defined(__aarch64__)
    /* Processo arm64: granularidade de 16 KiB (docs/pagesize-arm64.md). O kernel segue em 4 KiB. */
    {
        a64_mmap_adj_t adj = a64_mmap_adjust((unsigned long long)addr, len, flags, off);
        if (adj.err) { cx_errno = adj.err; return (void *)-1; }
        len = (size_t)adj.len;
    }
#endif
    /* MAP_FIXED de arquivo: o kernel substitui [addr, addr+len) e devolve esse endereço
     * (b = 8 privado, b = 9 compartilhado). c = endereço, r10 = deslocamento,
     * r8 = fd | (prot << 32). Erro não cai na cópia com read(), que ignoraria o endereço.
     * No arm64 o comprimento já saiu de a64_mmap_adjust (16 KiB); o endereço é o pedido,
     * então não há cursor para realinhar nem sobra de 4 KiB para devolver. */
    if ((flags & 0x10) && !(flags & 0x1000) && fd >= 0 && addr) {
        unsigned long long pack = (unsigned)fd | ((unsigned long long)(prot ? prot : 1) << 32);
        long mode = (flags & 1) ? 9 : 8;
        long k = SYS5(SYS_MMAP, len, mode, addr, off, pack);
        if (k < 0) { cx_errno = (int)-k; return (void *)-1; }
        if (cx_pte_prot((u64)k, len, prot)) return (void *)-1;
        return (void *)k;
    }
    /* MAP_FIXED sobre a pilha (página de guarda do Rust, por exemplo): a região já existe.
     * A pilha principal fica abaixo de USER_MMAP_BASE: o kernel recusa munmap ali, então a guarda
     * da pilha principal não é desmapeável. A das threads (pthread_create) fica na arena de mmap. */
    if ((flags & 0x10) && addr && (u64)addr >= 0x400000000000ULL && (u64)addr < 0x400040000000ULL) return addr;
    /* MAP_FIXED|MAP_ANON dentro da área de mmap (ex.: trecho de uma reserva do PartitionAlloc):
     * troca o trecho por páginas zeradas, ou só devolve as páginas se for PROT_NONE. O trecho continua do processo
     * (MOS_MUNMAP_KEEP): solto, o topo voltava ao alocador de endereços e o próximo mmap de outro componente caía nele */
    if ((flags & 0x1010) == 0x1010 && addr && (u64)addr >= CX_MMAP_BASE) {
#if defined(__aarch64__)
        long c = a64_map_fixed_anon(a64_sys, 0, (unsigned long long)addr, len, prot);
        if (c < 0) { cx_errno = (int)-c; return (void *)-1; }
        if (prot && cx_pte_prot((u64)c, len, prot)) return (void *)-1;
        note_map_jit(flags, (void *)c, (unsigned long long)len);
        return (void *)c;
#else
        SYS3(SYS_MUNMAP, addr, len, MOS_MUNMAP_KEEP);
        /* b = 3: o trecho vira memória sob demanda (páginas zeradas ao tocar; kernel/mm/lazy.c) */
        if (prot) {
            long c = SYS3(SYS_MMAP, len, 3, addr);
            if (c < 0) { cx_errno = (int)-c; return (void *)-1; }
            if (cx_pte_prot((u64)addr, len, prot)) return (void *)-1;
        }
        return addr;
#endif
    }
    /* anônimo: PROT_NONE = só reservar endereços (as páginas vêm no mprotect); com acesso = sob demanda, como no
     * macOS (o JavaScriptCore do Claude Code pede 1-4 GB assim e usa uma fração). */
    int anon = (flags & 0x1000) != 0;
    /* MAP_SHARED de arquivo (b = 5): a página física é a mesma para outro mapeamento do arquivo.
     * Pipe, terminal e /dev/null devolvem EINVAL e caem na cópia com shm_track, abaixo.
     * No guest arm64 o PR #11 realinha o cursor (a64_align_cursor) e devolve a sobra de 4 KiB
     * (a64_reclaim_slop) antes deste SYS_MMAP; o comprimento que chega aqui já é o span de 16 KiB. */
    if (!anon && fd >= 0 && (flags & 1) && !(off & 0xFFF)) {
#if defined(__aarch64__)
        long aln = a64_align_cursor(a64_sys, 0);
        if (aln < 0) { cx_errno = (int)-aln; return (void *)-1; }
#endif
        long k = SYS5(SYS_MMAP, len, 5, fd, off, prot ? prot : 1);
        if (k >= 0) {
#if defined(__aarch64__)
            long sl = a64_reclaim_slop(a64_sys, 0, (unsigned long long)k, len);
            if (sl < 0) { cx_errno = (int)-sl; return (void *)-1; }
#endif
            if (cx_pte_prot((u64)k, len, prot)) return (void *)-1;
            return (void *)k;
        }
        if (k == -9 || k == -13) { cx_errno = (int)-k; return (void *)-1; }
    }
    /* arquivo MAP_PRIVATE ou só leitura: o kernel lê cada página na primeira falta (b = 4, kernel/mm/lazy.c). */
    if (!anon && fd >= 0 && !(off & 0xFFF) && !((flags & 1) && (prot & 2))) {
#if defined(__aarch64__)
        long aln = a64_align_cursor(a64_sys, 0);
        if (aln < 0) { cx_errno = (int)-aln; return (void *)-1; }
#endif
        long k = SYS4(SYS_MMAP, len, 4, fd, off);
        if (k >= 0) {
#if defined(__aarch64__)
            long sl = a64_reclaim_slop(a64_sys, 0, (unsigned long long)k, len);
            if (sl < 0) { cx_errno = (int)-sl; return (void *)-1; }
#endif
            if (cx_pte_prot((u64)k, len, prot)) return (void *)-1;
            return (void *)k;
        }
        if (k == -9 || k == -13) { cx_errno = (int)-k; return (void *)-1; }   /* EBADF, EACCES */
    }
    long r;
#if defined(__aarch64__)
    if (anon && (flags & 1)) {
        /* MAP_SHARED|MAP_ANON: o mesmo alinhamento de 16 KiB, com o prot no 5º argumento (b = 6). */
        long aln = a64_align_cursor(a64_sys, 0);
        if (aln < 0) { cx_errno = (int)-aln; return (void *)-1; }
        r = SYS5(SYS_MMAP, len, 6, 0, 0, prot ? prot : 1);
        if (r < 0) { cx_errno = (int)-r; return (void *)-1; }
        long sl = a64_reclaim_slop(a64_sys, 0, (unsigned long long)r, len);
        if (sl < 0) { cx_errno = (int)-sl; return (void *)-1; }
    } else {
        /* O rt_alloc do tradutor também anda o cursor, em 4 KiB: cada faixa nova realinha. */
        unsigned long long at = 0;
        long mode = anon ? (prot ? 3 : 1) : 0;
        long rc = a64_map_fresh(a64_sys, 0, len, mode, &at);
        if (rc < 0) { cx_errno = (int)-rc; return (void *)-1; }
        r = (long)at;
    }
    note_map_jit(flags, (void *)r, (unsigned long long)len);
#else
    if (anon && (flags & 1)) r = SYS5(SYS_MMAP, len, 6, 0, 0, prot ? prot : 1);
    else r = SYS2(SYS_MMAP, len, anon ? (prot ? 3 : 1) : 0);
    if (r < 0) { cx_errno = (int)-r; return (void *)-1; }
#endif
    if (!(flags & 0x1000) && fd >= 0) {           /* MAP_ANON = 0x1000; senão: copia o arquivo */
        off_t cur = lseek(fd, 0, 1);
        lseek(fd, off, 0);
        size_t got = 0;
        while (got < len) { ssize_t k = read(fd, (char *)r + got, len - got); if (k <= 0) break; got += (size_t)k; }
        lseek(fd, cur, 0);
        if ((flags & 1) && (prot & 2)) shm_track((u64)r, len, fd, off);   /* MAP_SHARED com escrita: volta ao arquivo */
    }
    if (prot && cx_pte_prot((u64)r, len, prot)) return (void *)-1;
    return (void *)r;
}
int munmap(void *a, size_t len) {
#if defined(__aarch64__)
    a64_span_t sp = a64_span((unsigned long long)a, len);
    if (sp.err) { cx_errno = sp.err; return -1; }
    if (!sp.len) return 0;
    shm_flush(sp.addr, (size_t)sp.len, 1);
    long r = a64_sys(SYS_MUNMAP, (long)sp.addr, (long)sp.len, 0, 0, 0);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    return 0;
#else
    shm_flush((u64)a, len, 1);
    return SYS2(SYS_MUNMAP, a, len) < 0 ? -1 : 0;
#endif
}
/* msync: a cópia antiga (shm_flush) e o MAP_SHARED do kernel (MOS_MUNMAP_SYNC). O kernel não desmapeia.
 * No arm64 o trecho é o span de 16 KiB, como o munmap. */
int msync(void *a, size_t n, int f) {
    (void)f;
#if defined(__aarch64__)
    a64_span_t sp = a64_span((unsigned long long)a, n);
    if (sp.err) { cx_errno = sp.err; return -1; }
    if (!sp.len) return 0;
    shm_flush(sp.addr, (size_t)sp.len, 0);
    SYS3(SYS_MUNMAP, sp.addr, sp.len, MOS_MUNMAP_SYNC);
    return 0;
#else
    shm_flush((u64)a, n, 0);
    if (n) SYS3(SYS_MUNMAP, a, n, MOS_MUNMAP_SYNC);
    return 0;
#endif
}
void cx_mach_record_prot(u64 addr, u64 len, int prot);   /* mach.c: proteção registrada, lida pelo mach_vm_region */
int mprotect(void *a, size_t len, int prot) {
    /* liberar acesso a um trecho reservado: as páginas que faltam passam a vir sob demanda (as existentes ficam).
     * Com prot diferente de zero, b = 7 grava a PTE: sem PROT_EXEC fica NX, sem PROT_WRITE perde a escrita.
     * PROT_NONE não mexe na PTE. No arm64 ele desmapeia a página guest inteira (a guarda falta de verdade).
     * Endereço não múltiplo de 16 KiB é EINVAL; o comprimento sobe até fechar a página guest, e o b = 7
     * cobre esse span. A proteção pedida continua registrada para o mach_vm_region. */
#if defined(__aarch64__)
    a64_span_t sp = a64_span((unsigned long long)a, len);
    if (sp.err) { cx_errno = sp.err; return -1; }
    if (sp.len) cx_mach_record_prot(sp.addr, sp.len, prot);
    if (sp.len && sp.addr >= CX_MMAP_BASE) {
        long r = !prot ? a64_sys(SYS_MUNMAP, (long)sp.addr, (long)sp.len, MOS_MUNMAP_KEEP, 0, 0)
                       : a64_sys(SYS_MMAP, (long)sp.len, 3, (long)sp.addr, 0, 0);
        if (r < 0) { cx_errno = (int)-r; return -1; }
        if (prot && cx_pte_prot(sp.addr, sp.len, prot)) return -1;
    }
    return 0;
#else
    if (len) cx_mach_record_prot((u64)a, (u64)len, prot);
    if (prot && len && (u64)a >= CX_MMAP_BASE) {
        long r = SYS3(SYS_MMAP, len, 3, a);
        if (r < 0) { cx_errno = (int)-r; return -1; }
        if (cx_pte_prot((u64)a, len, prot)) return -1;
    }
    return 0;
#endif
}
#if !defined(__aarch64__)
/* x86_64 nativo: a página de usuário já é leitura+escrita+execução. No arm64 estas três funções
 * estão em libc_arm64.S e falam com o a64rt (proteção por thread só nas faixas MAP_JIT). */
void pthread_jit_write_protect_np(int enabled) { (void)enabled; }
int pthread_jit_write_protect_supported_np(void) { return 1; }
void pthread_jit_write_with_callback_np(void (*cb)(void *), void *ctx) { if (cb) cb(ctx); }
#endif
#if !defined(__aarch64__)   /* arm64: libc_arm64.S (DC CVAU + IC IVAU + DSB/ISB) */
void sys_icache_invalidate(void *start, size_t len) { (void)start; (void)len; }
void sys_dcache_flush(void *start, size_t len) { (void)start; (void)len; }
#endif
/* MADV_DONTNEED (4), MADV_FREE (5) e MADV_FREE_REUSABLE (7): o conteúdo não importa mais. As páginas privadas da faixa
 * anônima sob demanda saem e voltam zeradas no próximo toque (kernel: SYS_MUNMAP com MOS_MUNMAP_DISCARD), como o XNU faz
 * com MADV_FREE (VM_SYNC_KILLPAGES). O mimalloc 3 do Bun (Claude Code) devolve memória assim e depois a trata como
 * zerada; sem efeito, um bloco "zerado" vinha com dados velhos e o rehash de uma PropertyTable do JSC não terminava.
 * O resto (WILLNEED, RANDOM, FREE_REUSE...) segue sem efeito. O posix_madvise não descarta nada, como no POSIX. */
int madvise(void *a, size_t len, int adv) {
#if defined(__aarch64__)
    /* Mesma granularidade do mmap: endereço desalinhado é EINVAL e o descarte cobre a página guest. */
    if (adv == 4 || adv == 5 || adv == 7) {
        a64_span_t sp = a64_span((unsigned long long)a, len);
        if (sp.err) { cx_errno = sp.err; return -1; }
        if (sp.len && sp.addr >= CX_MMAP_BASE)
            a64_sys(SYS_MUNMAP, (long)sp.addr, (long)sp.len, MOS_MUNMAP_DISCARD, 0, 0);
    }
    return 0;
#else
    if ((adv == 4 || adv == 5 || adv == 7) && len && !((u64)a & 0xFFF) && (u64)a >= CX_MMAP_BASE)
        SYS3(SYS_MUNMAP, a, (len + 0xFFF) & ~(size_t)0xFFF, MOS_MUNMAP_DISCARD);
    return 0;
#endif
}
int posix_madvise(void *a, size_t len, int adv) { (void)a; (void)len; (void)adv; return 0; }
int mlock(const void *a, size_t len) { (void)a; (void)len; return 0; }

/* aleatoriedade (CommonCrypto faz parte do libSystem) */
int CCRandomGenerateBytes(void *buf, size_t n) { arc4random_buf(buf, n); return 0; }
int getentropy(void *buf, size_t n) { arc4random_buf(buf, n); return 0; }
int SecRandomCopyBytes(void *rnd, size_t n, void *buf) { (void)rnd; arc4random_buf(buf, n); return 0; }

/* libdispatch: semáforos de um processo com uma thread */
/* contador + quantos esperam; a espera dorme no futex sobre os 32 bits baixos do contador (x86: mesmo endereço) */
/* 1º word: isa ObjC (OS_dispatch_semaphore), como no macOS */
extern char OS_dispatch_semaphore_c[] __asm__("OBJC_CLASS_$_OS_dispatch_semaphore");
typedef struct { void *isa; long count; volatile int waiters; } cx_sema_t;
void *dispatch_semaphore_create(long v) { cx_sema_t *s = malloc(sizeof *s); if (s) { s->isa = OS_dispatch_semaphore_c; s->count = v; s->waiters = 0; } return s; }
long dispatch_semaphore_signal(void *s) {
    cx_sema_t *x = s;
    __sync_fetch_and_add(&x->count, 1);
    if (x->waiters) cx_futex_wake((volatile int *)&x->count, 1);
    return 0;
}
/* dispatch_time, como no macOS: absoluto, em ns na base do mach_absolute_time (SYS_TIME em ms * 10^6);
 * DISPATCH_TIME_NOW = 0, DISPATCH_TIME_FOREVER = ~0, DISPATCH_WALLTIME_NOW = ~1 (aqui o mesmo relógio). O bit 63
 * (DISPATCH_UP_OR_MONOTONIC_TIME_MASK, relógio contínuo do Swift) não muda a base no mOS. */
static u64 now_ns(void) { return (u64)SYS0(SYS_TIME) * 1000000ull; }
u64 cx_dispatch_deadline_ns(u64 t) { return t == 0 || t == ~1ull ? now_ns() : (t & ~(1ull << 63)); }
u64 dispatch_time(u64 when, long delta) {
    if (when == ~0ull) return when;
    u64 base = cx_dispatch_deadline_ns(when);
    if (delta < 0 && (u64)(-delta) >= base) return 1;
    return base + (u64)delta;
}
/* espera de verdade: agora há threads (atrasos do dispatch.c, executor do Swift) que podem sinalizar */
long dispatch_semaphore_wait(void *s, u64 timeout) {
    cx_sema_t *x = s;
    { extern void cx_dispatch_drain(void); cx_dispatch_drain(); }   /* dispatch.c: blocos adiados por esta thread */
    u64 limit = timeout == ~0ull ? ~0ull : cx_dispatch_deadline_ns(timeout);
    for (;;) {
        long c = x->count;
        if (c > 0) { if (__sync_bool_compare_and_swap(&x->count, c, c - 1)) return 0; continue; }
        long left = -1;
        if (limit != ~0ull) {
            u64 now = now_ns();
            if (now >= limit) return 1;
            left = (long)((limit - now + 999999) / 1000000);
        }
        __sync_fetch_and_add(&x->waiters, 1);
        if (x->count == c) cx_futex_wait((volatile int *)&x->count, (int)c, left);
        __sync_fetch_and_sub(&x->waiters, 1);
    }
}
/* contagem de referências em dispatch.c (antes: retain não fazia nada e release liberava na hora, o que
 * causava uso depois de liberar em todo par retain/release e free() nas filas estáticas) */
void cx_dispatch_retain(void *o);
void cx_dispatch_release(void *o);
void dispatch_release(void *o) { cx_dispatch_release(o); }
void dispatch_retain(void *o) { cx_dispatch_retain(o); }

/* unwinder: só usado em panics; sem tabelas, informa "fim da pilha" */
/* _Unwind_*: o libunwind do LLVM (tools/build-libsystem*.ps1; tabelas do Mach-O e do .eh_frame do libSystem, unwind_ls.c) */

/* processos, usuários e sistema */
int sched_yield(void) { SYS0(SYS_YIELD); return 0; }
/* pause: libc.c (sigsuspend com a máscara atual) */
/* status no formato do macOS (WIFEXITED/WIFSIGNALED/WIFSTOPPED/WIFCONTINUED).
 * WNOHANG, WUNTRACED e WCONTINUED passam ao kernel; o resto (WNOWAIT, WEXITED) é ignorado. */
int waitpid(int pid, int *st, int opt) {
    int s = 0;
    long r = cx_syscall(SYS_WAITPID, pid, (long)&s, opt & (MOS_WNOHANG | MOS_WUNTRACED | MOS_WCONTINUED), 0, 0);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    {   /* número do kernel (USR1 10, STOP 20, CHLD 17) → o do macOS. Parada: byte baixo 0x7f e o sinal no alto. */
        extern int cx_sig_from_mos(int);
        if ((s & 0xFF) == 0x7F) {
            int sig = (s >> 8) & 0xFF;
            if (sig) s = (cx_sig_from_mos(sig) << 8) | 0x7F;
        } else {
            int ts = s & 0x7F;
            if (ts) s = (s & ~0x7F) | cx_sig_from_mos(ts);
        }
    }
    if (st && r > 0) *st = s;
    return (int)r;
}
int wait(int *st) { return waitpid(-1, st, 0); }
int wait3(int *st, int opt, void *ru) { if (ru) memset(ru, 0, 144); return waitpid(-1, st, opt); }
int wait4(int pid, int *st, int opt, void *ru) { if (ru) memset(ru, 0, 144); return waitpid(pid, st, opt); }
int setuid(unsigned u) { (void)u; return 0; }
int setgid(unsigned g) { (void)g; return 0; }
int setgroups(int n, const unsigned *g) { (void)n; (void)g; return 0; }
/* setpgid e setsid: em pty.c (sessões e grupos do kernel, SYS_TTY) */
int chroot(const char *p) { (void)p; cx_errno = CX_EPERM; return -1; }
/* sigaltstack: libc.c (SYS_SIG) */
#define FA_MAGIC 0x6D4F5346u
#define AT_MAGIC 0x6D4F5354u
typedef struct { u32 magic; int n; mos_spawn_fa_t op[MOS_SPAWN_FA_MAX]; } cx_fa;
typedef struct { u32 magic; unsigned flags; } cx_at;

static cx_fa *fa_of(void *a) {
    cx_fa *f = a ? *(cx_fa **)a : 0;
    return f && f->magic == FA_MAGIC ? f : 0;
}
static int fa_push(void *a, mos_spawn_fa_t op) {
    cx_fa *f = fa_of(a);
    if (!f || f->n >= MOS_SPAWN_FA_MAX) return 22;
    f->op[f->n++] = op;
    return 0;
}
int posix_spawn_file_actions_init(void *a) {
    cx_fa *f = calloc(1, sizeof *f);
    if (!a || !f) return 12;
    f->magic = FA_MAGIC;
    *(void **)a = f;
    return 0;
}
int posix_spawn_file_actions_destroy(void *a) {
    cx_fa *f = fa_of(a);
    if (f) free(f);
    if (a) *(void **)a = 0;
    return 0;
}
int posix_spawn_file_actions_adddup2(void *a, int x, int y) {
    mos_spawn_fa_t op; memset(&op, 0, sizeof op);
    op.kind = MOS_SPAWN_FA_DUP2; op.a = x; op.b = y;
    return fa_push(a, op);
}
int posix_spawn_file_actions_addclose(void *a, int x) {
    mos_spawn_fa_t op; memset(&op, 0, sizeof op);
    op.kind = MOS_SPAWN_FA_CLOSE; op.a = x;
    return fa_push(a, op);
}
int posix_spawn_file_actions_addopen(void *a, int fd, const char *path, int flags, unsigned mode) {
    mos_spawn_fa_t op; memset(&op, 0, sizeof op);
    op.kind = MOS_SPAWN_FA_OPEN; op.a = fd; op.mode = mode;
    if ((flags & 3) == MAC_O_WRONLY) op.flags |= MOS_O_WRONLY;
    if ((flags & 3) == MAC_O_RDWR) op.flags |= MOS_O_RDWR;
    if (flags & MAC_O_CREAT) op.flags |= MOS_O_CREAT;
    if (flags & MAC_O_TRUNC) op.flags |= MOS_O_TRUNC;
    if (flags & MAC_O_APPEND) op.flags |= MOS_O_APPEND;
    if (path) {
        char buf[256];
        cx_path(path, buf, sizeof buf);
        memcpy(op.path, buf, sizeof op.path - 1);
    }
    return fa_push(a, op);
}
int posix_spawn_file_actions_addinherit_np(void *a, int fd) {
    return posix_spawn_file_actions_adddup2(a, fd, fd);
}
int posix_spawn_file_actions_addchdir_np(void *a, const char *p) {
    mos_spawn_fa_t op; memset(&op, 0, sizeof op);
    op.kind = MOS_SPAWN_FA_CHDIR;
    if (p) {
        char buf[256];
        cx_path(p, buf, sizeof buf);
        memcpy(op.path, buf, sizeof op.path - 1);
    }
    return fa_push(a, op);
}
/* addfchdir_np: o filho começa na pasta do fd (o std.process do Zig muda a pasta do filho assim) */
int posix_spawn_file_actions_addfchdir_np(void *a, int fd) {
    char d[1024];
    if (cx_syscall(SYS_FDPATH, fd, (long)d, sizeof d, 0, 0) < 0) return 9;   /* EBADF */
    return posix_spawn_file_actions_addchdir_np(a, d);
}
int posix_spawnattr_init(void *a) {
    cx_at *t = calloc(1, sizeof *t);
    if (!a || !t) return 12;
    t->magic = AT_MAGIC;
    *(void **)a = t;
    return 0;
}
int posix_spawnattr_destroy(void *a) {
    cx_at *t = a ? *(cx_at **)a : 0;
    if (t && t->magic == AT_MAGIC) free(t);
    if (a) *(void **)a = 0;
    return 0;
}
int posix_spawnattr_setflags(void *a, short f) {
    cx_at *t = a ? *(cx_at **)a : 0;
    if (!t || t->magic != AT_MAGIC) return 22;
    t->flags = (unsigned)(unsigned short)f;
    return 0;
}
int posix_spawnattr_setpgroup(void *a, int g) { (void)a; (void)g; return 0; }
int posix_spawnattr_setsigdefault(void *a, const void *s) { (void)a; (void)s; return 0; }
int posix_spawnattr_setsigmask(void *a, const void *s) { (void)a; (void)s; return 0; }
int posix_spawnp(int *pid, const char *f, const void *fa, const void *at, char *const *av, char *const *ev) {
    /* argv, envp e as file actions vão inteiros. A string de 256 bytes do SYS_SPAWN cortava a chave
     * ",r," da memória compartilhada e o filho do Electron saía com exit(0). */
    char path[1024];
    cx_path(f, path, sizeof path);
    mos_spawn_vec_t v;
    memset(&v, 0, sizeof v);
    v.argv = (u64)av;
    v.envp = (u64)ev;
    cx_fa *acts = fa_of((void *)fa);
    cx_at *attr = at ? *(cx_at **)at : 0;
    if (acts && acts->n) { v.acts = (u64)acts->op; v.nacts = (unsigned)acts->n; }
    if (attr && attr->magic == AT_MAGIC) v.flags = attr->flags;
    const char *use = (f && (f[0] == '/' || strchr(f, '/'))) ? path : f;
    long r = cx_syscall(SYS_SPAWNVEC, (long)use, (long)&v, 0, 0, 0);
    if (r < 0) return CX_ENOENT;
    if (pid) *pid = (int)r;
    return 0;
}
int posix_spawn(int *pid, const char *f, const void *fa, const void *at, char *const *av, char *const *ev) {
    return posix_spawnp(pid, f, fa, at, av, ev);
}
/* execve substitui a imagem do processo (mesmo pid, fds sem O_CLOEXEC e sessão).
 * /usr/bin/sandbox-exec não existe como arquivo: o Codex o chama com `-p <política> -- comando`.
 * A política é ignorada (o Seatbelt daqui não confina) e o comando roda. */
int execve(const char *path, char *const argv[], char *const envp[]) {
    if (!path || !path[0]) { cx_errno = CX_EINVAL; return -1; }
    const char *run = path;
    char *const *av = argv;
    const char *base = path;
    for (const char *s = path; *s; s++) if (*s == '/') base = s + 1;
    if (!strcmp(base, "sandbox-exec")) {
        int i = 1;
        while (av && av[i]) {
            if (!strcmp(av[i], "--")) { i++; break; }
            if ((!strcmp(av[i], "-p") || !strcmp(av[i], "-f") || !strcmp(av[i], "-n") || !strcmp(av[i], "-D")) && av[i + 1]) {
                i += 2;
                continue;
            }
            if (av[i][0] == '-') { i++; continue; }
            break;
        }
        if (!av || !av[i]) { cx_errno = CX_EINVAL; return -1; }
        run = av[i];
        av = av + i;
    }
    /* SYS_EXECVE troca a imagem no mesmo pid (o kernel segue links e "#!"); só volta com erro */
    char full[1024];
    cx_path(run, full, sizeof full);
    const char *use = strchr(run, '/') ? full : run;
    mos_spawn_vec_t v;
    memset(&v, 0, sizeof v);
    v.argv = (u64)(av ? av : argv);
    v.envp = (u64)envp;
    long r = cx_syscall(SYS_EXECVE, (long)use, (long)&v, 0, 0, 0);
    cx_errno = r < 0 ? (int)-r : CX_ENOENT;
    return -1;
}
extern char **environ;
int execv(const char *path, char *const argv[]) { return execve(path, argv, environ); }
/* execvp/execvpe: nome sem '/' procura nas pastas do PATH (como no macOS) */
int execvpe(const char *file, char *const argv[], char *const envp[]) {
    if (!file || !file[0]) { cx_errno = CX_ENOENT; return -1; }
    if (strchr(file, '/')) return execve(file, argv, envp);
    const char *pathv = getenv("PATH");
    if (!pathv || !pathv[0]) pathv = "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin";
    char cand[1024];
    for (const char *s = pathv; ; ) {
        const char *e = strchr(s, ':');
        size_t n = e ? (size_t)(e - s) : strlen(s);
        if (n == 0) n = 1, s = ".";
        if (n + strlen(file) + 2 < sizeof cand) {
            memcpy(cand, s, n);
            cand[n] = '/';
            strcpy(cand + n + 1, file);
            execve(cand, argv, envp);                 /* só volta se falhou */
        }
        if (!e) break;
        s = e + 1;
    }
    return execve(file, argv, envp);                  /* nome curto: /Sistema/Programas no kernel */
}
int execvp(const char *file, char *const argv[]) { return execvpe(file, argv, environ); }

typedef struct { char sysname[256], nodename[256], release[256], version[256], machine[256]; } mac_utsname_t;
int uname(mac_utsname_t *u) {
    memset(u, 0, sizeof *u);
    strcpy(u->sysname, "Darwin");
    strcpy(u->nodename, "mos");
    strcpy(u->release, "24.0.0");
    strcpy(u->version, "mOS Nucleo (compatibilidade Darwin)");
#if defined(__aarch64__)
    strcpy(u->machine, "arm64");               /* A64RT_UNAME_MACHINE */
#else
    strcpy(u->machine, "x86_64");
#endif
    return 0;
}
int gethostname(char *b, size_t n) { strlcpy(b, "mos", n); return 0; }

typedef struct {
    char *pw_name, *pw_passwd; unsigned pw_uid, pw_gid; long pw_change; char *pw_class, *pw_gecos, *pw_dir, *pw_shell; long pw_expire;
} mac_passwd_t;
int getpwuid_r(unsigned uid, mac_passwd_t *pw, char *buf, size_t len, mac_passwd_t **res) {
    (void)uid;
    const char *s[] = {"visitante", "*", "", "Visitante", "/Users/Visitante", "/bin/msh"};
    size_t o = 0;
    char *ptr[6];
    for (int i = 0; i < 6; i++) {
        size_t n = strlen(s[i]) + 1;
        if (o + n > len) { *res = 0; return 34; }  /* ERANGE */
        memcpy(buf + o, s[i], n);
        ptr[i] = buf + o;
        o += n;
    }
    memset(pw, 0, sizeof *pw);
    pw->pw_name = ptr[0]; pw->pw_passwd = ptr[1]; pw->pw_class = ptr[2]; pw->pw_gecos = ptr[3]; pw->pw_dir = ptr[4]; pw->pw_shell = ptr[5];
    pw->pw_uid = 501; pw->pw_gid = 20;
    *res = pw;
    return 0;
}
mac_passwd_t *getpwuid(unsigned uid) { static mac_passwd_t pw; static char b[256]; mac_passwd_t *r; getpwuid_r(uid, &pw, b, sizeof b, &r); return r; }
mac_passwd_t *getpwnam(const char *n) { (void)n; return getpwuid(501); }
char *getlogin(void) { return "visitante"; }

/* E/S vetorial e variantes "at" */
typedef struct { void *base; size_t len; } mac_iovec_t;
ssize_t writev(int fd, const mac_iovec_t *iov, int n) {
    ssize_t total = 0;
    for (int i = 0; i < n; i++) {
        ssize_t r = write(fd, iov[i].base, iov[i].len);
        if (r < 0) return total ? total : -1;
        total += r;
        if ((size_t)r < iov[i].len) break;
    }
    return total;
}
ssize_t readv(int fd, const mac_iovec_t *iov, int n) {
    ssize_t total = 0;
    for (int i = 0; i < n; i++) {
        ssize_t r = read(fd, iov[i].base, iov[i].len);
        if (r < 0) return total ? total : -1;
        total += r;
        if ((size_t)r < iov[i].len) break;
    }
    return total;
}
/* pread/pwrite: deslocamento explícito no kernel (SYS_PREAD/SYS_PWRITE), atômico entre threads. Com lseek+read+lseek,
 * duas threads no mesmo fd (o pool do libuv e o AsarURLLoader do Electron lendo o node_modules.asar) liam do lugar
 * errado. Fd que não é arquivo do kernel (EBADF: sockets e fds do próprio libSystem) segue pelo caminho antigo. */
ssize_t pread(int fd, void *b, size_t n, off_t off) {
    if (off < 0) { cx_errno = 22; return -1; }
    long r = SYS4(SYS_PREAD, fd, b, n, off);
    if (r >= 0) return r;
    if (r != -9) { cx_errno = (int)-r; return -1; }
    off_t c = lseek(fd, 0, 1); lseek(fd, off, 0); ssize_t k = read(fd, b, n); lseek(fd, c, 0); return k;
}
ssize_t pwrite(int fd, const void *b, size_t n, off_t off) {
    if (off < 0) { cx_errno = 22; return -1; }
    long r = SYS4(SYS_PWRITE, fd, b, n, off);
    if (r >= 0) return r;
    if (r != -9) { cx_errno = (int)-r; return -1; }
    off_t c = lseek(fd, 0, 1); lseek(fd, off, 0); ssize_t k = write(fd, b, n); lseek(fd, c, 0); return k;
}

typedef struct { char path[1024]; long index; } cx_dir_hdr_t;   /* prefixo de cx_dir_t (libc.c) */
/* dirfd e fdopendir ficam em libc.c (precisam do DIR) */
int stat(const char *p, void *st);
int fstat(int fd, void *st);
int lstat(const char *p, void *st);
const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap);   /* posixmore.c */
int fstatat(int dfd, const char *p, void *st, int flag) {
    if (!p[0] && (flag & 0x400)) return fstat(dfd, st);  /* AT_EMPTY_PATH */
    if (!p[0]) return fstat(dfd, st);
    char t[1024];
    const char *q = cx_at_path(dfd, p, t, sizeof t);     /* AT_FDCWD (-2), absoluto ou relativo ao dirfd */
    if (!q) return -1;
    return (flag & 0x20) ? lstat(q, st) : stat(q, st);   /* AT_SYMLINK_NOFOLLOW */
}
void *readdir(void *d);
int readdir_r(void *d, void *entry, void **result) {
    void *e = readdir(d);
    if (!e) { *result = 0; return 0; }
    memcpy(entry, e, 1048);
    *result = entry;
    return 0;
}
