/*
 * mOS — teste de host da libm do libSystem x86_64 (musl em userland/third_party/musl + mos/apple.c + fenv.c).
 * Os objetos freestanding entram com o prefixo mos_ (objcopy --prefix-symbols) e cada função é comparada com a
 * glibc do host em ULPs: entradas especiais e 20000 pseudoaleatórias por função, em double, float e long double
 * (80 bits, o mesmo formato do macOS x86_64). Também confere os valores da Apple (FP_*, sinpi/cospi/tanpi exatos,
 * __sincos_stret) e o fenv com o fenv_t da Apple. Rodar com tools/libm_hosttest.sh.
 */
#define _GNU_SOURCE
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int falhas, total;
#define CHECK(c, ...) do { total++; if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- distância em ULPs (sinais iguais, ambos finitos); NaN com NaN = 0 */
static double ulp_d(double a, double r) {
    if (isnan(a) && isnan(r)) return 0;
    if (isnan(a) || isnan(r)) return 1e30;
    if (a == r) return 0;
    if (isinf(a) || isinf(r)) return 1e30;
    int e; frexp(r, &e);
    double u = ldexp(1.0, (e < -1021 ? -1021 : e) - 53);
    return fabs(a - r) / u;
}
static double ulp_f(float a, float r) {
    if (isnan(a) && isnan(r)) return 0;
    if (isnan(a) || isnan(r)) return 1e30;
    if (a == r) return 0;
    if (isinf(a) || isinf(r)) return 1e30;
    int e; frexpf(r, &e);
    double u = ldexp(1.0, (e < -125 ? -125 : e) - 24);
    return fabs((double)a - (double)r) / u;
}
/* double/float contra a referência em long double (sem arredondar a referência) */
static double ulp_dl(double a, long double r) {
    if (isnan(a) && isnan(r)) return 0;
    if (isnan(a) || isnan(r)) return 1e30;
    if ((long double)a == r) return 0;
    if (fabsl(r) > DBL_MAX) return isinf(a) && !!signbit(a) == !!signbit(r) ? 0 : 1e30;
    if (isinf(a)) return 1e30;
    int e; frexpl(r, &e);
    long double u = ldexpl(1.0L, (e < -1021 ? -1021 : e) - 53);
    return (double)(fabsl((long double)a - r) / u);
}
static double ulp_fl(float a, long double r) {
    if (isnan(a) && isnan(r)) return 0;
    if (isnan(a) || isnan(r)) return 1e30;
    if ((long double)a == r) return 0;
    if (fabsl(r) > FLT_MAX) return isinf(a) && !!signbit(a) == !!signbit(r) ? 0 : 1e30;
    if (isinf(a)) return 1e30;
    int e; frexpl(r, &e);
    long double u = ldexpl(1.0L, (e < -125 ? -125 : e) - 24);
    return (double)(fabsl((long double)a - r) / u);
}
static double ulp_l(long double a, long double r) {
    if (isnan(a) && isnan(r)) return 0;
    if (isnan(a) || isnan(r)) return 1e30;
    if (a == r) return 0;
    if (isinf(a) || isinf(r)) return 1e30;
    int e; frexpl(r, &e);
    long double u = ldexpl(1.0L, (e < -16381 ? -16381 : e) - 64);
    return (double)(fabsl(a - r) / u);
}

static uint64_t rs = 0x9E3779B97F4A7C15ull;
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
/* mistura: faixa de trabalho [lo, hi] e de vez em quando um padrão de bits qualquer */
static double gen(double lo, double hi) {
    uint64_t r = rnd();
    if ((r & 15) == 0) { double d; uint64_t b = rnd(); memcpy(&d, &b, 8); if (isfinite(d) && d >= lo && d <= hi) return d; }
    return lo + (hi - lo) * ((double)(r >> 11) / 9007199254740992.0);
}

static const double esp[] = {0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, 1e-300, -1e-300, 4.9e-324, 1e300, -1e300, 0.1, 3.0,
                             M_PI, M_PI_2, M_PI_4, 710.0, -745.0, 1e22, 1e-10, 100.0, INFINITY, -INFINITY, NAN};
#define NESP (sizeof esp / sizeof esp[0])

#define DECL1(F) double mos_##F(double); float mos_##F##f(float); long double mos_##F##l(long double);
#define DECL2(F) double mos_##F(double, double); float mos_##F##f(float, float); long double mos_##F##l(long double, long double);
DECL1(sin) DECL1(cos) DECL1(tan) DECL1(asin) DECL1(acos) DECL1(atan) DECL1(sinh) DECL1(cosh) DECL1(tanh) DECL1(asinh)
DECL1(acosh) DECL1(atanh) DECL1(exp) DECL1(exp2) DECL1(expm1) DECL1(log) DECL1(log2) DECL1(log10) DECL1(log1p)
DECL1(cbrt) DECL1(sqrt) DECL1(erf) DECL1(erfc) DECL1(tgamma) DECL1(lgamma) DECL1(floor) DECL1(ceil) DECL1(trunc)
DECL1(round) DECL1(rint) DECL1(nearbyint) DECL1(fabs) DECL1(logb)
DECL2(atan2) DECL2(pow) DECL2(hypot) DECL2(fmod) DECL2(remainder) DECL2(fmin) DECL2(fmax) DECL2(fdim) DECL2(copysign)
DECL2(nextafter)
double mos_fma(double, double, double); float mos_fmaf(float, float, float); long double mos_fmal(long double, long double, long double);
double mos_frexp(double, int *); double mos_ldexp(double, int); double mos_modf(double, double *); double mos_remquo(double, double, int *);
long double mos_frexpl(long double, int *); long double mos_ldexpl(long double, int);
double mos_lgamma_r(double, int *); float mos_lgammaf_r(float, int *); long double mos_lgammal_r(long double, int *);
extern int mos_signgam;
long mos_lround(double); long long mos_llround(double); long long mos_llroundl(long double); long mos_lrint(double);
int mos_ilogb(double); double mos_j0(double), mos_j1(double), mos_y0(double), mos_y1(double), mos_jn(int, double);
int mos___fpclassifyd(double), mos___fpclassifyf(float), mos___fpclassifyl(long double), mos___isnand(double), mos___signbitd(double);
double mos___sinpi(double), mos___cospi(double), mos___tanpi(double), mos___exp10(double);
float mos___sinpif(float), mos___cospif(float);
typedef struct { double s, c; } sc_t;
sc_t mos___sincos_stret(double), mos___sincospi_stret(double);
double mos_nan(const char *);

typedef double (*d1)(double); typedef float (*f1)(float); typedef long double (*l1)(long double);
typedef double (*d2)(double, double); typedef float (*f2)(float, float); typedef long double (*l2)(long double, long double);

static double lim_d = 1.0, lim_f = 1.0, lim_l = 1.0;   /* ajustados por função abaixo */

static void t1(const char *n, d1 m, f1 mf, l1 ml, l1 gl, double lo, double hi) {
    double wd = 0, wf = 0, wl = 0, xd = 0, xf = 0, xl = 0;
    for (int i = 0; i < 20000 + (int)NESP; i++) {
        double x = i < (int)NESP ? esp[i] : gen(lo, hi);
        double u = ulp_dl(m(x), gl(x)); if (u > wd) { wd = u; xd = x; }
        float xf_ = (float)x; double v = ulp_fl(mf(xf_), gl(xf_)); if (v > wf) { wf = v; xf = x; }
        long double xl_ = (long double)x * (1.0L + (long double)(rnd() >> 40) * 0x1p-64L);
        if (i < (int)NESP) xl_ = x;
        double w = ulp_l(ml(xl_), gl(xl_)); if (w > wl) { wl = w; xl = (double)xl_; }
    }
    CHECK(wd <= lim_d, "%s: %.2f ulp em %.17g", n, wd, xd);
    CHECK(wf <= lim_f, "%sf: %.2f ulp em %.9g", n, wf, xf);
    CHECK(wl <= lim_l, "%sl: %.2f ulp em %.20Lg", n, wl, (long double)xl);
    printf("  %-10s double %5.2f  float %5.2f  long double %5.2f ulp\n", n, wd, wf, wl);
}
static void t2(const char *n, d2 m, f2 mf, l2 ml, l2 gl, double lo, double hi) {
    double wd = 0, wf = 0, wl = 0;
    for (int i = 0; i < 20000 + (int)(NESP * NESP); i++) {
        double x, y;
        if (i < (int)(NESP * NESP)) { x = esp[i / NESP]; y = esp[i % NESP]; } else { x = gen(lo, hi); y = gen(lo, hi); }
        double u = ulp_dl(m(x, y), gl(x, y)); if (u > wd) wd = u;
        double v = ulp_fl(mf((float)x, (float)y), gl((float)x, (float)y)); if (v > wf) wf = v;
        double w = ulp_l(ml(x, y), gl(x, y)); if (w > wl) wl = w;
    }
    CHECK(wd <= lim_d && wf <= lim_f && wl <= lim_l, "%s: %.2f / %.2f / %.2f ulp", n, wd, wf, wl);
    printf("  %-10s double %5.2f  float %5.2f  long double %5.2f ulp\n", n, wd, wf, wl);
}
#define T1(F, lo, hi) t1(#F, mos_##F, mos_##F##f, mos_##F##l, F##l, lo, hi)
#define T2(F, lo, hi) t2(#F, mos_##F, mos_##F##f, mos_##F##l, F##l, lo, hi)

/* funções exatas: comparadas com a double/float da glibc (que também são exatas) */
static void t1e(const char *n, d1 m, d1 g, f1 mf, f1 gf, l1 ml, l1 gl, double lo, double hi) {
    double w = 0;
    for (int i = 0; i < 20000 + (int)NESP; i++) {
        double x = i < (int)NESP ? esp[i] : gen(lo, hi);
        long double xl = (long double)x * (1.0L + (long double)(rnd() >> 40) * 0x1p-64L);
        double u = ulp_d(m(x), g(x)) + ulp_f(mf((float)x), gf((float)x)) + ulp_l(ml(xl), gl(xl));
        if (u > w) w = u;
    }
    CHECK(w == 0, "%s: %.2f ulp", n, w);
    printf("  %-10s exata\n", n);
}
static void t2e(const char *n, d2 m, d2 g, f2 mf, f2 gf, l2 ml, l2 gl, double lo, double hi) {
    double w = 0;
    for (int i = 0; i < 20000 + (int)(NESP * NESP); i++) {
        double x, y;
        if (i < (int)(NESP * NESP)) { x = esp[i / NESP]; y = esp[i % NESP]; } else { x = gen(lo, hi); y = gen(lo, hi); }
        double u = ulp_d(m(x, y), g(x, y)) + ulp_f(mf((float)x, (float)y), gf((float)x, (float)y)) + ulp_l(ml(x, y), gl(x, y));
        if (u > w) w = u;
    }
    CHECK(w == 0, "%s: %.2f ulp", n, w);
    printf("  %-10s exata\n", n);
}
#define T1E(F, lo, hi) t1e(#F, mos_##F, F, mos_##F##f, F##f, mos_##F##l, F##l, lo, hi)
#define T2E(F, lo, hi) t2e(#F, mos_##F, F, mos_##F##f, F##f, mos_##F##l, F##l, lo, hi)

/* fenv_t da Apple no x86_64 */
typedef struct { unsigned short control, status; unsigned mxcsr; char res[8]; } apple_fenv_t;
int mos_fegetround(void), mos_fesetround(int), mos_feclearexcept(int), mos_feraiseexcept(int), mos_fetestexcept(int);
int mos_fegetenv(apple_fenv_t *), mos_fesetenv(const apple_fenv_t *), mos_feholdexcept(apple_fenv_t *), mos_feupdateenv(const apple_fenv_t *);
extern const apple_fenv_t mos__FE_DFL_ENV;

int main(void) {
    printf("libm (musl) contra a glibc do host:\n");
    /* limites: 1 ULP onde o musl promete, e os máximos que a própria glibc documenta para x86_64 nas outras
     * (libm-test-ulps: sinh/tanh/asinh/atanh/atan2 2, erfc 3, tgamma 9, lgamma 4). long double: a referência (glibc) erra até 2-3. */
    lim_d = 1.0; lim_f = 1.0; lim_l = 4.0;
    T1(sin, -1e6, 1e6); T1(cos, -1e6, 1e6); T1(tan, -1e4, 1e4);
    T1(asin, -1, 1); T1(acos, -1, 1); T1(atan, -1e10, 1e10);
    T1(cosh, -745, 745); T1(acosh, 1, 1e10);
    T1(exp, -745, 709); T1(exp2, -1074, 1023); T1(expm1, -40, 709);
    T1(log, 0, 1e300); T1(log2, 0, 1e300); T1(log10, 0, 1e300); T1(log1p, -1, 1e10);
    T1(cbrt, -1e300, 1e300); T1(sqrt, 0, 1e300); T1(erf, -6, 6);
    T2(hypot, -1e300, 1e300);
    /* powl é o algoritmo do Cephes: até 5 ULPs na faixa comum e ~20 perto do overflow (docs/runtime-c-cxx.md) */
    lim_l = 24;
    T2(pow, 0, 100);
    lim_d = 2.0; lim_f = 2.0; lim_l = 4.0;
    T1(sinh, -745, 745); T1(tanh, -20, 20); T1(asinh, -1e10, 1e10); T1(atanh, -1, 1); T2(atan2, -1e10, 1e10);
    lim_d = 3.0; lim_f = 3.0; lim_l = 4.0;
    T1(erfc, -6, 27);
    lim_d = 9; lim_f = 4; lim_l = 16;
    T1(tgamma, -30, 170); T1(lgamma, 0.001, 1e6);
    T1E(floor, -1e20, 1e20); T1E(ceil, -1e20, 1e20); T1E(trunc, -1e20, 1e20); T1E(round, -1e20, 1e20);
    T1E(rint, -1e20, 1e20); T1E(nearbyint, -1e20, 1e20); T1E(fabs, -1e300, 1e300); T1E(logb, -1e300, 1e300);
    T2E(fmod, -1e10, 1e10); T2E(remainder, -1e10, 1e10); T2E(fmin, -1e10, 1e10); T2E(fmax, -1e10, 1e10);
    T2E(fdim, -1e10, 1e10); T2E(copysign, -1e10, 1e10); T2E(nextafter, -10, 10);

    /* ---- fma: exato */
    for (int i = 0; i < 20000; i++) {
        double a = gen(-1e10, 1e10), b = gen(-1e10, 1e10), c = gen(-1e20, 1e20);
        if (mos_fma(a, b, c) != fma(a, b, c)) { CHECK(0, "fma(%g,%g,%g)", a, b, c); break; }
        if (mos_fmal(a, b, c) != fmal(a, b, c)) { CHECK(0, "fmal(%g,%g,%g)", a, b, c); break; }
        if (mos_fmaf((float)a, (float)b, (float)c) != fmaf((float)a, (float)b, (float)c)) { CHECK(0, "fmaf"); break; }
    }
    /* ---- arredondamentos que a libm antiga errava */
    CHECK(mos_round(0.49999999999999994) == 0.0, "round(0.49999999999999994) = %g", mos_round(0.49999999999999994));
    CHECK(mos_round(-2.5) == -3.0 && mos_lround(2.5) == 3 && mos_llround(-2.5) == -3, "round meio");
    CHECK(mos_llroundl(4611686018427387903.5L) == 4611686018427387904LL, "llroundl grande");
    CHECK(mos_log1p(1e-20) == 1e-20, "log1p(1e-20) = %g", mos_log1p(1e-20));
    CHECK(mos_expm1(1e-20) == 1e-20, "expm1(1e-20)");
    CHECK(fabs(mos_sin(1e22) - sin(1e22)) == 0, "sin(1e22) = %.17g", mos_sin(1e22));
    CHECK(mos_ilogb(0) == FP_ILOGB0 && mos_ilogb(8) == 3, "ilogb");
    int e; double fr = mos_frexp(5e-324, &e); CHECK(fr == 0.5 && e == -1073, "frexp subnormal");
    long double frl = mos_frexpl(1e-4940L, &e); CHECK(frl == frexpl(1e-4940L, &e), "frexpl");
    CHECK(mos_ldexpl(1.0L, 16383) == ldexpl(1.0L, 16383), "ldexpl");
    double ip; CHECK(mos_modf(-3.75, &ip) == -0.75 && ip == -3.0, "modf");
    int q; double rq = mos_remquo(10.0, 3.0, &q); CHECK(rq == 1.0 && (q & 7) == 3, "remquo %g %d", rq, q);
    int sg; mos_lgamma_r(-0.5, &sg); CHECK(sg == -1, "lgamma_r sinal");
    mos_signgam = 0; (void)mos_lgamma(-2.5); CHECK(mos_signgam == -1, "signgam = %d", mos_signgam);
    mos_lgammal_r(-0.5L, &sg); CHECK(sg == -1, "lgammal_r sinal");
    CHECK(fabs(mos_j0(2.0) - j0(2.0)) < 1e-15 && fabs(mos_y1(3.0) - y1(3.0)) < 1e-15 && fabs(mos_jn(5, 7.0) - jn(5, 7.0)) < 1e-15, "Bessel");
    CHECK(isnan(mos_nan("")), "nan");

    /* ---- nomes da Apple */
    CHECK(mos___fpclassifyd(NAN) == 1 && mos___fpclassifyd(INFINITY) == 2 && mos___fpclassifyd(0.0) == 3 &&
          mos___fpclassifyd(1.0) == 4 && mos___fpclassifyd(1e-310) == 5, "__fpclassifyd com FP_* da Apple");
    CHECK(mos___fpclassifyf(1e-40f) == 5 && mos___fpclassifyl(0.0L) == 3 && mos___fpclassifyl(1e-4940L) == 5, "__fpclassifyf/l");
    CHECK(mos___isnand(NAN) && !mos___isnand(1) && mos___signbitd(-0.0), "__isnand/__signbitd");
    CHECK(mos___sinpi(0.5) == 1.0 && mos___sinpi(1.0) == 0.0 && !signbit(mos___sinpi(1.0)) && signbit(mos___sinpi(-1.0)), "sinpi exato");
    CHECK(mos___sinpi(1.5) == -1.0 && mos___sinpi(0x1p52 + 1) == 0.0 && mos___sinpi(1e300) == 0.0, "sinpi grande");
    CHECK(mos___cospi(0.5) == 0.0 && !signbit(mos___cospi(0.5)) && mos___cospi(1.0) == -1.0 && mos___cospi(0x1p52 + 1) == -1.0, "cospi");
    CHECK(mos___tanpi(0.25) == 1.0 && mos___tanpi(0.5) == INFINITY && mos___tanpi(1.5) == -INFINITY && mos___tanpi(-0.25) == -1.0, "tanpi");
    {
        double w = 0;
        for (int i = 0; i < 20000; i++) {
            double x = gen(-1000, 1000);
            /* x = k/2 + r exato, |r| <= 1/4; sin/cos de pi*r em long double */
            double k = rint(2 * x); long double r = (long double)x - (long double)k / 2, s = sinl(M_PIl * r), c = cosl(M_PIl * r);
            int q = (int)fmod(fabs(k), 4.0); if (k < 0) q = (4 - q) & 3;
            long double rs = q == 0 ? s : q == 1 ? c : q == 2 ? -s : -c, rc = q == 0 ? c : q == 1 ? -s : q == 2 ? -c : s;
            double u = ulp_dl(mos___sinpi(x), rs); if (u > w) w = u;
            u = ulp_dl(mos___cospi(x), rc); if (u > w) w = u;
        }
        CHECK(w <= 1.5, "sinpi/cospi: %.2f ulp", w);
        printf("  sinpi/cospi double %5.2f ulp\n", w);
    }
    CHECK(mos___sinpif(0.5f) == 1.0f && mos___cospif(1.0f) == -1.0f, "sinpif/cospif");
    sc_t sc = mos___sincos_stret(0.7); CHECK(sc.s == sin(0.7) && sc.c == cos(0.7), "__sincos_stret");
    sc = mos___sincospi_stret(0.5); CHECK(sc.s == 1.0 && sc.c == 0.0, "__sincospi_stret");
    CHECK(mos___exp10(3.0) == 1000.0, "__exp10");

    /* ---- fenv da Apple */
    CHECK(sizeof(apple_fenv_t) == 16, "fenv_t de 16 bytes");
    CHECK(mos__FE_DFL_ENV.control == 0x037F && mos__FE_DFL_ENV.mxcsr == 0x1F80, "_FE_DFL_ENV");
    CHECK(mos_fegetround() == 0, "arredondamento ao mais próximo");
    CHECK(mos_fesetround(0x400) == 0 && mos_fegetround() == 0x400 && fegetround() == FE_DOWNWARD, "FE_DOWNWARD no x87 e no SSE");
    volatile double one = 1.0, three = 3.0;
    double down = one / three;
    mos_fesetround(0x800); double up = one / three;
    CHECK(up > down, "SSE segue o arredondamento");
    volatile long double l1 = 1.0L, l3 = 3.0L; long double lu = l1 / l3; mos_fesetround(0x400); long double ld = l1 / l3;
    CHECK(lu > ld, "x87 segue o arredondamento");
    mos_fesetround(0);
    CHECK(mos_fesetround(0x1234) != 0, "fesetround inválido");
    mos_feclearexcept(0x3F);
    volatile double z = 0.0; volatile double r = one / z; (void)r;
    CHECK(mos_fetestexcept(0x04) == 0x04, "FE_DIVBYZERO");
    mos_feclearexcept(0x04); CHECK(mos_fetestexcept(0x3F) == 0, "feclearexcept");
    mos_feraiseexcept(0x20 | 0x08); CHECK(mos_fetestexcept(0x28) == 0x28, "feraiseexcept");
    apple_fenv_t env; mos_feholdexcept(&env);
    CHECK(mos_fetestexcept(0x3F) == 0, "feholdexcept limpa");
    r = one / z;
    mos_feupdateenv(&env);
    CHECK(mos_fetestexcept(0x2C) == 0x2C, "feupdateenv junta as pendentes");
    mos_fesetenv(&mos__FE_DFL_ENV);
    CHECK(mos_fetestexcept(0x3F) == 0 && mos_fegetround() == 0, "fesetenv(FE_DFL_ENV)");
    mos_fesetround(0xC00); mos_fegetenv(&env); mos_fesetround(0);
    CHECK((env.control & 0xC00) == 0xC00 && (env.mxcsr & 0x6000) == 0x6000, "fegetenv guarda os dois");
    mos_fesetenv(&env); CHECK(mos_fegetround() == 0xC00, "fesetenv devolve"); mos_fesetround(0);

    printf("libm: %d/%d ok\n", total - falhas, total);
    return falhas != 0;
}
