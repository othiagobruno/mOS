/* mOS ImageIO — CGImageMetadata e CGImageMetadataTag (XMP) sobre iio_xmp.c, e a ligação com a fonte (XMP do arquivo
 * mais o EXIF/TIFF/GPS/IPTC em XMP, como o macOS) e com o destino (XMP embutido ao gravar; cópia sem recomprimir com
 * os metadados trocados ou mesclados). Incluído no fim do imageio_cf.inc. */
#include "iio_xmp.h"
#ifndef IIO_CF_HOST
char *strrchr(const char *s, int c);
int snprintf(char *b, size_t n, const char *f, ...);
#else
#include <stdio.h>
#endif

IIOK(kCGImageMetadataNamespaceExif, "http://ns.adobe.com/exif/1.0/")
IIOK(kCGImageMetadataNamespaceExifAux, "http://ns.adobe.com/exif/1.0/aux/")
IIOK(kCGImageMetadataNamespaceExifEX, "http://cipa.jp/exif/1.0/")
IIOK(kCGImageMetadataNamespaceDublinCore, "http://purl.org/dc/elements/1.1/")
IIOK(kCGImageMetadataNamespaceIPTCCore, "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/")
IIOK(kCGImageMetadataNamespaceIPTCExtension, "http://iptc.org/std/Iptc4xmpExt/2008-02-29/")
IIOK(kCGImageMetadataNamespacePhotoshop, "http://ns.adobe.com/photoshop/1.0/")
IIOK(kCGImageMetadataNamespaceTIFF, "http://ns.adobe.com/tiff/1.0/")
IIOK(kCGImageMetadataNamespaceXMPBasic, "http://ns.adobe.com/xap/1.0/")
IIOK(kCGImageMetadataNamespaceXMPRights, "http://ns.adobe.com/xap/1.0/rights/")
IIOK(kCGImageMetadataPrefixExif, "exif")
IIOK(kCGImageMetadataPrefixExifAux, "aux")
IIOK(kCGImageMetadataPrefixExifEX, "exifEX")
IIOK(kCGImageMetadataPrefixDublinCore, "dc")
IIOK(kCGImageMetadataPrefixIPTCCore, "Iptc4xmpCore")
IIOK(kCGImageMetadataPrefixIPTCExtension, "Iptc4xmpExt")
IIOK(kCGImageMetadataPrefixPhotoshop, "photoshop")
IIOK(kCGImageMetadataPrefixTIFF, "tiff")
IIOK(kCGImageMetadataPrefixXMPBasic, "xmp")
IIOK(kCGImageMetadataPrefixXMPRights, "xmpRights")
IIOK(kCGImageMetadataEnumerateRecursively, "kCGImageMetadataEnumerateRecursively")
IIOK(kCGImageMetadataShouldExcludeXMP, "kCGImageMetadataShouldExcludeXMP")
IIOK(kCGImageMetadataShouldExcludeGPS, "kCGImageMetadataShouldExcludeGPS")
IIOK(kCGImageMetadataErrorDomain, "kCFErrorDomainCGImageMetadata")

long CFArrayGetCount(void *a);
void *CFArrayGetValueAtIndex(void *a, long i);
void *CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(void *a, const void *v);
u64 CFArrayGetTypeID(void);
long CFDictionaryGetCount(void *d);
void CFDictionaryGetKeysAndValues(void *d, const void **keys, const void **vals);

#define TID_META 0x2B2
#define TID_TAG 0x2B3
#define META_MAGIC 0x4D455441u
#define TAG_MAGIC 0x54414731u
typedef struct { cx_class_t *isa; u32 magic; xmp_doc doc; } imeta_t;
typedef struct { cx_class_t *isa; u32 magic; xmp_node *node; } itag_t;
static cx_class_t META_c, META_m, TAG_c, TAG_m;
static int meta_ready;
static void meta_dealloc(id self, SEL c) { (void)c; imeta_t *m = (imeta_t *)self; xmp_doc_free(&m->doc); m->magic = 0; cx_object_free(self); }
static void tag_dealloc(id self, SEL c) { (void)c; itag_t *t = (itag_t *)self; xmp_node_free(t->node); t->magic = 0; cx_object_free(self); }
static u64 meta_tid(id s, SEL c) { (void)s; (void)c; return TID_META; }
static u64 tag_tid(id s, SEL c) { (void)s; (void)c; return TID_TAG; }
static void meta_classes(void) {
    if (meta_ready) return;
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns) return;
    cx_define_class(&META_c, &META_m, "CGImageMetadata", ns, (u32)(sizeof(imeta_t) - sizeof(void *)));
    cx_define_class(&TAG_c, &TAG_m, "CGImageMetadataTag", ns, (u32)(sizeof(itag_t) - sizeof(void *)));
    cx_add_method(&META_c, "dealloc", (void *)meta_dealloc);
    cx_add_method(&TAG_c, "dealloc", (void *)tag_dealloc);
    cx_add_method(&META_c, "_cfTypeID", (void *)meta_tid);
    cx_add_method(&TAG_c, "_cfTypeID", (void *)tag_tid);
    meta_ready = 1;
}
static imeta_t *as_meta(const void *p) { const imeta_t *m = p; return m && m->magic == META_MAGIC ? (imeta_t *)m : 0; }
static itag_t *as_tag(const void *p) { const itag_t *t = p; return t && t->magic == TAG_MAGIC ? (itag_t *)t : 0; }
static imeta_t *meta_new(void) {
    meta_classes();
    imeta_t *m = meta_ready ? (imeta_t *)cx_alloc_instance(&META_c) : 0;
    if (!m) return 0;
    m->magic = META_MAGIC;
    memset(&m->doc, 0, sizeof m->doc);
    return m;
}
static void *tag_wrap(xmp_node *n) {
    meta_classes();
    itag_t *t = meta_ready && n ? (itag_t *)cx_alloc_instance(&TAG_c) : 0;
    if (!t) { xmp_node_free(n); return 0; }
    t->magic = TAG_MAGIC;
    t->node = n;
    return t;
}
u64 CGImageMetadataGetTypeID(void) { return TID_META; }
u64 CGImageMetadataTagGetTypeID(void) { return TID_TAG; }
static void doc_copy(xmp_doc *dst, const xmp_doc *src) {
    for (int i = 0; i < src->nns; i++) xmp_register(dst, src->ns[i].uri, src->ns[i].prefix);
    for (int i = 0; i < src->n; i++) xmp_doc_add(dst, xmp_node_copy(src->props[i]));
}
void *CGImageMetadataCreateMutable(void) { return meta_new(); }
void *CGImageMetadataCreateMutableCopy(void *meta) {
    imeta_t *s = as_meta(meta), *m = meta_new();
    if (m && s) doc_copy(&m->doc, &s->doc);
    return m;
}
void *CGImageMetadataCreateFromXMPData(void *data) {
    const u8 *b = data ? CFDataGetBytePtr(data) : 0;
    long n = data ? CFDataGetLength(data) : 0;
    if (!b || n <= 0) return 0;
    imeta_t *m = meta_new();
    if (!m) return 0;
    if (xmp_parse((const char *)b, (size_t)n, &m->doc)) { CFRelease(m); return 0; }
    return m;
}
void *CGImageMetadataCreateXMPData(void *meta, void *opts) {
    (void)opts;
    imeta_t *m = as_meta(meta);
    if (!m) return 0;
    size_t n;
    char *x = xmp_serialize(&m->doc, &n);
    if (!x) return 0;
    void *d = CFDataCreate(0, (const u8 *)x, (long)n);
    free(x);
    return d;
}
u8 CGImageMetadataRegisterNamespaceForPrefix(void *meta, void *xmlns, void *prefix, void **err) {
    if (err) *err = 0;
    imeta_t *m = as_meta(meta);
    return m && str_of(xmlns) && str_of(prefix) && xmp_register(&m->doc, str_of(xmlns), str_of(prefix));
}

/* ---- valores CF ↔ nós */
static char *cf_text(void *v) {
    char buf[64];
    if (!v) return 0;
    if (str_of(v)) return strdup(str_of(v));
    if (v == kCFBooleanTrue) return strdup("True");
    if (v == kCFBooleanFalse) return strdup("False");
    u64 t = CFGetTypeID(v);
    if (t == CFBooleanGetTypeID()) return strdup(CFBooleanGetValue(v) ? "True" : "False");
    if (t == CFNumberGetTypeID()) {
        double dv = 0;
        CFNumberGetValue(v, kNumF64, &dv);
        if (dv == (double)(long long)dv && dv < 1e15 && dv > -1e15) snprintf(buf, sizeof buf, "%lld", (long long)dv);
        else snprintf(buf, sizeof buf, "%.10g", dv);
        return strdup(buf);
    }
    return 0;
}
/* valor CF em nó (tipo pedido, ou deduzido: array → Seq, dicionário → estrutura) */
static xmp_node *node_from_value(const char *ns, const char *prefix, const char *name, int type, void *v) {
    if (!v) return 0;
    if (as_tag(v)) {
        xmp_node *c = xmp_node_copy(as_tag(v)->node);
        if (c && name) { free(c->name); c->name = strdup(name); }
        return c;
    }
    u64 t = CFGetTypeID(v);
    if (t == CFArrayGetTypeID()) {
        if (type < XMP_ARRAY_UNORDERED || type > XMP_ALTERNATE_TEXT) type = XMP_ARRAY_ORDERED;
        xmp_node *n = xmp_node_new(ns, prefix, name, type, 0);
        for (long i = 0; n && i < CFArrayGetCount(v); i++) xmp_node_add(n, node_from_value(ns, prefix, name, XMP_DEFAULT, CFArrayGetValueAtIndex(v, i)));
        return n;
    }
    if (t == CFDictionaryGetTypeID()) {
        long k = CFDictionaryGetCount(v);
        const void **keys = calloc((size_t)(k ? k : 1), sizeof *keys), **vals = calloc((size_t)(k ? k : 1), sizeof *vals);
        xmp_node *n = xmp_node_new(ns, prefix, name, type == XMP_ALTERNATE_TEXT ? XMP_ALTERNATE_TEXT : XMP_STRUCTURE, 0);
        if (keys && vals && n) {
            CFDictionaryGetKeysAndValues(v, keys, vals);
            for (long i = 0; i < k; i++) {
                const char *key = str_of(keys[i]);
                if (!key) continue;
                if (n->type == XMP_ALTERNATE_TEXT) {
                    xmp_node *li = node_from_value(ns, prefix, name, XMP_DEFAULT, (void *)vals[i]);
                    if (li) { li->lang = strdup(key); xmp_node_add(n, li); }
                } else {
                    const char *c = strchr(key, ':');     /* campo "prefixo:nome" ou só "nome" (namespace do pai) */
                    xmp_node *f = node_from_value(ns, prefix, c ? c + 1 : key, XMP_DEFAULT, (void *)vals[i]);
                    xmp_node_add(n, f);
                }
            }
        }
        free(keys); free(vals);
        return n;
    }
    char *s = cf_text(v);
    if (!s) return 0;
    xmp_node *n = xmp_node_new(ns, prefix, name, type == XMP_STRING ? XMP_STRING : XMP_DEFAULT, s);
    free(s);
    return n;
}
void *CGImageMetadataTagCreate(void *xmlns, void *prefix, void *name, int type, void *value) {
    const char *ns = str_of(xmlns), *nm = str_of(name);
    if (!ns || !nm || !value) return 0;
    xmp_node *n = node_from_value(ns, str_of(prefix), nm, type, value);
    if (!n) return 0;
    if (type >= XMP_DEFAULT && type <= XMP_STRUCTURE) n->type = type == XMP_DEFAULT && n->kids ? n->type : type;
    if (!n->prefix) { const char *p = xmp_prefix_for(0, ns); if (p) n->prefix = strdup(p); }
    return tag_wrap(n);
}
void *CGImageMetadataTagCopyNamespace(void *tag) { itag_t *t = as_tag(tag); return t && t->node->ns ? cfs(t->node->ns) : 0; }
void *CGImageMetadataTagCopyPrefix(void *tag) { itag_t *t = as_tag(tag); return t && t->node->prefix ? cfs(t->node->prefix) : 0; }
void *CGImageMetadataTagCopyName(void *tag) { itag_t *t = as_tag(tag); return t && t->node->name ? cfs(t->node->name) : 0; }
int CGImageMetadataTagGetType(void *tag) { itag_t *t = as_tag(tag); return t ? t->node->type : XMP_INVALID; }
/* valor: texto; arrays → CFArray de tags; estrutura → CFDictionary nome → tag; texto alternativo → CFArray de tags */
static void *node_value(const xmp_node *n) {
    if (n->type == XMP_DEFAULT || n->type == XMP_STRING) return cfs(n->value ? n->value : "");
    if (n->type == XMP_STRUCTURE) {
        void *d = CFDictionaryCreateMutable(0, 0, 0, 0);
        for (int i = 0; i < n->nkids; i++) {
            void *k = cfs(n->kids[i]->name), *t = tag_wrap(xmp_node_copy(n->kids[i]));
            if (k && t) CFDictionarySetValue(d, k, t);
        }
        return d;
    }
    void *a = CFArrayCreateMutable(0, 0, 0);
    for (int i = 0; i < n->nkids; i++) { void *t = tag_wrap(xmp_node_copy(n->kids[i])); if (t) CFArrayAppendValue(a, t); }
    return a;
}
void *CGImageMetadataTagCopyValue(void *tag) { itag_t *t = as_tag(tag); return t ? node_value(t->node) : 0; }
void *CGImageMetadataTagCopyQualifiers(void *tag) {
    itag_t *t = as_tag(tag);
    if (!t || !t->node->lang) return 0;
    xmp_node *q = xmp_node_new("http://www.w3.org/XML/1998/namespace", "xml", "lang", XMP_STRING, t->node->lang);
    void *qt = tag_wrap(q);
    if (!qt) return 0;
    const void *v[1] = {qt};
    return CFArrayCreate(0, v, 1, 0);
}

/* ---- caminhos */
static xmp_node *doc_node(imeta_t *m, void *parent, const char *path) {
    xmp_node *base = 0;
    itag_t *pt = as_tag(parent);
    if (pt) {
        /* o pai é uma cópia: acha o nó correspondente no documento pelo namespace e nome */
        for (int i = 0; i < m->doc.n && !base; i++)
            if (m->doc.props[i]->ns && pt->node->ns && !strcmp(m->doc.props[i]->ns, pt->node->ns) && !strcmp(m->doc.props[i]->name, pt->node->name)) base = m->doc.props[i];
        if (!base) return 0;
    }
    return xmp_find(&m->doc, base, path);
}
void *CGImageMetadataCopyTags(void *meta) {
    imeta_t *m = as_meta(meta);
    if (!m) return 0;
    void *a = CFArrayCreateMutable(0, 0, 0);
    for (int i = 0; i < m->doc.n; i++) { void *t = tag_wrap(xmp_node_copy(m->doc.props[i])); if (t) CFArrayAppendValue(a, t); }
    return a;
}
void *CGImageMetadataCopyTagWithPath(void *meta, void *parent, void *path) {
    imeta_t *m = as_meta(meta);
    const char *p = str_of(path);
    xmp_node *n = m && p ? doc_node(m, parent, p) : 0;
    return n ? tag_wrap(xmp_node_copy(n)) : 0;
}
void *CGImageMetadataCopyStringValueWithPath(void *meta, void *parent, void *path) {
    imeta_t *m = as_meta(meta);
    const char *p = str_of(path);
    xmp_node *n = m && p ? doc_node(m, parent, p) : 0;
    if (!n) return 0;
    if (n->type == XMP_ALTERNATE_TEXT) {                 /* x-default, ou o primeiro */
        for (int i = 0; i < n->nkids; i++) if (n->kids[i]->lang && !strcmp(n->kids[i]->lang, "x-default") && n->kids[i]->value) return cfs(n->kids[i]->value);
        return n->nkids && n->kids[0]->value ? cfs(n->kids[0]->value) : 0;
    }
    return n->value ? cfs(n->value) : 0;
}
/* o contêiner e o nome do último segmento; cria estruturas no meio do caminho */
static int set_at(imeta_t *m, void *parent, const char *path, xmp_node *node, int remove) {
    char buf[512];
    size_t l = strlen(path);
    if (l >= sizeof buf) { xmp_node_free(node); return 0; }
    memcpy(buf, path, l + 1);
    char *dot = strrchr(buf, '.');
    char *last = dot ? dot + 1 : buf;
    xmp_node *container = 0;
    if (dot) {
        *dot = 0;
        container = doc_node(m, parent, buf);
        if (!container) {
            if (remove) return 0;
            /* cria a estrutura que falta */
            const char *c = strchr(buf, ':');
            char pre[64] = "";
            if (c && (size_t)(c - buf) < sizeof pre) { memcpy(pre, buf, (size_t)(c - buf)); pre[c - buf] = 0; }
            const char *uri = xmp_uri_for(&m->doc, pre);
            if (!uri || strchr(buf, '.') || strchr(buf, '[')) { xmp_node_free(node); return 0; }
            container = xmp_node_new(uri, pre, c + 1, XMP_STRUCTURE, 0);
            xmp_doc_add(&m->doc, container);
        }
    } else if (as_tag(parent)) container = doc_node(m, parent, "");
    /* índice de array no último segmento: troca o item */
    char *br = strchr(last, '[');
    xmp_node **list = container ? container->kids : m->doc.props;
    int n = container ? container->nkids : m->doc.n;
    char seg[256];
    size_t sl = br ? (size_t)(br - last) : strlen(last);
    if (sl >= sizeof seg) { xmp_node_free(node); return 0; }
    memcpy(seg, last, sl);
    seg[sl] = 0;
    const char *c = strchr(seg, ':');
    const char *uri = 0, *name = seg;
    char pre[64] = "";
    if (c) { memcpy(pre, seg, (size_t)(c - seg)); pre[c - seg] = 0; uri = xmp_uri_for(&m->doc, pre); name = c + 1; if (!uri) { xmp_node_free(node); return 0; } }
    else if (container) uri = container->ns;
    int at = -1;
    for (int i = 0; i < n; i++) if (list[i]->ns && uri && !strcmp(list[i]->ns, uri) && !strcmp(list[i]->name, name)) at = i;
    if (br) {
        if (at < 0) { xmp_node_free(node); return 0; }
        xmp_node *arr = list[at];
        long idx = strtol(br + 1, 0, 10);
        if (remove) {
            if (idx < 0 || idx >= arr->nkids) return 0;
            xmp_node_free(arr->kids[idx]);
            memmove(arr->kids + idx, arr->kids + idx + 1, (size_t)(arr->nkids - idx - 1) * sizeof *arr->kids);
            arr->nkids--;
            return 1;
        }
        if (idx >= 0 && idx < arr->nkids) { xmp_node_free(arr->kids[idx]); arr->kids[idx] = node; }
        else xmp_node_add(arr, node);
        return 1;
    }
    if (remove) {
        if (at < 0) return 0;
        xmp_node_free(list[at]);
        memmove(list + at, list + at + 1, (size_t)(n - at - 1) * sizeof *list);
        if (container) container->nkids--; else m->doc.n--;
        return 1;
    }
    if (node) {
        free(node->ns); node->ns = strdup(uri ? uri : "");
        free(node->name); node->name = strdup(name);
        if (pre[0]) { free(node->prefix); node->prefix = strdup(pre); }
    }
    if (at >= 0) { xmp_node_free(list[at]); list[at] = node; return 1; }
    if (container) xmp_node_add(container, node); else xmp_doc_add(&m->doc, node);
    return 1;
}
u8 CGImageMetadataSetTagWithPath(void *meta, void *parent, void *path, void *tag) {
    imeta_t *m = as_meta(meta);
    itag_t *t = as_tag(tag);
    const char *p = str_of(path);
    if (!m || !p || !t) return 0;
    if (t->node->ns && t->node->prefix) xmp_register(&m->doc, t->node->ns, t->node->prefix);
    return (u8)set_at(m, parent, p, xmp_node_copy(t->node), 0);
}
u8 CGImageMetadataSetValueWithPath(void *meta, void *parent, void *path, void *value) {
    imeta_t *m = as_meta(meta);
    const char *p = str_of(path);
    if (!m || !p || !value) return 0;
    xmp_node *n = node_from_value("", 0, "v", XMP_DEFAULT, value);
    return n ? (u8)set_at(m, parent, p, n, 0) : 0;
}
u8 CGImageMetadataRemoveTagWithPath(void *meta, void *parent, void *path) {
    imeta_t *m = as_meta(meta);
    const char *p = str_of(path);
    return m && p ? (u8)set_at(m, parent, p, 0, 1) : 0;
}
/* enumeração: bloco (caminho, tag) → continua?; recursiva com a opção */
typedef struct { void *isa; int flags, reserved; void *invoke; void *desc; } meta_block_t;
static int enum_node(const xmp_doc *d, const xmp_node *n, const char *base, void *block, int rec) {
    meta_block_t *b = block;
    char path[600];
    const char *p = n->prefix ? n->prefix : xmp_prefix_for(d, n->ns);
    snprintf(path, sizeof path, "%s%s%s:%s", base, base[0] ? "." : "", p ? p : "", n->name);
    void *ps = cfs(path), *t = tag_wrap(xmp_node_copy(n));
    u8 go = ((u8 (*)(void *, void *, void *))b->invoke)(block, ps, t);
    CFRelease(t);
    if (!go) return 0;
    if (rec && n->type == XMP_STRUCTURE)
        for (int i = 0; i < n->nkids; i++) if (!enum_node(d, n->kids[i], path, block, rec)) return 0;
    if (rec && n->type >= XMP_ARRAY_UNORDERED && n->type <= XMP_ALTERNATE_TEXT)
        for (int i = 0; i < n->nkids; i++) {
            char ip[640];
            snprintf(ip, sizeof ip, "%s[%d]", path, i);
            void *ips = cfs(ip), *it = tag_wrap(xmp_node_copy(n->kids[i]));
            u8 g = ((u8 (*)(void *, void *, void *))b->invoke)(block, ips, it);
            CFRelease(it);
            if (!g) return 0;
        }
    return 1;
}
void CGImageMetadataEnumerateTagsUsingBlock(void *meta, void *rootPath, void *opts, void *block) {
    imeta_t *m = as_meta(meta);
    meta_block_t *b = block;
    if (!m || !b || !b->invoke) return;
    int rec = opt_bool(opts, kCGImageMetadataEnumerateRecursively);
    const char *rp = str_of(rootPath);
    if (rp && rp[0]) {
        xmp_node *n = xmp_find(&m->doc, 0, rp);
        if (!n) return;
        for (int i = 0; i < n->nkids; i++) if (!enum_node(&m->doc, n->kids[i], rp, block, rec)) return;
        return;
    }
    for (int i = 0; i < m->doc.n; i++) if (!enum_node(&m->doc, m->doc.props[i], "", block, rec)) return;
}

/* ---- propriedades do ImageIO ↔ XMP */
static const struct { const char *dict, *prop, *path; } iptc_map[] = {
    {"{IPTC}", "Keywords", "dc:subject"}, {"{IPTC}", "CaptionAbstract", "dc:description"}, {"{IPTC}", "ObjectName", "dc:title"},
    {"{IPTC}", "Byline", "dc:creator"}, {"{IPTC}", "CopyrightNotice", "dc:rights"}, {"{IPTC}", "City", "photoshop:City"},
    {"{IPTC}", "CountryPrimaryLocationName", "photoshop:Country"}, {"{IPTC}", "ProvinceState", "photoshop:State"},
    {"{IPTC}", "Headline", "photoshop:Headline"}, {"{IPTC}", "Credit", "photoshop:Credit"}, {"{IPTC}", "Source", "photoshop:Source"},
    {"{IPTC}", "DateCreated", "photoshop:DateCreated"}, {"{IPTC}", "SubLocation", "Iptc4xmpCore:Location"},
    {"{TIFF}", "DateTime", "xmp:ModifyDate"}, {"{TIFF}", "Software", "xmp:CreatorTool"}, {"{TIFF}", "Artist", "dc:creator"},
    {"{TIFF}", "ImageDescription", "dc:description"}, {"{TIFF}", "Copyright", "dc:rights"},
    {"{Exif}", "LensModel", "exifEX:LensModel"}, {"{Exif}", "LensMake", "exifEX:LensMake"}, {"{Exif}", "BodySerialNumber", "exifEX:BodySerialNumber"}};
static int prop_path(const char *dict, const char *prop, char *out, size_t cap) {
    if (!dict || !prop) return 0;
    for (u32 i = 0; i < sizeof iptc_map / sizeof iptc_map[0]; i++)
        if (!strcmp(dict, iptc_map[i].dict) && !strcmp(prop, iptc_map[i].prop)) { snprintf(out, cap, "%s", iptc_map[i].path); return 1; }
    if (!strcmp(dict, "{Exif}")) { snprintf(out, cap, "exif:%s", prop); return 1; }
    if (!strcmp(dict, "{TIFF}")) { snprintf(out, cap, "tiff:%s", prop); return 1; }
    if (!strcmp(dict, "{GPS}")) { snprintf(out, cap, "exif:GPS%s", prop); return 1; }
    if (!strcmp(dict, "{ExifAux}")) { snprintf(out, cap, "aux:%s", prop); return 1; }
    return 0;
}
void *CGImageMetadataCopyTagMatchingImageProperty(void *meta, void *dictName, void *propName) {
    char p[160];
    if (!prop_path(str_of(dictName), str_of(propName), p, sizeof p)) return 0;
    void *ps = cfs(p);
    void *t = CGImageMetadataCopyTagWithPath(meta, 0, ps);
    CFRelease(ps);
    return t;
}
u8 CGImageMetadataSetValueMatchingImageProperty(void *meta, void *dictName, void *propName, void *value) {
    char p[160];
    if (!prop_path(str_of(dictName), str_of(propName), p, sizeof p)) return 0;
    void *ps = cfs(p);
    u8 ok = CGImageMetadataSetValueWithPath(meta, 0, ps, value);
    CFRelease(ps);
    return ok;
}
/* propriedades do arquivo (EXIF, TIFF, GPS, IPTC) somadas ao XMP do arquivo, sem trocar o que o XMP já diz */
static void merge_props(imeta_t *m, void *dict, const char *dname, int skip_gps) {
    if (!dict || CFGetTypeID(dict) != CFDictionaryGetTypeID()) return;
    if (skip_gps && !strcmp(dname, "{GPS}")) return;
    long k = CFDictionaryGetCount(dict);
    const void **keys = calloc((size_t)(k ? k : 1), sizeof *keys), **vals = calloc((size_t)(k ? k : 1), sizeof *vals);
    if (keys && vals) {
        CFDictionaryGetKeysAndValues(dict, keys, vals);
        for (long i = 0; i < k; i++) {
            char p[160];
            if (!prop_path(dname, str_of(keys[i]), p, sizeof p)) continue;
            if (xmp_find(&m->doc, 0, p)) continue;
            void *ps = cfs(p);
            CGImageMetadataSetValueWithPath(m, 0, ps, (void *)vals[i]);
            CFRelease(ps);
        }
    }
    free(keys); free(vals);
}
void *CGImageSourceCopyMetadataAtIndex(void *src, size_t idx, void *opts) {
    isrc_t *s = as_src(src);
    if (!s || !s->n) return 0;
    imeta_t *m = meta_new();
    if (!m) return 0;
    size_t pn;
    const u8 *pk = idx == 0 ? xmp_find_packet(s->bytes, s->n, &pn) : 0;
    if (pk) xmp_parse((const char *)pk, pn, &m->doc);
    void *props = CGImageSourceCopyPropertiesAtIndex(src, idx, 0);
    if (props) {
        static const char *const dn[] = {"{TIFF}", "{Exif}", "{GPS}", "{IPTC}", "{ExifAux}"};
        const void *dk[] = {kCGImagePropertyTIFFDictionary, kCGImagePropertyExifDictionary, kCGImagePropertyGPSDictionary, kCGImagePropertyIPTCDictionary, kCGImagePropertyExifAuxDictionary};
        for (int i = 0; i < 5; i++) merge_props(m, CFDictionaryGetValue(props, dk[i]), dn[i], opt_bool(opts, kCGImageMetadataShouldExcludeGPS));
        CFRelease(props);
    }
    if (!m->doc.n) { CFRelease(m); return 0; }
    return m;
}

/* ---- destino */
static xmp_doc *dst_meta(idst_t *d, int create) {
    if (!d->meta && create) d->meta = calloc(1, sizeof(xmp_doc));
    return (xmp_doc *)d->meta;
}
static void dst_meta_free(idst_t *d) {
    if (d->meta) { xmp_doc_free((xmp_doc *)d->meta); free(d->meta); d->meta = 0; }
}
void CGImageDestinationAddImageAndMetadata(void *dst, void *img, void *meta, void *props) {
    idst_t *d = as_dst(dst);
    if (!d) return;
    CGImageDestinationAddImage(dst, img, props);
    imeta_t *m = as_meta(meta);
    if (m && d->nfr == 1) { xmp_doc *x = dst_meta(d, 1); if (x) doc_copy(x, &m->doc); }
}
/* XMP do destino dentro dos bytes já codificados */
static void dst_embed(idst_t *d, u8 **out, usize *on) {
    xmp_doc *m = dst_meta(d, 0);
    if (!m || !m->n) return;
    size_t xn;
    char *x = xmp_serialize(m, &xn);
    u8 *o2;
    size_t n2;
    if (x && xmp_embed(d->uti, *out, *on, (const u8 *)x, xn, &o2, &n2) == 0) { free(*out); *out = o2; *on = n2; }
    free(x);
}
static int dst_write(idst_t *d, const u8 *out, usize on) {
    int ok = 1;
    if (d->data) CFDataAppendBytes(d->data, out, (long)on);
    else if (d->consumer) ok = cx_consumer_put(d->consumer, out, on) == (long)on;
    else if (d->path) {
        int fd = open(d->path, 577, 0644);
        ok = fd >= 0;
        for (usize k = 0; ok && k < on;) { long w = write(fd, out + k, on - k); if (w <= 0) ok = 0; else k += (usize)w; }
        if (fd >= 0) close(fd);
    }
    return ok;
}
/* cópia da fonte sem recomprimir, com o XMP trocado (ou mesclado) e data/orientação nos metadados */
u8 CGImageDestinationCopyImageSource(void *dst, void *src, void *opts, void **err) {
    if (err) *err = 0;
    idst_t *d = as_dst(dst);
    isrc_t *s = as_src(src);
    if (!d || !s || d->finalized || d->nfr || !s->n) return 0;
    const char *t = iio_sniff(s->bytes, s->n);
    if (!t || strcmp(t, d->uti)) return 0;                /* o macOS também exige o mesmo formato */
    imeta_t *given = as_meta(CFDictionaryGetValue(opts ? opts : CFDictionaryCreateMutable(0, 0, 0, 0), kCGImageDestinationMetadata));
    int merge = opt_bool(opts, kCGImageDestinationMergeMetadata);
    xmp_doc *x = dst_meta(d, 1);
    if (!x) return 0;
    if (merge || !given) {
        size_t pn;
        const u8 *pk = xmp_find_packet(s->bytes, s->n, &pn);
        if (pk) xmp_parse((const char *)pk, pn, x);
    }
    if (given) {
        for (int i = 0; i < given->doc.nns; i++) xmp_register(x, given->doc.ns[i].uri, given->doc.ns[i].prefix);
        for (int i = 0; i < given->doc.n; i++) {
            xmp_node *g = given->doc.props[i];
            int at = -1;
            for (int k = 0; k < x->n; k++) if (x->props[k]->ns && g->ns && !strcmp(x->props[k]->ns, g->ns) && !strcmp(x->props[k]->name, g->name)) at = k;
            if (at >= 0) { xmp_node_free(x->props[at]); x->props[at] = xmp_node_copy(g); }
            else xmp_doc_add(x, xmp_node_copy(g));
        }
    }
    void *dt = opts ? CFDictionaryGetValue(opts, kCGImageDestinationDateTime) : 0;
    imeta_t tmp = {0, META_MAGIC, *x};
    if (dt) { void *p = cfs("xmp:ModifyDate"); CGImageMetadataSetValueWithPath(&tmp, 0, p, dt); CFRelease(p); }
    void *ori = opts ? CFDictionaryGetValue(opts, kCGImageDestinationOrientation) : 0;
    if (ori) { void *p = cfs("tiff:Orientation"); CGImageMetadataSetValueWithPath(&tmp, 0, p, ori); CFRelease(p); }
    *x = tmp.doc;
    u8 *out = malloc(s->n);
    if (!out) return 0;
    memcpy(out, s->bytes, s->n);
    usize on = s->n;
    dst_embed(d, &out, &on);
    int ok = dst_write(d, out, on);
    free(out);
    d->finalized = 1;
    return (u8)ok;
}
