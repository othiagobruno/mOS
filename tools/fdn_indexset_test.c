/* Teste de host das faixas do NSIndexSet (fdn_indexset.c): não passa pelo objc_msgSend. Compila como fdn_set_test. */
#include "../userland/libsystem/fdn_indexset.c"
int printf(const char *f, ...);
id objc_msgSend(id s, SEL c, ...) { (void)s; (void)c; return 0; }
SEL sel_registerName(const char *n) { return (SEL)n; }
id objc_retain(id o) { return o; } void objc_release(id o) { (void)o; } id objc_autorelease(id o) { return o; }
void cx_object_free(id o) { free(o); } void cx_kvo_forget(id o) { (void)o; }
const char *class_getName(id k) { (void)k; return "NSIndexSet"; }
id cx_alloc_instance(cx_class_t *k) { id o = calloc(1, sizeof(ixs_t)); *(cx_class_t **)o = k; return o; }
BOOL cx_is_subclass(cx_class_t *a, cx_class_t *b) { return a == b || (a == &mixs_c && b == &ixs_c); }
cx_class_t *cx_class_named(const char *n) { (void)n; return 0; }
id cx_str(const char *s) { (void)s; return 0; }
cx_class_t *cx_define_class(cx_class_t *a, cx_class_t *b, const char *n, cx_class_t *s, u32 e) { (void)b;(void)n;(void)s;(void)e; return a; }
void cx_add_method(cx_class_t *k, const char *s, void *f) { (void)k;(void)s;(void)f; }
void cx_add_cmethod(cx_class_t *k, const char *s, void *f) { (void)k;(void)s;(void)f; }
static int bad;
static void eq(const char *w, u64 a, u64 b) { if (a != b) { bad++; printf("FALHOU %s: %llu (queria %llu)\n", w, (unsigned long long)a, (unsigned long long)b); } }
int main(void) {
    id s = mk(&mixs_c);
    rng_t r1 = {10, 5}, r2 = {20, 3}, r3 = {15, 5};
    m_add_range(s, 0, r1); m_add_range(s, 0, r2);
    eq("count", i_count(s, 0), 8); eq("nr", X(s)->nr, 2);
    m_add_range(s, 0, r3);                       /* 15..19 junta 10..14 e 20..22 */
    eq("merge nr", X(s)->nr, 1); eq("merge count", i_count(s, 0), 13);
    m_remove(s, 0, 16);
    eq("split nr", X(s)->nr, 2); eq("has 16", i_has(s, 0, 16), 0); eq("has 17", i_has(s, 0, 17), 1);
    eq("gt 15", i_gt(s, 0, 15), 17); eq("lt 17", i_lt(s, 0, 17), 15); eq("ge 16", i_ge(s, 0, 16), 17); eq("le 16", i_le(s, 0, 16), 15);
    eq("first", i_first(s, 0), 10); eq("last", i_last(s, 0), 22); eq("gt last", i_gt(s, 0, 22), NOT_FOUND); eq("lt first", i_lt(s, 0, 10), NOT_FOUND);
    rng_t q = {12, 6}; eq("count in", i_count_in(s, 0, q), 5);
    u64 buf[4]; rng_t w = {0, 100};
    eq("get n", i_get(s, 0, buf, 4, &w), 4); eq("get b3", buf[3], 13); eq("get next", w.loc, 14); eq("get len", w.len, 86);
    m_add(s, 0, 9); eq("adjacent", X(s)->nr, 2); eq("first 9", i_first(s, 0), 9);
    m_shift(s, 0, 17, 3);                         /* 17..22 -> 20..25 */
    eq("shift has 17", i_has(s, 0, 17), 0); eq("shift has 20", i_has(s, 0, 20), 1); eq("shift last", i_last(s, 0), 25);
    m_shift(s, 0, 20, -2);                        /* 18..19 some; 20..25 -> 18..23 */
    eq("shl last", i_last(s, 0), 23); eq("shl count", i_count(s, 0), 7 + 6);
    rng_t big = {0, 1000}; m_remove_range(s, 0, big); eq("empty", i_count(s, 0), 0); eq("empty first", i_first(s, 0), NOT_FOUND);
    for (u64 i = 0; i < 100; i += 2) m_add(s, 0, i);
    eq("evens nr", X(s)->nr, 50);
    for (u64 i = 1; i < 100; i += 2) m_add(s, 0, i);
    eq("all nr", X(s)->nr, 1); eq("all count", i_count(s, 0), 100);
    eq("range of 57", __NSIndexSetIndexOfRangeContainingIndex(s, 57), 0); eq("range of 100", __NSIndexSetIndexOfRangeContainingIndex(s, 100), NOT_FOUND);
    printf("fdn_indexset: %s\n", bad ? "FALHOU" : "ok");
    return bad != 0;
}
