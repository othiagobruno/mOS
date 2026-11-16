/*
 * mOS libSystem — NSOrderedSet e NSMutableOrderedSet completos. Antes o resto da API ia para um NSMutableArray
 * por forwardingTargetForSelector: (o imutável aceitava mutação e replaceObjectAtIndex: criava duplicados).
 * Guarda a ordem num NSMutableArray e a pertinência num NSMutableSet (busca por hash).
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void cx_object_free(id o);
void cx_kvo_forget(id o);
id cx_enumerator_new(id array);

#define NOT_FOUND 0x7FFFFFFFFFFFFFFFULL
typedef struct { u64 loc, len; } rng_t;
typedef struct { cx_class_t *isa; id arr, set; } os_t;
#define O(o) ((os_t *)(o))
static cx_class_t os_c __asm__("OBJC_CLASS_$_NSOrderedSet"), os_m __asm__("OBJC_METACLASS_$_NSOrderedSet");
static cx_class_t mos_c __asm__("OBJC_CLASS_$_NSMutableOrderedSet"), mos_m __asm__("OBJC_METACLASS_$_NSMutableOrderedSet");
static id cls(const char *n) { return (id)cx_class_named(n); }
static void *inv_of(void *b) { return *(void **)((u8 *)b + 16); }
static id ar(id o) { return objc_autorelease(o); }
static u64 cnt(id a) { return a ? MSG0(u64, a, "count") : 0; }
static id at(id a, u64 i) { return MSG(id, a, "objectAtIndex:", i); }
static u64 ix_first(id s) { return s ? MSG0(u64, s, "firstIndex") : NOT_FOUND; }
static u64 ix_next(id s, u64 i) { return MSG(u64, s, "indexGreaterThanIndex:", i); }

static os_t *st(id o) {
    os_t *s = O(o);
    if (!s->arr) {
        s->arr = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init");
        s->set = MSG0(id, MSG0(id, cls("NSMutableSet"), "alloc"), "init");
    }
    return s;
}
static BOOL has(id o, id v) { return v && MSG(BOOL, st(o)->set, "containsObject:", v); }
static u64 index_of(id o, id v) {
    if (!has(o, v)) return NOT_FOUND;
    return MSG(u64, st(o)->arr, "indexOfObject:", v);
}
static void put(id o, id v, u64 i) {
    if (!v || has(o, v)) return;
    MSG(void, st(o)->arr, "insertObject:atIndex:", v, i);
    MSG(void, st(o)->set, "addObject:", v);
}
static void push(id o, id v) { put(o, v, cnt(st(o)->arr)); }
static void drop_at(id o, u64 i) {
    os_t *s = st(o);
    id v = objc_retain(at(s->arr, i));
    MSG(void, s->arr, "removeObjectAtIndex:", i);
    MSG(void, s->set, "removeObject:", v);
    objc_release(v);
}
static void clear(id o) { MSG0(void, st(o)->arr, "removeAllObjects"); MSG0(void, st(o)->set, "removeAllObjects"); }
/* elementos de qualquer coleção: array, ordered set ou set */
static id elems(id c) {
    if (!c) return 0;
    if (MSG(BOOL, c, "isKindOfClass:", (id)&os_c)) return st(c)->arr;
    if (MSG(BOOL, c, "respondsToSelector:", sel_registerName("objectAtIndex:"))) return c;
    return MSG0(id, c, "allObjects");
}
static void push_all(id o, id c, rng_t r, BOOL deep) {
    id a = elems(c);
    u64 n = cnt(a);
    for (u64 i = r.loc; i < n && i < r.loc + r.len; i++) {
        id v = at(a, i);
        if (deep) { v = MSG(id, v, "copyWithZone:", (void *)0); push(o, v); objc_release(v); }
        else push(o, v);
    }
}
static rng_t ALL = {0, NOT_FOUND};

/* ---------------------------------------------------------------- criação */
static id mk(id k) { id o = cx_alloc_instance((cx_class_t *)k); st(o); return o; }
static id o_alloc(id k, SEL c) { (void)c; return mk(k); }
static id o_init(id s, SEL c) { (void)c; st(s); return s; }
static id o_init_cap(id s, SEL c, u64 n) { (void)n; return o_init(s, c); }
static id o_init_obj(id s, SEL c, id v) { (void)c; push(s, v); return s; }
static id o_init_objs_count(id s, SEL c, const id *v, u64 n) { (void)c; for (u64 i = 0; v && i < n; i++) push(s, v[i]); return s; }
static id o_init_objs(id s, SEL c, id first, ...) {
    (void)c;
    va_list ap;
    va_start(ap, first);
    for (id v = first; v; v = va_arg(ap, id)) push(s, v);
    va_end(ap);
    return s;
}
static id o_init_coll(id s, SEL c, id o) { (void)c; push_all(s, o, ALL, NO); return s; }
static id o_init_coll_copy(id s, SEL c, id o, BOOL deep) { (void)c; push_all(s, o, ALL, deep); return s; }
static id o_init_coll_range(id s, SEL c, id o, rng_t r, BOOL deep) { (void)c; push_all(s, o, r, deep); return s; }
static id k_empty(id k, SEL c) { return ar(o_init(mk(k), c)); }
static id k_cap(id k, SEL c, u64 n) { return ar(o_init_cap(mk(k), c, n)); }
static id k_obj(id k, SEL c, id v) { return ar(o_init_obj(mk(k), c, v)); }
static id k_objs_count(id k, SEL c, const id *v, u64 n) { return ar(o_init_objs_count(mk(k), c, v, n)); }
static id k_objs(id k, SEL c, id first, ...) {
    (void)c;
    id s = mk(k);
    va_list ap;
    va_start(ap, first);
    for (id v = first; v; v = va_arg(ap, id)) push(s, v);
    va_end(ap);
    return ar(s);
}
static id k_coll(id k, SEL c, id o) { return ar(o_init_coll(mk(k), c, o)); }
static id k_coll_copy(id k, SEL c, id o, BOOL d) { return ar(o_init_coll_copy(mk(k), c, o, d)); }
static id k_coll_range(id k, SEL c, id o, rng_t r, BOOL d) { return ar(o_init_coll_range(mk(k), c, o, r, d)); }
static void o_dealloc(id s, SEL c) {
    (void)c;
    if (O(s)->arr) objc_release(O(s)->arr);
    if (O(s)->set) objc_release(O(s)->set);
    cx_kvo_forget(s);
    cx_object_free(s);
}

/* ---------------------------------------------------------------- consulta */
static u64 o_count(id s, SEL c) { (void)c; return cnt(st(s)->arr); }
static id o_at(id s, SEL c, u64 i) { (void)c; return at(st(s)->arr, i); }
static u64 o_index(id s, SEL c, id v) { (void)c; return index_of(s, v); }
static BOOL o_contains(id s, SEL c, id v) { (void)c; return has(s, v); }
static id o_first(id s, SEL c) { (void)c; return MSG0(id, st(s)->arr, "firstObject"); }
static id o_last(id s, SEL c) { (void)c; return MSG0(id, st(s)->arr, "lastObject"); }
static void o_get(id s, SEL c, id *out, rng_t r) { (void)c; for (u64 i = 0; out && i < r.len; i++) out[i] = at(st(s)->arr, r.loc + i); }
static id o_at_indexes(id s, SEL c, id ixs) { (void)c; return MSG(id, st(s)->arr, "objectsAtIndexes:", ixs); }
static id o_array(id s, SEL c) { (void)c; return ar(MSG0(id, st(s)->arr, "copy")); }
static id o_set(id s, SEL c) { (void)c; return ar(MSG0(id, st(s)->set, "copy")); }
static BOOL o_equal_os(id s, SEL c, id o) {
    (void)c;
    if (o == s) return YES;
    if (!o || cnt(elems(o)) != cnt(st(s)->arr)) return NO;
    return MSG(BOOL, st(s)->arr, "isEqualToArray:", elems(o));
}
static BOOL o_is_equal(id s, SEL c, id o) {
    if (o == s) return YES;
    if (!o || !MSG(BOOL, o, "isKindOfClass:", (id)&os_c)) return NO;
    return o_equal_os(s, c, o);
}
static u64 o_hash(id s, SEL c) { (void)c; return cnt(st(s)->arr); }
static BOOL o_intersects(id s, SEL c, id o) {
    (void)c;
    id a = elems(o);
    u64 n = cnt(a);
    for (u64 i = 0; i < n; i++) if (has(s, at(a, i))) return YES;
    return NO;
}
static BOOL o_subset(id s, SEL c, id o) {
    (void)c;
    u64 n = cnt(st(s)->arr);
    for (u64 i = 0; i < n; i++) if (!o || !MSG(BOOL, o, "containsObject:", at(st(s)->arr, i))) return NO;
    return YES;
}
static id o_enum(id s, SEL c) { (void)c; return cx_enumerator_new(st(s)->arr); }
static id o_reverse_enum(id s, SEL c) { (void)c; return MSG0(id, st(s)->arr, "reverseObjectEnumerator"); }
static id o_reversed(id s, SEL c) {
    (void)c;
    id r = mk((id)&os_c);
    u64 n = cnt(st(s)->arr);
    for (u64 i = n; i-- > 0;) push(r, at(st(s)->arr, i));
    return ar(r);
}
typedef struct { u64 state; id *items; u64 *mut; u64 extra[5]; } fastenum_t;
static u64 o_fast(id s, SEL c, fastenum_t *f, id *buf, u64 len) { (void)c; return MSG(u64, st(s)->arr, "countByEnumeratingWithState:objects:count:", f, buf, len); }
/* os métodos de bloco, busca e ordenação são os do array (mesmos índices) */
static void o_enum_block(id s, SEL c, void *b) { (void)c; MSG(void, st(s)->arr, "enumerateObjectsUsingBlock:", b); }
static void o_enum_opts(id s, SEL c, u64 o, void *b) { (void)c; MSG(void, st(s)->arr, "enumerateObjectsWithOptions:usingBlock:", o, b); }
static void o_enum_at(id s, SEL c, id ix, u64 o, void *b) { (void)c; MSG(void, st(s)->arr, "enumerateObjectsAtIndexes:options:usingBlock:", ix, o, b); }
static u64 o_pass(id s, SEL c, void *b) { (void)c; return MSG(u64, st(s)->arr, "indexOfObjectPassingTest:", b); }
static u64 o_pass_opts(id s, SEL c, u64 o, void *b) { (void)c; return MSG(u64, st(s)->arr, "indexOfObjectWithOptions:passingTest:", o, b); }
static u64 o_pass_at(id s, SEL c, id ix, u64 o, void *b) { (void)c; return MSG(u64, st(s)->arr, "indexOfObjectAtIndexes:options:passingTest:", ix, o, b); }
static id o_passing(id s, SEL c, void *b) { (void)c; return MSG(id, st(s)->arr, "indexesOfObjectsPassingTest:", b); }
static id o_passing_opts(id s, SEL c, u64 o, void *b) { (void)c; return MSG(id, st(s)->arr, "indexesOfObjectsWithOptions:passingTest:", o, b); }
static id o_passing_at(id s, SEL c, id ix, u64 o, void *b) { (void)c; return MSG(id, st(s)->arr, "indexesOfObjectsAtIndexes:options:passingTest:", ix, o, b); }
static u64 o_bsearch(id s, SEL c, id v, rng_t r, u64 o, void *b) { (void)c; return MSG(u64, st(s)->arr, "indexOfObject:inSortedRange:options:usingComparator:", v, r, o, b); }
static id o_sorted_cmp(id s, SEL c, void *b) { (void)c; return MSG(id, st(s)->arr, "sortedArrayUsingComparator:", b); }
static id o_sorted_opts(id s, SEL c, u64 o, void *b) { (void)c; return MSG(id, st(s)->arr, "sortedArrayWithOptions:usingComparator:", o, b); }
static id o_sorted_desc(id s, SEL c, id d) { (void)c; return MSG(id, st(s)->arr, "sortedArrayUsingDescriptors:", d); }
static id o_filtered(id s, SEL c, id pred) { (void)c; return k_coll((id)&os_c, 0, MSG(id, st(s)->arr, "filteredArrayUsingPredicate:", pred)); }
static id o_value_for_key(id s, SEL c, id key) {
    (void)c;
    id vals = MSG(id, st(s)->arr, "valueForKey:", key);
    if (key && cx_cstr(key)[0] == '@') return vals;
    return k_coll((id)&os_c, 0, vals);
}
static void o_set_value_for_key(id s, SEL c, id v, id k) { (void)c; MSG(void, st(s)->arr, "setValue:forKey:", v, k); }
static id o_desc_loc(id s, SEL c, id loc) {
    (void)c;
    id d = MSG(id, st(s)->arr, "descriptionWithLocale:", loc);
    id out = MSG0(id, MSG0(id, cls("NSMutableString"), "alloc"), "init");
    MSG(void, out, "appendString:", cx_str("{"));
    if (d) MSG(void, out, "appendString:", d);
    MSG(void, out, "appendString:", cx_str("}"));
    return ar(out);
}
static id o_desc(id s, SEL c) { return o_desc_loc(s, c, 0); }
static id copy_as(id s, cx_class_t *k) { id r = mk((id)k); push_all(r, s, ALL, NO); return r; }
static id o_copy(id s, SEL c) { (void)c; return *(cx_class_t **)s == &os_c ? objc_retain(s) : copy_as(s, &os_c); }
static id o_copy_zone(id s, SEL c, void *z) { (void)z; return o_copy(s, c); }
static id o_mcopy(id s, SEL c) { (void)c; return copy_as(s, &mos_c); }
static id o_mcopy_zone(id s, SEL c, void *z) { (void)z; return o_mcopy(s, c); }
static void o_encode(id s, SEL c, id coder) { (void)c; MSG(void, coder, "encodeObject:forKey:", st(s)->arr, cx_str("NS.objects")); }
static id o_decode(id s, SEL c, id coder) { (void)c; push_all(s, MSG(id, coder, "decodeObjectForKey:", cx_str("NS.objects")), ALL, NO); return s; }
static BOOL yes(id s, SEL c) { (void)s; (void)c; return YES; }

/* ---------------------------------------------------------------- mutável */
static void m_add(id s, SEL c, id v) { (void)c; push(s, v); }
static void m_add_objs(id s, SEL c, const id *v, u64 n) { (void)c; for (u64 i = 0; v && i < n; i++) push(s, v[i]); }
static void m_add_array(id s, SEL c, id a) { (void)c; push_all(s, a, ALL, NO); }
static void m_insert(id s, SEL c, id v, u64 i) { (void)c; put(s, v, i); }
static void m_insert_at(id s, SEL c, id objs, id ixs) {
    (void)c;
    u64 k = 0;
    for (u64 i = ix_first(ixs); i != NOT_FOUND; i = ix_next(ixs, i)) put(s, at(objs, k++), i);
}
static void m_remove_at(id s, SEL c, u64 i) { (void)c; drop_at(s, i); }
static void m_remove(id s, SEL c, id v) { (void)c; u64 i = index_of(s, v); if (i != NOT_FOUND) drop_at(s, i); }
static void m_remove_range(id s, SEL c, rng_t r) { (void)c; for (u64 k = 0; k < r.len; k++) drop_at(s, r.loc); }
static void m_remove_at_indexes(id s, SEL c, id ixs) {
    (void)c;
    for (u64 i = ixs ? MSG0(u64, ixs, "lastIndex") : NOT_FOUND; i != NOT_FOUND; i = MSG(u64, ixs, "indexLessThanIndex:", i)) drop_at(s, i);
}
static void m_remove_in(id s, SEL c, id a) {
    (void)c;
    id e = elems(a);
    u64 n = cnt(e);
    for (u64 i = 0; i < n; i++) m_remove(s, 0, at(e, i));
}
static void m_remove_all(id s, SEL c) { (void)c; clear(s); }
/* trocar por um objeto que já está em outro índice não faz nada (o conjunto não admite repetição) */
static void m_replace(id s, SEL c, u64 i, id v) {
    (void)c;
    os_t *t = st(s);
    if (!v) return;
    id old = at(t->arr, i);
    if (old == v || MSG(BOOL, old, "isEqual:", v)) { MSG(void, t->arr, "replaceObjectAtIndex:withObject:", i, v); return; }
    if (has(s, v)) return;
    objc_retain(old);
    MSG(void, t->set, "removeObject:", old);
    MSG(void, t->arr, "replaceObjectAtIndex:withObject:", i, v);
    MSG(void, t->set, "addObject:", v);
    objc_release(old);
}
static void m_set_at(id s, SEL c, id v, u64 i) {
    if (i == cnt(st(s)->arr)) { put(s, v, i); return; }
    m_replace(s, c, i, v);
}
static void m_replace_range(id s, SEL c, rng_t r, const id *v, u64 n) {
    (void)c;
    for (u64 k = 0; k < r.len; k++) drop_at(s, r.loc);
    u64 at_i = r.loc;
    for (u64 k = 0; v && k < n; k++) if (!has(s, v[k])) put(s, v[k], at_i++);
}
static void m_replace_at_indexes(id s, SEL c, id ixs, id objs) {
    u64 k = 0;
    for (u64 i = ix_first(ixs); i != NOT_FOUND; i = ix_next(ixs, i)) m_replace(s, c, i, at(objs, k++));
}
static void m_exchange(id s, SEL c, u64 i, u64 j) { (void)c; MSG(void, st(s)->arr, "exchangeObjectAtIndex:withObjectAtIndex:", i, j); }
static void m_move(id s, SEL c, id ixs, u64 to) {
    (void)c;
    os_t *t = st(s);
    id moved = MSG(id, t->arr, "objectsAtIndexes:", ixs);
    objc_retain(moved);
    MSG(void, t->arr, "removeObjectsAtIndexes:", ixs);
    u64 n = cnt(moved);
    for (u64 k = 0; k < n; k++) MSG(void, t->arr, "insertObject:atIndex:", at(moved, k), to + k);
    objc_release(moved);
}
static void m_intersect(id s, SEL c, id o) {
    (void)c;
    if (o == s) return;
    for (u64 i = cnt(st(s)->arr); i-- > 0;) if (!o || !MSG(BOOL, o, "containsObject:", at(st(s)->arr, i))) drop_at(s, i);
}
static void m_minus(id s, SEL c, id o) {
    (void)c;
    if (o == s) { clear(s); return; }
    for (u64 i = cnt(st(s)->arr); i-- > 0;) if (o && MSG(BOOL, o, "containsObject:", at(st(s)->arr, i))) drop_at(s, i);
}
static void m_union(id s, SEL c, id o) { (void)c; if (o != s) push_all(s, o, ALL, NO); }
static void m_sort_cmp(id s, SEL c, void *b) { (void)c; MSG(void, st(s)->arr, "sortUsingComparator:", b); }
static void m_sort_opts(id s, SEL c, u64 o, void *b) { (void)c; MSG(void, st(s)->arr, "sortWithOptions:usingComparator:", o, b); }
static void m_sort_range(id s, SEL c, rng_t r, u64 o, void *b) {
    (void)c; (void)o;
    os_t *t = st(s);
    id sub = MSG(id, t->arr, "subarrayWithRange:", r);
    id sorted = MSG(id, sub, "sortedArrayUsingComparator:", b);
    for (u64 k = 0; k < r.len; k++) MSG(void, t->arr, "replaceObjectAtIndex:withObject:", r.loc + k, at(sorted, k));
}
static void m_sort_desc(id s, SEL c, id d) { (void)c; MSG(void, st(s)->arr, "sortUsingDescriptors:", d); }
static void m_filter(id s, SEL c, id pred) {
    (void)c;
    for (u64 i = cnt(st(s)->arr); i-- > 0;) if (!pred || !MSG(BOOL, pred, "evaluateWithObject:", at(st(s)->arr, i))) drop_at(s, i);
}

void cx_fdn_orderedset_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("NSOrderedSet") == &os_c) return;
    cx_define_class(&os_c, &os_m, "NSOrderedSet", ob, (u32)(sizeof(os_t) - 8));
    cx_define_class(&mos_c, &mos_m, "NSMutableOrderedSet", &os_c, 0);
    cx_class_t *k = &os_c, *m = &mos_c;
#define M(c, s, f) cx_add_method(c, s, (void *)(f))
#define CM(c, s, f) cx_add_cmethod(c, s, (void *)(f))
    CM(k, "alloc", o_alloc); CM(k, "allocWithZone:", o_alloc); CM(k, "supportsSecureCoding", yes);
    CM(k, "orderedSet", k_empty); CM(k, "orderedSetWithObject:", k_obj); CM(k, "orderedSetWithObjects:", k_objs);
    CM(k, "orderedSetWithObjects:count:", k_objs_count); CM(k, "orderedSetWithOrderedSet:", k_coll);
    CM(k, "orderedSetWithOrderedSet:range:copyItems:", k_coll_range); CM(k, "orderedSetWithArray:", k_coll);
    CM(k, "orderedSetWithArray:range:copyItems:", k_coll_range); CM(k, "orderedSetWithSet:", k_coll);
    CM(k, "orderedSetWithSet:copyItems:", k_coll_copy);
    M(k, "init", o_init); M(k, "initWithObject:", o_init_obj); M(k, "initWithObjects:", o_init_objs);
    M(k, "initWithObjects:count:", o_init_objs_count); M(k, "initWithOrderedSet:", o_init_coll);
    M(k, "initWithOrderedSet:copyItems:", o_init_coll_copy); M(k, "initWithOrderedSet:range:copyItems:", o_init_coll_range);
    M(k, "initWithArray:", o_init_coll); M(k, "initWithArray:copyItems:", o_init_coll_copy);
    M(k, "initWithArray:range:copyItems:", o_init_coll_range); M(k, "initWithSet:", o_init_coll);
    M(k, "initWithSet:copyItems:", o_init_coll_copy); M(k, "dealloc", o_dealloc);
    M(k, "count", o_count); M(k, "objectAtIndex:", o_at); M(k, "objectAtIndexedSubscript:", o_at);
    M(k, "indexOfObject:", o_index); M(k, "containsObject:", o_contains); M(k, "firstObject", o_first); M(k, "lastObject", o_last);
    M(k, "getObjects:range:", o_get); M(k, "objectsAtIndexes:", o_at_indexes); M(k, "array", o_array); M(k, "set", o_set);
    M(k, "isEqualToOrderedSet:", o_equal_os); M(k, "isEqual:", o_is_equal); M(k, "hash", o_hash);
    M(k, "intersectsOrderedSet:", o_intersects); M(k, "intersectsSet:", o_intersects);
    M(k, "isSubsetOfOrderedSet:", o_subset); M(k, "isSubsetOfSet:", o_subset);
    M(k, "objectEnumerator", o_enum); M(k, "reverseObjectEnumerator", o_reverse_enum); M(k, "reversedOrderedSet", o_reversed);
    M(k, "countByEnumeratingWithState:objects:count:", o_fast);
    M(k, "enumerateObjectsUsingBlock:", o_enum_block); M(k, "enumerateObjectsWithOptions:usingBlock:", o_enum_opts);
    M(k, "enumerateObjectsAtIndexes:options:usingBlock:", o_enum_at);
    M(k, "indexOfObjectPassingTest:", o_pass); M(k, "indexOfObjectWithOptions:passingTest:", o_pass_opts);
    M(k, "indexOfObjectAtIndexes:options:passingTest:", o_pass_at);
    M(k, "indexesOfObjectsPassingTest:", o_passing); M(k, "indexesOfObjectsWithOptions:passingTest:", o_passing_opts);
    M(k, "indexesOfObjectsAtIndexes:options:passingTest:", o_passing_at);
    M(k, "indexOfObject:inSortedRange:options:usingComparator:", o_bsearch);
    M(k, "sortedArrayUsingComparator:", o_sorted_cmp); M(k, "sortedArrayWithOptions:usingComparator:", o_sorted_opts);
    M(k, "sortedArrayUsingDescriptors:", o_sorted_desc); M(k, "filteredOrderedSetUsingPredicate:", o_filtered);
    M(k, "valueForKey:", o_value_for_key); M(k, "setValue:forKey:", o_set_value_for_key);
    M(k, "description", o_desc); M(k, "descriptionWithLocale:", o_desc_loc);
    M(k, "copy", o_copy); M(k, "copyWithZone:", o_copy_zone); M(k, "mutableCopy", o_mcopy); M(k, "mutableCopyWithZone:", o_mcopy_zone);
    M(k, "encodeWithCoder:", o_encode); M(k, "initWithCoder:", o_decode);
    CM(m, "orderedSetWithCapacity:", k_cap); M(m, "initWithCapacity:", o_init_cap);
    M(m, "addObject:", m_add); M(m, "addObjects:count:", m_add_objs); M(m, "addObjectsFromArray:", m_add_array);
    M(m, "insertObject:atIndex:", m_insert); M(m, "insertObjects:atIndexes:", m_insert_at);
    M(m, "setObject:atIndex:", m_set_at); M(m, "setObject:atIndexedSubscript:", m_set_at);
    M(m, "removeObjectAtIndex:", m_remove_at); M(m, "removeObject:", m_remove); M(m, "removeObjectsInRange:", m_remove_range);
    M(m, "removeObjectsAtIndexes:", m_remove_at_indexes); M(m, "removeObjectsInArray:", m_remove_in); M(m, "removeAllObjects", m_remove_all);
    M(m, "replaceObjectAtIndex:withObject:", m_replace); M(m, "replaceObjectsInRange:withObjects:count:", m_replace_range);
    M(m, "replaceObjectsAtIndexes:withObjects:", m_replace_at_indexes);
    M(m, "exchangeObjectAtIndex:withObjectAtIndex:", m_exchange); M(m, "moveObjectsAtIndexes:toIndex:", m_move);
    M(m, "intersectOrderedSet:", m_intersect); M(m, "intersectSet:", m_intersect);
    M(m, "minusOrderedSet:", m_minus); M(m, "minusSet:", m_minus); M(m, "unionOrderedSet:", m_union); M(m, "unionSet:", m_union);
    M(m, "sortUsingComparator:", m_sort_cmp); M(m, "sortWithOptions:usingComparator:", m_sort_opts);
    M(m, "sortRange:options:usingComparator:", m_sort_range); M(m, "sortUsingDescriptors:", m_sort_desc);
    M(m, "filterUsingPredicate:", m_filter);
#undef M
#undef CM
}
