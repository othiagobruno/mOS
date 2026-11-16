/* frente ISA/JIT: FRECPS/FRSQRTS vetoriais nativos (passos de Newton) × interpretador × QEMU,
 * com valores normais, zero × inf, subnormais e FPSR. */
#include "a64t.h"
typedef float v4f __attribute__((vector_size(16)));
typedef double v2d __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(const void *p) { unsigned long long a[2]; __builtin_memcpy(a, p, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static void mixq(void) { unsigned long long f; __asm__ volatile("mrs %0, fpsr\n msr fpsr, xzr" : "=r"(f)); h = (h ^ f) * 1099511628211ull; }
int main(void) {
    unsigned long long s = 0x5EED;
    for (int it = 0; it < 300; it++) {
        float f[8]; double g[4];
        for (int i = 0; i < 8; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; f[i] = (float)((s >> 11) % 100000) / 997.0f - 50.0f; }
        for (int i = 0; i < 4; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; g[i] = (double)((s >> 11) % 1000003) / 7919.0 - 60.0; }
        if (it % 17 == 0) { f[1] = 0.0f; f[5] = __builtin_inff(); g[0] = 0.0; g[2] = -__builtin_inf(); }
        if (it % 23 == 0) { f[2] = 1e-39f; g[1] = 1e300; g[3] = 1e10; }
        v4f a = { f[0], f[1], f[2], f[3] }, b = { f[4], f[5], f[6], f[7] }, r;
        v2d c = { g[0], g[1] }, e = { g[2], g[3] }, q;
        __asm__ volatile("frecps %0.4s, %1.4s, %2.4s" : "=w"(r) : "w"(a), "w"(b)); mixv(&r); mixq();
        __asm__ volatile("frsqrts %0.4s, %1.4s, %2.4s" : "=w"(r) : "w"(a), "w"(b)); mixv(&r); mixq();
        __asm__ volatile("frecps %0.2s, %1.2s, %2.2s" : "=w"(r) : "w"(a), "w"(b)); mixv(&r); mixq();
        __asm__ volatile("frsqrts %0.2s, %1.2s, %2.2s" : "=w"(r) : "w"(a), "w"(b)); mixv(&r); mixq();
        __asm__ volatile("frecps %0.2d, %1.2d, %2.2d" : "=w"(q) : "w"(c), "w"(e)); mixv(&q); mixq();
        __asm__ volatile("frsqrts %0.2d, %1.2d, %2.2d" : "=w"(q) : "w"(c), "w"(e)); mixv(&q); mixq();
        if (it % 50 == 49) { a64t_puts("h "); a64t_hex(h); a64t_puts("\n"); }
    }
    return 0;
}
