/*
 * mOS — CoreFoundation dos apps em Swift completo (OpenSwiftUI e dependências), em libMOSCFRuntime.a,
 * ligada estaticamente no app (dentro do app estas definições valem no lugar das do libSystem).
 *
 * Regra: todo objeto CF que o Swift enxerga é um OBJETO SWIFT NATIVO. Sem interoperabilidade ObjC o Swift
 * retém tipos CF com swift_retain/swift_release (como no Linux); os objetos CF do libSystem do mOS são
 * objetos ObjC com o contador fora do objeto e seriam corrompidos. Então:
 *  - _CFRuntimeRegisterClass/_CFRuntimeCreateInstance (tipos do OpenAttributeGraph/Compute: Graph,
 *    Subgraph...) e os tipos básicos daqui (CFString, CFNumber, CFBoolean, CFArray, CFDictionary, CFData)
 *    criam instâncias da classe Swift SwiftFoundation._MOSCFObject (swift_allocObject); o deinit dela chama
 *    o finalize da CFRuntimeClass (mos_cf_finalize). Por serem instâncias de classe de verdade, os objetos CF
 *    funcionam em Any/AnyObject, type(of:), Mirror. O CFRuntimeBase do swift-corelibs (DEPLOYMENT_RUNTIME_SWIFT)
 *    tem 24 bytes: isa, contador do Swift (o mesmo do HeapObject) e _cfinfoa, onde fica o índice do tipo CF.
 *  - CFString guarda UTF-8. As constantes CFSTR("...") que o clang gera têm outro layout
 *    ({isa = __CFConstantStringClassReference, flags, ptr, len}) e também são aceitas (ConstString).
 *  - CFRetain/CFRelease/CFGetRetainCount no C/C++ das deps passam por mos_oag_CF* (o prelude
 *    mos_openswiftui_prelude.h troca os nomes): objetos daqui vão para swift_retain/release; o resto
 *    (CGPath, CGColorSpace... do libSystem) segue para o CFRetain do libSystem.
 *  - vm_remap: o mOS não remapeia páginas entre endereços; copiamos o conteúdo antigo para a região nova
 *    (os motores endereçam por deslocamento a partir da base nova; a região antiga continua mapeada).
 *  - CFAutorelease: objetos Swift nativos não têm pool (sem ObjC) e ficam com a referência recebida.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>

/* aqui chamamos as funções reais do libSystem */
#undef CFRetain
#undef CFRelease
#undef CFGetRetainCount

extern "C" {
typedef struct objc_class *Class;
typedef struct objc_object *id;
id objc_autorelease(id o);
extern char mos_cf_const_isa[] __asm__("___CFConstantStringClassReference");      /* CFSTR estilo Apple (libSystem) */
/* classes Swift dos objetos CF (userland/sdk/openswiftui/swift/SwiftFoundation, sem library evolution: metadata
 * estática). O isa das CFSTR do swift-corelibs (DEPLOYMENT_RUNTIME_SWIFT + TARGET_OS_MAC) é exatamente
 * SwiftFoundation._NSCFConstantString; todo objeto CF criado aqui é um SwiftFoundation._MOSCFObject. */
extern char mos_swift_cfstr_isa[] __asm__("_$s15SwiftFoundation19_NSCFConstantStringCN");
extern char mos_cfobject_class[] __asm__("_$s15SwiftFoundation12_MOSCFObjectCN");

/* runtime Swift completo (build\swift\full\runtime, ligado no app) */
void *swift_allocObject(const void *metadata, size_t size, size_t alignMask);
void *swift_retain(void *object);
void swift_release(void *object);
size_t swift_retainCount(void *object);
}

typedef CFIndex MOSNumberType;   /* CFNumberType */
typedef const struct __CFBoolean *MOSBooleanRef;
typedef const struct __CFNumber *MOSNumberRef;

namespace {
constexpr int kMaxTypes = 96;
struct CFType { const CFRuntimeClass *cls; };
CFType g_types[kMaxTypes];
int g_count = 0;

/* _cfinfoa (CFRuntimeBase do swift-corelibs: {isa, contador do Swift, _cfinfoa}) guarda 0x4D4F5300 | índice */
constexpr uint64_t kInfoTag = 0x4D4F530000000000ull;   /* "MOS" */
uint64_t &cfinfo(const void *o) { return ((uint64_t *)o)[2]; }
int type_index_of(const void *o) {
    if (*(void *const *)o != mos_cfobject_class) return -1;
    uint64_t v = cfinfo(o);
    if ((v & 0xFFFFFF0000000000ull) != kInfoTag) return -1;
    int i = (int)(v & 0xFFFF);
    return i < g_count ? i : -1;
}
bool is_const_string(const void *cf) { return cf && *(void *const *)cf == mos_cf_const_isa; }
/* CFSTR("...") do CoreFoundation do swift-corelibs: objeto Swift imortal com o layout do MOSString */
bool is_swift_const_string(const void *cf) { return cf && *(void *const *)cf == mos_swift_cfstr_isa; }
} // namespace

extern "C" {

/* deinit de SwiftFoundation._MOSCFObject: finalize do tipo CF; depois o Swift libera a memória */
void mos_cf_finalize(void *o) {
    int i = type_index_of(o);
    if (i >= 0 && g_types[i].cls->finalize) g_types[i].cls->finalize((CFTypeRef)o);
}

/* typeIDs para classes CF registradas: 0x400 + índice (fora dos tipos do sistema) */
CFTypeID _CFRuntimeRegisterClass(const CFRuntimeClass *const cls) {
    if (!cls || g_count >= kMaxTypes) return 0;
    g_types[g_count].cls = cls;
    return 0x400 + g_count++;
}

const CFRuntimeClass *_CFRuntimeGetClassWithTypeID(CFTypeID typeID) {
    CFTypeID i = typeID - 0x400;
    return (typeID >= 0x400 && i < (CFTypeID)g_count) ? g_types[i].cls : nullptr;
}

/* extraBytes = tamanho da struct além de CFRuntimeBase (24 bytes: isa, contador do Swift, _cfinfoa) */
CFTypeRef _CFRuntimeCreateInstance(CFAllocatorRef allocator, CFTypeID typeID, CFIndex extraBytes, unsigned char *category) {
    (void)allocator; (void)category;
    CFTypeID i = typeID - 0x400;
    if (typeID < 0x400 || i >= (CFTypeID)g_count || extraBytes < 0) return nullptr;
    size_t size = sizeof(CFRuntimeBase) + (size_t)extraBytes;
    void *o = swift_allocObject(mos_cfobject_class, size, 15);     /* isa + contador = 1 */
    if (!o) return nullptr;
    cfinfo(o) = kInfoTag | (uint64_t)i;
    memset((uint8_t *)o + sizeof(CFRuntimeBase), 0, (size_t)extraBytes);
    if (g_types[i].cls->init) g_types[i].cls->init((CFTypeRef)o);
    return (CFTypeRef)o;
}

/* CFRetain/CFRelease/CFGetRetainCount de verdade, valendo para TODO o app (C sem o prelude, Swift por
 * @_silgen_name...): objeto Swift do CF daqui -> swift_retain/release; constante CFSTR do swift-corelibs ->
 * imortal; o resto (objetos do libSystem: NSString, CGPath...) -> objc_retain/release, que é exatamente o
 * que o CFRetain/CFRelease do libSystem fazem (userland/libsystem/darwin.c). */
id objc_retain(id o);
void objc_release(id o);
CFTypeRef CFRetain(CFTypeRef cf) {
    if (!cf || is_swift_const_string(cf)) return cf;            /* constantes imortais */
    if (type_index_of(cf) >= 0) { swift_retain((void *)cf); return cf; }
    return (CFTypeRef)objc_retain((id)cf);
}
void CFRelease(CFTypeRef cf) {
    if (!cf || is_swift_const_string(cf)) return;
    if (type_index_of(cf) >= 0) { swift_release((void *)cf); return; }
    objc_release((id)cf);
}
CFIndex CFGetRetainCount(CFTypeRef cf) {
    if (cf && type_index_of(cf) >= 0) return (CFIndex)swift_retainCount((void *)cf);
    return 1;                                   /* o libSystem não expõe a contagem dos seus objetos */
}
/* nomes usados pelo prelude mos_openswiftui_prelude.h (código compilado antes desta mudança) */
CFTypeRef mos_oag_CFRetain(CFTypeRef cf) { return CFRetain(cf); }
void mos_oag_CFRelease(CFTypeRef cf) { CFRelease(cf); }
CFIndex mos_oag_CFGetRetainCount(CFTypeRef cf) { return CFGetRetainCount(cf); }
CFTypeRef CFAutorelease(CFTypeRef cf) {
    if (cf && type_index_of(cf) < 0 && !is_const_string(cf) && !is_swift_const_string(cf)) objc_autorelease((id)cf);
    return cf;
}

} // extern "C"

/* ======================================================================== tipos básicos */
namespace {
struct MOSString { CFRuntimeBase base; const char *ptr; CFIndex len; char *buf; CFIndex cap; };
struct MOSNumber { CFRuntimeBase base; MOSNumberType type; int is_float; union { int64_t i; double d; } v; };
struct MOSBoolean { CFRuntimeBase base; bool value; };
struct MOSArray { CFRuntimeBase base; const void **items; CFIndex count, cap; bool retains; };
struct MOSDict { CFRuntimeBase base; const void **keys, **vals; CFIndex count, cap; bool retain_keys, retain_vals; };
struct MOSData { CFRuntimeBase base; uint8_t *bytes; CFIndex length, capacity; };

void string_finalize(CFTypeRef cf) { free(((MOSString *)cf)->buf); }
void array_finalize(CFTypeRef cf) {
    MOSArray *a = (MOSArray *)cf;
    if (a->retains) for (CFIndex i = 0; i < a->count; i++) mos_oag_CFRelease(a->items[i]);
    free(a->items);
}
void dict_finalize(CFTypeRef cf) {
    MOSDict *d = (MOSDict *)cf;
    for (CFIndex i = 0; i < d->count; i++) {
        if (d->retain_keys) mos_oag_CFRelease(d->keys[i]);
        if (d->retain_vals) mos_oag_CFRelease(d->vals[i]);
    }
    free(d->keys); free(d->vals);
}
void data_finalize(CFTypeRef cf) { free(((MOSData *)cf)->bytes); }

const CFRuntimeClass string_class = {0, "CFString", nullptr, nullptr, string_finalize, nullptr, nullptr, nullptr, nullptr};
const CFRuntimeClass number_class = {0, "CFNumber", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
const CFRuntimeClass boolean_class = {0, "CFBoolean", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
const CFRuntimeClass array_class = {0, "CFArray", nullptr, nullptr, array_finalize, nullptr, nullptr, nullptr, nullptr};
const CFRuntimeClass dict_class = {0, "CFDictionary", nullptr, nullptr, dict_finalize, nullptr, nullptr, nullptr, nullptr};
const CFRuntimeClass data_class = {0, "CFData", nullptr, nullptr, data_finalize, nullptr, nullptr, nullptr, nullptr};

CFTypeID tid(const CFRuntimeClass &c) {
    for (int i = 0; i < g_count; i++) if (g_types[i].cls == &c) return 0x400 + (CFTypeID)i;
    return _CFRuntimeRegisterClass(&c);
}
template <class T> T *make(const CFRuntimeClass &c) {
    return (T *)_CFRuntimeCreateInstance(nullptr, tid(c), sizeof(T) - sizeof(CFRuntimeBase), nullptr);
}
template <class T> T *as(const void *cf, const CFRuntimeClass &c) {
    int i = cf ? type_index_of(cf) : -1;
    return (i >= 0 && g_types[i].cls == &c) ? (T *)cf : nullptr;
}
/* constante CFSTR("...") gerada pelo clang: {isa, flags, ptr, len}. Layout diferente do MOSString, cujo
 * CFRuntimeBase tem 24 bytes (DEPLOYMENT_RUNTIME_SWIFT: isa, contador do Swift, _cfinfoa). flags 0x7d0 = UTF-16. */
struct ConstString { const void *isa; uintptr_t flags; const char *ptr; CFIndex len; };
/* bytes UTF-8 de qualquer string (nossa ou constante CFSTR em 8 bits) */
bool str_bytes(const void *cf, const char **p, CFIndex *n) {
    if (!cf) return false;
    const MOSString *s = as<MOSString>(cf, string_class);
    if (!s && is_swift_const_string(cf)) s = (const MOSString *)cf;
    if (s) { *p = s->ptr ? s->ptr : ""; *n = s->len; return true; }
    if (is_const_string(cf)) {
        const ConstString *c = (const ConstString *)cf;
        if ((c->flags & 0x10) != 0) { *p = ""; *n = 0; return true; }   /* UTF-16 (literal não ASCII): ainda sem suporte */
        *p = c->ptr ? c->ptr : ""; *n = c->len;
        return true;
    }
    return false;
}
bool str_reserve(MOSString *s, CFIndex n) {
    if (s->buf && n + 1 <= s->cap) return true;
    CFIndex cap = s->cap ? s->cap : 32;
    while (cap < n + 1) cap *= 2;
    char *b = (char *)malloc((size_t)cap);
    if (!b) return false;
    if (s->len) memcpy(b, s->ptr, (size_t)s->len);
    b[s->len] = 0;
    free(s->buf);
    s->buf = b; s->ptr = b; s->cap = cap;
    return true;
}
void str_append(MOSString *s, const char *p, CFIndex n) {
    if (n <= 0 || !str_reserve(s, s->len + n)) return;
    memcpy(s->buf + s->len, p, (size_t)n);
    s->len += n;
    s->buf[s->len] = 0;
}
MOSString *str_new(const char *p, CFIndex n) {
    MOSString *s = make<MOSString>(string_class);
    if (!s) return nullptr;
    s->ptr = "";
    str_reserve(s, n);
    if (n > 0) str_append(s, p, n);
    return s;
}
/* UTF-16 -> UTF-8 */
void append_utf16(MOSString *s, const UniChar *c, CFIndex n) {
    for (CFIndex i = 0; i < n; i++) {
        uint32_t cp = c[i];
        if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < n && c[i + 1] >= 0xDC00 && c[i + 1] < 0xE000) cp = 0x10000 + ((cp - 0xD800) << 10) + (c[++i] - 0xDC00);
        char b[4]; int k;
        if (cp < 0x80) { b[0] = (char)cp; k = 1; }
        else if (cp < 0x800) { b[0] = (char)(0xC0 | cp >> 6); b[1] = (char)(0x80 | (cp & 63)); k = 2; }
        else if (cp < 0x10000) { b[0] = (char)(0xE0 | cp >> 12); b[1] = (char)(0x80 | ((cp >> 6) & 63)); b[2] = (char)(0x80 | (cp & 63)); k = 3; }
        else { b[0] = (char)(0xF0 | cp >> 18); b[1] = (char)(0x80 | ((cp >> 12) & 63)); b[2] = (char)(0x80 | ((cp >> 6) & 63)); b[3] = (char)(0x80 | (cp & 63)); k = 4; }
        str_append(s, b, k);
    }
}
/* percorre UTF-8 entregando unidades UTF-16 */
template <class F> void each_utf16(const char *p, CFIndex n, F f) {
    const uint8_t *u = (const uint8_t *)p;
    CFIndex i = 0;
    while (i < n) {
        uint32_t cp = u[i];
        int k = cp < 0x80 ? 1 : cp < 0xE0 ? 2 : cp < 0xF0 ? 3 : 4;
        if (k == 2 && i + 1 < n) cp = ((cp & 31) << 6) | (u[i + 1] & 63);
        else if (k == 3 && i + 2 < n) cp = ((cp & 15) << 12) | ((u[i + 1] & 63) << 6) | (u[i + 2] & 63);
        else if (k == 4 && i + 3 < n) cp = ((cp & 7) << 18) | ((u[i + 1] & 63) << 12) | ((u[i + 2] & 63) << 6) | (u[i + 3] & 63);
        i += k;
        if (cp >= 0x10000) { cp -= 0x10000; if (!f((UniChar)(0xD800 + (cp >> 10)))) return; if (!f((UniChar)(0xDC00 + (cp & 0x3FF)))) return; }
        else if (!f((UniChar)cp)) return;
    }
}
uint64_t fnv(const void *p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) { h ^= ((const uint8_t *)p)[i]; h *= 1099511628211ull; }
    return h;
}
double num_double(const MOSNumber *n) { return n->is_float ? n->v.d : (double)n->v.i; }
} // namespace

extern "C" {

/* ---- CFString */
CFTypeID CFStringGetTypeID(void) { return tid(string_class); }
CFStringRef CFStringCreateWithCString(CFAllocatorRef alloc, const char *cstr, CFStringEncoding enc) {
    (void)alloc; (void)enc;
    return cstr ? (CFStringRef)str_new(cstr, (CFIndex)strlen(cstr)) : nullptr;
}
CFStringRef CFStringCreateWithCharacters(CFAllocatorRef alloc, const UniChar *chars, CFIndex n) {
    (void)alloc;
    MOSString *s = str_new("", 0);
    if (s && chars) append_utf16(s, chars, n);
    return (CFStringRef)s;
}
CFStringRef CFStringCreateWithBytes(CFAllocatorRef alloc, const UInt8 *bytes, CFIndex n, CFStringEncoding enc, Boolean ext) {
    (void)alloc; (void)ext;
    if (!bytes && n) return nullptr;
    if (enc == kCFStringEncodingUnicode || enc == kCFStringEncodingUTF16LE || enc == kCFStringEncodingUTF16BE) {
        MOSString *s = str_new("", 0);
        CFIndex units = n / 2;
        for (CFIndex i = 0; s && i < units; i++) {
            UniChar c = enc == kCFStringEncodingUTF16BE ? (UniChar)(bytes[2 * i] << 8 | bytes[2 * i + 1]) : (UniChar)(bytes[2 * i] | bytes[2 * i + 1] << 8);
            if (i == 0 && (c == 0xFEFF)) continue;
            append_utf16(s, &c, 1);
        }
        return (CFStringRef)s;
    }
    if (enc == kCFStringEncodingISOLatin1 || enc == kCFStringEncodingWindowsLatin1 || enc == kCFStringEncodingMacRoman) {
        MOSString *s = str_new("", 0);
        for (CFIndex i = 0; s && i < n; i++) { UniChar c = bytes[i]; append_utf16(s, &c, 1); }
        return (CFStringRef)s;
    }
    return (CFStringRef)str_new((const char *)bytes, n);                 /* UTF-8 / ASCII */
}
CFStringRef CFStringCreateCopy(CFAllocatorRef alloc, CFStringRef str) {
    (void)alloc;
    const char *p; CFIndex n;
    return str_bytes(str, &p, &n) ? (CFStringRef)str_new(p, n) : nullptr;
}
CFMutableStringRef CFStringCreateMutable(CFAllocatorRef alloc, CFIndex maxLength) { (void)alloc; (void)maxLength; return (CFMutableStringRef)str_new("", 0); }
CFMutableStringRef CFStringCreateMutableCopy(CFAllocatorRef alloc, CFIndex maxLength, CFStringRef str) {
    (void)alloc; (void)maxLength;
    const char *p = ""; CFIndex n = 0;
    str_bytes(str, &p, &n);
    return (CFMutableStringRef)str_new(p, n);
}
void CFStringAppend(CFMutableStringRef str, CFStringRef appended) {
    MOSString *s = as<MOSString>(str, string_class);
    const char *p; CFIndex n;
    if (s && str_bytes(appended, &p, &n)) str_append(s, p, n);
}
void CFStringAppendCString(CFMutableStringRef str, const char *cstr, CFStringEncoding enc) {
    (void)enc;
    MOSString *s = as<MOSString>(str, string_class);
    if (s && cstr) str_append(s, cstr, (CFIndex)strlen(cstr));
}
CFIndex CFStringGetLength(CFStringRef str) {
    const char *p; CFIndex n, units = 0;
    if (!str_bytes(str, &p, &n)) return 0;
    each_utf16(p, n, [&](UniChar) { units++; return true; });
    return units;
}
UniChar CFStringGetCharacterAtIndex(CFStringRef str, CFIndex idx) {
    const char *p; CFIndex n, k = 0; UniChar out = 0;
    if (!str_bytes(str, &p, &n)) return 0;
    each_utf16(p, n, [&](UniChar c) { if (k++ == idx) { out = c; return false; } return true; });
    return out;
}
void CFStringGetCharacters(CFStringRef str, CFRange range, UniChar *buffer) {
    const char *p; CFIndex n, k = 0;
    if (!buffer || !str_bytes(str, &p, &n)) return;
    each_utf16(p, n, [&](UniChar c) {
        if (k >= range.location && k < range.location + range.length) buffer[k - range.location] = c;
        return ++k < range.location + range.length;
    });
}
const char *CFStringGetCStringPtr(CFStringRef str, CFStringEncoding enc) {
    const char *p; CFIndex n;
    if (enc != kCFStringEncodingUTF8 && enc != kCFStringEncodingASCII) return nullptr;
    return str_bytes(str, &p, &n) ? p : nullptr;                         /* sempre terminadas em NUL */
}
Boolean CFStringGetCString(CFStringRef str, char *buffer, CFIndex size, CFStringEncoding enc) {
    (void)enc;
    const char *p; CFIndex n;
    if (!buffer || size <= 0 || !str_bytes(str, &p, &n) || n + 1 > size) return false;
    memcpy(buffer, p, (size_t)n);
    buffer[n] = 0;
    return true;
}
CFIndex CFStringGetMaximumSizeForEncoding(CFIndex length, CFStringEncoding enc) {
    return enc == kCFStringEncodingUTF8 ? length * 3 : enc == kCFStringEncodingASCII || enc == kCFStringEncodingISOLatin1 ? length : length * 4;
}
CFComparisonResult CFStringCompare(CFStringRef a, CFStringRef b, CFStringCompareFlags opts) {
    (void)opts;
    const char *p = "", *q = ""; CFIndex n = 0, m = 0;
    str_bytes(a, &p, &n); str_bytes(b, &q, &m);
    int c = memcmp(p, q, (size_t)(n < m ? n : m));
    if (!c) c = n < m ? -1 : n > m ? 1 : 0;
    return c < 0 ? kCFCompareLessThan : c > 0 ? kCFCompareGreaterThan : kCFCompareEqualTo;
}
Boolean CFStringHasPrefix(CFStringRef s, CFStringRef pre) {
    const char *p, *q; CFIndex n, m;
    return str_bytes(s, &p, &n) && str_bytes(pre, &q, &m) && m <= n && !memcmp(p, q, (size_t)m);
}
Boolean CFStringHasSuffix(CFStringRef s, CFStringRef suf) {
    const char *p, *q; CFIndex n, m;
    return str_bytes(s, &p, &n) && str_bytes(suf, &q, &m) && m <= n && !memcmp(p + n - m, q, (size_t)m);
}

/* ---- CFNumber / CFBoolean */
static bool type_is_float(MOSNumberType t) { return t == 5 || t == 6 || t == 12 || t == 13 || t == 16; }
static size_t type_size(MOSNumberType t) {
    switch (t) {
    case 1: case 7: return 1;
    case 2: case 8: return 2;
    case 3: case 9: case 5: case 12: return 4;
    default: return 8;
    }
}
CFTypeID CFNumberGetTypeID(void) { return tid(number_class); }
MOSNumberRef CFNumberCreate(CFAllocatorRef alloc, MOSNumberType type, const void *value) {
    (void)alloc;
    if (!value) return nullptr;
    MOSNumber *n = make<MOSNumber>(number_class);
    if (!n) return nullptr;
    n->type = type;
    n->is_float = type_is_float(type);
    switch (type_size(type) * 2 + (n->is_float ? 1 : 0)) {
    case 2: n->v.i = *(const int8_t *)value; break;
    case 4: n->v.i = *(const int16_t *)value; break;
    case 8: n->v.i = *(const int32_t *)value; break;
    case 9: n->v.d = *(const float *)value; break;
    case 16: n->v.i = *(const int64_t *)value; break;
    default: n->v.d = *(const double *)value; break;
    }
    return (MOSNumberRef)n;
}
MOSNumberType CFNumberGetType(MOSNumberRef number) { const MOSNumber *n = as<MOSNumber>(number, number_class); return n ? n->type : 0; }
Boolean CFNumberIsFloatType(MOSNumberRef number) { const MOSNumber *n = as<MOSNumber>(number, number_class); return n && n->is_float; }
Boolean CFNumberGetValue(MOSNumberRef number, MOSNumberType type, void *out) {
    const MOSNumber *n = as<MOSNumber>(number, number_class);
    if (!n || !out) return false;
    double d = num_double(n);
    int64_t i = n->is_float ? (int64_t)n->v.d : n->v.i;
    switch (type_size(type) * 2 + (type_is_float(type) ? 1 : 0)) {
    case 2: *(int8_t *)out = (int8_t)i; break;
    case 4: *(int16_t *)out = (int16_t)i; break;
    case 8: *(int32_t *)out = (int32_t)i; break;
    case 9: *(float *)out = (float)d; break;
    case 16: *(int64_t *)out = i; break;
    default: *(double *)out = d; break;
    }
    return !(n->is_float && !type_is_float(type) && (double)i != n->v.d);
}
CFComparisonResult CFNumberCompare(MOSNumberRef a, MOSNumberRef b, void *ctx) {
    (void)ctx;
    const MOSNumber *x = as<MOSNumber>(a, number_class), *y = as<MOSNumber>(b, number_class);
    double p = x ? num_double(x) : 0, q = y ? num_double(y) : 0;
    return p < q ? kCFCompareLessThan : p > q ? kCFCompareGreaterThan : kCFCompareEqualTo;
}
MOSBooleanRef mos_kCFBooleanTrue __asm__("_kCFBooleanTrue") = nullptr;
MOSBooleanRef mos_kCFBooleanFalse __asm__("_kCFBooleanFalse") = nullptr;
__attribute__((constructor)) static void mos_cf_init_booleans(void) {
    MOSBoolean *t = make<MOSBoolean>(boolean_class), *f = make<MOSBoolean>(boolean_class);   /* nunca soltos */
    if (t) t->value = true;
    if (f) f->value = false;
    mos_kCFBooleanTrue = (MOSBooleanRef)t;
    mos_kCFBooleanFalse = (MOSBooleanRef)f;
}
CFTypeID CFBooleanGetTypeID(void) { return tid(boolean_class); }
Boolean CFBooleanGetValue(MOSBooleanRef b) { const MOSBoolean *v = as<MOSBoolean>(b, boolean_class); return v && v->value; }

/* ---- callbacks: qualquer conjunto não nulo = reter/soltar como CF (o caso de todo uso real) */
static const void *cb_retain(CFAllocatorRef a, const void *v) { (void)a; return mos_oag_CFRetain(v); }
static void cb_release(CFAllocatorRef a, const void *v) { (void)a; mos_oag_CFRelease(v); }
const CFArrayCallBacks kCFTypeArrayCallBacks = {0, cb_retain, cb_release, CFCopyDescription, CFEqual};
const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks = {0, cb_retain, cb_release, CFCopyDescription, CFEqual, CFHash};
const CFDictionaryKeyCallBacks kCFCopyStringDictionaryKeyCallBacks = {0, cb_retain, cb_release, CFCopyDescription, CFEqual, CFHash};
const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks = {0, cb_retain, cb_release, CFCopyDescription, CFEqual};

/* ---- CFArray */
CFTypeID CFArrayGetTypeID(void) { return tid(array_class); }
CFMutableArrayRef CFArrayCreateMutable(CFAllocatorRef alloc, CFIndex capacity, const CFArrayCallBacks *cb) {
    (void)alloc; (void)capacity;
    MOSArray *a = make<MOSArray>(array_class);
    if (a) a->retains = cb && cb->retain;
    return (CFMutableArrayRef)a;
}
void CFArrayAppendValue(CFMutableArrayRef array, const void *value) {
    MOSArray *a = as<MOSArray>(array, array_class);
    if (!a) return;
    if (a->count == a->cap) {
        CFIndex cap = a->cap ? a->cap * 2 : 8;
        const void **n = (const void **)realloc(a->items, (size_t)cap * sizeof(void *));
        if (!n) return;
        a->items = n; a->cap = cap;
    }
    a->items[a->count++] = a->retains ? mos_oag_CFRetain(value) : value;
}
CFArrayRef CFArrayCreate(CFAllocatorRef alloc, const void **values, CFIndex n, const CFArrayCallBacks *cb) {
    CFMutableArrayRef a = CFArrayCreateMutable(alloc, n, cb);
    for (CFIndex i = 0; a && values && i < n; i++) CFArrayAppendValue(a, values[i]);
    return a;
}
CFIndex CFArrayGetCount(CFArrayRef array) { const MOSArray *a = as<MOSArray>(array, array_class); return a ? a->count : 0; }
const void *CFArrayGetValueAtIndex(CFArrayRef array, CFIndex i) {
    const MOSArray *a = as<MOSArray>(array, array_class);
    return (a && i >= 0 && i < a->count) ? a->items[i] : nullptr;
}
void CFArrayApplyFunction(CFArrayRef array, CFRange range, CFArrayApplierFunction CF_NOESCAPE f, void *ctx) {
    const MOSArray *a = as<MOSArray>(array, array_class);
    for (CFIndex i = range.location; a && f && i < range.location + range.length && i < a->count; i++) f(a->items[i], ctx);
}

/* ---- CFDictionary (busca linear por CFEqual: dicionários de atributos de texto são pequenos) */
CFTypeID CFDictionaryGetTypeID(void) { return tid(dict_class); }
static CFIndex dict_find(const MOSDict *d, const void *key) {
    for (CFIndex i = 0; i < d->count; i++) if (d->keys[i] == key || (d->retain_keys && CFEqual(d->keys[i], key))) return i;
    return -1;
}
CFMutableDictionaryRef CFDictionaryCreateMutable(CFAllocatorRef alloc, CFIndex capacity, const CFDictionaryKeyCallBacks *kcb, const CFDictionaryValueCallBacks *vcb) {
    (void)alloc; (void)capacity;
    MOSDict *d = make<MOSDict>(dict_class);
    if (d) { d->retain_keys = kcb && kcb->retain; d->retain_vals = vcb && vcb->retain; }
    return (CFMutableDictionaryRef)d;
}
void CFDictionarySetValue(CFMutableDictionaryRef dict, const void *key, const void *value) {
    MOSDict *d = as<MOSDict>(dict, dict_class);
    if (!d) return;
    CFIndex i = dict_find(d, key);
    if (i >= 0) {
        const void *old = d->vals[i];
        d->vals[i] = d->retain_vals ? mos_oag_CFRetain(value) : value;
        if (d->retain_vals) mos_oag_CFRelease(old);
        return;
    }
    if (d->count == d->cap) {
        CFIndex cap = d->cap ? d->cap * 2 : 8;
        const void **k = (const void **)realloc(d->keys, (size_t)cap * sizeof(void *));
        if (!k) return;
        d->keys = k;
        const void **v = (const void **)realloc(d->vals, (size_t)cap * sizeof(void *));
        if (!v) return;
        d->vals = v; d->cap = cap;
    }
    d->keys[d->count] = d->retain_keys ? mos_oag_CFRetain(key) : key;
    d->vals[d->count] = d->retain_vals ? mos_oag_CFRetain(value) : value;
    d->count++;
}
void CFDictionaryRemoveValue(CFMutableDictionaryRef dict, const void *key) {
    MOSDict *d = as<MOSDict>(dict, dict_class);
    CFIndex i = d ? dict_find(d, key) : -1;
    if (i < 0) return;
    if (d->retain_keys) mos_oag_CFRelease(d->keys[i]);
    if (d->retain_vals) mos_oag_CFRelease(d->vals[i]);
    d->count--;
    d->keys[i] = d->keys[d->count]; d->vals[i] = d->vals[d->count];
}
CFDictionaryRef CFDictionaryCreate(CFAllocatorRef alloc, const void **keys, const void **values, CFIndex n,
                                   const CFDictionaryKeyCallBacks *kcb, const CFDictionaryValueCallBacks *vcb) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(alloc, n, kcb, vcb);
    for (CFIndex i = 0; d && keys && values && i < n; i++) CFDictionarySetValue(d, keys[i], values[i]);
    return d;
}
CFMutableDictionaryRef CFDictionaryCreateMutableCopy(CFAllocatorRef alloc, CFIndex capacity, CFDictionaryRef src) {
    const MOSDict *s = as<MOSDict>(src, dict_class);
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(alloc, capacity, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    for (CFIndex i = 0; d && s && i < s->count; i++) CFDictionarySetValue(d, s->keys[i], s->vals[i]);
    return d;
}
CFIndex CFDictionaryGetCount(CFDictionaryRef dict) { const MOSDict *d = as<MOSDict>(dict, dict_class); return d ? d->count : 0; }
Boolean CFDictionaryGetValueIfPresent(CFDictionaryRef dict, const void *key, const void **value) {
    const MOSDict *d = as<MOSDict>(dict, dict_class);
    CFIndex i = d ? dict_find(d, key) : -1;
    if (i < 0) return false;
    if (value) *value = d->vals[i];
    return true;
}
const void *CFDictionaryGetValue(CFDictionaryRef dict, const void *key) { const void *v = nullptr; CFDictionaryGetValueIfPresent(dict, key, &v); return v; }
Boolean CFDictionaryContainsKey(CFDictionaryRef dict, const void *key) { return CFDictionaryGetValueIfPresent(dict, key, nullptr); }
void CFDictionaryGetKeysAndValues(CFDictionaryRef dict, const void **keys, const void **values) {
    const MOSDict *d = as<MOSDict>(dict, dict_class);
    for (CFIndex i = 0; d && i < d->count; i++) { if (keys) keys[i] = d->keys[i]; if (values) values[i] = d->vals[i]; }
}
void CFDictionaryApplyFunction(CFDictionaryRef dict, CFDictionaryApplierFunction CF_NOESCAPE f, void *ctx) {
    const MOSDict *d = as<MOSDict>(dict, dict_class);
    for (CFIndex i = 0; d && f && i < d->count; i++) f(d->keys[i], d->vals[i], ctx);
}

/* ---- CFData */
static bool data_reserve(MOSData *d, CFIndex n) {
    if (n <= d->capacity) return true;
    CFIndex cap = d->capacity ? d->capacity : 64;
    while (cap < n) cap *= 2;
    uint8_t *b = (uint8_t *)realloc(d->bytes, (size_t)cap);
    if (!b) return false;
    memset(b + d->capacity, 0, (size_t)(cap - d->capacity));
    d->bytes = b; d->capacity = cap;
    return true;
}
CFTypeID CFDataGetTypeID(void) { return tid(data_class); }
CFMutableDataRef CFDataCreateMutable(CFAllocatorRef alloc, CFIndex capacity) { (void)alloc; (void)capacity; return (CFMutableDataRef)make<MOSData>(data_class); }
CFDataRef CFDataCreate(CFAllocatorRef alloc, const UInt8 *bytes, CFIndex length) {
    MOSData *d = (MOSData *)CFDataCreateMutable(alloc, length);
    if (d && bytes && length > 0 && data_reserve(d, length)) { memcpy(d->bytes, bytes, (size_t)length); d->length = length; }
    return (CFDataRef)d;
}
CFIndex CFDataGetLength(CFDataRef data) { const MOSData *d = as<MOSData>(data, data_class); return d ? d->length : 0; }
const UInt8 *CFDataGetBytePtr(CFDataRef data) { const MOSData *d = as<MOSData>(data, data_class); return d ? d->bytes : nullptr; }
UInt8 *CFDataGetMutableBytePtr(CFMutableDataRef data) { MOSData *d = as<MOSData>(data, data_class); return d ? d->bytes : nullptr; }
void CFDataSetLength(CFMutableDataRef data, CFIndex length) {
    MOSData *d = as<MOSData>(data, data_class);
    if (!d || length < 0 || !data_reserve(d, length)) return;
    if (length > d->length) memset(d->bytes + d->length, 0, (size_t)(length - d->length));
    d->length = length;
}
void CFDataAppendBytes(CFMutableDataRef data, const UInt8 *bytes, CFIndex length) {
    MOSData *d = as<MOSData>(data, data_class);
    if (!d || !bytes || length <= 0 || !data_reserve(d, d->length + length)) return;
    memcpy(d->bytes + d->length, bytes, (size_t)length);
    d->length += length;
}

/* ---- polimórficas */
CFTypeID CFGetTypeID(CFTypeRef cf) {
    if (!cf) return 0;
    if (is_const_string(cf) || is_swift_const_string(cf)) return CFStringGetTypeID();
    int i = type_index_of(cf);
    return i >= 0 ? 0x400 + (CFTypeID)i : 1;
}
Boolean CFEqual(CFTypeRef a, CFTypeRef b) {
    if (a == b) return true;
    if (!a || !b) return false;
    const char *p, *q; CFIndex n, m;
    if (str_bytes(a, &p, &n)) return str_bytes(b, &q, &m) && n == m && !memcmp(p, q, (size_t)n);
    int i = type_index_of(a), j = type_index_of(b);
    if (i < 0 || i != j) return false;
    const CFRuntimeClass *c = g_types[i].cls;
    if (c == &number_class) return num_double((const MOSNumber *)a) == num_double((const MOSNumber *)b);
    if (c == &data_class) { const MOSData *x = (const MOSData *)a, *y = (const MOSData *)b; return x->length == y->length && !memcmp(x->bytes, y->bytes, (size_t)x->length); }
    if (c == &array_class) {
        const MOSArray *x = (const MOSArray *)a, *y = (const MOSArray *)b;
        if (x->count != y->count) return false;
        for (CFIndex k = 0; k < x->count; k++) if (!CFEqual(x->items[k], y->items[k])) return false;
        return true;
    }
    if (c == &dict_class) {
        const MOSDict *x = (const MOSDict *)a, *y = (const MOSDict *)b;
        if (x->count != y->count) return false;
        for (CFIndex k = 0; k < x->count; k++) {
            const void *v;
            if (!CFDictionaryGetValueIfPresent((CFDictionaryRef)y, x->keys[k], &v) || !CFEqual(x->vals[k], v)) return false;
        }
        return true;
    }
    return c->equal ? c->equal(a, b) : false;
}
CFHashCode CFHash(CFTypeRef cf) {
    if (!cf) return 0;
    const char *p; CFIndex n;
    if (str_bytes(cf, &p, &n)) return (CFHashCode)fnv(p, (size_t)n);
    int i = type_index_of(cf);
    if (i >= 0) {
        const CFRuntimeClass *c = g_types[i].cls;
        if (c == &number_class) { double d = num_double((const MOSNumber *)cf); return (CFHashCode)fnv(&d, sizeof d); }
        if (c == &data_class) { const MOSData *x = (const MOSData *)cf; return (CFHashCode)fnv(x->bytes, (size_t)x->length); }
        if (c == &array_class) return (CFHashCode)((const MOSArray *)cf)->count;
        if (c == &dict_class) return (CFHashCode)((const MOSDict *)cf)->count;
        if (c->hash) return c->hash(cf);
    }
    return (CFHashCode)(uintptr_t)cf >> 4;
}
CFStringRef CFCopyDescription(CFTypeRef cf) {
    if (!cf) return nullptr;
    const char *p; CFIndex n;
    if (str_bytes(cf, &p, &n)) return (CFStringRef)str_new(p, n);
    char buf[96];
    if (const MOSNumber *x = as<MOSNumber>(cf, number_class)) {
        if (x->is_float) snprintf(buf, sizeof buf, "%g", x->v.d); else snprintf(buf, sizeof buf, "%lld", (long long)x->v.i);
    } else if (const MOSBoolean *b = as<MOSBoolean>(cf, boolean_class)) {
        snprintf(buf, sizeof buf, "%s", b->value ? "true" : "false");
    } else {
        int i = type_index_of(cf);
        snprintf(buf, sizeof buf, "<%s %p>", i >= 0 && g_types[i].cls->className ? g_types[i].cls->className : "CFType", cf);
    }
    return (CFStringRef)str_new(buf, (CFIndex)strlen(buf));
}

/* sem interoperabilidade ObjC a metadata de uma classe Swift já é a própria classe */
const void *swift_getObjCClassMetadata(const void *theClass) { return theClass; }
/* o Compute pergunta os ivars de objetos ObjC para compará-los: sem lista, compara por identidade */
void *class_copyIvarList(Class cls, unsigned int *outCount) { (void)cls; if (outCount) *outCount = 0; return nullptr; }

/* só o servidor de depuração do Compute usa (indisponível no mOS) */
CFURLRef CFURLCreateWithBytes(CFAllocatorRef alloc, const UInt8 *bytes, CFIndex length, CFStringEncoding encoding, CFURLRef baseURL) {
    (void)alloc; (void)bytes; (void)length; (void)encoding; (void)baseURL;
    return nullptr;
}

/* kCFAllocatorDefault é NULL no mOS ("use o padrão") */
CFAllocatorRef CFAllocatorGetDefault(void) { return nullptr; }

/* Mach: vm_remap como cópia (veja o cabeçalho) */
typedef int kern_return_t;
typedef unsigned int mach_port_t;
typedef uintptr_t vm_address_t;
typedef uintptr_t vm_size_t;
kern_return_t vm_remap(mach_port_t target, vm_address_t *target_addr, vm_size_t size, vm_address_t mask, int flags,
                       mach_port_t src_task, vm_address_t src_addr, int copy, int *cur_prot, int *max_prot, unsigned inherit) {
    (void)target; (void)mask; (void)flags; (void)src_task; (void)copy; (void)inherit;
    if (!target_addr || !*target_addr) return 4; /* KERN_INVALID_ARGUMENT */
    memcpy((void *)*target_addr, (const void *)src_addr, size);
    if (cur_prot) *cur_prot = 3;
    if (max_prot) *max_prot = 7;
    return 0;
}

} // extern "C"
