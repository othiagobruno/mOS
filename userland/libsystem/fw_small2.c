/*
 * mOS libSystem — frameworks pequenos que os apps do corpus ligam (docs/lacunas/natalia.md), parte 2.
 *  - LinkPresentation: LPMetadataProvider baixa a página e lê título, og:image, og:video e ícone; a imagem e o ícone
 *    vêm baixados num NSItemProvider (NSData).
 *  - ScriptingBridge: SBElementArray (lista de elementos de um objeto roteirizado) com as buscas por nome, ID e posição.
 *  - PackageKit (privado): PKReceipt lê os recibos de /var/db/receipts (o instalador do mOS grava o mesmo formato do
 *    macOS) e PKBOM lê o Bom ("BOMStore") com caminhos, tamanhos e tipos.
 *  - CommerceKit/StoreFoundation (privados, App Store): não há App Store no mOS; a compra termina com erro e a fila de
 *    downloads está vazia. CKDownloadDirectory devolve a pasta de downloads da loja em $TMPDIR, como no macOS.
 *  - login.framework: SACLockScreenImmediate bloqueia a tela (a de login do mOS, sem fechar os apps).
 *  - Os que dependem de serviço que o mOS não tem respondem como o macOS sem ele: Automator (sem motor de ações),
 *    AdServices (sem Apple Ads), Sidecar (sem iPad), Tempo de Uso (sem histórico), MessageUI (sem conta de Mail/SMS),
 *    ExtensionKit (sem extensões de app), EventKitUI e Core Spotlight CSUserQuery (busca pelo mdquery).
 */
#include "foundation.h"
#include "libc.h"
#include "sys.h"

id objc_autorelease(id o);
char *strcasestr(const char *h, const char *n);
id objc_retain(id o);
void objc_release(id o);
const char *sel_getName(SEL s);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_global_queue(long prio, unsigned long flags);
void *dispatch_get_main_queue(void);
void _Block_release(const void *b);
cx_class_t *cx_pb_def(cx_class_t *k, cx_class_t *m, const char *n, const char *super);
void cx_pb_props(cx_class_t *k, const char *const *names);
void cx_pb_inits(cx_class_t *k, const char *const *sels);
void cx_pb_news(cx_class_t *k, const char *const *sels);
id cx_pb_bag(id o);
id cx_pb_err(const char *dom, long code, const char *msg);
static id cls(const char *n) { return (id)cx_class_named(n); }
static id bget(id o, const char *k) { return MSG(id, cx_pb_bag(o), "objectForKey:", cx_str(k)); }
static void bset(id o, const char *k, id v) { if (v) MSG(void, cx_pb_bag(o), "setObject:forKey:", v, cx_str(k)); else MSG(void, cx_pb_bag(o), "removeObjectForKey:", cx_str(k)); }
#define DEF(name) cx_class_t name##_c __asm__("OBJC_CLASS_$_" #name); cx_class_t name##_m __asm__("OBJC_METACLASS_$_" #name);
typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;
#define INVOKE(b, T) ((T)((blk_t *)(b))->invoke)

DEF(LPLinkMetadata) DEF(LPMetadataProvider) DEF(SBElementArray) DEF(PKReceipt) DEF(PKBOM) DEF(AMWorkflow) DEF(AMWorkflowController)
DEF(AAAttribution) DEF(SSPurchase) DEF(SSDownload) DEF(SSDownloadMetadata) DEF(CKPurchaseController) DEF(CKDownloadQueue)
DEF(SidecarDisplayManager) DEF(SidecarDisplayConfig) DEF(SidecarDevice) DEF(STWebHistory) DEF(MFMailComposeViewController)
DEF(MFMessageComposeViewController) DEF(EXHostViewController) DEF(EXAppExtensionBrowserViewController) DEF(EKEventEditViewController)
DEF(CSUserQuery) DEF(CSUserQueryContext)

/* ================================================================ LinkPresentation */
enum { LPErrorMetadataFetchFailed = 2, LPErrorMetadataFetchCancelled = 3 };
static size_t utf8_put(char *o, u32 c) {
    if (c < 0x80) { o[0] = (char)c; return 1; }
    if (c < 0x800) { o[0] = (char)(0xc0 | c >> 6); o[1] = (char)(0x80 | (c & 63)); return 2; }
    if (c < 0x10000) { o[0] = (char)(0xe0 | c >> 12); o[1] = (char)(0x80 | ((c >> 6) & 63)); o[2] = (char)(0x80 | (c & 63)); return 3; }
    o[0] = (char)(0xf0 | c >> 18); o[1] = (char)(0x80 | ((c >> 12) & 63)); o[2] = (char)(0x80 | ((c >> 6) & 63)); o[3] = (char)(0x80 | (c & 63));
    return 4;
}
/* entidades HTML: numéricas, as básicas e as letras acentuadas do Latin-1 (o texto só encolhe: cabe no lugar) */
static void html_decode(char *s) {
    static const struct { const char *n; u32 c; } ents[] = {
        {"amp", '&'}, {"quot", '"'}, {"apos", '\''}, {"lt", '<'}, {"gt", '>'}, {"nbsp", ' '}, {"copy", 0xa9}, {"reg", 0xae},
        {"hellip", 0x2026}, {"mdash", 0x2014}, {"ndash", 0x2013}, {"lsquo", 0x2018}, {"rsquo", 0x2019}, {"ldquo", 0x201c},
        {"rdquo", 0x201d}, {"laquo", 0xab}, {"raquo", 0xbb}, {"middot", 0xb7}, {"euro", 0x20ac}, {"ordm", 0xba}, {"ordf", 0xaa}};
    static const char vowels[] = "aeiouAEIOU";
    char *o = s;
    for (char *p = s; *p;) {
        if (*p == '&') {
            char *e = strchr(p, ';');
            if (e && e - p <= 10) {
                char nm[12];
                size_t l = (size_t)(e - p - 1);
                memcpy(nm, p + 1, l); nm[l] = 0;
                u32 cp = 0;
                if (nm[0] == '#') cp = (u32)strtol(nm + 1 + (nm[1] == 'x' || nm[1] == 'X'), 0, nm[1] == 'x' || nm[1] == 'X' ? 16 : 10);
                for (size_t i = 0; !cp && i < sizeof ents / sizeof ents[0]; i++) if (!strcmp(nm, ents[i].n)) cp = ents[i].c;
                if (!cp && l >= 4) {                                             /* aacute, ccedil, atilde, acirc, agrave, auml */
                    static const struct { const char *suf; u32 lo[5], up[5]; } acc[] = {
                        {"acute", {0xe1, 0xe9, 0xed, 0xf3, 0xfa}, {0xc1, 0xc9, 0xcd, 0xd3, 0xda}},
                        {"grave", {0xe0, 0xe8, 0xec, 0xf2, 0xf9}, {0xc0, 0xc8, 0xcc, 0xd2, 0xd9}},
                        {"circ", {0xe2, 0xea, 0xee, 0xf4, 0xfb}, {0xc2, 0xca, 0xce, 0xd4, 0xdb}},
                        {"uml", {0xe4, 0xeb, 0xef, 0xf6, 0xfc}, {0xc4, 0xcb, 0xcf, 0xd6, 0xdc}}};
                    const char *v = strchr(vowels, nm[0]);
                    for (int k = 0; v && !cp && k < 4; k++)
                        if (!strcmp(nm + 1, acc[k].suf)) { int i = (int)(v - vowels); cp = i < 5 ? acc[k].lo[i] : acc[k].up[i - 5]; }
                    if (!cp && !strcmp(nm + 1, "tilde")) cp = nm[0] == 'a' ? 0xe3 : nm[0] == 'o' ? 0xf5 : nm[0] == 'n' ? 0xf1 : nm[0] == 'A' ? 0xc3 : nm[0] == 'O' ? 0xd5 : nm[0] == 'N' ? 0xd1 : 0;
                    if (!cp && !strcmp(nm + 1, "cedil")) cp = nm[0] == 'c' ? 0xe7 : nm[0] == 'C' ? 0xc7 : 0;
                }
                if (cp && cp < 0x110000 && (size_t)(e - p + 1) >= (cp < 0x80 ? 1u : cp < 0x800 ? 2u : cp < 0x10000 ? 3u : 4u)) {
                    o += utf8_put(o, cp);
                    p = e + 1;
                    continue;
                }
            }
        }
        *o++ = (*p == '\n' || *p == '\r' || *p == '\t') ? ' ' : *p;
        p++;
    }
    *o = 0;
}
/* valor de um atributo dentro de uma tag ("<meta property=... content=...>") */
static int tag_attr(const char *tag, const char *end, const char *name, char *out, size_t cap) {
    size_t nl = strlen(name);
    for (const char *p = tag; p + nl < end; p++) {
        if (strncasecmp(p, name, nl) || (p > tag && p[-1] != ' ' && p[-1] != '\t' && p[-1] != '\n')) continue;
        const char *q = p + nl;
        while (q < end && (*q == ' ')) q++;
        if (q >= end || *q != '=') continue;
        q++;
        while (q < end && *q == ' ') q++;
        char quote = (*q == '"' || *q == '\'') ? *q++ : 0;
        size_t o = 0;
        while (q < end && (quote ? *q != quote : (*q != ' ' && *q != '>')) && o + 1 < cap) out[o++] = *q++;
        out[o] = 0;
        html_decode(out);
        return 1;
    }
    return 0;
}
typedef struct { char title[512], image[1024], video[1024], icon[1024], url[1024]; } page_t;
static void parse_page(const char *h, size_t n, page_t *pg) {
    memset(pg, 0, sizeof *pg);
    const char *end = h + n;
    char og_title[512] = "", tw_title[512] = "";
    for (const char *p = h; p < end; p++) {
        if (*p != '<') continue;
        const char *te = memchr(p, '>', (size_t)(end - p));
        if (!te) break;
        if (!strncasecmp(p, "<title", 6) && !pg->title[0]) {
            const char *s = te + 1, *e = s;
            while (e + 8 <= end && strncasecmp(e, "</title>", 8)) e++;
            size_t l = (size_t)(e - s) < sizeof pg->title - 1 ? (size_t)(e - s) : sizeof pg->title - 1;
            memcpy(pg->title, s, l); pg->title[l] = 0;
            html_decode(pg->title);
        } else if (!strncasecmp(p, "<meta", 5)) {
            char key[96] = "", val[1024] = "";
            if (!tag_attr(p, te, "property", key, sizeof key)) tag_attr(p, te, "name", key, sizeof key);
            if (key[0] && tag_attr(p, te, "content", val, sizeof val)) {
                if (!strcasecmp(key, "og:title")) snprintf(og_title, sizeof og_title, "%s", val);
                else if (!strcasecmp(key, "twitter:title")) snprintf(tw_title, sizeof tw_title, "%s", val);
                else if ((!strcasecmp(key, "og:image") || !strcasecmp(key, "og:image:url") || !strcasecmp(key, "twitter:image")) && !pg->image[0]) snprintf(pg->image, sizeof pg->image, "%s", val);
                else if ((!strcasecmp(key, "og:video") || !strcasecmp(key, "og:video:url")) && !pg->video[0]) snprintf(pg->video, sizeof pg->video, "%s", val);
                else if (!strcasecmp(key, "og:url")) snprintf(pg->url, sizeof pg->url, "%s", val);
            }
        } else if (!strncasecmp(p, "<link", 5)) {
            char rel[96] = "", href[1024] = "";
            if (tag_attr(p, te, "rel", rel, sizeof rel) && tag_attr(p, te, "href", href, sizeof href)) {
                int apple = strcasestr(rel, "apple-touch-icon") != 0, icon = strcasestr(rel, "icon") != 0;
                if (icon && (apple || !pg->icon[0])) snprintf(pg->icon, sizeof pg->icon, "%s", href);
            }
        }
        p = te;
    }
    if (og_title[0]) snprintf(pg->title, sizeof pg->title, "%s", og_title);
    else if (tw_title[0] && !pg->title[0]) snprintf(pg->title, sizeof pg->title, "%s", tw_title);
}
static id abs_url(const char *ref, id base) {
    if (!ref || !ref[0]) return 0;
    id u = MSG(id, cls("NSURL"), "URLWithString:relativeToURL:", cx_str(ref), base);
    return u ? MSG0(id, u, "absoluteURL") : 0;
}
static id provider_for(id url) {                                                 /* baixa e embrulha os bytes */
    id d = url ? MSG(id, cls("NSData"), "dataWithContentsOfURL:", url) : 0;
    return d ? objc_autorelease(MSG(id, MSG0(id, cls("NSItemProvider"), "alloc"), "initWithObject:", d)) : 0;
}
typedef struct { id self, url; void *blk; } lp_job_t;
static void lp_run(void *ctx) {
    lp_job_t *j = ctx;
    id pool = MSG0(id, MSG0(id, cls("NSAutoreleasePool"), "alloc"), "init");
    id md = objc_autorelease(MSG0(id, MSG0(id, cls("LPLinkMetadata"), "alloc"), "init")), err = 0;
    bset(md, "originalURL", j->url);
    bset(md, "URL", j->url);
    if (MSG0(BOOL, j->url, "isFileURL")) {                                       /* arquivo local: o nome */
        bset(md, "title", MSG0(id, j->url, "lastPathComponent"));
    } else {
        id data = MSG(id, cls("NSData"), "dataWithContentsOfURL:", j->url);
        if (bget(j->self, "_cancelled")) err = cx_pb_err("LPErrorDomain", LPErrorMetadataFetchCancelled, "busca cancelada");
        else if (!data) err = cx_pb_err("LPErrorDomain", LPErrorMetadataFetchFailed, "não foi possível baixar a página");
        else {
            size_t n = (size_t)MSG0(u64, data, "length");
            page_t *pg = malloc(sizeof *pg);
            parse_page(MSG0(const char *, data, "bytes"), n < (4u << 20) ? n : (4u << 20), pg);
            if (pg->title[0]) bset(md, "title", cx_str(pg->title));
            if (pg->url[0]) { id u = abs_url(pg->url, j->url); if (u) bset(md, "URL", u); }
            if (pg->video[0]) bset(md, "remoteVideoURL", abs_url(pg->video, j->url));
            id fetch = bget(j->self, "shouldFetchSubresources");
            if (!fetch || MSG0(BOOL, fetch, "boolValue")) {
                if (pg->image[0]) bset(md, "imageProvider", provider_for(abs_url(pg->image, j->url)));
                id icon = abs_url(pg->icon[0] ? pg->icon : "/favicon.ico", j->url);
                bset(md, "iconProvider", provider_for(icon));
            }
            free(pg);
        }
    }
    if (j->blk) INVOKE(j->blk, void (*)(void *, id, id))(j->blk, err ? 0 : md, err);
    if (j->blk) _Block_release(j->blk);
    objc_release(j->url);
    objc_release(j->self);
    free(j);
    MSG0(void, pool, "drain");
}
static void lp_start(id s, SEL c, id url, void *blk) {
    (void)c;
    bset(s, "_started", cx_num_i(1));
    lp_job_t *j = calloc(1, sizeof *j);
    j->self = objc_retain(s); j->url = objc_retain(url); j->blk = _Block_copy(blk);
    dispatch_async_f(dispatch_get_global_queue(0, 0), j, lp_run);
}
static void lp_start_req(id s, SEL c, id req, void *blk) { lp_start(s, c, MSG0(id, req, "URL"), blk); }
static void lp_cancel(id s, SEL c) { (void)c; bset(s, "_cancelled", cx_num_i(1)); }
static id lp_copy(id s, SEL c, void *zone) {
    (void)c; (void)zone;
    id o = MSG0(id, MSG0(id, (id)object_getClass(s), "alloc"), "init");
    MSG(void, cx_pb_bag(o), "addEntriesFromDictionary:", cx_pb_bag(s));
    return o;
}

/* ================================================================ ScriptingBridge: SBElementArray */
static id sb_match(id s, const char *key, id want) {
    u64 n = MSG0(u64, s, "count");
    SEL k = sel_registerName(key);
    for (u64 i = 0; i < n; i++) {
        id o = MSG(id, s, "objectAtIndex:", i);
        if (!o || !cx_responds(object_getClass(o), k)) continue;
        id v = MSG0(id, o, key);
        if (v && MSG(BOOL, v, "isEqual:", want)) return o;
    }
    return 0;
}
static id sb_with_name(id s, SEL c, id name) { (void)c; return sb_match(s, "name", name); }
static id sb_with_id(id s, SEL c, id ident) { (void)c; return sb_match(s, "id", ident); }
static id sb_at_location(id s, SEL c, id loc) {
    (void)c;
    if (!loc) return 0;
    if (cx_is_str(loc)) return sb_match(s, "name", loc);
    long i = MSG0(long, loc, "integerValue");                                    /* posição como no AppleScript: 1 = primeiro, -1 = último */
    long n = (long)MSG0(u64, s, "count");
    if (i < 0) i = n + i + 1;
    return i >= 1 && i <= n ? MSG(id, s, "objectAtIndex:", (u64)(i - 1)) : 0;
}
static id sb_apply(id s, SEL c, SEL sel, id arg) {
    id out = MSG0(id, cls("NSMutableArray"), "array");
    int with = strstr(sel_getName(c), "withObject:") != 0;
    u64 n = MSG0(u64, s, "count");
    for (u64 i = 0; i < n; i++) {
        id o = MSG(id, s, "objectAtIndex:", i);
        id v = with ? ((id (*)(id, SEL, id))objc_msgSend)(o, sel, arg) : ((id (*)(id, SEL))objc_msgSend)(o, sel);
        MSG(void, out, "addObject:", v ? v : MSG0(id, cls("NSNull"), "null"));
    }
    return out;
}
static id sb_get(id s, SEL c) { (void)c; return MSG(id, cls("NSArray"), "arrayWithArray:", s); }

/* ================================================================ PackageKit: Bom e recibos */
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
typedef struct { u32 id, parent; u8 type; u64 size; char name[256]; } bnode_t;
typedef struct { bnode_t *v; int n; } bom_t;
static int bom_blk(const u8 *b, size_t n, u32 idx, const u8 **p, u32 *len) {
    if (n < 32) return -1;
    u32 io = be32(b + 16);
    if ((u64)io + 4 > n) return -1;
    u32 cnt = be32(b + io);
    if (idx >= cnt || (u64)io + 4 + 8ull * (idx + 1) > n) return -1;
    u32 a = be32(b + io + 4 + 8 * idx), l = be32(b + io + 8 + 8 * idx);
    if ((u64)a + l > n) return -1;
    *p = b + a; *len = l;
    return 0;
}
/* árvore "Paths": folhas encadeadas; cada par = (BOMPathInfo1 {id, info2}, BOMFile {pai, nome}) */
static int bom_load(const u8 *b, size_t n, bom_t *out) {
    memset(out, 0, sizeof *out);
    if (n < 32 || memcmp(b, "BOMStore", 8)) return -1;
    u32 vo = be32(b + 24), tree = 0xffffffffu;
    if ((u64)vo + 4 > n) return -1;
    u32 nv = be32(b + vo);
    size_t o = vo + 4;
    for (u32 i = 0; i < nv && o + 5 <= n; i++) {
        u8 ln = b[o + 4];
        if (o + 5 + ln > n) break;
        if (ln == 5 && !memcmp(b + o + 5, "Paths", 5)) tree = be32(b + o);
        o += 5u + ln;
    }
    const u8 *t, *pg;
    u32 tl, pl;
    if (tree == 0xffffffffu || bom_blk(b, n, tree, &t, &tl) || tl < 21 || memcmp(t, "tree", 4)) return -1;
    u32 node = be32(t + 8);
    for (int g = 0; g < 64 && !bom_blk(b, n, node, &pg, &pl) && pl >= 12 && !be16(pg) && be16(pg + 2); g++) node = be32(pg + 12);
    int cap = 0;
    for (int pages = 0; pages < 1000000 && node && !bom_blk(b, n, node, &pg, &pl) && pl >= 12; pages++) {
        u16 cnt = be16(pg + 2);
        for (u16 i = 0; i < cnt && 12 + 8u * (i + 1) <= pl; i++) {
            const u8 *vp, *kp, *ip;
            u32 vl, kl, il;
            if (bom_blk(b, n, be32(pg + 12 + 8 * i), &vp, &vl) || bom_blk(b, n, be32(pg + 16 + 8 * i), &kp, &kl) || vl < 8 || kl < 5) continue;
            if (out->n == cap) { cap = cap ? cap * 2 : 256; bnode_t *x = realloc(out->v, sizeof *x * (size_t)cap); if (!x) return -1; out->v = x; }
            bnode_t *d = &out->v[out->n++];
            memset(d, 0, sizeof *d);
            d->id = be32(vp);
            d->parent = be32(kp);
            u32 k = kl - 4 < 255 ? kl - 4 : 255;
            memcpy(d->name, kp + 4, k);
            d->name[k] = 0;
            if (!bom_blk(b, n, be32(vp + 4), &ip, &il) && il >= 23) { d->type = ip[0]; d->size = be32(ip + 18); }
        }
        node = be32(pg + 4);                                                     /* forward */
    }
    return 0;
}
static void bom_path(const bom_t *bm, int i, char *out, size_t cap) {
    const char *parts[64];
    int np = 0;
    for (int cur = i, guard = 0; cur >= 0 && guard < 64; guard++) {
        parts[np++] = bm->v[cur].name;
        u32 par = bm->v[cur].parent;
        cur = -1;
        if (par) for (int k = 0; k < bm->n; k++) if (bm->v[k].id == par) { cur = k; break; }
    }
    size_t o = 0;
    out[0] = 0;
    for (int k = np - 1; k >= 0; k--) {
        if (!strcmp(parts[k], ".")) continue;
        o += (size_t)snprintf(out + o, o < cap ? cap - o : 0, "%s%s", o ? "/" : "", parts[k]);
    }
}
/* PKBOM: guarda os caminhos (relativos, sem "./") e os tamanhos */
static id bom_init_data(id s, SEL c, id data) {
    (void)c;
    bom_t bm;
    if (!data || bom_load(MSG0(const u8 *, data, "bytes"), (size_t)MSG0(u64, data, "length"), &bm)) { objc_release(s); return 0; }
    id paths = MSG0(id, cls("NSMutableArray"), "array"), attrs = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    u64 total = 0, files = 0;
    char p[1024];
    for (int i = 0; i < bm.n; i++) {
        bom_path(&bm, i, p, sizeof p);
        if (!p[0]) continue;
        MSG(void, paths, "addObject:", cx_str(p));
        id a = MSG0(id, cls("NSMutableDictionary"), "dictionary");
        static const char *const types[] = {"NSFileTypeUnknown", "NSFileTypeRegular", "NSFileTypeDirectory", "NSFileTypeSymbolicLink", "NSFileTypeCharacterSpecial"};
        MSG(void, a, "setObject:forKey:", cx_str(types[bm.v[i].type < 5 ? bm.v[i].type : 0]), cx_str("NSFileType"));
        MSG(void, a, "setObject:forKey:", cx_num_i((i64)bm.v[i].size), cx_str("NSFileSize"));
        MSG(void, attrs, "setObject:forKey:", a, cx_str(p));
        if (bm.v[i].type == 1) total += bm.v[i].size;
        files++;
    }
    free(bm.v);
    bset(s, "_paths", paths);
    bset(s, "_attrs", attrs);
    bset(s, "_total", cx_num_i((i64)total));
    bset(s, "_count", cx_num_i((i64)files));
    return s;
}
static id bom_init_path(id s, SEL c, id path) {
    id d = path ? MSG(id, cls("NSData"), "dataWithContentsOfFile:", path) : 0;
    return bom_init_data(s, c, d);
}
static u64 bom_total(id s, SEL c) { (void)c; id v = bget(s, "_total"); return v ? MSG0(u64, v, "unsignedLongLongValue") : 0; }
static u64 bom_count(id s, SEL c) { (void)c; id v = bget(s, "_count"); return v ? MSG0(u64, v, "unsignedLongLongValue") : 0; }
static id bom_enum(id s, SEL c) { (void)c; return MSG0(id, bget(s, "_paths"), "objectEnumerator"); }
static id bom_attrs(id s, SEL c, id p) { (void)c; return MSG(id, bget(s, "_attrs"), "objectForKey:", p); }
static id bom_subpaths(id s, SEL c, id dir) {
    (void)c;
    id out = MSG0(id, cls("NSMutableArray"), "array"), all = bget(s, "_paths");
    const char *d = dir ? cx_cstr(dir) : "";
    while (*d == '.' || *d == '/') d++;
    size_t dl = strlen(d);
    for (u64 i = 0; i < MSG0(u64, all, "count"); i++) {
        id p = MSG(id, all, "objectAtIndex:", i);
        const char *q = cx_cstr(p);
        if (!dl) MSG(void, out, "addObject:", p);
        else if (!strncmp(q, d, dl) && q[dl] == '/') MSG(void, out, "addObject:", cx_str(q + dl + 1));
    }
    return out;
}
/* PKReceipt: o dicionário do .plist + o caminho do .bom ao lado */
static id rc_from_plist(id plist_path) {
    id d = MSG(id, cls("NSDictionary"), "dictionaryWithContentsOfFile:", plist_path);
    if (!d) return 0;
    id r = objc_autorelease(MSG0(id, MSG0(id, cls("PKReceipt"), "alloc"), "init"));
    bset(r, "_dict", d);
    bset(r, "_plist", plist_path);
    const char *p = cx_cstr(plist_path);
    size_t l = strlen(p);
    char bomp[1100];
    snprintf(bomp, sizeof bomp, "%.*s.bom", (int)(l > 6 ? l - 6 : l), p);
    bset(r, "_bom", cx_str(bomp));
    return r;
}
static id rc_dir(id vol) {
    const char *v = vol && cx_is_str(vol) && cx_cstr(vol)[0] ? cx_cstr(vol) : "/";
    char d[1100];
    snprintf(d, sizeof d, "%s%svar/db/receipts", v, v[strlen(v) - 1] == '/' ? "" : "/");
    return cx_str(d);
}
static id rc_all(id dir, id prefix, int exact) {
    id out = MSG0(id, cls("NSMutableArray"), "array");
    id fm = MSG0(id, cls("NSFileManager"), "defaultManager");
    id names = MSG(id, fm, "contentsOfDirectoryAtPath:error:", dir, (id *)0);
    names = names ? MSG(id, names, "sortedArrayUsingSelector:", sel_registerName("compare:")) : 0;
    for (u64 i = 0; names && i < MSG0(u64, names, "count"); i++) {
        id nm = MSG(id, names, "objectAtIndex:", i);
        if (!MSG(BOOL, nm, "hasSuffix:", cx_str(".plist"))) continue;
        id r = rc_from_plist(MSG(id, dir, "stringByAppendingPathComponent:", nm));
        if (!r) continue;
        id ident = MSG(id, bget(r, "_dict"), "objectForKey:", cx_str("PackageIdentifier"));
        if (prefix && (!ident || (exact ? !MSG(BOOL, ident, "isEqualToString:", prefix) : !MSG(BOOL, ident, "hasPrefix:", prefix)))) continue;
        MSG(void, out, "addObject:", r);
    }
    return out;
}
static id rc_on_volume(id k, SEL c, id vol) { (void)k; (void)c; return rc_all(rc_dir(vol), 0, 0); }
static id rc_on_volume_matching(id k, SEL c, id vol, id ident) {
    (void)k; (void)c;
    if (ident && cx_is_str(ident) && strchr(cx_cstr(ident), '*')) {          /* "com.x.*": prefixo */
        const char *s = cx_cstr(ident);
        return rc_all(rc_dir(vol), cx_str_n(s, (u64)(strchr(s, '*') - s)), 0);
    }
    return rc_all(rc_dir(vol), ident, 1);
}
static id rc_at_path(id k, SEL c, id dir) { (void)k; (void)c; return rc_all(dir, 0, 0); }
static id rc_with_ident(id k, SEL c, id ident, id vol) {
    (void)k; (void)c;
    id a = rc_all(rc_dir(vol), ident, 1);
    return MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)0) : 0;
}
static id rc_with_file(id k, SEL c, id file, id vol) {
    (void)k; (void)c;
    id a = rc_all(rc_dir(vol), 0, 0);
    for (u64 i = 0; i < MSG0(u64, a, "count"); i++) {
        id r = MSG(id, a, "objectAtIndex:", i), f = MSG(id, bget(r, "_dict"), "objectForKey:", cx_str("PackageFileName"));
        if (f && MSG(BOOL, f, "isEqualToString:", file)) return r;
    }
    return 0;
}
static id rc_key(id s, const char *k) { id d = bget(s, "_dict"); return d ? MSG(id, d, "objectForKey:", cx_str(k)) : 0; }
static id rc_ident(id s, SEL c) { (void)c; return rc_key(s, "PackageIdentifier"); }
static id rc_version(id s, SEL c) { (void)c; return rc_key(s, "PackageVersion"); }
static id rc_date(id s, SEL c) { (void)c; return rc_key(s, "InstallDate"); }
static id rc_prefix(id s, SEL c) { (void)c; return rc_key(s, "InstallPrefixPath"); }
static id rc_process(id s, SEL c) { (void)c; return rc_key(s, "InstallProcessName"); }
static id rc_name(id s, SEL c) { (void)c; return rc_key(s, "PackageFileName"); }
static id rc_info(id s, SEL c) { (void)c; return rc_key(s, "AdditionalInfo"); }
static id rc_groups(id s, SEL c) { (void)c; id g = rc_key(s, "PackageGroups"); return g ? g : MSG0(id, cls("NSArray"), "array"); }
static BOOL rc_secure(id s, SEL c) { (void)s; (void)c; return 0; }
static id rc_storage(id s, SEL c) {
    (void)c;
    id out = MSG0(id, cls("NSMutableArray"), "array"), fm = MSG0(id, cls("NSFileManager"), "defaultManager");
    const char *keys[] = {"_plist", "_bom"};
    for (int i = 0; i < 2; i++) { id p = bget(s, keys[i]); if (p && MSG(BOOL, fm, "fileExistsAtPath:", p)) MSG(void, out, "addObject:", p); }
    return out;
}
static id rc_bom(id s, SEL c) {
    (void)c;
    id b = bget(s, "_bomobj");
    if (!b) {
        b = MSG(id, MSG0(id, cls("PKBOM"), "alloc"), "initWithBOMPath:", bget(s, "_bom"));
        if (b) { bset(s, "_bomobj", b); objc_release(b); }
    }
    return b;
}
static id rc_enum(id s, SEL c) { id b = rc_bom(s, c); return b ? MSG0(id, b, "directoryEnumerator") : MSG0(id, MSG0(id, cls("NSArray"), "array"), "objectEnumerator"); }
static id rc_init_bom(id s, SEL c, id bomp) {
    (void)c;
    const char *p = bomp ? cx_cstr(bomp) : "";
    size_t l = strlen(p);
    char pl[1100];
    snprintf(pl, sizeof pl, "%.*s.plist", (int)(l > 4 ? l - 4 : l), p);
    id d = MSG(id, cls("NSDictionary"), "dictionaryWithContentsOfFile:", cx_str(pl));
    if (d) bset(s, "_dict", d);
    bset(s, "_plist", cx_str(pl));
    bset(s, "_bom", bomp);
    return s;
}
static BOOL rc_remove(id s, SEL c, id dir, id *err) {
    (void)c; (void)dir;
    if (err) *err = 0;
    id fm = MSG0(id, cls("NSFileManager"), "defaultManager"), paths = rc_storage(s, 0);
    BOOL ok = 1;
    for (u64 i = 0; i < MSG0(u64, paths, "count"); i++) { id one = MSG(id, paths, "objectAtIndex:", i); ok &= MSG(BOOL, fm, "removeItemAtPath:error:", one, err); }
    return ok;
}
static void rc_noop(id k, SEL c) { (void)k; (void)c; }

/* ================================================================ App Store (CommerceKit, StoreFoundation) */
id CKDownloadDirectory(id target) {
    const char *t = getenv("TMPDIR");
    char p[1100];
    snprintf(p, sizeof p, "%s%scom.apple.appstore", t ? t : "/tmp/", t && t[strlen(t) - 1] != '/' ? "/" : "");
    id base = cx_str(p);
    return target ? MSG(id, base, "stringByAppendingPathComponent:", target) : base;
}
static id g_purchase, g_queue;
static id ck_shared_purchase(id k, SEL c) { (void)c; if (!g_purchase) g_purchase = MSG0(id, MSG0(id, k, "alloc"), "init"); return g_purchase; }
static id ck_shared_queue(id k, SEL c) { (void)c; if (!g_queue) g_queue = MSG0(id, MSG0(id, k, "alloc"), "init"); return g_queue; }
static void ck_perform(id s, SEL c, id purchase, u64 opts, void *blk) {
    (void)s; (void)c; (void)opts;
    if (blk) INVOKE(blk, void (*)(void *, id, BOOL, id, id))(blk, purchase, 0, cx_pb_err("com.apple.commerce.client", 500, "o mOS não tem a App Store"), 0);
}
static id ck_empty(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSArray"), "array"); }
static void ck_observer(id s, SEL c, id o) { (void)s; (void)c; (void)o; }
static id ck_add_observer(id s, SEL c, id o) { (void)s; (void)c; return o; }

/* ================================================================ login.framework */
int SACLockScreenImmediate(void) { return SYS2(SYS_DESKTOP, MOS_DESK_LOCK, 0) < 0 ? -1 : 0; }

/* ================================================================ serviços ausentes */
static id no_obj_err(id k, SEL c, id *err) { (void)k; (void)c; if (err) *err = cx_pb_err("com.apple.AdServices.AAAttributionErrorDomain", 3 /* PlatformNotSupported */, "o mOS não tem Apple Ads"); return 0; }
static id am_run(id k, SEL c, id url, id input, id *err) {
    (void)k; (void)c; (void)url; (void)input;
    if (err) *err = cx_pb_err("com.apple.Automator", -212, "o mOS não tem o motor de ações do Automator");
    return 0;
}
static id am_init_url(id s, SEL c, id url, id *err) { objc_release(s); return am_run(0, c, url, 0, err); }
static void am_ctl_run(id s, SEL c, id sender) {
    (void)c; (void)sender;
    id d = bget(s, "delegate");
    if (d && cx_responds(object_getClass(d), sel_registerName("workflowController:didError:")))
        MSG(void, d, "workflowController:didError:", s, cx_pb_err("com.apple.Automator", -212, "o mOS não tem o motor de ações do Automator"));
}
static BOOL am_running(id s, SEL c) { (void)s; (void)c; return 0; }
static id sc_shared(id k, SEL c) { (void)c; static id m; if (!m) m = MSG0(id, MSG0(id, k, "alloc"), "init"); return m; }
static void sc_connect(id s, SEL c, id dev, void *blk) {
    (void)s; (void)c; (void)dev;
    if (blk) INVOKE(blk, void (*)(void *, id))(blk, cx_pb_err("SidecarErrorDomain", 1, "não há iPad para o Sidecar no mOS"));
}
static void sc_connect_cfg(id s, SEL c, id dev, id cfg, void *blk) { (void)cfg; sc_connect(s, c, dev, blk); }
static id st_init(id s, SEL c, id bundle, id *err) { (void)c; if (err) *err = 0; bset(s, "bundleIdentifier", bundle); return s; }
static void st_noop1(id s, SEL c, id a) { (void)s; (void)c; (void)a; }
static void st_noop0(id s, SEL c) { (void)s; (void)c; }
static void st_fetch(id s, SEL c, id interval, void *blk) {
    (void)s; (void)c; (void)interval;
    if (blk) INVOKE(blk, void (*)(void *, id, id))(blk, MSG0(id, cls("NSSet"), "set"), 0);
}
static BOOL no0(id s, SEL c) { (void)s; (void)c; return 0; }
/* CSUserQuery: a busca do usuário pelo índice do mdquery (kMDItemTextContent/DisplayName) */
id MDQueryCreate(void *alloc, id query, id valueListAttrs, id sortingAttrs);
u8 MDQueryExecute(id q, u64 options);
long MDQueryGetResultCount(id q);
void *MDQueryGetResultAtIndex(id q, long i);
id MDItemCopyAttribute(void *item, id name);
void *_Block_copy(const void *b);
/* blocos guardados copiados (o app pode passar um bloco da pilha) */
static void uq_set_found(id s, SEL c, void *b) { (void)c; id v = b ? (id)_Block_copy(b) : 0; bset(s, "foundItemsHandler", v); if (v) objc_release(v); }
static void uq_set_done(id s, SEL c, void *b) { (void)c; id v = b ? (id)_Block_copy(b) : 0; bset(s, "completionHandler", v); if (v) objc_release(v); }
static id uq_init(id s, SEL c, id text, id ctx) { (void)c; bset(s, "queryString", text); bset(s, "queryContext", ctx); return s; }
static void uq_start(id s, SEL c) {
    (void)c;
    const char *t = bget(s, "queryString") ? cx_cstr(bget(s, "queryString")) : "";
    char q[600];
    snprintf(q, sizeof q, "kMDItemDisplayName == \"*%s*\"cd || kMDItemTextContent == \"%s\"cd", t, t);
    id mq = MDQueryCreate(0, cx_str(q), 0, 0);
    id items = MSG0(id, cls("NSMutableArray"), "array");
    if (mq && MDQueryExecute(mq, 1 /* kMDQuerySynchronous */)) {
        long n = MDQueryGetResultCount(mq);
        for (long i = 0; i < n; i++) {
            id path = MDItemCopyAttribute(MDQueryGetResultAtIndex(mq, i), cx_str("kMDItemPath"));
            if (path) { MSG(void, items, "addObject:", path); objc_release(path); }
        }
    }
    if (mq) objc_release(mq);
    bset(s, "foundItems", items);
    void *h = bget(s, "foundItemsHandler");
    if (h) INVOKE(h, void (*)(void *, id))(h, items);
    void *done = bget(s, "completionHandler");
    if (done) INVOKE(done, void (*)(void *, id))(done, 0);
}

void cx_small2_init(void) {
    cx_class_t *k;
    k = cx_pb_def(&LPLinkMetadata_c, &LPLinkMetadata_m, "LPLinkMetadata", "NSObject");
    cx_pb_props(k, (const char *const[]){"URL", "originalURL", "title", "iconProvider", "imageProvider", "videoProvider", "remoteVideoURL", 0});
    cx_add_method(k, "copyWithZone:", (void *)lp_copy);
    k = cx_pb_def(&LPMetadataProvider_c, &LPMetadataProvider_m, "LPMetadataProvider", "NSObject");
    cx_pb_props(k, (const char *const[]){"#timeout", "?shouldFetchSubresources", 0});
    cx_add_method(k, "startFetchingMetadataForURL:completionHandler:", (void *)lp_start);
    cx_add_method(k, "startFetchingMetadataForRequest:completionHandler:", (void *)lp_start_req);
    cx_add_method(k, "cancel", (void *)lp_cancel);

    cx_class_t *arr = cx_class_named("NSMutableArray");
    if (!cx_class_named("SBElementArray")) cx_define_class(&SBElementArray_c, &SBElementArray_m, "SBElementArray", arr, 0);
    k = &SBElementArray_c;
    cx_add_method(k, "objectWithName:", (void *)sb_with_name);
    cx_add_method(k, "objectWithID:", (void *)sb_with_id);
    cx_add_method(k, "objectAtLocation:", (void *)sb_at_location);
    cx_add_method(k, "arrayByApplyingSelector:", (void *)sb_apply);
    cx_add_method(k, "arrayByApplyingSelector:withObject:", (void *)sb_apply);
    cx_add_method(k, "get", (void *)sb_get);

    k = cx_pb_def(&PKBOM_c, &PKBOM_m, "PKBOM", "NSObject");
    cx_add_method(k, "initWithBOMPath:", (void *)bom_init_path);
    cx_add_method(k, "initWithBOMData:", (void *)bom_init_data);
    cx_add_method(k, "totalSize", (void *)bom_total);
    cx_add_method(k, "fileCount", (void *)bom_count);
    cx_add_method(k, "directoryEnumerator", (void *)bom_enum);
    cx_add_method(k, "subpathsOfDirectoryAtPath:", (void *)bom_subpaths);
    cx_add_method(k, "attributesOfItemAtPath:", (void *)bom_attrs);
    k = cx_pb_def(&PKReceipt_c, &PKReceipt_m, "PKReceipt", "NSObject");
    cx_add_cmethod(k, "receiptsOnVolumeAtPath:", (void *)rc_on_volume);
    cx_add_cmethod(k, "receiptsOnVolumeAtPath:matchingPackageIdentifier:", (void *)rc_on_volume_matching);
    cx_add_cmethod(k, "receiptsAtPath:", (void *)rc_at_path);
    cx_add_cmethod(k, "receiptWithIdentifier:volume:", (void *)rc_with_ident);
    cx_add_cmethod(k, "receiptWithPackageFileName:volume:", (void *)rc_with_file);
    cx_add_cmethod(k, "_clearCache", (void *)rc_noop);
    cx_add_method(k, "initWithBOMPath:", (void *)rc_init_bom);
    cx_add_method(k, "packageIdentifier", (void *)rc_ident);
    cx_add_method(k, "packageVersion", (void *)rc_version);
    cx_add_method(k, "installDate", (void *)rc_date);
    cx_add_method(k, "installPrefixPath", (void *)rc_prefix);
    cx_add_method(k, "installProcessName", (void *)rc_process);
    cx_add_method(k, "additionalInfo", (void *)rc_info);
    cx_add_method(k, "_packageName", (void *)rc_name);
    cx_add_method(k, "packageGroups", (void *)rc_groups);
    cx_add_method(k, "_isSecure", (void *)rc_secure);
    cx_add_method(k, "receiptStoragePaths", (void *)rc_storage);
    cx_add_method(k, "_BOM", (void *)rc_bom);
    cx_add_method(k, "_directoryEnumerator", (void *)rc_enum);
    cx_add_method(k, "_removeReceiptInDirectory:error:", (void *)rc_remove);

    k = cx_pb_def(&SSDownloadMetadata_c, &SSDownloadMetadata_m, "SSDownloadMetadata", "NSObject");
    cx_pb_props(k, (const char *const[]){"kind", "title", "bundleIdentifier", "bundleVersion", "itemIdentifier", "artistName", "dictionary", 0});
    cx_pb_inits(k, (const char *const[]){"initWithDictionary:", "initWithKind:", 0});
    k = cx_pb_def(&SSPurchase_c, &SSPurchase_m, "SSPurchase", "NSObject");
    cx_pb_props(k, (const char *const[]){"buyParameters", "accountIdentifier", "appleID", "downloadMetadata", "itemIdentifier", "?isUpdate",
                                         "?isRedownload", "?checkPreflightAterPurchase", "#purchaseType", 0});
    k = cx_pb_def(&SSDownload_c, &SSDownload_m, "SSDownload", "NSObject");
    cx_pb_props(k, (const char *const[]){"metadata", "status", "?cancelled", 0});
    cx_pb_inits(k, (const char *const[]){"initWithDownloadMetadata:", 0});
    k = cx_pb_def(&CKPurchaseController_c, &CKPurchaseController_m, "CKPurchaseController", "NSObject");
    cx_add_cmethod(k, "sharedPurchaseController", (void *)ck_shared_purchase);
    cx_add_method(k, "performPurchase:withOptions:completionHandler:", (void *)ck_perform);
    k = cx_pb_def(&CKDownloadQueue_c, &CKDownloadQueue_m, "CKDownloadQueue", "NSObject");
    cx_add_cmethod(k, "sharedDownloadQueue", (void *)ck_shared_queue);
    cx_add_method(k, "downloads", (void *)ck_empty);
    cx_add_method(k, "addObserver:", (void *)ck_add_observer);
    cx_add_method(k, "removeObserver:", (void *)ck_observer);

    k = cx_pb_def(&AAAttribution_c, &AAAttribution_m, "AAAttribution", "NSObject");
    cx_add_cmethod(k, "attributionTokenWithError:", (void *)no_obj_err);
    k = cx_pb_def(&AMWorkflow_c, &AMWorkflow_m, "AMWorkflow", "NSObject");
    cx_add_cmethod(k, "runWorkflowAtURL:withInput:error:", (void *)am_run);
    cx_add_method(k, "initWithContentsOfURL:error:", (void *)am_init_url);
    cx_pb_props(k, (const char *const[]){"fileURL", "actions", "input", 0});
    k = cx_pb_def(&AMWorkflowController_c, &AMWorkflowController_m, "AMWorkflowController", "NSController");
    cx_pb_props(k, (const char *const[]){"workflow", "workflowView", "delegate", 0});
    cx_add_method(k, "run:", (void *)am_ctl_run);
    cx_add_method(k, "isRunning", (void *)am_running);
    cx_add_method(k, "canRun", (void *)am_running);
    cx_add_method(k, "stop:", (void *)st_noop1);
    k = cx_pb_def(&SidecarDisplayManager_c, &SidecarDisplayManager_m, "SidecarDisplayManager", "NSObject");
    cx_add_cmethod(k, "sharedManager", (void *)sc_shared);
    cx_add_method(k, "devices", (void *)ck_empty);
    cx_add_method(k, "connectedDevices", (void *)ck_empty);
    cx_add_method(k, "connectToDevice:completion:", (void *)sc_connect);
    cx_add_method(k, "connectToDevice:withConfig:completion:", (void *)sc_connect_cfg);
    cx_add_method(k, "disconnectFromDevice:completion:", (void *)sc_connect);
    k = cx_pb_def(&SidecarDisplayConfig_c, &SidecarDisplayConfig_m, "SidecarDisplayConfig", "NSObject");
    cx_pb_props(k, (const char *const[]){"?sidebarEnabled", "?touchBarEnabled", "#sidebarPosition", "#touchBarPosition", 0});
    k = cx_pb_def(&SidecarDevice_c, &SidecarDevice_m, "SidecarDevice", "NSObject");
    cx_pb_props(k, (const char *const[]){"name", "identifier", "model", 0});
    k = cx_pb_def(&STWebHistory_c, &STWebHistory_m, "STWebHistory", "NSObject");
    cx_pb_props(k, (const char *const[]){"bundleIdentifier", 0});
    cx_add_method(k, "initWithBundleIdentifier:error:", (void *)st_init);
    cx_add_method(k, "deleteHistoryForURL:", (void *)st_noop1);
    cx_add_method(k, "deleteHistoryDuringInterval:", (void *)st_noop1);
    cx_add_method(k, "deleteAllHistory", (void *)st_noop0);
    cx_add_method(k, "fetchHistoryDuringInterval:completionHandler:", (void *)st_fetch);
    k = cx_pb_def(&MFMailComposeViewController_c, &MFMailComposeViewController_m, "MFMailComposeViewController", "NSViewController");
    cx_add_cmethod(k, "canSendMail", (void *)no0);
    cx_pb_props(k, (const char *const[]){"mailComposeDelegate", 0});
    k = cx_pb_def(&MFMessageComposeViewController_c, &MFMessageComposeViewController_m, "MFMessageComposeViewController", "NSViewController");
    cx_add_cmethod(k, "canSendText", (void *)no0);
    cx_add_cmethod(k, "canSendSubject", (void *)no0);
    cx_add_cmethod(k, "canSendAttachments", (void *)no0);
    cx_pb_props(k, (const char *const[]){"messageComposeDelegate", "recipients", "body", "subject", 0});
    k = cx_pb_def(&EXHostViewController_c, &EXHostViewController_m, "EXHostViewController", "NSViewController");
    cx_pb_props(k, (const char *const[]){"configuration", "delegate", "placeholderView", 0});
    k = cx_pb_def(&EXAppExtensionBrowserViewController_c, &EXAppExtensionBrowserViewController_m, "EXAppExtensionBrowserViewController", "NSViewController");
    k = cx_pb_def(&EKEventEditViewController_c, &EKEventEditViewController_m, "EKEventEditViewController", "NSViewController");
    cx_pb_props(k, (const char *const[]){"event", "eventStore", "editViewDelegate", 0});
    k = cx_pb_def(&CSUserQueryContext_c, &CSUserQueryContext_m, "CSUserQueryContext", "NSObject");
    cx_pb_props(k, (const char *const[]){"#maxResultCount", "#maxSuggestionCount", "?enableRankedResults", "fetchAttributes", "filterQueries", 0});
    k = cx_pb_def(&CSUserQuery_c, &CSUserQuery_m, "CSUserQuery", "NSObject");
    cx_pb_props(k, (const char *const[]){"queryString", "queryContext", "foundItems", "foundItemsHandler", "completionHandler", 0});
    cx_add_method(k, "setFoundItemsHandler:", (void *)uq_set_found);
    cx_add_method(k, "setCompletionHandler:", (void *)uq_set_done);
    cx_add_method(k, "initWithUserQueryString:userQueryContext:", (void *)uq_init);
    cx_add_method(k, "start", (void *)uq_start);
    cx_add_method(k, "cancel", (void *)st_noop0);
}
