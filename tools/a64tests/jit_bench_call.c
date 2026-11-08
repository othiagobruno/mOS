/* frente JIT: benchmark de chamadas e retornos (fib recursivo + chamadas por ponteiro), que
 * exercita a pilha de retorno e o cache de alvos indiretos. Uso: jit_bench_call.elf [N]. */
#include "a64t.h"
static unsigned long parse(const char *s) { unsigned long v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + (unsigned long)(*s++ - '0'); return v; }
__attribute__((noinline)) static unsigned long fib(unsigned n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
__attribute__((noinline)) static unsigned long f1(unsigned long x) { return x * 3 + 1; }
__attribute__((noinline)) static unsigned long f2(unsigned long x) { return x ^ (x >> 3); }
__attribute__((noinline)) static unsigned long f3(unsigned long x) { return x + 0x9E3779B9; }
static unsigned long (*const tab[3])(unsigned long) = { f1, f2, f3 };
int main(int argc, char **argv) {
    unsigned long n = argc > 1 ? parse(argv[1]) : 27;
    unsigned long h = fib((unsigned)n);
    for (unsigned long i = 0; i < 3000000; i++) h = tab[i % 3](h);
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
