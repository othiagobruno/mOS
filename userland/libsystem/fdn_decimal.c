/*
 * mOS libSystem — NSDecimal (as funções C), NSDecimalNumber exato sobre os 20 bytes do NSDecimal (antes guardava
 * double), NSDecimalNumberHandler com os comportamentos e as exceções, e -[NSNumber decimalValue].
 * A aritmética está em fdn_decimal_core.c.
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_decimal_core.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_exception_throw(id e) __attribute__((noreturn));
id class_getSuperclass(id k);
const char *sel_getName(SEL s);
IMP cx_lookup(cx_class_t *cls, SEL sel);
char *strncpy(char *d, const char *s, size_t n);

static id cls(const char *n) { return (id)cx_class_named(n); }
static const char *cs(id s) { return s ? cx_cstr(s) : ""; }

/* ---------------------------------------------------------------- funções C */
int NSDecimalAdd(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, unsigned long m) { return r && a && b ? fdn_dec_add(r, a, b, (int)m) : DEC_OVER; }
int NSDecimalSubtract(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, unsigned long m) { return r && a && b ? fdn_dec_sub(r, a, b, (int)m) : DEC_OVER; }
int NSDecimalMultiply(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, unsigned long m) { return r && a && b ? fdn_dec_mul(r, a, b, (int)m) : DEC_OVER; }
int NSDecimalDivide(nsdec_t *r, const nsdec_t *a, const nsdec_t *b, unsigned long m) { return r && a && b ? fdn_dec_div(r, a, b, (int)m) : DEC_OVER; }
int NSDecimalPower(nsdec_t *r, const nsdec_t *a, unsigned long p, unsigned long m) { return r && a ? fdn_dec_pow(r, a, p, (int)m) : DEC_OVER; }
int NSDecimalMultiplyByPowerOf10(nsdec_t *r, const nsdec_t *a, short p, unsigned long m) { return r && a ? fdn_dec_pow10(r, a, p, (int)m) : DEC_OVER; }
void NSDecimalRound(nsdec_t *r, const nsdec_t *a, long scale, unsigned long m) { if (r && a) fdn_dec_round(r, a, (int)scale, (int)m); }
int NSDecimalNormalize(nsdec_t *a, nsdec_t *b, unsigned long m) { return a && b ? fdn_dec_normalize(a, b, (int)m) : DEC_OVER; }
void NSDecimalCompact(nsdec_t *d) { if (d) fdn_dec_compact(d); }
long NSDecimalCompare(const nsdec_t *a, const nsdec_t *b) { return a && b ? fdn_dec_cmp(a, b) : 0; }
void NSDecimalCopy(nsdec_t *dst, const nsdec_t *src) { if (dst && src) *dst = *src; }
unsigned char NSDecimalIsNotANumber(const nsdec_t *d) { return d && fdn_dec_isnan(d); }

/* separador decimal de um NSLocale ou de um dicionário com NSDecimalSeparator (como a Foundation aceita os dois) */
static void sep_of(id locale, char *out, int cap) {
    out[0] = '.'; out[1] = 0;
    if (!locale) return;
    id v = 0;
    if (MSG(BOOL, locale, "respondsToSelector:", sel_registerName("objectForKey:"))) {
        v = MSG(id, locale, "objectForKey:", cx_str("decimal"));            /* NSLocaleDecimalSeparator */
        if (!v) v = MSG(id, locale, "objectForKey:", cx_str("NSDecimalSeparator"));
    }
    if (v && *cs(v)) { strncpy(out, cs(v), (size_t)cap - 1); out[cap - 1] = 0; }
}
id NSDecimalString(const nsdec_t *d, id locale) {
    char b[400], sep[16];
    if (!d) return objc_autorelease(cx_str(""));
    sep_of(locale, sep, sizeof sep);
    fdn_dec_to_str(d, b, sizeof b, sep);
    return objc_autorelease(cx_str(b));
}

/* ---------------------------------------------------------------- NSDecimalNumber */
/* a instância é a do NSNumber (double em ->d, para o que herdar) seguida dos 20 bytes do NSDecimal */
typedef struct { cx_class_t *isa; u32 kind; u32 pad; double d; i64 i; } nsnum_t;
static cx_class_t *dn_cls, *dh_cls;
static u32 dn_off;
static nsdec_t *DV(id o) { return (nsdec_t *)((char *)o + dn_off); }
static int is_dn(id o) {
    if (!o) return 0;
    for (cx_class_t *k = *(cx_class_t **)o; k; k = (cx_class_t *)class_getSuperclass((id)k)) if (k == dn_cls) return 1;
    return 0;
}
static void sync(id o) {
    nsnum_t *n = (nsnum_t *)o;
    n->kind = 1;
    n->d = fdn_dec_to_double(DV(o));
    n->i = fdn_dec_to_i64(DV(o));
}
static id dn_make(cx_class_t *k, const nsdec_t *d) {
    id o = cx_alloc_instance(k ? k : dn_cls);
    *DV(o) = *d;
    sync(o);
    return objc_autorelease(o);
}
static nsdec_t dec_of(id n) {
    nsdec_t d;
    if (!n) { fdn_dec_nan(&d); return d; }
    if (is_dn(n)) return *DV(n);
    const char *t = MSG(BOOL, n, "respondsToSelector:", sel_registerName("objCType")) ? MSG0(const char *, n, "objCType") : 0;
    if (!t && MSG(BOOL, n, "isKindOfClass:", cls("NSNumber"))) {
        nsnum_t *x = (nsnum_t *)n; /* NSNumber da casa: 0 inteiro, 1 double, 2 bool */
        if (x->kind == 1) fdn_dec_from_double(&d, x->d); else fdn_dec_from_i64(&d, x->i);
        return d;
    }
    if (t && (*t == 'd' || *t == 'f')) fdn_dec_from_double(&d, MSG0(double, n, "doubleValue"));
    else if (t && (*t == 'Q' || *t == 'L' || *t == 'I' || *t == 'S' || *t == 'C'))
        fdn_dec_from_u64(&d, MSG0(unsigned long long, n, "unsignedLongLongValue"), 0, 0);
    else if (t) fdn_dec_from_i64(&d, MSG0(long long, n, "longLongValue"));
    else fdn_dec_from_double(&d, MSG0(double, n, "doubleValue"));
    return d;
}

static id dn_alloc(id k, SEL c) { (void)c; return cx_alloc_instance((cx_class_t *)k); }
static id dn_init_mant(id s, SEL c, unsigned long long m, short e, BOOL neg) {
    (void)c;
    fdn_dec_from_u64(DV(s), m, e, neg);
    sync(s);
    return s;
}
static id dn_init_dec(id s, SEL c, nsdec_t d) { (void)c; *DV(s) = d; sync(s); return s; }
static id dn_init_str_loc(id s, SEL c, id str, id loc) {
    (void)c;
    char sep[16];
    sep_of(loc, sep, sizeof sep);
    fdn_dec_parse(DV(s), cs(str), sep);
    sync(s);
    return s;
}
static id dn_init_str(id s, SEL c, id str) { return dn_init_str_loc(s, c, str, 0); }
static id dn_init_d(id s, SEL c, double v) { (void)c; fdn_dec_from_double(DV(s), v); sync(s); return s; }
static id dn_init_f(id s, SEL c, float v) { (void)c; fdn_dec_from_double(DV(s), (double)v); sync(s); return s; }
static id dn_init_i(id s, SEL c, long long v) { (void)c; fdn_dec_from_i64(DV(s), v); sync(s); return s; }
static id dn_init_i32(id s, SEL c, int v) { return dn_init_i(s, c, v); }
static id dn_init_u(id s, SEL c, unsigned long long v) { (void)c; fdn_dec_from_u64(DV(s), v, 0, 0); sync(s); return s; }
static id dn_init_u32(id s, SEL c, unsigned v) { return dn_init_u(s, c, v); }
static id dn_init_b(id s, SEL c, BOOL v) { return dn_init_i(s, c, v ? 1 : 0); }
static id dn_init_plain(id s, SEL c) { (void)c; fdn_dec_from_i64(DV(s), 0); sync(s); return s; }

static id dn_c_mant(id k, SEL c, unsigned long long m, short e, BOOL neg) { return objc_autorelease(dn_init_mant(cx_alloc_instance((cx_class_t *)k), c, m, e, neg)); }
static id dn_c_dec(id k, SEL c, nsdec_t d) { (void)c; return dn_make((cx_class_t *)k, &d); }
static id dn_c_str(id k, SEL c, id s) { return objc_autorelease(dn_init_str(cx_alloc_instance((cx_class_t *)k), c, s)); }
static id dn_c_str_loc(id k, SEL c, id s, id l) { return objc_autorelease(dn_init_str_loc(cx_alloc_instance((cx_class_t *)k), c, s, l)); }
static id dn_c_d(id k, SEL c, double v) { return objc_autorelease(dn_init_d(cx_alloc_instance((cx_class_t *)k), c, v)); }
static id dn_c_f(id k, SEL c, float v) { return objc_autorelease(dn_init_f(cx_alloc_instance((cx_class_t *)k), c, v)); }
static id dn_c_i(id k, SEL c, long long v) { return objc_autorelease(dn_init_i(cx_alloc_instance((cx_class_t *)k), c, v)); }
static id dn_c_i32(id k, SEL c, int v) { return dn_c_i(k, c, v); }
static id dn_c_u(id k, SEL c, unsigned long long v) { return objc_autorelease(dn_init_u(cx_alloc_instance((cx_class_t *)k), c, v)); }
static id dn_c_u32(id k, SEL c, unsigned v) { return dn_c_u(k, c, v); }
static id dn_c_b(id k, SEL c, BOOL v) { return dn_c_i(k, c, v ? 1 : 0); }

static id cached(int which) {
    static id tab[5];
    if (!tab[which]) {
        nsdec_t d;
        switch (which) {
        case 0: fdn_dec_from_i64(&d, 0); break;
        case 1: fdn_dec_from_i64(&d, 1); break;
        case 2: fdn_dec_nan(&d); break;
        case 3: fdn_dec_from_u64(&d, 0, 0, 0); for (int i = 0; i < 8; i++) d.m[i] = 0xFFFF; d.length = 8; d.exp = 127; break;
        default: fdn_dec_from_u64(&d, 0, 0, 0); for (int i = 0; i < 8; i++) d.m[i] = 0xFFFF; d.length = 8; d.exp = 127; d.neg = 1; break;
        }
        tab[which] = objc_retain(dn_make(dn_cls, &d));
    }
    return tab[which];
}
static id dn_zero(id k, SEL c) { (void)k; (void)c; return cached(0); }
static id dn_one(id k, SEL c) { (void)k; (void)c; return cached(1); }
static id dn_nan(id k, SEL c) { (void)k; (void)c; return cached(2); }
static id dn_max(id k, SEL c) { (void)k; (void)c; return cached(3); }
static id dn_min(id k, SEL c) { (void)k; (void)c; return cached(4); }

/* comportamento padrão: o handler padrão, a menos que alguém troque com setDefaultBehavior: */
static id default_behavior;
static id dh_default(id k, SEL c);
static id dn_get_default(id k, SEL c) { (void)k; (void)c; return default_behavior ? default_behavior : dh_default((id)dh_cls, 0); }
static void dn_set_default(id k, SEL c, id b) {
    (void)k; (void)c;
    if (b) objc_retain(b);
    if (default_behavior) objc_release(default_behavior);
    default_behavior = b;
}

static nsdec_t dn_decimal(id s, SEL c) { (void)c; return *DV(s); }

enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW, OP_POW10, OP_ROUND };
static id run_op(id s, SEL sel, int op, id other, long arg, id beh) {
    if (!beh) beh = dn_get_default(0, 0);
    unsigned long mode = MSG0(unsigned long, beh, "roundingMode");
    short scale = MSG0(short, beh, "scale");
    nsdec_t a = *DV(s), b, r;
    int st;
    if (op <= OP_DIV) {
        if (!other) { /* a Foundation trata nil como NaN */
            fdn_dec_nan(&b);
        } else b = dec_of(other);
    }
    switch (op) {
    case OP_ADD: st = fdn_dec_add(&r, &a, &b, (int)mode); break;
    case OP_SUB: st = fdn_dec_sub(&r, &a, &b, (int)mode); break;
    case OP_MUL: st = fdn_dec_mul(&r, &a, &b, (int)mode); break;
    case OP_DIV: st = fdn_dec_div(&r, &a, &b, (int)mode); break;
    case OP_POW: st = fdn_dec_pow(&r, &a, (unsigned long)arg, (int)mode); break;
    case OP_POW10: st = fdn_dec_pow10(&r, &a, (int)arg, (int)mode); break;
    default: st = DEC_OK; r = a; break;
    }
    if (scale != DEC_NO_SCALE && !fdn_dec_isnan(&r)) {
        nsdec_t t;
        int st2 = fdn_dec_round(&t, &r, scale, (int)mode);
        if (fdn_dec_cmp(&t, &r) != 0 && st == DEC_OK && op != OP_ROUND) st = DEC_PREC;
        if (st2 > st) st = st2;
        r = t;
    }
    if (st != DEC_OK) {
        id rep = MSG(id, beh, "exceptionDuringOperation:error:leftOperand:rightOperand:", sel, (unsigned long)st, s,
                     op <= OP_DIV ? other : (id)0);
        if (rep) return rep;
        if (st == DEC_OVER || st == DEC_DIV0) return cached(2);
    }
    return dn_make(dn_cls, &r);
}
static id dn_add(id s, SEL c, id o) { return run_op(s, c, OP_ADD, o, 0, 0); }
static id dn_add_b(id s, SEL c, id o, id b) { return run_op(s, c, OP_ADD, o, 0, b); }
static id dn_sub(id s, SEL c, id o) { return run_op(s, c, OP_SUB, o, 0, 0); }
static id dn_sub_b(id s, SEL c, id o, id b) { return run_op(s, c, OP_SUB, o, 0, b); }
static id dn_mul(id s, SEL c, id o) { return run_op(s, c, OP_MUL, o, 0, 0); }
static id dn_mul_b(id s, SEL c, id o, id b) { return run_op(s, c, OP_MUL, o, 0, b); }
static id dn_div(id s, SEL c, id o) { return run_op(s, c, OP_DIV, o, 0, 0); }
static id dn_div_b(id s, SEL c, id o, id b) { return run_op(s, c, OP_DIV, o, 0, b); }
static id dn_pow(id s, SEL c, unsigned long p) { return run_op(s, c, OP_POW, 0, (long)p, 0); }
static id dn_pow_b(id s, SEL c, unsigned long p, id b) { return run_op(s, c, OP_POW, 0, (long)p, b); }
static id dn_pow10(id s, SEL c, short p) { return run_op(s, c, OP_POW10, 0, p, 0); }
static id dn_pow10_b(id s, SEL c, short p, id b) { return run_op(s, c, OP_POW10, 0, p, b); }
static id dn_round(id s, SEL c, id b) { return run_op(s, c, OP_ROUND, 0, 0, b); }

static long dn_compare(id s, SEL c, id o) {
    (void)c;
    nsdec_t b = dec_of(o);
    return fdn_dec_cmp(DV(s), &b);
}
static BOOL dn_eq(id s, SEL c, id o) {
    if (!o) return 0;
    if (o == s) return 1;
    if (!MSG(BOOL, o, "isKindOfClass:", cls("NSNumber"))) return 0;
    return dn_compare(s, c, o) == 0;
}
static u64 dn_hash(id s, SEL c) { (void)c; double d = ((nsnum_t *)s)->d; return (u64)(i64)d ^ (u64)(d * 1000003.0); }
static id dn_desc_loc(id s, SEL c, id loc) { (void)c; return NSDecimalString(DV(s), loc); }
static id dn_desc(id s, SEL c) { return dn_desc_loc(s, c, 0); }
static const char *dn_type(id s, SEL c) { (void)s; (void)c; return "d"; }
static double dn_dval(id s, SEL c) { (void)c; return fdn_dec_to_double(DV(s)); }
static float dn_fval(id s, SEL c) { (void)c; return (float)fdn_dec_to_double(DV(s)); }
static long long dn_ival(id s, SEL c) { (void)c; return fdn_dec_to_i64(DV(s)); }
static int dn_ival32(id s, SEL c) { (void)c; return (int)fdn_dec_to_i64(DV(s)); }
static unsigned long long dn_uval(id s, SEL c) { (void)c; return fdn_dec_to_u64(DV(s)); }
static unsigned dn_uval32(id s, SEL c) { (void)c; return (unsigned)fdn_dec_to_u64(DV(s)); }
static BOOL dn_bval(id s, SEL c) { (void)c; return DV(s)->length != 0 && !fdn_dec_isnan(DV(s)); }
static id dn_copy(id s, SEL c, void *z) { (void)c; (void)z; return objc_retain(s); }
static id dn_copy0(id s, SEL c) { (void)c; return objc_retain(s); }
static void dn_get_value(id s, SEL c, void *p) { (void)c; if (p) *(nsdec_t *)p = *DV(s); }

/* NSCoding com as chaves da Foundation (NS.exponent, NS.length, NS.negative, NS.compact, NS.mantissa) */
static void dn_encode(id s, SEL c, id coder) {
    (void)c;
    nsdec_t *d = DV(s);
    MSG(void, coder, "encodeInt:forKey:", (int)d->exp, cx_str("NS.exponent"));
    MSG(void, coder, "encodeInt:forKey:", (int)d->length, cx_str("NS.length"));
    MSG(void, coder, "encodeBool:forKey:", (BOOL)d->neg, cx_str("NS.negative"));
    MSG(void, coder, "encodeBool:forKey:", (BOOL)d->compact, cx_str("NS.compact"));
    MSG(void, coder, "encodeInt:forKey:", 1, cx_str("NS.mantissa.bo"));
    id data = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)d->m, (u64)sizeof d->m);
    MSG(void, coder, "encodeObject:forKey:", data, cx_str("NS.mantissa"));
}
static id dn_decode(id s, SEL c, id coder) {
    (void)c;
    nsdec_t d;
    memset(&d, 0, sizeof d);
    d.exp = MSG(int, coder, "decodeIntForKey:", cx_str("NS.exponent"));
    d.length = (unsigned)MSG(int, coder, "decodeIntForKey:", cx_str("NS.length")) & 15;
    d.neg = MSG(BOOL, coder, "decodeBoolForKey:", cx_str("NS.negative")) ? 1 : 0;
    d.compact = MSG(BOOL, coder, "decodeBoolForKey:", cx_str("NS.compact")) ? 1 : 0;
    id data = MSG(id, coder, "decodeObjectForKey:", cx_str("NS.mantissa"));
    if (data) {
        u64 n = MSG0(u64, data, "length");
        memcpy(d.m, MSG0(const void *, data, "bytes"), n < sizeof d.m ? n : sizeof d.m);
    }
    if (d.length > 8) d.length = 8;
    *DV(s) = d;
    sync(s);
    return s;
}
static BOOL yes(id s, SEL c) { (void)s; (void)c; return 1; }

/* ---------------------------------------------------------------- NSDecimalNumberHandler */
typedef struct { cx_class_t *isa; u64 mode; short scale; u8 r_exact, r_over, r_under, r_div0; } dh_t;
static id dh_init(id s, SEL c, unsigned long mode, short scale, BOOL ex, BOOL ov, BOOL un, BOOL dz) {
    (void)c;
    dh_t *h = (dh_t *)s;
    h->mode = mode; h->scale = scale;
    h->r_exact = ex; h->r_over = ov; h->r_under = un; h->r_div0 = dz;
    return s;
}
static id dh_plain_init(id s, SEL c) { return dh_init(s, c, DEC_PLAIN, DEC_NO_SCALE, 0, 1, 1, 1); }
static id dh_with(id k, SEL c, unsigned long mode, short scale, BOOL ex, BOOL ov, BOOL un, BOOL dz) {
    return objc_autorelease(dh_init(cx_alloc_instance((cx_class_t *)k), c, mode, scale, ex, ov, un, dz));
}
static id dh_default(id k, SEL c) {
    (void)k; (void)c;
    static id h;
    if (!h) h = objc_retain(dh_plain_init(cx_alloc_instance(dh_cls), 0));
    return h;
}
static unsigned long dh_mode(id s, SEL c) { (void)c; return ((dh_t *)s)->mode; }
static short dh_scale(id s, SEL c) { (void)c; return ((dh_t *)s)->scale; }
static id dh_exception(id s, SEL c, SEL op, unsigned long err, id l, id r) {
    (void)c; (void)l; (void)r;
    dh_t *h = (dh_t *)s;
    const char *name = 0, *why = 0;
    switch (err) {
    case DEC_PREC: if (h->r_exact) { name = "NSDecimalNumberExactnessException"; why = "Loss of precision"; } break;
    case DEC_UNDER: if (h->r_under) { name = "NSDecimalNumberUnderflowException"; why = "Underflow"; } break;
    case DEC_OVER: if (h->r_over) { name = "NSDecimalNumberOverflowException"; why = "Overflow"; } break;
    case DEC_DIV0: if (h->r_div0) { name = "NSDecimalNumberDivideByZeroException"; why = "Division by zero"; } break;
    }
    if (!name) return 0;
    char reason[160];
    snprintf(reason, sizeof reason, "NSDecimalNumber %s: %s", op ? sel_getName(op) : "", why);
    id e = MSG(id, cls("NSException"), "exceptionWithName:reason:userInfo:", cx_str(name), cx_str(reason), (id)0);
    objc_exception_throw(e);
}

/* ---------------------------------------------------------------- NSNumber decimalValue */
static nsdec_t num_decimal(id s, SEL c) { (void)c; return dec_of(s); }
static const char *num_type(id s, SEL c) { (void)c; u32 k = ((nsnum_t *)s)->kind; return k == 1 ? "d" : k == 2 ? "c" : "q"; }

static cx_class_t dn_c __asm__("OBJC_CLASS_$_NSDecimalNumber"), dn_m __asm__("OBJC_METACLASS_$_NSDecimalNumber");
static cx_class_t dh_c __asm__("OBJC_CLASS_$_NSDecimalNumberHandler"), dh_m __asm__("OBJC_METACLASS_$_NSDecimalNumberHandler");

void cx_fdn_decimal_init(void) {
    cx_class_t *num = cx_class_named("NSNumber"), *ob = cx_class_named("NSObject");
    if (!num || !ob || dn_cls) return;
    dn_off = cx_instance_size(num);
    dn_cls = cx_define_class(&dn_c, &dn_m, "NSDecimalNumber", num, (u32)sizeof(nsdec_t));
    dh_cls = cx_define_class(&dh_c, &dh_m, "NSDecimalNumberHandler", ob, (u32)(sizeof(dh_t) - 8));
#define M(s, f) cx_add_method(dn_cls, s, (void *)(f))
#define CM(s, f) cx_add_cmethod(dn_cls, s, (void *)(f))
    CM("alloc", dn_alloc); CM("allocWithZone:", dn_alloc);
    CM("decimalNumberWithMantissa:exponent:isNegative:", dn_c_mant); CM("decimalNumberWithDecimal:", dn_c_dec);
    CM("decimalNumberWithString:", dn_c_str); CM("decimalNumberWithString:locale:", dn_c_str_loc);
    CM("zero", dn_zero); CM("one", dn_one); CM("notANumber", dn_nan);
    CM("maximumDecimalNumber", dn_max); CM("minimumDecimalNumber", dn_min);
    CM("defaultBehavior", dn_get_default); CM("setDefaultBehavior:", dn_set_default);
    CM("numberWithDouble:", dn_c_d); CM("numberWithFloat:", dn_c_f); CM("numberWithBool:", dn_c_b);
    CM("numberWithInt:", dn_c_i32); CM("numberWithShort:", dn_c_i32); CM("numberWithChar:", dn_c_i32);
    CM("numberWithInteger:", dn_c_i); CM("numberWithLong:", dn_c_i); CM("numberWithLongLong:", dn_c_i);
    CM("numberWithUnsignedInt:", dn_c_u32); CM("numberWithUnsignedShort:", dn_c_u32); CM("numberWithUnsignedChar:", dn_c_u32);
    CM("numberWithUnsignedInteger:", dn_c_u); CM("numberWithUnsignedLong:", dn_c_u); CM("numberWithUnsignedLongLong:", dn_c_u);
    M("init", dn_init_plain);
    M("initWithMantissa:exponent:isNegative:", dn_init_mant); M("initWithDecimal:", dn_init_dec);
    M("initWithString:", dn_init_str); M("initWithString:locale:", dn_init_str_loc);
    M("initWithDouble:", dn_init_d); M("initWithFloat:", dn_init_f); M("initWithBool:", dn_init_b);
    M("initWithInt:", dn_init_i32); M("initWithShort:", dn_init_i32); M("initWithChar:", dn_init_i32);
    M("initWithInteger:", dn_init_i); M("initWithLong:", dn_init_i); M("initWithLongLong:", dn_init_i);
    M("initWithUnsignedInt:", dn_init_u32); M("initWithUnsignedShort:", dn_init_u32); M("initWithUnsignedChar:", dn_init_u32);
    M("initWithUnsignedInteger:", dn_init_u); M("initWithUnsignedLong:", dn_init_u); M("initWithUnsignedLongLong:", dn_init_u);
    M("decimalValue", dn_decimal); M("getValue:", dn_get_value);
    M("decimalNumberByAdding:", dn_add); M("decimalNumberByAdding:withBehavior:", dn_add_b);
    M("decimalNumberBySubtracting:", dn_sub); M("decimalNumberBySubtracting:withBehavior:", dn_sub_b);
    M("decimalNumberByMultiplyingBy:", dn_mul); M("decimalNumberByMultiplyingBy:withBehavior:", dn_mul_b);
    M("decimalNumberByDividingBy:", dn_div); M("decimalNumberByDividingBy:withBehavior:", dn_div_b);
    M("decimalNumberByRaisingToPower:", dn_pow); M("decimalNumberByRaisingToPower:withBehavior:", dn_pow_b);
    M("decimalNumberByMultiplyingByPowerOf10:", dn_pow10); M("decimalNumberByMultiplyingByPowerOf10:withBehavior:", dn_pow10_b);
    M("decimalNumberByRoundingAccordingToBehavior:", dn_round);
    M("compare:", dn_compare); M("isEqual:", dn_eq); M("isEqualToNumber:", dn_eq); M("hash", dn_hash);
    M("description", dn_desc); M("stringValue", dn_desc); M("descriptionWithLocale:", dn_desc_loc);
    M("objCType", dn_type); M("doubleValue", dn_dval); M("floatValue", dn_fval);
    M("longLongValue", dn_ival); M("longValue", dn_ival); M("integerValue", dn_ival);
    M("intValue", dn_ival32); M("shortValue", dn_ival32); M("charValue", dn_ival32);
    M("unsignedLongLongValue", dn_uval); M("unsignedLongValue", dn_uval); M("unsignedIntegerValue", dn_uval);
    M("unsignedIntValue", dn_uval32); M("unsignedShortValue", dn_uval32); M("unsignedCharValue", dn_uval32);
    M("boolValue", dn_bval); M("copy", dn_copy0); M("copyWithZone:", dn_copy);
    M("encodeWithCoder:", dn_encode); M("initWithCoder:", dn_decode);
    cx_add_cmethod(dn_cls, "supportsSecureCoding", (void *)yes);
#undef M
#undef CM
    cx_add_cmethod(dh_cls, "defaultDecimalNumberHandler", (void *)dh_default);
    cx_add_cmethod(dh_cls, "decimalNumberHandlerWithRoundingMode:scale:raiseOnExactness:raiseOnOverflow:raiseOnUnderflow:raiseOnDivideByZero:", (void *)dh_with);
    cx_add_method(dh_cls, "initWithRoundingMode:scale:raiseOnExactness:raiseOnOverflow:raiseOnUnderflow:raiseOnDivideByZero:", (void *)dh_init);
    cx_add_method(dh_cls, "init", (void *)dh_plain_init);
    cx_add_method(dh_cls, "roundingMode", (void *)dh_mode);
    cx_add_method(dh_cls, "scale", (void *)dh_scale);
    cx_add_method(dh_cls, "exceptionDuringOperation:error:leftOperand:rightOperand:", (void *)dh_exception);
    cx_add_method(num, "decimalValue", (void *)num_decimal);
    if (!cx_lookup(num, sel_registerName("objCType"))) cx_add_method(num, "objCType", (void *)num_type);
}
