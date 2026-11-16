/* frente ISA/JIT: SQSHL, UQSHL e SQSHLU por imediato (vetor 8B..2D e escalar B/H/S/D) nativos ×
 * interpretador × QEMU, com FPSR.QC. Entradas pequenas, negativas e perto dos limites; um hash por
 * instrução para achar a que diverge. */
#include "a64t.h"
typedef unsigned char v16 __attribute__((vector_size(16)));
static unsigned long long h;
static void mixv(v16 v) { unsigned long long a[2]; __builtin_memcpy(a, &v, 16); h = (h ^ a[0]) * 1099511628211ull; h = (h ^ a[1]) * 1099511628211ull; }
static void mixq(void) { unsigned long long f; __asm__ volatile("mrs %0, fpsr\n msr fpsr, xzr" : "=r"(f)); h = (h ^ f) * 1099511628211ull; }
static v16 rnd(unsigned long long *s, int mode) {
    v16 v; for (int i = 0; i < 16; i++) { *s ^= *s << 13; *s ^= *s >> 7; *s ^= *s << 17; v[i] = (unsigned char)(*s >> 24); }
    if (mode == 1) for (int i = 0; i < 16; i++) v[i] = (i & 1) ? (v[i] & 1 ? 0xFF : 0) : v[i] & 0x0F;   /* pequenos ±, por byte e por lane maior */
    if (mode == 2) for (int i = 0; i < 16; i++) v[i] = (unsigned char)((v[i] & 3) ? 0 : 0xFF);         /* 0 e -1 */
    if (mode == 3) for (int i = 0; i < 16; i++) v[i] = (unsigned char)(v[i] & 0x81);                  /* bits de sinal e baixos */
    return v;
}
static unsigned long long hs[64];
static int nh;
#define O1(ins) { v16 r; __asm__ volatile("movi v31.16b, #0x5a\n mov %0.16b, v31.16b\n" ins : "=&w"(r) : "w"(a) : "v31"); mixv(r); mixq(); hs[k] = hs[k] * 31 + h; k++; }
int main(void) {
    unsigned long long s = 0xC0FFEE;
    for (int it = 0; it < 400; it++) {
        v16 a = rnd(&s, it % 4);
        int k = 0;
        O1("sqshl %0.8b, %1.8b, #3") O1("sqshl %0.16b, %1.16b, #7") O1("sqshl %0.4h, %1.4h, #1") O1("sqshl %0.8h, %1.8h, #13")
        O1("sqshl %0.2s, %1.2s, #5") O1("sqshl %0.4s, %1.4s, #31") O1("sqshl %0.2d, %1.2d, #9") O1("sqshl %0.2d, %1.2d, #0")
        O1("uqshl %0.8b, %1.8b, #2") O1("uqshl %0.16b, %1.16b, #0") O1("uqshl %0.4h, %1.4h, #15") O1("uqshl %0.8h, %1.8h, #4")
        O1("uqshl %0.2s, %1.2s, #30") O1("uqshl %0.4s, %1.4s, #1") O1("uqshl %0.2d, %1.2d, #63") O1("uqshl %0.2d, %1.2d, #17")
        O1("sqshlu %0.8b, %1.8b, #1") O1("sqshlu %0.16b, %1.16b, #0") O1("sqshlu %0.4h, %1.4h, #8") O1("sqshlu %0.8h, %1.8h, #15")
        O1("sqshlu %0.2s, %1.2s, #3") O1("sqshlu %0.4s, %1.4s, #31") O1("sqshlu %0.2d, %1.2d, #1") O1("sqshlu %0.2d, %1.2d, #62")
        O1("sqshl %b0, %b1, #4") O1("sqshl %h0, %h1, #9") O1("sqshl %s0, %s1, #20") O1("sqshl %d0, %d1, #40")
        O1("uqshl %b0, %b1, #7") O1("uqshl %h0, %h1, #1") O1("uqshl %s0, %s1, #0") O1("uqshl %d0, %d1, #63")
        O1("sqshlu %b0, %b1, #6") O1("sqshlu %h0, %h1, #0") O1("sqshlu %s0, %s1, #16") O1("sqshlu %d0, %d1, #2")
        nh = k;
    }
    for (int i = 0; i < nh; i++) { a64t_puts("op "); a64t_hex((unsigned long long)i); a64t_puts(" "); a64t_hex(hs[i]); a64t_puts("\n"); }
    a64t_puts("hash "); a64t_hex(h); a64t_puts("\n");
    return 0;
}
