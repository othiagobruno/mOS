#pragma once
#define INFINITY (__builtin_huge_val())
#define NAN (__builtin_nan(""))
#define HUGE_VAL (__builtin_huge_val())
double floor(double x);
double ceil(double x);
double fabs(double x);
double sqrt(double x);
double log(double x);
double pow(double x, double y);
