/*
 * mOS libSystem — codificações do NSString e do CFString sobre fdn_encoding_core.c (conversores da ICU).
 * Antes: dataUsingEncoding:/initWithBytes:/CFStringCreateWithBytes tratavam quase tudo como UTF-8 ou Latin-1
 * (MacRoman e Windows-1252 acima de 0x7F saíam errados, Shift-JIS/EUC/ISO-2022 não existiam) e não havia
 * canBeConvertedToEncoding:, getBytes:…, stringEncodingForData:…, usedEncoding nem as funções de lista/nome.
 *  - NSString: init com bytes/dados/C string/arquivo/URL (nil se os bytes forem inválidos), dataUsingEncoding:
 *    com e sem perda ('?' no lugar), cStringUsingEncoding:, getCString:maxLength:encoding:, tamanhos,
 *    fastest/smallestEncoding, getBytes:maxLength:usedLength:encoding:options:range:remainingRange:,
 *    writeToFile/URL:atomically:encoding:error: (erro 517 se não couber na codificação), as C strings antigas
 *    (cString, lossyCString…), +defaultCStringEncoding, +availableStringEncodings,
 *    +localizedNameOfStringEncoding:, +stringEncodingForData:encodingOptions:convertedString:usedLossyConversion:,
 *    escapes de porcentagem com codificação, propertyList e propertyListFromStringsFileFormat;
 *  - usedEncoding: BOM, o atributo com.apple.TextEncoding ("utf-8;134217984") e UTF-8 válido;
 *  - CFString: CreateWithBytes/CString/PascalString, GetBytes/CString/CStringPtr/PascalString,
 *    GetMaximumSizeForEncoding, IsEncodingAvailable, GetListOfAvailableEncodings, GetNameOfEncoding,
 *    GetSmallestEncoding, Windows codepage e GetMostCompatibleMacStringEncoding.
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_encoding_core.h"

id objc_autorelease(id o);
void objc_release(id o);
id cx_str_n(const char *s, u64 n);
id fdn_parse_strings(const u8 *b, u64 n);
u64 CFStringConvertEncodingToNSStringEncoding(u32 cf);
u32 CFStringConvertNSStringEncodingToEncoding(u64 ns);
id CFStringConvertEncodingToIANACharSetName(u32 cf);
long getxattr(const char *path, const char *name, void *value, u64 size, u32 pos, int opts);

extern cx_class_t NSString_c __asm__("OBJC_CLASS_$_NSString");
typedef struct { u64 loc, len; } rng_t;

static id cls(const char *n) { return (id)cx_class_named(n); }
static u32 cf_of(u64 ns) { return CFStringConvertNSStringEncodingToEncoding(ns); }

static UChar *U16(id s, int32_t *n) {
    const char *p = s ? MSG0(const char *, s, "UTF8String") : 0;
    return fdn_u16_dup(p ? p : "", n);
}
/* NSString nova (+1) a partir de UTF-16 */
static id new_str(const UChar *u, int32_t n) {
    char *c = fdn_utf8_dup(u, n);
    id r = cx_str_n(c, strlen(c));
    free(c);
    return r;
}
/* troca o conteúdo de self (init*) pelo texto; nil (e solta self) se não decodificou */
static id adopt(id self, UChar *u, int32_t n) {
    if (!u) { objc_release(self); return 0; }
    char *c = fdn_utf8_dup(u, n);
    free(u);
    if (cx_is_str(self)) {
        nsstr_t *o = (nsstr_t *)self;
        free(o->s);
        o->s = c;
        o->n = strlen(c);
        return self;
    }
    id r = MSG(id, self, "initWithUTF8String:", c);
    free(c);
    return r;
}
static id mk_error(u64 code, id path) {
    id info = 0;
    if (path) info = MSG(id, cls("NSDictionary"), "dictionaryWithObject:forKey:", path, cx_str("NSFilePath"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), code, info);
}

/* ---------------------------------------------------------------- criação */
static id i_bytes(id self, SEL c, const u8 *b, u64 n, u64 enc) {
    (void)c;
    int32_t m;
    return adopt(self, enc_decode(b, (int32_t)n, cf_of(enc), &m), m);
}
static id i_bytes_nocopy(id self, SEL c, void *b, u64 n, u64 enc, BOOL freeit) {
    id r = i_bytes(self, c, b, n, enc);
    if (freeit) free(b);
    return r;
}
static id i_data(id self, SEL c, id data, u64 enc) {
    if (!data) { objc_release(self); return 0; }
    return i_bytes(self, c, MSG0(const u8 *, data, "bytes"), MSG0(u64, data, "length"), enc);
}
static id i_cstring(id self, SEL c, const char *s, u64 enc) {
    if (!s) { objc_release(self); return 0; }
    u32 cf = cf_of(enc);
    u64 n;
    if (cf == ENC_UTF16 || cf == ENC_UTF16BE || cf == ENC_UTF16LE) { const u16 *w = (const u16 *)s; n = 0; while (w[n]) n++; n *= 2; }
    else if (cf == ENC_UTF32 || cf == ENC_UTF32BE || cf == ENC_UTF32LE) { const u32 *w = (const u32 *)s; n = 0; while (w[n]) n++; n *= 4; }
    else n = strlen(s);
    return i_bytes(self, c, (const u8 *)s, n, enc);
}
static id k_cstring(id k, SEL c, const char *s, u64 enc) {
    id o = i_cstring(cx_alloc_instance((cx_class_t *)k), c, s, enc);
    return o ? objc_autorelease(o) : 0;
}

/* lê o arquivo; codificação dada ou adivinhada (BOM, com.apple.TextEncoding, UTF-8) */
static id read_text(id self, id path, id data, u64 enc, int detect, u64 *used, id *err) {
    if (err) *err = 0;
    if (!data) {
        if (err) *err = mk_error(260, path);   /* NSFileReadNoSuchFileError */
        objc_release(self);
        return 0;
    }
    const u8 *b = MSG0(const u8 *, data, "bytes");
    int32_t n = (int32_t)MSG0(u64, data, "length"), m;
    u32 cf = cf_of(enc);
    if (detect) {
        u32 sug[1];
        int ns = 0;
        const char *p = path ? MSG0(const char *, path, "UTF8String") : 0;
        char x[128];
        long xl = p ? getxattr(p, "com.apple.TextEncoding", x, sizeof x - 1, 0, 0) : -1;
        if (xl > 0) {
            x[xl] = 0;
            const char *semi = strchr(x, ';');
            if (semi) { sug[0] = (u32)strtoul(semi + 1, 0, 10); ns = 1; }
        }
        cf = enc_detect(b, n, sug, ns, 0);
        if (cf == ENC_INVALID) {
            if (err) *err = mk_error(264, path);   /* NSFileReadUnknownStringEncodingError */
            objc_release(self);
            return 0;
        }
    }
    UChar *u = enc_decode(b, n, cf, &m);
    if (!u) {
        if (err) *err = mk_error(261, path);   /* NSFileReadInapplicableStringEncodingError */
        objc_release(self);
        return 0;
    }
    if (used) *used = CFStringConvertEncodingToNSStringEncoding(cf);
    return adopt(self, u, m);
}
static id file_data(id path) { return path ? MSG(id, cls("NSData"), "dataWithContentsOfFile:", path) : 0; }
static id url_data(id url) { return url ? MSG(id, cls("NSData"), "dataWithContentsOfURL:", url) : 0; }
static id url_path(id url) { return url && MSG0(BOOL, url, "isFileURL") ? MSG0(id, url, "path") : 0; }
static id i_file_enc(id self, SEL c, id p, u64 enc, id *err) { (void)c; return read_text(self, p, file_data(p), enc, 0, 0, err); }
static id i_file_used(id self, SEL c, id p, u64 *used, id *err) { (void)c; return read_text(self, p, file_data(p), 0, 1, used, err); }
static id i_url_enc(id self, SEL c, id u, u64 enc, id *err) { (void)c; return read_text(self, url_path(u), url_data(u), enc, 0, 0, err); }
static id i_url_used(id self, SEL c, id u, u64 *used, id *err) { (void)c; return read_text(self, url_path(u), url_data(u), 0, 1, used, err); }
#define KREAD(name, inner, T) \
    static id name(id k, SEL c, id a, T b, id *err) { id o = inner(cx_alloc_instance((cx_class_t *)k), c, a, b, err); return o ? objc_autorelease(o) : 0; }
KREAD(k_file_enc, i_file_enc, u64)
KREAD(k_file_used, i_file_used, u64 *)
KREAD(k_url_enc, i_url_enc, u64)
KREAD(k_url_used, i_url_used, u64 *)

/* ---------------------------------------------------------------- bytes */
/* bytes na codificação (malloc, com 4 zeros de folga); NULL se não converteu */
static u8 *encode(id self, u64 enc, int lossy, int bom, int32_t *len) {
    int32_t n, used = 0;
    UChar *u = U16(self, &n);
    u32 cf = cf_of(enc);
    if (!enc_available(cf)) { free(u); return 0; }
    int32_t cap = enc_max_bytes(cf, n) + 8;
    u8 *b = malloc((u64)cap + 4);
    int32_t k = enc_encode(u, n, cf, lossy ? '?' : 0, bom, b, cap, &used);
    free(u);
    if (k != n) { free(b); return 0; }
    memset(b + used, 0, 4);
    *len = used;
    return b;
}
static id s_data_lossy(id self, SEL c, u64 enc, BOOL lossy) {
    (void)c;
    int32_t n;
    u32 cf = cf_of(enc);
    /* NSUnicodeStringEncoding sai com BOM, como no macOS */
    u8 *b = encode(self, enc, lossy, cf == ENC_UTF16 || cf == ENC_UTF32, &n);
    if (!b) return 0;
    id d = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)b, (u64)n);
    free(b);
    return d;
}
static id s_data(id self, SEL c, u64 enc) { return s_data_lossy(self, c, enc, NO); }
static const char *s_cstr_enc(id self, SEL c, u64 enc) {
    (void)c;
    int32_t n;
    u8 *b = encode(self, enc, 0, 0, &n);
    if (!b) return 0;
    id d = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)b, (u64)n + 4);
    free(b);
    return MSG0(const char *, d, "bytes");
}
static BOOL s_get_cstr_enc(id self, SEL c, char *buf, u64 max, u64 enc) {
    (void)c;
    int32_t n;
    u8 *b = encode(self, enc, 0, 0, &n);
    if (!b || !buf || (u64)n + 1 > max) { free(b); return NO; }
    memcpy(buf, b, (u64)n);
    buf[n] = 0;
    free(b);
    return YES;
}
static u64 s_len_bytes(id self, SEL c, u64 enc) {
    (void)c;
    int32_t n, used = 0;
    UChar *u = U16(self, &n);
    int32_t k = enc_encode(u, n, cf_of(enc), 0, 0, 0, 0, &used);
    free(u);
    return k == n ? (u64)used : 0;
}
static u64 s_max_bytes(id self, SEL c, u64 enc) { (void)c; return (u64)enc_max_bytes(cf_of(enc), (int32_t)MSG0(u64, self, "length")); }
static BOOL s_can_convert(id self, SEL c, u64 enc) {
    (void)c;
    int32_t n;
    UChar *u = U16(self, &n);
    int r = enc_available(cf_of(enc)) && enc_lossless(u, n, cf_of(enc));
    free(u);
    return r ? YES : NO;
}
static int all_ascii(id self) {
    const char *p = MSG0(const char *, self, "UTF8String");
    for (; p && *p; p++) if ((u8)*p >= 0x80) return 0;
    return 1;
}
static u64 s_fastest(id self, SEL c) { (void)c; (void)self; return 4; }    /* o texto fica em UTF-8 aqui */
static u64 s_smallest(id self, SEL c) {
    if (all_ascii(self)) return 1;
    return s_can_convert(self, c, 30) ? 30 : 10;
}
/* NSStringEncodingConversionAllowLossy = 1, ExternalRepresentation = 2 (BOM) */
static BOOL s_get_bytes(id self, SEL c, u8 *buf, u64 max, u64 *used, u64 enc, u64 opts, rng_t r, rng_t *rest) {
    (void)c;
    int32_t n, u8n = 0;
    UChar *u = U16(self, &n);
    if (r.loc > (u64)n || r.len > (u64)n - r.loc) { free(u); if (used) *used = 0; return NO; }
    u32 cf = cf_of(enc);
    int32_t k = enc_encode(u + r.loc, (int32_t)r.len, cf, (opts & 1) ? '?' : 0, (opts & 2) != 0, buf, buf ? (int32_t)max : 0, &u8n);
    free(u);
    if (used) *used = (u64)u8n;
    if (rest) { rest->loc = r.loc + (u64)k; rest->len = r.len - (u64)k; }
    return k > 0 || r.len == 0;
}
static BOOL write_text(id self, id path, id url, BOOL atomic, u64 enc, id *err) {
    int32_t n;
    u32 cf = cf_of(enc);
    u8 *b = encode(self, enc, 0, cf == ENC_UTF16 || cf == ENC_UTF32, &n);
    if (!b) { if (err) *err = mk_error(517, path); return NO; }   /* NSFileWriteInapplicableStringEncodingError */
    id d = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)b, (u64)n);
    free(b);
    BOOL ok = url ? MSG(BOOL, d, "writeToURL:atomically:", url, atomic) : MSG(BOOL, d, "writeToFile:atomically:", path, atomic);
    if (!ok) { if (err) *err = mk_error(512, path); return NO; }   /* NSFileWriteUnknownError */
    const char *p = path ? MSG0(const char *, path, "UTF8String") : 0;
    if (p) {
        /* com.apple.TextEncoding, como o TextEdit/Foundation gravam: "<iana>;<CFStringEncoding>" */
        char x[96];
        id iana = CFStringConvertEncodingToIANACharSetName(cf);
        snprintf(x, sizeof x, "%s;%u", iana ? MSG0(const char *, iana, "UTF8String") : "", (unsigned)cf);
        extern int setxattr(const char *, const char *, const void *, u64, u32, int);
        setxattr(p, "com.apple.TextEncoding", x, strlen(x), 0, 0);
    }
    if (err) *err = 0;
    return YES;
}
static BOOL s_write_file(id self, SEL c, id p, BOOL atomic, u64 enc, id *err) { (void)c; return write_text(self, p, 0, atomic, enc, err); }
static BOOL s_write_url(id self, SEL c, id u, BOOL atomic, u64 enc, id *err) { (void)c; return write_text(self, url_path(u), u, atomic, enc, err); }

/* ---------------------------------------------------------------- C strings antigas (defaultCStringEncoding) */
static u64 k_default_cenc(id k, SEL c) { (void)k; (void)c; return 30; }   /* NSMacOSRomanStringEncoding */
static const char *s_cstring(id self, SEL c) {
    const char *r = s_cstr_enc(self, c, 30);
    if (!r) {
        id e = MSG(id, cls("NSException"), "exceptionWithName:reason:userInfo:", cx_str("NSCharacterConversionException"),
                   cx_str("Conversion to encoding 30 failed"), (id)0);
        MSG0(void, e, "raise");
    }
    return r;
}
static const char *s_lossy_cstring(id self, SEL c) {
    (void)c;
    int32_t n;
    u8 *b = encode(self, 30, 1, 0, &n);
    if (!b) return "";
    id d = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)b, (u64)n + 1);
    free(b);
    return MSG0(const char *, d, "bytes");
}
static u64 s_cstring_len(id self, SEL c) { return s_len_bytes(self, c, 30); }
static void get_cstr_range(id self, char *buf, u64 max, rng_t r, rng_t *rest) {
    int32_t n, used = 0;
    UChar *u = U16(self, &n);
    if (r.loc > (u64)n || r.len > (u64)n - r.loc) { free(u); if (buf) buf[0] = 0; return; }
    int32_t k = enc_encode(u + r.loc, (int32_t)r.len, ENC_MACROMAN, 0, 0, (u8 *)buf, (int32_t)max, &used);
    free(u);
    buf[used] = 0;
    if (rest) { rest->loc = r.loc + (u64)k; rest->len = r.len - (u64)k; }
}
static void s_get_cstr(id self, SEL c, char *buf) { (void)c; rng_t r = { 0, MSG0(u64, self, "length") }; get_cstr_range(self, buf, 0x7fffffff, r, 0); }
static void s_get_cstr_max(id self, SEL c, char *buf, u64 max) { (void)c; rng_t r = { 0, MSG0(u64, self, "length") }; get_cstr_range(self, buf, max, r, 0); }
static void s_get_cstr_range(id self, SEL c, char *buf, u64 max, rng_t r, rng_t *rest) { (void)c; get_cstr_range(self, buf, max, r, rest); }

/* ---------------------------------------------------------------- classe */
static const u64 *k_available(id k, SEL c) {
    (void)k; (void)c;
    static u64 list[128];
    static int done;
    if (!done) {
        int i = 0;
        for (const uint32_t *l = enc_list(); *l != ENC_INVALID && i < 127; l++) list[i++] = CFStringConvertEncodingToNSStringEncoding(*l);
        list[i] = 0;
        done = 1;
    }
    return list;
}
static id k_enc_name(id k, SEL c, u64 enc) { (void)k; (void)c; const char *n = enc_name(cf_of(enc)); return n ? cx_str(n) : cx_str(""); }
/* +stringEncodingForData:encodingOptions:convertedString:usedLossyConversion: */
static u64 k_detect(id k, SEL c, id data, id opts, id *out, BOOL *lossy) {
    (void)k; (void)c;
    if (out) *out = 0;
    if (lossy) *lossy = NO;
    if (!data) return 0;
    const u8 *b = MSG0(const u8 *, data, "bytes");
    int32_t n = (int32_t)MSG0(u64, data, "length"), m;
    u32 sug[32];
    int ns = 0, only = 0, allow_lossy = 1, from_win = 0;
    id dis = 0;
    if (opts) {
        id s = MSG(id, opts, "objectForKey:", cx_str("NSStringEncodingDetectionSuggestedEncodingsKey"));
        u64 cnt = s ? MSG0(u64, s, "count") : 0;
        for (u64 i = 0; i < cnt && ns < 32; i++) sug[ns++] = cf_of(MSG0(u64, MSG(id, s, "objectAtIndex:", i), "unsignedIntegerValue"));
        id o = MSG(id, opts, "objectForKey:", cx_str("NSStringEncodingDetectionUseOnlySuggestedEncodingsKey"));
        only = o && MSG0(BOOL, o, "boolValue");
        id a = MSG(id, opts, "objectForKey:", cx_str("NSStringEncodingDetectionAllowLossyKey"));
        if (a) allow_lossy = MSG0(BOOL, a, "boolValue");
        id w = MSG(id, opts, "objectForKey:", cx_str("NSStringEncodingDetectionFromWindowsKey"));
        from_win = w && MSG0(BOOL, w, "boolValue");
        dis = MSG(id, opts, "objectForKey:", cx_str("NSStringEncodingDetectionDisallowedEncodingsKey"));
    }
    /* sem sugestão que sirva: o mais provável depois do UTF-8 (Windows-1252 se veio do Windows, senão MacRoman) */
    if (!only && ns < 31) sug[ns++] = from_win ? ENC_WIN1252 : ENC_MACROMAN;
    u32 cf = enc_detect(b, n, sug, ns, only);
    if (cf != ENC_INVALID && dis) {
        id num = MSG(id, cls("NSNumber"), "numberWithUnsignedLongLong:", CFStringConvertEncodingToNSStringEncoding(cf));
        if (MSG(BOOL, dis, "containsObject:", num)) cf = ENC_INVALID;
    }
    UChar *u = cf != ENC_INVALID ? enc_decode(b, n, cf, &m) : 0;
    if (!u && allow_lossy && ns) {
        /* com perda: a primeira candidata, trocando o que não decodifica pelo caractere de substituição */
        cf = sug[0];
        int32_t cap = n + 1;
        u = malloc((u64)cap * sizeof(UChar));
        m = 0;
        for (int32_t i = 0; i < n; i++) {
            int32_t one;
            UChar *t = enc_decode(b + i, 1, cf, &one);
            u[m++] = t && one ? t[0] : 0xFFFD;
            free(t);
        }
        if (lossy) *lossy = YES;
    }
    if (!u) return 0;
    if (out) *out = objc_autorelease(new_str(u, m));
    free(u);
    return CFStringConvertEncodingToNSStringEncoding(cf);
}

/* ---------------------------------------------------------------- porcentagem e property lists */
static id s_add_pct(id self, SEL c, u64 enc) {
    (void)c;
    int32_t n;
    u8 *b = encode(self, enc, 0, 0, &n);
    if (!b) return 0;
    static const char keep[] = "!$&'()*+,-./:;=?@_~#";
    char *o = malloc((u64)n * 3 + 1);
    int32_t k = 0;
    for (int32_t i = 0; i < n; i++) {
        u8 ch = b[i];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (ch && strchr(keep, ch))) o[k++] = (char)ch;
        else { snprintf(o + k, 4, "%%%02X", ch); k += 3; }
    }
    o[k] = 0;
    free(b);
    id r = objc_autorelease(cx_str(o));
    free(o);
    return r;
}
static int hexd(char ch) { return ch >= '0' && ch <= '9' ? ch - '0' : (ch | 32) >= 'a' && (ch | 32) <= 'f' ? (ch | 32) - 'a' + 10 : -1; }
static id s_rep_pct(id self, SEL c, u64 enc) {
    (void)c;
    const char *p = MSG0(const char *, self, "UTF8String");
    if (!p) return 0;
    u64 n = strlen(p);
    u8 *b = malloc(n + 1);
    int32_t k = 0;
    for (u64 i = 0; i < n; i++) {
        if (p[i] == '%') {
            int h1 = i + 1 < n ? hexd(p[i + 1]) : -1, h2 = i + 2 < n ? hexd(p[i + 2]) : -1;
            if (h1 < 0 || h2 < 0) { free(b); return 0; }
            b[k++] = (u8)(h1 * 16 + h2);
            i += 2;
        } else b[k++] = (u8)p[i];
    }
    int32_t m;
    UChar *u = enc_decode(b, k, cf_of(enc), &m);
    free(b);
    if (!u) return 0;
    id r = objc_autorelease(new_str(u, m));
    free(u);
    return r;
}
static id s_plist(id self, SEL c) {
    (void)c;
    const char *p = MSG0(const char *, self, "UTF8String");
    id d = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)p, (u64)strlen(p));
    id r = ((id (*)(id, SEL, id, u64, u64 *, id *))objc_msgSend)(cls("NSPropertyListSerialization"),
            sel_registerName("propertyListWithData:options:format:error:"), d, 0, 0, 0);
    if (!r) {
        id e = MSG(id, cls("NSException"), "exceptionWithName:reason:userInfo:", cx_str("NSParseErrorException"),
                   cx_str("-[NSString propertyList]: string is not a property list"), (id)0);
        MSG0(void, e, "raise");
    }
    return r;
}
static id s_plist_strings(id self, SEL c) {
    (void)c;
    const char *p = MSG0(const char *, self, "UTF8String");
    return fdn_parse_strings((const u8 *)p, strlen(p));
}

/* ---------------------------------------------------------------- CFString */
typedef long CFIndex;
id CFStringCreateWithBytes(void *alloc, const u8 *b, CFIndex n, u32 enc, u8 ext) {
    (void)alloc; (void)ext;
    int32_t m;
    UChar *u = b || !n ? enc_decode(b ? b : (const u8 *)"", (int32_t)n, enc, &m) : 0;
    if (!u) return 0;
    id r = new_str(u, m);
    free(u);
    return r;
}
id CFStringCreateWithCString(void *alloc, const char *s, u32 enc) {
    if (!s) return 0;
    u64 n;
    if (enc == ENC_UTF16 || enc == ENC_UTF16BE || enc == ENC_UTF16LE) { const u16 *w = (const u16 *)s; n = 0; while (w[n]) n++; n *= 2; }
    else n = strlen(s);
    return CFStringCreateWithBytes(alloc, (const u8 *)s, (CFIndex)n, enc, 0);
}
id CFStringCreateWithPascalString(void *alloc, const u8 *p, u32 enc) { return p ? CFStringCreateWithBytes(alloc, p + 1, p[0], enc, 0) : 0; }
CFIndex CFStringGetBytes(id s, CFIndex loc, CFIndex len, u32 enc, u8 loss, u8 ext, u8 *buf, CFIndex max, CFIndex *used) {
    int32_t n, u8n = 0;
    UChar *u = U16(s, &n);
    if (loc < 0 || loc > n) loc = n;
    if (len < 0 || loc + len > n) len = n - loc;
    int32_t k = enc_encode(u + loc, (int32_t)len, enc, loss, ext != 0, buf, buf ? (int32_t)max : 0, &u8n);
    free(u);
    if (used) *used = u8n;
    return k;
}
u8 CFStringGetCString(id s, char *buf, CFIndex size, u32 enc) {
    if (!s || !buf || size <= 0) return 0;
    int32_t n, used = 0;
    UChar *u = U16(s, &n);
    int32_t k = enc_encode(u, n, enc, 0, 0, (u8 *)buf, (int32_t)size - 1, &used);
    free(u);
    if (k != n) { buf[0] = 0; return 0; }
    buf[used] = 0;
    return 1;
}
u8 CFStringGetPascalString(id s, u8 *buf, CFIndex size, u32 enc) {
    if (!s || !buf || size < 1) return 0;
    int32_t n, used = 0;
    UChar *u = U16(s, &n);
    int32_t max = (int32_t)(size - 1 > 255 ? 255 : size - 1);
    int32_t k = enc_encode(u, n, enc, 0, 0, buf + 1, max, &used);
    free(u);
    if (k != n) return 0;
    buf[0] = (u8)used;
    return 1;
}
/* o ponteiro interno só existe quando o texto já está nessa forma: UTF-8 sempre; ASCII e MacRoman se for ASCII */
const char *CFStringGetCStringPtr(id s, u32 enc) {
    if (!s) return 0;
    if (enc == ENC_UTF8) return MSG0(const char *, s, "UTF8String");
    if ((enc == ENC_ASCII || enc == ENC_MACROMAN || enc == ENC_LATIN1 || enc == ENC_WIN1252) && all_ascii(s)) return MSG0(const char *, s, "UTF8String");
    return 0;
}
CFIndex CFStringGetMaximumSizeForEncoding(CFIndex len, u32 enc) { return enc_max_bytes(enc, (int32_t)len); }
u8 CFStringIsEncodingAvailable(u32 enc) { return (u8)enc_available(enc); }
const u32 *CFStringGetListOfAvailableEncodings(void) { return enc_list(); }
id CFStringGetNameOfEncoding(u32 enc) {
    const char *n = enc_name(enc);
    static id cache[64];
    static u32 keys[64];
    static int nk;
    if (!n) return 0;
    for (int i = 0; i < nk; i++) if (keys[i] == enc) return cache[i];   /* Get: não é dono */
    id s = cx_str(n);
    if (nk < 64) { keys[nk] = enc; cache[nk++] = s; }
    return s;
}
u32 CFStringGetSmallestEncoding(id s) {
    if (all_ascii(s)) return ENC_ASCII;
    int32_t n;
    UChar *u = U16(s, &n);
    u32 r = enc_lossless(u, n, ENC_MACROMAN) ? ENC_MACROMAN : ENC_UTF16;
    free(u);
    return r;
}
u32 CFStringConvertEncodingToWindowsCodepage(u32 enc) { u32 cp = enc_windows_codepage(enc); return cp ? cp : ENC_INVALID; }
u32 CFStringConvertWindowsCodepageToEncoding(u32 cp) { return enc_from_windows_codepage(cp); }
u32 CFStringGetMostCompatibleMacStringEncoding(u32 enc) { return enc_mac_compatible(enc); }

void cx_fdn_string_enc_init(void) {
    cx_class_t *k = &NSString_c;
#define M(s, f) cx_add_method(k, s, (void *)(f))
#define CM(s, f) cx_add_cmethod(k, s, (void *)(f))
    M("initWithBytes:length:encoding:", i_bytes); M("initWithBytesNoCopy:length:encoding:freeWhenDone:", i_bytes_nocopy);
    M("initWithData:encoding:", i_data); M("initWithCString:encoding:", i_cstring);
    M("initWithContentsOfFile:encoding:error:", i_file_enc); M("initWithContentsOfFile:usedEncoding:error:", i_file_used);
    M("initWithContentsOfURL:encoding:error:", i_url_enc); M("initWithContentsOfURL:usedEncoding:error:", i_url_used);
    CM("stringWithCString:encoding:", k_cstring);
    CM("stringWithContentsOfFile:encoding:error:", k_file_enc); CM("stringWithContentsOfFile:usedEncoding:error:", k_file_used);
    CM("stringWithContentsOfURL:encoding:error:", k_url_enc); CM("stringWithContentsOfURL:usedEncoding:error:", k_url_used);
    M("dataUsingEncoding:", s_data); M("dataUsingEncoding:allowLossyConversion:", s_data_lossy);
    M("cStringUsingEncoding:", s_cstr_enc); M("getCString:maxLength:encoding:", s_get_cstr_enc);
    M("lengthOfBytesUsingEncoding:", s_len_bytes); M("maximumLengthOfBytesUsingEncoding:", s_max_bytes);
    M("canBeConvertedToEncoding:", s_can_convert); M("fastestEncoding", s_fastest); M("smallestEncoding", s_smallest);
    M("getBytes:maxLength:usedLength:encoding:options:range:remainingRange:", s_get_bytes);
    M("writeToFile:atomically:encoding:error:", s_write_file); M("writeToURL:atomically:encoding:error:", s_write_url);
    M("cString", s_cstring); M("lossyCString", s_lossy_cstring); M("cStringLength", s_cstring_len);
    M("getCString:", s_get_cstr); M("getCString:maxLength:", s_get_cstr_max);
    M("getCString:maxLength:range:remainingRange:", s_get_cstr_range);
    CM("defaultCStringEncoding", k_default_cenc); CM("availableStringEncodings", k_available);
    CM("localizedNameOfStringEncoding:", k_enc_name);
    CM("stringEncodingForData:encodingOptions:convertedString:usedLossyConversion:", k_detect);
    M("stringByAddingPercentEscapesUsingEncoding:", s_add_pct); M("stringByReplacingPercentEscapesUsingEncoding:", s_rep_pct);
    M("propertyList", s_plist); M("propertyListFromStringsFileFormat", s_plist_strings);
#undef M
#undef CM
}
