/* mOS — a64rt: o mínimo de libc que o runtime e o motor usam (freestanding), e o log de diagnóstico. */
#include "rt.h"
#include <stdarg.h>

void *memset(void *d, int c, unsigned long n) { u8 *p = d; while (n--) *p++ = (u8)c; return d; }
void *memcpy(void *d, const void *s, unsigned long n) {
    u8 *a = d; const u8 *b = s;
    while (n >= 8) { u64 v; __builtin_memcpy(&v, b, 8); __builtin_memcpy(a, &v, 8); a += 8; b += 8; n -= 8; }
    while (n--) *a++ = *b++;
    return d;
}
void *memmove(void *d, const void *s, unsigned long n) {
    u8 *a = d; const u8 *b = s;
    if (a == b || !n) return d;
    if (a < b || a >= b + n) return memcpy(d, s, n);
    while (n--) a[n] = b[n];
    return d;
}
int memcmp(const void *x, const void *y, unsigned long n) {
    const u8 *a = x, *b = y;
    for (unsigned long i = 0; i < n; i++) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}
unsigned long strlen(const char *s) { unsigned long n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (u8)*a - (u8)*b; }
int strncmp(const char *a, const char *b, unsigned long n) {
    for (; n; n--, a++, b++) { if (*a != *b) return (u8)*a - (u8)*b; if (!*a) return 0; }
    return 0;
}

static void put(char *buf, unsigned *o, unsigned cap, char ch) { if (*o + 1 < cap) buf[(*o)++] = ch; }
static void putnum(char *buf, unsigned *o, unsigned cap, u64 v, unsigned base, int neg, int width, char pad) {
    char t[24]; int n = 0;
    do { t[n++] = "0123456789abcdef"[v % base]; v /= base; } while (v);
    if (neg) t[n++] = '-';
    while (n < width) t[n++] = pad;
    while (n) put(buf, o, cap, t[--n]);
}
static unsigned vfmt(char *buf, unsigned cap, const char *f, va_list ap) {
    unsigned o = 0;
    for (; *f; f++) {
        if (*f != '%') { put(buf, &o, cap, *f); continue; }
        f++;
        char pad = ' '; int width = 0, l = 0;
        if (*f == '0') { pad = '0'; f++; }
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        while (*f == 'l') { l++; f++; }
        switch (*f) {
        case 's': { const char *s = va_arg(ap, const char *); if (!s) s = "(nulo)"; while (*s) put(buf, &o, cap, *s++); break; }
        case 'c': put(buf, &o, cap, (char)va_arg(ap, int)); break;
        case 'd': { i64 v = l ? va_arg(ap, i64) : va_arg(ap, int); putnum(buf, &o, cap, v < 0 ? (u64)-v : (u64)v, 10, v < 0, width, pad); break; }
        case 'u': { u64 v = l ? va_arg(ap, u64) : va_arg(ap, unsigned); putnum(buf, &o, cap, v, 10, 0, width, pad); break; }
        case 'x': { u64 v = l ? va_arg(ap, u64) : va_arg(ap, unsigned); putnum(buf, &o, cap, v, 16, 0, width, pad); break; }
        case 'p': put(buf, &o, cap, '0'); put(buf, &o, cap, 'x'); putnum(buf, &o, cap, (u64)va_arg(ap, void *), 16, 0, 0, ' '); break;
        case '%': put(buf, &o, cap, '%'); break;
        default: put(buf, &o, cap, '?'); break;
        }
    }
    buf[o] = 0;
    return o;
}
void rt_log(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    unsigned n = vfmt(buf, sizeof buf, fmt, ap);
    va_end(ap);
    RS3(SYS_WRITE, 2, buf, n);
}
void rt_die(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    unsigned n = vfmt(buf, sizeof buf, fmt, ap);
    va_end(ap);
    RS3(SYS_WRITE, 2, "a64rt: ", 7);
    RS3(SYS_WRITE, 2, buf, n);
    RS3(SYS_WRITE, 2, "\n", 1);
    RS1(SYS_EXIT, 134);
    for (;;) {}
}
