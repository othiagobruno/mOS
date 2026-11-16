/*
 * mOS libSystem — família NSCell do AppKit:
 *   NSCell (valor, título, imagem, fonte, estado, flags, desenho, rastreio do mouse, tamanhos), NSActionCell (alvo e
 *   ação), NSButtonCell (push, checkbox, rádio, toggle), NSTextFieldCell, NSImageCell, NSSliderCell, NSStepperCell,
 *   NSComboBoxCell, NSLevelIndicatorCell; NSControl ganha +cellClass, -cell e -setCell:.
 *   Célula de um controle nativo do mOS (NSButton, NSTextField, NSSlider...) espelha o controle: ler ou mudar o valor,
 *   o título, a fonte ou o estado na célula lê e muda o controle. Num NSControl do app (subclasse direta) com célula,
 *   o controle desenha a célula e repassa o clique a ela, como no AppKit.
 *   NSMatrix: grade de células (rádio, realce, lista, trilha), seleção por clique, shift, ⌘ e teclado.
 *   NSSwitch: interruptor liga/desliga com clique, arrasto e espaço.
 * A geometria e as regras de seleção ficam em appkit_more_core.c (teste userland/tests-appkit/more_host.c).
 * Registro: cx_appkit_cells_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_more_core.h"

id objc_msgSend_stret(id self, SEL op, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
id cx_color(double r, double g, double b, double a);
void NSBeep(void);
extern id NSApp;
const char *sel_getName(SEL s);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
void NSRectFill(R r);
void NSFrameRect(R r);

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(NSSliderCell) K(NSStepperCell) K(NSComboBoxCell) K(NSLevelIndicatorCell) K(NSMatrix) K(NSSwitch)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static R msg_rect(id o, const char *s) { return ((R (*)(id, SEL))objc_msgSend_stret)(o, sl(s)); }
static void redraw(id v) { if (v && responds(v, "setNeedsDisplay:")) MSG(void, v, "setNeedsDisplay:", (BOOL)YES); }
static P m_loc(id view, id ev) {
    P w = ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow"));
    return ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:fromView:"), w, (id)0);
}
static BOOL flipped(id v) { return v && responds(v, "isFlipped") && MSG0(BOOL, v, "isFlipped"); }
static IMP super_imp(const char *cls, SEL s) { return class_getMethodImplementation(kls(cls), s); }

/* ---- propriedades por objeto: dicionário associado (sem limite de objetos) */
static char k_props;
static id props(id o, int make) {
    id d = objc_getAssociatedObject(o, &k_props);
    if (!d && make) { d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary"); objc_setAssociatedObject(o, &k_props, d, 1); }
    return d;
}
static void pset(id o, const char *k, id v) { id d = props(o, 1); if (v) MSG(void, d, "setObject:forKey:", v, cx_str(k)); else MSG(void, d, "removeObjectForKey:", cx_str(k)); }
static id pget(id o, const char *k) { id d = props(o, 0); return d ? MSG(id, d, "objectForKey:", cx_str(k)) : 0; }
static i64 pgi(id o, const char *k, i64 def) { id v = pget(o, k); return v ? MSG0(i64, v, "longLongValue") : def; }
static double pgd(id o, const char *k, double def) { id v = pget(o, k); return v ? MSG0(double, v, "doubleValue") : def; }
static void psi(id o, const char *k, i64 v) { pset(o, k, cx_num_i(v)); }
static void psd(id o, const char *k, double v) { pset(o, k, cx_num_d(v)); }

/* ---- desenho */
static void pmove(id p, double x, double y) { ((void (*)(id, SEL, P))objc_msgSend)(p, sl("moveToPoint:"), (P){x, y}); }
static void pline(id p, double x, double y) { ((void (*)(id, SEL, P))objc_msgSend)(p, sl("lineToPoint:"), (P){x, y}); }
static void fill(R r, double R_, double G, double B, double A) { MSG0(void, cx_color(R_, G, B, A), "setFill"); NSRectFill(r); }
static id path_rr(R r, double rad) { return ((id (*)(id, SEL, R, double, double))objc_msgSend)((id)kls("NSBezierPath"), sl("bezierPathWithRoundedRect:xRadius:yRadius:"), r, rad, rad); }
static id path_oval(R r) { return ((id (*)(id, SEL, R))objc_msgSend)((id)kls("NSBezierPath"), sl("bezierPathWithOvalInRect:"), r); }
static void fill_path(id p, double r, double g, double b, double a) { MSG0(void, cx_color(r, g, b, a), "setFill"); MSG0(void, p, "fill"); }
static void stroke_path(id p, double r, double g, double b, double a) { MSG0(void, cx_color(r, g, b, a), "setStroke"); MSG0(void, p, "stroke"); }
static id sysfont(double sz) { return MSG(id, (id)kls("NSFont"), "systemFontOfSize:", sz); }
static id text_attrs(id font, id color) {
    extern id NSFontAttributeName, NSForegroundColorAttributeName;
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", font ? font : sysfont(13), NSFontAttributeName);
    MSG(void, d, "setObject:forKey:", color ? color : cx_color(0, 0, 0, 1), NSForegroundColorAttributeName);
    return d;
}
static S text_size(id s, id at) { return s ? ((S (*)(id, SEL, id))objc_msgSend)(s, sl("sizeWithAttributes:"), at) : (S){0, 0}; }
/* texto numa caixa: alinhamento NSTextAlignment (0 esquerda, 1 direita, 2 centro, 4 natural), centrado na vertical */
static void draw_text(id s, R box, int align, id at) {
    if (!s || !MSG0(u64, s, "length")) return;
    S z = text_size(s, at);
    double x = box.x + (align == 1 ? box.w - z.w : align == 2 ? (box.w - z.w) / 2 : 0);
    if (x < box.x) x = box.x;
    R r = {x, box.y + (box.h - z.h) / 2, box.w - (x - box.x), z.h};
    ((void (*)(id, SEL, R, id))objc_msgSend)(s, sl("drawInRect:withAttributes:"), r, at);
}
static void draw_image(id img, R r, double alpha) {
    if (!img) return;
    ((void (*)(id, SEL, R, R, u64, double))objc_msgSend)(img, sl("drawInRect:fromRect:operation:fraction:"), r, (R){0, 0, 0, 0}, (u64)2, alpha);
}
static S image_size(id img) { return img ? ((S (*)(id, SEL))objc_msgSend)(img, sl("size")) : (S){0, 0}; }

/* ================================================================ NSCell */
/* o controle retém a célula; a célula não retém o controle (weak no AppKit) */
static char k_cv, k_target, k_cb_ds, k_mx_delegate;
static id cv_of(id self) { return objc_getAssociatedObject(self, &k_cv); }
/* célula de um controle nativo: o valor mora no controle */
static id mirror(id cell) { return pgi(cell, "mirror", 0) ? cv_of(cell) : 0; }
static int mirror_has(id cell, const char *s) { id c = mirror(cell); return c && responds(c, s); }

static id cell_init(id self, SEL c) {
    (void)c;
    psi(self, "type", 1); psi(self, "enabled", 1); psi(self, "alignment", 4); psi(self, "state", 0);
    return self;
}
static id cell_init_text(id self, SEL c, id s) { cell_init(self, c); psi(self, "type", 1); pset(self, "objectValue", s ? s : cx_str("")); return self; }
static id cell_init_image(id self, SEL c, id img) { cell_init(self, c); psi(self, "type", 2); pset(self, "image", img); return self; }
static id cell_init_coder(id self, SEL c, id coder) { (void)coder; return cell_init(self, c); }
static id cell_copy(id self, SEL c, void *zone) {
    (void)c; (void)zone;
    id n = cx_alloc_instance(object_getClass(self));
    id d = props(self, 0);
    if (d) objc_setAssociatedObject(n, &k_props, MSG0(id, d, "mutableCopy"), 1);
    { id nd = props(n, 0); if (nd) objc_release(nd); }   /* mutableCopy +1; a associação já retém */
    pset(n, "mirror", 0);
    return n;
}

static id cell_object(id self, SEL c) { (void)c; if (mirror_has(self, "objectValue")) return MSG0(id, mirror(self), "objectValue"); return pget(self, "objectValue"); }
static void cell_set_object(id self, SEL c, id v) {
    (void)c;
    if (mirror_has(self, "setObjectValue:")) { MSG(void, mirror(self), "setObjectValue:", v); return; }
    id f = pget(self, "formatter");
    if (v && f && responds(f, "stringForObjectValue:") && !MSG(BOOL, v, "isKindOfClass:", (id)kls("NSString"))) { /* guarda o objeto; o texto sai do formatter */ }
    pset(self, "objectValue", v);
    redraw(cv_of(self));
}
static id cell_string(id self, SEL c) {
    (void)c;
    if (mirror_has(self, "stringValue")) return MSG0(id, mirror(self), "stringValue");
    id v = pget(self, "objectValue");
    if (!v) return cx_str("");
    id f = pget(self, "formatter");
    if (f && responds(f, "stringForObjectValue:")) { id s = MSG(id, f, "stringForObjectValue:", v); if (s) return s; }
    if (MSG(BOOL, v, "isKindOfClass:", (id)kls("NSString"))) return v;
    if (MSG(BOOL, v, "isKindOfClass:", (id)kls("NSAttributedString"))) return MSG0(id, v, "string");
    return responds(v, "stringValue") ? MSG0(id, v, "stringValue") : MSG0(id, v, "description");
}
static void cell_set_string(id self, SEL c, id s) {
    (void)c;
    if (mirror_has(self, "setStringValue:")) { MSG(void, mirror(self), "setStringValue:", s); return; }
    id f = pget(self, "formatter");
    if (s && f && responds(f, "getObjectValue:forString:errorDescription:")) {
        id obj = 0;
        if (MSG(BOOL, f, "getObjectValue:forString:errorDescription:", &obj, s, (id *)0)) { cell_set_object(self, 0, obj); return; }
    }
    cell_set_object(self, 0, s ? MSG0(id, s, "copy") : cx_str(""));
}
static double cell_double(id self, SEL c) {
    (void)c;
    if (mirror_has(self, "doubleValue")) return MSG0(double, mirror(self), "doubleValue");
    id v = pget(self, "objectValue");
    return v && responds(v, "doubleValue") ? MSG0(double, v, "doubleValue") : 0;
}
static void cell_set_double(id self, SEL c, double d) {
    (void)c;
    if (mirror_has(self, "setDoubleValue:")) { MSG(void, mirror(self), "setDoubleValue:", d); return; }
    cell_set_object(self, 0, cx_num_d(d));
}
static i64 cell_int(id self, SEL c) { return (i64)cell_double(self, c); }
static int cell_int32(id self, SEL c) { return (int)cell_double(self, c); }
static float cell_float(id self, SEL c) { return (float)cell_double(self, c); }
static BOOL cell_bool(id self, SEL c) { return cell_double(self, c) != 0; }
static void cell_set_int(id self, SEL c, i64 v) { (void)c; if (mirror_has(self, "setIntegerValue:")) { MSG(void, mirror(self), "setIntegerValue:", v); return; } cell_set_object(self, 0, cx_num_i(v)); }
static void cell_set_int32(id self, SEL c, int v) { cell_set_int(self, c, v); }
static void cell_set_float(id self, SEL c, float v) { cell_set_double(self, c, v); }
static void cell_set_bool(id self, SEL c, BOOL v) { cell_set_int(self, c, v ? 1 : 0); }
static void cell_take_string(id self, SEL c, id from) { (void)c; cell_set_string(self, 0, MSG0(id, from, "stringValue")); }
static void cell_take_double(id self, SEL c, id from) { (void)c; cell_set_double(self, 0, MSG0(double, from, "doubleValue")); }
static void cell_take_int(id self, SEL c, id from) { (void)c; cell_set_int(self, 0, MSG0(i64, from, "integerValue")); }
static void cell_take_object(id self, SEL c, id from) { (void)c; cell_set_object(self, 0, MSG0(id, from, "objectValue")); }
static id cell_attr_string(id self, SEL c) {
    (void)c;
    id v = pget(self, "objectValue");
    if (v && MSG(BOOL, v, "isKindOfClass:", (id)kls("NSAttributedString"))) return v;
    id s = cell_string(self, 0);
    return objc_autorelease(MSG(id, MSG0(id, (id)kls("NSAttributedString"), "alloc"), "initWithString:attributes:", s, text_attrs(pget(self, "font"), 0)));
}
static void cell_set_attr_string(id self, SEL c, id a) { (void)c; cell_set_object(self, 0, a); }

/* propriedades simples (com espelho quando o controle tem o mesmo método) */
#define CELL_OBJ(get, set, key) \
    static id c_g_##get(id self, SEL c) { (void)c; if (mirror_has(self, #get)) return MSG0(id, mirror(self), #get); return pget(self, key); } \
    static void c_s_##get(id self, SEL c, id v) { (void)c; if (mirror_has(self, #set ":")) { MSG(void, mirror(self), #set ":", v); return; } pset(self, key, v); redraw(cv_of(self)); }
#define CELL_INT(get, set, key, def) \
    static i64 c_g_##get(id self, SEL c) { (void)c; if (mirror_has(self, #get)) return MSG0(i64, mirror(self), #get); return pgi(self, key, def); } \
    static void c_s_##get(id self, SEL c, i64 v) { (void)c; if (mirror_has(self, #set ":")) { MSG(void, mirror(self), #set ":", v); return; } psi(self, key, v); redraw(cv_of(self)); }
#define CELL_BOOL(get, set, key, def) \
    static BOOL c_g_##get(id self, SEL c) { (void)c; if (mirror_has(self, #get)) return MSG0(BOOL, mirror(self), #get); return pgi(self, key, def) != 0; } \
    static void c_s_##get(id self, SEL c, BOOL v) { (void)c; if (mirror_has(self, #set ":")) { MSG(void, mirror(self), #set ":", v); return; } psi(self, key, v ? 1 : 0); redraw(cv_of(self)); }
#define CELL_DBL(get, set, key, def) \
    static double c_g_##get(id self, SEL c) { (void)c; if (mirror_has(self, #get)) return MSG0(double, mirror(self), #get); return pgd(self, key, def); } \
    static void c_s_##get(id self, SEL c, double v) { (void)c; if (mirror_has(self, #set ":")) { MSG(void, mirror(self), #set ":", v); return; } psd(self, key, v); redraw(cv_of(self)); }

CELL_OBJ(title, setTitle, "title") CELL_OBJ(image, setImage, "image") CELL_OBJ(font, setFont, "font")
CELL_OBJ(formatter, setFormatter, "formatter") CELL_OBJ(representedObject, setRepresentedObject, "representedObject")
CELL_OBJ(menu, setMenu, "menu") CELL_OBJ(placeholderString, setPlaceholderString, "placeholderString")
CELL_OBJ(textColor, setTextColor, "textColor") CELL_OBJ(backgroundColor, setBackgroundColor, "backgroundColor")
CELL_OBJ(alternateTitle, setAlternateTitle, "alternateTitle") CELL_OBJ(alternateImage, setAlternateImage, "alternateImage")
CELL_OBJ(keyEquivalent, setKeyEquivalent, "keyEquivalent") CELL_OBJ(identifier, setIdentifier, "identifier")
CELL_OBJ(placeholderAttributedString, setPlaceholderAttributedString, "placeholderAttributedString")
CELL_INT(state, setState, "state", 0) CELL_INT(alignment, setAlignment, "alignment", 4) CELL_INT(tag, setTag, "tag", 0)
CELL_INT(lineBreakMode, setLineBreakMode, "lineBreakMode", 0) CELL_INT(controlSize, setControlSize, "controlSize", 0)
CELL_INT(focusRingType, setFocusRingType, "focusRingType", 0) CELL_INT(imagePosition, setImagePosition, "imagePosition", 0)
CELL_INT(bezelStyle, setBezelStyle, "bezelStyle", 1) CELL_INT(imageScaling, setImageScaling, "imageScaling", 0)
CELL_INT(highlightsBy, setHighlightsBy, "highlightsBy", 12) CELL_INT(showsStateBy, setShowsStateBy, "showsStateBy", 0)
CELL_INT(keyEquivalentModifierMask, setKeyEquivalentModifierMask, "keyMods", 0) CELL_INT(bezelStyleTF, setBezelStyleTF, "tfBezel", 0)
CELL_INT(baseWritingDirection, setBaseWritingDirection, "writingDir", -1) CELL_INT(controlTint, setControlTint, "tint", 0)
CELL_INT(sendActionOn, setSendActionOn, "sendActionOn", 2)
CELL_BOOL(isEnabled, setEnabled, "enabled", 1) CELL_BOOL(isEditable, setEditable, "editable", 0) CELL_BOOL(isSelectable, setSelectable, "selectable", 0)
CELL_BOOL(isBordered, setBordered, "bordered", 0) CELL_BOOL(isBezeled, setBezeled, "bezeled", 0) CELL_BOOL(isHighlighted, setHighlighted, "highlighted", 0)
CELL_BOOL(isContinuous, setContinuous, "continuous", 0) CELL_BOOL(wraps, setWraps, "wraps", 0) CELL_BOOL(isScrollable, setScrollable, "scrollable", 0)
CELL_BOOL(allowsMixedState, setAllowsMixedState, "mixed", 0) CELL_BOOL(showsFirstResponder, setShowsFirstResponder, "showsFR", 0)
CELL_BOOL(refusesFirstResponder, setRefusesFirstResponder, "refusesFR", 0) CELL_BOOL(truncatesLastVisibleLine, setTruncatesLastVisibleLine, "truncLast", 0)
CELL_BOOL(usesSingleLineMode, setUsesSingleLineMode, "singleLine", 0) CELL_BOOL(sendsActionOnEndEditing, setSendsActionOnEndEditing, "sendsEnd", 0)
CELL_BOOL(allowsEditingTextAttributes, setAllowsEditingTextAttributes, "editAttrs", 0) CELL_BOOL(importsGraphics, setImportsGraphics, "importsGfx", 0)
CELL_BOOL(drawsBackground, setDrawsBackground, "drawsBG", 0) CELL_BOOL(isTransparent, setTransparent, "transparent", 0)
CELL_BOOL(allowsUndo, setAllowsUndo, "allowsUndo", 1) CELL_BOOL(showsBorderOnlyWhileMouseInside, setShowsBorderOnlyWhileMouseInside, "borderInside", 0)
CELL_BOOL(isOpaque, setOpaque, "opaque", 0) CELL_BOOL(acceptsFirstResponderC, setAcceptsFirstResponderC, "afr", 1)
CELL_DBL(minValue, setMinValue, "minValue", 0) CELL_DBL(maxValue, setMaxValue, "maxValue", 1) CELL_DBL(increment, setIncrement, "increment", 1)
CELL_DBL(altIncrementValue, setAltIncrementValue, "altInc", 0) CELL_DBL(knobThickness, setKnobThickness, "knobThickness", 21)
CELL_DBL(warningValue, setWarningValue, "warningValue", 0) CELL_DBL(criticalValue, setCriticalValue, "criticalValue", 0)

static i64 cell_type(id self, SEL c) { (void)c; return pgi(self, "type", 1); }
static void cell_set_type(id self, SEL c, i64 t) { (void)c; psi(self, "type", t); }
static void cell_set_control_view(id self, SEL c, id v) { (void)c; objc_setAssociatedObject(self, &k_cv, v, 0); }
static id cell_target(id self, SEL c) { (void)self; (void)c; return 0; }
static SEL cell_action(id self, SEL c) { (void)self; (void)c; return 0; }
static void cell_set_target_noop(id self, SEL c, id t) { (void)self; (void)c; (void)t; }
static void cell_set_action_noop(id self, SEL c, SEL a) { (void)self; (void)c; (void)a; }
static BOOL cell_accepts_fr(id self, SEL c) { (void)c; return pgi(self, "enabled", 1) && !pgi(self, "refusesFR", 0); }
static i64 cell_next_state(id self, SEL c) { (void)c; i64 s = c_g_state(self, 0); if (pgi(self, "mixed", 0)) return s == 1 ? 0 : s == 0 ? -1 : 1; return s ? 0 : 1; }
static void cell_set_next_state(id self, SEL c) { (void)c; c_s_state(self, 0, cell_next_state(self, 0)); }
static i64 cell_compare(id self, SEL c, id other) { (void)c; return MSG(i64, cell_string(self, 0), "compare:", other ? cell_string(other, 0) : cx_str("")); }
static void cell_set_font_size_ctl(id self, SEL c, u64 size) { (void)c; psi(self, "controlSize", (i64)size); }

/* geometria */
static R cell_drawing_rect(id self, SEL c, R b) {
    (void)c;
    if (pgi(self, "bezeled", 0)) return (R){b.x + 3, b.y + 2, b.w - 6, b.h - 4};
    if (pgi(self, "bordered", 0)) return (R){b.x + 2, b.y + 1, b.w - 4, b.h - 2};
    return (R){b.x + 2, b.y, b.w - 4, b.h};
}
static R cell_title_rect(id self, SEL c, R b) { return cell_drawing_rect(self, c, b); }
static R cell_image_rect(id self, SEL c, R b) {
    (void)c;
    S z = image_size(c_g_image(self, 0));
    if (z.w <= 0 || z.h <= 0) return (R){b.x, b.y, 0, 0};
    double k = 1;
    if (z.w > b.w || z.h > b.h) { double kw = b.w / z.w, kh = b.h / z.h; k = kw < kh ? kw : kh; }
    return (R){b.x + (b.w - z.w * k) / 2, b.y + (b.h - z.h * k) / 2, z.w * k, z.h * k};
}
static S cell_size(id self, SEL c) {
    (void)c;
    S z = {0, 0};
    if (pgi(self, "type", 1) == 2) z = image_size(c_g_image(self, 0));
    else { z = text_size(cell_string(self, 0), text_attrs(c_g_font(self, 0), 0)); z.w += 4; }
    if (pgi(self, "bezeled", 0)) { z.w += 6; z.h += 4; } else if (pgi(self, "bordered", 0)) { z.w += 4; z.h += 2; }
    return z;
}
static S cell_size_for_bounds(id self, SEL c, R b) {
    S z = cell_size(self, c);
    if (pgi(self, "wraps", 0) && z.w > b.w && b.w > 0) {
        double lines = (double)(long)(z.w / b.w + 0.999);
        z.h *= lines; z.w = b.w;
    }
    return z;
}

/* desenho */
static void cell_draw_interior(id self, SEL c, R f, id view) {
    (void)c; (void)view;
    R r = cell_drawing_rect(self, 0, f);
    if (pgi(self, "type", 1) == 2) { draw_image(c_g_image(self, 0), cell_image_rect(self, 0, r), pgi(self, "enabled", 1) ? 1 : 0.5); return; }
    id col = pgi(self, "enabled", 1) ? (c_g_textColor(self, 0) ? c_g_textColor(self, 0) : cx_color(0, 0, 0, 1)) : cx_color(0, 0, 0, 0.35);
    id s = cell_string(self, 0);
    if ((!s || !MSG0(u64, s, "length")) && c_g_placeholderString(self, 0)) { s = c_g_placeholderString(self, 0); col = cx_color(0, 0, 0, 0.3); }
    draw_text(s, r, (int)c_g_alignment(self, 0), text_attrs(c_g_font(self, 0), col));
}
static void cell_draw(id self, SEL c, R f, id view) {
    (void)c;
    if (pgi(self, "drawsBG", 0)) {
        id bg = c_g_backgroundColor(self, 0);
        MSG0(void, bg ? bg : cx_color(1, 1, 1, 1), "setFill"); NSRectFill(f);
    }
    if (pgi(self, "bezeled", 0)) {
        fill(f, 1, 1, 1, 1);
        MSG0(void, cx_color(0.70, 0.70, 0.72, 1), "set"); NSFrameRect(f);
    } else if (pgi(self, "bordered", 0)) { MSG0(void, cx_color(0, 0, 0, 1), "set"); NSFrameRect(f); }
    MSG(void, self, "drawInteriorWithFrame:inView:", f, view);
    if (pgi(self, "highlighted", 0) && pgi(self, "type", 1) != 1) fill(f, 0, 0, 0, 0.12);
}
static void cell_highlight(id self, SEL c, BOOL on, R f, id view) { (void)c; (void)f; psi(self, "highlighted", on); redraw(view); }
static void cell_draw_focus(id self, SEL c, R f, id view) {
    (void)c; (void)self; (void)view;
    id p = path_rr((R){f.x - 2, f.y - 2, f.w + 4, f.h + 4}, 4);
    MSG(void, p, "setLineWidth:", 3.0);
    stroke_path(p, 0.25, 0.55, 1, 0.5);
}
static void cell_draw_focus_mask(id self, SEL c, R f, id view) { (void)self; (void)c; (void)view; fill(f, 0, 0, 0, 1); }
static R cell_focus_bounds(id self, SEL c, R f, id view) { (void)self; (void)c; (void)view; return f; }

/* NSCellHitResult: None 0, ContentArea 1, EditableTextArea 2, TrackableArea 4 */
static u64 cell_hit_test(id self, SEL c, id ev, R f, id view) {
    (void)c;
    P p = m_loc(view, ev);
    if (p.x < f.x || p.y < f.y || p.x > f.x + f.w || p.y > f.y + f.h) return 0;
    u64 r = 1;
    if (pgi(self, "editable", 0)) r |= 2;
    if (pgi(self, "enabled", 1)) r |= 4;
    return r;
}
/* rastreio: o mOS entrega o mouse em mouseDown:/mouseDragged:/mouseUp: separados. Por isso trackMouse: guarda o
 * estado e o controle (cx_cell_track_*) continua o rastreio nos eventos seguintes, com a mesma ordem de chamadas
 * do AppKit: startTrackingAt:, continueTracking:at:inView: e stopTracking:at:inView:mouseIsUp:. */
static BOOL cell_start_tracking(id self, SEL c, P p, id view) { (void)c; (void)p; (void)view; return pgi(self, "continuous", 0) != 0; }
static BOOL cell_continue_tracking(id self, SEL c, P last, P cur, id view) { (void)c; (void)last; (void)cur; (void)view; return pgi(self, "continuous", 0) != 0; }
static void cell_stop_tracking(id self, SEL c, P last, P stop, id view, BOOL up) { (void)self; (void)c; (void)last; (void)stop; (void)view; (void)up; }
static SEL cell_action_of(id cell) { return responds(cell, "action") ? MSG0(SEL, cell, "action") : 0; }
static void cell_send(id cell, id view) {
    SEL a = cell_action_of(cell);
    id t = responds(cell, "target") ? MSG0(id, cell, "target") : 0;
    if (!a) return;
    if (view && responds(view, "sendAction:to:")) MSG(BOOL, view, "sendAction:to:", a, t);
    else MSG(BOOL, NSApp, "sendAction:to:from:", a, t, cell);
}
static BOOL cell_track(id self, SEL c, id ev, R f, id view, BOOL until_up) {
    (void)c;
    P p = m_loc(view, ev);
    psd(self, "tr_x", p.x); psd(self, "tr_y", p.y);
    pset(self, "tr_frame", MSG(id, (id)kls("NSValue"), "valueWithRect:", f));
    psi(self, "tr_until_up", until_up); psi(self, "tracking", 1);
    BOOL cont = MSG(BOOL, self, "startTrackingAt:inView:", p, view);
    psi(self, "tr_cont", cont);
    if (cont && pgi(self, "enabled", 1)) cell_send(self, view);
    return YES;
}
void cx_cell_track_drag(id cell, id ev, id view) {
    if (!pgi(cell, "tracking", 0)) return;
    P last = {pgd(cell, "tr_x", 0), pgd(cell, "tr_y", 0)}, cur = m_loc(view, ev);
    R f = ((R (*)(id, SEL))objc_msgSend_stret)(pget(cell, "tr_frame"), sl("rectValue"));
    int inside = cur.x >= f.x && cur.y >= f.y && cur.x <= f.x + f.w && cur.y <= f.y + f.h;
    if (!pgi(cell, "tr_until_up", 0) && !inside) {
        psi(cell, "tracking", 0);
        MSG(void, cell, "stopTracking:at:inView:mouseIsUp:", last, cur, view, (BOOL)NO);
        MSG(void, cell, "highlight:withFrame:inView:", (BOOL)NO, f, view);
        return;
    }
    if (responds(cell, "setHighlighted:")) { BOOL want = inside; if (pgi(cell, "highlighted", 0) != want) MSG(void, cell, "highlight:withFrame:inView:", want, f, view); }
    BOOL cont = MSG(BOOL, cell, "continueTracking:at:inView:", last, cur, view);
    psd(cell, "tr_x", cur.x); psd(cell, "tr_y", cur.y);
    if (cont && pgi(cell, "continuous", 0) && pgi(cell, "enabled", 1)) cell_send(cell, view);
}
/* devolve 1 se o soltar foi dentro da célula (clique completo) */
int cx_cell_track_up(id cell, id ev, id view) {
    if (!pgi(cell, "tracking", 0)) return 0;
    psi(cell, "tracking", 0);
    P last = {pgd(cell, "tr_x", 0), pgd(cell, "tr_y", 0)}, cur = m_loc(view, ev);
    R f = ((R (*)(id, SEL))objc_msgSend_stret)(pget(cell, "tr_frame"), sl("rectValue"));
    int inside = cur.x >= f.x && cur.y >= f.y && cur.x <= f.x + f.w && cur.y <= f.y + f.h;
    MSG(void, cell, "stopTracking:at:inView:mouseIsUp:", last, cur, view, (BOOL)YES);
    if (pgi(cell, "highlighted", 0)) MSG(void, cell, "highlight:withFrame:inView:", (BOOL)NO, f, view);
    return inside;
}
static void cell_perform_click(id self, SEL c, id sender) {
    (void)c; (void)sender;
    if (!pgi(self, "enabled", 1)) { NSBeep(); return; }
    id view = cv_of(self);
    if (responds(self, "setNextState")) MSG0(void, self, "setNextState");
    cell_send(self, view);
}
static void cell_reset_cursor(id self, SEL c, R f, id view) { (void)self; (void)c; (void)f; (void)view; }
static void cell_calc_draw_info(id self, SEL c, R f) { (void)self; (void)c; (void)f; }
static id cell_field_editor(id self, SEL c, id view) { (void)self; (void)c; return view && responds(view, "window") ? MSG(id, MSG0(id, view, "window"), "fieldEditor:forObject:", (BOOL)YES, view) : 0; }
static void cell_end_editing(id self, SEL c, id text) { (void)c; if (text && responds(text, "string")) cell_set_string(self, 0, MSG0(id, text, "string")); }
static BOOL cell_is_entry_acceptable(id self, SEL c, id s) {
    (void)c;
    id f = pget(self, "formatter");
    if (!f || !responds(f, "getObjectValue:forString:errorDescription:")) return YES;
    id obj = 0;
    return MSG(BOOL, f, "getObjectValue:forString:errorDescription:", &obj, s, (id *)0);
}

/* ---- NSActionCell: alvo, ação e tag próprios */
static id ac_target(id self, SEL c) { (void)c; if (mirror_has(self, "target")) return MSG0(id, mirror(self), "target"); return objc_getAssociatedObject(self, &k_target); }
static void ac_set_target(id self, SEL c, id t) { (void)c; if (mirror_has(self, "setTarget:")) { MSG(void, mirror(self), "setTarget:", t); return; } objc_setAssociatedObject(self, &k_target, t, 0); }
static SEL ac_action(id self, SEL c) { (void)c; if (mirror_has(self, "action")) return MSG0(SEL, mirror(self), "action"); id v = pget(self, "action"); return v ? sl(cx_cstr(v)) : 0; }
static void ac_set_action(id self, SEL c, SEL a) { (void)c; if (mirror_has(self, "setAction:")) { MSG(void, mirror(self), "setAction:", a); return; } pset(self, "action", a ? cx_str(sel_getName(a)) : 0); }

/* ================================================================ NSButtonCell */
/* NSButtonType: MomentaryLight 0, PushOnPushOff 1, Toggle 2, Switch 3 (checkbox), Radio 4, MomentaryChange 5, OnOff 6, MomentaryPushIn 7 */
static void bc_set_type(id self, SEL c, u64 t) {
    (void)c;
    if (mirror_has(self, "setButtonType:")) MSG(void, mirror(self), "setButtonType:", t);
    psi(self, "buttonType", (i64)t);
    if (t == 3 || t == 4) { psi(self, "imagePosition", 2); psi(self, "bordered", 0); }
}
static id bc_init(id self, SEL c) { cell_init(self, c); psi(self, "bordered", 1); psi(self, "bezelStyle", 1); psi(self, "alignment", 2); psi(self, "buttonType", 7); return self; }
static id bc_init_text(id self, SEL c, id s) { bc_init(self, c); pset(self, "title", s ? s : cx_str("")); return self; }
static id bc_init_image(id self, SEL c, id img) { bc_init(self, c); pset(self, "image", img); psi(self, "imagePosition", 1); return self; }
static id bc_title(id self, SEL c) { (void)c; if (mirror_has(self, "title")) return MSG0(id, mirror(self), "title"); id t = pget(self, "title"); return t ? t : cx_str(""); }
static id bc_string(id self, SEL c) { return bc_title(self, c); }
static void bc_set_title(id self, SEL c, id t) { c_s_title(self, c, t); }
static double bc_double(id self, SEL c) { return (double)c_g_state(self, c); }
static void bc_set_double(id self, SEL c, double v) { c_s_state(self, c, (i64)v); }
static id bc_object(id self, SEL c) { return cx_num_i(c_g_state(self, c)); }
static void bc_set_object(id self, SEL c, id v) { c_s_state(self, c, v && responds(v, "integerValue") ? MSG0(i64, v, "integerValue") : 0); }
static S bc_cell_size(id self, SEL c) {
    (void)c;
    i64 t = pgi(self, "buttonType", 7);
    S z = text_size(bc_title(self, 0), text_attrs(c_g_font(self, 0), 0));
    if (t == 3 || t == 4) return (S){z.w + 22, z.h > 18 ? z.h : 18};
    S iz = image_size(c_g_image(self, 0));
    double w = z.w + (iz.w ? iz.w + 4 : 0) + (pgi(self, "bordered", 1) ? 28 : 4);
    double h = (z.h > iz.h ? z.h : iz.h) + (pgi(self, "bordered", 1) ? 8 : 2);
    return (S){w, h < 21 && pgi(self, "bordered", 1) ? 21 : h};
}
static void bc_draw(id self, SEL c, R f, id view) {
    (void)c;
    i64 t = pgi(self, "buttonType", 7), st = c_g_state(self, 0);
    BOOL en = c_g_isEnabled(self, 0), hi = pgi(self, "highlighted", 0);
    id at = text_attrs(c_g_font(self, 0), en ? cx_color(0, 0, 0, 1) : cx_color(0, 0, 0, 0.35));
    id title = (st && c_g_alternateTitle(self, 0) && (t == 1 || t == 2 || t == 5)) ? c_g_alternateTitle(self, 0) : bc_title(self, 0);
    if (t == 3 || t == 4) {
        double sz = 14, y = f.y + (f.h - sz) / 2;
        R box = {f.x + 2, y, sz, sz};
        id p = t == 4 ? path_oval(box) : path_rr(box, 3);
        if (st) fill_path(p, 0.13, 0.45, 0.95, en ? 1 : 0.4); else fill_path(p, 1, 1, 1, 1);
        stroke_path(p, 0.55, 0.55, 0.58, 1);
        if (hi) fill_path(p, 0, 0, 0, 0.15);
        if (st == 1 && t == 4) fill_path(path_oval((R){box.x + 4.5, box.y + 4.5, 5, 5}), 1, 1, 1, 1);
        else if (st == 1) {
            id ck = MSG0(id, (id)kls("NSBezierPath"), "bezierPath");
            int fl = flipped(view);
            double y0 = box.y, h = box.h;
            P a = {box.x + 3.5, fl ? y0 + h * 0.52 : y0 + h * 0.48}, b = {box.x + 6, fl ? y0 + h * 0.75 : y0 + h * 0.25}, d = {box.x + 11, fl ? y0 + h * 0.25 : y0 + h * 0.75};
            pmove(ck, a.x, a.y); pline(ck, b.x, b.y); pline(ck, d.x, d.y);
            MSG(void, ck, "setLineWidth:", 2.0);
            stroke_path(ck, 1, 1, 1, 1);
        } else if (st == -1) fill((R){box.x + 3, box.y + sz / 2 - 1, sz - 6, 2}, 1, 1, 1, 1);
        draw_text(title, (R){f.x + 22, f.y, f.w - 22, f.h}, 0, at);
        return;
    }
    if (pgi(self, "bordered", 1) && !pgi(self, "transparent", 0)) {
        id p = path_rr((R){f.x + 1, f.y + 1, f.w - 2, f.h - 2}, 5);
        BOOL on = hi || (st && (t == 1 || t == 2 || t == 6));
        if (on) fill_path(p, 0.13, 0.45, 0.95, 1); else fill_path(p, 1, 1, 1, 1);
        stroke_path(p, 0.70, 0.70, 0.72, 1);
        if (on) at = text_attrs(c_g_font(self, 0), cx_color(1, 1, 1, 1));
    } else if (hi) fill(f, 0, 0, 0, 0.12);
    if (pgi(self, "transparent", 0)) return;
    id img = (st && c_g_alternateImage(self, 0)) ? c_g_alternateImage(self, 0) : c_g_image(self, 0);
    i64 ip = c_g_imagePosition(self, 0);   /* NoImage 0, ImageOnly 1, Left 2, Right 3, Below 4, Above 5, Overlaps 6, Leading 7, Trailing 8 */
    S iz = image_size(img);
    if (img && (ip == 1 || !title || !MSG0(u64, title, "length"))) {
        double k = iz.h > f.h - 4 && iz.h > 0 ? (f.h - 4) / iz.h : 1;
        draw_image(img, (R){f.x + (f.w - iz.w * k) / 2, f.y + (f.h - iz.h * k) / 2, iz.w * k, iz.h * k}, en ? 1 : 0.5);
        return;
    }
    R tr = f;
    if (img && ip != 0) {
        double k = iz.h > f.h - 4 && iz.h > 0 ? (f.h - 4) / iz.h : 1, iw = iz.w * k;
        S ts = text_size(title, at);
        double total = iw + 4 + ts.w, x0 = f.x + (f.w - total) / 2;
        if (ip == 3 || ip == 8) { draw_image(img, (R){x0 + ts.w + 4, f.y + (f.h - iz.h * k) / 2, iw, iz.h * k}, en ? 1 : 0.5); tr = (R){x0, f.y, ts.w, f.h}; }
        else { draw_image(img, (R){x0, f.y + (f.h - iz.h * k) / 2, iw, iz.h * k}, en ? 1 : 0.5); tr = (R){x0 + iw + 4, f.y, ts.w, f.h}; }
        draw_text(title, tr, 0, at);
        return;
    }
    draw_text(title, (R){f.x + 4, f.y, f.w - 8, f.h}, (int)c_g_alignment(self, 0) == 4 ? 2 : (int)c_g_alignment(self, 0), at);
}
/* clique completo num botão-célula: alterna o estado conforme o tipo e manda a ação */
static void bc_after_click(id cell, id view) {
    i64 t = pgi(cell, "buttonType", 7);
    if (t == 1 || t == 2 || t == 3 || t == 6) cell_set_next_state(cell, 0);
    else if (t == 4) c_s_state(cell, 0, 1);
    cell_send(cell, view);
}
static void bc_perform_click(id self, SEL c, id sender) {
    (void)c; (void)sender;
    if (!c_g_isEnabled(self, 0)) { NSBeep(); return; }
    bc_after_click(self, cv_of(self));
}

/* ================================================================ NSTextFieldCell / NSImageCell */
static id tfc_init(id self, SEL c) { cell_init(self, c); psi(self, "alignment", 4); return self; }
static void tfc_draw(id self, SEL c, R f, id view) {
    (void)c;
    if (pgi(self, "drawsBG", 0) || pgi(self, "bezeled", 0)) {
        id bg = c_g_backgroundColor(self, 0);
        MSG0(void, bg ? bg : cx_color(1, 1, 1, 1), "setFill"); NSRectFill(f);
    }
    if (pgi(self, "bezeled", 0) || pgi(self, "bordered", 0)) { MSG0(void, cx_color(0.70, 0.70, 0.72, 1), "set"); NSFrameRect(f); }
    cell_draw_interior(self, 0, f, view);
}
/* NSImageScaling: ProportionallyDown 0, AxesIndependently 1, None 2, ProportionallyUpOrDown 3 */
static void imc_draw_interior(id self, SEL c, R f, id view) {
    (void)c; (void)view;
    id img = c_g_image(self, 0);
    S z = image_size(img);
    if (!img || z.w <= 0 || z.h <= 0) return;
    i64 sc = c_g_imageScaling(self, 0);
    double w = z.w, h = z.h;
    if (sc == 1) { w = f.w; h = f.h; }
    else if (sc == 3 || (sc == 0 && (z.w > f.w || z.h > f.h))) { double k = f.w / z.w < f.h / z.h ? f.w / z.w : f.h / z.h; w = z.w * k; h = z.h * k; }
    draw_image(img, (R){f.x + (f.w - w) / 2, f.y + (f.h - h) / 2, w, h}, c_g_isEnabled(self, 0) ? 1 : 0.5);
}
static void imc_draw(id self, SEL c, R f, id view) { (void)c; imc_draw_interior(self, 0, f, view); }

/* ================================================================ NSSliderCell */
static id slc_init(id self, SEL c) { cell_init(self, c); psd(self, "minValue", 0); psd(self, "maxValue", 1); psi(self, "continuous", 1); pset(self, "objectValue", cx_num_d(0)); return self; }
static BOOL slc_vertical(id self) { id v = pget(self, "vertical"); if (v) return MSG0(i64, v, "longLongValue") == 1; return NO; }
static R slc_track(id self, R f) { return slc_vertical(self) ? (R){f.x + f.w / 2 - 2, f.y + 10, 4, f.h - 20} : (R){f.x + 10, f.y + f.h / 2 - 2, f.w - 20, 4}; }
static R slc_track_rect(id self, SEL c) {
    (void)c;
    id fv = pget(self, "lastFrameV");
    return slc_track(self, fv ? ((R (*)(id, SEL))objc_msgSend_stret)(fv, sl("rectValue")) : (R){0, 0, 100, 21});
}
static R slc_knob_rect(id self, SEL c, BOOL flip) {
    (void)c; (void)flip;
    id fv = pget(self, "lastFrameV");
    R f = fv ? ((R (*)(id, SEL))objc_msgSend_stret)(fv, sl("rectValue")) : (R){0, 0, 100, 21};
    R t = slc_track(self, f);
    double v = cell_double(self, 0), mn = c_g_minValue(self, 0), mx = c_g_maxValue(self, 0);
    if (slc_vertical(self)) { double y = sl_knob_pos(v, t.y, t.h, mn, mx); return (R){f.x + f.w / 2 - 9, y - 9, 18, 18}; }
    double x = sl_knob_pos(v, t.x, t.w, mn, mx);
    return (R){x - 9, f.y + f.h / 2 - 9, 18, 18};
}
static i64 slc_ticks(id self, SEL c) { (void)c; return pgi(self, "ticks", 0); }
static void slc_set_ticks(id self, SEL c, i64 n) { (void)c; psi(self, "ticks", n); redraw(cv_of(self)); }
static BOOL slc_only_ticks(id self, SEL c) { (void)c; return pgi(self, "onlyTicks", 0) != 0; }
static void slc_set_only_ticks(id self, SEL c, BOOL b) { (void)c; psi(self, "onlyTicks", b); }
static double slc_tick_value(id self, SEL c, i64 i) { (void)c; i64 n = pgi(self, "ticks", 0); double mn = c_g_minValue(self, 0), mx = c_g_maxValue(self, 0); return n <= 1 ? (mn + mx) / 2 : mn + i * (mx - mn) / (n - 1); }
static double slc_closest_tick(id self, SEL c, double v) { (void)c; return sl_closest_tick(v, c_g_minValue(self, 0), c_g_maxValue(self, 0), (int)pgi(self, "ticks", 0)); }
static u64 slc_tick_pos(id self, SEL c) { (void)c; return (u64)pgi(self, "tickPos", 0); }
static void slc_set_tick_pos(id self, SEL c, u64 p) { (void)c; psi(self, "tickPos", (i64)p); }
static i64 slc_is_vertical(id self, SEL c) { (void)c; return slc_vertical(self) ? 1 : 0; }
static void slc_set_vertical(id self, SEL c, BOOL v) { (void)c; psi(self, "vertical", v ? 1 : 0); }
static u64 slc_type(id self, SEL c) { (void)c; return (u64)pgi(self, "sliderType", 0); }
static void slc_set_type(id self, SEL c, u64 t) { (void)c; psi(self, "sliderType", (i64)t); }
static void slc_draw(id self, SEL c, R f, id view) {
    (void)c; (void)view;
    pset(self, "lastFrameV", MSG(id, (id)kls("NSValue"), "valueWithRect:", f));
    R t = slc_track(self, f);
    fill_path(path_rr(t, 2), 0.82, 0.82, 0.84, 1);
    double v = cell_double(self, 0), mn = c_g_minValue(self, 0), mx = c_g_maxValue(self, 0);
    i64 n = pgi(self, "ticks", 0);
    for (i64 i = 0; i < n; i++) {
        double tv = slc_tick_value(self, 0, i);
        if (slc_vertical(self)) fill((R){t.x + 8, sl_knob_pos(tv, t.y, t.h, mn, mx) - 0.5, 4, 1}, 0.5, 0.5, 0.5, 1);
        else fill((R){sl_knob_pos(tv, t.x, t.w, mn, mx) - 0.5, t.y - 7, 1, 4}, 0.5, 0.5, 0.5, 1);
    }
    if (slc_vertical(self)) fill_path(path_rr((R){t.x, t.y, t.w, sl_knob_pos(v, t.y, t.h, mn, mx) - t.y}, 2), 0.13, 0.45, 0.95, 1);
    else fill_path(path_rr((R){t.x, t.y, sl_knob_pos(v, t.x, t.w, mn, mx) - t.x, t.h}, 2), 0.13, 0.45, 0.95, 1);
    R k = slc_knob_rect(self, 0, NO);
    id kp = path_oval(k);
    fill_path(kp, 1, 1, 1, 1);
    stroke_path(kp, 0.6, 0.6, 0.62, 1);
}
static void slc_set_from_point(id self, P p, R f) {
    R t = slc_track(self, f);
    double mn = c_g_minValue(self, 0), mx = c_g_maxValue(self, 0);
    double v = slc_vertical(self) ? sl_value_at(p.y, t.y, t.h, mn, mx) : sl_value_at(p.x, t.x, t.w, mn, mx);
    if (pgi(self, "onlyTicks", 0)) v = sl_closest_tick(v, mn, mx, (int)pgi(self, "ticks", 0));
    cell_set_double(self, 0, v);
}
static BOOL slc_start(id self, SEL c, P p, id view) {
    (void)c;
    id fv = pget(self, "lastFrameV");
    R f = fv ? ((R (*)(id, SEL))objc_msgSend_stret)(fv, sl("rectValue")) : msg_rect(view, "bounds");
    slc_set_from_point(self, p, f);
    redraw(view);
    return YES;
}
static BOOL slc_continue(id self, SEL c, P last, P cur, id view) { (void)last; return slc_start(self, c, cur, view); }
static S slc_cell_size(id self, SEL c) { (void)c; return slc_vertical(self) ? (S){21, 100} : (S){100, 21}; }

/* ================================================================ NSStepperCell */
static id stc_init(id self, SEL c) { cell_init(self, c); psd(self, "minValue", 0); psd(self, "maxValue", 59); psd(self, "increment", 1); psi(self, "autorepeat", 1); pset(self, "objectValue", cx_num_d(0)); return self; }
static BOOL stc_wraps(id self, SEL c) { (void)c; return pgi(self, "valueWraps", 0) != 0; }
static void stc_set_wraps(id self, SEL c, BOOL b) { (void)c; psi(self, "valueWraps", b); }
static BOOL stc_autorepeat(id self, SEL c) { (void)c; return pgi(self, "autorepeat", 1) != 0; }
static void stc_set_autorepeat(id self, SEL c, BOOL b) { (void)c; psi(self, "autorepeat", b); }
static void stc_draw(id self, SEL c, R f, id view) {
    (void)c;
    R b = {f.x + (f.w - 13) / 2, f.y + 1, 13, f.h - 2};
    if (b.h > 23) { b.y += (b.h - 23) / 2; b.h = 23; }
    id p = path_rr(b, 4);
    fill_path(p, 1, 1, 1, 1); stroke_path(p, 0.70, 0.70, 0.72, 1);
    fill((R){b.x + 2, b.y + b.h / 2, b.w - 4, 0.5}, 0.70, 0.70, 0.72, 1);
    i64 hi = pgi(self, "hiPart", 0);   /* 1 = cima, 2 = baixo */
    int fl = flipped(view);
    for (int up = 0; up < 2; up++) {
        double cy = (up ^ fl) ? b.y + b.h * 0.72 : b.y + b.h * 0.28;
        if (hi == (up ? 1 : 2)) fill((R){b.x + 1, (up ^ fl) ? b.y + b.h / 2 : b.y + 1, b.w - 2, b.h / 2 - 1}, 0, 0, 0, 0.12);
        id tri = MSG0(id, (id)kls("NSBezierPath"), "bezierPath");
        double dy = (up ^ fl) ? 2 : -2;
        pmove(tri, b.x + 3.5, cy - dy);
        pline(tri, b.x + b.w - 3.5, cy - dy);
        pline(tri, b.x + b.w / 2, cy + dy);
        MSG0(void, tri, "closePath");
        fill_path(tri, 0.25, 0.25, 0.27, c_g_isEnabled(self, 0) ? 1 : 0.4);
    }
}
static BOOL stc_start(id self, SEL c, P p, id view) {
    (void)c;
    id fv = pget(self, "tr_frame");
    R f = fv ? ((R (*)(id, SEL))objc_msgSend_stret)(fv, sl("rectValue")) : msg_rect(view, "bounds");
    int up = (p.y >= f.y + f.h / 2) ^ flipped(view);
    psi(self, "hiPart", up ? 1 : 2);
    double v = st_step(cell_double(self, 0), c_g_increment(self, 0), c_g_minValue(self, 0), c_g_maxValue(self, 0), up, (int)pgi(self, "valueWraps", 0));
    cell_set_double(self, 0, v);
    redraw(view);
    return YES;   /* manda a ação no clique, como o NSStepper */
}
static void stc_stop(id self, SEL c, P a, P b, id view, BOOL up) { (void)c; (void)a; (void)b; (void)up; psi(self, "hiPart", 0); redraw(view); }
static S stc_cell_size(id self, SEL c) { (void)self; (void)c; return (S){19, 28}; }

/* ================================================================ NSComboBoxCell */
static id cbc_init(id self, SEL c) { tfc_init(self, c); psi(self, "editable", 1); psi(self, "bezeled", 1); pset(self, "items", MSG0(id, (id)kls("NSMutableArray"), "array")); psi(self, "visibleItems", 5); psi(self, "sel", -1); psd(self, "itemHeight", 16); return self; }
static id cbc_items(id self) { id a = pget(self, "items"); if (!a) { a = MSG0(id, (id)kls("NSMutableArray"), "array"); pset(self, "items", a); } return a; }
static id cbc_source(id self) { return pgi(self, "usesDS", 0) ? objc_getAssociatedObject(self, &k_cb_ds) : 0; }
static i64 cbc_count(id self, SEL c) {
    (void)c;
    id ds = cbc_source(self);
    if (ds && responds(ds, "numberOfItemsInComboBoxCell:")) return MSG(i64, ds, "numberOfItemsInComboBoxCell:", self);
    return (i64)MSG0(u64, cbc_items(self), "count");
}
static id cbc_item_at(id self, SEL c, i64 i) {
    (void)c;
    id ds = cbc_source(self);
    if (ds && responds(ds, "comboBoxCell:objectValueForItemAtIndex:")) return MSG(id, ds, "comboBoxCell:objectValueForItemAtIndex:", self, i);
    id a = cbc_items(self);
    return i >= 0 && (u64)i < MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)i) : 0;
}
static void cbc_add(id self, SEL c, id o) { (void)c; if (o) MSG(void, cbc_items(self), "addObject:", o); }
static void cbc_add_many(id self, SEL c, id arr) { (void)c; if (arr) MSG(void, cbc_items(self), "addObjectsFromArray:", arr); }
static void cbc_insert(id self, SEL c, id o, i64 i) { (void)c; if (o) MSG(void, cbc_items(self), "insertObject:atIndex:", o, (u64)i); }
static void cbc_remove(id self, SEL c, id o) { (void)c; MSG(void, cbc_items(self), "removeObject:", o); }
static void cbc_remove_at(id self, SEL c, i64 i) { (void)c; MSG(void, cbc_items(self), "removeObjectAtIndex:", (u64)i); }
static void cbc_remove_all(id self, SEL c) { (void)c; MSG0(void, cbc_items(self), "removeAllObjects"); psi(self, "sel", -1); }
static id cbc_values(id self, SEL c) { (void)c; return MSG0(id, cbc_items(self), "copy"); }
static i64 cbc_index_of(id self, SEL c, id o) {
    (void)c;
    i64 n = cbc_count(self, 0);
    for (i64 i = 0; i < n; i++) { id it = cbc_item_at(self, 0, i); if (it && MSG(BOOL, it, "isEqual:", o)) return i; }
    return (i64)0x7fffffffffffffffLL;   /* NSNotFound */
}
static i64 cbc_selected(id self, SEL c) { (void)c; return pgi(self, "sel", -1); }
static void cbc_select(id self, SEL c, i64 i) {
    (void)c;
    psi(self, "sel", i);
    id v = cbc_item_at(self, 0, i);
    if (v) cell_set_object(self, 0, v);
}
static void cbc_deselect(id self, SEL c, i64 i) { (void)c; if (pgi(self, "sel", -1) == i) psi(self, "sel", -1); }
static id cbc_obj_selected(id self, SEL c) { (void)c; i64 i = pgi(self, "sel", -1); return i >= 0 ? cbc_item_at(self, 0, i) : 0; }
static id cbc_ds(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_cb_ds); }
static void cbc_set_ds(id self, SEL c, id d) { (void)c; objc_setAssociatedObject(self, &k_cb_ds, d, 0); }
static BOOL cbc_uses_ds(id self, SEL c) { (void)c; return pgi(self, "usesDS", 0) != 0; }
static void cbc_set_uses_ds(id self, SEL c, BOOL b) { (void)c; psi(self, "usesDS", b); }
static i64 cbc_visible(id self, SEL c) { (void)c; return pgi(self, "visibleItems", 5); }
static void cbc_set_visible(id self, SEL c, i64 n) { (void)c; psi(self, "visibleItems", n); }
static double cbc_item_h(id self, SEL c) { (void)c; return pgd(self, "itemHeight", 16); }
static void cbc_set_item_h(id self, SEL c, double h) { (void)c; psd(self, "itemHeight", h); }
static BOOL cbc_has_scroller(id self, SEL c) { (void)c; return pgi(self, "scroller", 1) != 0; }
static void cbc_set_has_scroller(id self, SEL c, BOOL b) { (void)c; psi(self, "scroller", b); }
static BOOL cbc_completes(id self, SEL c) { (void)c; return pgi(self, "completes", 0) != 0; }
static void cbc_set_completes(id self, SEL c, BOOL b) { (void)c; psi(self, "completes", b); }
static id cbc_complete(id self, SEL c, id prefix) {
    (void)c;
    id ds = cbc_source(self);
    if (ds && responds(ds, "comboBoxCell:completedString:")) return MSG(id, ds, "comboBoxCell:completedString:", self, prefix);
    i64 n = cbc_count(self, 0);
    for (i64 i = 0; i < n; i++) {
        id it = cbc_item_at(self, 0, i);
        id s = it && responds(it, "stringValue") ? MSG0(id, it, "stringValue") : it;
        if (s && MSG(BOOL, s, "isKindOfClass:", (id)kls("NSString")) && MSG(BOOL, MSG0(id, s, "lowercaseString"), "hasPrefix:", MSG0(id, prefix, "lowercaseString"))) return s;
    }
    return 0;
}
static void cbc_note(id self, SEL c) { (void)self; (void)c; }
static void cbc_draw(id self, SEL c, R f, id view) {
    (void)c;
    R tf = {f.x, f.y, f.w - 20, f.h};
    tfc_draw(self, 0, tf, view);
    R btn = {f.x + f.w - 20, f.y, 20, f.h};
    fill_path(path_rr(btn, 4), 0.13, 0.45, 0.95, 1);
    id tri = MSG0(id, (id)kls("NSBezierPath"), "bezierPath");
    int fl = flipped(view);
    double cy = btn.y + btn.h / 2, d = fl ? -2 : 2;
    pmove(tri, btn.x + 6, cy + d); pline(tri, btn.x + 14, cy + d);
    pline(tri, btn.x + 10, cy - d); MSG0(void, tri, "closePath");
    fill_path(tri, 1, 1, 1, 1);
}

/* ================================================================ NSLevelIndicatorCell */
/* NSLevelIndicatorStyle: Relevancy 0, ContinuousCapacity 1, DiscreteCapacity 2, Rating 3 */
static id lic_init_style(id self, SEL c, u64 st) { cell_init(self, c); psi(self, "liStyle", (i64)st); psd(self, "minValue", 0); psd(self, "maxValue", st == 3 ? 5 : 1); pset(self, "objectValue", cx_num_d(0)); return self; }
static id lic_init(id self, SEL c) { return lic_init_style(self, c, 0); }
static u64 lic_style(id self, SEL c) { (void)c; return (u64)pgi(self, "liStyle", 0); }
static void lic_set_style(id self, SEL c, u64 s) { (void)c; psi(self, "liStyle", (i64)s); }
static void lic_draw(id self, SEL c, R f, id view) {
    (void)c; (void)view;
    double v = cell_double(self, 0), mn = c_g_minValue(self, 0), mx = c_g_maxValue(self, 0);
    double frac = mx > mn ? (v - mn) / (mx - mn) : 0;
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    i64 st = pgi(self, "liStyle", 0);
    double warn = c_g_warningValue(self, 0), crit = c_g_criticalValue(self, 0);
    double r = 0.2, g = 0.75, b = 0.3;
    if (crit > 0 && v >= crit) { r = 0.9; g = 0.2; b = 0.2; } else if (warn > 0 && v >= warn) { r = 0.95; g = 0.75; b = 0.1; }
    if (st == 3) {
        int n = (int)(mx + 0.5);
        for (int i = 0; i < n; i++) {
            R s = {f.x + i * 13, f.y + (f.h - 11) / 2, 11, 11};
            fill_path(path_oval(s), i < v ? 0.25 : 0.85, i < v ? 0.25 : 0.85, i < v ? 0.27 : 0.86, 1);
        }
        return;
    }
    if (st == 2) {
        int n = mx - mn > 0 && mx - mn < 64 ? (int)(mx - mn) : 10;
        double w = (f.w - (n - 1) * 2) / n;
        for (int i = 0; i < n; i++) {
            R s = {f.x + i * (w + 2), f.y + 2, w, f.h - 4};
            if (i < frac * n) fill(s, r, g, b, 1); else fill(s, 0.88, 0.88, 0.9, 1);
        }
        return;
    }
    fill_path(path_rr((R){f.x, f.y + 2, f.w, f.h - 4}, 3), 0.88, 0.88, 0.9, 1);
    if (frac > 0) fill_path(path_rr((R){f.x, f.y + 2, f.w * frac, f.h - 4}, 3), st == 0 ? 0.45 : r, st == 0 ? 0.45 : g, st == 0 ? 0.47 : b, 1);
}

/* ================================================================ NSControl: célula */
static char k_cell, k_cellclass;
static int builtin_name(const char *n) {
    static const char *b[] = {"NSButton", "NSTextField", "NSSecureTextField", "NSSearchField", "NSTokenField", "NSComboBox", "NSSlider",
                              "NSStepper", "NSLevelIndicator", "NSPopUpButton", "NSSegmentedControl", "NSColorWell", "NSDatePicker",
                              "NSImageView", "NSPathControl", "NSMatrix", "NSSwitch", "NSTableView", "NSOutlineView", "NSBrowser", 0};
    for (int i = 0; b[i]; i++) if (!strcmp(n, b[i])) return 1;
    return 0;
}
/* primeira classe do sistema na cadeia do controle (pula as subclasses do app) */
static const char *system_base(id self) {
    for (cx_class_t *k = object_getClass(self); k; k = k->super) {
        const char *n = cx_class_name(k);
        if (n[0] == 'N' && n[1] == 'S') return n;
    }
    return "";
}
static const char *default_cell_for(const char *base) {
    if (!strcmp(base, "NSButton")) return "NSButtonCell";
    if (!strcmp(base, "NSTextField") || !strcmp(base, "NSSecureTextField") || !strcmp(base, "NSTokenField")) return "NSTextFieldCell";
    if (!strcmp(base, "NSSearchField")) return "NSSearchFieldCell";
    if (!strcmp(base, "NSComboBox")) return "NSComboBoxCell";
    if (!strcmp(base, "NSSlider")) return "NSSliderCell";
    if (!strcmp(base, "NSStepper")) return "NSStepperCell";
    if (!strcmp(base, "NSLevelIndicator")) return "NSLevelIndicatorCell";
    if (!strcmp(base, "NSImageView")) return "NSImageCell";
    if (!strcmp(base, "NSPopUpButton")) return "NSPopUpButtonCell";
    if (!strcmp(base, "NSSegmentedControl")) return "NSSegmentedCell";
    if (!strcmp(base, "NSDatePicker")) return "NSDatePickerCell";
    if (!strcmp(base, "NSPathControl")) return "NSPathCell";
    return 0;
}
static id ctl_cell_class(id k, SEL c) {
    (void)c;
    id own = objc_getAssociatedObject(k, &k_cellclass);
    if (own) return own;
    const char *n = default_cell_for(cx_class_name((cx_class_t *)k));
    return n && kls(n) ? (id)kls(n) : 0;
}
static void ctl_set_cell_class(id k, SEL c, id cellk) { (void)c; objc_setAssociatedObject(k, &k_cellclass, cellk, 1); }
static id ctl_cell(id self, SEL c) {
    (void)c;
    id cell = objc_getAssociatedObject(self, &k_cell);
    if (cell) return cell;
    id ck = MSG0(id, (id)object_getClass(self), "cellClass");
    if (!ck) { const char *n = default_cell_for(system_base(self)); ck = n && kls(n) ? (id)kls(n) : (id)kls("NSActionCell"); }
    cell = MSG0(id, MSG0(id, ck, "alloc"), "init");
    if (!cell) return 0;
    MSG(void, cell, "setControlView:", self);
    /* controle nativo: a célula lê e escreve no controle */
    if (builtin_name(system_base(self))) psi(cell, "mirror", 1), pset(cell, "controlView", self);
    objc_setAssociatedObject(self, &k_cell, cell, 1);
    objc_release(cell);
    return cell;
}
static void ctl_set_cell(id self, SEL c, id cell) {
    (void)c;
    objc_setAssociatedObject(self, &k_cell, cell, 1);
    if (cell) {
        MSG(void, cell, "setControlView:", self);
        if (builtin_name(system_base(self))) psi(cell, "mirror", 1), pset(cell, "controlView", self);
    }
    redraw(self);
}
static id ctl_selected_cell(id self, SEL c) { return ctl_cell(self, c); }
static i64 ctl_selected_tag(id self, SEL c) { (void)c; id cell = ctl_cell(self, 0); return cell ? MSG0(i64, cell, "tag") : -1; }
static void ctl_update_cell(id self, SEL c, id cell) { (void)c; (void)cell; redraw(self); }
static void ctl_update_cell_inside(id self, SEL c, id cell) { (void)c; (void)cell; redraw(self); }
static void ctl_draw_cell(id self, SEL c, id cell) { (void)c; (void)cell; redraw(self); }
static void ctl_select_cell(id self, SEL c, id cell) { (void)c; (void)cell; redraw(self); }
static void ctl_calc_size(id self, SEL c) { (void)self; (void)c; }

/* NSControl do app (subclasse direta de NSControl) com célula: o controle desenha a célula e repassa o mouse */
static int app_cell_control(id self) { return !strcmp(system_base(self), "NSControl") && objc_getAssociatedObject(self, &k_cell) != 0; }
static int app_cell_capable(id self) { return !strcmp(system_base(self), "NSControl") && (objc_getAssociatedObject(self, &k_cell) || MSG0(id, (id)object_getClass(self), "cellClass")); }
static IMP old_ctl_draw, old_ctl_down, old_ctl_drag, old_ctl_up;
static void ctl_draw_rect(id self, SEL c, R r) {
    if (app_cell_capable(self)) {
        id cell = ctl_cell(self, 0);
        if (cell) { MSG(void, cell, "drawWithFrame:inView:", msg_rect(self, "bounds"), self); return; }
    }
    if (old_ctl_draw) ((void (*)(id, SEL, R))old_ctl_draw)(self, c, r);
}
static void ctl_mouse_down2(id self, SEL c, id ev) {
    if (app_cell_capable(self)) {
        id cell = ctl_cell(self, 0);
        if (cell && MSG0(BOOL, cell, "isEnabled")) {
            R b = msg_rect(self, "bounds");
            MSG(void, cell, "highlight:withFrame:inView:", (BOOL)YES, b, self);
            MSG(BOOL, cell, "trackMouse:inRect:ofView:untilMouseUp:", ev, b, self, (BOOL)NO);
        }
        return;
    }
    if (old_ctl_down) ((void (*)(id, SEL, id))old_ctl_down)(self, c, ev);
}
static void ctl_mouse_dragged2(id self, SEL c, id ev) {
    if (app_cell_control(self)) { cx_cell_track_drag(ctl_cell(self, 0), ev, self); return; }
    if (old_ctl_drag) ((void (*)(id, SEL, id))old_ctl_drag)(self, c, ev);
}
static void ctl_mouse_up2(id self, SEL c, id ev) {
    if (app_cell_control(self)) {
        id cell = ctl_cell(self, 0);
        if (cx_cell_track_up(cell, ev, self)) {
            if (MSG(BOOL, cell, "isKindOfClass:", (id)kls("NSButtonCell"))) bc_after_click(cell, self);
            else if (!pgi(cell, "tr_cont", 0)) cell_send(cell, self);
        }
        return;
    }
    if (old_ctl_up) ((void (*)(id, SEL, id))old_ctl_up)(self, c, ev);
}

/* ================================================================ NSMatrix */
/* NSMatrixMode: Radio 0, Highlight 1, List 2, Track 3 */
static id mx_cells(id self) { id a = pget(self, "cells"); if (!a) { a = MSG0(id, (id)kls("NSMutableArray"), "array"); pset(self, "cells", a); } return a; }
static i64 mx_rows(id self) { return pgi(self, "rows", 0); }
static i64 mx_cols(id self) { return pgi(self, "cols", 0); }
static id mx_proto_cell(id self) {
    id p = pget(self, "prototype");
    if (p) return objc_autorelease(MSG(id, p, "copyWithZone:", (void *)0));
    id k = pget(self, "cellClassObj");
    if (!k) k = (id)kls("NSActionCell");
    return objc_autorelease(MSG0(id, MSG0(id, k, "alloc"), "init"));
}
static id mx_make_cell(id self) { id c = mx_proto_cell(self); MSG(void, c, "setControlView:", self); return c; }
static mx_geom mx_g(id self) {
    mx_geom g = {(int)mx_rows(self), (int)mx_cols(self), pgd(self, "cw", 100), pgd(self, "ch", 17), pgd(self, "sx", 1), pgd(self, "sy", 1)};
    return g;
}
static id mx_init_full(id self, SEL c, R f, i64 mode, id proto_or_class, i64 rows, i64 cols, int is_class) {
    (void)c;
    self = ((id (*)(id, SEL, R))super_imp("NSControl", sl("initWithFrame:")))(self, sl("initWithFrame:"), f);
    if (!self) return 0;
    psi(self, "mode", mode); psi(self, "allowsEmpty", mode == 0 ? 0 : 1); psi(self, "anchor", -1);
    psd(self, "cw", 100); psd(self, "ch", 17); psd(self, "sx", 1); psd(self, "sy", 1);
    if (proto_or_class) pset(self, is_class ? "cellClassObj" : "prototype", proto_or_class);
    psi(self, "rows", 0); psi(self, "cols", 0);
    MSG(void, self, "renewRows:columns:", rows, cols);
    if (rows > 0 && cols > 0 && f.w > 0 && f.h > 0) {
        psd(self, "cw", (f.w - (cols - 1) * 1.0) / cols);
        psd(self, "ch", (f.h - (rows - 1) * 1.0) / rows);
    }
    if (mode == 0 && rows * cols > 0) { id c0 = MSG(id, mx_cells(self), "objectAtIndex:", (u64)0); MSG(void, c0, "setState:", (i64)1); }
    return self;
}
static id mx_init_frame(id self, SEL c, R f) { return mx_init_full(self, c, f, 0, 0, 0, 0, 0); }
static id mx_init_proto(id self, SEL c, R f, i64 mode, id proto, i64 r, i64 cl) { return mx_init_full(self, c, f, mode, proto, r, cl, 0); }
static id mx_init_class(id self, SEL c, R f, i64 mode, id k, i64 r, i64 cl) { return mx_init_full(self, c, f, mode, k, r, cl, 1); }
static id mx_init_coder(id self, SEL c, id coder) { (void)coder; return mx_init_full(self, c, (R){0, 0, 100, 100}, 0, 0, 0, 0, 0); }
static void mx_renew(id self, SEL c, i64 rows, i64 cols) {
    (void)c;
    id old = MSG0(id, mx_cells(self), "copy");
    i64 orr = mx_rows(self), oc = mx_cols(self);
    id n = MSG0(id, (id)kls("NSMutableArray"), "array");
    for (i64 r = 0; r < rows; r++)
        for (i64 k = 0; k < cols; k++) {
            id cell = (r < orr && k < oc) ? MSG(id, old, "objectAtIndex:", (u64)(r * oc + k)) : mx_make_cell(self);
            MSG(void, n, "addObject:", cell);
        }
    objc_release(old);
    pset(self, "cells", n); psi(self, "rows", rows); psi(self, "cols", cols);
    redraw(self);
}
static id mx_cell_at(id self, SEL c, i64 r, i64 k) {
    (void)c;
    if (r < 0 || k < 0 || r >= mx_rows(self) || k >= mx_cols(self)) return 0;
    return MSG(id, mx_cells(self), "objectAtIndex:", (u64)(r * mx_cols(self) + k));
}
static id mx_cell_with_tag(id self, SEL c, i64 tag) {
    (void)c;
    id a = mx_cells(self);
    for (u64 i = 0, n = MSG0(u64, a, "count"); i < n; i++) { id x = MSG(id, a, "objectAtIndex:", i); if (MSG0(i64, x, "tag") == tag) return x; }
    return 0;
}
static BOOL mx_get_rc(id self, SEL c, i64 *r, i64 *k, id cell) {
    (void)c;
    id a = mx_cells(self);
    u64 i = MSG(u64, a, "indexOfObjectIdenticalTo:", cell);
    if (i == (u64)0x7fffffffffffffffULL || i >= MSG0(u64, a, "count")) return NO;
    if (r) *r = (i64)i / mx_cols(self);
    if (k) *k = (i64)i % mx_cols(self);
    return YES;
}
static id mx_all_cells(id self, SEL c) { (void)c; return mx_cells(self); }
static void mx_num_rc(id self, SEL c, i64 *r, i64 *k) { (void)c; if (r) *r = mx_rows(self); if (k) *k = mx_cols(self); }
static i64 mx_nrows(id self, SEL c) { (void)c; return mx_rows(self); }
static i64 mx_ncols(id self, SEL c) { (void)c; return mx_cols(self); }
static void mx_insert_row(id self, SEL c, i64 at, id cells) {
    (void)c;
    i64 cols = mx_cols(self) ? mx_cols(self) : (cells ? (i64)MSG0(u64, cells, "count") : 1);
    if (!mx_cols(self)) psi(self, "cols", cols);
    if (at > mx_rows(self)) at = mx_rows(self);
    for (i64 k = cols - 1; k >= 0; k--) {
        id cell = cells && (u64)k < MSG0(u64, cells, "count") ? MSG(id, cells, "objectAtIndex:", (u64)k) : mx_make_cell(self);
        MSG(void, cell, "setControlView:", self);
        MSG(void, mx_cells(self), "insertObject:atIndex:", cell, (u64)(at * cols));
    }
    psi(self, "rows", mx_rows(self) + 1);
    redraw(self);
}
static void mx_add_row(id self, SEL c) { mx_insert_row(self, c, mx_rows(self), 0); }
static void mx_add_row_cells(id self, SEL c, id cells) { mx_insert_row(self, c, mx_rows(self), cells); }
static void mx_insert_row_at(id self, SEL c, i64 at) { mx_insert_row(self, c, at, 0); }
static void mx_insert_col(id self, SEL c, i64 at, id cells) {
    (void)c;
    i64 rows = mx_rows(self) ? mx_rows(self) : 1, cols = mx_cols(self);
    if (!mx_rows(self)) psi(self, "rows", rows);
    if (at > cols) at = cols;
    for (i64 r = rows - 1; r >= 0; r--) {
        id cell = cells && (u64)r < MSG0(u64, cells, "count") ? MSG(id, cells, "objectAtIndex:", (u64)r) : mx_make_cell(self);
        MSG(void, cell, "setControlView:", self);
        MSG(void, mx_cells(self), "insertObject:atIndex:", cell, (u64)(r * cols + at));
    }
    psi(self, "cols", cols + 1);
    redraw(self);
}
static void mx_add_col(id self, SEL c) { mx_insert_col(self, c, mx_cols(self), 0); }
static void mx_add_col_cells(id self, SEL c, id cells) { mx_insert_col(self, c, mx_cols(self), cells); }
static void mx_insert_col_at(id self, SEL c, i64 at) { mx_insert_col(self, c, at, 0); }
static void mx_remove_row(id self, SEL c, i64 r) {
    (void)c;
    if (r < 0 || r >= mx_rows(self)) return;
    i64 cols = mx_cols(self);
    for (i64 k = 0; k < cols; k++) MSG(void, mx_cells(self), "removeObjectAtIndex:", (u64)(r * cols));
    psi(self, "rows", mx_rows(self) - 1);
    redraw(self);
}
static void mx_remove_col(id self, SEL c, i64 k) {
    (void)c;
    i64 cols = mx_cols(self);
    if (k < 0 || k >= cols) return;
    for (i64 r = mx_rows(self) - 1; r >= 0; r--) MSG(void, mx_cells(self), "removeObjectAtIndex:", (u64)(r * cols + k));
    psi(self, "cols", cols - 1);
    redraw(self);
}
static void mx_put_cell(id self, SEL c, id cell, i64 r, i64 k) {
    (void)c;
    if (r < 0 || k < 0 || r >= mx_rows(self) || k >= mx_cols(self) || !cell) return;
    MSG(void, cell, "setControlView:", self);
    MSG(void, mx_cells(self), "replaceObjectAtIndex:withObject:", (u64)(r * mx_cols(self) + k), cell);
    redraw(self);
}
static id mx_prototype(id self, SEL c) { (void)c; return pget(self, "prototype"); }
static void mx_set_prototype(id self, SEL c, id p) { (void)c; pset(self, "prototype", p); }
static id mx_cell_class_i(id self, SEL c) { (void)c; id k = pget(self, "cellClassObj"); return k ? k : (id)kls("NSActionCell"); }
static void mx_set_cell_class_i(id self, SEL c, id k) { (void)c; pset(self, "cellClassObj", k); }
static i64 mx_mode(id self, SEL c) { (void)c; return pgi(self, "mode", 0); }
static void mx_set_mode(id self, SEL c, i64 m) { (void)c; psi(self, "mode", m); }
static BOOL mx_allows_empty(id self, SEL c) { (void)c; return pgi(self, "allowsEmpty", 0) != 0; }
static void mx_set_allows_empty(id self, SEL c, BOOL b) { (void)c; psi(self, "allowsEmpty", b); }
static S mx_cell_size(id self, SEL c) { (void)c; return (S){pgd(self, "cw", 100), pgd(self, "ch", 17)}; }
static void mx_set_cell_size(id self, SEL c, S z) { (void)c; psd(self, "cw", z.w); psd(self, "ch", z.h); redraw(self); }
static S mx_spacing(id self, SEL c) { (void)c; return (S){pgd(self, "sx", 1), pgd(self, "sy", 1)}; }
static void mx_set_spacing(id self, SEL c, S z) { (void)c; psd(self, "sx", z.w); psd(self, "sy", z.h); redraw(self); }
static void mx_size_to_cells(id self, SEL c) {
    (void)c;
    mx_geom g = mx_g(self);
    S z = {g.cols * g.cw + (g.cols > 1 ? (g.cols - 1) * g.sx : 0), g.rows * g.ch + (g.rows > 1 ? (g.rows - 1) * g.sy : 0)};
    ((void (*)(id, SEL, S))objc_msgSend)(self, sl("setFrameSize:"), z);
}
static void mx_size_to_fit(id self, SEL c) {
    double w = 0, h = 0;
    id a = mx_cells(self);
    for (u64 i = 0, n = MSG0(u64, a, "count"); i < n; i++) {
        S z = ((S (*)(id, SEL))objc_msgSend)(MSG(id, a, "objectAtIndex:", i), sl("cellSize"));
        if (z.w > w) w = z.w;
        if (z.h > h) h = z.h;
    }
    if (w > 0) psd(self, "cw", w);
    if (h > 0) psd(self, "ch", h);
    mx_size_to_cells(self, c);
}
/* y da matriz conta de cima para baixo; num NSView não invertido a linha 0 fica no alto */
static R mx_frame_of(id self, i64 r, i64 k) {
    mx_geom g = mx_g(self);
    double x, y, w, h;
    mx_cell_frame(&g, (int)r, (int)k, &x, &y, &w, &h);
    if (!flipped(self)) { R b = msg_rect(self, "bounds"); y = b.h - y - h; }
    return (R){x, y, w, h};
}
static R mx_cell_frame_at(id self, SEL c, i64 r, i64 k) { (void)c; return mx_frame_of(self, r, k); }
static BOOL mx_rc_for_point(id self, SEL c, P p, i64 *r, i64 *k) {
    (void)c;
    mx_geom g = mx_g(self);
    double y = p.y;
    if (!flipped(self)) { R b = msg_rect(self, "bounds"); y = b.h - p.y; }
    int rr, cc;
    if (!mx_hit(&g, p.x, y, &rr, &cc)) return NO;
    if (r) *r = rr;
    if (k) *k = cc;
    return YES;
}
static void mx_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    if (pgi(self, "drawsBG", 0)) { id bg = pget(self, "bgColor"); MSG0(void, bg ? bg : cx_color(1, 1, 1, 1), "setFill"); NSRectFill(msg_rect(self, "bounds")); }
    i64 rows = mx_rows(self), cols = mx_cols(self), mode = pgi(self, "mode", 0);
    for (i64 r = 0; r < rows; r++)
        for (i64 k = 0; k < cols; k++) {
            id cell = mx_cell_at(self, 0, r, k);
            R f = mx_frame_of(self, r, k);
            if ((mode == 1 || mode == 2) && pgi(cell, "mxSel", 0) && !MSG(BOOL, cell, "isKindOfClass:", (id)kls("NSButtonCell")))
                fill(f, 0.13, 0.45, 0.95, 0.25);
            MSG(void, cell, "drawWithFrame:inView:", f, self);
        }
}
/* aplica a seleção do núcleo às células: rádio mexe no estado; realce/lista marcam "mxSel" e realçam */
static void mx_apply(id self, unsigned char *sel, i64 n) {
    i64 mode = pgi(self, "mode", 0);
    for (i64 i = 0; i < n; i++) {
        id cell = MSG(id, mx_cells(self), "objectAtIndex:", (u64)i);
        psi(cell, "mxSel", sel[i]);
        if (mode == 0) MSG(void, cell, "setState:", (i64)sel[i]);
        else if (mode == 1 && MSG(BOOL, cell, "isKindOfClass:", (id)kls("NSButtonCell"))) MSG(void, cell, "setHighlighted:", (BOOL)sel[i]);
    }
    redraw(self);
}
static void mx_read_sel(id self, unsigned char *sel, i64 n) {
    i64 mode = pgi(self, "mode", 0);
    for (i64 i = 0; i < n; i++) {
        id cell = MSG(id, mx_cells(self), "objectAtIndex:", (u64)i);
        sel[i] = mode == 0 ? MSG0(i64, cell, "state") != 0 : pgi(cell, "mxSel", 0) != 0;
    }
}
static void mx_select_index(id self, i64 idx, int mods) {
    i64 n = mx_rows(self) * mx_cols(self);
    if (n <= 0 || n > 4096) return;
    unsigned char sel[4096];
    mx_read_sel(self, sel, n);
    int anchor = (int)pgi(self, "anchor", -1);
    mx_click((int)pgi(self, "mode", 0), (int)pgi(self, "allowsEmpty", 0), sel, (int)n, (int)idx, mods, &anchor);
    psi(self, "anchor", anchor);
    psi(self, "selIdx", idx);
    mx_apply(self, sel, n);
}
static id mx_selected_cell(id self, SEL c) {
    (void)c;
    i64 n = mx_rows(self) * mx_cols(self), mode = pgi(self, "mode", 0);
    i64 last = pgi(self, "selIdx", -1);
    if (last >= 0 && last < n) {
        id cell = MSG(id, mx_cells(self), "objectAtIndex:", (u64)last);
        if (mode == 0 ? MSG0(i64, cell, "state") != 0 : pgi(cell, "mxSel", 0)) return cell;
    }
    for (i64 i = 0; i < n; i++) {
        id cell = MSG(id, mx_cells(self), "objectAtIndex:", (u64)i);
        if (mode == 0 ? MSG0(i64, cell, "state") != 0 : pgi(cell, "mxSel", 0) != 0) return cell;
    }
    return 0;
}
static id mx_selected_cells(id self, SEL c) {
    (void)c;
    id out = MSG0(id, (id)kls("NSMutableArray"), "array");
    i64 n = mx_rows(self) * mx_cols(self), mode = pgi(self, "mode", 0);
    for (i64 i = 0; i < n; i++) {
        id cell = MSG(id, mx_cells(self), "objectAtIndex:", (u64)i);
        if (mode == 0 ? MSG0(i64, cell, "state") != 0 : pgi(cell, "mxSel", 0) != 0) MSG(void, out, "addObject:", cell);
    }
    return out;
}
static i64 mx_selected_row(id self, SEL c) { (void)c; id cell = mx_selected_cell(self, 0); i64 r = -1; if (cell) mx_get_rc(self, 0, &r, 0, cell); return r; }
static i64 mx_selected_col(id self, SEL c) { (void)c; id cell = mx_selected_cell(self, 0); i64 k = -1; if (cell) mx_get_rc(self, 0, 0, &k, cell); return k; }
static void mx_select_cell_at(id self, SEL c, i64 r, i64 k) {
    (void)c;
    if (r < 0 || k < 0) {   /* -1: limpa (se a matriz permitir seleção vazia) */
        if (pgi(self, "allowsEmpty", 0)) { MSG0(void, self, "deselectAllCells"); }
        return;
    }
    if (r >= mx_rows(self) || k >= mx_cols(self)) return;
    mx_select_index(self, r * mx_cols(self) + k, 0);
}
static BOOL mx_select_cell_with_tag(id self, SEL c, i64 tag) {
    (void)c;
    id cell = mx_cell_with_tag(self, 0, tag);
    i64 r, k;
    if (!cell || !mx_get_rc(self, 0, &r, &k, cell)) return NO;
    mx_select_cell_at(self, 0, r, k);
    return YES;
}
static void mx_select_cell(id self, SEL c, id cell) { (void)c; i64 r, k; if (mx_get_rc(self, 0, &r, &k, cell)) mx_select_cell_at(self, 0, r, k); }
static void mx_deselect_all(id self, SEL c) {
    (void)c;
    i64 n = mx_rows(self) * mx_cols(self);
    if (n <= 0 || n > 4096) return;
    if (pgi(self, "mode", 0) == 0 && !pgi(self, "allowsEmpty", 0)) return;
    unsigned char sel[4096] = {0};
    mx_apply(self, sel, n);
    psi(self, "selIdx", -1);
}
static void mx_deselect_selected(id self, SEL c) { (void)c; id cell = mx_selected_cell(self, 0); if (cell) { psi(cell, "mxSel", 0); if (pgi(self, "mode", 0) == 0) MSG(void, cell, "setState:", (i64)0); redraw(self); } }
static void mx_select_all(id self, SEL c, id sender) {
    (void)c; (void)sender;
    if (pgi(self, "mode", 0) != 2) return;
    i64 n = mx_rows(self) * mx_cols(self);
    if (n <= 0 || n > 4096) return;
    unsigned char sel[4096];
    for (i64 i = 0; i < n; i++) sel[i] = 1;
    mx_apply(self, sel, n);
}
static void mx_set_state_at(id self, SEL c, i64 st, i64 r, i64 k) { (void)c; id cell = mx_cell_at(self, 0, r, k); if (!cell) return; if (pgi(self, "mode", 0) == 0 && st) mx_select_cell_at(self, 0, r, k); else MSG(void, cell, "setState:", st); redraw(self); }
static i64 mx_selected_tag(id self, SEL c) { (void)c; id cell = mx_selected_cell(self, 0); return cell ? MSG0(i64, cell, "tag") : -1; }
static id mx_key_cell(id self, SEL c) { (void)c; id k = pget(self, "keyCell"); return k ? k : mx_selected_cell(self, 0); }
static void mx_set_key_cell(id self, SEL c, id k) { (void)c; pset(self, "keyCell", k); }
static SEL mx_double_action(id self, SEL c) { (void)c; id v = pget(self, "doubleAction"); return v ? sl(cx_cstr(v)) : 0; }
static void mx_set_double_action(id self, SEL c, SEL a) { (void)c; pset(self, "doubleAction", a ? cx_str(sel_getName(a)) : 0); }
static id mx_bg(id self, SEL c) { (void)c; return pget(self, "bgColor"); }
static void mx_set_bg(id self, SEL c, id col) { (void)c; pset(self, "bgColor", col); redraw(self); }
static BOOL mx_draws_bg(id self, SEL c) { (void)c; return pgi(self, "drawsBG", 0) != 0; }
static void mx_set_draws_bg(id self, SEL c, BOOL b) { (void)c; psi(self, "drawsBG", b); redraw(self); }
static BOOL mx_autosize(id self, SEL c) { (void)c; return pgi(self, "autosize", 0) != 0; }
static void mx_set_autosize(id self, SEL c, BOOL b) { (void)c; psi(self, "autosize", b); }
static BOOL mx_is_flipped(id self, SEL c) { (void)self; (void)c; return YES; }
static BOOL mx_accepts_fr(id self, SEL c) { (void)self; (void)c; return YES; }
static id mx_delegate(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_mx_delegate); }
static void mx_set_delegate(id self, SEL c, id d) { (void)c; objc_setAssociatedObject(self, &k_mx_delegate, d, 0); }
static void mx_send_cell_action(id self, id cell, int dbl) {
    SEL a = 0;
    id t = 0;
    if (dbl) { a = mx_double_action(self, 0); t = MSG0(id, self, "target"); }
    if (!a && cell) { a = cell_action_of(cell); t = responds(cell, "target") ? MSG0(id, cell, "target") : 0; }
    if (!a) { a = MSG0(SEL, self, "action"); t = MSG0(id, self, "target"); }
    if (a) MSG(BOOL, self, "sendAction:to:", a, t);
}
static BOOL mx_send_action(id self, SEL c) { (void)c; mx_send_cell_action(self, mx_selected_cell(self, 0), 0); return YES; }
static i64 ev_mods_to_core(id ev) { u64 f = MSG0(u64, ev, "modifierFlags"); return ((f & (1u << 17)) ? 1 : 0) | ((f & (1u << 20)) ? 2 : 0); }
static void mx_mouse_down(id self, SEL c, id ev) {
    (void)c;
    if (!MSG0(BOOL, self, "isEnabled")) return;
    i64 r, k;
    if (!mx_rc_for_point(self, 0, m_loc(self, ev), &r, &k)) return;
    id cell = mx_cell_at(self, 0, r, k);
    if (!cell || !MSG0(BOOL, cell, "isEnabled")) return;
    psi(self, "downIdx", r * mx_cols(self) + k);
    i64 mode = pgi(self, "mode", 0);
    if (mode == 3) {   /* trilha: a célula rastreia o mouse sozinha (sliders numa matriz) */
        R f = mx_frame_of(self, r, k);
        pset(self, "trackCell", cell);
        MSG(BOOL, cell, "trackMouse:inRect:ofView:untilMouseUp:", ev, f, self, (BOOL)YES);
        return;
    }
    if (mode != 0 || !MSG(BOOL, cell, "isKindOfClass:", (id)kls("NSButtonCell"))) mx_select_index(self, r * mx_cols(self) + k, (int)ev_mods_to_core(ev));
    if (MSG(BOOL, cell, "isKindOfClass:", (id)kls("NSButtonCell"))) MSG(void, cell, "setHighlighted:", (BOOL)YES);
    redraw(self);
}
static void mx_mouse_dragged(id self, SEL c, id ev) {
    (void)c;
    id tc = pget(self, "trackCell");
    if (tc) { cx_cell_track_drag(tc, ev, self); return; }
    i64 r, k, mode = pgi(self, "mode", 0);
    if (mode != 2 || !mx_rc_for_point(self, 0, m_loc(self, ev), &r, &k)) return;
    i64 idx = r * mx_cols(self) + k;
    if (idx != pgi(self, "selIdx", -1)) mx_select_index(self, idx, 1);   /* arrastar numa lista estende a seleção */
}
static void mx_mouse_up(id self, SEL c, id ev) {
    (void)c;
    id tc = pget(self, "trackCell");
    if (tc) { cx_cell_track_up(tc, ev, self); pset(self, "trackCell", 0); mx_send_cell_action(self, tc, 0); return; }
    i64 di = pgi(self, "downIdx", -1);
    psi(self, "downIdx", -1);
    if (di < 0 || di >= mx_rows(self) * mx_cols(self)) return;
    id cell = MSG(id, mx_cells(self), "objectAtIndex:", (u64)di);
    if (MSG(BOOL, cell, "isKindOfClass:", (id)kls("NSButtonCell"))) MSG(void, cell, "setHighlighted:", (BOOL)NO);
    i64 r, k;
    int inside = mx_rc_for_point(self, 0, m_loc(self, ev), &r, &k) && r * mx_cols(self) + k == di;
    if (!inside) { redraw(self); return; }
    i64 mode = pgi(self, "mode", 0);
    if (mode == 0 && MSG(BOOL, cell, "isKindOfClass:", (id)kls("NSButtonCell"))) mx_select_index(self, di, 0);
    else if (mode != 0 && MSG(BOOL, cell, "isKindOfClass:", (id)kls("NSButtonCell"))) {
        i64 t = pgi(cell, "buttonType", 7);
        if (t == 1 || t == 2 || t == 3 || t == 6) cell_set_next_state(cell, 0);
    }
    i64 clicks = MSG0(i64, ev, "clickCount");
    mx_send_cell_action(self, cell, clicks >= 2 && mx_double_action(self, 0));
    redraw(self);
}
static void mx_key_down(id self, SEL c, id ev) {
    (void)c;
    id ch = MSG0(id, ev, "charactersIgnoringModifiers");
    unsigned short u = ch && MSG0(u64, ch, "length") ? MSG(unsigned short, ch, "characterAtIndex:", (u64)0) : 0;
    i64 rows = mx_rows(self), cols = mx_cols(self), cur = pgi(self, "selIdx", 0);
    if (rows * cols <= 0) return;
    if (cur < 0) cur = 0;
    i64 r = cur / cols, k = cur % cols;
    if (u == 0xF700) r = r > 0 ? r - 1 : 0;            /* ↑ */
    else if (u == 0xF701) r = r + 1 < rows ? r + 1 : r; /* ↓ */
    else if (u == 0xF702) k = k > 0 ? k - 1 : 0;        /* ← */
    else if (u == 0xF703) k = k + 1 < cols ? k + 1 : k; /* → */
    else if (u == ' ' || u == 13) { mx_send_cell_action(self, mx_cell_at(self, 0, r, k), 0); return; }
    else { NSBeep(); return; }
    mx_select_index(self, r * cols + k, (int)(ev_mods_to_core(ev) & 1));
    mx_send_cell_action(self, mx_cell_at(self, 0, r, k), 0);
}
static void mx_sort(id self, SEL c, SEL cmp, void *ctx) {
    (void)c; (void)ctx;
    id a = mx_cells(self);
    MSG(void, a, "sortUsingSelector:", cmp);
    redraw(self);
}
static id mx_init(id self, SEL c) { return mx_init_frame(self, c, (R){0, 0, 100, 100}); }

/* ================================================================ NSSwitch */
static id sw_init_frame(id self, SEL c, R f) {
    (void)c;
    self = ((id (*)(id, SEL, R))super_imp("NSControl", sl("initWithFrame:")))(self, sl("initWithFrame:"), f);
    if (self) psi(self, "swState", 0);
    return self;
}
static i64 sw_state(id self, SEL c) { (void)c; return pgi(self, "swState", 0); }
static void sw_set_state(id self, SEL c, i64 s) {
    (void)c;
    psi(self, "swState", s ? 1 : 0);
    redraw(self);
    extern void cx_ax_note_view(id obj, const char *note);
    cx_ax_note_view(self, "AXValueChanged");
}
static double sw_double(id self, SEL c) { return (double)sw_state(self, c); }
static void sw_set_double(id self, SEL c, double v) { sw_set_state(self, c, v != 0); }
static id sw_object(id self, SEL c) { return cx_num_i(sw_state(self, c)); }
static void sw_set_object(id self, SEL c, id v) { sw_set_state(self, c, v && responds(v, "integerValue") ? MSG0(i64, v, "integerValue") : 0); }
static S sw_intrinsic(id self, SEL c) { (void)c; return pgi(self, "controlSizeSW", 0) == 1 ? (S){32, 18} : (S){38, 22}; }
static void sw_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    R b = msg_rect(self, "bounds");
    S z = sw_intrinsic(self, 0);
    R t = {b.x + (b.w - z.w) / 2, b.y + (b.h - z.h) / 2, z.w, z.h};
    BOOL on = sw_state(self, 0) != 0, en = MSG0(BOOL, self, "isEnabled");
    double drag = pgd(self, "dragFrac", -1), frac = drag >= 0 ? drag : (on ? 1 : 0);
    id track = path_rr(t, z.h / 2);
    double gr = 0.85 + (0.13 - 0.85) * frac, gg = 0.85 + (0.45 - 0.85) * frac, gb = 0.86 + (0.95 - 0.86) * frac;
    fill_path(track, gr, gg, gb, en ? 1 : 0.5);
    double d = z.h - 4, x = t.x + 2 + frac * (z.w - d - 4);
    id knob = path_oval((R){x, t.y + 2, d, d});
    fill_path(knob, 1, 1, 1, 1);
    stroke_path(knob, 0, 0, 0, 0.15);
}
static void sw_mouse_down(id self, SEL c, id ev) {
    (void)c;
    if (!MSG0(BOOL, self, "isEnabled")) return;
    P p = m_loc(self, ev);
    psd(self, "downX", p.x); psi(self, "moved", 0); psd(self, "dragFrac", -1);
}
static void sw_mouse_dragged(id self, SEL c, id ev) {
    (void)c;
    if (!MSG0(BOOL, self, "isEnabled")) return;
    P p = m_loc(self, ev);
    S z = sw_intrinsic(self, 0);
    double dx = p.x - pgd(self, "downX", p.x);
    if (dx > 2 || dx < -2) psi(self, "moved", 1);
    if (!pgi(self, "moved", 0)) return;
    double base = sw_state(self, 0) ? 1 : 0, f = base + dx / (z.w - z.h);
    psd(self, "dragFrac", f < 0 ? 0 : f > 1 ? 1 : f);
    redraw(self);
}
static void sw_mouse_up(id self, SEL c, id ev) {
    (void)c; (void)ev;
    if (!MSG0(BOOL, self, "isEnabled")) return;
    i64 old = sw_state(self, 0), nw;
    if (pgi(self, "moved", 0)) nw = pgd(self, "dragFrac", old) >= 0.5;
    else nw = !old;
    psd(self, "dragFrac", -1);
    sw_set_state(self, 0, nw);
    if (nw != old) { SEL a = MSG0(SEL, self, "action"); if (a) MSG(BOOL, self, "sendAction:to:", a, MSG0(id, self, "target")); }
}
static void sw_key_down(id self, SEL c, id ev) {
    (void)c;
    id ch = MSG0(id, ev, "charactersIgnoringModifiers");
    unsigned short u = ch && MSG0(u64, ch, "length") ? MSG(unsigned short, ch, "characterAtIndex:", (u64)0) : 0;
    if (u != ' ') { NSBeep(); return; }
    MSG(void, self, "performClick:", (id)0);
}
static void sw_perform_click(id self, SEL c, id sender) {
    (void)c; (void)sender;
    if (!MSG0(BOOL, self, "isEnabled")) return;
    sw_set_state(self, 0, !sw_state(self, 0));
    SEL a = MSG0(SEL, self, "action");
    if (a) MSG(BOOL, self, "sendAction:to:", a, MSG0(id, self, "target"));
}
static void sw_set_control_size(id self, SEL c, u64 s) { (void)c; psi(self, "controlSizeSW", (i64)s); redraw(self); }
static id sw_ax_role(id self, SEL c) { (void)self; (void)c; return cx_str("AXCheckBox"); }
static id sw_ax_subrole(id self, SEL c) { (void)self; (void)c; return cx_str("AXSwitch"); }
static id sw_ax_value(id self, SEL c) { return cx_num_i(sw_state(self, c)); }
static BOOL sw_ax_press(id self, SEL c) { (void)c; sw_perform_click(self, 0, 0); return YES; }

/* ================================================================ registro */
#define ADD(k, s, f) cx_add_method(k, s, (void *)(f))
#define P_(k, get, set) ADD(k, #get, c_g_##get); ADD(k, #set ":", c_s_##get)
static cx_class_t *ensure(cx_class_t *k, cx_class_t *m, const char *name, const char *super) {
    cx_class_t *e = kls(name);
    if (e) return e;
    cx_class_t *s = kls(super);
    if (!s) s = kls("NSObject");
    return cx_define_class(k, m, name, s, 0);
}
void cx_appkit_cells_init(void) {
    cx_class_t *cell = kls("NSCell"), *acell = kls("NSActionCell"), *bcell = kls("NSButtonCell"), *tfc = kls("NSTextFieldCell"), *imc = kls("NSImageCell");
    cx_class_t *ctl = kls("NSControl");
    if (!cell || !acell || !ctl) return;
    ADD(cell, "init", cell_init); ADD(cell, "initTextCell:", cell_init_text); ADD(cell, "initImageCell:", cell_init_image);
    ADD(cell, "initWithCoder:", cell_init_coder); ADD(cell, "copyWithZone:", cell_copy);
    ADD(cell, "objectValue", cell_object); ADD(cell, "setObjectValue:", cell_set_object);
    ADD(cell, "stringValue", cell_string); ADD(cell, "setStringValue:", cell_set_string);
    ADD(cell, "attributedStringValue", cell_attr_string); ADD(cell, "setAttributedStringValue:", cell_set_attr_string);
    ADD(cell, "doubleValue", cell_double); ADD(cell, "setDoubleValue:", cell_set_double);
    ADD(cell, "floatValue", cell_float); ADD(cell, "setFloatValue:", cell_set_float);
    ADD(cell, "intValue", cell_int32); ADD(cell, "setIntValue:", cell_set_int32);
    ADD(cell, "integerValue", cell_int); ADD(cell, "setIntegerValue:", cell_set_int);
    ADD(cell, "boolValue", cell_bool); ADD(cell, "setBoolValue:", cell_set_bool);
    ADD(cell, "takeStringValueFrom:", cell_take_string); ADD(cell, "takeDoubleValueFrom:", cell_take_double);
    ADD(cell, "takeFloatValueFrom:", cell_take_double); ADD(cell, "takeIntValueFrom:", cell_take_int);
    ADD(cell, "takeIntegerValueFrom:", cell_take_int); ADD(cell, "takeObjectValueFrom:", cell_take_object);
    P_(cell, title, setTitle); P_(cell, image, setImage); P_(cell, font, setFont); P_(cell, formatter, setFormatter);
    P_(cell, representedObject, setRepresentedObject); P_(cell, menu, setMenu); P_(cell, identifier, setIdentifier);
    P_(cell, state, setState); P_(cell, alignment, setAlignment); P_(cell, tag, setTag); P_(cell, lineBreakMode, setLineBreakMode);
    P_(cell, controlSize, setControlSize); P_(cell, focusRingType, setFocusRingType); P_(cell, baseWritingDirection, setBaseWritingDirection);
    P_(cell, controlTint, setControlTint);
    P_(cell, isEnabled, setEnabled); P_(cell, isEditable, setEditable); P_(cell, isSelectable, setSelectable);
    P_(cell, isBordered, setBordered); P_(cell, isBezeled, setBezeled); P_(cell, isHighlighted, setHighlighted);
    P_(cell, isContinuous, setContinuous); P_(cell, wraps, setWraps); P_(cell, isScrollable, setScrollable);
    P_(cell, allowsMixedState, setAllowsMixedState); P_(cell, showsFirstResponder, setShowsFirstResponder);
    P_(cell, refusesFirstResponder, setRefusesFirstResponder); P_(cell, truncatesLastVisibleLine, setTruncatesLastVisibleLine);
    P_(cell, usesSingleLineMode, setUsesSingleLineMode); P_(cell, sendsActionOnEndEditing, setSendsActionOnEndEditing);
    P_(cell, allowsEditingTextAttributes, setAllowsEditingTextAttributes); P_(cell, importsGraphics, setImportsGraphics);
    P_(cell, allowsUndo, setAllowsUndo); P_(cell, isOpaque, setOpaque);
    ADD(cell, "enabled", c_g_isEnabled); ADD(cell, "editable", c_g_isEditable); ADD(cell, "selectable", c_g_isSelectable);
    ADD(cell, "bordered", c_g_isBordered); ADD(cell, "bezeled", c_g_isBezeled); ADD(cell, "highlighted", c_g_isHighlighted);
    ADD(cell, "continuous", c_g_isContinuous); ADD(cell, "scrollable", c_g_isScrollable);
    ADD(cell, "type", cell_type); ADD(cell, "setType:", cell_set_type);
    ADD(cell, "controlView", cv_of); ADD(cell, "setControlView:", cell_set_control_view);
    ADD(cell, "target", cell_target); ADD(cell, "setTarget:", cell_set_target_noop);
    ADD(cell, "action", cell_action); ADD(cell, "setAction:", cell_set_action_noop);
    ADD(cell, "acceptsFirstResponder", cell_accepts_fr); ADD(cell, "nextState", cell_next_state); ADD(cell, "setNextState", cell_set_next_state);
    ADD(cell, "compare:", cell_compare); ADD(cell, "setControlSize:", cell_set_font_size_ctl);
    ADD(cell, "drawingRectForBounds:", cell_drawing_rect); ADD(cell, "titleRectForBounds:", cell_title_rect); ADD(cell, "imageRectForBounds:", cell_image_rect);
    ADD(cell, "cellSize", cell_size); ADD(cell, "cellSizeForBounds:", cell_size_for_bounds);
    ADD(cell, "drawWithFrame:inView:", cell_draw); ADD(cell, "drawInteriorWithFrame:inView:", cell_draw_interior);
    ADD(cell, "highlight:withFrame:inView:", cell_highlight); ADD(cell, "drawFocusRingMaskWithFrame:inView:", cell_draw_focus_mask);
    ADD(cell, "focusRingMaskBoundsForFrame:inView:", cell_focus_bounds); ADD(cell, "_drawFocusRingWithFrame:", cell_draw_focus);
    ADD(cell, "hitTestForEvent:inRect:ofView:", cell_hit_test); ADD(cell, "trackMouse:inRect:ofView:untilMouseUp:", cell_track);
    ADD(cell, "startTrackingAt:inView:", cell_start_tracking); ADD(cell, "continueTracking:at:inView:", cell_continue_tracking);
    ADD(cell, "stopTracking:at:inView:mouseIsUp:", cell_stop_tracking);
    ADD(cell, "performClick:", cell_perform_click); ADD(cell, "resetCursorRect:inView:", cell_reset_cursor); ADD(cell, "calcDrawInfo:", cell_calc_draw_info);
    ADD(cell, "fieldEditorForView:", cell_field_editor); ADD(cell, "endEditing:", cell_end_editing); ADD(cell, "isEntryAcceptable:", cell_is_entry_acceptable);
    ADD(cell, "sendActionOn:", c_s_sendActionOn);
    ADD(acell, "target", ac_target); ADD(acell, "setTarget:", ac_set_target); ADD(acell, "action", ac_action); ADD(acell, "setAction:", ac_set_action);
    if (bcell) {
        ADD(bcell, "init", bc_init); ADD(bcell, "initTextCell:", bc_init_text); ADD(bcell, "initImageCell:", bc_init_image);
        ADD(bcell, "setButtonType:", bc_set_type); ADD(bcell, "title", bc_title); ADD(bcell, "setTitle:", bc_set_title);
        ADD(bcell, "stringValue", bc_string); ADD(bcell, "doubleValue", bc_double); ADD(bcell, "setDoubleValue:", bc_set_double);
        ADD(bcell, "objectValue", bc_object); ADD(bcell, "setObjectValue:", bc_set_object);
        ADD(bcell, "cellSize", bc_cell_size); ADD(bcell, "drawWithFrame:inView:", bc_draw);
        ADD(bcell, "performClick:", bc_perform_click);
        P_(bcell, alternateTitle, setAlternateTitle); P_(bcell, alternateImage, setAlternateImage); P_(bcell, keyEquivalent, setKeyEquivalent);
        P_(bcell, keyEquivalentModifierMask, setKeyEquivalentModifierMask); P_(bcell, imagePosition, setImagePosition);
        P_(bcell, bezelStyle, setBezelStyle); P_(bcell, highlightsBy, setHighlightsBy); P_(bcell, showsStateBy, setShowsStateBy);
        P_(bcell, imageScaling, setImageScaling); P_(bcell, backgroundColor, setBackgroundColor); P_(bcell, isTransparent, setTransparent);
        P_(bcell, showsBorderOnlyWhileMouseInside, setShowsBorderOnlyWhileMouseInside);
        ADD(bcell, "transparent", c_g_isTransparent);
    }
    if (tfc) {
        ADD(tfc, "init", tfc_init); ADD(tfc, "drawWithFrame:inView:", tfc_draw);
        P_(tfc, textColor, setTextColor); P_(tfc, backgroundColor, setBackgroundColor); P_(tfc, drawsBackground, setDrawsBackground);
        P_(tfc, placeholderString, setPlaceholderString); P_(tfc, placeholderAttributedString, setPlaceholderAttributedString);
        ADD(tfc, "bezelStyle", c_g_bezelStyleTF); ADD(tfc, "setBezelStyle:", c_s_bezelStyleTF);
    }
    if (imc) {
        ADD(imc, "drawWithFrame:inView:", imc_draw); ADD(imc, "drawInteriorWithFrame:inView:", imc_draw_interior);
        P_(imc, imageScaling, setImageScaling);
    }
    cx_class_t *k;
    k = ensure(&NSSliderCell_c, &NSSliderCell_m, "NSSliderCell", "NSActionCell");
    ADD(k, "init", slc_init); ADD(k, "drawWithFrame:inView:", slc_draw); ADD(k, "knobRectFlipped:", slc_knob_rect); ADD(k, "trackRect", slc_track_rect);
    ADD(k, "startTrackingAt:inView:", slc_start); ADD(k, "continueTracking:at:inView:", slc_continue); ADD(k, "cellSize", slc_cell_size);
    ADD(k, "numberOfTickMarks", slc_ticks); ADD(k, "setNumberOfTickMarks:", slc_set_ticks);
    ADD(k, "allowsTickMarkValuesOnly", slc_only_ticks); ADD(k, "setAllowsTickMarkValuesOnly:", slc_set_only_ticks);
    ADD(k, "tickMarkValueAtIndex:", slc_tick_value); ADD(k, "closestTickMarkValueToValue:", slc_closest_tick);
    ADD(k, "tickMarkPosition", slc_tick_pos); ADD(k, "setTickMarkPosition:", slc_set_tick_pos);
    ADD(k, "isVertical", slc_is_vertical); ADD(k, "setVertical:", slc_set_vertical);
    ADD(k, "sliderType", slc_type); ADD(k, "setSliderType:", slc_set_type);
    P_(k, minValue, setMinValue); P_(k, maxValue, setMaxValue); P_(k, altIncrementValue, setAltIncrementValue); P_(k, knobThickness, setKnobThickness);
    k = ensure(&NSStepperCell_c, &NSStepperCell_m, "NSStepperCell", "NSActionCell");
    ADD(k, "init", stc_init); ADD(k, "drawWithFrame:inView:", stc_draw); ADD(k, "startTrackingAt:inView:", stc_start);
    ADD(k, "stopTracking:at:inView:mouseIsUp:", stc_stop); ADD(k, "cellSize", stc_cell_size);
    ADD(k, "valueWraps", stc_wraps); ADD(k, "setValueWraps:", stc_set_wraps); ADD(k, "autorepeat", stc_autorepeat); ADD(k, "setAutorepeat:", stc_set_autorepeat);
    P_(k, minValue, setMinValue); P_(k, maxValue, setMaxValue); P_(k, increment, setIncrement);
    k = ensure(&NSComboBoxCell_c, &NSComboBoxCell_m, "NSComboBoxCell", tfc ? "NSTextFieldCell" : "NSActionCell");
    ADD(k, "init", cbc_init); ADD(k, "drawWithFrame:inView:", cbc_draw);
    ADD(k, "numberOfItems", cbc_count); ADD(k, "itemObjectValueAtIndex:", cbc_item_at); ADD(k, "addItemWithObjectValue:", cbc_add);
    ADD(k, "addItemsWithObjectValues:", cbc_add_many); ADD(k, "insertItemWithObjectValue:atIndex:", cbc_insert);
    ADD(k, "removeItemWithObjectValue:", cbc_remove); ADD(k, "removeItemAtIndex:", cbc_remove_at); ADD(k, "removeAllItems", cbc_remove_all);
    ADD(k, "objectValues", cbc_values); ADD(k, "indexOfItemWithObjectValue:", cbc_index_of);
    ADD(k, "indexOfSelectedItem", cbc_selected); ADD(k, "selectItemAtIndex:", cbc_select); ADD(k, "deselectItemAtIndex:", cbc_deselect);
    ADD(k, "objectValueOfSelectedItem", cbc_obj_selected); ADD(k, "selectItemWithObjectValue:", cbc_select);
    ADD(k, "dataSource", cbc_ds); ADD(k, "setDataSource:", cbc_set_ds); ADD(k, "usesDataSource", cbc_uses_ds); ADD(k, "setUsesDataSource:", cbc_set_uses_ds);
    ADD(k, "numberOfVisibleItems", cbc_visible); ADD(k, "setNumberOfVisibleItems:", cbc_set_visible);
    ADD(k, "itemHeight", cbc_item_h); ADD(k, "setItemHeight:", cbc_set_item_h);
    ADD(k, "hasVerticalScroller", cbc_has_scroller); ADD(k, "setHasVerticalScroller:", cbc_set_has_scroller);
    ADD(k, "completes", cbc_completes); ADD(k, "setCompletes:", cbc_set_completes); ADD(k, "completedString:", cbc_complete);
    ADD(k, "noteNumberOfItemsChanged", cbc_note); ADD(k, "reloadData", cbc_note); ADD(k, "scrollItemAtIndexToTop:", cbc_deselect);
    ADD(k, "scrollItemAtIndexToVisible:", cbc_deselect);
    k = ensure(&NSLevelIndicatorCell_c, &NSLevelIndicatorCell_m, "NSLevelIndicatorCell", "NSActionCell");
    ADD(k, "init", lic_init); ADD(k, "initWithLevelIndicatorStyle:", lic_init_style); ADD(k, "drawWithFrame:inView:", lic_draw);
    ADD(k, "levelIndicatorStyle", lic_style); ADD(k, "setLevelIndicatorStyle:", lic_set_style);
    P_(k, minValue, setMinValue); P_(k, maxValue, setMaxValue); P_(k, warningValue, setWarningValue); P_(k, criticalValue, setCriticalValue);

    /* NSControl ↔ célula */
    cx_add_cmethod(ctl, "cellClass", (void *)ctl_cell_class);
    cx_add_cmethod(ctl, "setCellClass:", (void *)ctl_set_cell_class);
    ADD(ctl, "cell", ctl_cell); ADD(ctl, "setCell:", ctl_set_cell); ADD(ctl, "selectedCell", ctl_selected_cell);
    ADD(ctl, "selectedTag", ctl_selected_tag); ADD(ctl, "updateCell:", ctl_update_cell); ADD(ctl, "updateCellInside:", ctl_update_cell_inside);
    ADD(ctl, "drawCell:", ctl_draw_cell); ADD(ctl, "drawCellInside:", ctl_draw_cell); ADD(ctl, "selectCell:", ctl_select_cell);
    ADD(ctl, "calcSize", ctl_calc_size);
    old_ctl_draw = class_getMethodImplementation(ctl, sl("drawRect:"));
    old_ctl_down = class_getMethodImplementation(ctl, sl("mouseDown:"));
    old_ctl_drag = class_getMethodImplementation(ctl, sl("mouseDragged:"));
    old_ctl_up = class_getMethodImplementation(ctl, sl("mouseUp:"));
    ADD(ctl, "drawRect:", ctl_draw_rect); ADD(ctl, "mouseDown:", ctl_mouse_down2);
    ADD(ctl, "mouseDragged:", ctl_mouse_dragged2); ADD(ctl, "mouseUp:", ctl_mouse_up2);

    k = ensure(&NSMatrix_c, &NSMatrix_m, "NSMatrix", "NSControl");
    ADD(k, "init", mx_init); ADD(k, "initWithFrame:", mx_init_frame); ADD(k, "initWithCoder:", mx_init_coder);
    ADD(k, "initWithFrame:mode:prototype:numberOfRows:numberOfColumns:", mx_init_proto);
    ADD(k, "initWithFrame:mode:cellClass:numberOfRows:numberOfColumns:", mx_init_class);
    ADD(k, "renewRows:columns:", mx_renew); ADD(k, "cellAtRow:column:", mx_cell_at); ADD(k, "cellWithTag:", mx_cell_with_tag);
    ADD(k, "getRow:column:ofCell:", mx_get_rc); ADD(k, "getRow:column:forPoint:", mx_rc_for_point); ADD(k, "cells", mx_all_cells);
    ADD(k, "getNumberOfRows:columns:", mx_num_rc); ADD(k, "numberOfRows", mx_nrows); ADD(k, "numberOfColumns", mx_ncols);
    ADD(k, "addRow", mx_add_row); ADD(k, "addRowWithCells:", mx_add_row_cells); ADD(k, "insertRow:", mx_insert_row_at);
    ADD(k, "insertRow:withCells:", mx_insert_row); ADD(k, "removeRow:", mx_remove_row);
    ADD(k, "addColumn", mx_add_col); ADD(k, "addColumnWithCells:", mx_add_col_cells); ADD(k, "insertColumn:", mx_insert_col_at);
    ADD(k, "insertColumn:withCells:", mx_insert_col); ADD(k, "removeColumn:", mx_remove_col);
    ADD(k, "putCell:atRow:column:", mx_put_cell); ADD(k, "prototype", mx_prototype); ADD(k, "setPrototype:", mx_set_prototype);
    ADD(k, "cellClass", mx_cell_class_i); ADD(k, "setCellClass:", mx_set_cell_class_i);
    ADD(k, "mode", mx_mode); ADD(k, "setMode:", mx_set_mode); ADD(k, "allowsEmptySelection", mx_allows_empty); ADD(k, "setAllowsEmptySelection:", mx_set_allows_empty);
    ADD(k, "cellSize", mx_cell_size); ADD(k, "setCellSize:", mx_set_cell_size); ADD(k, "intercellSpacing", mx_spacing); ADD(k, "setIntercellSpacing:", mx_set_spacing);
    ADD(k, "sizeToCells", mx_size_to_cells); ADD(k, "sizeToFit", mx_size_to_fit); ADD(k, "cellFrameAtRow:column:", mx_cell_frame_at);
    ADD(k, "drawRect:", mx_draw); ADD(k, "isFlipped", mx_is_flipped); ADD(k, "acceptsFirstResponder", mx_accepts_fr);
    ADD(k, "selectedCell", mx_selected_cell); ADD(k, "selectedCells", mx_selected_cells); ADD(k, "selectedRow", mx_selected_row); ADD(k, "selectedColumn", mx_selected_col);
    ADD(k, "selectCellAtRow:column:", mx_select_cell_at); ADD(k, "selectCellWithTag:", mx_select_cell_with_tag); ADD(k, "selectCell:", mx_select_cell);
    ADD(k, "deselectAllCells", mx_deselect_all); ADD(k, "deselectSelectedCell", mx_deselect_selected); ADD(k, "selectAll:", mx_select_all);
    ADD(k, "setState:atRow:column:", mx_set_state_at); ADD(k, "selectedTag", mx_selected_tag);
    ADD(k, "keyCell", mx_key_cell); ADD(k, "setKeyCell:", mx_set_key_cell); ADD(k, "doubleAction", mx_double_action); ADD(k, "setDoubleAction:", mx_set_double_action);
    ADD(k, "backgroundColor", mx_bg); ADD(k, "setBackgroundColor:", mx_set_bg); ADD(k, "drawsBackground", mx_draws_bg); ADD(k, "setDrawsBackground:", mx_set_draws_bg);
    ADD(k, "cellBackgroundColor", mx_bg); ADD(k, "setCellBackgroundColor:", mx_set_bg); ADD(k, "drawsCellBackground", mx_draws_bg); ADD(k, "setDrawsCellBackground:", mx_set_draws_bg);
    ADD(k, "autosizesCells", mx_autosize); ADD(k, "setAutosizesCells:", mx_set_autosize);
    ADD(k, "delegate", mx_delegate); ADD(k, "setDelegate:", mx_set_delegate); ADD(k, "sendAction", mx_send_action);
    ADD(k, "mouseDown:", mx_mouse_down); ADD(k, "mouseDragged:", mx_mouse_dragged); ADD(k, "mouseUp:", mx_mouse_up); ADD(k, "keyDown:", mx_key_down);
    ADD(k, "sortUsingSelector:", mx_sort);

    k = ensure(&NSSwitch_c, &NSSwitch_m, "NSSwitch", "NSControl");
    ADD(k, "initWithFrame:", sw_init_frame); ADD(k, "state", sw_state); ADD(k, "setState:", sw_set_state);
    ADD(k, "doubleValue", sw_double); ADD(k, "setDoubleValue:", sw_set_double); ADD(k, "integerValue", sw_state); ADD(k, "setIntegerValue:", sw_set_state);
    ADD(k, "objectValue", sw_object); ADD(k, "setObjectValue:", sw_set_object); ADD(k, "intrinsicContentSize", sw_intrinsic);
    ADD(k, "drawRect:", sw_draw); ADD(k, "mouseDown:", sw_mouse_down); ADD(k, "mouseDragged:", sw_mouse_dragged); ADD(k, "mouseUp:", sw_mouse_up);
    ADD(k, "keyDown:", sw_key_down); ADD(k, "performClick:", sw_perform_click); ADD(k, "acceptsFirstResponder", mx_accepts_fr);
    ADD(k, "setControlSize:", sw_set_control_size);
    ADD(k, "accessibilityRole", sw_ax_role); ADD(k, "accessibilitySubrole", sw_ax_subrole); ADD(k, "accessibilityValue", sw_ax_value);
    ADD(k, "accessibilityPerformPress", sw_ax_press);
}
