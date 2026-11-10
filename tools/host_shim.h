/* mOS — shim para compilar kernel/fs no host (tools/dmgtest.c) */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <strings.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef size_t usize;

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* contagem de alocações vivas para achar vazamentos (tools/dmgtest.c) */
extern long long g_live_allocs, g_live_bytes;
#ifdef _WIN32
#include <malloc.h>
#define host_usable_size(p) _msize(p)
#else
#include <malloc.h>
#define host_usable_size(p) malloc_usable_size(p)
#endif
static inline void *kmalloc(size_t n) { void *p = malloc(n ? n : 1); if (p) { g_live_allocs++; g_live_bytes += host_usable_size(p); } return p; }
static inline void *kcalloc(size_t n, size_t s) { void *p = kmalloc(n * s); if (p) memset(p, 0, n * s); return p; }
static inline void kfree(void *p) { if (!p) return; g_live_allocs--; g_live_bytes -= host_usable_size(p); free(p); }
static inline void *kmalloc_pages(size_t n) { return kmalloc(n); }   /* no kernel: páginas alinhadas (fat.c); no host tanto faz */
static inline void *krealloc(void *p, size_t n) {
    if (!p) return kmalloc(n);
    size_t old = host_usable_size(p);
    void *q = realloc(p, n ? n : 1);
    if (q) g_live_bytes += (long long)host_usable_size(q) - (long long)old;
    return q;
}
static inline char *host_strdup(const char *s) { size_t n = strlen(s) + 1; char *d = kmalloc(n); if (d) memcpy(d, s, n); return d; }
#define strdup host_strdup
#define kprintf(...) fprintf(stderr, __VA_ARGS__)

/* relógio (fs/fat.c carimba datas; o teste fornece rtc_now) */
#include <ctype.h>
typedef struct { int year, month, day, hour, minute, second, weekday; } datetime_t;
void rtc_now(datetime_t *dt);
uint32_t rand32(void);
