/*
 * mOS — AU v3: AUAudioUnit, AUAudioUnitBus, AUAudioUnitBusArray, AUParameterNode/AUParameter/AUParameterGroup/AUParameterTree.
 * O AUAudioUnit é a classe base que os apps estendem (o WhatsApp tem a sua): renderBlock chama o internalRenderBlock da
 * subclasse (com recursos alocados), busses com AVAudioFormat (o formato muda só sem recursos alocados),
 * maximumFramesToRender, parameterTree e componentDescription. Parâmetros: valor com observador/provedor do
 * implementador, faixa, endereço, árvore com busca por endereço e por caminho de identificadores.
 * Código original do mOS (MIT).
 */
#include "foundation.h"

typedef int OSStatus;
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
int strcmp(const char *a, const char *b);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
#define BLK_FN(b) (((void **)(b))[2])

typedef struct { u32 type, subtype, manuf, flags, mask; } acdesc_t;
typedef struct { cx_class_t *isa; id format; BOOL enabled; id name; i64 index, type; id owner; u32 maxch; } bus_t;
typedef struct { cx_class_t *isa; id busses; id owner; i64 type; } busarr_t;
typedef struct {
    cx_class_t *isa;
    id ident, name, children, key_path;
    u64 address;
    float value, minv, maxv;
    u32 unit, flags;
    void *observer, *provider, *str_from_value;
    id parent;
} param_t;
typedef struct {
    cx_class_t *isa;
    acdesc_t desc;
    id inputs, outputs, tree, name;
    u32 max_frames;
    BOOL allocated, bypass;
    void *render_block;   /* o bloco de render que entrega para a subclasse */
    void *in_handler;
} auv3_t;
#define BUS(o) ((bus_t *)(o))
#define BA(o) ((busarr_t *)(o))
#define PRM(o) ((param_t *)(o))
#define AU(o) ((auv3_t *)(o))
cx_class_t AUAudioUnit_c __asm__("OBJC_CLASS_$_AUAudioUnit"), AUAudioUnit_m __asm__("OBJC_METACLASS_$_AUAudioUnit");
cx_class_t AUAudioUnitBus_c __asm__("OBJC_CLASS_$_AUAudioUnitBus"), AUAudioUnitBus_m __asm__("OBJC_METACLASS_$_AUAudioUnitBus");
cx_class_t AUAudioUnitBusArray_c __asm__("OBJC_CLASS_$_AUAudioUnitBusArray"), AUAudioUnitBusArray_m __asm__("OBJC_METACLASS_$_AUAudioUnitBusArray");
cx_class_t AUParameterNode_c __asm__("OBJC_CLASS_$_AUParameterNode"), AUParameterNode_m __asm__("OBJC_METACLASS_$_AUParameterNode");
cx_class_t AUParameter_c __asm__("OBJC_CLASS_$_AUParameter"), AUParameter_m __asm__("OBJC_METACLASS_$_AUParameter");
cx_class_t AUParameterGroup_c __asm__("OBJC_CLASS_$_AUParameterGroup"), AUParameterGroup_m __asm__("OBJC_METACLASS_$_AUParameterGroup");
cx_class_t AUParameterTree_c __asm__("OBJC_CLASS_$_AUParameterTree"), AUParameterTree_m __asm__("OBJC_METACLASS_$_AUParameterTree");
static id cls(const char *n) { return (id)cx_class_named(n); }
static void setobj(id *s, id v) { if (*s == v) return; if (v) objc_retain(v); if (*s) objc_release(*s); *s = v; }
static void super_dealloc(id s, SEL c) { IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c); if (d) ((void (*)(id, SEL))d)(s, c); }
static id error_with(i64 code, const char *msg) {
    id info = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, info, "setObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSOSStatusErrorDomain"), code, info);
}

/* ---------------------------------------------------------------- AUAudioUnitBus */
static id bus_init(id s, SEL c, id fmt, id *err) {
    (void)c;
    if (!fmt) { if (err) *err = error_with(-50, "AUAudioUnitBus precisa de um AVAudioFormat."); objc_release(s); return 0; }
    setobj(&BUS(s)->format, fmt);
    BUS(s)->enabled = YES;
    BUS(s)->maxch = 8;
    return s;
}
static id bus_format(id s, SEL c) { (void)c; return BUS(s)->format; }
static BOOL bus_set_format(id s, SEL c, id fmt, id *err) {
    (void)c;
    id own = BUS(s)->owner;
    if (!fmt) { if (err) *err = error_with(-50, "Formato nulo."); return NO; }
    if (own && AU(own)->allocated) { if (err) *err = error_with(-10875 /* kAudioUnitErr_FailedInitialization */, "Os recursos de render estão alocados: desaloque antes de mudar o formato."); return NO; }
    u32 ch = (u32)MSG0(u32, fmt, "channelCount");
    if (ch > BUS(s)->maxch) { if (err) *err = error_with(-10868 /* kAudioUnitErr_FormatNotSupported */, "Canais demais para o bus."); return NO; }
    setobj(&BUS(s)->format, fmt);
    if (err) *err = 0;
    return YES;
}
static BOOL bus_enabled(id s, SEL c) { (void)c; return BUS(s)->enabled; }
static void bus_set_enabled(id s, SEL c, BOOL v) { (void)c; BUS(s)->enabled = v; }
static id bus_name(id s, SEL c) { (void)c; return BUS(s)->name; }
static void bus_set_name(id s, SEL c, id v) { (void)c; setobj(&BUS(s)->name, v); }
static i64 bus_index(id s, SEL c) { (void)c; return BUS(s)->index; }
static i64 bus_type(id s, SEL c) { (void)c; return BUS(s)->type; }
static id bus_owner(id s, SEL c) { (void)c; return BUS(s)->owner; }
static u32 bus_maxch(id s, SEL c) { (void)c; return BUS(s)->maxch; }
static void bus_set_maxch(id s, SEL c, u32 v) { (void)c; BUS(s)->maxch = v; }
static void bus_dealloc(id s, SEL c) { setobj(&BUS(s)->format, 0); setobj(&BUS(s)->name, 0); super_dealloc(s, c); }

/* ---------------------------------------------------------------- AUAudioUnitBusArray */
static void ba_bind(id s) {
    u64 n = BA(s)->busses ? MSG0(u64, BA(s)->busses, "count") : 0;
    for (u64 i = 0; i < n; i++) { id b = MSG(id, BA(s)->busses, "objectAtIndex:", i); BUS(b)->owner = BA(s)->owner; BUS(b)->index = (i64)i; BUS(b)->type = BA(s)->type; }
}
static id ba_init(id s, SEL c, id au, i64 type, id busses) {
    (void)c;
    BA(s)->owner = au; BA(s)->type = type;
    setobj(&BA(s)->busses, busses ? MSG0(id, busses, "copy") : MSG0(id, cls("NSArray"), "array"));
    if (busses) objc_release(BA(s)->busses), objc_retain(BA(s)->busses);
    ba_bind(s);
    return s;
}
static id ba_init_empty(id s, SEL c, id au, i64 type) { return ba_init(s, c, au, type, 0); }
static u64 ba_count(id s, SEL c) { (void)c; return BA(s)->busses ? MSG0(u64, BA(s)->busses, "count") : 0; }
static id ba_at(id s, SEL c, u64 i) { (void)c; return i < ba_count(s, 0) ? MSG(id, BA(s)->busses, "objectAtIndex:", i) : 0; }
static BOOL ba_countable(id s, SEL c) { (void)s; (void)c; return NO; }
static void ba_replace(id s, SEL c, id busses) { (void)c; setobj(&BA(s)->busses, busses); ba_bind(s); }
static id ba_owner(id s, SEL c) { (void)c; return BA(s)->owner; }
static i64 ba_type(id s, SEL c) { (void)c; return BA(s)->type; }
static BOOL ba_set_count(id s, SEL c, u64 n, id *err) { (void)s; (void)c; (void)n; if (err) *err = error_with(-4, "Este array de bus não muda de tamanho."); return NO; }
static void ba_dealloc(id s, SEL c) { setobj(&BA(s)->busses, 0); super_dealloc(s, c); }

/* ---------------------------------------------------------------- parâmetros */
static id prm_create(id k, SEL c, id ident, id name, u64 addr, float minv, float maxv, u32 unit, id unit_name, u32 flags, id strings, id deps) {
    (void)k; (void)c; (void)unit_name; (void)strings; (void)deps;
    id p = cx_alloc_instance(&AUParameter_c);
    setobj(&PRM(p)->ident, ident); setobj(&PRM(p)->name, name);
    PRM(p)->address = addr; PRM(p)->minv = minv; PRM(p)->maxv = maxv; PRM(p)->unit = unit; PRM(p)->flags = flags;
    PRM(p)->value = minv;
    return objc_autorelease(p);
}
static id grp_create(id k, SEL c, id ident, id name, id children) {
    (void)k; (void)c;
    id g = cx_alloc_instance(&AUParameterGroup_c);
    setobj(&PRM(g)->ident, ident); setobj(&PRM(g)->name, name); setobj(&PRM(g)->children, children);
    for (u64 i = 0, n = children ? MSG0(u64, children, "count") : 0; i < n; i++) PRM(MSG(id, children, "objectAtIndex:", i))->parent = g;
    return objc_autorelease(g);
}
static id tree_create(id k, SEL c, id children) {
    (void)k; (void)c;
    id t = cx_alloc_instance(&AUParameterTree_c);
    setobj(&PRM(t)->children, children);
    for (u64 i = 0, n = children ? MSG0(u64, children, "count") : 0; i < n; i++) PRM(MSG(id, children, "objectAtIndex:", i))->parent = t;
    return objc_autorelease(t);
}
static id find_addr(id node, u64 addr) {
    if (!node) return 0;
    if (!PRM(node)->children) return PRM(node)->address == addr ? node : 0;   /* folha: parâmetro */
    for (u64 i = 0, n = PRM(node)->children ? MSG0(u64, PRM(node)->children, "count") : 0; i < n; i++) {
        id r = find_addr(MSG(id, PRM(node)->children, "objectAtIndex:", i), addr);
        if (r) return r;
    }
    return 0;
}
static id tree_by_addr(id s, SEL c, u64 addr) { (void)c; return find_addr(s, addr); }
static id tree_by_path(id s, SEL c, id path) {
    (void)c;
    const char *p = path ? cx_cstr(path) : 0;
    id node = s;
    while (p && *p && node) {
        const char *dot = p;
        while (*dot && *dot != '.') dot++;
        id next = 0;
        for (u64 i = 0, n = PRM(node)->children ? MSG0(u64, PRM(node)->children, "count") : 0; i < n && !next; i++) {
            id ch = MSG(id, PRM(node)->children, "objectAtIndex:", i);
            const char *ci = PRM(ch)->ident ? cx_cstr(PRM(ch)->ident) : 0;
            size_t l = (size_t)(dot - p), k = 0;
            if (ci) { while (k < l && ci[k] == p[k]) k++; if (k == l && !ci[l]) next = ch; }
        }
        node = next;
        p = *dot ? dot + 1 : dot;
    }
    return node == s ? 0 : node;
}
/* valor: o provedor do implementador manda na leitura; a escrita vai para o observador do implementador */
static float prm_value(id s, SEL c) {
    (void)c;
    if (PRM(s)->provider) return ((float (*)(void *, id))BLK_FN(PRM(s)->provider))(PRM(s)->provider, s);
    return PRM(s)->value;
}
static void prm_set_value(id s, SEL c, float v) {
    (void)c;
    if (v < PRM(s)->minv) v = PRM(s)->minv;
    if (v > PRM(s)->maxv) v = PRM(s)->maxv;
    PRM(s)->value = v;
    /* o observador fica na árvore (implementorValueObserver) ou no parâmetro */
    for (id n = s; n; n = PRM(n)->parent)
        if (PRM(n)->observer) { ((void (*)(void *, id, float))BLK_FN(PRM(n)->observer))(PRM(n)->observer, s, v); break; }
}
static void prm_set_value_orig(id s, SEL c, float v, void *token) { (void)token; prm_set_value(s, c, v); }
static u64 prm_address(id s, SEL c) { (void)c; return PRM(s)->address; }
static float prm_min(id s, SEL c) { (void)c; return PRM(s)->minv; }
static float prm_max(id s, SEL c) { (void)c; return PRM(s)->maxv; }
static u32 prm_unit(id s, SEL c) { (void)c; return PRM(s)->unit; }
static u32 prm_flags(id s, SEL c) { (void)c; return PRM(s)->flags; }
static id prm_ident(id s, SEL c) { (void)c; return PRM(s)->ident; }
static id prm_name(id s, SEL c) { (void)c; return PRM(s)->name; }
static id prm_children(id s, SEL c) { (void)c; return PRM(s)->children ? PRM(s)->children : MSG0(id, cls("NSArray"), "array"); }
static id prm_all(id s, SEL c) {
    id out = MSG0(id, cls("NSMutableArray"), "array");
    id kids = prm_children(s, c);
    for (u64 i = 0, n = MSG0(u64, kids, "count"); i < n; i++) {
        id ch = MSG(id, kids, "objectAtIndex:", i);
        if (PRM(ch)->children) { id sub = prm_all(ch, c); MSG(void, out, "addObjectsFromArray:", sub); }
        else MSG(void, out, "addObject:", ch);
    }
    return out;
}
static void set_block(void **slot, void *b) { if (*slot) _Block_release(*slot); *slot = b ? _Block_copy(b) : 0; }
static void prm_set_observer(id s, SEL c, void *b) { (void)c; set_block(&PRM(s)->observer, b); }
static void prm_set_provider(id s, SEL c, void *b) { (void)c; set_block(&PRM(s)->provider, b); }
static void *prm_observer(id s, SEL c) { (void)c; return PRM(s)->observer; }
static void *prm_provider(id s, SEL c) { (void)c; return PRM(s)->provider; }
static void prm_set_str(id s, SEL c, void *b) { (void)c; set_block(&PRM(s)->str_from_value, b); }
static id prm_string(id s, SEL c, const float *v) {
    (void)c;
    float x = v ? *v : prm_value(s, 0);
    if (PRM(s)->str_from_value) return ((id (*)(void *, id, const float *))BLK_FN(PRM(s)->str_from_value))(PRM(s)->str_from_value, s, &x);
    return MSG(id, cls("NSString"), "stringWithFormat:", cx_str("%.2f"), (double)x);
}
static void prm_dealloc(id s, SEL c) {
    setobj(&PRM(s)->ident, 0); setobj(&PRM(s)->name, 0); setobj(&PRM(s)->children, 0);
    set_block(&PRM(s)->observer, 0); set_block(&PRM(s)->provider, 0); set_block(&PRM(s)->str_from_value, 0);
    super_dealloc(s, c);
}

/* ---------------------------------------------------------------- AUAudioUnit */
static id au_init(id s, SEL c, acdesc_t d, u32 opts, id *err) {
    (void)c; (void)opts;
    AU(s)->desc = d;
    AU(s)->max_frames = 512;
    if (err) *err = 0;
    return s;
}
static id au_init_noopts(id s, SEL c, acdesc_t d, id *err) { return au_init(s, c, d, 0, err); }
/* instancia de forma assíncrona: só a própria classe (ou subclasse registrada) — o bloco recebe a unidade */
static void au_instantiate(id k, SEL c, acdesc_t d, u32 opts, void *blk) {
    (void)c;
    id err = 0;
    id u = au_init(cx_alloc_instance((cx_class_t *)k), 0, d, opts, &err);
    if (blk) ((void (*)(void *, id, id))BLK_FN(blk))(blk, objc_autorelease(u), err);
}
static acdesc_t au_desc(id s, SEL c) { (void)c; return AU(s)->desc; }
static id au_bus_array(id s, i64 type) {
    id *slot = type == 1 ? &AU(s)->inputs : &AU(s)->outputs;
    if (!*slot) *slot = ba_init(cx_alloc_instance(&AUAudioUnitBusArray_c), 0, s, type, 0);
    return *slot;
}
static id au_inputs(id s, SEL c) { (void)c; return au_bus_array(s, 1); }
static id au_outputs(id s, SEL c) { (void)c; return au_bus_array(s, 2); }
static id au_tree(id s, SEL c) { (void)c; return AU(s)->tree; }
static void au_set_tree(id s, SEL c, id t) { (void)c; setobj(&AU(s)->tree, t); }
static u32 au_max_frames(id s, SEL c) { (void)c; return AU(s)->max_frames; }
static void au_set_max_frames(id s, SEL c, u32 v) { (void)c; if (!AU(s)->allocated) AU(s)->max_frames = v; }
static BOOL au_allocated(id s, SEL c) { (void)c; return AU(s)->allocated; }
static BOOL au_bypass(id s, SEL c) { (void)c; return AU(s)->bypass; }
static void au_set_bypass(id s, SEL c, BOOL v) { (void)c; AU(s)->bypass = v; }
static id au_name(id s, SEL c) { (void)c; return AU(s)->name ? AU(s)->name : cx_str("AUAudioUnit"); }
static BOOL au_alloc_res(id s, SEL c, id *err) {
    (void)c;
    /* cada bus de saída habilitado precisa de formato (o padrão da classe base é o que veio no bus) */
    id outs = au_outputs(s, 0);
    for (u64 i = 0, n = ba_count(outs, 0); i < n; i++) {
        id b = ba_at(outs, 0, i);
        if (BUS(b)->enabled && !BUS(b)->format) { if (err) *err = error_with(-10868, "Bus de saída sem formato."); return NO; }
    }
    AU(s)->allocated = YES;
    if (err) *err = 0;
    return YES;
}
static void au_dealloc_res(id s, SEL c) { (void)c; AU(s)->allocated = NO; }
static void au_reset(id s, SEL c) { (void)s; (void)c; }
/* a classe base não processa: a subclasse sobrescreve internalRenderBlock */
static void *au_internal_render(id s, SEL c) { (void)s; (void)c; return 0; }
typedef OSStatus (*render_inv)(void *blk, u32 *flags, const void *ts, u32 frames, i64 bus, void *abl, void *events, void *pull);
/* bloco de verdade (global: _Block_copy devolve o mesmo), chamável por quem recebe o AURenderBlock */
typedef struct { void *isa; int fl, re; void *inv; void *desc; id unit; } render_blk;
extern void *_NSConcreteGlobalBlock[];
void *calloc(size_t n, size_t s);
void free(void *p);
static struct { unsigned long reserved, size; } g_render_desc = {0, sizeof(render_blk)};
static OSStatus render_entry(void *blk, u32 *flags, const void *ts, u32 frames, i64 bus, void *abl, void *pull);
static render_blk *malloc_render(void) {
    render_blk *r = calloc(1, sizeof *r);
    if (!r) return 0;
    r->isa = _NSConcreteGlobalBlock;
    r->fl = 1 << 28;   /* BLOCK_IS_GLOBAL */
    r->inv = (void *)render_entry;
    r->desc = &g_render_desc;
    return r;
}
static void free_render(void *r) { free(r); }
static OSStatus render_entry(void *blk, u32 *flags, const void *ts, u32 frames, i64 bus, void *abl, void *pull) {
    render_blk *r = blk;
    id s = r->unit;
    if (!AU(s)->allocated) return -10867;   /* kAudioUnitErr_Uninitialized */
    if (frames > AU(s)->max_frames) return -10874;   /* kAudioUnitErr_TooManyFramesToProcess */
    void *ib = MSG0(void *, s, "internalRenderBlock");
    if (!ib) return -10863;   /* kAudioUnitErr_CannotDoInCurrentContext: a classe base não tem render */
    return ((render_inv)BLK_FN(ib))(ib, flags, ts, frames, bus, abl, 0, pull);
}
static void *au_render_block(id s, SEL c) {
    (void)c;
    if (!AU(s)->render_block) {
        render_blk *r = malloc_render();
        if (!r) return 0;
        r->unit = s;
        AU(s)->render_block = r;
    }
    return AU(s)->render_block;
}
static void au_dealloc(id s, SEL c) {
    setobj(&AU(s)->inputs, 0); setobj(&AU(s)->outputs, 0); setobj(&AU(s)->tree, 0); setobj(&AU(s)->name, 0);
    free_render(AU(s)->render_block);
    super_dealloc(s, c);
}

void cx_auv3_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
#define DEF(k, n, sup, sz) if (!cx_class_named(n)) cx_define_class(&k##_c, &k##_m, n, sup, (u32)(sz))
    DEF(AUAudioUnit, "AUAudioUnit", ob, sizeof(auv3_t) - 8);
    DEF(AUAudioUnitBus, "AUAudioUnitBus", ob, sizeof(bus_t) - 8);
    DEF(AUAudioUnitBusArray, "AUAudioUnitBusArray", ob, sizeof(busarr_t) - 8);
    DEF(AUParameterNode, "AUParameterNode", ob, sizeof(param_t) - 8);
    DEF(AUParameter, "AUParameter", &AUParameterNode_c, 0);
    DEF(AUParameterGroup, "AUParameterGroup", &AUParameterNode_c, 0);
    DEF(AUParameterTree, "AUParameterTree", &AUParameterGroup_c, 0);
#undef DEF
#define M(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define CM(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))
    M(AUAudioUnitBus, "initWithFormat:error:", bus_init); M(AUAudioUnitBus, "format", bus_format); M(AUAudioUnitBus, "setFormat:error:", bus_set_format);
    M(AUAudioUnitBus, "isEnabled", bus_enabled); M(AUAudioUnitBus, "setEnabled:", bus_set_enabled); M(AUAudioUnitBus, "name", bus_name);
    M(AUAudioUnitBus, "setName:", bus_set_name); M(AUAudioUnitBus, "index", bus_index); M(AUAudioUnitBus, "busType", bus_type);
    M(AUAudioUnitBus, "ownerAudioUnit", bus_owner); M(AUAudioUnitBus, "maximumChannelCount", bus_maxch); M(AUAudioUnitBus, "setMaximumChannelCount:", bus_set_maxch);
    M(AUAudioUnitBus, "dealloc", bus_dealloc);
    M(AUAudioUnitBusArray, "initWithAudioUnit:busType:busses:", ba_init); M(AUAudioUnitBusArray, "initWithAudioUnit:busType:", ba_init_empty);
    M(AUAudioUnitBusArray, "count", ba_count); M(AUAudioUnitBusArray, "objectAtIndexedSubscript:", ba_at); M(AUAudioUnitBusArray, "objectAtIndex:", ba_at);
    M(AUAudioUnitBusArray, "isCountChangeable", ba_countable); M(AUAudioUnitBusArray, "setBusCount:error:", ba_set_count);
    M(AUAudioUnitBusArray, "replaceBusses:", ba_replace); M(AUAudioUnitBusArray, "ownerAudioUnit", ba_owner); M(AUAudioUnitBusArray, "busType", ba_type);
    M(AUAudioUnitBusArray, "dealloc", ba_dealloc);
    CM(AUParameterTree, "createParameterWithIdentifier:name:address:min:max:unit:unitName:flags:valueStrings:dependentParameters:", prm_create);
    CM(AUParameterTree, "createGroupWithIdentifier:name:children:", grp_create);
    CM(AUParameterTree, "createTreeWithChildren:", tree_create);
    M(AUParameterTree, "parameterWithAddress:", tree_by_addr); M(AUParameterTree, "valueForKeyPath:", tree_by_path);
    M(AUParameterNode, "identifier", prm_ident); M(AUParameterNode, "displayName", prm_name);
    M(AUParameterNode, "implementorValueObserver", prm_observer); M(AUParameterNode, "setImplementorValueObserver:", prm_set_observer);
    M(AUParameterNode, "implementorValueProvider", prm_provider); M(AUParameterNode, "setImplementorValueProvider:", prm_set_provider);
    M(AUParameterNode, "setImplementorStringFromValueCallback:", prm_set_str); M(AUParameterNode, "dealloc", prm_dealloc);
    M(AUParameter, "value", prm_value); M(AUParameter, "setValue:", prm_set_value); M(AUParameter, "setValue:originator:", prm_set_value_orig);
    M(AUParameter, "address", prm_address); M(AUParameter, "minValue", prm_min); M(AUParameter, "maxValue", prm_max);
    M(AUParameter, "unit", prm_unit); M(AUParameter, "flags", prm_flags); M(AUParameter, "stringFromValue:", prm_string);
    M(AUParameterGroup, "children", prm_children); M(AUParameterGroup, "allParameters", prm_all);
    CM(AUAudioUnit, "instantiateWithComponentDescription:options:completionHandler:", au_instantiate);
    M(AUAudioUnit, "initWithComponentDescription:options:error:", au_init); M(AUAudioUnit, "initWithComponentDescription:error:", au_init_noopts);
    M(AUAudioUnit, "componentDescription", au_desc); M(AUAudioUnit, "inputBusses", au_inputs); M(AUAudioUnit, "outputBusses", au_outputs);
    M(AUAudioUnit, "parameterTree", au_tree); M(AUAudioUnit, "setParameterTree:", au_set_tree);
    M(AUAudioUnit, "maximumFramesToRender", au_max_frames); M(AUAudioUnit, "setMaximumFramesToRender:", au_set_max_frames);
    M(AUAudioUnit, "renderResourcesAllocated", au_allocated); M(AUAudioUnit, "allocateRenderResourcesAndReturnError:", au_alloc_res);
    M(AUAudioUnit, "deallocateRenderResources", au_dealloc_res); M(AUAudioUnit, "reset", au_reset);
    M(AUAudioUnit, "shouldBypassEffect", au_bypass); M(AUAudioUnit, "setShouldBypassEffect:", au_set_bypass);
    M(AUAudioUnit, "internalRenderBlock", au_internal_render); M(AUAudioUnit, "renderBlock", au_render_block);
    M(AUAudioUnit, "audioUnitName", au_name); M(AUAudioUnit, "dealloc", au_dealloc);
#undef M
#undef CM
}
