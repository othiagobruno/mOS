/*
 * mOS libSystem — trechos de atributos (ver fdn_astr_core.h).
 */
#include "fdn_astr_core.h"

void *realloc(void *p, size_t n);
void free(void *p);
void *memmove(void *d, const void *s, size_t n);

static void *keep(const runs_cb *cb, void *v) { return cb && cb->retain ? cb->retain(v) : v; }
static void drop(const runs_cb *cb, void *v) { if (cb && cb->release) cb->release(v); }
static int same(const runs_cb *cb, void *a, void *b) { return a == b || (cb && cb->equal && a && b && cb->equal(a, b)); }

void runs_free(runs_t *r, const runs_cb *cb) {
    for (size_t i = 0; i < r->n; i++) drop(cb, r->val[i]);
    free(r->len); free(r->val);
    r->len = 0; r->val = 0; r->n = r->cap = 0;
}
uint64_t runs_total(const runs_t *r) {
    uint64_t t = 0;
    for (size_t i = 0; i < r->n; i++) t += r->len[i];
    return t;
}
void *runs_at(const runs_t *r, uint64_t idx, uint64_t *start, uint64_t *len) {
    uint64_t a = 0;
    for (size_t i = 0; i < r->n; i++) {
        if (idx < a + r->len[i]) {
            if (start) *start = a;
            if (len) *len = r->len[i];
            return r->val[i];
        }
        a += r->len[i];
    }
    if (start) *start = a;
    if (len) *len = 0;
    return 0;
}
static void grow(runs_t *r, size_t want) {
    if (want <= r->cap) return;
    size_t c = r->cap ? r->cap * 2 : 8;
    while (c < want) c *= 2;
    r->len = realloc(r->len, c * sizeof *r->len);
    r->val = realloc(r->val, c * sizeof *r->val);
    r->cap = c;
}
static void ins(runs_t *r, size_t i, uint64_t len, void *val) {
    grow(r, r->n + 1);
    memmove(r->len + i + 1, r->len + i, (r->n - i) * sizeof *r->len);
    memmove(r->val + i + 1, r->val + i, (r->n - i) * sizeof *r->val);
    r->len[i] = len; r->val[i] = val;
    r->n++;
}
static void del(runs_t *r, const runs_cb *cb, size_t i) {
    drop(cb, r->val[i]);
    memmove(r->len + i, r->len + i + 1, (r->n - i - 1) * sizeof *r->len);
    memmove(r->val + i, r->val + i + 1, (r->n - i - 1) * sizeof *r->val);
    r->n--;
}
/* corta para haver fronteira em pos; devolve o índice do trecho que começa em pos (ou n) */
static size_t split(runs_t *r, const runs_cb *cb, uint64_t pos) {
    uint64_t a = 0;
    for (size_t i = 0; i < r->n; i++) {
        if (pos == a) return i;
        if (pos < a + r->len[i]) {
            uint64_t left = pos - a;
            ins(r, i + 1, r->len[i] - left, keep(cb, r->val[i]));
            r->len[i] = left;
            return i + 1;
        }
        a += r->len[i];
    }
    return r->n;
}
static void merge(runs_t *r, const runs_cb *cb) {
    size_t i = 0;
    while (i < r->n) {
        if (!r->len[i]) { del(r, cb, i); continue; }
        if (i > 0 && same(cb, r->val[i - 1], r->val[i])) { r->len[i - 1] += r->len[i]; del(r, cb, i); continue; }
        i++;
    }
}
void runs_set(runs_t *r, const runs_cb *cb, uint64_t loc, uint64_t len, void *val) {
    if (!len) return;
    size_t a = split(r, cb, loc), b = split(r, cb, loc + len);
    for (size_t i = b; i > a; i--) del(r, cb, i - 1);
    ins(r, a, len, keep(cb, val));
    merge(r, cb);
}
void runs_replace(runs_t *r, const runs_cb *cb, uint64_t loc, uint64_t len, uint64_t newlen, void *val) {
    size_t a = split(r, cb, loc), b = split(r, cb, loc + len);
    for (size_t i = b; i > a; i--) del(r, cb, i - 1);
    if (newlen) ins(r, a, newlen, keep(cb, val));
    merge(r, cb);
}
