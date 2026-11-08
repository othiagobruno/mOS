/* suíte diferencial C: ponto flutuante (aritmética IEEE, conversões, arredondamento, libm própria) */
#include "rt.h"
#include "m.h"

static const uint64_t DSPEC[] = {
    0x0000000000000000ull, 0x8000000000000000ull, 0x3FF0000000000000ull, 0xBFF0000000000000ull, 0x3FE0000000000000ull,
    0x4000000000000000ull, 0x7FF0000000000000ull, 0xFFF0000000000000ull, 0x7FF8000000000000ull, 0x7FEFFFFFFFFFFFFFull,
    0x0010000000000000ull, 0x000FFFFFFFFFFFFFull, 0x0000000000000001ull, 0x8000000000000001ull, 0x4330000000000000ull,
    0x4330000000000001ull, 0x3FF8000000000000ull, 0x4004000000000000ull, 0xC00C000000000000ull, 0x3FB999999999999Aull,
    0x400921FB54442D18ull, 0x3CB0000000000000ull, 0x43E0000000000000ull, 0x41DFFFFFFFC00000ull, 0xC1E0000000000000ull,
};
#define ND (sizeof DSPEC / sizeof DSPEC[0])
static double dv(unsigned i) {
    if (i < ND) return m_from(DSPEC[i]);
    uint64_t r = rnd64();
    switch (r & 3) {
    case 0: return (double)(int64_t)(r >> 20) / 1024.0;
    case 1: return m_from((r & 0x800FFFFFFFFFFFFFull) | ((uint64_t)(1023 - 40 + ((r >> 52) % 80)) << 52));
    case 2: return m_from(r & 0x801FFFFFFFFFFFFFull);           /* subnormais e pequenos */
    default: return m_from(r);
    }
}

static void arith(void) {
    hsh hd, hf, hm; h_init(&hd); h_init(&hf); h_init(&hm);
    for (unsigned i = 0; i < ND + 150; i++)
        for (unsigned j = 0; j < ND + 60; j++) {
            volatile double a = dv(i), b = dv(j + 3);
            h_d(&hd, a + b); h_d(&hd, a - b); h_d(&hd, a * b); h_d(&hd, a / b); h_d(&hd, m_sqrt(m_fabs(a)));
            h_u(&hd, a < b); h_u(&hd, a <= b); h_u(&hd, a == b); h_u(&hd, a != a); h_u(&hd, !(a >= b));
            volatile float fa = (float)a, fb = (float)b;
            h_f(&hf, fa + fb); h_f(&hf, fa - fb); h_f(&hf, fa * fb); h_f(&hf, fa / fb); h_f(&hf, __builtin_sqrtf(fa < 0 ? -fa : fa));
            h_u(&hf, fa < fb); h_u(&hf, fa == fb);
            h_d(&hd, (double)fa * (double)fb);
            h_d(&hm, __builtin_fma(a, b, (double)fa)); h_f(&hm, __builtin_fmaf(fa, fb, fa));
        }
    kv_x("f64 arith", hd.h); kv_x("f32 arith", hf.h);
    kv_x("fma f64/f32", hm.h);   /* linhas "fma ...": o QEMU do Windows erra fma (ver run.ps1) */
}

static void conv(void) {
    hsh h; h_init(&h);
    for (unsigned i = 0; i < 6000; i++) {
        double d = dv(i % 40 == 0 ? i / 40 : 1000);
        h_f(&h, (float)d);
        float f = (float)d; h_d(&h, (double)f);
        _Float16 x = (_Float16)f; h_f(&h, (float)x);
        _Float16 y = (_Float16)d; h_d(&h, (double)y);
        if (d == d && d > -9.2e18 && d < 9.2e18) { h_u(&h, (uint64_t)(int64_t)d); h_u(&h, (uint64_t)(int32_t)(d > -2.1e9 && d < 2.1e9 ? d : 0)); }
        if (d == d && d > -1 && d < 1.8e19) h_u(&h, (uint64_t)d);
        if (d == d && d > -1 && d < 4.2e9) h_u(&h, (uint64_t)(uint32_t)d);
        uint64_t r = rnd64();
        h_d(&h, (double)(int64_t)r); h_d(&h, (double)r); h_f(&h, (float)(int64_t)r); h_f(&h, (float)r);
        h_d(&h, (double)(int32_t)r); h_d(&h, (double)(uint32_t)r); h_f(&h, (float)(int32_t)r); h_f(&h, (float)(uint32_t)r);
        h_f(&h, (float)(int16_t)r); h_d(&h, (double)(int8_t)r);
    }
    kv_x("conversoes", h.h);
}

static void rounding(void) {
    hsh h; h_init(&h);
    for (unsigned i = 0; i < 5000; i++) {
        double d = dv(i % 30 == 0 ? i / 30 : 1000);
        if (i % 3 == 0) d = (double)(int64_t)(rnd64() >> 50) * 0.25;     /* meios inteiros */
        h_d(&h, __builtin_floor(d)); h_d(&h, __builtin_ceil(d)); h_d(&h, __builtin_trunc(d)); h_d(&h, __builtin_round(d));
        h_d(&h, __builtin_rint(d)); h_d(&h, __builtin_nearbyint(d));
        float f = (float)d;
        h_f(&h, __builtin_floorf(f)); h_f(&h, __builtin_ceilf(f)); h_f(&h, __builtin_truncf(f)); h_f(&h, __builtin_roundf(f)); h_f(&h, __builtin_rintf(f));
        if (d == d && m_fabs(d) < 9e18) h_u(&h, (uint64_t)__builtin_llround(d));
        if (d == d && m_fabs(d) < 9e18) h_u(&h, (uint64_t)__builtin_llrint(d));
        double e = dv(1000);
        if (d == d && e == e && (d != 0 || e != 0)) { h_d(&h, __builtin_fmin(d, e)); h_d(&h, __builtin_fmax(d, e)); }
        h_d(&h, __builtin_copysign(d, e)); h_d(&h, m_fabs(d));
    }
    kv_x("arredondamento", h.h);
}

static void libm(void) {
    hsh h; h_init(&h);
    for (int i = -400; i <= 400; i++) {
        double x = (double)i / 37.0;
        h_d(&h, m_exp(x)); h_d(&h, m_log(m_fabs(x) + 1e-3)); h_d(&h, m_sin(x * 3.0)); h_d(&h, m_cos(x * 3.0));
        h_d(&h, m_atan(x)); h_d(&h, m_atan2(x, 1.5 - x)); h_d(&h, m_tanh(x)); h_d(&h, m_hypot(x, 3.0 - x));
        h_d(&h, m_pow(1.0 + m_fabs(x), x / 3.0)); h_d(&h, m_pow(-2.0, (double)(i % 9)));
    }
    kv_x("libm", h.h);
    kv_d("exp(1)", m_exp(1.0)); kv_d("log(10)", m_log(10.0)); kv_d("sin(1e5)", m_sin(1e5)); kv_d("cos(-3)", m_cos(-3.0));
    kv_d("atan2(1,-1)", m_atan2(1.0, -1.0)); kv_d("pow(2,0.5)", m_pow(2.0, 0.5)); kv_d("pow(10,-300)", m_pow(10.0, -300.0));
    /* integração numérica e série: muita aritmética encadeada */
    double s = 0; for (int k = 1; k <= 200000; k++) s += 1.0 / ((double)k * (double)k);
    kv_d("zeta2", s);
    double a = 0, dx = 1.0 / 4096; for (int k = 0; k < 4096; k++) { double t = (k + 0.5) * dx; a += 4.0 / (1.0 + t * t) * dx; }
    kv_d("pi", a);
    float fs = 0; for (int k = 0; k < 100000; k++) fs += 0.1f; kv_d("soma 0.1f", (double)fs);
}

static void run(void) { put_s("c_fp\n"); arith(); conv(); rounding(); libm(); }
