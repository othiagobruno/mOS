/* mOS: os aliases fracos do musl (weak_alias), que o triplo do Darwin (arm64-apple-macos11-elf) não liga, como funções.
 * Só os nomes que o macOS exporta. */
#include <math.h>

double __lgamma_r(double, int *);
float __lgammaf_r(float, int *);
long double __lgammal_r(long double, int *);

double lgamma_r(double x, int *s) { return __lgamma_r(x, s); }
float lgammaf_r(float x, int *s) { return __lgammaf_r(x, s); }
long double lgammal_r(long double x, int *s) { return __lgammal_r(x, s); }
double drem(double x, double y) { return remainder(x, y); }
float dremf(float x, float y) { return remainderf(x, y); }
