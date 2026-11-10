/*
 * mOS libSystem — coleções do CoreFoundation em ponte com o Foundation do mOS (porte do VS Code).
 * CFDictionaryRef = NSDictionary*, CFArrayRef = NSArray*, CFNumberRef/CFBooleanRef = NSNumber*.
 * O Chromium monta assim os dicionários de propriedades do IOSurface (CFDictionaryCreateMutable +
 * CFNumberCreate) e lê as respostas. Os callbacks de chave/valor são aceitos e ignorados: tudo é
 * tratado como objeto CF (retain/release), que é o caso de kCFType*CallBacks.
 * As leituras simples (CFDictionaryGetValue, CFArrayGetCount...) ficam em darwin.c.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);

/* ---- callbacks: só existem para o app poder passar &kCFType...CallBacks */
typedef struct { long version; void *retain, *release, *desc, *equal, *hash; } cf_cb_t;
const cf_cb_t kCFTypeDictionaryKeyCallBacks = {0}, kCFTypeDictionaryValueCallBacks = {0};
const cf_cb_t kCFCopyStringDictionaryKeyCallBacks = {0};
const cf_cb_t kCFTypeArrayCallBacks = {0}, kCFTypeSetCallBacks = {0};
const cf_cb_t kCFCopyStringSetCallBacks = {0};
const cf_cb_t kCFTypeBagCallBacks = {0};

/* ---- identificadores de tipo (os mesmos números do CF da Apple) */
enum { TID_STRING = 7, TID_NULL = 16, TID_DICT = 18, TID_ARRAY = 19, TID_DATA = 20, TID_BOOL = 21, TID_NUMBER = 22,
       TID_URL = 29, TID_ERROR = 30, TID_UUID = 34, TID_DATE = 42 };
u64 CFNullGetTypeID(void) { return TID_NULL; }
u64 CFURLGetTypeID(void) { return TID_URL; }
u64 CFUUIDGetTypeID(void) { return TID_UUID; }
id CFCopyTypeIDDescription(u64 t) {
    const char *n = t == TID_STRING ? "CFString" : t == TID_NULL ? "CFNull" : t == TID_DICT ? "CFDictionary" : t == TID_ARRAY ? "CFArray"
                  : t == TID_DATA ? "CFData" : t == TID_BOOL ? "CFBoolean" : t == TID_NUMBER ? "CFNumber" : t == TID_URL ? "CFURL"
                  : t == TID_ERROR ? "CFError" : t == TID_UUID ? "CFUUID" : t == TID_DATE ? "CFDate" : "CFType";
    return objc_retain(cx_str(n));
}
u64 CFStringGetTypeID(void) { return TID_STRING; }
u64 CFDictionaryGetTypeID(void) { return TID_DICT; }
u64 CFArrayGetTypeID(void) { return TID_ARRAY; }
u64 CFDataGetTypeID(void) { return TID_DATA; }
u64 CFBooleanGetTypeID(void) { return TID_BOOL; }
u64 CFNumberGetTypeID(void) { return TID_NUMBER; }
u64 CFDateGetTypeID(void) { return TID_DATE; }
u64 CFErrorGetTypeID(void) { return TID_ERROR; }

typedef struct { cx_class_t *isa; u32 kind; u32 pad; double d; i64 i; } cfnum_t;   /* = nsnum_t de foundation.c */

static BOOL kind_of(id o, const char *cls) {
    cx_class_t *k = cx_class_named(cls);
    return k && cx_is_subclass(object_getClass(o), k);
}
/* Tipos de outros arquivos (IOSurface, CVPixelBuffer, CGColor...) respondem a -_cfTypeID */
u64 CFGetTypeID(id o) {
    if (!o) return 0;
    if (cx_is_str(o)) return TID_STRING;
    if (kind_of(o, "NSNumber")) return ((cfnum_t *)o)->kind == 2 ? TID_BOOL : TID_NUMBER;
    if (kind_of(o, "NSDictionary")) return TID_DICT;
    if (kind_of(o, "NSSet") || kind_of(o, "NSMutableSet")) return 17;
    if (kind_of(o, "NSAttributedString") || kind_of(o, "NSMutableAttributedString")) return 31;
    if (kind_of(o, "__mOSCFLocale")) return 25;
    if (kind_of(o, "__mOSCFCharacterSet")) return 45;
    if (kind_of(o, "NSArray")) return TID_ARRAY;
    if (kind_of(o, "NSData")) return TID_DATA;
    if (kind_of(o, "NSDate")) return TID_DATE;
    if (kind_of(o, "NSError")) return TID_ERROR;
    if (kind_of(o, "NSURL")) return TID_URL;
    if (kind_of(o, "NSNull")) return TID_NULL;
    if (kind_of(o, "NSUUID")) return TID_UUID;
    if (kind_of(o, "NSColor")) return 0x2A2;           /* CGColor = NSColor do mOS (coregraphics.c) */
    if (cx_responds(object_getClass(o), sel_registerName("_cfTypeID"))) return MSG0(u64, o, "_cfTypeID");
    return 1;
}

/* ---- CFBoolean / CFNumber */
u8 CFBooleanGetValue(id b) { return b && MSG0(BOOL, b, "boolValue"); }

enum { kSInt8 = 1, kSInt16, kSInt32, kSInt64, kFloat32, kFloat64, kChar, kShort, kInt, kLong, kLongLong,
       kFloat, kDouble, kCFIndex, kNSInteger, kCGFloat };
static BOOL is_float_type(long t) { return t == kFloat32 || t == kFloat64 || t == kFloat || t == kDouble || t == kCGFloat; }
id CFNumberCreate(void *alloc, long type, const void *p) {
    (void)alloc;
    if (!p) return 0;
    switch (type) {
    case kSInt8: case kChar: return cx_num_i(*(const i8 *)p);
    case kSInt16: case kShort: return cx_num_i(*(const i16 *)p);
    case kSInt32: case kInt: return cx_num_i(*(const i32 *)p);
    case kFloat32: case kFloat: return cx_num_d(*(const float *)p);
    case kFloat64: case kDouble: case kCGFloat: return cx_num_d(*(const double *)p);
    default: return cx_num_i(*(const i64 *)p);
    }
}
u8 CFNumberIsFloatType(id n) { return n && kind_of(n, "NSNumber") && ((cfnum_t *)n)->kind == 1; }
long CFNumberGetType(id n) { return CFNumberIsFloatType(n) ? kFloat64 : kSInt64; }
u8 CFNumberGetValue(id n, long type, void *out) {
    if (!n || !out) return 0;
    i64 i = MSG0(i64, n, "longLongValue");
    double d = MSG0(double, n, "doubleValue");
    switch (type) {
    case kSInt8: case kChar: *(i8 *)out = (i8)i; break;
    case kSInt16: case kShort: *(i16 *)out = (i16)i; break;
    case kSInt32: case kInt: *(i32 *)out = (i32)i; break;
    case kFloat32: case kFloat: *(float *)out = (float)d; break;
    case kFloat64: case kDouble: case kCGFloat: *(double *)out = d; break;
    default: *(i64 *)out = i; break;
    }
    /* true = sem perda (inteiro pedido de número fracionário perde) */
    return is_float_type(type) || !CFNumberIsFloatType(n) || (double)i == d;
}
long CFNumberCompare(id a, id b, void *ctx) { (void)ctx; return a && b ? MSG(long, a, "compare:", b) : 0; }

/* ---- CFDictionary (regra Create/Copy: +1) */
static id new_mdict(void) { return objc_retain(MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary")); }
id CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb) { (void)alloc; (void)cap; (void)kcb; (void)vcb; return new_mdict(); }
id CFDictionaryCreate(void *alloc, const id *keys, const id *vals, long n, const void *kcb, const void *vcb) {
    (void)alloc; (void)kcb; (void)vcb;
    id d = new_mdict();
    for (long i = 0; i < n; i++) if (keys[i] && vals[i]) MSG(void, d, "setObject:forKey:", vals[i], keys[i]);
    return d;
}
id CFDictionaryCreateMutableCopy(void *alloc, long cap, id src) {
    (void)alloc; (void)cap;
    id d = new_mdict();
    if (src) {
        nsdict_t *s = (nsdict_t *)src;
        for (u64 i = 0; i < s->n; i++) MSG(void, d, "setObject:forKey:", s->vals[i], s->keys[i]);
    }
    return d;
}
id CFDictionaryCreateCopy(void *alloc, id src) { return CFDictionaryCreateMutableCopy(alloc, 0, src); }
void CFDictionarySetValue(id d, id key, id val) {
    if (!d || !key) return;
    if (val) MSG(void, d, "setObject:forKey:", val, key);
    else MSG(void, d, "removeObjectForKey:", key);
}
void CFDictionaryAddValue(id d, id key, id val) {
    if (d && key && val && !MSG(id, d, "objectForKey:", key)) MSG(void, d, "setObject:forKey:", val, key);
}
void CFDictionaryReplaceValue(id d, id key, id val) {
    if (d && key && val && MSG(id, d, "objectForKey:", key)) MSG(void, d, "setObject:forKey:", val, key);
}
void CFDictionaryRemoveValue(id d, id key) { if (d && key) MSG(void, d, "removeObjectForKey:", key); }
void CFDictionaryRemoveAllValues(id d) { if (d) MSG0(void, d, "removeAllObjects"); }
void CFDictionaryGetKeysAndValues(id d, id *keys, id *vals) {
    if (!d) return;
    nsdict_t *s = (nsdict_t *)d;
    for (u64 i = 0; i < s->n; i++) { if (keys) keys[i] = s->keys[i]; if (vals) vals[i] = s->vals[i]; }
}
void CFDictionaryApplyFunction(id d, void (*fn)(id, id, void *), void *ctx) {
    if (!d || !fn) return;
    nsdict_t *s = (nsdict_t *)d;
    u64 n = s->n;
    id *k = malloc((n ? n : 1) * sizeof(id) * 2);   /* cópia: o callback pode mexer no dicionário */
    if (!k) return;
    for (u64 i = 0; i < n; i++) { k[i] = s->keys[i]; k[n + i] = s->vals[i]; }
    for (u64 i = 0; i < n; i++) fn(k[i], k[n + i], ctx);
    free(k);
}

/* ---- CFArray */
static id new_marr(void) { return objc_retain(MSG0(id, (id)cx_class_named("NSMutableArray"), "array")); }
id CFArrayCreateMutable(void *alloc, long cap, const void *cb) { (void)alloc; (void)cap; (void)cb; return new_marr(); }
id CFArrayCreate(void *alloc, const id *vals, long n, const void *cb) {
    (void)alloc; (void)cb;
    id a = new_marr();
    for (long i = 0; i < n; i++) if (vals[i]) MSG(void, a, "addObject:", vals[i]);
    return a;
}
id CFArrayCreateMutableCopy(void *alloc, long cap, id src) {
    (void)alloc; (void)cap;
    id a = new_marr();
    if (src) MSG(void, a, "addObjectsFromArray:", src);
    return a;
}
id CFArrayCreateCopy(void *alloc, id src) { return CFArrayCreateMutableCopy(alloc, 0, src); }
void CFArrayAppendValue(id a, id v) { if (a && v) MSG(void, a, "addObject:", v); }
void CFArrayAppendArray(id a, id src, long loc, long len) {
    if (!a || !src) return;
    for (long i = 0; i < len; i++) { id o = MSG(id, src, "objectAtIndex:", (u64)(loc + i)); MSG(void, a, "addObject:", o); }
}
void CFArrayInsertValueAtIndex(id a, long i, id v) { if (a && v) MSG(void, a, "insertObject:atIndex:", v, (u64)i); }
void CFArraySetValueAtIndex(id a, long i, id v) {
    if (!a || !v) return;
    if ((u64)i == MSG0(u64, a, "count")) MSG(void, a, "addObject:", v);
    else MSG(void, a, "replaceObjectAtIndex:withObject:", (u64)i, v);
}
void CFArrayRemoveValueAtIndex(id a, long i) { if (a) MSG(void, a, "removeObjectAtIndex:", (u64)i); }
void CFArrayRemoveAllValues(id a) { if (a) MSG0(void, a, "removeAllObjects"); }
void CFArrayApplyFunction(id a, long loc, long len, void (*fn)(id, void *), void *ctx) {
    if (!a || !fn) return;
    for (long i = 0; i < len; i++) fn(MSG(id, a, "objectAtIndex:", (u64)(loc + i)), ctx);
}
u8 CFArrayContainsValue(id a, long loc, long len, id v) {
    if (!a) return 0;
    for (long i = 0; i < len; i++) { id o = MSG(id, a, "objectAtIndex:", (u64)(loc + i)); if (MSG(BOOL, o, "isEqual:", v)) return 1; }
    return 0;
}
