/* frente JIT: Neon nativo × interpretador, parte 2 — 3diff ((S|U)ADDL/ADDW/SUBL/SUBW/MULL/MLAL/MLSL
 * e as formas "2"), por elemento (MUL/MLA/MLS em H e S; FMUL em S e D — FMLA/FMLS por elemento em jit_fpv_fma.c), ADDP vetorial e
 * escalar, FADDP. FP com valores normais (caminho rápido) e especiais (caminho lento). */
#include "a64t.h"
typedef unsigned char v16 __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(v16 v) { unsigned long long a[2]; __builtin_memcpy(a, &v, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static v16 rnd(unsigned long long *s) {
    v16 v; for (int i = 0; i < 16; i++) { *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; v[i] = (unsigned char)(*s >> 24); }
    return v;
}
static v16 fpvals(unsigned long long *s, int dbl, int special) {
    v16 v; unsigned long long a[2];
    for (int l = 0; l < 2; l++) {
        *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17;
        unsigned long long x = *s;
        if (dbl) a[l] = special && (x & 1) ? 0x7ff0000000000000ull : (0x3ff0000000000000ull | (x & 0xfffffffffffffull)) ^ ((x >> 63) << 63);
        else a[l] = ((0x3f800000ull | (x & 0x7fffff)) << 32 | (0x40000000ull | ((x >> 23) & 0x7fffff))) ^ (special && (x & 2) ? 0x0000000080000001ull : 0);
    }
    __builtin_memcpy(&v, a, 16);
    return v;
}
#define O2(ins) { v16 r; __asm__ volatile(ins : "=w"(r) : "w"(a), "w"(b)); mixv(r); }
#define OD(ins) { v16 r = c; __asm__ volatile(ins : "+w"(r) : "w"(a), "w"(b)); mixv(r); }
#define OE(ins) { v16 r = c; register v16 e __asm__("v9") = b; __asm__ volatile(ins : "+w"(r) : "w"(a), "w"(e)); mixv(r); }
int main(void) {
    unsigned long long s = 0x1234567;
    for (int it = 0; it < 300; it++) {
        v16 a = rnd(&s), b = rnd(&s), c = rnd(&s);
        O2("uaddl %0.8h, %1.8b, %2.8b") O2("saddl2 %0.4s, %1.8h, %2.8h") O2("uaddw %0.2d, %1.2d, %2.2s") O2("ssubw2 %0.8h, %1.8h, %2.16b")
        O2("usubl %0.4s, %1.4h, %2.4h") O2("ssubl2 %0.2d, %1.4s, %2.4s")
        O2("umull %0.8h, %1.8b, %2.8b") O2("smull2 %0.4s, %1.8h, %2.8h") O2("umull %0.2d, %1.2s, %2.2s") O2("smull2 %0.2d, %1.4s, %2.4s")
        OD("umlal %0.4s, %1.4h, %2.4h") OD("smlal2 %0.8h, %1.16b, %2.16b") OD("umlsl2 %0.2d, %1.4s, %2.4s") OD("smlsl %0.4s, %1.4h, %2.4h")
        OE("mul %0.8h, %1.8h, v9.h[3]") OE("mul %0.2s, %1.2s, v9.s[1]") OE("mla %0.4s, %1.4s, v9.s[2]") OE("mls %0.4h, %1.4h, v9.h[7]")
        O2("addp %0.16b, %1.16b, %2.16b") O2("addp %0.8b, %1.8b, %2.8b") O2("addp %0.8h, %1.8h, %2.8h") O2("addp %0.2s, %1.2s, %2.2s") O2("addp %0.2d, %1.2d, %2.2d")
        { v16 r; __asm__ volatile("addp d0, %1.2d\n mov %0.16b, v0.16b" : "=w"(r) : "w"(a) : "v0"); mixv(r); }
        int sp = it % 3 == 0;
        v16 fa = fpvals(&s, 0, sp), fb = fpvals(&s, 0, sp), fc = fpvals(&s, 0, sp), da = fpvals(&s, 1, sp), db = fpvals(&s, 1, sp);
        __asm__ volatile("msr fpsr, xzr");
        a = fa; b = fb; c = fc;
        OE("fmul %0.4s, %1.4s, v9.s[1]") OE("fmul %0.2s, %1.2s, v9.s[3]")
        { unsigned long long f; __asm__ volatile("mrs %0, fpsr" : "=r"(f)); h ^= f; __asm__ volatile("msr fpsr, xzr"); }
        
        O2("faddp %0.4s, %1.4s, %2.4s")
        a = da; b = db; c = da;
        OE("fmul %0.2d, %1.2d, v9.d[1]") O2("faddp %0.2d, %1.2d, %2.2d")
        
        { unsigned long long f; __asm__ volatile("mrs %0, fpsr" : "=r"(f)); h = (h ^ f) * 1099511628211ull; }
    }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
