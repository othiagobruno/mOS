/* Cabeçalhos que o zig cc -target x86_64-freestanding-none não traz.
 * As funções são as do próprio libSystem. */
#pragma once
#include <stddef.h>
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);
