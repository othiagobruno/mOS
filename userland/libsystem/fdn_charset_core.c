/*
 * mOS libSystem — faixas de escalares Unicode para o NSCharacterSet (ver fdn_charset_core.h).
 */
#include "fdn_charset_core.h"
#include "fdn_icu.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memmove(void *d, const void *s, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);

void crs_free(crs_t *s) { free(s->r); s->r = 0; s->n = s->cap = 0; }
void crs_clear(crs_t *s) { s->n = 0; }
static int reserve(crs_t *s, uint32_t n) {
    if (n <= s->cap) return 1;
    uint32_t c = s->cap ? s->cap : 8;
    while (c < n) c *= 2;
    cr_t *r = realloc(s->r, c * sizeof(cr_t));
    if (!r) return 0;
    s->r = r;
    s->cap = c;
    return 1;
}
int crs_copy(crs_t *d, const crs_t *s) {
    if (!reserve(d, s->n ? s->n : 1)) return 0;
    memcpy(d->r, s->r, s->n * sizeof(cr_t));
    d->n = s->n;
    return 1;
}
/* primeira faixa com hi + 1 >= lo (encosta em lo ou vem depois) */
static uint32_t lower(const crs_t *s, uint32_t lo) {
    uint32_t a = 0, b = s->n;
    while (a < b) { uint32_t m = (a + b) / 2; if ((uint64_t)s->r[m].hi + 1 < lo) a = m + 1; else b = m; }
    return a;
}
void crs_add(crs_t *s, uint32_t lo, uint32_t hi) {
    if (lo > CR_MAX || lo > hi) return;
    if (hi > CR_MAX) hi = CR_MAX;
    uint32_t i = lower(s, lo), j = i;
    while (j < s->n && (uint64_t)s->r[j].lo <= (uint64_t)hi + 1) {
        if (s->r[j].lo < lo) lo = s->r[j].lo;
        if (s->r[j].hi > hi) hi = s->r[j].hi;
        j++;
    }
    if (j == i) {
        if (!reserve(s, s->n + 1)) return;
        memmove(s->r + i + 1, s->r + i, (s->n - i) * sizeof(cr_t));
        s->n++;
    } else if (j > i + 1) {
        memmove(s->r + i + 1, s->r + j, (s->n - j) * sizeof(cr_t));
        s->n -= j - i - 1;
    }
    s->r[i].lo = lo;
    s->r[i].hi = hi;
}
void crs_remove(crs_t *s, uint32_t lo, uint32_t hi) {
    if (lo > hi || lo > CR_MAX) return;
    if (hi > CR_MAX) hi = CR_MAX;
    for (uint32_t i = lower(s, lo); i < s->n;) {
        cr_t r = s->r[i];
        if (r.lo > hi) break;
        if (r.hi < lo) { i++; continue; }
        int left = r.lo < lo, right = r.hi > hi;
        if (left && right) {
            if (!reserve(s, s->n + 1)) return;
            memmove(s->r + i + 2, s->r + i + 1, (s->n - i - 1) * sizeof(cr_t));
            s->n++;
            s->r[i].hi = lo - 1;
            s->r[i + 1].lo = hi + 1;
            s->r[i + 1].hi = r.hi;
            return;
        }
        if (left) { s->r[i].hi = lo - 1; i++; }
        else if (right) { s->r[i].lo = hi + 1; return; }
        else { memmove(s->r + i, s->r + i + 1, (s->n - i - 1) * sizeof(cr_t)); s->n--; }
    }
}
int crs_has(const crs_t *s, uint32_t c) {
    uint32_t a = 0, b = s->n;
    while (a < b) {
        uint32_t m = (a + b) / 2;
        if (s->r[m].hi < c) a = m + 1;
        else if (s->r[m].lo > c) b = m;
        else return 1;
    }
    return 0;
}
void crs_invert(crs_t *s) {
    crs_t t = {0};
    uint32_t next = 0;
    for (uint32_t i = 0; i < s->n; i++) {
        if (s->r[i].lo > next) crs_add(&t, next, s->r[i].lo - 1);
        next = s->r[i].hi + 1;
    }
    if (next <= CR_MAX) crs_add(&t, next, CR_MAX);
    free(s->r);
    *s = t;
}
void crs_union(crs_t *s, const crs_t *o) {
    if (s == o) return;
    for (uint32_t i = 0; i < o->n; i++) crs_add(s, o->r[i].lo, o->r[i].hi);
}
void crs_intersect(crs_t *s, const crs_t *o) {
    if (s == o) return;
    crs_t t = {0};
    uint32_t i = 0, j = 0;
    while (i < s->n && j < o->n) {
        uint32_t lo = s->r[i].lo > o->r[j].lo ? s->r[i].lo : o->r[j].lo;
        uint32_t hi = s->r[i].hi < o->r[j].hi ? s->r[i].hi : o->r[j].hi;
        if (lo <= hi) crs_add(&t, lo, hi);
        if (s->r[i].hi < o->r[j].hi) i++; else j++;
    }
    free(s->r);
    *s = t;
}
int crs_superset(const crs_t *s, const crs_t *o) {
    for (uint32_t j = 0; j < o->n; j++) {
        uint32_t k = lower(s, o->r[j].lo);
        while (k < s->n && s->r[k].hi < o->r[j].lo) k++;
        if (k >= s->n || s->r[k].lo > o->r[j].lo || s->r[k].hi < o->r[j].hi) return 0;
    }
    return 1;
}
int crs_equal(const crs_t *a, const crs_t *b) {
    if (a->n != b->n) return 0;
    for (uint32_t i = 0; i < a->n; i++) if (a->r[i].lo != b->r[i].lo || a->r[i].hi != b->r[i].hi) return 0;
    return 1;
}
int crs_has_plane(const crs_t *s, uint32_t plane) {
    if (plane > 16) return 0;
    uint32_t lo = plane << 16, hi = lo | 0xFFFF, k = lower(s, lo);
    while (k < s->n && s->r[k].hi < lo) k++;
    return k < s->n && s->r[k].lo <= hi;
}
static void fill_plane(const crs_t *s, uint32_t plane, uint8_t *b) {
    memset(b, 0, 8192);
    uint32_t base = plane << 16;
    for (uint32_t i = lower(s, base); i < s->n && s->r[i].lo <= base + 0xFFFF; i++) {
        uint32_t lo = s->r[i].lo < base ? base : s->r[i].lo, hi = s->r[i].hi > base + 0xFFFF ? base + 0xFFFF : s->r[i].hi;
        for (uint32_t c = lo; c <= hi; c++) b[(c - base) >> 3] |= (uint8_t)(1u << ((c - base) & 7));
    }
}
uint8_t *crs_bitmap(const crs_t *s, uint32_t *len) {
    uint32_t planes = 0;
    for (uint32_t p = 1; p <= 16; p++) if (crs_has_plane(s, p)) planes++;
    uint32_t n = 8192 + planes * 8193;
    uint8_t *b = malloc(n), *o = b + 8192;
    if (!b) { *len = 0; return 0; }
    fill_plane(s, 0, b);
    for (uint32_t p = 1; p <= 16; p++) if (crs_has_plane(s, p)) { *o++ = (uint8_t)p; fill_plane(s, p, o); o += 8192; }
    *len = n;
    return b;
}
static void read_plane(crs_t *s, uint32_t plane, const uint8_t *b) {
    uint32_t base = plane << 16;
    for (uint32_t c = 0; c < 65536;) {
        if (!(b[c >> 3] >> (c & 7) & 1)) { c++; continue; }
        uint32_t e = c;
        while (e + 1 < 65536 && (b[(e + 1) >> 3] >> ((e + 1) & 7) & 1)) e++;
        crs_add(s, base + c, base + e);
        c = e + 1;
    }
}
void crs_from_bitmap(crs_t *s, const uint8_t *b, uint32_t len) {
    crs_clear(s);
    if (!b || len < 8192) return;
    read_plane(s, 0, b);
    for (uint32_t o = 8192; o + 8193 <= len; o += 8193) if (b[o] >= 1 && b[o] <= 16) read_plane(s, b[o], b + o + 1);
}
int crs_from_icu_pattern(crs_t *s, const char *pattern) {
    const fdn_icu_t *I = fdn_icu();
    if (!I || !I->uset_openPattern) return 0;
    UChar pat[160];
    int32_t n = fdn_u16_from_utf8(pattern, -1, pat, 160);
    UErrorCode e = U_ZERO_ERROR;
    void *u = I->uset_openPattern(pat, n, &e);
    if (!u || U_FAILURE(e)) { if (u) I->uset_close(u); return 0; }
    int32_t k = I->uset_getItemCount(u);
    for (int32_t i = 0; i < k; i++) {
        UChar32 lo, hi;
        e = U_ZERO_ERROR;
        /* item de faixa devolve 0 (as cadeias {..} do conjunto não entram num NSCharacterSet) */
        if (I->uset_getItem(u, i, &lo, &hi, 0, 0, &e) == 0 && !U_FAILURE(e)) crs_add(s, (uint32_t)lo, (uint32_t)hi);
    }
    I->uset_close(u);
    return 1;
}
