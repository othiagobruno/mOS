/* PDFKit (pdfkit.c) no host x86_64: runtime ObjC mínimo (objc_msgSend em assembly, classes com superclasse,
 * retain/release com dealloc) e as classes Foundation que o PDFKit usa, por cima do Core Graphics de verdade.
 * Compilação e PDFs de entrada: ver tools/pdfkit_host.py. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct cls_s cls_t;
typedef struct { cls_t *isa; } obj_t;
typedef obj_t *id;
typedef const char *SEL;
typedef void *IMP;
typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { P o; S s; } R;
typedef struct { double a, b, c, d, tx, ty; } T;

/* ---------------------------------------------------------------- runtime */
/* cx_class_t do libSystem: {isa, super, cache, vtable, data}; o dado do runtime fica numa tabela à parte */
typedef struct pubcls { struct pubcls *isa, *super; void *cache, *vtable; uint64_t data; } pubcls;
typedef struct { const char *sel; void *imp; } meth;
struct cls_s { pubcls pub; const char *name; cls_t *super; uint32_t size; meth *m, *cm; int nm, ncm; pubcls *ext; };
static cls_t *classes[256];
static int ncls;
static cls_t *cls_of_pub(const void *p) {
    for (int i = 0; i < ncls; i++) if (classes[i] == p || classes[i]->ext == p) return classes[i];
    return 0;
}
static cls_t *mkcls(const char *name, cls_t *super, uint32_t size) {
    cls_t *c = calloc(1, sizeof *c);
    c->name = name; c->super = super; c->size = size;
    c->pub.super = super ? (pubcls *)(super->ext ? super->ext : &super->pub) : 0;
    classes[ncls++] = c;
    return c;
}
static void add(cls_t *c, const char *sel, void *imp, int cmeth) {
    if (cmeth) { c->cm = realloc(c->cm, (size_t)(c->ncm + 1) * sizeof(meth)); c->cm[c->ncm].sel = sel; c->cm[c->ncm++].imp = imp; }
    else { c->m = realloc(c->m, (size_t)(c->nm + 1) * sizeof(meth)); c->m[c->nm].sel = sel; c->m[c->nm++].imp = imp; }
}
typedef struct { long rc; } hdr_t;
static id alloc_of(cls_t *c) {
    hdr_t *h = calloc(1, sizeof *h + c->size + 64);
    h->rc = 1;
    id o = (id)(h + 1);
    o->isa = c->ext ? (cls_t *)c->ext : c;
    return o;
}
static void *find_imp(cls_t *c, const char *sel, int cmeth) {
    for (; c; c = c->super) {
        meth *m = cmeth ? c->cm : c->m;
        int n = cmeth ? c->ncm : c->nm;
        for (int i = n - 1; i >= 0; i--) if (!strcmp(m[i].sel, sel)) return m[i].imp;
    }
    return 0;
}
static int missing;
static long nil_imp(void) { return 0; }
void *host_lookup(id self, const char *sel) {
    cls_t *c = cls_of_pub(self);
    void *imp;
    if (c) imp = find_imp(c, sel, 1);                    /* self é uma classe */
    else imp = find_imp(cls_of_pub(self->isa), sel, 0);
    if (!imp) { fprintf(stderr, "  [método ausente: %s %s]\n", c ? c->name : cls_of_pub(self->isa) ? cls_of_pub(self->isa)->name : "?", sel); missing++; return (void *)nil_imp; }
    return imp;
}
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\n"
        "  test %rdi, %rdi\n  jz 1f\n"
        "  push %rdi\n  push %rsi\n  push %rdx\n  push %rcx\n  push %r8\n  push %r9\n  sub $136, %rsp\n"
        "  movdqu %xmm0, 0(%rsp)\n  movdqu %xmm1, 16(%rsp)\n  movdqu %xmm2, 32(%rsp)\n  movdqu %xmm3, 48(%rsp)\n"
        "  movdqu %xmm4, 64(%rsp)\n  movdqu %xmm5, 80(%rsp)\n  movdqu %xmm6, 96(%rsp)\n  movdqu %xmm7, 112(%rsp)\n"
        "  call host_lookup\n  mov %rax, %r11\n"
        "  movdqu 0(%rsp), %xmm0\n  movdqu 16(%rsp), %xmm1\n  movdqu 32(%rsp), %xmm2\n  movdqu 48(%rsp), %xmm3\n"
        "  movdqu 64(%rsp), %xmm4\n  movdqu 80(%rsp), %xmm5\n  movdqu 96(%rsp), %xmm6\n  movdqu 112(%rsp), %xmm7\n"
        "  add $136, %rsp\n  pop %r9\n  pop %r8\n  pop %rcx\n  pop %rdx\n  pop %rsi\n  pop %rdi\n  jmp *%r11\n"
        "1: xor %eax, %eax\n  xor %edx, %edx\n  xorps %xmm0, %xmm0\n  xorps %xmm1, %xmm1\n  ret\n"
        ".globl objc_msgSend_stret\nobjc_msgSend_stret:\n"
        "  test %rsi, %rsi\n  jz 2f\n"
        "  push %rdi\n  push %rsi\n  push %rdx\n  push %rcx\n  push %r8\n  push %r9\n  sub $136, %rsp\n"
        "  movdqu %xmm0, 0(%rsp)\n  movdqu %xmm1, 16(%rsp)\n  movdqu %xmm2, 32(%rsp)\n  movdqu %xmm3, 48(%rsp)\n"
        "  mov %rsi, %rdi\n  mov %rdx, %rsi\n  call host_lookup\n  mov %rax, %r11\n"
        "  movdqu 0(%rsp), %xmm0\n  movdqu 16(%rsp), %xmm1\n  movdqu 32(%rsp), %xmm2\n  movdqu 48(%rsp), %xmm3\n"
        "  add $136, %rsp\n  pop %r9\n  pop %r8\n  pop %rcx\n  pop %rdx\n  pop %rsi\n  pop %rdi\n  jmp *%r11\n"
        "2: mov %rdi, %rax\n  ret\n");
id objc_msgSend(id self, SEL op, ...);
id objc_msgSend_stret(id self, SEL op, ...);
/* __VA_OPT__ em vez de ## para que MSG dentro de MSG expanda */
#define MSG(T, o, s, ...) ((T (*)(id, SEL, ...))objc_msgSend)((id)(o), s __VA_OPT__(,) __VA_ARGS__)
#define MSG0(T, o, s) ((T (*)(id, SEL))objc_msgSend)((id)(o), s)
const char *sel_registerName(const char *s) { return s; }
id objc_retain(id o) { if (o && !cls_of_pub(o)) ((hdr_t *)o - 1)->rc++; return o; }
void cx_object_free(id o) { free((hdr_t *)o - 1); }
void objc_release(id o) {
    if (!o || cls_of_pub(o)) return;
    hdr_t *h = (hdr_t *)o - 1;
    if (h->rc <= 0) return;                               /* constante */
    if (--h->rc > 0) return;
    void *d = find_imp(cls_of_pub(o->isa), "dealloc", 0);
    if (d) ((void (*)(id, SEL))d)(o, "dealloc"); else free(h);
}
id objc_autorelease(id o) { return o; }               /* o teste não solta autorelease: vazamento aceito */
void CFRelease(id o) { objc_release(o); }
id CFRetain(id o) { return objc_retain(o); }
/* API do runtime do libSystem */
void *cx_class_named(const char *name) {
    for (int i = 0; i < ncls; i++) if (!strcmp(classes[i]->name, name)) return classes[i]->ext ? (void *)classes[i]->ext : (void *)classes[i];
    return 0;
}
void *cx_define_class(void *pub, void *meta, const char *name, void *super, uint32_t extra) {
    (void)meta;
    cls_t *sc = cls_of_pub(super);
    cls_t *c = mkcls(name, sc, (sc ? sc->size : 8) + extra);
    c->ext = pub;
    ((pubcls *)pub)->super = super;
    return pub;
}
void cx_add_method(void *k, const char *sel, void *imp) { add(cls_of_pub(k), sel, imp, 0); }
void cx_add_cmethod(void *k, const char *sel, void *imp) { add(cls_of_pub(k), sel, imp, 1); }
uint32_t cx_instance_size(void *k) { cls_t *c = cls_of_pub(k); return c ? c->size : 8; }
void *cx_lookup(void *k, const char *sel) { cls_t *c = cls_of_pub(k); return c ? find_imp(c, sel, 0) : 0; }
int8_t cx_responds(void *k, const char *sel) { cls_t *c = cls_of_pub(k); return c && find_imp(c, sel, 0) ? 1 : 0; }
id cx_alloc_instance(void *k) { return alloc_of(cls_of_pub(k)); }
void *objc_allocateClassPair(void *super, const char *name, uint64_t extra) { return mkcls(name, cls_of_pub(super), (cls_of_pub(super) ? cls_of_pub(super)->size : 8) + (uint32_t)extra); }
int class_addIvar(void *k, const char *n, size_t size, uint8_t a, const char *t) { (void)n; (void)a; (void)t; cls_of_pub(k)->size += (uint32_t)size; return 1; }
int class_addMethod(void *k, const char *sel, void *imp, const char *t) { (void)t; add(cls_of_pub(k), sel, imp, 0); return 1; }
void objc_registerClassPair(void *k) { (void)k; }
id class_createInstance(void *k, uint64_t extra) { (void)extra; return alloc_of(cls_of_pub(k)); }
void *class_getMethodImplementation(void *k, const char *sel) { return cx_lookup(k, sel); }

/* ---------------------------------------------------------------- Foundation mínima */
static cls_t *NSObject_k, *NSString_k, *NSArray_k, *NSDict_k, *NSValue_k, *NSNumber_k, *NSData_k, *NSURL_k, *NSColor_k, *NSNC_k, *NSDate_k, *NSCS_k, *NSImage_k, *NSAttr_k;
char __CFConstantStringClassReference[64];
typedef struct { cls_t *isa; uint64_t flags; char *s; uint64_t n; } str_t;
static id mkstr(const char *s) { str_t *o = (str_t *)alloc_of(NSString_k); o->s = strdup(s); o->n = strlen(s); return (id)o; }
static int is_str(id o) { return o && (o->isa == (cls_t *)__CFConstantStringClassReference || o->isa == NSString_k); }
int cx_is_str(void *o) { return is_str(o); }
id cx_str(const char *s) { return mkstr(s ? s : ""); }
const char *cx_cstr(id o) { return is_str(o) ? ((str_t *)o)->s : ""; }
static long u8len16(const char *s) { long n = 0; for (const unsigned char *p = (const unsigned char *)s; *p; p++) { if ((*p & 0xC0) != 0x80) n += *p >= 0xF0 ? 2 : 1; } return n; }
static long s_length(id self, SEL c) { (void)c; return u8len16(cx_cstr(self)); }
static const char *s_utf8(id self, SEL c) { (void)c; return cx_cstr(self); }
static int8_t s_equal(id self, SEL c, id o) { (void)c; return is_str(o) && !strcmp(cx_cstr(self), cx_cstr(o)); }
static void s_append(id self, SEL c, id o) {
    (void)c;
    str_t *s = (str_t *)self;
    const char *a = cx_cstr(o);
    s->s = realloc(s->s, s->n + strlen(a) + 1);
    strcpy(s->s + s->n, a);
    s->n += strlen(a);
}
static id s_mstring(id k, SEL c) { (void)k; (void)c; return mkstr(""); }
static id s_with_utf8(id k, SEL c, const char *u) { (void)k; (void)c; return mkstr(u); }
static id s_copy(id self, SEL c) { (void)c; return objc_retain(self); }
static id s_trim(id self, SEL c, id set) {
    (void)c; (void)set;
    const char *s = cx_cstr(self);
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    char *t = strdup(s);
    size_t n = strlen(t);
    while (n && (t[n - 1] == ' ' || t[n - 1] == '\t' || t[n - 1] == '\n')) t[--n] = 0;
    id r = mkstr(t);
    free(t);
    return r;
}
id CFStringCreateWithCString(void *al, const char *s, uint32_t e) { (void)al; (void)e; return mkstr(s); }
id CFStringCreateWithCharacters(void *al, const uint16_t *u, long n) {
    (void)al;
    char *b = malloc((size_t)n * 4 + 1);
    long k = 0;
    for (long i = 0; i < n; i++) {
        uint32_t c = u[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n) { c = 0x10000 + ((c - 0xD800) << 10) + (u[i + 1] - 0xDC00); i++; }
        if (c < 0x80) b[k++] = (char)c;
        else if (c < 0x800) { b[k++] = (char)(0xC0 | c >> 6); b[k++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { b[k++] = (char)(0xE0 | c >> 12); b[k++] = (char)(0x80 | (c >> 6 & 63)); b[k++] = (char)(0x80 | (c & 63)); }
        else { b[k++] = (char)(0xF0 | c >> 18); b[k++] = (char)(0x80 | (c >> 12 & 63)); b[k++] = (char)(0x80 | (c >> 6 & 63)); b[k++] = (char)(0x80 | (c & 63)); }
    }
    b[k] = 0;
    id r = mkstr(b);
    free(b);
    return r;
}
/* arrays */
typedef struct { cls_t *isa; long n, cap; id *v; } arr_t;
static id mkarr(void) { return alloc_of(NSArray_k); }
static void a_add(id self, SEL c, id o) { (void)c; arr_t *a = (arr_t *)self; if (a->n == a->cap) { a->cap = a->cap ? a->cap * 2 : 8; a->v = realloc(a->v, (size_t)a->cap * sizeof(id)); } a->v[a->n++] = objc_retain(o); }
static long a_count(id self, SEL c) { (void)c; return ((arr_t *)self)->n; }
static id a_at(id self, SEL c, unsigned long i) { (void)c; arr_t *a = (arr_t *)self; return (long)i < a->n ? a->v[i] : 0; }
static void a_insert(id self, SEL c, id o, unsigned long i) {
    a_add(self, c, o);
    arr_t *a = (arr_t *)self;
    memmove(a->v + i + 1, a->v + i, (size_t)(a->n - 1 - (long)i) * sizeof(id));
    a->v[i] = o;
}
static void a_remove_at(id self, SEL c, unsigned long i) { (void)c; arr_t *a = (arr_t *)self; if ((long)i >= a->n) return; id o = a->v[i]; memmove(a->v + i, a->v + i + 1, (size_t)(a->n - 1 - (long)i) * sizeof(id)); a->n--; objc_release(o); }
static unsigned long a_index_ident(id self, SEL c, id o) { (void)c; arr_t *a = (arr_t *)self; for (long i = 0; i < a->n; i++) if (a->v[i] == o) return (unsigned long)i; return (unsigned long)-1; }
static void a_remove_ident(id self, SEL c, id o) { long i = (long)a_index_ident(self, c, o); if (i >= 0) a_remove_at(self, c, (unsigned long)i); }
static int8_t a_contains(id self, SEL c, id o) { return (long)a_index_ident(self, c, o) >= 0; }
static void a_exchange(id self, SEL c, unsigned long i, unsigned long j) { (void)c; arr_t *a = (arr_t *)self; id t = a->v[i]; a->v[i] = a->v[j]; a->v[j] = t; }
static void a_remove_last(id self, SEL c) { arr_t *a = (arr_t *)self; if (a->n) a_remove_at(self, c, (unsigned long)(a->n - 1)); }
static void a_remove_all(id self, SEL c) { arr_t *a = (arr_t *)self; while (a->n) a_remove_at(self, c, 0); }
static id a_copy(id self, SEL c) { arr_t *a = (arr_t *)self; id n = mkarr(); for (long i = 0; i < a->n; i++) a_add(n, c, a->v[i]); return n; }
static id a_array(id k, SEL c) { (void)k; (void)c; return mkarr(); }
static void a_dealloc(id self, SEL c) { (void)c; arr_t *a = (arr_t *)self; for (long i = 0; i < a->n; i++) objc_release(a->v[i]); free(a->v); cx_object_free(self); }
static id a_join(id self, SEL c, id sep) {
    (void)c;
    arr_t *a = (arr_t *)self;
    id r = mkstr("");
    for (long i = 0; i < a->n; i++) { if (i) s_append(r, 0, sep); s_append(r, 0, a->v[i]); }
    return r;
}
static id s_split_set(id self, SEL c, id set) {
    (void)c;
    const char *chars = cx_cstr(set), *s = cx_cstr(self);
    id r = mkarr();
    char *t = strdup(s), *p = t, *st = t;
    for (;; p++) {
        if (!*p || strchr(chars, *p)) { char e = *p; *p = 0; a_add(r, 0, mkstr(st)); if (!e) break; st = p + 1; }
    }
    free(t);
    return r;
}
long CFArrayGetCount(id a) { return a ? ((arr_t *)a)->n : 0; }
id CFArrayGetValueAtIndex(id a, long i) { return ((arr_t *)a)->v[i]; }
id CFArrayCreate(void *al, const void **v, long n, const void *cb) { (void)al; (void)cb; id a = mkarr(); for (long i = 0; i < n; i++) a_add(a, 0, (id)v[i]); return a; }
/* dicionários (chave por conteúdo de string) */
typedef struct { cls_t *isa; long n, cap; id *k, *v; } dict_t;
static id mkdict(void) { return alloc_of(NSDict_k); }
static long d_find(dict_t *d, id k) { for (long i = 0; i < d->n; i++) if (d->k[i] == k || (is_str(k) && is_str(d->k[i]) && !strcmp(cx_cstr(k), cx_cstr(d->k[i])))) return i; return -1; }
static void d_set(id self, SEL c, id v, id k) {
    (void)c;
    dict_t *d = (dict_t *)self;
    long i = d_find(d, k);
    if (i >= 0) { objc_retain(v); objc_release(d->v[i]); d->v[i] = v; return; }
    if (d->n == d->cap) { d->cap = d->cap ? d->cap * 2 : 8; d->k = realloc(d->k, (size_t)d->cap * sizeof(id)); d->v = realloc(d->v, (size_t)d->cap * sizeof(id)); }
    d->k[d->n] = k; d->v[d->n++] = objc_retain(v);
}
static id d_get(id self, SEL c, id k) { (void)c; dict_t *d = (dict_t *)self; long i = d_find(d, k); return i >= 0 ? d->v[i] : 0; }
static void d_remove(id self, SEL c, id k) { (void)c; dict_t *d = (dict_t *)self; long i = d_find(d, k); if (i < 0) return; objc_release(d->v[i]); d->n--; d->k[i] = d->k[d->n]; d->v[i] = d->v[d->n]; }
static id d_dict(id k, SEL c) { (void)k; (void)c; return mkdict(); }
static id d_with(id k, SEL c, id v, id key) { (void)k; id d = mkdict(); d_set(d, c, v, key); return d; }
static void d_add_all(id self, SEL c, id o) { dict_t *s = (dict_t *)o; for (long i = 0; s && i < s->n; i++) d_set(self, c, s->v[i], s->k[i]); }
static id d_copy(id self, SEL c) { id n = mkdict(); d_add_all(n, c, self); return n; }
id CFDictionaryCreateMutable(void *al, long cap, const void *k, const void *v) { (void)al; (void)cap; (void)k; (void)v; return mkdict(); }
void CFDictionarySetValue(id d, const void *k, const void *v) { d_set(d, 0, (id)v, (id)k); }
id CFDictionaryGetValue(id d, const void *k) { return d ? d_get(d, 0, (id)k) : 0; }
/* NSValue, NSNumber, NSDate */
typedef struct { cls_t *isa; double v[4]; } val_t;
static id v_rect(id k, SEL c, R r) { (void)k; (void)c; val_t *o = (val_t *)alloc_of(NSValue_k); memcpy(o->v, &r, sizeof r); return (id)o; }
static id v_point(id k, SEL c, P p) { (void)k; (void)c; val_t *o = (val_t *)alloc_of(NSValue_k); memcpy(o->v, &p, sizeof p); return (id)o; }
static R v_rectv(id self, SEL c) { (void)c; R r; memcpy(&r, ((val_t *)self)->v, sizeof r); return r; }
static P v_pointv(id self, SEL c) { (void)c; P p; memcpy(&p, ((val_t *)self)->v, sizeof p); return p; }
static id n_long(id k, SEL c, long v) { (void)k; (void)c; val_t *o = (val_t *)alloc_of(NSNumber_k); o->v[0] = (double)v; return (id)o; }
static id n_bool(id k, SEL c, int8_t v) { return n_long(k, c, v); }
static long n_longv(id self, SEL c) { (void)c; return (long)((val_t *)self)->v[0]; }
id CFNumberCreate(void *al, long t, const void *p) { (void)al; double v = t == 13 ? *(const double *)p : *(const int *)p; val_t *o = (val_t *)alloc_of(NSNumber_k); o->v[0] = v; return (id)o; }
uint8_t CFNumberGetValue(id n, long t, void *out) { if (!n || n->isa != NSNumber_k) return 0; double v = ((val_t *)n)->v[0]; if (t == 13) *(double *)out = v; else *(int *)out = (int)v; return 1; }
static id dt_ref(id k, SEL c, double t) { (void)k; (void)c; val_t *o = (val_t *)alloc_of(NSDate_k); o->v[0] = t; return (id)o; }
static double dt_refv(id self, SEL c) { (void)c; return ((val_t *)self)->v[0]; }
id CFDateCreate(void *al, double t) { return dt_ref(0, 0, t); (void)al; }
/* NSData */
typedef struct { cls_t *isa; uint8_t *b; long n; } data_t;
static id mkdata(const void *b, long n) { data_t *o = (data_t *)alloc_of(NSData_k); o->b = malloc((size_t)n + 1); if (n) memcpy(o->b, b, (size_t)n); o->n = n; return (id)o; }
static id dt_bytes_len(id k, SEL c, const void *b, unsigned long n) { (void)k; (void)c; return mkdata(b, (long)n); }
static id dt_data(id k, SEL c) { (void)k; (void)c; return mkdata("", 0); }
static const void *dt_bytes(id self, SEL c) { (void)c; return ((data_t *)self)->b; }
static long dt_len(id self, SEL c) { (void)c; return ((data_t *)self)->n; }
static id dt_copy(id self, SEL c) { (void)c; return mkdata(((data_t *)self)->b, ((data_t *)self)->n); }
static void dt_append(id self, SEL c, const void *b, long n) { (void)c; data_t *d = (data_t *)self; d->b = realloc(d->b, (size_t)(d->n + n + 1)); memcpy(d->b + d->n, b, (size_t)n); d->n += n; }
void CFDataAppendBytes(id d, const uint8_t *b, long n) { dt_append(d, 0, b, n); }
id CFDataCreate(void *al, const uint8_t *b, long n) { (void)al; return mkdata(b, n); }
const uint8_t *CFDataGetBytePtr(id d) { return ((data_t *)d)->b; }
long CFDataGetLength(id d) { return ((data_t *)d)->n; }
static void dt_dealloc(id self, SEL c) { (void)c; free(((data_t *)self)->b); cx_object_free(self); }
/* NSURL: {isa, string} */
typedef struct { cls_t *isa; id str; } url_t;
static id u_file(id k, SEL c, id path) { (void)k; (void)c; url_t *o = (url_t *)alloc_of(NSURL_k); char b[1200]; snprintf(b, sizeof b, "file://%s", cx_cstr(path)); o->str = mkstr(b); return (id)o; }
static id u_string(id k, SEL c, id s) { (void)k; (void)c; url_t *o = (url_t *)alloc_of(NSURL_k); o->str = objc_retain(s); return (id)o; }
static id u_abs(id self, SEL c) { (void)c; return ((url_t *)self)->str; }
static id u_path(id self, SEL c) { (void)c; const char *s = cx_cstr(((url_t *)self)->str); return mkstr(!strncmp(s, "file://", 7) ? s + 7 : s); }
id CFURLGetString(id u) { return u ? ((url_t *)u)->str : 0; }
uint8_t CFURLGetFileSystemRepresentation(id u, uint8_t r, uint8_t *buf, long max) { (void)r; snprintf((char *)buf, (size_t)max, "%s", cx_cstr(u_path(u, 0))); return 1; }
static id dt_contents_url(id k, SEL c, id url) {
    (void)k; (void)c;
    FILE *f = fopen(cx_cstr(u_path(url, 0)), "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) n = 0;
    fclose(f);
    id d = mkdata(b, n);
    free(b);
    return d;
}
static int8_t dt_write_file(id self, SEL c, id path, int8_t atom) { (void)c; (void)atom; FILE *f = fopen(cx_cstr(path), "wb"); if (!f) return 0; fwrite(((data_t *)self)->b, 1, (size_t)((data_t *)self)->n, f); fclose(f); return 1; }
static int8_t dt_write_url(id self, SEL c, id url, int8_t atom) { return dt_write_file(self, c, u_path(url, 0), atom); }
/* NSColor: {isa, r, g, b, a} (o mesmo do cx_color do Core Graphics) */
typedef struct { cls_t *isa; double r, g, b, a; } col_t;
id cx_color(double r, double g, double b, double a) { col_t *o = (col_t *)alloc_of(NSColor_k); o->r = r; o->g = g; o->b = b; o->a = a; return (id)o; }
static id c_rgba(id k, SEL c, double r, double g, double b, double a) { (void)k; (void)c; return cx_color(r, g, b, a); }
static id c_self(id self, SEL c, id n) { (void)c; (void)n; return self; }
static double c_r(id self, SEL c) { (void)c; return ((col_t *)self)->r; }
static double c_g(id self, SEL c) { (void)c; return ((col_t *)self)->g; }
static double c_b(id self, SEL c) { (void)c; return ((col_t *)self)->b; }
static double c_a(id self, SEL c) { (void)c; return ((col_t *)self)->a; }
/* NSNotificationCenter: conta as notificações por nome */
static char posted[32][64];
static int nposted[32], npnames;
static id nc_default(id k, SEL c) { (void)k; (void)c; static id nc; if (!nc) nc = alloc_of(NSNC_k); return nc; }
static void nc_post(id self, SEL c, id name, id obj, id info) {
    (void)self; (void)c; (void)obj; (void)info;
    const char *n = cx_cstr(name);
    for (int i = 0; i < npnames; i++) if (!strcmp(posted[i], n)) { nposted[i]++; return; }
    if (npnames < 32) { snprintf(posted[npnames], 64, "%s", n); nposted[npnames++] = 1; }
}
static void nc_post2(id self, SEL c, id name, id obj) { nc_post(self, c, name, obj, 0); }
static int posted_count(const char *n) { for (int i = 0; i < npnames; i++) if (!strcmp(posted[i], n)) return nposted[i]; return 0; }
/* NSCharacterSet: {isa, chars} */
static id cs_with(id k, SEL c, id s) { (void)k; (void)c; return mkstr(cx_cstr(s)); }
static id cs_ws(id k, SEL c) { (void)k; (void)c; return mkstr(" \t"); }
/* NSImage: {isa, CGImage, w, h} */
typedef struct { cls_t *isa; void *cg; double w, h; } img_t;
void *CGImageRetain(void *i);
static id im_init_cg(id self, SEL c, void *cg, S sz) { (void)c; img_t *o = (img_t *)self; o->cg = cg ? CGImageRetain(cg) : 0; o->w = sz.w; o->h = sz.h; return self; }
static S im_size(id self, SEL c) { (void)c; S z = {((img_t *)self)->w, ((img_t *)self)->h}; return z; }
static void *im_cg(id self, SEL c, void *r, id ctx, id hints) { (void)c; (void)r; (void)ctx; (void)hints; return ((img_t *)self)->cg; }
static id o_alloc(id k, SEL c) { (void)c; return alloc_of(cls_of_pub(k)); }
static id o_init(id self, SEL c) { (void)c; return self; }
static id o_retain(id self, SEL c) { (void)c; return objc_retain(self); }
static void o_release(id self, SEL c) { (void)c; objc_release(self); }
static id o_auto(id self, SEL c) { (void)c; return self; }
static void o_dealloc(id self, SEL c) { (void)c; cx_object_free(self); }
static int8_t o_kind(id self, SEL c, id k) { (void)c; for (cls_t *x = cls_of_pub(self->isa); x; x = x->super) if ((void *)x == (void *)k || x->ext == (void *)k) return 1; return 0; }
static int8_t o_responds(id self, SEL c, const char *s) { (void)c; return find_imp(cls_of_pub(self->isa), s, 0) != 0; }
static id o_copy(id self, SEL c) { (void)c; if (find_imp(cls_of_pub(self->isa), "copyWithZone:", 0)) return MSG(id, self, "copyWithZone:", (void *)0); return objc_retain(self); }
static id attr_init(id self, SEL c, id s) { (void)c; ((url_t *)self)->str = objc_retain(s); return self; }
static id attr_string(id self, SEL c) { (void)c; return ((url_t *)self)->str; }
static void foundation_init(void) {
    NSObject_k = mkcls("NSObject", 0, 8);
    add(NSObject_k, "alloc", (void *)o_alloc, 1); add(NSObject_k, "init", (void *)o_init, 0); add(NSObject_k, "retain", (void *)o_retain, 0);
    add(NSObject_k, "release", (void *)o_release, 0); add(NSObject_k, "autorelease", (void *)o_auto, 0); add(NSObject_k, "dealloc", (void *)o_dealloc, 0);
    add(NSObject_k, "isKindOfClass:", (void *)o_kind, 0); add(NSObject_k, "copy", (void *)o_copy, 0); add(NSObject_k, "respondsToSelector:", (void *)o_responds, 0);
    NSString_k = mkcls("NSString", NSObject_k, 32);
    NSString_k->ext = (pubcls *)__CFConstantStringClassReference;             /* constantes e strings novas: a mesma classe */
    cls_t *ms = mkcls("NSMutableString", NSString_k, 32);
    add(NSString_k, "length", (void *)s_length, 0); add(NSString_k, "UTF8String", (void *)s_utf8, 0); add(NSString_k, "isEqual:", (void *)s_equal, 0);
    add(NSString_k, "isEqualToString:", (void *)s_equal, 0); add(NSString_k, "appendString:", (void *)s_append, 0); add(NSString_k, "copy", (void *)s_copy, 0);
    add(NSString_k, "componentsSeparatedByCharactersInSet:", (void *)s_split_set, 0); add(NSString_k, "stringByTrimmingCharactersInSet:", (void *)s_trim, 0);
    add(ms, "string", (void *)s_mstring, 1); add(NSString_k, "stringWithUTF8String:", (void *)s_with_utf8, 1);
    NSArray_k = mkcls("NSMutableArray", NSObject_k, 32);
    cls_t *na = mkcls("NSArray", NSObject_k, 32); (void)na;
    NSArray_k->super = na;
    add(NSArray_k, "array", (void *)a_array, 1); add(NSArray_k, "addObject:", (void *)a_add, 0); add(NSArray_k, "count", (void *)a_count, 0);
    add(NSArray_k, "objectAtIndex:", (void *)a_at, 0); add(NSArray_k, "insertObject:atIndex:", (void *)a_insert, 0);
    add(NSArray_k, "removeObjectAtIndex:", (void *)a_remove_at, 0); add(NSArray_k, "indexOfObjectIdenticalTo:", (void *)a_index_ident, 0);
    add(NSArray_k, "removeObjectIdenticalTo:", (void *)a_remove_ident, 0); add(NSArray_k, "removeObject:", (void *)a_remove_ident, 0);
    add(NSArray_k, "containsObject:", (void *)a_contains, 0); add(NSArray_k, "exchangeObjectAtIndex:withObjectAtIndex:", (void *)a_exchange, 0);
    add(NSArray_k, "removeLastObject", (void *)a_remove_last, 0); add(NSArray_k, "removeAllObjects", (void *)a_remove_all, 0);
    add(NSArray_k, "copy", (void *)a_copy, 0); add(NSArray_k, "dealloc", (void *)a_dealloc, 0); add(NSArray_k, "componentsJoinedByString:", (void *)a_join, 0);
    NSDict_k = mkcls("NSMutableDictionary", NSObject_k, 40);
    cls_t *nd = mkcls("NSDictionary", NSObject_k, 40);
    NSDict_k->super = nd;
    add(NSDict_k, "dictionary", (void *)d_dict, 1); add(nd, "dictionaryWithObject:forKey:", (void *)d_with, 1);
    add(NSDict_k, "setObject:forKey:", (void *)d_set, 0); add(NSDict_k, "objectForKey:", (void *)d_get, 0);
    add(NSDict_k, "removeObjectForKey:", (void *)d_remove, 0); add(NSDict_k, "addEntriesFromDictionary:", (void *)d_add_all, 0);
    add(NSDict_k, "mutableCopy", (void *)d_copy, 0);
    NSValue_k = mkcls("NSValue", NSObject_k, 40);
    add(NSValue_k, "valueWithRect:", (void *)v_rect, 1); add(NSValue_k, "valueWithPoint:", (void *)v_point, 1);
    add(NSValue_k, "rectValue", (void *)v_rectv, 0); add(NSValue_k, "pointValue", (void *)v_pointv, 0);
    NSNumber_k = mkcls("NSNumber", NSObject_k, 40);
    add(NSNumber_k, "numberWithLong:", (void *)n_long, 1); add(NSNumber_k, "numberWithBool:", (void *)n_bool, 1); add(NSNumber_k, "longValue", (void *)n_longv, 0);
    NSDate_k = mkcls("NSDate", NSObject_k, 40);
    add(NSDate_k, "dateWithTimeIntervalSinceReferenceDate:", (void *)dt_ref, 1); add(NSDate_k, "timeIntervalSinceReferenceDate", (void *)dt_refv, 0);
    NSData_k = mkcls("NSMutableData", NSObject_k, 24);
    cls_t *ndd = mkcls("NSData", NSObject_k, 24);
    NSData_k->super = ndd;
    add(ndd, "dataWithBytes:length:", (void *)dt_bytes_len, 1); add(NSData_k, "data", (void *)dt_data, 1); add(ndd, "dataWithContentsOfURL:", (void *)dt_contents_url, 1);
    add(NSData_k, "bytes", (void *)dt_bytes, 0); add(NSData_k, "length", (void *)dt_len, 0); add(NSData_k, "copy", (void *)dt_copy, 0);
    add(NSData_k, "appendBytes:length:", (void *)dt_append, 0); add(NSData_k, "dealloc", (void *)dt_dealloc, 0);
    add(NSData_k, "writeToFile:atomically:", (void *)dt_write_file, 0); add(NSData_k, "writeToURL:atomically:", (void *)dt_write_url, 0);
    NSURL_k = mkcls("NSURL", NSObject_k, 16);
    add(NSURL_k, "fileURLWithPath:", (void *)u_file, 1); add(NSURL_k, "URLWithString:", (void *)u_string, 1);
    add(NSURL_k, "absoluteString", (void *)u_abs, 0); add(NSURL_k, "path", (void *)u_path, 0);
    NSColor_k = mkcls("NSColor", NSObject_k, 40);
    add(NSColor_k, "colorWithCalibratedRed:green:blue:alpha:", (void *)c_rgba, 1); add(NSColor_k, "colorUsingColorSpaceName:", (void *)c_self, 0);
    add(NSColor_k, "redComponent", (void *)c_r, 0); add(NSColor_k, "greenComponent", (void *)c_g, 0); add(NSColor_k, "blueComponent", (void *)c_b, 0); add(NSColor_k, "alphaComponent", (void *)c_a, 0);
    NSNC_k = mkcls("NSNotificationCenter", NSObject_k, 8);
    add(NSNC_k, "defaultCenter", (void *)nc_default, 1); add(NSNC_k, "postNotificationName:object:userInfo:", (void *)nc_post, 0);
    add(NSNC_k, "postNotificationName:object:", (void *)nc_post2, 0);
    NSCS_k = mkcls("NSCharacterSet", NSObject_k, 8);
    add(NSCS_k, "characterSetWithCharactersInString:", (void *)cs_with, 1); add(NSCS_k, "whitespaceCharacterSet", (void *)cs_ws, 1);
    NSImage_k = mkcls("NSImage", NSObject_k, 40);
    add(NSImage_k, "initWithCGImage:size:", (void *)im_init_cg, 0); add(NSImage_k, "size", (void *)im_size, 0);
    add(NSImage_k, "CGImageForProposedRect:context:hints:", (void *)im_cg, 0);
    NSAttr_k = mkcls("NSAttributedString", NSObject_k, 16);
    add(NSAttr_k, "initWithString:", (void *)attr_init, 0); add(NSAttr_k, "string", (void *)attr_string, 0);
    mkcls("NSGraphicsContext", NSObject_k, 8);
    mkcls("NSFont", NSObject_k, 8);
}

/* ---------------------------------------------------------------- o que o AppKit faz para um contexto de bitmap */
int cx_bm_pattern_color(const void *c, const void *color, int stroke);
int cx_bm_set_cap(const void *, int); int cx_bm_set_join(const void *, int); int cx_bm_set_aa(const void *, int);
int cx_bm_move(const void *, double, double); int cx_bm_line(const void *, double, double);
int cx_bm_curve(const void *, double, double, double, double, double, double); int cx_bm_quad(const void *, double, double, double, double);
int cx_bm_close(const void *); int cx_bm_begin(const void *); int cx_bm_rect(const void *, double, double, double, double);
int cx_bm_fill(const void *, int); int cx_bm_stroke(const void *); int cx_bm_draw(const void *, int);
int cx_bm_set_stroke(const void *, uint32_t); void cx_bm_set_fill(const void *, uint32_t); int cx_bm_set_line_width(const void *, double);
int cx_bm_set_alpha(const void *, double); int cx_bm_save(const void *); int cx_bm_restore(const void *); int cx_bm_fill_rect(const void *, double, double, double, double);
int cx_bm_ellipse(const void *, double, double, double, double);
static uint32_t argb(double r, double g, double b, double a) { return (uint32_t)(a * 255 + .5) << 24 | (uint32_t)(r * 255 + .5) << 16 | (uint32_t)(g * 255 + .5) << 8 | (uint32_t)(b * 255 + .5); }
void CGContextSetFillColorWithColor(void *c, void *k) { col_t *o = k; if (!cx_bm_pattern_color(c, k, 0)) cx_bm_set_fill(c, argb(o->r, o->g, o->b, o->a)); }
void CGContextSetStrokeColorWithColor(void *c, void *k) { col_t *o = k; if (!cx_bm_pattern_color(c, k, 1)) cx_bm_set_stroke(c, argb(o->r, o->g, o->b, o->a)); }
void CGContextSetRGBFillColor(void *c, double r, double g, double b, double a) { cx_bm_set_fill(c, argb(r, g, b, a)); }
void CGContextSetRGBStrokeColor(void *c, double r, double g, double b, double a) { cx_bm_set_stroke(c, argb(r, g, b, a)); }
void CGContextSetAlpha(void *c, double a) { cx_bm_set_alpha(c, a); }
void CGContextSetLineWidth(void *c, double w) { cx_bm_set_line_width(c, w); }
void CGContextSetLineCap(void *c, int v) { cx_bm_set_cap(c, v); }
void CGContextSetLineJoin(void *c, int v) { cx_bm_set_join(c, v); }
void CGContextSaveGState(void *c) { cx_bm_save(c); }
void CGContextRestoreGState(void *c) { cx_bm_restore(c); }
void CGContextBeginPath(void *c) { cx_bm_begin(c); }
void CGContextMoveToPoint(void *c, double x, double y) { cx_bm_move(c, x, y); }
void CGContextAddLineToPoint(void *c, double x, double y) { cx_bm_line(c, x, y); }
void CGContextAddCurveToPoint(void *c, double a, double b, double d, double e, double x, double y) { cx_bm_curve(c, a, b, d, e, x, y); }
void CGContextClosePath(void *c) { cx_bm_close(c); }
void CGContextAddRect(void *c, R r) { cx_bm_rect(c, r.o.x, r.o.y, r.s.w, r.s.h); }
void CGContextAddEllipseInRect(void *c, R r) { cx_bm_ellipse(c, r.o.x, r.o.y, r.s.w, r.s.h); }
void CGContextFillPath(void *c) { cx_bm_fill(c, 0); }
void CGContextStrokePath(void *c) { cx_bm_stroke(c); }
void CGContextDrawPath(void *c, int m) { cx_bm_draw(c, m); }
void CGContextFillRect(void *c, R r) { cx_bm_fill_rect(c, r.o.x, r.o.y, r.s.w, r.s.h); }
void CGContextStrokeRect(void *c, R r) { cx_bm_begin(c); cx_bm_rect(c, r.o.x, r.o.y, r.s.w, r.s.h); cx_bm_stroke(c); }
typedef struct { int type; P *pts; } el_t;
void CGPathApply(const void *path, void *info, void (*fn)(void *, const void *));
static void addpath_el(void *c, const void *e0) {
    const el_t *e = e0;
    if (e->type == 0) cx_bm_move(c, e->pts[0].x, e->pts[0].y);
    else if (e->type == 1) cx_bm_line(c, e->pts[0].x, e->pts[0].y);
    else if (e->type == 2) cx_bm_quad(c, e->pts[0].x, e->pts[0].y, e->pts[1].x, e->pts[1].y);
    else if (e->type == 3) cx_bm_curve(c, e->pts[0].x, e->pts[0].y, e->pts[1].x, e->pts[1].y, e->pts[2].x, e->pts[2].y);
    else if (e->type == 4) cx_bm_close(c);
}
void CGContextAddPath(void *c, const void *p) { CGPathApply(p, c, addpath_el); }
/* o resto que o Core Graphics chama de outros arquivos */
uint32_t cx_cg_fill_argb(void) { return 0xFF000000u; }
void cx_cg_note_blend(int m) { (void)m; }
int cx_win_current(double *x, double *y) { (void)x; (void)y; return 0; }
void cx_win_clip(void) {}
int mos_img_decode(const uint8_t *d, size_t n, void *o) { (void)d; (void)n; (void)o; return -1; }
const uint8_t *cx_colorsync_profile_bytes(const void *p, size_t *n) { (void)p; *n = 0; return 0; }
const void *kCFBooleanTrue = "t";
long ct_host_sys5(long n, long op, long face, long tag, void *buf, long cap) { (void)n; (void)op; (void)face; (void)tag; (void)buf; (void)cap; return -1; }
void *CTFontCreateWithName(void *n, double s, const void *m) { (void)n; (void)s; (void)m; return 0; }
void *CTFontCreateWithGraphicsFont(void *f, double s, const void *m, void *a) { (void)f; (void)s; (void)m; (void)a; return 0; }
void CTFontDrawGlyphs(void *f, const uint16_t *g, const P *p, size_t n, void *ctx) { (void)f; (void)g; (void)p; (void)n; (void)ctx; }
double CTFontGetAdvancesForGlyphs(void *f, int o, const uint16_t *g, S *adv, long n) { (void)f; (void)o; (void)g; for (long i = 0; adv && i < n; i++) { adv[i].w = 0; adv[i].h = 0; } return 0; }
uint8_t CTFontGetGlyphsForCharacters(void *f, const uint16_t *c, uint16_t *g, long n) { (void)f; for (long i = 0; i < n; i++) g[i] = c[i]; return 1; }
void *CTFontCreatePathForGlyph(void *f, uint16_t g, const void *m) { (void)f; (void)g; (void)m; return 0; }
void cx_pdfkit_view_init(void) {}

/* ---------------------------------------------------------------- teste */
void cx_pdfkit_init(void);
extern char OBJC_CLASS_$_PDFDocument[] __asm__("OBJC_CLASS_$_PDFDocument");
extern char OBJC_CLASS_$_PDFPage[] __asm__("OBJC_CLASS_$_PDFPage");
extern char OBJC_CLASS_$_PDFAnnotation[] __asm__("OBJC_CLASS_$_PDFAnnotation");
extern char OBJC_CLASS_$_PDFOutline[] __asm__("OBJC_CLASS_$_PDFOutline");
extern char OBJC_CLASS_$_PDFDestination[] __asm__("OBJC_CLASS_$_PDFDestination");
extern const void *PDFAnnotationSubtypeSquare, *PDFAnnotationKeyContents, *PDFDocumentTitleAttribute;
void *CGImageCreate(size_t, size_t, size_t, size_t, size_t, void *, uint32_t, void *, const void *, int, int);
void *CGDataProviderCreateWithData(void *, const void *, size_t, void *);
void *CGColorSpaceCreateDeviceRGB(void);
static int fails;
#define CHECK(c, m) do { int ok_ = (c); printf("%s %s\n", ok_ ? "ok  " : "FAIL", m); if (!ok_) fails++; } while (0)
static id open_doc(const char *path) {
    id url = MSG(id, cx_class_named("NSURL"), "fileURLWithPath:", mkstr(path));
    return MSG(id, MSG0(id, OBJC_CLASS_$_PDFDocument, "alloc"), "initWithURL:", url);
}
static const char *S_(id s) { return s ? cx_cstr(s) : "(nil)"; }
static int contains(id s, const char *t) { return s && strstr(cx_cstr(s), t) != 0; }
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "uso: pdfkit_host w.pdf wenc.pdf saida-dir\n"); return 2; }
    foundation_init();
    cx_pdfkit_init();
    char out[1024];
    /* 1 leitura */
    id doc = open_doc(argv[1]);
    CHECK(doc && MSG0(long, doc, "pageCount") == 2, "PDFDocument initWithURL: 2 páginas");
    id p0 = MSG(id, doc, "pageAtIndex:", 0ul), p1 = MSG(id, doc, "pageAtIndex:", 1ul);
    R b0 = ((R (*)(id, SEL, long))objc_msgSend_stret)(p0, "boundsForBox:", 0l), b1 = ((R (*)(id, SEL, long))objc_msgSend_stret)(p1, "boundsForBox:", 1l);
    CHECK(b0.s.w == 400 && b0.s.h == 450 && b1.s.w == 300 && b1.s.h == 200, "boundsForBox: das duas páginas (400x450, 300x200)");
    CHECK(MSG(long, doc, "indexForPage:", p1) == 1 && MSG0(id, p1, "document") == doc, "indexForPage: e page.document");
    CHECK(!strcmp(S_(MSG0(id, p1, "label")), "2"), "label da página 2");
    id attrs = MSG0(id, doc, "documentAttributes");
    CHECK(!strcmp(S_(MSG(id, attrs, "objectForKey:", mkstr("Title"))), "Relatório de teste — mOS"), "documentAttributes Title (UTF-16 do Info)");
    id kw = MSG(id, attrs, "objectForKey:", mkstr("Keywords"));
    CHECK(kw && MSG0(long, kw, "count") == 2 && !strcmp(S_(MSG(id, kw, "objectAtIndex:", 1ul)), "quartz"), "Keywords como array");
    CHECK(MSG(id, attrs, "objectForKey:", mkstr("CreationDate")) != 0, "CreationDate como NSDate");
    id str = MSG0(id, doc, "string");
    CHECK(contains(str, "Hamburg Wxyz 123") && contains(str, "Segunda pagina"), "document.string com o texto das duas páginas");
    CHECK(MSG0(long, p0, "numberOfCharacters") > 20, "numberOfCharacters");
    /* busca */
    id found = MSG(id, doc, "findString:withOptions:", mkstr("hamburg"), 1ul);
    CHECK(found && MSG0(long, found, "count") == 1, "findString: sem caixa acha 1");
    id fs = found ? MSG(id, found, "objectAtIndex:", 0ul) : 0;
    CHECK(fs && !strcmp(S_(MSG0(id, fs, "string")), "Hamburg"), "seleção encontrada = \"Hamburg\"");
    R fb = fs ? ((R (*)(id, SEL, id))objc_msgSend_stret)(fs, "boundsForPage:", p0) : (R){{0, 0}, {0, 0}};
    CHECK(fb.o.x > 15 && fb.o.x < 25 && fb.o.y > 370 && fb.o.y < 385 && fb.s.w > 80 && fb.s.w < 140, "boundsForPage: da seleção no lugar do texto");
    CHECK(posted_count("PDFDidFindMatch") == 1 && posted_count("PDFDidEndFind") == 1, "notificações de busca");
    id found2 = MSG(id, doc, "findString:withOptions:", mkstr("PAGINA"), 1ul);
    CHECK(found2 && MSG0(long, found2, "count") == 1 && MSG0(id, MSG(id, found2, "objectAtIndex:", 0ul), "pages") && MSG(id, MSG0(id, MSG(id, found2, "objectAtIndex:", 0ul), "pages"), "objectAtIndex:", 0ul) == p1, "busca acha na página 2");
    CHECK(MSG0(long, MSG(id, doc, "findString:withOptions:", mkstr("hamburg"), 0ul), "count") == 0, "busca com caixa não acha \"hamburg\"");
    id next = MSG(id, doc, "findString:fromSelection:withOptions:", mkstr("a"), fs, 1ul);
    CHECK(next && MSG(id, MSG0(id, next, "pages"), "objectAtIndex:", 0ul) == p1 && !strcmp(S_(MSG0(id, next, "string")), "a"), "findString:fromSelection: acha a próxima (na página 2)");
    /* palavra, linha, índice */
    P mid = {fb.o.x + fb.s.w / 2, fb.o.y + fb.s.h / 2};
    long ci = MSG(long, p0, "characterIndexAtPoint:", mid);
    CHECK(ci >= 0, "characterIndexAtPoint:");
    CHECK(!strcmp(S_(MSG0(id, MSG(id, p0, "selectionForWordAtPoint:", mid), "string")), "Hamburg"), "selectionForWordAtPoint:");
    CHECK(!strcmp(S_(MSG0(id, MSG(id, p0, "selectionForLineAtPoint:", mid), "string")), "Hamburg Wxyz 123"), "selectionForLineAtPoint:");
    id all = MSG0(id, doc, "selectionForEntireDocument");
    CHECK(MSG0(long, MSG0(id, all, "pages"), "count") == 2 && contains(MSG0(id, all, "string"), "Segunda"), "selectionForEntireDocument");
    CHECK(MSG0(long, MSG0(id, all, "selectionsByLine"), "count") >= 3, "selectionsByLine");
    /* anotações e outline do arquivo */
    id an = MSG0(id, p0, "annotations");
    long nan = an ? MSG0(long, an, "count") : 0;
    int url_ok = 0, dest_ok = 0;
    for (long i = 0; i < nan; i++) {
        id a = MSG(id, an, "objectAtIndex:", (unsigned long)i);
        id u = MSG0(id, a, "URL");
        if (u && !strcmp(S_(MSG0(id, u, "absoluteString")), "https://example.org/mos")) url_ok = 1;
        id d = MSG0(id, a, "destination");
        if (d && MSG0(id, d, "page") == p1) dest_ok = 1;
    }
    CHECK(nan == 2 && url_ok, "2 anotações Link, uma com URL");
    CHECK(dest_ok, "link com destino nomeado resolvido para a página 2");
    CHECK(!strcmp(S_(MSG0(id, MSG(id, an, "objectAtIndex:", 0ul), "type")), "Link"), "type sem a barra (\"Link\")");
    id root = MSG0(id, doc, "outlineRoot");
    CHECK(root && MSG0(long, root, "numberOfChildren") == 2, "outlineRoot com 2 itens");
    id c2 = root ? MSG(id, root, "childAtIndex:", 1ul) : 0;
    CHECK(c2 && !strcmp(S_(MSG0(id, c2, "label")), "Capítulo 2") && MSG0(id, MSG0(id, c2, "destination"), "page") == p1, "item do outline: rótulo e destino");
    /* 2 edição e regravação */
    id cp = MSG0(id, p1, "copy");
    MSG(void, doc, "insertPage:atIndex:", cp, 0ul);
    MSG(void, p0, "setRotation:", 90l);
    id sq = MSG(id, MSG0(id, OBJC_CLASS_$_PDFAnnotation, "alloc"), "initWithBounds:forType:withProperties:", ((R){{50, 50}, {100, 40}}), (id)PDFAnnotationSubtypeSquare, (id)0);
    MSG(void, sq, "setContents:", mkstr("nota quadrada"));
    MSG(void, sq, "setColor:", cx_color(1, 0, 0, 1));
    MSG(void, p0, "addAnnotation:", sq);
    MSG(void, p0, "removeAnnotation:", MSG(id, MSG0(id, p0, "annotations"), "objectAtIndex:", 0ul));
    id na = MSG0(id, attrs, "mutableCopy");
    MSG(void, na, "setObject:forKey:", mkstr("Título novo"), (id)PDFDocumentTitleAttribute);
    MSG(void, doc, "setDocumentAttributes:", na);
    id kid = MSG0(id, MSG0(id, OBJC_CLASS_$_PDFOutline, "alloc"), "init");
    MSG(void, kid, "setLabel:", mkstr("Novo item"));
    MSG(void, kid, "setDestination:", MSG(id, MSG0(id, OBJC_CLASS_$_PDFDestination, "alloc"), "initWithPage:atPoint:", p0, ((P){0, 400})));
    MSG(void, root, "insertChild:atIndex:", kid, 0ul);
    snprintf(out, sizeof out, "%s/edit.pdf", argv[3]);
    CHECK(MSG(int8_t, doc, "writeToFile:", mkstr(out)), "writeToFile:");
    id d2 = open_doc(out);
    CHECK(d2 && MSG0(long, d2, "pageCount") == 3, "regravado tem 3 páginas");
    id q0 = MSG(id, d2, "pageAtIndex:", 0ul), q1 = MSG(id, d2, "pageAtIndex:", 1ul);
    CHECK(contains(MSG0(id, q0, "string"), "Segunda pagina") && contains(MSG0(id, q1, "string"), "Hamburg"), "ordem nova das páginas (cópia da 2ª no início)");
    CHECK(MSG0(long, q1, "rotation") == 90, "rotação gravada");
    id an2 = MSG0(id, q1, "annotations");
    int sq_ok = 0;
    for (long i = 0; i < MSG0(long, an2, "count"); i++) { id a = MSG(id, an2, "objectAtIndex:", (unsigned long)i); if (!strcmp(S_(MSG0(id, a, "type")), "Square") && !strcmp(S_(MSG0(id, a, "contents")), "nota quadrada")) sq_ok = 1; }
    CHECK(sq_ok && MSG0(long, an2, "count") == 2, "anotação nova gravada e a removida saiu");
    CHECK(!strcmp(S_(MSG(id, MSG0(id, d2, "documentAttributes"), "objectForKey:", mkstr("Title"))), "Título novo"), "Title novo gravado");
    id r2 = MSG0(id, d2, "outlineRoot");
    CHECK(r2 && MSG0(long, r2, "numberOfChildren") == 3 && !strcmp(S_(MSG0(id, MSG(id, r2, "childAtIndex:", 0ul), "label")), "Novo item") &&
              MSG0(id, MSG0(id, MSG(id, r2, "childAtIndex:", 0ul), "destination"), "page") == q1, "outline novo gravado (item aponta para a página certa)");
    MSG(void, d2, "removePageAtIndex:", 0ul);
    MSG(void, d2, "exchangePageAtIndex:withPageAtIndex:", 0ul, 1ul);
    id dd = MSG0(id, d2, "dataRepresentation");
    id d3 = MSG(id, MSG0(id, OBJC_CLASS_$_PDFDocument, "alloc"), "initWithData:", dd);
    CHECK(d3 && MSG0(long, d3, "pageCount") == 2 && contains(MSG0(id, MSG(id, d3, "pageAtIndex:", 1ul), "string"), "Hamburg"), "remover e trocar páginas, dataRepresentation e initWithData:");
    /* 3 cifra */
    id e = open_doc(argv[2]);
    CHECK(e && MSG0(int8_t, e, "isEncrypted") && MSG0(int8_t, e, "isLocked") && MSG0(long, e, "pageCount") == 0, "cifrado: travado e sem páginas");
    CHECK(!MSG(int8_t, e, "unlockWithPassword:", mkstr("errada")), "senha errada não abre");
    CHECK(MSG(int8_t, e, "unlockWithPassword:", mkstr("usuario")) && MSG0(long, e, "pageCount") == 2 && !MSG0(int8_t, e, "isLocked"), "senha de usuário abre (2 páginas)");
    CHECK(!MSG0(int8_t, e, "allowsCopying") && MSG0(int8_t, e, "allowsPrinting"), "permissões: imprime, não copia");
    CHECK(contains(MSG0(id, e, "string"), "Hamburg") && posted_count("PDFDidUnlock") == 1, "texto decifrado e notificação de destravar");
    /* 4 página a partir de imagem e miniatura */
    static uint32_t px[64 * 32];
    for (int i = 0; i < 64 * 32; i++) px[i] = 0xFF0060C0u;
    void *prov = CGDataProviderCreateWithData(0, px, sizeof px, 0);
    void *cgimg = CGImageCreate(64, 32, 8, 32, 64 * 4, CGColorSpaceCreateDeviceRGB(), 6 | 0x2000, prov, 0, 0, 0);
    id nsimg = MSG(id, MSG0(id, cx_class_named("NSImage"), "alloc"), "initWithCGImage:size:", cgimg, ((S){64, 32}));
    id ip = MSG(id, MSG0(id, OBJC_CLASS_$_PDFPage, "alloc"), "initWithImage:", nsimg);
    R ib = ((R (*)(id, SEL, long))objc_msgSend_stret)(ip, "boundsForBox:", 0l);
    CHECK(ib.s.w == 64 && ib.s.h == 32, "initWithImage: página do tamanho da imagem");
    id nd = MSG0(id, MSG0(id, OBJC_CLASS_$_PDFDocument, "alloc"), "init");
    MSG(void, nd, "insertPage:atIndex:", ip, 0ul);
    snprintf(out, sizeof out, "%s/img.pdf", argv[3]);
    CHECK(MSG(int8_t, nd, "writeToFile:", mkstr(out)), "documento novo com a página de imagem gravado");
    id th = MSG(id, p1, "thumbnailOfSize:forBox:", ((S){60, 60}), 1l);
    S ts = th ? MSG0(S, th, "size") : (S){0, 0};
    CHECK(th && ts.w == 60 && ts.h == 40, "thumbnailOfSize: mantém a proporção (60x40)");
    CHECK(missing == 0, "nenhum método ausente no caminho testado");
    printf("%d falha(s)\n", fails);
    return fails ? 1 : 0;
}
