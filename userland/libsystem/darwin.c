/*
 * mOS libSystem — funções da libc do macOS que apps grandes usam e que faltavam (porte do VS Code,
 * docs/porte-vscode.md). O mOS só tem a codificação UTF-8; as variantes _l ignoram o locale.
 */
#include "sys.h"

size_t strlen(const char *s);
int cx_fdprintf(int fd, const char *fmt, ...);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *malloc(size_t n);
void free(void *p);

typedef int wchar_t_;
typedef struct { u32 pending; int need; } cx_mbstate_t;   /* cabe no mbstate_t do macOS (128 bytes) */

/* decodifica um caractere UTF-8; devolve bytes usados, 0 para o NUL, -1 inválido, -2 incompleto */
static long utf8_dec(wchar_t_ *out, const unsigned char *s, size_t n) {
    if (!n) return -2;
    unsigned c = s[0];
    int len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (!len) return -1;
    if ((size_t)len > n) return -2;
    unsigned v = len == 1 ? c : c & (0x3F >> (len - 1));
    for (int i = 1; i < len; i++) {
        if ((s[i] & 0xC0) != 0x80) return -1;
        v = v << 6 | (s[i] & 0x3F);
    }
    if (out) *out = (wchar_t_)v;
    return v ? len : 0;
}

int mbtowc(wchar_t_ *pwc, const char *s, size_t n) {
    if (!s) return 0;                               /* UTF-8 não tem estado */
    long r = utf8_dec(pwc, (const unsigned char *)s, n);
    return r < 0 ? -1 : (int)r;
}
int mbtowc_l(wchar_t_ *pwc, const char *s, size_t n, void *loc) { (void)loc; return mbtowc(pwc, s, n); }

size_t mbrtowc(wchar_t_ *pwc, const char *s, size_t n, void *ps) {
    (void)ps;
    if (!s) return 0;
    long r = utf8_dec(pwc, (const unsigned char *)s, n);
    return r == -1 ? (size_t)-1 : r == -2 ? (size_t)-2 : (size_t)r;
}
size_t mbrtowc_l(wchar_t_ *pwc, const char *s, size_t n, void *ps, void *loc) { (void)loc; return mbrtowc(pwc, s, n, ps); }
int mblen(const char *s, size_t n) { return mbtowc(0, s, n); }
int mblen_l(const char *s, size_t n, void *loc) { (void)loc; return mblen(s, n); }
size_t mbrlen(const char *s, size_t n, void *ps) { return mbrtowc(0, s, n, ps); }
size_t mbrlen_l(const char *s, size_t n, void *ps, void *loc) { (void)loc; return mbrlen(s, n, ps); }

int wctomb(char *s, wchar_t_ wc) {
    if (!s) return 0;
    unsigned v = (unsigned)wc;
    if (v < 0x80) { s[0] = (char)v; return 1; }
    if (v < 0x800) { s[0] = (char)(0xC0 | v >> 6); s[1] = (char)(0x80 | (v & 0x3F)); return 2; }
    if (v < 0x10000) { s[0] = (char)(0xE0 | v >> 12); s[1] = (char)(0x80 | ((v >> 6) & 0x3F)); s[2] = (char)(0x80 | (v & 0x3F)); return 3; }
    if (v < 0x110000) {
        s[0] = (char)(0xF0 | v >> 18); s[1] = (char)(0x80 | ((v >> 12) & 0x3F));
        s[2] = (char)(0x80 | ((v >> 6) & 0x3F)); s[3] = (char)(0x80 | (v & 0x3F));
        return 4;
    }
    return -1;
}
int wctomb_l(char *s, wchar_t_ wc, void *loc) { (void)loc; return wctomb(s, wc); }
size_t wcrtomb(char *s, wchar_t_ wc, void *ps) { (void)ps; char b[4]; int r = wctomb(s ? s : b, wc); return r < 0 ? (size_t)-1 : (size_t)r; }
size_t wcrtomb_l(char *s, wchar_t_ wc, void *ps, void *loc) { (void)loc; return wcrtomb(s, wc, ps); }
int mbsinit(const void *ps) { (void)ps; return 1; }

/* ---- diagnóstico do porte (/Sistema/Biblioteca/dyld-registrar): o carregador liga os stubs de
 * funções ausentes a esta função, que avisa uma vez por nome e devolve 0 */
/* RendererMainPlatformDelegate::EnableSandbox chama estes três e exige que existam
 * (o retorno de CGSSetDenyWindowServerConnections tem de ser kCGErrorSuccess, em coregraphics.c).
 * Não há LaunchServices nem CoreServices da Apple: o efeito pedido é "este processo não fala com eles". */
void SetApplicationIsDaemon(int is_daemon) { (void)is_daemon; }
void _LSSetApplicationLaunchServicesServerConnectionStatus(int status, void *err_block) { (void)status; (void)err_block; }
void _CSCheckFixDisable(void) {}

long cx_unresolved_log(const char *name) {
    static const char *seen[1024];
    static int nseen;
    for (int i = 0; i < nseen; i++) if (seen[i] == name) return 0;
    if (nseen < 1024) seen[nseen++] = name;
    cx_fdprintf(2, "dyld[mOS]: ausente (seguindo com 0): %s\n", name);
    return 0;
}

/* ---- thread_local por thread (chamado por cx_tlv_bootstrap, tlv.S). desc = {thunk, key, offset};
 * key = registro da imagem {modelo, tamanho inicializado, tamanho total, índice}. Os blocos de cada
 * thread ficam numa tabela no slot TSD_TLV da TSD (extra.c: cx_tsd). */
/* slots fixos da TSD (abaixo de KEY_FIRST = 128, extra.c), um por uso — NÃO repetir:
 *   100 TSD_THREAD_DICT (nsurl.c)   101 TSD_THREAD_OBJ (nsurl.c)   102 TSD_MIG_REPLY (mach.c)   103 TSD_ERRNO (libc.c)
 *   104 TSD_POOL (objc.c: autorelease pool da thread)
 *   110 TSD_TLV (aqui)
 * O TLV já esteve no 101 e colidia com o NSThread: um helper do Chrome que chamava +[NSThread currentThread]
 * tinha a tabela de thread_local sobrescrita pelo objeto e caía no próximo acesso a uma variável thread_local. */
#define TSD_TLV 110
#define TLV_IMAGES 128      /* índices de imagem (MAX_IMAGES de kernel/compat/macho.c, inclui as do dlopen) */
void **cx_tsd(void);
void *calloc(size_t n, size_t s);
void *cx_tlv_get(const u64 *desc) {
    const u64 *rec = (const u64 *)desc[1];
    void **t = cx_tsd();
    void **tab = t[TSD_TLV];
    if (!tab) { tab = calloc(TLV_IMAGES, sizeof(void *)); t[TSD_TLV] = tab; }
    u64 idx = rec[3] < TLV_IMAGES ? rec[3] : 0;
    if (!tab[idx]) {
        u64 total = rec[2] ? rec[2] : 16;
        u8 *b = calloc(1, (size_t)total);                 /* calloc: a parte __thread_bss já vem zerada */
        if (!b) {
            cx_fdprintf(2, "tlv[mOS]: sem memória para o TLS da thread (imagem %llu, %llu bytes, registro %llx)\n",
                        (unsigned long long)idx, (unsigned long long)total, (unsigned long long)(u64)rec);
            return (u8 *)0 + desc[2];
        }
        if (rec[0] && rec[1]) memcpy(b, (const void *)rec[0], (size_t)rec[1]);
        tab[idx] = b;
    }
    return (u8 *)tab[idx] + desc[2];
}

/* ---- os_log, syslog e asl: oslog.c (vão ao logd) */
/* objetos os_* (os_log, os_activity...): os do mOS são estáticos, então retain/release não fazem nada */
void *os_retain(void *o) { return o; }
void os_release(void *o) { (void)o; }

/* ---- CoreFoundation em ponte com o Foundation do mOS (CFStringRef = NSString*, CFBundleRef = NSBundle*...) */
typedef void *id_;
typedef struct cx_class cx_class_t;
cx_class_t *cx_class_named(const char *name);
id_ objc_msgSend(id_ self, void *op, ...);
void *sel_registerName(const char *name);
id_ objc_retain(id_ o);
void objc_release(id_ o);
#define MSG0_(T, o, s) ((T (*)(id_, void *))objc_msgSend)((id_)(o), sel_registerName(s))
#define MSG1_(T, o, s, a) ((T (*)(id_, void *, __typeof__(a)))objc_msgSend)((id_)(o), sel_registerName(s), (a))

/* abaixo de 64 KiB não há objeto: são as constantes falsas do mOS (kCFAllocatorMallocZone = 8...), que no macOS são
 * objetos estáticos em que retain/release não fazem nada */
id_ CFRetain(id_ o) { return (u64)o >= 0x10000 ? objc_retain(o) : o; }
void CFRelease(id_ o) { if ((u64)o >= 0x10000) objc_release(o); }

id_ CFBundleGetMainBundle(void) { return MSG0_(id_, cx_class_named("NSBundle"), "mainBundle"); }
/* regra "Create/Copy": quem chama é dono (+1) */
id_ CFStringCreateWithFileSystemRepresentation(void *alloc, const char *s) {
    (void)alloc;
    if (!s) return 0;
    id_ str = MSG1_(id_, cx_class_named("NSString"), "stringWithUTF8String:", s);
    return str ? objc_retain(str) : 0;
}

id_ CFBundleGetValueForInfoDictionaryKey(id_ bundle, id_ key) {
    return bundle && key ? MSG1_(id_, bundle, "objectForInfoDictionaryKey:", key) : 0;
}

/* ---- constantes CFString do CoreFoundation, no formato de @"..." ({isa, flags, ptr, len}) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cx_cfconst_t;
#define CFCONST(var, text) \
    static cx_cfconst_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;
CFCONST(kCFBundleNameKey, "CFBundleName")
CFCONST(kCFBundleIdentifierKey, "CFBundleIdentifier")
CFCONST(kCFBundleVersionKey, "CFBundleVersion")
CFCONST(kCFBundleExecutableKey, "CFBundleExecutable")
CFCONST(kCFBundleInfoDictionaryVersionKey, "CFBundleInfoDictionaryVersion")
CFCONST(kCFBundleDevelopmentRegionKey, "CFBundleDevelopmentRegion")
CFCONST(kCFBundleLocalizationsKey, "CFBundleLocalizations")
CFCONST(kCFRunLoopDefaultMode, "kCFRunLoopDefaultMode")
CFCONST(kCFRunLoopCommonModes, "kCFRunLoopCommonModes")
CFCONST(NSDefaultRunLoopMode, "kCFRunLoopDefaultMode")
CFCONST(NSRunLoopCommonModes, "kCFRunLoopCommonModes")
CFCONST(NSLocalizedDescriptionKey, "NSLocalizedDescription")
CFCONST(NSLocalizedFailureReasonErrorKey, "NSLocalizedFailureReason")
CFCONST(NSLocalizedRecoverySuggestionErrorKey, "NSLocalizedRecoverySuggestion")
/* quarentena (LaunchServices): o Homebrew grava com.apple.quarantine nos downloads dos casks pela Fiddle */
CFCONST(kLSQuarantineAgentNameKey, "LSQuarantineAgentName")
CFCONST(kLSQuarantineAgentBundleIdentifierKey, "LSQuarantineAgentBundleIdentifier")
CFCONST(kLSQuarantineTimeStampKey, "LSQuarantineTimeStamp")
CFCONST(kCFURLQuarantinePropertiesKey, "NSURLQuarantinePropertiesKey")
/* o VFS não guarda atributos estendidos: a propriedade (ex.: a quarentena) é aceita e não fica gravada */
int CFURLSetResourcePropertyForKey(const void *url, const void *key, const void *value, void **err) { (void)url; (void)key; (void)value; if (err) *err = 0; return 1; }
CFCONST(NSUnderlyingErrorKey, "NSUnderlyingError")
CFCONST(NSFilePathErrorKey, "NSFilePath")
CFCONST(NSCocoaErrorDomain, "NSCocoaErrorDomain")
CFCONST(NSPOSIXErrorDomain, "NSPOSIXErrorDomain")
CFCONST(NSOSStatusErrorDomain, "NSOSStatusErrorDomain")
CFCONST(NSInvalidArgumentException, "NSInvalidArgumentException")
CFCONST(NSInternalInconsistencyException, "NSInternalInconsistencyException")
CFCONST(NSGenericException, "NSGenericException")
CFCONST(NSRangeException, "NSRangeException")
CFCONST(kCFErrorDomainPOSIX, "NSPOSIXErrorDomain")
CFCONST(kCFErrorDomainOSStatus, "NSOSStatusErrorDomain")
CFCONST(kCFErrorDomainCocoa, "NSCocoaErrorDomain")
CFCONST(kCFErrorLocalizedDescriptionKey, "NSLocalizedDescription")
CFCONST(kCFErrorUnderlyingErrorKey, "NSUnderlyingError")
CFCONST(kCFLocaleIdentifier, "locale:id")
CFCONST(kCFPreferencesCurrentApplication, "kCFPreferencesCurrentApplication")
CFCONST(kCFPreferencesAnyApplication, "kCFPreferencesAnyApplication")
CFCONST(kCFPreferencesCurrentUser, "kCFPreferencesCurrentUser")
CFCONST(kCFPreferencesAnyUser, "kCFPreferencesAnyUser")
CFCONST(kCFPreferencesCurrentHost, "kCFPreferencesCurrentHost")
CFCONST(kCFPreferencesAnyHost, "kCFPreferencesAnyHost")
/* kCFAllocatorDefault e kCFAllocatorSystemDefault são NULL de propósito: "use o padrão" */
const void *kCFAllocatorDefault = 0, *kCFAllocatorSystemDefault = 0, *kCFAllocatorMalloc = 0, *kCFAllocatorNull = 0;
id_ CFBundleGetIdentifier(id_ bundle) { return bundle ? MSG0_(id_, bundle, "bundleIdentifier") : 0; }

/* CFString: o NSString do mOS guarda UTF-8; os índices (CFRange) são tratados como bytes, o que vale
 * para ASCII (chaves, caminhos e nomes que o Chromium converte ao iniciar) */
#define ENC_UTF8  0x08000100u
#define ENC_ASCII 0x0600u
#define ENC_UTF16 0x0100u
/* as outras que o Chromium e o Node pedem: UTF-16 com ordem explícita (o base::SysNSStringToUTF16 usa LE), UTF-32,
 * Latin-1 e Mac Roman */
#define ENC_UTF16BE 0x10000100u
#define ENC_UTF16LE 0x14000100u
#define ENC_UTF32   0x0c000100u
#define ENC_UTF32BE 0x18000100u
#define ENC_UTF32LE 0x1c000100u
#define ENC_LATIN1  0x0201u
#define ENC_MACROMAN 0u
static const char *cf_utf8(id_ s) { return s ? MSG0_(const char *, s, "UTF8String") : 0; }
/* UTF-8 -> UTF-16 (sem terminador): devolve o número de unidades; out NULL só conta */
static long cf_u16(const char *s, u16 *out) {
    long n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (p && *p) {
        u32 c;
        int k;
        if (*p < 0x80) { c = *p; k = 1; }
        else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) { c = (u32)(*p & 0x1F) << 6 | (p[1] & 0x3F); k = 2; }
        else if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            c = (u32)(*p & 0x0F) << 12 | (u32)(p[1] & 0x3F) << 6 | (p[2] & 0x3F); k = 3;
        } else if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            c = (u32)(*p & 0x07) << 18 | (u32)(p[1] & 0x3F) << 12 | (u32)(p[2] & 0x3F) << 6 | (p[3] & 0x3F); k = 4;
        } else { c = 0xFFFD; k = 1; }
        p += k;
        if (c >= 0x10000) {
            if (out) { c -= 0x10000; out[n] = (u16)(0xD800 | (c >> 10)); out[n + 1] = (u16)(0xDC00 | (c & 0x3FF)); }
            n += 2;
        } else { if (out) out[n] = (u16)c; n++; }
    }
    return n;
}
/* em unidades UTF-16, como no CoreFoundation (e como o -[NSString length]); era em bytes UTF-8 */
long CFStringGetLength(id_ s) { const char *u = cf_utf8(s); return u ? cf_u16(u, 0) : 0; }
const char *CFStringGetCStringPtr(id_ s, u32 enc) { return (enc == ENC_UTF8 || enc == ENC_ASCII) ? cf_utf8(s) : 0; }
unsigned char CFStringGetCString(id_ s, char *buf, long size, u32 enc) {
    (void)enc;
    const char *u = cf_utf8(s);
    if (!u || !buf || size <= 0) return 0;
    size_t n = strlen(u);
    if ((long)n + 1 > size) return 0;
    memcpy(buf, u, n + 1);
    return 1;
}
/* CFIndex CFStringGetBytes(str, CFRange{loc,len}, enc, lossByte, isExternal, buffer, maxBufLen, usedBufLen*) */
/* range em unidades UTF-16; devolve quantas unidades converteu e, em *used, os bytes. O Chromium converte o texto
 * digitado com kCFStringEncodingUTF16LE: antes só a codificação 0x0100 era UTF-16 (e só ASCII), a LE caía no ramo UTF-8
 * e uma letra virava 1 byte = 0 unidades, e nenhum caractere chegava ao renderer. Sem buffer só mede. Caractere que a
 * codificação não tem vira lossByte, ou a conversão para ali (lossByte 0), como no CoreFoundation. */
long CFStringGetBytes(id_ s, long loc, long len, u32 enc, u8 loss, u8 ext, u8 *buf, long max, long *used) {
    (void)ext;
    const char *u = cf_utf8(s);
    long total = u ? cf_u16(u, 0) : 0;
    if (loc < 0 || loc > total) loc = total;
    if (len < 0 || loc + len > total) len = total - loc;
    u16 *w = total ? malloc((size_t)total * 2) : 0;
    if (total && !w) { if (used) *used = 0; return 0; }
    if (w) cf_u16(u, w);
    int u16le = enc == ENC_UTF16 || enc == ENC_UTF16LE, u16be = enc == ENC_UTF16BE;
    int u32le = enc == ENC_UTF32 || enc == ENC_UTF32LE, u32be = enc == ENC_UTF32BE;
    u32 limit = enc == ENC_ASCII || enc == ENC_MACROMAN ? 0x80 : enc == ENC_LATIN1 ? 0x100 : 0;   /* 0 = UTF-8 */
    long out = 0, i = 0;
    while (i < len) {
        u32 c = w[loc + i];
        int units = 1;
        if (!u16le && !u16be && c >= 0xD800 && c < 0xDC00 && i + 1 < len && w[loc + i + 1] >= 0xDC00 && w[loc + i + 1] < 0xE000) {
            c = 0x10000 + ((c - 0xD800) << 10) + (w[loc + i + 1] - 0xDC00);
            units = 2;
        }
        u8 t[4];
        int k;
        if (u16le) { t[0] = (u8)c; t[1] = (u8)(c >> 8); k = 2; }
        else if (u16be) { t[0] = (u8)(c >> 8); t[1] = (u8)c; k = 2; }
        else if (u32le) { t[0] = (u8)c; t[1] = (u8)(c >> 8); t[2] = (u8)(c >> 16); t[3] = (u8)(c >> 24); k = 4; }
        else if (u32be) { t[0] = (u8)(c >> 24); t[1] = (u8)(c >> 16); t[2] = (u8)(c >> 8); t[3] = (u8)c; k = 4; }
        else if (limit) {
            if (c < limit) t[0] = (u8)c;
            else if (loss) t[0] = loss;
            else break;
            k = 1;
        } else if (c < 0x80) { t[0] = (u8)c; k = 1; }
        else if (c < 0x800) { t[0] = (u8)(0xC0 | c >> 6); t[1] = (u8)(0x80 | (c & 0x3F)); k = 2; }
        else if (c < 0x10000) { t[0] = (u8)(0xE0 | c >> 12); t[1] = (u8)(0x80 | ((c >> 6) & 0x3F)); t[2] = (u8)(0x80 | (c & 0x3F)); k = 3; }
        else { t[0] = (u8)(0xF0 | c >> 18); t[1] = (u8)(0x80 | ((c >> 12) & 0x3F)); t[2] = (u8)(0x80 | ((c >> 6) & 0x3F)); t[3] = (u8)(0x80 | (c & 0x3F)); k = 4; }
        if (buf) {
            if (out + k > max) break;
            memcpy(buf + out, t, (size_t)k);
        }
        out += k;
        i += units;
    }
    free(w);
    if (used) *used = out;
    return i;
}
/* tamanho máximo do caminho em UTF-8 (+ NUL) */
long CFStringGetMaximumSizeOfFileSystemRepresentation(id_ s) { return CFStringGetLength(s) * 3 + 1; }
long CFStringGetMaximumSizeForEncoding(long len, u32 enc) { return enc == ENC_UTF16 ? len * 2 : len * 3; }
u8 CFStringGetFileSystemRepresentation(id_ s, char *buf, long max) { return CFStringGetCString(s, buf, max, ENC_UTF8); }
/* CFComparisonResult CFStringCompare(a, b, options): kCFCompareCaseInsensitive = 1 */
long CFStringCompare(id_ a, id_ b, u64 opts) {
    const char *x = cf_utf8(a), *y = cf_utf8(b);
    if (!x) x = "";
    if (!y) y = "";
    for (;; x++, y++) {
        int cx = (unsigned char)*x, cy = (unsigned char)*y;
        if (opts & 1) { if (cx >= 'A' && cx <= 'Z') cx += 32; if (cy >= 'A' && cy <= 'Z') cy += 32; }
        if (cx != cy) return cx < cy ? -1 : 1;
        if (!cx) return 0;
    }
}
/* CFDictionary/CFArray/CFNumber em ponte com NSDictionary/NSArray/NSNumber */
id_ CFDictionaryGetValue(id_ d, id_ key) { return d && key ? MSG1_(id_, d, "objectForKey:", key) : 0; }
u8 CFDictionaryGetValueIfPresent(id_ d, id_ key, id_ *out) { id_ v = CFDictionaryGetValue(d, key); if (out) *out = v; return v != 0; }
u8 CFDictionaryContainsKey(id_ d, id_ key) { return CFDictionaryGetValue(d, key) != 0; }
long CFDictionaryGetCount(id_ d) { return d ? MSG0_(long, d, "count") : 0; }
long CFArrayGetCount(id_ a) { return a ? MSG0_(long, a, "count") : 0; }
id_ CFArrayGetValueAtIndex(id_ a, long i) { return a ? MSG1_(id_, a, "objectAtIndex:", (u64)i) : 0; }

/* CFNotificationCenter: o centro local é o NSNotificationCenter padrão; observadores C ainda não são
 * chamados (registro aceito), e o centro "Darwin" (entre processos) não existe no mOS */
id_ CFNotificationCenterGetLocalCenter(void) { return MSG0_(id_, cx_class_named("NSNotificationCenter"), "defaultCenter"); }
id_ CFNotificationCenterGetDarwinNotifyCenter(void) { return CFNotificationCenterGetLocalCenter(); }
id_ CFNotificationCenterGetDistributedCenter(void) { return CFNotificationCenterGetLocalCenter(); }
void CFNotificationCenterAddObserver(id_ c, const void *obs, void *cb, id_ name, const void *obj, long behavior) {
    (void)c; (void)obs; (void)cb; (void)name; (void)obj; (void)behavior;
}
void CFNotificationCenterRemoveObserver(id_ c, const void *obs, id_ name, const void *obj) { (void)c; (void)obs; (void)name; (void)obj; }
void CFNotificationCenterRemoveEveryObserver(id_ c, const void *obs) { (void)c; (void)obs; }
void CFNotificationCenterPostNotification(id_ c, id_ name, const void *obj, id_ info, u8 now) { (void)c; (void)name; (void)obj; (void)info; (void)now; }

/* SystemConfiguration (SCDynamicStore, proxies, CFNetworkCopySystemProxySettings): sysconfig.c, sobre o configd */

u8 CFEqual(id_ a, id_ b) { return a == b || (a && b && MSG1_(signed char, a, "isEqual:", b)); }
u64 CFHash(id_ a) { return a ? MSG0_(u64, a, "hash") : 0; }

/* "NoCopy": o mOS copia mesmo assim (o desalocador do chamador não é usado) */
id_ CFStringCreateWithBytes(void *alloc, const u8 *b, long n, u32 enc, u8 ext);
id_ CFStringCreateWithBytesNoCopy(void *alloc, const u8 *b, long n, u32 enc, u8 ext, void *dealloc) {
    (void)dealloc;
    return CFStringCreateWithBytes(alloc, b, n, enc, ext);
}
long long llround(double x) { return x >= 0 ? (long long)(x + 0.5) : -(long long)(-x + 0.5); }
long long llroundf(float x) { return llround((double)x); }

id_ CFStringCreateWithCString(void *alloc, const char *s, u32 enc) { (void)enc; return CFStringCreateWithFileSystemRepresentation(alloc, s); }
/* um ponto de código em UTF-8 */
static long put_utf8(char *t, unsigned c) {
    if (c < 0x80) { t[0] = (char)c; return 1; }
    if (c < 0x800) { t[0] = (char)(0xC0 | c >> 6); t[1] = (char)(0x80 | (c & 63)); return 2; }
    if (c < 0x10000) { t[0] = (char)(0xE0 | c >> 12); t[1] = (char)(0x80 | (c >> 6 & 63)); t[2] = (char)(0x80 | (c & 63)); return 3; }
    t[0] = (char)(0xF0 | c >> 18); t[1] = (char)(0x80 | (c >> 12 & 63)); t[2] = (char)(0x80 | (c >> 6 & 63)); t[3] = (char)(0x80 | (c & 63));
    return 4;
}
/* Codificações do CoreFoundation que chegam aqui: UTF-8 (0x08000100), ASCII (0x0600), Unicode/UTF-16 com BOM ou na ordem
 * da máquina (0x0100), UTF-16BE (0x10000100), UTF-16LE (0x14000100), UTF-32 (0x0c000100/BE 0x18000100/LE 0x1c000100),
 * ISO Latin 1 (0x0201), Windows-1252 (0x0500), MacRoman (0). Antes só 0x0100 era UTF-16 (e sem acentos): o Chrome cria os
 * textos da interface com UTF-16LE e eles viravam só a 1ª letra ("O\0K\0" lido como C string). */
id_ CFStringCreateWithBytes(void *alloc, const u8 *b, long n, u32 enc, u8 ext) {
    (void)ext;
    if (!b || n < 0) return 0;
    char *t = malloc((size_t)n * 2 + 4);                   /* UTF-16 -> no máximo 3 bytes a cada 2; Latin-1 -> 2 a cada 1 */
    if (!t) return 0;
    long o = 0;
    if (enc == ENC_UTF16 || enc == 0x10000100u || enc == 0x14000100u) {
        int be = enc == 0x10000100u;
        long i = 0;
        if (enc == ENC_UTF16 && n >= 2) {                   /* BOM; sem ela, a ordem da máquina (little-endian) */
            if (b[0] == 0xFE && b[1] == 0xFF) { be = 1; i = 2; } else if (b[0] == 0xFF && b[1] == 0xFE) i = 2;
        }
        for (; i + 1 < n; i += 2) {
            unsigned c = be ? (unsigned)b[i] << 8 | b[i + 1] : b[i] | (unsigned)b[i + 1] << 8;
            if (c >= 0xD800 && c < 0xDC00 && i + 3 < n) {   /* par substituto */
                unsigned d = be ? (unsigned)b[i + 2] << 8 | b[i + 3] : b[i + 2] | (unsigned)b[i + 3] << 8;
                if (d >= 0xDC00 && d < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00); i += 2; }
            }
            o += put_utf8(t + o, c);
        }
    } else if (enc == 0x0c000100u || enc == 0x18000100u || enc == 0x1c000100u) {
        int be = enc == 0x18000100u;
        for (long i = 0; i + 3 < n; i += 4) {
            unsigned c = be ? (unsigned)b[i] << 24 | (unsigned)b[i + 1] << 16 | (unsigned)b[i + 2] << 8 | b[i + 3]
                            : b[i] | (unsigned)b[i + 1] << 8 | (unsigned)b[i + 2] << 16 | (unsigned)b[i + 3] << 24;
            o += put_utf8(t + o, c < 0x110000 ? c : '?');
        }
    } else if (enc == 0x0201u || enc == 0x0500u || enc == 0) {
        for (long i = 0; i < n; i++) o += put_utf8(t + o, b[i]);   /* Latin-1 (aproximação para 1252/MacRoman acima de 0x7F) */
    } else { memcpy(t, b, (size_t)n); o = n; }
    t[o] = 0;
    id_ r = CFStringCreateWithFileSystemRepresentation(alloc, t);
    free(t);
    return r;
}

/* ---- sysctl: informações do sistema. O mOS se apresenta como Darwin 23 (macOS 14) para apps que
 * exigem uma versão mínima; os números de CPU vêm do kernel. */
extern int cx_errno;
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
/* statvfs/statfs: o volume do sistema (FAT32); números aproximados, suficientes para "há espaço" */
typedef struct { u64 bsize, frsize; u32 blocks, bfree, bavail, files, ffree, favail; u64 fsid, flag, namemax; } cx_statvfs_t;
int statvfs(const char *path, void *out) {
    (void)path;
    if (!out) return -1;
    memset(out, 0, sizeof(cx_statvfs_t));
    cx_statvfs_t *s = out;
    s->bsize = s->frsize = 4096;
    s->blocks = 131072; s->bfree = s->bavail = 65536;        /* 512 MiB, metade livre */
    s->files = s->ffree = s->favail = 65536;
    s->namemax = 255;
    return 0;
}
int fstatvfs(int fd, void *out) { (void)fd; return statvfs("/", out); }
/* resolver: sem /etc/resolv.conf no mOS; falha documentada (o Chromium usa o resolvedor interno) */
int res_9_ninit(void *state) { (void)state; return -1; }
void res_9_ndestroy(void *state) { (void)state; }
void res_9_nclose(void *state) { (void)state; }

/* confstr: pastas do usuário do Darwin (o Chromium monta o sandbox dos processos filhos com elas) */
int mkdir(const char *p, unsigned mode);
size_t strlcpy(char *d, const char *s, size_t n);
size_t confstr(int name, char *buf, size_t len) {
    const char *v;
    switch (name) {
    case 65536: v = "/Users/Visitante/"; break;                       /* _CS_DARWIN_USER_DIR */
    case 65537: v = "/Users/Visitante/tmp/"; break;                   /* _CS_DARWIN_USER_TEMP_DIR */
    case 65538: v = "/Users/Visitante/Library/Caches/"; break;        /* _CS_DARWIN_USER_CACHE_DIR */
    case 1: v = "/usr/bin:/bin:/Sistema/Programas"; break;            /* _CS_PATH */
    default: cx_errno = CX_EINVAL; return 0;
    }
    if (name == 65537) mkdir("/Users/Visitante/tmp", 0700);
    if (name == 65538) { mkdir("/Users/Visitante/Library", 0700); mkdir("/Users/Visitante/Library/Caches", 0700); }
    size_t n = strlen(v) + 1;
    if (buf && len) strlcpy(buf, v, len);
    return n;
}

/* ---- arquivos temporários: troca os X finais por letras aleatórias até achar um nome livre */
int open(const char *path, int flags, ...);
int access(const char *p, int m);
void arc4random_buf(void *b, size_t n);
static int fill_template(char *t, int suffix) {
    size_t n = strlen(t);
    if ((size_t)suffix > n) return -1;
    size_t end = n - (size_t)suffix, x = 0;
    while (x < end && t[end - 1 - x] == 'X') x++;
    if (x < 6) { cx_errno = CX_EINVAL; return -1; }
    static const char al[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    u8 r[32];
    arc4random_buf(r, sizeof r);
    for (size_t i = 0; i < x; i++) t[end - x + i] = al[r[i % 32] % 62];
    return 0;
}
int mkstemps(char *t, int suffix) {
    for (int tries = 0; tries < 100; tries++) {
        if (fill_template(t, suffix) < 0) return -1;
        if (access(t, 0) == 0) continue;
        return open(t, 0x0202 | 0x0800, 0600);               /* O_RDWR | O_CREAT | O_EXCL (macOS) */
    }
    cx_errno = CX_EEXIST;
    return -1;
}
int mkstemp(char *t) { return mkstemps(t, 0); }
int mkostemp(char *t, int flags) { (void)flags; return mkstemps(t, 0); }
char *mktemp(char *t) { return fill_template(t, 0) < 0 ? 0 : t; }
char *mkdtemp(char *t) {
    for (int tries = 0; tries < 100; tries++) {
        if (fill_template(t, 0) < 0) return 0;
        if (access(t, 0) == 0) continue;
        return mkdir(t, 0700) == 0 ? t : 0;
    }
    return 0;
}

/* ---- poll: escrita sempre pronta; leitura pela consulta ao kernel (cx_fd_readable, kqueue.c).
 * Espera em fatias de 1 ms até o prazo (timeout < 0 = sem prazo). */
typedef struct { int fd; short events, revents; } cx_pollfd_t;
int cx_fd_readable(int fd, long *avail, int *eof);
int cx_fd_writable(int fd);                                    /* kqueue.c: sockets em connect não estão prontos */
int cx_kqueue_peek(int fd);
int poll(cx_pollfd_t *fds, unsigned n, int timeout) {
    u64 end = (u64)SYS0(SYS_TIME) + (u64)(timeout < 0 ? 0 : timeout);
    for (;;) {
        int ready = 0;
        for (unsigned i = 0; i < n; i++) {
            fds[i].revents = (short)((fds[i].events & 0x4) && cx_fd_writable(fds[i].fd) ? 0x4 : 0);   /* POLLOUT */
            long avail; int eof;
            int kq = cx_kqueue_peek(fds[i].fd);               /* fd de kqueue: legível com evento pronto */
            if (kq >= 0) { if ((fds[i].events & 0x1) && kq) fds[i].revents |= 0x1; }
            else if ((fds[i].events & 0x1) && cx_fd_readable(fds[i].fd, &avail, &eof)) fds[i].revents |= eof && !avail ? 0x11 : 0x1;  /* POLLIN (+POLLHUP) */
            if (fds[i].revents) ready++;
        }
        if (ready || timeout == 0 || (timeout > 0 && (u64)SYS0(SYS_TIME) >= end)) return ready;
        /* um sinal com tratador interrompe o cochilo (o kernel devolve -EINTR): poll/select voltam com EINTR, como no
         * macOS. O readline do bash espera em pselect e só trata ^C e SIGWINCH quando ele volta assim */
        if (SYS1(SYS_SLEEP, 1) == -4) { cx_errno = 4; return -1; }
    }
}

/* ---- select sobre o poll acima (fd_set do macOS: 1024 bits em int32; timeval {long sec; int usec}) */
typedef struct { long sec; int usec; } cx_timeval_t;
#define CX_FDSET_MAX 1024
static int fdset_has(const void *s, int fd) { return s && (((const u32 *)s)[fd / 32] >> (fd % 32)) & 1; }
static void fdset_put(void *s, int fd) { ((u32 *)s)[fd / 32] |= 1u << (fd % 32); }
int select(int nfds, void *rd, void *wr, void *ex, cx_timeval_t *tv) {
    cx_pollfd_t p[64];               /* até 64 descritores marcados (o polling do Node usa um) */
    unsigned n = 0;
    if (nfds > CX_FDSET_MAX) nfds = CX_FDSET_MAX;
    for (int fd = 0; fd < nfds && n < 64; fd++) {
        short ev = (short)((fdset_has(rd, fd) ? 0x1 : 0) | (fdset_has(wr, fd) ? 0x4 : 0));
        if (ev) { p[n].fd = fd; p[n].events = ev; p[n].revents = 0; n++; }
    }
    int timeout = tv ? (int)(tv->sec * 1000 + tv->usec / 1000) : -1;
    int r = poll(p, n, timeout);
    unsigned words = (unsigned)(nfds + 31) / 32;
    for (unsigned w = 0; w < words; w++) { if (rd) ((u32 *)rd)[w] = 0; if (wr) ((u32 *)wr)[w] = 0; if (ex) ((u32 *)ex)[w] = 0; }
    int count = 0;
    for (unsigned i = 0; r > 0 && i < n; i++) {
        if (rd && (p[i].revents & 0x11)) { fdset_put(rd, p[i].fd); count++; }
        if (wr && (p[i].revents & 0x4)) { fdset_put(wr, p[i].fd); count++; }
    }
    return r < 0 ? r : count;
}
int select$1050(int nfds, void *rd, void *wr, void *ex, cx_timeval_t *tv) { return select(nfds, rd, wr, ex, tv); }
int select$DARWIN_EXTSN(int nfds, void *rd, void *wr, void *ex, cx_timeval_t *tv) { return select(nfds, rd, wr, ex, tv); }
/* FD_SET do SDK novo pergunta se o fd cabe no fd_set */
int __darwin_check_fd_set_overflow(int fd, const void *set, int unlimited) { (void)set; return unlimited || (fd >= 0 && fd < CX_FDSET_MAX); }

/* file actions de posix_spawn ficam em extra.c (dup2, close, open, chdir) */
int posix_spawnattr_setbinpref_np(void *a, size_t n, int *pref, size_t *ocount) { (void)a; (void)pref; if (ocount) *ocount = n; return 0; }
/* porta especial do PRÓXIMO filho de posix_spawn (bootstrap, which = 4): o mach.c/kernel usam no
 * machipc_spawn. Movido para o kernel (MACH_OP_SPAWN_SPECIAL); o objeto `a` do atributo não é usado. */
int posix_spawnattr_setspecialport_np(void *a, u32 port, int which) {
    (void)a;
    u64 args[8] = {(u64)which, port, 0, 0, 0, 0, 0, 0};
    cx_syscall(SYS_MACH_PORT, (long)MACH_OP_SPAWN_SPECIAL, (long)args, 0, 0, 0);
    return 0;
}

/* bootstrap (launchd), task_get/set_special_port e bootstrap_port: agora em userland/libsystem/mach.c,
 * usando as syscalls de portas Mach do kernel (registro de serviços global entre processos). */

/* ---- sandbox e Keychain: o mOS não tem; "permitido" e "sem keychain" */
/* Sandbox (Seatbelt): o mOS não restringe nada, mas aceita os perfis e lembra que um foi aplicado.
 * sandbox_check(pid, NULL, 0) ("o processo está em sandbox?") responde sim depois de um perfil aplicado,
 * ou num helper do Chromium (--type=...) sem --no-sandbox: no macOS ele recebe o perfil do processo
 * principal na partida (Seatbelt exec) e o renderer faz CHECK disso. Operações específicas: permitidas. */
char *strdup(const char *s);
static volatile int sandbox_applied;
static int helper_expects_sandbox(void) {
    extern char **NXArgv;
    int helper = 0, nosb = 0;
    for (int i = 1; NXArgv && NXArgv[i]; i++) {
        if (!strncmp(NXArgv[i], "--type=", 7)) helper = 1;
        if (!strcmp(NXArgv[i], "--no-sandbox")) nosb = 1;
    }
    return helper && !nosb;
}
int sandbox_check(int pid, const char *op, int type, ...) {
    (void)type;
    if (op) return 0;                                          /* operação permitida */
    if (pid != (int)SYS0(SYS_GETPID)) return 0;
    return sandbox_applied || helper_expects_sandbox();
}
int sandbox_init(const char *profile, u64 flags, char **err) { (void)profile; (void)flags; if (err) *err = 0; sandbox_applied = 1; return 0; }
int sandbox_init_with_parameters(const char *profile, u64 flags, const char *const params[], char **err) {
    (void)profile; (void)flags; (void)params; if (err) *err = 0; sandbox_applied = 1; return 0;
}
void sandbox_free_error(char *err) { (void)err; }
/* API privada usada pelo sandbox::Seatbelt do Chromium. O perfil compilado NÃO é opaco: o Chromium lê os campos de
 * struct sandbox_profile {builtin, data, size} (sandbox/mac/seatbelt.cc) e copia `size` bytes de `data` para mandar ao
 * helper. Antes era uma struct de 8 bytes: o Chrome lia o "size" 16 bytes adiante, na memória estática vizinha (a classe
 * OS_dispatch_queue_main), e pedia uma alocação de ~70 TiB — o PartitionAlloc derrubava o processo por falta de memória.
 * O mOS não aplica sandbox (sandbox_apply só marca): o bytecode é um marcador pequeno. */
typedef struct { const char *builtin; const u8 *data; size_t size; } sb_profile_t;
static const u8 sb_bytecode[8] = {'m', 'O', 'S', '-', 's', 'b', 0, 0};
static sb_profile_t sb_profile = {0, sb_bytecode, sizeof sb_bytecode};
typedef struct { u64 magic, pad[7]; } sb_params_t;          /* parâmetros: opacos para quem chama */
static sb_params_t sb_params = {0x5350, {0}};
void *sandbox_create_params(void) { return &sb_params; }
int sandbox_set_param(void *params, const char *key, const char *value) { (void)params; (void)key; (void)value; return 0; }
void sandbox_free_params(void *params) { (void)params; }
void *sandbox_compile_string(const char *profile, void *params, char **err) { (void)profile; (void)params; if (err) *err = 0; return &sb_profile; }
void sandbox_free_profile(void *profile) { (void)profile; }
int sandbox_apply(void *profile) { (void)profile; sandbox_applied = 1; return 0; }
char *sandbox_extension_issue_file(const char *ext_class, const char *path, u32 flags) {
    (void)ext_class; (void)flags;
    return strdup(path ? path : "");                           /* "token" da extensão: o próprio caminho */
}
long sandbox_extension_consume(const char *token) { (void)token; return 1; }
int sandbox_extension_release(long handle) { (void)handle; return 0; }

/* csops: o binário não é assinado (sem CS_VALID, igual ao SecCode). ADHOC + GET_TASK_ALLOW +
 * DEBUGGED deixam o V8 e o Crashpad seguirem sem tratar o processo como inválido. */
int csops(int pid, unsigned int ops, void *useraddr, size_t usersize) {
    (void)pid;
    if (ops == 0) {                                                                 /* CS_OPS_STATUS */
        if (!useraddr || usersize < 4) { cx_errno = CX_EINVAL; return -1; }
        *(u32 *)useraddr = 0x2u | 0x4u | 0x10000000u | 0x40000000u;                 /* ADHOC | GET_TASK_ALLOW | DEBUGGED | DEV_CODE */
        return 0;
    }
    if (ops == 5 && useraddr && usersize >= 20) { memset(useraddr, 0, 20); return 0; } /* CS_OPS_CDHASH */
    /* CS_OPS_TEAMID: sem Team ID (o XNU responde ENOENT); CS_OPS_VALIDATION_CATEGORY: CS_VALIDATION_CATEGORY_NONE.
     * O ProcessRequirement do Chromium (rendezvous Mach com os helpers) trata isso como build sem assinatura e
     * pula a validação; com ENOSYS o requisito falhava e os renderers não recebiam as portas ("about:blank
     * demorou demais" e um renderer novo atrás do outro). */
    if (ops == 14) { cx_errno = CX_ENOENT; return -1; }
    if (ops == 17 && useraddr && usersize >= 4) { *(u32 *)useraddr = 10; return 0; }
    if (ops == 4 && useraddr && usersize > 1) {                                     /* CS_OPS_PIDPATH */
        extern char **NXArgv;
        const char *p = NXArgv && NXArgv[0] ? NXArgv[0] : "/app";
        size_t n = strlen(p);
        if (n + 1 > usersize) { cx_errno = CX_ENOMEM; return -1; }
        memcpy(useraddr, p, n + 1);
        return 0;
    }
    cx_errno = CX_ENOSYS;
    return -1;
}
int csops_audittoken(int pid, unsigned int ops, void *useraddr, size_t usersize, void *token) {
    (void)token;
    return csops(pid, ops, useraddr, usersize);
}
int SecKeychainAddCallback(void *cb, u32 mask, void *ctx) { (void)cb; (void)mask; (void)ctx; return 0; }
int SecKeychainRemoveCallback(void *cb) { (void)cb; return 0; }

/* ---- NSSearchPathForDirectoriesInDomains: pastas do usuário do mOS */
id_ NSSearchPathForDirectoriesInDomains(unsigned long dir, unsigned long mask, u8 expand) {
    (void)expand;
    /* NSApplicationDirectory (1), NSAdminApplicationDirectory (4), NSAllApplicationsDirectory (100): os apps do mOS
     * ficam em /Aplicativos (domínio local, máscara 2) e ~/Applications (usuário, 1). O Electron pergunta aqui no
     * app.isInApplicationsFolder(); com ~/Library, o Grok Bot abria o "Mover para a pasta Aplicativos?" e parava */
    if (dir == 1 || dir == 4 || dir == 100) {
        id_ a = MSG0_(id_, cx_class_named("NSMutableArray"), "array");
        id_ str = cx_class_named("NSString");
        if (mask & 1) MSG1_(void, a, "addObject:", MSG1_(id_, str, "stringWithUTF8String:", dir == 4 ? "/Users/Visitante/Applications/Utilities" : "/Users/Visitante/Applications"));
        if (mask & 2) MSG1_(void, a, "addObject:", MSG1_(id_, str, "stringWithUTF8String:", dir == 4 ? "/Aplicativos/Utilitários" : "/Aplicativos"));
        if (dir == 100 && (mask & 2)) MSG1_(void, a, "addObject:", MSG1_(id_, str, "stringWithUTF8String:", "/Aplicativos/Utilitários"));
        return a;
    }
    const char *p;
    switch (dir) {
    case 5: p = "/Users/Visitante/Library"; break;                        /* NSLibraryDirectory */
    case 9: p = "/Users/Visitante/Documentos"; break;                     /* NSDocumentDirectory */
    case 12: p = "/Users/Visitante/Mesa"; break;                          /* NSDesktopDirectory */
    case 13: p = "/Users/Visitante/Library/Caches"; break;                /* NSCachesDirectory */
    case 14: p = "/Users/Visitante/Library/Application Support"; break;   /* NSApplicationSupportDirectory */
    case 15: p = "/Users/Visitante/Downloads"; break;                     /* NSDownloadsDirectory */
    default: p = "/Users/Visitante/Library"; break;
    }
    if (dir == 14 || dir == 13) { mkdir("/Users/Visitante/Library", 0700); mkdir(p, 0700); }
    id_ a = MSG0_(id_, cx_class_named("NSMutableArray"), "array");
    MSG1_(void, a, "addObject:", MSG1_(id_, cx_class_named("NSString"), "stringWithUTF8String:", p));
    return a;
}

/* variantes _l (locale): o mOS só tem o locale C/UTF-8 */
int vsnprintf(char *b, size_t n, const char *fmt, __builtin_va_list ap);
int snprintf_l(char *b, size_t n, void *loc, const char *fmt, ...) {
    (void)loc;
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    int r = vsnprintf(b, n, fmt, ap);
    __builtin_va_end(ap);
    return r;
}
int pthread_getname_np(void *t, char *buf, size_t len) { (void)t; if (buf && len) buf[0] = 0; return 0; }
typedef struct { const char *name; int kind; u64 num; const char *str; } sysctl_ent_t;   /* kind: 4/8 bytes, 0 texto */
static int sysctl_put(const sysctl_ent_t *e, void *old, size_t *oldlen) {
    u64 n = e->kind ? (u64)e->kind : strlen(e->str) + 1;
    u64 v = e->num;
    if (!strcmp(e->name, "hw.ncpu") || !strcmp(e->name, "hw.activecpu") || !strcmp(e->name, "hw.logicalcpu") ||
        !strcmp(e->name, "hw.physicalcpu") || !strcmp(e->name, "hw.logicalcpu_max") || !strcmp(e->name, "hw.physicalcpu_max") ||
        !strcmp(e->name, "machdep.cpu.core_count") || !strcmp(e->name, "machdep.cpu.thread_count") ||
        !strncmp(e->name, "hw.perflevel0.", 14) && e->kind) {
        long c = SYS0(SYS_CPUS);
        v = c > 0 ? (u64)c : 1;
    }
    if (!oldlen) return 0;
    if (!old) { *oldlen = n; return 0; }
    if (*oldlen < n) { cx_errno = CX_ENOMEM; return -1; }
    if (e->kind == 4) *(u32 *)old = (u32)v;
    else if (e->kind == 8) *(u64 *)old = v;
    else memcpy(old, e->str, n);
    *oldlen = n;
    return 0;
}
static const sysctl_ent_t sysctl_tab[] = {
    {"hw.ncpu", 4, 1, 0}, {"hw.activecpu", 4, 1, 0}, {"hw.logicalcpu", 4, 1, 0}, {"hw.physicalcpu", 4, 1, 0},
    {"hw.logicalcpu_max", 4, 1, 0}, {"hw.physicalcpu_max", 4, 1, 0},
    {"machdep.cpu.core_count", 4, 1, 0}, {"machdep.cpu.thread_count", 4, 1, 0},
    {"hw.memsize", 8, 2ull << 30, 0}, {"hw.pagesize", 8, 4096, 0}, {"hw.pagesize32", 4, 4096, 0},
    {"hw.cachelinesize", 8, 64, 0}, {"hw.l1dcachesize", 8, 32768, 0}, {"hw.l1icachesize", 8, 32768, 0},
    {"hw.l2cachesize", 8, 262144, 0}, {"hw.l3cachesize", 8, 8u << 20, 0},
    {"hw.cpufrequency", 8, 2000000000ull, 0}, {"hw.cpufrequency_max", 8, 2000000000ull, 0}, {"hw.busfrequency", 8, 100000000ull, 0},
    {"hw.byteorder", 4, 1234, 0}, {"hw.cputype", 4, 0x01000007, 0}, {"hw.cpusubtype", 4, 3, 0}, {"hw.cpu64bit_capable", 4, 1, 0},
    {"hw.optional.x86_64", 4, 1, 0}, {"hw.optional.sse2", 4, 1, 0}, {"hw.optional.sse3", 4, 1, 0}, {"hw.optional.arm64", 4, 0, 0},
    {"sysctl.proc_translated", 4, 0, 0}, {"sysctl.proc_native", 4, 1, 0},
    {"hw.model", 0, 0, "mOS1,1"}, {"hw.machine", 0, 0, "x86_64"}, {"hw.targettype", 0, 0, "mOS"},
    {"machdep.cpu.brand_string", 0, 0, "mOS Virtual CPU"}, {"machdep.cpu.vendor", 0, 0, "GenuineIntel"},
    {"machdep.cpu.family", 4, 6, 0}, {"machdep.cpu.model", 4, 85, 0}, {"machdep.cpu.stepping", 4, 4, 0},
    {"kern.ostype", 0, 0, "Darwin"}, {"kern.osrelease", 0, 0, "23.0.0"}, {"kern.osversion", 0, 0, "23A344"},
    {"kern.osproductversion", 0, 0, "14.0"}, {"kern.osproductversioncompat", 0, 0, "14.0"},
    {"kern.iossupportversion", 0, 0, "17.0"},   /* Mac Catalyst: versão do iOSSupport do macOS 14 (UIKit em /System/iOSSupport) */
    {"kern.version", 0, 0, "Darwin Kernel Version 23.0.0: mOS Nucleo; RELEASE_X86_64"},
    {"kern.hostname", 0, 0, "mOS"}, {"kern.osrevision", 4, 199506, 0},
    {"kern.maxfilesperproc", 4, 32, 0}, {"kern.maxfiles", 4, 256, 0}, {"kern.argmax", 4, 262144, 0},
    {"kern.maxproc", 4, 64, 0}, {"kern.maxprocperuid", 4, 64, 0}, {"kern.securelevel", 4, 0, 0},
    {"hw.nperflevels", 4, 1, 0}, {"hw.perflevel0.logicalcpu", 4, 1, 0}, {"hw.perflevel0.physicalcpu", 4, 1, 0},
    {"hw.perflevel0.logicalcpu_max", 4, 1, 0}, {"hw.perflevel0.physicalcpu_max", 4, 1, 0}, {"hw.perflevel0.name", 0, 0, "Performance"},
    {"kern.memorystatus_vm_pressure_level", 4, 1, 0}, {"kern.memorystatus_level", 4, 100, 0},
    {"kern.hv_support", 4, 0, 0}, {"kern.tcsm_available", 4, 0, 0}, {"kern.tcsm_enable", 4, 0, 0}, {"kern.secure_kernel", 4, 0, 0}, {"security.mac.lockdown_mode_state", 4, 0, 0},
    /* pedidos pelo Google Chrome (base/mac, base/cpu, net): CPU x86_64 com SSE2/SSE3, sem AVX-512 */
    {"kern.hv_vmm_present", 4, 0, 0}, {"kern.nx", 4, 1, 0}, {"hw.packages", 4, 1, 0}, {"hw.optional.avx512f", 4, 0, 0},
    {"hw.cacheconfig", 8, 1, 0},                                   /* 1 CPU compartilha cada nível de cache */
    {"machdep.cpu.signature", 4, 0x50654, 0},                      /* família 6, modelo 85, stepping 4 (acima) */
    {"machdep.cpu.feature_bits", 8, 0x178bfbffull | (0x00000201ull << 32), 0},   /* EDX: FPU..SSE2; ECX: SSE3, SSSE3 */
    {"machdep.cpu.extfeature_bits", 8, 0x20100800ull, 0},          /* SYSCALL, XD (NX), EM64T */
    {"net.inet.ip.portrange.first", 4, 49152, 0}, {"net.inet.ip.portrange.last", 4, 65535, 0},
};
int sysctlbyname(const char *name, void *old, size_t *oldlen, const void *newp, size_t newlen) {
    (void)newp; (void)newlen;
    if (!name) { cx_errno = CX_EINVAL; return -1; }
#if defined(__aarch64__)   /* processo arm64: as chaves do Apple Silicon (darwin_arm64.c) antes das do x86 */
    { extern int cx_sysctl_arm64(const char *, void *, unsigned long *);
      int r = cx_sysctl_arm64(name, old, (unsigned long *)oldlen);
      if (r <= 0) { if (r < 0) { cx_errno = CX_ENOMEM; return -1; } return 0; } }
#endif
    for (u32 i = 0; i < sizeof sysctl_tab / sizeof sysctl_tab[0]; i++)
        if (!strcmp(sysctl_tab[i].name, name)) return sysctl_put(&sysctl_tab[i], old, oldlen);
    cx_fdprintf(2, "[sysctl] chave desconhecida no mOS: %s\n", name);
    cx_errno = CX_ENOENT;
    return -1;
}
/* sysctl por MIB: os mais comuns (CTL_KERN=1, CTL_HW=6) mapeados para os nomes */
int sysctl(int *mib, unsigned n, void *old, size_t *oldlen, void *newp, size_t newlen) {
    if (!mib || n < 2) { cx_errno = CX_EINVAL; return -1; }
    const char *name = 0;
    if (mib[0] == 1) switch (mib[1]) {
        case 1: name = "kern.ostype"; break;        case 2: name = "kern.osrelease"; break;
        case 3: name = "kern.osrevision"; break;    case 4: name = "kern.version"; break;
        case 6: name = "kern.maxproc"; break;       case 7: name = "kern.maxfiles"; break;
        case 8: name = "kern.argmax"; break;        case 10: name = "kern.hostname"; break;
        case 65: name = "kern.osversion"; break;
        case 14: {                                  /* KERN_PROC: pty.c (KERN_PROC_PID com nome e grupo; sem depurador) */
            extern int cx_kinfo_proc(int *mib, unsigned n, void *old, size_t *oldlen);
            return cx_kinfo_proc(mib, n, old, oldlen);
        }
    }
    else if (mib[0] == 6) switch (mib[1]) {
        case 1: name = "hw.machine"; break;         case 2: name = "hw.model"; break;
        case 3: name = "hw.ncpu"; break;            case 4: name = "hw.byteorder"; break;
        case 7: name = "hw.pagesize32"; break;      case 24: name = "hw.memsize"; break;
        case 25: name = "hw.activecpu"; break;
    }
    if (!name) { cx_errno = CX_ENOENT; return -1; }
    return sysctlbyname(name, old, oldlen, newp, newlen);
}
int sysctlnametomib(const char *name, int *mib, size_t *n) {
    static const struct { const char *s; int a, b; } t[] = {
        {"kern.ostype", 1, 1}, {"kern.osrelease", 1, 2}, {"kern.version", 1, 4}, {"kern.osversion", 1, 65},
        {"hw.ncpu", 6, 3}, {"hw.memsize", 6, 24}, {"hw.pagesize", 6, 7}, {"hw.activecpu", 6, 25}, {"hw.model", 6, 2},
    };
    for (u32 i = 0; i < sizeof t / sizeof t[0]; i++)
        if (!strcmp(t[i].s, name)) {
            if (!n || *n < 2) { cx_errno = CX_ENOMEM; return -1; }
            mib[0] = t[i].a; mib[1] = t[i].b; *n = 2;
            return 0;
        }
    cx_errno = CX_ENOENT;
    return -1;
}

/* ---- atributos de pthread que faltavam (o pthread_create do mOS usa pilhas de 512 KiB) */
int pthread_attr_getstacksize(const void *a, size_t *s) { (void)a; if (s) *s = 512 << 10; return 0; }
int pthread_attr_getstack(const void *a, void **addr, size_t *s) { (void)a; if (addr) *addr = 0; if (s) *s = 512 << 10; return 0; }
int pthread_attr_setstack(void *a, void *addr, size_t s) { (void)a; (void)addr; (void)s; return 0; }
int pthread_attr_setdetachstate(void *a, int st) { (void)a; (void)st; return 0; }
int pthread_attr_getdetachstate(const void *a, int *st) { (void)a; if (st) *st = 1; return 0; }   /* JOINABLE */
int pthread_attr_setguardsize(void *a, size_t s) { (void)a; (void)s; return 0; }
int pthread_attr_getguardsize(const void *a, size_t *s) { (void)a; if (s) *s = 4096; return 0; }
int pthread_attr_setscope(void *a, int s) { (void)a; (void)s; return 0; }
int pthread_attr_setschedpolicy(void *a, int p) { (void)a; (void)p; return 0; }
int pthread_attr_setschedparam(void *a, const void *p) { (void)a; (void)p; return 0; }
int pthread_attr_getschedparam(const void *a, int *p) { (void)a; if (p) *p = 31; return 0; }
int pthread_attr_setinheritsched(void *a, int i) { (void)a; (void)i; return 0; }
int pthread_attr_set_qos_class_np(void *a, unsigned q, int r) { (void)a; (void)q; (void)r; return 0; }
int pthread_set_qos_class_self_np(unsigned q, int r) { (void)q; (void)r; return 0; }
unsigned qos_class_self(void) { return 0x21; }                    /* QOS_CLASS_USER_INTERACTIVE */
unsigned qos_class_main(void) { return 0x21; }
int pthread_setschedparam(void *t, int pol, const void *p) { (void)t; (void)pol; (void)p; return 0; }
int pthread_getschedparam(void *t, int *pol, int *p) { (void)t; if (pol) *pol = 1; if (p) *p = 31; return 0; }
int sched_get_priority_min(int p) { (void)p; return 15; }
int sched_get_priority_max(int p) { (void)p; return 47; }

/* ---- limites de recursos */
typedef struct { u64 cur, max; } cx_rlimit_t;
#define RLIM_INFINITY 0x7FFFFFFFFFFFFFFFull
int getrlimit(int res, cx_rlimit_t *r) {
    if (!r) { cx_errno = CX_EFAULT; return -1; }
    switch (res) {
    case 3: r->cur = r->max = 8u << 20; break;          /* RLIMIT_STACK */
    case 8: r->cur = r->max = 256; break;               /* RLIMIT_NOFILE: PROC_MAX_FDS do kernel (proc.h) */
    case 4: r->cur = r->max = 0; break;                 /* RLIMIT_CORE */
    default: r->cur = r->max = RLIM_INFINITY; break;
    }
    return 0;
}
int setrlimit(int res, const cx_rlimit_t *r) { (void)res; (void)r; return 0; }

/* MB_CUR_MAX no macOS é ___mb_cur_max() */
int __mb_cur_max = 4;
int ___mb_cur_max(void) { return 4; }
int ___mb_cur_max_l(void *loc) { (void)loc; return 4; }
