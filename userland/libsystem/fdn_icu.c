/*
 * mOS libSystem — carregador da ICU para a Foundation (fdn_icu.h).
 *
 * No mOS: dlopen("/usr/lib/libicucore.A.dylib") e dlsym com o nome sem versão (a libicucore é compilada com
 * U_DISABLE_RENAMING, como a da Apple). Se o app já carregou a libicucore, o dlopen devolve a mesma imagem.
 * No host (FDN_ICU_HOST): a ICU do Linux, libicui18n/libicuuc, com o sufixo _NN da versão.
 */
#include "fdn_icu.h"
#ifdef FDN_ICU_HOST
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include "libc.h"
void *dlopen(const char *path, int mode);
void *dlsym(void *h, const char *name);
int snprintf(char *b, size_t n, const char *fmt, ...);
#endif

static fdn_icu_t g_icu;
static volatile int g_state;   /* 0 nunca tentou, 1 carregando, 2 pronto */

static void *sym(void *h, const char *name, const char *suffix) {
    char full[96];
    if (!suffix[0]) return dlsym(h, name);
    snprintf(full, sizeof full, "%s%s", name, suffix);
    return dlsym(h, full);
}

static void load(void) {
    void *h;
    const char *suffix = "";
#ifdef FDN_ICU_HOST
    static char sfx[8];
    void *uc = 0;
    h = 0;
    for (int v = 90; v >= 60 && !h; v--) {
        char p1[64], p2[64];
        snprintf(p1, sizeof p1, "libicui18n.so.%d", v);
        snprintf(p2, sizeof p2, "libicuuc.so.%d", v);
        uc = dlopen(p2, RTLD_NOW | RTLD_GLOBAL);
        if (!uc) continue;
        h = dlopen(p1, RTLD_NOW | RTLD_GLOBAL);
        if (h) snprintf(sfx, sizeof sfx, "_%d", v);
    }
    suffix = sfx;
    if (!h) return;
    (void)uc;
    h = RTLD_DEFAULT;
#else
    h = dlopen("/usr/lib/libicucore.A.dylib", 0);
    if (!h) return;
#endif
#define FDN_ICU_LOAD(name, ret, args) g_icu.name = (ret (*) args)sym(h, #name, suffix);
    FDN_ICU_FUNCS(FDN_ICU_LOAD)
#undef FDN_ICU_LOAD
    /* o mínimo para locale, calendário e formatação: sem isso a tabela não conta como carregada */
    g_icu.ok = g_icu.uloc_getLanguage && g_icu.uloc_getCountry && g_icu.ucal_open && g_icu.ucal_get &&
               g_icu.udat_open && g_icu.udat_format && g_icu.unum_open && g_icu.unum_formatDouble;
}

const fdn_icu_t *fdn_icu(void) {
    if (g_state == 2) return &g_icu;
    int expected = 0;
    if (__atomic_compare_exchange_n(&g_state, &expected, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        load();
        __atomic_store_n(&g_state, 2, __ATOMIC_RELEASE);
    } else {
        while (__atomic_load_n(&g_state, __ATOMIC_ACQUIRE) != 2) { }
    }
    return &g_icu;
}

/* ------------------------------------------------------------------ UTF-8 / UTF-16 */

int32_t fdn_u16_from_utf8(const char *src, int32_t n, UChar *out, int32_t cap) {
    const unsigned char *s = (const unsigned char *)src;
    int32_t i = 0, o = 0;
    if (!s) return 0;
    if (n < 0) n = (int32_t)strlen(src);
    while (i < n) {
        uint32_t c = s[i], cp;
        int need;
        if (c < 0x80) { cp = c; need = 0; }
        else if (c >= 0xC2 && c <= 0xDF) { cp = c & 0x1F; need = 1; }
        else if (c >= 0xE0 && c <= 0xEF) { cp = c & 0x0F; need = 2; }
        else if (c >= 0xF0 && c <= 0xF4) { cp = c & 0x07; need = 3; }
        else { cp = 0xFFFD; need = -1; }
        i++;
        for (int k = 0; k < need; k++) {
            if (i >= n || (s[i] & 0xC0) != 0x80) { cp = 0xFFFD; break; }
            cp = (cp << 6) | (s[i++] & 0x3F);
        }
        if ((need == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) || (need == 3 && (cp < 0x10000 || cp > 0x10FFFF)))
            cp = 0xFFFD;
        if (cp >= 0x10000) {
            if (o + 1 < cap) { out[o] = (UChar)(0xD800 + ((cp - 0x10000) >> 10)); out[o + 1] = (UChar)(0xDC00 + ((cp - 0x10000) & 0x3FF)); }
            o += 2;
        } else {
            if (o < cap) out[o] = (UChar)cp;
            o++;
        }
    }
    if (o < cap) out[o] = 0;
    return o;
}

int32_t fdn_utf8_from_u16(const UChar *s, int32_t n, char *out, int32_t cap) {
    int32_t o = 0;
    if (!s) n = 0;
    if (n < 0) { n = 0; while (s[n]) n++; }
    for (int32_t i = 0; i < n; i++) {
        uint32_t c = s[i];
        unsigned char b[4];
        int k;
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < n && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (s[i + 1] - 0xDC00);
            i++;
        } else if (c >= 0xD800 && c <= 0xDFFF) c = 0xFFFD;
        if (c < 0x80) { b[0] = (unsigned char)c; k = 1; }
        else if (c < 0x800) { b[0] = (unsigned char)(0xC0 | (c >> 6)); b[1] = (unsigned char)(0x80 | (c & 0x3F)); k = 2; }
        else if (c < 0x10000) { b[0] = (unsigned char)(0xE0 | (c >> 12)); b[1] = (unsigned char)(0x80 | ((c >> 6) & 0x3F)); b[2] = (unsigned char)(0x80 | (c & 0x3F)); k = 3; }
        else { b[0] = (unsigned char)(0xF0 | (c >> 18)); b[1] = (unsigned char)(0x80 | ((c >> 12) & 0x3F)); b[2] = (unsigned char)(0x80 | ((c >> 6) & 0x3F)); b[3] = (unsigned char)(0x80 | (c & 0x3F)); k = 4; }
        for (int j = 0; j < k; j++) { if (o < cap) out[o] = (char)b[j]; o++; }
    }
    if (o < cap) out[o] = 0;
    else if (cap > 0) out[cap - 1] = 0;
    return o;
}

UChar *fdn_u16_dup(const char *utf8, int32_t *len) {
    int32_t n = fdn_u16_from_utf8(utf8 ? utf8 : "", -1, 0, 0);
    UChar *r = malloc(((size_t)n + 1) * sizeof(UChar));
    if (!r) { if (len) *len = 0; return 0; }
    fdn_u16_from_utf8(utf8 ? utf8 : "", -1, r, n + 1);
    if (len) *len = n;
    return r;
}

char *fdn_utf8_dup(const UChar *s, int32_t n) {
    int32_t m = fdn_utf8_from_u16(s, n, 0, 0);
    char *r = malloc((size_t)m + 1);
    if (!r) return 0;
    fdn_utf8_from_u16(s, n, r, m + 1);
    return r;
}
