#pragma once
#include <stddef.h>
typedef struct { int fd; } FILE;
int printf(const char *fmt, ...);
int snprintf(char *s, size_t n, const char *fmt, ...);
