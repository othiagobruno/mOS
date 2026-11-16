/* libSystem: o que kernel/web/webp.c pede do web.h do kernel, para o ImageIO (iio_webp.c) compilar
 * o mesmo decodificador no userland. Só tipos, alocação e surface_t. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef size_t usize;
#define kmalloc(n) malloc(n)
#define kcalloc(n, s) calloc((n), (s))
#define kfree(p) free(p)
typedef struct { int w, h, stride; u32 *px; } surface_t;
static inline surface_t *surface_new(int w, int h) {
    surface_t *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->w = w; s->h = h; s->stride = w;
    s->px = calloc((usize)w * (usize)h, 4);
    if (!s->px) { free(s); return NULL; }
    return s;
}
u32 *webp_decode_argb(const u8 *d, usize n, int *w, int *h);
