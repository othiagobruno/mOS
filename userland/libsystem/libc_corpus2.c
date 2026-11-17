/*
 * mOS libSystem — mais símbolos do corpus (docs/lacunas/leonardo.md):
 *   - complex: cexp, clog, cpow, catanh (+ f/l) — blender, inkscape, kicad, lmms, tic80
 *   - OSAtomicEnqueue/OSAtomicDequeue (pilha LIFO sem trava) — bettertouchtool, hex-fiend, teams
 *   - tmpnam (nome de arquivo temporário) — calibre, chrome, intellij
 * Entra no libSystem.mos pelo glob de tools/build.sh. Teste de host: tools/libc_corpus2_hosttest.sh
 */
#include "sys.h"

/* ---------------------------------------------------------------- complex (núcleo double, como csqrt) */
double exp(double), log(double), sin(double), cos(double), atan2(double, double), hypot(double, double);
long double expl(long double), logl(long double), sinl(long double), cosl(long double),
            atan2l(long double, long double), hypotl(long double, long double);

double _Complex cexp(double _Complex z) {
    double x = __real__ z, y = __imag__ z, e = exp(x);
    double _Complex r; __real__ r = e * cos(y); __imag__ r = e * sin(y); return r;
}
double _Complex clog(double _Complex z) {
    double x = __real__ z, y = __imag__ z;
    double _Complex r; __real__ r = log(hypot(x, y)); __imag__ r = atan2(y, x); return r;
}
/* multiplicação complexa auxiliar */
static double _Complex cmul(double _Complex a, double _Complex b) {
    double ar = __real__ a, ai = __imag__ a, br = __real__ b, bi = __imag__ b;
    double _Complex r; __real__ r = ar * br - ai * bi; __imag__ r = ar * bi + ai * br; return r;
}
double _Complex cpow(double _Complex z, double _Complex w) { return cexp(cmul(w, clog(z))); }
/* catanh(z) = 0.5 * (clog(1+z) - clog(1-z)) */
double _Complex catanh(double _Complex z) {
    double _Complex one; __real__ one = 1; __imag__ one = 0;
    double _Complex a; __real__ a = 1 + __real__ z; __imag__ a = __imag__ z;   /* 1+z */
    double _Complex b; __real__ b = 1 - __real__ z; __imag__ b = -__imag__ z;  /* 1-z */
    (void)one;
    double _Complex la = clog(a), lb = clog(b), r;
    __real__ r = 0.5 * (__real__ la - __real__ lb);
    __imag__ r = 0.5 * (__imag__ la - __imag__ lb);
    return r;
}
/* variantes float: calculam em double e truncam (como csqrtf) */
static float _Complex to_f(double _Complex d) { float _Complex o; __real__ o = (float)__real__ d; __imag__ o = (float)__imag__ d; return o; }
static double _Complex from_f(float _Complex f) { double _Complex o; __real__ o = (double)__real__ f; __imag__ o = (double)__imag__ f; return o; }
float _Complex cexpf(float _Complex z) { return to_f(cexp(from_f(z))); }
float _Complex clogf(float _Complex z) { return to_f(clog(from_f(z))); }
float _Complex cpowf(float _Complex z, float _Complex w) { return to_f(cpow(from_f(z), from_f(w))); }
float _Complex catanhf(float _Complex z) { return to_f(catanh(from_f(z))); }
/* variantes long double: math de long double de verdade */
long double _Complex cexpl(long double _Complex z) {
    long double x = __real__ z, y = __imag__ z, e = expl(x);
    long double _Complex r; __real__ r = e * cosl(y); __imag__ r = e * sinl(y); return r;
}
long double _Complex clogl(long double _Complex z) {
    long double x = __real__ z, y = __imag__ z;
    long double _Complex r; __real__ r = logl(hypotl(x, y)); __imag__ r = atan2l(y, x); return r;
}
static long double _Complex cmull(long double _Complex a, long double _Complex b) {
    long double ar = __real__ a, ai = __imag__ a, br = __real__ b, bi = __imag__ b;
    long double _Complex r; __real__ r = ar * br - ai * bi; __imag__ r = ar * bi + ai * br; return r;
}
long double _Complex cpowl(long double _Complex z, long double _Complex w) { return cexpl(cmull(w, clogl(z))); }
long double _Complex catanhl(long double _Complex z) {
    long double _Complex a; __real__ a = 1 + __real__ z; __imag__ a = __imag__ z;
    long double _Complex b; __real__ b = 1 - __real__ z; __imag__ b = -__imag__ z;
    long double _Complex la = clogl(a), lb = clogl(b), r;
    __real__ r = 0.5L * (__real__ la - __real__ lb);
    __imag__ r = 0.5L * (__imag__ la - __imag__ lb);
    return r;
}

/* ---------------------------------------------------------------- OSAtomic: pilha LIFO sem trava */
/* OSQueueHead = { void *head; long count }. O ponteiro "próximo" fica em (elem + offset). CAS no ponteiro
 * da cabeça (o uso do mOS não sofre ABA; sem cmpxchg16b). */
void OSAtomicEnqueue(void *list, void *elem, size_t offset) {
    void *volatile *head = (void *volatile *)list;
    void *old;
    do {
        old = __atomic_load_n(head, __ATOMIC_RELAXED);
        *(void **)((char *)elem + offset) = old;
    } while (!__atomic_compare_exchange_n(head, &old, elem, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED));
}
void *OSAtomicDequeue(void *list, size_t offset) {
    void *volatile *head = (void *volatile *)list;
    void *old, *next;
    do {
        old = __atomic_load_n(head, __ATOMIC_ACQUIRE);
        if (!old) return 0;
        next = *(void **)((char *)old + offset);
    } while (!__atomic_compare_exchange_n(head, &old, next, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
    return old;
}

/* ---------------------------------------------------------------- tmpnam (<stdio.h>) */
int getpid(void);
u64 mach_absolute_time(void);
static char tmpnam_buf[1024];   /* L_tmpnam = 1024 no macOS */
char *tmpnam(char *s) {
    static unsigned ctr;
    char *b = s ? s : tmpnam_buf;
    unsigned n = __atomic_add_fetch(&ctr, 1, __ATOMIC_RELAXED);
    u64 mix = (u64)getpid() * 2654435761ull ^ (mach_absolute_time() + n * 0x9E3779B1u);
    const char *pfx = "/tmp/tmp.";
    int i = 0; while (pfx[i]) { b[i] = pfx[i]; i++; }
    /* 12 dígitos hex de mix */
    for (int k = 44; k >= 0; k -= 4) { unsigned d = (unsigned)((mix >> k) & 0xF); b[i++] = d < 10 ? '0' + d : 'a' + d - 10; }
    b[i] = 0;
    return b;
}
