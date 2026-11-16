/* Host: userland/libsystem/auv3.c com runtime mínimo e uma subclasse de AUAudioUnit feita aqui (como a do WhatsApp).
 * → "auv3: N/N ok" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int OSStatus;
extern char AUAudioUnit_c[] __asm__("OBJC_CLASS_$_AUAudioUnit"), AUAudioUnitBus_c[] __asm__("OBJC_CLASS_$_AUAudioUnitBus");
extern char AUAudioUnitBusArray_c[] __asm__("OBJC_CLASS_$_AUAudioUnitBusArray"), AUParameterTree_c[] __asm__("OBJC_CLASS_$_AUParameterTree");
void cx_auv3_init(void);
void *_NSConcreteGlobalBlock[4];
static char FAKE, NSOBJ;
enum { K_STR = 1, K_ARR, K_FMT, K_CLASS };
typedef struct fo { void *isa; int kind; const char *s; struct fo **v; int n, cap; unsigned ch; } fo;
static fo *fnew(int k) { fo *o = calloc(1, sizeof *o); o->isa = &FAKE; o->kind = k; return o; }
void *cx_str(const char *s) { fo *o = fnew(K_STR); o->s = strdup(s); return o; }
const char *cx_cstr(void *o) { return o && ((fo *)o)->isa == &FAKE && ((fo *)o)->kind == K_STR ? ((fo *)o)->s : 0; }
static fo cls_arr = {&FAKE, K_CLASS, "NSArray"}, cls_marr = {&FAKE, K_CLASS, "NSMutableArray"};
typedef struct { void *key; const char *name; unsigned extra; void *super; const char *sels[80]; void *imps[80]; int n; } cls_t;
static cls_t tab[16]; static int ntab;
static cls_t *Kc(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; tab[ntab].key = c; return &tab[ntab++]; }
static cls_t *Kf(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; return 0; }
void *cx_class_named(const char *n) {
    if (!strcmp(n, "NSObject")) return &NSOBJ;
    if (!strcmp(n, "NSArray")) return &cls_arr;
    if (!strcmp(n, "NSMutableArray")) return &cls_marr;
    for (int i = 0; i < ntab; i++) if (tab[i].name && !strcmp(tab[i].name, n)) return tab[i].key;
    return 0;
}
void *cx_define_class(void *c, void *m, const char *name, void *super, unsigned extra) { (void)m; cls_t *k = Kc(c); k->name = name; k->super = super; k->extra = extra; return c; }
void cx_add_method(void *c, const char *s, void *f) { cls_t *k = Kc(c); k->sels[k->n] = s; k->imps[k->n++] = f; }
void cx_add_cmethod(void *c, const char *s, void *f) { cx_add_method(c, s, f); }
void *cx_alloc_instance(void *c) { void **o = calloc(1, 256); o[0] = c; return o; }
static void *imp(void *c, const char *s) { for (cls_t *k = Kf(c); k; k = Kf(k->super)) for (int i = 0; i < k->n; i++) if (!strcmp(k->sels[i], s)) return k->imps[i]; return 0; }
void *class_getMethodImplementation(void *c, const char *s) { (void)c; (void)s; return 0; }
void *object_getClass(void *o) { return *(void **)o; }
const char *sel_registerName(const char *s) { return s; }
void *objc_retain(void *o) { return o; }
void objc_release(void *o) { (void)o; }
void *objc_autorelease(void *o) { return o; }
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }
static long r0(void) { return 0; }
static void *f_arr(void *k, const char *c) { (void)k; (void)c; return fnew(K_ARR); }
static void push(fo *a, fo *x) { if (a->n == a->cap) { a->cap = a->cap ? a->cap * 2 : 8; a->v = realloc(a->v, a->cap * sizeof *a->v); } a->v[a->n++] = x; }
static void f_add(fo *a, const char *c, fo *x) { (void)c; push(a, x); }
static void f_addall(fo *a, const char *c, fo *b) { (void)c; for (int i = 0; i < b->n; i++) push(a, b->v[i]); }
static unsigned long f_count(fo *a, const char *c) { (void)c; return (unsigned long)a->n; }
static void *f_at(fo *a, const char *c, unsigned long i) { (void)c; return a->v[i]; }
static void *f_self(void *s, const char *c) { (void)c; return s; }
static unsigned f_ch(fo *f, const char *c) { (void)c; return f->ch; }
void *fake_lookup(void *self, const char *sel) {
    if (!self) return (void *)r0;
    void *f = imp(self, sel);
    if (f) return f;
    void *isa = *(void **)self;
    if (isa != &FAKE) { f = imp(isa, sel); return f ? f : (void *)r0; }
    fo *o = self;
    if ((o == &cls_arr || o == &cls_marr) && !strcmp(sel, "array")) return (void *)f_arr;
    if (o->kind == K_ARR) {
        if (!strcmp(sel, "addObject:")) return (void *)f_add;
        if (!strcmp(sel, "addObjectsFromArray:")) return (void *)f_addall;
        if (!strcmp(sel, "count")) return (void *)f_count;
        if (!strcmp(sel, "objectAtIndex:")) return (void *)f_at;
        if (!strcmp(sel, "copy")) return (void *)f_self;
    }
    if (o->kind == K_FMT && !strcmp(sel, "channelCount")) return (void *)f_ch;
    return (void *)r0;
}
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\npush %rbp\nmov %rsp,%rbp\nsub $0x60,%rsp\n"
        "mov %rdi,0(%rsp)\nmov %rsi,8(%rsp)\nmov %rdx,16(%rsp)\nmov %rcx,24(%rsp)\nmov %r8,32(%rsp)\nmov %r9,40(%rsp)\n"
        "movdqu %xmm0,48(%rsp)\nmovdqu %xmm1,64(%rsp)\ncall fake_lookup\nmov %rax,%r11\n"
        "mov 0(%rsp),%rdi\nmov 8(%rsp),%rsi\nmov 16(%rsp),%rdx\nmov 24(%rsp),%rcx\nmov 32(%rsp),%r8\nmov 40(%rsp),%r9\n"
        "movdqu 48(%rsp),%xmm0\nmovdqu 64(%rsp),%xmm1\nleave\njmp *%r11\n");
static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
#define C(o, s, R, ...) ((R (*)())imp(*(void **)(o), s))(o, s, ##__VA_ARGS__)
typedef struct { unsigned type, sub, manuf, flags, mask; } acdesc;
/* a subclasse: internalRenderBlock escreve 0,25 em cada amostra */
static char MyAU_c[256], MyAU_m[256];
typedef struct { unsigned nb, pad; struct { unsigned ch, size; float *data; } b[1]; } abl_t;
static OSStatus my_render(void *blk, unsigned *fl, const void *ts, unsigned frames, long bus, abl_t *abl, void *ev, void *pull) {
    (void)blk; (void)fl; (void)ts; (void)bus; (void)ev; (void)pull;
    for (unsigned i = 0; i < frames; i++) abl->b[0].data[i] = 0.25f;
    return 0;
}
static struct { void *isa; int fl, re; void *inv; } my_blk = {0, 0, 0, (void *)my_render};
static void *my_internal(void *s, const char *c) { (void)s; (void)c; return &my_blk; }
static float observed = -1; static void *observed_p;
static void obs_inv(void *b, void *p, float v) { (void)b; observed_p = p; observed = v; }
static float prov_inv(void *b, void *p) { (void)b; (void)p; return 0.75f; }
static struct { void *isa; int fl, re; void (*inv)(void *, void *, float); } obs_blk = {0, 0, 0, obs_inv};
static struct { void *isa; int fl, re; float (*inv)(void *, void *); } prov_blk = {0, 0, 0, prov_inv};
int main(void) {
    cx_auv3_init();
    cx_define_class(MyAU_c, MyAU_m, "MyAU", AUAudioUnit_c, 0);
    cx_add_method(MyAU_c, "internalRenderBlock", (void *)my_internal);
    acdesc d = {0x61756678, 0x6d796175, 0x6d6f7320, 0, 0};
    void *err = 0;
    void *au = ((void *(*)(void *, const char *, acdesc, unsigned, void **))imp(AUAudioUnit_c, "initWithComponentDescription:options:error:"))(cx_alloc_instance(MyAU_c), "", d, 0, &err);
    acdesc back = ((acdesc (*)(void *, const char *))imp(AUAudioUnit_c, "componentDescription"))(au, "");
    expect(au && back.sub == 0x6d796175, "init da subclasse e componentDescription");
    fo *fmt = fnew(K_FMT); fmt->ch = 2;
    void *bus = ((void *(*)(void *, const char *, void *, void **))imp(AUAudioUnitBus_c, "initWithFormat:error:"))(cx_alloc_instance(AUAudioUnitBus_c), "", fmt, &err);
    fo *busses = fnew(K_ARR); push(busses, bus);
    void *outs = ((void *(*)(void *, const char *, void *, long, void *))imp(AUAudioUnitBusArray_c, "initWithAudioUnit:busType:busses:"))(cx_alloc_instance(AUAudioUnitBusArray_c), "", au, 2, busses);
    expect(C(outs, "count", unsigned long) == 1 && C(C(outs, "objectAtIndexedSubscript:", void *, 0UL), "ownerAudioUnit", void *) == au, "array de bus com dono");
    void *render = C(au, "renderBlock", void *);
    float buf[1024]; abl_t abl = {1, 0, {{1, sizeof buf, buf}}};
    OSStatus (*inv)(void *, unsigned *, const void *, unsigned, long, abl_t *, void *) = ((void **)render)[2];
    unsigned fl = 0;
    expect(inv(render, &fl, 0, 256, 0, &abl, 0) == -10867, "sem recursos alocados: kAudioUnitErr_Uninitialized");
    expect(C(au, "allocateRenderResourcesAndReturnError:", signed char, &err) == 1, "aloca recursos");
    fo *f6 = fnew(K_FMT); f6->ch = 6;
    expect(!C(bus, "setFormat:error:", signed char, f6, &err), "não muda formato com recursos alocados");
    buf[10] = 0;
    expect(inv(render, &fl, 0, 256, 0, &abl, 0) == 0 && buf[10] == 0.25f, "renderBlock chama o internalRenderBlock da subclasse");
    expect(inv(render, &fl, 0, 4096, 0, &abl, 0) == -10874, "frames demais: kAudioUnitErr_TooManyFramesToProcess");
    void *p1 = ((void *(*)(void *, const char *, void *, void *, unsigned long long, float, float, unsigned, void *, unsigned, void *, void *))imp(AUParameterTree_c, "createParameterWithIdentifier:name:address:min:max:unit:unitName:flags:valueStrings:dependentParameters:"))
        (AUParameterTree_c, "", cx_str("gain"), cx_str("Ganho"), 7ULL, 0.0f, 1.0f, 0, 0, 0, 0, 0);
    fo *kids = fnew(K_ARR); push(kids, p1);
    void *grp = ((void *(*)(void *, const char *, void *, void *, void *))imp(AUParameterTree_c, "createGroupWithIdentifier:name:children:"))(AUParameterTree_c, "", cx_str("mix"), cx_str("Mix"), kids);
    fo *top = fnew(K_ARR); push(top, grp);
    void *tree = ((void *(*)(void *, const char *, void *))imp(AUParameterTree_c, "createTreeWithChildren:"))(AUParameterTree_c, "", top);
    C(tree, "setImplementorValueObserver:", void, &obs_blk);
    ((void (*)(void *, const char *, float))imp(*(void **)p1, "setValue:"))(p1, "", 2.0f);
    expect(observed == 1.0f && observed_p == p1, "setValue: limita à faixa e avisa o observador da árvore");
    expect(C(tree, "parameterWithAddress:", void *, 7ULL) == p1 && C(tree, "valueForKeyPath:", void *, cx_str("mix.gain")) == p1, "busca por endereço e por caminho");
    C(p1, "setImplementorValueProvider:", void, &prov_blk);
    expect(((float (*)(void *, const char *))imp(*(void **)p1, "value"))(p1, "") == 0.75f, "o provedor do implementador manda na leitura");
    fo *all = C(tree, "allParameters", void *);
    expect(all && all->n == 1 && all->v[0] == (fo *)p1, "allParameters percorre os grupos");
    printf("auv3: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
