/* mOS Core Text — parágrafo e frame (incluído no fim do coretext.c).
 * CTParagraphStyle (os 18 especificadores; o NSParagraphStyle do AppKit também serve, como no macOS), CTTextTab,
 * CTRunDelegate, CTGlyphInfo, CTRubyAnnotation, as constantes de atributo, e o framesetter de verdade: parágrafo
 * por parágrafo, recuos, alinhamento (natural pela direção do parágrafo, justificado menos a última linha),
 * tabulações (esquerda, direita, centro, decimal; padrão de 28 pt), altura de linha (múltiplo, mínimo, máximo,
 * espaçamento), espaço antes/depois do parágrafo, modos de quebra (palavra, caractere, recorte e as três
 * truncagens), caminho retangular ou não (a caixa dele), progressão de cima para baixo, faixa visível e o tamanho
 * sugerido. A quebra é a da UAX #14 e o bidi o da UAX #9 (ct_layout.c). Teste: tools/coretext_host.py. */

CTCONST(kCTParagraphStyleAttributeName, "NSParagraphStyle")
CTCONST(kCTRunDelegateAttributeName, "CTRunDelegate")
CTCONST(kCTUnderlineStyleAttributeName, "NSUnderline")
CTCONST(kCTUnderlineColorAttributeName, "NSUnderlineColor")
CTCONST(kCTStrokeWidthAttributeName, "NSStrokeWidth")
CTCONST(kCTStrokeColorAttributeName, "NSStrokeColor")
CTCONST(kCTSuperscriptAttributeName, "NSSuperScript")
CTCONST(kCTGlyphInfoAttributeName, "NSGlyphInfo")
CTCONST(kCTCharacterShapeAttributeName, "NSCharacterShape")
CTCONST(kCTBaselineClassAttributeName, "CTBaselineClass")
CTCONST(kCTBaselineInfoAttributeName, "CTBaselineInfo")
CTCONST(kCTBaselineReferenceInfoAttributeName, "CTBaselineReferenceInfo")
CTCONST(kCTBaselineOffsetAttributeName, "CTBaselineOffset")
CTCONST(kCTBaselineClassRoman, "CTBaselineClassRoman")
CTCONST(kCTBaselineClassIdeographicCentered, "CTBaselineClassIdeographicCentered")
CTCONST(kCTBaselineClassIdeographicLow, "CTBaselineClassIdeographicLow")
CTCONST(kCTBaselineClassIdeographicHigh, "CTBaselineClassIdeographicHigh")
CTCONST(kCTBaselineClassHanging, "CTBaselineClassHanging")
CTCONST(kCTBaselineClassMath, "CTBaselineClassMath")
CTCONST(kCTBaselineReferenceFont, "CTBaselineReferenceFont")
CTCONST(kCTBaselineOriginalFont, "CTBaselineOriginalFont")
CTCONST(kCTRubyAnnotationAttributeName, "CTRubyAnnotation")
CTCONST(kCTRubyAnnotationSizeFactorAttributeName, "CTRubyAnnotationSizeFactor")
CTCONST(kCTRubyAnnotationScaleToFitAttributeName, "CTRubyAnnotationScaleToFit")
CTCONST(kCTBackgroundColorAttributeName, "CTBackgroundColor")
CTCONST(kCTHorizontalInVerticalFormsAttributeName, "CTHorizontalInVerticalForms")
CTCONST(kCTAdaptiveImageProviderAttributeName, "CTAdaptiveImageProvider")
CTCONST(kCTFrameProgressionAttributeName, "CTFrameProgression")
CTCONST(kCTFramePathFillRuleAttributeName, "CTFramePathFillRule")
CTCONST(kCTFramePathWidthAttributeName, "CTFramePathWidth")
CTCONST(kCTFrameClippingPathsAttributeName, "CTFrameClippingPaths")
CTCONST(kCTFramePathClippingPathAttributeName, "CTFramePathClippingPath")
CTCONST(kCTTypesetterOptionAllowUnboundedLayout, "CTTypesetterOptionAllowUnboundedLayout")
CTCONST(kCTTypesetterOptionDisableBidiProcessing, "CTTypesetterOptionDisableBidiProcessing")
CTCONST(kCTTabColumnTerminatorsAttributeName, "NSTabColumnTerminatorsAttributeName")
CTCONST(kCTFontManagerErrorDomain, "com.apple.CoreText.CTFontManagerErrorDomain")
CTCONST(kCTFontManagerErrorFontURLsKey, "CTFontManagerErrorFontURLs")
CTCONST(kCTFontManagerErrorFontDescriptorsKey, "CTFontManagerErrorFontDescriptors")
CTCONST(kCTFontManagerErrorFontAssetNameKey, "CTFontManagerErrorFontAssetNameKey")

double floor(double);
double ceil(double);
void *realloc(void *p, size_t n);
static id ct_num(id o, double *v) { if (o && cx_responds(object_getClass(o), sel_registerName("doubleValue"))) { *v = MSG0(double, o, "doubleValue"); return o; } return 0; }
static int ct_inst(id o, cx_class_t *k) { return o && object_getClass(o) == k; }
#define CTOBJ(N) static cx_class_t N##_c, N##_m;
CTOBJ(CTParagraphStyle) CTOBJ(CTTextTab) CTOBJ(CTRunDelegate) CTOBJ(CTGlyphInfo) CTOBJ(CTRubyAnnotation)
static void ct_obj_dealloc(id self, SEL c);
static int ct_objs_ready;
static void ct_objs_init(void) {
    if (ct_objs_ready) return;
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns) return;
    cx_define_class(&CTParagraphStyle_c, &CTParagraphStyle_m, "CTParagraphStyle", ns, 8);
    cx_define_class(&CTTextTab_c, &CTTextTab_m, "CTTextTab", ns, 8);
    cx_define_class(&CTRunDelegate_c, &CTRunDelegate_m, "CTRunDelegate", ns, 8);
    cx_define_class(&CTGlyphInfo_c, &CTGlyphInfo_m, "CTGlyphInfo", ns, 8);
    cx_define_class(&CTRubyAnnotation_c, &CTRubyAnnotation_m, "CTRubyAnnotation", ns, 8);
    cx_add_method(&CTParagraphStyle_c, "dealloc", (void *)ct_obj_dealloc);
    cx_add_method(&CTTextTab_c, "dealloc", (void *)ct_obj_dealloc);
    cx_add_method(&CTRunDelegate_c, "dealloc", (void *)ct_obj_dealloc);
    cx_add_method(&CTGlyphInfo_c, "dealloc", (void *)ct_obj_dealloc);
    cx_add_method(&CTRubyAnnotation_c, "dealloc", (void *)ct_obj_dealloc);
    ct_objs_ready = 1;
}
static id ct_obj_new(cx_class_t *k, void *state) {
    ct_objs_init();
    id o = ct_objs_ready ? cx_alloc_instance(k) : 0;
    if (!o) { free(state); return 0; }
    *(void **)((u8 *)o + 8) = state;
    return o;
}

/* ---------------------------------------------------------------- CTTextTab */
typedef struct { u8 align; double loc; id opts; } ct_ttab_t;
id CTTextTabCreate(u8 alignment, double location, id options) {
    ct_ttab_t *t = calloc(1, sizeof *t);
    if (!t) return 0;
    t->align = alignment; t->loc = location; t->opts = options ? objc_retain(options) : 0;
    return ct_obj_new(&CTTextTab_c, t);
}
u8 CTTextTabGetAlignment(id tab) { ct_ttab_t *t = ct_inst(tab, &CTTextTab_c) ? ct_state(tab) : 0; return t ? t->align : 0; }
double CTTextTabGetLocation(id tab) { ct_ttab_t *t = ct_inst(tab, &CTTextTab_c) ? ct_state(tab) : 0; return t ? t->loc : 0; }
id CTTextTabGetOptions(id tab) { ct_ttab_t *t = ct_inst(tab, &CTTextTab_c) ? ct_state(tab) : 0; return t ? t->opts : 0; }
unsigned CTTextTabGetTypeID(void) { return 0x43545454u; }

/* ---------------------------------------------------------------- CTParagraphStyle */
typedef struct {
    u8 align, brk;
    double first, head, tail, deftab, mult, maxh, minh, spacing, after, before, maxsp, minsp, adj;
    int8_t dir;
    u64 bounds;
    struct { u8 align; double loc; } tabs[64];
    int ntabs, has_tabs;
} ct_ps_t;
static void ct_ps_default(ct_ps_t *p) {
    memset(p, 0, sizeof *p);
    p->align = 4;                     /* natural */
    p->dir = -1;
    p->maxsp = 1e9;
}
enum { PS_ALIGN, PS_FIRST, PS_HEAD, PS_TAIL, PS_TABS, PS_DEFTAB, PS_BRK, PS_MULT, PS_MAXH, PS_MINH, PS_SPACING, PS_AFTER, PS_BEFORE,
       PS_DIR, PS_MAXSP, PS_MINSP, PS_ADJ, PS_BOUNDS };
typedef struct { u32 spec; size_t size; const void *value; } ct_ps_setting;
static void ct_ps_set(ct_ps_t *p, u32 spec, const void *v, size_t size) {
    if (!v) return;
    double d = size >= sizeof(double) ? *(const double *)v : 0;
    switch (spec) {
    case PS_ALIGN: p->align = *(const u8 *)v; break;
    case PS_BRK: p->brk = *(const u8 *)v; break;
    case PS_DIR: p->dir = *(const int8_t *)v; break;
    case PS_FIRST: p->first = d; break;
    case PS_HEAD: p->head = d; break;
    case PS_TAIL: p->tail = d; break;
    case PS_DEFTAB: p->deftab = d; break;
    case PS_MULT: p->mult = d; break;
    case PS_MAXH: p->maxh = d; break;
    case PS_MINH: p->minh = d; break;
    case PS_SPACING: p->spacing = d; break;
    case PS_AFTER: p->after = d; break;
    case PS_BEFORE: p->before = d; break;
    case PS_MAXSP: p->maxsp = d; break;
    case PS_MINSP: p->minsp = d; break;
    case PS_ADJ: p->adj = d; break;
    case PS_BOUNDS: p->bounds = size >= 8 ? *(const u64 *)v : 0; break;
    case PS_TABS: {
        id arr = *(id const *)v;
        long n = arr ? (long)MSG0(u64, arr, "count") : 0;
        p->ntabs = 0;
        p->has_tabs = 1;
        for (long i = 0; i < n && p->ntabs < 64; i++) {
            id t = MSG(id, arr, "objectAtIndex:", (u64)i);
            if (ct_inst(t, &CTTextTab_c)) { p->tabs[p->ntabs].align = CTTextTabGetAlignment(t); p->tabs[p->ntabs].loc = CTTextTabGetLocation(t); }
            else if (t && cx_responds(object_getClass(t), sel_registerName("location"))) {
                p->tabs[p->ntabs].loc = MSG0(double, t, "location");
                p->tabs[p->ntabs].align = cx_responds(object_getClass(t), sel_registerName("alignment")) ? (u8)MSG0(long, t, "alignment") : 0;
            } else continue;
            p->ntabs++;
        }
        break;
    }
    }
}
id CTParagraphStyleCreate(const ct_ps_setting *settings, size_t n) {
    ct_ps_t *p = malloc(sizeof *p);
    if (!p) return 0;
    ct_ps_default(p);
    for (size_t i = 0; settings && i < n; i++) ct_ps_set(p, settings[i].spec, settings[i].value, settings[i].size);
    return ct_obj_new(&CTParagraphStyle_c, p);
}
id CTParagraphStyleCreateCopy(id style) {
    ct_ps_t *p = malloc(sizeof *p), *s = ct_inst(style, &CTParagraphStyle_c) ? ct_state(style) : 0;
    if (!p) return 0;
    if (s) *p = *s; else ct_ps_default(p);
    return ct_obj_new(&CTParagraphStyle_c, p);
}
unsigned CTParagraphStyleGetTypeID(void) { return 0x43545053u; }
u8 CTParagraphStyleGetValueForSpecifier(id style, u32 spec, size_t size, void *out) {
    ct_ps_t def, *p = ct_inst(style, &CTParagraphStyle_c) ? ct_state(style) : 0;
    if (!p) { ct_ps_default(&def); p = &def; }
    if (!out) return 0;
    switch (spec) {
    case PS_ALIGN: if (size < 1) return 0; *(u8 *)out = p->align; return 1;
    case PS_BRK: if (size < 1) return 0; *(u8 *)out = p->brk; return 1;
    case PS_DIR: if (size < 1) return 0; *(int8_t *)out = p->dir; return 1;
    case PS_BOUNDS: if (size < 8) return 0; *(u64 *)out = p->bounds; return 1;
    case PS_TABS: {
        if (size < sizeof(id)) return 0;
        id arr = MSG0(id, cls("NSMutableArray"), "array");
        int n = p->has_tabs ? p->ntabs : 12;
        for (int i = 0; i < n; i++) {
            id t = p->has_tabs ? CTTextTabCreate(p->tabs[i].align, p->tabs[i].loc, 0) : CTTextTabCreate(0, 28.0 * (i + 1), 0);
            MSG(void, arr, "addObject:", t);
            objc_release(t);
        }
        *(id *)out = arr;
        return 1;
    }
    }
    if (size < sizeof(double)) return 0;
    double v = 0;
    switch (spec) {
    case PS_FIRST: v = p->first; break;
    case PS_HEAD: v = p->head; break;
    case PS_TAIL: v = p->tail; break;
    case PS_DEFTAB: v = p->deftab; break;
    case PS_MULT: v = p->mult; break;
    case PS_MAXH: v = p->maxh; break;
    case PS_MINH: v = p->minh; break;
    case PS_SPACING: v = p->spacing; break;
    case PS_AFTER: v = p->after; break;
    case PS_BEFORE: v = p->before; break;
    case PS_MAXSP: v = p->maxsp; break;
    case PS_MINSP: v = p->minsp; break;
    case PS_ADJ: v = p->adj; break;
    default: return 0;
    }
    *(double *)out = v;
    return 1;
}
/* estilo do parágrafo no índice: CTParagraphStyle ou NSParagraphStyle */
static void ct_ps_at(id astr, long idx, ct_ps_t *out) {
    ct_ps_default(out);
    id v = ct_dict_get(ct_attrs(astr, idx), (id)kCTParagraphStyleAttributeName);
    if (!v) return;
    if (ct_inst(v, &CTParagraphStyle_c)) { *out = *(ct_ps_t *)ct_state(v); return; }
    cx_class_t *k = object_getClass(v);
#define RESP(s) cx_responds(k, sel_registerName(s))
    if (RESP("alignment")) out->align = (u8)MSG0(long, v, "alignment");
    if (RESP("firstLineHeadIndent")) out->first = MSG0(double, v, "firstLineHeadIndent");
    if (RESP("headIndent")) out->head = MSG0(double, v, "headIndent");
    if (RESP("tailIndent")) out->tail = MSG0(double, v, "tailIndent");
    if (RESP("lineSpacing")) out->spacing = MSG0(double, v, "lineSpacing");
    if (RESP("paragraphSpacing")) out->after = MSG0(double, v, "paragraphSpacing");
    if (RESP("paragraphSpacingBefore")) out->before = MSG0(double, v, "paragraphSpacingBefore");
    if (RESP("lineHeightMultiple")) out->mult = MSG0(double, v, "lineHeightMultiple");
    if (RESP("minimumLineHeight")) out->minh = MSG0(double, v, "minimumLineHeight");
    if (RESP("maximumLineHeight")) out->maxh = MSG0(double, v, "maximumLineHeight");
    if (RESP("defaultTabInterval")) out->deftab = MSG0(double, v, "defaultTabInterval");
    if (RESP("lineBreakMode")) out->brk = (u8)MSG0(long, v, "lineBreakMode");
    if (RESP("baseWritingDirection")) out->dir = (int8_t)MSG0(long, v, "baseWritingDirection");
    if (RESP("tabStops")) { id t = MSG0(id, v, "tabStops"); ct_ps_set(out, PS_TABS, &t, sizeof t); }
#undef RESP
}
static int ct_para_direction(id astr, long idx) {
    ct_ps_t p;
    ct_ps_at(astr, idx, &p);
    return p.dir;
}

/* ---------------------------------------------------------------- CTRunDelegate, CTGlyphInfo, CTRubyAnnotation */
typedef struct { long version; void (*dealloc)(void *); double (*ascent)(void *); double (*descent)(void *); double (*width)(void *); } ct_rd_cb;
typedef struct { ct_rd_cb cb; void *ref; } ct_rd_t;
id CTRunDelegateCreate(const ct_rd_cb *cb, void *ref) {
    if (!cb) return 0;
    ct_rd_t *d = calloc(1, sizeof *d);
    if (!d) return 0;
    d->cb = *cb; d->ref = ref;
    return ct_obj_new(&CTRunDelegate_c, d);
}
void *CTRunDelegateGetRefCon(id rd) { ct_rd_t *d = ct_inst(rd, &CTRunDelegate_c) ? ct_state(rd) : 0; return d ? d->ref : 0; }
unsigned CTRunDelegateGetTypeID(void) { return 0x43545244u; }
static int ct_run_delegate_at(id astr, long idx, double *adv, double *asc, double *desc) {
    id v = ct_dict_get(ct_attrs(astr, idx), (id)kCTRunDelegateAttributeName);
    ct_rd_t *d = ct_inst(v, &CTRunDelegate_c) ? ct_state(v) : 0;
    if (!d) return 0;
    *adv = d->cb.width ? d->cb.width(d->ref) : 0;
    *asc = d->cb.ascent ? d->cb.ascent(d->ref) : 0;
    *desc = d->cb.descent ? d->cb.descent(d->ref) : 0;
    return 1;
}
typedef struct { u16 glyph; int has_glyph; id name, base; u16 cid; int coll; } ct_gi_t;
id CTGlyphInfoCreateWithGlyphName(id name, id font, id base) {
    ct_gi_t *g = calloc(1, sizeof *g);
    if (!g) return 0;
    g->name = name ? objc_retain(name) : 0;
    g->base = base ? objc_retain(base) : 0;
    if (font && name) { g->glyph = CTFontGetGlyphWithName(font, name); g->has_glyph = g->glyph != 0; }
    return ct_obj_new(&CTGlyphInfo_c, g);
}
id CTGlyphInfoCreateWithGlyph(u16 glyph, id font, id base) {
    ct_gi_t *g = calloc(1, sizeof *g);
    if (!g) return 0;
    g->glyph = glyph; g->has_glyph = 1;
    g->base = base ? objc_retain(base) : 0;
    if (font) g->name = CTFontCopyNameForGlyph(font, glyph);
    return ct_obj_new(&CTGlyphInfo_c, g);
}
id CTGlyphInfoCreateWithCharacterIdentifier(u16 cid, int collection, id base) {
    ct_gi_t *g = calloc(1, sizeof *g);
    if (!g) return 0;
    g->cid = cid; g->coll = collection;
    g->base = base ? objc_retain(base) : 0;
    if (collection == 0) { g->glyph = cid; g->has_glyph = 1; }   /* kCTCharacterCollectionIdentityMapping */
    return ct_obj_new(&CTGlyphInfo_c, g);
}
id CTGlyphInfoGetGlyphName(id gi) { ct_gi_t *g = ct_inst(gi, &CTGlyphInfo_c) ? ct_state(gi) : 0; return g ? g->name : 0; }
u16 CTGlyphInfoGetGlyph(id gi) { ct_gi_t *g = ct_inst(gi, &CTGlyphInfo_c) ? ct_state(gi) : 0; return g ? g->glyph : 0; }
u16 CTGlyphInfoGetCharacterIdentifier(id gi) { ct_gi_t *g = ct_inst(gi, &CTGlyphInfo_c) ? ct_state(gi) : 0; return g ? (g->cid ? g->cid : g->glyph) : 0; }
int CTGlyphInfoGetCharacterCollection(id gi) { ct_gi_t *g = ct_inst(gi, &CTGlyphInfo_c) ? ct_state(gi) : 0; return g ? g->coll : 0; }
unsigned CTGlyphInfoGetTypeID(void) { return 0x43544749u; }
/* atributo kCTGlyphInfoAttributeName: o glifo pedido no lugar do da cmap */
static void ct_glyph_info_at(id astr, long idx, id font, u16 *glyph) {
    id v = ct_dict_get(ct_attrs(astr, idx), (id)kCTGlyphInfoAttributeName);
    ct_gi_t *g = ct_inst(v, &CTGlyphInfo_c) ? ct_state(v) : 0;
    if (!g) return;
    if (!g->has_glyph && g->name && font) { g->glyph = CTFontGetGlyphWithName(font, g->name); g->has_glyph = g->glyph != 0; }
    if (g->has_glyph) *glyph = g->glyph;
}
typedef struct { u8 align, overhang; double size; id text[4]; id attrs; } ct_ruby_t;
id CTRubyAnnotationCreate(u8 alignment, u8 overhang, double sizeFactor, id text[4]) {
    ct_ruby_t *r = calloc(1, sizeof *r);
    if (!r) return 0;
    r->align = alignment; r->overhang = overhang; r->size = sizeFactor;
    for (int i = 0; text && i < 4; i++) r->text[i] = text[i] ? objc_retain(text[i]) : 0;
    return ct_obj_new(&CTRubyAnnotation_c, r);
}
id CTRubyAnnotationCreateWithAttributes(u8 alignment, u8 overhang, u8 position, id string, id attrs) {
    ct_ruby_t *r = calloc(1, sizeof *r);
    if (!r) return 0;
    r->align = alignment; r->overhang = overhang; r->size = 0.5;
    if (position < 4 && string) r->text[position] = objc_retain(string);
    r->attrs = attrs ? objc_retain(attrs) : 0;
    double v;
    if (attrs && ct_num(ct_dict_get(attrs, (id)kCTRubyAnnotationSizeFactorAttributeName), &v)) r->size = v;
    return ct_obj_new(&CTRubyAnnotation_c, r);
}
id CTRubyAnnotationCreateCopy(id ruby) {
    ct_ruby_t *s = ct_inst(ruby, &CTRubyAnnotation_c) ? ct_state(ruby) : 0;
    if (!s) return 0;
    id o = CTRubyAnnotationCreate(s->align, s->overhang, s->size, s->text);
    if (o && s->attrs) ((ct_ruby_t *)ct_state(o))->attrs = objc_retain(s->attrs);
    return o;
}
u8 CTRubyAnnotationGetAlignment(id ruby) { ct_ruby_t *r = ct_inst(ruby, &CTRubyAnnotation_c) ? ct_state(ruby) : 0; return r ? r->align : 0; }
u8 CTRubyAnnotationGetOverhang(id ruby) { ct_ruby_t *r = ct_inst(ruby, &CTRubyAnnotation_c) ? ct_state(ruby) : 0; return r ? r->overhang : 0; }
double CTRubyAnnotationGetSizeFactor(id ruby) { ct_ruby_t *r = ct_inst(ruby, &CTRubyAnnotation_c) ? ct_state(ruby) : 0; return r ? r->size : 0; }
id CTRubyAnnotationGetTextForPosition(id ruby, u8 pos) { ct_ruby_t *r = ct_inst(ruby, &CTRubyAnnotation_c) ? ct_state(ruby) : 0; return r && pos < 4 ? r->text[pos] : 0; }
unsigned CTRubyAnnotationGetTypeID(void) { return 0x43545241u; }
static void ct_obj_dealloc(id self, SEL c) {
    (void)c;
    void *st = ct_state(self);
    cx_class_t *k = object_getClass(self);
    if (st) {
        if (k == &CTTextTab_c) objc_release(((ct_ttab_t *)st)->opts);
        else if (k == &CTRunDelegate_c) { ct_rd_t *d = st; if (d->cb.dealloc) d->cb.dealloc(d->ref); }
        else if (k == &CTGlyphInfo_c) { objc_release(((ct_gi_t *)st)->name); objc_release(((ct_gi_t *)st)->base); }
        else if (k == &CTRubyAnnotation_c) { ct_ruby_t *r = st; for (int i = 0; i < 4; i++) objc_release(r->text[i]); objc_release(r->attrs); }
        free(st);
    }
    *(void **)((u8 *)self + 8) = 0;
    cx_object_free(self);
}

/* ---------------------------------------------------------------- linha: o que faltava */
unsigned CTLineGetTypeID(void) { return 0x43544C4Eu; }
unsigned CTTypesetterGetTypeID(void) { return 0x43545453u; }
unsigned CTFrameGetTypeID(void) { return 0x43544652u; }
unsigned CTFramesetterGetTypeID(void) { return 0x43544653u; }
double CTLineGetPenOffsetForFlush(id line, double flush, double width) {
    ct_line_t *b = ct_state(line);
    if (!b) return 0;
    if (flush < 0) flush = 0;
    if (flush > 1) flush = 1;
    double w = b->width - CTLineGetTrailingWhitespaceWidth(line), off = (width - w) * flush;
    return off > 0 ? off : 0;
}
/* índice do texto na posição x da linha (o meio do glifo decide entre o antes e o depois) */
long CTLineGetStringIndexForPosition(id line, ct_pt_t pos) {
    ct_line_t *b = ct_state(line);
    if (!b) return -1;
    if (!b->n) return b->range.loc;
    double x = 0;
    for (long i = 0; i < b->n; i++) {
        ct_clus *g = &b->g[i];
        int rtl = g->level & 1;
        if (pos.x < x + g->adv || i == b->n - 1) {
            int before = pos.x < x + g->adv / 2;
            if (pos.x >= x + g->adv) before = 0;
            if (rtl) before = !before;
            return before ? g->idx : g->idx + g->units;
        }
        x += g->adv;
    }
    return b->range.loc + b->range.len;
}
/* cursores: para cada caractere, a borda de entrada e a de saída, na ordem lógica */
void CTLineEnumerateCaretOffsets(id line, void *block) {
    ct_line_t *b = ct_state(line);
    ct_block_t *bl = block;
    if (!b || !bl || !bl->invoke) return;
    long n = b->n;
    double *x = malloc((size_t)(n ? n : 1) * sizeof *x);
    long *ord = malloc((size_t)(n ? n : 1) * sizeof *ord);
    if (!x || !ord) { free(x); free(ord); return; }
    double acc = 0;
    for (long i = 0; i < n; i++) { x[i] = acc; acc += b->g[i].adv; ord[i] = i; }
    for (long i = 1; i < n; i++) { long k = ord[i], j = i - 1; while (j >= 0 && b->g[ord[j]].idx > b->g[k].idx) { ord[j + 1] = ord[j]; j--; } ord[j + 1] = k; }
    void (*inv)(void *, double, long, u8, u8 *) = (void (*)(void *, double, long, u8, u8 *))bl->invoke;
    u8 stop = 0;
    for (long k = 0; k < n && !stop; k++) {
        ct_clus *g = &b->g[ord[k]];
        double lead = (g->level & 1) ? x[ord[k]] + g->adv : x[ord[k]], trail = (g->level & 1) ? x[ord[k]] : x[ord[k]] + g->adv;
        for (int u = 0; u < g->units && !stop; u++) {
            inv(block, lead, g->idx + u, 1, &stop);
            if (!stop) inv(block, trail, g->idx + u, 0, &stop);
        }
    }
    free(x);
    free(ord);
}
id CTTypesetterCreateLineWithOffset(id ts, ct_range_t range, double offset) { (void)offset; return CTTypesetterCreateLine(ts, range); }
long CTTypesetterSuggestLineBreakWithOffset(id ts, long start, double width, double offset) { (void)offset; return ct_suggest(ct_state(ts), start, width, 1); }
long CTTypesetterSuggestClusterBreakWithOffset(id ts, long start, double width, double offset) { (void)offset; return ct_suggest(ct_state(ts), start, width, 0); }
id CTTypesetterCreateWithAttributedStringAndOptions(id astr, id opts) {
    id o = CTTypesetterCreateWithAttributedString(astr);
    ct_ts_t *t = ct_state(o);
    if (!t || !opts) return o;
    double v;
    int redo = 0;
    if (ct_num(ct_dict_get(opts, (id)kCTTypesetterOptionForcedEmbeddingLevel), &v)) { t->forced = (int)v & 1; redo = 1; }
    id dis = ct_dict_get(opts, (id)kCTTypesetterOptionDisableBidiProcessing);
    if (dis && cx_responds(object_getClass(dis), sel_registerName("boolValue")) && MSG0(BOOL, dis, "boolValue")) {
        if (t->lv) memset(t->lv, 0, (size_t)(t->utf16 > 100000 ? 100000 : t->utf16));
        t->para = 0;
        return o;
    }
    if (redo && t->text) t->para = ctl_bidi_levels(t->text, t->utf16 > 100000 ? 100000 : t->utf16, t->forced, t->lv);
    return o;
}

/* ---------------------------------------------------------------- framesetter e frame */
ct_rect_t CGPathGetBoundingBox(const void *path);
id CTFramesetterCreateWithTypesetter(id ts) {
    if (!ts || object_getClass(ts) != &CTTypesetter_c) return 0;
    ct_lines_init();
    ct_fs_t *f = calloc(1, sizeof *f);
    if (!f) return 0;
    f->ts = objc_retain(ts);
    id o = cx_alloc_instance(&CTFramesetter_c);
    if (!o) { objc_release(ts); free(f); return 0; }
    *(void **)((u8 *)o + 8) = f;
    return o;
}
id CTFramesetterCreateWithAttributedString(id astr) {
    id ts = astr ? CTTypesetterCreateWithAttributedString(astr) : 0;
    if (!ts) return 0;
    id o = CTFramesetterCreateWithTypesetter(ts);
    objc_release(ts);
    return o;
}
id CTFramesetterGetTypesetter(id fs) { ct_fs_t *f = fs && object_getClass(fs) == &CTFramesetter_c ? ct_state(fs) : 0; return f ? f->ts : 0; }
/* largura ocupada por uma tabulação na posição x (a partir da margem do parágrafo) */
static double ct_tab_advance(const ct_ps_t *p, double x, double seg) {
    double interval = p->deftab > 0 ? p->deftab : 28;
    for (int i = 0; i < (p->has_tabs ? p->ntabs : 0); i++) {
        double loc = p->tabs[i].loc;
        u8 a = p->tabs[i].align;
        double target = a == 1 ? loc - seg : a == 2 ? loc - seg / 2 : loc;          /* direita, centro; 3 = justificada como esquerda */
        if (target > x + 0.01) return target - x;
    }
    if (!p->has_tabs) {
        double stop = floor(x / 28 + 1) * 28;
        if (stop > 12 * 28 && p->deftab > 0) stop = floor(x / interval + 1) * interval;
        return stop - x;
    }
    return floor(x / interval + 1) * interval - x;
}
typedef struct { id line; double asc, desc, lead, height, x, width_used; long a, b; int last; } ct_fline;
/* largura do trecho [i, j) dos clusters (para tabulação direita/centro: até a próxima tabulação) */
static double ct_seg_width(ct_ts_t *t, long i, long end_idx) {
    double w = 0;
    for (; i < t->n && t->g[i].idx < end_idx && t->g[i].cp != '\t'; i++) w += t->g[i].adv;
    return w;
}
/* uma linha do frame a partir de pos com a largura w; devolve o fim (índice UTF-16) */
static long ct_fit(ct_ts_t *t, long pos, long stop, double w, const ct_ps_t *p, double indent, int brk_mode) {
    long i = 0;
    while (i < t->n && t->g[i].idx + t->g[i].units <= pos) i++;
    double x = indent;
    long last_brk = -1, last_cl = -1, end = pos;
    int any = 0;
    for (; i < t->n && t->g[i].idx < stop; i++) {
        ct_clus *g = &t->g[i];
        long e = g->idx + g->units;
        double adv = g->cp == '\t' ? ct_tab_advance(p, x, ct_seg_width(t, i + 1, stop)) : g->adv;
        int hard = g->cp == '\n' || g->cp == '\r' || g->cp == 0x2028 || g->cp == 0x2029 || (t->brk && e <= t->seen && t->brk[e] == 2);
        if (brk_mode < 2 && !hard && any && !ct_hangs(g->cp) && x + adv > indent + w + 1e-6) {
            if (brk_mode == 0 && last_brk > pos) return last_brk;
            if (last_cl > pos) return last_cl;
            return end > pos ? end : e;
        }
        x += adv;
        end = e;
        any = 1;
        if (t->brk && e <= t->seen) { if (t->brk[e]) last_brk = e; } else if (ct_hangs(g->cp)) last_brk = e;
        if (!t->cl || (e <= t->seen && t->cl[e])) last_cl = e;
        if (hard) {
            if (g->cp == '\r' && i + 1 < t->n && t->g[i + 1].cp == '\n') end = t->g[i + 1].idx + t->g[i + 1].units;
            return end;
        }
    }
    return end > pos ? end : stop;
}
/* as tabulações da linha pegam a largura pela posição */
static void ct_line_tabs(ct_line_t *b, const ct_ps_t *p, double indent) {
    double x = indent;
    int any = 0;
    for (long i = 0; i < b->n; i++) if (b->g[i].cp == '\t') any = 1;
    if (!any) return;
    for (long i = 0; i < b->n; i++) {
        if (b->g[i].cp == '\t') {
            double seg = 0;
            for (long j = i + 1; j < b->n && b->g[j].cp != '\t'; j++) seg += b->g[j].adv;
            b->g[i].adv = ct_tab_advance(p, x, seg);
        }
        x += b->g[i].adv;
    }
    ct_recompute(b);
}
/* layout inteiro: linhas, origens (relativas à caixa, y para cima) e o fim do que coube */
static int ct_layout(ct_ts_t *t, id ts, ct_range_t range, double W, double H, int bounded, id arr, ct_pt_t **origins, long *nout,
                     long *fit_end, double *used_w, double *used_h) {
    long stop = range.loc + range.len, pos = range.loc, cap = 64, n = 0;
    ct_pt_t *org = malloc((size_t)cap * sizeof *org);
    if (!org) return 0;
    double top = H, maxw = 0;
    int first_para_line = 1, done = 0;
    ct_ps_t p;
    ct_ps_at(t->astr, pos, &p);
    double prev_after = 0;
    while (pos < stop && !done) {
        if (first_para_line) { ct_ps_at(t->astr, pos, &p); top -= (n ? prev_after : 0) + p.before; }
        double indent = first_para_line ? p.first : p.head;
        double right = p.tail > 0 ? p.tail : W + p.tail;
        double w = right - indent;
        if (w < 1) w = 1;
        int mode = p.brk == 1 ? 1 : (p.brk >= 2 ? 2 : 0);
        long e = ct_fit(t, pos, stop, w, &p, indent, mode);
        if (e <= pos) e = pos + 1;
        int last = e >= stop || (t->text && e <= t->seen && e > 0 && (t->text[e - 1] == '\n' || t->text[e - 1] == '\r' || t->text[e - 1] == 0x2029 || t->text[e - 1] == 0x2028));
        ct_range_t rr = {pos, e - pos};
        id line = CTTypesetterCreateLine(ts, rr);
        ct_line_t *lb = ct_state(line);
        if (!lb) break;
        ct_line_tabs(lb, &p, indent);
        /* truncagem: a linha do parágrafo que passou da largura */
        if (p.brk >= 3 && lb->width - CTLineGetTrailingWhitespaceWidth(line) > w + 1e-6) {
            id tr = CTLineCreateTruncatedLine(line, w, p.brk == 3 ? 1 : p.brk == 5 ? 2 : 0, 0);
            if (tr) { objc_release(line); line = tr; lb = ct_state(line); }
        }
        if (p.align == 3 && !last) {
            id j = CTLineCreateJustifiedLine(line, 1.0, w);
            if (j) { objc_release(line); line = j; lb = ct_state(line); }
        }
        double asc = lb->ascent, desc = lb->descent, lead = lb->leading;
        double h = asc + desc + lead;
        if (p.mult > 0) h *= p.mult;
        if (p.minh > 0 && h < p.minh) h = p.minh;
        if (p.maxh > 0 && h > p.maxh) h = p.maxh;
        double extra = h - (asc + desc + lead);
        double spacing = p.spacing + p.adj;
        if (spacing < p.minsp) spacing = p.minsp;
        if (spacing > p.maxsp) spacing = p.maxsp;
        double baseline = top - asc - (extra > 0 ? extra : 0);
        if (bounded && n > 0 && baseline - desc < -0.5) { objc_release(line); break; }
        if (bounded && n == 0 && baseline - desc < -0.5 && H < asc + desc) { objc_release(line); break; }
        int rtl_para = t->lv ? (t->para & 1) : 0;
        if (p.dir >= 0) rtl_para = p.dir == 1;
        u8 al = p.align == 4 ? (rtl_para ? 1 : 0) : p.align;
        double lw = lb->width - CTLineGetTrailingWhitespaceWidth(line);
        double x = indent + (al == 1 ? w - lw : al == 2 ? (w - lw) / 2 : 0);
        if (n == cap) { cap *= 2; ct_pt_t *no = realloc(org, (size_t)cap * sizeof *no); if (!no) { objc_release(line); break; } org = no; }
        org[n].x = x;
        org[n].y = baseline;
        n++;
        MSG(void, arr, "addObject:", line);
        objc_release(line);
        if (x + lw > maxw) maxw = x + lw;
        top = baseline - desc - lead - spacing;
        if (last) { first_para_line = 1; prev_after = p.after; } else first_para_line = 0;
        pos = e;
    }
    *origins = org;
    *nout = n;
    *fit_end = pos;
    if (used_w) *used_w = maxw;
    if (used_h) *used_h = H - top - (n ? 0 : 0);
    return 1;
}
static ct_ts_t *ct_fs_ts(id fs, id *ts_out, int *owned) {
    *owned = 0;
    if (fs && object_getClass(fs) == &CTFramesetter_c) { ct_fs_t *f = ct_state(fs); *ts_out = f ? f->ts : 0; }
    else { *ts_out = fs ? CTTypesetterCreateWithAttributedString(fs) : 0; *owned = 1; }   /* compatível: a string direto */
    return *ts_out ? ct_state(*ts_out) : 0;
}
static void ct_clamp(ct_ts_t *t, ct_range_t *r) {
    if (r->loc == 0 && r->len == 0) { r->loc = 0; r->len = t->utf16; }
    if (r->loc < 0) r->loc = 0;
    if (r->len < 0) r->len = 0;
    if (r->loc > t->utf16) r->loc = t->utf16;
    if (r->loc + r->len > t->utf16) r->len = t->utf16 - r->loc;
}
id CTFramesetterCreateFrame(id fs, ct_range_t range, void *path, id attrs) {
    ct_lines_init();
    id ts;
    int owned;
    ct_ts_t *t = ct_fs_ts(fs, &ts, &owned);
    if (!t) return 0;
    ct_clamp(t, &range);
    ct_rect_t box = path ? CGPathGetBoundingBox(path) : (ct_rect_t){0, 0, 1e7, 1e7};
    ct_frame_t *fr = calloc(1, sizeof *fr);
    if (!fr) { if (owned) objc_release(ts); return 0; }
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    long n = 0, end = range.loc;
    ct_pt_t *org = 0;
    double pw = box.w;
    double v;
    if (attrs && ct_num(ct_dict_get(attrs, (id)kCTFramePathWidthAttributeName), &v) && v > 0) pw = v;
    ct_layout(t, ts, range, pw, box.h, path != 0, arr, &org, &n, &end, 0, 0);
    fr->path = path ? objc_retain((id)path) : 0;
    fr->astr = objc_retain(t->astr);
    fr->attrs = attrs ? objc_retain(attrs) : 0;
    fr->lines = objc_retain(arr);
    fr->n = n;
    fr->origins = org;
    fr->x0 = box.x; fr->y0 = box.y;
    fr->range.loc = range.loc;
    fr->range.len = end - range.loc;
    fr->visible = fr->range;
    if (owned) objc_release(ts);
    id o = cx_alloc_instance(&CTFrame_c);
    if (!o) { objc_release(fr->lines); objc_release(fr->path); objc_release(fr->astr); objc_release(fr->attrs); free(fr->origins); free(fr); return 0; }
    *(void **)((u8 *)o + 8) = fr;
    return o;
}
id CTFrameGetFrameAttributes(id frame) { ct_frame_t *f = ct_state(frame); return f ? f->attrs : 0; }
/* tamanho do texto com as restrições (0 ou CGFLOAT_MAX = sem limite) e o que coube */
ct_sz_t CTFramesetterSuggestFrameSizeWithConstraints(id fs, ct_range_t range, id attrs, ct_sz_t constraints, ct_range_t *fit) {
    (void)attrs;
    ct_sz_t z = {0, 0};
    id ts;
    int owned;
    ct_ts_t *t = ct_fs_ts(fs, &ts, &owned);
    if (fit) { fit->loc = 0; fit->len = 0; }
    if (!t) return z;
    ct_clamp(t, &range);
    double W = constraints.w > 0 && constraints.w < 1e7 ? constraints.w : 1e7, H = constraints.h > 0 && constraints.h < 1e7 ? constraints.h : 1e7;
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    ct_pt_t *org = 0;
    long n = 0, end = range.loc;
    double uw = 0, uh = 0;
    ct_layout(t, ts, range, W, H, H < 1e7, arr, &org, &n, &end, &uw, &uh);
    free(org);
    if (owned) objc_release(ts);
    z.w = ceil(uw); z.h = ceil(uh);
    if (fit) { fit->loc = range.loc; fit->len = end - range.loc; }
    return z;
}
