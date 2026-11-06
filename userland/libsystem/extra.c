/*
 * mOS libSystem — complementos: libm estendida, pthreads de um processo com uma thread,
 * memmem, dirname/basename, realpath, strptime, pathconf.
 */
#include "sys.h"
#include "libc.h"

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
static int cpu_has_fsgsbase(void) {
    u32 a = 7, b = 0, c = 0, d = 0;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    return b & 1;
}
/* tabela da thread atual (cria e liga ao GS na primeira vez) */
void **cx_tsd(void) {
    if (gs_ok < 0) gs_ok = cpu_has_fsgsbase();
    if (!gs_ok) return tsd_global;
    u64 base;
    __asm__ volatile("rdgsbase %0" : "=r"(base));
    if (!base) {
        void **t = calloc(TSD_SLOTS, sizeof(void *));
        if (!t) return tsd_global;
        base = (u64)t;
        __asm__ volatile("wrgsbase %0" :: "r"(base) : "memory");
        t[0] = pthread_self();
    }
    return (void **)base;
}
int pthread_key_create(pthread_key_t *k, void (*dtor)(void *)) {
    (void)dtor;
    for (int i = KEY_FIRST; i < TSD_SLOTS; i++)
        if (!__sync_lock_test_and_set(&key_used[i], 1)) { cx_tsd()[i] = 0; *k = (pthread_key_t)i; return 0; }
    return 35; /* EAGAIN */
}
int pthread_key_delete(pthread_key_t k) { if (k < TSD_SLOTS) key_used[k] = 0; return 0; }
void *pthread_getspecific(pthread_key_t k) { return k < TSD_SLOTS ? cx_tsd()[k] : 0; }
int pthread_setspecific(pthread_key_t k, const void *v) { if (k >= TSD_SLOTS) return CX_EINVAL; cx_tsd()[k] = (void *)v; return 0; }
typedef struct { long sig; char opaque[8]; } pthread_once_t;
int pthread_once(pthread_once_t *o, void (*fn)(void)) {
    if (o->opaque[0] == 0x5A) return 0;
    o->opaque[0] = 0x5A;
    fn();
    return 0;
}
/* threads de verdade: SYS_THREAD_CREATE com pilha própria (mmap) */
typedef struct { void *(*fn)(void *); void *arg; } cx_tstart_t;
static __attribute__((noreturn)) void cx_thread_entry(cx_tstart_t *ts) {
    void *(*fn)(void *) = ts->fn;
    void *arg = ts->arg;
    free(ts);
    cx_tsd();                                   /* tabela TSD própria desta thread (GS) */
    void *r = fn(arg);
    SYS1(SYS_THREAD_EXIT, (long)r);
    for (;;) {}
}
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg) {
    (void)attr;
    size_t sz = 512 << 10;
    char *stack = mmap(0, sz, 3, 0x1002, -1, 0);
    if (stack == (char *)-1) return 35;
    cx_tstart_t *ts = malloc(sizeof *ts);
    if (!ts) return 35;
    ts->fn = fn;
    ts->arg = arg;
    u64 top = ((u64)stack + sz) & ~15ULL;
    top -= 8;                                   /* rsp = 16n - 8, como logo após um call */
    *(u64 *)top = 0;
    long tid = SYS3(SYS_THREAD_CREATE, cx_thread_entry, top, ts);
    if (tid < 0) { free(ts); return 35; }
    cx_fdprintf(2, "[thread] %ld: entrada %p, arg %p\n", tid, (void *)fn, arg);   /* diagnóstico temporário (porte do VS Code) */
    *t = (unsigned long)tid;
    return 0;
}
int pthread_join(unsigned long t, void **ret) {
    long r = SYS1(SYS_THREAD_JOIN, t);
    if (ret) *ret = (void *)r;
    return 0;
}
int pthread_detach(unsigned long t) { (void)t; return 0; }static char main_thread[8192];
void *pthread_self(void) { long tid = SYS0(SYS_GETTID); return tid > 1 ? (void *)(main_thread + (tid & 0xFFF)) : main_thread; }
int pthread_equal(void *a, void *b) { return a == b; }
/* thread principal = a que rodou cx_darwin_init (cx_main_tid, dispatch.c); antes disso, ou em apps que
 * não passam por ele, toda thread responde 1 como antes (feito pela sessão do VS Code, com o ok da DMG) */
extern int cx_main_tid;
int pthread_main_np(void) { return !cx_main_tid || (int)SYS0(SYS_GETTID) == cx_main_tid; }
/* pthread_mutex_t do macOS tem 64 bytes ({long sig; char opaque[56]}): usamos o primeiro int do opaque */
static volatile int *mx(void *m) { return (volatile int *)((char *)m + 8); }
int pthread_mutex_init(void *m, const void *a) { (void)a; *mx(m) = 0; return 0; }
int pthread_mutex_destroy(void *m) { (void)m; return 0; }
int pthread_mutex_lock(void *m) { while (__sync_lock_test_and_set(mx(m), 1)) SYS0(SYS_YIELD); return 0; }
int pthread_mutex_trylock(void *m) { return __sync_lock_test_and_set(mx(m), 1) ? 16 : 0; }
int pthread_mutex_unlock(void *m) { __sync_lock_release(mx(m)); return 0; }
int pthread_mutexattr_init(void *a) { (void)a; return 0; }
int pthread_mutexattr_settype(void *a, int t) { (void)a; (void)t; return 0; }
int pthread_mutexattr_destroy(void *a) { (void)a; return 0; }
int pthread_cond_init(void *c, const void *a) { (void)c; (void)a; return 0; }
int pthread_cond_destroy(void *c) { (void)c; return 0; }
static volatile int *cv(void *c) { return (volatile int *)((char *)c + 8); }
int pthread_cond_signal(void *c) { __sync_fetch_and_add(cv(c), 1); return 0; }
int pthread_cond_broadcast(void *c) { __sync_fetch_and_add(cv(c), 1); return 0; }
int pthread_cond_wait(void *c, void *m) {
    int seq = *cv(c);
    pthread_mutex_unlock(m);
    while (*cv(c) == seq) SYS0(SYS_YIELD);
    return pthread_mutex_lock(m);
}
/* espera com prazo: rel_ms < 0 = sem prazo; devolve ETIMEDOUT (60) se o prazo vencer */
static int cond_wait_ms(void *c, void *m, long rel_ms) {
    int seq = *cv(c);
    u64 end = (u64)SYS0(SYS_TIME) + (u64)(rel_ms < 0 ? 0 : rel_ms);
    pthread_mutex_unlock(m);
    int r = 0;
    while (*cv(c) == seq) {
        if (rel_ms >= 0 && (u64)SYS0(SYS_TIME) >= end) { r = 60; break; }
        SYS0(SYS_YIELD);
    }
    pthread_mutex_lock(m);
    return r;
}
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
int pthread_rwlock_rdlock(void *l) { (void)l; return 0; }
int pthread_rwlock_wrlock(void *l) { (void)l; return 0; }
int pthread_rwlock_unlock(void *l) { (void)l; return 0; }
int pthread_attr_init(void *a) { (void)a; return 0; }
int pthread_attr_destroy(void *a) { (void)a; return 0; }
int pthread_attr_setstacksize(void *a, size_t s) { (void)a; (void)s; return 0; }
size_t pthread_get_stacksize_np(void *t) { (void)t; return 256 << 10; }
void *pthread_get_stackaddr_np(void *t) { (void)t; return (void *)0x400040000000ULL; }
int pthread_setname_np(const char *n) { cx_fdprintf(2, "[thread] %ld chama-se \"%s\"\n", SYS0(SYS_GETTID), n ? n : ""); return 0; }   /* diagnóstico temporário */
int pthread_threadid_np(void *t, u64 *id) { (void)t; if (id) *id = 1; return 0; }
void os_unfair_lock_lock(void *l) { (void)l; }
void os_unfair_lock_unlock(void *l) { (void)l; }
int os_unfair_lock_trylock(void *l) { (void)l; return 1; }
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
char *realpath(const char *path, char *out) {
    char tmp[1024], res[1024];
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
        else if (o + n + 2 < sizeof res) { res[o++] = '/'; memcpy(res + o, s, n); o += n; }
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
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    /* MAP_FIXED sobre a pilha (página de guarda do Rust, por exemplo): a região já existe */
    if ((flags & 0x10) && addr && (u64)addr >= 0x400000000000ULL && (u64)addr < 0x400040000000ULL) return addr;
    /* MAP_FIXED|MAP_ANON dentro da área de mmap (ex.: trecho de uma reserva do PartitionAlloc):
     * troca o trecho por páginas zeradas, ou só devolve as páginas se for PROT_NONE */
    if ((flags & 0x1010) == 0x1010 && addr && (u64)addr >= CX_MMAP_BASE) {
        SYS2(SYS_MUNMAP, addr, len);
        if (prot) { long c = SYS3(SYS_MMAP, len, 2, addr); if (c < 0) { cx_errno = (int)-c; return (void *)-1; } }
        return addr;
    }
    /* PROT_NONE anônimo = só reservar endereços (as páginas vêm no mprotect) */
    long r = SYS2(SYS_MMAP, len, (!prot && (flags & 0x1000)) ? 1 : 0);
    if (r < 0) { cx_errno = (int)-r; return (void *)-1; }
    if (!(flags & 0x1000) && fd >= 0) {           /* MAP_ANON = 0x1000; senão: copia o arquivo */
        off_t cur = lseek(fd, 0, 1);
        lseek(fd, off, 0);
        size_t got = 0;
        while (got < len) { ssize_t k = read(fd, (char *)r + got, len - got); if (k <= 0) break; got += (size_t)k; }
        lseek(fd, cur, 0);
    }
    return (void *)r;
}
int munmap(void *a, size_t len) { return SYS2(SYS_MUNMAP, a, len) < 0 ? -1 : 0; }
int mprotect(void *a, size_t len, int prot) {
    /* liberar acesso a um trecho reservado aloca as páginas que faltam (as existentes ficam) */
    if (prot && len && (u64)a >= CX_MMAP_BASE) {
        long r = SYS3(SYS_MMAP, len, 2, a);
        if (r < 0) { cx_errno = (int)-r; return -1; }
    }
    return 0;
}
int madvise(void *a, size_t len, int adv) { (void)a; (void)len; (void)adv; return 0; }
int posix_madvise(void *a, size_t len, int adv) { (void)a; (void)len; (void)adv; return 0; }
int mlock(const void *a, size_t len) { (void)a; (void)len; return 0; }

/* aleatoriedade (CommonCrypto faz parte do libSystem) */
int CCRandomGenerateBytes(void *buf, size_t n) { arc4random_buf(buf, n); return 0; }
int getentropy(void *buf, size_t n) { arc4random_buf(buf, n); return 0; }
int SecRandomCopyBytes(void *rnd, size_t n, void *buf) { (void)rnd; arc4random_buf(buf, n); return 0; }

/* libdispatch: semáforos de um processo com uma thread */
typedef struct { long count; } cx_sema_t;
void *dispatch_semaphore_create(long v) { cx_sema_t *s = malloc(sizeof *s); if (s) s->count = v; return s; }
long dispatch_semaphore_signal(void *s) { ((cx_sema_t *)s)->count++; return 0; }
long dispatch_semaphore_wait(void *s, u64 timeout) {
    cx_sema_t *x = s;
    if (x->count > 0) { x->count--; return 0; }
    (void)timeout;
    return 1;   /* nenhuma outra thread pode sinalizar: equivale a esgotar o tempo */
}
u64 dispatch_time(u64 when, long delta) { return when + (u64)delta; }
void dispatch_release(void *o) { free(o); }
void dispatch_retain(void *o) { (void)o; }

/* unwinder: só usado em panics; sem tabelas, informa "fim da pilha" */
int _Unwind_Backtrace(void *fn, void *arg) { (void)fn; (void)arg; return 5; }
u64 _Unwind_GetIP(void *c) { (void)c; return 0; }
u64 _Unwind_GetIPInfo(void *c, int *before) { (void)c; if (before) *before = 0; return 0; }
u64 _Unwind_GetCFA(void *c) { (void)c; return 0; }
u64 _Unwind_GetDataRelBase(void *c) { (void)c; return 0; }
u64 _Unwind_GetTextRelBase(void *c) { (void)c; return 0; }
u64 _Unwind_GetLanguageSpecificData(void *c) { (void)c; return 0; }
u64 _Unwind_GetRegionStart(void *c) { (void)c; return 0; }
void _Unwind_SetGR(void *c, int i, u64 v) { (void)c; (void)i; (void)v; }
void _Unwind_SetIP(void *c, u64 v) { (void)c; (void)v; }
int _Unwind_RaiseException(void *e) { (void)e; return 5; }
void _Unwind_DeleteException(void *e) { (void)e; }
__attribute__((noreturn)) void _Unwind_Resume(void *e) { (void)e; cx_puts_fd(2, "panic sem tabela de unwind: abortando\n"); abort(); }

/* processos, usuários e sistema */
int sched_yield(void) { SYS0(SYS_YIELD); return 0; }
int pause(void) { for (;;) SYS1(SYS_SLEEP, 1000); }
int waitpid(int pid, int *st, int opt) {
    (void)opt;
    long r = SYS1(SYS_WAIT, pid);
    if (r < -255) { cx_errno = 10; return -1; }   /* ECHILD */
    if (st) *st = (int)(r & 0xFF) << 8;
    return pid;
}
int wait(int *st) { (void)st; cx_errno = 10; return -1; }
int setuid(unsigned u) { (void)u; return 0; }
int setgid(unsigned g) { (void)g; return 0; }
int setgroups(int n, const unsigned *g) { (void)n; (void)g; return 0; }
int setpgid(int a, int b) { (void)a; (void)b; return 0; }
int setsid(void) { return 1; }
int chroot(const char *p) { (void)p; cx_errno = CX_EPERM; return -1; }
int sigaltstack(const void *ss, void *old) { (void)ss; (void)old; return 0; }
int posix_spawn_file_actions_init(void *a) { (void)a; return 0; }
int posix_spawn_file_actions_destroy(void *a) { (void)a; return 0; }
int posix_spawn_file_actions_adddup2(void *a, int x, int y) { (void)a; (void)x; (void)y; return 0; }
int posix_spawn_file_actions_addclose(void *a, int x) { (void)a; (void)x; return 0; }
int posix_spawnattr_init(void *a) { (void)a; return 0; }
int posix_spawnattr_destroy(void *a) { (void)a; return 0; }
int posix_spawnattr_setflags(void *a, short f) { (void)a; (void)f; return 0; }
int posix_spawnattr_setpgroup(void *a, int g) { (void)a; (void)g; return 0; }
int posix_spawnattr_setsigdefault(void *a, const void *s) { (void)a; (void)s; return 0; }
int posix_spawnattr_setsigmask(void *a, const void *s) { (void)a; (void)s; return 0; }
int posix_spawnp(int *pid, const char *f, const void *fa, const void *at, char *const *av, char *const *ev) {
    (void)fa; (void)at; (void)ev;
    char args[2048] = "";
    for (int i = 1; av && av[i]; i++) {
        strlcat(args, i > 1 ? " \"" : "\"", sizeof args);
        strlcat(args, av[i], sizeof args);
        strlcat(args, "\"", sizeof args);
    }
    char path[1024];
    cx_path(f, path, sizeof path);
    long r = SYS2(SYS_SPAWN, f[0] == '/' || strchr(f, '/') ? path : f, args);
    if (r < 0) return CX_ENOENT;
    if (pid) *pid = (int)r;
    return 0;
}
int posix_spawn(int *pid, const char *f, const void *fa, const void *at, char *const *av, char *const *ev) {
    return posix_spawnp(pid, f, fa, at, av, ev);
}

typedef struct { char sysname[256], nodename[256], release[256], version[256], machine[256]; } mac_utsname_t;
int uname(mac_utsname_t *u) {
    memset(u, 0, sizeof *u);
    strcpy(u->sysname, "Darwin");
    strcpy(u->nodename, "mos");
    strcpy(u->release, "24.0.0");
    strcpy(u->version, "mOS Nucleo (compatibilidade Darwin)");
    strcpy(u->machine, "x86_64");
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
ssize_t pread(int fd, void *b, size_t n, off_t off) { off_t c = lseek(fd, 0, 1); lseek(fd, off, 0); ssize_t r = read(fd, b, n); lseek(fd, c, 0); return r; }
ssize_t pwrite(int fd, const void *b, size_t n, off_t off) { off_t c = lseek(fd, 0, 1); lseek(fd, off, 0); ssize_t r = write(fd, b, n); lseek(fd, c, 0); return r; }

typedef struct { char path[1024]; long index; } cx_dir_hdr_t;   /* prefixo de cx_dir_t (libc.c) */
int dirfd(void *d) { (void)d; return -1; }
int stat(const char *p, void *st);
int fstat(int fd, void *st);
int fstatat(int dfd, const char *p, void *st, int flag) {
    (void)flag;
    if (p[0] == '/' || dfd == -2) return stat(p, st);   /* AT_FDCWD = -2 */
    if (!p[0]) return fstat(dfd, st);
    return stat(p, st);
}
void *readdir(void *d);
int readdir_r(void *d, void *entry, void **result) {
    void *e = readdir(d);
    if (!e) { *result = 0; return 0; }
    memcpy(entry, e, 1048);
    *result = entry;
    return 0;
}
