/*
 * mOS libSystem — Dictionary Services (DCS*, CoreServices). Os dicionários são os do app Dicionário do mOS
 * (kernel/apps/referencia.c): Wikcionário em português e em inglês, pela API REST deles (rest_v1/page/definition). Sem
 * rede ou sem verbete, DCSCopyTextDefinition devolve NULL, como o macOS quando a palavra não está no dicionário.
 * DCSGetTermRangeInString acha a palavra em volta da posição (letras, números, hífen e apóstrofo internos).
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, id v);
extern const char kCFTypeArrayCallBacks[];
typedef struct { long loc, len; } cf_range;

static id cls(const char *n) { return (id)cx_class_named(n); }
#define ST(T, o) (*(T **)((u8 *)(o) + 8))
static cx_class_t Dict_c, Dict_m, Rec_c, Rec_m;
typedef struct { const char *ident, *name, *shortn, *lang; } dict_t;
typedef struct { id head, text; const dict_t *d; } rec_t;
static const dict_t g_dicts[] = {
    {"org.wiktionary.pt", "Wikcionário (português)", "Wikcionário", "pt"},
    {"org.wiktionary.en", "Wiktionary (English)", "Wiktionary", "en"},
};
static id g_dict_obj[2];
static void boot(void) {
    static int done;
    if (done) return;
    done = 1;
    cx_class_t *ns = cx_class_named("NSObject");
    cx_define_class(&Dict_c, &Dict_m, "__DCSDictionary", ns, 8);
    cx_define_class(&Rec_c, &Rec_m, "__DCSRecord", ns, 8);
    for (int i = 0; i < 2; i++) { g_dict_obj[i] = objc_retain(cx_alloc_instance(&Dict_c)); ST(const dict_t, g_dict_obj[i]) = &g_dicts[i]; }
}
static const dict_t *dict_of(id d) { return d && object_getClass(d) == &Dict_c ? ST(const dict_t, d) : 0; }
/* o da língua do sistema primeiro */
static int pref_index(void) { const char *l = cx_locale_id(); return l && !strncmp(l, "en", 2) ? 1 : 0; }

id DCSCopyAvailableDictionaries(void) {
    boot();
    id s = MSG0(id, cls("NSMutableSet"), "set");
    for (int i = 0; i < 2; i++) MSG(void, s, "addObject:", g_dict_obj[i]);
    return objc_retain(s);
}
id DCSGetActiveDictionaries(void) {
    boot();
    int p = pref_index();
    id a = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(a, g_dict_obj[p]); CFArrayAppendValue(a, g_dict_obj[1 - p]);
    return objc_autorelease(a);
}
id DCSDictionaryGetName(id d) { const dict_t *x = dict_of(d); return x ? cx_str(x->name) : 0; }
id DCSDictionaryGetShortName(id d) { const dict_t *x = dict_of(d); return x ? cx_str(x->shortn) : 0; }
id DCSDictionaryGetIdentifier(id d) { const dict_t *x = dict_of(d); return x ? cx_str(x->ident) : 0; }
id DCSDictionaryGetURL(id d) {
    const dict_t *x = dict_of(d);
    if (!x) return 0;
    char u[64];
    snprintf(u, sizeof u, "https://%s.wiktionary.org/", x->lang);
    return MSG(id, cls("NSURL"), "URLWithString:", cx_str(u));
}

/* ---- palavra em volta de uma posição */
static int word_char(u16 c) {
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return 1;
    if (c >= 0xc0 && c != 0xd7 && c != 0xf7 && c < 0x2000) return 1;           /* letras latinas, gregas, cirílicas... */
    if (c >= 0x3040 && c < 0xd800) return 1;                                     /* kana, CJK, hangul */
    return 0;
}
cf_range DCSGetTermRangeInString(id dict, id text, long offset) {
    (void)dict;
    cf_range r = {-1, 0};                                                        /* kCFNotFound */
    long n = text ? (long)MSG0(u64, text, "length") : 0;
    if (offset < 0 || offset >= n) return r;
    u16 *u = malloc((size_t)n * 2);
    if (!u) return r;
    MSG(void, text, "getCharacters:range:", u, (u64)0, (u64)n);
    if (!word_char(u[offset])) { free(u); return r; }
    long a = offset, b = offset;
    while (a > 0 && (word_char(u[a - 1]) || ((u[a - 1] == '-' || u[a - 1] == '\'') && a > 1 && word_char(u[a - 2])))) a--;
    while (b + 1 < n && (word_char(u[b + 1]) || ((u[b + 1] == '-' || u[b + 1] == '\'') && b + 2 < n && word_char(u[b + 2])))) b++;
    free(u);
    r.loc = a; r.len = b - a + 1;
    return r;
}

/* ---- verbetes pela API do Wikcionário: {"pt": [{"partOfSpeech", "definitions": [{"definition": "<html>"}]}]} */
static void strip_html(const char *in, char *out, size_t cap) {
    size_t o = 0;
    int tag = 0;
    for (const char *p = in; *p && o + 1 < cap; p++) {
        if (*p == '<') { tag = 1; continue; }
        if (*p == '>') { tag = 0; continue; }
        if (tag) continue;
        if (*p == '&') {
            if (!strncmp(p, "&amp;", 5)) { out[o++] = '&'; p += 4; continue; }
            if (!strncmp(p, "&quot;", 6)) { out[o++] = '"'; p += 5; continue; }
            if (!strncmp(p, "&#39;", 5)) { out[o++] = '\''; p += 4; continue; }
            if (!strncmp(p, "&lt;", 4)) { out[o++] = '<'; p += 3; continue; }
            if (!strncmp(p, "&gt;", 4)) { out[o++] = '>'; p += 3; continue; }
            if (!strncmp(p, "&nbsp;", 6)) { out[o++] = ' '; p += 5; continue; }
        }
        out[o++] = *p;
    }
    out[o] = 0;
}
/* lista de (classe gramatical, definições) da palavra no dicionário d; NULL sem rede ou sem verbete */
static id fetch(const dict_t *d, id word) {
    id enc = MSG(id, word, "stringByAddingPercentEncodingWithAllowedCharacters:", MSG0(id, cls("NSCharacterSet"), "URLPathAllowedCharacterSet"));
    if (!enc) return 0;
    char u[600];
    snprintf(u, sizeof u, "https://%s.wiktionary.org/api/rest_v1/page/definition/%s", d->lang, cx_cstr(enc));
    id url = MSG(id, cls("NSURL"), "URLWithString:", cx_str(u));
    id data = url ? MSG(id, cls("NSData"), "dataWithContentsOfURL:", url) : 0;
    if (!data) return 0;
    id json = MSG(id, cls("NSJSONSerialization"), "JSONObjectWithData:options:error:", data, (u64)0, (id *)0);
    if (!json || !MSG(BOOL, json, "isKindOfClass:", cls("NSDictionary"))) return 0;
    id entries = MSG(id, json, "objectForKey:", cx_str(d->lang));
    if (!entries) {                                                              /* às vezes o verbete vem noutra língua */
        id keys = MSG0(id, json, "allKeys");
        if (keys && MSG0(u64, keys, "count")) { id k0 = MSG(id, keys, "objectAtIndex:", (u64)0); entries = MSG(id, json, "objectForKey:", k0); }
    }
    return entries && MSG(BOOL, entries, "isKindOfClass:", cls("NSArray")) && MSG0(u64, entries, "count") ? entries : 0;
}
static id entry_text(id e, int with_pos) {
    id out = MSG0(id, cls("NSMutableString"), "string");
    id pos = MSG(id, e, "objectForKey:", cx_str("partOfSpeech"));
    if (with_pos && pos) { MSG(void, out, "appendString:", pos); MSG(void, out, "appendString:", cx_str(". ")); }
    id defs = MSG(id, e, "objectForKey:", cx_str("definitions"));
    u64 n = defs ? MSG0(u64, defs, "count") : 0, k = 0;
    for (u64 i = 0; i < n; i++) {
        id def = MSG(id, MSG(id, defs, "objectAtIndex:", i), "objectForKey:", cx_str("definition"));
        if (!def || !cx_is_str(def)) continue;
        const char *h = cx_cstr(def);
        size_t cap = strlen(h) + 1;
        char *t = malloc(cap);
        strip_html(h, t, cap);
        if (t[0]) {
            char num[16];
            k++;
            snprintf(num, sizeof num, "%s%llu ", k > 1 ? " " : "", (unsigned long long)k);
            MSG(void, out, "appendString:", cx_str(num));
            MSG(void, out, "appendString:", cx_str(t));
        }
        free(t);
    }
    return k ? out : 0;
}
id DCSCopyTextDefinition(id dict, id text, cf_range range) {
    boot();
    if (!text || range.loc < 0 || range.len <= 0) return 0;
    id word = MSG(id, text, "substringWithRange:", (u64)range.loc, (u64)range.len);
    const dict_t *one = dict_of(dict);
    int p = pref_index();
    for (int i = 0; i < 2; i++) {
        const dict_t *d = one ? one : &g_dicts[i == 0 ? p : 1 - p];
        id entries = fetch(d, word);
        if (entries) {
            id out = MSG0(id, cls("NSMutableString"), "string");
            MSG(void, out, "appendString:", word);
            for (u64 k = 0; k < MSG0(u64, entries, "count"); k++) {
                id t = entry_text(MSG(id, entries, "objectAtIndex:", k), 1);
                if (t) { MSG(void, out, "appendString:", cx_str(" | ")); MSG(void, out, "appendString:", t); }
            }
            return objc_retain(out);
        }
        if (one) break;
    }
    return 0;
}
/* registros: um por classe gramatical do verbete */
id DCSCopyRecordsForSearchString(id dict, id string, u64 method, long max) {
    (void)method;
    boot();
    const dict_t *d = dict_of(dict);
    if (!d || !string) return 0;
    id entries = fetch(d, string);
    if (!entries) return 0;
    id out = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    for (u64 k = 0; k < MSG0(u64, entries, "count") && (max <= 0 || (long)k < max); k++) {
        id t = entry_text(MSG(id, entries, "objectAtIndex:", k), 1);
        if (!t) continue;
        id r = cx_alloc_instance(&Rec_c);
        rec_t *x = calloc(1, sizeof *x);
        x->head = objc_retain(string); x->text = objc_retain(t); x->d = d;
        ST(rec_t, r) = x;
        CFArrayAppendValue(out, r);
        objc_release(r);
    }
    return out;
}
id DCSRecordGetHeadword(id rec) { rec_t *x = rec && object_getClass(rec) == &Rec_c ? ST(rec_t, rec) : 0; return x ? x->head : 0; }
id DCSRecordGetRawHeadword(id rec) { return DCSRecordGetHeadword(rec); }
id DCSRecordGetDictionary(id rec) { rec_t *x = rec && object_getClass(rec) == &Rec_c ? ST(rec_t, rec) : 0; return x ? g_dict_obj[x->d == &g_dicts[1]] : 0; }
/* versão 0 = texto (kDCSTextDefinitionVersion... na Apple há XML/HTML; aqui o texto puro serve a todas) */
id DCSRecordCopyData(id rec, long version) { (void)version; rec_t *x = rec && object_getClass(rec) == &Rec_c ? ST(rec_t, rec) : 0; return x ? objc_retain(x->text) : 0; }
id DCSRecordCopyDataURL(id rec) { (void)rec; return 0; }
