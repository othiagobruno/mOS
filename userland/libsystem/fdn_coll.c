/*
 * mOS libSystem — os métodos públicos de NSArray, NSMutableArray, NSDictionary, NSMutableDictionary e NSDate que
 * faltavam (auditoria dos seletores contra o libSystem). Tudo pelos primitivos (count, objectAtIndex:,
 * objectForKey:...), então vale também para subclasses de outras implementações (as pontes do Swift, apps).
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id cx_enumerator_new(id array);

#define NOT_FOUND 0x7FFFFFFFFFFFFFFFULL
typedef struct { u64 loc, len; } rng_t;
static id cls(const char *n) { return (id)cx_class_named(n); }
static void *inv_of(void *b) { return *(void **)((u8 *)b + 16); }
static id ar(id o) { return objc_autorelease(o); }
static u64 cnt(id a) { return a ? MSG0(u64, a, "count") : 0; }
static id at(id a, u64 i) { return MSG(id, a, "objectAtIndex:", i); }
static BOOL eq(id a, id b) { return a == b || (a && b && MSG(BOOL, a, "isEqual:", b)); }
static id marr(void) { return MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init"); }
static void add(id a, id o) { MSG(void, a, "addObject:", o); }
static id mstr(void) { return MSG0(id, MSG0(id, cls("NSMutableString"), "alloc"), "init"); }
static void app(id s, const char *t) { MSG(void, s, "appendString:", cx_str(t)); }
static void appo(id s, id t) { if (t) MSG(void, s, "appendString:", t); }
static id iset_new(void) { return MSG0(id, MSG0(id, cls("NSMutableIndexSet"), "alloc"), "init"); }
/* índices de um NSIndexSet, um por vez */
static u64 ix_first(id s) { return s ? MSG0(u64, s, "firstIndex") : NOT_FOUND; }
static u64 ix_next(id s, u64 i) { return MSG(u64, s, "indexGreaterThanIndex:", i); }

/* ---------------------------------------------------------------- ordenação estável (merge sort) */
typedef long (*cmp_fn)(id a, id b, void *ctx);
static void msort(id *v, id *tmp, u64 n, cmp_fn f, void *ctx) {
    if (n < 2) return;
    u64 h = n / 2;
    msort(v, tmp, h, f, ctx);
    msort(v + h, tmp, n - h, f, ctx);
    u64 i = 0, j = h, k = 0;
    while (i < h && j < n) tmp[k++] = f(v[j], v[i], ctx) < 0 ? v[j++] : v[i++];
    while (i < h) tmp[k++] = v[i++];
    while (j < n) tmp[k++] = v[j++];
    memcpy(v, tmp, n * sizeof(id));
}
static long by_block(id a, id b, void *blk) { return ((long (*)(void *, id, id))inv_of(blk))(blk, a, b); }
typedef struct { long (*fn)(id, id, void *); void *ctx; } fctx_t;
static long by_func(id a, id b, void *c) { fctx_t *f = c; return f->fn(a, b, f->ctx); }
static long by_sel(id a, id b, void *sel) { return ((long (*)(id, SEL, id))objc_msgSend)(a, (SEL)sel, b); }
static id *items_of(id a, u64 *n) {
    *n = cnt(a);
    id *v = malloc((*n ? *n : 1) * sizeof(id));
    for (u64 i = 0; i < *n; i++) v[i] = at(a, i);
    return v;
}
static id sorted(id a, cmp_fn f, void *ctx) {
    u64 n;
    id *v = items_of(a, &n), *t = malloc((n ? n : 1) * sizeof(id));
    msort(v, t, n, f, ctx);
    id r = MSG(id, cls("NSArray"), "arrayWithObjects:count:", v, n);
    free(v); free(t);
    return r;
}
static void sort_in_place(id m, cmp_fn f, void *ctx) {
    u64 n;
    id *v = items_of(m, &n), *t = malloc((n ? n : 1) * sizeof(id));
    for (u64 i = 0; i < n; i++) objc_retain(v[i]);
    msort(v, t, n, f, ctx);
    for (u64 i = 0; i < n; i++) { MSG(void, m, "replaceObjectAtIndex:withObject:", i, v[i]); objc_release(v[i]); }
    free(v); free(t);
}

/* ---------------------------------------------------------------- NSArray */
static id a_sub(id s, SEL c, rng_t r) {
    (void)c;
    u64 n = cnt(s);
    if (r.loc > n || r.len > n - r.loc) {
        id e = MSG(id, cls("NSException"), "exceptionWithName:reason:userInfo:", cx_str("NSRangeException"),
                   cx_str("-[NSArray subarrayWithRange:]: range out of bounds"), (id)0);
        MSG0(void, e, "raise");
        return 0;
    }
    id *v = malloc((r.len ? r.len : 1) * sizeof(id));
    for (u64 i = 0; i < r.len; i++) v[i] = at(s, r.loc + i);
    id a = MSG(id, cls("NSArray"), "arrayWithObjects:count:", v, r.len);
    free(v);
    return a;
}
static void a_get_range(id s, SEL c, id *out, rng_t r) { (void)c; for (u64 i = 0; out && i < r.len; i++) out[i] = at(s, r.loc + i); }
static void a_get(id s, SEL c, id *out) { (void)c; u64 n = cnt(s); for (u64 i = 0; out && i < n; i++) out[i] = at(s, i); }
static u64 a_index_in(id s, SEL c, id o, rng_t r) {
    (void)c;
    u64 n = cnt(s);
    for (u64 i = r.loc; i < n && i < r.loc + r.len; i++) if (eq(at(s, i), o)) return i;
    return NOT_FOUND;
}
static id a_common(id s, SEL c, id o) {
    (void)c;
    u64 n = cnt(s);
    for (u64 i = 0; i < n; i++) { id x = at(s, i); if (o && MSG(BOOL, o, "containsObject:", x)) return x; }
    return 0;
}
static id a_reverse_enum(id s, SEL c) {
    (void)c;
    u64 n = cnt(s);
    id m = marr();
    for (u64 i = n; i-- > 0;) add(m, at(s, i));
    id e = cx_enumerator_new(m);
    objc_release(m);
    return e;
}
static id a_obj_enum(id s, SEL c) { (void)c; return cx_enumerator_new(s); }
static id a_hint(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSData"), "data"); }
static id a_sorted_func(id s, SEL c, long (*fn)(id, id, void *), void *ctx) { (void)c; fctx_t f = {fn, ctx}; return sorted(s, by_func, &f); }
static id a_sorted_func_hint(id s, SEL c, long (*fn)(id, id, void *), void *ctx, id hint) { (void)hint; return a_sorted_func(s, c, fn, ctx); }
static id a_sorted_opts(id s, SEL c, u64 opts, void *blk) { (void)c; (void)opts; return sorted(s, by_block, blk); }
static id a_sorted_cmp(id s, SEL c, void *blk) { (void)c; return sorted(s, by_block, blk); }
static void a_perform(id s, SEL c, SEL sel) {
    (void)c;
    u64 n;
    id *v = items_of(s, &n);
    for (u64 i = 0; i < n; i++) ((void (*)(id, SEL))objc_msgSend)(v[i], sel);
    free(v);
}
static void a_perform_with(id s, SEL c, SEL sel, id arg) {
    (void)c;
    u64 n;
    id *v = items_of(s, &n);
    for (u64 i = 0; i < n; i++) ((void (*)(id, SEL, id))objc_msgSend)(v[i], sel, arg);
    free(v);
}
static id a_at_indexes(id s, SEL c, id ixs) {
    (void)c;
    id m = marr();
    for (u64 i = ix_first(ixs); i != NOT_FOUND; i = ix_next(ixs, i)) add(m, at(s, i));
    id r = MSG0(id, m, "copy");
    objc_release(m);
    return ar(r);
}
/* enumeração com opções (NSEnumerationReverse = 2) e por índices */
static void each(id s, id ixs, u64 opts, void *blk, int test, id out, u64 *first) {
    u64 n;
    id *v = items_of(s, &n);
    u64 *idx = malloc((n ? n : 1) * sizeof(u64)), k = 0;
    if (ixs) { for (u64 i = ix_first(ixs); i != NOT_FOUND && i < n; i = ix_next(ixs, i)) idx[k++] = i; }
    else for (u64 i = 0; i < n; i++) idx[k++] = i;
    BOOL stop = NO;
    if (first) *first = NOT_FOUND;
    for (u64 j = 0; j < k && !stop; j++) {
        u64 i = (opts & 2) ? idx[k - 1 - j] : idx[j];
        if (test) {
            BOOL ok = ((BOOL (*)(void *, id, u64, BOOL *))inv_of(blk))(blk, v[i], i, &stop);
            if (ok) { if (first) { *first = i; break; } MSG(void, out, "addIndex:", i); }
        } else ((void (*)(void *, id, u64, BOOL *))inv_of(blk))(blk, v[i], i, &stop);
    }
    free(v); free(idx);
}
static void a_enum_opts(id s, SEL c, u64 o, void *b) { (void)c; each(s, 0, o, b, 0, 0, 0); }
static void a_enum_at(id s, SEL c, id ixs, u64 o, void *b) { (void)c; each(s, ixs, o, b, 0, 0, 0); }
static u64 a_index_pass(id s, SEL c, void *b) { (void)c; u64 f; each(s, 0, 0, b, 1, 0, &f); return f; }
static u64 a_index_pass_opts(id s, SEL c, u64 o, void *b) { (void)c; u64 f; each(s, 0, o, b, 1, 0, &f); return f; }
static u64 a_index_pass_at(id s, SEL c, id ixs, u64 o, void *b) { (void)c; u64 f; each(s, ixs, o, b, 1, 0, &f); return f; }
static id a_indexes_pass(id s, SEL c, void *b) { (void)c; id r = iset_new(); each(s, 0, 0, b, 1, r, 0); return ar(r); }
static id a_indexes_pass_opts(id s, SEL c, u64 o, void *b) { (void)c; id r = iset_new(); each(s, 0, o, b, 1, r, 0); return ar(r); }
static id a_indexes_pass_at(id s, SEL c, id ixs, u64 o, void *b) { (void)c; id r = iset_new(); each(s, ixs, o, b, 1, r, 0); return ar(r); }
static id a_init_copy_items(id s, SEL c, id o, BOOL deep) {
    (void)c;
    u64 n = cnt(o);
    for (u64 i = 0; i < n; i++) {
        id x = at(o, i);
        if (deep) { x = MSG(id, x, "copyWithZone:", (void *)0); add(s, x); objc_release(x); }
        else add(s, x);
    }
    return s;
}
static id a_filtered(id s, SEL c, id pred) {
    (void)c;
    u64 n;
    id *v = items_of(s, &n);
    id m = marr();
    for (u64 i = 0; i < n; i++) if (pred && MSG(BOOL, pred, "evaluateWithObject:", v[i])) add(m, v[i]);
    free(v);
    id r = MSG0(id, m, "copy");
    objc_release(m);
    return ar(r);
}
static id a_paths_ext(id s, SEL c, id exts) {
    (void)c;
    u64 n = cnt(s);
    id m = marr();
    for (u64 i = 0; i < n; i++) {
        id p = at(s, i);
        if (!cx_is_str(p)) continue;
        id e = MSG0(id, p, "pathExtension");
        if (e && MSG(BOOL, exts, "containsObject:", e)) add(m, p);
    }
    id r = MSG0(id, m, "copy");
    objc_release(m);
    return ar(r);
}
static id path_of_url(id url) { return url ? MSG0(id, url, "path") : 0; }
static BOOL write_url(id s, id url, id *err) {
    id p = path_of_url(url);
    BOOL ok = p && MSG(BOOL, s, "writeToFile:atomically:", p, (BOOL)YES);
    if (!ok && err) *err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (long)512, (id)0);
    return ok;
}
static BOOL a_write_url_err(id s, SEL c, id url, id *err) { (void)c; return write_url(s, url, err); }
static BOOL a_write_url(id s, SEL c, id url, BOOL atom) { (void)c; (void)atom; return write_url(s, url, 0); }
static id read_url(id k, id url, const char *sel_file, id *err) {
    id p = path_of_url(url);
    id r = p ? ((id (*)(id, SEL, id))objc_msgSend)(k, sel_registerName(sel_file), p) : 0;
    if (!r && err) *err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (long)260, (id)0);
    return r;
}
static id a_with_url_err(id k, SEL c, id url, id *err) { (void)c; return read_url(k, url, "arrayWithContentsOfFile:", err); }
static id a_with_url(id k, SEL c, id url) { (void)c; return read_url(k, url, "arrayWithContentsOfFile:", 0); }
static id a_init_url(id s, SEL c, id url) {
    (void)c;
    id r = read_url(cls("NSArray"), url, "arrayWithContentsOfFile:", 0);
    if (!r) { objc_release(s); return 0; }
    return a_init_copy_items(s, c, r, NO);
}
static id a_init_url_err(id s, SEL c, id url, id *err) {
    id r = read_url(cls("NSArray"), url, "arrayWithContentsOfFile:", err);
    if (!r) { objc_release(s); return 0; }
    return a_init_copy_items(s, c, r, NO);
}
static void a_observe_at(id s, SEL c, id obs, id ixs, id kp, u64 opts, void *ctx) {
    (void)c;
    for (u64 i = ix_first(ixs); i != NOT_FOUND; i = ix_next(ixs, i))
        MSG(void, at(s, i), "addObserver:forKeyPath:options:context:", obs, kp, opts, ctx);
}
static void a_unobserve_at(id s, SEL c, id obs, id ixs, id kp) {
    (void)c;
    for (u64 i = ix_first(ixs); i != NOT_FOUND; i = ix_next(ixs, i)) MSG(void, at(s, i), "removeObserver:forKeyPath:", obs, kp);
}
/* descrição com recuo (o formato do -description do NSArray/NSDictionary aninhados) */
static void indent(id out, u64 lvl) { for (u64 i = 0; i < lvl; i++) app(out, "    "); }
static id desc_of(id o, id loc, u64 lvl) {
    if (o && (MSG(BOOL, o, "isKindOfClass:", cls("NSArray")) || MSG(BOOL, o, "isKindOfClass:", cls("NSDictionary"))))
        return MSG(id, o, "descriptionWithLocale:indent:", loc, lvl);
    if (o && MSG(BOOL, o, "respondsToSelector:", sel_registerName("descriptionWithLocale:")))
        return MSG(id, o, "descriptionWithLocale:", loc);
    return o ? MSG0(id, o, "description") : cx_str("(null)");
}
/* strings com espaço ou pontuação saem entre aspas, como no formato de property list antigo */
static id quoted(id d, id o) {
    if (!o || !cx_is_str(o)) return d;
    const char *t = cx_cstr(o);
    int need = !*t;
    for (const char *p = t; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || ch == '$' || ch == '/' || ch >= 0x80)) { need = 1; break; }
    }
    if (!need) return d;
    id q = mstr();
    app(q, "\"");
    for (const char *p = t; *p; p++) {
        char b[3] = {*p, 0, 0};
        if (*p == '"' || *p == '\\') { b[0] = '\\'; b[1] = *p; }
        else if (*p == '\n') { b[0] = '\\'; b[1] = 'n'; }
        app(q, b);
    }
    app(q, "\"");
    return ar(q);
}
static id a_desc_indent(id s, SEL c, id loc, u64 lvl) {
    (void)c;
    u64 n = cnt(s);
    id out = mstr();
    indent(out, 0);
    app(out, "(\n");
    for (u64 i = 0; i < n; i++) {
        id x = at(s, i);
        indent(out, lvl + 1);
        appo(out, quoted(desc_of(x, loc, lvl + 1), x));
        app(out, i + 1 < n ? ",\n" : "\n");
    }
    indent(out, lvl);
    app(out, ")");
    return ar(out);
}
static id a_desc_loc(id s, SEL c, id loc) { return a_desc_indent(s, c, loc, 0); }

/* ---------------------------------------------------------------- NSMutableArray */
static void m_exchange(id s, SEL c, u64 i, u64 j) {
    (void)c;
    if (i == j) return;
    id x = objc_retain(at(s, i)), y = objc_retain(at(s, j));
    MSG(void, s, "replaceObjectAtIndex:withObject:", i, y);
    MSG(void, s, "replaceObjectAtIndex:withObject:", j, x);
    objc_release(x); objc_release(y);
}
static void m_remove_at_indexes(id s, SEL c, id ixs) {
    (void)c;
    u64 n = cnt(ixs), k = 0;
    u64 *v = malloc((n ? n : 1) * sizeof(u64));
    for (u64 i = ix_first(ixs); i != NOT_FOUND; i = ix_next(ixs, i)) v[k++] = i;
    while (k-- > 0) MSG(void, s, "removeObjectAtIndex:", v[k]);
    free(v);
}
static int cmp_u64(const void *a, const void *b) { u64 x = *(const u64 *)a, y = *(const u64 *)b; return x < y ? -1 : x > y; }
static void m_remove_indices(id s, SEL c, const u64 *idx, u64 n) {
    (void)c;
    u64 *v = malloc((n ? n : 1) * sizeof(u64));
    memcpy(v, idx, n * sizeof(u64));
    qsort(v, n, sizeof(u64), cmp_u64);
    for (u64 k = n; k-- > 0;) if (k + 1 == n || v[k] != v[k + 1]) MSG(void, s, "removeObjectAtIndex:", v[k]);
    free(v);
}
static void m_remove_in_array(id s, SEL c, id o) {
    (void)c;
    for (u64 i = cnt(s); i-- > 0;) if (o && MSG(BOOL, o, "containsObject:", at(s, i))) MSG(void, s, "removeObjectAtIndex:", i);
}
static void m_replace_range_from(id s, SEL c, rng_t r, id o, rng_t r2) {
    (void)c;
    id keep = o ? objc_retain(o) : 0;
    for (u64 k = 0; k < r.len; k++) MSG(void, s, "removeObjectAtIndex:", r.loc);
    for (u64 k = 0; k < r2.len; k++) MSG(void, s, "insertObject:atIndex:", at(o, r2.loc + k), r.loc + k);
    if (keep) objc_release(keep);
}
static void m_replace_range(id s, SEL c, rng_t r, id o) { rng_t all = {0, cnt(o)}; m_replace_range_from(s, c, r, o, all); }
static void m_insert_at_indexes(id s, SEL c, id objs, id ixs) {
    (void)c;
    u64 k = 0;
    for (u64 i = ix_first(ixs); i != NOT_FOUND; i = ix_next(ixs, i)) MSG(void, s, "insertObject:atIndex:", at(objs, k++), i);
}
static void m_replace_at_indexes(id s, SEL c, id ixs, id objs) {
    (void)c;
    u64 k = 0;
    for (u64 i = ix_first(ixs); i != NOT_FOUND; i = ix_next(ixs, i)) MSG(void, s, "replaceObjectAtIndex:withObject:", i, at(objs, k++));
}
static void m_sort_func(id s, SEL c, long (*fn)(id, id, void *), void *ctx) { (void)c; fctx_t f = {fn, ctx}; sort_in_place(s, by_func, &f); }
static void m_sort_cmp(id s, SEL c, void *blk) { (void)c; sort_in_place(s, by_block, blk); }
static void m_sort_opts(id s, SEL c, u64 o, void *blk) { (void)c; (void)o; sort_in_place(s, by_block, blk); }
static void m_filter(id s, SEL c, id pred) {
    (void)c;
    for (u64 i = cnt(s); i-- > 0;) if (!pred || !MSG(BOOL, pred, "evaluateWithObject:", at(s, i))) MSG(void, s, "removeObjectAtIndex:", i);
}

/* ---------------------------------------------------------------- NSDictionary */
static id d_keys(id d) { return MSG0(id, d, "allKeys"); }
static id d_get(id d, id k) { return MSG(id, d, "objectForKey:", k); }
static id d_keys_for(id s, SEL c, id o) {
    (void)c;
    id ks = d_keys(s), m = marr();
    u64 n = cnt(ks);
    for (u64 i = 0; i < n; i++) { id k = at(ks, i); if (eq(d_get(s, k), o)) add(m, k); }
    id r = MSG0(id, m, "copy");
    objc_release(m);
    return ar(r);
}
static id d_objs_for(id s, SEL c, id keys, id marker) {
    (void)c;
    u64 n = cnt(keys);
    id m = marr();
    for (u64 i = 0; i < n; i++) { id v = d_get(s, at(keys, i)); add(m, v ? v : marker); }
    id r = MSG0(id, m, "copy");
    objc_release(m);
    return ar(r);
}
static void d_get_objs_keys(id s, SEL c, id *objs, id *keys, u64 max) {
    (void)c;
    id ks = d_keys(s);
    u64 n = cnt(ks);
    for (u64 i = 0; i < n && i < max; i++) { id k = at(ks, i); if (keys) keys[i] = k; if (objs) objs[i] = d_get(s, k); }
}
static void d_get_objs_keys_all(id s, SEL c, id *objs, id *keys) { d_get_objs_keys(s, c, objs, keys, ~0ULL); }
static void d_enum_opts(id s, SEL c, u64 opts, void *blk) {
    (void)c;
    id ks = d_keys(s);
    u64 n = cnt(ks);
    BOOL stop = NO;
    for (u64 j = 0; j < n && !stop; j++) {
        id k = at(ks, (opts & 2) ? n - 1 - j : j);
        ((void (*)(void *, id, id, BOOL *))inv_of(blk))(blk, k, d_get(s, k), &stop);
    }
}
typedef struct { id dict; cmp_fn f; void *ctx; } vctx_t;
static long by_value(id a, id b, void *c) { vctx_t *v = c; return v->f(d_get(v->dict, a), d_get(v->dict, b), v->ctx); }
static id d_sorted_by_value(id s, cmp_fn f, void *ctx) { vctx_t v = {s, f, ctx}; return sorted(d_keys(s), by_value, &v); }
static id d_keys_sorted_sel(id s, SEL c, SEL sel) { (void)c; return d_sorted_by_value(s, by_sel, (void *)sel); }
static id d_keys_sorted_cmp(id s, SEL c, void *blk) { (void)c; return d_sorted_by_value(s, by_block, blk); }
static id d_keys_sorted_opts(id s, SEL c, u64 o, void *blk) { (void)c; (void)o; return d_sorted_by_value(s, by_block, blk); }
static id d_keys_passing(id s, SEL c, void *blk) {
    (void)c;
    id ks = d_keys(s), set = MSG0(id, MSG0(id, cls("NSMutableSet"), "alloc"), "init");
    u64 n = cnt(ks);
    BOOL stop = NO;
    for (u64 i = 0; i < n && !stop; i++) {
        id k = at(ks, i);
        if (((BOOL (*)(void *, id, id, BOOL *))inv_of(blk))(blk, k, d_get(s, k), &stop)) MSG(void, set, "addObject:", k);
    }
    id r = MSG0(id, set, "copy");
    objc_release(set);
    return ar(r);
}
static id d_keys_passing_opts(id s, SEL c, u64 o, void *blk) { (void)o; return d_keys_passing(s, c, blk); }
static id d_init_objs_keys(id s, SEL c, id objs, id keys) {
    (void)c;
    u64 n = cnt(keys);
    for (u64 i = 0; i < n; i++) MSG(void, s, "setObject:forKey:", at(objs, i), at(keys, i));
    return s;
}
static id d_with_objs_keys(id k, SEL c, id objs, id keys) {
    id m = MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
    d_init_objs_keys(m, c, objs, keys);
    id r = (cx_class_t *)k == cx_class_named("NSMutableDictionary") ? m : MSG0(id, m, "copy");
    if (r != m) objc_release(m);
    return ar(r);
}
static id d_init_objs_and_keys(id s, SEL c, id first, ...) {
    (void)c;
    va_list ap;
    va_start(ap, first);
    for (id v = first; v; v = va_arg(ap, id)) { id k = va_arg(ap, id); if (!k) break; MSG(void, s, "setObject:forKey:", v, k); }
    va_end(ap);
    return s;
}
static id d_init_dict_copy(id s, SEL c, id o, BOOL deep) {
    (void)c;
    id ks = o ? d_keys(o) : 0;
    u64 n = cnt(ks);
    for (u64 i = 0; i < n; i++) {
        id k = at(ks, i), v = d_get(o, k);
        if (deep) { v = MSG(id, v, "copyWithZone:", (void *)0); MSG(void, s, "setObject:forKey:", v, k); objc_release(v); }
        else MSG(void, s, "setObject:forKey:", v, k);
    }
    return s;
}
static id d_init_dict(id s, SEL c, id o) { return d_init_dict_copy(s, c, o, NO); }
/* NSSharedKeySet: só uma otimização de memória; o conjunto de chaves basta */
static id d_shared_keyset(id k, SEL c, id keys) { (void)k; (void)c; return keys ? ar(MSG0(id, keys, "copy")) : 0; }
static id d_with_shared(id k, SEL c, id ks) { (void)ks; (void)c; return ar(MSG0(id, MSG0(id, k, "alloc"), "init")); }
static id d_key_enum(id s, SEL c) { (void)c; return cx_enumerator_new(d_keys(s)); }
static id d_obj_enum(id s, SEL c) { (void)c; return cx_enumerator_new(MSG0(id, s, "allValues")); }
static id d_strings_format(id s, SEL c) {
    (void)c;
    id ks = d_keys(s), out = mstr();
    u64 n = cnt(ks);
    for (u64 i = 0; i < n; i++) {
        id k = at(ks, i), v = d_get(s, k);
        appo(out, quoted(MSG0(id, k, "description"), k));
        app(out, " = ");
        id dv = MSG0(id, v, "description");
        appo(out, quoted(dv, cx_is_str(v) ? v : dv));
        app(out, ";\n");
    }
    return ar(out);
}
static id d_desc_indent(id s, SEL c, id loc, u64 lvl) {
    (void)c;
    id ks = d_keys(s), out = mstr();
    u64 n = cnt(ks);
    app(out, "{\n");
    for (u64 i = 0; i < n; i++) {
        id k = at(ks, i), v = d_get(s, k);
        indent(out, lvl + 1);
        appo(out, quoted(desc_of(k, loc, lvl + 1), k));
        app(out, " = ");
        appo(out, quoted(desc_of(v, loc, lvl + 1), v));
        app(out, ";\n");
    }
    indent(out, lvl);
    app(out, "}");
    return ar(out);
}
static id d_desc_loc(id s, SEL c, id loc) { return d_desc_indent(s, c, loc, 0); }
static BOOL d_write_url_err(id s, SEL c, id url, id *err) { (void)c; return write_url(s, url, err); }
static BOOL d_write_url(id s, SEL c, id url, BOOL a) { (void)c; (void)a; return write_url(s, url, 0); }
static id d_with_url(id k, SEL c, id url) { (void)c; return read_url(k, url, "dictionaryWithContentsOfFile:", 0); }
static id d_with_url_err(id k, SEL c, id url, id *err) { (void)c; return read_url(k, url, "dictionaryWithContentsOfFile:", err); }
/* atributos de arquivo (NSDictionary devolvido por attributesOfItemAtPath:) */
static id attr(id s, const char *k) { return d_get(s, cx_str(k)); }
static u64 d_file_size(id s, SEL c) { (void)c; id v = attr(s, "NSFileSize"); return v ? MSG0(u64, v, "unsignedLongLongValue") : 0; }
static id d_file_mod(id s, SEL c) { (void)c; return attr(s, "NSFileModificationDate"); }
static id d_file_create(id s, SEL c) { (void)c; return attr(s, "NSFileCreationDate"); }
static id d_file_type(id s, SEL c) { (void)c; return attr(s, "NSFileType"); }
static u64 d_file_perm(id s, SEL c) { (void)c; id v = attr(s, "NSFilePosixPermissions"); return v ? MSG0(u64, v, "unsignedLongValue") : 0; }
static id d_file_owner(id s, SEL c) { (void)c; return attr(s, "NSFileOwnerAccountName"); }
static id d_file_group(id s, SEL c) { (void)c; return attr(s, "NSFileGroupOwnerAccountName"); }
static id d_file_owner_id(id s, SEL c) { (void)c; return attr(s, "NSFileOwnerAccountID"); }
static id d_file_group_id(id s, SEL c) { (void)c; return attr(s, "NSFileGroupOwnerAccountID"); }
static long d_fs_number(id s, SEL c) { (void)c; id v = attr(s, "NSFileSystemNumber"); return v ? MSG0(long, v, "integerValue") : 0; }
static u64 d_fs_file_number(id s, SEL c) { (void)c; id v = attr(s, "NSFileSystemFileNumber"); return v ? MSG0(u64, v, "unsignedLongValue") : 0; }
static BOOL d_flag(id s, const char *k) { id v = attr(s, k); return v && MSG0(BOOL, v, "boolValue"); }
static BOOL d_ext_hidden(id s, SEL c) { (void)c; return d_flag(s, "NSFileExtensionHidden"); }
static BOOL d_immutable(id s, SEL c) { (void)c; return d_flag(s, "NSFileImmutable"); }
static BOOL d_append_only(id s, SEL c) { (void)c; return d_flag(s, "NSFileAppendOnly"); }
static u32 d_code(id s, const char *k) { id v = attr(s, k); return v ? MSG0(u32, v, "unsignedIntValue") : 0; }
static u32 d_creator(id s, SEL c) { (void)c; return d_code(s, "NSFileHFSCreatorCode"); }
static u32 d_typecode(id s, SEL c) { (void)c; return d_code(s, "NSFileHFSTypeCode"); }

static void md_remove_keys(id s, SEL c, id keys) {
    (void)c;
    u64 n = cnt(keys);
    for (u64 i = 0; i < n; i++) MSG(void, s, "removeObjectForKey:", at(keys, i));
}

/* ---------------------------------------------------------------- NSDate (nsdate_t de foundation.c: segundos desde 1970) */
typedef struct { cx_class_t *isa; double t; } nsdate_t;
#define REF_1970 978307200.0
static double dt(id d) { return d ? MSG0(double, d, "timeIntervalSince1970") : 0; }
static id dt_set(id s, double t) { ((nsdate_t *)s)->t = t; return s; }
static id dt_new(id k, double t) { return ar(dt_set(cx_alloc_instance((cx_class_t *)k), t)); }
static id t_init(id s, SEL c) { (void)c; return dt_set(s, cx_now()); }   /* [[NSDate alloc] init] é agora */
static id t_init_now(id s, SEL c, double v) { (void)c; return dt_set(s, cx_now() + v); }
static id t_init_1970(id s, SEL c, double v) { (void)c; return dt_set(s, v); }
static id t_init_ref(id s, SEL c, double v) { (void)c; return dt_set(s, v + REF_1970); }
static id t_init_since(id s, SEL c, double v, id d) { (void)c; return dt_set(s, dt(d) + v); }
static id t_with_ref(id k, SEL c, double v) { (void)c; return dt_new(k, v + REF_1970); }
static id t_with_since(id k, SEL c, double v, id d) { (void)c; return dt_new(k, dt(d) + v); }
static id t_adding(id s, SEL c, double v) { (void)c; return dt_new((id)*(cx_class_t **)s, dt(s) + v); }
static id t_earlier(id s, SEL c, id o) { (void)c; return !o || dt(s) <= dt(o) ? s : o; }
static id t_later(id s, SEL c, id o) { (void)c; return !o || dt(s) >= dt(o) ? s : o; }
static long t_compare(id s, SEL c, id o) { (void)c; double a = dt(s), b = dt(o); return a < b ? -1 : a > b ? 1 : 0; }
static BOOL t_equal_date(id s, SEL c, id o) { (void)c; return o && dt(s) == dt(o); }
static BOOL t_is_equal(id s, SEL c, id o) { return o && MSG(BOOL, o, "isKindOfClass:", cls("NSDate")) && t_equal_date(s, c, o); }
static u64 t_hash(id s, SEL c) { (void)c; return (u64)(i64)dt(s); }
static id t_copy(id s, SEL c) { (void)c; return objc_retain(s); }
static id t_copy_zone(id s, SEL c, void *z) { (void)z; return t_copy(s, c); }
static void t_encode(id s, SEL c, id coder) { (void)c; MSG(void, coder, "encodeDouble:forKey:", dt(s) - REF_1970, cx_str("NS.time")); }
static id t_decode(id s, SEL c, id coder) { (void)c; return dt_set(s, MSG(double, coder, "decodeDoubleForKey:", cx_str("NS.time")) + REF_1970); }

void cx_fdn_coll_init(void) {
    cx_class_t *A = cx_class_named("NSArray"), *MA = cx_class_named("NSMutableArray");
    cx_class_t *D = cx_class_named("NSDictionary"), *MD = cx_class_named("NSMutableDictionary");
    if (!A || !MA || !D || !MD) return;
#define M(k, s, f) cx_add_method(k, s, (void *)(f))
#define CM(k, s, f) cx_add_cmethod(k, s, (void *)(f))
    M(A, "subarrayWithRange:", a_sub); M(A, "getObjects:range:", a_get_range); M(A, "getObjects:", a_get);
    M(A, "indexOfObject:inRange:", a_index_in); M(A, "firstObjectCommonWithArray:", a_common);
    M(A, "objectEnumerator", a_obj_enum); M(A, "reverseObjectEnumerator", a_reverse_enum); M(A, "sortedArrayHint", a_hint);
    M(A, "sortedArrayUsingFunction:context:", a_sorted_func); M(A, "sortedArrayUsingFunction:context:hint:", a_sorted_func_hint);
    M(A, "sortedArrayWithOptions:usingComparator:", a_sorted_opts); M(A, "sortedArrayUsingComparator:", a_sorted_cmp);
    M(A, "makeObjectsPerformSelector:", a_perform); M(A, "makeObjectsPerformSelector:withObject:", a_perform_with);
    M(A, "objectsAtIndexes:", a_at_indexes);
    M(A, "enumerateObjectsWithOptions:usingBlock:", a_enum_opts); M(A, "enumerateObjectsAtIndexes:options:usingBlock:", a_enum_at);
    M(A, "indexOfObjectPassingTest:", a_index_pass); M(A, "indexOfObjectWithOptions:passingTest:", a_index_pass_opts);
    M(A, "indexOfObjectAtIndexes:options:passingTest:", a_index_pass_at);
    M(A, "indexesOfObjectsPassingTest:", a_indexes_pass); M(A, "indexesOfObjectsWithOptions:passingTest:", a_indexes_pass_opts);
    M(A, "indexesOfObjectsAtIndexes:options:passingTest:", a_indexes_pass_at);
    M(A, "initWithArray:copyItems:", a_init_copy_items); M(A, "filteredArrayUsingPredicate:", a_filtered);
    M(A, "pathsMatchingExtensions:", a_paths_ext);
    M(A, "writeToURL:error:", a_write_url_err); M(A, "writeToURL:atomically:", a_write_url);
    CM(A, "arrayWithContentsOfURL:error:", a_with_url_err); CM(A, "arrayWithContentsOfURL:", a_with_url);
    M(A, "initWithContentsOfURL:", a_init_url); M(A, "initWithContentsOfURL:error:", a_init_url_err);
    M(A, "addObserver:toObjectsAtIndexes:forKeyPath:options:context:", a_observe_at);
    M(A, "removeObserver:fromObjectsAtIndexes:forKeyPath:", a_unobserve_at);
    M(A, "descriptionWithLocale:", a_desc_loc); M(A, "descriptionWithLocale:indent:", a_desc_indent);
    M(MA, "exchangeObjectAtIndex:withObjectAtIndex:", m_exchange); M(MA, "removeObjectsAtIndexes:", m_remove_at_indexes);
    M(MA, "removeObjectsFromIndices:numIndices:", m_remove_indices); M(MA, "removeObjectsInArray:", m_remove_in_array);
    M(MA, "replaceObjectsInRange:withObjectsFromArray:range:", m_replace_range_from);
    M(MA, "replaceObjectsInRange:withObjectsFromArray:", m_replace_range);
    M(MA, "insertObjects:atIndexes:", m_insert_at_indexes); M(MA, "replaceObjectsAtIndexes:withObjects:", m_replace_at_indexes);
    M(MA, "sortUsingFunction:context:", m_sort_func); M(MA, "sortUsingComparator:", m_sort_cmp);
    M(MA, "sortWithOptions:usingComparator:", m_sort_opts); M(MA, "filterUsingPredicate:", m_filter);
    M(D, "allKeysForObject:", d_keys_for); M(D, "objectsForKeys:notFoundMarker:", d_objs_for);
    M(D, "getObjects:andKeys:count:", d_get_objs_keys); M(D, "getObjects:andKeys:", d_get_objs_keys_all);
    M(D, "enumerateKeysAndObjectsWithOptions:usingBlock:", d_enum_opts);
    M(D, "keysSortedByValueUsingSelector:", d_keys_sorted_sel); M(D, "keysSortedByValueUsingComparator:", d_keys_sorted_cmp);
    M(D, "keysSortedByValueWithOptions:usingComparator:", d_keys_sorted_opts);
    M(D, "keysOfEntriesPassingTest:", d_keys_passing); M(D, "keysOfEntriesWithOptions:passingTest:", d_keys_passing_opts);
    CM(D, "dictionaryWithObjects:forKeys:", d_with_objs_keys);
    M(D, "initWithObjects:forKeys:", d_init_objs_keys); M(D, "initWithObjectsAndKeys:", d_init_objs_and_keys);
    M(D, "initWithDictionary:", d_init_dict); M(D, "initWithDictionary:copyItems:", d_init_dict_copy);
    CM(D, "sharedKeySetForKeys:", d_shared_keyset); CM(MD, "dictionaryWithSharedKeySet:", d_with_shared);
    M(D, "keyEnumerator", d_key_enum); M(D, "objectEnumerator", d_obj_enum);
    M(D, "descriptionInStringsFileFormat", d_strings_format);
    M(D, "descriptionWithLocale:", d_desc_loc); M(D, "descriptionWithLocale:indent:", d_desc_indent);
    M(D, "writeToURL:error:", d_write_url_err); M(D, "writeToURL:atomically:", d_write_url);
    CM(D, "dictionaryWithContentsOfURL:", d_with_url); CM(D, "dictionaryWithContentsOfURL:error:", d_with_url_err);
    M(D, "fileSize", d_file_size); M(D, "fileModificationDate", d_file_mod); M(D, "fileCreationDate", d_file_create);
    M(D, "fileType", d_file_type); M(D, "filePosixPermissions", d_file_perm);
    M(D, "fileOwnerAccountName", d_file_owner); M(D, "fileGroupOwnerAccountName", d_file_group);
    M(D, "fileOwnerAccountID", d_file_owner_id); M(D, "fileGroupOwnerAccountID", d_file_group_id);
    M(D, "fileSystemNumber", d_fs_number); M(D, "fileSystemFileNumber", d_fs_file_number);
    M(D, "fileExtensionHidden", d_ext_hidden); M(D, "fileIsImmutable", d_immutable); M(D, "fileIsAppendOnly", d_append_only);
    M(D, "fileHFSCreatorCode", d_creator); M(D, "fileHFSTypeCode", d_typecode);
    M(MD, "removeObjectsForKeys:", md_remove_keys);
    cx_class_t *T = cx_class_named("NSDate");
    if (T) {
        M(T, "init", t_init); M(T, "initWithTimeIntervalSinceNow:", t_init_now); M(T, "initWithTimeIntervalSince1970:", t_init_1970);
        M(T, "initWithTimeIntervalSinceReferenceDate:", t_init_ref); M(T, "initWithTimeInterval:sinceDate:", t_init_since);
        CM(T, "dateWithTimeIntervalSinceReferenceDate:", t_with_ref); CM(T, "dateWithTimeInterval:sinceDate:", t_with_since);
        M(T, "addTimeInterval:", t_adding); M(T, "dateByAddingTimeInterval:", t_adding);
        M(T, "earlierDate:", t_earlier); M(T, "laterDate:", t_later); M(T, "compare:", t_compare);
        M(T, "isEqualToDate:", t_equal_date); M(T, "isEqual:", t_is_equal); M(T, "hash", t_hash);
        M(T, "copy", t_copy); M(T, "copyWithZone:", t_copy_zone);
        M(T, "encodeWithCoder:", t_encode); M(T, "initWithCoder:", t_decode);
    }
#undef M
#undef CM
}
