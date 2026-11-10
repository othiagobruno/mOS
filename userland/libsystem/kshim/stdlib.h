/* Cabeçalhos que o zig cc -target x86_64-freestanding-none não traz.
 * As funções são as do próprio libSystem. */
#pragma once
#ifndef MOS_SIZE_T
#define MOS_SIZE_T
typedef unsigned long size_t;
#endif
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void abort(void);
int abs(int x);
#define NULL ((void *)0)
