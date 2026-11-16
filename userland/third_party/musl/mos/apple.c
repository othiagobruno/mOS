/* mOS: funções de <math.h> que só a libm da Apple exporta, em cima da libm do musl.
 * Valores de classificação: FP_NAN 1, FP_INFINITE 2, FP_ZERO 3, FP_NORMAL 4, FP_SUBNORMAL 5 (mos/include/math.h). */
#include <math.h>
#include <stdint.h>
#include "libm.h"

/* ---- classificação (o <math.h> antigo da Apple chama estes em vez de inline) */
int __fpclassifyd(double x) { return __fpclassify(x); }
int __isnand(double x) { return isnan(x); }
int __isnanf(float x) { return isnan(x); }
int __isnanl(long double x) { return isnan(x); }
int __isinfd(double x) { return isinf(x); }
int __isinff(float x) { return isinf(x); }
int __isinfl(long double x) { return isinf(x); }
int __isfinited(double x) { return isfinite(x); }
int __isfinitef(float x) { return isfinite(x); }
int __isfinitel(long double x) { return isfinite(x); }
int __isnormald(double x) { return isnormal(x); }
int __isnormalf(float x) { return isnormal(x); }
int __isnormall(long double x) { return isnormal(x); }
int __signbitd(double x) { return __signbit(x); }
double __inf(void) { return INFINITY; }
float __inff(void) { return INFINITY; }
long double __infl(void) { return INFINITY; }
float __nan(void) { return NAN; }
int __math_errhandling(void) { return MATH_ERREXCEPT; }

/* ---- __exp10: nome que o clang gera para pow(10, x) em alvos Darwin */
double __exp10(double x) { return exp10(x); }
float __exp10f(float x) { return exp10f(x); }

/* ---- sinpi/cospi/tanpi: x = k/2 + r com r em [-1/4, 1/4] exato (2x - rint(2x) não arredonda para |x| < 2^52) e
 * pi*r em precisão dupla estendida (pi = PI_HI + PI_LO, produto com fma), passado aos núcleos __sin/__cos/__tan do musl. */
static const double PI_HI = 3.141592653589793116, PI_LO = 1.2246467991473532072e-16;

static void pi_mul(double r, double *hi, double *lo) {
    double h = PI_HI * r;
    *lo = __builtin_fma(PI_HI, r, -h) + PI_LO * r;
    *hi = h;
}
static double sin_pir(double r) { double h, l; if (r == 0) return r; pi_mul(r, &h, &l); return __sin(h, l, 1); }
static double cos_pir(double r) { double h, l; pi_mul(r, &h, &l); return __cos(h, l); }

/* reduz |x| finito: devolve k mod 4 e r. De 2^52 a 2^53 todo double é inteiro (k = 2|x|); acima, inteiro par. */
static int pi_reduce(double ax, double *r) {
    if (ax >= 0x1p52) {
        *r = 0;
        return ax >= 0x1p53 ? 0 : (int)fmod(ax, 2.0) * 2;
    }
    double t = ax * 2, k = rint(t);
    *r = (t - k) * 0.5;
    return (int)fmod(k, 4.0);
}

double __sinpi(double x) {
    if (!isfinite(x)) return x - x;
    double ax = fabs(x), r, y;
    int q = pi_reduce(ax, &r);
    switch (q) {
    case 0: y = sin_pir(r); break;
    case 1: y = cos_pir(r); break;
    case 2: y = -sin_pir(r); break;
    default: y = -cos_pir(r); break;
    }
    if (y == 0) y = 0;                        /* sinpi(n) = +0 para n > 0 */
    return signbit(x) ? -y : y;
}
double __cospi(double x) {
    if (!isfinite(x)) return x - x;
    double ax = fabs(x), r, y;
    int q = pi_reduce(ax, &r);
    switch (q) {
    case 0: y = cos_pir(r); break;
    case 1: y = -sin_pir(r); break;
    case 2: y = -cos_pir(r); break;
    default: y = sin_pir(r); break;
    }
    return y == 0 ? 0 : y;                    /* cospi(n + 1/2) = +0 */
}
double __tanpi(double x) {
    if (!isfinite(x)) return x - x;
    double ax = fabs(x), r, h, l, y;
    int q = pi_reduce(ax, &r);
    if (r == 0) {
        if (q & 1) y = q == 1 ? INFINITY : -INFINITY;   /* polo em n + 1/2 */
        else y = q == 0 ? 0.0 : -0.0;                    /* tanpi(n): +0 para n par, -0 para n ímpar */
    } else {
        pi_mul(r, &h, &l);
        y = __tan(h, l, q & 1);               /* q ímpar: -1/tan(pi r) */
    }
    return signbit(x) ? -y : y;
}
float __sinpif(float x) { return (float)__sinpi(x); }
float __cospif(float x) { return (float)__cospi(x); }
float __tanpif(float x) { return (float)__tanpi(x); }

/* ---- __sincos_stret: o clang/swiftc (-O) troca sin(x) e cos(x) do mesmo x por uma chamada que devolve os dois */
typedef struct { double s, c; } sincos_d;
typedef struct { float s, c; } sincos_f;
sincos_d __sincos_stret(double x) { sincos_d r; sincos(x, &r.s, &r.c); return r; }
sincos_f __sincosf_stret(float x) { sincos_f r; sincosf(x, &r.s, &r.c); return r; }
sincos_d __sincospi_stret(double x) { sincos_d r = {__sinpi(x), __cospi(x)}; return r; }
sincos_f __sincospif_stret(float x) { sincos_f r = {__sinpif(x), __cospif(x)}; return r; }
