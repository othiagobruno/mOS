#pragma once
#define HUGE_VAL (__builtin_huge_val())
#define INFINITY (__builtin_huge_val())
#define NAN (__builtin_nan(""))
double sqrt(double x);
double log(double x);
double log2(double x);
double pow(double x, double y);
double floor(double x);
double ceil(double x);
double fabs(double x);
double cos(double x);
float cosf(float x);
