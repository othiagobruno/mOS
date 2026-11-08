/* frente JIT: FP half nativo (F16C) × interpretador — FADD/FSUB/FMUL/FDIV escalar (H) e vetorial
  * FSQRT/FCVT H↔S/D/FCMP de half (4H, 8H) com normais, denormais, extremos, inf e NaN, em RN, RM e com FZ16. Hash e FPSR. */
#include "a64t.h"
typedef unsigned short v8 __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(v8 v) { unsigned long long a[2]; __builtin_memcpy(a, &v, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static void mixf(void) { unsigned long long f; __asm__ volatile("mrs %0, fpsr\n msr fpsr, xzr" : "=r"(f)); h = (h ^ f) * 1099511628211ull; }
static const unsigned short sp[] = { 0x3c00, 0xbc00, 0x7bff, 0x0400, 0x0001, 0x03ff, 0x8000, 0, 0x7c00, 0xfc00, 0x7e00, 0x7d00, 0x3555, 0x5bff, 0x0401 };
#define N(a) (sizeof a / sizeof a[0])
#define OV(ins) { v8 r; __asm__ volatile(ins : "=w"(r) : "w"(a), "w"(b)); mixv(r); mixf(); }
int main(void) {
    static const unsigned long long fpcrs[] = { 0, 2ull << 22, 1ull << 19 };
    unsigned long long s = 99;
    for (unsigned k = 0; k < N(fpcrs); k++) {
        __asm__ volatile("msr fpcr, %0" :: "r"(fpcrs[k]));
        for (int it = 0; it < 300; it++) {
            v8 a, b;
            int normal = it < 150;
            for (int l = 0; l < 8; l++) {
                s ^= s << 13; s ^= s >> 7; s ^= s << 17;
                a[l] = normal ? (unsigned short)(0x3800 | (s & 0x3ff) | ((s >> 20) & 0x8000)) : sp[s % N(sp)];
                b[l] = normal ? (unsigned short)(0x4000 | ((s >> 10) & 0x3ff)) : sp[(s >> 8) % N(sp)];
            }
            OV("fadd %0.8h, %1.8h, %2.8h") OV("fsub %0.4h, %1.4h, %2.4h") OV("fmul %0.8h, %1.8h, %2.8h") OV("fdiv %0.8h, %1.8h, %2.8h") OV("fdiv %0.4h, %1.4h, %2.4h")
            OV("fadd %h0, %h1, %h2") OV("fsub %h0, %h1, %h2") OV("fmul %h0, %h1, %h2") OV("fdiv %h0, %h1, %h2")
            OV("fsqrt %h0, %h1") OV("fcvt %s0, %h1") OV("fcvt %d0, %h1") OV("fcvt %h0, %s1") OV("fcvt %h0, %d1")
            { unsigned long long f; __asm__ volatile("fcmp %h1, %h2\n mrs %0, nzcv" : "=r"(f) : "w"(a), "w"(b)); h = (h ^ f) * 1099511628211ull; mixf(); }
            { unsigned long long f; __asm__ volatile("fcmpe %h1, #0.0\n mrs %0, nzcv" : "=r"(f) : "w"(a)); h = (h ^ f) * 1099511628211ull; mixf(); }
        }
    }
    __asm__ volatile("msr fpcr, xzr");
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
