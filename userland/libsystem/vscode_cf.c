/*
 * mOS libSystem — o que faltava de CoreFoundation para o Electron/VS Code (inventário do machocheck):
 * locale, UUID constantes, conjuntos (CFSet), CFCharacterSet com bitmap, CFBinaryHeap, buscas e conversões
 * de CFString (UTF-16 <-> UTF-8), erros, recursos de URL, ponteiros de função do bundle e os singletons
 * vazios que os literais @[] e @{} referenciam (__NSArray0__struct, __NSDictionary0__struct).
 * As CFString do mOS guardam UTF-8 (CFStringGetLength conta bytes, em darwin.c); aqui as funções que
 * trabalham em UTF-16 (GetCharacters, GetCharacterAtIndex) decodificam e indexam em UTF-16 de verdade.
 */
#include "foundation.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int tolower(int c);
char *strrchr(const char *s, int c);
void *dlsym(void *h, const char *name);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id CFDataCreate(void *alloc, const u8 *bytes, long n);
id CFRetain(id o);
id CFPreferencesCopyAppValue(id key, id app);
id CFStringCreateWithCString(void *alloc, const char *s, u32 enc);
long write(int fd, const void *b, unsigned long n);

static id cls(const char *n) { return (id)cx_class_named(n); }
static BOOL resp(id o, const char *s) { return o && cx_responds(object_getClass(o), sel_registerName(s)); }
typedef struct { long loc, len; } cf_range_t;

/* ================================================================ singletons dos literais vazios */
extern cx_class_t NSArray_c_ __asm__("OBJC_CLASS_$_NSArray");
extern cx_class_t NSDictionary_c_ __asm__("OBJC_CLASS_$_NSDictionary");
/* antes do objeto ficam 16 bytes sem o OBJ_MAGIC: o retain/release do runtime não mexe neles */
__attribute__((used)) static struct { u64 pad[2]; nsarr_t o; } arr0 = {{0, 0}, {&NSArray_c_, 0, 0, 0}};
__attribute__((used)) static struct { u64 pad[2]; nsdict_t o; } dict0 = {{0, 0}, {&NSDictionary_c_, 0, 0, 0, 0}};
__asm__(".globl __NSArray0__struct\n.set __NSArray0__struct, arr0+16\n"
        ".globl __NSDictionary0__struct\n.set __NSDictionary0__struct, dict0+16\n");

/* ================================================================ constantes que não são strings */
static struct { u64 pad[2]; void *isa; } null_obj;
const void *kCFNull = 0;                      /* preenchido em cx_vscode_cf_init com [NSNull null] */
const void *kCFAllocatorMallocZone = (const void *)8;   /* alocador "padrão": os Create* ignoram o alocador */
const double kCFAbsoluteTimeIntervalSince1970 = 978307200.0;
/* CoreMedia: CMTime {value, timescale, flags, epoch} */
typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cm_time_t;
const cm_time_t kCMTimeZero = {0, 1, 1, 0};
const cm_time_t kCMTimeInvalid = {0, 0, 0, 0};
const cm_time_t kCMTimeIndefinite = {0, 0, 0x11, 0}; /* Valid | Indefinite */
const double kCLLocationAccuracyBest = -1.0;
const double kCLLocationAccuracyHundredMeters = 100.0;
/* CoreGraphics: a conexão do WindowServer (CGS*) é um inteiro qualquer diferente de zero */
int _CGSDefaultConnection(void) { return 1; }

void cx_vscode_cf_init(void) {
    (void)null_obj;
    id n = MSG0(id, cls("NSNull"), "null");
    kCFNull = n;
}

/* ================================================================ alocadores e tipos básicos
 * CFAllocatorCreate devolve um objeto de verdade, com o contexto (o CFRelease nele funciona; antes era a constante 8 e o
 * renderer do VS Code morria em objc_release ao criar uma fonte de memória, no SkFontMgr_mac do Skia). Os Create* do
 * libSystem continuam alocando com malloc; CFAllocatorAllocate/Reallocate/Deallocate usam as funções do contexto, e
 * CFDataCreateWithBytesNoCopy (cfdata.c), que copia os bytes, devolve-os na hora pelo deallocate do alocador. */
typedef struct {
    long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
    void *copy_desc;
    void *(*allocate)(long size, unsigned long hint, void *info);
    void *(*reallocate)(void *ptr, long size, unsigned long hint, void *info);
    void (*deallocate)(void *ptr, void *info);
    long (*preferred)(long size, unsigned long hint, void *info);
} cf_alloc_ctx_t;
typedef struct { cx_class_t *isa; cf_alloc_ctx_t ctx; } cf_alloc_t;
static cx_class_t cfalloc_c, cfalloc_m;
void cx_object_free(id o);
static void cfa_dealloc(id self, SEL c) {
    (void)c;
    cf_alloc_t *a = (cf_alloc_t *)self;
    if (a->ctx.release && a->ctx.info) a->ctx.release(a->ctx.info);
    cx_object_free(self);
}
/* o alocador personalizado por trás de um ponteiro (0 para as constantes, que no mOS são NULL ou 8) */
static cf_alloc_t *cf_alloc_obj(const void *a) {
    return (u64)a >= 0x10000 && ((cf_alloc_t *)a)->isa == &cfalloc_c ? (cf_alloc_t *)a : 0;
}
void *CFAllocatorCreate(void *alloc, const void *ctx) {
    (void)alloc;
    if (!ctx || !cfalloc_c.isa) return 0;
    cf_alloc_t *a = (cf_alloc_t *)cx_alloc_instance(&cfalloc_c);
    if (!a) return 0;
    memcpy(&a->ctx, ctx, sizeof a->ctx);
    if (a->ctx.retain && a->ctx.info) a->ctx.info = (void *)a->ctx.retain(a->ctx.info);
    return a;
}
void cx_cf_allocator_dispose_bytes(const void *alloc, void *bytes) {   /* cfdata.c */
    cf_alloc_t *a = cf_alloc_obj(alloc);
    if (a && a->ctx.deallocate && bytes) a->ctx.deallocate(bytes, a->ctx.info);
}
void *CFAllocatorAllocate(const void *alloc, long size, unsigned long hint) {
    cf_alloc_t *a = cf_alloc_obj(alloc);
    if (size <= 0) return 0;
    return a && a->ctx.allocate ? a->ctx.allocate(size, hint, a->ctx.info) : malloc((size_t)size);
}
void *CFAllocatorReallocate(const void *alloc, void *ptr, long size, unsigned long hint) {
    cf_alloc_t *a = cf_alloc_obj(alloc);
    if (a) {
        if (!ptr) return size > 0 && a->ctx.allocate ? a->ctx.allocate(size, hint, a->ctx.info) : 0;
        if (size <= 0) { if (a->ctx.deallocate) a->ctx.deallocate(ptr, a->ctx.info); return 0; }
        return a->ctx.reallocate ? a->ctx.reallocate(ptr, size, hint, a->ctx.info) : 0;
    }
    if (size <= 0) { free(ptr); return 0; }
    return realloc(ptr, (size_t)size);
}
void CFAllocatorDeallocate(const void *alloc, void *ptr) {
    cf_alloc_t *a = cf_alloc_obj(alloc);
    if (!ptr) return;
    if (a) { if (a->ctx.deallocate) a->ctx.deallocate(ptr, a->ctx.info); }
    else free(ptr);
}
long CFAllocatorGetPreferredSizeForSize(const void *alloc, long size, unsigned long hint) {
    cf_alloc_t *a = cf_alloc_obj(alloc);
    return a && a->ctx.preferred ? a->ctx.preferred(size, hint, a->ctx.info) : size;
}
void CFAllocatorGetContext(const void *alloc, void *ctx) {
    cf_alloc_t *a = cf_alloc_obj(alloc);
    if (!ctx) return;
    if (a) memcpy(ctx, &a->ctx, sizeof a->ctx);
    else { memset(ctx, 0, sizeof(cf_alloc_ctx_t)); }
}
const void *CFAllocatorGetDefault(void) { return 0; }
void CFAllocatorSetDefault(const void *alloc) { (void)alloc; }
unsigned long CFAllocatorGetTypeID(void) { return 2; }
id CFAutorelease(id o) { return o ? objc_autorelease(o) : 0; }
id CFCopyDescription(id o) { return o ? objc_retain(MSG0(id, o, "description")) : 0; }

/* ================================================================ locale e preferências */
static id cur_locale_id(void) {
    id l = MSG0(id, cls("NSLocale"), "currentLocale");
    id s = l && resp(l, "localeIdentifier") ? MSG0(id, l, "localeIdentifier") : 0;
    return s ? s : cx_str("pt_BR");
}
typedef struct { void *isa; id ident; } cfloc_t;
static cx_class_t loc_c, loc_m;
static id loc_new(id ident) {
    cfloc_t *l = (cfloc_t *)cx_alloc_instance(&loc_c);
    l->ident = objc_retain(ident ? ident : cx_str("pt_BR"));
    return (id)l;
}
static id loc_ident(id self, SEL c) { (void)c; return ((cfloc_t *)self)->ident; }
id CFLocaleCopyCurrent(void) { return loc_new(cur_locale_id()); }
id CFLocaleCreate(void *alloc, id ident) { (void)alloc; return loc_new(ident); }
/* chaves comparadas pelo texto (as constantes ficam em vscode_consts.c) */
id CFLocaleGetValue(id loc, id key) {
    const char *k = key ? cx_cstr(key) : "";
    id ident = loc && resp(loc, "localeIdentifier") ? MSG0(id, loc, "localeIdentifier") : cur_locale_id();
    const char *s = cx_cstr(ident);
    const char *us = s;
    while (*us && *us != '_' && *us != '-') us++;
    if (!strcmp(k, "kCFLocaleCountryCodeKey")) return *us ? cx_str(us + 1) : cx_str("BR");
    if (!strcmp(k, "kCFLocaleLanguageCodeKey")) return cx_str_n(s, (u64)(us - s));
    if (!strcmp(k, "kCFLocaleIdentifierKey") || !strcmp(k, "localeIdentifier")) return ident;
    if (!strcmp(k, "kCFLocaleDecimalSeparatorKey")) return cx_str(",");
    if (!strcmp(k, "kCFLocaleGroupingSeparatorKey")) return cx_str(".");
    if (!strcmp(k, "kCFLocaleUsesMetricSystemKey") || !strcmp(k, "kCFLocaleMeasurementSystemKey")) return cx_str("Metric");
    return 0;
}
u8 CFPreferencesGetAppBooleanValue(id key, id app, u8 *valid) {
    id v = CFPreferencesCopyAppValue(key, app);
    if (!v) { if (valid) *valid = 0; return 0; }
    if (valid) *valid = 1;
    u8 b = cx_is_str(v) ? (!strcmp(cx_cstr(v), "YES") || !strcmp(cx_cstr(v), "true") || !strcmp(cx_cstr(v), "1"))
                        : resp(v, "boolValue") && MSG0(BOOL, v, "boolValue");
    objc_release(v);
    return b;
}

/* ================================================================ UUID */
typedef struct { u8 b[16]; } cf_uuid_bytes_t;
typedef struct { void *isa; cf_uuid_bytes_t u; } cfuuid_t;
static cx_class_t uuid_c, uuid_m;
static id uuid_make(const cf_uuid_bytes_t *b) {
    cfuuid_t *u = (cfuuid_t *)cx_alloc_instance(&uuid_c);
    u->u = *b;
    return (id)u;
}
static id uuid_string(id self, SEL c) {
    (void)c;
    const u8 *b = ((cfuuid_t *)self)->u.b;
    char s[37];
    static const char *hx = "0123456789ABCDEF";
    int o = 0;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) s[o++] = '-';
        s[o++] = hx[b[i] >> 4]; s[o++] = hx[b[i] & 15];
    }
    s[o] = 0;
    return cx_str(s);
}
static void uuid_get_bytes(id self, SEL c, u8 *out) { (void)c; memcpy(out, ((cfuuid_t *)self)->u.b, 16); }
void arc4random_buf(void *buf, size_t n);
id CFUUIDCreate(void *alloc) {
    (void)alloc;
    cf_uuid_bytes_t b;
    arc4random_buf(b.b, 16);
    b.b[6] = (u8)((b.b[6] & 0x0F) | 0x40);
    b.b[8] = (u8)((b.b[8] & 0x3F) | 0x80);
    return uuid_make(&b);
}
id CFUUIDCreateFromUUIDBytes(void *alloc, cf_uuid_bytes_t b) { (void)alloc; return uuid_make(&b); }   /* Postico */
cf_uuid_bytes_t CFUUIDGetUUIDBytes(id u) {
    cf_uuid_bytes_t r;
    memset(&r, 0, sizeof r);
    if (!u) return r;
    if (object_getClass(u) == &uuid_c) return ((cfuuid_t *)u)->u;
    if (resp(u, "getUUIDBytes:")) MSG(void, u, "getUUIDBytes:", r.b);
    return r;
}
/* constante por bytes: a mesma instância para os mesmos bytes (o macOS garante identidade) */
#define UUIDC_MAX 64
static id uuidc[UUIDC_MAX];
id CFUUIDGetConstantUUIDWithBytes(void *alloc, u8 b0, u8 b1, u8 b2, u8 b3, u8 b4, u8 b5, u8 b6, u8 b7, u8 b8, u8 b9,
                                  u8 b10, u8 b11, u8 b12, u8 b13, u8 b14, u8 b15) {
    (void)alloc;
    cf_uuid_bytes_t b = {{b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15}};
    for (int i = 0; i < UUIDC_MAX && uuidc[i]; i++) if (!memcmp(((cfuuid_t *)uuidc[i])->u.b, b.b, 16)) return uuidc[i];
    id u = uuid_make(&b);
    for (int i = 0; i < UUIDC_MAX; i++) if (!uuidc[i]) { uuidc[i] = u; break; }
    return u;
}

/* ================================================================ CFSet (sobre NSMutableSet) */
id CFSetCreateMutable(void *alloc, long cap, const void *cb) { (void)alloc; (void)cap; (void)cb; return objc_retain(MSG0(id, cls("NSMutableSet"), "set")); }
void CFSetAddValue(id s, id v) {
    if (!s || !v) return;
    if (MSG(BOOL, s, "containsObject:", v)) return;
    MSG(void, s, "addObject:", v);
}
id CFSetCreate(void *alloc, const id *vals, long n, const void *cb) {
    id s = CFSetCreateMutable(alloc, n, cb);
    for (long i = 0; i < n; i++) CFSetAddValue(s, vals[i]);
    return s;
}
long CFSetGetCount(id s) { return s ? (long)MSG0(u64, s, "count") : 0; }
void CFSetGetValues(id s, id *out) {
    if (!s || !out) return;
    id a = MSG0(id, s, "allObjects");
    nsarr_t *r = (nsarr_t *)a;
    for (u64 i = 0; a && i < r->n; i++) out[i] = r->items[i];
}

/* ================================================================ CFString */
static const char *utf8(id s) { return s ? cx_cstr(s) : ""; }
/* UTF-8 -> UTF-16; devolve o número de unidades */
static long to_utf16(const char *s, u16 *out, long cap) {
    const u8 *p = (const u8 *)s;
    long n = 0;
    while (*p) {
        u32 c = *p++;
        if (c >= 0xF0 && p[0] && p[1] && p[2]) { c = (c & 7) << 18 | (u32)(p[0] & 63) << 12 | (u32)(p[1] & 63) << 6 | (p[2] & 63); p += 3; }
        else if (c >= 0xE0 && p[0] && p[1]) { c = (c & 15) << 12 | (u32)(p[0] & 63) << 6 | (p[1] & 63); p += 2; }
        else if (c >= 0xC0 && p[0]) { c = (c & 31) << 6 | (p[0] & 63); p += 1; }
        if (c >= 0x10000) {
            c -= 0x10000;
            if (out && n < cap) out[n] = (u16)(0xD800 | c >> 10);
            n++;
            if (out && n < cap) out[n] = (u16)(0xDC00 | (c & 0x3FF));
            n++;
        } else {
            if (out && n < cap) out[n] = (u16)c;
            n++;
        }
    }
    return n;
}
static char *from_utf16(const u16 *u, long n, u64 *len) {
    char *o = malloc((size_t)n * 3 + 1);
    u64 w = 0;
    if (!o) return 0;
    for (long i = 0; i < n; i++) {
        u32 c = u[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n && u[i + 1] >= 0xDC00 && u[i + 1] < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (u[i + 1] - 0xDC00); i++; }
        if (c < 0x80) o[w++] = (char)c;
        else if (c < 0x800) { o[w++] = (char)(0xC0 | c >> 6); o[w++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { o[w++] = (char)(0xE0 | c >> 12); o[w++] = (char)(0x80 | (c >> 6 & 63)); o[w++] = (char)(0x80 | (c & 63)); }
        else { o[w++] = (char)(0xF0 | c >> 18); o[w++] = (char)(0x80 | (c >> 12 & 63)); o[w++] = (char)(0x80 | (c >> 6 & 63)); o[w++] = (char)(0x80 | (c & 63)); }
    }
    o[w] = 0;
    *len = w;
    return o;
}
id CFStringCreateWithCharacters(void *alloc, const u16 *chars, long n) {
    (void)alloc;
    u64 len = 0;
    char *s = from_utf16(chars, n, &len);
    if (!s) return 0;
    id r = objc_retain(cx_str_n(s, len));
    free(s);
    return r;
}
void CFStringGetCharacters(id s, cf_range_t r, u16 *out) {
    long n = to_utf16(utf8(s), 0, 0);
    u16 *all = malloc((size_t)(n + 1) * 2);
    if (!all) return;
    to_utf16(utf8(s), all, n);
    for (long i = 0; i < r.len; i++) out[i] = r.loc + i < n ? all[r.loc + i] : 0;
    free(all);
}
u16 CFStringGetCharacterAtIndex(id s, long i) {
    u16 c = 0;
    cf_range_t r = {i, 1};
    CFStringGetCharacters(s, r, &c);
    return c;
}
id CFStringCreateWithCStringNoCopy(void *alloc, const char *s, u32 enc, void *dealloc) {
    (void)dealloc;
    return CFStringCreateWithCString(alloc, s, enc);
}
u8 CFStringHasSuffix(id s, id suf) {
    const char *a = utf8(s), *b = utf8(suf);
    size_t na = strlen(a), nb = strlen(b);
    return nb <= na && !memcmp(a + na - nb, b, nb);
}
enum { kCFCompareCaseInsensitive = 1, kCFCompareBackwards = 4, kCFCompareAnchored = 8 };
/* índices em unidades UTF-16, como o CFStringGetLength (darwin.c) e o CFStringGetCharacters */
static int eq16_at(const u16 *a, const u16 *b, long n, int ci) {
    for (long i = 0; i < n; i++) {
        u16 x = a[i], y = b[i];
        if (ci) { if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; }
        if (x != y) return 0;
    }
    return 1;
}
u8 CFStringFindWithOptions(id s, id find, cf_range_t range, u64 opts, cf_range_t *res) {
    const char *a8 = utf8(s), *b8 = utf8(find);
    long na = to_utf16(a8, 0, 0), nb = to_utf16(b8, 0, 0);
    long lo = range.loc < 0 ? 0 : range.loc, hi = range.loc + range.len;
    if (hi > na) hi = na;
    int ci = (opts & kCFCompareCaseInsensitive) != 0;
    if (!nb || hi - lo < nb) return 0;
    u16 *a = malloc((size_t)(na + 1) * 2), *b = malloc((size_t)(nb + 1) * 2);
    u8 found = 0;
    if (a && b) {
        to_utf16(a8, a, na);
        to_utf16(b8, b, nb);
        if (opts & kCFCompareBackwards) {
            long start = opts & kCFCompareAnchored ? hi - nb : lo;
            for (long i = hi - nb; i >= start && !found; i--)
                if (eq16_at(a + i, b, nb, ci)) { if (res) { res->loc = i; res->len = nb; } found = 1; }
        } else {
            long end = opts & kCFCompareAnchored ? lo : hi - nb;
            for (long i = lo; i <= end && !found; i++)
                if (eq16_at(a + i, b, nb, ci)) { if (res) { res->loc = i; res->len = nb; } found = 1; }
        }
    }
    free(a);
    free(b);
    return found;
}
id CFStringCreateExternalRepresentation(void *alloc, id s, u32 enc, u8 loss) {
    (void)alloc; (void)loss;
    const char *a = utf8(s);
    if (enc == 0x100 /* kCFStringEncodingUnicode = UTF-16 com BOM */ || enc == 0x14000100 /* UTF16LE */) {
        long n = to_utf16(a, 0, 0);
        int bom = enc == 0x100;
        u16 *u = malloc((size_t)(n + 1) * 2);
        if (!u) return 0;
        if (bom) u[0] = 0xFEFF;
        to_utf16(a, u + bom, n);
        id d = CFDataCreate(0, (const u8 *)u, (n + bom) * 2);
        free(u);
        return d;
    }
    return CFDataCreate(0, (const u8 *)a, (long)strlen(a));
}
/* hifenização: só dizer "não disponível" faz o Chromium não quebrar palavras */
u8 CFStringIsHyphenationAvailableForLocale(id loc) { (void)loc; return 0; }
long CFStringGetHyphenationLocationBeforeIndex(id s, long i, cf_range_t r, u64 opts, id loc, u32 *ch) {
    (void)s; (void)i; (void)r; (void)opts; (void)loc; (void)ch;
    return -1;   /* kCFNotFound */
}
static int hexval(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
id CFURLCreateStringByReplacingPercentEscapes(void *alloc, id s, id leave) {
    (void)alloc;
    const char *a = utf8(s), *keep = leave ? utf8(leave) : 0;
    size_t n = strlen(a);
    char *o = malloc(n + 1);
    size_t w = 0;
    if (!o) return 0;
    for (size_t i = 0; i < n; i++) {
        int h, l;
        if (a[i] == '%' && i + 2 < n + 0 + 1 && (h = hexval(a[i + 1])) >= 0 && (l = hexval(a[i + 2])) >= 0) {
            char c = (char)(h << 4 | l);
            int stay = !keep;      /* NULL: não decodifica nada, como no macOS */
            for (const char *k = keep; k && *k; k++) if (*k == c) stay = 1;
            if (!stay) { o[w++] = c; i += 2; continue; }
        }
        o[w++] = a[i];
    }
    id r = objc_retain(cx_str_n(o, w));
    free(o);
    return r;
}

/* ================================================================ CFCharacterSet (bitmap de 0x110000 bits) */
typedef struct { void *isa; u8 *bits; } cfcs_t;
static cx_class_t cs_c, cs_m;
#define CS_BYTES (0x110000 / 8)
static id cs_new(void) {
    cfcs_t *c = (cfcs_t *)cx_alloc_instance(&cs_c);
    c->bits = calloc(1, CS_BYTES);
    return (id)c;
}
static u8 *cs_bits(id s) { return s && object_getClass(s) == &cs_c ? ((cfcs_t *)s)->bits : 0; }
static void cs_add(u8 *b, u32 lo, u32 hi) { for (u32 c = lo; c <= hi && c < 0x110000; c++) b[c >> 3] |= (u8)(1 << (c & 7)); }
static int cs_has(const u8 *b, u32 c) { return b && c < 0x110000 && (b[c >> 3] >> (c & 7) & 1); }
id CFCharacterSetCreateMutable(void *alloc) { (void)alloc; return cs_new(); }
void CFCharacterSetAddCharactersInRange(id s, cf_range_t r) { u8 *b = cs_bits(s); if (b && r.len > 0) cs_add(b, (u32)r.loc, (u32)(r.loc + r.len - 1)); }
void CFCharacterSetAddCharactersInString(id s, id str) {
    u8 *b = cs_bits(s);
    if (!b) return;
    long n = to_utf16(utf8(str), 0, 0);
    u16 *u = malloc((size_t)(n + 1) * 2);
    if (!u) return;
    to_utf16(utf8(str), u, n);
    for (long i = 0; i < n; i++) cs_add(b, u[i], u[i]);
    free(u);
}
void CFCharacterSetUnion(id s, id other) {
    u8 *a = cs_bits(s), *b = cs_bits(other);
    if (a && b) for (u32 i = 0; i < CS_BYTES; i++) a[i] |= b[i];
}
u8 CFCharacterSetIsLongCharacterMember(id s, u32 c) { return (u8)cs_has(cs_bits(s), c); }
/* conjuntos predefinidos (kCFCharacterSet*): faixas ASCII/Latin-1 e os blocos mais comuns */
id CFCharacterSetGetPredefined(long which) {
    static id cache[16];
    if (which < 1 || which > 15) return 0;
    if (cache[which]) return cache[which];
    id s = cs_new();
    u8 *b = cs_bits(s);
    switch (which) {
    case 1: cs_add(b, ' ', ' '); cs_add(b, '\t', '\t'); cs_add(b, 0xA0, 0xA0); cs_add(b, 0x2000, 0x200B); cs_add(b, 0x3000, 0x3000); break;
    case 2: cs_add(b, ' ', ' '); cs_add(b, '\t', '\r'); cs_add(b, 0x85, 0x85); cs_add(b, 0xA0, 0xA0); cs_add(b, 0x2000, 0x200B); cs_add(b, 0x2028, 0x2029); cs_add(b, 0x3000, 0x3000); break;
    case 3: cs_add(b, '0', '9'); cs_add(b, 0x660, 0x669); cs_add(b, 0xFF10, 0xFF19); break;
    case 4: case 9:
        cs_add(b, 'A', 'Z'); cs_add(b, 'a', 'z'); cs_add(b, 0xAA, 0xAA); cs_add(b, 0xB5, 0xB5); cs_add(b, 0xBA, 0xBA);
        cs_add(b, 0xC0, 0xD6); cs_add(b, 0xD8, 0xF6); cs_add(b, 0xF8, 0x2FF); cs_add(b, 0x370, 0x3FF); cs_add(b, 0x400, 0x52F);
        cs_add(b, 0x4E00, 0x9FFF); cs_add(b, 0x3040, 0x30FF); cs_add(b, 0xAC00, 0xD7A3);
        if (which == 9) { cs_add(b, '0', '9'); }
        break;
    case 5: cs_add(b, 'a', 'z'); cs_add(b, 0xDF, 0xF6); cs_add(b, 0xF8, 0xFF); break;
    case 6: cs_add(b, 'A', 'Z'); cs_add(b, 0xC0, 0xD6); cs_add(b, 0xD8, 0xDE); break;
    case 13: cs_add(b, 'A', 'Z'); cs_add(b, 0xC0, 0xD6); cs_add(b, 0xD8, 0xDE); break;
    case 7: cs_add(b, 0x300, 0x36F); cs_add(b, 0x20D0, 0x20FF); break;
    case 8: cs_add(b, 0xC0, 0xC5); cs_add(b, 0xC7, 0xCF); cs_add(b, 0xD1, 0xD6); cs_add(b, 0xD9, 0xDD); cs_add(b, 0xE0, 0xE5); cs_add(b, 0xE7, 0xEF); cs_add(b, 0xF1, 0xF6); cs_add(b, 0xF9, 0xFD); break;
    case 10: cs_add(b, '!', '#'); cs_add(b, '%', '*'); cs_add(b, ',', '/'); cs_add(b, ':', ';'); cs_add(b, '?', '@'); cs_add(b, '[', ']'); cs_add(b, '_', '_'); cs_add(b, '{', '{'); cs_add(b, '}', '}'); cs_add(b, 0xA1, 0xA1); cs_add(b, 0xBF, 0xBF); cs_add(b, 0x2010, 0x2027); break;
    case 11: cs_add(b, 0, 0x1F); cs_add(b, 0x7F, 0x9F); break;
    case 12: cs_add(b, 0xFFFE, 0xFFFF); cs_add(b, 0xD800, 0xDFFF); break;
    case 14: cs_add(b, '$', '$'); cs_add(b, '+', '+'); cs_add(b, '<', '>'); cs_add(b, '^', '^'); cs_add(b, '`', '`'); cs_add(b, '|', '|'); cs_add(b, '~', '~'); cs_add(b, 0xA2, 0xA9); cs_add(b, 0x2100, 0x2BFF); cs_add(b, 0x1F300, 0x1FAFF); break;
    case 15: cs_add(b, '\n', '\r'); cs_add(b, 0x85, 0x85); cs_add(b, 0x2028, 0x2029); break;
    }
    cache[which] = s;
    return s;
}
/* o formato do macOS: 8192 bytes do plano básico */
id CFCharacterSetCreateBitmapRepresentation(void *alloc, id s) {
    (void)alloc;
    u8 *b = cs_bits(s);
    u8 z[8192];
    if (!b) { memset(z, 0, sizeof z); return CFDataCreate(0, z, 8192); }
    return CFDataCreate(0, b, 8192);
}

/* ================================================================ CFBinaryHeap */
typedef struct {
    long version;
    const void *(*retain)(void *alloc, const void *p);
    void (*release)(void *alloc, const void *p);
    id (*desc)(const void *p);
    long (*compare)(const void *a, const void *b, void *ctx);
} cfbh_cb_t;
typedef struct { long version; void *info; void *retain, *release, *desc; } cfbh_ctx_t;
typedef struct { void *isa; const void **v; long n, cap; cfbh_cb_t cb; void *info; } cfbh_t;
static cx_class_t bh_c, bh_m;
static long bh_cmp(cfbh_t *h, const void *a, const void *b) {
    if (h->cb.compare) return h->cb.compare(a, b, h->info);
    return a < b ? -1 : a > b;
}
id CFBinaryHeapCreate(void *alloc, long cap, const cfbh_cb_t *cb, const cfbh_ctx_t *ctx) {
    (void)alloc;
    cfbh_t *h = (cfbh_t *)cx_alloc_instance(&bh_c);
    h->cap = cap > 8 ? cap : 8;
    h->v = malloc(sizeof(void *) * (size_t)h->cap);
    if (cb) h->cb = *cb;
    if (ctx) h->info = ctx->info;
    return (id)h;
}
void CFBinaryHeapAddValue(id hp, const void *v) {
    cfbh_t *h = (cfbh_t *)hp;
    if (!h) return;
    if (h->n == h->cap) {
        const void **nv = realloc(h->v, sizeof(void *) * (size_t)h->cap * 2);
        if (!nv) return;
        h->v = nv; h->cap *= 2;
    }
    if (h->cb.retain) v = h->cb.retain(0, v);
    long i = h->n++;
    h->v[i] = v;
    while (i > 0) {
        long p = (i - 1) / 2;
        if (bh_cmp(h, h->v[i], h->v[p]) >= 0) break;
        const void *t = h->v[i]; h->v[i] = h->v[p]; h->v[p] = t;
        i = p;
    }
}
u8 CFBinaryHeapGetMinimumIfPresent(id hp, const void **out) {
    cfbh_t *h = (cfbh_t *)hp;
    if (!h || !h->n) return 0;
    if (out) *out = h->v[0];
    return 1;
}
void CFBinaryHeapRemoveMinimumValue(id hp) {
    cfbh_t *h = (cfbh_t *)hp;
    if (!h || !h->n) return;
    const void *min = h->v[0];
    h->v[0] = h->v[--h->n];
    long i = 0;
    for (;;) {
        long l = 2 * i + 1, r = l + 1, m = i;
        if (l < h->n && bh_cmp(h, h->v[l], h->v[m]) < 0) m = l;
        if (r < h->n && bh_cmp(h, h->v[r], h->v[m]) < 0) m = r;
        if (m == i) break;
        const void *t = h->v[i]; h->v[i] = h->v[m]; h->v[m] = t;
        i = m;
    }
    if (h->cb.release) h->cb.release(0, min);
}

/* ================================================================ erros, URLs e bundles */
id CFErrorCopyUserInfo(id e) {
    id i = e && resp(e, "userInfo") ? MSG0(id, e, "userInfo") : 0;
    return objc_retain(i ? i : MSG0(id, cls("NSDictionary"), "dictionary"));
}
id CFErrorCopyDescription(id e) {
    id d = e && resp(e, "localizedDescription") ? MSG0(id, e, "localizedDescription") : 0;
    return objc_retain(d ? d : cx_str("Operação não concluída."));
}
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } urlk_t;
#define URLK(var, text) \
    static urlk_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_cs;
URLK(kCFURLVolumeNameKey, "NSURLVolumeNameKey")
URLK(kCFURLVolumeTotalCapacityKey, "NSURLVolumeTotalCapacityKey")
URLK(kCFURLVolumeAvailableCapacityKey, "NSURLVolumeAvailableCapacityKey")
URLK(kCFURLVolumeAvailableCapacityForImportantUsageKey, "NSURLVolumeAvailableCapacityForImportantUsageKey")
URLK(kCFURLVolumeIsLocalKey, "NSURLVolumeIsLocalKey")
URLK(kCFURLVolumeIsInternalKey, "NSURLVolumeIsInternalKey")
URLK(kCFURLVolumeIsBrowsableKey, "NSURLVolumeIsBrowsableKey")
URLK(kCFURLVolumeIsEjectableKey, "NSURLVolumeIsEjectableKey")
URLK(kCFURLVolumeIsRemovableKey, "NSURLVolumeIsRemovableKey")
URLK(kCFURLIsExcludedFromBackupKey, "NSURLIsExcludedFromBackupKey")
extern void *kCFBooleanTrue, *kCFBooleanFalse;
id CFNumberCreate(void *alloc, long type, const void *p);
id CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
void CFDictionarySetValue(id d, id key, id val);
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
int statfs(const char *path, void *buf);

typedef struct {
    u32 f_bsize; i32 f_iosize; u64 f_blocks, f_bfree, f_bavail, f_files, f_ffree; i32 f_fsid[2]; u32 f_owner;
    u32 f_type, f_flags, f_fssubtype; char f_fstypename[16], f_mntonname[1024], f_mntfromname[1024]; u32 f_flags_ext, f_reserved[7];
} vol_statfs_t;
_Static_assert(sizeof(vol_statfs_t) == 2168, "statfs do volume");

static const char *url_fs_path(id url) {
    if (!url) return 0;
    if (resp(url, "fileSystemRepresentation")) {
        const char *p = MSG0(const char *, url, "fileSystemRepresentation");
        if (p && *p) return p;
    }
    if (resp(url, "path")) {
        id s = MSG0(id, url, "path");
        const char *p = s ? cx_cstr(s) : 0;
        if (p && *p) return p;
    }
    return 0;
}
/* 1 = preenchido, 0 = não é chave de volume, -1 = a chave é e o caminho não existe. */
static int vol_prop(id url, const char *k, id *value) {
    int which = 0;
    if (!strcmp(k, "NSURLVolumeNameKey")) which = 1;
    else if (!strcmp(k, "NSURLVolumeTotalCapacityKey")) which = 2;
    else if (!strcmp(k, "NSURLVolumeAvailableCapacityKey")) which = 3;
    else if (!strcmp(k, "NSURLVolumeAvailableCapacityForImportantUsageKey")) which = 4;
    else if (!strcmp(k, "NSURLVolumeIsLocalKey")) which = 5;
    else if (!strcmp(k, "NSURLVolumeIsInternalKey")) which = 6;
    else if (!strcmp(k, "NSURLVolumeIsBrowsableKey")) which = 7;
    else if (!strcmp(k, "NSURLVolumeIsEjectableKey")) which = 8;
    else if (!strcmp(k, "NSURLVolumeIsRemovableKey")) which = 9;
    else if (!strcmp(k, "NSURLIsExcludedFromBackupKey")) which = 10;
    else return 0;
    const char *path = url_fs_path(url);
    vol_statfs_t fs;
    if (!path || statfs(path, &fs) < 0) return -1;
    id v = 0;
    if (which == 1) v = objc_retain(cx_str("mOS"));
    else if (which == 2 || which == 3 || which == 4) {
        /* ImportantUsage = disponível: não há espaço purgável separado neste volume. */
        unsigned long long bytes = (which == 2 ? fs.f_blocks : fs.f_bavail) * fs.f_bsize;
        long long n = (long long)bytes;
        v = CFNumberCreate(0, 4, &n);
    } else if (which == 10) v = objc_retain((id)kCFBooleanFalse);
    else {
        int yes = which == 7 || ((which == 5 || which == 6) && (fs.f_flags & 0x00001000u));
        v = objc_retain((id)(yes ? kCFBooleanTrue : kCFBooleanFalse));
    }
    if (value) *value = v;
    return v ? 1 : -1;
}
u8 CFURLCopyResourcePropertyForKey(id url, id key, id *value, id *err) {
    if (err) *err = 0;
    if (value) *value = 0;
    if (!url || !key) return 0;
    if (resp(url, "getResourceValue:forKey:error:")) {
        id v = 0;
        BOOL ok = MSG(BOOL, url, "getResourceValue:forKey:error:", &v, key, (id *)0);
        if (ok && value) *value = v ? objc_retain(v) : 0;
        if (ok) return 1;
    }
    const char *k = cx_cstr(key);
    if (!strcmp(k, "NSURLLocalizedNameKey") || !strcmp(k, "NSURLNameKey")) {
        id p = resp(url, "lastPathComponent") ? MSG0(id, url, "lastPathComponent") : 0;
        if (value) *value = p ? objc_retain(p) : 0;
        return p != 0;
    }
    int vol = vol_prop(url, k ? k : "", value);
    if (vol) return vol > 0;
    return 1;    /* propriedade sem valor: sucesso com NULL, como o macOS para chaves que não se aplicam */
}
id CFURLCopyResourcePropertiesForKeys(id url, id keys, id *err) {
    if (err) *err = 0;
    if (!url || !keys) return 0;
    id d = CFDictionaryCreateMutable(0, 0, 0, 0);
    if (!d) return 0;
    long n = CFArrayGetCount(keys);
    for (long i = 0; i < n; i++) {
        id k = CFArrayGetValueAtIndex(keys, i);
        id v = 0;
        if (!CFURLCopyResourcePropertyForKey(url, k, &v, err)) { objc_release(d); return 0; }
        if (v) { CFDictionarySetValue(d, k, v); objc_release(v); }
    }
    return d;
}
void CFBundleGetPackageInfo(id bundle, u32 *type, u32 *creator) {
    (void)bundle;
    if (type) *type = 0x4150504C;      /* 'APPL' */
    if (creator) *creator = 0x3F3F3F3F; /* '????' */
}
void *CFBundleGetFunctionPointerForName(id bundle, id name) {
    (void)bundle;
    return name ? dlsym((void *)-2 /* RTLD_DEFAULT */, cx_cstr(name)) : 0;
}

/* ================================================================ CFMachPort e alertas */
id CFMachPortCreateRunLoopSource(void *alloc, id port, long order) {
    (void)alloc; (void)port; (void)order;
    return objc_retain(MSG0(id, cls("NSObject"), "new"));
}
void CFMachPortInvalidate(id port) { (void)port; }
/* sem servidor de alertas do sistema: responde "padrão" (kCFUserNotificationDefaultResponse) */
int CFUserNotificationDisplayAlert(double timeout, u64 flags, id icon, id sound, id loc, id header, id msg,
                                   id def, id alt, id other, u64 *response) {
    (void)timeout; (void)flags; (void)icon; (void)sound; (void)loc; (void)def; (void)alt; (void)other;
    if (header) { const char *h = cx_cstr(header); write(2, h, strlen(h)); write(2, "\n", 1); }
    if (msg) { const char *m = cx_cstr(msg); write(2, m, strlen(m)); write(2, "\n", 1); }
    if (response) *response = 0;
    return 0;
}

/* ================================================================ Foundation: NSGetSizeAndAlignment */
/* tamanho e alinhamento de um tipo do Objective-C codificado (@encode) */
static const char *skip_type(const char *t, u64 *size, u64 *align);
static int strchr_q(char c) { return c == 'r' || c == 'n' || c == 'N' || c == 'o' || c == 'O' || c == 'R' || c == 'V' || c == 'A' || c == 'j'; }
static const char *skip_num(const char *t) { while (*t >= '0' && *t <= '9') t++; return t; }
static const char *skip_type(const char *t, u64 *size, u64 *align) {
    while (*t && strchr_q(*t)) t++;
    u64 s = 0, a = 1;
    switch (*t) {
    case 'c': case 'C': case 'B': s = a = 1; t++; break;
    case 's': case 'S': s = a = 2; t++; break;
    case 'i': case 'I': case 'f': s = a = 4; t++; break;
    case 'l': case 'L': s = a = 8; t++; break;
    case 'q': case 'Q': case 'd': case '@': case '#': case ':': case '*': case '?': s = a = 8; t++;
        if (t[-1] == '@' && *t == '"') { t++; while (*t && *t != '"') t++; if (*t) t++; }
        if (t[-1] == '@' && *t == '?') t++;
        break;
    case '^': { u64 is, ia; t = skip_type(t + 1, &is, &ia); s = a = 8; break; }
    case '[': {
        long n = 0;
        t++;
        while (*t >= '0' && *t <= '9') n = n * 10 + (*t++ - '0');
        u64 es, ea;
        t = skip_type(t, &es, &ea);
        if (*t == ']') t++;
        s = es * (u64)n; a = ea;
        break;
    }
    case '{': case '(': {
        char close = *t == '{' ? '}' : ')';
        int uni = *t == '(';
        t++;
        while (*t && *t != '=' && *t != close) t++;
        if (*t == '=') t++;
        while (*t && *t != close) {
            u64 fs, fa;
            if (*t == '"') { t++; while (*t && *t != '"') t++; if (*t) t++; continue; }
            t = skip_type(t, &fs, &fa);
            if (fa > a) a = fa;
            if (uni) { if (fs > s) s = fs; }
            else { s = (s + fa - 1) / fa * fa + fs; }
        }
        if (*t) t++;
        s = (s + a - 1) / a * a;
        break;
    }
    case 'b': t = skip_num(t + 1); s = a = 4; break;
    case 'v': s = 0; a = 1; t++; break;
    default: if (*t) t++; break;
    }
    t = skip_num(t);
    *size = s;
    *align = a;
    return t;
}
const char *NSGetSizeAndAlignment(const char *type, u64 *size, u64 *align) {
    u64 s = 0, a = 1;
    const char *r = skip_type(type, &s, &a);
    if (size) *size = s;
    if (align) *align = a;
    return r;
}

/* ================================================================ classes internas */
void cx_vscode_cf_classes(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&loc_c, &loc_m, "__mOSCFLocale", ob, 8);
    cx_add_method(&loc_c, "localeIdentifier", (void *)loc_ident);
    cx_define_class(&uuid_c, &uuid_m, "__mOSCFUUID", ob, 16);
    cx_add_method(&uuid_c, "UUIDString", (void *)uuid_string);
    cx_add_method(&uuid_c, "description", (void *)uuid_string);
    cx_add_method(&uuid_c, "getUUIDBytes:", (void *)uuid_get_bytes);
    cx_define_class(&cs_c, &cs_m, "__mOSCFCharacterSet", ob, 8);
    cx_define_class(&bh_c, &bh_m, "__mOSCFBinaryHeap", ob, (u32)(sizeof(cfbh_t) - 8));
    cx_define_class(&cfalloc_c, &cfalloc_m, "__NSCFAllocator", ob, (u32)(sizeof(cf_alloc_t) - 8));
    cx_add_method(&cfalloc_c, "dealloc", (void *)cfa_dealloc);
    cx_vscode_cf_init();
}
