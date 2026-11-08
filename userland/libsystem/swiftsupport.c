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

/* ---- dladdr: imagem via o dyld do kernel; símbolo pela export trie mapeada na imagem */
typedef struct { const char *dli_fname; void *dli_fbase; const char *dli_sname; void *dli_saddr; } Dl_info;
u32 _dyld_image_count(void);
const void *_dyld_get_image_header(u32 i);
const char *_dyld_get_image_name(u32 i);
const void *_dyld_get_image_header_containing_address(const void *addr);

static u64 cx_uleb(const u8 **p, const u8 *end) {
    u64 v = 0; int s = 0;
    while (*p < end && s <= 56) {
        u8 b = *(*p)++;
        v |= (u64)(b & 0x7f) << s;
        if (!(b & 0x80)) break;
        s += 7;
    }
    return v;
}
static const u8 *image_trie(const void *hdr, u32 *tsz, u64 *slide) {
    const u32 *mh = hdr;
    if (!mh || mh[0] != 0xFEEDFACFu) return 0;
    const u8 *p = (const u8 *)hdr + 32;
    u32 ncmds = mh[4];
    u64 text_vm = 0, trie_off = 0, trie_sz = 0;
    u64 seg[8][4];
    int ns = 0;
    for (u32 i = 0; i < ncmds && i < 80; i++) {
        if (p + 8 > (const u8 *)hdr + 32768) break;
        u32 cmd = *(const u32 *)p, sz = *(const u32 *)(p + 4);
        if (sz < 8) break;
        if (cmd == 0x19 && ns < 8 && sz >= 72) {
            seg[ns][0] = *(const u64 *)(p + 24);
            seg[ns][1] = *(const u64 *)(p + 32);
            seg[ns][2] = *(const u64 *)(p + 40);
            seg[ns][3] = *(const u64 *)(p + 48);
            if (!strncmp((const char *)p + 8, "__TEXT", 6)) text_vm = seg[ns][0];
            ns++;
        }
        if (cmd == 0x80000033u && sz >= 16) { trie_off = *(const u32 *)(p + 8); trie_sz = *(const u32 *)(p + 12); }
        if ((cmd == 0x22 || cmd == 0x80000022u) && !trie_off && sz >= 48) {
            trie_off = *(const u32 *)(p + 40);
            trie_sz = *(const u32 *)(p + 44);
        }
        p += sz;
    }
    if (!text_vm || !trie_off || !trie_sz) return 0;
    *slide = (u64)hdr - text_vm;
    for (int i = 0; i < ns; i++) {
        if (trie_off < seg[i][2] || trie_off + trie_sz > seg[i][2] + seg[i][3]) continue;
        *tsz = (u32)trie_sz;
        return (const u8 *)hdr + (seg[i][0] - text_vm) + (trie_off - seg[i][2]);
    }
    return 0;
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
/* como trie_near, mas guarda todos os símbolos com endereço (trie nova: deslocamento a partir do cabeçalho;
 * antiga: vmaddr sem slide, reconhecida por ser >= o vmaddr do __TEXT) */
static void trie_collect(const u8 *base, const u8 *end, const u8 *p, char *name, int nl, int depth,
                         u64 hdr, u64 slide, u64 text_vm, dl_tab_t *t) {
    if (depth > 64 || p < base || p >= end || nl > 180) return;
    u64 tsz = cx_uleb(&p, end);
    if (tsz && p + tsz <= end && nl > 0) {
        const u8 *q = p;
        u64 flags = cx_uleb(&q, end);
        if (!(flags & 0x18)) {
            u64 raw = cx_uleb(&q, end);
            u64 addr = (text_vm && raw >= text_vm) ? raw + slide : hdr + raw;
            if (raw && addr >= hdr) dl_add(t, addr, name, nl);
        }
        p += tsz;
    }
    if (p >= end) return;
    u8 nc = *p++;
    for (u8 i = 0; i < nc && p < end; i++) {
        char child[192];
        int n2 = nl;
        memcpy(child, name, (size_t)n2);
        while (p < end && *p && n2 < 180) child[n2++] = (char)*p++;
        while (p < end && *p) p++;
        if (p < end) p++;
        u64 off = cx_uleb(&p, end);
        if (off && base + off < end) trie_collect(base, end, base + off, child, n2, depth + 1, hdr, slide, text_vm, t);
    }
}
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
            trie_collect(trie, trie + tsz, trie, name, 0, 0, (u64)h, slide, text_vm, &nt);
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
