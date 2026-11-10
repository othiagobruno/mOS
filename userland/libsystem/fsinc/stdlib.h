/* Cabeçalhos mínimos para o zig cc -target x86_64-freestanding-none.
 * O alvo arm64 (macos) já tem a libc do zig; este diretório só entra no build x86. */
#pragma once
#include <stddef.h>
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void abort(void);
__attribute__((noreturn)) void exit(int code);
int atoi(const char *s);
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
