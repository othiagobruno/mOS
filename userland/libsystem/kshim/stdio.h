#pragma once
#include "stddef.h"
typedef struct { int fd; } FILE;
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int snprintf(char *b, size_t n, const char *fmt, ...);
/* arquivos: implementados em stdio.c; aqui o FILE só passa como ponteiro */
FILE *fopen(const char *path, const char *mode);
size_t fread(void *p, size_t sz, size_t n, FILE *f);
size_t fwrite(const void *p, size_t sz, size_t n, FILE *f);
int fclose(FILE *f);
#define stdout ((FILE *)1)
#define stderr ((FILE *)2)
