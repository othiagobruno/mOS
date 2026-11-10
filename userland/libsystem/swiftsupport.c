/*
 * mOS libSystem — o que o runtime Swift completo (C++) e a libc++ importam e o libSystem ainda não tinha:
 * divisão de inteiros de 128 bits (compiler-rt), variantes long double/float da libm, caracteres largos
 * (wchar_t de 32 bits, como no macOS), dladdr e o argv para a stdlib Swift.
 */
#include "foundation.h"
#include "libc.h"
#include "dladdr_trie.h"

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

/* ---- dladdr: imagem via o dyld do kernel; símbolo pela export trie mapeada na imagem */
typedef struct { const char *dli_fname; void *dli_fbase; const char *dli_sname; void *dli_saddr; } Dl_info;
u32 _dyld_image_count(void);
const void *_dyld_get_image_header(u32 i);
const char *_dyld_get_image_name(u32 i);
const void *_dyld_get_image_header_containing_address(const void *addr);

static const u8 *image_trie(const void *hdr, u32 *tsz, u64 *slide) {
    return dlt_image(hdr, tsz, slide);
}
/* ---- tabela de símbolos por imagem para o dladdr (endereço -> nome), a partir da export trie */
typedef struct { u64 addr; u32 name; } dl_ent_t;
typedef struct { const void *hdr; dl_ent_t *e; u32 n, cap; char *names; u32 nlen, ncap; } dl_tab_t;
static dl_tab_t dl_tabs[96];
static u32 dl_ntabs;
static volatile int dl_lock;
static void dl_add(dl_tab_t *t, u64 addr, const char *name, int nl) {
    if (t->n == t->cap) { u32 c = t->cap ? t->cap * 2 : 1024; dl_ent_t *e = realloc(t->e, c * sizeof *e); if (!e) return; t->e = e; t->cap = c; }
    if (t->nlen + (u32)nl + 1 > t->ncap) {
        u32 c = t->ncap ? t->ncap * 2 : 65536;
        while (c < t->nlen + (u32)nl + 1) c *= 2;
        char *s = realloc(t->names, c); if (!s) return; t->names = s; t->ncap = c;
    }
    memcpy(t->names + t->nlen, name, (size_t)nl);
    t->names[t->nlen + (u32)nl] = 0;
    t->e[t->n].addr = addr; t->e[t->n].name = t->nlen; t->n++;
    t->nlen += (u32)nl + 1;
}
static void dl_sink(void *ctx, unsigned long long addr, const char *name, int nl) { dl_add((dl_tab_t *)ctx, addr, name, nl); }
void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *));
static int dl_cmp(const void *x, const void *y) {
    u64 a = ((const dl_ent_t *)x)->addr, b = ((const dl_ent_t *)y)->addr;
    return a < b ? -1 : a > b;
}
static dl_tab_t *dl_table(const void *h) {
    for (u32 i = 0; i < dl_ntabs; i++) if (dl_tabs[i].hdr == h) return &dl_tabs[i];
    while (__sync_lock_test_and_set(&dl_lock, 1)) {}
    dl_tab_t *t = 0;
    for (u32 i = 0; i < dl_ntabs; i++) if (dl_tabs[i].hdr == h) t = &dl_tabs[i];
    if (!t && dl_ntabs < sizeof dl_tabs / sizeof dl_tabs[0]) {
        dl_tab_t nt = {0};
        nt.hdr = h;
        u32 tsz = 0; u64 slide = 0;
        const u8 *trie = image_trie(h, &tsz, &slide);
        if (trie && tsz) {
            char name[192];
            u64 text_vm = (u64)h - slide;
            dlt_collect(trie, trie + tsz, trie, name, 0, 0, (u64)h, slide, text_vm, dl_sink, &nt);
            if (nt.n) qsort(nt.e, nt.n, sizeof *nt.e, dl_cmp);
        }
        dl_tabs[dl_ntabs] = nt;
        __sync_synchronize();
        t = &dl_tabs[dl_ntabs++];
    }
    __sync_lock_release(&dl_lock);
    return t;
}
static char dl_sym[4][192];
static unsigned dl_sym_i;
int dladdr(const void *addr, Dl_info *info) {
    if (!info || !addr) return 0;
    memset(info, 0, sizeof *info);
    u64 a = (u64)addr;
    if (a >= 0x400000100000ULL && a < 0x400001000000ULL) {
        info->dli_fname = "/usr/lib/libSystem.B.dylib";
        info->dli_fbase = (void *)0x400000100000ULL;
        return 1;
    }
    const void *h = _dyld_get_image_header_containing_address(addr);
    if (!h) return 0;
    info->dli_fbase = (void *)h;
    u32 n = _dyld_image_count();
    for (u32 i = 0; i < n; i++) if (_dyld_get_image_header(i) == h) { info->dli_fname = _dyld_get_image_name(i); break; }
    /* tabela ordenada da imagem, montada na primeira consulta: percorrer a trie inteira a cada chamada custava
     * dezenas de milhares de nós por dladdr numa imagem grande (SwiftUI.framework: 41 mil símbolos), e o
     * class_getImageName / NSBundle bundleForClass: chamam dladdr para cada classe */
    dl_tab_t *t = dl_table(h);
    if (!t || !t->n) return 1;
    u32 lo = 0, hi = t->n;
    while (hi - lo > 1) { u32 mid = (lo + hi) / 2; if (t->e[mid].addr <= a) lo = mid; else hi = mid; }
    if (t->e[lo].addr > a) return 1;
    u64 best = t->e[lo].addr;
    const char *found = t->names + t->e[lo].name;
    unsigned slot = dl_sym_i++ & 3;
    size_t fl = strlen(found);
    if (fl > sizeof dl_sym[slot] - 1) fl = sizeof dl_sym[slot] - 1;
    memcpy(dl_sym[slot], found, fl);
    dl_sym[slot][fl] = 0;
    info->dli_sname = dl_sym[slot];
    info->dli_saddr = (void *)best;
    return 1;
}

/* ---- argv para CommandLine.arguments */
extern char **NXArgv;
extern int NXArgc;
char **_swift_stdlib_getUnsafeArgvArgc(int *argc) { if (argc) *argc = NXArgc; return NXArgv; }
