/*
 * mOS SDK — funções de <math.h> que o libSystem do mOS exporta (libc.c/extra.c), para o Swift completo.
 * O módulo Foundation do mOS reexporta este módulo (como o Foundation do Linux reexporta a Glibc).
 */
#pragma once
#include <stdarg.h>
#include <stddef.h>

/* stdio que o Foundation usa (String(format:)) */
int vsnprintf(char *buffer, size_t size, const char *format, va_list args);

double sin(double), cos(double), tan(double), asin(double), acos(double), atan(double), atan2(double, double);
double sqrt(double), cbrt(double), hypot(double, double), pow(double, double), exp(double), exp2(double), log(double), log2(double), log10(double);
double floor(double), ceil(double), round(double), trunc(double), rint(double), fmod(double, double), remainder(double, double);
double fabs(double), copysign(double, double), fmin(double, double), fmax(double, double), fma(double, double, double);
double frexp(double, int *), ldexp(double, int), modf(double, double *);

float sinf(float), cosf(float), tanf(float), atan2f(float, float), sqrtf(float), powf(float, float), expf(float), logf(float);
float floorf(float), ceilf(float), roundf(float), fabsf(float), fmodf(float, float), copysignf(float, float), fmaf(float, float, float);
