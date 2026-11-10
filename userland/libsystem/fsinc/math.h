#pragma once
/* ag_dec.c inclui math.h e não chama função nenhuma da libm.
 * O sqlite precisa de INFINITY. */
#define INFINITY (__builtin_inff())
double fabs(double x);
double floor(double x);
double ceil(double x);
double sqrt(double x);
double log(double x);
double pow(double x, double y);
