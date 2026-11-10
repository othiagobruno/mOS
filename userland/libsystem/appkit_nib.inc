/* ================================================================ .nib e .storyboard
 * Carrega os .nib compilados (NIBArchive) como o Cocoa: cada objeto é criado com alloc + initWithCoder:
 * (um NSCoder por chaves sobre o arquivo), então subclasses do app que sobrescrevem initWithCoder:/
 * awakeFromNib funcionam. Depois vêm as conexões (outlets e ações), awakeFromNib e as janelas
 * "visíveis ao abrir". Storyboards (.storyboardc) são um Info.plist + um nib por cena.
 * Incluído no fim do appkit.c (usa o estado interno das views). */
#include "nibarchive.h"
extern u8 *cx_read_file(const char *path, u64 *len);
extern id cx_plist_file(const char *path);
void *class_getInstanceVariable(cx_class_t *c, const char *name);
void object_setIvar(id o, void *iv, id v);
const char *object_getClassName(id o);

CLASS(NSCoder) CLASS(_CXNibCoder) CLASS(NSNib) CLASS(NSViewController) CLASS(NSWindowController) CLASS(NSStoryboard) CLASS(_CXSceneOwner)

typedef struct {
    nib_t n;
    u8 *buf;
    id *memo;
    u8 *busy;
    u8 *stub;               /* classe ausente: objeto genérico, conexões ignoradas */
    id owner, ext, constraints, made, main_menu;
    unsigned root;
    char dir[600];          /* pasta do nib (para os nibs de view de um storyboard) */
} nibctx_t;
typedef struct { nibctx_t *ctx; unsigned idx; } coderst_t;
static id fr_placeholder;            /* "First Responder": ações com alvo nil */
static nibctx_t *cur_ctx;             /* nib em carga (para controllers decodificados dentro dele) */
static void *nib_alloc(unsigned long s) { return calloc(1, s); }
static coderst_t *cst(id c) { return (coderst_t *)ST(c); }
static int sfx(const char *s, const char *suf) { size_t a = strlen(s), b = strlen(suf); return a >= b && !strcmp(s + a - b, suf); }

static id nib_decode(nibctx_t *x, unsigned idx);
static const nib_val_t *nv(nibctx_t *x, unsigned idx, const char *k) { return nib_get(&x->n, idx, k); }
static BOOL nis(nibctx_t *x, unsigned idx, const char *cls) { return !strcmp(nib_class(&x->n, idx), cls); }
static id nib_obj(nibctx_t *x, unsigned idx, const char *k) { const nib_val_t *v = nv(x, idx, k); return v && v->type == NV_OBJ ? nib_decode(x, v->obj) : 0; }
static int nib_ref(nibctx_t *x, unsigned idx, const char *k) { const nib_val_t *v = nv(x, idx, k); return v && v->type == NV_OBJ ? (int)v->obj : -1; }
static double nib_n(nibctx_t *x, unsigned idx, const char *k, double dflt) { const nib_val_t *v = nv(x, idx, k); return v && v->type != NV_OBJ && v->type != NV_DATA && v->type != NV_NIL ? nib_num(v) : dflt; }
static const char *nib_cstr(nibctx_t *x, unsigned idx, const char *k) { id s = nib_obj(x, idx, k); return s && cx_is_str(s) ? cx_cstr(s) : 0; }
/* "{{x, y}, {w, h}}" / "{w, h}" → números */
static int parse_nums(const char *s, double *out, int max) {
    int n = 0;
    while (s && *s && n < max) {
        if ((*s >= '0' && *s <= '9') || *s == '-' || *s == '.') { char *e; out[n++] = strtod(s, &e); s = e; }
        else s++;
    }
    return n;
}
static int nib_items(nibctx_t *x, unsigned idx, unsigned *out, int max) {
    const nib_obj_t *o = &x->n.obj[idx];
    int k = 0;
    for (unsigned i = 0; i < o->count && k < max; i++) {
        const nib_val_t *v = &x->n.val[o->first + i];
        if (v->type == NV_OBJ && !strcmp(x->n.key[v->key], "UINibEncoderEmptyKey")) out[k++] = v->obj;
    }
    return k;
}

/* ---- NSCoder sobre o nib */
static id coder_new(nibctx_t *x, unsigned idx) {
    id c = cx_alloc_instance(&_CXNibCoder_c);
    coderst_t *s = calloc(1, sizeof *s);
    s->ctx = x; s->idx = idx;
    ST(c) = s;
    return c;
}
static id cd_obj(id self, SEL c, id key) { (void)c; coderst_t *s = cst(self); return nib_obj(s->ctx, s->idx, cx_cstr(key)); }
static BOOL cd_bool(id self, SEL c, id key) { (void)c; coderst_t *s = cst(self); return nib_n(s->ctx, s->idx, cx_cstr(key), 0) != 0; }
static i64 cd_int(id self, SEL c, id key) { (void)c; coderst_t *s = cst(self); return (i64)nib_n(s->ctx, s->idx, cx_cstr(key), 0); }
static double cd_double(id self, SEL c, id key) { (void)c; coderst_t *s = cst(self); return nib_n(s->ctx, s->idx, cx_cstr(key), 0); }
static float cd_float(id self, SEL c, id key) { return (float)cd_double(self, c, key); }
static BOOL cd_contains(id self, SEL c, id key) { (void)c; coderst_t *s = cst(self); return nv(s->ctx, s->idx, cx_cstr(key)) != 0; }
static BOOL cd_keyed(id self, SEL c) { (void)self; (void)c; return YES; }
static NSSize cd_size(id self, SEL c, id key) { double v[2] = {0, 0}; coderst_t *s = cst(self); (void)c; parse_nums(nib_cstr(s->ctx, s->idx, cx_cstr(key)), v, 2); NSSize z = {v[0], v[1]}; return z; }
static NSPoint cd_point(id self, SEL c, id key) { double v[2] = {0, 0}; coderst_t *s = cst(self); (void)c; parse_nums(nib_cstr(s->ctx, s->idx, cx_cstr(key)), v, 2); NSPoint p = {v[0], v[1]}; return p; }
static NSRect cd_rect(id self, SEL c, id key) { double v[4] = {0, 0, 0, 0}; coderst_t *s = cst(self); (void)c; parse_nums(nib_cstr(s->ctx, s->idx, cx_cstr(key)), v, 4); return r_make(v[0], v[1], v[2], v[3]); }
static id o_init_coder(id self, SEL c, id coder) { (void)c; (void)coder; return MSG0(id, self, "init"); }

/* ---- valores simples */
static id nib_color(nibctx_t *x, unsigned idx) {
    int sp = (int)nib_n(x, idx, "NSColorSpace", 0);
    const nib_val_t *d = 0;
    double v[4] = {0, 0, 0, 1};
    if (sp == 1 || sp == 2) { d = nv(x, idx, "NSRGB"); if (d && d->type == NV_DATA) { char t[64]; snprintf(t, sizeof t, "%.*s", (int)(d->len < 60 ? d->len : 60), d->data); int k = parse_nums(t, v, 4); if (k < 4) v[3] = 1; } return cx_color(v[0], v[1], v[2], v[3]); }
    if (sp == 3 || sp == 4) { d = nv(x, idx, "NSWhite"); if (d && d->type == NV_DATA) { char t[64]; snprintf(t, sizeof t, "%.*s", (int)(d->len < 60 ? d->len : 60), d->data); int k = parse_nums(t, v, 2); if (k < 2) v[1] = 1; } return cx_color(v[0], v[0], v[0], v[1]); }
    if (sp == 6) {   /* cor de catálogo: usa a cor de sistema de mesmo nome se existir */
        const char *name = nib_cstr(x, idx, "NSColorName");
        if (name && cx_responds(NSColor_c.isa, sel_registerName(name))) return MSG0(id, &NSColor_c, name);
        int k = nib_ref(x, idx, "NSColor");
        if (k >= 0) return nib_color(x, (unsigned)k);
    }
    return MSG0(id, &NSColor_c, "labelColor");
}
static id nib_font(nibctx_t *x, unsigned idx) {
    const char *name = nib_cstr(x, idx, "NSName");
    double size = nib_n(x, idx, "NSSize", 13);
    if (name && strstr(name, "Bold")) return MSG(id, &NSFont_c, "boldSystemFontOfSize:", size);
    if (name && (strstr(name, "Mono") || strstr(name, "Menlo") || strstr(name, "Courier"))) return MSG(id, &NSFont_c, "monospacedSystemFontOfSize:weight:", size, 0.0);
    return MSG(id, &NSFont_c, "systemFontOfSize:", size);
}
static id nib_image(nibctx_t *x, unsigned idx) {
    const char *name = nib_cstr(x, idx, "NSResourceName");
    if (!name) name = nib_cstr(x, idx, "NSImageName");
    if (!name) name = nib_cstr(x, idx, "NSName");
    return name ? MSG(id, &NSImage_c, "imageNamed:", cx_str(name)) : 0;
}

/* ---- views: um initWithCoder: para toda a hierarquia do AppKit */
static void nib_text_cell(nibctx_t *x, id self, unsigned cell) {
    u32 fl = (u32)nib_n(x, cell, "NSCellFlags", 0);
    id txt = nib_obj(x, cell, "NSContents");
    MSG(void, self, "setEditable:", (int)!!(fl & 0x10000000));
    MSG(void, self, "setBezeled:", (int)!!(fl & 0x00C00000));
    if (txt && cx_is_str(txt)) MSG(void, self, "setStringValue:", txt);
    id ph = nib_obj(x, cell, "NSPlaceholderString");
    if (ph && cx_is_str(ph)) MSG(void, self, "setPlaceholderString:", ph);
    int bg = nib_ref(x, cell, "NSBackgroundColor");
    BOOL dbg = nib_n(x, cell, "NSDrawsBackground", 0) != 0;
    MSG(void, self, "setDrawsBackground:", (int)dbg);
    if (dbg && bg >= 0) MSG(void, self, "setBackgroundColor:", nib_color(x, (unsigned)bg));
    int tc = nib_ref(x, cell, "NSTextColor");
    if (tc >= 0) { const char *cn = nib_cstr(x, (unsigned)tc, "NSColorName"); if (!cn || (strcmp(cn, "controlTextColor") && strcmp(cn, "labelColor") && strcmp(cn, "textColor"))) MSG(void, self, "setTextColor:", nib_color(x, (unsigned)tc)); }
}
static id v_init_coder(id self, SEL c, id coder) {
    nibctx_t *x = cst(coder)->ctx;
    unsigned idx = cst(coder)->idx;
    double f[4] = {0, 0, 0, 0};
    const char *fs = nib_cstr(x, idx, "NSFrame");
    if (fs) parse_nums(fs, f, 4);
    else { double z[2] = {0, 0}; parse_nums(nib_cstr(x, idx, "NSFrameSize"), z, 2); f[2] = z[0]; f[3] = z[1]; }
    self = MSG(id, self, "initWithFrame:", r_make(f[0], f[1], f[2], f[3]));
    (void)c;
    if (!self) return 0;
    x->memo[idx] = self;
    vstate_t *s = vs(self);
    id vid = nib_obj(x, idx, "NSUserInterfaceItemIdentifier");
    if (!vid) vid = nib_obj(x, idx, "NSViewIdentifier");
    if (vid && cx_is_str(vid)) MSG(void, self, "setIdentifier:", vid);
    u32 vf = (u32)nib_n(x, idx, "NSvFlags", 0);
    s->autoresize = vf & 0x3F;
    if (vf & 0x80000000u) s->hidden = YES;
    if (nib_n(x, idx, "NSDoNotTranslateAutoresizingMask", 0)) s->no_tamic = YES;
    if (nib_n(x, idx, "NSTag", 0)) s->tag = (int)nib_n(x, idx, "NSTag", 0);
    int cell = nib_ref(x, idx, "NSCell");
    if (nv(x, idx, "NSEnabled") && !nib_n(x, idx, "NSEnabled", 1)) s->enabled = NO;
    switch (s->kind) {
    case K_BUTTON:
        if (cell >= 0) {
            unsigned ce = (unsigned)cell;
            id t = nib_obj(x, ce, "NSContents");
            if (t && cx_is_str(t)) MSG(void, self, "setTitle:", t);
            id k = nib_obj(x, ce, "NSKeyEquivalent");
            if (k && cx_is_str(k) && strlen(cx_cstr(k))) MSG(void, self, "setKeyEquivalent:", k);
            int alt = nib_ref(x, ce, "NSAlternateImage");
            const char *an = alt >= 0 ? nib_cstr(x, (unsigned)alt, "NSImageName") : 0;
            if (an && strstr(an, "Switch")) MSG(void, self, "setButtonType:", 3);
            else if (an && strstr(an, "Radio")) MSG(void, self, "setButtonType:", 4);
            if (nib_n(x, ce, "NSState", 0)) MSG(void, self, "setState:", 1);
            int img = nib_ref(x, ce, "NSNormalImage");
            if (img >= 0) { id im = nib_image(x, (unsigned)img); if (im) MSG(void, self, "setImage:", im); }
            int fo = nib_ref(x, ce, "NSSupport");
            if (fo >= 0 && nis(x, (unsigned)fo, "NSFont")) MSG(void, self, "setFont:", nib_font(x, (unsigned)fo));
        }
        break;
    case K_TEXT:
        if (cell >= 0) {
            nib_text_cell(x, self, (unsigned)cell);
            int fo = nib_ref(x, (unsigned)cell, "NSSupport");
            if (fo >= 0 && nis(x, (unsigned)fo, "NSFont")) MSG(void, self, "setFont:", nib_font(x, (unsigned)fo));
        }
        { int al = (int)nib_n(x, idx, "NSControlTextAlignment", 0); MSG(void, self, "setAlignment:", al == 4 ? 0 : al == 1 ? 2 : al == 2 ? 1 : al); }
        break;
    case K_IMAGE:
        if (cell >= 0) { int im = nib_ref(x, (unsigned)cell, "NSContents"); if (im >= 0) { id i = nib_image(x, (unsigned)im); if (i) MSG(void, self, "setImage:", i); } }
        break;
    case K_SLIDER:
        if (cell >= 0) {
            MSG(void, self, "setMinValue:", nib_n(x, (unsigned)cell, "NSMinValue", 0));
            MSG(void, self, "setMaxValue:", nib_n(x, (unsigned)cell, "NSMaxValue", 1));
            MSG(void, self, "setDoubleValue:", nib_n(x, (unsigned)cell, "NSValue", 0));
        }
        break;
    case K_PROGRESS:
        MSG(void, self, "setMinValue:", nib_n(x, idx, "NSMinValue", 0));
        MSG(void, self, "setMaxValue:", nib_n(x, idx, "NSMaxValue", 100));
        break;
    case K_BOX: {
        if (nib_n(x, idx, "NSTransparent", 0)) { s->kind = K_VIEW; break; }     /* caixa invisível: só agrupa */
        if (nv(x, idx, "NSTitlePosition") && !nib_n(x, idx, "NSTitlePosition", 0)) break;   /* sem título */
        int tc = nib_ref(x, idx, "NSTitleCell");
        id t = tc >= 0 ? nib_obj(x, (unsigned)tc, "NSContents") : 0;
        if (t && cx_is_str(t)) MSG(void, self, "setTitle:", t);
        break;
    }
    case K_STACK:
        s->kind = K_VIEW;           /* o nib já traz os frames resolvidos pelo Interface Builder */
        break;
    case K_TABLE: {
        unsigned cols[64];
        int ci = nib_ref(x, idx, "NSTableColumns");
        int nc = ci >= 0 ? nib_items(x, (unsigned)ci, cols, 64) : 0;
        for (int i = 0; i < nc; i++) {
            id col = MSG(id, cx_alloc_instance(&NSTableColumn_c), "initWithIdentifier:", nib_obj(x, cols[i], "NSIdentifier"));
            int hc = nib_ref(x, cols[i], "NSHeaderCell");
            id t = hc >= 0 ? nib_obj(x, (unsigned)hc, "NSContents") : 0;
            if (t && cx_is_str(t)) MSG(void, col, "setTitle:", t);
            MSG(void, col, "setWidth:", nib_n(x, cols[i], "NSWidth", 120));
            x->memo[cols[i]] = col;
            MSG(void, self, "addTableColumn:", col);
        }
        if (nv(x, idx, "NSRowHeight")) MSG(void, self, "setRowHeight:", nib_n(x, idx, "NSRowHeight", 24) + 4);
        if (nv(x, idx, "NSHeaderView")) {
            id hv = nib_obj(x, idx, "NSHeaderView");
            if (hv) MSG(void, self, "setHeaderView:", hv);
        } else s->header = NO;
        id arch = nib_obj(x, idx, "NSTableViewArchivedReusableViewsKey");
        if (!arch) arch = nib_obj(x, idx, "NSTableViewReusableViewsKey");
        if (arch) tv_install_archived(self, arch);
        break;
    }
    }
    if (s->kind == K_SCROLL) {
        int clip = nib_ref(x, idx, "NSContentView");
        int doc = clip >= 0 ? nib_ref(x, (unsigned)clip, "NSDocView") : -1;
        if (doc >= 0) { id d = nib_decode(x, (unsigned)doc); if (is_view(d)) MSG(void, self, "setDocumentView:", d); }
    } else {
        int sv = nib_ref(x, idx, "NSSubviews");
        unsigned subs[256];
        int ns = sv >= 0 ? nib_items(x, (unsigned)sv, subs, 256) : 0;
        for (int i = 0; i < ns; i++) {
            if (nis(x, subs[i], "NSScroller") || nis(x, subs[i], "NSTableHeaderView") || nis(x, subs[i], "_NSCornerView")) continue;
            id v = nib_decode(x, subs[i]);
            /* um NSClipView decodificado à parte (por um outlet) não tira do NSScrollView a view de documento */
            if (is_view(v) && vs(v)->superview != self && !(nis(x, idx, "NSClipView") && vs(v)->superview)) v_add_sub(self, 0, v);
        }
    }
    static const char *gk[] = {"NSViewLayoutGuides", "IBNSSafeAreaLayoutGuide", "IBNSLayoutMarginsGuide"};
    for (int gi = 0; gi < 3; gi++) {
        int gr = nib_ref(x, idx, gk[gi]);
        if (gr < 0) continue;
        unsigned gl[32];
        int ng = nis(x, (unsigned)gr, "NSLayoutGuide") ? (gl[0] = (unsigned)gr, 1) : nib_items(x, (unsigned)gr, gl, 32);
        for (int j = 0; j < ng; j++) { id g = nib_decode(x, gl[j]); if (is_guide(g)) v_add_guide(self, 0, g); }
    }
    int cs = nib_ref(x, idx, "NSViewConstraints");
    unsigned cons[256];
    int ncs = cs >= 0 ? nib_items(x, (unsigned)cs, cons, 256) : 0;
    for (int i = 0; i < ncs; i++) { id k = nib_decode(x, cons[i]); if (k) cx_arr_add(x->constraints, k); }
    return self;
}
static id lc_init_coder(id self, SEL c, id coder) {
    (void)c;
    nibctx_t *x = cst(coder)->ctx;
    unsigned idx = cst(coder)->idx;
    lc_t *s = lc(self);
    id a = nib_obj(x, idx, "NSFirstItem"), b = nib_obj(x, idx, "NSSecondItem");
    s->a = objc_retain(a);
    s->b = objc_retain(b);
    s->aa = (int)nib_n(x, idx, "NSFirstAttribute", 0);
    s->ba = b ? (int)nib_n(x, idx, "NSSecondAttribute", 0) : LA_NONE;
    s->rel = (int)nib_n(x, idx, "NSRelation", 0);
    s->mult = nib_n(x, idx, "NSMultiplier", 1);
    s->k = nib_n(x, idx, "NSConstant", 0);
    s->prio = (float)nib_n(x, idx, "NSPriority", 1000);
    return is_view(a) ? self : 0;      /* is_view inclui NSLayoutGuide */
}

/* ---- janelas, menus e objetos especiais */
static id nib_window(nibctx_t *x, unsigned idx) {
    double r[4] = {0, 0, 480, 360};
    parse_nums(nib_cstr(x, idx, "NSWindowRect"), r, 4);
    const char *cn = nib_cstr(x, idx, "NSWindowClass");
    cx_class_t *k = cn ? cx_class_named(cn) : 0;
    if (!k || !cx_is_subclass(k, &NSWindow_c)) k = &NSWindow_c;
    id w = MSG0(id, k, "alloc");
    x->memo[idx] = w;
    w = MSG(id, w, "initWithContentRect:styleMask:backing:defer:", r_make(r[0], r[1], r[2], r[3]), (u64)nib_n(x, idx, "NSWindowStyleMask", 15), (u64)2, 1);
    x->memo[idx] = w;
    const char *t = nib_cstr(x, idx, "NSWindowTitle");
    if (t) MSG(void, w, "setTitle:", cx_str(t));
    id cv = nib_obj(x, idx, "NSWindowView");
    if (is_view(cv)) MSG(void, w, "setContentView:", cv);
    return w;
}
static id nib_menu(nibctx_t *x, unsigned idx) {
    const char *t = nib_cstr(x, idx, "NSTitle");
    id m = MSG(id, cx_alloc_instance(&NSMenu_c), "initWithTitle:", cx_str(t ? t : ""));
    x->memo[idx] = m;
    unsigned items[256];
    int ii = nib_ref(x, idx, "NSMenuItems");
    int n = ii >= 0 ? nib_items(x, (unsigned)ii, items, 256) : 0;
    for (int i = 0; i < n; i++) { id it = nib_decode(x, items[i]); if (it) MSG(void, m, "addItem:", it); }
    const char *name = nib_cstr(x, idx, "NSName");
    if (name && !strcmp(name, "_NSMainMenu")) x->main_menu = m;
    return m;
}
static id nib_menu_item(nibctx_t *x, unsigned idx) {
    id it;
    const char *tt = nib_cstr(x, idx, "NSTitle");
    if (nib_n(x, idx, "NSIsSeparator", 0) || ((!tt || !*tt) && !nv(x, idx, "NSSubmenu"))) it = objc_retain(MSG0(id, &NSMenuItem_c, "separatorItem"));
    else {
        const char *t = nib_cstr(x, idx, "NSTitle"), *k = nib_cstr(x, idx, "NSKeyEquiv");
        it = MSG(id, cx_alloc_instance(&NSMenuItem_c), "initWithTitle:action:keyEquivalent:", cx_str(t ? t : ""), (SEL)0, cx_str(k ? k : ""));
        if (nv(x, idx, "NSKeyEquivModMask")) MSG(void, it, "setKeyEquivalentModifierMask:", (u64)nib_n(x, idx, "NSKeyEquivModMask", 0));
        if (nib_n(x, idx, "NSIsDisabled", 0)) MSG(void, it, "setEnabled:", 0);
        if (nv(x, idx, "NSTag")) MSG(void, it, "setTag:", (i64)nib_n(x, idx, "NSTag", 0));
        if (nib_n(x, idx, "NSState", 0)) MSG(void, it, "setState:", (i64)1);
    }
    x->memo[idx] = it;
    id sub = nib_obj(x, idx, "NSSubmenu");
    if (sub) MSG(void, it, "setSubmenu:", sub);
    return it;
}
static cx_class_t *nib_class_for(nibctx_t *x, unsigned idx, const char **orig) {
    x->stub[idx] = 0;
    const char *cn = nib_class(&x->n, idx);
    *orig = cn;
    if (!strcmp(cn, "NSClassSwapper") || !strcmp(cn, "NSCustomView") || !strcmp(cn, "NSCustomObject")) {
        const char *want = nib_cstr(x, idx, "NSClassName");
        const char *base = nib_cstr(x, idx, "NSOriginalClassName");
        if (!strcmp(cn, "NSCustomView")) base = "NSView";
        if (base) *orig = base;
        cx_class_t *k = want ? cx_class_named(want) : 0;
        if (!k && want && !(want[0] == '_' && want[1] == 'T' && want[2] == 't'))
            cx_fdprintf(2, "AppKit[mOS]: classe %s do nib não existe; usando %s\n", want, base ? base : "NSObject");
        if (!k) x->stub[idx] = 1;
        if (!k && base) { if (!strncmp(base, "IB", 2)) base += 2; k = cx_class_named(base); x->stub[idx] = !strcmp(base, "NSObject"); }
        return k ? k : &NSObject_c;
    }
    cx_class_t *k = cx_class_named(cn);
    if (!k && nv(x, idx, "NSFrame")) k = &NSView_c;      /* view de um tipo ainda não implementado */
    return k;
}
static id nib_decode(nibctx_t *x, unsigned idx) {
    if (idx >= x->n.nobj) return 0;
    if (x->memo[idx] || x->busy[idx]) return x->memo[idx];
    const char *cn = nib_class(&x->n, idx);
    id r = 0;
    x->busy[idx] = 1;
    if (!strcmp(cn, "NSString") || !strcmp(cn, "NSMutableString") || !strcmp(cn, "NSLocalizableString")) {
        const nib_val_t *b = nv(x, idx, "NS.bytes");
        r = b && b->type == NV_DATA ? cx_str_n((const char *)b->data, b->len) : cx_str("");
    } else if (!strcmp(cn, "NSArray") || !strcmp(cn, "NSMutableArray") || !strcmp(cn, "NSSet") || !strcmp(cn, "NSMutableSet") || !strcmp(cn, "NSOrderedSet")) {
        r = cx_arr_new(0);
        x->memo[idx] = r;
        unsigned it[512];
        int n = nib_items(x, idx, it, 512);
        for (int i = 0; i < n; i++) { id v = nib_decode(x, it[i]); if (v) cx_arr_add(r, v); }
    } else if (!strcmp(cn, "NSDictionary") || !strcmp(cn, "NSMutableDictionary")) {
        r = MSG0(id, MSG0(id, (id)cx_class_named("NSMutableDictionary"), "alloc"), "init");
        x->memo[idx] = r;
        unsigned it[512];
        int n = nib_items(x, idx, it, 512);
        for (int i = 0; i + 1 < n; i += 2) { id k = nib_decode(x, it[i]), v = nib_decode(x, it[i + 1]); if (k && v) MSG(void, r, "setObject:forKey:", v, k); }
    } else if (!strcmp(cn, "NSNumber")) {
        const nib_val_t *v = x->n.obj[idx].count ? &x->n.val[x->n.obj[idx].first] : 0;
        r = v && (v->type == NV_FLOAT || v->type == NV_DOUBLE) ? cx_num_d(v->d) : cx_num_i(v ? v->i : 0);
    } else if (!strcmp(cn, "NSColor")) r = nib_color(x, idx);
    else if (!strcmp(cn, "NSFont")) r = nib_font(x, idx);
    else if (!strcmp(cn, "NSCustomResource") || !strcmp(cn, "NSButtonImageSource") || !strcmp(cn, "NSImage")) r = nib_image(x, idx);
    else if (!strcmp(cn, "NSWindowTemplate")) r = nib_window(x, idx);
    else if (!strcmp(cn, "NSMenu")) r = nib_menu(x, idx);
    else if (!strcmp(cn, "NSMenuItem")) r = nib_menu_item(x, idx);
    else if (!strcmp(cn, "NSNibExternalObjectPlaceholder")) {
        const char *pid = nib_cstr(x, idx, "NSExternalObjectPlaceholderIdentifier");
        r = pid && x->ext ? MSG(id, x->ext, "objectForKey:", cx_str(pid)) : 0;
    } else if (!strcmp(cn, "NSCustomObject") && nib_cstr(x, idx, "NSClassName") && !strcmp(nib_cstr(x, idx, "NSClassName"), "FirstResponder")) r = fr_placeholder;
    else if (!strcmp(cn, "NSCustomObject") && nib_cstr(x, idx, "NSClassName") && !strcmp(nib_cstr(x, idx, "NSClassName"), "NSApplication")) r = NSApp;
    else {
        const char *orig;
        cx_class_t *k = nib_class_for(x, idx, &orig);
        if (k) {
            id o = MSG0(id, k, "alloc");
            x->memo[idx] = o;
            id cd = coder_new(x, idx);
            nibctx_t *save = cur_ctx;
            cur_ctx = x;
            if (cx_responds(k, sel_registerName("initWithCoder:"))) r = MSG(id, o, "initWithCoder:", cd);
            else r = MSG0(id, o, "init");
            cur_ctx = save;
            objc_release(cd);
            if (r && r != fr_placeholder && r != NSApp && !is_view(r)) cx_arr_add(x->made, r);
            else if (is_view(r)) cx_arr_add(x->made, r);
        }
    }
    x->memo[idx] = r;
    x->busy[idx] = 0;
    return r;
}

/* ---- conexões */
static void set_outlet(id src, const char *label, id dst) {
    if (!src || !label || !*label) return;
    char sel[300];
    snprintf(sel, sizeof sel, "set%c%s:", toupper((unsigned char)label[0]), label + 1);
    SEL s = sel_registerName(sel);
    if (cx_responds(src->isa, s)) { ((void (*)(id, SEL, id))objc_msgSend)(src, s, dst); return; }
    void *iv = class_getInstanceVariable(src->isa, label);
    if (!iv) { snprintf(sel, sizeof sel, "_%s", label); iv = class_getInstanceVariable(src->isa, sel); }
    if (iv) { object_setIvar(src, iv, objc_retain(dst)); return; }
    cx_fdprintf(2, "AppKit[mOS]: outlet '%s' sem destino em %s\n", label, object_getClassName(src));
}
static void nib_connect(nibctx_t *x, unsigned conn) {
    const char *cn = nib_class(&x->n, conn);
    const char *label = nib_cstr(x, conn, "NSLabel");
    int si = nib_ref(x, conn, "NSSource"), di = nib_ref(x, conn, "NSDestination");
    if ((si >= 0 && x->stub[si]) || (di >= 0 && x->stub[di])) return;   /* objeto de classe ausente */
    id src = nib_obj(x, conn, "NSSource"), dst = nib_obj(x, conn, "NSDestination");
    if (!strcmp(cn, "NSNibOutletConnector")) set_outlet(src, label, dst);
    else if (!strcmp(cn, "NSNibBindingConnector")) {
        const char *bn = nib_cstr(x, conn, "NSBinding");
        id kp = nib_obj(x, conn, "NSKeyPath");
        if (bn && kp && src && dst) cx_bind(src, bn, dst, kp);
    }
    else if (!strcmp(cn, "NSNibControlConnector") && src && label) {
        id target = dst == fr_placeholder ? 0 : dst;
        SEL a = sel_registerName(label);
        if (cx_is_subclass(src->isa, &NSMenuItem_c)) { MSG(void, src, "setAction:", a); if (target) MSG(void, src, "setTarget:", target); }
        else if (cx_responds(src->isa, sel_registerName("setAction:"))) { MSG(void, src, "setAction:", a); MSG(void, src, "setTarget:", target); }
    }
}

/* carrega um nib já lido; devolve os objetos de nível superior */
static id nib_load_buf(u8 *buf, u64 len, id owner, id ext, const char *dir, id *top) {
    if (!fr_placeholder) fr_placeholder = cx_alloc_instance(&NSObject_c);
    nibctx_t *x = calloc(1, sizeof *x);
    if (!nib_parse(&x->n, buf, len, nib_alloc)) { cx_puts_fd(2, "AppKit[mOS]: .nib inválido (só o formato NIBArchive é suportado)\n"); return 0; }
    x->buf = buf;
    x->memo = calloc(x->n.nobj + 1, sizeof(id));
    x->busy = calloc(x->n.nobj + 1, 1);
    x->stub = calloc(x->n.nobj + 1, 1);
    x->owner = owner;
    x->ext = ext;
    x->constraints = cx_arr_new(0);
    x->made = cx_arr_new(0);
    snprintf(x->dir, sizeof x->dir, "%s", dir ? dir : "");
    int od = nib_ref(x, 0, "IB.objectdata");
    if (od < 0) return 0;
    int root = nib_ref(x, (unsigned)od, "NSRoot");
    if (root >= 0) { x->root = (unsigned)root; x->memo[root] = owner ? owner : NSApp; }
    /* todos os objetos listados (nível superior e filhos) */
    unsigned keys[2048], vals[2048];
    int ki = nib_ref(x, (unsigned)od, "NSObjectsKeys"), vi = nib_ref(x, (unsigned)od, "NSObjectsValues");
    int nk = ki >= 0 ? nib_items(x, (unsigned)ki, keys, 2048) : 0, nvl = vi >= 0 ? nib_items(x, (unsigned)vi, vals, 2048) : 0;
    for (int i = 0; i < nk; i++) nib_decode(x, keys[i]);
    /* conexões */
    unsigned conns[2048];
    int ci = nib_ref(x, (unsigned)od, "NSConnections");
    int nc = ci >= 0 ? nib_items(x, (unsigned)ci, conns, 2048) : 0;
    for (int i = 0; i < nc; i++) nib_connect(x, conns[i]);
    /* restrições do Auto Layout (só valem para views com translatesAutoresizingMaskIntoConstraints = NO) */
    lc_activate_all(0, 0, x->constraints);
    if (x->main_menu && (!owner || owner == NSApp)) MSG(void, NSApp, "setMainMenu:", x->main_menu);
    /* awakeFromNib: objetos criados e o dono */
    SEL awake = sel_registerName("awakeFromNib");
    nsarr_t *made = (nsarr_t *)x->made;
    for (u64 i = 0; i < made->n; i++) if (cx_responds(made->items[i]->isa, awake)) MSG0(void, made->items[i], "awakeFromNib");
    if (owner && owner != NSApp && cx_responds(owner->isa, awake)) MSG0(void, owner, "awakeFromNib");
    /* nível superior: cujo pai é o dono */
    id tops = cx_arr_new(0);
    for (int i = 0; i < nk && i < nvl; i++)
        if (vals[i] == x->root) { id o = x->memo[keys[i]]; if (o && o != fr_placeholder && o != NSApp && o != owner) cx_arr_add(tops, o); }
    if (top) *top = objc_autorelease(tops);
    /* janelas visíveis ao abrir */
    unsigned vis[64];
    int wi = nib_ref(x, (unsigned)od, "NSVisibleWindows");
    int nw = wi >= 0 ? nib_items(x, (unsigned)wi, vis, 64) : 0;
    for (int i = 0; i < nw; i++) { id w = x->memo[vis[i]]; if (w && cx_is_subclass(w->isa, &NSWindow_c)) MSG(void, w, "makeKeyAndOrderFront:", (id)0); }
    return tops;
}
/* caminho de um .nib (arquivo ou pasta com keyedobjects*.nib) */
static BOOL nib_file(const char *p, char *out, usize cap) {
    char t[1100];
    snprintf(t, sizeof t, "%s/keyedobjects.nib", p);
    if (access(t, 0) == 0) { snprintf(out, cap, "%s", t); return YES; }
    snprintf(t, sizeof t, "%s/keyedobjects-110000.nib", p);
    if (access(t, 0) == 0) { snprintf(out, cap, "%s", t); return YES; }
    snprintf(t, sizeof t, "%s/keyedobjects-101300.nib", p);
    if (access(t, 0) == 0) { snprintf(out, cap, "%s", t); return YES; }
    if (access(p, 0) == 0) { snprintf(out, cap, "%s", p); return YES; }
    return NO;
}
static BOOL load_nib_path(const char *path, id owner, id ext, id *top) {
    char f[1100];
    if (!nib_file(path, f, sizeof f)) return NO;
    u64 len;
    u8 *b = cx_read_file(f, &len);
    if (!b) return NO;
    char dir[600];
    snprintf(dir, sizeof dir, "%s", path);
    char *sl = strrchr(dir, '/');
    if (sl) *sl = 0;
    return nib_load_buf(b, len, owner, ext, dir, top) != 0;
}
static id nib_path_named(id name, const char *dir) {
    if (!name) return 0;
    if (dir && *dir) {
        char p[1100];
        snprintf(p, sizeof p, "%s/%s.nib", dir, cx_cstr(name));
        if (access(p, 0) == 0) return cx_str(p);
    }
    return MSG(id, MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle"), "pathForResource:ofType:", name, cx_str("nib"));
}
static BOOL load_nib_named(id name, id owner, id *top) {
    id p = nib_path_named(name, 0);
    if (!p) { cx_fdprintf(2, "AppKit[mOS]: nib '%s' não encontrado no bundle\n", name ? cx_cstr(name) : "?"); return NO; }
    return load_nib_path(cx_cstr(p), owner, 0, top);
}
static BOOL b_load_nib(id self, SEL c, id name, id owner, id *top) { (void)self; (void)c; return load_nib_named(name, owner, top); }
static BOOL b_load_nib_old(id cls, SEL c, id name, id owner) { (void)cls; (void)c; return load_nib_named(name, owner, 0); }
static BOOL b_load_nib_opts(id self, SEL c, id name, id owner, id opts) { (void)self; (void)c; (void)opts; return load_nib_named(name, owner, 0); }

/* ---- NSNib */
typedef struct { id path; } nibst_t;
static id nb_init_named(id self, SEL c, id name, id bundle) { (void)c; (void)bundle; nibst_t *s = calloc(1, sizeof *s); s->path = objc_retain(nib_path_named(name, 0)); ST(self) = s; if (!s->path) { objc_release(self); return 0; } return self; }
static BOOL nb_instantiate(id self, SEL c, id owner, id *top) { (void)c; nibst_t *s = ST(self); return s && load_nib_path(cx_cstr(s->path), owner, 0, top); }

/* ---- NSViewController */
typedef struct { id view, nib, title, rep, storyboard, ident; char dir[600]; BOOL loading; } vcst_t;
static vcst_t *vc(id o) { if (!ST(o)) ST(o) = calloc(1, sizeof(vcst_t)); return ST(o); }
static id vc_init_nib(id self, SEL c, id nib, id bundle) { (void)c; (void)bundle; keep(&vc(self)->nib, nib); return self; }
static id vc_init(id self, SEL c) { (void)c; vc(self); return self; }
static id vc_init_coder(id self, SEL c, id coder) {
    (void)c;
    nibctx_t *x = cst(coder)->ctx;
    unsigned idx = cst(coder)->idx;
    vcst_t *s = vc(self);
    const char *nn = nib_cstr(x, idx, "NSNibName");
    if (nn) keep(&s->nib, cx_str(nn));
    const char *id_ = nib_cstr(x, idx, "NSStoryboardIdentifier");
    if (id_) keep(&s->ident, cx_str(id_));
    snprintf(s->dir, sizeof s->dir, "%s", x->dir);
    return self;
}
static void vc_set_view(id self, SEL c, id v) { (void)c; keep(&vc(self)->view, v); }
static void vc_load_view(id self, SEL c) {
    (void)c;
    vcst_t *s = vc(self);
    id name = s->nib;
    if (!name) {   /* sem nib: tenta o nome da classe, como o Cocoa */
        const char *cn = object_getClassName(self);
        /* classe Swift (_TtC7Postico27FavoritesListViewController): o Cocoa usa o nome sem o módulo */
        if (cn && !strncmp(cn, "_TtC", 4)) {
            const char *p = cn + 4;
            while (*p >= '0' && *p <= '9') {
                long n = 0;
                while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
                if ((long)strlen(p) < n) break;
                if (!(p[n] >= '0' && p[n] <= '9')) { if (!p[n]) cn = p; break; }
                p += n;
            }
        }
        if (nib_path_named(cx_str(cn), 0)) name = cx_str(cn);
    }
    id p = name ? nib_path_named(name, s->dir) : 0;
    id ext = 0;
    if (s->storyboard) { ext = MSG0(id, MSG0(id, (id)cx_class_named("NSMutableDictionary"), "alloc"), "init"); MSG(void, ext, "setObject:forKey:", s->storyboard, cx_str("NSStoryboardPlaceholder")); }
    if (p) load_nib_path(cx_cstr(p), self, ext, 0);
    if (!s->view) keep(&s->view, objc_autorelease(MSG(id, cx_alloc_instance(&NSView_c), "initWithFrame:", r_make(0, 0, 480, 270))));
}
static id vc_view(id self, SEL c) {
    (void)c;
    vcst_t *s = vc(self);
    if (!s->view && !s->loading) {
        s->loading = YES;
        MSG0(void, self, "loadView");
        s->loading = NO;
        MSG0(void, self, "viewDidLoad");
    }
    return s->view;
}
static BOOL vc_loaded(id self, SEL c) { (void)c; return vc(self)->view != 0; }
static id vc_title(id self, SEL c) { (void)c; return vc(self)->title; }
static void vc_set_title(id self, SEL c, id t) { (void)c; keep(&vc(self)->title, t); }
static id vc_rep(id self, SEL c) { (void)c; return vc(self)->rep; }
static void vc_set_rep(id self, SEL c, id r) { (void)c; keep(&vc(self)->rep, r); }
static id vc_nib_name(id self, SEL c) { (void)c; return vc(self)->nib; }
static id vc_storyboard(id self, SEL c) { (void)c; return vc(self)->storyboard; }
static void vc_set_storyboard(id self, SEL c, id sb) { (void)c; vc(self)->storyboard = sb; }

/* ---- NSWindowController */
typedef struct { id window, nib, owner, cvc; BOOL loading; } wcst_t;
static wcst_t *wc(id o) { if (!ST(o)) ST(o) = calloc(1, sizeof(wcst_t)); return ST(o); }
static id wc_init_window(id self, SEL c, id w) { (void)c; keep(&wc(self)->window, w); return self; }
static id wc_init_nib(id self, SEL c, id nib) { (void)c; keep(&wc(self)->nib, nib); return self; }
static id wc_init_nib_owner(id self, SEL c, id nib, id owner) { (void)c; keep(&wc(self)->nib, nib); wc(self)->owner = owner; return self; }
static void wc_set_window(id self, SEL c, id w) { (void)c; keep(&wc(self)->window, w); }
static void wc_set_cvc(id self, SEL c, id v) {
    (void)c;
    wcst_t *s = wc(self);
    keep(&s->cvc, v);
    if (s->window && v) MSG(void, s->window, "setContentViewController:", v);
}
static id wc_cvc(id self, SEL c) { (void)c; return wc(self)->cvc; }
static id wc_window(id self, SEL c) {
    (void)c;
    wcst_t *s = wc(self);
    if (!s->window && s->nib && !s->loading) {
        s->loading = YES;
        if (cx_responds(self->isa, sel_registerName("windowWillLoad"))) MSG0(void, self, "windowWillLoad");
        load_nib_named(s->nib, s->owner ? s->owner : self, 0);
        s->loading = NO;
        if (s->window) MSG0(void, self, "windowDidLoad");
    }
    return s->window;
}
static void wc_show(id self, SEL c, id sender) { (void)c; (void)sender; id w = MSG0(id, self, "window"); if (w) MSG(void, w, "makeKeyAndOrderFront:", (id)0); }
static void wc_close(id self, SEL c) { (void)c; id w = wc(self)->window; if (w) MSG0(void, w, "close"); }
static id wc_init_coder(id self, SEL c, id coder) {
    (void)c;
    nibctx_t *x = cst(coder)->ctx;
    unsigned idx = cst(coder)->idx;
    id w = nib_obj(x, idx, "IBWindowTemplate");
    if (w) keep(&wc(self)->window, w);
    id cvc = nib_obj(x, idx, "IBWindowTemplateContentViewController");
    if (cvc) wc_set_cvc(self, 0, cvc);
    return self;
}
static id wc_nib_name(id self, SEL c) { (void)c; return wc(self)->nib; }
static BOOL wc_loaded(id self, SEL c) { (void)c; return wc(self)->window != 0; }
static void w_set_cvc(id self, SEL c, id v) {
    (void)c;
    wstate_t *s = ws(self);
    keep(&s->cvc, v);
    id view = v ? MSG0(id, v, "view") : 0;
    if (view) {
        MSG(void, self, "setContentView:", view);
    }
    if (v) { id t = MSG0(id, v, "title"); if (t && cx_is_str(t) && strlen(cx_cstr(t))) MSG(void, self, "setTitle:", t); }
}
static id w_cvc(id self, SEL c) { (void)c; return ws(self)->cvc; }

/* ---- NSStoryboard */
typedef struct { char dir[600]; id info; } sbst_t;
static sbst_t *sb(id o) { return ST(o); }
static id so_scene;     /* _CXSceneOwner: recebe o outlet "sceneController" */
static void so_set_scene(id self, SEL c, id v) { (void)self; (void)c; so_scene = v; }
static id sb_with_name(id cls, SEL c, id name, id bundle) {
    (void)cls; (void)c; (void)bundle;
    id p = MSG(id, MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle"), "pathForResource:ofType:", name, cx_str("storyboardc"));
    if (!p) { cx_fdprintf(2, "AppKit[mOS]: storyboard '%s' não encontrado\n", cx_cstr(name)); return 0; }
    id o = cx_alloc_instance(&NSStoryboard_c);
    sbst_t *s = calloc(1, sizeof *s);
    snprintf(s->dir, sizeof s->dir, "%s", cx_cstr(p));
    char f[700];
    snprintf(f, sizeof f, "%s/Info.plist", s->dir);
    s->info = cx_plist_file(f);
    ST(o) = s;
    return objc_autorelease(o);
}
static id sb_instantiate(id self, SEL c, id ident) {
    (void)c;
    sbst_t *s = sb(self);
    id map = s->info ? MSG(id, s->info, "objectForKey:", cx_str("NSViewControllerIdentifiersToNibNames")) : 0;
    id nib = map && ident ? MSG(id, map, "objectForKey:", ident) : 0;
    if (!nib) { cx_fdprintf(2, "AppKit[mOS]: cena '%s' não existe no storyboard\n", ident ? cx_cstr(ident) : "?"); return 0; }
    char p[1100];
    snprintf(p, sizeof p, "%s/%s.nib", s->dir, cx_cstr(nib));
    id owner = cx_alloc_instance(&_CXSceneOwner_c);
    id ext = MSG0(id, MSG0(id, (id)cx_class_named("NSMutableDictionary"), "alloc"), "init");
    MSG(void, ext, "setObject:forKey:", self, cx_str("NSStoryboardPlaceholder"));
    so_scene = 0;
    load_nib_path(p, owner, ext, 0);
    id r = so_scene;
    so_scene = 0;
    if (r && cx_is_subclass(r->isa, &NSViewController_c)) vc(r)->storyboard = self;
    if (r && cx_is_subclass(r->isa, &NSWindowController_c) && wc(r)->cvc) vc(wc(r)->cvc)->storyboard = self;
    return r;
}
static id sb_initial(id self, SEL c) {
    sbst_t *s = sb(self);
    id ep = s->info ? MSG(id, s->info, "objectForKey:", cx_str("NSStoryboardDesignatedEntryPointIdentifier")) : 0;
    return ep ? sb_instantiate(self, c, ep) : 0;
}
static id sb_main(id cls, SEL c) {
    id n = MSG(id, MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle"), "objectForInfoDictionaryKey:", cx_str("NSMainStoryboardFile"));
    return n ? sb_with_name(cls, c, n, 0) : 0;
}
/* abre a interface principal do Info.plist (NSMainNibFile / NSMainStoryboardFile) */
static void load_main_interface(void) {
    id bundle = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id nib = MSG(id, bundle, "objectForInfoDictionaryKey:", cx_str("NSMainNibFile"));
    if (nib && cx_is_str(nib)) { load_nib_named(nib, NSApp, 0); return; }
    id sbn = MSG(id, bundle, "objectForInfoDictionaryKey:", cx_str("NSMainStoryboardFile"));
    if (!sbn || !cx_is_str(sbn)) return;
    id s = sb_with_name((id)&NSStoryboard_c, 0, sbn, 0);
    if (!s) return;
    id menu = sb(s)->info ? MSG(id, sb(s)->info, "objectForKey:", cx_str("NSStoryboardMainMenu")) : 0;
    if (menu) { char p[1100]; snprintf(p, sizeof p, "%s/%s.nib", sb(s)->dir, cx_cstr(menu)); load_nib_path(p, NSApp, 0, 0); }
    id ctl = sb_initial(s, 0);
    objc_retain(ctl);
    if (ctl && cx_is_subclass(ctl->isa, &NSWindowController_c)) MSG(void, ctl, "showWindow:", (id)0);
    else if (ctl && cx_is_subclass(ctl->isa, &NSViewController_c)) {
        id v = MSG0(id, ctl, "view");
        NSSize z = vs(v)->frame.s;
        id w = MSG(id, cx_alloc_instance(&NSWindow_c), "initWithContentRect:styleMask:backing:defer:", r_make(0, 0, z.w > 0 ? z.w : 480, z.h > 0 ? z.h : 270), (u64)15, (u64)2, 0);
        MSG(void, w, "setContentViewController:", ctl);
        MSG(void, w, "makeKeyAndOrderFront:", (id)0);
    }
}

static void nib_register(void) {
    DEF(NSCoder, NSObject); DEF(_CXNibCoder, NSCoder);
    M(_CXNibCoder, "decodeObjectForKey:", cd_obj); M(_CXNibCoder, "decodeBoolForKey:", cd_bool); M(_CXNibCoder, "decodeIntegerForKey:", cd_int);
    M(_CXNibCoder, "decodeIntForKey:", cd_int); M(_CXNibCoder, "decodeInt32ForKey:", cd_int); M(_CXNibCoder, "decodeInt64ForKey:", cd_int);
    M(_CXNibCoder, "decodeDoubleForKey:", cd_double); M(_CXNibCoder, "decodeFloatForKey:", cd_float); M(_CXNibCoder, "containsValueForKey:", cd_contains);
    M(_CXNibCoder, "allowsKeyedCoding", cd_keyed); M(_CXNibCoder, "decodeSizeForKey:", cd_size); M(_CXNibCoder, "decodePointForKey:", cd_point);
    M(_CXNibCoder, "decodeRectForKey:", cd_rect); M(_CXNibCoder, "decodeObjectOfClass:forKey:", cd_obj);
    M(NSObject, "initWithCoder:", o_init_coder);
    M(NSView, "initWithCoder:", v_init_coder);
    M(NSLayoutConstraint, "initWithCoder:", lc_init_coder);
    { extern cx_class_t NSBundle_x __asm__("OBJC_CLASS_$_NSBundle");
      cx_add_method(&NSBundle_x, "loadNibNamed:owner:topLevelObjects:", (void *)b_load_nib);
      cx_add_method(&NSBundle_x, "loadNibNamed:owner:options:", (void *)b_load_nib_opts);
      cx_add_cmethod(&NSBundle_x, "loadNibNamed:owner:", (void *)b_load_nib_old); }
    DEF(NSNib, NSObject);
    M(NSNib, "initWithNibNamed:bundle:", nb_init_named); M(NSNib, "instantiateWithOwner:topLevelObjects:", nb_instantiate);
    DEF(NSViewController, NSResponder);
    M(NSViewController, "initWithNibName:bundle:", vc_init_nib); M(NSViewController, "init", vc_init); M(NSViewController, "initWithCoder:", vc_init_coder);
    M(NSViewController, "view", vc_view); M(NSViewController, "setView:", vc_set_view); M(NSViewController, "loadView", vc_load_view);
    M(NSViewController, "viewDidLoad", noop_m); M(NSViewController, "viewWillAppear", noop_m); M(NSViewController, "viewDidAppear", noop_m);
    M(NSViewController, "viewWillDisappear", noop_m); M(NSViewController, "viewDidDisappear", noop_m); M(NSViewController, "isViewLoaded", vc_loaded);
    M(NSViewController, "viewIfLoaded", vc_view); M(NSViewController, "title", vc_title); M(NSViewController, "setTitle:", vc_set_title);
    M(NSViewController, "representedObject", vc_rep); M(NSViewController, "setRepresentedObject:", vc_set_rep); M(NSViewController, "nibName", vc_nib_name);
    M(NSViewController, "storyboard", vc_storyboard); M(NSViewController, "setStoryboard:", vc_set_storyboard);
    M(NSViewController, "updateViewConstraints", noop_m); M(NSViewController, "viewWillLayout", noop_m); M(NSViewController, "viewDidLayout", noop_m);
    DEF(NSWindowController, NSResponder);
    M(NSWindowController, "initWithWindow:", wc_init_window); M(NSWindowController, "initWithWindowNibName:", wc_init_nib);
    M(NSWindowController, "initWithWindowNibName:owner:", wc_init_nib_owner); M(NSWindowController, "initWithCoder:", wc_init_coder);
    M(NSWindowController, "window", wc_window); M(NSWindowController, "setWindow:", wc_set_window); M(NSWindowController, "showWindow:", wc_show);
    M(NSWindowController, "close", wc_close); M(NSWindowController, "windowDidLoad", noop_m); M(NSWindowController, "windowWillLoad", noop_m);
    M(NSWindowController, "contentViewController", wc_cvc); M(NSWindowController, "setContentViewController:", wc_set_cvc);
    M(NSWindowController, "windowNibName", wc_nib_name); M(NSWindowController, "loadWindow", noop_m); M(NSWindowController, "isWindowLoaded", wc_loaded);
    M(NSWindow, "setContentViewController:", w_set_cvc); M(NSWindow, "contentViewController", w_cvc);
    DEF(NSStoryboard, NSObject);
    CM(NSStoryboard, "storyboardWithName:bundle:", sb_with_name); CM(NSStoryboard, "mainStoryboard", sb_main);
    M(NSStoryboard, "instantiateInitialController", sb_initial); M(NSStoryboard, "instantiateControllerWithIdentifier:", sb_instantiate);
    DEF(_CXSceneOwner, NSObject);
    M(_CXSceneOwner, "setSceneController:", so_set_scene);
}
