/* mOS — matemática em ponto flutuante sem libm */
#include "kernel.h"

float fsqrt(float x) { float r; __asm__("sqrtss %1,%0" : "=x"(r) : "x"(x)); return r; }
float fabsf_(float x) { return x < 0 ? -x : x; }

float ffloor(float x) {
    float t = (float)(i64)x;
    return t > x ? t - 1.0f : t;
}
float fceil(float x) {
    float t = (float)(i64)x;
    return t < x ? t + 1.0f : t;
}

float fsin(float x) {
    /* redução para [-pi, pi] e polinômio de grau 11 */
    const float tp = 2.0f * PI_F;
    x = x - tp * ffloor((x + PI_F) / tp);
    float x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6 + x2 * (1.0f / 120 + x2 * (-1.0f / 5040 + x2 * (1.0f / 362880 + x2 * (-1.0f / 39916800))))));
}
float fcos(float x) { return fsin(x + PI_F / 2); }

float fexp(float x) {
    if (x < -80) return 0;
    if (x > 80) x = 80;
    /* e^x = 2^(x*log2e) */
    float t = x * 1.44269504f;
    float ip = ffloor(t), fp = t - ip;
    float p = 1.0f + fp * (0.69314718f + fp * (0.24022650f + fp * (0.05550411f + fp * (0.00961813f + fp * 0.00133336f))));
    i32 e = (i32)ip;
    union { float f; u32 u; } v = { .f = p };
    v.u += (u32)e << 23;
    return v.f;
}

static float flog2(float x) {
    union { float f; u32 u; } v = { .f = x };
    float e = (float)(i32)((v.u >> 23) & 0xFF) - 127;
    v.u = (v.u & 0x007FFFFF) | 0x3F800000;
    float m = v.f; /* [1,2) */
    float y = (m - 1) / (m + 1), y2 = y * y;
    float ln = 2 * y * (1 + y2 * (1.0f / 3 + y2 * (1.0f / 5 + y2 * (1.0f / 7 + y2 / 9))));
    return e + ln * 1.44269504f;
}

float fpow(float b, float e) {
    if (b <= 0) return 0;
    return fexp(e * flog2(b) * 0.69314718f);
}
