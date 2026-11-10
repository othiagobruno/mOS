#pragma once
#include "stddef.h"
typedef struct FILE FILE;
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
