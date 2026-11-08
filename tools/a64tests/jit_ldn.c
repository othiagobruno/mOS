/* frente JIT: LD2/LD3/LD4 e ST2/ST3/ST4 (todos os tamanhos, Q=0/1, pós-índice) nativos ×
 * interpretador. Hash dos registradores e do buffer de saída. */
#include "a64t.h"
typedef unsigned char v16 __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(v16 v) { unsigned long long a[2]; __builtin_memcpy(a, &v, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static unsigned char src[256], dst[256];
#define LD(ins) { register v16 r0 __asm__("v4"), r1 __asm__("v5"), r2 __asm__("v6"), r3 __asm__("v7"); const unsigned char *p = src + off; \
    __asm__ volatile(ins : "=w"(r0), "=w"(r1), "=w"(r2), "=w"(r3), "+r"(p) :: "memory"); mixv(r0); mixv(r1); mixv(r2); mixv(r3); h += (unsigned long long)(p - src); }
#define ST(ins) { register v16 r0 __asm__("v4") = a, r1 __asm__("v5") = b, r2 __asm__("v6") = c, r3 __asm__("v7") = a ^ b; unsigned char *p = dst + off; \
    __asm__ volatile(ins : "+r"(p) : "w"(r0), "w"(r1), "w"(r2), "w"(r3) : "memory"); h += (unsigned long long)(p - dst); }
int main(void) {
    unsigned long long s = 77;
    for (int i = 0; i < 256; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; src[i] = (unsigned char)s; }
    for (int off = 0; off < 64; off += 9) {
        LD("ld2 {v4.16b, v5.16b}, [%4]") LD("ld2 {v4.8h, v5.8h}, [%4], #32") LD("ld2 {v4.2s, v5.2s}, [%4]") LD("ld2 {v4.2d, v5.2d}, [%4]")
        LD("ld3 {v4.16b, v5.16b, v6.16b}, [%4], #48") LD("ld3 {v4.4h, v5.4h, v6.4h}, [%4]") LD("ld3 {v4.4s, v5.4s, v6.4s}, [%4]")
        LD("ld4 {v4.16b, v5.16b, v6.16b, v7.16b}, [%4], #64") LD("ld4 {v4.8b, v5.8b, v6.8b, v7.8b}, [%4]") LD("ld4 {v4.8h, v5.8h, v6.8h, v7.8h}, [%4]") LD("ld4 {v4.2d, v5.2d, v6.2d, v7.2d}, [%4]")
        v16 a, b, c;
        for (int i = 0; i < 16; i++) { a[i] = src[off + i]; b[i] = src[off + 40 + i]; c[i] = (unsigned char)(a[i] * 7 + b[i]); }
        ST("st2 {v4.16b, v5.16b}, [%0]") ST("st2 {v4.4s, v5.4s}, [%0], #32") ST("st3 {v4.8b, v5.8b, v6.8b}, [%0]") ST("st3 {v4.8h, v5.8h, v6.8h}, [%0], #48")
        ST("st4 {v4.16b, v5.16b, v6.16b, v7.16b}, [%0]") ST("st4 {v4.2s, v5.2s, v6.2s, v7.2s}, [%0], #32") ST("st4 {v4.2d, v5.2d, v6.2d, v7.2d}, [%0]")
        for (int i = 0; i < 256; i += 8) { unsigned long long x; __builtin_memcpy(&x, dst + i, 8); h = (h ^ x) * 1099511628211ull; }
    }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
