/*
 * mOS libSystem — literais constantes do Objective-C (Xcode 15+): @{...}, @[...], @42 e @1.5 compilados
 * como objetos prontos nas seções __objc_dictobj/arrayobj/intobj/doubleobj/floatobj. O carregador
 * (kernel/compat/macho.c) passa essas seções como kinds 12..16 e o runtime chama cx_darwin_literals
 * antes de registrar as classes.
 *
 *   NSConstantDictionary {isa, options, count, keys*, objects*}  -> nsdict_t {isa, keys, vals, n, cap} (no lugar)
 *   NSConstantArray      {isa, count, objects*}                  -> nsarr_t  {isa, items, n} (imutável: sem cap)
 *   NSConstantIntegerNumber {isa, encoding*, value}, NSConstantDoubleNumber {isa, double},
 *   NSConstantFloatNumber {isa, float}: classes próprias, subclasses de NSNumber
 */
#include "foundation.h"

id objc_retain(id o);

typedef struct { cx_class_t *isa; const char *enc; i64 v; } cint_t;
typedef struct { cx_class_t *isa; double v; } cdbl_t;
typedef struct { cx_class_t *isa; float v; } cflt_t;

cx_class_t NSConstantIntegerNumber_c __asm__("OBJC_CLASS_$_NSConstantIntegerNumber");
cx_class_t NSConstantIntegerNumber_m __asm__("OBJC_METACLASS_$_NSConstantIntegerNumber");
cx_class_t NSConstantDoubleNumber_c __asm__("OBJC_CLASS_$_NSConstantDoubleNumber");
cx_class_t NSConstantDoubleNumber_m __asm__("OBJC_METACLASS_$_NSConstantDoubleNumber");
cx_class_t NSConstantFloatNumber_c __asm__("OBJC_CLASS_$_NSConstantFloatNumber");
cx_class_t NSConstantFloatNumber_m __asm__("OBJC_METACLASS_$_NSConstantFloatNumber");

/* valores: inteiros respeitam o sinal da codificação ('Q', 'L', 'I'... sem sinal) */
static int is_unsigned(const cint_t *n) { char e = n->enc ? n->enc[0] : 'q'; return e == 'Q' || e == 'L' || e == 'I' || e == 'S' || e == 'C'; }
static i64 ci(id self) { return ((cint_t *)self)->v; }
static double cd(id self) {
    cx_class_t *k = *(cx_class_t **)self;
    if (k == &NSConstantDoubleNumber_c) return ((cdbl_t *)self)->v;
    if (k == &NSConstantFloatNumber_c) return ((cflt_t *)self)->v;
    return is_unsigned((cint_t *)self) ? (double)(u64)ci(self) : (double)ci(self);
}
static int is_int(id self) { return *(cx_class_t **)self == &NSConstantIntegerNumber_c; }
static i64 iv(id self) { return is_int(self) ? ci(self) : (i64)cd(self); }

static int m_int(id self, SEL c) { (void)c; return (int)iv(self); }
static i64 m_long(id self, SEL c) { (void)c; return iv(self); }
static u64 m_ulong(id self, SEL c) { (void)c; return (u64)iv(self); }
static unsigned m_uint(id self, SEL c) { (void)c; return (unsigned)iv(self); }
static BOOL m_bool(id self, SEL c) { (void)c; return is_int(self) ? ci(self) != 0 : cd(self) != 0.0; }
static double m_double(id self, SEL c) { (void)c; return cd(self); }
static float m_float(id self, SEL c) { (void)c; return (float)cd(self); }
static char m_char(id self, SEL c) { (void)c; return (char)iv(self); }
static short m_short(id self, SEL c) { (void)c; return (short)iv(self); }
static const char *m_type(id self, SEL c) {
    (void)c;
    if (is_int(self)) return ((cint_t *)self)->enc ? ((cint_t *)self)->enc : "q";
    return *(cx_class_t **)self == &NSConstantFloatNumber_c ? "f" : "d";
}
static id m_desc(id self, SEL c) {
    (void)c;
    char b[48];
    if (is_int(self)) {
        if (is_unsigned((cint_t *)self)) snprintf(b, sizeof b, "%llu", (unsigned long long)ci(self));
        else snprintf(b, sizeof b, "%lld", (long long)ci(self));
    } else snprintf(b, sizeof b, "%.17g", cd(self));
    return cx_str(b);
}
static BOOL m_equal(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    if (!o || !MSG(BOOL, o, "isKindOfClass:", (id)cx_class_named("NSNumber"))) return NO;
    return MSG0(double, o, "doubleValue") == cd(self);
}
static u64 m_hash(id self, SEL c) { (void)c; return (u64)iv(self); }
static id m_copy(id self, SEL c) { (void)c; return objc_retain(self); }
static i64 m_compare(id self, SEL c, id o) {
    (void)c;
    double a = cd(self), b = o ? MSG0(double, o, "doubleValue") : 0;
    return a < b ? -1 : a > b ? 1 : 0;
}

static void number_class(cx_class_t *k, cx_class_t *m, const char *name) {
    cx_define_class(k, m, name, cx_class_named("NSNumber"), 16);
    static const struct { const char *s; void *f; } tab[] = {
        {"intValue", (void *)m_int}, {"integerValue", (void *)m_long}, {"longValue", (void *)m_long},
        {"longLongValue", (void *)m_long}, {"unsignedIntValue", (void *)m_uint}, {"unsignedIntegerValue", (void *)m_ulong},
        {"unsignedLongValue", (void *)m_ulong}, {"unsignedLongLongValue", (void *)m_ulong}, {"boolValue", (void *)m_bool},
        {"doubleValue", (void *)m_double}, {"floatValue", (void *)m_float}, {"charValue", (void *)m_char},
        {"shortValue", (void *)m_short}, {"objCType", (void *)m_type}, {"description", (void *)m_desc},
        {"stringValue", (void *)m_desc}, {"isEqual:", (void *)m_equal}, {"isEqualToNumber:", (void *)m_equal},
        {"hash", (void *)m_hash}, {"copy", (void *)m_copy}, {"copyWithZone:", (void *)m_copy}, {"compare:", (void *)m_compare},
    };
    for (u32 i = 0; i < sizeof tab / sizeof tab[0]; i++) cx_add_method(k, tab[i].s, tab[i].f);
}

/* @{} e @[] vazios (Xcode 15+): o compilador referencia direto os objetos ___NSDictionary0__ e
 * ___NSArray0__ do CoreFoundation. Mesmo layout de nsdict_t {isa, keys, vals, n, cap} e
 * nsarr_t {isa, items, n}, com contagem 0; o isa é preenchido na inicialização. */
u64 cx_empty_dict[5] __asm__("__NSDictionary0__");
u64 cx_empty_array[3] __asm__("__NSArray0__");
static void empty_literals(void) {
    if (!cx_empty_dict[0]) cx_empty_dict[0] = (u64)cx_class_named("NSDictionary");
    if (!cx_empty_array[0]) cx_empty_array[0] = (u64)cx_class_named("NSArray");
}

static int classes_ready;
static void literal_classes(void) {
    empty_literals();
    if (classes_ready) return;
    classes_ready = 1;
    number_class(&NSConstantIntegerNumber_c, &NSConstantIntegerNumber_m, "NSConstantIntegerNumber");
    number_class(&NSConstantDoubleNumber_c, &NSConstantDoubleNumber_m, "NSConstantDoubleNumber");
    number_class(&NSConstantFloatNumber_c, &NSConstantFloatNumber_m, "NSConstantFloatNumber");
}

void cx_literals_init(void) { empty_literals(); }   /* cx_darwin_init: vale também sem seções de literais */
void cx_darwin_literals(u8 *a, u64 size, u64 kind) {
    literal_classes();
    if (kind == 12) {                                  /* dicionários: 40 bytes cada */
        cx_class_t *k = cx_class_named("NSDictionary");
        for (u64 o = 0; o + 40 <= size; o += 40) {
            u64 *e = (u64 *)(a + o);
            u64 count = e[2], keys = e[3], objs = e[4];
            e[0] = (u64)k; e[1] = keys; e[2] = objs; e[3] = count; e[4] = count;
        }
    } else if (kind == 13) {                           /* arrays: 24 bytes cada */
        cx_class_t *k = cx_class_named("NSArray");
        for (u64 o = 0; o + 24 <= size; o += 24) {
            u64 *e = (u64 *)(a + o);
            u64 count = e[1], objs = e[2];
            e[0] = (u64)k; e[1] = objs; e[2] = count;
        }
    } else if (kind == 14) {
        for (u64 o = 0; o + 24 <= size; o += 24) *(cx_class_t **)(a + o) = &NSConstantIntegerNumber_c;
    } else if (kind == 15) {
        for (u64 o = 0; o + 16 <= size; o += 16) *(cx_class_t **)(a + o) = &NSConstantDoubleNumber_c;
    } else if (kind == 16) {
        for (u64 o = 0; o + 16 <= size; o += 16) *(cx_class_t **)(a + o) = &NSConstantFloatNumber_c;
    }
}
