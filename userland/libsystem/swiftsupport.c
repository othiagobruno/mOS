/*
 * mOS libSystem — o que o runtime Swift completo (C++) e a libc++ importam e o libSystem ainda não tinha:
 * divisão de inteiros de 128 bits (compiler-rt), variantes long double/float da libm, caracteres largos
 * (wchar_t de 32 bits, como no macOS), dladdr e o argv para a stdlib Swift.
 */
#include "foundation.h"
#include "libc.h"

typedef unsigned __int128 u128;
typedef __int128 i128;

/* ---- 128 bits: deslocamento e subtração (sem usar / e %, que chamariam a si mesmas) */
static u128 udivmod(u128 n, u128 d, u128 *rem) {
    if (d == 0) { if (rem) *rem = 0; return 0; }
    u128 q = 0, r = 0;
    for (int i = 127; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= (u128)1 << i; }
    }
    if (rem) *rem = r;
    return q;
}
u128 __udivti3(u128 a, u128 b) { return udivmod(a, b, 0); }
u128 __umodti3(u128 a, u128 b) { u128 r; udivmod(a, b, &r); return r; }
i128 __divti3(i128 a, i128 b) {
    int neg = (a < 0) ^ (b < 0);
    u128 q = udivmod(a < 0 ? -(u128)a : (u128)a, b < 0 ? -(u128)b : (u128)b, 0);
    return neg ? -(i128)q : (i128)q;
}
i128 __modti3(i128 a, i128 b) {
    u128 r;
    udivmod(a < 0 ? -(u128)a : (u128)a, b < 0 ? -(u128)b : (u128)b, &r);
    return a < 0 ? -(i128)r : (i128)r;
}

/* ---- libm: long double (80 bits no x86-64) e float via double */
double ceil(double), floor(double), cos(double), sin(double), exp(double), exp2(double), log(double), log2(double), log10(double),
       fmod(double, double), remainder(double, double), nearbyint(double), rint(double), round(double), trunc(double), fma(double, double, double),
       strtod(const char *, char **);
long double ceill(long double x) { return ceil((double)x); }
long double floorl(long double x) { return floor((double)x); }
long double cosl(long double x) { return cos((double)x); }
long double sinl(long double x) { return sin((double)x); }
long double expl(long double x) { return exp((double)x); }
long double exp2l(long double x) { return exp2((double)x); }
long double logl(long double x) { return log((double)x); }
long double log2l(long double x) { return log2((double)x); }
long double log10l(long double x) { return log10((double)x); }
long double fmodl(long double x, long double y) { return fmod((double)x, (double)y); }
long double remainderl(long double x, long double y) { return remainder((double)x, (double)y); }
long double nearbyintl(long double x) { return nearbyint((double)x); }
long double rintl(long double x) { return rint((double)x); }
long double roundl(long double x) { return round((double)x); }
long double truncl(long double x) { return trunc((double)x); }
long double fmal(long double x, long double y, long double z) { return fma((double)x, (double)y, (double)z); }
float exp2f(float x) { return (float)exp2(x); }
float log2f(float x) { return (float)log2(x); }
float log10f(float x) { return (float)log10(x); }
float fmodf(float x, float y) { return (float)fmod(x, y); }
float remainderf(float x, float y) { return (float)remainder(x, y); }
long double strtold_l(const char *s, char **end, void *loc) { (void)loc; return strtod(s, end); }

/* ---- caracteres largos (wchar_t = 32 bits) */
typedef int wchar32;
size_t wcslen(const wchar32 *s) { size_t n = 0; while (s[n]) n++; return n; }
wchar32 *wmemchr(const wchar32 *s, wchar32 c, size_t n) { for (size_t i = 0; i < n; i++) if (s[i] == c) return (wchar32 *)(s + i); return 0; }
int wmemcmp(const wchar32 *a, const wchar32 *b, size_t n) { for (size_t i = 0; i < n; i++) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1; return 0; }
/* conversões numéricas: só dígitos ASCII importam, então estreitamos para char */
static void narrow(const wchar32 *s, char *buf, size_t cap, size_t *used) {
    size_t i = 0;
    while (s[i] && i + 1 < cap) { buf[i] = s[i] < 128 ? (char)s[i] : '?'; i++; }
    buf[i] = 0;
    *used = i;
}
long long strtoll(const char *s, char **e, int b);
unsigned long long strtoull(const char *s, char **e, int b);
#define WNUM(name, T, call)                                                               \
    T name(const wchar32 *s, wchar32 **end, int base) {                                   \
        char buf[128]; size_t n; narrow(s, buf, sizeof buf, &n); char *e;                 \
        T v = (T)call; if (end) *end = (wchar32 *)(s + (e - buf)); (void)base; return v;  \
    }
WNUM(wcstol, long, strtoll(buf, &e, base))
WNUM(wcstoll, long long, strtoll(buf, &e, base))
WNUM(wcstoul, unsigned long, strtoull(buf, &e, base))
WNUM(wcstoull, unsigned long long, strtoull(buf, &e, base))
double wcstod(const wchar32 *s, wchar32 **end) { char buf[128]; size_t n; narrow(s, buf, sizeof buf, &n); char *e; double v = strtod(buf, &e); if (end) *end = (wchar32 *)(s + (e - buf)); return v; }
float wcstof(const wchar32 *s, wchar32 **end) { return (float)wcstod(s, end); }
long double wcstold(const wchar32 *s, wchar32 **end) { return wcstod(s, end); }
int vsnprintf(char *b, size_t n, const char *fmt, va_list ap);
int swprintf(wchar32 *out, size_t n, const wchar32 *fmt, ...) {
    char f[256], tmp[512];
    size_t used;
    narrow(fmt, f, sizeof f, &used);
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(tmp, sizeof tmp, f, ap);
    va_end(ap);
    size_t i = 0;
    for (; tmp[i] && i + 1 < n; i++) out[i] = (unsigned char)tmp[i];
    if (n) out[i] = 0;
    return r;
}

/* ---- dladdr: o mOS não tem tabela de símbolos em runtime; informa só a imagem */
typedef struct { const char *dli_fname; void *dli_fbase; const char *dli_sname; void *dli_saddr; } Dl_info;
int dladdr(const void *addr, Dl_info *info) {
    (void)addr;
    if (!info) return 0;
    info->dli_fname = "app"; info->dli_fbase = 0; info->dli_sname = 0; info->dli_saddr = 0;
    return 0;
}

/* ---- argv para CommandLine.arguments */
extern char **NXArgv;
extern int NXArgc;
char **_swift_stdlib_getUnsafeArgvArgc(int *argc) { if (argc) *argc = NXArgc; return NXArgv; }
