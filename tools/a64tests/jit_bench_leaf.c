/* frente JIT: chamadas diretas (BL) a folhas aritméticas, no estilo de helpers curtos
 * de um app (hash, mistura). Uso: jit_bench_leaf.elf [N] (padrão 2000000). */
#include "a64t.h"
static unsigned long parse(const char *s) {
    unsigned long v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (unsigned long)(*s++ - '0');
    return v;
}
__attribute__((noinline)) static unsigned long leaf_add(unsigned long x) { return x + 0x9E3779B97F4A7C15ull; }
__attribute__((noinline)) static unsigned long leaf_mix(unsigned long x) { return (x ^ (x >> 17)) * 0xBF58476D1CE4E5B9ull; }
__attribute__((noinline)) static unsigned long leaf_rot(unsigned long x) { return (x << 13) | (x >> 51); }
int main(int argc, char **argv) {
    unsigned long n = argc > 1 ? parse(argv[1]) : 2000000ul;
    unsigned long h = 1;
    for (unsigned long i = 0; i < n; i++) {
        h = leaf_add(h);
        h = leaf_mix(h);
        h = leaf_rot(h);
    }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
