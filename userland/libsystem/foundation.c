/*
 * mOS libSystem — Foundation (subconjunto) sobre o runtime de objc.c.
 * As classes são objetos estáticos exportados com os nomes do ABI da Apple
 * (OBJC_CLASS_$_NSString, ...), então os apps ligam nelas como no macOS.
 */
#include "objc.h"
#include "libc.h"
#include "foundation.h"

#define CLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
CLASS(NSObject) CLASS(NSString) CLASS(NSMutableString) CLASS(NSNumber) CLASS(NSArray) CLASS(NSMutableArray)
CLASS(NSDictionary) CLASS(NSMutableDictionary) CLASS(NSAutoreleasePool) CLASS(NSProcessInfo) CLASS(NSDate)
CLASS(NSTimer) CLASS(NSRunLoop) CLASS(NSNotificationCenter) CLASS(NSNotification) CLASS(NSThread)
CLASS(NSUserDefaults) CLASS(NSBundle) CLASS(NSFileManager) CLASS(NSData) CLASS(NSCharacterSet) CLASS(NSValue)
static id cx_autorelease(id o);
CLASS(NSError) CLASS(NSSet) CLASS(NSMutableSet) CLASS(NSNull) CLASS(NSException)
cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
static cx_class_t NSConstantString_m;
cx_class_t NSConstantStringObjC_c __asm__("OBJC_CLASS_$_NSConstantString");
static cx_class_t NSConstantStringObjC_m;

/* ================================================================ strings */
id cx_str_n(const char *s, u64 n) {
    nsstr_t *o = (nsstr_t *)cx_alloc_instance(&NSString_c);
    o->s = malloc(n + 1);
    memcpy(o->s, s, n);
    o->s[n] = 0;
    o->n = n;
    return (id)o;
}
id cx_str(const char *s) { return s ? cx_str_n(s, strlen(s)) : 0; }

BOOL cx_is_str(id o) { return o && cx_is_subclass(o->isa, &NSString_c); }

const char *cx_cstr(id o) {
    if (!o) return "(null)";
    if (cx_is_str(o)) return ((nsstr_t *)o)->s ? ((nsstr_t *)o)->s : "";
    id d = MSG0(id, o, "description");
    return cx_is_str(d) ? ((nsstr_t *)d)->s : "?";
}

/* usado pelo printf para "%@" */
const char *cx_describe(void *o) { return cx_cstr((id)o); }

static u64 utf16_len(const char *s, u64 n) {
    u64 k = 0;
    for (u64 i = 0; i < n; i++) {
        u8 c = (u8)s[i];
        if ((c & 0xC0) != 0x80) k += c >= 0xF0 ? 2 : 1;
    }
    return k;
}
/* posição em bytes do índice UTF-16 idx */
static u64 byte_at(const char *s, u64 n, u64 idx) {
    u64 k = 0, i = 0;
    while (i < n && k < idx) {
        u8 c = (u8)s[i];
        k += c >= 0xF0 ? 2 : 1;
        i++;
        while (i < n && ((u8)s[i] & 0xC0) == 0x80) i++;
    }
    return i;
}

static id s_string(id cls, SEL c) { (void)c; id o = cx_str(""); if (cls != (id)&NSString_c) o->isa = (cx_class_t *)cls; return o; }
static id s_with_utf8(id cls, SEL c, const char *s) { (void)c; id o = cx_str(s); if (o && cls != (id)&NSString_c) o->isa = (cx_class_t *)cls; return o; }
static id s_with_string(id cls, SEL c, id s) { return s_with_utf8(cls, c, cx_cstr(s)); }
static id s_alloc(id cls, SEL c) { (void)c; nsstr_t *o = (nsstr_t *)cx_alloc_instance((cx_class_t *)cls); o->s = strdup(""); return (id)o; }
static void str_set(id self, const char *s, u64 n) { nsstr_t *o = (nsstr_t *)self; o->s = malloc(n + 1); memcpy(o->s, s, n); o->s[n] = 0; o->n = n; }
static id i_init_utf8(id self, SEL c, const char *s) { (void)c; if (!s) return 0; str_set(self, s, strlen(s)); return self; }
static id i_init_string(id self, SEL c, id s) { (void)c; const char *t = cx_cstr(s); str_set(self, t, strlen(t)); return self; }
static u64 utf8_put(char *o, u32 cp) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xC0 | cp >> 6); o[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) { o[0] = (char)(0xE0 | cp >> 12); o[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); o[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
    o[0] = (char)(0xF0 | cp >> 18); o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    o[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); o[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}
/* NSUTF16StringEncoding = 10, NSUTF16LE = 0x94000100, NSUTF16BE = 0x90000100.
 * CF: UTF-16 = 0x0100, UTF-16LE = 0x14000100, UTF-16BE = 0x10000100.
 * be: 0 little, 1 big, -1 BOM (senão little, que é o nativo daqui). */
static int enc_is_utf16(u64 enc, int *be) {
    if (enc == 10 || enc == 0x0100) { *be = -1; return 1; }
    if (enc == 0x94000100ull || enc == 0x14000100ull) { *be = 0; return 1; }
    if (enc == 0x90000100ull || enc == 0x10000100ull) { *be = 1; return 1; }
    return 0;
}
static id i_init_bytes(id self, SEL c, const void *b, u64 n, u64 enc) {
    (void)c;
    if (!b) return 0;
    int be = 0;
    if (!enc_is_utf16(enc, &be)) { str_set(self, b, n); return self; }
    const u8 *p = b;
    u64 i = 0;
    if (be < 0) {
        be = 0;
        if (n >= 2) {
            u16 bom = (u16)p[0] | ((u16)p[1] << 8);
            if (bom == 0xFEFF) i = 2;
            else if (bom == 0xFFFE) { be = 1; i = 2; }
        }
    }
    char *out = malloc(n * 2 + 1);
    if (!out) return 0;
    u64 o = 0;
    while (i + 1 < n) {
        u16 u = be ? (u16)((p[i] << 8) | p[i + 1]) : (u16)(p[i] | (p[i + 1] << 8));
        i += 2;
        u32 cp = u;
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n) {
            u16 u2 = be ? (u16)((p[i] << 8) | p[i + 1]) : (u16)(p[i] | (p[i + 1] << 8));
            if (u2 >= 0xDC00 && u2 <= 0xDFFF) { i += 2; cp = 0x10000 + (((u32)(u - 0xD800) << 10) | (u32)(u2 - 0xDC00)); }
        }
        if (cp >= 0x110000) continue;
        o += utf8_put(out + o, cp);
    }
    out[o] = 0;
    str_set(self, out, o);
    free(out);
    return self;
}
static id i_init_chars(id self, SEL c, const void *ch, u64 n) { return i_init_bytes(self, c, ch, n * 2, 0x94000100ull); }
static id i_init_chars_free(id self, SEL c, void *ch, u64 n, BOOL free_it) {
    id r = i_init_chars(self, c, ch, n);
    if (free_it && ch) free(ch);
    return r;
}
static id s_with_chars(id cls, SEL c, const void *ch, u64 n) {
    id o = i_init_chars(MSG0(id, cls, "alloc"), c, ch, n);
    return o ? cx_autorelease(o) : 0;
}
static id i_init_cstr(id self, SEL c, const char *s, u64 enc) { (void)enc; return i_init_utf8(self, c, s); }
static id i_init(id self, SEL c) { (void)c; return self; }

id cx_str_vfmt(id fmt, va_list ap) {
    char *out = 0;
    vasprintf(&out, cx_cstr(fmt), ap);
    id o = cx_str(out ? out : "");
    free(out);
    return o;
}
static id s_with_format(id cls, SEL c, id fmt, ...) {
    (void)c;
    va_list ap;
    va_start(ap, fmt);
    id o = cx_str_vfmt(fmt, ap);
    va_end(ap);
    if (cls != (id)&NSString_c) o->isa = (cx_class_t *)cls;
    return cx_autorelease(o);
}
static id i_init_format(id self, SEL c, id fmt, ...) {
    (void)c;
    va_list ap;
    va_start(ap, fmt);
    id o = cx_str_vfmt(fmt, ap);
    va_end(ap);
    str_set(self, ((nsstr_t *)o)->s, ((nsstr_t *)o)->n);
    return self;
}
static id i_init_format_args(id self, SEL c, id fmt, va_list ap) {
    (void)c;
    id o = cx_str_vfmt(fmt, ap);
    str_set(self, ((nsstr_t *)o)->s, ((nsstr_t *)o)->n);
    return self;
}

#define S(o) (((nsstr_t *)(o))->s)
#define N(o) (((nsstr_t *)(o))->n)
static const char *i_utf8(id self, SEL c) { (void)c; return S(self); }
static const char *i_cstr_enc(id self, SEL c, u64 e) { (void)c; (void)e; return S(self); }
static u64 i_length(id self, SEL c) { (void)c; return utf16_len(S(self), N(self)); }
static u64 i_lenbytes(id self, SEL c, u64 enc) { (void)c; (void)enc; return N(self); }
static u16 i_char_at(id self, SEL c, u64 idx) {
    (void)c;
    u64 b = byte_at(S(self), N(self), idx);
    const char *p = S(self) + b;
    u32 cp = (u8)*p;
    if (cp >= 0xC0) {
        int extra = cp >= 0xF0 ? 3 : cp >= 0xE0 ? 2 : 1;
        cp &= 0x3F >> extra;
        for (int k = 1; k <= extra; k++) cp = cp << 6 | ((u8)p[k] & 0x3F);
    }
    return (u16)(cp > 0xFFFF ? 0xFFFD : cp);
}
static BOOL i_eq_str(id self, SEL c, id o) { (void)c; return o && cx_is_str(o) && N(self) == N(o) && !memcmp(S(self), S(o), N(self)); }
static u64 i_str_hash(id self, SEL c) { (void)c; u64 h = 1469598103934665603ULL; for (u64 i = 0; i < N(self); i++) { h ^= (u8)S(self)[i]; h *= 1099511628211ULL; } return h; }
static i64 i_compare(id self, SEL c, id o) { (void)c; int r = strcmp(S(self), cx_cstr(o)); return r < 0 ? -1 : r > 0; }
static i64 i_casecmp(id self, SEL c, id o) { (void)c; int r = strcasecmp(S(self), cx_cstr(o)); return r < 0 ? -1 : r > 0; }
static BOOL i_has_prefix(id self, SEL c, id p) { (void)c; const char *t = cx_cstr(p); return !strncmp(S(self), t, strlen(t)); }
static BOOL i_has_suffix(id self, SEL c, id p) { (void)c; const char *t = cx_cstr(p); u64 l = strlen(t); return l <= N(self) && !memcmp(S(self) + N(self) - l, t, l); }
static BOOL i_contains(id self, SEL c, id p) { (void)c; return strstr(S(self), cx_cstr(p)) != 0; }
typedef struct { u64 loc, len; } nsrange_t;
static nsrange_t i_range_of(id self, SEL c, id p) {
    (void)c;
    const char *f = strstr(S(self), cx_cstr(p));
    nsrange_t r = {0x7FFFFFFFFFFFFFFFULL, 0};
    if (f) { r.loc = utf16_len(S(self), (u64)(f - S(self))); r.len = utf16_len(cx_cstr(p), strlen(cx_cstr(p))); }
    return r;
}
static id i_append(id self, SEL c, id o) {
    (void)c;
    const char *t = cx_cstr(o);
    u64 l = strlen(t);
    char *b = malloc(N(self) + l + 1);
    memcpy(b, S(self), N(self));
    memcpy(b + N(self), t, l + 1);
    id r = cx_str_n(b, N(self) + l);
    free(b);
    return r;
}
static id i_append_fmt(id self, SEL c, id fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    id o = cx_str_vfmt(fmt, ap);
    va_end(ap);
    return i_append(self, c, o);
}
static id i_substr_from(id self, SEL c, u64 i) { (void)c; u64 b = byte_at(S(self), N(self), i); return cx_str_n(S(self) + b, N(self) - b); }
static id i_substr_to(id self, SEL c, u64 i) { (void)c; u64 b = byte_at(S(self), N(self), i); return cx_str_n(S(self), b); }
static id i_substr_range(id self, SEL c, nsrange_t r) {
    (void)c;
    u64 a = byte_at(S(self), N(self), r.loc), b = byte_at(S(self), N(self), r.loc + r.len);
    return cx_str_n(S(self) + a, b - a);
}
static id map_case(id self, int up, int cap) {
    id r = cx_str_n(S(self), N(self));
    int start = 1;
    for (u64 i = 0; i < N(r); i++) {
        char ch = S(r)[i];
        if (cap) { S(r)[i] = (char)(start ? toupper(ch) : tolower(ch)); start = isspace((u8)ch); }
        else S(r)[i] = (char)(up ? toupper(ch) : tolower(ch));
    }
    return r;
}
static id i_upper(id self, SEL c) { (void)c; return map_case(self, 1, 0); }
static id i_lower(id self, SEL c) { (void)c; return map_case(self, 0, 0); }
static id i_capital(id self, SEL c) { (void)c; return map_case(self, 0, 1); }
static id i_replace(id self, SEL c, id a, id b) {
    (void)c;
    const char *x = cx_cstr(a), *y = cx_cstr(b);
    u64 xl = strlen(x), yl = strlen(y);
    if (!xl) return self;
    char *out = 0;
    size_t cap = 0;
    u64 o = 0;
    for (const char *p = S(self); *p;) {
        const char *src;
        u64 n;
        if (!strncmp(p, x, xl)) { src = y; n = yl; p += xl; } else { src = p; n = 1; p++; }
        if (o + n + 1 > cap) { cap = (o + n + 1) * 2; out = realloc(out, cap); }
        memcpy(out + o, src, n);
        o += n;
    }
    id r = cx_str_n(out ? out : "", o);
    free(out);
    return r;
}
static id i_components(id self, SEL c, id sep) {
    (void)c;
    id arr = MSG0(id, &NSMutableArray_c, "array");
    const char *t = cx_cstr(sep), *p = S(self);
    u64 tl = strlen(t);
    for (;;) {
        const char *f = tl ? strstr(p, t) : 0;
        if (!f) { MSG(void, arr, "addObject:", cx_str(p)); break; }
        MSG(void, arr, "addObject:", cx_str_n(p, (u64)(f - p)));
        p = f + tl;
    }
    return arr;
}
static id i_trim(id self, SEL c, id set) {
    (void)c; (void)set;
    const char *a = S(self), *b = S(self) + N(self);
    while (a < b && isspace((u8)*a)) a++;
    while (b > a && isspace((u8)b[-1])) b--;
    return cx_str_n(a, (u64)(b - a));
}
static int i_int(id self, SEL c) { (void)c; return atoi(S(self)); }
static i64 i_ll(id self, SEL c) { (void)c; return strtoll(S(self), 0, 10); }
static double i_double(id self, SEL c) { (void)c; return strtod(S(self), 0); }
static float i_float(id self, SEL c) { (void)c; return (float)strtod(S(self), 0); }
static BOOL i_bool(id self, SEL c) { (void)c; char ch = (char)toupper((u8)S(self)[0]); return ch == 'Y' || ch == 'T' || (ch >= '1' && ch <= '9'); }
static id i_self(id self, SEL c) { (void)c; return self; }
static id i_str_copy(id self, SEL c) { (void)c; return cx_str_n(S(self), N(self)); }
static id i_str_mcopy(id self, SEL c) { (void)c; id r = cx_str_n(S(self), N(self)); r->isa = &NSMutableString_c; return r; }
static id i_last_path(id self, SEL c) { (void)c; const char *s = strrchr(S(self), '/'); return cx_str(s && s[1] ? s + 1 : S(self)); }
static id i_path_ext(id self, SEL c) { (void)c; const char *b = strrchr(S(self), '/'), *d = strrchr(S(self), '.'); return cx_str(d && (!b || d > b) ? d + 1 : ""); }
static id i_del_last(id self, SEL c) { (void)c; const char *s = strrchr(S(self), '/'); return s ? cx_str_n(S(self), s == S(self) ? 1 : (u64)(s - S(self))) : cx_str(""); }
static id i_append_path(id self, SEL c, id comp) {
    (void)c;
    const char *t = cx_cstr(comp);
    char *b = malloc(N(self) + strlen(t) + 2);
    strcpy(b, S(self));
    if (N(self) && S(self)[N(self) - 1] != '/') strcat(b, "/");
    strcat(b, t);
    id r = cx_str(b);
    free(b);
    return r;
}
static BOOL i_write_file(id self, SEL c, id path, BOOL atom, u64 enc, id *err) {
    (void)c; (void)atom; (void)enc; (void)err;
    FILE_ *f = fopen(cx_cstr(path), "w");
    if (!f) return NO;
    fwrite(S(self), 1, N(self), f);
    fclose(f);
    return YES;
}
static id s_with_file(id cls, SEL c, id path, u64 enc, id *err) {
    (void)cls; (void)c; (void)enc; (void)err;
    FILE_ *f = fopen(cx_cstr(path), "r");
    if (!f) return 0;
    char *buf = 0;
    size_t cap = 0, n = 0, k;
    char tmp[4096];
    while ((k = fread(tmp, 1, sizeof tmp, f)) > 0) {
        if (n + k + 1 > cap) { cap = (n + k + 1) * 2; buf = realloc(buf, cap); }
        memcpy(buf + n, tmp, k);
        n += k;
    }
    fclose(f);
    id r = cx_str_n(buf ? buf : "", n);
    free(buf);
    return r;
}
static id i_init_file(id self, SEL c, id path, u64 enc, id *err) {
    id r = s_with_file(0, c, path, enc, err);
    if (!r) return 0;
    str_set(self, S(r), N(r));
    return self;
}
/* NSMutableString */
static void m_append(id self, SEL c, id o) {
    (void)c;
    const char *t = cx_cstr(o);
    u64 l = strlen(t);
    char *b = malloc(N(self) + l + 1);
    memcpy(b, S(self), N(self));
    memcpy(b + N(self), t, l + 1);
    S(self) = b;
    N(self) += l;
}
static void m_append_fmt(id self, SEL c, id fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    id o = cx_str_vfmt(fmt, ap);
    va_end(ap);
    m_append(self, c, o);
}
static void m_set(id self, SEL c, id o) { (void)c; const char *t = cx_cstr(o); str_set(self, t, strlen(t)); }
static void m_insert(id self, SEL c, id o, u64 idx) {
    (void)c;
    u64 b = byte_at(S(self), N(self), idx);
    const char *t = cx_cstr(o);
    u64 l = strlen(t);
    char *nb = malloc(N(self) + l + 1);
    memcpy(nb, S(self), b);
    memcpy(nb + b, t, l);
    memcpy(nb + b + l, S(self) + b, N(self) - b + 1);
    S(self) = nb;
    N(self) += l;
}
static void m_delete(id self, SEL c, nsrange_t r) {
    (void)c;
    u64 a = byte_at(S(self), N(self), r.loc), b = byte_at(S(self), N(self), r.loc + r.len);
    memmove(S(self) + a, S(self) + b, N(self) - b + 1);
    N(self) -= b - a;
}
static id s_mstring(id cls, SEL c) { (void)c; id o = cx_str(""); o->isa = (cx_class_t *)cls; return o; }
static id s_mcap(id cls, SEL c, u64 cap) { (void)cap; return s_mstring(cls, c); }

/* ================================================================ NSObject */
static id o_alloc(id cls, SEL c) { (void)c; return cx_alloc_instance((cx_class_t *)cls); }
static id o_alloc_zone(id cls, SEL c, void *z) { (void)z; return o_alloc(cls, c); }
static id o_new(id cls, SEL c) { (void)c; return MSG0(id, MSG0(id, cls, "alloc"), "init"); }
static id o_class_self(id cls, SEL c) { (void)c; return cls; }
static cx_class_t *o_class(id self, SEL c) { (void)c; return self->isa; }
static cx_class_t *o_superclass(id self, SEL c) { (void)c; return self->isa->super; }
static cx_class_t *c_superclass(id cls, SEL c) { (void)c; return ((cx_class_t *)cls)->super; }
static BOOL o_is_kind(id self, SEL c, cx_class_t *k) { (void)c; return cx_is_subclass(self->isa, k); }
static BOOL o_is_member(id self, SEL c, cx_class_t *k) { (void)c; return self->isa == k; }
static BOOL c_is_subclass(id cls, SEL c, cx_class_t *k) { (void)c; return cx_is_subclass((cx_class_t *)cls, k); }
static BOOL o_responds(id self, SEL c, SEL s) { (void)c; return cx_responds(self->isa, s); }
static BOOL c_instances_respond(id cls, SEL c, SEL s) { (void)c; return cx_responds((cx_class_t *)cls, s); }
/* protocolos de verdade (objcrt.c); as classes nativas do mOS não declaram protocolos, então seguem dizendo YES */
static BOOL o_conforms(id self, SEL c, void *p) {
    (void)c;
    extern BOOL class_conformsToProtocol(cx_class_t *cls, void *p);
    extern int cx_class_is_builtin(cx_class_t *cls);
    extern BOOL class_isMetaClass(cx_class_t *cls);
    cx_class_t *k = object_getClass(self);
    if (k && class_isMetaClass(k)) k = (cx_class_t *)self;   /* +conformsToProtocol: (self é a classe) */
    return class_conformsToProtocol(k, p) || cx_class_is_builtin(k) ? YES : NO;
}
static id o_perform(id self, SEL c, SEL s) { (void)c; return ((id (*)(id, SEL))objc_msgSend)(self, s); }
static id o_perform1(id self, SEL c, SEL s, id a) { (void)c; return ((id (*)(id, SEL, id))objc_msgSend)(self, s, a); }
static id o_perform2(id self, SEL c, SEL s, id a, id b) { (void)c; return ((id (*)(id, SEL, id, id))objc_msgSend)(self, s, a, b); }
static void o_perform_main(id self, SEL c, SEL s, id a, BOOL w) { (void)c; (void)w; ((void (*)(id, SEL, id))objc_msgSend)(self, s, a); }
static id o_description(id self, SEL c) {
    (void)c;
    char b[96];
    snprintf(b, sizeof b, "<%s: %p>", cx_class_name(self->isa), (void *)self);
    return cx_str(b);
}
static id c_description(id cls, SEL c) { (void)c; return cx_str(cx_class_name((cx_class_t *)cls)); }
static u64 o_hash(id self, SEL c) { (void)c; return (u64)self >> 4; }
static BOOL o_is_equal(id self, SEL c, id o) { (void)c; return self == o; }
static id o_copy(id self, SEL c) { (void)c; return cx_responds(self->isa, sel_registerName("copyWithZone:")) ? MSG(id, self, "copyWithZone:", (void *)0) : self; }
static id o_mcopy(id self, SEL c) { (void)c; return cx_responds(self->isa, sel_registerName("mutableCopyWithZone:")) ? MSG(id, self, "mutableCopyWithZone:", (void *)0) : self; }
extern id objc_retain(id), objc_autorelease(id);
extern void objc_release(id), cx_object_free(id);
extern id objc_autorelease(id);
static id cx_autorelease(id o) { return o ? objc_autorelease(o) : o; }
extern i32 cx_retain_count(id);
static u64 o_retain_count(id self, SEL c) { (void)c; return (u64)cx_retain_count(self); }
static id o_retain(id self, SEL c) { (void)c; return objc_retain(self); }
static void o_release(id self, SEL c) { (void)c; objc_release(self); }
static id o_autorelease(id self, SEL c) { (void)c; return objc_autorelease(self); }
static void nc_remove_all(id target);
static void o_dealloc(id self, SEL c) { (void)c; nc_remove_all(self); cx_object_free(self); }
static void o_void(id self, SEL c) { (void)self; (void)c; }
static void *o_zone(id self, SEL c) { (void)self; (void)c; return 0; }
static IMP o_method_for(id self, SEL c, SEL s) { (void)c; return cx_lookup(self->isa, s); }

/* KVC simples: getter/setter ou ivar "_chave"/"chave" */
/* KVC com conversão de escalares pela codificação de tipos do método (BOOL/int/double viram NSNumber) */
extern const char *cx_method_types(cx_class_t *c, SEL sel);
static const char *kv_skipq(const char *t) { while (t && *t && strchr("rnNoORV", *t)) t++; return t; }
static const char *kv_arg(const char *t, int n) {   /* tipo do n-ésimo item (0 = retorno) */
    for (int i = 0; t && *t && i < n; i++) {
        t = kv_skipq(t);
        if (*t == '{') { int d = 0; do { if (*t == '{') d++; else if (*t == '}') d--; t++; } while (*t && d); }
        else if (*t == '^') { t++; continue; }
        else t++;
        while (*t >= '0' && *t <= '9') t++;
    }
    return kv_skipq(t);
}
extern id cx_num_bool(BOOL b);
static id o_value_for_key(id self, SEL c, id key) {
    (void)c;
    const char *k = cx_cstr(key);
    if (!strcmp(k, "self")) return self;
    SEL g = sel_registerName(k);
    if (!cx_responds(self->isa, g)) {
        char isk[140];
        snprintf(isk, sizeof isk, "is%c%s", toupper((u8)k[0]), k + 1);
        if (cx_responds(self->isa, sel_registerName(isk))) g = sel_registerName(isk);
    }
    if (cx_responds(self->isa, g)) {
        const char *t = kv_arg(cx_method_types(self->isa, g), 0);
        switch (t ? *t : '@') {
        case 'c': case 'B': return cx_num_bool(((BOOL (*)(id, SEL))objc_msgSend)(self, g));
        case 'C': case 's': case 'S': case 'i': case 'I': return cx_num_i(((int (*)(id, SEL))objc_msgSend)(self, g));
        case 'l': case 'q': case 'L': case 'Q': return cx_num_i(((i64 (*)(id, SEL))objc_msgSend)(self, g));
        case 'f': return cx_num_d(((float (*)(id, SEL))objc_msgSend)(self, g));
        case 'd': return cx_num_d(((double (*)(id, SEL))objc_msgSend)(self, g));
        case 'v': return 0;
        default: return ((id (*)(id, SEL))objc_msgSend)(self, g);
        }
    }
    char nm[128];
    snprintf(nm, sizeof nm, "_%s", k);
    extern void *class_getInstanceVariable(cx_class_t *, const char *);
    extern id object_getIvar(id, void *);
    void *iv = class_getInstanceVariable(self->isa, nm);
    if (!iv) iv = class_getInstanceVariable(self->isa, k);
    return iv ? object_getIvar(self, iv) : 0;
}
static void o_set_value_for_key(id self, SEL c, id v, id key) {
    (void)c;
    const char *k = cx_cstr(key);
    char nm[140];
    snprintf(nm, sizeof nm, "set%c%s:", toupper((u8)k[0]), k + 1);
    SEL s = sel_registerName(nm);
    if (cx_responds(self->isa, s)) {
        const char *t = kv_arg(cx_method_types(self->isa, s), 3);
        switch (t ? *t : '@') {
        case 'c': case 'B': case 'C': case 's': case 'S': case 'i': case 'I':
            ((void (*)(id, SEL, int))objc_msgSend)(self, s, v ? MSG0(int, v, "intValue") : 0); return;
        case 'l': case 'q': case 'L': case 'Q':
            ((void (*)(id, SEL, i64))objc_msgSend)(self, s, v ? MSG0(i64, v, "longLongValue") : 0); return;
        case 'f': ((void (*)(id, SEL, float))objc_msgSend)(self, s, v ? (float)MSG0(double, v, "doubleValue") : 0); return;
        case 'd': ((void (*)(id, SEL, double))objc_msgSend)(self, s, v ? MSG0(double, v, "doubleValue") : 0); return;
        default: ((void (*)(id, SEL, id))objc_msgSend)(self, s, v); return;
        }
    }
    snprintf(nm, sizeof nm, "_%s", k);
    extern void *class_getInstanceVariable(cx_class_t *, const char *);
    extern void object_setIvar(id, void *, id);
    void *iv = class_getInstanceVariable(self->isa, nm);
    if (!iv) iv = class_getInstanceVariable(self->isa, k);
    if (iv) { objc_retain(v); object_setIvar(self, iv, v); }
}
/* caminhos "a.b.c" */
static id o_value_for_path(id self, SEL c, id path) {
    const char *p = cx_cstr(path);
    id o = self;
    char part[160];
    while (o && *p) {
        int n = 0;
        while (*p && *p != '.' && n < 159) part[n++] = *p++;
        part[n] = 0;
        if (*p == '.') p++;
        o = MSG(id, o, "valueForKey:", cx_str(part));
    }
    (void)c;
    return o;
}
static void o_set_value_for_path(id self, SEL c, id v, id path) {
    (void)c;
    const char *p = cx_cstr(path), *last = strrchr(p, '.');
    if (!last) { MSG(void, self, "setValue:forKey:", v, path); return; }
    id owner = o_value_for_path(self, c, cx_str_n(p, (u64)(last - p)));
    if (owner) MSG(void, owner, "setValue:forKey:", v, cx_str(last + 1));
}

/* ================================================================ NSNumber */
typedef struct { cx_class_t *isa; u32 kind; u32 pad; double d; i64 i; } nsnum_t;  /* kind: 0 int, 1 double, 2 bool */
id cx_num_i(i64 v) { nsnum_t *n = (nsnum_t *)cx_alloc_instance(&NSNumber_c); n->kind = 0; n->i = v; n->d = (double)v; return (id)n; }
id cx_num_d(double v) { nsnum_t *n = (nsnum_t *)cx_alloc_instance(&NSNumber_c); n->kind = 1; n->d = v; n->i = (i64)v; return (id)n; }
/* __kCFBooleanTrue/False são os PRÓPRIOS objetos (o clang compila @YES/@NO como &__kCFBooleanTrue);
 * kCFBooleanTrue/False são ponteiros para eles. Antes os dois primeiros eram ponteiros, e @YES virava um
 * objeto com isa inválido (o Electron cai em [CATransaction setValue:@YES ...]; sessão do VS Code). */
nsnum_t kcf_true __asm__("__kCFBooleanTrue") = {&NSNumber_c, 2, 0, 1, 1};
nsnum_t kcf_false __asm__("__kCFBooleanFalse") = {&NSNumber_c, 2, 0, 0, 0};
id cx_num_bool(BOOL b) { return (id)(b ? &kcf_true : &kcf_false); }
void *kCFBooleanTrue = &kcf_true, *kCFBooleanFalse = &kcf_false;
static id n_int(id cls, SEL c, i64 v) { (void)cls; (void)c; return cx_num_i(v); }
static id n_uint(id cls, SEL c, u64 v) { (void)cls; (void)c; return cx_num_i((i64)v); }
static id n_int32(id cls, SEL c, int v) { (void)cls; (void)c; return cx_num_i(v); }
static id n_double(id cls, SEL c, double v) { (void)cls; (void)c; return cx_num_d(v); }
static id n_float(id cls, SEL c, float v) { (void)cls; (void)c; return cx_num_d(v); }
static id n_bool(id cls, SEL c, BOOL v) { (void)cls; (void)c; return (id)(v ? &kcf_true : &kcf_false); }
static id n_init_int(id self, SEL c, i64 v) { (void)c; nsnum_t *n = (nsnum_t *)self; n->kind = 0; n->i = v; n->d = (double)v; return self; }
static id n_init_double(id self, SEL c, double v) { (void)c; nsnum_t *n = (nsnum_t *)self; n->kind = 1; n->d = v; n->i = (i64)v; return self; }
static i64 n_ival(id self, SEL c) { (void)c; return ((nsnum_t *)self)->i; }
static int n_ival32(id self, SEL c) { (void)c; return (int)((nsnum_t *)self)->i; }
static double n_dval(id self, SEL c) { (void)c; return ((nsnum_t *)self)->d; }
static float n_fval(id self, SEL c) { (void)c; return (float)((nsnum_t *)self)->d; }
static BOOL n_bval(id self, SEL c) { (void)c; return ((nsnum_t *)self)->i != 0 || ((nsnum_t *)self)->d != 0; }
static id n_string(id self, SEL c) {
    (void)c;
    nsnum_t *n = (nsnum_t *)self;
    char b[64];
    if (n->kind == 1) snprintf(b, sizeof b, "%.15g", n->d);
    else snprintf(b, sizeof b, "%lld", n->i);
    return cx_str(b);
}
static BOOL n_eq(id self, SEL c, id o) {
    (void)c;
    if (!o || !cx_is_subclass(o->isa, &NSNumber_c)) return NO;
    return ((nsnum_t *)self)->d == ((nsnum_t *)o)->d && ((nsnum_t *)self)->i == ((nsnum_t *)o)->i;
}
static u64 n_hash(id self, SEL c) { (void)c; return (u64)((nsnum_t *)self)->i * 2654435761u; }
static i64 n_cmp(id self, SEL c, id o) { (void)c; double a = ((nsnum_t *)self)->d, b = ((nsnum_t *)o)->d; return a < b ? -1 : a > b; }

/* ================================================================ NSArray */
id cx_arr_new(cx_class_t *cls) { return cx_alloc_instance(cls ? cls : &NSMutableArray_c); }
void cx_arr_add(id a, id o) {
    nsarr_t *r = (nsarr_t *)a;
    if (r->n == r->cap) { r->cap = r->cap ? r->cap * 2 : 8; r->items = realloc(r->items, r->cap * sizeof(id)); }
    r->items[r->n++] = objc_retain(o);
}
static void arr_dealloc(id self, SEL c) { nsarr_t *r = (nsarr_t *)self; for (u64 i = 0; i < r->n; i++) objc_release(r->items[i]); free(r->items); o_dealloc(self, c); }
static void dict_dealloc(id self, SEL c) {
    nsdict_t *d = (nsdict_t *)self;
    for (u64 i = 0; i < d->n; i++) { objc_release(d->keys[i]); objc_release(d->vals[i]); }
    free(d->keys); free(d->vals);
    o_dealloc(self, c);
}
static void str_dealloc(id self, SEL c) { nsstr_t *s = (nsstr_t *)self; if (!(s->flags & 0x7C0)) free(s->s); o_dealloc(self, c); }
static id a_array(id cls, SEL c) { (void)c; return cx_arr_new((cx_class_t *)cls); }
static id a_cap(id cls, SEL c, u64 n) { (void)n; return a_array(cls, c); }
static id a_with_obj(id cls, SEL c, id o) { id a = a_array(cls, c); cx_arr_add(a, o); return a; }
static id a_with_objs_count(id cls, SEL c, const id *objs, u64 n) { id a = a_array(cls, c); for (u64 i = 0; i < n; i++) cx_arr_add(a, objs[i]); return a; }
static id a_with_objs(id cls, SEL c, id first, ...) {
    id a = a_array(cls, c);
    va_list ap;
    va_start(ap, first);
    for (id o = first; o; o = va_arg(ap, id)) cx_arr_add(a, o);
    va_end(ap);
    return cx_autorelease(a);
}
static id a_with_array(id cls, SEL c, id o) { id a = a_array(cls, c); nsarr_t *s = (nsarr_t *)o; for (u64 i = 0; o && i < s->n; i++) cx_arr_add(a, s->items[i]); return a; }
static id a_init_objs_count(id self, SEL c, const id *objs, u64 n) { (void)c; for (u64 i = 0; i < n; i++) cx_arr_add(self, objs[i]); return self; }
static id a_init_array(id self, SEL c, id o) { (void)c; nsarr_t *s = (nsarr_t *)o; for (u64 i = 0; o && i < s->n; i++) cx_arr_add(self, s->items[i]); return self; }
static u64 a_count(id self, SEL c) { (void)c; return ((nsarr_t *)self)->n; }
static id a_at(id self, SEL c, u64 i) {
    nsarr_t *r = (nsarr_t *)self;
    if (i >= r->n) { cx_fdprintf(2, "*** -[NSArray objectAtIndex:]: índice %llu além do fim (%llu)\n", (unsigned long long)i, (unsigned long long)r->n); (void)c; abort(); }
    return r->items[i];
}
static id a_first(id self, SEL c) { (void)c; nsarr_t *r = (nsarr_t *)self; return r->n ? r->items[0] : 0; }
static id a_last(id self, SEL c) { (void)c; nsarr_t *r = (nsarr_t *)self; return r->n ? r->items[r->n - 1] : 0; }
static BOOL obj_eq(id a, id b) { return a == b || (a && b && MSG(BOOL, a, "isEqual:", b)); }
static u64 a_index_of(id self, SEL c, id o) { (void)c; nsarr_t *r = (nsarr_t *)self; for (u64 i = 0; i < r->n; i++) if (obj_eq(r->items[i], o)) return i; return 0x7FFFFFFFFFFFFFFFULL; }
static BOOL a_contains(id self, SEL c, id o) { return a_index_of(self, c, o) != 0x7FFFFFFFFFFFFFFFULL; }
static id a_join(id self, SEL c, id sep) {
    (void)c;
    nsarr_t *r = (nsarr_t *)self;
    id out = cx_str("");
    for (u64 i = 0; i < r->n; i++) {
        if (i) m_append(out, 0, sep);
        m_append(out, 0, MSG0(id, r->items[i], "description"));
    }
    return out;
}
static id a_description(id self, SEL c) {
    (void)c;
    nsarr_t *r = (nsarr_t *)self;
    id out = cx_str("(\n");
    for (u64 i = 0; i < r->n; i++) {
        m_append(out, 0, cx_str("    "));
        m_append(out, 0, MSG0(id, r->items[i], "description"));
        m_append(out, 0, cx_str(i + 1 < r->n ? ",\n" : "\n"));
    }
    m_append(out, 0, cx_str(")"));
    return out;
}
typedef struct { u64 state; id *items; u64 *mut; u64 extra[5]; } fastenum_t;
static u64 mutations;
static u64 a_fast(id self, SEL c, fastenum_t *st, id *buf, u64 len) {
    (void)c; (void)buf; (void)len;
    nsarr_t *r = (nsarr_t *)self;
    if (st->state) return 0;
    st->state = 1;
    st->items = r->items;
    st->mut = &r->n;   /* por objeto: qualquer inserção/remoção muda a contagem */
    return r->n;
}
static id a_adding(id self, SEL c, id o) { id a = a_with_array((id)&NSArray_c, c, self); cx_arr_add(a, o); return a; }
static id a_copy(id self, SEL c) { return a_with_array((id)&NSArray_c, c, self); }
static id a_mcopy(id self, SEL c) { return a_with_array((id)&NSMutableArray_c, c, self); }
static void a_enum_block(id self, SEL c, void *blk) {
    (void)c;
    nsarr_t *r = (nsarr_t *)self;
    void (*inv)(void *, id, u64, BOOL *) = *(void **)((u8 *)blk + 16);
    BOOL stop = NO;
    for (u64 i = 0; i < r->n && !stop; i++) inv(blk, r->items[i], i, &stop);
}
static void a_add(id self, SEL c, id o) { (void)c; if (o) cx_arr_add(self, o); mutations++; }
static void a_add_all(id self, SEL c, id o) { nsarr_t *s = (nsarr_t *)o; for (u64 i = 0; o && i < s->n; i++) a_add(self, c, s->items[i]); }
static void a_insert(id self, SEL c, id o, u64 idx) {
    (void)c;
    nsarr_t *r = (nsarr_t *)self;
    cx_arr_add(self, o);
    if (idx < r->n - 1) { memmove(r->items + idx + 1, r->items + idx, (r->n - 1 - idx) * sizeof(id)); r->items[idx] = o; }
    mutations++;
}
static void a_remove_at(id self, SEL c, u64 idx) {
    (void)c;
    nsarr_t *r = (nsarr_t *)self;
    if (idx >= r->n) return;
    objc_release(r->items[idx]);
    memmove(r->items + idx, r->items + idx + 1, (r->n - idx - 1) * sizeof(id));
    r->n--;
    mutations++;
}
static void a_remove_last(id self, SEL c) { nsarr_t *r = (nsarr_t *)self; if (r->n) a_remove_at(self, c, r->n - 1); }
static void a_remove_all(id self, SEL c) { (void)c; nsarr_t *r = (nsarr_t *)self; for (u64 i = 0; i < r->n; i++) objc_release(r->items[i]); r->n = 0; mutations++; }
static void a_remove_obj(id self, SEL c, id o) { nsarr_t *r = (nsarr_t *)self; for (u64 i = r->n; i-- > 0;) if (obj_eq(r->items[i], o)) a_remove_at(self, c, i); }
static void a_replace(id self, SEL c, u64 i, id o) { (void)c; nsarr_t *r = (nsarr_t *)self; if (i < r->n) { objc_retain(o); objc_release(r->items[i]); r->items[i] = o; } else if (i == r->n) cx_arr_add(self, o); }
static void a_set_sub(id self, SEL c, id o, u64 i) { a_replace(self, c, i, o); }
static SEL sort_sel;
static int sort_cmp(const void *a, const void *b) { return (int)((i64 (*)(id, SEL, id))objc_msgSend)(*(id *)a, sort_sel, *(id *)b); }
static void a_sort_sel(id self, SEL c, SEL s) { (void)c; nsarr_t *r = (nsarr_t *)self; sort_sel = s; qsort(r->items, r->n, sizeof(id), sort_cmp); }
static id a_sorted_sel(id self, SEL c, SEL s) { id a = a_mcopy(self, c); a_sort_sel(a, c, s); return a; }
static void *sort_blk;
static int sort_cmp_blk(const void *a, const void *b) { i64 (*inv)(void *, id, id) = *(void **)((u8 *)sort_blk + 16); return (int)inv(sort_blk, *(id *)a, *(id *)b); }
static id a_sorted_cmp(id self, SEL c, void *blk) { id a = a_mcopy(self, c); nsarr_t *r = (nsarr_t *)a; sort_blk = blk; qsort(r->items, r->n, sizeof(id), sort_cmp_blk); return a; }

/* ================================================================ NSDictionary */
static void d_set(id self, id v, id k) {
    nsdict_t *d = (nsdict_t *)self;
    for (u64 i = 0; i < d->n; i++) if (obj_eq(d->keys[i], k)) { objc_retain(v); objc_release(d->vals[i]); d->vals[i] = v; return; }
    if (d->n == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 8;
        d->keys = realloc(d->keys, d->cap * sizeof(id));
        d->vals = realloc(d->vals, d->cap * sizeof(id));
    }
    d->keys[d->n] = cx_is_str(k) ? MSG0(id, k, "copy") : objc_retain(k);
    d->vals[d->n++] = objc_retain(v);
}
static id d_dict(id cls, SEL c) { (void)c; return cx_alloc_instance((cx_class_t *)cls); }
static id d_cap(id cls, SEL c, u64 n) { (void)n; return d_dict(cls, c); }
static id d_with_objs_keys(id cls, SEL c, const id *v, const id *k, u64 n) { id d = d_dict(cls, c); for (u64 i = 0; i < n; i++) d_set(d, v[i], k[i]); return d; }
static id d_init_objs_keys(id self, SEL c, const id *v, const id *k, u64 n) { (void)c; for (u64 i = 0; i < n; i++) d_set(self, v[i], k[i]); return self; }
static id d_with_obj_key(id cls, SEL c, id v, id k) { id d = d_dict(cls, c); d_set(d, v, k); return d; }
static id d_with_objs_and_keys(id cls, SEL c, id first, ...) {
    id d = d_dict(cls, c);
    va_list ap;
    va_start(ap, first);
    for (id v = first; v; v = va_arg(ap, id)) { id k = va_arg(ap, id); d_set(d, v, k); }
    va_end(ap);
    return cx_autorelease(d);
}
static id d_with_dict(id cls, SEL c, id o) { id d = d_dict(cls, c); nsdict_t *s = (nsdict_t *)o; for (u64 i = 0; o && i < s->n; i++) d_set(d, s->vals[i], s->keys[i]); return d; }
static id d_get(id self, SEL c, id k) { (void)c; nsdict_t *d = (nsdict_t *)self; for (u64 i = 0; i < d->n; i++) if (obj_eq(d->keys[i], k)) return d->vals[i]; return 0; }
static u64 d_count(id self, SEL c) { (void)c; return ((nsdict_t *)self)->n; }
static id d_keys(id self, SEL c) { nsdict_t *d = (nsdict_t *)self; return a_with_objs_count((id)&NSArray_c, c, d->keys, d->n); }
static id d_vals(id self, SEL c) { nsdict_t *d = (nsdict_t *)self; return a_with_objs_count((id)&NSArray_c, c, d->vals, d->n); }
static void d_setobj(id self, SEL c, id v, id k) { (void)c; d_set(self, v, k); mutations++; }
static void d_remove(id self, SEL c, id k) {
    (void)c;
    nsdict_t *d = (nsdict_t *)self;
    for (u64 i = 0; i < d->n; i++)
        if (obj_eq(d->keys[i], k)) { objc_release(d->keys[i]); objc_release(d->vals[i]); d->keys[i] = d->keys[d->n - 1]; d->vals[i] = d->vals[d->n - 1]; d->n--; mutations++; return; }
}
static void d_remove_all(id self, SEL c) { (void)c; nsdict_t *d = (nsdict_t *)self; for (u64 i = 0; i < d->n; i++) { objc_release(d->keys[i]); objc_release(d->vals[i]); } d->n = 0; }
static u64 d_fast(id self, SEL c, fastenum_t *st, id *buf, u64 len) {
    (void)c; (void)buf; (void)len;
    nsdict_t *d = (nsdict_t *)self;
    if (st->state) return 0;
    st->state = 1;
    st->items = d->keys;
    st->mut = &d->n;
    return d->n;
}
static void d_enum_block(id self, SEL c, void *blk) {
    (void)c;
    nsdict_t *d = (nsdict_t *)self;
    void (*inv)(void *, id, id, BOOL *) = *(void **)((u8 *)blk + 16);
    BOOL stop = NO;
    for (u64 i = 0; i < d->n && !stop; i++) inv(blk, d->keys[i], d->vals[i], &stop);
}
static id d_description(id self, SEL c) {
    (void)c;
    nsdict_t *d = (nsdict_t *)self;
    id out = cx_str("{\n");
    for (u64 i = 0; i < d->n; i++) {
        m_append(out, 0, cx_str("    "));
        m_append(out, 0, MSG0(id, d->keys[i], "description"));
        m_append(out, 0, cx_str(" = "));
        m_append(out, 0, MSG0(id, d->vals[i], "description"));
        m_append(out, 0, cx_str(";\n"));
    }
    m_append(out, 0, cx_str("}"));
    return out;
}
static id d_copy(id self, SEL c) { return d_with_dict((id)&NSDictionary_c, c, self); }
static id d_mcopy(id self, SEL c) { return d_with_dict((id)&NSMutableDictionary_c, c, self); }

/* ================================================================ NSLog e funções */
extern const char *__progname;
void NSLogv(id fmt, va_list ap) {
    id s = cx_str_vfmt(fmt, ap);
    long t = time(0);
    char ts[32];
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", gmtime(&t));
    cx_fdprintf(2, "%s.000 %s[%d:%d] %s\n", ts, __progname, getpid(), 1, S(s));
}
void NSLog(id fmt, ...) { va_list ap; va_start(ap, fmt); NSLogv(fmt, ap); va_end(ap); }
id NSStringFromClass(cx_class_t *c) { return c ? cx_str(cx_class_name(c)) : 0; }
cx_class_t *NSClassFromString(id s) { return s ? cx_class_named(cx_cstr(s)) : 0; }
id NSStringFromSelector(SEL s) { return s ? cx_str(s) : 0; }
SEL NSSelectorFromString(id s) { return s ? sel_registerName(cx_cstr(s)) : 0; }
id NSHomeDirectory(void) { return cx_str("/Users/Visitante"); }
id NSTemporaryDirectory(void) { return cx_str("/tmp/"); }
id NSUserName(void) { return cx_str("visitante"); }
id NSFullUserName(void) { return cx_str("Visitante"); }

/* ================================================================ NSDate / NSTimer / NSRunLoop */
typedef struct { cx_class_t *isa; double t; } nsdate_t;   /* segundos desde 1970 */
double cx_now(void) { mac_tv_t tv; gettimeofday(&tv, 0); return (double)tv.sec + tv.usec / 1e6; }
static id dt_date(id cls, SEL c) { (void)c; nsdate_t *d = (nsdate_t *)cx_alloc_instance((cx_class_t *)cls); d->t = cx_now(); return (id)d; }
static id dt_since_now(id cls, SEL c, double s) { nsdate_t *d = (nsdate_t *)dt_date(cls, c); d->t += s; return (id)d; }
static id dt_since_1970(id cls, SEL c, double s) { nsdate_t *d = (nsdate_t *)dt_date(cls, c); d->t = s; return (id)d; }
static double dt_ti1970(id self, SEL c) { (void)c; return ((nsdate_t *)self)->t; }
static double dt_tinow(id self, SEL c) { (void)c; return ((nsdate_t *)self)->t - cx_now(); }
static double dt_tiref(id self, SEL c) { (void)c; return ((nsdate_t *)self)->t - 978307200.0; }
static double dt_since(id self, SEL c, id o) { (void)c; return ((nsdate_t *)self)->t - ((nsdate_t *)o)->t; }
static id dt_desc(id self, SEL c) {
    (void)c;
    long t = (long)((nsdate_t *)self)->t;
    char b[48];
    strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S +0000", gmtime(&t));
    return cx_str(b);
}

cx_timer_t *cx_timers;
static id tm_make(id cls, double iv, id target, SEL sel, id info, BOOL rep, void *blk, BOOL sched) {
    cx_timer_t *t = (cx_timer_t *)cx_alloc_instance((cx_class_t *)cls);
    t->interval = iv < 0.001 ? 0.001 : iv;
    t->fire = cx_now() + t->interval;
    t->target = objc_retain(target);
    t->sel = sel;
    t->info = objc_retain(info);
    t->repeats = rep;
    t->block = blk ? _Block_copy(blk) : 0;
    t->valid = 1;
    if (sched) { t->next = cx_timers; cx_timers = t; }
    return (id)t;
}
static id tm_sched(id cls, SEL c, double iv, id target, SEL sel, id info, BOOL rep) { (void)c; return tm_make(cls, iv, target, sel, info, rep, 0, YES); }
static id tm_sched_blk(id cls, SEL c, double iv, BOOL rep, void *blk) { (void)c; return tm_make(cls, iv, 0, 0, 0, rep, blk, YES); }
static id tm_timer(id cls, SEL c, double iv, id target, SEL sel, id info, BOOL rep) { (void)c; return tm_make(cls, iv, target, sel, info, rep, 0, NO); }
static id tm_timer_blk(id cls, SEL c, double iv, BOOL rep, void *blk) { (void)c; return tm_make(cls, iv, 0, 0, 0, rep, blk, NO); }
static void tm_invalidate(id self, SEL c) { (void)c; ((cx_timer_t *)self)->valid = 0; }
static BOOL tm_valid(id self, SEL c) { (void)c; return ((cx_timer_t *)self)->valid; }
static id tm_info(id self, SEL c) { (void)c; return ((cx_timer_t *)self)->info; }
static double tm_interval(id self, SEL c) { (void)c; return ((cx_timer_t *)self)->interval; }
void cx_timer_fire(cx_timer_t *t) {
    if (t->block) { void (*inv)(void *, id) = *(void **)((u8 *)t->block + 16); inv(t->block, (id)t); }
    else if (t->target) ((void (*)(id, SEL, id))objc_msgSend)(t->target, t->sel, (id)t);
    if (!t->repeats) t->valid = 0;
}
static void tm_fire(id self, SEL c) { (void)c; cx_timer_fire((cx_timer_t *)self); }
/* dispara os timers vencidos; devolve o tempo até o próximo (segundos, -1 = nenhum) */
double cx_run_timers(void) {
    double now = cx_now(), next = -1;
    for (cx_timer_t **pp = &cx_timers; *pp;) {
        cx_timer_t *t = *pp;
        if (!t->valid) { *pp = t->next; continue; }
        if (now >= t->fire) {
            t->fire = now + t->interval;
            cx_timer_fire(t);
            if (!t->valid) { *pp = t->next; continue; }
        }
        double d = t->fire - now;
        if (next < 0 || d < next) next = d;
        pp = &t->next;
    }
    return next;
}
static id rl_current(id cls, SEL c) { (void)c; static id rl; if (!rl) rl = cx_alloc_instance((cx_class_t *)cls); return rl; }
static void rl_add_timer(id self, SEL c, id t, id mode) {
    (void)self; (void)c; (void)mode;
    cx_timer_t *tm = (cx_timer_t *)t;
    for (cx_timer_t *x = cx_timers; x; x = x->next) if (x == tm) return;
    tm->next = cx_timers;
    cx_timers = tm;
}
/* o AppKit troca este laço pelo dele (eventos de janela) */
void (*cx_runloop_hook)(double until);
static void rl_run_until(id self, SEL c, id date) {
    (void)self; (void)c;
    double until = date ? ((nsdate_t *)date)->t : 1e300;
    if (cx_runloop_hook) { cx_runloop_hook(until); return; }
    while (cx_now() < until) {
        double n = cx_run_timers();
        if (n < 0 && !date) break;
        usleep((unsigned)(n < 0 || n > 0.05 ? 50000 : n * 1e6 + 1000));
    }
}
static void rl_run(id self, SEL c) { rl_run_until(self, c, 0); }
static BOOL rl_run_mode(id self, SEL c, id mode, id date) { (void)mode; rl_run_until(self, c, date); return YES; }

/* ================================================================ diversos */
static id singleton(id cls) {
    static struct { cx_class_t *c; id o; } tab[16];
    for (int i = 0; i < 16; i++) {
        if (tab[i].c == (cx_class_t *)cls) return tab[i].o;
        if (!tab[i].c) { tab[i].c = (cx_class_t *)cls; tab[i].o = MSG0(id, MSG0(id, cls, "alloc"), "init"); return tab[i].o; }
    }
    return 0;
}
static id g_singleton(id cls, SEL c) { (void)c; return singleton(cls); }

/* NSProcessInfo */
extern char **NXArgv, **environ;
extern int NXArgc;
static id pi_name(id self, SEL c) { (void)self; (void)c; return cx_str(__progname); }
static id pi_args(id self, SEL c) { (void)self; id a = a_array((id)&NSMutableArray_c, c); for (int i = 0; i < NXArgc; i++) cx_arr_add(a, cx_str(NXArgv[i])); return a; }
static id pi_env(id self, SEL c) {
    (void)self;
    id d = d_dict((id)&NSMutableDictionary_c, c);
    for (char **e = environ; e && *e; e++) {
        char *eq = strchr(*e, '=');
        if (eq) d_set(d, cx_str(eq + 1), cx_str_n(*e, (u64)(eq - *e)));
    }
    return d;
}
static int pi_pid(id self, SEL c) { (void)self; (void)c; return getpid(); }
static id pi_osver(id self, SEL c) { (void)self; (void)c; return cx_str("mOS (compatível com macOS 15)"); }
static double pi_uptime(id self, SEL c) { (void)self; (void)c; return (double)clock() / 1e6; }
static u64 pi_cpus(id self, SEL c) { (void)self; (void)c; return 1; }
static u64 pi_mem(id self, SEL c) { (void)self; (void)c; return 1ull << 30; }

/* NSNotificationCenter: observadores em lista */
typedef struct obs { id target; SEL sel; id name; id object; void *block; struct obs *next; } obs_t;
static obs_t *observers;
typedef struct { cx_class_t *isa; id name, object, info; } nsnote_t;
id cx_note(id name, id object, id info) { nsnote_t *n = (nsnote_t *)cx_alloc_instance(&NSNotification_c); n->name = name; n->object = object; n->info = info; return (id)n; }
static id nn_name(id self, SEL c) { (void)c; return ((nsnote_t *)self)->name; }
static id nn_object(id self, SEL c) { (void)c; return ((nsnote_t *)self)->object; }
static id nn_info(id self, SEL c) { (void)c; return ((nsnote_t *)self)->info; }
static id nn_with(id cls, SEL c, id name, id obj, id info) { (void)cls; (void)c; return cx_note(name, obj, info); }
static void nc_add(id self, SEL c, id target, SEL sel, id name, id obj) {
    (void)self; (void)c;
    obs_t *o = calloc(1, sizeof *o);
    o->target = target; o->sel = sel; o->name = name; o->object = obj;
    o->next = observers;
    observers = o;
}
static id nc_add_block(id self, SEL c, id name, id obj, id queue, void *blk) {
    (void)self; (void)c; (void)queue;
    obs_t *o = calloc(1, sizeof *o);
    o->name = name; o->object = obj; o->block = _Block_copy(blk);
    o->next = observers;
    observers = o;
    return (id)o;
}
void cx_post(id note) {
    nsnote_t *n = (nsnote_t *)note;
    for (obs_t *o = observers; o; o = o->next) {
        if (o->name && !obj_eq(o->name, n->name)) continue;
        if (o->object && o->object != n->object) continue;
        if (o->block) { void (*inv)(void *, id) = *(void **)((u8 *)o->block + 16); inv(o->block, note); }
        else if (o->target) ((void (*)(id, SEL, id))objc_msgSend)(o->target, o->sel, note);
    }
}
static void nc_post(id self, SEL c, id note) { (void)self; (void)c; cx_post(note); }
static void nc_post_name(id self, SEL c, id name, id obj) { (void)self; (void)c; cx_post(cx_note(name, obj, 0)); }
static void nc_post_name_info(id self, SEL c, id name, id obj, id info) { (void)self; (void)c; cx_post(cx_note(name, obj, info)); }
static void nc_remove(id self, SEL c, id target) {
    (void)self; (void)c;
    for (obs_t **pp = &observers; *pp;) if ((*pp)->target == target || (id)*pp == target) *pp = (*pp)->next; else pp = &(*pp)->next;
}
static void nc_remove_name(id self, SEL c, id target, id name, id obj) { (void)name; (void)obj; nc_remove(self, c, target); }
static void nc_remove_all(id target) { for (obs_t **pp = &observers; *pp;) if ((*pp)->target == target) *pp = (*pp)->next; else pp = &(*pp)->next; }

/* NSThread: o processo tem uma thread */
static BOOL th_is_main(id self, SEL c) { (void)self; (void)c; return YES; }
static id th_current(id cls, SEL c) { return g_singleton(cls, c); }
static void th_sleep(id cls, SEL c, double s) { (void)cls; (void)c; usleep((unsigned)(s * 1e6)); }

/* NSUserDefaults: em memória (persistência fica para quando o mOS tiver um diretório de prefs) */
static id ud_dict;
static id ud_obj(id self, SEL c, id k) { (void)self; return ud_dict ? d_get(ud_dict, c, k) : 0; }
static void ud_set(id self, SEL c, id v, id k) { (void)self; if (!ud_dict) ud_dict = d_dict((id)&NSMutableDictionary_c, c); d_set(ud_dict, v, k); }
static BOOL ud_bool(id self, SEL c, id k) { id v = ud_obj(self, c, k); return v ? MSG0(BOOL, v, "boolValue") : NO; }
static i64 ud_int(id self, SEL c, id k) { id v = ud_obj(self, c, k); return v ? MSG0(i64, v, "integerValue") : 0; }
static double ud_double(id self, SEL c, id k) { id v = ud_obj(self, c, k); return v ? MSG0(double, v, "doubleValue") : 0; }
static void ud_set_bool(id self, SEL c, BOOL b, id k) { ud_set(self, c, cx_num_i(b), k); }
static void ud_set_int(id self, SEL c, i64 b, id k) { ud_set(self, c, cx_num_i(b), k); }
static void ud_set_double(id self, SEL c, double b, id k) { ud_set(self, c, cx_num_d(b), k); }
static void ud_register(id self, SEL c, id d) {
    nsdict_t *s = (nsdict_t *)d;
    for (u64 i = 0; d && i < s->n; i++) if (!ud_obj(self, c, s->keys[i])) ud_set(self, c, s->vals[i], s->keys[i]);
}
static BOOL yes_m(id self, SEL c) { (void)self; (void)c; return YES; }

/* NSBundle: o mainBundle (path NULL) vem do argv[0]; +bundleWithPath: cria instâncias para outros
 * bundles (.app ou .framework; num .framework o FAT do mOS não tem os links Resources/Current, então
 * também se procura em Versions/A) */
typedef struct { cx_class_t *isa; char *path; id info; id outer; } nsbundle_t;
#define BUNDLE(o) ((nsbundle_t *)(o))
static void b_root(id self, char *p, u64 cap) {
    if (self && BUNDLE(self)->path) { strlcpy(p, BUNDLE(self)->path, cap); return; }
    strlcpy(p, NXArgv && NXArgv[0] ? NXArgv[0] : "/", cap);
    char *m = strstr(p, ".app/");
    if (m) m[4] = 0;
    else { char *s = strrchr(p, '/'); if (s && s != p) *s = 0; }
}
static int b_ends(const char *p, const char *suf) { u64 l = strlen(p), s = strlen(suf); return l >= s && !strcmp(p + l - s, suf); }
/* pasta de recursos: X.app/Contents/Resources, X.framework/Resources (ou Versions/A/Resources) */
static void b_res_dir(id self, char *out, u64 cap) {
    char r[1024];
    b_root(self, r, sizeof r);
    if (b_ends(r, ".app")) snprintf(out, cap, "%s/Contents/Resources", r);
    else if (b_ends(r, ".framework")) {
        snprintf(out, cap, "%s/Resources", r);
        if (access(out, 0) != 0) snprintf(out, cap, "%s/Versions/A/Resources", r);
    } else strlcpy(out, r, cap);
}
static id b_path(id self, SEL c) { (void)c; char p[1024]; b_root(self, p, sizeof p); return cx_str(p); }
static id b_resources(id self, SEL c) { (void)c; char p[1100]; b_res_dir(self, p, sizeof p); return cx_str(p); }
static id b_with_path(id cls, SEL c, id path) {
    (void)c;
    if (!path || !N(path) || access(S(path), 0) != 0) return 0;
    nsbundle_t *b = (nsbundle_t *)cx_alloc_instance((cx_class_t *)cls);
    b->path = strdup(S(path));
    u64 l = strlen(b->path);
    while (l > 1 && b->path[l - 1] == '/') b->path[--l] = 0;
    return cx_autorelease((id)b);
}
static id b_init_path(id self, SEL c, id path) {
    (void)c;
    if (!path || !N(path) || access(S(path), 0) != 0) { objc_release(self); return 0; }
    BUNDLE(self)->path = strdup(S(path));
    return self;
}
static id b_main(id cls, SEL c) { return g_singleton(cls, c); }
static id b_for_class(id cls, SEL c, id k) { (void)k; return g_singleton(cls, c); }
static id b_resource(id self, SEL c, id name, id type) {
    id dir = b_resources(self, c);
    char f[1200];
    snprintf(f, sizeof f, "%s/%s%s%s", S(dir), cx_cstr(name), type && N(type) ? "." : "", type ? cx_cstr(type) : "");
    if (access(f, 0) == 0) return cx_str(f);
    /* recursos localizados: Base.lproj, pt-BR, pt, en */
    static const char *lp[] = {"Base", "pt-BR", "pt_BR", "pt", "en", "English"};
    for (u32 i = 0; i < sizeof lp / sizeof lp[0]; i++) {
        snprintf(f, sizeof f, "%s/%s.lproj/%s%s%s", S(dir), lp[i], cx_cstr(name), type && N(type) ? "." : "", type ? cx_cstr(type) : "");
        if (access(f, 0) == 0) return cx_str(f);
    }
    return 0;
}
static id b_resource_dir(id self, SEL c, id name, id type, id sub) {
    if (!sub || !N(sub)) return b_resource(self, c, name, type);
    char n[600];
    snprintf(n, sizeof n, "%s/%s", cx_cstr(sub), cx_cstr(name));
    return b_resource(self, c, cx_str(n), type);
}
extern id cx_plist_file(const char *path);
static id b_info_dict(id self, SEL c) {
    nsbundle_t *b = BUNDLE(self);
    if (!b->info) {                                  /* cache por instância (o mainBundle é um singleton) */
        char r[1024], f[1100];
        b_root(self, r, sizeof r);
        if (b_ends(r, ".app")) snprintf(f, sizeof f, "%s/Contents/Info.plist", r);
        else { b_res_dir(self, f, sizeof f - 12); strlcat(f, "/Info.plist", sizeof f); }
        b->info = cx_plist_file(f);
        if (!b->info) b->info = d_dict((id)&NSDictionary_c, c);
    }
    return b->info;
}
/* helper do Electron (Code Helper.app dentro de Contents/Frameworks) não traz ElectronAsarIntegrity.
 * O plist do .app de fora tem o dicionário aninhado Resources/app.asar → {algorithm, hash}. */
static id b_outer_info(id self) {
    nsbundle_t *b = BUNDLE(self);
    if (b->outer == (id)1) return 0;
    if (b->outer) return b->outer;
    char r[1024];
    b_root(self, r, sizeof r);
    for (;;) {
        char *s = strrchr(r, '/');
        if (!s || s == r) break;
        *s = 0;
        if (!b_ends(r, ".app")) continue;
        char f[1100];
        snprintf(f, sizeof f, "%s/Contents/Info.plist", r);
        id info = cx_plist_file(f);
        if (!info) continue;
        b->outer = info;
        return info;
    }
    b->outer = (id)1;
    return 0;
}
static id b_info(id self, SEL c, id key) {
    id v = d_get(b_info_dict(self, c), c, key);
    if (v) return v;
    id o = b_outer_info(self);
    return o ? d_get(o, c, key) : 0;
}
static id b_exec(id self, SEL c) {
    char r[1024], f[1300];
    b_root(self, r, sizeof r);
    id e = b_info(self, c, cx_str("CFBundleExecutable"));
    const char *base = strrchr(r, '/');
    base = base ? base + 1 : r;
    char name[256];
    if (e && N(e)) strlcpy(name, S(e), sizeof name);
    else { strlcpy(name, base, sizeof name); char *dot = strrchr(name, '.'); if (dot) *dot = 0; }
    if (b_ends(r, ".app")) snprintf(f, sizeof f, "%s/Contents/MacOS/%s", r, name);
    else if (b_ends(r, ".framework")) {
        snprintf(f, sizeof f, "%s/%s", r, name);
        if (access(f, 0) != 0) snprintf(f, sizeof f, "%s/Versions/A/%s", r, name);
    } else if (!BUNDLE(self)->path && NXArgv && NXArgv[0]) strlcpy(f, NXArgv[0], sizeof f);
    else return 0;
    return cx_str(f);
}
static id dict_with_file(id cls, SEL c, id path) { (void)cls; (void)c; id d = cx_plist_file(cx_cstr(path)); return d ? cx_autorelease(d) : 0; }
static id dict_init_file(id self, SEL c, id path) { (void)c; objc_release(self); return cx_plist_file(cx_cstr(path)); }
static id b_ident(id self, SEL c) {
    id i = b_info(self, c, cx_str("CFBundleIdentifier"));
    return i ? i : (BUNDLE(self)->path ? 0 : cx_str("com.mos.app"));
}

/* NSFileManager */
static BOOL fm_exists(id self, SEL c, id path) { (void)self; (void)c; return access(cx_cstr(path), 0) == 0; }
static BOOL fm_exists_dir(id self, SEL c, id path, BOOL *isdir) {
    (void)self; (void)c;
    struct { u8 b[160]; } st;
    if (stat(cx_cstr(path), &st) != 0) return NO;
    if (isdir) *isdir = ((*(u16 *)(st.b + 4)) & 0xF000) == 0x4000;
    return YES;
}
static id fm_contents(id self, SEL c, id path, id *err) {
    (void)self; (void)err;
    void *d = opendir(cx_cstr(path));
    if (!d) return 0;
    id a = a_array((id)&NSMutableArray_c, c);
    u8 *e;
    while ((e = readdir(d))) cx_arr_add(a, cx_str((const char *)e + 21));
    closedir(d);
    return a;
}
static BOOL fm_mkdir(id self, SEL c, id path, BOOL inter, id attrs, id *err) { (void)self; (void)c; (void)inter; (void)attrs; (void)err; return mkdir(cx_cstr(path), 0755) == 0; }
static BOOL fm_remove(id self, SEL c, id path, id *err) { (void)self; (void)c; (void)err; return unlink(cx_cstr(path)) == 0; }
static id fm_cwd(id self, SEL c) { (void)self; (void)c; char b[1024]; return cx_str(getcwd(b, sizeof b)); }

/* NSNull, NSCharacterSet, NSError, NSException (mínimos) */
static id null_desc(id self, SEL c) { (void)self; (void)c; return cx_str("<null>"); }
static id err_desc(id self, SEL c) { (void)self; (void)c; return cx_str("Erro"); }
static id exc_raise(id cls, SEL c, id name, id fmt, ...) {
    (void)cls; (void)c;
    va_list ap;
    va_start(ap, fmt);
    id m = cx_str_vfmt(fmt, ap);
    va_end(ap);
    cx_fdprintf(2, "*** Exceção '%s': %s\n", cx_cstr(name), cx_cstr(m));
    abort();
}

/* ================================================================ registro */
#define M(cls, s, f) cx_add_method(&cls##_c, s, (void *)(f))
#define CM(cls, s, f) cx_add_cmethod(&cls##_c, s, (void *)(f))

/* NSStringFromRect/Size/Point no formato da Apple: "{{x, y}, {w, h}}" */
typedef struct { double x, y; } cx_pt_t;
typedef struct { double w, h; } cx_sz_t;
typedef struct { cx_pt_t o; cx_sz_t s; } cx_rc_t;
id NSStringFromRect(cx_rc_t r) { char b[128]; snprintf(b, sizeof b, "{{%g, %g}, {%g, %g}}", r.o.x, r.o.y, r.s.w, r.s.h); return cx_autorelease(cx_str(b)); }
id NSStringFromSize(cx_sz_t s) { char b[64]; snprintf(b, sizeof b, "{%g, %g}", s.w, s.h); return cx_autorelease(cx_str(b)); }
id NSStringFromPoint(cx_pt_t p) { char b[64]; snprintf(b, sizeof b, "{%g, %g}", p.x, p.y); return cx_autorelease(cx_str(b)); }
id NSStringFromCGRect(cx_rc_t r) __attribute__((alias("NSStringFromRect")));
id NSStringFromCGSize(cx_sz_t s) __attribute__((alias("NSStringFromSize")));
id NSStringFromCGPoint(cx_pt_t p) __attribute__((alias("NSStringFromPoint")));

/* ---- construtores de conveniência: no Cocoa devolvem +0 (autorelease); as funções internas acima devolvem +1 */
#define AR0(f) static id f##_ar(id k, SEL c) { return cx_autorelease(f(k, c)); }
#define AR1(f, T) static id f##_ar(id k, SEL c, T a) { return cx_autorelease(f(k, c, a)); }
#define AR2(f, T, U) static id f##_ar(id k, SEL c, T a, U b) { return cx_autorelease(f(k, c, a, b)); }
#define AR3(f, T, U, V) static id f##_ar(id k, SEL c, T a, U b, V d) { return cx_autorelease(f(k, c, a, b, d)); }
AR0(s_string) AR1(s_with_utf8, const char *) AR1(s_with_string, id) AR3(s_with_file, id, u64, id *) AR0(s_mstring) AR1(s_mcap, u64)
AR1(n_int32, int) AR1(n_int, i64) AR1(n_uint, u64) AR1(n_double, double) AR1(n_float, float)
AR0(a_array) AR1(a_with_obj, id) AR2(a_with_objs_count, const id *, u64) AR1(a_with_array, id) AR1(a_cap, u64)
AR0(d_dict) AR1(d_cap, u64) AR3(d_with_objs_keys, const id *, const id *, u64) AR2(d_with_obj_key, id, id) AR1(d_with_dict, id)
AR0(dt_date) AR1(dt_since_now, double) AR1(dt_since_1970, double) AR3(nn_with, id, id, id)

void cx_foundation_init(void) {
    cx_define_class(&NSObject_c, &NSObject_m, "NSObject", 0, 0);
    CM(NSObject, "alloc", o_alloc); CM(NSObject, "allocWithZone:", o_alloc_zone); CM(NSObject, "new", o_new);
    CM(NSObject, "class", o_class_self); CM(NSObject, "self", o_class_self); CM(NSObject, "superclass", c_superclass);
    CM(NSObject, "description", c_description); CM(NSObject, "isSubclassOfClass:", c_is_subclass);
    CM(NSObject, "instancesRespondToSelector:", c_instances_respond); CM(NSObject, "initialize", o_void); CM(NSObject, "load", o_void);
    M(NSObject, "init", i_init); M(NSObject, "dealloc", o_dealloc); M(NSObject, "finalize", o_void);
    M(NSObject, "retain", o_retain); M(NSObject, "release", o_release); M(NSObject, "autorelease", o_autorelease); M(NSObject, "retainCount", o_retain_count);
    M(NSObject, "class", o_class); M(NSObject, "superclass", o_superclass); M(NSObject, "self", i_self); M(NSObject, "zone", o_zone);
    M(NSObject, "isKindOfClass:", o_is_kind); M(NSObject, "isMemberOfClass:", o_is_member); M(NSObject, "respondsToSelector:", o_responds);
    M(NSObject, "conformsToProtocol:", o_conforms); M(NSObject, "performSelector:", o_perform); M(NSObject, "performSelector:withObject:", o_perform1);
    M(NSObject, "performSelector:withObject:withObject:", o_perform2); M(NSObject, "performSelectorOnMainThread:withObject:waitUntilDone:", o_perform_main);
    M(NSObject, "description", o_description); M(NSObject, "debugDescription", o_description); M(NSObject, "hash", o_hash); M(NSObject, "isEqual:", o_is_equal);
    M(NSObject, "copy", o_copy); M(NSObject, "mutableCopy", o_mcopy); M(NSObject, "methodForSelector:", o_method_for);
    M(NSObject, "valueForKey:", o_value_for_key); M(NSObject, "setValue:forKey:", o_set_value_for_key); M(NSObject, "valueForKeyPath:", o_value_for_path); M(NSObject, "setValue:forKeyPath:", o_set_value_for_path);

    u32 str_extra = sizeof(nsstr_t) - 8;
    cx_define_class(&NSString_c, &NSString_m, "NSString", &NSObject_c, str_extra);
    cx_define_class(&NSMutableString_c, &NSMutableString_m, "NSMutableString", &NSString_c, 0);
    cx_define_class(&NSConstantString_c, &NSConstantString_m, "__NSCFConstantString", &NSString_c, 0);
    cx_define_class(&NSConstantStringObjC_c, &NSConstantStringObjC_m, "NSConstantString", &NSString_c, 0);
    CM(NSString, "alloc", s_alloc); CM(NSString, "string", s_string_ar); CM(NSString, "stringWithUTF8String:", s_with_utf8_ar);
    CM(NSString, "stringWithCString:encoding:", s_with_utf8_ar); CM(NSString, "stringWithString:", s_with_string_ar);
    CM(NSString, "stringWithFormat:", s_with_format); CM(NSString, "localizedStringWithFormat:", s_with_format);
    CM(NSString, "stringWithContentsOfFile:encoding:error:", s_with_file_ar); CM(NSString, "stringWithContentsOfFile:usedEncoding:error:", s_with_file_ar);
    M(NSString, "dealloc", str_dealloc); M(NSString, "initWithUTF8String:", i_init_utf8); M(NSString, "initWithString:", i_init_string); M(NSString, "initWithFormat:", i_init_format);
    M(NSString, "initWithFormat:arguments:", i_init_format_args); M(NSString, "initWithBytes:length:encoding:", i_init_bytes);
    M(NSString, "initWithCharacters:length:", i_init_chars); M(NSString, "initWithCharactersNoCopy:length:freeWhenDone:", i_init_chars_free);
    CM(NSString, "stringWithCharacters:length:", s_with_chars);
    M(NSString, "initWithCString:encoding:", i_init_cstr); M(NSString, "initWithContentsOfFile:encoding:error:", i_init_file);
    M(NSString, "UTF8String", i_utf8); M(NSString, "cStringUsingEncoding:", i_cstr_enc); M(NSString, "fileSystemRepresentation", i_utf8);
    M(NSString, "length", i_length); M(NSString, "lengthOfBytesUsingEncoding:", i_lenbytes); M(NSString, "characterAtIndex:", i_char_at);
    M(NSString, "isEqualToString:", i_eq_str); M(NSString, "isEqual:", i_eq_str); M(NSString, "hash", i_str_hash);
    M(NSString, "compare:", i_compare); M(NSString, "caseInsensitiveCompare:", i_casecmp); M(NSString, "localizedCompare:", i_compare);
    M(NSString, "hasPrefix:", i_has_prefix); M(NSString, "hasSuffix:", i_has_suffix); M(NSString, "containsString:", i_contains);
    M(NSString, "rangeOfString:", i_range_of); M(NSString, "stringByAppendingString:", i_append); M(NSString, "stringByAppendingFormat:", i_append_fmt);
    M(NSString, "substringFromIndex:", i_substr_from); M(NSString, "substringToIndex:", i_substr_to); M(NSString, "substringWithRange:", i_substr_range);
    M(NSString, "uppercaseString", i_upper); M(NSString, "lowercaseString", i_lower); M(NSString, "capitalizedString", i_capital);
    M(NSString, "stringByReplacingOccurrencesOfString:withString:", i_replace); M(NSString, "componentsSeparatedByString:", i_components);
    M(NSString, "stringByTrimmingCharactersInSet:", i_trim); M(NSString, "intValue", i_int); M(NSString, "integerValue", i_ll);
    M(NSString, "longLongValue", i_ll); M(NSString, "doubleValue", i_double); M(NSString, "floatValue", i_float); M(NSString, "boolValue", i_bool);
    M(NSString, "description", i_self); M(NSString, "copy", i_str_copy); M(NSString, "copyWithZone:", i_str_copy); M(NSString, "mutableCopy", i_str_mcopy);
    M(NSString, "lastPathComponent", i_last_path); M(NSString, "pathExtension", i_path_ext); M(NSString, "stringByDeletingLastPathComponent", i_del_last);
    M(NSString, "stringByAppendingPathComponent:", i_append_path); M(NSString, "writeToFile:atomically:encoding:error:", i_write_file);
    M(NSString, "stringByExpandingTildeInPath", i_self); M(NSString, "stringByStandardizingPath", i_self);
    CM(NSMutableString, "string", s_mstring_ar); CM(NSMutableString, "stringWithCapacity:", s_mcap_ar);
    M(NSMutableString, "appendString:", m_append); M(NSMutableString, "appendFormat:", m_append_fmt); M(NSMutableString, "setString:", m_set);
    M(NSMutableString, "insertString:atIndex:", m_insert); M(NSMutableString, "deleteCharactersInRange:", m_delete);
    M(NSMutableString, "copy", i_str_copy);

    cx_define_class(&NSNumber_c, &NSNumber_m, "NSNumber", &NSObject_c, sizeof(nsnum_t) - 8);
    CM(NSNumber, "numberWithInt:", n_int32_ar); CM(NSNumber, "numberWithInteger:", n_int_ar); CM(NSNumber, "numberWithLong:", n_int_ar);
    CM(NSNumber, "numberWithLongLong:", n_int_ar); CM(NSNumber, "numberWithUnsignedInt:", n_uint_ar); CM(NSNumber, "numberWithUnsignedInteger:", n_uint_ar);
    CM(NSNumber, "numberWithUnsignedLongLong:", n_uint_ar); CM(NSNumber, "numberWithShort:", n_int32_ar); CM(NSNumber, "numberWithChar:", n_int32_ar);
    CM(NSNumber, "numberWithDouble:", n_double_ar); CM(NSNumber, "numberWithFloat:", n_float_ar); CM(NSNumber, "numberWithBool:", n_bool);
    M(NSNumber, "initWithInt:", n_init_int); M(NSNumber, "initWithInteger:", n_init_int); M(NSNumber, "initWithDouble:", n_init_double);
    M(NSNumber, "intValue", n_ival32); M(NSNumber, "integerValue", n_ival); M(NSNumber, "longValue", n_ival); M(NSNumber, "longLongValue", n_ival);
    M(NSNumber, "unsignedIntValue", n_ival32); M(NSNumber, "unsignedIntegerValue", n_ival); M(NSNumber, "doubleValue", n_dval); M(NSNumber, "floatValue", n_fval);
    M(NSNumber, "boolValue", n_bval); M(NSNumber, "charValue", n_ival32); M(NSNumber, "stringValue", n_string); M(NSNumber, "description", n_string);
    M(NSNumber, "isEqual:", n_eq); M(NSNumber, "isEqualToNumber:", n_eq); M(NSNumber, "hash", n_hash); M(NSNumber, "compare:", n_cmp);
    M(NSNumber, "copy", i_self); M(NSNumber, "copyWithZone:", i_self);

    cx_define_class(&NSArray_c, &NSArray_m, "NSArray", &NSObject_c, sizeof(nsarr_t) - 8);
    cx_define_class(&NSMutableArray_c, &NSMutableArray_m, "NSMutableArray", &NSArray_c, 0);
    CM(NSArray, "array", a_array_ar); CM(NSArray, "arrayWithObject:", a_with_obj_ar); CM(NSArray, "arrayWithObjects:count:", a_with_objs_count_ar);
    CM(NSArray, "arrayWithObjects:", a_with_objs); CM(NSArray, "arrayWithArray:", a_with_array_ar); CM(NSArray, "arrayWithCapacity:", a_cap_ar);
    M(NSArray, "initWithObjects:count:", a_init_objs_count); M(NSArray, "initWithArray:", a_init_array); M(NSArray, "initWithCapacity:", i_init);
    M(NSArray, "dealloc", arr_dealloc); M(NSArray, "count", a_count); M(NSArray, "objectAtIndex:", a_at); M(NSArray, "objectAtIndexedSubscript:", a_at);
    M(NSArray, "firstObject", a_first); M(NSArray, "lastObject", a_last); M(NSArray, "containsObject:", a_contains); M(NSArray, "indexOfObject:", a_index_of);
    M(NSArray, "componentsJoinedByString:", a_join); M(NSArray, "description", a_description); M(NSArray, "countByEnumeratingWithState:objects:count:", a_fast);
    M(NSArray, "arrayByAddingObject:", a_adding); M(NSArray, "copy", a_copy); M(NSArray, "mutableCopy", a_mcopy);
    M(NSArray, "enumerateObjectsUsingBlock:", a_enum_block); M(NSArray, "sortedArrayUsingSelector:", a_sorted_sel); M(NSArray, "sortedArrayUsingComparator:", a_sorted_cmp);
    M(NSMutableArray, "addObject:", a_add); M(NSMutableArray, "addObjectsFromArray:", a_add_all); M(NSMutableArray, "insertObject:atIndex:", a_insert);
    M(NSMutableArray, "removeObjectAtIndex:", a_remove_at); M(NSMutableArray, "removeLastObject", a_remove_last); M(NSMutableArray, "removeAllObjects", a_remove_all);
    M(NSMutableArray, "removeObject:", a_remove_obj); M(NSMutableArray, "replaceObjectAtIndex:withObject:", a_replace);
    M(NSMutableArray, "setObject:atIndexedSubscript:", a_set_sub); M(NSMutableArray, "sortUsingSelector:", a_sort_sel);

    cx_define_class(&NSDictionary_c, &NSDictionary_m, "NSDictionary", &NSObject_c, sizeof(nsdict_t) - 8);
    cx_define_class(&NSMutableDictionary_c, &NSMutableDictionary_m, "NSMutableDictionary", &NSDictionary_c, 0);
    CM(NSDictionary, "dictionary", d_dict_ar); CM(NSDictionary, "dictionaryWithCapacity:", d_cap_ar); CM(NSDictionary, "dictionaryWithObjects:forKeys:count:", d_with_objs_keys_ar);
    CM(NSDictionary, "dictionaryWithObject:forKey:", d_with_obj_key_ar); CM(NSDictionary, "dictionaryWithObjectsAndKeys:", d_with_objs_and_keys);
    CM(NSDictionary, "dictionaryWithDictionary:", d_with_dict_ar); CM(NSDictionary, "dictionaryWithContentsOfFile:", dict_with_file); M(NSDictionary, "initWithContentsOfFile:", dict_init_file);
    CM(NSArray, "arrayWithContentsOfFile:", dict_with_file); M(NSArray, "initWithContentsOfFile:", dict_init_file);
    M(NSDictionary, "dealloc", dict_dealloc); M(NSDictionary, "initWithObjects:forKeys:count:", d_init_objs_keys); M(NSDictionary, "objectForKey:", d_get); M(NSDictionary, "objectForKeyedSubscript:", d_get);
    M(NSDictionary, "valueForKey:", d_get); M(NSDictionary, "count", d_count); M(NSDictionary, "allKeys", d_keys); M(NSDictionary, "allValues", d_vals);
    M(NSDictionary, "countByEnumeratingWithState:objects:count:", d_fast); M(NSDictionary, "enumerateKeysAndObjectsUsingBlock:", d_enum_block);
    M(NSDictionary, "description", d_description); M(NSDictionary, "copy", d_copy); M(NSDictionary, "mutableCopy", d_mcopy);
    M(NSMutableDictionary, "setObject:forKey:", d_setobj); M(NSMutableDictionary, "setObject:forKeyedSubscript:", d_setobj);
    M(NSMutableDictionary, "setValue:forKey:", d_setobj); M(NSMutableDictionary, "removeObjectForKey:", d_remove); M(NSMutableDictionary, "removeAllObjects", d_remove_all);

    cx_define_class(&NSAutoreleasePool_c, &NSAutoreleasePool_m, "NSAutoreleasePool", &NSObject_c, 0);
    M(NSAutoreleasePool, "drain", o_void);

    cx_define_class(&NSProcessInfo_c, &NSProcessInfo_m, "NSProcessInfo", &NSObject_c, 0);
    CM(NSProcessInfo, "processInfo", g_singleton);
    M(NSProcessInfo, "processName", pi_name); M(NSProcessInfo, "arguments", pi_args); M(NSProcessInfo, "environment", pi_env);
    M(NSProcessInfo, "processIdentifier", pi_pid); M(NSProcessInfo, "operatingSystemVersionString", pi_osver); M(NSProcessInfo, "systemUptime", pi_uptime);
    M(NSProcessInfo, "processorCount", pi_cpus); M(NSProcessInfo, "activeProcessorCount", pi_cpus); M(NSProcessInfo, "physicalMemory", pi_mem);

    cx_define_class(&NSDate_c, &NSDate_m, "NSDate", &NSObject_c, 8);
    CM(NSDate, "date", dt_date_ar); CM(NSDate, "now", dt_date_ar); CM(NSDate, "dateWithTimeIntervalSinceNow:", dt_since_now_ar);
    CM(NSDate, "dateWithTimeIntervalSince1970:", dt_since_1970_ar); CM(NSDate, "distantFuture", dt_date_ar);
    M(NSDate, "init", i_self); M(NSDate, "timeIntervalSince1970", dt_ti1970); M(NSDate, "timeIntervalSinceNow", dt_tinow);
    M(NSDate, "timeIntervalSinceReferenceDate", dt_tiref); M(NSDate, "timeIntervalSinceDate:", dt_since); M(NSDate, "description", dt_desc);

    cx_define_class(&NSTimer_c, &NSTimer_m, "NSTimer", &NSObject_c, sizeof(cx_timer_t) - 8);
    CM(NSTimer, "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", tm_sched);
    CM(NSTimer, "scheduledTimerWithTimeInterval:repeats:block:", tm_sched_blk);
    CM(NSTimer, "timerWithTimeInterval:target:selector:userInfo:repeats:", tm_timer);
    CM(NSTimer, "timerWithTimeInterval:repeats:block:", tm_timer_blk);
    M(NSTimer, "invalidate", tm_invalidate); M(NSTimer, "isValid", tm_valid); M(NSTimer, "userInfo", tm_info); M(NSTimer, "fire", tm_fire);
    M(NSTimer, "timeInterval", tm_interval);

    cx_define_class(&NSRunLoop_c, &NSRunLoop_m, "NSRunLoop", &NSObject_c, 0);
    CM(NSRunLoop, "currentRunLoop", rl_current); CM(NSRunLoop, "mainRunLoop", rl_current);
    M(NSRunLoop, "addTimer:forMode:", rl_add_timer); M(NSRunLoop, "run", rl_run); M(NSRunLoop, "runUntilDate:", rl_run_until);
    M(NSRunLoop, "runMode:beforeDate:", rl_run_mode);

    cx_define_class(&NSNotification_c, &NSNotification_m, "NSNotification", &NSObject_c, 24);
    CM(NSNotification, "notificationWithName:object:userInfo:", nn_with_ar);
    M(NSNotification, "name", nn_name); M(NSNotification, "object", nn_object); M(NSNotification, "userInfo", nn_info);
    cx_define_class(&NSNotificationCenter_c, &NSNotificationCenter_m, "NSNotificationCenter", &NSObject_c, 0);
    CM(NSNotificationCenter, "defaultCenter", g_singleton);
    M(NSNotificationCenter, "addObserver:selector:name:object:", nc_add); M(NSNotificationCenter, "addObserverForName:object:queue:usingBlock:", nc_add_block);
    M(NSNotificationCenter, "postNotification:", nc_post); M(NSNotificationCenter, "postNotificationName:object:", nc_post_name);
    M(NSNotificationCenter, "postNotificationName:object:userInfo:", nc_post_name_info); M(NSNotificationCenter, "removeObserver:", nc_remove);
    M(NSNotificationCenter, "removeObserver:name:object:", nc_remove_name);

    cx_define_class(&NSThread_c, &NSThread_m, "NSThread", &NSObject_c, 0);
    CM(NSThread, "isMainThread", th_is_main); CM(NSThread, "currentThread", th_current); CM(NSThread, "mainThread", th_current);
    CM(NSThread, "sleepForTimeInterval:", th_sleep); M(NSThread, "isMainThread", th_is_main);

    cx_define_class(&NSUserDefaults_c, &NSUserDefaults_m, "NSUserDefaults", &NSObject_c, 0);
    CM(NSUserDefaults, "standardUserDefaults", g_singleton);
    M(NSUserDefaults, "objectForKey:", ud_obj); M(NSUserDefaults, "stringForKey:", ud_obj); M(NSUserDefaults, "arrayForKey:", ud_obj);
    M(NSUserDefaults, "dictionaryForKey:", ud_obj); M(NSUserDefaults, "setObject:forKey:", ud_set); M(NSUserDefaults, "boolForKey:", ud_bool);
    M(NSUserDefaults, "integerForKey:", ud_int); M(NSUserDefaults, "doubleForKey:", ud_double); M(NSUserDefaults, "setBool:forKey:", ud_set_bool);
    M(NSUserDefaults, "setInteger:forKey:", ud_set_int); M(NSUserDefaults, "setDouble:forKey:", ud_set_double); M(NSUserDefaults, "registerDefaults:", ud_register);
    M(NSUserDefaults, "synchronize", yes_m); M(NSUserDefaults, "removeObjectForKey:", o_void);

    cx_define_class(&NSBundle_c, &NSBundle_m, "NSBundle", &NSObject_c, sizeof(nsbundle_t) - 8);
    CM(NSBundle, "mainBundle", b_main); CM(NSBundle, "bundleWithPath:", b_with_path); CM(NSBundle, "bundleForClass:", b_for_class);
    M(NSBundle, "initWithPath:", b_init_path); M(NSBundle, "executablePath", b_exec);
    M(NSBundle, "bundlePath", b_path); M(NSBundle, "resourcePath", b_resources); M(NSBundle, "pathForResource:ofType:", b_resource);
    M(NSBundle, "objectForInfoDictionaryKey:", b_info); M(NSBundle, "infoDictionary", b_info_dict); M(NSBundle, "pathForResource:ofType:inDirectory:", b_resource_dir); M(NSBundle, "bundleIdentifier", b_ident); M(NSBundle, "loadNibNamed:owner:topLevelObjects:", yes_m);

    cx_define_class(&NSFileManager_c, &NSFileManager_m, "NSFileManager", &NSObject_c, 0);
    CM(NSFileManager, "defaultManager", g_singleton);
    M(NSFileManager, "fileExistsAtPath:", fm_exists); M(NSFileManager, "fileExistsAtPath:isDirectory:", fm_exists_dir);
    M(NSFileManager, "contentsOfDirectoryAtPath:error:", fm_contents); M(NSFileManager, "createDirectoryAtPath:withIntermediateDirectories:attributes:error:", fm_mkdir);
    M(NSFileManager, "removeItemAtPath:error:", fm_remove); M(NSFileManager, "currentDirectoryPath", fm_cwd);

    cx_define_class(&NSData_c, &NSData_m, "NSData", &NSObject_c, 16);
    cx_define_class(&NSCharacterSet_c, &NSCharacterSet_m, "NSCharacterSet", &NSObject_c, 0);
    CM(NSCharacterSet, "whitespaceCharacterSet", g_singleton); CM(NSCharacterSet, "whitespaceAndNewlineCharacterSet", g_singleton);
    CM(NSCharacterSet, "newlineCharacterSet", g_singleton);
    cx_define_class(&NSValue_c, &NSValue_m, "NSValue", &NSObject_c, 32);
    cx_define_class(&NSError_c, &NSError_m, "NSError", &NSObject_c, 0);
    M(NSError, "localizedDescription", err_desc); M(NSError, "description", err_desc);
    cx_define_class(&NSSet_c, &NSSet_m, "NSSet", &NSArray_c, 0);
    cx_define_class(&NSMutableSet_c, &NSMutableSet_m, "NSMutableSet", &NSMutableArray_c, 0);
    CM(NSSet, "set", a_array_ar); CM(NSSet, "setWithArray:", a_with_array_ar); CM(NSSet, "setWithObjects:", a_with_objs);
    M(NSSet, "allObjects", a_copy); M(NSSet, "anyObject", a_first); M(NSSet, "member:", a_first);
    cx_define_class(&NSNull_c, &NSNull_m, "NSNull", &NSObject_c, 0);
    CM(NSNull, "null", g_singleton); M(NSNull, "description", null_desc);
    cx_define_class(&NSException_c, &NSException_m, "NSException", &NSObject_c, 0);
    CM(NSException, "raise:format:", exc_raise);
}
