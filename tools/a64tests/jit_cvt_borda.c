/* frente ISA/JIT: bordas de conversão achadas pelo fuzz interp × JIT (15 nov.).
 * FCVTAS/FCVTAU: o JIT somava ±0,5 e truncava; erra quando a soma arredonda (0,49999997f,
 * 2^52 + 1, 2^23 + 1). UCVTF de u64 com bit 63: (n >> 1) * 2 sem sticky arredonda duas vezes
 * (2^63 + 1025). */
#include "a64t.h"
static unsigned long long h = 1469598103934665603ull;
static void mix(unsigned long long v) { h = (h ^ v) * 1099511628211ull; }
static void mixq(void) { unsigned long long f; __asm__ volatile("mrs %0, fpsr\n msr fpsr, xzr" : "=r"(f)); mix(f); }
static void out(const char *s, unsigned long long v) { a64t_puts(s); a64t_puts(" "); a64t_hex(v); a64t_puts("\n"); }
int main(void) {
    static const unsigned long long dv[] = {
        0x3FDFFFFFFFFFFFFFull, 0x3FE0000000000000ull, 0xBFE0000000000000ull, 0xBFDFFFFFFFFFFFFFull,
        0x4330000000000001ull, 0xC330000000000001ull, 0x432FFFFFFFFFFFFFull, 0x3FF8000000000000ull,
        0x4004000000000000ull, 0xC004000000000000ull, 0x4002000000000000ull, 0x0000000000000001ull,
        0x433FFFFFFFFFFFFFull, 0x43DFFFFFFFFFFFFFull, 0x41DFFFFFFFC00000ull, 0x8000000000000000ull };
    static const unsigned sv[] = {
        0x3EFFFFFFu, 0x3F000000u, 0xBF000000u, 0xBEFFFFFFu, 0x4B000001u, 0xCB000001u, 0x4AFFFFFFu,
        0x3FC00000u, 0x40200000u, 0xC0200000u, 0x5EFFFFFFu, 0x4EFFFFFFu, 0x00000001u, 0x80000000u };
    for (unsigned i = 0; i < sizeof dv / 8; i++) {
        double d; __builtin_memcpy(&d, &dv[i], 8);
        long long a; unsigned long long b; int c; unsigned e;
        __asm__ volatile("fcvtas %0, %d1" : "=r"(a) : "w"(d)); mix((unsigned long long)a); mixq();
        __asm__ volatile("fcvtau %0, %d1" : "=r"(b) : "w"(d)); mix(b); mixq();
        __asm__ volatile("fcvtas %w0, %d1" : "=r"(c) : "w"(d)); mix((unsigned)c); mixq();
        __asm__ volatile("fcvtau %w0, %d1" : "=r"(e) : "w"(d)); mix(e); mixq();
        out("d", h);
    }
    for (unsigned i = 0; i < sizeof sv / 4; i++) {
        float f; __builtin_memcpy(&f, &sv[i], 4);
        long long a; unsigned long long b; int c; unsigned e;
        __asm__ volatile("fcvtas %0, %s1" : "=r"(a) : "w"(f)); mix((unsigned long long)a); mixq();
        __asm__ volatile("fcvtau %0, %s1" : "=r"(b) : "w"(f)); mix(b); mixq();
        __asm__ volatile("fcvtas %w0, %s1" : "=r"(c) : "w"(f)); mix((unsigned)c); mixq();
        __asm__ volatile("fcvtau %w0, %s1" : "=r"(e) : "w"(f)); mix(e); mixq();
        out("s", h);
    }
    static const unsigned long long uv[] = {
        0x8000000000000401ull, 0x8000000000000400ull, 0x8000000000000C00ull, 0x8000000000000BFFull,
        0xFFFFFFFFFFFFFFFFull, 0x8000008000000001ull, 0x8000010000000000ull, 0x8000000000000001ull,
        0xFFFFFFFFFFFFFC00ull, 0x7FFFFFFFFFFFFFFFull, 0x0000000000000001ull, 0x8000004000000000ull };
    for (unsigned i = 0; i < sizeof uv / 8; i++) {
        unsigned long long n = uv[i], r[2] = { n, n ^ 0x0000000000000400ull };
        double d; float f;
        __asm__ volatile("ucvtf %d0, %1" : "=w"(d) : "r"(n)); { unsigned long long x; __builtin_memcpy(&x, &d, 8); mix(x); } mixq();
        __asm__ volatile("ucvtf %s0, %1" : "=w"(f) : "r"(n)); { unsigned x; __builtin_memcpy(&x, &f, 4); mix(x); } mixq();
        __asm__ volatile("fmov %d0, %1" : "=w"(d) : "r"(n));
        __asm__ volatile("ucvtf %d0, %d0" : "+w"(d)); { unsigned long long x; __builtin_memcpy(&x, &d, 8); mix(x); } mixq();
        typedef unsigned long long v2 __attribute__((vector_size(16)));
        v2 v; __builtin_memcpy(&v, r, 16);
        __asm__ volatile("ucvtf %0.2d, %0.2d" : "+w"(v)); { unsigned long long x[2]; __builtin_memcpy(x, &v, 16); mix(x[0]); mix(x[1]); } mixq();
        out("u", h);
    }
    return 0;
}
