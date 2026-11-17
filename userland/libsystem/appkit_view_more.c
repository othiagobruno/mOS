/*
 * mOS libSystem — área segura das views e as correspondências do DataDetection.
 *   NSView: safeAreaInsets (a parte da view sob a barra de título de uma janela com conteúdo em tela cheia, pela
 *   contentLayoutRect da janela, mais additionalSafeAreaInsets dos controladores e da própria view), safeAreaRect,
 *   additionalSafeAreaInsets; NSViewController.additionalSafeAreaInsets.
 *   DataDetection (macOS 15): DDMatch (matchedString, range), DDMatchLink (URL), DDMatchEmailAddress
 *   (emailAddress), DDMatchPhoneNumber (phoneNumber), usados pelo NSPasteboard.detectedValues(for:) do overlay Swift.
 * Registro: cx_appkit_view_more_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
typedef struct { double x, y, w, h; } R;
typedef struct { double top, left, bottom, right; } EI;
static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }

static char k_extra;
static EI get_extra(id o) {
    EI z = {0, 0, 0, 0};
    id v = objc_getAssociatedObject(o, &k_extra);
    if (v) ((void (*)(id, SEL, void *))objc_msgSend)(v, sl("getValue:"), &z);
    return z;
}
static void set_extra(id o, EI e) {
    id v = ((id (*)(id, SEL, const void *, const char *))objc_msgSend)((id)kls("NSValue"), sl("valueWithBytes:objCType:"), &e, "{NSEdgeInsets=dddd}");
    objc_setAssociatedObject(o, &k_extra, v, 1);
}
static EI v_extra(id self, SEL c) { (void)c; return get_extra(self); }
static void v_set_extra(id self, SEL c, EI e) { (void)c; set_extra(self, e); MSG(void, self, "setNeedsLayout:", (BOOL)YES); }
/* insets: o que da view fica fora do retângulo de layout do conteúdo da janela, mais os extras pedidos */
static EI v_safe_insets(id self, SEL c) {
    (void)c;
    EI r = {0, 0, 0, 0};
    id win = MSG0(id, self, "window");
    R b = ((R (*)(id, SEL))objc_msgSend)(self, sl("bounds"));
    if (win && responds(win, "contentLayoutRect")) {
        R lay = ((R (*)(id, SEL))objc_msgSend)(win, sl("contentLayoutRect"));
        R v = ((R (*)(id, SEL, R, id))objc_msgSend)(self, sl("convertRect:toView:"), b, (id)0);
        double top = (v.y + v.h) - (lay.y + lay.h), bottom = lay.y - v.y, left = lay.x - v.x, right = (v.x + v.w) - (lay.x + lay.w);
        r.top = top > 0 ? top : 0; r.bottom = bottom > 0 ? bottom : 0; r.left = left > 0 ? left : 0; r.right = right > 0 ? right : 0;
    }
    /* extras da própria view e dos controladores acima (contentViewController) */
    EI e = get_extra(self);
    r.top += e.top; r.left += e.left; r.bottom += e.bottom; r.right += e.right;
    id vc = win && responds(win, "contentViewController") ? MSG0(id, win, "contentViewController") : 0;
    if (vc && MSG0(id, vc, "viewIfLoaded") == self) { EI v = get_extra(vc); r.top += v.top; r.left += v.left; r.bottom += v.bottom; r.right += v.right; }
    return r;
}
static R v_safe_rect(id self, SEL c) {
    (void)c;
    R b = ((R (*)(id, SEL))objc_msgSend)(self, sl("bounds"));
    EI i = v_safe_insets(self, 0);
    BOOL flipped = MSG0(BOOL, self, "isFlipped");
    R r = {b.x + i.left, b.y + (flipped ? i.top : i.bottom), b.w - i.left - i.right, b.h - i.top - i.bottom};
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}
static EI vc_extra(id self, SEL c) { (void)c; return get_extra(self); }
static void vc_set_extra(id self, SEL c, EI e) {
    (void)c;
    set_extra(self, e);
    id v = MSG0(id, self, "viewIfLoaded");
    if (v) MSG(void, v, "setNeedsLayout:", (BOOL)YES);
}

/* ================================================================ DataDetection */
#define CLS(n) cx_class_t n##_c __asm__("OBJC_CLASS_$_" #n), n##_m __asm__("OBJC_METACLASS_$_" #n);
CLS(DDMatch) CLS(DDMatchLink) CLS(DDMatchEmailAddress) CLS(DDMatchPhoneNumber)
static char k_str, k_rng, k_val;
typedef struct { u64 loc, len; } RG;
id cx_ddmatch_new(int kind, id matched, RG range, id value) {
    cx_class_t *k = kind == 1 ? &DDMatchLink_c : kind == 2 ? &DDMatchEmailAddress_c : kind == 3 ? &DDMatchPhoneNumber_c : &DDMatch_c;
    id o = MSG0(id, MSG0(id, (id)k, "alloc"), "init");
    objc_setAssociatedObject(o, &k_str, matched, 3);
    objc_setAssociatedObject(o, &k_rng, ((id (*)(id, SEL, RG))objc_msgSend)((id)kls("NSValue"), sl("valueWithRange:"), range), 1);
    if (value) objc_setAssociatedObject(o, &k_val, value, 1);
    return o;
}
static id dm_str(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_str); }
static RG dm_range(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_rng); RG z = {0, 0}; return v ? MSG0(RG, v, "rangeValue") : z; }
static id dm_val(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_val); }
static id dm_label(id self, SEL c) { (void)self; (void)c; return 0; }

/* NSMenuItem.indentationLevel (0 a 15), lido pelo menu flutuante (appkit_menupop.c) */
static char k_indent;
static i64 mi_indent(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_indent); return v ? MSG0(i64, v, "longLongValue") : 0; }
static void mi_set_indent(id self, SEL c, i64 v) {
    (void)c;
    if (v < 0) v = 0;
    if (v > 15) v = 15;
    objc_setAssociatedObject(self, &k_indent, MSG(id, (id)kls("NSNumber"), "numberWithLongLong:", v), 1);
}
static void def(cx_class_t *k, cx_class_t *m, const char *n, cx_class_t *sup) { if (!cx_class_named(n)) cx_define_class(k, m, n, sup, 0); }
void cx_appkit_view_more_init(void) {
    cx_class_t *v = kls("NSView"), *vc = kls("NSViewController");
    if (v) {
        cx_add_method(v, "safeAreaInsets", (void *)v_safe_insets);
        cx_add_method(v, "safeAreaRect", (void *)v_safe_rect);
        cx_add_method(v, "additionalSafeAreaInsets", (void *)v_extra);
        cx_add_method(v, "setAdditionalSafeAreaInsets:", (void *)v_set_extra);
    }
    if (vc) {
        cx_add_method(vc, "additionalSafeAreaInsets", (void *)vc_extra);
        cx_add_method(vc, "setAdditionalSafeAreaInsets:", (void *)vc_set_extra);
    }
    { cx_class_t *mi = kls("NSMenuItem"); if (mi) { cx_add_method(mi, "indentationLevel", (void *)mi_indent); cx_add_method(mi, "setIndentationLevel:", (void *)mi_set_indent); } }
    def(&DDMatch_c, &DDMatch_m, "DDMatch", kls("NSObject"));
    def(&DDMatchLink_c, &DDMatchLink_m, "DDMatchLink", &DDMatch_c);
    def(&DDMatchEmailAddress_c, &DDMatchEmailAddress_m, "DDMatchEmailAddress", &DDMatch_c);
    def(&DDMatchPhoneNumber_c, &DDMatchPhoneNumber_m, "DDMatchPhoneNumber", &DDMatch_c);
    cx_add_method(&DDMatch_c, "matchedString", (void *)dm_str);
    cx_add_method(&DDMatch_c, "range", (void *)dm_range);
    cx_add_method(&DDMatchLink_c, "URL", (void *)dm_val);
    cx_add_method(&DDMatchEmailAddress_c, "emailAddress", (void *)dm_val);
    cx_add_method(&DDMatchEmailAddress_c, "label", (void *)dm_label);
    cx_add_method(&DDMatchPhoneNumber_c, "phoneNumber", (void *)dm_val);
    cx_add_method(&DDMatchPhoneNumber_c, "label", (void *)dm_label);
}
