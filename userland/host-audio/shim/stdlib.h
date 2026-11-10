#pragma once
#include "stddef.h"
void *malloc(size_t n);
void *calloc(size_t n, size_t z);
void *realloc(void *p, size_t n);
void free(void *p);
int abs(int x);
void qsort(void *b, unsigned long n, unsigned long z, int (*cmp)(const void *, const void *));
int rand(void);
void srand(unsigned int s);
char *getenv(const char *n);
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
double strtod(const char *s, char **end);
