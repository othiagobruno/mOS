/* Shim só do teste de host. Declara a libc do Linux que stdio.c e sortb.c chamam. */
#pragma once
#include <stdarg.h>
#include <stddef.h>
extern int cx_errno;
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
void *memchr(const void *, int, size_t);
void *malloc(size_t);
void *realloc(void *, size_t);
void free(void *);
size_t strlen(const char *);
size_t strnlen(const char *, size_t);
char *strchr(const char *, int);
char *strcpy(char *, const char *);
int isdigit(int);
int isspace(int);
int isxdigit(int);
double strtod(const char *, char **);
long double strtold(const char *, char **);
long strtol(const char *, char **, int);
unsigned long strtoul(const char *, char **, int);
long write(int, const void *, unsigned long);
long read(int, void *, unsigned long);
int open(const char *, int, ...);
int close(int);
long lseek(int, long, int);
char *strerror(int);
__attribute__((noreturn)) void abort(void);
__attribute__((noreturn)) void exit(int);
void cx_stdio_init(void);
void cx_stdio_flush_all(void);
void cx_puts_fd(int fd, const char *s);
int cx_fdprintf(int fd, const char *fmt, ...);
int cx_snprintf(char *b, size_t n, const char *fmt, ...);
