/* O que sys_zerados.c deixava como `void CF*()`. Cada uma faz o trabalho do macOS
 * em cima do que o libSystem já tem (string UTF-8, NSURL, NSAttributedString, run loop). */
#include "foundation.h"
#include "cf_public_core.inc"

id objc_retain(id o);
void objc_release(id o);
id CFStringCreateWithCString(void *alloc, const char *s, unsigned enc);
id CFStringCreateWithCharacters(void *alloc, const unsigned short *ch, long n);
long CFStringGetLength(id s);
void CFStringAppend(id m, id s);
void CFStringLowercase(id m, void *locale);
unsigned char CFCharacterSetIsLongCharacterMember(id s, unsigned c);
id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, id v);
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
id CFDataCreate(void *alloc, const unsigned char *b, long n);
const unsigned char *CFDataGetBytePtr(id d);
long CFDataGetLength(id d);
long CFSetGetCount(id s);
void CFSetGetValues(id s, id *out);
id CFURLCreateWithString(void *alloc, id s, id base);
id CFURLGetString(id url);
id CFNumberCreate(void *alloc, long type, const void *p);
const char *getenv(const char *n);
void *calloc(unsigned long n, unsigned long s);

#define UTF8 0x08000100u
typedef struct { long loc, len; } rng_t;
void CFStringGetCharacters(id s, rng_t r, unsigned short *o);
unsigned char CFStringFindWithOptions(id s, id find, rng_t range, unsigned long opts, rng_t *res);
static id S(const char *s) { return CFStringCreateWithCString(0, s ? s : "", UTF8); }
static const char *C(id o) { return o ? cx_cstr(o) : ""; }



id CFBundleGetInfoDictionary(id bundle) { return bundle ? MSG0(id, bundle, "infoDictionary") : 0; }
id CFCopyHomeDirectoryURL(void) {
    const char *h = getenv("HOME");
    id s = S(h && h[0] ? h : "/");
    id u = CFURLCreateWithString(0, s, 0);
    objc_release(s);
    return u;
}
const void *CFGetAllocator(id o) { (void)o; return 0; }
long CFGetRetainCount(id o) { return o ? MSG0(long, o, "retainCount") : 0; }

/* CFSetApplyFunction: fdn_set.c */

void CFStringAppendCString(id m, const char *s, unsigned enc) {
    id t; (void)enc;
    t = S(s);
    CFStringAppend(m, t);
    objc_release(t);
}
id CFStringConvertEncodingToIANACharSetName(unsigned enc) {
    if (enc == 0x08000100u || enc == 0x0600) return S("UTF-8");
    if (enc == 0x14000100u || enc == 0x100) return S("UTF-16");
    if (enc == 1 || enc == 0x0600) return S("US-ASCII");
    return S("UTF-8");
}
id CFStringCreateFromExternalRepresentation(void *alloc, id data, unsigned enc) {
    (void)alloc; (void)enc;
    return data ? CFStringCreateWithCString(0, (const char *)CFDataGetBytePtr(data), UTF8) : 0;
}
id CFStringCreateWithCharactersNoCopy(void *alloc, const unsigned short *ch, long n, void *dealloc) {
    (void)dealloc;
    return CFStringCreateWithCharacters(alloc, ch, n);
}
id CFStringCreateWithFormat(void *alloc, void *opts, id fmt, ...) {
    va_list ap; id r;
    (void)alloc; (void)opts;
    va_start(ap, fmt);
    r = cx_str_vfmt(fmt, ap);
    va_end(ap);
    return r ? objc_retain(r) : 0;
}
unsigned CFStringGetFastestEncoding(id s) { (void)s; return UTF8; }

typedef struct { int k; char *s; int i, n, loc, len; } tok_o;
static cx_class_t tok_c, tok_m;
static int tok_ready;
static id tok_box(tok_o *t) {
    typedef struct { void *isa; void *p; } box_t;
    if (!tok_ready) {
        tok_ready = 1;
        cx_define_class(&tok_c, &tok_m, "__mOSCFTokenizer", cx_class_named("NSObject"), 8);
    }
    {
        box_t *o = (box_t *)cx_alloc_instance(&tok_c);
        o->p = t;
        return (id)o;
    }
}
static tok_o *as_tok(id o) {
    typedef struct { void *isa; void *p; } box_t;
    return o && object_getClass(o) == &tok_c ? ((box_t *)o)->p : 0;
}
id CFStringTokenizerCreate(void *alloc, id str, rng_t r, unsigned long opts, id locale) {
    tok_o *t = calloc(1, sizeof *t);
    (void)alloc; (void)r; (void)opts; (void)locale;
    if (!t) return 0;
    t->k = 1; t->s = malloc(strlen(C(str)) + 1);
    if (t->s) memcpy(t->s, C(str), strlen(C(str)) + 1);
    t->n = t->s ? (int)strlen(t->s) : 0;
    t->loc = -1;
    return tok_box(t);
}
void CFStringTokenizerSetString(id tok, id str, rng_t r) {
    tok_o *t = as_tok(tok);
    (void)r;
    if (!t) return;
    free(t->s);
    t->s = malloc(strlen(C(str)) + 1);
    if (t->s) memcpy(t->s, C(str), strlen(C(str)) + 1);
    t->n = t->s ? (int)strlen(t->s) : 0;
    t->i = 0; t->loc = -1;
}
unsigned char CFStringTokenizerAdvanceToNextToken(id tok) {
    tok_o *t = as_tok(tok);
    if (!t || !t->s) return 0;
    while (t->i < t->n && (t->s[t->i] == ' ' || t->s[t->i] == '\n' || t->s[t->i] == '\t')) t->i++;
    if (t->i >= t->n) { t->loc = -1; t->len = 0; return 0; }
    t->loc = t->i;
    while (t->i < t->n && t->s[t->i] != ' ' && t->s[t->i] != '\n' && t->s[t->i] != '\t') t->i++;
    t->len = t->i - t->loc;
    return 1;
}
rng_t CFStringTokenizerGetCurrentTokenRange(id tok) {
    tok_o *t = as_tok(tok);
    rng_t r = {t ? t->loc : -1, t ? t->len : 0};
    return r;
}
unsigned char CFStringTokenizerGoToTokenAtIndex(id tok, long idx) {
    tok_o *t = as_tok(tok);
    if (!t) return 0;
    t->i = idx < 0 ? 0 : (int)idx;
    return CFStringTokenizerAdvanceToNextToken(tok);
}
/* kCFStringTokenizerAttributeLatinTranscription 0x10000, kCFStringTokenizerAttributeLanguage 0x20000 */
id CFStringTokenizerCopyCurrentTokenAttribute(id tok, unsigned long attr) {
    extern unsigned char CFStringTransform(id m, rng_t *range, id transform, unsigned char reverse);
    id CFStringTokenizerCopyBestStringLanguage(id s, rng_t r);
    tok_o *t = as_tok(tok);
    if (!t || t->loc < 0 || !t->s) return 0;
    id w = cx_str_n(t->s + t->loc, (unsigned long)t->len);
    if (attr == 0x20000) { rng_t r = {0, 0}; id l = CFStringTokenizerCopyBestStringLanguage(w, r); objc_release(w); return l; }
    if (attr == 0x10000) {
        id m = MSG0(id, w, "mutableCopy");
        objc_release(w);
        CFStringTransform(m, 0, S("Any-Latin"), 0);
        return m;
    }
    objc_release(w);
    return 0;
}
id CFStringTokenizerCopyBestStringLanguage(id s, rng_t r) {
    const char *p = C(s);
    (void)r;
    while (*p) { if ((unsigned char)*p >= 0xC0) return S("pt"); p++; }
    return S("en");
}


unsigned char CFURLCanBeDecomposed(id url) { cf_url_parts u; cf_url_parse(C(CFURLGetString(url)), &u); return u.scheme[0] != 0; }
void CFURLClearResourcePropertyCacheForKey(id url, id key) { (void)url; (void)key; }
id CFURLCopyHostName(id url) { cf_url_parts u; cf_url_parse(C(CFURLGetString(url)), &u); return u.host[0] ? S(u.host) : 0; }
id CFURLCopyPath(id url) { cf_url_parts u; cf_url_parse(C(CFURLGetString(url)), &u); return S(u.path[0] ? u.path : "/"); }
id CFURLCopyQueryString(id url, id chars) {
    cf_url_parts u; (void)chars;
    cf_url_parse(C(CFURLGetString(url)), &u);
    return u.query[0] ? S(u.query) : 0;
}
long CFURLGetBytes(id url, unsigned char *buf, long cap) {
    const char *s = C(CFURLGetString(url));
    long n = (long)strlen(s);
    if (buf && cap > 0) { long k = n < cap ? n : cap; memcpy(buf, s, (size_t)k); }
    return n;
}
rng_t CFURLGetByteRangeForComponent(id url, long comp, unsigned char *range_incl) {
    cf_url_parts u; const char *s = C(CFURLGetString(url)); rng_t r = {-1, 0}; const char *hit = 0;
    (void)range_incl;
    cf_url_parse(s, &u);
    if (comp == 1) hit = u.scheme[0] ? strstr(s, u.scheme) : 0;
    else if (comp == 8 || comp == 2) hit = u.host[0] ? strstr(s, u.host) : 0;
    else if (comp == 3) hit = u.path[0] ? strstr(s, u.path) : 0;
    else if (comp == 11) hit = u.query[0] ? strstr(s, u.query) : 0;
    if (hit) { r.loc = (long)(hit - s); r.len = (long)strlen(comp == 1 ? u.scheme : comp == 3 ? u.path : comp == 11 ? u.query : u.host); }
    return r;
}

id CFDictionaryCreateMutable(void *alloc, long cap, const void *k, const void *v);
void CFDictionarySetValue(id d, id k, id val);
/* CFNetworkCopyProxiesForAutoConfigurationScript: cfpac.c (avaliação real com o JavaScriptCore) */

void *CFFileSecurityCopyAccessControlList(void *s) { (void)s; return 0; }
