/*
 * mOS libSystem — property lists (XML e binário bplist00) → objetos da Foundation.
 * Usado por NSBundle (Info.plist), NSDictionary/NSArray ...WithContentsOfFile:,
 * NSPropertyListSerialization e pelo carregador de storyboards.
 */
#include "foundation.h"
#include "libc.h"

long long strtoll(const char *s, char **e, int b);
double strtod(const char *s, char **e);

static id mdict(void) { return MSG0(id, MSG0(id, (id)cx_class_named("NSMutableDictionary"), "alloc"), "init"); }
static void dset(id d, id k, id v) { if (d && k && v) MSG(void, d, "setObject:forKey:", v, k); }
static id mbool(BOOL b) { return MSG(id, (id)cx_class_named("NSNumber"), "numberWithBool:", (int)b); }
static id mdata(const u8 *p, u64 n) { return MSG(id, (id)cx_class_named("NSData"), "dataWithBytes:length:", p, n); }

/* ---------------------------------------------------------------- XML */
typedef struct { const char *p, *e; } xp_t;
static void xs_skip(xp_t *x) {
    for (;;) {
        while (x->p < x->e && (*x->p == ' ' || *x->p == '\n' || *x->p == '\r' || *x->p == '\t')) x->p++;
        if (x->e - x->p >= 4 && !strncmp(x->p, "<!--", 4)) { const char *q = strstr(x->p, "-->"); x->p = q && q < x->e ? q + 3 : x->e; continue; }
        if (x->e - x->p >= 2 && (!strncmp(x->p, "<?", 2) || !strncmp(x->p, "<!", 2))) { while (x->p < x->e && *x->p != '>') x->p++; if (x->p < x->e) x->p++; continue; }
        return;
    }
}
/* lê "<tag>" ou "<tag/>"; devolve 1 = abertura, 2 = vazia, 0 = erro; nome em tag */
static int xs_open(xp_t *x, char *tag, int cap) {
    xs_skip(x);
    if (x->p >= x->e || *x->p != '<' || x->p[1] == '/') return 0;
    x->p++;
    int n = 0;
    while (x->p < x->e && *x->p != '>' && *x->p != ' ' && *x->p != '/' && n < cap - 1) tag[n++] = *x->p++;
    tag[n] = 0;
    while (x->p < x->e && *x->p != '>' && *x->p != '/') x->p++;
    if (x->p < x->e && *x->p == '/') { while (x->p < x->e && *x->p != '>') x->p++; x->p++; return 2; }
    x->p++;
    return 1;
}
static void xs_close(xp_t *x) { xs_skip(x); if (x->p < x->e && *x->p == '<' && x->p[1] == '/') { while (x->p < x->e && *x->p != '>') x->p++; x->p++; } }
/* texto até o próximo '<', com entidades XML decodificadas */
static id xs_text(xp_t *x) {
    const char *s = x->p;
    while (x->p < x->e && *x->p != '<') x->p++;
    u64 n = (u64)(x->p - s);
    char *b = malloc(n + 1), *o = b;
    for (const char *q = s; q < x->p;) {
        if (*q == '&') {
            if (!strncmp(q, "&lt;", 4)) { *o++ = '<'; q += 4; continue; }
            if (!strncmp(q, "&gt;", 4)) { *o++ = '>'; q += 4; continue; }
            if (!strncmp(q, "&amp;", 5)) { *o++ = '&'; q += 5; continue; }
            if (!strncmp(q, "&quot;", 6)) { *o++ = '"'; q += 6; continue; }
            if (!strncmp(q, "&apos;", 6)) { *o++ = '\''; q += 6; continue; }
        }
        *o++ = *q++;
    }
    *o = 0;
    id r = cx_str(b);
    free(b);
    return r;
}
static int b64v(char c) { return c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : -1; }
static id xs_value(xp_t *x, int depth) {
    char tag[32];
    int k = xs_open(x, tag, sizeof tag);
    if (!k || depth > 64) return 0;
    if (!strcmp(tag, "dict")) {
        id d = mdict();
        if (k == 2) return d;
        for (;;) {
            xs_skip(x);
            if (x->p >= x->e || (x->p[0] == '<' && x->p[1] == '/')) break;
            char kt[32];
            if (xs_open(x, kt, sizeof kt) != 1 || strcmp(kt, "key")) break;
            id key = xs_text(x);
            xs_close(x);
            id v = xs_value(x, depth + 1);
            dset(d, key, v);
        }
        xs_close(x);
        return d;
    }
    if (!strcmp(tag, "array")) {
        id a = cx_arr_new(0);
        if (k == 2) return a;
        for (;;) {
            xs_skip(x);
            if (x->p >= x->e || (x->p[0] == '<' && x->p[1] == '/')) break;
            id v = xs_value(x, depth + 1);
            if (!v) break;
            cx_arr_add(a, v);
        }
        xs_close(x);
        return a;
    }
    if (!strcmp(tag, "true")) return mbool(YES);
    if (!strcmp(tag, "false")) return mbool(NO);
    if (k == 2) return !strcmp(tag, "string") ? cx_str("") : 0;
    id t = xs_text(x);
    xs_close(x);
    if (!strcmp(tag, "string") || !strcmp(tag, "date")) return t;
    if (!strcmp(tag, "integer")) return cx_num_i(strtoll(cx_cstr(t), 0, 10));
    if (!strcmp(tag, "real")) return cx_num_d(strtod(cx_cstr(t), 0));
    if (!strcmp(tag, "data")) {
        const char *s = cx_cstr(t);
        u64 n = strlen(s), o = 0;
        u8 *b = malloc(n);
        u32 acc = 0; int bits = 0;
        for (u64 i = 0; i < n; i++) { int v = b64v(s[i]); if (v < 0) continue; acc = acc << 6 | (u32)v; bits += 6; if (bits >= 8) { bits -= 8; b[o++] = (u8)(acc >> bits); } }
        id d = mdata(b, o);
        free(b);
        return d;
    }
    return t;
}

/* ---------------------------------------------------------------- binário */
typedef struct { const u8 *b; u64 n; u8 osz, rsz; u64 nobj, top, otab; } bp_t;
static u64 be(const u8 *p, int n) { u64 v = 0; for (int i = 0; i < n; i++) v = v << 8 | p[i]; return v; }
static id bp_obj(bp_t *bp, u64 idx, int depth);
static BOOL bp_len(bp_t *bp, const u8 **p, u64 *len) {
    u8 m = **p & 0x0F;
    (*p)++;
    if (m != 0x0F) { *len = m; return YES; }
    u8 t = **p;
    if ((t & 0xF0) != 0x10) return NO;
    int nb = 1 << (t & 0x0F);
    *len = be(*p + 1, nb);
    *p += 1 + nb;
    return YES;
}
static id bp_obj(bp_t *bp, u64 idx, int depth) {
    if (idx >= bp->nobj || depth > 64) return 0;
    u64 off = be(bp->b + bp->otab + idx * bp->osz, bp->osz);
    if (off >= bp->n) return 0;
    const u8 *p = bp->b + off;
    u8 t = *p >> 4;
    u64 len;
    switch (t) {
    case 0x0: return (*p & 0x0F) == 9 ? mbool(YES) : (*p & 0x0F) == 8 ? mbool(NO) : 0;
    case 0x1: { int nb = 1 << (*p & 0x0F); return cx_num_i((i64)be(p + 1, nb > 8 ? 8 : nb)); }
    case 0x2: {
        int nb = 1 << (*p & 0x0F);
        u64 v = be(p + 1, nb);
        if (nb == 4) { union { u32 u; float f; } x; x.u = (u32)v; return cx_num_d(x.f); }
        union { u64 u; double d; } x; x.u = v; return cx_num_d(x.d);
    }
    case 0x3: { union { u64 u; double d; } x; x.u = be(p + 1, 8); return cx_num_d(x.d); }
    case 0x4: if (!bp_len(bp, &p, &len) || p + len > bp->b + bp->n) return 0; return mdata(p, len);
    case 0x5: if (!bp_len(bp, &p, &len) || p + len > bp->b + bp->n) return 0; return cx_str_n((const char *)p, len);
    case 0x6: {   /* UTF-16 BE → UTF-8 */
        if (!bp_len(bp, &p, &len) || p + len * 2 > bp->b + bp->n) return 0;
        char *o = malloc(len * 3 + 1);
        u64 k = 0;
        for (u64 i = 0; i < len; i++) {
            u32 c = (u32)be(p + i * 2, 2);
            if (c >= 0xD800 && c < 0xDC00 && i + 1 < len) { u32 d = (u32)be(p + (i + 1) * 2, 2); c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00); i++; }
            if (c < 0x80) o[k++] = (char)c;
            else if (c < 0x800) { o[k++] = (char)(0xC0 | c >> 6); o[k++] = (char)(0x80 | (c & 63)); }
            else if (c < 0x10000) { o[k++] = (char)(0xE0 | c >> 12); o[k++] = (char)(0x80 | (c >> 6 & 63)); o[k++] = (char)(0x80 | (c & 63)); }
            else { o[k++] = (char)(0xF0 | c >> 18); o[k++] = (char)(0x80 | (c >> 12 & 63)); o[k++] = (char)(0x80 | (c >> 6 & 63)); o[k++] = (char)(0x80 | (c & 63)); }
        }
        id s = cx_str_n(o, k);
        free(o);
        return s;
    }
    case 0x8: {                                                /* UID (NSKeyedArchiver): o mesmo formato do plist XML */
        id d = mdict();
        dset(d, cx_str("CF$UID"), cx_num_i((i64)be(p + 1, (*p & 0x0F) + 1)));
        return d;
    }
    case 0xA: case 0xC: {
        if (!bp_len(bp, &p, &len)) return 0;
        id a = cx_arr_new(0);
        for (u64 i = 0; i < len; i++) { id v = bp_obj(bp, be(p + i * bp->rsz, bp->rsz), depth + 1); if (v) cx_arr_add(a, v); }
        return a;
    }
    case 0xD: {
        if (!bp_len(bp, &p, &len)) return 0;
        id d = mdict();
        for (u64 i = 0; i < len; i++)
            dset(d, bp_obj(bp, be(p + i * bp->rsz, bp->rsz), depth + 1), bp_obj(bp, be(p + (len + i) * bp->rsz, bp->rsz), depth + 1));
        return d;
    }
    }
    return 0;
}

id cx_plist_parse(const u8 *b, u64 n) {
    if (!b || !n) return 0;
    if (n >= 40 && !memcmp(b, "bplist00", 8)) {
        const u8 *t = b + n - 32;
        bp_t bp = {b, n, t[6], t[7], be(t + 8, 8), be(t + 16, 8), be(t + 24, 8)};
        if (!bp.osz || !bp.rsz || bp.otab >= n) return 0;
        return bp_obj(&bp, bp.top, 0);
    }
    xp_t x = {(const char *)b, (const char *)b + n};
    char tag[32];
    xs_skip(&x);
    const char *save = x.p;
    if (xs_open(&x, tag, sizeof tag) == 1 && !strcmp(tag, "plist")) return xs_value(&x, 0);
    x.p = save;
    return xs_value(&x, 0);
}
/* lê um arquivo inteiro (malloc; NUL no fim) */
u8 *cx_read_file(const char *path, u64 *len) {
    FILE_ *f = fopen(path, "rb");
    if (!f) return 0;
    u64 cap = 65536, n = 0;
    u8 *b = malloc(cap + 1);
    for (;;) {
        if (n == cap) { cap *= 2; b = realloc(b, cap + 1); }
        size_t r = fread(b + n, 1, cap - n, f);
        if (!r) break;
        n += r;
    }
    fclose(f);
    b[n] = 0;
    if (len) *len = n;
    return b;
}
id cx_plist_file(const char *path) {
    u64 n;
    u8 *b = cx_read_file(path, &n);
    if (!b) return 0;
    id r = cx_plist_parse(b, n);
    free(b);
    return r;
}
