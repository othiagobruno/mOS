/* frente JIT: benchmark de inteiros (xorshift + multiplicação + desvios, crivo, ordenação por
 * inserção). Uso: jit_bench_int.elf [N] (padrão 200000). Imprime um hash do resultado, que tem que
 * ser igual no interpretador e no JIT; o tempo dá os MIPS (ver docs/tradutor-arm64.md, JIT). */
#include "a64t.h"
static unsigned long parse(const char *s) { unsigned long v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + (unsigned long)(*s++ - '0'); return v; }
static unsigned char sieve[65536];
static unsigned int arr[512];
int main(int argc, char **argv) {
    unsigned long n = argc > 1 ? parse(argv[1]) : 200000;
    unsigned long long h = 0x12345678, x = 1;
    for (unsigned long i = 0; i < n * 10; i++) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        h = (h ^ x) * 0x100000001B3ull;
        if (x & 1) h += i; else h -= i >> 3;
        h = (h << 5) | (h >> 59);
    }
    for (unsigned long r = 0; r < n / 2000 + 1; r++) {
        for (unsigned i = 0; i < sizeof sieve; i++) sieve[i] = 1;
        unsigned cnt = 0;
        for (unsigned i = 2; i < sizeof sieve; i++)
            if (sieve[i]) { cnt++; for (unsigned j = i * 2; j < sizeof sieve; j += i) sieve[j] = 0; }
        h = h * 31 + cnt;
    }
    for (unsigned long r = 0; r < n / 2000 + 1; r++) {
        for (unsigned i = 0; i < 512; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; arr[i] = (unsigned)x; }
        for (int i = 1; i < 512; i++) {
            unsigned v = arr[i]; int j = i - 1;
            while (j >= 0 && arr[j] > v) { arr[j + 1] = arr[j]; j--; }
            arr[j + 1] = v;
        }
        h = h * 131 + arr[0] + arr[511] + arr[256];
    }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
