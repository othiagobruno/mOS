/*
 * mOS libSystem — busca, comparação, caixa, fronteiras e transliteração do NSString (ver fdn_text_core.h).
 *
 * Busca com opções: cada ponto de código do texto vira uma "chave" (NFD se não for literal, sem marcas com
 * TX_DIACRITIC, largura normal com TX_WIDTH, caixa dobrada com TX_CASE); a agulha vira a sequência das chaves e
 * o casamento precisa começar e terminar em fronteira de ponto de código. Assim os índices voltam no texto
 * original, mesmo quando a dobra muda o tamanho (ß -> ss).
 */
#include "fdn_text_core.h"

void *malloc(size_t n);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);

#define U_NON_SPACING_MARK 6
#define U_DECIMAL_DIGIT 9

static UChar32 cp_at(const UChar *s, int32_t n, int32_t i, int32_t *w) {
    UChar32 c = s[i];
    *w = 1;
    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < n && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
        c = 0x10000 + ((c - 0xD800) << 10) + (s[i + 1] - 0xDC00);
        *w = 2;
    }
    return c;
}
static int32_t put16(UChar *o, UChar32 c) {
    if (c < 0x10000) { o[0] = (UChar)c; return 1; }
    c -= 0x10000;
    o[0] = (UChar)(0xD800 + (c >> 10));
    o[1] = (UChar)(0xDC00 + (c & 0x3FF));
    return 2;
}
/* chaves de um ponto de código; devolve quantas (até 18) */
static int keys_of(const fdn_icu_t *I, UChar32 c, unsigned opts, UChar32 *out) {
    UChar a[24], b[24];
    int32_t n = put16(a, c);
    UErrorCode e = U_ZERO_ERROR;
    if (I && I->ok) {
        if ((opts & TX_WIDTH) && ((c >= 0xFF00 && c <= 0xFFEF) || c == 0x3000)) {
            const void *k = I->unorm2_getNFKCInstance(&e);
            int32_t m = k && !U_FAILURE(e) ? I->unorm2_normalize(k, a, n, b, 24, &e) : -1;
            if (m > 0 && !U_FAILURE(e)) { memcpy(a, b, (size_t)m * 2); n = m; }
            e = U_ZERO_ERROR;
        }
        if (!(opts & TX_LITERAL) || (opts & TX_DIACRITIC)) {
            const void *d = I->unorm2_getNFDInstance(&e);
            int32_t m = d && !U_FAILURE(e) ? I->unorm2_normalize(d, a, n, b, 24, &e) : -1;
            if (m > 0 && !U_FAILURE(e)) { memcpy(a, b, (size_t)m * 2); n = m; }
            e = U_ZERO_ERROR;
        }
        if (opts & TX_CASE) {
            int32_t m = I->u_strFoldCase(b, 24, a, n, 0, &e);
            if (m > 0 && !U_FAILURE(e)) { memcpy(a, b, (size_t)m * 2); n = m; }
        }
    } else if ((opts & TX_CASE) && c >= 'A' && c <= 'Z') a[0] = (UChar)(c + 32);
    int k = 0;
    for (int32_t i = 0, w; i < n && k < 18; i += w) {
        UChar32 x = cp_at(a, n, i, &w);
        if ((opts & TX_DIACRITIC) && I && I->ok && I->u_charType(x) == U_NON_SPACING_MARK) continue;
        out[k++] = x;
    }
    return k;
}
typedef struct { UChar32 *k; int32_t *owner; int32_t nk; int32_t *ost, *oen, *ok0, *okn; int32_t no; } keyed_t;
static void keyed_free(keyed_t *t) { free(t->k); free(t->owner); free(t->ost); free(t->oen); free(t->ok0); free(t->okn); }
static int keyed(const fdn_icu_t *I, const UChar *s, int32_t n, unsigned opts, keyed_t *t) {
    int32_t cap = n * 4 + 8;
    t->k = malloc((size_t)cap * sizeof(UChar32));
    t->owner = malloc((size_t)cap * sizeof(int32_t));
    t->ost = malloc((size_t)(n + 1) * sizeof(int32_t));
    t->oen = malloc((size_t)(n + 1) * sizeof(int32_t));
    t->ok0 = malloc((size_t)(n + 1) * sizeof(int32_t));
    t->okn = malloc((size_t)(n + 1) * sizeof(int32_t));
    t->nk = t->no = 0;
    if (!t->k || !t->owner || !t->ost || !t->oen || !t->ok0 || !t->okn) return 0;
    for (int32_t i = 0, w; i < n; i += w) {
        UChar32 c = cp_at(s, n, i, &w), tmp[18];
        int m = keys_of(I, c, opts, tmp);
        if (t->nk + m > cap) {
            cap = (t->nk + m) * 2;
            t->k = realloc(t->k, (size_t)cap * sizeof(UChar32));
            t->owner = realloc(t->owner, (size_t)cap * sizeof(int32_t));
        }
        t->ost[t->no] = i; t->oen[t->no] = i + w; t->ok0[t->no] = t->nk; t->okn[t->no] = m;
        for (int j = 0; j < m; j++) { t->k[t->nk] = tmp[j]; t->owner[t->nk] = t->no; t->nk++; }
        t->no++;
    }
    return 1;
}
static int is_mark(const fdn_icu_t *I, UChar32 c) { return I && I->ok && I->u_charType(c) == U_NON_SPACING_MARK; }

static int search_regex(const fdn_icu_t *I, const UChar *hay, int32_t hn, const UChar *pat, int32_t pn, unsigned opts, int32_t *loc, int32_t *len) {
    if (!I || !I->ok || !I->uregex_open) return 0;
    UParseError pe;
    UErrorCode e = U_ZERO_ERROR;
    void *re = I->uregex_open(pat, pn, (opts & TX_CASE) ? 2u : 0u, &pe, &e);   /* UREGEX_CASE_INSENSITIVE = 2 */
    if (!re || U_FAILURE(e)) { if (re) I->uregex_close(re); return 0; }
    I->uregex_setText(re, hay, hn, &e);
    int found = 0;
    int32_t best = -1, blen = 0;
    if (opts & TX_ANCHORED) {
        if (!(opts & TX_BACKWARDS)) {
            if (I->uregex_lookingAt && I->uregex_lookingAt(re, 0, &e)) { best = 0; blen = I->uregex_end(re, 0, &e); found = 1; }
        } else {
            for (int32_t st = hn; st >= 0 && !found; st--) {
                e = U_ZERO_ERROR;
                if (I->uregex_find(re, st, &e) && I->uregex_end(re, 0, &e) == hn) { best = I->uregex_start(re, 0, &e); blen = hn - best; found = 1; }
            }
        }
    } else if (I->uregex_find(re, 0, &e)) {
        do {
            best = I->uregex_start(re, 0, &e);
            blen = I->uregex_end(re, 0, &e) - best;
            found = 1;
        } while ((opts & TX_BACKWARDS) && I->uregex_findNext(re, &e));
    }
    I->uregex_close(re);
    if (found) { *loc = best; *len = blen; }
    return found;
}

int tx_search(const UChar *hay, int32_t hn, const UChar *needle, int32_t nn, unsigned opts, int32_t *loc, int32_t *len) {
    const fdn_icu_t *I = fdn_icu();
    if (!nn || !needle) return 0;
    if (opts & TX_REGEX) return search_regex(I, hay, hn, needle, nn, opts, loc, len);
    keyed_t H = {0}, N = {0};
    int found = 0;
    if (!keyed(I, hay, hn, opts, &H) || !keyed(I, needle, nn, opts, &N) || !N.nk) goto out;
    for (int32_t step = 0; step < H.no; step++) {
        int32_t o = (opts & TX_BACKWARDS) ? H.no - 1 - step : step;
        if ((opts & TX_ANCHORED) && !(opts & TX_BACKWARDS) && o != 0) break;
        if (!H.okn[o]) continue;
        int32_t k0 = H.ok0[o];
        if (k0 + N.nk > H.nk) continue;
        int32_t j = 0;
        while (j < N.nk && H.k[k0 + j] == N.k[j]) j++;
        if (j < N.nk) continue;
        int32_t last = H.owner[k0 + N.nk - 1];
        if (H.ok0[last] + H.okn[last] != k0 + N.nk) continue;   /* terminou no meio das chaves de um ponto de código */
        int32_t end_o = last + 1;
        if (opts & TX_DIACRITIC) while (end_o < H.no && !H.okn[end_o]) end_o++;   /* marcas soltas vão junto */
        else if (end_o < H.no && is_mark(I, cp_at(hay, hn, H.ost[end_o], &j))) continue;   /* a base sozinha não casa */
        if ((opts & TX_ANCHORED) && (opts & TX_BACKWARDS) && end_o != H.no) continue;   /* tem de terminar no fim */
        *loc = H.ost[o];
        *len = H.oen[end_o - 1] - H.ost[o];
        found = 1;
        break;
    }
out:
    keyed_free(&H);
    keyed_free(&N);
    return found;
}

static int is_digit(UChar32 c) { return c >= '0' && c <= '9'; }
int tx_compare(const UChar *a, int32_t an, const UChar *b, int32_t bn, unsigned opts, const char *locale) {
    const fdn_icu_t *I = fdn_icu();
    if (locale && I && I->ok && I->ucol_open) {
        UErrorCode e = U_ZERO_ERROR;
        void *col = I->ucol_open(locale, &e);
        if (col && !U_FAILURE(e)) {
            /* força: diacríticos ignorados = primária; só caixa = secundária; senão terciária */
            int32_t strength = (opts & TX_DIACRITIC) ? 0 : (opts & TX_CASE) ? 1 : 2;
            I->ucol_setStrength(col, strength);
            if (opts & TX_NUMERIC) I->ucol_setAttribute(col, 7, 17, &e);   /* UCOL_NUMERIC_COLLATION = ON */
            int32_t r = I->ucol_strcoll(col, a, an, b, bn);
            if (!r && (opts & TX_FORCED)) { I->ucol_setStrength(col, 3); r = I->ucol_strcoll(col, a, an, b, bn); }
            I->ucol_close(col);
            if (r || !(opts & TX_FORCED)) return r < 0 ? -1 : r > 0;
            return tx_compare(a, an, b, bn, TX_LITERAL, 0);
        }
        if (col) I->ucol_close(col);
    }
    keyed_t A = {0}, B = {0};
    int r = 0;
    if (!keyed(I, a, an, opts, &A) || !keyed(I, b, bn, opts, &B)) { keyed_free(&A); keyed_free(&B); return 0; }
    int32_t i = 0, j = 0;
    while (i < A.nk && j < B.nk && !r) {
        if ((opts & TX_NUMERIC) && is_digit(A.k[i]) && is_digit(B.k[j])) {
            while (i + 1 < A.nk && A.k[i] == '0' && is_digit(A.k[i + 1])) i++;
            while (j + 1 < B.nk && B.k[j] == '0' && is_digit(B.k[j + 1])) j++;
            int32_t ie = i, je = j;
            while (ie < A.nk && is_digit(A.k[ie])) ie++;
            while (je < B.nk && is_digit(B.k[je])) je++;
            if (ie - i != je - j) { r = ie - i < je - j ? -1 : 1; break; }
            for (; i < ie && !r; i++, j++) if (A.k[i] != B.k[j]) r = A.k[i] < B.k[j] ? -1 : 1;
            continue;
        }
        if (A.k[i] != B.k[j]) r = A.k[i] < B.k[j] ? -1 : 1;
        i++; j++;
    }
    if (!r) r = i < A.nk ? 1 : j < B.nk ? -1 : 0;
    keyed_free(&A);
    keyed_free(&B);
    if (!r && (opts & TX_FORCED) && (opts & ~(unsigned)TX_FORCED)) return tx_compare(a, an, b, bn, TX_LITERAL, 0);
    return r;
}

UChar *tx_case(int kind, const UChar *s, int32_t n, const char *locale, int32_t *on) {
    const fdn_icu_t *I = fdn_icu();
    int32_t cap = n * 3 + 8;
    UChar *o = malloc((size_t)cap * sizeof(UChar));
    if (!o) { *on = 0; return 0; }
    if (!I || !I->ok) {
        for (int32_t i = 0; i < n; i++) {
            UChar c = s[i];
            if (kind == 0 && c >= 'a' && c <= 'z') c -= 32;
            else if (kind == 1 && c >= 'A' && c <= 'Z') c += 32;
            o[i] = c;
        }
        *on = n;
        return o;
    }
    UErrorCode e = U_ZERO_ERROR;
    const char *l = locale ? locale : "";
    int32_t m = kind == 0 ? I->u_strToUpper(o, cap, s, n, l, &e) : kind == 1 ? I->u_strToLower(o, cap, s, n, l, &e)
                          : I->u_strToTitle(o, cap, s, n, 0, l, &e);
    if (U_FAILURE(e)) { memcpy(o, s, (size_t)n * 2); m = n; }
    *on = m;
    return o;
}

UChar *tx_fold(const UChar *s, int32_t n, unsigned opts, int32_t *on) {
    const fdn_icu_t *I = fdn_icu();
    int32_t cap = n * 4 + 8, k = 0;
    UChar *o = malloc((size_t)cap * sizeof(UChar));
    if (!o) { *on = 0; return 0; }
    for (int32_t i = 0, w; i < n; i += w) {
        UChar32 c = cp_at(s, n, i, &w), t[18];
        int m = keys_of(I, c, opts | TX_LITERAL, t);
        if (k + m * 2 > cap) { cap = (k + m * 2) * 2; o = realloc(o, (size_t)cap * sizeof(UChar)); }
        for (int j = 0; j < m; j++) k += put16(o + k, t[j]);
    }
    /* recompõe (NFC) o que sobrou decomposto */
    if (I && I->ok) {
        UErrorCode e = U_ZERO_ERROR;
        const void *nfc = I->unorm2_getNFCInstance(&e);
        UChar *r = malloc((size_t)(k + 8) * sizeof(UChar));
        int32_t m = nfc && r && !U_FAILURE(e) ? I->unorm2_normalize(nfc, o, k, r, k + 8, &e) : -1;
        if (m >= 0 && !U_FAILURE(e)) { free(o); *on = m; return r; }
        free(r);
    }
    *on = k;
    return o;
}

void tx_composed(const UChar *s, int32_t n, int32_t idx, int32_t *st, int32_t *len) {
    const fdn_icu_t *I = fdn_icu();
    *st = idx; *len = idx < n ? 1 : 0;
    if (idx >= n) return;
    if (I && I->ok && I->ubrk_open) {
        UErrorCode e = U_ZERO_ERROR;
        void *bi = I->ubrk_open(0, "", s, n, &e);   /* UBRK_CHARACTER */
        if (bi && !U_FAILURE(e)) {
            int32_t a = idx == 0 ? 0 : I->ubrk_preceding(bi, idx + 1);
            if (a < 0) a = 0;
            int32_t b = I->ubrk_following(bi, a);
            if (b < 0 || b <= idx) b = n;
            *st = a; *len = b - a;
            I->ubrk_close(bi);
            return;
        }
        if (bi) I->ubrk_close(bi);
    }
    int32_t w;
    if (idx > 0 && s[idx] >= 0xDC00 && s[idx] <= 0xDFFF && s[idx - 1] >= 0xD800 && s[idx - 1] <= 0xDBFF) idx--;
    cp_at(s, n, idx, &w);
    *st = idx; *len = w;
}

static int is_term(UChar c, int para) { return c == '\n' || c == '\r' || c == 0x85 || c == 0x2029 || (!para && c == 0x2028); }
void tx_line(const UChar *s, int32_t n, int32_t loc, int32_t len, int para, int32_t *start, int32_t *end, int32_t *cend) {
    if (loc > n) loc = n;
    if (len < 0 || loc + len > n) len = n - loc;
    int32_t p = loc;
    if (p > 0 && p < n && s[p] == '\n' && s[p - 1] == '\r') p--;
    while (p > 0 && !is_term(s[p - 1], para)) p--;
    int32_t q = len > 0 ? loc + len - 1 : loc;
    if (q < p) q = p;
    if (len > 0 && q < n && is_term(s[q], para)) {
        /* o último caractere da faixa é o terminador da linha */
        int32_t c = q;
        if (s[q] == '\n' && q > p && s[q - 1] == '\r') c = q - 1;
        if (s[q] == '\r' && q + 1 < n && s[q + 1] == '\n') q++;
        if (start) *start = p;
        if (cend) *cend = c;
        if (end) *end = q + 1;
        return;
    }
    while (q < n && !is_term(s[q], para)) q++;
    if (cend) *cend = q;
    if (q < n) q += (s[q] == '\r' && q + 1 < n && s[q + 1] == '\n') ? 2 : 1;
    if (start) *start = p;
    if (end) *end = q;
}

int tx_segments(const UChar *s, int32_t n, int kind, const char *locale, int (*fn)(void *, int32_t, int32_t), void *ctx) {
    const fdn_icu_t *I = fdn_icu();
    if (!I || !I->ok || !I->ubrk_open) return 0;
    UErrorCode e = U_ZERO_ERROR;
    void *bi = I->ubrk_open(kind, locale ? locale : "", s, n, &e);
    if (!bi || U_FAILURE(e)) { if (bi) I->ubrk_close(bi); return 0; }
    int32_t a = I->ubrk_first(bi);
    for (int32_t b = I->ubrk_next(bi); b >= 0; a = b, b = I->ubrk_next(bi)) {
        if (kind == 1 && I->ubrk_getRuleStatus(bi) < 100) continue;   /* UBRK_WORD_NONE: espaço e pontuação */
        if (kind == 3) {
            /* frase sem o espaço do fim, como a Foundation */
            int32_t z = b;
            while (z > a && (s[z - 1] == ' ' || s[z - 1] == '\t' || s[z - 1] == '\n' || s[z - 1] == '\r')) z--;
            if (z > a && fn(ctx, a, z)) break;
            continue;
        }
        if (fn(ctx, a, b)) break;
    }
    I->ubrk_close(bi);
    return 1;
}

UChar *tx_transform(const UChar *s, int32_t n, const char *id, int reverse, int32_t *on) {
    const fdn_icu_t *I = fdn_icu();
    *on = 0;
    if (!I || !I->ok || !I->utrans_openU || !id) return 0;
    UChar uid[256];
    int32_t idn = fdn_u16_from_utf8(id, -1, uid, 256);
    if (idn > 255) return 0;
    UParseError pe;
    UErrorCode e = U_ZERO_ERROR;
    void *t = I->utrans_openU(uid, idn, reverse ? 1 : 0, 0, 0, &pe, &e);
    if (!t || U_FAILURE(e)) { if (t) I->utrans_close(t); return 0; }
    int32_t cap = n * 6 + 64, len = n, limit = n;
    UChar *b = malloc((size_t)cap * sizeof(UChar));
    if (!b) { I->utrans_close(t); return 0; }
    memcpy(b, s, (size_t)n * 2);
    I->utrans_transUChars(t, b, &len, cap, 0, &limit, &e);
    if (e == U_BUFFER_OVERFLOW_ERROR) {
        cap = len + 16;
        b = realloc(b, (size_t)cap * sizeof(UChar));
        memcpy(b, s, (size_t)n * 2);
        len = n; limit = n; e = U_ZERO_ERROR;
        I->utrans_transUChars(t, b, &len, cap, 0, &limit, &e);
    }
    I->utrans_close(t);
    if (U_FAILURE(e)) { free(b); return 0; }
    *on = len;
    return b;
}

/* ---------------------------------------------------------------- partes do NSString (fdn_string.c) */
int tx_utf8_unit_at(const char *str, size_t n, size_t idx, UChar *out, size_t *total) {
    const unsigned char *p = (const unsigned char *)str;
    size_t k = 0;
    for (size_t i = 0; i < n;) {
        unsigned b = p[i];
        uint32_t cp;
        int w;
        if (b < 0x80) { cp = b; w = 1; }
        else if (b >= 0xF0 && i + 3 < n) { cp = ((uint32_t)(b & 7) << 18) | ((uint32_t)(p[i + 1] & 0x3F) << 12) | ((uint32_t)(p[i + 2] & 0x3F) << 6) | (p[i + 3] & 0x3F); w = 4; }
        else if (b >= 0xE0 && b < 0xF0 && i + 2 < n) { cp = ((uint32_t)(b & 15) << 12) | ((uint32_t)(p[i + 1] & 0x3F) << 6) | (p[i + 2] & 0x3F); w = 3; }
        else if (b >= 0xC0 && b < 0xE0 && i + 1 < n) { cp = ((uint32_t)(b & 31) << 6) | (p[i + 1] & 0x3F); w = 2; }
        else { cp = 0xFFFD; w = 1; }
        if (cp >= 0x10000) {
            if (k == idx) { *out = (UChar)(0xD800 + ((cp - 0x10000) >> 10)); return 1; }
            if (k + 1 == idx) { *out = (UChar)(0xDC00 + ((cp - 0x10000) & 0x3FF)); return 1; }
            k += 2;
        } else {
            if (k == idx) { *out = (UChar)cp; return 1; }
            k++;
        }
        i += (size_t)w;
    }
    if (total) *total = k;
    return 0;
}

int tx_find_set(const UChar *u, int32_t a, int32_t e, unsigned opts, int (*has)(void *, uint32_t), void *ctx, int32_t *loc, int32_t *len) {
    int back = (opts & TX_BACKWARDS) != 0, anch = (opts & TX_ANCHORED) != 0;
    for (int32_t i = back ? e - 1 : a; back ? i >= a : i < e;) {
        uint32_t cp = u[i];
        int32_t st = i, w = 1;
        if (!back && cp >= 0xD800 && cp <= 0xDBFF && i + 1 < e && u[i + 1] >= 0xDC00 && u[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (u[i + 1] - 0xDC00); w = 2;
        } else if (back && cp >= 0xDC00 && cp <= 0xDFFF && i - 1 >= a && u[i - 1] >= 0xD800 && u[i - 1] <= 0xDBFF) {
            cp = 0x10000 + ((uint32_t)(u[i - 1] - 0xD800) << 10) + (cp - 0xDC00); st = i - 1; w = 2;
        }
        if (has(ctx, cp)) { *loc = st; *len = w; return 1; }
        if (anch) break;
        i = back ? st - 1 : i + w;
    }
    return 0;
}

typedef struct { int32_t *v; int n, cap; } segs_t;
static void seg_add(segs_t *g, int32_t a, int32_t b, int32_t ea, int32_t eb) {
    if (g->n + 4 > g->cap) { g->cap = g->cap ? g->cap * 2 : 64; g->v = realloc(g->v, (size_t)g->cap * sizeof(int32_t)); }
    g->v[g->n++] = a; g->v[g->n++] = b; g->v[g->n++] = ea; g->v[g->n++] = eb;
}
static int seg_cb(void *ctx, int32_t a, int32_t b) { seg_add(ctx, a, b, a, b); return 0; }

int32_t *tx_enum(const UChar *u, int32_t n, int32_t a0, int32_t e0, int kind, const char *locale, int *count) {
    segs_t g = { 0, 0, 0 };
    if (kind == 0 || kind == 1) {
        for (int32_t i = a0; i < e0;) {
            int32_t s, e, ce;
            tx_line(u, n, i, 0, kind == 1, &s, &e, &ce);
            if (s < a0) s = a0;
            if (e > e0) e = e0;
            if (ce > e) ce = e;
            if (ce < s) ce = s;
            seg_add(&g, s, ce, s, e);
            if (e <= i) break;
            i = e;
        }
    } else if (kind == 2 || kind == 5 || kind == 6) {
        for (int32_t i = a0; i < e0;) {
            int32_t s, l;
            tx_composed(u, n, i, &s, &l);
            if (s < i) { l -= i - s; s = i; }
            if (s + l > e0) l = e0 - s;
            if (l <= 0) l = 1;
            seg_add(&g, s, s + l, s, s + l);
            i = s + l;
        }
    } else if (kind == 3 || kind == 4) {
        segs_t w = { 0, 0, 0 };
        tx_segments(u + a0, e0 - a0, kind == 3 ? 1 : 3, locale, seg_cb, &w);
        int cnt = w.n / 4;
        /* faixa envolvente: do fim da anterior até o começo da próxima (cobre espaços e pontuação) */
        for (int k = 0; k < cnt; k++) {
            int32_t s = a0 + w.v[k * 4], e = a0 + w.v[k * 4 + 1];
            int32_t es = k == 0 ? a0 : g.v[g.n - 1];
            int32_t ee = k + 1 < cnt ? a0 + w.v[(k + 1) * 4] : e0;
            seg_add(&g, s, e, es, ee);
        }
        free(w.v);
    }
    *count = g.n / 4;
    return g.v;
}

UChar *tx_replace(const UChar *u, int32_t n, int32_t loc, int32_t len, const UChar *t, int32_t tn,
                  const UChar *p, int32_t pn, unsigned opts, int32_t *on, int32_t *count) {
    *count = 0;
    UChar *o = 0;
    int32_t end = loc + len;
    if (tn <= 0) goto copy;
    if (opts & TX_REGEX) {
        const fdn_icu_t *I = fdn_icu();
        if (!I || !I->ok) goto copy;
        UErrorCode e = U_ZERO_ERROR;
        void *re = I->uregex_open(t, tn, (opts & TX_CASE) ? 2 : 0, 0, &e);
        if (!re || U_FAILURE(e)) { if (re) I->uregex_close(re); goto copy; }
        /* só a faixa: casa e troca nela e cola as pontas */
        I->uregex_setText(re, u + loc, len, &e);
        int32_t cnt = 0;
        while (I->uregex_findNext(re, &e)) cnt++;
        if (!cnt) { I->uregex_close(re); goto copy; }
        I->uregex_reset(re, 0, &e);
        e = U_ZERO_ERROR;
        int32_t need = I->uregex_replaceAll(re, p, pn, 0, 0, &e);
        UChar *mid = malloc((size_t)(need + 1) * sizeof(UChar));
        e = U_ZERO_ERROR;
        I->uregex_replaceAll(re, p, pn, mid, need + 1, &e);
        I->uregex_close(re);
        *on = loc + need + (n - end);
        o = malloc((size_t)(*on + 1) * sizeof(UChar));
        memcpy(o, u, (size_t)loc * 2);
        memcpy(o + loc, mid, (size_t)need * 2);
        memcpy(o + loc + need, u + end, (size_t)(n - end) * 2);
        free(mid);
        *count = cnt;
        return o;
    }
    {
        unsigned so = opts & ~(unsigned)(TX_BACKWARDS | TX_ANCHORED);
        segs_t h = { 0, 0, 0 };
        if (opts & TX_ANCHORED) {
            int32_t l, m;
            if (tx_search(u + loc, len, t, tn, so | TX_ANCHORED | (opts & TX_BACKWARDS), &l, &m)) seg_add(&h, loc + l, loc + l + m, 0, 0);
        } else {
            for (int32_t i = loc; i < end;) {
                int32_t l, m;
                if (!tx_search(u + i, end - i, t, tn, so, &l, &m) || m <= 0) break;
                seg_add(&h, i + l, i + l + m, 0, 0);
                i += l + m;
            }
        }
        int k = h.n / 4;
        if (!k) { free(h.v); goto copy; }
        int32_t tot = n;
        for (int j = 0; j < k; j++) tot += pn - (h.v[j * 4 + 1] - h.v[j * 4]);
        o = malloc((size_t)(tot + 1) * sizeof(UChar));
        int32_t pos = 0, w = 0;
        for (int j = 0; j < k; j++) {
            int32_t a = h.v[j * 4], b = h.v[j * 4 + 1];
            memcpy(o + w, u + pos, (size_t)(a - pos) * 2); w += a - pos;
            memcpy(o + w, p, (size_t)pn * 2); w += pn;
            pos = b;
        }
        memcpy(o + w, u + pos, (size_t)(n - pos) * 2); w += n - pos;
        free(h.v);
        *on = w; *count = k;
        return o;
    }
copy:
    o = malloc((size_t)(n + 1) * sizeof(UChar));
    memcpy(o, u, (size_t)n * 2);
    *on = n;
    return o;
}
