/*
 * mOS libSystem — o resto do runtime Objective-C moderno: protocolos, objetos associados, introspecção de
 * classes (métodos, ivars, propriedades, lista de classes), codificação de tipos, imp_implementationWithBlock,
 * _objc_msgForward e o typeinfo de exceções ObjC (OBJC_EHTYPE_id).
 *
 * Por quê (tools/machocheck.c, docs/compat-apps.md): os sete apps importam ~35 funções da libobjc que o objc.c
 * não tinha (objc_setAssociatedObject/getAssociatedObject e imp_implementationWithBlock nos sete;
 * protocol_*, class_copy*List, class_addProtocol, property_* em seis), e o checklist do runtime pede protocolos,
 * propriedades e objetos associados.
 *
 * Protocolos: são os protocol_t do próprio binário (layout da Apple: isa, name, protocols, instanceMethods,
 * classMethods, optionalInstanceMethods, optionalClassMethods, instanceProperties, size, flags...). O carregador
 * já repassa __objc_protolist (kind 6) e __objc_protorefs (kind 10); cx_objc_protocols registra o primeiro de
 * cada nome e aponta as protorefs para ele (como o runtime da Apple faz entre imagens). A conformidade de uma
 * classe vem do baseProtocols do class_ro_t, dos protocolos de categorias e dos acrescentados com
 * class_addProtocol, e vale pela herança (classe e superclasses) e pela herança de protocolos.
 *
 * Objetos associados: tabela global (objeto, chave) -> valor com a política (assign/retain/copy); os valores
 * são soltos quando o objeto morre (cx_assoc_remove_all, chamado por cx_object_free em objc.c).
 */
#include "foundation.h"
#include "libc.h"

void *realloc(void *p, size_t n);
void *calloc(size_t a, size_t b);
char *strdup(const char *s);
int strncmp(const char *a, const char *b, size_t n);
id objc_retain(id o);
void objc_release(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
u32 cx_class_list(cx_class_t ***out);                  /* objc.c */
int cx_fdprintf(int fd, const char *fmt, ...);

static cx_rw_t *rw_of(cx_class_t *c) {
    cx_rw_t *rw = c ? c->vtable : 0;
    return rw && rw->magic == RW_MAGIC ? rw : 0;
}
static volatile int g_lock;
#if defined(__aarch64__)
static void lk(void) { while (__sync_lock_test_and_set(&g_lock, 1)) __asm__ volatile("yield"); }
#else
static void lk(void) { while (__sync_lock_test_and_set(&g_lock, 1)) __builtin_ia32_pause(); }
#endif
static void ul(void) { __sync_lock_release(&g_lock); }

/* ================================================================ protocolos */
typedef struct proto {
    void *isa;
    const char *name;
    struct { u64 count; struct proto *list[]; } *protocols;
    const u8 *inst, *cls, *opt_inst, *opt_cls, *props;
    u32 size, flags;
} proto_t;
typedef struct { SEL name; const char *types; } objc_method_description;

static proto_t **g_protos;
static u32 g_nprotos, g_capprotos;
proto_t *objc_getProtocol(const char *name) {
    if (!name) return 0;
    for (u32 i = 0; i < g_nprotos; i++) if (!strcmp(g_protos[i]->name, name)) return g_protos[i];
    return 0;
}
static proto_t *proto_register(proto_t *p) {
    if (!p || !p->name) return p;
    proto_t *old = objc_getProtocol(p->name);
    if (old) return old;
    if (g_nprotos == g_capprotos) {
        g_capprotos = g_capprotos ? g_capprotos * 2 : 128;
        g_protos = realloc(g_protos, g_capprotos * sizeof *g_protos);
    }
    g_protos[g_nprotos++] = p;
    return p;
}
/* chamado por cx_objc_init_image para as seções __objc_protolist (6) e __objc_protorefs (10) */
void cx_objc_protocols(u8 *a, u64 size, u64 kind) {
    for (u64 o = 0; o + 8 <= size; o += 8) {
        proto_t **slot = (proto_t **)(a + o);
        proto_t *canon = proto_register(*slot);
        if (kind == 10 || kind == 6) *slot = canon;
    }
}
const char *protocol_getName(proto_t *p) { return p ? p->name : "nil"; }
BOOL protocol_isEqual(proto_t *a, proto_t *b) { return a == b || (a && b && !strcmp(a->name, b->name)); }
BOOL protocol_conformsToProtocol(proto_t *p, proto_t *other) {
    if (!p || !other) return NO;
    if (protocol_isEqual(p, other)) return YES;
    if (p->protocols)
        for (u64 i = 0; i < p->protocols->count; i++)
            if (protocol_conformsToProtocol(p->protocols->list[i], other)) return YES;
    return NO;
}
/* method_list_t: {entsize_flags, count, {name, types, imp}[]} (sempre absoluta nos protocolos) */
static const char *ml_find(const u8 *ml, const char *sel) {
    if (!ml) return 0;
    u32 es = *(const u32 *)ml & 0xFFFC, n = *(const u32 *)(ml + 4);
    for (u32 i = 0; i < n; i++) {
        const u8 *e = ml + 8 + (u64)i * es;
        const char *nm = *(const char *const *)e;
        if (nm && !strcmp(nm, sel)) return *(const char *const *)(e + 8);
    }
    return 0;
}
objc_method_description protocol_getMethodDescription(proto_t *p, SEL sel, BOOL required, BOOL instance) {
    objc_method_description d = {0, 0};
    if (!p || !sel) return d;
    const u8 *ml = required ? (instance ? p->inst : p->cls) : (instance ? p->opt_inst : p->opt_cls);
    const char *t = ml_find(ml, sel);
    if (!t && p->protocols)
        for (u64 i = 0; i < p->protocols->count && !d.name; i++) d = protocol_getMethodDescription(p->protocols->list[i], sel, required, instance);
    else if (t) { d.name = sel; d.types = t; }
    return d;
}
objc_method_description *protocol_copyMethodDescriptionList(proto_t *p, BOOL required, BOOL instance, u32 *count) {
    if (count) *count = 0;
    const u8 *ml = p ? (required ? (instance ? p->inst : p->cls) : (instance ? p->opt_inst : p->opt_cls)) : 0;
    if (!ml) return 0;
    u32 es = *(const u32 *)ml & 0xFFFC, n = *(const u32 *)(ml + 4);
    objc_method_description *out = calloc(n + 1, sizeof *out);
    for (u32 i = 0; i < n; i++) {
        const u8 *e = ml + 8 + (u64)i * es;
        out[i].name = sel_registerName(*(const char *const *)e);
        out[i].types = *(const char *const *)(e + 8);
    }
    if (count) *count = n;
    return out;
}
/* property_list_t: {entsize, count, {name, attributes}[]} */
typedef struct { const char *name, *attrs; } prop_t;
static prop_t *pl_find(const u8 *pl, const char *name) {
    if (!pl) return 0;
    u32 es = *(const u32 *)pl, n = *(const u32 *)(pl + 4);
    for (u32 i = 0; i < n; i++) {
        prop_t *e = (prop_t *)(pl + 8 + (u64)i * es);
        if (e->name && !strcmp(e->name, name)) return e;
    }
    return 0;
}
prop_t *protocol_getProperty(proto_t *p, const char *name, BOOL required, BOOL instance) {
    if (!p || !name || !required || !instance) return 0;
    prop_t *r = pl_find(p->props, name);
    for (u64 i = 0; !r && p->protocols && i < p->protocols->count; i++) r = protocol_getProperty(p->protocols->list[i], name, required, instance);
    return r;
}
proto_t **protocol_copyProtocolList(proto_t *p, u32 *count) {
    if (count) *count = 0;
    if (!p || !p->protocols || !p->protocols->count) return 0;
    u32 n = (u32)p->protocols->count;
    proto_t **out = calloc(n + 1, sizeof *out);
    for (u32 i = 0; i < n; i++) out[i] = p->protocols->list[i];
    if (count) *count = n;
    return out;
}
proto_t **objc_copyProtocolList(u32 *count) {
    proto_t **out = calloc(g_nprotos + 1, sizeof *out);
    for (u32 i = 0; i < g_nprotos; i++) out[i] = g_protos[i];
    if (count) *count = g_nprotos;
    return out;
}
proto_t *objc_allocateProtocol(const char *name) {
    if (objc_getProtocol(name)) return 0;
    proto_t *p = calloc(1, sizeof *p);
    p->name = strdup(name);
    p->size = sizeof *p;
    return p;
}
void objc_registerProtocol(proto_t *p) { proto_register(p); }

/* protocolos acrescentados em tempo de execução (class_addProtocol) e por categorias */
static struct { cx_class_t *c; proto_t *p; } *g_extra;
static u32 g_nextra, g_capextra;
static void extra_add(cx_class_t *c, proto_t *p) {
    if (g_nextra == g_capextra) {
        g_capextra = g_capextra ? g_capextra * 2 : 64;
        g_extra = realloc(g_extra, g_capextra * sizeof *g_extra);
    }
    g_extra[g_nextra].c = c;
    g_extra[g_nextra++].p = p;
}
/* category_t: name, cls, instanceMethods, classMethods, protocols, instanceProperties (objc.c anexa os métodos) */
void cx_objc_category_protocols(const u8 *cat) {
    cx_class_t *c = *(cx_class_t *const *)(cat + 8);
    const u64 *pl = *(const u64 *const *)(cat + 32);
    if (!c || !pl) return;
    for (u64 i = 0; i < pl[0]; i++) extra_add(c, proto_register((proto_t *)pl[1 + i]));
}
/* class_ro_t: flags, instanceStart, instanceSize, reserved, ivarLayout, name, baseMethods, baseProtocols (+40)... */
static const u64 *ro_protocols(cx_class_t *c) {
    const u8 *ro = (const u8 *)(c->data & ~7ULL);    /* os bits baixos são flags (Swift) */
    return ro && rw_of(c) ? *(const u64 *const *)(ro + 40) : 0;
}
/* classes do próprio mOS (foundation.c/appkit.c: NSString, NSArray...) não têm class_ro_t nem lista de
 * protocolos, então -conformsToProtocol: continua dizendo YES para elas. Sobe a cadeia: a primeira classe
 * compilada (do app) decide "não é nativa"; subclasses criadas em tempo de execução (objc_allocateClassPair, KVO)
 * de uma classe compilada são puladas; NSObject/NSProxy sozinhos não contam como nativos. */
int cx_class_is_builtin(cx_class_t *c) {
    for (; c; c = c->super) {
        cx_rw_t *rw = rw_of(c);
        if (!rw) continue;
        if (c->data & ~7ULL) return 0;
        if (c->super && (c->super->data & ~7ULL)) continue;
        if (!c->super || !strcmp(rw->name, "NSObject") || !strcmp(rw->name, "NSProxy")) return 0;
        return 1;
    }
    return 0;
}
static BOOL class_direct_conforms(cx_class_t *c, proto_t *p) {
    const u64 *pl = ro_protocols(c);
    for (u64 i = 0; pl && i < pl[0]; i++) if (protocol_conformsToProtocol((proto_t *)pl[1 + i], p)) return YES;
    for (u32 i = 0; i < g_nextra; i++) if (g_extra[i].c == c && protocol_conformsToProtocol(g_extra[i].p, p)) return YES;
    return NO;
}
BOOL class_conformsToProtocol(cx_class_t *c, proto_t *p) {
    if (!p) return NO;
    for (; c; c = c->super) if (class_direct_conforms(c, p)) return YES;
    return NO;
}
BOOL class_addProtocol(cx_class_t *c, proto_t *p) {
    if (!c || !p || class_direct_conforms(c, p)) return NO;
    extra_add(c, p);
    return YES;
}
proto_t **class_copyProtocolList(cx_class_t *c, u32 *count) {
    if (count) *count = 0;
    if (!c) return 0;
    const u64 *pl = ro_protocols(c);
    u32 n = pl ? (u32)pl[0] : 0, k = 0;
    for (u32 i = 0; i < g_nextra; i++) if (g_extra[i].c == c) n++;
    if (!n) return 0;
    proto_t **out = calloc(n + 1, sizeof *out);
    for (u64 i = 0; pl && i < pl[0]; i++) out[k++] = (proto_t *)pl[1 + i];
    for (u32 i = 0; i < g_nextra; i++) if (g_extra[i].c == c) out[k++] = g_extra[i].p;
    if (count) *count = k;
    return out;
}

/* ================================================================ objetos associados */
enum { OBJC_ASSOCIATION_ASSIGN = 0 };
typedef struct { id obj; const void *key; id val; u64 policy; } assoc_t;
static assoc_t *g_assoc;
static u32 g_nassoc, g_capassoc;
int cx_assoc_any;                                       /* objc.c só procura se houver alguma */

static id assoc_hold(id v, u64 policy) {
    if (!v) return 0;
    if ((policy & 3) == 3) return MSG0(id, v, "copy");
    if ((policy & 3) == 1) return objc_retain(v);
    return v;
}
static void assoc_drop(id v, u64 policy) { if (v && (policy & 3)) objc_release(v); }

void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy) {
    if (!obj) return;
    { extern int cx_is_crr(id o);   /* objeto Swift: avisa, para o dealloc dele passar por objc_destructInstance */
      if (value && cx_is_crr(obj)) ((void (*)(id, SEL))objc_msgSend)(obj, sel_registerName("_noteAssociatedObjects")); }
    id held = assoc_hold(value, policy);
    id old = 0;
    u64 oldpol = 0;
    lk();
    u32 i = 0;
    for (; i < g_nassoc; i++) if (g_assoc[i].obj == obj && g_assoc[i].key == key) break;
    if (i < g_nassoc) {
        old = g_assoc[i].val; oldpol = g_assoc[i].policy;
        if (held) { g_assoc[i].val = held; g_assoc[i].policy = policy; }
        else g_assoc[i] = g_assoc[--g_nassoc];
    } else if (held) {
        if (g_nassoc == g_capassoc) {
            g_capassoc = g_capassoc ? g_capassoc * 2 : 256;
            g_assoc = realloc(g_assoc, g_capassoc * sizeof *g_assoc);
        }
        g_assoc[g_nassoc].obj = obj; g_assoc[g_nassoc].key = key;
        g_assoc[g_nassoc].val = held; g_assoc[g_nassoc++].policy = policy;
        cx_assoc_any = 1;
    }
    ul();
    assoc_drop(old, oldpol);
}
id objc_getAssociatedObject(id obj, const void *key) {
    id v = 0;
    lk();
    for (u32 i = 0; i < g_nassoc; i++) if (g_assoc[i].obj == obj && g_assoc[i].key == key) { v = g_assoc[i].val; break; }
    ul();
    return v;
}
void objc_removeAssociatedObjects(id obj) {
    for (;;) {                                         /* solta fora da trava: o dealloc do valor pode associar */
        id v = 0;
        u64 pol = 0;
        int found = 0;
        lk();
        for (u32 i = 0; i < g_nassoc; i++)
            if (g_assoc[i].obj == obj) { v = g_assoc[i].val; pol = g_assoc[i].policy; g_assoc[i] = g_assoc[--g_nassoc]; found = 1; break; }
        ul();
        if (!found) return;
        assoc_drop(v, pol);
    }
}
void cx_assoc_remove_all(id obj) { if (cx_assoc_any) objc_removeAssociatedObjects(obj); }

/* ================================================================ introspecção de classes */
cx_meth_t **class_copyMethodList(cx_class_t *c, u32 *count) {
    if (count) *count = 0;
    cx_rw_t *rw = rw_of(c);
    if (!rw && c) { cx_lookup(c, sel_registerName("class")); rw = rw_of(c); }   /* realiza */
    if (!rw || !rw->nm) return 0;
    cx_meth_t **out = calloc(rw->nm + 1, sizeof *out);
    for (u32 i = 0; i < rw->nm; i++) out[i] = &rw->m[i];
    if (count) *count = rw->nm;
    return out;
}
void **class_copyIvarList(cx_class_t *c, u32 *count) {
    if (count) *count = 0;
    cx_rw_t *rw = rw_of(c);
    if (!rw || !rw->nivars) return 0;
    void **out = calloc(rw->nivars + 1, sizeof *out);
    for (u32 i = 0; i < rw->nivars; i++) out[i] = &rw->ivars[i];
    if (count) *count = rw->nivars;
    return out;
}
const char *ivar_getTypeEncoding(void *iv) { return iv ? ((const char *const *)iv)[2] : 0; }
prop_t **class_copyPropertyList(cx_class_t *c, u32 *count) {
    if (count) *count = 0;
    cx_rw_t *rw = rw_of(c);
    if (!rw || !rw->nprops) return 0;
    prop_t **out = calloc(rw->nprops + 1, sizeof *out);
    for (u32 i = 0; i < rw->nprops; i++) out[i] = (prop_t *)&rw->props[i];
    if (count) *count = rw->nprops;
    return out;
}
prop_t *class_getProperty(cx_class_t *c, const char *name) {
    for (; c && name; c = c->super) {
        cx_rw_t *rw = rw_of(c);
        for (u32 i = 0; rw && i < rw->nprops; i++) if (!strcmp(rw->props[i].name, name)) return (prop_t *)&rw->props[i];
    }
    return 0;
}
BOOL class_addProperty(cx_class_t *c, const char *name, const void *attrs, u32 n) {
    cx_rw_t *rw = rw_of(c);
    if (!rw || !name || class_getProperty(c, name)) return NO;
    char *copy = calloc(1, (n ? n : 0) + 1);
    if (!copy) return NO;
    if (attrs && n) memcpy(copy, attrs, n);
    rw->props = realloc(rw->props, (rw->nprops + 1) * sizeof *rw->props);
    rw->props[rw->nprops].name = strdup(name);
    rw->props[rw->nprops++].attrs = copy;
    return YES;
}
const char *property_getName(prop_t *p) { return p ? p->name : 0; }
const char *property_getAttributes(prop_t *p) { return p ? p->attrs : 0; }
typedef struct { const char *name, *value; } prop_attr_t;
/* "T@\"NSString\",C,N,V_nome" -> {T, @"NSString"}, {C, ""}, {N, ""}, {V, _nome} (uma alocação só, como na Apple) */
prop_attr_t *property_copyAttributeList(prop_t *p, u32 *count) {
    if (count) *count = 0;
    if (!p || !p->attrs || !*p->attrs) return 0;
    const char *a = p->attrs;
    u32 n = 1;
    for (const char *q = a; *q; q++) if (*q == ',') n++;
    size_t len = strlen(a);
    prop_attr_t *out = calloc(1, (n + 1) * sizeof *out + len * 2 + n * 2 + 2);
    char *s = (char *)(out + n + 1);
    u32 k = 0;
    while (*a) {
        const char *e = a;
        while (*e && *e != ',') e++;
        out[k].name = s; *s++ = *a; *s++ = 0;
        out[k].value = s;
        memcpy(s, a + 1, (size_t)(e - a - 1)); s += e - a - 1; *s++ = 0;
        k++;
        a = *e ? e + 1 : e;
    }
    if (count) *count = k;
    return out;
}
char *property_copyAttributeValue(prop_t *p, const char *attr) {
    u32 n = 0;
    prop_attr_t *l = property_copyAttributeList(p, &n);
    char *r = 0;
    for (u32 i = 0; i < n && !r; i++) if (!strcmp(l[i].name, attr)) r = strdup(l[i].value);
    free(l);
    return r;
}

cx_class_t **objc_copyClassList(u32 *count) {
    cx_class_t **all;
    u32 n = cx_class_list(&all);
    cx_class_t **out = calloc(n + 1, sizeof *out);
    memcpy(out, all, n * sizeof *out);
    if (count) *count = n;
    return out;
}
int objc_getClassList(cx_class_t **buf, int cap) {
    cx_class_t **all;
    u32 n = cx_class_list(&all);
    for (int i = 0; buf && i < cap && (u32)i < n; i++) buf[i] = all[i];
    return (int)n;
}
void objc_disposeClassPair(cx_class_t *c) { (void)c; }      /* a classe fica (pode haver instâncias vivas) */
id objc_constructInstance(cx_class_t *c, void *bytes) {
    if (!c || !bytes) return 0;
    memset(bytes, 0, cx_instance_size(c));
    ((id)bytes)->isa = c;
    extern id cx_cxx_construct(id o);            /* ivars C++ (objc.c), como no runtime da Apple */
    return cx_cxx_construct((id)bytes);
}
void *object_getIndexedIvars(id o) { return o ? (u8 *)o + cx_instance_size(o->isa) : 0; }

/* ================================================================ codificação de tipos */
static const char *skip_type(const char *t) {
    while (*t && strchr("rnNoORV+", *t)) t++;         /* qualificadores */
    if (!*t) return t;
    char c = *t++;
    if (c == '@' && *t == '"') { t++; while (*t && *t != '"') t++; if (*t) t++; }
    else if (c == '@' && *t == '?') t++;               /* bloco */
    else if (c == '^') t = skip_type(t);
    else if (c == '[' || c == '{' || c == '(') {
        char close = c == '[' ? ']' : c == '{' ? '}' : ')';
        int depth = 1;
        while (*t && depth) { if (*t == c) depth++; else if (*t == close) depth--; t++; }
    } else if (c == 'b') while (*t >= '0' && *t <= '9') t++;
    while (*t >= '0' && *t <= '9') t++;                /* deslocamento na pilha */
    return t;
}
static const char *nth_type(const char *t, u32 n) {
    if (!t) return 0;
    for (u32 i = 0; i < n && *t; i++) t = skip_type(t);
    return *t ? t : 0;
}
static char *dup_type(const char *t) {
    if (!t) return 0;
    const char *e = skip_type(t);
    while (e > t && e[-1] >= '0' && e[-1] <= '9') e--;
    size_t n = (size_t)(e - t);
    char *r = malloc(n + 1);
    memcpy(r, t, n);
    r[n] = 0;
    return r;
}
u32 method_getNumberOfArguments(cx_meth_t *m) {
    if (!m || !m->types) return 0;
    const char *t = skip_type(m->types);            /* pula o retorno */
    u32 n = 0;
    while (*t) { t = skip_type(t); n++; }
    return n;
}
char *method_copyReturnType(cx_meth_t *m) { return dup_type(m && m->types ? m->types : "v"); }
char *method_copyArgumentType(cx_meth_t *m, u32 i) { return m ? dup_type(nth_type(m->types, i + 1)) : 0; }
static void copy_out(const char *t, char *dst, size_t len) {
    if (!dst || !len) return;
    memset(dst, 0, len);
    if (!t) return;
    char *s = dup_type(t);
    size_t n = strlen(s);
    memcpy(dst, s, n < len ? n : len - 1);
    free(s);
}
void method_getReturnType(cx_meth_t *m, char *dst, size_t len) { copy_out(m ? m->types : 0, dst, len); }
void method_getArgumentType(cx_meth_t *m, u32 i, char *dst, size_t len) { copy_out(m ? nth_type(m->types, i + 1) : 0, dst, len); }

/* ================================================================ imp_implementationWithBlock */
#define CX_TRAMP_N 4096
#if defined(__aarch64__)
#define CX_TRAMP_SZ 32                 /* objc_tramp_arm64.S: 5 instruções por entrada */
#else
#define CX_TRAMP_SZ 16
#endif
extern void *cx_imp_blocks[CX_TRAMP_N];
extern u8 cx_imp_tramps[], cx_imp_tramps_end[];
IMP imp_implementationWithBlock(id block) {
    if (!block) return 0;
    void *b = _Block_copy(block);
    lk();
    for (u32 i = 0; i < CX_TRAMP_N; i++)
        if (!cx_imp_blocks[i]) { cx_imp_blocks[i] = b; ul(); return (IMP)(cx_imp_tramps + CX_TRAMP_SZ * i); }
    ul();
    cx_fdprintf(2, "objc: imp_implementationWithBlock: os %d trampolins acabaram\n", CX_TRAMP_N);
    _Block_release(b);
    return 0;
}
static long tramp_index(IMP imp) {
    u8 *p = (u8 *)imp;
    if (p < cx_imp_tramps || p >= cx_imp_tramps_end || (p - cx_imp_tramps) % CX_TRAMP_SZ) return -1;
    return (p - cx_imp_tramps) / CX_TRAMP_SZ;
}
id imp_getBlock(IMP imp) { long i = tramp_index(imp); return i < 0 ? 0 : (id)cx_imp_blocks[i]; }
BOOL imp_removeBlock(IMP imp) {
    long i = tramp_index(imp);
    if (i < 0 || !cx_imp_blocks[i]) return NO;
    lk();
    void *b = cx_imp_blocks[i];
    cx_imp_blocks[i] = 0;
    ul();
    _Block_release(b);
    return YES;
}

/* ================================================================ encaminhamento e exceções */
/* _objc_msgForward está em objc_forward.S: o corpo em C descartava os argumentos. */
void _objc_msgForward_stret(void *ret, id self, SEL sel, ...) { (void)self; (void)sel; if (ret) memset(ret, 0, 32); }

/* typeinfo das exceções ObjC (objc_ehtype_vtable, OBJC_EHTYPE_id): objc_exc.c */
