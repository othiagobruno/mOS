/* frente JIT: FP vetorial nativo × interpretador — FADD/FSUB/FMUL/FDIV/FMLA/FMLS em 4S, 2S e 2D com
 * lanes que misturam normais, zeros com sinal, denormais, extremos, inf e NaN; RN, outros modos e FZ.
 * Hash dos resultados e do FPSR. */
#include "a64t.h"
typedef unsigned int v4 __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(v4 v) { unsigned long long a[2]; __builtin_memcpy(a, &v, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static void mix(unsigned long long v) { h = (h ^ v) * 1099511628211ull; }
static const unsigned int fs[] = { 0x3f800000, 0x40490fdb, 0xbf000000, 0x7f7fffff, 0x00800000, 0x00000001, 0x80000000,
    0, 0x7f800000, 0x7fc00000, 0x7f800001, 0x3eaaaaab, 0x4b000001, 0x33800000, 0x00800001, 0x42f6e979 };
static const unsigned long long ds[] = { 0x3ff0000000000000, 0x400921fb54442d18, 0xbfe0000000000000, 0x7fefffffffffffff,
    0x0010000000000000, 1, 0x8000000000000000, 0, 0x7ff0000000000000, 0x7ff8000000000000, 0x3fd5555555555555, 0x0010000000000001 };
#define N(a) (sizeof a / sizeof a[0])
#define OP(ins) { v4 r = c; __asm__ volatile(ins : "+w"(r) : "w"(a), "w"(b)); mixv(r); }
/* fundidas: FPSR zerado antes (o QEMU do Windows erra o FMA em RN com IXC já ligado); o FPSR
 * acumulado é misturado no hash aqui mesmo */
#define OPF(ins) { unsigned long long f0; __asm__ volatile("mrs %0, fpsr" : "=r"(f0)); mix(f0); v4 r = c; \
    __asm__ volatile("msr fpsr, xzr\n " ins : "+w"(r) : "w"(a), "w"(b)); mixv(r); }
int main(void) {
    static const unsigned long long fpcrs[] = { 0, 2ull << 22, 1ull << 24 };
    unsigned long long st = 12345;
    for (unsigned k = 0; k < N(fpcrs); k++) {
        __asm__ volatile("msr fpcr, %0" :: "r"(fpcrs[k]));
        for (int it = 0; it < 400; it++) {
            v4 a, b, c;
            int normal = it < 200;                  /* metade só com normais (caminho rápido) */
            for (int l = 0; l < 4; l++) {
                st ^= st << 13; st ^= st >> 7; st ^= st << 17;
                a[l] = normal ? (0x3f000000u | (unsigned)(st & 0x7fffff)) : fs[st % N(fs)];
                b[l] = normal ? (0x40000000u | (unsigned)((st >> 23) & 0x7fffff)) : fs[(st >> 8) % N(fs)];
                c[l] = normal ? (0x3e800000u | (unsigned)((st >> 40) & 0x7fffff)) : fs[(st >> 16) % N(fs)];
            }
            __asm__ volatile("msr fpsr, xzr");
            OP("fadd %0.4s, %1.4s, %2.4s") OP("fsub %0.4s, %1.4s, %2.4s") OP("fmul %0.4s, %1.4s, %2.4s") OP("fdiv %0.4s, %1.4s, %2.4s")
            
            OP("fadd %0.2s, %1.2s, %2.2s") OP("fmul %0.2s, %1.2s, %2.2s") OP("fdiv %0.2s, %1.2s, %2.2s")
            { unsigned long long f; __asm__ volatile("mrs %0, fpsr" : "=r"(f)); mix(f); }
            v4 x, y, z;
            for (int l = 0; l < 2; l++) {
                st ^= st << 13; st ^= st >> 7; st ^= st << 17;
                unsigned long long p = normal ? (0x3ff0000000000000ull | (st & 0xfffffffffffffull)) : ds[st % N(ds)];
                unsigned long long q = normal ? (0x4000000000000000ull | ((st >> 7) & 0xfffffffffffffull)) : ds[(st >> 9) % N(ds)];
                __builtin_memcpy((char *)&x + 8 * l, &p, 8); __builtin_memcpy((char *)&y + 8 * l, &q, 8); __builtin_memcpy((char *)&z + 8 * l, &p, 8);
            }
            a = x; b = y; c = z;
            __asm__ volatile("msr fpsr, xzr");
            OP("fadd %0.2d, %1.2d, %2.2d") OP("fsub %0.2d, %1.2d, %2.2d") OP("fmul %0.2d, %1.2d, %2.2d") OP("fdiv %0.2d, %1.2d, %2.2d")
            
            { unsigned long long f; __asm__ volatile("mrs %0, fpsr" : "=r"(f)); mix(f); }
        }
    }
    __asm__ volatile("msr fpcr, xzr");
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
