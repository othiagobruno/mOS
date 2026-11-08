/* frente JIT: Neon inteiro nativo × interpretador — deslocamentos (SHL/USHR/SSHR/USRA/SSRA em todos
 * os tamanhos e nos extremos), USHLL/SSHLL, SHRN(2), XTN(2), TBL/TBX de 1 a 4 registradores, UZP,
 * TRN, ZIP, EXT, CMxx, MAX/MIN, ADDV/UMAXV, DUP/INS/UMOV, em Q=0 e Q=1. Hash dos registradores. */
#include "a64t.h"
typedef unsigned char v16 __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(v16 v) { unsigned long long a[2]; __builtin_memcpy(a, &v, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static v16 rnd(unsigned long long *s) {
    v16 v; for (int i = 0; i < 16; i++) { *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; v[i] = (unsigned char)(*s >> 24); }
    return v;
}
#define O2(ins) { v16 r; __asm__ volatile(ins : "=w"(r) : "w"(a), "w"(b)); mixv(r); }
#define O1(ins) { v16 r; __asm__ volatile(ins : "=w"(r) : "w"(a)); mixv(r); }
#define OD(ins) { v16 r = c; __asm__ volatile(ins : "+w"(r) : "w"(a), "w"(b)); mixv(r); }
int main(void) {
    unsigned long long s = 0x9E3779B97F4A7C15ull;
    for (int it = 0; it < 300; it++) {
        v16 a = rnd(&s), b = rnd(&s), c = rnd(&s);
        if (it & 1) for (int i = 0; i < 16; i++) b[i] &= 0x3f;          /* índices de TBL na faixa */
        O1("shl %0.16b, %1.16b, #3") O1("shl %0.8h, %1.8h, #15") O1("shl %0.4s, %1.4s, #1") O1("shl %0.2d, %1.2d, #63") O1("shl %0.8b, %1.8b, #7")
        O1("ushr %0.16b, %1.16b, #1") O1("ushr %0.16b, %1.16b, #8") O1("ushr %0.8h, %1.8h, #16") O1("ushr %0.4s, %1.4s, #9") O1("ushr %0.2d, %1.2d, #64")
        O1("sshr %0.16b, %1.16b, #3") O1("sshr %0.16b, %1.16b, #8") O1("sshr %0.4h, %1.4h, #16") O1("sshr %0.4s, %1.4s, #31") O1("sshr %0.2s, %1.2s, #32")
        OD("usra %0.8h, %1.8h, #4") OD("ssra %0.16b, %1.16b, #2") OD("usra %0.2d, %1.2d, #60")
        O1("uxtl %0.8h, %1.8b") O1("uxtl2 %0.4s, %1.8h") O1("sxtl %0.2d, %1.2s") O1("sxtl2 %0.8h, %1.16b") O1("ushll %0.4s, %1.4h, #7") O1("sshll %0.8h, %1.8b, #3")
        O1("shrn %0.8b, %1.8h, #3") O1("shrn %0.4h, %1.4s, #16") O1("shrn %0.2s, %1.2d, #1")
        OD("shrn2 %0.16b, %1.8h, #8") OD("xtn2 %0.8h, %1.4s")
        O1("xtn %0.8b, %1.8h") O1("xtn %0.4h, %1.4s") O1("xtn %0.2s, %1.2d")
        O2("tbl %0.16b, {%1.16b}, %2.16b") O2("tbl %0.8b, {%1.16b}, %2.8b")
        { v16 r; register v16 t0 __asm__("v20") = a, t1 __asm__("v21") = c, t2 __asm__("v22") = b, t3 __asm__("v23") = a;
          __asm__ volatile("tbl %0.16b, {v20.16b, v21.16b}, %5.16b" : "=w"(r) : "w"(t0), "w"(t1), "w"(t2), "w"(t3), "w"(b)); mixv(r);
          __asm__ volatile("tbl %0.16b, {v20.16b, v21.16b, v22.16b}, %5.16b" : "=w"(r) : "w"(t0), "w"(t1), "w"(t2), "w"(t3), "w"(b)); mixv(r);
          __asm__ volatile("tbl %0.16b, {v20.16b, v21.16b, v22.16b, v23.16b}, %5.16b" : "=w"(r) : "w"(t0), "w"(t1), "w"(t2), "w"(t3), "w"(b)); mixv(r);
          r = c; __asm__ volatile("tbx %0.16b, {v20.16b, v21.16b}, %5.16b" : "+w"(r) : "w"(t0), "w"(t1), "w"(t2), "w"(t3), "w"(b)); mixv(r); }
        OD("tbx %0.16b, {%1.16b}, %2.16b") OD("tbx %0.8b, {%1.16b}, %2.8b")
        O2("uzp1 %0.16b, %1.16b, %2.16b") O2("uzp2 %0.8h, %1.8h, %2.8h") O2("uzp1 %0.4s, %1.4s, %2.4s") O2("uzp2 %0.2d, %1.2d, %2.2d") O2("uzp1 %0.8b, %1.8b, %2.8b") O2("uzp2 %0.2s, %1.2s, %2.2s")
        O2("trn1 %0.16b, %1.16b, %2.16b") O2("trn2 %0.8h, %1.8h, %2.8h") O2("trn1 %0.4s, %1.4s, %2.4s") O2("trn2 %0.2d, %1.2d, %2.2d") O2("trn1 %0.4h, %1.4h, %2.4h") O2("trn2 %0.8b, %1.8b, %2.8b")
        O2("zip1 %0.16b, %1.16b, %2.16b") O2("zip2 %0.4s, %1.4s, %2.4s") O2("zip2 %0.4h, %1.4h, %2.4h")
        O2("ext %0.16b, %1.16b, %2.16b, #5") O2("ext %0.8b, %1.8b, %2.8b, #3")
        O2("cmhi %0.16b, %1.16b, %2.16b") O2("cmge %0.8h, %1.8h, %2.8h") O2("cmtst %0.4s, %1.4s, %2.4s") O2("cmeq %0.2d, %1.2d, %2.2d") O2("cmgt %0.2d, %1.2d, %2.2d")
        O2("umax %0.16b, %1.16b, %2.16b") O2("smin %0.8h, %1.8h, %2.8h") O2("umin %0.4s, %1.4s, %2.4s") O2("mul %0.8h, %1.8h, %2.8h")
        O1("addv b0, %1.16b\n mov %0.16b, v0.16b") O1("umaxv h0, %1.8h\n mov %0.16b, v0.16b") O1("sminv s0, %1.4s\n mov %0.16b, v0.16b")
        O1("dup %0.8h, %1.h[5]") O1("dup %0.4s, %1.s[3]") O1("cmlt %0.16b, %1.16b, #0") O1("neg %0.4s, %1.4s") O1("not %0.8b, %1.8b")
        OD("ins %0.s[2], %1.s[1]") OD("bsl %0.16b, %1.16b, %2.16b") OD("bif %0.16b, %1.16b, %2.16b")
    }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
