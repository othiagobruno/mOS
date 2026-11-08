/* frente JIT: Neon nativo × interpretador, parte 3 — saturantes (SQADD/UQADD/SQSUB/UQSUB em B e H,
 * SQXTN/UQXTN/SQXTUN e "2") com FPSR.QC, arredondados (SRSHR/URSHR/SRSRA/URSRA/RSHRN), ADDHN/
 * SUBHN/RADDHN/RSUBHN, LD2..LD4/ST2..ST4 de lane e LD2R..LD4R. Hash de registradores, buffer e QC. */
#include "a64t.h"
typedef unsigned char v16 __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(v16 v) { unsigned long long a[2]; __builtin_memcpy(a, &v, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static void mixq(void) { unsigned long long f; __asm__ volatile("mrs %0, fpsr\n msr fpsr, xzr" : "=r"(f)); h = (h ^ f) * 1099511628211ull; }
static v16 rnd(unsigned long long *s) {
    v16 v; for (int i = 0; i < 16; i++) { *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; v[i] = (unsigned char)(*s >> 24); }
    return v;
}
static unsigned char mem[256];
#define O2(ins) { v16 r; __asm__ volatile(ins : "=w"(r) : "w"(a), "w"(b)); mixv(r); mixq(); }
#define O1(ins) { v16 r; __asm__ volatile(ins : "=w"(r) : "w"(a)); mixv(r); mixq(); }
#define OD(ins) { v16 r = c; __asm__ volatile(ins : "+w"(r) : "w"(a), "w"(b)); mixv(r); mixq(); }
#define LDL(ins) { register v16 r0 __asm__("v4") = a, r1 __asm__("v5") = b, r2 __asm__("v6") = c, r3 __asm__("v7") = a; const unsigned char *p = mem + off; \
    __asm__ volatile(ins : "+w"(r0), "+w"(r1), "+w"(r2), "+w"(r3), "+r"(p) :: "memory"); mixv(r0); mixv(r1); mixv(r2); mixv(r3); h += (unsigned long long)(p - mem); }
int main(void) {
    unsigned long long s = 0xABCDEF;
    for (int i = 0; i < 256; i++) mem[i] = (unsigned char)(i * 37 + 11);
    for (int it = 0; it < 400; it++) {
        v16 a = rnd(&s), b = rnd(&s), c = rnd(&s);
        if (it & 1) for (int i = 0; i < 16; i++) { a[i] |= 0x70; b[i] |= 0x60; }    /* força saturação */
        O2("sqadd %0.16b, %1.16b, %2.16b") O2("uqadd %0.8h, %1.8h, %2.8h") O2("sqsub %0.4h, %1.4h, %2.4h") O2("uqsub %0.8b, %1.8b, %2.8b")
        O1("sqxtn %0.8b, %1.8h") O1("uqxtn %0.4h, %1.4s") O1("sqxtun %0.8b, %1.8h") O1("sqxtun %0.4h, %1.4s") O1("uqxtn %0.8b, %1.8h")
        OD("sqxtn2 %0.16b, %1.8h") OD("uqxtn2 %0.8h, %1.4s")
        O1("srshr %0.16b, %1.16b, #3") O1("urshr %0.8h, %1.8h, #16") O1("srshr %0.4s, %1.4s, #1") O1("urshr %0.2d, %1.2d, #40") O1("urshr %0.8b, %1.8b, #8")
        OD("ursra %0.8h, %1.8h, #5") OD("srsra %0.4s, %1.4s, #31")
        O1("rshrn %0.8b, %1.8h, #4") O1("rshrn %0.2s, %1.2d, #32") OD("rshrn2 %0.8h, %1.4s, #7")
        O2("addhn %0.8b, %1.8h, %2.8h") O2("raddhn %0.4h, %1.4s, %2.4s") O2("subhn %0.2s, %1.2d, %2.2d") OD("rsubhn2 %0.16b, %1.8h, %2.8h")
        int off = it % 200;
        LDL("ld2 {v4.b, v5.b}[7], [%4]") LDL("ld3 {v4.h, v5.h, v6.h}[3], [%4], #6") LDL("ld4 {v4.s, v5.s, v6.s, v7.s}[1], [%4]") LDL("ld2 {v4.d, v5.d}[1], [%4], #16")
        LDL("ld3r {v4.8h, v5.8h, v6.8h}, [%4]") LDL("ld4r {v4.2s, v5.2s, v6.2s, v7.2s}, [%4], #16") LDL("ld2r {v4.16b, v5.16b}, [%4]")
        LDL("st2 {v4.b, v5.b}[9], [%4]") LDL("st4 {v4.h, v5.h, v6.h, v7.h}[5], [%4], #8") LDL("st3 {v4.s, v5.s, v6.s}[2], [%4]")
        for (int i = 0; i < 256; i += 8) { unsigned long long x; __builtin_memcpy(&x, mem + i, 8); h = (h ^ x) * 1099511628211ull; }
    }
    {   /* fora da faixa com os bits baixos iguais ao limite: QC tem que ligar (0x1FFFF → 0xFFFF) */
        static const unsigned int w[4] = { 0x0001FFFF, 0xFFFF8000, 0x00017FFF, 0x80007FFF };
        static const unsigned short hw[8] = { 0x01FF, 0xFF80, 0x017F, 0x807F, 0x00FF, 0xFF7F, 0x0080, 0x7FFF };
        v16 a, b, c = { 0 };
        __builtin_memcpy(&a, w, 16); __builtin_memcpy(&b, hw, 16);
        O1("uqxtn %0.4h, %1.4s") O1("sqxtn %0.4h, %1.4s") O1("sqxtun %0.4h, %1.4s") OD("uqxtn2 %0.8h, %1.4s")
        a = b;
        O1("uqxtn %0.8b, %1.8h") O1("sqxtn %0.8b, %1.8h") O1("sqxtun %0.8b, %1.8h") OD("sqxtn2 %0.16b, %1.8h")
    }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
