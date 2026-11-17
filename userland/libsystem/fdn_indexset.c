/*
 * mOS libSystem — NSIndexSet e NSMutableIndexSet completos (antes: a classe vinha do AppKit com 2 métodos e a
 * mutável guardava índices soltos). Representação: lista ordenada de faixas disjuntas e não adjacentes.
 * Também as funções que o Foundation do Swift usa para o IndexSet: __NSIndexSetRangeCount & cia.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void cx_object_free(id o);
void cx_kvo_forget(id o);
const char *class_getName(id k);

#define NOT_FOUND 0x7FFFFFFFFFFFFFFFULL
typedef struct { u64 loc, len; } rng_t;
typedef struct { cx_class_t *isa; rng_t *r; u64 nr, cap, count; } ixs_t;
#define X(o) ((ixs_t *)(o))

cx_class_t ixs_c __asm__("OBJC_CLASS_$_NSIndexSet"), ixs_m __asm__("OBJC_METACLASS_$_NSIndexSet");
cx_class_t mixs_c __asm__("OBJC_CLASS_$_NSMutableIndexSet"), mixs_m __asm__("OBJC_METACLASS_$_NSMutableIndexSet");
static id cls(const char *n) { return (id)cx_class_named(n); }
static void *blk_inv(void *b) { return *(void **)((u8 *)b + 16); }

/* ---------------------------------------------------------------- faixas */
static u64 end_of(rng_t r) { return r.loc + r.len; }
static void reserve(ixs_t *s, u64 n) {
    if (n <= s->cap) return;
    u64 c = s->cap ? s->cap : 4;
    while (c < n) c *= 2;
    s->r = realloc(s->r, c * sizeof(rng_t));
    s->cap = c;
}
static void recount(ixs_t *s) { u64 n = 0; for (u64 i = 0; i < s->nr; i++) n += s->r[i].len; s->count = n; }
/* primeira faixa cujo fim é >= loc (a que contém loc, encosta nele, ou vem depois) */
static u64 lower(const ixs_t *s, u64 loc) {
    u64 lo = 0, hi = s->nr;
    while (lo < hi) { u64 m = (lo + hi) / 2; if (end_of(s->r[m]) < loc) lo = m + 1; else hi = m; }
    return lo;
}
static void add_range(ixs_t *s, u64 loc, u64 len) {
    if (!len) return;
    if (loc >= NOT_FOUND) return;
    if (len > NOT_FOUND - loc) len = NOT_FOUND - loc;
    u64 e = loc + len, i = lower(s, loc), j = i;
    /* funde todas as faixas que tocam [loc, e] */
    while (j < s->nr && s->r[j].loc <= e) {
        if (s->r[j].loc < loc) loc = s->r[j].loc;
        if (end_of(s->r[j]) > e) e = end_of(s->r[j]);
        j++;
    }
    if (j == i) {
        reserve(s, s->nr + 1);
        memmove(s->r + i + 1, s->r + i, (s->nr - i) * sizeof(rng_t));
        s->nr++;
    } else if (j > i + 1) {
        memmove(s->r + i + 1, s->r + j, (s->nr - j) * sizeof(rng_t));
        s->nr -= j - i - 1;
    }
    s->r[i].loc = loc;
    s->r[i].len = e - loc;
    recount(s);
}
static void remove_range(ixs_t *s, u64 loc, u64 len) {
    if (!len || !s->nr) return;
    if (len > NOT_FOUND - loc) len = NOT_FOUND - loc;
    u64 e = loc + len;
    for (u64 i = lower(s, loc); i < s->nr;) {
        rng_t r = s->r[i];
        if (r.loc >= e) break;
        if (end_of(r) <= loc) { i++; continue; }
        int keep_left = r.loc < loc, keep_right = end_of(r) > e;
        if (keep_left && keep_right) {
            reserve(s, s->nr + 1);
            memmove(s->r + i + 2, s->r + i + 1, (s->nr - i - 1) * sizeof(rng_t));
            s->nr++;
            s->r[i].len = loc - r.loc;
            s->r[i + 1].loc = e;
            s->r[i + 1].len = end_of(r) - e;
            break;
        } else if (keep_left) { s->r[i].len = loc - r.loc; i++; }
        else if (keep_right) { s->r[i].len = end_of(r) - e; s->r[i].loc = e; break; }
        else { memmove(s->r + i, s->r + i + 1, (s->nr - i - 1) * sizeof(rng_t)); s->nr--; }
    }
    recount(s);
}
static long range_of(const ixs_t *s, u64 idx) {
    u64 i = lower(s, idx + 1);
    if (i < s->nr && s->r[i].loc <= idx && idx < end_of(s->r[i])) return (long)i;
    /* lower(idx+1) pode cair na faixa seguinte quando idx é o último da anterior */
    if (i > 0 && s->r[i - 1].loc <= idx && idx < end_of(s->r[i - 1])) return (long)(i - 1);
    return -1;
}
static int has(const ixs_t *s, u64 idx) { return range_of(s, idx) >= 0; }
static u64 count_in(const ixs_t *s, u64 loc, u64 len) {
    u64 e = len > NOT_FOUND - loc ? NOT_FOUND : loc + len, n = 0;
    for (u64 i = lower(s, loc); i < s->nr && s->r[i].loc < e; i++) {
        u64 a = s->r[i].loc > loc ? s->r[i].loc : loc, b = end_of(s->r[i]) < e ? end_of(s->r[i]) : e;
        if (b > a) n += b - a;
    }
    return n;
}
static u64 greater(const ixs_t *s, u64 idx, int or_equal) {
    u64 want = or_equal ? idx : idx + 1;
    if (!or_equal && idx >= NOT_FOUND - 1) return NOT_FOUND;
    for (u64 i = lower(s, want); i < s->nr; i++) {
        if (end_of(s->r[i]) <= want) continue;
        return s->r[i].loc > want ? s->r[i].loc : want;
    }
    return NOT_FOUND;
}
static u64 less(const ixs_t *s, u64 idx, int or_equal) {
    if (!or_equal && idx == 0) return NOT_FOUND;
    u64 want = or_equal ? idx : idx - 1;
    for (u64 i = s->nr; i-- > 0;) {
        if (s->r[i].loc > want) continue;
        return end_of(s->r[i]) - 1 < want ? end_of(s->r[i]) - 1 : want;
    }
    return NOT_FOUND;
}
static void shift(ixs_t *s, u64 start, long delta) {
    if (!delta || !s->nr) return;
    if (delta > 0) {
        /* parte a faixa que contém start e empurra tudo de start em diante */
        long k = range_of(s, start);
        if (k >= 0 && s->r[k].loc < start) {
            rng_t r = s->r[k];
            reserve(s, s->nr + 1);
            memmove(s->r + k + 2, s->r + k + 1, (s->nr - (u64)k - 1) * sizeof(rng_t));
            s->nr++;
            s->r[k].len = start - r.loc;
            s->r[k + 1].loc = start;
            s->r[k + 1].len = end_of(r) - start;
        }
        for (u64 i = 0; i < s->nr; i++) if (s->r[i].loc >= start) s->r[i].loc += (u64)delta;
        /* o que passar do limite some */
        while (s->nr && s->r[s->nr - 1].loc >= NOT_FOUND) s->nr--;
        if (s->nr && end_of(s->r[s->nr - 1]) > NOT_FOUND) s->r[s->nr - 1].len = NOT_FOUND - s->r[s->nr - 1].loc;
    } else {
        /* para trás: o intervalo [start-|delta|, start) é engolido (como a Foundation) */
        u64 d = (u64)(-delta), from = start >= d ? start - d : 0;
        remove_range(s, from, start - from);
        ixs_t t = {0};
        for (u64 i = 0; i < s->nr; i++) {
            rng_t r = s->r[i];
            if (r.loc >= start) r.loc -= start - from;
            add_range(&t, r.loc, r.len);
        }
        free(s->r);
        *s = (ixs_t){s->isa, t.r, t.nr, t.cap, 0};
    }
    recount(s);
}

/* ---------------------------------------------------------------- criação */
static id ar(id o) { return objc_autorelease(o); }
static id mk(cx_class_t *k) { return cx_alloc_instance(k); }
static id i_alloc(id k, SEL c) { (void)c; return mk((cx_class_t *)k); }
static id i_init(id s, SEL c) { (void)c; return s; }
static id i_init_index(id s, SEL c, u64 i) { (void)c; add_range(X(s), i, 1); return s; }
static id i_init_range(id s, SEL c, rng_t r) { (void)c; add_range(X(s), r.loc, r.len); return s; }
static void copy_from(ixs_t *d, id o) {
    if (!o) return;
    if (cx_is_subclass(*(cx_class_t **)o, &ixs_c)) {
        ixs_t *s = X(o);
        reserve(d, s->nr);
        memcpy(d->r, s->r, s->nr * sizeof(rng_t));
        d->nr = s->nr;
        d->count = s->count;
        return;
    }
    /* outra implementação: pelo protocolo público */
    for (u64 i = MSG0(u64, o, "firstIndex"); i != NOT_FOUND; i = MSG(u64, o, "indexGreaterThanIndex:", i)) add_range(d, i, 1);
}
static id i_init_set(id s, SEL c, id o) { (void)c; copy_from(X(s), o); return s; }
static id c_empty(id k, SEL c) { (void)c; return ar(mk((cx_class_t *)k)); }
static id c_index(id k, SEL c, u64 i) { return ar(i_init_index(mk((cx_class_t *)k), c, i)); }
static id c_range(id k, SEL c, rng_t r) { return ar(i_init_range(mk((cx_class_t *)k), c, r)); }
static id c_set(id k, SEL c, id o) { return ar(i_init_set(mk((cx_class_t *)k), c, o)); }
static void i_dealloc(id s, SEL c) { (void)c; free(X(s)->r); cx_kvo_forget(s); cx_object_free(s); }

/* ---------------------------------------------------------------- consulta */
static u64 i_count(id s, SEL c) { (void)c; return X(s)->count; }
static u64 i_first(id s, SEL c) { (void)c; return X(s)->nr ? X(s)->r[0].loc : NOT_FOUND; }
static u64 i_last(id s, SEL c) { (void)c; return X(s)->nr ? end_of(X(s)->r[X(s)->nr - 1]) - 1 : NOT_FOUND; }
static u64 i_gt(id s, SEL c, u64 i) { (void)c; return greater(X(s), i, 0); }
static u64 i_ge(id s, SEL c, u64 i) { (void)c; return greater(X(s), i, 1); }
static u64 i_lt(id s, SEL c, u64 i) { (void)c; return less(X(s), i, 0); }
static u64 i_le(id s, SEL c, u64 i) { (void)c; return less(X(s), i, 1); }
static BOOL i_has(id s, SEL c, u64 i) { (void)c; return has(X(s), i); }
static BOOL i_has_range(id s, SEL c, rng_t r) { (void)c; return r.len && count_in(X(s), r.loc, r.len) == r.len; }
static BOOL i_intersects_range(id s, SEL c, rng_t r) { (void)c; return r.len && count_in(X(s), r.loc, r.len) > 0; }
static u64 i_count_in(id s, SEL c, rng_t r) { (void)c; return count_in(X(s), r.loc, r.len); }
static BOOL i_has_set(id s, SEL c, id o) {
    (void)c;
    if (!o) return YES;
    ixs_t t = {0};
    copy_from(&t, o);
    BOOL ok = YES;
    for (u64 i = 0; i < t.nr && ok; i++) if (count_in(X(s), t.r[i].loc, t.r[i].len) != t.r[i].len) ok = NO;
    free(t.r);
    return ok;
}
static BOOL i_equal_set(id s, SEL c, id o) {
    (void)c;
    if (o == s) return YES;
    if (!o) return NO;
    ixs_t t = {0};
    copy_from(&t, o);
    BOOL ok = t.nr == X(s)->nr && !memcmp(t.r, X(s)->r, t.nr * sizeof(rng_t));
    free(t.r);
    return ok;
}
static BOOL i_is_equal(id s, SEL c, id o) {
    if (o == s) return YES;
    if (!o || !MSG(BOOL, o, "isKindOfClass:", (id)&ixs_c)) return NO;
    return i_equal_set(s, c, o);
}
static u64 i_hash(id s, SEL c) { (void)c; return X(s)->count ^ (X(s)->nr ? X(s)->r[0].loc : 0); }
/* getIndexes:maxCount:inIndexRange: escreve até max índices e avança o NSRangePointer */
static u64 i_get(id s, SEL c, u64 *buf, u64 max, rng_t *rp) {
    (void)c;
    u64 loc = rp ? rp->loc : 0, e = rp ? (rp->len > NOT_FOUND - rp->loc ? NOT_FOUND : rp->loc + rp->len) : NOT_FOUND, n = 0, next = e;
    for (u64 i = greater(X(s), loc, 1); i != NOT_FOUND && i < e; i = greater(X(s), i, 0)) {
        if (n == max) { next = i; break; }
        if (buf) buf[n] = i;
        n++;
    }
    if (rp) { if (next > e) next = e; rp->len = e - next; rp->loc = next; }
    return n;
}

/* ---------------------------------------------------------------- blocos (opção NSEnumerationReverse = 2) */
static void each_index(ixs_t *s, u64 loc, u64 len, u64 opts, void *blk, int test, ixs_t *out, u64 *first) {
    u64 e = len > NOT_FOUND - loc ? NOT_FOUND : loc + len;
    BOOL stop = NO;
    ixs_t snap = {0};
    copy_from(&snap, (id)s);  /* o bloco pode mudar o conjunto */
    if (first) *first = NOT_FOUND;
    if (opts & 2) {
        for (u64 k = snap.nr; k-- > 0 && !stop;) {
            rng_t r = snap.r[k];
            for (u64 i = end_of(r); i-- > r.loc && !stop;) {
                if (i >= e) continue;
                if (i < loc) break;
                if (test) {
                    BOOL ok = ((BOOL (*)(void *, u64, BOOL *))blk_inv(blk))(blk, i, &stop);
                    if (ok) { if (first) { *first = i; stop = YES; } else add_range(out, i, 1); }
                } else ((void (*)(void *, u64, BOOL *))blk_inv(blk))(blk, i, &stop);
            }
        }
    } else {
        for (u64 k = 0; k < snap.nr && !stop; k++) {
            rng_t r = snap.r[k];
            for (u64 i = r.loc < loc ? loc : r.loc; i < end_of(r) && i < e && !stop; i++) {
                if (test) {
                    BOOL ok = ((BOOL (*)(void *, u64, BOOL *))blk_inv(blk))(blk, i, &stop);
                    if (ok) { if (first) { *first = i; stop = YES; } else add_range(out, i, 1); }
                } else ((void (*)(void *, u64, BOOL *))blk_inv(blk))(blk, i, &stop);
            }
        }
    }
    free(snap.r);
}
static void i_enum(id s, SEL c, void *b) { (void)c; each_index(X(s), 0, NOT_FOUND, 0, b, 0, 0, 0); }
static void i_enum_opts(id s, SEL c, u64 o, void *b) { (void)c; each_index(X(s), 0, NOT_FOUND, o, b, 0, 0, 0); }
static void i_enum_range(id s, SEL c, rng_t r, u64 o, void *b) { (void)c; each_index(X(s), r.loc, r.len, o, b, 0, 0, 0); }
static u64 i_pass(id s, SEL c, void *b) { (void)c; u64 f; each_index(X(s), 0, NOT_FOUND, 0, b, 1, 0, &f); return f; }
static u64 i_pass_opts(id s, SEL c, u64 o, void *b) { (void)c; u64 f; each_index(X(s), 0, NOT_FOUND, o, b, 1, 0, &f); return f; }
static u64 i_pass_range(id s, SEL c, rng_t r, u64 o, void *b) { (void)c; u64 f; each_index(X(s), r.loc, r.len, o, b, 1, 0, &f); return f; }
static id i_passing(id s, SEL c, void *b) { (void)c; id o = mk(&ixs_c); each_index(X(s), 0, NOT_FOUND, 0, b, 1, X(o), 0); return ar(o); }
static id i_passing_opts(id s, SEL c, u64 op, void *b) { (void)c; id o = mk(&ixs_c); each_index(X(s), 0, NOT_FOUND, op, b, 1, X(o), 0); return ar(o); }
static id i_passing_range(id s, SEL c, rng_t r, u64 op, void *b) { (void)c; id o = mk(&ixs_c); each_index(X(s), r.loc, r.len, op, b, 1, X(o), 0); return ar(o); }
static void each_range(ixs_t *s, u64 loc, u64 len, u64 opts, void *blk) {
    u64 e = len > NOT_FOUND - loc ? NOT_FOUND : loc + len;
    BOOL stop = NO;
    ixs_t snap = {0};
    copy_from(&snap, (id)s);
    void (*inv)(void *, rng_t, BOOL *) = blk_inv(blk);
    for (u64 k = 0; k < snap.nr && !stop; k++) {
        u64 kk = (opts & 2) ? snap.nr - 1 - k : k;
        rng_t r = snap.r[kk];
        u64 a = r.loc > loc ? r.loc : loc, b = end_of(r) < e ? end_of(r) : e;
        if (b <= a) continue;
        rng_t x = {a, b - a};
        inv(blk, x, &stop);
    }
    free(snap.r);
}
static void i_ranges(id s, SEL c, void *b) { (void)c; each_range(X(s), 0, NOT_FOUND, 0, b); }
static void i_ranges_opts(id s, SEL c, u64 o, void *b) { (void)c; each_range(X(s), 0, NOT_FOUND, o, b); }
static void i_ranges_in(id s, SEL c, rng_t r, u64 o, void *b) { (void)c; each_range(X(s), r.loc, r.len, o, b); }

/* ---------------------------------------------------------------- cópia, descrição, NSCoding */
static id copy_as(id s, cx_class_t *k) { id o = mk(k); copy_from(X(o), s); return o; }
static id i_copy(id s, SEL c) { (void)c; return *(cx_class_t **)s == &ixs_c ? objc_retain(s) : copy_as(s, &ixs_c); }
static id i_copy_zone(id s, SEL c, void *z) { (void)z; return i_copy(s, c); }
static id i_mcopy(id s, SEL c) { (void)c; return copy_as(s, &mixs_c); }
static id i_mcopy_zone(id s, SEL c, void *z) { (void)z; return i_mcopy(s, c); }
static id i_desc(id s, SEL c) {
    (void)c;
    id out = MSG0(id, MSG0(id, cls("NSMutableString"), "alloc"), "init");
    char b[96];
    ixs_t *x = X(s);
    snprintf(b, sizeof b, "<%s: %p>", class_getName((id)*(cx_class_t **)s), (void *)s);
    MSG(void, out, "appendString:", cx_str(b));
    if (!x->count) { MSG(void, out, "appendString:", cx_str("(no indexes)")); return ar(out); }
    snprintf(b, sizeof b, "[number of indexes: %llu (in %llu range%s), indexes: (", (unsigned long long)x->count,
             (unsigned long long)x->nr, x->nr == 1 ? "" : "s");
    MSG(void, out, "appendString:", cx_str(b));
    for (u64 i = 0; i < x->nr; i++) {
        if (x->r[i].len == 1) snprintf(b, sizeof b, "%s%llu", i ? " " : "", (unsigned long long)x->r[i].loc);
        else snprintf(b, sizeof b, "%s%llu-%llu", i ? " " : "", (unsigned long long)x->r[i].loc, (unsigned long long)(end_of(x->r[i]) - 1));
        MSG(void, out, "appendString:", cx_str(b));
    }
    MSG(void, out, "appendString:", cx_str(")]"));
    return ar(out);
}
/* chaves do NSIndexSet: NSRangeCount; uma faixa em NSLocation/NSLength; várias em NSRangeData (varints) */
static void i_encode(id s, SEL c, id coder) {
    (void)c;
    ixs_t *x = X(s);
    MSG(void, coder, "encodeInteger:forKey:", (long)x->nr, cx_str("NSRangeCount"));
    if (x->nr == 1) {
        MSG(void, coder, "encodeInteger:forKey:", (long)x->r[0].loc, cx_str("NSLocation"));
        MSG(void, coder, "encodeInteger:forKey:", (long)x->r[0].len, cx_str("NSLength"));
    } else if (x->nr > 1) {
        u8 *b = malloc(x->nr * 20 + 1);
        u64 n = 0;
        for (u64 i = 0; i < x->nr; i++) {
            u64 v[2] = {x->r[i].loc, x->r[i].len};
            for (int k = 0; k < 2; k++) { u64 t = v[k]; do { u8 by = t & 0x7F; t >>= 7; b[n++] = by | (t ? 0x80 : 0); } while (t); }
        }
        id data = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)b, n);
        MSG(void, coder, "encodeObject:forKey:", data, cx_str("NSRangeData"));
        free(b);
    }
}
static id i_decode(id s, SEL c, id coder) {
    (void)c;
    long nr = MSG(long, coder, "decodeIntegerForKey:", cx_str("NSRangeCount"));
    if (nr == 1) add_range(X(s), (u64)MSG(long, coder, "decodeIntegerForKey:", cx_str("NSLocation")),
                           (u64)MSG(long, coder, "decodeIntegerForKey:", cx_str("NSLength")));
    else if (nr > 1) {
        id d = MSG(id, coder, "decodeObjectForKey:", cx_str("NSRangeData"));
        const u8 *b = d ? MSG0(const u8 *, d, "bytes") : 0;
        u64 n = d ? MSG0(u64, d, "length") : 0, p = 0;
        while (b && p < n) {
            u64 v[2] = {0, 0};
            for (int k = 0; k < 2 && p < n; k++) { int sh = 0; u8 by; do { by = b[p++]; v[k] |= (u64)(by & 0x7F) << sh; sh += 7; } while ((by & 0x80) && p < n && sh < 64); }
            add_range(X(s), v[0], v[1]);
        }
    }
    return s;
}
static BOOL yes(id s, SEL c) { (void)s; (void)c; return YES; }

/* ---------------------------------------------------------------- mutável */
static void m_add(id s, SEL c, u64 i) { (void)c; add_range(X(s), i, 1); }
static void m_add_range(id s, SEL c, rng_t r) { (void)c; add_range(X(s), r.loc, r.len); }
static void m_add_set(id s, SEL c, id o) {
    (void)c;
    ixs_t t = {0};
    copy_from(&t, o);
    for (u64 i = 0; i < t.nr; i++) add_range(X(s), t.r[i].loc, t.r[i].len);
    free(t.r);
}
static void m_remove(id s, SEL c, u64 i) { (void)c; remove_range(X(s), i, 1); }
static void m_remove_range(id s, SEL c, rng_t r) { (void)c; remove_range(X(s), r.loc, r.len); }
static void m_remove_set(id s, SEL c, id o) {
    (void)c;
    ixs_t t = {0};
    copy_from(&t, o);
    for (u64 i = 0; i < t.nr; i++) remove_range(X(s), t.r[i].loc, t.r[i].len);
    free(t.r);
}
static void m_remove_all(id s, SEL c) { (void)c; X(s)->nr = 0; X(s)->count = 0; }
static void m_shift(id s, SEL c, u64 start, long delta) { (void)c; shift(X(s), start, delta); }

/* ---------------------------------------------------------------- para o IndexSet do Swift */
u64 __NSIndexSetRangeCount(id s) { return s ? X(s)->nr : 0; }
void __NSIndexSetRangeAtIndex(id s, u64 i, u64 *loc, u64 *len) {
    if (s && i < X(s)->nr) { if (loc) *loc = X(s)->r[i].loc; if (len) *len = X(s)->r[i].len; }
    else { if (loc) *loc = NOT_FOUND; if (len) *len = 0; }
}
u64 __NSIndexSetIndexOfRangeContainingIndex(id s, u64 idx) { long k = s ? range_of(X(s), idx) : -1; return k < 0 ? NOT_FOUND : (u64)k; }

void cx_fdn_indexset_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("NSIndexSet") == &ixs_c) return;
    cx_define_class(&ixs_c, &ixs_m, "NSIndexSet", ob, (u32)(sizeof(ixs_t) - 8));
    cx_define_class(&mixs_c, &mixs_m, "NSMutableIndexSet", &ixs_c, 0);
    cx_class_t *k = &ixs_c, *m = &mixs_c;
#define M(c, s, f) cx_add_method(c, s, (void *)(f))
#define CM(c, s, f) cx_add_cmethod(c, s, (void *)(f))
    CM(k, "alloc", i_alloc); CM(k, "allocWithZone:", i_alloc);
    CM(k, "indexSet", c_empty); CM(k, "indexSetWithIndex:", c_index); CM(k, "indexSetWithIndexesInRange:", c_range);
    CM(k, "supportsSecureCoding", yes);
    M(k, "init", i_init); M(k, "initWithIndex:", i_init_index); M(k, "initWithIndexesInRange:", i_init_range);
    M(k, "initWithIndexSet:", i_init_set); M(k, "dealloc", i_dealloc);
    M(k, "count", i_count); M(k, "firstIndex", i_first); M(k, "lastIndex", i_last);
    M(k, "indexGreaterThanIndex:", i_gt); M(k, "indexGreaterThanOrEqualToIndex:", i_ge);
    M(k, "indexLessThanIndex:", i_lt); M(k, "indexLessThanOrEqualToIndex:", i_le);
    M(k, "containsIndex:", i_has); M(k, "containsIndexesInRange:", i_has_range); M(k, "containsIndexes:", i_has_set);
    M(k, "intersectsIndexesInRange:", i_intersects_range); M(k, "countOfIndexesInRange:", i_count_in);
    M(k, "isEqualToIndexSet:", i_equal_set); M(k, "isEqual:", i_is_equal); M(k, "hash", i_hash);
    M(k, "getIndexes:maxCount:inIndexRange:", i_get);
    M(k, "enumerateIndexesUsingBlock:", i_enum); M(k, "enumerateIndexesWithOptions:usingBlock:", i_enum_opts);
    M(k, "enumerateIndexesInRange:options:usingBlock:", i_enum_range);
    M(k, "indexPassingTest:", i_pass); M(k, "indexWithOptions:passingTest:", i_pass_opts); M(k, "indexInRange:options:passingTest:", i_pass_range);
    M(k, "indexesPassingTest:", i_passing); M(k, "indexesWithOptions:passingTest:", i_passing_opts);
    M(k, "indexesInRange:options:passingTest:", i_passing_range);
    M(k, "enumerateRangesUsingBlock:", i_ranges); M(k, "enumerateRangesWithOptions:usingBlock:", i_ranges_opts);
    M(k, "enumerateRangesInRange:options:usingBlock:", i_ranges_in);
    M(k, "copy", i_copy); M(k, "copyWithZone:", i_copy_zone); M(k, "mutableCopy", i_mcopy); M(k, "mutableCopyWithZone:", i_mcopy_zone);
    M(k, "description", i_desc); M(k, "encodeWithCoder:", i_encode); M(k, "initWithCoder:", i_decode);
    M(m, "addIndex:", m_add); M(m, "addIndexesInRange:", m_add_range); M(m, "addIndexes:", m_add_set);
    M(m, "removeIndex:", m_remove); M(m, "removeIndexesInRange:", m_remove_range); M(m, "removeIndexes:", m_remove_set);
    M(m, "removeAllIndexes", m_remove_all); M(m, "shiftIndexesStartingAtIndex:by:", m_shift);
#undef M
#undef CM
}
