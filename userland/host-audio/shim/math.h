#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#define INFINITY (__builtin_huge_val())
#define NAN (__builtin_nan(""))
#define HUGE_VAL (__builtin_huge_val())
double floor(double x);
double ceil(double x);
double fabs(double x);
double sqrt(double x);
double log(double x);
double log10(double x);
double exp(double x);
double pow(double x, double y);
double frexp(double x, int *e);
double ldexp(double x, int e);
/* o libSystem ainda não exporta as versões float; os codecs usam estas */
#define frexpf(x, e) ((float)frexp((double)(x), (e)))
#define ldexpf(x, e) ((float)ldexp((double)(x), (e)))
#ifdef __cplusplus
}
#endif
