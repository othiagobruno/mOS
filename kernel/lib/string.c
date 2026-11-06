/* mOS — funções de string e memória (o compilador também as chama implicitamente) */
#include "kernel.h"

int _fltused = 0; /* exigido pelo ABI MSVC quando há ponto flutuante */

void *memcpy(void *d, const void *s, usize n) {
    u8 *dp = d;
    const u8 *sp = s;
    if (n >= 16 && !(((uptr)dp | (uptr)sp) & 7)) {
        u64 q = n / 8;
        __asm__ volatile("rep movsq" : "+D"(dp), "+S"(sp), "+c"(q) :: "memory");
        n &= 7;
    }
    __asm__ volatile("rep movsb" : "+D"(dp), "+S"(sp), "+c"(n) :: "memory");
    return d;
}

__attribute__((no_builtin)) void *memmove(void *d, const void *s, usize n) {
    u8 *dp = d;
    const u8 *sp = s;
    if (dp == sp || n == 0) return d;
    if (dp < sp || dp >= sp + n) return memcpy(d, s, n);   /* para trás sem sobreposição destrutiva */
    /* sobreposição com destino acima: copia do fim para o começo, 8 bytes por vez (o "std; rep movsb"
     * de antes é microcodificado byte a byte e muito lento) */
    dp += n; sp += n;
    while (n && ((uptr)dp & 7)) { *--dp = *--sp; n--; }
    while (n >= 32) {
        u64 a, b, c, e;
        __builtin_memcpy(&a, sp - 8, 8); __builtin_memcpy(&b, sp - 16, 8);
        __builtin_memcpy(&c, sp - 24, 8); __builtin_memcpy(&e, sp - 32, 8);
        __builtin_memcpy(dp - 8, &a, 8); __builtin_memcpy(dp - 16, &b, 8);
        __builtin_memcpy(dp - 24, &c, 8); __builtin_memcpy(dp - 32, &e, 8);
        dp -= 32; sp -= 32; n -= 32;
    }
    while (n >= 8) { u64 a; __builtin_memcpy(&a, sp - 8, 8); __builtin_memcpy(dp - 8, &a, 8); dp -= 8; sp -= 8; n -= 8; }
    while (n) { *--dp = *--sp; n--; }
    return d;
}

void *memset(void *d, int c, usize n) {
    u8 *dp = d;
    __asm__ volatile("rep stosb" : "+D"(dp), "+c"(n) : "a"(c) : "memory");
    return d;
}

__attribute__((no_builtin)) int memcmp(const void *a, const void *b, usize n) {
    const u8 *x = a, *y = b;
    usize i = 0;
    for (; i + 8 <= n; i += 8) {                       /* 8 bytes por vez até achar a palavra diferente */
        u64 p, q;
        __builtin_memcpy(&p, x + i, 8); __builtin_memcpy(&q, y + i, 8);
        if (p != q) break;
    }
    for (; i < n; i++)
        if (x[i] != y[i]) return x[i] - y[i];
    return 0;
}

__attribute__((no_builtin)) usize strlen(const char *s) {
    const char *p = s;
    while ((uptr)p & 7) { if (!*p) return (usize)(p - s); p++; }
    /* palavra alinhada nunca cruza página: ler além do terminador é seguro */
    for (;;) {
        u64 w = *(const u64 *)p;
        if ((w - 0x0101010101010101ULL) & ~w & 0x8080808080808080ULL) break;
        p += 8;
    }
    while (*p) p++;
    return (usize)(p - s);
}
int strcmp(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return (u8)*a - (u8)*b; }
int strncmp(const char *a, const char *b, usize n) {
    for (usize i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i]) return (u8)a[i] - (u8)b[i];
    }
    return 0;
}
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)) {} return r; }
char *strncpy(char *d, const char *s, usize n) {
    usize i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    for (; i < n; i++) d[i] = 0;
    return d;
}
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }
char *strchr(const char *s, int c) { for (; *s; s++) if (*s == (char)c) return (char *)s; return c ? NULL : (char *)s; }
char *strrchr(const char *s, int c) { const char *r = NULL; for (; *s; s++) if (*s == (char)c) r = s; return (char *)r; }
char *strstr(const char *h, const char *n) {
    usize l = strlen(n);
    if (!l) return (char *)h;
    for (; *h; h++) if (!strncmp(h, n, l)) return (char *)h;
    return NULL;
}
char *strdup(const char *s) { usize n = strlen(s) + 1; char *d = kmalloc(n); if (d) memcpy(d, s, n); return d; }
int tolower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
int toupper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalpha(int c) { return (c | 32) >= 'a' && (c | 32) <= 'z'; }
int isalnum(int c) { return isdigit(c) || isalpha(c); }
int strcasecmp(const char *a, const char *b) {
    while (*a && tolower(*a) == tolower(*b)) a++, b++;
    return tolower((u8)*a) - tolower((u8)*b);
}
int strncasecmp(const char *a, const char *b, usize n) {
    for (usize i = 0; i < n; i++) {
        int x = tolower((u8)a[i]), y = tolower((u8)b[i]);
        if (x != y || !x) return x - y;
    }
    return 0;
}
long strtol(const char *s, char **end, int base) {
    while (isspace(*s)) s++;
    int neg = 0;
    if (*s == '-') neg = 1, s++; else if (*s == '+') s++;
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] | 32) == 'x') s += 2, base = 16;
    if (base == 0) base = 10;
    long v = 0;
    for (;;) {
        int d;
        if (isdigit(*s)) d = *s - '0';
        else if (isalpha(*s)) d = tolower(*s) - 'a' + 10;
        else break;
        if (d >= base) break;
        v = v * base + d;
        s++;
    }
    if (end) *end = (char *)s;
    return neg ? -v : v;
}
int atoi(const char *s) { return (int)strtol(s, NULL, 10); }

static u32 rng_state = 0x6d4f5321;
void srand32(u32 s) { rng_state = s ? s : 1; }
u32 rand32(void) {
    u32 x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rng_state = x;
}
