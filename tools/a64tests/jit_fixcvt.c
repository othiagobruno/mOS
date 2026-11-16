/* frente ISA/JIT: SCVTF/UCVTF e FCVTZS/FCVTZU de ponto fixo (#fbits) vetoriais × interp × QEMU. */
#include "a64t.h"
typedef float v4f __attribute__((vector_size(16)));
typedef double v2d __attribute__((vector_size(16)));
typedef int v4i __attribute__((vector_size(16)));
typedef long long v2l __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(const void *p) { unsigned long long a[2]; __builtin_memcpy(a, p, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static void mixq(void) { unsigned long long f; __asm__ volatile("mrs %0, fpsr\n msr fpsr, xzr" : "=r"(f)); h = (h ^ f) * 1099511628211ull; }
#define OP(ins, out, in) { __asm__ volatile(ins : "=w"(out) : "w"(in)); mixv(&out); mixq(); }
int main(void) {
    unsigned long long s = 0xF1F0;
    for (int it = 0; it < 400; it++) {
        int iv[4]; long long lv[2]; float fv[4]; double dv[2];
        for (int i = 0; i < 4; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; iv[i] = (int)(s >> (it % 40)); fv[i] = (float)(int)(s >> 40) / 1024.0f; }
        for (int i = 0; i < 2; i++) { s ^= s << 13; s ^= s >> 7; s ^= s << 17; lv[i] = (long long)s >> (it % 60); dv[i] = (double)(long long)(s >> 20) / 65536.0; }
        v4i a; __builtin_memcpy(&a, iv, 16); v2l b; __builtin_memcpy(&b, lv, 16);
        v4f f; __builtin_memcpy(&f, fv, 16); v2d g; __builtin_memcpy(&g, dv, 16);
        v4f rf; v2d rd; v4i ri; v2l rl;
        OP("scvtf %0.4s, %1.4s, #7", rf, a) OP("ucvtf %0.4s, %1.4s, #31", rf, a) OP("scvtf %0.2s, %1.2s, #1", rf, a)
        OP("scvtf %0.2d, %1.2d, #40", rd, b) OP("ucvtf %0.2d, %1.2d, #3", rd, b)
        OP("fcvtzs %0.4s, %1.4s, #8", ri, f) OP("fcvtzu %0.4s, %1.4s, #12", ri, f) OP("fcvtzs %0.2s, %1.2s, #20", ri, f)
        OP("fcvtzs %0.2d, %1.2d, #16", rl, g) OP("fcvtzu %0.2d, %1.2d, #5", rl, g)
        if (it % 50 == 49) { a64t_puts("h "); a64t_hex(h); a64t_puts("\n"); }
    }
    return 0;
}
