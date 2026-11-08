/* frente JIT: FP escalar nativo × interpretador — FADD/FSUB/FMUL/FDIV/FNMUL/FSQRT/FABS/FNEG em
 * S e D sobre valores normais, extremos, denormais, infinitos, zeros com sinal e NaNs (quietos e
 * sinalizadores), em RN e nos outros modos (FPCR.RMode) e com FZ. Imprime hash dos bits e FPSR. */
#include "a64t.h"
static unsigned long long h = 1469598103934665603ull;
static void mix(unsigned long long v) { h = (h ^ v) * 1099511628211ull; }
static unsigned long long fpsr(void) { unsigned long long v; __asm__ volatile("mrs %0, fpsr" : "=r"(v)); return v; }
static void setfpcr(unsigned long long v) { __asm__ volatile("msr fpcr, %0" :: "r"(v)); }
static void clrfpsr(void) { __asm__ volatile("msr fpsr, xzr"); }
static const unsigned int fs[] = { 0x3f800000, 0x40490fdb, 0xbf000000, 0x7f7fffff, 0x00800000, 0x00000001, 0x007fffff,
    0x80000000, 0x00000000, 0x7f800000, 0xff800000, 0x7fc00000, 0x7f800001, 0x3eaaaaab, 0x4b000001, 0xc2f6e979, 0x33800000, 0x00800001 };
static const unsigned long long ds[] = { 0x3ff0000000000000, 0x400921fb54442d18, 0xbfe0000000000000, 0x7fefffffffffffff,
    0x0010000000000000, 0x0000000000000001, 0x000fffffffffffff, 0x8000000000000000, 0, 0x7ff0000000000000, 0xfff0000000000000,
    0x7ff8000000000000, 0x7ff0000000000001, 0x3fd5555555555555, 0x4330000000000001, 0xc05edd2f1a9fbe77, 0x3ca0000000000000, 0x0010000000000001 };
#define NF (sizeof fs / sizeof fs[0])
#define ND (sizeof ds / sizeof ds[0])
#define OPS(T, a, b, r) \
    __asm__ volatile("fadd %" T "0, %" T "1, %" T "2" : "=w"(r) : "w"(a), "w"(b)); mix(rb(r)); \
    __asm__ volatile("fsub %" T "0, %" T "1, %" T "2" : "=w"(r) : "w"(a), "w"(b)); mix(rb(r)); \
    __asm__ volatile("fmul %" T "0, %" T "1, %" T "2" : "=w"(r) : "w"(a), "w"(b)); mix(rb(r)); \
    __asm__ volatile("fdiv %" T "0, %" T "1, %" T "2" : "=w"(r) : "w"(a), "w"(b)); mix(rb(r)); \
    __asm__ volatile("fnmul %" T "0, %" T "1, %" T "2" : "=w"(r) : "w"(a), "w"(b)); mix(rb(r)); \
    __asm__ volatile("fsqrt %" T "0, %" T "1" : "=w"(r) : "w"(a)); mix(rb(r)); \
    __asm__ volatile("fabs %" T "0, %" T "1" : "=w"(r) : "w"(a)); mix(rb(r)); \
    __asm__ volatile("fneg %" T "0, %" T "1" : "=w"(r) : "w"(a)); mix(rb(r)); \
    __asm__ volatile("fmadd %" T "0, %" T "1, %" T "2, %" T "3" : "=w"(r) : "w"(a), "w"(b), "w"(a)); mix(rb(r)); \
    __asm__ volatile("fmsub %" T "0, %" T "1, %" T "2, %" T "3" : "=w"(r) : "w"(a), "w"(b), "w"(b)); mix(rb(r)); \
    __asm__ volatile("fnmadd %" T "0, %" T "1, %" T "2, %" T "3" : "=w"(r) : "w"(b), "w"(b), "w"(a)); mix(rb(r)); \
    __asm__ volatile("fnmsub %" T "0, %" T "1, %" T "2, %" T "3" : "=w"(r) : "w"(a), "w"(a), "w"(b)); mix(rb(r)); \
    mix(fpsr());
static unsigned long long rbf(float x) { unsigned int u; __builtin_memcpy(&u, &x, 4); return u; }
static unsigned long long rbd(double x) { unsigned long long u; __builtin_memcpy(&u, &x, 8); return u; }
int main(void) {
    static const unsigned long long fpcrs[] = { 0, 1ull << 22, 2ull << 22, 3ull << 22, 1ull << 24, 1ull << 25 };
    for (unsigned k = 0; k < sizeof fpcrs / sizeof fpcrs[0]; k++) {
        setfpcr(fpcrs[k]);
        for (unsigned i = 0; i < NF; i++) for (unsigned j = 0; j < NF; j++) {
            float a, b, r; unsigned int ua = fs[i], ub = fs[j];
            __builtin_memcpy(&a, &ua, 4); __builtin_memcpy(&b, &ub, 4);
            clrfpsr();
#define rb rbf
            OPS("s", a, b, r)
#undef rb
        }
        for (unsigned i = 0; i < ND; i++) for (unsigned j = 0; j < ND; j++) {
            double a, b, r;
            __builtin_memcpy(&a, &ds[i], 8); __builtin_memcpy(&b, &ds[j], 8);
            clrfpsr();
#define rb rbd
            OPS("d", a, b, r)
#undef rb
        }
        /* laço quente: muitas operações inexatas seguidas (IXC acumulado) */
        double x = 1.0, y = 3.0;
        clrfpsr();
        for (int i = 0; i < 20000; i++) { x = x * 1.0000001 + y / 7.0; y = y - x * 1e-9; }
        mix(rbd(x)); mix(rbd(y)); mix(fpsr());
    }
    setfpcr(0);
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
