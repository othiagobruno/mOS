#pragma once
#ifndef MOS_SIZE_T
#define MOS_SIZE_T
typedef unsigned long size_t;
#endif
typedef struct { int fd; } FILE;
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int snprintf(char *d, size_t n, const char *fmt, ...);
#define NULL ((void *)0)
