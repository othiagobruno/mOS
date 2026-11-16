/*
 * mOS libSystem — coleções do CoreFoundation em ponte com o Foundation do mOS (porte do VS Code).
 * CFDictionaryRef = NSDictionary*, CFArrayRef = NSArray*, CFNumberRef/CFBooleanRef = NSNumber*.
 * O Chromium monta assim os dicionários de propriedades do IOSurface (CFDictionaryCreateMutable +
 * CFNumberCreate) e lê as respostas. Os callbacks de chave/valor são aceitos e ignorados: tudo é
 * tratado como objeto CF (retain/release), que é o caso de kCFType*CallBacks.
 * As leituras simples (CFDictionaryGetValue, CFArrayGetCount...) ficam em darwin.c.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);

/* ---- callbacks (CFArrayCallBacks, CFDictionaryKeyCallBacks/ValueCallBacks: version, retain, release,
 * copyDescription, equal, hash) e as constantes kCFType* com as funções de verdade */
typedef struct {
    long version;
    const void *(*retain)(const void *alloc, const void *v);
    void (*release)(const void *alloc, const void *v);
    id (*desc)(const void *v);
    u8 (*equal)(const void *a, const void *b);
    u64 (*hash)(const void *v);
} cf_cb_t;
static const void *cft_retain(const void *a, const void *v) { (void)a; return v ? (const void *)objc_retain((id)v) : v; }
static void cft_release(const void *a, const void *v) { (void)a; if (v) objc_release((id)v); }
static id cft_desc(const void *v) { return v ? objc_retain(MSG0(id, (id)v, "description")) : 0; }
static u8 cft_equal(const void *a, const void *b) { return a == b || (a && b && MSG(BOOL, (id)a, "isEqual:", (id)b)); }
static u64 cft_hash(const void *v) { return v ? MSG0(u64, (id)v, "hash") : 0; }
static const void *cft_copy_str(const void *a, const void *v) { (void)a; return v ? (const void *)MSG0(id, (id)v, "copy") : v; }
const cf_cb_t kCFTypeDictionaryKeyCallBacks = { 0, cft_retain, cft_release, cft_desc, cft_equal, cft_hash };
const cf_cb_t kCFTypeDictionaryValueCallBacks = { 0, cft_retain, cft_release, cft_desc, cft_equal, 0 };
const cf_cb_t kCFCopyStringDictionaryKeyCallBacks = { 0, cft_copy_str, cft_release, cft_desc, cft_equal, cft_hash };
const cf_cb_t kCFTypeArrayCallBacks = { 0, cft_retain, cft_release, cft_desc, cft_equal, 0 };   /* kCFTypeSetCallBacks e kCFCopyStringSetCallBacks: fdn_set.c */
const cf_cb_t kCFTypeBagCallBacks = { 0, cft_retain, cft_release, cft_desc, cft_equal, cft_hash };

/* o comportamento de objeto CF (retain + isEqual:), que o NSMutableArray/NSMutableDictionary já têm */
static int is_cftype(const cf_cb_t *cb, int with_hash) {
    if (!cb) return 0;
    if (cb->retain != cft_retain && cb->retain != cft_copy_str) return 0;
    if (cb->release != cft_release || cb->equal != cft_equal) return 0;
    return !with_hash || cb->hash == cft_hash;
}
static const void *cb_keep(const cf_cb_t *cb, const void *v) { return cb->retain ? cb->retain(0, v) : v; }
static void cb_drop(const cf_cb_t *cb, const void *v) { if (cb->release) cb->release(0, v); }
static int cb_eq(const cf_cb_t *cb, const void *a, const void *b) { return a == b || (cb->equal && cb->equal(a, b)); }
static u64 cb_hash(const cf_cb_t *cb, const void *v) { return cb->hash ? cb->hash(v) : (u64)v >> 3; }
static const cf_cb_t NULL_CB = { 0, 0, 0, 0, 0, 0 };

/* ---------------------------------------------------------------- __NSCFArrayCB */
typedef struct { cx_class_t *isa; const void **items; u64 n, cap; cf_cb_t cb; } cba_t;   /* começa como nsarr_t */
typedef struct { cx_class_t *isa; const void **keys, **vals; u64 n, cap; cf_cb_t kcb, vcb; u64 *hashes; } cbd_t;   /* começa como nsdict_t */
static cx_class_t cba_c, cba_m, cbd_c, cbd_m;
#define A(o) ((cba_t *)(o))
#define D(o) ((cbd_t *)(o))
#define NOTFOUND 0x7FFFFFFFFFFFFFFFULL
void cx_object_free(id o);
void cx_kvo_forget(id o);
id cx_enumerator_new(id array);

static void raise_range(const char *what) {
    id e = MSG(id, (id)cx_class_named("NSException"), "exceptionWithName:reason:userInfo:", cx_str("NSRangeException"), cx_str(what), (id)0);
    MSG0(void, e, "raise");
}
static id cba_new(const cf_cb_t *cb) {
    id a = cx_alloc_instance(&cba_c);
    A(a)->cb = cb ? *cb : NULL_CB;
    return a;
}
static void cba_grow(cba_t *a, u64 want) {
    if (want <= a->cap) return;
    u64 c = a->cap ? a->cap * 2 : 8;
    while (c < want) c *= 2;
    a->items = realloc(a->items, c * sizeof(void *));
    a->cap = c;
}
static void cba_insert(id self, SEL c, const void *v, u64 i) {
    (void)c;
    cba_t *a = A(self);
    if (i > a->n) { raise_range("*** -[__NSCFArray insertObject:atIndex:]: index beyond bounds"); return; }
    cba_grow(a, a->n + 1);
    memmove(a->items + i + 1, a->items + i, (a->n - i) * sizeof(void *));
    a->items[i] = cb_keep(&a->cb, v);
    a->n++;
}
static void cba_add(id self, SEL c, const void *v) { cba_insert(self, c, v, A(self)->n); }
static void cba_remove_at(id self, SEL c, u64 i) {
    (void)c;
    cba_t *a = A(self);
    if (i >= a->n) { raise_range("*** -[__NSCFArray removeObjectAtIndex:]: index beyond bounds"); return; }
    const void *v = a->items[i];
    memmove(a->items + i, a->items + i + 1, (a->n - i - 1) * sizeof(void *));
    a->n--;
    cb_drop(&a->cb, v);
}
static void cba_replace(id self, SEL c, u64 i, const void *v) {
    (void)c;
    cba_t *a = A(self);
    if (i == a->n) { cba_add(self, c, v); return; }
    if (i > a->n) { raise_range("*** -[__NSCFArray replaceObjectAtIndex:withObject:]: index beyond bounds"); return; }
    const void *old = a->items[i];
    a->items[i] = cb_keep(&a->cb, v);
    cb_drop(&a->cb, old);
}
static void cba_set_sub(id self, SEL c, const void *v, u64 i) { cba_replace(self, c, i, v); }
static void cba_remove_all(id self, SEL c) {
    (void)c;
    cba_t *a = A(self);
    while (a->n) { const void *v = a->items[--a->n]; cb_drop(&a->cb, v); }
}
static void cba_remove_last(id self, SEL c) { if (A(self)->n) cba_remove_at(self, c, A(self)->n - 1); }
typedef struct { u64 loc, len; } rng_t;
static void cba_remove_range(id self, SEL c, rng_t r) {
    if (r.loc > A(self)->n || r.len > A(self)->n - r.loc) { raise_range("*** -[__NSCFArray removeObjectsInRange:]: range beyond bounds"); return; }
    for (u64 k = 0; k < r.len; k++) cba_remove_at(self, c, r.loc);
}
static u64 cba_index_in(id self, SEL c, const void *v, rng_t r) {
    (void)c;
    cba_t *a = A(self);
    for (u64 i = r.loc; i < r.loc + r.len && i < a->n; i++) if (cb_eq(&a->cb, a->items[i], v)) return i;
    return NOTFOUND;
}
static u64 cba_index(id self, SEL c, const void *v) { rng_t r = { 0, A(self)->n }; return cba_index_in(self, c, v, r); }
static BOOL cba_contains(id self, SEL c, const void *v) { return cba_index(self, c, v) != NOTFOUND; }
static void cba_remove_obj(id self, SEL c, const void *v) {
    cba_t *a = A(self);
    for (u64 i = a->n; i-- > 0;) if (cb_eq(&a->cb, a->items[i], v)) cba_remove_at(self, c, i);
}
static void cba_remove_obj_range(id self, SEL c, const void *v, rng_t r) {
    cba_t *a = A(self);
    for (u64 i = r.loc + r.len; i-- > r.loc;) if (i < a->n && cb_eq(&a->cb, a->items[i], v)) cba_remove_at(self, c, i);
}
static void cba_remove_ident(id self, SEL c, const void *v) {
    cba_t *a = A(self);
    for (u64 i = a->n; i-- > 0;) if (a->items[i] == v) cba_remove_at(self, c, i);
}
static void cba_exchange(id self, SEL c, u64 i, u64 j) {
    (void)c;
    cba_t *a = A(self);
    if (i >= a->n || j >= a->n) { raise_range("*** -[__NSCFArray exchangeObjectAtIndex:withObjectAtIndex:]: index beyond bounds"); return; }
    const void *t = a->items[i]; a->items[i] = a->items[j]; a->items[j] = t;
}
static void cba_add_array(id self, SEL c, id src) {
    u64 n = src ? MSG0(u64, src, "count") : 0;
    for (u64 i = 0; i < n; i++) cba_add(self, c, MSG(id, src, "objectAtIndex:", i));
}
static id cba_copy_with(id self, int mut) {
    (void)mut;
    id r = cba_new(&A(self)->cb);
    for (u64 i = 0; i < A(self)->n; i++) cba_add(r, 0, A(self)->items[i]);
    return r;
}
static id cba_copy(id self, SEL c) { (void)c; return cba_copy_with(self, 0); }
static id cba_copy_zone(id self, SEL c, void *z) { (void)c; (void)z; return cba_copy_with(self, 0); }
static id cba_mcopy(id self, SEL c) { (void)c; return cba_copy_with(self, 1); }
static id cba_mcopy_zone(id self, SEL c, void *z) { (void)c; (void)z; return cba_copy_with(self, 1); }
static BOOL cba_equal(id self, SEL c, id o) {
    (void)c;
    if (o == self) return YES;
    if (!o || !MSG(BOOL, o, "isKindOfClass:", (id)cx_class_named("NSArray"))) return NO;
    u64 n = MSG0(u64, o, "count");
    if (n != A(self)->n) return NO;
    for (u64 i = 0; i < n; i++) if (!cb_eq(&A(self)->cb, A(self)->items[i], MSG(id, o, "objectAtIndex:", i))) return NO;
    return YES;
}
static u64 cba_hash(id self, SEL c) { (void)c; return A(self)->n; }
static id cba_desc(id self, SEL c) {
    (void)c;
    cba_t *a = A(self);
    u64 cap = 64 + a->n * 96;
    char *b = malloc(cap);
    u64 k = (u64)snprintf(b, cap, "(\n");
    for (u64 i = 0; i < a->n; i++) {
        id d = a->cb.desc ? a->cb.desc(a->items[i]) : 0;
        if (d) { k += (u64)snprintf(b + k, cap - k, "    %s%s\n", MSG0(const char *, d, "UTF8String"), i + 1 < a->n ? "," : ""); objc_release(d); }
        else k += (u64)snprintf(b + k, cap - k, "    <%p>%s\n", a->items[i], i + 1 < a->n ? "," : "");
        if (k + 128 > cap) { cap *= 2; b = realloc(b, cap); }
    }
    snprintf(b + k, cap - k, ")");
    id r = cx_str(b);
    free(b);
    return r;
}
static void cba_sort_fn(id self, SEL c, long (*cmp)(const void *, const void *, void *), void *ctx) {
    (void)c;
    cba_t *a = A(self);
    for (u64 i = 1; i < a->n; i++) {               /* inserção: estável, como o CFArraySortValues */
        const void *v = a->items[i];
        u64 j = i;
        while (j > 0 && cmp(a->items[j - 1], v, ctx) > 0) { a->items[j] = a->items[j - 1]; j--; }
        a->items[j] = v;
    }
}
static void cba_dealloc(id self, SEL c) {
    (void)c;
    cba_remove_all(self, 0);
    free(A(self)->items);
    cx_kvo_forget(self);
    cx_object_free(self);
}

/* ---------------------------------------------------------------- __NSCFDictionaryCB */
static id cbd_new(const cf_cb_t *kcb, const cf_cb_t *vcb) {
    id d = cx_alloc_instance(&cbd_c);
    D(d)->kcb = kcb ? *kcb : NULL_CB;
    D(d)->vcb = vcb ? *vcb : NULL_CB;
    return d;
}
static long cbd_find(cbd_t *d, const void *k) {
    u64 h = cb_hash(&d->kcb, k);
    for (u64 i = 0; i < d->n; i++) if (d->hashes[i] == h && cb_eq(&d->kcb, d->keys[i], k)) return (long)i;
    return -1;
}
static const void *cbd_get(id self, SEL c, const void *k) { (void)c; long i = cbd_find(D(self), k); return i < 0 ? 0 : D(self)->vals[i]; }
static void cbd_set(id self, SEL c, const void *v, const void *k) {
    (void)c;
    cbd_t *d = D(self);
    long i = cbd_find(d, k);
    if (i >= 0) {
        const void *old = d->vals[i];
        d->vals[i] = cb_keep(&d->vcb, v);
        cb_drop(&d->vcb, old);
        return;
    }
    if (d->n == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 8;
        d->keys = realloc(d->keys, d->cap * sizeof(void *));
        d->vals = realloc(d->vals, d->cap * sizeof(void *));
        d->hashes = realloc(d->hashes, d->cap * sizeof(u64));
    }
    d->keys[d->n] = cb_keep(&d->kcb, k);
    d->vals[d->n] = cb_keep(&d->vcb, v);
    d->hashes[d->n] = cb_hash(&d->kcb, d->keys[d->n]);
    d->n++;
}
static void cbd_remove_at(cbd_t *d, u64 i) {
    const void *k = d->keys[i], *v = d->vals[i];
    d->n--;
    d->keys[i] = d->keys[d->n]; d->vals[i] = d->vals[d->n]; d->hashes[i] = d->hashes[d->n];
    cb_drop(&d->kcb, k);
    cb_drop(&d->vcb, v);
}
static void cbd_remove(id self, SEL c, const void *k) { (void)c; long i = cbd_find(D(self), k); if (i >= 0) cbd_remove_at(D(self), (u64)i); }
static void cbd_remove_all(id self, SEL c) { (void)c; while (D(self)->n) cbd_remove_at(D(self), D(self)->n - 1); }
static u64 cbd_count(id self, SEL c) { (void)c; return D(self)->n; }
static id cbd_keys(id self, SEL c) {
    (void)c;
    id a = cba_new(&D(self)->kcb);
    for (u64 i = 0; i < D(self)->n; i++) cba_add(a, 0, D(self)->keys[i]);
    return objc_autorelease(a);
}
static id cbd_vals(id self, SEL c) {
    (void)c;
    id a = cba_new(&D(self)->vcb);
    for (u64 i = 0; i < D(self)->n; i++) cba_add(a, 0, D(self)->vals[i]);
    return objc_autorelease(a);
}
static id cbd_key_enum(id self, SEL c) { return cx_enumerator_new(cbd_keys(self, c)); }
static id cbd_obj_enum(id self, SEL c) { return cx_enumerator_new(cbd_vals(self, c)); }
static void cbd_enum_block(id self, SEL c, void *blk) {
    (void)c;
    typedef void (*fn_t)(void *, const void *, const void *, BOOL *);
    fn_t fn = *(fn_t *)((u8 *)blk + 16);
    BOOL stop = NO;
    for (u64 i = 0; i < D(self)->n && !stop; i++) fn(blk, D(self)->keys[i], D(self)->vals[i], &stop);
}
static id cbd_copy_with(id self) {
    id r = cbd_new(&D(self)->kcb, &D(self)->vcb);
    for (u64 i = 0; i < D(self)->n; i++) cbd_set(r, 0, D(self)->vals[i], D(self)->keys[i]);
    return r;
}
static id cbd_copy(id self, SEL c) { (void)c; return cbd_copy_with(self); }
static id cbd_copy_zone(id self, SEL c, void *z) { (void)c; (void)z; return cbd_copy_with(self); }
static BOOL cbd_equal(id self, SEL c, id o) {
    (void)c;
    if (o == self) return YES;
    if (!o || !MSG(BOOL, o, "isKindOfClass:", (id)cx_class_named("NSDictionary"))) return NO;
    if (MSG0(u64, o, "count") != D(self)->n) return NO;
    for (u64 i = 0; i < D(self)->n; i++) {
        id v = MSG(id, o, "objectForKey:", (id)D(self)->keys[i]);
        if (!v || !cb_eq(&D(self)->vcb, D(self)->vals[i], v)) return NO;
    }
    return YES;
}
static u64 cbd_hash_m(id self, SEL c) { (void)c; return D(self)->n; }
static id cbd_desc(id self, SEL c) {
    (void)c;
    cbd_t *d = D(self);
    u64 cap = 64 + d->n * 160;
    char *b = malloc(cap);
    u64 k = (u64)snprintf(b, cap, "{\n");
    for (u64 i = 0; i < d->n; i++) {
        id kd = d->kcb.desc ? d->kcb.desc(d->keys[i]) : 0, vd = d->vcb.desc ? d->vcb.desc(d->vals[i]) : 0;
        char ks[32], vs[32];
        snprintf(ks, sizeof ks, "<%p>", d->keys[i]); snprintf(vs, sizeof vs, "<%p>", d->vals[i]);
        k += (u64)snprintf(b + k, cap - k, "    %s = %s;\n", kd ? MSG0(const char *, kd, "UTF8String") : ks, vd ? MSG0(const char *, vd, "UTF8String") : vs);
        if (kd) objc_release(kd);
        if (vd) objc_release(vd);
        if (k + 200 > cap) { cap *= 2; b = realloc(b, cap); }
    }
    snprintf(b + k, cap - k, "}");
    id r = cx_str(b);
    free(b);
    return r;
}
static void cbd_dealloc(id self, SEL c) {
    (void)c;
    cbd_remove_all(self, 0);
    free(D(self)->keys); free(D(self)->vals); free(D(self)->hashes);
    cx_kvo_forget(self);
    cx_object_free(self);
}

/* ponteiros crus ou callbacks próprios? (CFArrayGetFirstIndexOfValue & cia comparam pelos callbacks) */
int cx_cfarray_equal(id a, const void *x, const void *y) {
    if (a && cx_is_subclass(*(cx_class_t **)a, &cba_c)) return cb_eq(&A(a)->cb, x, y);
    return cft_equal(x, y);
}
int cx_cfdict_value_equal(id d, const void *x, const void *y) {
    if (d && cx_is_subclass(*(cx_class_t **)d, &cbd_c)) return cb_eq(&D(d)->vcb, x, y);
    return cft_equal(x, y);
}

void cx_cfcoll_init(void) {
    cx_class_t *ma = cx_class_named("NSMutableArray"), *md = cx_class_named("NSMutableDictionary");
    if (!ma || !md || cx_class_named("__NSCFArrayCB") == &cba_c) return;
    cx_define_class(&cba_c, &cba_m, "__NSCFArrayCB", ma, (u32)(sizeof(cba_t) - 8));
    cx_define_class(&cbd_c, &cbd_m, "__NSCFDictionaryCB", md, (u32)(sizeof(cbd_t) - 8));
#define AM(s, f) cx_add_method(&cba_c, s, (void *)(f))
#define DM(s, f) cx_add_method(&cbd_c, s, (void *)(f))
    AM("addObject:", cba_add); AM("insertObject:atIndex:", cba_insert); AM("removeObjectAtIndex:", cba_remove_at);
    AM("replaceObjectAtIndex:withObject:", cba_replace); AM("setObject:atIndexedSubscript:", cba_set_sub);
    AM("removeAllObjects", cba_remove_all); AM("removeLastObject", cba_remove_last); AM("removeObjectsInRange:", cba_remove_range);
    AM("indexOfObject:", cba_index); AM("indexOfObject:inRange:", cba_index_in); AM("containsObject:", cba_contains);
    AM("removeObject:", cba_remove_obj); AM("removeObject:inRange:", cba_remove_obj_range);
    AM("removeObjectIdenticalTo:", cba_remove_ident); AM("exchangeObjectAtIndex:withObjectAtIndex:", cba_exchange);
    AM("addObjectsFromArray:", cba_add_array); AM("sortUsingFunction:context:", cba_sort_fn);
    AM("copy", cba_copy); AM("copyWithZone:", cba_copy_zone); AM("mutableCopy", cba_mcopy); AM("mutableCopyWithZone:", cba_mcopy_zone);
    AM("isEqual:", cba_equal); AM("isEqualToArray:", cba_equal); AM("hash", cba_hash);
    AM("description", cba_desc); AM("debugDescription", cba_desc); AM("dealloc", cba_dealloc);
    DM("objectForKey:", cbd_get); DM("objectForKeyedSubscript:", cbd_get); DM("valueForKey:", cbd_get);
    DM("setObject:forKey:", cbd_set); DM("setObject:forKeyedSubscript:", cbd_set); DM("removeObjectForKey:", cbd_remove);
    DM("removeAllObjects", cbd_remove_all); DM("count", cbd_count); DM("allKeys", cbd_keys); DM("allValues", cbd_vals);
    DM("keyEnumerator", cbd_key_enum); DM("objectEnumerator", cbd_obj_enum);
    DM("enumerateKeysAndObjectsUsingBlock:", cbd_enum_block);
    DM("copy", cbd_copy); DM("copyWithZone:", cbd_copy_zone); DM("mutableCopy", cbd_copy); DM("mutableCopyWithZone:", cbd_copy_zone);
    DM("isEqual:", cbd_equal); DM("isEqualToDictionary:", cbd_equal); DM("hash", cbd_hash_m);
    DM("description", cbd_desc); DM("debugDescription", cbd_desc); DM("dealloc", cbd_dealloc);
#undef AM
#undef DM
}

/* ---- identificadores de tipo (os mesmos números do CF da Apple) */
enum { TID_STRING = 7, TID_NULL = 16, TID_DICT = 18, TID_ARRAY = 19, TID_DATA = 20, TID_BOOL = 21, TID_NUMBER = 22,
       TID_URL = 29, TID_ERROR = 30, TID_UUID = 34, TID_DATE = 42 };
u64 CFNullGetTypeID(void) { return TID_NULL; }
u64 CFURLGetTypeID(void) { return TID_URL; }
u64 CFUUIDGetTypeID(void) { return TID_UUID; }
id CFCopyTypeIDDescription(u64 t) {
    const char *n = t == TID_STRING ? "CFString" : t == TID_NULL ? "CFNull" : t == TID_DICT ? "CFDictionary" : t == TID_ARRAY ? "CFArray"
                  : t == TID_DATA ? "CFData" : t == TID_BOOL ? "CFBoolean" : t == TID_NUMBER ? "CFNumber" : t == TID_URL ? "CFURL"
                  : t == TID_ERROR ? "CFError" : t == TID_UUID ? "CFUUID" : t == TID_DATE ? "CFDate" : "CFType";
    return objc_retain(cx_str(n));
}
u64 CFStringGetTypeID(void) { return TID_STRING; }
u64 CFDictionaryGetTypeID(void) { return TID_DICT; }
u64 CFArrayGetTypeID(void) { return TID_ARRAY; }
u64 CFDataGetTypeID(void) { return TID_DATA; }
u64 CFBooleanGetTypeID(void) { return TID_BOOL; }
u64 CFNumberGetTypeID(void) { return TID_NUMBER; }
u64 CFDateGetTypeID(void) { return TID_DATE; }
u64 CFErrorGetTypeID(void) { return TID_ERROR; }

typedef struct { cx_class_t *isa; u32 kind; u32 pad; double d; i64 i; } cfnum_t;   /* = nsnum_t de foundation.c */

static BOOL kind_of(id o, const char *cls) {
    cx_class_t *k = cx_class_named(cls);
    return k && cx_is_subclass(object_getClass(o), k);
}
/* Tipos de outros arquivos (IOSurface, CVPixelBuffer, CGColor...) respondem a -_cfTypeID */
u64 CFGetTypeID(id o) {
    if (!o) return 0;
    if (cx_is_str(o)) return TID_STRING;
    if (kind_of(o, "NSNumber")) return ((cfnum_t *)o)->kind == 2 ? TID_BOOL : TID_NUMBER;
    if (kind_of(o, "NSDictionary")) return TID_DICT;
    if (kind_of(o, "NSSet") || kind_of(o, "NSMutableSet")) return 17;
    if (kind_of(o, "NSAttributedString") || kind_of(o, "NSMutableAttributedString")) return 31;
    if (kind_of(o, "__mOSCFLocale")) return 25;
    if (kind_of(o, "NSCharacterSet")) return 45;
    if (kind_of(o, "NSArray")) return TID_ARRAY;
    if (kind_of(o, "NSData")) return TID_DATA;
    if (kind_of(o, "NSDate")) return TID_DATE;
    if (kind_of(o, "NSError")) return TID_ERROR;
    if (kind_of(o, "NSURL")) return TID_URL;
    if (kind_of(o, "NSNull")) return TID_NULL;
    if (kind_of(o, "NSUUID")) return TID_UUID;
    if (kind_of(o, "NSColor")) return 0x2A2;           /* CGColor = NSColor do mOS (coregraphics.c) */
    if (cx_responds(object_getClass(o), sel_registerName("_cfTypeID"))) return MSG0(u64, o, "_cfTypeID");
    return 1;
}

/* ---- CFBoolean / CFNumber */
u8 CFBooleanGetValue(id b) { return b && MSG0(BOOL, b, "boolValue"); }

enum { kSInt8 = 1, kSInt16, kSInt32, kSInt64, kFloat32, kFloat64, kChar, kShort, kInt, kLong, kLongLong,
       kFloat, kDouble, kCFIndex, kNSInteger, kCGFloat };
static BOOL is_float_type(long t) { return t == kFloat32 || t == kFloat64 || t == kFloat || t == kDouble || t == kCGFloat; }
id CFNumberCreate(void *alloc, long type, const void *p) {
    (void)alloc;
    if (!p) return 0;
    switch (type) {
    case kSInt8: case kChar: return cx_num_i(*(const i8 *)p);
    case kSInt16: case kShort: return cx_num_i(*(const i16 *)p);
    case kSInt32: case kInt: return cx_num_i(*(const i32 *)p);
    case kFloat32: case kFloat: return cx_num_d(*(const float *)p);
    case kFloat64: case kDouble: case kCGFloat: return cx_num_d(*(const double *)p);
    default: return cx_num_i(*(const i64 *)p);
    }
}
u8 CFNumberIsFloatType(id n) { return n && kind_of(n, "NSNumber") && ((cfnum_t *)n)->kind == 1; }
long CFNumberGetType(id n) { return CFNumberIsFloatType(n) ? kFloat64 : kSInt64; }
u8 CFNumberGetValue(id n, long type, void *out) {
    if (!n || !out) return 0;
    i64 i = MSG0(i64, n, "longLongValue");
    double d = MSG0(double, n, "doubleValue");
    switch (type) {
    case kSInt8: case kChar: *(i8 *)out = (i8)i; break;
    case kSInt16: case kShort: *(i16 *)out = (i16)i; break;
    case kSInt32: case kInt: *(i32 *)out = (i32)i; break;
    case kFloat32: case kFloat: *(float *)out = (float)d; break;
    case kFloat64: case kDouble: case kCGFloat: *(double *)out = d; break;
    default: *(i64 *)out = i; break;
    }
    /* true = sem perda (inteiro pedido de número fracionário perde) */
    return is_float_type(type) || !CFNumberIsFloatType(n) || (double)i == d;
}
long CFNumberCompare(id a, id b, void *ctx) { (void)ctx; return a && b ? MSG(long, a, "compare:", b) : 0; }

/* ---- CFDictionary (regra Create/Copy: +1) */
static id new_mdict(void) { return objc_retain(MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary")); }
static id dict_for(const void *kcb, const void *vcb) {
    if (is_cftype(kcb, 1) && is_cftype(vcb, 0) && ((const cf_cb_t *)kcb)->retain == cft_retain) return new_mdict();
    return cbd_new(kcb, vcb);
}
static int is_cbd(id d) { return d && cx_is_subclass(*(cx_class_t **)d, &cbd_c); }
id CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb) { (void)alloc; (void)cap; return dict_for(kcb, vcb); }
id CFDictionaryCreate(void *alloc, const id *keys, const id *vals, long n, const void *kcb, const void *vcb) {
    (void)alloc;
    id d = dict_for(kcb, vcb);
    for (long i = 0; i < n; i++) if (is_cbd(d) || (keys[i] && vals[i])) MSG(void, d, "setObject:forKey:", vals[i], keys[i]);
    return d;
}
id CFDictionaryCreateMutableCopy(void *alloc, long cap, id src) {
    (void)alloc; (void)cap;
    if (is_cbd(src)) return cbd_copy_with(src);
    id d = new_mdict();
    if (src) {
        nsdict_t *s = (nsdict_t *)src;
        for (u64 i = 0; i < s->n; i++) MSG(void, d, "setObject:forKey:", s->vals[i], s->keys[i]);
    }
    return d;
}
id CFDictionaryCreateCopy(void *alloc, id src) { return CFDictionaryCreateMutableCopy(alloc, 0, src); }
void CFDictionarySetValue(id d, id key, id val) {
    if (!d) return;
    if (is_cbd(d)) { cbd_set(d, 0, val, key); return; }   /* NULL e ponteiros crus valem com callbacks crus */
    if (!key) return;
    if (val) MSG(void, d, "setObject:forKey:", val, key);
    else MSG(void, d, "removeObjectForKey:", key);
}
void CFDictionaryAddValue(id d, id key, id val) {
    if (!d) return;
    if (is_cbd(d)) { if (cbd_find(D(d), key) < 0) cbd_set(d, 0, val, key); return; }
    if (key && val && !MSG(id, d, "objectForKey:", key)) MSG(void, d, "setObject:forKey:", val, key);
}
void CFDictionaryReplaceValue(id d, id key, id val) {
    if (!d) return;
    if (is_cbd(d)) { if (cbd_find(D(d), key) >= 0) cbd_set(d, 0, val, key); return; }
    if (key && val && MSG(id, d, "objectForKey:", key)) MSG(void, d, "setObject:forKey:", val, key);
}
void CFDictionaryRemoveValue(id d, id key) { if (d && (key || is_cbd(d))) MSG(void, d, "removeObjectForKey:", key); }
void CFDictionaryRemoveAllValues(id d) { if (d) MSG0(void, d, "removeAllObjects"); }
void CFDictionaryGetKeysAndValues(id d, id *keys, id *vals) {
    if (!d) return;
    nsdict_t *s = (nsdict_t *)d;
    for (u64 i = 0; i < s->n; i++) { if (keys) keys[i] = s->keys[i]; if (vals) vals[i] = s->vals[i]; }
}
void CFDictionaryApplyFunction(id d, void (*fn)(id, id, void *), void *ctx) {
    if (!d || !fn) return;
    nsdict_t *s = (nsdict_t *)d;
    u64 n = s->n;
    id *k = malloc((n ? n : 1) * sizeof(id) * 2);   /* cópia: o callback pode mexer no dicionário */
    if (!k) return;
    for (u64 i = 0; i < n; i++) { k[i] = s->keys[i]; k[n + i] = s->vals[i]; }
    for (u64 i = 0; i < n; i++) fn(k[i], k[n + i], ctx);
    free(k);
}

/* ---- CFArray */
static id new_marr(void) { return objc_retain(MSG0(id, (id)cx_class_named("NSMutableArray"), "array")); }
static id array_for(const void *cb) { return is_cftype(cb, 0) && ((const cf_cb_t *)cb)->retain == cft_retain ? new_marr() : cba_new(cb); }
static int is_cba(id a) { return a && cx_is_subclass(*(cx_class_t **)a, &cba_c); }
id CFArrayCreateMutable(void *alloc, long cap, const void *cb) { (void)alloc; (void)cap; return array_for(cb); }
id CFArrayCreate(void *alloc, const id *vals, long n, const void *cb) {
    (void)alloc;
    id a = array_for(cb);
    for (long i = 0; i < n; i++) if (is_cba(a) || vals[i]) MSG(void, a, "addObject:", vals[i]);
    return a;
}
id CFArrayCreateMutableCopy(void *alloc, long cap, id src) {
    (void)alloc; (void)cap;
    if (is_cba(src)) return cba_copy_with(src, 1);
    id a = new_marr();
    if (src) MSG(void, a, "addObjectsFromArray:", src);
    return a;
}
id CFArrayCreateCopy(void *alloc, id src) { return CFArrayCreateMutableCopy(alloc, 0, src); }
void CFArrayAppendValue(id a, id v) { if (a && (v || is_cba(a))) MSG(void, a, "addObject:", v); }
void CFArrayAppendArray(id a, id src, long loc, long len) {
    if (!a || !src) return;
    for (long i = 0; i < len; i++) { id o = MSG(id, src, "objectAtIndex:", (u64)(loc + i)); MSG(void, a, "addObject:", o); }
}
void CFArrayInsertValueAtIndex(id a, long i, id v) { if (a && (v || is_cba(a))) MSG(void, a, "insertObject:atIndex:", v, (u64)i); }
void CFArraySetValueAtIndex(id a, long i, id v) {
    if (!a || (!v && !is_cba(a))) return;
    if ((u64)i == MSG0(u64, a, "count")) MSG(void, a, "addObject:", v);
    else MSG(void, a, "replaceObjectAtIndex:withObject:", (u64)i, v);
}
void CFArrayRemoveValueAtIndex(id a, long i) { if (a) MSG(void, a, "removeObjectAtIndex:", (u64)i); }
void CFArrayRemoveAllValues(id a) { if (a) MSG0(void, a, "removeAllObjects"); }
void CFArrayApplyFunction(id a, long loc, long len, void (*fn)(id, void *), void *ctx) {
    if (!a || !fn) return;
    for (long i = 0; i < len; i++) fn(MSG(id, a, "objectAtIndex:", (u64)(loc + i)), ctx);
}
u8 CFArrayContainsValue(id a, long loc, long len, id v) {
    if (!a) return 0;
    for (long i = 0; i < len; i++) { id o = MSG(id, a, "objectAtIndex:", (u64)(loc + i)); if (cx_cfarray_equal(a, o, v)) return 1; }
    return 0;
}

/* ---- buscas e ordem pelos callbacks (ponteiros crus não recebem retain nem isEqual:) */
typedef long (*cf_cmp_fn)(const void *, const void *, void *);
typedef struct { long loc, len; } cfr_t;
static long arr_count(id a) { return a ? (long)MSG0(u64, a, "count") : 0; }
static const void *arr_at(id a, long i) { return MSG(id, a, "objectAtIndex:", (u64)i); }
/* o vetor de dentro, quando o layout é o nosso (NSArray do libSystem ou __NSCFArrayCB) */
static const void **arr_items(id a) {
    cx_class_t *k = *(cx_class_t **)a, *base = cx_class_named("NSArray");
    if (is_cba(a) || k == base || k == cx_class_named("NSMutableArray")) return (const void **)((nsarr_t *)a)->items;
    return 0;
}
long CFArrayGetFirstIndexOfValue(id a, cfr_t r, const void *v) {
    long n = arr_count(a);
    for (long i = r.loc < 0 ? 0 : r.loc; i < r.loc + r.len && i < n; i++) if (cx_cfarray_equal(a, arr_at(a, i), v)) return i;
    return -1;
}
long CFArrayGetLastIndexOfValue(id a, cfr_t r, const void *v) {
    long n = arr_count(a);
    for (long i = (r.loc + r.len < n ? r.loc + r.len : n) - 1; i >= r.loc && i >= 0; i--) if (cx_cfarray_equal(a, arr_at(a, i), v)) return i;
    return -1;
}
long CFArrayGetCountOfValue(id a, cfr_t r, const void *v) {
    long n = arr_count(a), k = 0;
    for (long i = r.loc < 0 ? 0 : r.loc; i < r.loc + r.len && i < n; i++) if (cx_cfarray_equal(a, arr_at(a, i), v)) k++;
    return k;
}
void CFArrayGetValues(id a, cfr_t r, const void **out) {
    long n = arr_count(a), k = 0;
    if (!out) return;
    for (long i = r.loc; i < r.loc + r.len && i < n; i++) out[k++] = arr_at(a, i);
}
/* posição onde v entraria na faixa ordenada (o CF devolve o índice do igual ou do primeiro maior) */
long CFArrayBSearchValues(id a, cfr_t r, const void *v, cf_cmp_fn cmp, void *ctx) {
    long lo = r.loc, hi = r.loc + r.len;
    if (!a || !cmp) return lo;
    while (lo < hi) {
        long m = lo + (hi - lo) / 2;
        long c = cmp(arr_at(a, m), v, ctx);
        if (c < 0) lo = m + 1; else hi = m;
    }
    return lo;
}
void CFArrayExchangeValuesAtIndices(id a, long i, long j) {
    if (!a || i == j) return;
    const void **it = arr_items(a);
    long n = arr_count(a);
    if (i < 0 || j < 0 || i >= n || j >= n) return;
    if (it) { const void *t = it[i]; it[i] = it[j]; it[j] = t; return; }
    MSG(void, a, "exchangeObjectAtIndex:withObjectAtIndex:", (u64)i, (u64)j);
}
/* estável (inserção binária), troca só a ordem dos ponteiros: nada de retain/release */
void CFArraySortValues(id a, cfr_t r, cf_cmp_fn cmp, void *ctx) {
    if (!a || !cmp || r.len <= 1) return;
    const void **it = arr_items(a);
    long n = arr_count(a);
    if (r.loc < 0 || r.loc + r.len > n) return;
    const void **v = it ? it + r.loc : malloc((u64)r.len * sizeof(void *));
    if (!it) for (long i = 0; i < r.len; i++) v[i] = objc_retain((id)arr_at(a, r.loc + i));
    for (long i = 1; i < r.len; i++) {
        const void *x = v[i];
        long lo = 0, hi = i;
        while (lo < hi) { long m = (lo + hi) / 2; if (cmp(v[m], x, ctx) <= 0) lo = m + 1; else hi = m; }
        memmove(v + lo + 1, v + lo, (u64)(i - lo) * sizeof(void *));
        v[lo] = x;
    }
    if (!it) {
        for (long i = 0; i < r.len; i++) { MSG(void, a, "replaceObjectAtIndex:withObject:", (u64)(r.loc + i), (id)v[i]); objc_release((id)v[i]); }
        free(v);
    }
}
u8 CFDictionaryGetValueIfPresent(id d, const void *k, const void **out) {
    if (!d) return 0;
    if (is_cbd(d)) {
        long i = cbd_find(D(d), k);
        if (i >= 0 && out) *out = D(d)->vals[i];
        return i >= 0;
    }
    id v = k ? MSG(id, d, "objectForKey:", (id)k) : 0;
    if (v && out) *out = v;
    return v != 0;
}
u8 CFDictionaryContainsKey(id d, const void *k) { return CFDictionaryGetValueIfPresent(d, k, 0); }
long CFDictionaryGetCountOfKey(id d, const void *k) { return CFDictionaryContainsKey(d, k); }
long CFDictionaryGetCountOfValue(id d, const void *v) {
    if (!d) return 0;
    nsdict_t *s = (nsdict_t *)d;
    long k = 0;
    for (u64 i = 0; i < s->n; i++) if (cx_cfdict_value_equal(d, s->vals[i], v)) k++;
    return k;
}
u8 CFDictionaryContainsValue(id d, const void *v) { return CFDictionaryGetCountOfValue(d, v) > 0; }
