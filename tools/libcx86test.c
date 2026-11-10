/* S09 — %Lf, NSInvocation/SysV com 16 argumentos e qsort_r/qsort_b.
 * Host (x86_64 System V, o mesmo long double de 16 bytes do macOS):
 *   # cópia fora de libsystem: #include "sys.h" não pode achar o sys.h de produção ao lado do fonte
 *   cp userland/libsystem/stdio.c /tmp/mos-stdio.c && cp userland/libsystem/sortb.c /tmp/mos-sortb.c
 *   gcc -std=c11 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -fno-builtin \
 *       -ffunction-sections -fdata-sections -I tools/hostshim -I userland/libsystem \
 *       -c /tmp/mos-stdio.c -o /tmp/stdio.o
 *   gcc -std=c11 -Wall -Wno-unused-function -ffunction-sections -I tools/hostshim -I userland/libsystem \
 *       -c /tmp/mos-sortb.c -o /tmp/sortb.o
 *   gcc -std=c11 -Wall -ffunction-sections -I userland/libsystem \
 *       -c userland/libsystem/ffi_x64.c -o /tmp/ffi.o
 *   gcc -c userland/libsystem/ffi_x64.S -o /tmp/ffi_s.o
 *   gcc -std=c11 -Wall -Wextra -fno-builtin -ffunction-sections -I userland/libsystem \
 *       -c tools/libcx86test.c -o /tmp/t.o
 *   gcc -Wl,--gc-sections -Wl,--allow-multiple-definition \
 *       /tmp/stdio.o /tmp/sortb.o /tmp/ffi.o /tmp/ffi_s.o /tmp/t.o -o /tmp/libcx86test -lm
 * Não é o build do mOS nem a VM. Confere o código que o libSystem x86 executa.
 */
#define _POSIX_C_SOURCE 200809L
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ffi_x64.h"

int snprintf(char *b, unsigned long n, const char *fmt, ...);
int sscanf(const char *s, const char *fmt, ...);
void qsort_r(void *base, unsigned long n, unsigned long sz, void *thunk, int (*cmp)(void *, const void *, const void *));
void qsort_b(void *base, unsigned long n, unsigned long sz, void *block);

int cx_errno;
const char *__progname = "libcx86test";

static int checks, fails;
static void wr(int fd, const char *s) {
    unsigned long n = 0;
    while (s[n]) n++;
    if (n) write(fd, s, n);
}
static void fail(int line, const char *msg) {
    char b[32];
    int i = 0, n = line;
    char tmp[16];
    fails++;
    wr(2, "FAIL linha ");
    if (!n) tmp[i++] = '0';
    while (n) { tmp[i++] = (char)('0' + n % 10); n /= 10; }
    while (i) b[n++] = tmp[--i];
    b[n] = 0;
    wr(2, b);
    wr(2, " ");
    wr(2, msg);
    wr(2, "\n");
}
#define CHECK(c) do { checks++; if (!(c)) fail(__LINE__, #c); } while (0)

static void on_alarm(int sig) {
    (void)sig;
    wr(2, "TIMEOUT\n");
    _exit(2);
}

/* ---- %Lf ---- */
static void test_printf(void) {
    char b[128];
    int n;
    long double ld;
    double d;
    float f;
    CHECK(sizeof(long double) == 16);
    n = snprintf(b, sizeof b, "%.2Lf|%d|%s", (long double)2.5, 42, "fim");
    CHECK(n == 11 && strcmp(b, "2.50|42|fim") == 0);
    /* 6 inteiros enchem os registradores; o long double e o 7º inteiro vão para a pilha */
    n = snprintf(b, sizeof b, "%d%d%d%d%d%d %.1Lf %d", 1, 2, 3, 4, 5, 6, (long double)8.5, 9);
    CHECK(n > 0 && strcmp(b, "123456 8.5 9") == 0);
    n = snprintf(b, sizeof b, "%.1Lf", (long double)-2.5);
    CHECK(strcmp(b, "-2.5") == 0);
    n = snprintf(b, sizeof b, "%Ld", 1234567890123LL);
    CHECK(strcmp(b, "1234567890123") == 0);
    /* prova de que o snprintf ligado é o do stdio.c, não o da glibc */
    n = snprintf(b, sizeof b, "%@", (void *)0);
    CHECK(strcmp(b, "(objeto)") == 0);
    ld = 999;
    n = -1;
    CHECK(sscanf("1.25 7", "%Lf %d", &ld, &n) == 2 && ld == 1.25L && n == 7);
    d = 0; f = 0; n = 0;
    CHECK(sscanf("1.5 2.25 3", "%lf %f %d", &d, &f, &n) == 3 && d == 1.5 && f == 2.25f && n == 3);
}

/* ---- SysV / NSInvocation ---- */
static double m_mix(void *self, void *sel, char c, short s, int i, long l, long l2, short s2, char c2, int i2,
                    float fv, double d1, double d2, double d3, double d4, double d5) {
    (void)self; (void)sel;
    return c + s * 10.0 + i * 100.0 + l * 1000.0 + l2 * 1e4 + s2 * 1e5 + c2 * 1e6 + i2 * 1e7
        + fv + d1 / 2 + d2 / 4 + d3 / 8 + d4 / 16 + d5 / 32;
}
static double m_sum9(void *self, void *sel, double a, double b, double c, double d, double e, double f, double g, double h, double i) {
    (void)self; (void)sel;
    return a + b + c + d + e + f + g + h + i;
}
static float m_twice(void *self, void *sel, float x) { (void)self; (void)sel; return x * 2.f; }
static int m_add(void *self, void *sel, int a, int b) { (void)self; (void)sel; return a + b; }
typedef struct { long a, b; } Pair;
typedef struct { long a, b, c, d, e; } Big;
static Pair m_pair(void *self, void *sel, long k) { Pair p; (void)self; (void)sel; p.a = k; p.b = k + 1; return p; }
static Big m_big(void *self, void *sel, long k) {
    Big b; (void)self; (void)sel;
    b.a = k; b.b = k + 1; b.c = k + 2; b.d = k + 3; b.e = k + 4;
    return b;
}
static double m_ld(void *self, void *sel, long double x) { (void)self; (void)sel; return (double)x + 1.0; }

static void test_invoke(void) {
    void *self = (void *)1, *sel = (void *)2;
    char c = -3, c2 = 9;
    short s = 4, s2 = -8;
    int i = -5, i2 = 1;
    long l = 6, l2 = 7;
    float fv = 0.5f;
    double ds[5] = {1, 2, 3, 4, 5};
    const char *types[16] = {"@", ":", "c", "s", "i", "q", "q", "s", "c", "i", "f", "d", "d", "d", "d", "d"};
    const void *vals[16] = {&self, &sel, &c, &s, &i, &l, &l2, &s2, &c2, &i2, &fv, &ds[0], &ds[1], &ds[2], &ds[3], &ds[4]};
    double got = 0, exp;
    double nine[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    const char *t9[11] = {"@", ":", "d", "d", "d", "d", "d", "d", "d", "d", "d"};
    const void *v9[11];
    float tw = 0, x = 1.5f;
    int sum = 0, aa = 20, bb = 22;
    const char *ti[4] = {"@", ":", "i", "i"};
    const void *vi[4] = {&self, &sel, &aa, &bb};
    const char *tf[3] = {"@", ":", "f"};
    const void *vf[3] = {&self, &sel, &x};
    long k = 100;
    Pair pair;
    Big big;
    const char *tq[3] = {"@", ":", "q"};
    const void *vk[3] = {&self, &sel, &k};
    int u;
    exp = m_mix(self, sel, c, s, i, l, l2, s2, c2, i2, fv, ds[0], ds[1], ds[2], ds[3], ds[4]);
    cx_sysv_invoke((void *)m_mix, "d", 8, types, vals, 16, &got);
    CHECK(got == exp);
    v9[0] = &self; v9[1] = &sel;
    for (u = 0; u < 9; u++) v9[u + 2] = &nine[u];
    got = 0;
    cx_sysv_invoke((void *)m_sum9, "d", 8, t9, v9, 11, &got);
    CHECK(got == 45);
    cx_sysv_invoke((void *)m_twice, "f", 4, tf, vf, 3, &tw);
    CHECK(tw == 3.f);
    cx_sysv_invoke((void *)m_add, "i", 4, ti, vi, 4, &sum);
    CHECK(sum == 42);
    memset(&pair, 0, sizeof pair);
    cx_sysv_invoke((void *)m_pair, "{Pair=qq}", 16, tq, vk, 3, &pair);
    CHECK(pair.a == 100 && pair.b == 101);
    memset(&big, 0, sizeof big);
    cx_sysv_invoke((void *)m_big, "{Big=qqqqq}", 40, tq, vk, 3, &big);
    CHECK(big.a == 100 && big.d == 103 && big.e == 104);
    {
        long double ld = 2.5L;
        const char *tl[3] = {"@", ":", "D"};
        const void *vl[3] = {&self, &sel, &ld};
        got = 0;
        cx_sysv_invoke((void *)m_ld, "d", 8, tl, vl, 3, &got);
        CHECK(got == 3.5);
    }
}

/* ---- qsort_r / qsort_b ---- */
typedef struct { void *isa; int flags, reserved; int (*invoke)(void *, const void *, const void *); } blk_t;
static unsigned long comps;
static int cmp_asc(void *thunk, const void *a, const void *b) {
    (void)thunk;
    comps++;
    return *(const int *)a - *(const int *)b;
}
static int cmp_dir(void *thunk, const void *a, const void *b) {
    return *(int *)thunk * (*(const int *)a - *(const int *)b);
}
static jmp_buf jb;
static int inv_jump(void *blk, const void *a, const void *b) {
    (void)blk;
    if (*(const int *)a == 7 || *(const int *)b == 7) longjmp(jb, 1);
    return *(const int *)a - *(const int *)b;
}
static int inv_asc(void *blk, const void *a, const void *b) {
    (void)blk;
    return *(const int *)a - *(const int *)b;
}
static int inv_nest(void *blk, const void *a, const void *b) {
    int z[4] = {4, 1, 3, 2};
    blk_t inner;
    (void)blk;
    inner.isa = 0; inner.flags = 0; inner.reserved = 0; inner.invoke = inv_asc;
    qsort_b(z, 4, sizeof z[0], &inner);
    if (z[0] != 1 || z[3] != 4) return 0;
    return *(const int *)a - *(const int *)b;
}

static void test_sort(void) {
    enum { N = 4096 };
    int *a = malloc(N * sizeof(int));
    int dir = -1;
    int small[6] = {1, 4, 2, 8, 3, 7};
    int one = 42, calls_before;
    blk_t blk;
    int w[5] = {9, 1, 8, 2, 7};
    int jumped[5] = {1, 7, 3, 9, 2};
    int after[3] = {3, 1, 2};
    int i, ok;
    CHECK(a != 0);
    if (!a) return;
    for (i = 0; i < N; i++) a[i] = N - 1 - i;
    comps = 0;
    qsort_r(a, N, sizeof a[0], 0, cmp_asc);
    ok = a[0] == 0 && a[N - 1] == N - 1;
    for (i = 1; i < N; i++) if (a[i] < a[i - 1]) ok = 0;
    /* inserção reversa faria ~N²/2 comparações; heapsort fica bem abaixo de 64N */
    CHECK(ok && comps > 0 && comps < (unsigned long)N * 64);
    calls_before = (int)comps;
    qsort_r(&one, 0, sizeof one, 0, cmp_asc);
    qsort_r(&one, 1, sizeof one, 0, cmp_asc);
    CHECK(comps == (unsigned long)calls_before && one == 42);
    qsort_r(small, 6, sizeof small[0], &dir, cmp_dir);
    CHECK(small[0] == 8 && small[5] == 1);
    blk.isa = 0; blk.flags = 0; blk.reserved = 0; blk.invoke = inv_nest;
    qsort_b(w, 5, sizeof w[0], &blk);
    CHECK(w[0] == 1 && w[4] == 9);
    blk.invoke = inv_jump;
    {
        int saiu = 0;
        if (setjmp(jb) == 0) qsort_b(jumped, 5, sizeof jumped[0], &blk);
        else saiu = 1;
        CHECK(saiu == 1);
    }
    blk.invoke = inv_asc;
    qsort_b(after, 3, sizeof after[0], &blk);
    CHECK(after[0] == 1 && after[2] == 3);
    free(a);
}

int main(void) {
    signal(SIGALRM, on_alarm);
    alarm(5);
    test_printf();
    test_invoke();
    test_sort();
    alarm(0);
    if (fails) {
        wr(2, "libcx86test: falhou\n");
        return 1;
    }
    {
        char msg[64];
        int n = checks, i = 0, o = 0;
        char dig[16];
        wr(1, "libcx86test: ");
        if (!n) dig[i++] = '0';
        while (n) { dig[i++] = (char)('0' + n % 10); n /= 10; }
        while (i) msg[o++] = dig[--i];
        msg[o] = 0;
        wr(1, msg);
        wr(1, "/");
        wr(1, msg);
        wr(1, " PASS\n");
    }
    return 0;
}
