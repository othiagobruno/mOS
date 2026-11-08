/*
 * mOS libSystem arm64 — o que a libm do x86 fazia com o x87 (fpatan) e o compiler-rt do zig não traz: atan2.
 * sin/cos/tan/fmod/log2/exp2 vêm do compiler-rt do zig compilado para arm64 (tools/build-libsystem-arm64.ps1); o resto da
 * libm do libc.c (atan, asin, acos, log, exp, pow...) é derivado e serve às duas arquiteturas.
 */
#if defined(__aarch64__)
double sqrt(double);

/* atan(t) para |t| <= tan(pi/8): duas reduções pelo meio-ângulo, atan(u) = 2 atan(u / (1 + sqrt(1 + u^2))), e a série
 * de Taylor até u^23 (|u| <= 0.0995: o termo seguinte fica abaixo de 1e-25) */
static double atan_small(double u) {
    u = u / (1.0 + sqrt(1.0 + u * u));
    u = u / (1.0 + sqrt(1.0 + u * u));
    double u2 = u * u, s = 0;
    for (int k = 23; k >= 3; k -= 2) s = (k & 2 ? -1.0 : 1.0) / k + u2 * s;   /* k = 3, 7, 11...: termo negativo */
    return 4.0 * (u + u * u2 * s);
}
/* atan(t) para 0 <= t <= 1 */
static double atan01(double t) {
    const double pi_4 = 0.78539816339744830962;
    if (t > 0.41421356237309504880) return pi_4 + atan_small((t - 1.0) / (t + 1.0));
    return atan_small(t);
}
double atan2(double y, double x) {
    const double pi = 3.14159265358979323846, pi_2 = 1.57079632679489661923, pi_4 = 0.78539816339744830962;
    if (x != x || y != y) return x + y;
    int sy = __builtin_signbit(y) != 0, sx = __builtin_signbit(x) != 0;
    double ay = __builtin_fabs(y), ax = __builtin_fabs(x), r;
    if (ay == 0) r = sx ? pi : 0;                                     /* atan2(±0, x) */
    else if (ax == 0) r = pi_2;
    else if (__builtin_isinf(ax) && __builtin_isinf(ay)) r = sx ? 3 * pi_4 : pi_4;
    else if (__builtin_isinf(ax)) r = sx ? pi : 0;
    else if (__builtin_isinf(ay)) r = pi_2;
    else {
        r = ay <= ax ? atan01(ay / ax) : pi_2 - atan01(ax / ay);
        if (sx) r = pi - r;
    }
    return sy ? -r : r;
}
#endif
