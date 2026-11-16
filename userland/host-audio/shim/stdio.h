#pragma once
#include "stddef.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct FILE FILE;
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
#ifdef __cplusplus
}
#endif
