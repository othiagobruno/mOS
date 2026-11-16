/*
 * mOS libSystem — a família drand48: gerador congruente linear de 48 bits x = (a*x + c) mod 2^48 (a = 0x5DEECE66D,
 * c = 0xB por padrão, configuráveis por lcong48), o mesmo da BSD/glibc. srand48/seed48/lcong48 semeiam; drand48/
 * erand48 dão [0,1); lrand48/nrand48 dão [0, 2^31); mrand48/jrand48 dão [-2^31, 2^31). Teste: tools/rand48_hosttest.sh
 */
#include "sys.h"

/* gerador de 48 bits da família drand48: x = (a*x + c) mod 2^48, a/c configuráveis por lcong48 */
static u64 rand48_x = 0x1234ABCD330Eull;
static u64 rand48_a = 0x5DEECE66Dull;
static u16 rand48_c = 0xB;
static u64 lcg48_step(u64 *x) { *x = (*x * rand48_a + rand48_c) & 0xFFFFFFFFFFFFull; return *x; }
static u32 rand48_next(void) { return (u32)(lcg48_step(&rand48_x) >> 16); }
/* [0,1) a partir dos 48 bits altos de um estado de 48 bits */
static double erand48_x(u64 x) { return (double)x / 281474976710656.0; }   /* 2^48 */
void srand48(long s) { rand48_x = ((u64)(u32)s << 16 | 0x330E) & 0xFFFFFFFFFFFFull; rand48_a = 0x5DEECE66Dull; rand48_c = 0xB; }
long mrand48(void) { return (long)(i32)rand48_next(); }
long lrand48(void) { return (long)(lcg48_step(&rand48_x) >> 17); }     /* [0, 2^31) */
double drand48(void) { return erand48_x(lcg48_step(&rand48_x)); }
/* variantes com estado explícito xsubi[3] (16 bits cada, little-endian como no Darwin) */
static u64 xsubi_get(const u16 *x) { return (u64)x[0] | (u64)x[1] << 16 | (u64)x[2] << 32; }
static void xsubi_put(u16 *x, u64 v) { x[0] = (u16)v; x[1] = (u16)(v >> 16); x[2] = (u16)(v >> 32); }
double erand48(u16 x[3]) { u64 v = xsubi_get(x); u64 r = (v * rand48_a + rand48_c) & 0xFFFFFFFFFFFFull; xsubi_put(x, r); return erand48_x(r); }
long nrand48(u16 x[3]) { u64 v = xsubi_get(x); u64 r = (v * rand48_a + rand48_c) & 0xFFFFFFFFFFFFull; xsubi_put(x, r); return (long)(r >> 17); }
long jrand48(u16 x[3]) { u64 v = xsubi_get(x); u64 r = (v * rand48_a + rand48_c) & 0xFFFFFFFFFFFFull; xsubi_put(x, r); return (long)(i32)(u32)(r >> 16); }
u16 *seed48(u16 seed16v[3]) {
    static u16 prev[3];
    xsubi_put(prev, rand48_x);
    rand48_x = xsubi_get(seed16v);
    rand48_a = 0x5DEECE66Dull;
    rand48_c = 0xB;
    return prev;
}
void lcong48(u16 p[7]) {
    rand48_x = xsubi_get(p);
    rand48_a = (u64)p[3] | (u64)p[4] << 16 | (u64)p[5] << 32;
    rand48_c = p[6];
}
