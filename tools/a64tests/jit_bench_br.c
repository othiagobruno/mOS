/* frente JIT: laço com desvios condicionais curtos (estado que muda a cada passo),
 * sem chamada. É o formato de um scanner/decodificador. Uso: jit_bench_br.elf [N]
 * (padrão 5000000). */
#include "a64t.h"
static unsigned long parse(const char *s) {
    unsigned long v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (unsigned long)(*s++ - '0');
    return v;
}
int main(int argc, char **argv) {
    unsigned long n = argc > 1 ? parse(argv[1]) : 5000000ul;
    unsigned long h = 0x123456789abcdefull;
    for (unsigned long i = 0; i < n; i++) {
        if (h & 1) h = h * 3 + 1;
        else h = h >> 1;
        if (h & 2) h ^= 0xA5A5A5A5A5A5A5A5ull;
        else h += 7 + (i & 15);
        if (h & 4) h = (h << 3) | (h >> 61);
        else h -= i & 31;
        h ^= h >> 11;
    }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
