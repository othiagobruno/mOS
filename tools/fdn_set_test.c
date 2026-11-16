/* Teste de host da tabela hash do NSSet/CFSet (fdn_set.c), no modo CFSet com callbacks: não passa pelo objc_msgSend.
 * Compila com os cabeçalhos do libSystem (-nostdinc + o include do zig), como em tools/fdn-i18n-teste.sh. */
#include "../userland/libsystem/fdn_set.c"
int printf(const char *f, ...);
cx_class_t NSSet_c __asm__("OBJC_CLASS_$_NSSet"), NSMutableSet_c __asm__("OBJC_CLASS_$_NSMutableSet");
id objc_msgSend(id s, SEL c, ...) { (void)s; (void)c; return 0; }
SEL sel_registerName(const char *n) { return (SEL)n; }
id objc_retain(id o) { return o; } void objc_release(id o) { (void)o; } id objc_autorelease(id o) { return o; }
void cx_object_free(id o) { free(o); } void cx_kvo_forget(id o) { (void)o; }
id cx_enumerator_new(id a) { return a; } id cx_arr_new(cx_class_t *k) { (void)k; return 0; } void cx_arr_add(id a, id o) { (void)a; (void)o; }
id cx_alloc_instance(cx_class_t *k) { id o = calloc(1, sizeof(nsset_t)); *(cx_class_t **)o = k; return o; }
BOOL cx_is_subclass(cx_class_t *a, cx_class_t *b) { return a == b || (a == &NSMutableSet_c && b == &NSSet_c) || (a == &NSCountedSet_c && b != 0); }
cx_class_t *cx_class_named(const char *n) { (void)n; return 0; }
id cx_str(const char *s) { (void)s; return 0; }
cx_class_t *cx_define_class(cx_class_t *a, cx_class_t *b, const char *n, cx_class_t *s, u32 e) { (void)b;(void)n;(void)s;(void)e; return a; }
void cx_add_method(cx_class_t *k, const char *s, void *f) { (void)k;(void)s;(void)f; }
void cx_add_cmethod(cx_class_t *k, const char *s, void *f) { (void)k;(void)s;(void)f; }
static u64 h_mod(const void *v) { return (u64)v % 7; } /* colisões de propósito */
static unsigned char e_eq(const void *a, const void *b) { return a == b; }
int main(void) {
    cfset_cb_t cb = {0, 0, 0, 0, e_eq, h_mod};
    id s = CFSetCreateMutable(0, 0, &cb);
    int bad = 0;
    for (long i = 1; i <= 5000; i++) CFSetAddValue(s, (id)i);
    for (long i = 1; i <= 5000; i++) CFSetAddValue(s, (id)i);
    if (CFSetGetCount(s) != 5000) { printf("count %ld\n", CFSetGetCount(s)); bad++; }
    for (long i = 1; i <= 5000; i += 2) CFSetRemoveValue(s, (id)i);
    if (CFSetGetCount(s) != 2500) { printf("count2 %ld\n", CFSetGetCount(s)); bad++; }
    for (long i = 1; i <= 5000; i++) if (CFSetContainsValue(s, (id)i) != (i % 2 == 0)) { printf("contains %ld\n", i); bad++; break; }
    for (long i = 2; i <= 5000; i += 4) CFSetRemoveValue(s, (id)i);
    for (long i = 1; i <= 5000; i++) if (CFSetContainsValue(s, (id)i) != (i % 4 == 0)) { printf("contains2 %ld\n", i); bad++; break; }
    if (CFSetGetValue(s, (id)8) != (id)8 || CFSetGetValue(s, (id)7)) { printf("getvalue\n"); bad++; }
    id raw = CFSetCreateMutable(0, 0, 0);   /* NULL: ponteiros, identidade */
    CFSetAddValue(raw, (id)0x1234); CFSetAddValue(raw, (id)0x1234); CFSetAddValue(raw, (id)0x10);
    if (CFSetGetCount(raw) != 2) { printf("raw %ld\n", CFSetGetCount(raw)); bad++; }
    CFSetRemoveAllValues(s);
    if (CFSetGetCount(s) || CFSetContainsValue(s, (id)8)) { printf("clear\n"); bad++; }
    id cs = new_set(&NSCountedSet_c); S(cs)->cb = calloc(1, sizeof(cfset_cb_t));
    set_add(S(cs), (id)5, 1); set_add(S(cs), (id)5, 1); set_add(S(cs), (id)6, 1);
    if (cs_count_for(cs, 0, (id)5) != 2 || cs_count_for(cs, 0, (id)6) != 1 || S(cs)->n != 2) { printf("counted\n"); bad++; }
    set_remove(S(cs), (id)5, 0);
    if (cs_count_for(cs, 0, (id)5) != 1) { printf("counted rm\n"); bad++; }
    printf("fdn_set: %s\n", bad ? "FALHOU" : "ok");
    return bad;
}
