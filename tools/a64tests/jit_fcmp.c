/* frente JIT: FCMP/FCMPE/FCCMP/FCSEL nativos × interpretador, com valores especiais (zeros com
 * sinal, denormais, infinitos, NaN quieto e sinalizador), com e sem FZ. Hash de NZCV, FPSR e
 * dos resultados de FCSEL/CSEL nas condições que leem N, Z, C e V. */
#include "a64t.h"
static unsigned long long h = 1469598103934665603ull;
static void mix(unsigned long long v) { h = (h ^ v) * 1099511628211ull; }
static const unsigned long long ds[] = { 0x3ff0000000000000, 0xbff0000000000000, 0x8000000000000000, 0,
    0x0000000000000001, 0x7ff0000000000000, 0xfff0000000000000, 0x7ff8000000000000, 0x7ff0000000000001, 0x7fefffffffffffff };
static const unsigned int fs[] = { 0x3f800000, 0xbf800000, 0x80000000, 0, 0x00000001, 0x7f800000, 0xff800000, 0x7fc00000, 0x7f800001 };
#define N(a) (sizeof a / sizeof a[0])
int main(void) {
    for (int fz = 0; fz < 2; fz++) {
        __asm__ volatile("msr fpcr, %0" :: "r"((unsigned long long)fz << 24));
        for (unsigned i = 0; i < N(ds); i++) for (unsigned j = 0; j < N(ds); j++) {
            double a, b; unsigned long long r, f, s;
            __builtin_memcpy(&a, &ds[i], 8); __builtin_memcpy(&b, &ds[j], 8);
            __asm__ volatile("msr fpsr, xzr\n fcmp %d1, %d2\n mrs %0, nzcv" : "=r"(f) : "w"(a), "w"(b)); mix(f);
            __asm__ volatile("fcmpe %d1, %d2\n cset %0, mi" : "=r"(r) : "w"(a), "w"(b)); mix(r);
            __asm__ volatile("fcmp %d1, #0.0\n cset %0, gt" : "=r"(r) : "w"(a)); mix(r);
            __asm__ volatile("fcmp %d1, %d2\n cset %0, le" : "=r"(r) : "w"(a), "w"(b)); mix(r);
            __asm__ volatile("fcmp %d1, %d2\n cset %0, hi" : "=r"(r) : "w"(a), "w"(b)); mix(r);
            __asm__ volatile("fcmp %d1, %d2\n cset %0, vs" : "=r"(r) : "w"(a), "w"(b)); mix(r);
            __asm__ volatile("cmp %1, #4\n fccmp %d2, %d3, #5, lo\n mrs %0, nzcv" : "=r"(f) : "r"((unsigned long long)i), "w"(a), "w"(b)); mix(f);
            __asm__ volatile("cmp %1, %2\n fccmpe %d3, %d4, #10, ne\n mrs %0, nzcv" : "=r"(f) : "r"((unsigned long long)i), "r"((unsigned long long)j), "w"(a), "w"(b)); mix(f);
            double sel;
            __asm__ volatile("fcmp %d1, %d2\n fcsel %d0, %d1, %d2, lt" : "=w"(sel) : "w"(a), "w"(b)); __builtin_memcpy(&s, &sel, 8); mix(s);
            __asm__ volatile("mrs %0, fpsr" : "=r"(f)); mix(f);
        }
        for (unsigned i = 0; i < N(fs); i++) for (unsigned j = 0; j < N(fs); j++) {
            float a, b; unsigned long long f, s = 0;
            __builtin_memcpy(&a, &fs[i], 4); __builtin_memcpy(&b, &fs[j], 4);
            __asm__ volatile("msr fpsr, xzr\n fcmpe %s1, %s2\n mrs %0, nzcv" : "=r"(f) : "w"(a), "w"(b)); mix(f);
            float sel;
            __asm__ volatile("fcmp %s1, %s2\n fcsel %s0, %s2, %s1, ge" : "=w"(sel) : "w"(a), "w"(b)); __builtin_memcpy(&s, &sel, 4); mix(s);
            __asm__ volatile("mrs %0, fpsr" : "=r"(f)); mix(f);
        }
    }
    __asm__ volatile("msr fpcr, xzr");
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
