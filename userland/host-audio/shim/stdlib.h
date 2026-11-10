#pragma once
#include "stddef.h"
void *malloc(size_t n);
void *calloc(size_t n, size_t z);
void *realloc(void *p, size_t n);
void free(void *p);
int abs(int x);
