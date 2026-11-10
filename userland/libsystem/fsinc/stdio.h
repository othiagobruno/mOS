#pragma once
#include <stddef.h>
typedef struct FILE FILE;
extern FILE *stderr;
extern FILE *stdout;
int fprintf(FILE *f, const char *fmt, ...);
int printf(const char *fmt, ...);
int sprintf(char *b, const char *fmt, ...);
int snprintf(char *b, size_t n, const char *fmt, ...);
int sscanf(const char *s, const char *fmt, ...);
int vsnprintf(char *b, size_t n, const char *fmt, __builtin_va_list ap);
