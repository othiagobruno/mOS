/* Views primitivas e contêineres do SwiftUI: Text, Image, Button, Label,
 * Toggle, TextField, TextEditor, ScrollView, List, ForEach, Divider,
 * ProgressView, Menu, Picker, NavigationView, Section, Stepper, Slider,
 * AnyView, TupleView, _ConditionalContent, Alert e LocalizedStringKey.
 * O init guarda os argumentos num sui_node. O body chama a closure de
 * conteúdo uma vez e guarda a view devolvida. Views folha devolvem 0 (Never).
 */
#ifdef SWIFTUI_VIEWS_HOST
#define SUI_HOST 1
#endif
#include "swiftui_model.h"

static void *sui_view_body(void);

#define SUI_VIEW(mn, mc, k, nm) \
    sui_desc mn = { k, nm }; \
    sui_conf mc = { (void *)sui_view_body, "View" };

/* Descritores e conformidades a View. */
SUI_VIEW($s7SwiftUI4TextVMn, $s7SwiftUI4TextVAA4ViewAAMc, SUI_K_TEXT, "Text")
SUI_VIEW($s7SwiftUI5ImageVMn, $s7SwiftUI5ImageVAA4ViewAAMc, SUI_K_IMAGE, "Image")
SUI_VIEW($s7SwiftUI6ButtonVMn, $s7SwiftUI6ButtonVyxGAA4ViewAAMc, SUI_K_BUTTON, "Button")
SUI_VIEW($s7SwiftUI5LabelVMn, $s7SwiftUI5LabelVyxq_GAA4ViewAAMc, SUI_K_LABEL, "Label")
SUI_VIEW($s7SwiftUI6ToggleVMn, $s7SwiftUI6ToggleVyxGAA4ViewAAMc, SUI_K_TOGGLE, "Toggle")
SUI_VIEW($s7SwiftUI9TextFieldVMn, $s7SwiftUI9TextFieldVyxGAA4ViewAAMc, SUI_K_TEXTFIELD, "TextField")
SUI_VIEW($s7SwiftUI10TextEditorVMn, $s7SwiftUI10TextEditorVAA4ViewAAMc, SUI_K_TEXTEDITOR, "TextEditor")
SUI_VIEW($s7SwiftUI10ScrollViewVMn, $s7SwiftUI10ScrollViewVyxGAA0D0AAMc, SUI_K_SCROLL, "ScrollView")
SUI_VIEW($s7SwiftUI7TabViewVMn, $s7SwiftUI7TabViewVyxq_GAA0D0AAMc, SUI_K_TABVIEW, "TabView")
SUI_VIEW($s7SwiftUI4ListVMn, $s7SwiftUI4ListVyxq_GAA4ViewAAMc, SUI_K_LIST, "List")
SUI_VIEW($s7SwiftUI7ForEachVMn, $s7SwiftUI7ForEachVyxq_q0_GAA4ViewA2aER0_rlMc, SUI_K_FOREACH, "ForEach")
SUI_VIEW($s7SwiftUI7DividerVMn, $s7SwiftUI7DividerVAA4ViewAAMc, SUI_K_DIVIDER, "Divider")
SUI_VIEW($s7SwiftUI12ProgressViewVMn, $s7SwiftUI12ProgressViewVyxq_GAA0D0AAMc, SUI_K_PROGRESS, "ProgressView")
SUI_VIEW($s7SwiftUI4MenuVMn, $s7SwiftUI4MenuVyxq_GAA4ViewAAMc, SUI_K_MENU, "Menu")
SUI_VIEW($s7SwiftUI6PickerVMn, $s7SwiftUI6PickerVyxq_q0_GAA4ViewAAMc, SUI_K_PICKER, "Picker")
SUI_VIEW($s7SwiftUI14NavigationViewVMn, $s7SwiftUI14NavigationViewVyxGAA0D0AAMc, SUI_K_NAVVIEW, "NavigationView")
SUI_VIEW($s7SwiftUI7SectionVMn, $s7SwiftUI7SectionVyxq_q0_GAA4ViewA2aERzAaER_AaER0_rlMc, SUI_K_SECTION, "Section")
SUI_VIEW($s7SwiftUI7StepperVMn, $s7SwiftUI7StepperVyxGAA4ViewAAMc, SUI_K_STEPPER, "Stepper")
SUI_VIEW($s7SwiftUI5GroupVMn, $s7SwiftUI5GroupVyxGAA4ViewA2aERzlMc, SUI_K_GROUP, "Group")
SUI_VIEW($s7SwiftUI7AnyViewVMn, $s7SwiftUI7AnyViewVAA0D0AAMc, SUI_K_ANYVIEW, "AnyView")
SUI_VIEW($s7SwiftUI9TupleViewVMn, $s7SwiftUI9TupleViewVyxGAA0D0AAMc, SUI_K_TUPLE, "TupleView")
SUI_VIEW($s7SwiftUI9EmptyViewVMn, $s7SwiftUI9EmptyViewVAA0D0AAMc, SUI_K_EMPTY, "EmptyView")
SUI_VIEW($s7SwiftUI19_ConditionalContentVMn, $s7SwiftUI19_ConditionalContentVyxq_GAA4ViewA2aERzAaER_rlMc, SUI_K_COND, "_ConditionalContent")

sui_desc $s7SwiftUI6SpacerVMn = { SUI_K_SPACER, "Spacer" };
sui_desc $s7SwiftUI6SliderVMn = { SUI_K_SLIDER, "Slider" };
sui_desc $s7SwiftUI14NavigationLinkVMn = { SUI_K_NAVLINK, "NavigationLink" };
sui_desc $s7SwiftUI19_ConditionalContentV7StorageOMn = { SUI_K_ENUM, "_ConditionalContent.Storage" };
sui_desc $s7SwiftUI5AlertV6ButtonVMn = { SUI_K_ALERTBTN, "Alert.Button" };
static sui_desc sui_alert_desc = { SUI_K_ALERT, "Alert" };
static sui_desc sui_lsk_desc = { SUI_K_LSK, "LocalizedStringKey" };
static sui_desc sui_interp_desc = { SUI_K_INTERP, "LocalizedStringKey.StringInterpolation" };

/* Never e Optional também são View; Never não tem body. */
static void *sui_never_body(void) { return 0; }
sui_conf $ss5NeverO7SwiftUI4ViewACMc = { (void *)sui_never_body, "View" };
sui_wtab $ss5NeverO7SwiftUI4ViewACWP = { &$ss5NeverO7SwiftUI4ViewACMc, (void *)sui_never_body };
sui_conf $sxSg7SwiftUI4ViewA2bCRzlMc = { (void *)sui_view_body, "View" };

/* Metadados (N) e tabelas de testemunhas (WP). */
sui_meta $s7SwiftUI4TextVN = { &$s7SwiftUI4TextVMn };
sui_meta $s7SwiftUI5ImageVN = { &$s7SwiftUI5ImageVMn };
sui_meta $s7SwiftUI7AnyViewVN = { &$s7SwiftUI7AnyViewVMn };
sui_meta $s7SwiftUI9EmptyViewVN = { &$s7SwiftUI9EmptyViewVMn };
sui_wtab $s7SwiftUI4TextVAA4ViewAAWP = { &$s7SwiftUI4TextVAA4ViewAAMc, (void *)sui_view_body };
sui_wtab $s7SwiftUI5ImageVAA4ViewAAWP = { &$s7SwiftUI5ImageVAA4ViewAAMc, (void *)sui_view_body };
sui_wtab $s7SwiftUI7AnyViewVAA0D0AAWP = { &$s7SwiftUI7AnyViewVAA0D0AAMc, (void *)sui_view_body };
sui_wtab $s7SwiftUI9EmptyViewVAA0D0AAWP = { &$s7SwiftUI9EmptyViewVAA0D0AAMc, (void *)sui_view_body };

/* Acessores de metadado (Ma). */
void *$s7SwiftUI10TextEditorVMa(void) { return &$s7SwiftUI10TextEditorVMn; }
void *$s7SwiftUI7ForEachVMa(void) { return &$s7SwiftUI7ForEachVMn; }
void *$s7SwiftUI7DividerVMa(void) { return &$s7SwiftUI7DividerVMn; }
void *$s7SwiftUI7SectionVMa(void) { return &$s7SwiftUI7SectionVMn; }
void *$s7SwiftUI9TupleViewVMa(void) { return &$s7SwiftUI9TupleViewVMn; }
void *$s7SwiftUI19_ConditionalContentVMa(void) { return &$s7SwiftUI19_ConditionalContentVMn; }
void *$s7SwiftUI19_ConditionalContentV7StorageOMa(void) { return &$s7SwiftUI19_ConditionalContentV7StorageOMn; }
void *$s7SwiftUI5AlertV6ButtonVMa(void) { return &$s7SwiftUI5AlertV6ButtonVMn; }
void *$s7SwiftUI18LocalizedStringKeyV0D13InterpolationVMa(void) { return &sui_interp_desc; }

static sui_node *sui_node_new(const sui_desc *d) {
    return (sui_node *)sui_new(d, sizeof(sui_node));
}

static sui_fn sui_mkfn(void *fn, void *ctx) { sui_fn f; f.fn = fn; f.ctx = ctx; return f; }

/* String Swift lida por ponteiro (argumento genérico StringProtocol). */
static char *sui_str_at(const uint64_t *s) {
    return s ? sui_strdup2(s[0], s[1]) : sui_cdup("");
}

static const char *sui_lsk_text(const sui_lsk *k) {
    return (k && k->key) ? k->key : "";
}

/* ---- LocalizedStringKey ---- */

static void sui_interp_put(sui_interp *in, const char *s, int n) {
    char *nb;
    if (!in || !s) return;
    if (n < 0) n = (int)strlen(s);
    if (in->len + n + 1 > in->cap) {
        int cap = (in->len + n + 1) * 2;
        nb = (char *)realloc(in->buf, (size_t)cap);
        if (!nb) return;
        in->buf = nb;
        in->cap = cap;
    }
    memcpy(in->buf + in->len, s, (size_t)n);
    in->len += n;
    in->buf[in->len] = 0;
}

void *$s7SwiftUI18LocalizedStringKeyV13stringLiteralACSS_tcfC(uint64_t w0, uint64_t w1) {
    sui_lsk *k = (sui_lsk *)sui_new(&sui_lsk_desc, sizeof *k);
    if (!k) return 0;
    k->key = sui_strdup2(w0, w1);
    return k;
}

void *$s7SwiftUI18LocalizedStringKeyV0D13InterpolationV15literalCapacity18interpolationCountAESi_SitcfC(int64_t cap, int64_t count) {
    sui_interp *in = (sui_interp *)sui_new(&sui_interp_desc, sizeof *in);
    if (!in) return 0;
    if (cap < 0) cap = 0;
    in->cap = (int)cap + 16 * (int)(count > 0 ? count : 0) + 1;
    in->buf = (char *)malloc((size_t)in->cap);
    if (!in->buf) in->cap = 0;
    else in->buf[0] = 0;
    return in;
}

void $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV13appendLiteralyySSF(uint64_t w0, uint64_t w1) {
    sui_interp *in = (sui_interp *)sui_self();
    char tmp[512];
    int n = sui_unstr(w0, w1, tmp, (int)sizeof tmp);
    if (n > 0) sui_interp_put(in, tmp, n);
}

void $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV06appendF0yySSF(uint64_t w0, uint64_t w1) {
    sui_interp *in = (sui_interp *)sui_self();
    char tmp[512];
    int n = sui_unstr(w0, w1, tmp, (int)sizeof tmp);
    if (n >= 0) sui_interp_put(in, tmp, n);
    if (in) in->args++;
}

static int sui_fmt_int(char *o, int64_t v) {
    char t[24];
    int n = 0, i = 0;
    uint64_t u = v < 0 ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
    do { t[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (v < 0) o[i++] = '-';
    while (n) o[i++] = t[--n];
    o[i] = 0;
    return i;
}

static int sui_fmt_double(char *o, double v, int prec) {
    int i = 0, k;
    int64_t ip;
    double frac, scale = 1;
    if (prec < 0) prec = 6;
    if (prec > 9) prec = 9;
    if (v < 0) { o[i++] = '-'; v = -v; }
    for (k = 0; k < prec; k++) scale *= 10;
    v += 0.5 / scale;
    ip = (int64_t)v;
    frac = v - (double)ip;
    i += sui_fmt_int(o + i, ip);
    if (prec > 0) {
        o[i++] = '.';
        for (k = 0; k < prec; k++) {
            int d;
            frac *= 10;
            d = (int)frac;
            o[i++] = (char)('0' + d);
            frac -= d;
        }
    }
    o[i] = 0;
    return i;
}

/* appendInterpolation(_:specifier:): %d/%ld/%i lê Int, %f/%g lê Double, com .N. */
void $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV06appendF0_9specifieryx_SStAA18_FormatSpecifiableRzlF(const void *value, uint64_t s0, uint64_t s1, void *meta) {
    sui_interp *in = (sui_interp *)sui_self();
    char spec[32], out[48];
    int i, prec = -1, n = 0;
    char conv = 'd';
    (void)meta;
    sui_unstr(s0, s1, spec, (int)sizeof spec);
    for (i = 0; spec[i]; i++) {
        if (spec[i] == '.') {
            prec = 0;
            while (spec[i + 1] >= '0' && spec[i + 1] <= '9') prec = prec * 10 + (spec[++i] - '0');
        } else if (spec[i] == 'f' || spec[i] == 'g' || spec[i] == 'e' || spec[i] == 'd' ||
                   spec[i] == 'i' || spec[i] == 'u' || spec[i] == '@') {
            conv = spec[i];
        }
    }
    if (!value) n = 0, out[0] = 0;
    else if (conv == 'f' || conv == 'g' || conv == 'e') n = sui_fmt_double(out, *(const double *)value, prec);
    else n = sui_fmt_int(out, *(const int64_t *)value);
    sui_interp_put(in, out, n);
    if (in) in->args++;
}

void *$s7SwiftUI18LocalizedStringKeyV19stringInterpolationA2C0dG0V_tcfC(sui_interp *in) {
    sui_lsk *k = (sui_lsk *)sui_new(&sui_lsk_desc, sizeof *k);
    if (!k) return 0;
    k->key = sui_cdup(in && in->buf ? in->buf : "");
    k->formatted = in ? in->args > 0 : 0;
    return k;
}

/* ---- Text ---- */

static sui_node *sui_text_new(const char *s) {
    sui_node *t = sui_node_new(&$s7SwiftUI4TextVMn);
    if (!t) return 0;
    t->text = sui_cdup(s);
    return t;
}

static sui_node *sui_clone(const sui_node *src) {
    sui_node *n;
    if (!src) return 0;
    n = (sui_node *)malloc(sizeof *n);
    if (!n) return 0;
    *n = *src;
    n->text = sui_cdup(src->text);
    return n;
}

void *$s7SwiftUI4TextV_9tableName6bundle7commentAcA18LocalizedStringKeyV_SSSgSo8NSBundleCSgs06StaticI0VSgtcfC(sui_lsk *key, uint64_t t0, uint64_t t1, void *bundle) {
    sui_node *t = sui_text_new(sui_lsk_text(key));
    (void)t0; (void)t1;
    if (t) t->native = bundle;
    return t;
}

void *$s7SwiftUI4TextVyACxcSyRzlufC(const uint64_t *s, void *meta, void *wt) {
    sui_node *t = sui_node_new(&$s7SwiftUI4TextVMn);
    (void)meta; (void)wt;
    if (!t) return 0;
    t->text = sui_str_at(s);
    return t;
}

void *$s7SwiftUI4TextVyAC10Foundation16AttributedStringVcfC(void *attr) {
    sui_node *t = sui_text_new("");
    if (t) t->native = attr;
    return t;
}

void *$s7SwiftUI4TextV4boldACyF(void) {
    sui_node *t = sui_clone((sui_node *)sui_self());
    if (t) t->flags |= SUI_F_BOLD;
    return t;
}

void *$s7SwiftUI4TextV4fontyAcA4FontVSgF(void *font) {
    sui_node *t = sui_clone((sui_node *)sui_self());
    if (t) t->font = font;
    return t;
}

/* Font.Weight? chega como (CGFloat, flag de nil). */
void *$s7SwiftUI4TextV10fontWeightyAcA4FontV0E0VSgF(double weight, int is_nil) {
    sui_node *t = sui_clone((sui_node *)sui_self());
    if (!t) return 0;
    if (is_nil & 1) t->flags &= ~(uint64_t)SUI_F_WEIGHT;
    else { t->flags |= SUI_F_WEIGHT; t->num[1] = weight; }
    return t;
}

void *$s7SwiftUI4TextV15foregroundColoryAcA0E0VSgF(void *color) {
    sui_node *t = sui_clone((sui_node *)sui_self());
    if (t) t->color = color;
    return t;
}

void *$s7SwiftUI4TextV15foregroundStyleyACxAA05ShapeE0RzlF(void *style, void *meta, void *wt) {
    sui_node *t = sui_clone((sui_node *)sui_self());
    (void)meta; (void)wt;
    if (t) t->color = style;
    return t;
}

void *$s7SwiftUI4TextV7kerningyAC12CoreGraphics7CGFloatVF(double k) {
    sui_node *t = sui_clone((sui_node *)sui_self());
    if (t) t->num[2] = k;
    return t;
}

void *$s7SwiftUI4TextV9underline_5colorACSb_AA5ColorVSgtF(int on, void *color) {
    sui_node *t = sui_clone((sui_node *)sui_self());
    if (!t) return 0;
    if (on & 1) { t->flags |= SUI_F_UNDERLINE; t->extra = color; }
    else t->flags &= ~(uint64_t)SUI_F_UNDERLINE;
    return t;
}

/* Text + Text: concatena; o estilo vem do primeiro. */
void *$s7SwiftUI4TextV1poiyA2C_ACtFZ(sui_node *a, sui_node *b) {
    const char *sa = (a && a->text) ? a->text : "";
    const char *sb = (b && b->text) ? b->text : "";
    size_t na = strlen(sa), nb = strlen(sb);
    sui_node *t = a ? sui_clone(a) : sui_text_new("");
    char *s;
    if (!t) return 0;
    s = (char *)malloc(na + nb + 1);
    if (!s) return t;
    memcpy(s, sa, na);
    memcpy(s + na, sb, nb + 1);
    free(t->text);
    t->text = s;
    return t;
}

/* ---- Image ---- */

void *$s7SwiftUI5ImageV10systemNameACSS_tcfC(uint64_t w0, uint64_t w1) {
    sui_node *i = sui_node_new(&$s7SwiftUI5ImageVMn);
    if (!i) return 0;
    i->text = sui_strdup2(w0, w1);
    i->flags |= SUI_F_TEMPLATE;
    i->code = 1;
    return i;
}

void *$s7SwiftUI5ImageV_6bundleACSS_So8NSBundleCSgtcfC(uint64_t w0, uint64_t w1, void *bundle) {
    sui_node *i = sui_node_new(&$s7SwiftUI5ImageVMn);
    if (!i) return 0;
    i->text = sui_strdup2(w0, w1);
    i->native = bundle;
    i->code = 2;
    return i;
}

static void *sui_image_native(void *img, int64_t code) {
    sui_node *i = sui_node_new(&$s7SwiftUI5ImageVMn);
    if (!i) return 0;
    i->native = img;
    i->code = code;
    return i;
}

void *$s7SwiftUI5ImageV02nsC0ACSo7NSImageC_tcfC(void *img) { return sui_image_native(img, 3); }
void *$s7SwiftUI5ImageV02uiC0ACSo7UIImageC_tcfC(void *img) { return sui_image_native(img, 4); }

/* num[0..3] guardam o capInsets; num de Text não se mistura com Image. */
void *$s7SwiftUI5ImageV9resizable9capInsets12resizingModeAcA04EdgeF0V_AC08ResizingH0OtF(double top, double leading, double bottom, double trailing, int mode) {
    sui_node *i = sui_clone((sui_node *)sui_self());
    if (!i) return 0;
    i->flags |= SUI_F_RESIZABLE;
    i->num[0] = top; i->num[1] = leading; i->num[2] = bottom; i->num[3] = trailing;
    i->aux[0] = mode & 0xFF;
    return i;
}

void *$s7SwiftUI5ImageV13interpolationyA2C13InterpolationOF(int q) {
    sui_node *i = sui_clone((sui_node *)sui_self());
    if (i) i->aux[1] = q & 0xFF;
    return i;
}

/* TemplateRenderingMode?: template 0, original 1, nil 2. */
void *$s7SwiftUI5ImageV13renderingModeyA2C017TemplateRenderingE0OSgF(int mode) {
    sui_node *i = sui_clone((sui_node *)sui_self());
    if (!i) return 0;
    mode &= 0xFF;
    i->flags &= ~(uint64_t)(SUI_F_TEMPLATE | SUI_F_ORIGINAL);
    if (mode == 0) i->flags |= SUI_F_TEMPLATE;
    else if (mode == 1) i->flags |= SUI_F_ORIGINAL;
    return i;
}

/* ---- Controles ---- */

void *$s7SwiftUI6ButtonV6action5labelACyxGyyc_xyXEtcfC(void *afn, void *actx, void *lfn, void *lctx) {
    sui_node *b = sui_node_new(&$s7SwiftUI6ButtonVMn);
    if (!b) return 0;
    b->action = sui_mkfn(afn, actx);
    b->label = sui_mkfn(lfn, lctx);
    return b;
}

void *$s7SwiftUI6ButtonVA2A4TextVRszrlE_6actionACyAEGAA18LocalizedStringKeyV_yyctcfC(sui_lsk *title, void *afn, void *actx) {
    sui_node *b = sui_node_new(&$s7SwiftUI6ButtonVMn);
    if (!b) return 0;
    b->text = sui_cdup(sui_lsk_text(title));
    b->child = sui_text_new(b->text);
    b->action = sui_mkfn(afn, actx);
    return b;
}

void *$s7SwiftUI6ButtonVA2A4TextVRszrlE_6actionACyAEGqd___yyctcSyRd__lufC(const uint64_t *title, void *afn, void *actx, void *meta, void *wt) {
    sui_node *b = sui_node_new(&$s7SwiftUI6ButtonVMn);
    (void)meta; (void)wt;
    if (!b) return 0;
    b->text = sui_str_at(title);
    b->child = sui_text_new(b->text);
    b->action = sui_mkfn(afn, actx);
    return b;
}

void *$s7SwiftUI5LabelV5title4iconACyxq_GxyXE_q_yXEtcfC(void *tfn, void *tctx, void *ifn, void *ictx) {
    sui_node *l = sui_node_new(&$s7SwiftUI5LabelVMn);
    if (!l) return 0;
    l->content = sui_mkfn(tfn, tctx);
    l->label = sui_mkfn(ifn, ictx);
    return l;
}

void *$s7SwiftUI5LabelVA2A4TextVRszAA5ImageVRs_rlE_06systemE0ACyAeGGAA18LocalizedStringKeyV_SStcfC(sui_lsk *title, uint64_t s0, uint64_t s1) {
    sui_node *l = sui_node_new(&$s7SwiftUI5LabelVMn);
    if (!l) return 0;
    l->text = sui_cdup(sui_lsk_text(title));
    l->child = sui_text_new(l->text);
    l->extra = $s7SwiftUI5ImageV10systemNameACSS_tcfC(s0, s1);
    l->flags |= SUI_F_BUILT;
    return l;
}

void *$s7SwiftUI6ToggleV4isOn5labelACyxGAA7BindingVySbG_xyXEtcfC(void *binding, void *lfn, void *lctx) {
    sui_node *t = sui_node_new(&$s7SwiftUI6ToggleVMn);
    if (!t) return 0;
    t->binding = binding;
    t->label = sui_mkfn(lfn, lctx);
    return t;
}

void *$s7SwiftUI6ToggleVA2A4TextVRszrlE_4isOnACyAEGAA18LocalizedStringKeyV_AA7BindingVySbGtcfC(sui_lsk *title, void *binding) {
    sui_node *t = sui_node_new(&$s7SwiftUI6ToggleVMn);
    if (!t) return 0;
    t->text = sui_cdup(sui_lsk_text(title));
    t->child = sui_text_new(t->text);
    t->binding = binding;
    return t;
}

static sui_node *sui_field(const sui_desc *d, char *title, void *binding, void *efn, void *ectx, void *cfn, void *cctx) {
    sui_node *f = sui_node_new(d);
    if (!f) { free(title); return 0; }
    f->text = title;
    f->binding = binding;
    f->edit = sui_mkfn(efn, ectx);
    f->action = sui_mkfn(cfn, cctx);
    return f;
}

void *$s7SwiftUI9TextFieldVA2A0C0VRszrlE_4text16onEditingChanged0F6CommitACyAEGAA18LocalizedStringKeyV_AA7BindingVySSGySbcyyctcfC(sui_lsk *title, void *binding, void *efn, void *ectx, void *cfn, void *cctx) {
    return sui_field(&$s7SwiftUI9TextFieldVMn, sui_cdup(sui_lsk_text(title)), binding, efn, ectx, cfn, cctx);
}

void *$s7SwiftUI9TextFieldVA2A0C0VRszrlE_4text16onEditingChanged0F6CommitACyAEGqd___AA7BindingVySSGySbcyyctcSyRd__lufC(const uint64_t *title, void *binding, void *efn, void *ectx, void *cfn, void *cctx) {
    return sui_field(&$s7SwiftUI9TextFieldVMn, sui_str_at(title), binding, efn, ectx, cfn, cctx);
}

void *$s7SwiftUI10TextEditorV4textAcA7BindingVySSG_tcfC(void *binding) {
    return sui_field(&$s7SwiftUI10TextEditorVMn, sui_cdup(""), binding, 0, 0, 0, 0);
}

/* ---- Contêineres ---- */

static sui_node *sui_container(const sui_desc *d, void *fn, void *ctx) {
    sui_node *n = sui_node_new(d);
    if (!n) return 0;
    n->content = sui_mkfn(fn, ctx);
    return n;
}

void *$s7SwiftUI10ScrollViewV_15showsIndicators7contentACyxGAA4AxisO3SetV_SbxyXEtcfC(int axes, int shows, void *fn, void *ctx) {
    sui_node *s = sui_container(&$s7SwiftUI10ScrollViewVMn, fn, ctx);
    if (!s) return 0;
    s->code = axes & 0xFF;
    if (shows & 1) s->flags |= SUI_F_INDICATORS;
    return s;
}

/* TabView { ... } sem seleção (SelectionValue == Int): a aba 0 abre; cada filho traz o tabItem. */
void *$s7SwiftUI7TabViewVAASiRszrlE7contentACySiq_Gq_yXE_tcfC(void *fn, void *ctx) {
    sui_node *t = sui_container(&$s7SwiftUI7TabViewVMn, fn, ctx);
    if (t) t->num[0] = 0;
    return t;
}

void *$s7SwiftUI4ListV9selection7contentACyxq_GAA7BindingVyxSgGSg_q_yXEtcfC(void *selection, void *fn, void *ctx) {
    sui_node *l = sui_container(&$s7SwiftUI4ListVMn, fn, ctx);
    if (l) l->binding = selection;
    return l;
}

void *$s7SwiftUI4ListVAAs5NeverORszrlE7contentACyAEq_Gq_yXE_tcfC(void *fn, void *ctx) {
    return sui_container(&$s7SwiftUI4ListVMn, fn, ctx);
}

/* ForEach guarda os dados e o KeyPath do id; a closure monta cada linha. */
void *$s7SwiftUI7ForEachVA2A4ViewR0_rlE_2id7contentACyxq_q0_Gx_s7KeyPathCy7ElementQzq_Gq0_AKctcfC(void *data, void *idpath, void *fn, void *ctx) {
    sui_node *f = sui_container(&$s7SwiftUI7ForEachVMn, fn, ctx);
    if (!f) return 0;
    f->native = data;
    f->extra = idpath;
    return f;
}

void *$s7SwiftUI7ForEachVAA7Element_2IDQZRs_AA4ViewR0_s12IdentifiableADRpzrlE_7contentACyxq_q0_Gx_q0_AIctcfC(void *data, void *fn, void *ctx) {
    sui_node *f = sui_container(&$s7SwiftUI7ForEachVMn, fn, ctx);
    if (f) f->native = data;
    return f;
}

void $s7SwiftUI7ForEachV7contentyq0_7ElementQzcvs(void *fn, void *ctx) {
    sui_node *f = (sui_node *)sui_self();
    if (!f) return;
    f->content = sui_mkfn(fn, ctx);
    f->child = 0;
    f->flags &= ~(uint64_t)SUI_F_BUILT;
}

void *$s7SwiftUI7DividerVACycfC(void) { return sui_node_new(&$s7SwiftUI7DividerVMn); }

/* ProgressView(value:total:) com V = Double: Optional<Double> é 8 bytes + flag. */
void *$s7SwiftUI12ProgressViewV5value5totalACyAA05EmptyD0VAGGqd__Sg_qd__tcAGRszAGRs_SBRd__lufC(const unsigned char *value, const double *total, void *meta, void *wt) {
    sui_node *p = sui_node_new(&$s7SwiftUI12ProgressViewVMn);
    (void)meta; (void)wt;
    if (!p) return 0;
    p->num[1] = total ? *total : 1.0;
    if (!value || (value[8] & 1)) p->flags |= SUI_F_INDETERMINATE;
    else memcpy(&p->num[0], value, sizeof(double));
    if (p->num[1] > 0 && p->num[0] > p->num[1]) p->num[0] = p->num[1];
    if (p->num[0] < 0) p->num[0] = 0;
    return p;
}

void *$s7SwiftUI12ProgressViewVA2A05EmptyD0VRs_rlEACyA2EGycAERszrlufC(void) {
    sui_node *p = sui_node_new(&$s7SwiftUI12ProgressViewVMn);
    if (p) p->flags |= SUI_F_INDETERMINATE;
    return p;
}

void *$s7SwiftUI12ProgressViewVA2A05EmptyD0VRs_rlEyACyAA4TextVAEGAA18LocalizedStringKeyVcAGRszrlufC(sui_lsk *title) {
    sui_node *p = sui_node_new(&$s7SwiftUI12ProgressViewVMn);
    if (!p) return 0;
    p->flags |= SUI_F_INDETERMINATE;
    p->text = sui_cdup(sui_lsk_text(title));
    p->child = sui_text_new(p->text);
    return p;
}

void *$s7SwiftUI4MenuV7content5labelACyxq_Gq_yXE_xyXEtcfC(void *cfn, void *cctx, void *lfn, void *lctx) {
    sui_node *m = sui_container(&$s7SwiftUI4MenuVMn, cfn, cctx);
    if (m) m->label = sui_mkfn(lfn, lctx);
    return m;
}

void *$s7SwiftUI6PickerVA2A4TextVRszrlE_9selection7contentACyAEq_q0_GAA18LocalizedStringKeyV_AA7BindingVyq_Gq0_yXEtcfC(sui_lsk *title, void *selection, void *fn, void *ctx) {
    sui_node *p = sui_container(&$s7SwiftUI6PickerVMn, fn, ctx);
    if (!p) return 0;
    p->text = sui_cdup(sui_lsk_text(title));
    p->binding = selection;
    return p;
}

void *$s7SwiftUI14NavigationViewV7contentACyxGxyXE_tcfC(void *fn, void *ctx) {
    return sui_container(&$s7SwiftUI14NavigationViewVMn, fn, ctx);
}

void *$s7SwiftUI14NavigationLinkV11destination5labelACyxq_Gq__xyXEtcfC(void *dfn, void *dctx, void *lfn, void *lctx) {
    sui_node *n = sui_container(&$s7SwiftUI14NavigationLinkVMn, lfn, lctx);
    if (n) n->label = sui_mkfn(dfn, dctx);
    return n;
}

void *$s7SwiftUI7SectionVA2A4ViewRzAaDR_AA05EmptyD0VRs0_rlE6header7contentACyxq_AFGx_q_yXEtcfC(void *header, void *fn, void *ctx) {
    sui_node *s = sui_container(&$s7SwiftUI7SectionVMn, fn, ctx);
    if (s) s->extra = header;
    return s;
}

/* Stepper e Slider com V = Int/Double: ClosedRange é (lower, upper) por ponteiro. */
void *$s7SwiftUI7StepperVA2A4TextVRszrlE_5value2in4step16onEditingChangedACyAEGAA18LocalizedStringKeyV_AA7BindingVyqd__GSNyqd__G6StrideQyd__ySbctcSxRd__lufC(sui_lsk *title, void *binding, const int64_t *range, const int64_t *step, void *efn, void *ectx) {
    sui_node *s = sui_node_new(&$s7SwiftUI7StepperVMn);
    if (!s) return 0;
    s->text = sui_cdup(sui_lsk_text(title));
    s->binding = binding;
    if (range) { s->num[0] = (double)range[0]; s->num[1] = (double)range[1]; }
    s->num[2] = step ? (double)*step : 1;
    s->edit = sui_mkfn(efn, ectx);
    return s;
}

void *$s7SwiftUI6SliderVA2A9EmptyViewVRszAERs_rlE5value2in4step16onEditingChangedACyA2EGAA7BindingVyqd__G_SNyqd__G6StrideQyd__ySbctcSBRd__SBAPRQlufC(void *binding, const double *range, const double *step, void *efn, void *ectx) {
    sui_node *s = sui_node_new(&$s7SwiftUI6SliderVMn);
    if (!s) return 0;
    s->binding = binding;
    if (range) { s->num[0] = range[0]; s->num[1] = range[1]; }
    else s->num[1] = 1;
    s->num[2] = step ? *step : 0;
    s->edit = sui_mkfn(efn, ectx);
    return s;
}

void *$s7SwiftUI7AnyViewVyACxcAA0D0RzlufC(void *view, void *meta, void *wt) {
    sui_node *a = sui_node_new(&$s7SwiftUI7AnyViewVMn);
    (void)wt;
    if (!a) return 0;
    a->child = view;
    a->native = meta;
    a->flags |= SUI_F_BUILT;
    return a;
}

void *$s7SwiftUI9TupleViewVyACyxGxcfC(void *value) {
    sui_node *t = sui_node_new(&$s7SwiftUI9TupleViewVMn);
    if (!t) return 0;
    t->child = value;
    t->flags |= SUI_F_BUILT;
    return t;
}

/* Storage: { view, caso } com trueContent 0 e falseContent 1. */
void *$s7SwiftUI19_ConditionalContentVA2A4ViewRzAaDR_rlE7storageACyxq_GAC7StorageOyxq__G_tcfC(const uint64_t *storage) {
    sui_node *c = sui_node_new(&$s7SwiftUI19_ConditionalContentVMn);
    if (!c) return 0;
    if (storage) {
        c->child = (void *)(uintptr_t)storage[0];
        c->code = (int64_t)(storage[1] & 1);
    }
    c->flags |= SUI_F_BUILT;
    return c;
}

/* ---- Alert ---- */

void *$s7SwiftUI5AlertV6ButtonV7default_6actionAeA4TextV_yycSgtFZ(void *label, void *fn, void *ctx) {
    sui_node *b = sui_node_new(&$s7SwiftUI5AlertV6ButtonVMn);
    if (!b) return 0;
    b->child = label;
    b->action = sui_mkfn(fn, ctx);
    b->code = 0;
    return b;
}

void *$s7SwiftUI5AlertV5title7message13dismissButtonAcA4TextV_AHSgAC0G0VSgtcfC(void *title, void *message, void *button) {
    sui_node *a = sui_node_new(&sui_alert_desc);
    if (!a) return 0;
    a->child = title;
    a->extra = message;
    a->native = button;
    return a;
}

/* ---- body e ativação ---- */

static void *sui_build(sui_node *n, sui_fn f) {
    if (!(n->flags & SUI_F_BUILT)) {
        n->child = (void *)(uintptr_t)sui_call0r(f);
        n->flags |= SUI_F_BUILT;
    }
    return n->child;
}

static void *sui_view_body(void) {
    sui_node *n = (sui_node *)sui_self();
    switch (sui_kind(n)) {
    case SUI_K_BUTTON:
    case SUI_K_TOGGLE:
        if (n->child) return n->child;
        return sui_build(n, n->label);
    case SUI_K_LABEL:
        if (n->child) return n->child;
        if (!n->extra && n->label.fn) n->extra = (void *)(uintptr_t)sui_call0r(n->label);
        return sui_build(n, n->content);
    case SUI_K_SCROLL: case SUI_K_LIST: case SUI_K_FOREACH: case SUI_K_MENU:
    case SUI_K_PICKER: case SUI_K_NAVVIEW: case SUI_K_NAVLINK: case SUI_K_SECTION:
        return sui_build(n, n->content);
    case SUI_K_ANYVIEW: case SUI_K_TUPLE: case SUI_K_COND: case SUI_K_PROGRESS:
        return n->child;
    default:
        return 0;
    }
}

/* Ponte para o AppKit: clique num Button/Alert.Button chama a ação;
 * num Toggle inverte o Binding. Devolve 1 quando tratou. */
uint64_t $s7SwiftUI7BindingV12wrappedValuexvg(void);
void $s7SwiftUI7BindingV12wrappedValuexvs(uint64_t value);

int mos_sui_activate(void *view) {
    sui_node *n = (sui_node *)view;
    switch (sui_kind(n)) {
    case SUI_K_BUTTON:
    case SUI_K_ALERTBTN:
        if (!n->action.fn) return 0;
        sui_call0(n->action);
        return 1;
    case SUI_K_TOGGLE: {
        sui_fn g, s;
        uint64_t v;
        if (!n->binding) return 0;
        g.fn = (void *)$s7SwiftUI7BindingV12wrappedValuexvg; g.ctx = n->binding;
        s.fn = (void *)$s7SwiftUI7BindingV12wrappedValuexvs; s.ctx = n->binding;
        v = sui_call0r(g);
        sui_call1(s, (v & 1) ? 0 : 1);
        return 1;
    }
    default:
        return 0;
    }
}
