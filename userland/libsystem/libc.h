/* mOS libSystem — declarações internas compartilhadas entre libc.c e stdio.c */
#pragma once
#include <stdarg.h>
#include <stddef.h>
#ifndef MOS_SSIZE_T_DEFINED
#define MOS_SSIZE_T_DEFINED
typedef long ssize_t;
#endif
#ifndef MOS_OFF_T_DEFINED
#define MOS_OFF_T_DEFINED
typedef long off_t;
#endif

extern int cx_errno;
extern char **environ;

void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
void *memchr(const void *s, int c, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t n);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int strncasecmp(const char *a, const char *b, size_t n);
char *strcpy(char *d, const char *s);
char *strchr(const char *s, int c);
char *strdup(const char *s);
size_t strlcpy(char *d, const char *s, size_t n);
size_t strlcat(char *d, const char *s, size_t n);
char *strerror(int e);
int tolower(int c);
int toupper(int c);
int isdigit(int c);
int isspace(int c);
int isalpha(int c);
int isxdigit(int c);
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
char *getenv(const char *name);
__attribute__((noreturn)) void exit(int code);
__attribute__((noreturn)) void abort(void);
ssize_t write(int fd, const void *b, size_t n);
ssize_t read(int fd, void *b, size_t n);
int open(const char *path, int flags, ...);
int close(int fd);
off_t lseek(int fd, off_t off, int wh);
double strtod(const char *s, char **end);
long double strtold(const char *s, char **end);
long strtol(const char *s, char **end, int base);
unsigned long strtoul(const char *s, char **end, int base);
const char *cx_path(const char *in, char *out, size_t cap);

/* stdio.c */
void cx_stdio_init(void);
void cx_stdio_flush_all(void);
void cx_puts_fd(int fd, const char *s);
int cx_fdprintf(int fd, const char *fmt, ...);
int cx_snprintf(char *buf, size_t n, const char *fmt, ...);
void cx_libc_init(void);
