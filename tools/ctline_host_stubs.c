/*
 * Harness Linux de CTLine e CFHost. Não entra no libSystem.
 * Implementa o mínimo para ligar coretext.c (-DCT_HOST) e cfhost.c com objc_msg.S:
 * o caminho sem int 0x80 (a macro CT_HOST) e os símbolos que esses dois arquivos chamam.
 * NSAttributedString de propósito não responde a attributesAtIndex:effectiveRange:, para o
 * teste cair no layout {string, attrs} em +8, o mesmo do appkit.c.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <netinet/in.h>
#include "objc.h"
#include "foundation.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
void cx_object_free(id o);

#define OBJ_MAGIC 0x4F424A43u
typedef struct { u32 magic; i32 rc; u64 pad; } objhdr_t;
typedef struct {
    const char *name;
    u32 size;
    struct { SEL sel; IMP imp; } *m;
    int nm, cap;
} stub_rw;

id NSFontAttributeName, NSForegroundColorAttributeName;
int ct_host_performs;
const void *kCFStreamErrorDomainNetDB = "kCFStreamErrorDomainNetDB";

static cx_class_t NSObject_c, NSObject_m, NSString_c, NSString_m, NSArray_c, NSArray_m;
static cx_class_t NSData_c, NSData_m, NSDict_c, NSDict_m, NSFont_c, NSFont_m, NSColor_c, NSColor_m, NSAstr_c, NSAstr_m;
static cx_class_t *reg[32];
static int nreg;

static objhdr_t *hdr(id o) {
    if (!o || ((u64)o & 7)) return 0;
    objhdr_t *h = (objhdr_t *)((u8 *)o - 16);
    return h->magic == OBJ_MAGIC ? h : 0;
}
static id imp_missing(id self, SEL sel, ...) {
    if (sel && !strcmp(sel, "dealloc")) cx_object_free(self);
    return 0;
}

SEL sel_registerName(const char *name) {
    static char **tab;
    static int n, cap;
    if (!name) name = "";
    for (int i = 0; i < n; i++) if (!strcmp(tab[i], name)) return tab[i];
    if (n == cap) { cap = cap ? cap * 2 : 32; tab = realloc(tab, (size_t)cap * sizeof *tab); }
    size_t L = strlen(name);
    char *s = malloc(L + 1);
    memcpy(s, name, L + 1);
    tab[n++] = s;
    return s;
}
static void add_imp(cx_class_t *cls, const char *sel, IMP imp) {
    stub_rw *rw = cls->vtable;
    if (rw->nm == rw->cap) {
        rw->cap = rw->cap ? rw->cap * 2 : 8;
        rw->m = realloc(rw->m, (size_t)rw->cap * sizeof *rw->m);
    }
    rw->m[rw->nm].sel = sel_registerName(sel);
    rw->m[rw->nm].imp = imp;
    rw->nm++;
}
static void *lookup(cx_class_t *c, SEL sel) {
    for (int d = 0; c && d < 32; d++, c = c->super) {
        stub_rw *rw = c->vtable;
        if (!rw) continue;
        for (int i = 0; i < rw->nm; i++) if (rw->m[i].sel == sel) return (void *)rw->m[i].imp;
    }
    return (void *)imp_missing;
}
void *cx_msg_lookup(cx_class_t *cls, SEL sel) { return lookup(cls, sel); }
void *cx_msg_lookup_stret(cx_class_t *cls, SEL sel) { return lookup(cls, sel); }

cx_class_t *cx_define_class(cx_class_t *cls, cx_class_t *meta, const char *name, cx_class_t *super, u32 extra) {
    stub_rw *rw = calloc(1, sizeof *rw), *mrw = calloc(1, sizeof *mrw);
    u32 base = super && super->vtable ? ((stub_rw *)super->vtable)->size : 8;
    rw->name = mrw->name = name;
    rw->size = (base + extra + 7u) & ~7u;
    mrw->size = 8;
    cls->isa = meta;
    cls->super = super;
    cls->vtable = rw;
    meta->vtable = mrw;
    if (super) {
        cx_class_t *root = super;
        while (root->super) root = root->super;
        meta->isa = root->isa;
        meta->super = super->isa;
    } else {
        meta->isa = meta;
        meta->super = cls;
    }
    if (nreg < 32) reg[nreg++] = cls;
    return cls;
}
void cx_add_method(cx_class_t *cls, const char *sel, void *imp) { add_imp(cls, sel, (IMP)imp); }
void cx_add_cmethod(cx_class_t *cls, const char *sel, void *imp) { add_imp(cls->isa, sel, (IMP)imp); }
cx_class_t *cx_class_named(const char *name) {
    for (int i = 0; i < nreg; i++) {
        stub_rw *rw = reg[i]->vtable;
        if (rw && rw->name && !strcmp(rw->name, name)) return reg[i];
    }
    return 0;
}
const char *cx_class_name(cx_class_t *cls) { stub_rw *rw = cls ? cls->vtable : 0; return rw ? rw->name : 0; }
u32 cx_instance_size(cx_class_t *cls) { stub_rw *rw = cls ? cls->vtable : 0; return rw ? rw->size : 8; }
BOOL cx_responds(cx_class_t *cls, SEL sel) { return lookup(cls, sel) != (void *)imp_missing; }
BOOL cx_is_subclass(cx_class_t *cls, cx_class_t *of) {
    for (int d = 0; cls && d < 32; d++, cls = cls->super) if (cls == of) return YES;
    return NO;
}
cx_class_t *object_getClass(id o) { return o ? o->isa : 0; }
id cx_alloc_instance(cx_class_t *cls) {
    u32 sz = cx_instance_size(cls);
    if (sz < 16) sz = 16;
    u8 *b = calloc(1, 16 + sz);
    objhdr_t *h = (objhdr_t *)b;
    h->magic = OBJ_MAGIC;
    h->rc = 1;
    id o = (id)(b + 16);
    o->isa = cls;
    return o;
}
void cx_object_free(id o) {
    objhdr_t *h = hdr(o);
    if (!h) return;
    h->magic = 0;
    free(h);
}
id objc_retain(id o) { objhdr_t *h = hdr(o); if (h) h->rc++; return o; }
id objc_autorelease(id o) { return o; }
void objc_release(id o) {
    objhdr_t *h = hdr(o);
    if (!h) return;
    if (--h->rc == 0) {
        h->rc = 0x3FFFFFFF;
        ((void (*)(id, SEL))objc_msgSend)(o, sel_registerName("dealloc"));
    }
}

id cx_str(const char *s) {
    if (!s) return 0;
    nsstr_t *o = (nsstr_t *)cx_alloc_instance(&NSString_c);
    size_t n = strlen(s);
    o->s = malloc(n + 1);
    memcpy(o->s, s, n + 1);
    o->n = n;
    return (id)o;
}
id cx_str_n(const char *s, u64 n) { (void)n; return cx_str(s); }
BOOL cx_is_str(id o) { return o && cx_is_subclass(o->isa, &NSString_c); }
const char *cx_cstr(id o) {
    if (!cx_is_str(o)) return "";
    char *s = ((nsstr_t *)o)->s;
    return s ? s : "";
}
static u64 str_len(id self, SEL c) { (void)c; return ((nsstr_t *)self)->n; }
static u16 str_at(id self, SEL c, u64 idx) {
    (void)c;
    nsstr_t *s = (nsstr_t *)self;
    if (!s->s || idx >= s->n) return 0;
    return (u16)(u8)s->s[idx];
}
static BOOL str_eq(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    if (!cx_is_str(o)) return NO;
    return ((nsstr_t *)self)->n == ((nsstr_t *)o)->n && !strcmp(cx_cstr(self), cx_cstr(o));
}

typedef struct { id *items; u64 n, cap; } arrb;
static id alloc_m(id cls, SEL c) { (void)c; return cx_alloc_instance((cx_class_t *)cls); }
static id init_m(id self, SEL c) { (void)c; return self; }
static id arr_init(id self, SEL c) {
    (void)c;
    *(void **)((u8 *)self + 8) = calloc(1, sizeof(arrb));
    return self;
}
static id arr_array(id cls, SEL c) { return arr_init(alloc_m(cls, c), c); }
static void arr_add(id self, SEL c, id o) {
    (void)c;
    arrb *a = *(arrb **)((u8 *)self + 8);
    if (!a || !o) return;
    if (a->n == a->cap) { a->cap = a->cap ? a->cap * 2 : 4; a->items = realloc(a->items, a->cap * sizeof(id)); }
    a->items[a->n++] = objc_retain(o);
}
static u64 arr_count(id self, SEL c) { (void)c; arrb *a = *(arrb **)((u8 *)self + 8); return a ? a->n : 0; }
static id arr_at(id self, SEL c, u64 i) {
    (void)c;
    arrb *a = *(arrb **)((u8 *)self + 8);
    if (!a || i >= a->n) return 0;
    return a->items[i];
}

typedef struct { id *k, *v; u64 n, cap; } dictb;
static id dict_init(id self, SEL c) {
    (void)c;
    *(void **)((u8 *)self + 8) = calloc(1, sizeof(dictb));
    return self;
}
static void dict_put(id self, id key, id val) {
    dictb *d = *(dictb **)((u8 *)self + 8);
    if (!d || !key) return;
    if (d->n == d->cap) { d->cap = d->cap ? d->cap * 2 : 4; d->k = realloc(d->k, d->cap * sizeof(id)); d->v = realloc(d->v, d->cap * sizeof(id)); }
    d->k[d->n] = key;
    d->v[d->n] = val;
    d->n++;
}
static id dict_new(id cls, SEL c) { return dict_init(alloc_m(cls, c), c); }
static void dict_set(id self, SEL c, id val, id key) { (void)c; dict_put(self, key, val); }
static id dict_get(id self, SEL c, id key) {
    (void)c;
    dictb *d = *(dictb **)((u8 *)self + 8);
    if (!d || !key) return 0;
    for (u64 i = 0; i < d->n; i++) {
        if (d->k[i] == key) return d->v[i];
        if (d->k[i] && MSG(BOOL, d->k[i], "isEqual:", key)) return d->v[i];
    }
    return 0;
}

static double font_pt(id self, SEL c) { (void)c; return *(double *)((u8 *)self + 8); }
static id font_nm(id self, SEL c) {
    (void)self; (void)c;
    static id n;
    if (!n) n = cx_str("Helvetica");
    return n;
}
static id font_make(cx_class_t *cls, double size) {
    id f = cx_alloc_instance(cls);
    *(double *)((u8 *)f + 8) = size > 0 ? size : 13;
    return f;
}
static id font_sys(id cls, SEL c, double size) { (void)c; return font_make((cx_class_t *)cls, size); }

typedef struct { id str, attrs; } astrb;
static id astr_string(id self, SEL c) {
    (void)c;
    astrb *a = *(astrb **)((u8 *)self + 8);
    return a ? a->str : 0;
}

id ct_test_font(double size) { return font_make(&NSFont_c, size); }
id ct_test_color(double r, double g, double b, double a) {
    id c = cx_alloc_instance(&NSColor_c);
    double *p = (double *)((u8 *)c + 8);
    p[0] = r; p[1] = g; p[2] = b; p[3] = a;
    return c;
}
id ct_test_astr(const char *s, id font, id color) {
    id dict = dict_init(cx_alloc_instance(&NSDict_c), 0);
    if (font) dict_put(dict, NSFontAttributeName, font);
    if (color) dict_put(dict, NSForegroundColorAttributeName, color);
    id astr = cx_alloc_instance(&NSAstr_c);
    astrb *b = calloc(1, sizeof *b);
    b->str = cx_str(s ? s : "");
    b->attrs = dict;
    *(void **)((u8 *)astr + 8) = b;
    return astr;
}

typedef struct { cx_class_t *isa; u8 *bytes; u64 len; } data_t;
id CFDataCreate(void *alloc, const u8 *bytes, long n) {
    (void)alloc;
    if (n < 0) n = 0;
    data_t *d = (data_t *)cx_alloc_instance(&NSData_c);
    d->bytes = malloc((size_t)n + 1);
    if (n && bytes) memcpy(d->bytes, bytes, (size_t)n);
    d->bytes[n] = 0;
    d->len = (u64)n;
    return (id)d;
}
const u8 *CFDataGetBytePtr(id d) { return d ? ((data_t *)d)->bytes : 0; }
long CFDataGetLength(id d) { return d ? (long)((data_t *)d)->len : 0; }

static int g_loop;
void *CFRunLoopGetCurrent(void) { return &g_loop; }
void CFRunLoopPerformBlock(void *rl, void *mode, void *block) {
    (void)rl; (void)mode;
    ct_host_performs++;
    typedef struct { void *isa; int flags, reserved; void (*invoke)(void *); } blk;
    if (block && ((blk *)block)->invoke) ((blk *)block)->invoke(block);
}
int gethostname(char *b, size_t n) {
    if (!b || n < 4) return -1;
    memcpy(b, "mos", 4);
    return 0;
}

typedef struct cf_ai {
    int flags, family, socktype, protocol;
    u32 addrlen;
    char *canonname;
    void *addr;
    struct cf_ai *next;
} cf_ai;
static int parse_v4(const char *s, unsigned *o) {
    if (!s) return 0;
    unsigned v[4];
    int part = 0;
    while (part < 4) {
        if (*s < '0' || *s > '9') return 0;
        unsigned n = 0;
        int digits = 0;
        while (*s >= '0' && *s <= '9') {
            n = n * 10u + (unsigned)(*s - '0');
            if (n > 255 || ++digits > 3) return 0;
            s++;
        }
        v[part++] = n;
        if (part < 4) { if (*s != '.') return 0; s++; }
    }
    if (*s) return 0;
    o[0] = v[0]; o[1] = v[1]; o[2] = v[2]; o[3] = v[3];
    return 1;
}
static cf_ai *ai_new(const u8 *sa, u32 len) {
    cf_ai *ai = calloc(1, sizeof *ai);
    u8 *p = malloc(len);
    memcpy(p, sa, len);
    ai->family = sa[1];
    ai->socktype = 1;
    ai->protocol = 6;
    ai->addrlen = len;
    ai->addr = p;
    return ai;
}
static void ai_push(cf_ai **res, cf_ai *ai) {
    if (!*res) *res = ai;
    else { cf_ai *t = *res; while (t->next) t = t->next; t->next = ai; }
}
void freeaddrinfo(cf_ai *ai) {
    while (ai) { cf_ai *n = ai->next; free(ai->addr); free(ai->canonname); free(ai); ai = n; }
}
int getaddrinfo(const char *host, const char *svc, const cf_ai *hints, cf_ai **res) {
    (void)svc; (void)hints;
    if (!res) return 4;
    *res = 0;
    if (!host || !host[0] || !strcmp(host, "localhost")) {
        u8 sin[16] = {16, 2, 0, 0, 127, 0, 0, 1};
        u8 s6[28];
        memset(s6, 0, sizeof s6);
        s6[0] = 28; s6[1] = 30; s6[23] = 1;
        ai_push(res, ai_new(sin, 16));
        ai_push(res, ai_new(s6, 28));
        return 0;
    }
    unsigned v[4];
    if (parse_v4(host, v)) {
        u8 sin[16] = {16, 2, 0, 0, (u8)v[0], (u8)v[1], (u8)v[2], (u8)v[3]};
        ai_push(res, ai_new(sin, 16));
        return 0;
    }
    if (!strcmp(host, "no.such.host.invalid")) return 8;
    typedef struct lai { int flags, family, socktype, protocol; unsigned addrlen; void *addr; char *canon; struct lai *next; } lai;
    typedef int (*gai_fn)(const char *, const char *, const void *, lai **);
    typedef void (*fai_fn)(lai *);
    gai_fn real = (gai_fn)dlsym(RTLD_NEXT, "getaddrinfo");
    fai_fn rfree = (fai_fn)dlsym(RTLD_NEXT, "freeaddrinfo");
    if (!real || !rfree) return 8;
    lai *lr = 0;
    int e = real(host, "0", 0, &lr);
    if (e || !lr) { if (lr) rfree(lr); return 8; }
    for (lai *it = lr; it; it = it->next) {
        if (!it->addr) continue;
        unsigned short fam = *(unsigned short *)it->addr;
        if (fam == AF_INET && it->addrlen >= sizeof(struct sockaddr_in)) {
            struct sockaddr_in *in = it->addr;
            u8 sin[16] = {16, 2, 0, 0, 0, 0, 0, 0};
            memcpy(sin + 2, &in->sin_port, 2);
            memcpy(sin + 4, &in->sin_addr, 4);
            ai_push(res, ai_new(sin, 16));
        } else if (fam == AF_INET6 && it->addrlen >= sizeof(struct sockaddr_in6)) {
            struct sockaddr_in6 *in = it->addr;
            u8 s6[28];
            memset(s6, 0, sizeof s6);
            s6[0] = 28; s6[1] = 30;
            memcpy(s6 + 2, &in->sin6_port, 2);
            memcpy(s6 + 8, &in->sin6_addr, 16);
            ai_push(res, ai_new(s6, 28));
        }
    }
    rfree(lr);
    return *res ? 0 : 8;
}

enum { BM_MAGIC = 0xC7B17A01u };
typedef struct { u32 magic, fill; int w, h; double a, b, c, d, tx, ty; u8 *data; } bm_t;
void *ct_test_bitmap(int w, int h) {
    bm_t *b = calloc(1, sizeof *b);
    b->magic = BM_MAGIC;
    b->fill = 0xFF000000;
    b->w = w; b->h = h;
    b->a = b->d = 1;
    b->data = calloc((size_t)w * (size_t)h, 1);
    return b;
}
int ct_test_ink(void *p) {
    bm_t *b = p;
    if (!b || b->magic != BM_MAGIC) return 0;
    int n = 0;
    for (int i = 0; i < b->w * b->h; i++) if (b->data[i]) n++;
    return n;
}
u32 ct_test_fill(void *p) { bm_t *b = p; return b && b->magic == BM_MAGIC ? b->fill : 0; }
typedef struct { double a, b, c, d, tx, ty; } ct_tm;
ct_tm CGContextGetTextMatrix(void *c) {
    ct_tm t = {1, 0, 0, 1, 0, 0};
    bm_t *b = c;
    if (!b || b->magic != BM_MAGIC) return t;
    t.a = b->a; t.b = b->b; t.c = b->c; t.d = b->d; t.tx = b->tx; t.ty = b->ty;
    return t;
}
void CGContextSetTextMatrix(void *c, ct_tm t) {
    bm_t *b = c;
    if (!b || b->magic != BM_MAGIC) return;
    b->a = t.a; b->b = t.b; b->c = t.c; b->d = t.d; b->tx = t.tx; b->ty = t.ty;
}
int cx_bitmap_draw_glyph(void *ctx, double x, double y, const u8 *alpha, int gw, int gh, int left, int top) {
    bm_t *b = ctx;
    if (!b || b->magic != BM_MAGIC || !alpha || gw <= 0 || gh <= 0) return 0;
    int ox = (int)x + left, oy = (int)y + top;
    for (int gy = 0; gy < gh; gy++) for (int gx = 0; gx < gw; gx++) {
        if (!alpha[gy * gw + gx]) continue;
        int px = ox + gx, py = oy + gy;
        if (px < 0 || py < 0 || px >= b->w || py >= b->h) continue;
        b->data[py * b->w + px] = 255;
    }
    return 1;
}
void cx_bm_set_fill(const void *c, u32 argb) { bm_t *b = (bm_t *)c; if (b && b->magic == BM_MAGIC) b->fill = argb; }

void host_runtime_init(void) {
    cx_define_class(&NSObject_c, &NSObject_m, "NSObject", 0, 0);
    cx_add_cmethod(&NSObject_c, "alloc", (void *)alloc_m);
    cx_add_method(&NSObject_c, "init", (void *)init_m);
    cx_define_class(&NSString_c, &NSString_m, "NSString", &NSObject_c, 24);
    cx_add_method(&NSString_c, "length", (void *)str_len);
    cx_add_method(&NSString_c, "characterAtIndex:", (void *)str_at);
    cx_add_method(&NSString_c, "isEqual:", (void *)str_eq);
    cx_define_class(&NSArray_c, &NSArray_m, "NSMutableArray", &NSObject_c, 8);
    cx_add_cmethod(&NSArray_c, "array", (void *)arr_array);
    cx_add_method(&NSArray_c, "init", (void *)arr_init);
    cx_add_method(&NSArray_c, "addObject:", (void *)arr_add);
    cx_add_method(&NSArray_c, "count", (void *)arr_count);
    cx_add_method(&NSArray_c, "objectAtIndex:", (void *)arr_at);
    cx_define_class(&NSData_c, &NSData_m, "NSData", &NSObject_c, 16);
    cx_define_class(&NSDict_c, &NSDict_m, "NSMutableDictionary", &NSObject_c, 8);
    cx_add_cmethod(&NSDict_c, "dictionary", (void *)dict_new);
    cx_add_method(&NSDict_c, "init", (void *)dict_init);
    cx_add_method(&NSDict_c, "objectForKey:", (void *)dict_get);
    cx_add_method(&NSDict_c, "setObject:forKey:", (void *)dict_set);
    cx_define_class(&NSFont_c, &NSFont_m, "NSFont", &NSObject_c, 16);
    cx_add_cmethod(&NSFont_c, "systemFontOfSize:", (void *)font_sys);
    cx_add_method(&NSFont_c, "pointSize", (void *)font_pt);
    cx_add_method(&NSFont_c, "fontName", (void *)font_nm);
    cx_define_class(&NSColor_c, &NSColor_m, "NSColor", &NSObject_c, 32);
    cx_define_class(&NSAstr_c, &NSAstr_m, "NSAttributedString", &NSObject_c, 8);
    cx_add_method(&NSAstr_c, "string", (void *)astr_string);
    NSFontAttributeName = cx_str("NSFont");
    NSForegroundColorAttributeName = cx_str("NSColor");
}
