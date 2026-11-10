#pragma once
#include "stddef.h"
typedef struct { int fd; } FILE;
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int snprintf(char *b, size_t n, const char *fmt, ...);
#define stdout ((FILE *)1)
#define stderr ((FILE *)2)
