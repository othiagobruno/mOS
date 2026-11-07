/*
 * mOS libSystem — runtime de Objective-C compatível com o ABI 2 da Apple (x86_64).
 *
 * Lê as seções __objc_* que o loader (kernel/compat/macho.c) passa no bloco de partida:
 * únifica seletores, realiza classes (listas de métodos normais e relativas, ivars não-frágeis),
 * anexa categorias e chama +load. objc_msgSend (objc_msg.S) consulta cx_lookup com cache por
 * classe. Seletores ausentes não derrubam o app: são registrados no log e devolvem nil, o que
 * torna a compatibilidade parcial com AppKit/Foundation muito mais tolerante.
 */
#include "objc.h"
#include "libc.h"

/* ================================================================ seletores */
typedef struct selent { const char *name; u32 hash; struct selent *next; } selent_t;
static selent_t *sel_tab[4096];

static u32 strhash(const char *s) { u32 h = 2166136261u; while (*s) { h ^= (u8)*s++; h *= 16777619u; } return h; }

SEL sel_registerName(const char *name) {
    if (!name) return 0;
    u32 h = strhash(name);
    for (selent_t *e = sel_tab[h & 4095]; e; e = e->next)
        if (e->hash == h && !strcmp(e->name, name)) return e->name;
    selent_t *e = malloc(sizeof *e);
    e->name = strdup(name);
    e->hash = h;
    e->next = sel_tab[h & 4095];
    sel_tab[h & 4095] = e;
    return e->name;
}
SEL sel_getUid(const char *name) { return sel_registerName(name); }
const char *sel_getName(SEL s) { return s ? s : "<null selector>"; }
BOOL sel_isEqual(SEL a, SEL b) { return a == b; }

/* ================================================================ classes */
static cx_class_t **all_classes;
static u32 nclasses, capclasses;
void *_objc_empty_cache[2];
void *_objc_empty_vtable[2];

static cx_rw_t *rw_of(cx_class_t *c) {
    cx_rw_t *rw = c ? c->vtable : 0;
    return rw && rw->magic == RW_MAGIC ? rw : 0;
}

static void register_class(cx_class_t *c) {
    if (nclasses == capclasses) {
        capclasses = capclasses ? capclasses * 2 : 256;
        all_classes = realloc(all_classes, capclasses * sizeof *all_classes);
    }
    all_classes[nclasses++] = c;
}
/* para objc_getClassList/objc_copyClassList (objcrt.c) */
u32 cx_class_list(cx_class_t ***out) { if (out) *out = all_classes; return nclasses; }

static int objc_ready;
void cx_objc_boot(u64 sects, u64 n);
cx_class_t *cx_class_named(const char *name) {
    for (u32 i = 0; i < nclasses; i++) {
        cx_rw_t *rw = rw_of(all_classes[i]);
        if (rw && !strcmp(rw->name, name)) return all_classes[i];
    }
    /* programa sem seções ObjC (C/Rust que só usa CoreFoundation/Security, ex.: o Codex CLI): o runtime e as
     * classes do Foundation ainda não subiram — sobe agora, como o swiftbridge.c faz, e procura de novo */
    if (!objc_ready) { cx_objc_boot(0, 0); return cx_class_named(name); }
    return 0;
}

const char *cx_class_name(cx_class_t *c) { cx_rw_t *rw = rw_of(c); return rw ? rw->name : "?"; }
u32 cx_instance_size(cx_class_t *c) { cx_rw_t *rw = rw_of(c); return rw ? rw->size : 8; }

static void add_meth(cx_rw_t *rw, SEL sel, IMP imp, const char *types) {
    if (rw->nm == rw->capm) {
        rw->capm = rw->capm ? rw->capm * 2 : 16;
        rw->m = realloc(rw->m, rw->capm * sizeof *rw->m);
    }
    rw->m[rw->nm].sel = sel;
    rw->m[rw->nm].imp = imp;
    rw->m[rw->nm].types = types;
    rw->nm++;
}

static void flush_caches(void) {
    for (u32 i = 0; i < nclasses; i++) {
        cx_class_t *cls[2] = {all_classes[i], all_classes[i]->isa};
        for (int k = 0; k < 2; k++) {
            cx_rw_t *rw = rw_of(cls[k]);
            if (rw && rw->cache) { memset(rw->cache, 0, (rw->cache_mask + 1) * sizeof *rw->cache); rw->cache_used = 0; }
        }
    }
}

static void load_methods(cx_rw_t *rw, const u8 *ml) {
    if (!ml) return;
    u32 ef = *(const u32 *)ml, cnt = *(const u32 *)(ml + 4);
    int rel = (ef & 0x80000000u) != 0;
    u32 es = ef & 0xFFFC;
    if (!es) es = rel ? 12 : 24;
    for (u32 i = 0; i < cnt; i++) {
        const u8 *e = ml + 8 + (u64)i * es;
        SEL sel;
        const char *types;
        IMP imp;
        if (rel) {
            const char **selref = (const char **)(e + *(const i32 *)e);
            sel = sel_registerName(*selref);
            types = (const char *)(e + 4 + *(const i32 *)(e + 4));
            imp = (IMP)(e + 8 + *(const i32 *)(e + 8));
        } else {
            sel = sel_registerName(*(const char *const *)e);
            types = *(const char *const *)(e + 8);
            imp = *(IMP const *)(e + 16);
        }
        add_meth(rw, sel, imp, types);
    }
}

static void realize(cx_class_t *c);

/* ---- classes com retain/release próprios (RR customizado)
 * Objetos Swift (classe raiz Swift._SwiftObject do runtime Swift com interop ObjC) não passam pelo new_obj:
 * vêm do swift_allocObject, sem o objhdr_t, com a contagem dentro do objeto. Como no runtime da Apple, a
 * libobjc manda -retain/-release/-autorelease a eles. Só entram aqui classes raiz (sem superclasse) com
 * -retain no próprio método e as subclasses delas: o -retain do NSObject chama objc_retain, e encaminhar a
 * ele entraria em laço. O teste é por ponteiro da classe (sem ler a classe), porque objc_retain também
 * recebe objetos Swift sem ObjC do resto do mOS, cujo "isa" é metadata sem o formato de objc_class. */
static cx_class_t **crr_tab;
static u32 crr_mask, crr_n;
static int crr_has(cx_class_t *c) {
    if (!crr_tab || !c) return 0;
    for (u32 i = (u32)((u64)c >> 4) & crr_mask; crr_tab[i]; i = (i + 1) & crr_mask) if (crr_tab[i] == c) return 1;
    return 0;
}
static void crr_add(cx_class_t *c) {
    if (crr_has(c)) return;
    if (!crr_tab || (crr_n + 1) * 2 > crr_mask + 1) {
        cx_class_t **old = crr_tab;
        u32 om = crr_mask;
        crr_mask = old ? om * 2 + 1 : 63;
        crr_tab = calloc(crr_mask + 1, sizeof *crr_tab);
        crr_n = 0;
        if (old) { for (u32 i = 0; i <= om; i++) if (old[i]) crr_add(old[i]); free(old); }
    }
    u32 i = (u32)((u64)c >> 4) & crr_mask;
    while (crr_tab[i]) i = (i + 1) & crr_mask;
    crr_tab[i] = c;
    crr_n++;
}
/* objeto de uma classe com RR customizado (sem objhdr_t) */
static int is_crr(id o) { return o && !((u64)o & 7) && crr_has(o->isa); }
static id crr_send(id o, const char *sel);
static void note_crr(cx_class_t *c, cx_rw_t *rw) {
    if (rw->meta) return;
    if (c->super) { if (crr_has(c->super)) crr_add(c); return; }
    SEL r = sel_registerName("retain");
    for (u32 i = 0; i < rw->nm; i++) if (rw->m[i].sel == r) { crr_add(c); return; }
}

static void realize_ro(cx_class_t *c) {
    const u8 *ro = (const u8 *)(c->data & ~7ULL);
    if (!ro) return;
    u32 flags = *(const u32 *)ro, start = *(const u32 *)(ro + 4), size = *(const u32 *)(ro + 8);
    cx_rw_t *rw = calloc(1, sizeof *rw);
    rw->magic = RW_MAGIC;
    rw->cls = c;
    rw->meta = flags & 1;
    rw->name = *(const char *const *)(ro + 24);
    c->vtable = rw;                     /* antes de realizar a superclasse: evita recursão em ciclos */
    if (c->super) realize(c->super);
    if (c->isa && c->isa != c) realize(c->isa);
    if (!rw->meta) {
        u32 super_size = c->super ? cx_instance_size(c->super) : 0;
        const u8 *iv = *(const u8 *const *)(ro + 48);
        if (super_size > start) {
            u32 delta = (super_size - start + 7) & ~7u;
            if (iv) {
                u32 ies = *(const u32 *)iv & 0xFFFF, icnt = *(const u32 *)(iv + 4);
                for (u32 i = 0; i < icnt; i++) {
                    u32 *off = *(u32 *const *)(iv + 8 + (u64)i * ies);
                    if (off) *off += delta;
                }
            }
            size += delta;
        }
        rw->size = size < 8 ? 8 : size;
        if (iv) {
            u32 ies = *(const u32 *)iv & 0xFFFF, icnt = *(const u32 *)(iv + 4);
            rw->ivars = calloc(icnt ? icnt : 1, sizeof *rw->ivars);
            for (u32 i = 0; i < icnt; i++) {
                const u8 *e = iv + 8 + (u64)i * ies;
                rw->ivars[i].offset = *(u32 *const *)e;
                rw->ivars[i].name = *(const char *const *)(e + 8);
                rw->ivars[i].type = *(const char *const *)(e + 16);
            }
            rw->nivars = icnt;
        }
        const u8 *pl = *(const u8 *const *)(ro + 64);
        if (pl) {
            u32 pes = *(const u32 *)pl, pcnt = *(const u32 *)(pl + 4);
            rw->props = calloc(pcnt ? pcnt : 1, sizeof *rw->props);
            for (u32 i = 0; i < pcnt; i++) {
                rw->props[i].name = *(const char *const *)(pl + 8 + (u64)i * pes);
                rw->props[i].attrs = *(const char *const *)(pl + 16 + (u64)i * pes);
            }
            rw->nprops = pcnt;
        }
        register_class(c);
    }
    load_methods(rw, *(const u8 *const *)(ro + 32));
    note_crr(c, rw);
}

static void realize(cx_class_t *c) {
    if (!c || rw_of(c)) return;
    realize_ro(c);
}

cx_class_t *cx_define_class(cx_class_t *cls, cx_class_t *meta, const char *name, cx_class_t *super, u32 extra) {
    cx_rw_t *rw = calloc(1, sizeof *rw), *mrw = calloc(1, sizeof *mrw);
    rw->magic = mrw->magic = RW_MAGIC;
    rw->name = mrw->name = name;
    rw->cls = cls;
    mrw->cls = meta;
    mrw->meta = 1;
    u32 base = super ? cx_instance_size(super) : 8;
    rw->size = (base + extra + 7) & ~7u;
    cls->isa = meta;
    cls->super = super;
    cls->vtable = rw;
    cls->cache = _objc_empty_cache;
    meta->vtable = mrw;
    meta->cache = _objc_empty_cache;
    if (super) {
        cx_class_t *root = super;
        while (root->super) root = root->super;
        meta->isa = root->isa;
        meta->super = super->isa;
    } else {
        meta->isa = meta;
        meta->super = cls;
    }
    register_class(cls);
    return cls;
}

void cx_add_method(cx_class_t *cls, const char *sel, void *imp) {
    if (!rw_of(cls)) { cx_fdprintf(2, "objc: classe não definida ao adicionar -%s\n", sel); return; }
    add_meth(rw_of(cls), sel_registerName(sel), (IMP)imp, "@@:");
}
void cx_add_cmethod(cx_class_t *cls, const char *sel, void *imp) { add_meth(rw_of(cls->isa), sel_registerName(sel), (IMP)imp, "@@:"); }

/* tipos (codificação ObjC) de um método, procurando nas superclasses */
const char *cx_method_types(cx_class_t *c, SEL sel) {
    for (; c; c = c->super) {
        cx_rw_t *rw = rw_of(c);
        if (!rw) continue;
        for (u32 i = rw->nm; i-- > 0;) if (rw->m[i].sel == sel) return rw->m[i].types;
    }
    return 0;
}

/* ================================================================ busca de métodos */
static IMP find_method(cx_class_t *c, SEL sel) {
    for (; c; c = c->super) {
        cx_rw_t *rw = rw_of(c);
        if (!rw) { realize(c); rw = rw_of(c); }
        if (!rw) continue;
        for (u32 i = rw->nm; i-- > 0;) if (rw->m[i].sel == sel) return rw->m[i].imp;
    }
    return 0;
}

static void cache_put(cx_rw_t *rw, SEL sel, IMP imp) {
    if (!rw->cache || (rw->cache_used + 1) * 4 > (rw->cache_mask + 1) * 3) {
        u32 nm = rw->cache ? (rw->cache_mask + 1) * 2 : 16;
        void *old = rw->cache;
        u32 om = rw->cache_mask;
        rw->cache = calloc(nm, sizeof *rw->cache);
        rw->cache_mask = nm - 1;
        rw->cache_used = 0;
        if (old) {
            for (u32 i = 0; i <= om; i++) {
                struct { SEL sel; IMP imp; } *o = (void *)((u8 *)old + i * 16);
                if (o->sel) cache_put(rw, o->sel, o->imp);
            }
            free(old);
        }
    }
    u32 i = (u32)((u64)sel >> 3) & rw->cache_mask;
    while (rw->cache[i].sel) i = (i + 1) & rw->cache_mask;
    rw->cache[i].sel = sel;
    rw->cache[i].imp = imp;
    rw->cache_used++;
}

static id missing_imp(id self, SEL sel);
static void *missing_stret(void *ret, id self, SEL sel);

static void maybe_initialize(cx_class_t *meta);

IMP cx_lookup(cx_class_t *c, SEL sel) {
    cx_rw_t *rw = rw_of(c);
    if (!rw) { realize(c); rw = rw_of(c); if (!rw) return (IMP)missing_imp; }
    if (rw->meta && !(rw->magic & 0)) maybe_initialize(c);
    if (rw->cache) {
        u32 i = (u32)((u64)sel >> 3) & rw->cache_mask;
        while (rw->cache[i].sel) {
            if (rw->cache[i].sel == sel) return rw->cache[i].imp;
            i = (i + 1) & rw->cache_mask;
        }
    }
    IMP imp = find_method(c, sel);
    if (!imp) return 0;
    cache_put(rw, sel, imp);
    return imp;
}

/* chamados por objc_msgSend (objc_msg.S) */
IMP cx_msg_lookup(cx_class_t *c, SEL sel) { IMP i = cx_lookup(c, sel); return i ? i : (IMP)missing_imp; }
IMP cx_msg_lookup_stret(cx_class_t *c, SEL sel) { IMP i = cx_lookup(c, sel); return i ? i : (IMP)missing_stret; }

BOOL cx_responds(cx_class_t *c, SEL sel) { return cx_lookup(c, sel) != 0; }
BOOL cx_is_subclass(cx_class_t *c, cx_class_t *of) { for (; c; c = c->super) if (c == of) return YES; return NO; }

/* +initialize: chamado uma vez, na primeira mensagem à classe */
static void maybe_initialize(cx_class_t *meta) {
    cx_rw_t *mrw = rw_of(meta);
    if (!mrw || mrw->magic != RW_MAGIC || (mrw->meta & 2)) return;
    mrw->meta |= 2;
    SEL s = sel_registerName("initialize");
    for (u32 i = mrw->nm; i-- > 0;)
        if (mrw->m[i].sel == s) {
            /* o objeto-classe é a instância cujo isa é esta metaclasse */
            for (u32 k = 0; k < nclasses; k++)
                if (all_classes[k]->isa == meta) { ((void (*)(id, SEL))mrw->m[i].imp)((id)all_classes[k], s); break; }
            break;
        }
}

static const char *warned[256];
static u32 nwarned;
static void warn_missing(id self, SEL sel) {
    for (u32 i = 0; i < nwarned; i++) if (warned[i] == sel) return;
    if (nwarned < 256) warned[nwarned++] = sel;
    cx_class_t *c = self ? self->isa : 0;
    cx_rw_t *rw = rw_of(c);
    cx_fdprintf(2, "objc[mOS]: %c[%s %s] ainda não implementado no mOS (devolvendo nil)\n",
                rw && rw->meta ? '+' : '-', rw ? rw->name : "?", sel ? sel : "?");
}
static id missing_imp(id self, SEL sel) { warn_missing(self, sel); return 0; }
static void *missing_stret(void *ret, id self, SEL sel) { warn_missing(self, sel); memset(ret, 0, 32); return ret; }

/* ================================================================ API pública do runtime */
cx_class_t *object_getClass(id o) { return o ? o->isa : 0; }
cx_class_t *object_setClass(id o, cx_class_t *c) { cx_class_t *old = o ? o->isa : 0; if (o) o->isa = c; return old; }
const char *object_getClassName(id o) { return o ? cx_class_name(o->isa) : "nil"; }
const char *class_getName(cx_class_t *c) { return c ? cx_class_name(c) : "nil"; }
cx_class_t *class_getSuperclass(cx_class_t *c) { return c ? c->super : 0; }
u64 class_getInstanceSize(cx_class_t *c) { return cx_instance_size(c); }
BOOL class_isMetaClass(cx_class_t *c) { cx_rw_t *rw = rw_of(c); return rw && (rw->meta & 1); }
BOOL class_respondsToSelector(cx_class_t *c, SEL s) { return c && cx_lookup(c, s) != 0; }
static cx_class_t *class_by_hook(const char *name);
cx_class_t *objc_getClass(const char *name) { cx_class_t *c = cx_class_named(name); return c ? c : class_by_hook(name); }
cx_class_t *objc_lookUpClass(const char *name) { return objc_getClass(name); }
cx_class_t *objc_getMetaClass(const char *name) { cx_class_t *c = cx_class_named(name); return c ? c->isa : 0; }
cx_class_t *objc_getRequiredClass(const char *name) { return cx_class_named(name); }
IMP class_getMethodImplementation(cx_class_t *c, SEL s) { return cx_msg_lookup(c, s); }
IMP class_getMethodImplementation_stret(cx_class_t *c, SEL s) { return cx_msg_lookup_stret(c, s); }

typedef struct { SEL name; const char *types; IMP imp; } method_t;
static method_t *method_of(cx_class_t *c, SEL s) {
    for (; c; c = c->super) {
        cx_rw_t *rw = rw_of(c);
        if (!rw) continue;
        for (u32 i = rw->nm; i-- > 0;) if (rw->m[i].sel == s) return (method_t *)&rw->m[i];
    }
    return 0;
}
/* cx_meth_t = {sel, imp, types}: method_* traduzem */
void *class_getInstanceMethod(cx_class_t *c, SEL s) { return method_of(c, s); }
void *class_getClassMethod(cx_class_t *c, SEL s) { return c ? method_of(c->isa, s) : 0; }
SEL method_getName(cx_meth_t *m) { return m ? m->sel : 0; }
IMP method_getImplementation(cx_meth_t *m) { return m ? m->imp : 0; }
const char *method_getTypeEncoding(cx_meth_t *m) { return m ? m->types : 0; }
IMP method_setImplementation(cx_meth_t *m, IMP imp) { IMP o = m->imp; m->imp = imp; flush_caches(); return o; }
void method_exchangeImplementations(cx_meth_t *a, cx_meth_t *b) { IMP t = a->imp; a->imp = b->imp; b->imp = t; flush_caches(); }
BOOL class_addMethod(cx_class_t *c, SEL s, IMP imp, const char *types) {
    cx_rw_t *rw = rw_of(c);
    if (!rw) return NO;
    for (u32 i = 0; i < rw->nm; i++) if (rw->m[i].sel == s) return NO;
    add_meth(rw, s, imp, types);
    flush_caches();
    return YES;
}
IMP class_replaceMethod(cx_class_t *c, SEL s, IMP imp, const char *types) {
    cx_rw_t *rw = rw_of(c);
    if (!rw) return 0;
    for (u32 i = 0; i < rw->nm; i++) if (rw->m[i].sel == s) { IMP o = rw->m[i].imp; rw->m[i].imp = imp; flush_caches(); return o; }
    add_meth(rw, s, imp, types);
    flush_caches();
    return 0;
}

/* classes criadas em tempo de execução */
cx_class_t *objc_allocateClassPair(cx_class_t *super, const char *name, u64 extra) {
    if (cx_class_named(name)) return 0;
    cx_class_t *c = calloc(1, sizeof *c + extra), *m = calloc(1, sizeof *m);
    cx_define_class(c, m, strdup(name), super, 0);
    return c;
}
void objc_registerClassPair(cx_class_t *c) { (void)c; }

/* ================================================================ o que o runtime Swift (com interop ObjC) chama
 * Ver docs/swift-objc-runtime.md, fase 1. A metadata de uma classe Swift começa com os mesmos 5 words do
 * objc_class (isa, superclass, cache[2], data com os bits Swift embaixo); o rw do mOS mora no 2º word do cache,
 * que o compilador Swift também emite como _objc_empty_vtable/0, então a realização é a mesma das classes ObjC. */

/* classe Swift (estática ou instanciada pelo runtime, ex. genéricas) que o Swift entrega para realizar;
 * a realização desliza as variáveis de offset dos campos (as mesmas que a lista de ivars aponta) */
cx_class_t *_objc_realizeClassFromSwift(cx_class_t *c, const void *previously) {
    (void)previously;
    if (!c) return 0;
    if (!rw_of(c)) { realize(c); flush_caches(); }
    return c;
}
cx_class_t *objc_readClassPair(cx_class_t *c, const void *image_info) { (void)image_info; return _objc_realizeClassFromSwift(c, 0); }
int _objc_supportsLazyRealization(void) { return 1; }
int cx_is_crr(id o) { return is_crr(o); }

/* ganchos: o Swift procura classes pelo nome mangled (objc_getClass("_TtC...")) e dá nome a imagens */
typedef BOOL (*cx_getclass_hook_t)(const char *name, cx_class_t **out);
typedef BOOL (*cx_imagename_hook_t)(cx_class_t *c, const char **out);
typedef const char *(*cx_namer_hook_t)(cx_class_t *c);
static BOOL getclass_default(const char *name, cx_class_t **out) { *out = cx_class_named(name); return *out != 0; }
static cx_getclass_hook_t getclass_hook = getclass_default;
static cx_class_t *class_by_hook(const char *name) {
    cx_class_t *c = 0;
    if (!name || getclass_hook == getclass_default) return 0;
    return getclass_hook(name, &c) ? c : 0;
}
void objc_setHook_getClass(cx_getclass_hook_t h, cx_getclass_hook_t *old) { if (old) *old = getclass_hook; getclass_hook = h; }
static BOOL imagename_default(cx_class_t *c, const char **out) {
    extern int dladdr(const void *addr, void *info);
    struct { const char *fname; void *fbase; const char *sname; void *saddr; } info = {0};
    *out = (c && dladdr(c, &info)) ? info.fname : 0;
    return *out != 0;
}
static cx_imagename_hook_t imagename_hook = imagename_default;
void objc_setHook_getImageName(cx_imagename_hook_t h, cx_imagename_hook_t *old) { if (old) *old = imagename_hook; imagename_hook = h; }
const char *class_getImageName(cx_class_t *c) { const char *n = 0; return imagename_hook(c, &n) ? n : 0; }
static cx_namer_hook_t namer_hook;
void objc_setHook_lazyClassNamer(cx_namer_hook_t h, cx_namer_hook_t *old) { if (old) *old = namer_hook; namer_hook = h; }
static void *copy_fixup;
void _objc_setClassCopyFixupHandler(void *h) { copy_fixup = h; }   /* só KVO copia classes; o mOS não copia */

/* imagens carregadas: o Swift descobre metadata/conformidades das imagens por aqui (ImageInspectionMachO) */
typedef void (*cx_loadimage_t)(const void *mh);
static cx_loadimage_t loadimage_fns[16];
static int nloadimage;
static void loadimage_bridge(const void *mh, i64 slide) { (void)slide; for (int i = 0; i < nloadimage; i++) loadimage_fns[i](mh); }
void objc_addLoadImageFunc(cx_loadimage_t f) {
    extern void _dyld_register_func_for_add_image(void (*)(const void *, i64));
    if (!f || nloadimage == 16) return;
    loadimage_fns[nloadimage++] = f;
    if (nloadimage == 1) _dyld_register_func_for_add_image(loadimage_bridge);
    else { extern u32 _dyld_image_count(void); extern const void *_dyld_get_image_header(u32);
           for (u32 i = 0, n = _dyld_image_count(); i < n; i++) f(_dyld_get_image_header(i)); }
}

BOOL object_isClass(id o) { cx_rw_t *rw = o ? rw_of(o->isa) : 0; return rw && (rw->meta & 1); }
cx_class_t *class_setSuperclass(cx_class_t *c, cx_class_t *s) {
    if (!c) return 0;
    cx_class_t *old = c->super;
    c->super = s;
    if (c->isa && s) c->isa->super = s->isa;
    flush_caches();
    return old;
}
id objc_autorelease(id o);
id _objc_rootAutorelease(id o) { return objc_autorelease(o); }
/* dyld: "este endereço é uma constante ObjC da imagem (ex.: __NSCFConstantString)?". O mOS reescreve as
 * CFString constantes no carregamento (fix_cfstrings), então o Swift não deve tratá-las como as da Apple. */
BOOL _dyld_is_objc_constant(int kind, const void *addr) { (void)kind; (void)addr; return NO; }
/* versão do sistema para #available no runtime Swift (Availability.mm): a mesma do kern.osproductversion */
int os_system_version_get_current_version(u32 v[3]) { if (!v) return -1; v[0] = 14; v[1] = 0; v[2] = 0; return 0; }
/* compiler-rt (os_version_check): o que `if #available`/@available chamam (a stdlib Swift com SWIFT_RUNTIME_OS_VERSIONING
 * e código C/ObjC). Plataforma 1 = macOS; as outras (iOS do Catalyst etc.) não existem no mOS. */
static int cx_os_at_least(u32 maj, u32 min, u32 sub) {
    u32 v[3] = {14, 0, 0};
    if (v[0] != maj) return v[0] > maj;
    if (v[1] != min) return v[1] > min;
    return v[2] >= sub;
}
int __isPlatformVersionAtLeast(u32 platform, u32 maj, u32 min, u32 sub) { return platform == 1 ? cx_os_at_least(maj, min, sub) : 0; }
int __isPlatformOrVariantPlatformVersionAtLeast(u32 p, u32 maj, u32 min, u32 sub, u32 vp, u32 vmaj, u32 vmin, u32 vsub) {
    return __isPlatformVersionAtLeast(p, maj, min, sub) || __isPlatformVersionAtLeast(vp, vmaj, vmin, vsub);
}
int __isOSVersionAtLeast(int maj, int min, int sub) { return cx_os_at_least((u32)maj, (u32)min, (u32)sub); }
/* o que o compiler-rt dos binários chama no libSystem do macOS 11+: {plataforma, versão 0xMMMMmmpp}; vale se alguma bater */
BOOL _availability_version_check(u32 count, const struct { u32 platform, version; } *v) {
    for (u32 i = 0; i < count; i++)
        if (v[i].platform == 1 && cx_os_at_least(v[i].version >> 16, (v[i].version >> 8) & 0xFF, v[i].version & 0xFF)) return YES;
    return NO;
}

/* isa do mOS é ponteiro puro e não há tagged pointers: a máscara do x86_64 da Apple preserva o ponteiro */
const u64 objc_debug_isa_class_mask = 0x00007ffffffffff8ULL;
const u64 objc_debug_taggedpointer_mask = 0;
BOOL class_addIvar(cx_class_t *c, const char *name, u64 size, u8 align, const char *types) {
    cx_rw_t *rw = rw_of(c);
    (void)name; (void)types;
    if (!rw) return NO;
    u32 a = 1u << align;
    rw->size = (rw->size + a - 1) & ~(a - 1);
    rw->size += (u32)size;
    return YES;
}

/* variáveis de instância por nome (KVC simples, object_getIvar) */
void *class_getInstanceVariable(cx_class_t *c, const char *name) {
    for (; c; c = c->super) {
        cx_rw_t *rw = rw_of(c);
        if (!rw) continue;
        for (u32 i = 0; i < rw->nivars; i++) if (!strcmp(rw->ivars[i].name, name)) return &rw->ivars[i];
    }
    return 0;
}
i64 ivar_getOffset(void *iv) { return iv ? *((u32 **)iv)[1] : 0; }   /* {name, offset, type} */
const char *ivar_getName(void *iv) { return iv ? *(const char **)iv : 0; }
id object_getIvar(id o, void *iv) { return iv && o ? *(id *)((u8 *)o + **(u32 **)((u8 *)iv + 8)) : 0; }
void object_setIvar(id o, void *iv, id v) { if (iv && o) *(id *)((u8 *)o + **(u32 **)((u8 *)iv + 8)) = v; }

/* ================================================================ objetos e memória
 * Cada objeto criado aqui tem um cabeçalho de 16 bytes antes do isa: {magic, contagem}. Objetos
 * sem cabeçalho (strings constantes, classes, blocks, estáticos) ignoram retain/release. Ao chegar
 * a zero, -dealloc roda (o do app primeiro, o do NSObject por último libera a memória). */
#define OBJ_MAGIC 0x4F424A43u
typedef struct { u32 magic; i32 rc; u64 pad; } objhdr_t;
static objhdr_t *hdr(id o) {
    if (!o || ((u64)o & 7)) return 0;
    objhdr_t *h = (objhdr_t *)((u8 *)o - 16);
    return h->magic == OBJ_MAGIC ? h : 0;
}
static id new_obj(cx_class_t *cls, u64 size) {
    u8 *b = calloc(1, 16 + size);
    if (!b) return 0;
    objhdr_t *h = (objhdr_t *)b;
    h->magic = OBJ_MAGIC;
    h->rc = 1;
    id o = (id)(b + 16);
    o->isa = cls;
    return o;
}
id cx_alloc_instance(cx_class_t *cls) { u32 sz = cx_instance_size(cls); return new_obj(cls, sz < 16 ? 16 : sz); }
id class_createInstance(cx_class_t *c, u64 extra) { return new_obj(c, cx_instance_size(c) + extra); }

/* referências weak que zeram no dealloc */
typedef struct { id *loc; id obj; } weakref_t;
static weakref_t *weaks;
static u32 nweaks, capweaks;
static volatile int weak_lock;
static void wl(void) { while (__sync_lock_test_and_set(&weak_lock, 1)) SYS0(SYS_YIELD); }
static void wu(void) { __sync_lock_release(&weak_lock); }
static void weak_unregister(id *loc) {
    for (u32 i = 0; i < nweaks; i++) if (weaks[i].loc == loc) { weaks[i] = weaks[--nweaks]; return; }
}
id objc_storeWeak(id *loc, id v) {
    wl();
    weak_unregister(loc);
    int crr = v && !hdr(v) && is_crr(v);
    if (crr) {
        /* objeto Swift: pergunta se aceita referência fraca e avisa que tem uma (assim o
         * swift_deallocClassInstance chama objc_destructInstance, que zera a referência) */
        wu();
        int ok = (int)(u64)crr_send(v, "allowsWeakReference") & 0xFF;
        if (ok) crr_send(v, "_setWeaklyReferenced");
        wl();
        if (!ok) { *loc = 0; wu(); return 0; }
    }
    if (v && (hdr(v) || crr)) {
        if (nweaks == capweaks) { capweaks = capweaks ? capweaks * 2 : 64; weaks = realloc(weaks, capweaks * sizeof *weaks); }
        weaks[nweaks].loc = loc;
        weaks[nweaks++].obj = v;
    }
    *loc = v;
    wu();
    return v;
}
id objc_initWeak(id *loc, id v) { *loc = 0; return objc_storeWeak(loc, v); }
id objc_initWeakOrNil(id *loc, id v) { return objc_initWeak(loc, v); }
id objc_storeWeakOrNil(id *loc, id v) { return objc_storeWeak(loc, v); }
id objc_retain(id o);
id objc_loadWeakRetained(id *loc) { return objc_retain(*loc); }
id objc_loadWeak(id *loc) { return *loc; }
void objc_destroyWeak(id *loc) { objc_storeWeak(loc, 0); }
void objc_copyWeak(id *dst, id *src) { objc_initWeak(dst, *src); }
void objc_moveWeak(id *dst, id *src) { objc_initWeak(dst, *src); objc_storeWeak(src, 0); }
static void weak_clear(id o) {
    wl();
    for (u32 i = 0; i < nweaks;) {
        if (weaks[i].obj == o) { *weaks[i].loc = 0; weaks[i] = weaks[--nweaks]; }
        else i++;
    }
    wu();
}

static id crr_send(id o, const char *sel) {
    static SEL cache[8]; static const char *names[8]; static int n;
    SEL s = 0;
    for (int i = 0; i < n; i++) if (names[i] == sel) { s = cache[i]; break; }
    if (!s) { s = sel_registerName(sel); if (n < 8) { names[n] = sel; cache[n++] = s; } }
    return ((id (*)(id, SEL))objc_msgSend)(o, s);
}
/* objetos do libdispatch (dispatch.c, extra.c): o isa é uma classe OS_dispatch_* (swiftobjc.c), mas eles não têm o
 * cabeçalho objhdr_t. Retain/release ObjC (ARC do Chromium com OS_OBJECT_USE_OBJC, o Swift) usam a contagem do
 * dispatch.c, como o libdispatch da Apple; nunca o hdr()/dealloc/cx_object_free genérico. O dispatch_data não entra:
 * no mOS ele é um CFData de verdade. */
#define DQ(N) extern cx_class_t N##_dqc __asm__("OBJC_CLASS_$_" #N);
DQ(OS_dispatch_queue) DQ(OS_dispatch_queue_serial) DQ(OS_dispatch_queue_concurrent) DQ(OS_dispatch_queue_main)
DQ(OS_dispatch_queue_global) DQ(OS_dispatch_queue_attr) DQ(OS_dispatch_group) DQ(OS_dispatch_semaphore)
DQ(OS_dispatch_source) DQ(OS_dispatch_io) DQ(OS_dispatch_workloop) DQ(OS_dispatch_object)
#undef DQ
void cx_dispatch_retain(void *o);
void cx_dispatch_release(void *o);
long cx_dispatch_retain_count(void *o);
int cx_is_dispatch_object(id o) {
    if (!o || ((u64)o & 7)) return 0;
    cx_class_t *c = o->isa;
    return c == &OS_dispatch_queue_serial_dqc || c == &OS_dispatch_queue_concurrent_dqc || c == &OS_dispatch_queue_main_dqc ||
           c == &OS_dispatch_queue_global_dqc || c == &OS_dispatch_group_dqc || c == &OS_dispatch_semaphore_dqc ||
           c == &OS_dispatch_source_dqc || c == &OS_dispatch_queue_dqc || c == &OS_dispatch_queue_attr_dqc ||
           c == &OS_dispatch_io_dqc || c == &OS_dispatch_workloop_dqc || c == &OS_dispatch_object_dqc;
}
id objc_retain(id o) {
    if (cx_is_dispatch_object(o)) { cx_dispatch_retain(o); return o; }
    objhdr_t *h = hdr(o);
    if (h) __sync_fetch_and_add(&h->rc, 1);
    else if (is_crr(o)) crr_send(o, "retain");
    return o;
}
void objc_release(id o) {
    if (cx_is_dispatch_object(o)) { cx_dispatch_release(o); return; }
    objhdr_t *h = hdr(o);
    if (!h) { if (is_crr(o)) crr_send(o, "release"); return; }
    if (__sync_sub_and_fetch(&h->rc, 1) == 0) {
        h->rc = 0x3FFFFFFF;                        /* evita re-entrada durante o dealloc */
        ((void (*)(id, SEL))objc_msgSend)(o, sel_registerName("dealloc"));
    }
}
i32 cx_retain_count(id o) {
    if (cx_is_dispatch_object(o)) { long n = cx_dispatch_retain_count(o); return n > 0x7FFFFFFF ? 0x7FFFFFFF : (i32)n; }
    objhdr_t *h = hdr(o);
    if (h) return h->rc;
    if (is_crr(o)) { u64 n = (u64)crr_send(o, "retainCount"); return n > 0x7FFFFFFF ? 0x7FFFFFFF : (i32)n; }
    return 1;
}
/* objc_isUniquelyReferenced: o isUniquelyReferenced_nonNull do Swift para objetos ObjC */
BOOL objc_isUniquelyReferenced(id o) { return cx_retain_count(o) == 1; }
/* chamado pelo -[NSObject dealloc]: zera weaks e devolve a memória */
/* objc_destructInstance: o clang gera -.cxx_destruct para soltar as ivars __strong (ARC) de cada classe */
void cx_destruct_instance(id o) {
    static SEL cxx;
    if (!cxx) cxx = sel_registerName(".cxx_destruct");
    for (cx_class_t *c = o->isa; c; c = c->super) {
        cx_rw_t *rw = rw_of(c);
        if (!rw) continue;
        for (u32 i = rw->nm; i-- > 0;) if (rw->m[i].sel == cxx) { ((void (*)(id, SEL))rw->m[i].imp)(o, cxx); break; }
    }
}
/* também chamado pelo swift_deallocClassInstance (runtime Swift com interop) para objetos Swift */
void *objc_destructInstance(id o) {
    if (o) {
        cx_destruct_instance(o);
        weak_clear(o);
        { extern void cx_assoc_remove_all(id o); cx_assoc_remove_all(o); }
    }
    return o;
}
void cx_object_free(id o) {
    if (cx_is_dispatch_object(o)) return;           /* sem objhdr_t: a memória é do dispatch.c */
    objhdr_t *h = hdr(o);
    if (!h) return;
    weak_clear(o);
    cx_destruct_instance(o);
    { extern void cx_assoc_remove_all(id o); cx_assoc_remove_all(o); }   /* objetos associados (objcrt.c) */
    h->magic = 0xDEADBEEF;
    free(h);
}
id object_dispose(id o) { cx_object_free(o); return 0; }

/* autorelease pools: pilha de objetos; o marcador é a altura da pilha */
static id *pool;
static u64 npool, cappool;
static volatile int pool_lock;
id objc_autorelease(id o) {
    if (!hdr(o) && !is_crr(o) && !cx_is_dispatch_object(o)) return o;
    while (__sync_lock_test_and_set(&pool_lock, 1)) SYS0(SYS_YIELD);
    if (npool == cappool) { cappool = cappool ? cappool * 2 : 1024; pool = realloc(pool, cappool * sizeof(id)); }
    pool[npool++] = o;
    __sync_lock_release(&pool_lock);
    return o;
}
void *objc_autoreleasePoolPush(void) { return (void *)(npool + 1); }
void objc_autoreleasePoolPop(void *m) {
    u64 mk = (u64)m - 1;
    while (npool > mk) {
        id o = pool[--npool];
        objc_release(o);
    }
}
void *_objc_autoreleasePoolPush(void) { return objc_autoreleasePoolPush(); }
void _objc_autoreleasePoolPop(void *p) { objc_autoreleasePoolPop(p); }
id objc_retainAutorelease(id o) { return objc_autorelease(objc_retain(o)); }
id objc_retainAutoreleasedReturnValue(id o) { return objc_retain(o); }
id objc_claimAutoreleasedReturnValue(id o) { return objc_retain(o); }
id objc_unsafeClaimAutoreleasedReturnValue(id o) { return o; }
id objc_autoreleaseReturnValue(id o) { return objc_autorelease(o); }
id objc_retainAutoreleaseReturnValue(id o) { return objc_autorelease(objc_retain(o)); }
void objc_storeStrong(id *loc, id v) { objc_retain(v); id old = *loc; *loc = v; objc_release(old); }int objc_sync_enter(id o) { (void)o; return 0; }
int objc_sync_exit(id o) { (void)o; return 0; }
void objc_enumerationMutation(id o) { cx_fdprintf(2, "coleção alterada durante a enumeração (%s)\n", object_getClassName(o)); abort(); }
__attribute__((noreturn)) void objc_exception_throw(id e) {
    const char *d = e ? MSG0(const char *, MSG0(id, e, "description"), "UTF8String") : "nil";
    cx_fdprintf(2, "*** Exceção Objective-C não tratada: %s\n", d ? d : "?");
    abort();
}
__attribute__((noreturn)) void objc_exception_rethrow(void) { abort(); }
void *objc_begin_catch(void *e) { return e; }
void objc_end_catch(void) {}
void objc_terminate(void) { abort(); }
int __objc_personality_v0(void) { return 8; /* _URC_CONTINUE_UNWIND */ }

/* atalhos que o clang moderno emite no lugar de mensagens */
id objc_alloc(cx_class_t *c) { return c ? MSG0(id, c, "alloc") : 0; }
id objc_allocWithZone(cx_class_t *c) { return c ? MSG0(id, c, "alloc") : 0; }
id objc_alloc_init(cx_class_t *c) { return c ? MSG0(id, MSG0(id, c, "alloc"), "init") : 0; }
id objc_opt_new(cx_class_t *c) { return c ? MSG0(id, c, "new") : 0; }
id objc_opt_self(id o) { return o; }
cx_class_t *objc_opt_class(id o) { return o ? MSG0(cx_class_t *, o, "class") : 0; }
BOOL objc_opt_isKindOfClass(id o, cx_class_t *c) { return o ? MSG(BOOL, o, "isKindOfClass:", c) : NO; }
BOOL objc_opt_respondsToSelector(id o, SEL s) { return o ? MSG(BOOL, o, "respondsToSelector:", s) : NO; }

/* propriedades sintetizadas */
id objc_getProperty(id self, SEL cmd, i64 off, BOOL atomic) { (void)cmd; (void)atomic; return *(id *)((u8 *)self + off); }
static void set_slot(id self, i64 off, id nv) { id *slot = (id *)((u8 *)self + off); id old = *slot; *slot = nv; objc_release(old); }
void objc_setProperty(id self, SEL cmd, i64 off, id v, BOOL atomic, signed char copy) {
    (void)cmd; (void)atomic;
    set_slot(self, off, (copy && v) ? MSG0(id, v, copy == 2 ? "mutableCopy" : "copy") : objc_retain(v));
}
void objc_setProperty_nonatomic(id s, SEL c, id v, i64 off) { (void)c; set_slot(s, off, objc_retain(v)); }
void objc_setProperty_atomic(id s, SEL c, id v, i64 off) { (void)c; set_slot(s, off, objc_retain(v)); }
void objc_setProperty_nonatomic_copy(id s, SEL c, id v, i64 off) { (void)c; set_slot(s, off, v ? MSG0(id, v, "copy") : 0); }
void objc_setProperty_atomic_copy(id s, SEL c, id v, i64 off) { (void)c; set_slot(s, off, v ? MSG0(id, v, "copy") : 0); }
id objc_getProperty_r(id self, SEL cmd, i64 off, BOOL atomic) { (void)cmd; (void)atomic; return objc_autorelease(objc_retain(*(id *)((u8 *)self + off))); }
void objc_copyStruct(void *d, const void *s, i64 sz, BOOL atomic, BOOL strong) { (void)atomic; (void)strong; memcpy(d, s, (size_t)sz); }
void objc_copyCppObjectAtomic(void *d, const void *s, void (*cp)(void *, const void *)) { cp(d, s); }

/* ================================================================ blocks */
typedef struct { u64 reserved, size; void (*copy)(void *, const void *); void (*dispose)(const void *); } block_desc_t;
typedef struct { void *isa; int flags, reserved; void *invoke; block_desc_t *desc; } block_t;
typedef struct byref { void *isa; struct byref *forwarding; int flags, size; void (*keep)(void *, void *); void (*destroy)(void *); } byref_t;

cx_class_t _NSConcreteStackBlock_c __asm__("_NSConcreteStackBlock");
cx_class_t _NSConcreteGlobalBlock_c __asm__("_NSConcreteGlobalBlock");
cx_class_t _NSConcreteMallocBlock_c __asm__("_NSConcreteMallocBlock");
static cx_class_t block_meta[3];

void *_Block_copy(const void *arg) {
    block_t *b = (block_t *)arg;
    if (!b) return 0;
    if (b->flags & (1 << 28)) return b;            /* global */
    if (b->flags & (1 << 24)) return b;            /* já no heap */
    block_t *n = malloc(b->desc->size);
    memcpy(n, b, b->desc->size);
    n->isa = &_NSConcreteMallocBlock_c;
    n->flags |= 1 << 24;
    if (b->flags & (1 << 25)) b->desc->copy(n, b);
    return n;
}
void _Block_release(const void *b) { (void)b; }
id objc_retainBlock(id b) { return _Block_copy(b); }
/* flags: 3 objeto, 7 block, 8 __block (byref), +16 __weak, +128 chamado de um helper de byref */
void _Block_object_assign(void *dst, const void *obj, int flags) {
    if (flags & 0x80) {                                   /* campo dentro de uma variável __block */
        *(const void **)dst = ((flags & 0x1F) == 3) ? objc_retain((id)obj) : obj;
        return;
    }
    switch (flags & 0x1F) {
    case 8: {                                             /* __block: move para o heap uma vez */
        byref_t *src = (byref_t *)obj;
        if (src->forwarding == src) {
            byref_t *cp = malloc((size_t)src->size);
            memcpy(cp, src, (size_t)src->size);
            cp->forwarding = cp;
            src->forwarding = cp;
            if (src->flags & (1 << 25)) src->keep(cp, src);
            *(void **)dst = cp;
        } else *(void **)dst = src->forwarding;
        return;
    }
    case 7: *(void **)dst = _Block_copy(obj); return;
    case 3: *(const void **)dst = objc_retain((id)obj); return;
    default: *(const void **)dst = obj; return;           /* __weak e afins */
    }
}
void _Block_object_dispose(const void *obj, int flags) { if (!(flags & 0x80) && (flags & 0x1F) == 3) objc_release((id)obj); }
static id block_copy_m(id self, SEL s) { (void)s; return _Block_copy(self); }
static id block_self_m(id self, SEL s) { (void)s; return self; }

/* ================================================================ imagem compilada */
static void fix_cfstrings(u8 *a, u64 size) {
    /* {isa, flags, ptr, len}: constantes UTF-16 (flag 0x10) viram UTF-8 */
    for (u64 o = 0; o + 32 <= size; o += 32) {
        u64 *e = (u64 *)(a + o);
        u32 flags = (u32)e[1];
        if (!(flags & 0x10)) continue;
        const u16 *w = (const u16 *)e[2];
        u64 n = e[3];
        char *s = malloc(n * 3 + 1);
        u64 k = 0;
        for (u64 i = 0; i < n; i++) {
            u32 c = w[i];
            if (c >= 0xD800 && c < 0xDC00 && i + 1 < n) { c = 0x10000 + ((c - 0xD800) << 10) + (w[i + 1] - 0xDC00); i++; }
            if (c < 0x80) s[k++] = (char)c;
            else if (c < 0x800) { s[k++] = (char)(0xC0 | c >> 6); s[k++] = (char)(0x80 | (c & 0x3F)); }
            else if (c < 0x10000) { s[k++] = (char)(0xE0 | c >> 12); s[k++] = (char)(0x80 | ((c >> 6) & 0x3F)); s[k++] = (char)(0x80 | (c & 0x3F)); }
            else { s[k++] = (char)(0xF0 | c >> 18); s[k++] = (char)(0x80 | ((c >> 12) & 0x3F)); s[k++] = (char)(0x80 | ((c >> 6) & 0x3F)); s[k++] = (char)(0x80 | (c & 0x3F)); }
        }
        s[k] = 0;
        e[1] = (flags & ~0x10u) | 0x08;
        e[2] = (u64)s;
        e[3] = k;
    }
}

static void attach_category(const u8 *cat) {
    cx_class_t *c = *(cx_class_t *const *)(cat + 8);
    if (!c) return;
    realize(c);
    cx_rw_t *rw = rw_of(c), *mrw = rw_of(c->isa);
    if (rw) load_methods(rw, *(const u8 *const *)(cat + 16));
    if (mrw) load_methods(mrw, *(const u8 *const *)(cat + 24));
}

void cx_objc_init_image(u64 sects, u64 n) {
    const u64 *s = (const u64 *)sects;
    /* literais constantes do Xcode 15+ (__objc_dictobj/arrayobj/intobj/doubleobj/floatobj = kinds 12..16): literals.c */
    { extern void cx_darwin_literals(u8 *a, u64 size, u64 kind) __attribute__((weak));
      if (cx_darwin_literals) for (u64 i = 0; i < n; i++) if (s[i * 3 + 2] >= 12) cx_darwin_literals((u8 *)s[i * 3], s[i * 3 + 1], s[i * 3 + 2]); }
    /* ordem: strings, seletores, classes, categorias, +load */
    for (int pass = 0; pass < 5; pass++)
        for (u64 i = 0; i < n; i++) {
            u8 *a = (u8 *)s[i * 3];
            u64 size = s[i * 3 + 1], kind = s[i * 3 + 2];
            if (pass == 0 && kind == 7) fix_cfstrings(a, size);
            if (pass == 0 && (kind == 6 || kind == 10)) { extern void cx_objc_protocols(u8 *a, u64 size, u64 kind); cx_objc_protocols(a, size, kind); }
            if (pass == 3 && (kind == 2 || kind == 11)) {
                extern void cx_objc_category_protocols(const u8 *cat);
                for (u64 o = 0; o + 8 <= size; o += 8) cx_objc_category_protocols(*(const u8 **)(a + o));
            }
            if (pass == 1 && kind == 3)
                for (u64 o = 0; o + 8 <= size; o += 8) *(SEL *)(a + o) = sel_registerName(*(const char **)(a + o));
            if (pass == 2 && kind == 1)
                for (u64 o = 0; o + 8 <= size; o += 8) realize(*(cx_class_t **)(a + o));
            if (pass == 3 && (kind == 2 || kind == 11))
                for (u64 o = 0; o + 8 <= size; o += 8) attach_category(*(const u8 **)(a + o));
            if (pass == 4 && kind == 8)
                for (u64 o = 0; o + 8 <= size; o += 8) {
                    cx_class_t *c = *(cx_class_t **)(a + o);
                    cx_rw_t *mrw = rw_of(c->isa);
                    SEL ld = sel_registerName("load");
                    for (u32 k = 0; mrw && k < mrw->nm; k++)
                        if (mrw->m[k].sel == ld) { ((void (*)(id, SEL))mrw->m[k].imp)((id)c, ld); break; }
                }
        }
    flush_caches();
}

/* chamado por cx_start antes dos inicializadores */
static int objc_ready;
void cx_objc_boot(u64 sects, u64 n) {
    if (!objc_ready) {
        objc_ready = 1;
        cx_foundation_init();
        /* blocks são objetos: [block copy] etc. */
        cx_class_t *nsobj = cx_class_named("NSObject");
        cx_class_t *bl[3] = {&_NSConcreteStackBlock_c, &_NSConcreteGlobalBlock_c, &_NSConcreteMallocBlock_c};
        static const char *names[3] = {"__NSStackBlock__", "__NSGlobalBlock__", "__NSMallocBlock__"};
        for (int i = 0; i < 3; i++) {
            cx_define_class(bl[i], &block_meta[i], names[i], nsobj, 0);
            cx_add_method(bl[i], "copy", block_copy_m);
            cx_add_method(bl[i], "retain", block_self_m);
            cx_add_method(bl[i], "autorelease", block_self_m);
        }
        cx_appkit_init();
    { extern void cx_darwin_init(void) __attribute__((weak)); if (cx_darwin_init) cx_darwin_init(); }   /* gancho: NSURL/CF da sessão do VS Code */
    { extern void cx_swiftobjc_init(void); cx_swiftobjc_init(); }   /* classes OS_* para o Swift com interop (swiftobjc.c) */
    { extern void cx_chrome_init(void) __attribute__((weak)); if (cx_chrome_init) cx_chrome_init(); }   /* classes privadas que o Chrome pede (chrome.c) */
    }
    if (n) cx_objc_init_image(sects, n);
}
