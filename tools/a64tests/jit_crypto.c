/* frente JIT: cripto nativa × interpretador — AESE/AESD/AESMC/AESIMC (AES-NI), PMULL/PMULL2 de 64
 * bits (PCLMULQDQ), EOR3/BCAX/RAX1/XAR, com 500 entradas aleatórias e uma cifra AES-128 de 10
 * rodadas encadeadas. Hash dos resultados. */
#include "a64t.h"
typedef unsigned char v16 __attribute__((vector_size(16)));
static unsigned long long h = 1469598103934665603ull;
static void mixv(v16 v) { unsigned long long a[2]; __builtin_memcpy(a, &v, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static v16 rnd(unsigned long long *s) {
    v16 v; for (int i = 0; i < 16; i++) { *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; v[i] = (unsigned char)(*s >> 24); }
    return v;
}
#define OD(ins) { v16 r = c; __asm__ volatile(ins : "+w"(r) : "w"(a), "w"(b)); mixv(r); }
#define O1(ins) { v16 r; __asm__ volatile(ins : "=w"(r) : "w"(a)); mixv(r); }
#define O2(ins) { v16 r; __asm__ volatile(ins : "=w"(r) : "w"(a), "w"(b)); mixv(r); }
int main(void) {
    unsigned long long s = 0xC0FFEE;
    for (int it = 0; it < 500; it++) {
        v16 a = rnd(&s), b = rnd(&s), c = rnd(&s);
        OD("aese %0.16b, %1.16b") OD("aesd %0.16b, %1.16b") O1("aesmc %0.16b, %1.16b") O1("aesimc %0.16b, %1.16b")
        O2("pmull %0.1q, %1.1d, %2.1d") O2("pmull2 %0.1q, %1.2d, %2.2d")
        { v16 r; __asm__ volatile("eor3 %0.16b, %1.16b, %2.16b, %3.16b" : "=w"(r) : "w"(a), "w"(b), "w"(c)); mixv(r); }
        { v16 r; __asm__ volatile("bcax %0.16b, %1.16b, %2.16b, %3.16b" : "=w"(r) : "w"(a), "w"(b), "w"(c)); mixv(r); }
        O2("rax1 %0.2d, %1.2d, %2.2d") O2("xar %0.2d, %1.2d, %2.2d, #17") O2("xar %0.2d, %1.2d, %2.2d, #0") O2("xar %0.2d, %1.2d, %2.2d, #63")
    }
    v16 st = rnd(&s);
    for (int blk = 0; blk < 2000; blk++) {           /* rodadas de AES encadeadas, como numa cifra real */
        v16 k = rnd(&s);
        for (int r = 0; r < 9; r++) __asm__ volatile("aese %0.16b, %1.16b\n aesmc %0.16b, %0.16b" : "+w"(st) : "w"(k));
        __asm__ volatile("aese %0.16b, %1.16b\n eor %0.16b, %0.16b, %1.16b" : "+w"(st) : "w"(k));
        for (int r = 0; r < 9; r++) __asm__ volatile("aesd %0.16b, %1.16b\n aesimc %0.16b, %0.16b" : "+w"(st) : "w"(k));
        mixv(st);
    }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
