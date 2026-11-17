/*
 * mOS libSystem — NSSet, NSMutableSet e NSCountedSet com tabela hash (antes NSSet era subclasse de NSArray e
 * -member: devolvia o primeiro elemento). Também os CFSet com callbacks: CFSetCreate(…, NULL) guarda ponteiros
 * crus (identidade, sem retain), e callbacks próprios (retain/release/equal/hash) são respeitados, como no CF.
 *
 * Layout: os primeiros campos são os de nsarr_t {isa, items, n, cap}, com os elementos densos em items; a
 * tabela (endereçamento aberto, sondagem linear, remoção por deslocamento) guarda índice+1 em items.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void cx_object_free(id o);
void cx_kvo_forget(id o);
id cx_enumerator_new(id array);
IMP cx_lookup(cx_class_t *cls, SEL sel);

typedef struct cfset_cb {
    long version;
    const void *(*retain)(const void *alloc, const void *v);
    void (*release)(const void *alloc, const void *v);
    id (*copy_desc)(const void *v);
    unsigned char (*equal)(const void *a, const void *b);
    u64 (*hash)(const void *v);
} cfset_cb_t;

#define S(o) ((nsset_t *)(o))

extern cx_class_t NSSet_c __asm__("OBJC_CLASS_$_NSSet");
extern cx_class_t NSMutableSet_c __asm__("OBJC_CLASS_$_NSMutableSet");
cx_class_t NSCountedSet_c __asm__("OBJC_CLASS_$_NSCountedSet"), NSCountedSet_m __asm__("OBJC_METACLASS_$_NSCountedSet");

static id cls(const char *n) { return (id)cx_class_named(n); }
static void *blk_inv(void *b) { return *(void **)((u8 *)b + 16); }

/* ---------------------------------------------------------------- tabela */
static u64 h_of(nsset_t *s, id o) {
    if (s->cb) return s->cb->hash ? s->cb->hash(o) : (u64)o >> 3;
    return MSG0(u64, o, "hash");
}
static int eq_of(nsset_t *s, id a, id b) {
    if (a == b) return 1;
    if (s->cb) return s->cb->equal ? s->cb->equal(a, b) != 0 : 0;
    return a && b && MSG(BOOL, a, "isEqual:", b);
}
/* o callback de retain do CF devolve o valor que fica guardado (kCFCopyStringSetCallBacks guarda uma cópia) */
static id keep(nsset_t *s, id o) {
    if (s->cb) return s->cb->retain ? (id)s->cb->retain(0, o) : o;
    return objc_retain(o);
}
static void drop(nsset_t *s, id o) {
    if (s->cb) { if (s->cb->release) s->cb->release(0, o); }
    else objc_release(o);
}
static void rehash(nsset_t *s, u64 want) {
    u64 cap = 16;
    while (cap < want * 2) cap <<= 1;
    free(s->slots);
    s->slots = calloc(cap, sizeof(u64));
    s->scap = cap;
    for (u64 i = 0; i < s->n; i++) {
        u64 k = h_of(s, s->items[i]) & (cap - 1);
        while (s->slots[k]) k = (k + 1) & (cap - 1);
        s->slots[k] = i + 1;
    }
}
/* devolve a posição na tabela (com o elemento ou a vaga onde ele entraria) */
static u64 probe(nsset_t *s, id o, int *found) {
    u64 m = s->scap - 1, k = h_of(s, o) & m;
    while (s->slots[k]) {
        if (eq_of(s, s->items[s->slots[k] - 1], o)) { *found = 1; return k; }
        k = (k + 1) & m;
    }
    *found = 0;
    return k;
}
static long find(nsset_t *s, id o) {
    if (!s->n || !s->scap) return -1;
    int f;
    u64 k = probe(s, o, &f);
    return f ? (long)(s->slots[k] - 1) : -1;
}
static void slot_clear(nsset_t *s, u64 k) {
    u64 m = s->scap - 1, j = k;
    s->slots[k] = 0;
    for (;;) {
        j = (j + 1) & m;
        if (!s->slots[j]) return;
        u64 want = h_of(s, s->items[s->slots[j] - 1]) & m;
        /* j fica onde está se 'want' estiver no intervalo circular (k, j] */
        if (k <= j ? (k < want && want <= j) : (k < want || want <= j)) continue;
        s->slots[k] = s->slots[j];
        s->slots[j] = 0;
        k = j;
    }
}
static void set_add(nsset_t *s, id o, u64 times) {
    if (!o && !s->cb) return;
    if (s->n + 1 > s->scap / 2) rehash(s, s->n + 1);
    int f;
    u64 k = probe(s, o, &f);
    if (f) { if (s->counts) s->counts[s->slots[k] - 1] += times; return; }
    if (s->n == s->cap) {
        u64 nc = s->cap ? s->cap * 2 : 8;
        s->items = realloc(s->items, nc * sizeof(id));
        if (s->counts || s->isa == &NSCountedSet_c) s->counts = realloc(s->counts, nc * sizeof(u64));
        s->cap = nc;
    }
    s->items[s->n] = keep(s, o);
    if (s->counts) s->counts[s->n] = times;
    s->slots[k] = ++s->n;
    s->mut++;
}
static void set_remove_at(nsset_t *s, u64 idx) {
    int f;
    id o = s->items[idx];
    u64 k = probe(s, o, &f);
    if (!f) return;
    slot_clear(s, k);
    u64 last = s->n - 1;
    if (idx != last) {
        /* o último vai para o buraco: corrige a vaga dele na tabela */
        u64 m = s->scap - 1, j = h_of(s, s->items[last]) & m;
        while (s->slots[j] != last + 1) j = (j + 1) & m;
        s->slots[j] = idx + 1;
        s->items[idx] = s->items[last];
        if (s->counts) s->counts[idx] = s->counts[last];
    }
    s->n--;
    s->mut++;
    drop(s, o);
}
static void set_remove(nsset_t *s, id o, int all) {
    long i = find(s, o);
    if (i < 0) return;
    if (s->counts && !all && s->counts[i] > 1) { s->counts[i]--; return; }
    set_remove_at(s, (u64)i);
}
static void set_clear(nsset_t *s) {
    for (u64 i = 0; i < s->n; i++) drop(s, s->items[i]);
    s->n = 0;
    if (s->slots) memset(s->slots, 0, s->scap * sizeof(u64));
    s->mut++;
}

/* elementos de qualquer coleção (NSArray, NSSet, NSOrderedSet, de outra implementação também) */
static id all_of(id c) {
    if (!c) return 0;
    return MSG(BOOL, c, "respondsToSelector:", sel_registerName("objectAtIndex:")) ? c : MSG0(id, c, "allObjects");
}
static void add_collection(nsset_t *s, id c) {
    if (!c) return;
    if (cx_is_subclass(*(cx_class_t **)c, &NSSet_c)) {
        nsset_t *o = S(c);
        for (u64 i = 0; i < o->n; i++) set_add(s, o->items[i], o->counts && s->counts ? o->counts[i] : 1);
        return;
    }
    id a = all_of(c);
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) set_add(s, MSG(id, a, "objectAtIndex:", i), 1);
}

/* ---------------------------------------------------------------- criação */
static id new_set(cx_class_t *k) {
    nsset_t *s = (nsset_t *)cx_alloc_instance(k);
    if (k == &NSCountedSet_c || cx_is_subclass(k, &NSCountedSet_c)) s->counts = calloc(1, sizeof(u64));
    return (id)s;
}
static id st_alloc(id k, SEL c) { (void)c; return new_set((cx_class_t *)k); }
static id st_init(id s, SEL c) { (void)c; return s; }
static id st_init_cap(id s, SEL c, u64 n) { (void)c; if (n) rehash(S(s), n); return s; }
static id st_init_objs_count(id s, SEL c, const id *v, u64 n) { (void)c; for (u64 i = 0; v && i < n; i++) set_add(S(s), v[i], 1); return s; }
static id st_init_objs(id s, SEL c, id first, ...) {
    (void)c;
    va_list ap;
    va_start(ap, first);
    for (id o = first; o; o = va_arg(ap, id)) set_add(S(s), o, 1);
    va_end(ap);
    return s;
}
static id st_init_coll(id s, SEL c, id o) { (void)c; add_collection(S(s), o); return s; }
static id st_init_set_copy(id s, SEL c, id o, BOOL deep) {
    (void)c;
    if (!deep || !o) { add_collection(S(s), o); return s; }
    id a = MSG0(id, o, "allObjects");
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) {
        id src = MSG(id, a, "objectAtIndex:", i);
        id e = MSG(id, src, "copyWithZone:", (void *)0);
        set_add(S(s), e, 1);
        objc_release(e);
    }
    return s;
}
static id ar(id o) { return objc_autorelease(o); }
static id c_set(id k, SEL c) { (void)c; return ar(new_set((cx_class_t *)k)); }
static id c_cap(id k, SEL c, u64 n) { return ar(st_init_cap(new_set((cx_class_t *)k), c, n)); }
static id c_obj(id k, SEL c, id o) { id s = new_set((cx_class_t *)k); set_add(S(s), o, 1); (void)c; return ar(s); }
static id c_objs_count(id k, SEL c, const id *v, u64 n) { return ar(st_init_objs_count(new_set((cx_class_t *)k), c, v, n)); }
static id c_objs(id k, SEL c, id first, ...) {
    (void)c;
    id s = new_set((cx_class_t *)k);
    va_list ap;
    va_start(ap, first);
    for (id o = first; o; o = va_arg(ap, id)) set_add(S(s), o, 1);
    va_end(ap);
    return ar(s);
}
static id c_coll(id k, SEL c, id o) { return ar(st_init_coll(new_set((cx_class_t *)k), c, o)); }

static void st_dealloc(id s, SEL c) {
    (void)c;
    nsset_t *t = S(s);
    for (u64 i = 0; i < t->n; i++) drop(t, t->items[i]);
    free(t->items); free(t->slots); free(t->counts); free(t->cb);
    cx_kvo_forget(s);
    cx_object_free(s);
}

/* ---------------------------------------------------------------- consulta */
static u64 st_count(id s, SEL c) { (void)c; return S(s)->n; }
static id st_member(id s, SEL c, id o) { (void)c; long i = find(S(s), o); return i < 0 ? 0 : S(s)->items[i]; }
static BOOL st_contains(id s, SEL c, id o) { (void)c; return find(S(s), o) >= 0; }
static id st_any(id s, SEL c) { (void)c; return S(s)->n ? S(s)->items[0] : 0; }
static id st_all(id s, SEL c) {
    (void)c;
    id a = cx_arr_new(0);
    for (u64 i = 0; i < S(s)->n; i++) cx_arr_add(a, S(s)->items[i]);
    return ar(a);
}
static id st_enum(id s, SEL c) { return cx_enumerator_new(st_all(s, c)); }
typedef struct { u64 state; id *items; u64 *mut; u64 extra[5]; } fastenum_t;
static u64 st_fast(id s, SEL c, fastenum_t *st, id *buf, u64 len) {
    (void)c; (void)buf; (void)len;
    if (st->state) return 0;
    st->state = 1;
    st->items = S(s)->items;
    st->mut = &S(s)->mut;
    return S(s)->n;
}
static void st_enum_block(id s, SEL c, void *blk) {
    (void)c;
    void (*inv)(void *, id, BOOL *) = blk_inv(blk);
    BOOL stop = NO;
    nsset_t *t = S(s);
    for (u64 i = 0; i < t->n && !stop; i++) inv(blk, t->items[i], &stop);
}
static void st_enum_opts(id s, SEL c, u64 opts, void *blk) { (void)opts; st_enum_block(s, c, blk); }
static id st_passing(id s, SEL c, void *blk) {
    (void)c;
    BOOL (*inv)(void *, id, BOOL *) = blk_inv(blk);
    id r = new_set(&NSSet_c);
    BOOL stop = NO;
    for (u64 i = 0; i < S(s)->n && !stop; i++) if (inv(blk, S(s)->items[i], &stop)) set_add(S(r), S(s)->items[i], 1);
    return ar(r);
}
static id st_passing_opts(id s, SEL c, u64 opts, void *blk) { (void)opts; return st_passing(s, c, blk); }
static BOOL st_intersects(id s, SEL c, id o) {
    (void)c;
    if (!o) return NO;
    id a = cx_is_subclass(*(cx_class_t **)o, &NSSet_c) ? 0 : all_of(o);
    if (!a) { for (u64 i = 0; i < S(o)->n; i++) if (find(S(s), S(o)->items[i]) >= 0) return YES; return NO; }
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) if (find(S(s), MSG(id, a, "objectAtIndex:", i)) >= 0) return YES;
    return NO;
}
static BOOL st_subset(id s, SEL c, id o) {
    (void)c;
    if (!o) return S(s)->n == 0;
    for (u64 i = 0; i < S(s)->n; i++) if (!MSG(BOOL, o, "containsObject:", S(s)->items[i])) return NO;
    return YES;
}
static BOOL st_equal_set(id s, SEL c, id o) {
    if (o == s) return YES;
    if (!o || MSG0(u64, o, "count") != S(s)->n) return NO;
    return st_subset(s, c, o);
}
static BOOL st_is_equal(id s, SEL c, id o) {
    if (o == s) return YES;
    if (!o || !MSG(BOOL, o, "isKindOfClass:", (id)&NSSet_c)) return NO;
    return st_equal_set(s, c, o);
}
static u64 st_hash(id s, SEL c) { (void)c; return S(s)->n; }
static void st_perform(id s, SEL c, SEL sel) {
    (void)c;
    id a = st_all(s, c);
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) ((void (*)(id, SEL))objc_msgSend)(MSG(id, a, "objectAtIndex:", i), sel);
}
static void st_perform_with(id s, SEL c, SEL sel, id arg) {
    id a = st_all(s, c);
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) ((void (*)(id, SEL, id))objc_msgSend)(MSG(id, a, "objectAtIndex:", i), sel, arg);
}
static id st_copy_into(id s, cx_class_t *k) {
    id r = new_set(k);
    nsset_t *t = S(s);
    if (t->cb) { S(r)->cb = malloc(sizeof(cfset_cb_t)); *S(r)->cb = *t->cb; }
    for (u64 i = 0; i < t->n; i++) set_add(S(r), t->items[i], t->counts && S(r)->counts ? t->counts[i] : 1);
    return r;
}
static id st_adding(id s, SEL c, id o) { (void)c; id r = st_copy_into(s, &NSSet_c); set_add(S(r), o, 1); return ar(r); }
static id st_adding_coll(id s, SEL c, id o) { (void)c; id r = st_copy_into(s, &NSSet_c); add_collection(S(r), o); return ar(r); }
static id st_copy(id s, SEL c) {
    (void)c;
    cx_class_t *k = *(cx_class_t **)s;
    if (k == &NSSet_c) return objc_retain(s);  /* imutável: a cópia é ele mesmo */
    return st_copy_into(s, k == &NSCountedSet_c ? &NSCountedSet_c : &NSSet_c);
}
static id st_copy_zone(id s, SEL c, void *z) { (void)z; return st_copy(s, c); }
static id st_mcopy(id s, SEL c) {
    (void)c;
    return st_copy_into(s, *(cx_class_t **)s == &NSCountedSet_c ? &NSCountedSet_c : &NSMutableSet_c);
}
static id st_mcopy_zone(id s, SEL c, void *z) { (void)z; return st_mcopy(s, c); }
static id st_filtered(id s, SEL c, id pred) {
    (void)c;
    id r = new_set(&NSSet_c);
    for (u64 i = 0; i < S(s)->n; i++)
        if (pred && MSG(BOOL, pred, "evaluateWithObject:", S(s)->items[i])) set_add(S(r), S(s)->items[i], 1);
    return ar(r);
}
static id st_sorted_desc(id s, SEL c, id descs) { return MSG(id, st_all(s, c), "sortedArrayUsingDescriptors:", descs); }
/* KVC: o conjunto dos valores; @count e os operadores ficam com o NSArray */
static id st_value_for_key(id s, SEL c, id key) {
    const char *k = key ? cx_cstr(key) : "";
    if (k[0] == '@') return MSG(id, st_all(s, c), "valueForKeyPath:", key);
    id r = new_set(&NSSet_c);
    for (u64 i = 0; i < S(s)->n; i++) {
        id v = MSG(id, S(s)->items[i], "valueForKey:", key);
        set_add(S(r), v ? v : MSG0(id, cls("NSNull"), "null"), 1);
    }
    return ar(r);
}
static void st_set_value_for_key(id s, SEL c, id v, id key) {
    (void)c;
    for (u64 i = 0; i < S(s)->n; i++) MSG(void, S(s)->items[i], "setValue:forKey:", v, key);
}
static id st_desc_loc(id s, SEL c, id loc) {
    (void)c;
    id out = MSG0(id, MSG0(id, cls("NSMutableString"), "alloc"), "init");
    MSG(void, out, "appendString:", cx_str("{(\n"));
    for (u64 i = 0; i < S(s)->n; i++) {
        id e = S(s)->items[i];
        id d = MSG(BOOL, e, "respondsToSelector:", sel_registerName("descriptionWithLocale:"))
                   ? MSG(id, e, "descriptionWithLocale:", loc) : MSG0(id, e, "description");
        MSG(void, out, "appendString:", cx_str("    "));
        if (d) MSG(void, out, "appendString:", d);
        MSG(void, out, "appendString:", cx_str(i + 1 < S(s)->n ? ",\n" : "\n"));
    }
    MSG(void, out, "appendString:", cx_str(")}"));
    return ar(out);
}
static id st_desc(id s, SEL c) { return st_desc_loc(s, c, 0); }
static void st_encode(id s, SEL c, id coder) {
    MSG(void, coder, "encodeObject:forKey:", st_all(s, c), cx_str("NS.objects"));
}
static id st_decode(id s, SEL c, id coder) {
    (void)c;
    add_collection(S(s), MSG(id, coder, "decodeObjectForKey:", cx_str("NS.objects")));
    return s;
}
static BOOL yes(id s, SEL c) { (void)s; (void)c; return YES; }

/* ---------------------------------------------------------------- mutáveis */
static void ms_add(id s, SEL c, id o) { (void)c; set_add(S(s), o, 1); }
static void ms_remove(id s, SEL c, id o) { (void)c; set_remove(S(s), o, 0); }
static void ms_add_array(id s, SEL c, id a) { (void)c; add_collection(S(s), a); }
static void ms_union(id s, SEL c, id o) { (void)c; add_collection(S(s), o); }
static void ms_minus(id s, SEL c, id o) {
    (void)c;
    if (!o) return;
    if (o == s) { set_clear(S(s)); return; }
    id a = MSG0(id, o, "allObjects");
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) set_remove(S(s), MSG(id, a, "objectAtIndex:", i), 1);
}
static void ms_intersect(id s, SEL c, id o) {
    (void)c;
    if (o == s) return;
    for (u64 i = S(s)->n; i-- > 0;) if (!o || !MSG(BOOL, o, "containsObject:", S(s)->items[i])) set_remove_at(S(s), i);
}
static void ms_set_set(id s, SEL c, id o) {
    if (o == s) return;
    id keep_alive = o ? objc_retain(o) : 0;
    set_clear(S(s));
    add_collection(S(s), o);
    (void)c;
    if (keep_alive) objc_release(keep_alive);
}
static void ms_remove_all(id s, SEL c) { (void)c; set_clear(S(s)); }
static void ms_filter(id s, SEL c, id pred) {
    (void)c;
    for (u64 i = S(s)->n; i-- > 0;) if (!pred || !MSG(BOOL, pred, "evaluateWithObject:", S(s)->items[i])) set_remove_at(S(s), i);
}

/* ---------------------------------------------------------------- NSCountedSet */
static u64 cs_count_for(id s, SEL c, id o) { (void)c; long i = find(S(s), o); return i < 0 || !S(s)->counts ? (i < 0 ? 0 : 1) : S(s)->counts[i]; }
static id cs_desc(id s, SEL c) {
    (void)c;
    id out = MSG0(id, MSG0(id, cls("NSMutableString"), "alloc"), "init");
    char b[48];
    snprintf(b, sizeof b, "<NSCountedSet: %p> (", (void *)s);
    MSG(void, out, "appendString:", cx_str(b));
    for (u64 i = 0; i < S(s)->n; i++) {
        id d = MSG0(id, S(s)->items[i], "description");
        if (d) MSG(void, out, "appendString:", d);
        snprintf(b, sizeof b, " [%llu]%s", (unsigned long long)(S(s)->counts ? S(s)->counts[i] : 1), i + 1 < S(s)->n ? ", " : "");
        MSG(void, out, "appendString:", cx_str(b));
    }
    MSG(void, out, "appendString:", cx_str(")"));
    return ar(out);
}

/* ---------------------------------------------------------------- CFSet (com callbacks) */
/* kCFTypeSetCallBacks e kCFCopyStringSetCallBacks com as funções de verdade: apps copiam a struct e trocam um
 * campo (o caso clássico: retain/release NULL para um conjunto que não segura os objetos) */
static const void *cb_retain(const void *a, const void *v) { (void)a; return objc_retain((id)v); }
static void cb_release(const void *a, const void *v) { (void)a; objc_release((id)v); }
static id cb_desc(const void *v) { return v ? objc_retain(MSG0(id, (id)v, "description")) : 0; }
static unsigned char cb_equal(const void *a, const void *b) { return a == b || (a && b && MSG(BOOL, (id)a, "isEqual:", (id)b)); }
static u64 cb_hash(const void *v) { return v ? MSG0(u64, (id)v, "hash") : 0; }
static const void *cb_copy_str(const void *a, const void *v) { (void)a; return v ? (const void *)MSG(id, (id)v, "copyWithZone:", (void *)0) : 0; }
const cfset_cb_t kCFTypeSetCallBacks = {0, cb_retain, cb_release, cb_desc, cb_equal, cb_hash};
const cfset_cb_t kCFCopyStringSetCallBacks = {0, cb_copy_str, cb_release, cb_desc, cb_equal, cb_hash};
static const cfset_cb_t *cf_type_cb = &kCFTypeSetCallBacks;
static id cfset_new(cx_class_t *k, const cfset_cb_t *cb) {
    id s = new_set(k);
    /* kCFTypeSetCallBacks (ou kCFCopyStringSetCallBacks) = objetos: o caminho do NSSet. Outros: guarda a cópia */
    if (cb != cf_type_cb && !(cb && cb->retain == cb_retain && cb->release == cb_release && cb->equal == cb_equal && cb->hash == cb_hash)) {
        S(s)->cb = calloc(1, sizeof(cfset_cb_t));
        if (cb) *S(s)->cb = *cb;
    }
    return s;
}
id CFSetCreateMutable(void *alloc, long cap, const cfset_cb_t *cb) { (void)alloc; id s = cfset_new(&NSMutableSet_c, cb); if (cap > 0) rehash(S(s), (u64)cap); return s; }
id CFSetCreate(void *alloc, const id *vals, long n, const cfset_cb_t *cb) {
    (void)alloc;
    id s = cfset_new(&NSSet_c, cb);
    for (long i = 0; vals && i < n; i++) set_add(S(s), vals[i], 1);
    return s;
}
static int is_ours(id s) { return s && cx_is_subclass(*(cx_class_t **)s, &NSSet_c); }
long CFSetGetCount(id s) { return !s ? 0 : is_ours(s) ? (long)S(s)->n : (long)MSG0(u64, s, "count"); }
u64 CFSetGetTypeID(void) { return 17; }
unsigned char CFSetContainsValue(id s, id v) { return !s ? 0 : is_ours(s) ? find(S(s), v) >= 0 : MSG(BOOL, s, "containsObject:", v); }
id CFSetGetValue(id s, id v) {
    if (!s) return 0;
    if (!is_ours(s)) return MSG(id, s, "member:", v);
    long i = find(S(s), v);
    return i < 0 ? 0 : S(s)->items[i];
}
unsigned char CFSetGetValueIfPresent(id s, id v, id *out) {
    long i = s && is_ours(s) ? find(S(s), v) : -1;
    id g = i >= 0 ? S(s)->items[i] : s && !is_ours(s) ? MSG(id, s, "member:", v) : 0;
    if (out) *out = g;
    return i >= 0 || g != 0;
}
long CFSetGetCountOfValue(id s, id v) {
    if (!s) return 0;
    if (!is_ours(s)) return MSG(BOOL, s, "containsObject:", v) ? 1 : 0;
    long i = find(S(s), v);
    return i < 0 ? 0 : S(s)->counts ? (long)S(s)->counts[i] : 1;
}
void CFSetGetValues(id s, id *out) {
    if (!s || !out) return;
    if (is_ours(s)) { for (u64 i = 0; i < S(s)->n; i++) out[i] = S(s)->items[i]; return; }
    id a = MSG0(id, s, "allObjects");
    u64 n = MSG0(u64, a, "count");
    for (u64 i = 0; i < n; i++) out[i] = MSG(id, a, "objectAtIndex:", i);
}
void CFSetApplyFunction(id s, void (*fn)(id, void *), void *ctx) {
    if (!s || !fn) return;
    long n = CFSetGetCount(s);
    id *v = n > 0 ? malloc((size_t)n * sizeof(id)) : 0;
    if (!v) return;
    CFSetGetValues(s, v);
    for (long i = 0; i < n; i++) fn(v[i], ctx);
    free(v);
}
void CFSetAddValue(id s, id v) { if (s) { if (is_ours(s)) set_add(S(s), v, 1); else MSG(void, s, "addObject:", v); } }
void CFSetRemoveValue(id s, id v) { if (s) { if (is_ours(s)) set_remove(S(s), v, 1); else MSG(void, s, "removeObject:", v); } }
void CFSetRemoveAllValues(id s) { if (s) { if (is_ours(s)) set_clear(S(s)); else MSG0(void, s, "removeAllObjects"); } }
/* Replace: só troca se existir; Set: troca ou acrescenta (o valor novo entra no lugar do igual) */
void CFSetReplaceValue(id s, id v) { if (s && CFSetContainsValue(s, v)) { CFSetRemoveValue(s, v); CFSetAddValue(s, v); } }
void CFSetSetValue(id s, id v) { if (!s) return; CFSetRemoveValue(s, v); CFSetAddValue(s, v); }
id CFSetCreateCopy(void *alloc, id s) {
    (void)alloc;
    if (!s) return 0;
    if (is_ours(s)) return st_copy_into(s, &NSSet_c);
    return st_init_coll(cfset_new(&NSSet_c, cf_type_cb), 0, s);
}
id CFSetCreateMutableCopy(void *alloc, long cap, id s) {
    (void)alloc; (void)cap;
    if (s && is_ours(s)) return st_copy_into(s, &NSMutableSet_c);
    return st_init_coll(cfset_new(&NSMutableSet_c, cf_type_cb), 0, s);
}

/* ---------------------------------------------------------------- registro */
void cx_fdn_set_init(void) {
    cx_class_t *ns = &NSSet_c, *ms = &NSMutableSet_c;
    if (cx_class_named("NSCountedSet") == &NSCountedSet_c) return;
    cx_define_class(&NSCountedSet_c, &NSCountedSet_m, "NSCountedSet", ms, 0);
#define M(k, s, f) cx_add_method(k, s, (void *)(f))
#define CM(k, s, f) cx_add_cmethod(k, s, (void *)(f))
    CM(ns, "alloc", st_alloc); CM(ns, "allocWithZone:", st_alloc);
    CM(ns, "set", c_set); CM(ns, "setWithObject:", c_obj); CM(ns, "setWithObjects:count:", c_objs_count);
    CM(ns, "setWithObjects:", c_objs); CM(ns, "setWithArray:", c_coll); CM(ns, "setWithSet:", c_coll);
    CM(ns, "supportsSecureCoding", yes);
    M(ns, "init", st_init); M(ns, "initWithObjects:count:", st_init_objs_count); M(ns, "initWithObjects:", st_init_objs);
    M(ns, "initWithArray:", st_init_coll); M(ns, "initWithSet:", st_init_coll); M(ns, "initWithSet:copyItems:", st_init_set_copy);
    M(ns, "dealloc", st_dealloc);
    M(ns, "count", st_count); M(ns, "member:", st_member); M(ns, "containsObject:", st_contains);
    M(ns, "anyObject", st_any); M(ns, "allObjects", st_all); M(ns, "objectEnumerator", st_enum);
    M(ns, "countByEnumeratingWithState:objects:count:", st_fast);
    M(ns, "enumerateObjectsUsingBlock:", st_enum_block); M(ns, "enumerateObjectsWithOptions:usingBlock:", st_enum_opts);
    M(ns, "objectsPassingTest:", st_passing); M(ns, "objectsWithOptions:passingTest:", st_passing_opts);
    M(ns, "intersectsSet:", st_intersects); M(ns, "isSubsetOfSet:", st_subset); M(ns, "isEqualToSet:", st_equal_set);
    M(ns, "isEqual:", st_is_equal); M(ns, "hash", st_hash);
    M(ns, "makeObjectsPerformSelector:", st_perform); M(ns, "makeObjectsPerformSelector:withObject:", st_perform_with);
    M(ns, "setByAddingObject:", st_adding); M(ns, "setByAddingObjectsFromSet:", st_adding_coll);
    M(ns, "setByAddingObjectsFromArray:", st_adding_coll);
    M(ns, "filteredSetUsingPredicate:", st_filtered); M(ns, "sortedArrayUsingDescriptors:", st_sorted_desc);
    M(ns, "valueForKey:", st_value_for_key); M(ns, "setValue:forKey:", st_set_value_for_key);
    M(ns, "description", st_desc); M(ns, "descriptionWithLocale:", st_desc_loc);
    M(ns, "copy", st_copy); M(ns, "copyWithZone:", st_copy_zone); M(ns, "mutableCopy", st_mcopy); M(ns, "mutableCopyWithZone:", st_mcopy_zone);
    M(ns, "encodeWithCoder:", st_encode); M(ns, "initWithCoder:", st_decode);
    CM(ms, "setWithCapacity:", c_cap); M(ms, "initWithCapacity:", st_init_cap);
    M(ms, "addObject:", ms_add); M(ms, "removeObject:", ms_remove); M(ms, "addObjectsFromArray:", ms_add_array);
    M(ms, "unionSet:", ms_union); M(ms, "minusSet:", ms_minus); M(ms, "intersectSet:", ms_intersect);
    M(ms, "setSet:", ms_set_set); M(ms, "removeAllObjects", ms_remove_all); M(ms, "filterUsingPredicate:", ms_filter);
    M(&NSCountedSet_c, "countForObject:", cs_count_for); M(&NSCountedSet_c, "description", cs_desc);
#undef M
#undef CM
}
