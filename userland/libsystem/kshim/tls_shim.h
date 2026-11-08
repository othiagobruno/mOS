/* shim: kernel/net/crypto.c e pk.c compilados dentro do libSystem (cryptokit.c). Mesmo papel do tools/tls_shim.h. */
#pragma once
#include "../sys.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
void *malloc(size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *memmove(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int snprintf(char *b, size_t n, const char *f, ...);
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#define kmalloc(n) malloc(n)
#define kfree(p) free(p)
#define kprintf(...) ((void)0)
#if defined(__aarch64__)
static inline u64 rdtsc(void) { u64 v; __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v)); return v; }
#else
static inline u64 rdtsc(void) { u32 lo, hi; __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi)); return (u64)hi << 32 | lo; }
#endif
static inline u64 timer_ticks(void) { return (u64)SYS0(SYS_TIME); }
#ifndef NULL
#define NULL ((void *)0)
#endif
typedef struct { int year, month, day, hour, minute, second, weekday; } datetime_t;
