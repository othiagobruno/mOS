#define _GNU_SOURCE
/*
 * mOS — teste de host de userland/libsystem/libc_corpus2.c (prefixo mos_).
 * complex (cexp/clog/cpow/catanh + f/l) comparado à glibc com tolerância; OSAtomicEnqueue/Dequeue LIFO; tmpnam único.
 * Rodar: tools/libc_corpus2_hosttest.sh
 */
#include <stdio.h>
#include <string.h>
#include <complex.h>
#include <math.h>
#include <stdint.h>

/* mach_absolute_time não existe no host: stub para o tmpnam do mOS */
unsigned long long mach_absolute_time(void) { static unsigned long long t; return ++t * 1000003ull; }

double _Complex mos_cexp(double _Complex);
double _Complex mos_clog(double _Complex);
double _Complex mos_cpow(double _Complex, double _Complex);
double _Complex mos_catanh(double _Complex);
float _Complex mos_cexpf(float _Complex);
float _Complex mos_clogf(float _Complex);
float _Complex mos_cpowf(float _Complex, float _Complex);
float _Complex mos_catanhf(float _Complex);
long double _Complex mos_cexpl(long double _Complex);
long double _Complex mos_clogl(long double _Complex);
long double _Complex mos_cpowl(long double _Complex, long double _Complex);
long double _Complex mos_catanhl(long double _Complex);
void mos_OSAtomicEnqueue(void *, void *, size_t);
void *mos_OSAtomicDequeue(void *, size_t);
char *mos_tmpnam(char *);

static int fails = 0, total = 0;
#define CK(cond, msg) do { total++; if (!(cond)) { fails++; printf("FALHA: %s\n", msg); } } while (0)

static int closed(double _Complex a, double _Complex b, double tol) {
    double d = cabs(a - b), m = cabs(b);
    return d <= tol * (m > 1 ? m : 1);
}
static int closedl(long double _Complex a, long double _Complex b, long double tol) {
    long double d = cabsl(a - b), m = cabsl(b);
    return d <= tol * (m > 1 ? m : 1);
}

int main(void) {
    double _Complex pts[] = { 0.3 + 0.4*I, -1.2 + 0.7*I, 2.0 - 1.5*I, 0.1 + 0.0*I, 0.0 + 1.3*I, -0.5 - 0.5*I };
    for (int i = 0; i < 6; i++) {
        double _Complex z = pts[i];
        CK(closed(mos_cexp(z), cexp(z), 1e-12), "cexp");
        CK(closed(mos_clog(z), clog(z), 1e-12), "clog");
        CK(closed(mos_catanh(z), catanh(z), 1e-11), "catanh");
        double _Complex w = 0.5 + 0.2*I;
        CK(closed(mos_cpow(z, w), cpow(z, w), 1e-10), "cpow");
        /* float */
        float _Complex zf = (float)creal(z) + (float)cimag(z)*I;
        CK(closed(mos_cexpf(zf), cexpf(zf), 1e-5), "cexpf");
        CK(closed(mos_clogf(zf), clogf(zf), 1e-5), "clogf");
        CK(closed(mos_catanhf(zf), catanhf(zf), 1e-4), "catanhf");
        /* long double */
        long double _Complex zl = (long double)creal(z) + (long double)cimag(z)*I;
        CK(closedl(mos_cexpl(zl), cexpl(zl), 1e-15L), "cexpl");
        CK(closedl(mos_clogl(zl), clogl(zl), 1e-15L), "clogl");
        CK(closedl(mos_catanhl(zl), catanhl(zl), 1e-13L), "catanhl");
    }

    /* ---- OSAtomicEnqueue/Dequeue: LIFO ---- */
    {
        struct node { long v; void *next; };
        void *q = 0;  /* OSQueueHead: só a cabeça importa aqui */
        struct node a = {1, 0}, b = {2, 0}, c = {3, 0};
        size_t off = __builtin_offsetof(struct node, next);
        mos_OSAtomicEnqueue(&q, &a, off);
        mos_OSAtomicEnqueue(&q, &b, off);
        mos_OSAtomicEnqueue(&q, &c, off);
        struct node *x = mos_OSAtomicDequeue(&q, off); CK(x == &c, "dequeue c");
        x = mos_OSAtomicDequeue(&q, off); CK(x == &b, "dequeue b");
        x = mos_OSAtomicDequeue(&q, off); CK(x == &a, "dequeue a");
        x = mos_OSAtomicDequeue(&q, off); CK(x == 0, "dequeue vazio -> NULL");
    }

    /* ---- tmpnam: não-nulo, prefixo /tmp/, únicos ---- */
    {
        char buf[1024]; char *r1 = mos_tmpnam(buf);
        CK(r1 == buf && strncmp(r1, "/tmp/", 5) == 0, "tmpnam com buffer");
        char first[1024]; strcpy(first, r1);
        char *r2 = mos_tmpnam(0);
        CK(r2 && strncmp(r2, "/tmp/", 5) == 0 && strcmp(r2, first) != 0, "tmpnam interno único");
    }

    printf("%d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
