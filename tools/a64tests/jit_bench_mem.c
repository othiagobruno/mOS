/* frente JIT: benchmark de memória e strings (cópia por palavras com pares, cópia por bytes,
 * strlen, memcmp, memset). Uso: jit_bench_mem.elf [N] (padrão 2000). Imprime um hash. */
#include "a64t.h"
static unsigned long parse(const char *s) { unsigned long v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + (unsigned long)(*s++ - '0'); return v; }
static unsigned long long a[4096], b[4096];
static char s1[8192], s2[8192];
__attribute__((noinline)) static void cpy64(unsigned long long *d, const unsigned long long *s, unsigned long n) {
    for (unsigned long i = 0; i + 4 <= n; i += 4) {
        unsigned long long x0 = s[i], x1 = s[i + 1], x2 = s[i + 2], x3 = s[i + 3];
        d[i] = x0; d[i + 1] = x1; d[i + 2] = x2; d[i + 3] = x3;
    }
}
__attribute__((noinline)) static void cpy8(char *d, const char *s, unsigned long n) { while (n--) *d++ = *s++; }
__attribute__((noinline)) static unsigned long slen(const char *s) { const char *p = s; while (*p) p++; return (unsigned long)(p - s); }
__attribute__((noinline)) static int mcmp(const char *x, const char *y, unsigned long n) {
    for (unsigned long i = 0; i < n; i++) if (x[i] != y[i]) return (unsigned char)x[i] - (unsigned char)y[i];
    return 0;
}
__attribute__((noinline)) static void mset(char *d, int c, unsigned long n) { for (unsigned long i = 0; i < n; i++) d[i] = (char)c; }
int main(int argc, char **argv) {
    unsigned long n = argc > 1 ? parse(argv[1]) : 2000;
    unsigned long long h = 0;
    for (unsigned i = 0; i < 4096; i++) a[i] = i * 0x9E3779B97F4A7C15ull;
    for (unsigned long r = 0; r < n; r++) {
        a[r & 4095] ^= r;
        cpy64(b, a, 4096);
        h = h * 33 + b[(r * 7) & 4095];
        mset(s1, 'a' + (int)(r % 26), 8191);
        s1[8191] = 0;
        cpy8(s2, s1, 8192);
        s2[4000 + r % 100] = 'z';
        h = h * 33 + slen(s2) + (unsigned long)mcmp(s1, s2, 8192);
    }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
