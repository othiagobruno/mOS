/*
 * mOS Core Text — texto Unicode para o layout, em C puro (sem Objective-C): oportunidades de quebra de linha
 * (UAX #14), limites de grafema (UAX #29), níveis bidi do parágrafo (UAX #9 completo, com embeddings, isolates e
 * números) e a reordenação visual de uma linha (L1 + L2). Tudo pela ICU da libicucore do mOS
 * (/usr/lib/libicucore.A.dylib, a mesma que a Foundation abre); no teste de host, pela ICU do Linux
 * (tools/ct_layout_host.c). Sem ICU, regras simplificadas (espaço quebra, grafema = ponto de código + marcas,
 * bidi por classe forte) para o texto nunca sumir.
 */
#ifdef CT_LAYOUT_HOST
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#else
#include "sys.h"
#include "libc.h"
#define RTLD_NOW 2
#define RTLD_GLOBAL 0x100
void *dlopen(const char *path, int mode);
void *dlsym(void *h, const char *name);
int snprintf(char *b, size_t n, const char *f, ...);
#endif
#include "ct_layout.h"

typedef int UErrorCode;
typedef struct {
    void *(*brk_open)(int, const char *, const u16 *, int32_t, UErrorCode *);
    void (*brk_close)(void *);
    int32_t (*brk_first)(void *), (*brk_next)(void *), (*brk_status)(void *);
    void *(*bidi_open)(void);
    void (*bidi_close)(void *);
    void (*bidi_setpara)(void *, const u16 *, int32_t, u8, u8 *, UErrorCode *);
    const u8 *(*bidi_getlevels)(void *, UErrorCode *);
    u8 (*bidi_paralevel)(const void *);
    void (*bidi_reorder)(const u8 *, int32_t, int32_t *);
    int ok;
} ct_icu_t;
static ct_icu_t icu;
static int icu_state;
static void *sym(void *h, const char *name, const char *suffix) {
    if (!suffix[0]) return dlsym(h, name);
    char full[96];
    snprintf(full, sizeof full, "%s%s", name, suffix);
    return dlsym(h, full);
}
static void icu_load(void) {
    void *h = 0;
#ifdef CT_LAYOUT_HOST
    if (getenv("CT_LAYOUT_NOICU")) return;        /* o teste também exercita o caminho sem ICU */
#endif
    char sfx[8] = "";
#ifdef CT_LAYOUT_HOST
    for (int v = 90; v >= 50 && !h; v--) {
        char p[64];
        snprintf(p, sizeof p, "libicuuc.so.%d", v);
        h = dlopen(p, RTLD_NOW | RTLD_GLOBAL);
        if (h) snprintf(sfx, sizeof sfx, "_%d", v);
    }
#else
    h = dlopen("/usr/lib/libicucore.A.dylib", 0);
#endif
    if (!h) return;
    icu.brk_open = sym(h, "ubrk_open", sfx);
    icu.brk_close = sym(h, "ubrk_close", sfx);
    icu.brk_first = sym(h, "ubrk_first", sfx);
    icu.brk_next = sym(h, "ubrk_next", sfx);
    icu.brk_status = sym(h, "ubrk_getRuleStatus", sfx);
    icu.bidi_open = sym(h, "ubidi_open", sfx);
    icu.bidi_close = sym(h, "ubidi_close", sfx);
    icu.bidi_setpara = sym(h, "ubidi_setPara", sfx);
    icu.bidi_getlevels = sym(h, "ubidi_getLevels", sfx);
    icu.bidi_paralevel = sym(h, "ubidi_getParaLevel", sfx);
    icu.bidi_reorder = sym(h, "ubidi_reorderVisual", sfx);
    icu.ok = icu.brk_open && icu.brk_close && icu.brk_first && icu.brk_next && icu.brk_status && icu.bidi_open &&
             icu.bidi_close && icu.bidi_setpara && icu.bidi_getlevels && icu.bidi_paralevel && icu.bidi_reorder;
}
static ct_icu_t *ICU(void) {
    if (icu_state == 2) return icu.ok ? &icu : 0;
    int expected = 0;
    if (__atomic_compare_exchange_n(&icu_state, &expected, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        icu_load();
        __atomic_store_n(&icu_state, 2, __ATOMIC_RELEASE);
    } else
        while (__atomic_load_n(&icu_state, __ATOMIC_ACQUIRE) != 2) { }
    return icu.ok ? &icu : 0;
}
int ctl_has_icu(void) { return ICU() != 0; }

static u32 cp_at(const u16 *s, long n, long i, int *units) {
    u32 c = s[i];
    *units = 1;
    if (c >= 0xD800 && c < 0xDC00 && i + 1 < n && s[i + 1] >= 0xDC00 && s[i + 1] < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (s[i + 1] - 0xDC00); *units = 2; }
    return c;
}
static int is_mark(u32 c) {
    return (c >= 0x300 && c < 0x370) || (c >= 0x483 && c < 0x48A) || (c >= 0x591 && c < 0x5C8) || (c >= 0x610 && c < 0x61B) ||
           (c >= 0x64B && c < 0x660) || c == 0x670 || (c >= 0x6D6 && c < 0x6EE) || (c >= 0x900 && c < 0x904) || (c >= 0x93A && c < 0x950) ||
           (c >= 0x1AB0 && c < 0x1B00) || (c >= 0x1DC0 && c < 0x1E00) || (c >= 0x20D0 && c < 0x2100) || (c >= 0xFE00 && c < 0xFE10) ||
           (c >= 0xFE20 && c < 0xFE30) || c == 0x200D || (c >= 0x1F3FB && c <= 0x1F3FF) || (c >= 0xE0100 && c < 0xE01F0);
}
static int is_hard(u32 c) { return c == '\n' || c == '\r' || c == 0x0B || c == 0x0C || c == 0x85 || c == 0x2028 || c == 0x2029; }

/* brk[i] (i = 1..n) para a posição antes de s[i]: 0 nenhuma, 1 permitida, 2 obrigatória (depois de quebra
 * de parágrafo/linha). brk[n] vale para o fim. brk[0] = 0. */
void ctl_line_breaks(const u16 *s, long n, u8 *brk) {
    memset(brk, 0, (size_t)n + 1);
    ct_icu_t *I = ICU();
    if (I && n > 0) {
        UErrorCode e = 0;
        void *b = I->brk_open(2 /* UBRK_LINE */, "", s, (int32_t)n, &e);
        if (b && e <= 0) {
            for (int32_t p = I->brk_first(b); p != -1; p = I->brk_next(b)) {
                if (p <= 0 || p > n) continue;
                int st = I->brk_status(b);
                brk[p] = st >= 100 && st < 200 ? 2 : 1;          /* UBRK_LINE_HARD */
            }
            I->brk_close(b);
            return;
        }
        if (b) I->brk_close(b);
    }
    for (long i = 0; i < n;) {
        int u;
        u32 c = cp_at(s, n, i, &u);
        long nx = i + u;
        if (is_hard(c)) {
            if (c == '\r' && nx < n && s[nx] == '\n') { i = nx; continue; }
            brk[nx] = 2;
        } else if ((c == ' ' || c == '\t' || c == 0x3000) && nx <= n) {
            int u2;
            if (nx < n && (cp_at(s, n, nx, &u2) == ' ')) { i = nx; continue; }
            brk[nx] = 1;
        } else if (c >= 0x2E80 && c < 0xA000 && nx < n) brk[nx] = 1;   /* CJK: entre ideogramas */
        i = nx;
    }
    if (n > 0 && !brk[n]) brk[n] = 1;                                  /* o fim do texto */
}
/* cl[i] = 1 se há limite de grafema antes de s[i] (i = 0..n) */
void ctl_clusters(const u16 *s, long n, u8 *cl) {
    memset(cl, 0, (size_t)n + 1);
    if (n >= 0) { cl[0] = 1; cl[n] = 1; }
    ct_icu_t *I = ICU();
    if (I && n > 0) {
        UErrorCode e = 0;
        void *b = I->brk_open(0 /* UBRK_CHARACTER */, "", s, (int32_t)n, &e);
        if (b && e <= 0) {
            for (int32_t p = I->brk_first(b); p != -1; p = I->brk_next(b)) if (p >= 0 && p <= n) cl[p] = 1;
            I->brk_close(b);
            return;
        }
        if (b) I->brk_close(b);
    }
    for (long i = 0; i < n;) {
        int u;
        u32 c = cp_at(s, n, i, &u);
        int after_zwj = i && s[i - 1] == 0x200D && c >= 0x1F000;      /* sequência de emoji com ZWJ */
        if (i && !is_mark(c) && !after_zwj && !(c == '\n' && s[i - 1] == '\r')) cl[i] = 1;
        i += u;
    }
}
/* classe bidi simplificada para o caminho sem ICU */
static int simple_class(u32 cp) {                     /* 0 = L, 1 = R, 2 = número, -1 = neutro */
    if ((cp >= '0' && cp <= '9') || (cp >= 0x0660 && cp <= 0x0669) || (cp >= 0x06F0 && cp <= 0x06F9)) return 2;
    if ((cp >= 0x0590 && cp <= 0x08FF) || (cp >= 0xFB1D && cp <= 0xFDFF) || (cp >= 0xFE70 && cp <= 0xFEFF) ||
        (cp >= 0x10800 && cp <= 0x10FFF) || (cp >= 0x1E800 && cp <= 0x1EFFF))
        return 1;
    if (cp < 0x80) return ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z')) ? 0 : -1;
    if ((cp >= 0x2000 && cp <= 0x206F) || (cp >= 0x3000 && cp <= 0x303F) || (cp >= 0xA0 && cp <= 0xBF) || cp == 0xD7 || cp == 0xF7) return -1;
    return 0;
}
/* níveis de embutimento por unidade UTF-16 (todos os parágrafos do texto). para: 0 LTR, 1 RTL, −1 pelo
 * primeiro forte (P2/P3). Devolve o nível do primeiro parágrafo. */
int ctl_bidi_levels(const u16 *s, long n, int para, u8 *levels) {
    if (n <= 0) return para > 0 ? 1 : 0;
    ct_icu_t *I = ICU();
    if (I) {
        void *b = I->bidi_open();
        if (b) {
            UErrorCode e = 0;
            I->bidi_setpara(b, s, (int32_t)n, (u8)(para < 0 ? 0xFE /* UBIDI_DEFAULT_LTR */ : para), 0, &e);
            const u8 *lv = e <= 0 ? I->bidi_getlevels(b, &e) : 0;
            if (lv && e <= 0) {
                memcpy(levels, lv, (size_t)n);
                int pl = I->bidi_paralevel(b);
                I->bidi_close(b);
                return pl;
            }
            I->bidi_close(b);
        }
    }
    int base = para > 0 ? 1 : 0;
    if (para < 0)
        for (long i = 0; i < n; i++) { int u; int c = simple_class(cp_at(s, n, i, &u)); if (c == 0 || c == 1) { base = c; break; } }
    int strong = base;
    for (long i = 0; i < n;) {
        int u, c = simple_class(cp_at(s, n, i, &u)), dir;
        if (c == 0 || c == 1) strong = c;
        if (c < 0) {
            int before = strong, after = base;
            for (long j = i + u; j < n;) { int u2, cj = simple_class(cp_at(s, n, j, &u2)); if (cj >= 0) { after = cj == 2 ? strong : cj; break; } j += u2; }
            dir = before == after ? before : base;
        } else dir = c == 2 ? strong : c;
        int lv = c == 2 ? (dir || base ? 2 : 0) : (dir ? (base ? 1 : 1) : (base ? 2 : 0));
        if (base && dir && c != 2) lv = 1;
        for (int k = 0; k < u; k++) levels[i + k] = (u8)lv;
        i += u;
    }
    return base;
}
static int is_ws(u16 c) { return c == ' ' || c == '\t' || c == 0x3000 || (c >= 0x2000 && c <= 0x200A) || c == 0x205F; }
/* ordem visual de uma linha [a, b) do texto (L1: espaço e tabulação do fim e antes de tabulação ficam no nível do
 * parágrafo; L2: inverte do nível mais alto até 1). map[k] = índice lógico (em relação a a) da posição visual k. */
void ctl_reorder_line(const u16 *s, const u8 *levels, long a, long b, int para_level, int *map) {
    long n = b - a;
    if (n <= 0) return;
    u8 *lv = malloc((size_t)n);
    if (!lv) { for (long i = 0; i < n; i++) map[i] = (int32_t)i; return; }
    memcpy(lv, levels + a, (size_t)n);
    for (long i = n - 1; i >= 0 && (is_ws(s[a + i]) || s[a + i] == '\n' || s[a + i] == '\r' || s[a + i] == 0x2028 || s[a + i] == 0x2029); i--) lv[i] = (u8)para_level;
    for (long i = 0; i < n; i++)
        if (s[a + i] == '\t') { lv[i] = (u8)para_level; for (long j = i - 1; j >= 0 && is_ws(s[a + j]); j--) lv[j] = (u8)para_level; }
    ct_icu_t *I = ICU();
    if (I) { I->bidi_reorder(lv, (int32_t)n, map); free(lv); return; }
    for (long i = 0; i < n; i++) map[i] = (int32_t)i;
    int maxl = 0, minodd = 255;
    for (long i = 0; i < n; i++) { if (lv[i] > maxl) maxl = lv[i]; if ((lv[i] & 1) && lv[i] < minodd) minodd = lv[i]; }
    for (int k = maxl; k >= 1 && k >= (minodd == 255 ? maxl + 1 : minodd); k--)
        for (long i = 0; i < n;) {
            if (lv[map[i]] < k) { i++; continue; }
            long j = i;
            while (j < n && lv[map[j]] >= k) j++;
            for (long x = i, y = j - 1; x < y; x++, y--) { int32_t t = map[x]; map[x] = map[y]; map[y] = t; }
            i = j;
        }
    free(lv);
}
