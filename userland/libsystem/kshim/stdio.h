#pragma once
typedef unsigned long size_t;
typedef struct { int fd; } FILE;
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int snprintf(char *d, size_t n, const char *fmt, ...);
#define NULL ((void *)0)
