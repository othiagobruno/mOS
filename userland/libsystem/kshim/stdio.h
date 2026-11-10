#pragma once
#include <stddef.h>
typedef struct { int fd; } FILE;
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int snprintf(char *d, size_t n, const char *fmt, ...);
#define NULL ((void *)0)
