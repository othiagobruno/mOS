/* Cabeçalhos que o zig cc -target x86_64-freestanding-none não traz.
 * As funções são as do próprio libSystem. */
#pragma once
#include "stddef.h"
void *malloc(size_t n);
void *calloc(size_t n, size_t sz);
void *realloc(void *p, size_t n);
void free(void *p);
void abort(void);
int abs(int x);
