/* frente JIT: conversões nativas × interpretador — FCVTZS/FCVTZU (W e X, de S e D), SCVTF/UCVTF
 * (W e X, para S e D), FCVT S↔D, com valores nas bordas das faixas, negativos, denormais, inf e
 * NaN, em RN, nos outros modos e com FZ. Hash dos resultados e do FPSR. */
#include "a64t.h"
static unsigned long long h = 1469598103934665603ull;
static void mix(unsigned long long v) { h = (h ^ v) * 1099511628211ull; }
static const unsigned long long ds[] = { 0x3ff8000000000000, 0xc00c000000000000, 0x41dfffffffc00000, 0x41e0000000000000,
    0xc1e0000000000000, 0x41efffffffe00000, 0x41f0000000000000, 0x43dfffffffffffff, 0x43e0000000000000, 0xc3e0000000000000,
    0x43efffffffffffff, 0x43f0000000000000, 0x8000000000000000, 0, 1, 0x7ff0000000000000, 0x7ff8000000000000, 0xbfe0000000000000,
    0x36a0000000000000, 0x47efffffe0000000, 0x47f0000000000000, 0x3810000000000000, 0x380fffffffffffff };
static const unsigned int fs[] = { 0x3fc00000, 0xc0600000, 0x4effffff, 0x4f000000, 0xcf000000, 0x4f7fffff, 0x4f800000,
    0x5effffff, 0x5f000000, 0xdf000000, 0x5f7fffff, 0x5f800000, 0x80000000, 0, 1, 0x7f800000, 0x7fc00000, 0x7f800001, 0xbf000000 };
static const unsigned long long is[] = { 0, 1, 0xffffffffffffffff, 0x7fffffffffffffff, 0x8000000000000000, 0x1fffffffffffff,
    0x20000000000001, 0xffffffff, 0x7fffffff, 0x80000000, 0x123456789abcdef, 16777217 };
#define N(a) (sizeof a / sizeof a[0])
#define R(ins, out, in) __asm__ volatile(ins : "=r"(out) : "w"(in)); mix(out)
int main(void) {
    static const unsigned long long fpcrs[] = { 0, 1ull << 22, 2ull << 22, 3ull << 22, 1ull << 24 };
    for (unsigned k = 0; k < N(fpcrs); k++) {
        __asm__ volatile("msr fpcr, %0" :: "r"(fpcrs[k]));
        unsigned long long r, f;
        for (unsigned i = 0; i < N(ds); i++) {
            double a; __builtin_memcpy(&a, &ds[i], 8);
            __asm__ volatile("msr fpsr, xzr");
            R("fcvtzs %x0, %d1", r, a); R("fcvtzu %x0, %d1", r, a); R("fcvtzs %w0, %d1", r, a); R("fcvtzu %w0, %d1", r, a);
            float s; __asm__ volatile("fcvt %s0, %d1" : "=w"(s) : "w"(a)); unsigned int u; __builtin_memcpy(&u, &s, 4); mix(u);
            __asm__ volatile("mrs %0, fpsr" : "=r"(f)); mix(f);
        }
        for (unsigned i = 0; i < N(fs); i++) {
            float a; __builtin_memcpy(&a, &fs[i], 4);
            __asm__ volatile("msr fpsr, xzr");
            R("fcvtzs %x0, %s1", r, a); R("fcvtzu %x0, %s1", r, a); R("fcvtzs %w0, %s1", r, a); R("fcvtzu %w0, %s1", r, a);
            double dd; __asm__ volatile("fcvt %d0, %s1" : "=w"(dd) : "w"(a)); unsigned long long u; __builtin_memcpy(&u, &dd, 8); mix(u);
            __asm__ volatile("mrs %0, fpsr" : "=r"(f)); mix(f);
        }
        for (unsigned i = 0; i < N(is); i++) {
            unsigned long long v = is[i], b; double dd; float s; unsigned int u;
            __asm__ volatile("msr fpsr, xzr");
            __asm__ volatile("scvtf %d0, %x1" : "=w"(dd) : "r"(v)); __builtin_memcpy(&b, &dd, 8); mix(b);
            __asm__ volatile("ucvtf %d0, %x1" : "=w"(dd) : "r"(v)); __builtin_memcpy(&b, &dd, 8); mix(b);
            __asm__ volatile("scvtf %d0, %w1" : "=w"(dd) : "r"(v)); __builtin_memcpy(&b, &dd, 8); mix(b);
            __asm__ volatile("ucvtf %s0, %w1" : "=w"(s) : "r"(v)); __builtin_memcpy(&u, &s, 4); mix(u);
            __asm__ volatile("scvtf %s0, %x1" : "=w"(s) : "r"(v)); __builtin_memcpy(&u, &s, 4); mix(u);
            __asm__ volatile("ucvtf %s0, %x1" : "=w"(s) : "r"(v)); __builtin_memcpy(&u, &s, 4); mix(u);
            __asm__ volatile("mrs %0, fpsr" : "=r"(f)); mix(f);
        }
    }
    __asm__ volatile("msr fpcr, xzr");
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
