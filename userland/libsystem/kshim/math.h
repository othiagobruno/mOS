#pragma once
#define INFINITY (__builtin_inff())
#define NAN (__builtin_nanf(""))
#define HUGE_VAL (__builtin_huge_val())
double fabs(double x);
double sqrt(double x);
double floor(double x);
double log(double x);
double pow(double x, double y);
