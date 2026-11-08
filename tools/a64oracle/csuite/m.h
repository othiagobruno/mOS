/*
 * mOS — suíte diferencial C: libm mínima freestanding. Só usa + - * / e sqrt (IEEE, corretamente
 * arredondados nos dois lados) em ordem fixa, então o resultado é bit a bit o mesmo no x86_64 e
 * no arm64 — qualquer diferença é do tradutor. Precisão de ~1 ulp, sem pretensão de libm séria.
 */
#pragma once
#include <stdint.h>

static inline uint64_t m_bits(double x) { uint64_t b; __builtin_memcpy(&b, &x, 8); return b; }
static inline double m_from(uint64_t b) { double x; __builtin_memcpy(&x, &b, 8); return x; }
static inline double m_fabs(double x) { return m_from(m_bits(x) & 0x7FFFFFFFFFFFFFFFull); }
static inline int m_isnan(double x) { return x != x; }
static inline int m_isinf(double x) { return (m_bits(x) & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull; }
static inline double m_sqrt(double x) { return __builtin_sqrt(x); }
static inline double m_floor(double x) { return __builtin_floor(x); }
static inline double m_trunc(double x) { return __builtin_trunc(x); }
/* 2^k para k em [-1022, 1023] */
static inline double m_pow2i(int k) { return m_from((uint64_t)(k + 1023) << 52); }
static double m_scalbn(double x, int k) {
    while (k > 1000) { x *= m_pow2i(1000); k -= 1000; }
    while (k < -1000) { x *= m_pow2i(-1000); k += 1000; }
    return x * m_pow2i(k);
}

static double m_exp(double x) {
    if (m_isnan(x)) return x;
    if (x > 709.78) return m_from(0x7FF0000000000000ull);
    if (x < -745.2) return 0.0;
    const double ln2hi = 6.93147180369123816490e-01, ln2lo = 1.90821492927058770002e-10, inv = 1.44269504088896338700e+00;
    double kd = m_floor(x * inv + 0.5);
    int k = (int)kd;
    double r = (x - kd * ln2hi) - kd * ln2lo;
    /* Taylor até r^13/13! (|r| <= 0.35) */
    double t = 1.0, s = 1.0;
    for (int i = 1; i <= 13; i++) { t = t * r / (double)i; s = s + t; }
    return m_scalbn(s, k);
}

static double m_log(double x) {
    if (m_isnan(x) || x < 0) return m_from(0x7FF8000000000000ull);
    if (x == 0) return m_from(0xFFF0000000000000ull);
    if (m_isinf(x)) return x;
    uint64_t b = m_bits(x);
    int e = (int)((b >> 52) & 0x7FF);
    if (e == 0) { x *= m_pow2i(54); b = m_bits(x); e = (int)((b >> 52) & 0x7FF) - 54; }
    e -= 1023;
    double m = m_from((b & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull);  /* [1, 2) */
    if (m > 1.41421356237309504880) { m = m * 0.5; e++; }
    /* log(m) = 2 atanh(z), z = (m-1)/(m+1), |z| < 0.172 */
    double z = (m - 1.0) / (m + 1.0), z2 = z * z, t = z, s = 0.0;
    for (int i = 1; i <= 27; i += 2) { s = s + t / (double)i; t = t * z2; }
    const double ln2hi = 6.93147180369123816490e-01, ln2lo = 1.90821492927058770002e-10;
    return ((double)e * ln2lo + 2.0 * s) + (double)e * ln2hi;
}

static double m_pow(double x, double y) {
    if (y == 0) return 1.0;
    if (x == 1) return 1.0;
    if (m_isnan(x) || m_isnan(y)) return m_from(0x7FF8000000000000ull);
    if (x < 0) {
        double yi = m_trunc(y);
        if (yi != y) return m_from(0x7FF8000000000000ull);
        double r = m_exp(y * m_log(-x));
        return (m_fabs(m_trunc(y * 0.5) * 2.0 - y) == 1.0) ? -r : r;
    }
    return m_exp(y * m_log(x));
}

/* sin/cos: redução por pi/2 em três partes (Cody-Waite), boa para |x| < 1e6 */
static double m_sin_k(double r) { double r2 = r * r, t = r, s = r; for (int i = 1; i <= 11; i++) { t = -t * r2 / (double)((2 * i) * (2 * i + 1)); s = s + t; } return s; }
static double m_cos_k(double r) { double r2 = r * r, t = 1.0, s = 1.0; for (int i = 1; i <= 11; i++) { t = -t * r2 / (double)((2 * i - 1) * (2 * i)); s = s + t; } return s; }
static void m_reduce(double x, double *r, int *q) {
    const double p1 = 1.57079632673412561417e+00, p2 = 6.07710050650619224932e-11, p3 = 2.02226624879595063154e-21;
    double k = m_floor(x * 6.36619772367581382433e-01 + 0.5);
    *r = ((x - k * p1) - k * p2) - k * p3;
    *q = (int)((int64_t)k & 3);
}
static double m_sin(double x) {
    if (m_isnan(x) || m_isinf(x)) return m_from(0x7FF8000000000000ull);
    double r; int q; m_reduce(x, &r, &q);
    switch (q) { case 0: return m_sin_k(r); case 1: return m_cos_k(r); case 2: return -m_sin_k(r); default: return -m_cos_k(r); }
}
static double m_cos(double x) {
    if (m_isnan(x) || m_isinf(x)) return m_from(0x7FF8000000000000ull);
    double r; int q; m_reduce(x, &r, &q);
    switch (q) { case 0: return m_cos_k(r); case 1: return -m_sin_k(r); case 2: return -m_cos_k(r); default: return m_sin_k(r); }
}

static double m_atan(double x) {
    if (m_isnan(x)) return x;
    int neg = x < 0; if (neg) x = -x;
    int inv = x > 1.0; if (inv) x = 1.0 / x;
    /* atan(x) = 2 atan(x / (1 + sqrt(1 + x^2))) duas vezes, depois série */
    x = x / (1.0 + m_sqrt(1.0 + x * x));
    x = x / (1.0 + m_sqrt(1.0 + x * x));
    double x2 = x * x, t = x, s = 0.0;
    for (int i = 0; i < 12; i++) { s = s + t / (double)(2 * i + 1); t = -t * x2; }
    s = 4.0 * s;
    if (inv) s = 1.57079632679489655800e+00 - s;
    return neg ? -s : s;
}
static double m_atan2(double y, double x) {
    const double pi = 3.14159265358979311600e+00;
    if (m_isnan(x) || m_isnan(y)) return m_from(0x7FF8000000000000ull);
    if (x > 0) return m_atan(y / x);
    if (x < 0) return y >= 0 ? m_atan(y / x) + pi : m_atan(y / x) - pi;
    return y > 0 ? pi / 2 : y < 0 ? -pi / 2 : 0.0;
}
static double m_tanh(double x) { if (x > 20) return 1.0; if (x < -20) return -1.0; double e = m_exp(2.0 * x); return (e - 1.0) / (e + 1.0); }
static double m_hypot(double a, double b) { a = m_fabs(a); b = m_fabs(b); if (a < b) { double t = a; a = b; b = t; } if (a == 0) return 0; double r = b / a; return a * m_sqrt(1.0 + r * r); }
