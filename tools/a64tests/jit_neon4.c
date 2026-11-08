/* frente JIT: saturantes de 32/64 bits (SQADD/UQADD/SQSUB/UQSUB em 2S/4S/2D) e SQDMULH/SQRDMULH em
 * 4H/8H nativos × interpretador, com FPSR.QC; valores perto dos limites para forçar saturação. */
#include "a64t.h"
typedef unsigned char v16 __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(v16 v) { unsigned long long a[2]; __builtin_memcpy(a, &v, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static void mixq(void) { unsigned long long f; __asm__ volatile("mrs %0, fpsr\n msr fpsr, xzr" : "=r"(f)); h = (h ^ f) * 1099511628211ull; }
static v16 rnd(unsigned long long *s, int mode) {
    v16 v; for (int i = 0; i < 16; i++) { *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; v[i] = (unsigned char)(*s >> 24); }
    if (mode == 1) for (int i = 0; i < 16; i += 2) { v[i] = 0; v[i + 1] = 0x80; }          /* 0x8000 em todas as H */
    if (mode == 2) for (int i = 3; i < 16; i += 4) v[i] |= 0x7f;                            /* perto do máximo */
    return v;
}
#define O2(ins) { v16 r; __asm__ volatile(ins : "=w"(r) : "w"(a), "w"(b)); mixv(r); mixq(); }
int main(void) {
    unsigned long long s = 0xFEED;
    for (int it = 0; it < 600; it++) {
        v16 a = rnd(&s, it % 5 == 0 ? 1 : it % 3 == 0 ? 2 : 0), b = rnd(&s, it % 7 == 0 ? 1 : it % 2 ? 2 : 0);
        O2("sqadd %0.4s, %1.4s, %2.4s") O2("uqadd %0.2s, %1.2s, %2.2s") O2("sqsub %0.2d, %1.2d, %2.2d") O2("uqsub %0.4s, %1.4s, %2.4s")
        O2("sqadd %0.2d, %1.2d, %2.2d") O2("uqadd %0.2d, %1.2d, %2.2d") O2("uqsub %0.2d, %1.2d, %2.2d") O2("sqsub %0.2s, %1.2s, %2.2s")
        O2("sqdmulh %0.8h, %1.8h, %2.8h") O2("sqrdmulh %0.4h, %1.4h, %2.4h") O2("sqrdmulh %0.8h, %1.8h, %2.8h") O2("sqdmulh %0.4h, %1.4h, %2.4h")
    }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
