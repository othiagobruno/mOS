/* frente ISA/JIT: FCVT{N,P,M,Z,A}{S,U} vetoriais e escalares de S/D nativos × interpretador × QEMU:
 * empates (0,5, 2,5, -2,5), vizinhos de 0,5, bordas de 2^23/2^52, negativos no sem sinal, FPSR. */
#include "a64t.h"
typedef float v4f __attribute__((vector_size(16)));
typedef double v2d __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(const void *p) { unsigned long long a[2]; __builtin_memcpy(a, p, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static void mixq(void) { unsigned long long f; __asm__ volatile("mrs %0, fpsr\n msr fpsr, xzr" : "=r"(f)); h = (h ^ f) * 1099511628211ull; }
#define VS(ins) { v4f r; __asm__ volatile(ins : "=w"(r) : "w"(a)); mixv(&r); mixq(); }
#define VD(ins) { v2d r; __asm__ volatile(ins : "=w"(r) : "w"(b)); mixv(&r); mixq(); }
int main(void) {
    static const float fs[] = { 0.5f, 1.5f, 2.5f, -0.5f, -1.5f, -2.5f, 0.49999997f, -0.49999997f, 8388609.0f, 8388608.5f,
                                1e9f, -1e9f, 3.3f, -3.7f, 0.0f, -0.0f, 2147483520.0f, 4294967040.0f, 1e-40f, 123456.75f };
    static const double ds[] = { 0.5, 1.5, 2.5, -0.5, -1.5, -2.5, 0.49999999999999994, 4503599627370497.0, 4503599627370496.5,
                                 1e18, -1e18, 3.3, -3.7, 0.0, -0.0, 9.2e18, 1.8e19, 1e-310, 123456.75, -7.5 };
    unsigned nf = sizeof fs / 4, nd = sizeof ds / 8;
    for (unsigned i = 0; i < nf; i++) {
        v4f a = { fs[i], fs[(i + 3) % nf], fs[(i + 7) % nf], fs[(i + 11) % nf] };
        VS("fcvtns %0.4s, %1.4s") VS("fcvtnu %0.4s, %1.4s") VS("fcvtps %0.4s, %1.4s") VS("fcvtpu %0.4s, %1.4s")
        VS("fcvtms %0.4s, %1.4s") VS("fcvtmu %0.4s, %1.4s") VS("fcvtas %0.4s, %1.4s") VS("fcvtau %0.4s, %1.4s")
        VS("fcvtzs %0.2s, %1.2s") VS("fcvtas %0.2s, %1.2s") VS("fcvtns %s0, %s1") VS("fcvtau %s0, %s1")
        a64t_puts("s "); a64t_hex(h); a64t_puts("\n");
    }
    for (unsigned i = 0; i < nd; i++) {
        v2d b = { ds[i], ds[(i + 5) % nd] };
        VD("fcvtns %0.2d, %1.2d") VD("fcvtnu %0.2d, %1.2d") VD("fcvtps %0.2d, %1.2d") VD("fcvtpu %0.2d, %1.2d")
        VD("fcvtms %0.2d, %1.2d") VD("fcvtmu %0.2d, %1.2d") VD("fcvtas %0.2d, %1.2d") VD("fcvtau %0.2d, %1.2d")
        VD("fcvtzu %0.2d, %1.2d") VD("fcvtas %d0, %d1") VD("fcvtmu %d0, %d1")
        a64t_puts("d "); a64t_hex(h); a64t_puts("\n");
    }
    return 0;
}
