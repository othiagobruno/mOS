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

long CFArrayGetFirstIndexOfValue(id a, rng_t r, id v) {
    long i, n = CFArrayGetCount(a);
    if (r.loc < 0) r.loc = 0;
    for (i = r.loc; i < r.loc + r.len && i < n; i++) {
        id x = CFArrayGetValueAtIndex(a, i);
        if (x == v || (x && v && MSG(signed char, x, "isEqual:", v))) return i;
    }
    return -1;
}
void CFArrayGetValues(id a, rng_t r, id *out) {
    long i, n = CFArrayGetCount(a), k = 0;
    if (!out) return;
    for (i = r.loc; i < r.loc + r.len && i < n; i++) out[k++] = CFArrayGetValueAtIndex(a, i);
}

id CFAttributedStringCreateMutable(void *alloc, long max) {
    (void)alloc; (void)max;
    return MSG(id, MSG0(id, (id)cx_class_named("NSMutableAttributedString"), "alloc"), "initWithString:", cx_str(""));
}
void CFAttributedStringSetAttribute(id a, rng_t r, id name, id value) {
    if (a && name) MSG(void, a, "addAttribute:value:range:", name, value, r);
}
void CFAttributedStringRemoveAttribute(id a, rng_t r, id name) {
    (void)name;
    if (a) MSG(void, a, "setAttributes:range:", (id)0, r);
}
void CFAttributedStringReplaceString(id a, rng_t r, id s) {
    if (a) MSG(void, a, "replaceCharactersInRange:withString:", r, s ? s : cx_str(""));
}

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

void CFSetApplyFunction(id s, void (*fn)(id, void *), void *ctx) {
    long n = CFSetGetCount(s), i;
    id *v = n ? calloc((size_t)n, sizeof *v) : 0;
    if (!fn) { free(v); return; }
    if (v) CFSetGetValues(s, v);
    for (i = 0; i < n; i++) fn(v[i], ctx);
    free(v);
}

void CFStringAppendCString(id m, const char *s, unsigned enc) {
    id t; (void)enc;
    t = S(s);
    CFStringAppend(m, t);
    objc_release(t);
}
void CFStringAppendCharacters(id m, const unsigned short *ch, long n) {
    id t = CFStringCreateWithCharacters(0, ch, n);
    CFStringAppend(m, t);
    if (t) objc_release(t);
}
id CFStringConvertEncodingToIANACharSetName(unsigned enc) {
    if (enc == 0x08000100u || enc == 0x0600) return S("UTF-8");
    if (enc == 0x14000100u || enc == 0x100) return S("UTF-16");
    if (enc == 1 || enc == 0x0600) return S("US-ASCII");
    return S("UTF-8");
}
id CFStringCreateArrayBySeparatingStrings(void *alloc, id s, id sep) {
    const char *a = C(s), *b = C(sep);
    id arr = CFArrayCreateMutable(alloc, 0, 0);
    unsigned long sl = strlen(b);
    const char *p = a;
    if (!sl) { id one = S(a); CFArrayAppendValue(arr, one); objc_release(one); return arr; }
    for (;;) {
        const char *q = strstr(p, b);
        id part = CFStringCreateWithCString(0, "", UTF8);
        char *tmp;
        unsigned long n = q ? (unsigned long)(q - p) : strlen(p);
        tmp = malloc(n + 1);
        if (tmp) { memcpy(tmp, p, n); tmp[n] = 0; objc_release(part); part = S(tmp); free(tmp); }
        CFArrayAppendValue(arr, part);
        objc_release(part);
        if (!q) break;
        p = q + sl;
    }
    return arr;
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
rng_t CFStringFind(id s, id find, unsigned long opts) {
    rng_t in = {0, CFStringGetLength(s)}, out = {-1, 0};
    if (!CFStringFindWithOptions(s, find, in, opts, &out)) return (rng_t){-1, 0};
    return out;
}
long CFStringFindAndReplace(id s, id find, id repl, rng_t r, unsigned long opts) {
    long n = 0;
    (void)r; (void)opts;
    while (CFStringFind(s, find, 0).loc >= 0 && n < 64) {
        rng_t hit = CFStringFind(s, find, 0);
        id head, tail, neu;
        const char *a = C(s);
        char *buf;
        unsigned long L = strlen(a), fl = strlen(C(find)), rl = strlen(C(repl));
        if (hit.loc < 0 || (unsigned long)hit.loc > L) break;
        buf = malloc(L + rl + 1);
        if (!buf) break;
        memcpy(buf, a, (size_t)hit.loc);
        memcpy(buf + hit.loc, C(repl), rl);
        memcpy(buf + hit.loc + rl, a + hit.loc + (fl < L - (unsigned long)hit.loc ? fl : L - (unsigned long)hit.loc), L - (unsigned long)hit.loc - (fl < L ? fl : 0));
        /* reconstitui pelo texto UTF-8 inteiro: loc é UTF-16, então só é exato em ASCII */
        {
            const char *src = C(s);
            char *o = malloc(strlen(src) + rl + 1);
            unsigned long i = 0, oN = 0, fN = strlen(C(find));
            if (!o) { free(buf); break; }
            while (src[i]) {
                if (!strncmp(src + i, C(find), fN) && fN) {
                    memcpy(o + oN, C(repl), rl); oN += rl; i += fN; n++;
                } else o[oN++] = src[i++];
            }
            o[oN] = 0;
            neu = S(o); free(o); free(buf);
            if (neu && cx_responds(object_getClass(s), sel_registerName("setString:"))) MSG(void, s, "setString:", neu);
            if (neu) objc_release(neu);
            (void)head; (void)tail;
            break;
        }
    }
    return n;
}
rng_t CFStringFindCharacterFromSet(id s, id set, rng_t r, unsigned long opts) {
    long i, n = CFStringGetLength(s);
    rng_t miss = {-1, 0};
    (void)opts;
    if (r.loc < 0) r.loc = 0;
    for (i = r.loc; i < r.loc + r.len && i < n; i++) {
        unsigned short ch = 0;
        rng_t one = {i, 1};
        CFStringGetCharacters(s, one, &ch);
        if (CFCharacterSetIsLongCharacterMember(set, ch)) return one;
    }
    return miss;
}
void CFStringFold(id s, unsigned long opts, id locale) { (void)opts; CFStringLowercase(s, locale); }
const unsigned short *CFStringGetCharactersPtr(id s) { (void)s; return 0; }
unsigned CFStringGetFastestEncoding(id s) { (void)s; return UTF8; }
void CFStringReplace(id s, rng_t r, id repl) {
    (void)r;
    if (s && cx_responds(object_getClass(s), sel_registerName("setString:"))) MSG(void, s, "setString:", repl ? repl : cx_str(""));
}
unsigned char CFStringTransform(id s, rng_t *r, id transform, unsigned char reverse) {
    char buf[256], out[256];
    (void)r; (void)reverse;
    cf_norm_utf8(C(s), 0, buf, (int)sizeof buf);
    {
        int i = 0, o = 0;
        while (buf[i] && o + 1 < (int)sizeof out) {
            unsigned char c = (unsigned char)buf[i];
            if (c == 0xCC) { i += 2; continue; }
            out[o++] = (char)c; i++;
        }
        out[o] = 0;
    }
    if (transform && cx_responds(object_getClass(s), sel_registerName("setString:"))) {
        id n = S(out);
        MSG(void, s, "setString:", n);
        objc_release(n);
        return 1;
    }
    return 0;
}

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
id CFStringTokenizerCopyBestStringLanguage(id s, rng_t r) {
    const char *p = C(s);
    (void)r;
    while (*p) { if ((unsigned char)*p >= 0xC0) return S("pt"); p++; }
    return S("en");
}

double CFTimeZoneGetNextDaylightSavingTimeTransition(id tz, double at) {
    const char *n = tz ? C(tz) : "UTC";
    return cf_tz_next_dst(n[0] ? n : "UTC", at);
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
id CFNetworkCopyProxiesForAutoConfigurationScript(id script, id url) {
    const char *s = C(script);
    const char *p = strstr(s, "PROXY ");
    id arr = CFArrayCreateMutable(0, 0, 0);
    id d = CFDictionaryCreateMutable(0, 0, 0, 0);
    (void)url;
    if (p) {
        char host[128]; int port = 0, i = 0;
        p += 6;
        while (*p && *p != ':' && *p != ';' && *p != ' ' && i < 120) host[i++] = *p++;
        host[i] = 0;
        if (*p == ':') port = atoi(p + 1);
        CFDictionarySetValue(d, cx_str("kCFProxyTypeKey"), cx_str("kCFProxyTypeHTTP"));
        { id h = S(host); CFDictionarySetValue(d, cx_str("kCFProxyHostNameKey"), h); objc_release(h); }
        { long long pv = port; id n = CFNumberCreate(0, 4, &pv); if (n) { CFDictionarySetValue(d, cx_str("kCFProxyPortNumberKey"), n); objc_release(n); } }
    } else {
        CFDictionarySetValue(d, cx_str("kCFProxyTypeKey"), cx_str("kCFProxyTypeNone"));
    }
    CFArrayAppendValue(arr, d);
    objc_release(d);
    return arr;
}

void *CFFileSecurityCopyAccessControlList(void *s) { (void)s; return 0; }
