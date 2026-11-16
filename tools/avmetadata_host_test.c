/* Host: userland/libsystem/avmetadata.c (AVMetadataItem/AVMutableMetadataItem) com um runtime mínimo (objc_msgSend é um
 * trampolim que acha o IMP e salta). gcc -std=gnu11 -O1 -I userland/libsystem/kshim -I userland/libsystem ... → "avmetadata ok" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cmt_t;
extern char AVMetadataItem_c[] __asm__("OBJC_CLASS_$_AVMetadataItem");
extern char AVMutableMetadataItem_c[] __asm__("OBJC_CLASS_$_AVMutableMetadataItem");
void cx_avmetadata_init(void);
char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
extern const void *AVMetadataCommonIdentifierTitle, *AVMetadataIdentifierQuickTimeMetadataTitle, *AVMetadataKeySpaceiTunes, *AVFileTypeAIFF;

static char FAKE;
enum { K_STR = 1, K_NUM, K_ARR, K_CLASS, K_DATE };
typedef struct fobj { void *isa; int kind; const char *str; double num; struct fobj **v; int n, cap; } fobj;
static fobj *fnew(int k) { fobj *o = calloc(1, sizeof *o); o->isa = &FAKE; o->kind = k; return o; }
void *cx_str(const char *s) { fobj *o = fnew(K_STR); o->str = strdup(s); return o; }
void *cx_str_n(const char *s, unsigned long n) { fobj *o = fnew(K_STR); char *t = malloc(n + 1); memcpy(t, s, n); t[n] = 0; o->str = t; return o; }
typedef struct { void *isa; unsigned long long flags; const char *s; unsigned long long n; } cfc;
const char *cx_cstr(void *o) {
    if (!o) return 0;
    if (((void **)o)[0] == cx_cfconst_isa) return ((cfc *)o)->s;
    return ((fobj *)o)->isa == &FAKE && ((fobj *)o)->kind == K_STR ? ((fobj *)o)->str : 0;
}
int cx_is_str(void *o) { return cx_cstr(o) != 0; }
static fobj cls_num = {&FAKE, K_CLASS, "NSNumber"}, cls_str = {&FAKE, K_CLASS, "NSString"}, cls_date = {&FAKE, K_CLASS, "NSDate"},
            cls_data = {&FAKE, K_CLASS, "NSData"}, cls_marr = {&FAKE, K_CLASS, "NSMutableArray"};
static char ns_object_k;
typedef struct { void *key; unsigned extra; void *imps[64]; const char *sels[64]; int n; void *super; } cls_t;
static cls_t tab[4]; static int ntab;
static cls_t *Kc(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; tab[ntab].key = c; return &tab[ntab++]; }
static cls_t *Kf(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; return 0; }
void *cx_class_named(const char *n) {
    if (!strcmp(n, "NSObject")) return &ns_object_k;
    if (!strcmp(n, "NSNumber")) return &cls_num;
    if (!strcmp(n, "NSString")) return &cls_str;
    if (!strcmp(n, "NSDate")) return &cls_date;
    if (!strcmp(n, "NSData")) return &cls_data;
    if (!strcmp(n, "NSMutableArray")) return &cls_marr;
    return 0;
}
void *cx_define_class(void *c, void *m, const char *name, void *super, unsigned extra) { (void)m; (void)name; Kc(c)->extra = extra; Kc(c)->super = super; return c; }
void cx_add_method(void *c, const char *s, void *f) { cls_t *k = Kc(c); k->sels[k->n] = s; k->imps[k->n++] = f; }
void cx_add_cmethod(void *c, const char *s, void *f) { cx_add_method(c, s, f); }
void *cx_alloc_instance(void *c) { void **o = calloc(1, 16 + Kc(c)->extra + 32); o[0] = c; return o; }
static void *imp(void *c, const char *s) { for (cls_t *k = Kf(c); k; k = Kf(k->super)) for (int i = 0; i < k->n; i++) if (!strcmp(k->sels[i], s)) return k->imps[i]; return 0; }
void *class_getMethodImplementation(void *c, const char *s) { (void)c; (void)s; return 0; }
const char *sel_registerName(const char *s) { return s; }
void *objc_retain(void *o) { return o; }
void objc_release(void *o) { (void)o; }
void *objc_autorelease(void *o) { return o; }
static long f_ret0(void) { return 0; }
static void *f_self(fobj *s, const char *c) { (void)c; return s; }
static signed char f_kind(fobj *s, const char *c, fobj *k) {
    (void)c;
    if (((void **)s)[0] == cx_cfconst_isa) return k == &cls_str;
    return (k == &cls_num && s->kind == K_NUM) || (k == &cls_str && s->kind == K_STR) || (k == &cls_date && s->kind == K_DATE);
}
static signed char f_eq(fobj *a, const char *c, fobj *b) {
    (void)c;
    const char *x = cx_cstr(a), *y = cx_cstr(b);
    if (x && y) return !strcmp(x, y);
    return a == b || (a && b && a->kind == K_NUM && b->kind == K_NUM && a->num == b->num);
}
static long long f_ll(fobj *s, const char *c) { (void)c; return (long long)s->num; }
static double f_dbl(fobj *s, const char *c) { (void)c; return s->kind == K_STR ? atof(s->str) : s->num; }
static void *f_desc(fobj *s, const char *c) { (void)c; char b[64]; snprintf(b, sizeof b, "%g", s->num); return cx_str(b); }
static void *f_numd(fobj *s, const char *c, double v) { (void)s; (void)c; fobj *o = fnew(K_NUM); o->num = v; return o; }
static void *f_arr(fobj *s, const char *c) { (void)s; (void)c; return fnew(K_ARR); }
static unsigned long long f_count(fobj *s, const char *c) { (void)c; return (unsigned long long)s->n; }
static void *f_at(fobj *s, const char *c, unsigned long long i) { (void)c; return s->v[i]; }
static void f_add(fobj *s, const char *c, fobj *o) { (void)c; if (s->n == s->cap) { s->cap = s->cap ? 2 * s->cap : 8; s->v = realloc(s->v, s->cap * sizeof *s->v); } s->v[s->n++] = o; }
void *fake_lookup(void *self, const char *sel) {
    if (!self) return (void *)f_ret0;
    void *f = imp(self, sel);
    if (f) return f;
    void *isa = ((void **)self)[0];
    if (isa != &FAKE && isa != cx_cfconst_isa) { f = imp(isa, sel); return f ? f : (void *)f_ret0; }
    if (!strcmp(sel, "isKindOfClass:")) return (void *)f_kind;
    if (!strcmp(sel, "isEqual:")) return (void *)f_eq;
    if (!strcmp(sel, "copy")) return (void *)f_self;
    if (!strcmp(sel, "longLongValue")) return (void *)f_ll;
    if (!strcmp(sel, "doubleValue")) return (void *)f_dbl;
    if (!strcmp(sel, "description")) return (void *)f_desc;
    if (!strcmp(sel, "numberWithDouble:")) return (void *)f_numd;
    if (!strcmp(sel, "array")) return (void *)f_arr;
    if (!strcmp(sel, "count")) return (void *)f_count;
    if (!strcmp(sel, "objectAtIndex:")) return (void *)f_at;
    if (!strcmp(sel, "addObject:")) return (void *)f_add;
    fprintf(stderr, "sem seletor: %s\n", sel);
    return (void *)f_ret0;
}
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\n"
        "push %rbp\nmov %rsp,%rbp\nsub $0xc0,%rsp\n"
        "mov %rdi,0(%rsp)\nmov %rsi,8(%rsp)\nmov %rdx,16(%rsp)\nmov %rcx,24(%rsp)\nmov %r8,32(%rsp)\nmov %r9,40(%rsp)\nmov %rax,48(%rsp)\n"
        "movdqu %xmm0,64(%rsp)\nmovdqu %xmm1,80(%rsp)\n"
        "call fake_lookup\nmov %rax,%r11\n"
        "mov 0(%rsp),%rdi\nmov 8(%rsp),%rsi\nmov 16(%rsp),%rdx\nmov 24(%rsp),%rcx\nmov 32(%rsp),%r8\nmov 40(%rsp),%r9\nmov 48(%rsp),%rax\n"
        "movdqu 64(%rsp),%xmm0\nmovdqu 80(%rsp),%xmm1\n"
        "leave\njmp *%r11\n");

static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
#define CALL(o, sel, R, ...) ((R (*)())imp(((void **)(o))[0], sel))(o, sel, ##__VA_ARGS__)
#define CCALL(k, sel, R, ...) ((R (*)())imp(k, sel))(k, sel, ##__VA_ARGS__)
static const char *S(void *o) { const char *s = cx_cstr(o); return s ? s : "(nil)"; }

int main(void) {
    cx_avmetadata_init();
    expect(!strcmp(S((void *)AVMetadataCommonIdentifierTitle), "common/title") && !strcmp(S((void *)AVFileTypeAIFF), "public.aiff-audio") &&
           !strcmp(S((void *)AVMetadataIdentifierQuickTimeMetadataTitle), "mdta/com.apple.quicktime.title"), "constantes");
    fobj *tit2 = fnew(K_NUM); tit2->num = (double)0x54495432;   /* 'TIT2' */
    void *id1 = CCALL(AVMetadataItem_c, "identifierForKey:keySpace:", void *, tit2, cx_str("org.id3"));
    expect(!strcmp(S(id1), "org.id3/TIT2"), "identificador com chave numérica de 4 caracteres");
    void *id2 = CCALL(AVMetadataItem_c, "identifierForKey:keySpace:", void *, cx_str("\xa9" "alb"), cx_str("itsk"));
    expect(!strcmp(S(id2), "itsk/%A9alb"), "byte fora do ASCII em %XX");
    expect(!strcmp(S(CCALL(AVMetadataItem_c, "keyForIdentifier:", void *, id2)), "\xa9" "alb") &&
           !strcmp(S(CCALL(AVMetadataItem_c, "keySpaceForIdentifier:", void *, id2)), "itsk"), "ida e volta do identificador");
    expect(!strcmp(S(CCALL(AVMetadataItem_c, "keySpaceForIdentifier:", void *, cx_str("common/title"))), "comn"), "common -> comn");
    void *m = CCALL(AVMutableMetadataItem_c, "metadataItem", void *);
    CALL(m, "setIdentifier:", void, (void *)AVMetadataIdentifierQuickTimeMetadataTitle);
    CALL(m, "setValue:", void, cx_str("Meu filme"));
    expect(!strcmp(S(CALL(m, "key", void *)), "com.apple.quicktime.title") && !strcmp(S(CALL(m, "keySpace", void *)), "mdta"), "setIdentifier: separa chave e espaço");
    expect(!strcmp(S(CALL(m, "commonKey", void *)), "title") && !strcmp(S(CALL(m, "stringValue", void *)), "Meu filme"), "commonKey e stringValue");
    void *m2 = CCALL(AVMutableMetadataItem_c, "metadataItem", void *);
    CALL(m2, "setKey:", void, cx_str("title")); CALL(m2, "setKeySpace:", void, cx_str("comn"));
    fobj *n42 = fnew(K_NUM); n42->num = 42;
    CALL(m2, "setValue:", void, n42);
    expect(!strcmp(S(CALL(m2, "identifier", void *)), "common/title") && ((fobj *)CALL(m2, "numberValue", void *))->num == 42 &&
           !strcmp(S(CALL(m2, "stringValue", void *)), "42"), "chave + espaço dão o identificador; número");
    void *c2 = CALL(m2, "copy", void *);
    expect(((void **)c2)[0] == (void *)AVMetadataItem_c && !strcmp(S(CALL(c2, "identifier", void *)), "common/title"), "copy imutável");
    fobj *arr = fnew(K_ARR); f_add(arr, 0, m); f_add(arr, 0, m2);
    fobj *r1 = CCALL(AVMetadataItem_c, "metadataItemsFromArray:filteredByIdentifier:", void *, arr, (void *)AVMetadataCommonIdentifierTitle);
    fobj *r2 = CCALL(AVMetadataItem_c, "metadataItemsFromArray:withKey:keySpace:", void *, arr, cx_str("com.apple.quicktime.title"), cx_str("mdta"));
    expect(r1->n == 1 && r1->v[0] == m2 && r2->n == 1 && r2->v[0] == m, "filtros por identificador e por chave/espaço");
    cmt_t t = ((cmt_t (*)(void *, const char *))imp(AVMetadataItem_c, "time"))(m, "time");
    expect(t.flags == 0, "time inválido por padrão");
    if (!fails) puts("avmetadata ok");
    printf("avmetadata: %d/%d\n", total - fails, total);
    return fails != 0;
}
