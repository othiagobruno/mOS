/*
 * mOS libSystem — funções de math específicas da Apple que o compilador gera sozinho para alvos Darwin:
 * com sin(x) e cos(x) do mesmo x, o clang/swiftc (-O) chamam __sincos_stret, que devolve os dois de uma vez
 * (struct {sin, cos} em xmm0/xmm1; a versão float devolve os dois floats empacotados em xmm0).
 */
double sin(double), cos(double);

typedef struct { double s, c; } sincos_d;
typedef struct { float s, c; } sincos_f;

sincos_d __sincos_stret(double x) { sincos_d r = {sin(x), cos(x)}; return r; }
sincos_f __sincosf_stret(float x) { sincos_f r = {(float)sin(x), (float)cos(x)}; return r; }
/* __sincospi_stret: sin(πx) e cos(πx) */
sincos_d __sincospi_stret(double x) { const double pi = 3.14159265358979323846; sincos_d r = {sin(pi * x), cos(pi * x)}; return r; }
sincos_f __sincospif_stret(float x) { const double pi = 3.14159265358979323846; sincos_f r = {(float)sin(pi * x), (float)cos(pi * x)}; return r; }

/* variantes float da libm que faltavam (o overlay Swift do MOSDarwin as usa nas sobrecargas Float do tgmath):
 * calculadas em double, como o resto da libm do mOS */
double acos(double), acosh(double), asin(double), asinh(double), atan(double), atanh(double), cbrt(double), cosh(double), erfc(double), erf(double), expm1(double), lgamma(double), sinh(double), tan(double), tanh(double), tgamma(double);
double atan2(double, double), fdim(double, double), hypot(double, double), nextafter(double, double);
float acosf(float x) { return (float)acos(x); }
float acoshf(float x) { return (float)acosh(x); }
float asinf(float x) { return (float)asin(x); }
float asinhf(float x) { return (float)asinh(x); }
float atanf(float x) { return (float)atan(x); }
float atanhf(float x) { return (float)atanh(x); }
float cbrtf(float x) { return (float)cbrt(x); }
float coshf(float x) { return (float)cosh(x); }
float erfcf(float x) { return (float)erfc(x); }
float erff(float x) { return (float)erf(x); }
float expm1f(float x) { return (float)expm1(x); }
float lgammaf(float x) { return (float)lgamma(x); }
float sinhf(float x) { return (float)sinh(x); }
float tanf(float x) { return (float)tan(x); }
float tanhf(float x) { return (float)tanh(x); }
float tgammaf(float x) { return (float)tgamma(x); }
float atan2f(float x, float y) { return (float)atan2(x, y); }
float fdimf(float x, float y) { return (float)fdim(x, y); }
float hypotf(float x, float y) { return (float)hypot(x, y); }
float nextafterf(float x, float y) { return (float)nextafter(x, y); }
