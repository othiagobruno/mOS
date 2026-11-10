#pragma once
typedef unsigned long size_t;
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void abort(void);
int abs(int x);
#define NULL ((void *)0)
