/* frente JIT: laço de SHA256H/H2/SU0/SU1 e SHA1C/M/P/H/SU0/SU1 (via chamada leve ao interpretador)
 * × interpretador; também serve de benchmark do caminho de TLS. Uso: jit_sha.elf [N]. */
#include "a64t.h"
typedef unsigned int v4 __attribute__((vector_size(16)));
static unsigned long parse(const char *s) { unsigned long v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + (unsigned long)(*s++ - '0'); return v; }
int main(int argc, char **argv) {
    unsigned long n = argc > 1 ? parse(argv[1]) : 20000;
    v4 abcd = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a }, efgh = { 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    v4 w0 = { 1, 2, 3, 4 }, w1 = { 5, 6, 7, 8 }, w2 = { 9, 10, 11, 12 }, w3 = { 13, 14, 15, 16 }, k = { 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5 };
    for (unsigned long i = 0; i < n; i++) {
        v4 t = w0 + k, s = abcd;
        __asm__ volatile("sha256h %q0, %q1, %2.4s\n sha256h2 %q1, %q3, %2.4s" : "+w"(abcd), "+w"(efgh) : "w"(t), "w"(s));
        __asm__ volatile("sha256su0 %0.4s, %1.4s\n sha256su1 %0.4s, %2.4s, %3.4s" : "+w"(w0) : "w"(w1), "w"(w2), "w"(w3));
        v4 x = w0; w0 = w1; w1 = w2; w2 = w3; w3 = x;
        unsigned int e = abcd[0];
        __asm__ volatile("sha1c %q0, %s2, %3.4s\n sha1su0 %1.4s, %4.4s, %3.4s" : "+w"(abcd), "+w"(w1) : "w"(e), "w"(t), "w"(w2));
        __asm__ volatile("sha1h %s0, %s0" : "+w"(e));
        abcd[1] ^= e;
    }
    a64t_puts("hash "); a64t_hex(((unsigned long long)abcd[0] << 32 | abcd[1]) ^ ((unsigned long long)efgh[2] << 32 | efgh[3])); a64t_puts("\n");
    return 0;
}
