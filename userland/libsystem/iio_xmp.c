/*
 * mOS ImageIO — XMP (ISO 16684-1) sem CoreFoundation: o pacote RDF/XML lido para uma árvore de propriedades
 * (simples, array não ordenado/ordenado/alternativo, texto alternativo com xml:lang, estrutura, qualificadores),
 * gravado de volta, achado dentro de um arquivo de imagem e embutido no contêiner certo (JPEG APP1, PNG iTXt,
 * TIFF tag 700, GIF extensão de aplicação "XMP DataXMP"). A casca CF (CGImageMetadata*) está em imageio_meta.inc.
 * Teste: tools/iio_xmp_host.py (pacotes do exiftool/Adobe, ida e volta, e os arquivos relidos pelo PIL).
 */
#ifdef XMP_HOST
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
int snprintf(char *b, size_t n, const char *f, ...);
#endif
#include "iio_xmp.h"

/* ---------------------------------------------------------------- utilidades */
static char *sdup(const char *s, size_t n) { char *d = malloc(n + 1); if (d) { memcpy(d, s, n); d[n] = 0; } return d; }
static char *sdup0(const char *s) { return s ? sdup(s, strlen(s)) : 0; }
typedef struct { char *b; size_t n, cap; } sb;
static void sb_put(sb *s, const char *d, size_t n) {
    if (s->n + n + 1 > s->cap) { size_t c = s->cap ? s->cap * 2 : 1024; while (c < s->n + n + 1) c *= 2; char *nb = realloc(s->b, c); if (!nb) return; s->b = nb; s->cap = c; }
    memcpy(s->b + s->n, d, n);
    s->n += n;
    s->b[s->n] = 0;
}
static void sb_s(sb *s, const char *d) { sb_put(s, d, strlen(d)); }
static void sb_esc(sb *s, const char *d, int attr) {
    for (; *d; d++) {
        if (*d == '<') sb_s(s, "&lt;");
        else if (*d == '>') sb_s(s, "&gt;");
        else if (*d == '&') sb_s(s, "&amp;");
        else if (attr && *d == '"') sb_s(s, "&quot;");
        else sb_put(s, d, 1);
    }
}

xmp_node *xmp_node_new(const char *ns, const char *prefix, const char *name, int type, const char *value) {
    xmp_node *n = calloc(1, sizeof *n);
    if (!n) return 0;
    n->ns = sdup0(ns); n->prefix = sdup0(prefix); n->name = sdup0(name); n->type = type; n->value = sdup0(value);
    return n;
}
void xmp_node_add(xmp_node *p, xmp_node *k) {
    if (!p || !k) return;
    xmp_node **nk = realloc(p->kids, (size_t)(p->nkids + 1) * sizeof *nk);
    if (!nk) return;
    p->kids = nk;
    p->kids[p->nkids++] = k;
}
void xmp_node_free(xmp_node *n) {
    if (!n) return;
    for (int i = 0; i < n->nkids; i++) xmp_node_free(n->kids[i]);
    free(n->kids); free(n->ns); free(n->prefix); free(n->name); free(n->value); free(n->lang);
    free(n);
}
xmp_node *xmp_node_copy(const xmp_node *n) {
    if (!n) return 0;
    xmp_node *c = xmp_node_new(n->ns, n->prefix, n->name, n->type, n->value);
    if (!c) return 0;
    c->lang = sdup0(n->lang);
    for (int i = 0; i < n->nkids; i++) xmp_node_add(c, xmp_node_copy(n->kids[i]));
    return c;
}
void xmp_doc_free(xmp_doc *d) {
    for (int i = 0; i < d->n; i++) xmp_node_free(d->props[i]);
    free(d->props);
    for (int i = 0; i < d->nns; i++) { free(d->ns[i].prefix); free(d->ns[i].uri); }
    free(d->ns);
    memset(d, 0, sizeof *d);
}
void xmp_doc_add(xmp_doc *d, xmp_node *n) {
    xmp_node **np = realloc(d->props, (size_t)(d->n + 1) * sizeof *np);
    if (!np) return;
    d->props = np;
    d->props[d->n++] = n;
}
/* prefixos conhecidos (os do macOS) e os registrados */
static const struct { const char *prefix, *uri; } known[] = {
    {"exif", "http://ns.adobe.com/exif/1.0/"}, {"aux", "http://ns.adobe.com/exif/1.0/aux/"}, {"exifEX", "http://cipa.jp/exif/1.0/"},
    {"dc", "http://purl.org/dc/elements/1.1/"}, {"Iptc4xmpCore", "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/"},
    {"Iptc4xmpExt", "http://iptc.org/std/Iptc4xmpExt/2008-02-29/"}, {"photoshop", "http://ns.adobe.com/photoshop/1.0/"},
    {"tiff", "http://ns.adobe.com/tiff/1.0/"}, {"xmp", "http://ns.adobe.com/xap/1.0/"}, {"xmpRights", "http://ns.adobe.com/xap/1.0/rights/"},
    {"xmpMM", "http://ns.adobe.com/xap/1.0/mm/"}, {"stEvt", "http://ns.adobe.com/xap/1.0/sType/ResourceEvent#"},
    {"stRef", "http://ns.adobe.com/xap/1.0/sType/ResourceRef#"}, {"crs", "http://ns.adobe.com/camera-raw-settings/1.0/"},
    {"pdf", "http://ns.adobe.com/pdf/1.3/"}, {"xml", "http://www.w3.org/XML/1998/namespace"}, {"rdf", "http://www.w3.org/1999/02/22-rdf-syntax-ns#"}};
const char *xmp_uri_for(const xmp_doc *d, const char *prefix) {
    if (!prefix) return 0;
    for (int i = 0; d && i < d->nns; i++) if (!strcmp(d->ns[i].prefix, prefix)) return d->ns[i].uri;
    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++) if (!strcmp(known[i].prefix, prefix)) return known[i].uri;
    return 0;
}
const char *xmp_prefix_for(const xmp_doc *d, const char *uri) {
    if (!uri) return 0;
    for (int i = 0; d && i < d->nns; i++) if (!strcmp(d->ns[i].uri, uri)) return d->ns[i].prefix;
    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++) if (!strcmp(known[i].uri, uri)) return known[i].prefix;
    return 0;
}
int xmp_register(xmp_doc *d, const char *uri, const char *prefix) {
    if (!uri || !prefix) return 0;
    for (int i = 0; i < d->nns; i++) {
        if (!strcmp(d->ns[i].prefix, prefix)) return !strcmp(d->ns[i].uri, uri);
        if (!strcmp(d->ns[i].uri, uri)) return !strcmp(d->ns[i].prefix, prefix);
    }
    void *nn = realloc(d->ns, (size_t)(d->nns + 1) * sizeof *d->ns);
    if (!nn) return 0;
    d->ns = nn;
    d->ns[d->nns].prefix = sdup0(prefix);
    d->ns[d->nns].uri = sdup0(uri);
    d->nns++;
    return 1;
}

/* ---------------------------------------------------------------- XML */
typedef struct { char *prefix, *local, *value; } xattr;
typedef struct xel {
    char *prefix, *local, *uri;             /* nome qualificado e o namespace resolvido */
    xattr *a; int na;
    char *text;
    struct xel **kids; int nk;
    struct xel *parent;
    struct { char *prefix, *uri; } *nsd; int nnsd;   /* xmlns declarados aqui */
} xel;
static void xel_free(xel *e) {
    if (!e) return;
    for (int i = 0; i < e->nk; i++) xel_free(e->kids[i]);
    for (int i = 0; i < e->na; i++) { free(e->a[i].prefix); free(e->a[i].local); free(e->a[i].value); }
    for (int i = 0; i < e->nnsd; i++) { free(e->nsd[i].prefix); free(e->nsd[i].uri); }
    free(e->kids); free(e->a); free(e->nsd); free(e->prefix); free(e->local); free(e->uri); free(e->text);
    free(e);
}
static const char *ns_lookup(const xel *e, const char *prefix) {
    for (; e; e = e->parent)
        for (int i = 0; i < e->nnsd; i++) if (!strcmp(e->nsd[i].prefix, prefix ? prefix : "")) return e->nsd[i].uri;
    if (prefix && !strcmp(prefix, "xml")) return "http://www.w3.org/XML/1998/namespace";
    return 0;
}
/* texto com entidades (&lt; &gt; &amp; &quot; &apos; &#n; &#xn;) em UTF-8 */
static char *unent(const char *s, size_t n) {
    char *o = malloc(n + 1);
    size_t k = 0;
    if (!o) return 0;
    for (size_t i = 0; i < n;) {
        if (s[i] == '&') {
            const char *e = memchr(s + i, ';', n - i);
            if (e) {
                size_t l = (size_t)(e - (s + i));
                unsigned cp = 0;
                int ok = 1;
                if (l == 3 && !memcmp(s + i, "&lt", 3)) cp = '<';
                else if (l == 3 && !memcmp(s + i, "&gt", 3)) cp = '>';
                else if (l == 4 && !memcmp(s + i, "&amp", 4)) cp = '&';
                else if (l == 5 && !memcmp(s + i, "&quot", 5)) cp = '"';
                else if (l == 5 && !memcmp(s + i, "&apos", 5)) cp = '\'';
                else if (l > 2 && s[i + 1] == '#') {
                    int hex = s[i + 2] == 'x' || s[i + 2] == 'X';
                    for (size_t j = i + 2 + (size_t)hex; j < i + l; j++) {
                        char c = s[j];
                        int v = c >= '0' && c <= '9' ? c - '0' : hex && c >= 'a' && c <= 'f' ? c - 'a' + 10 : hex && c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                        if (v < 0) { ok = 0; break; }
                        cp = cp * (hex ? 16 : 10) + (unsigned)v;
                    }
                } else ok = 0;
                if (ok) {
                    if (cp < 0x80) o[k++] = (char)cp;
                    else if (cp < 0x800) { o[k++] = (char)(0xC0 | cp >> 6); o[k++] = (char)(0x80 | (cp & 63)); }
                    else if (cp < 0x10000) { o[k++] = (char)(0xE0 | cp >> 12); o[k++] = (char)(0x80 | (cp >> 6 & 63)); o[k++] = (char)(0x80 | (cp & 63)); }
                    else { o[k++] = (char)(0xF0 | cp >> 18); o[k++] = (char)(0x80 | (cp >> 12 & 63)); o[k++] = (char)(0x80 | (cp >> 6 & 63)); o[k++] = (char)(0x80 | (cp & 63)); }
                    i += l + 1;
                    continue;
                }
            }
        }
        o[k++] = s[i++];
    }
    o[k] = 0;
    return o;
}
/* busca limitada ao trecho (o XML do arquivo não termina em zero) */
static const char *fnd(const char *s, size_t i, size_t n, const char *needle) {
    size_t k = strlen(needle);
    for (size_t j = i; j + k <= n; j++) if (s[j] == needle[0] && !memcmp(s + j, needle, k)) return s + j;
    return 0;
}
static int is_name(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == ':' || (unsigned char)c >= 0x80; }
static void split_qname(const char *q, size_t n, char **prefix, char **local) {
    const char *c = memchr(q, ':', n);
    if (c) { *prefix = sdup(q, (size_t)(c - q)); *local = sdup(c + 1, n - (size_t)(c - q) - 1); }
    else { *prefix = sdup("", 0); *local = sdup(q, n); }
}
/* árvore XML do trecho; NULL se malformado */
static xel *xml_parse(const char *s, size_t n) {
    xel *root = calloc(1, sizeof *root), *cur = root;
    sb text = {0};
    if (!root) return 0;
    for (size_t i = 0; i < n;) {
        if (s[i] != '<') {
            size_t j = i;
            while (j < n && s[j] != '<') j++;
            if (cur != root) sb_put(&text, s + i, j - i);
            i = j;
            continue;
        }
        if (i + 4 <= n && !memcmp(s + i, "<!--", 4)) { const char *e = fnd(s, i, n, "-->"); i = e ? (size_t)(e - s) + 3 : n; continue; }
        if (i + 9 <= n && !memcmp(s + i, "<![CDATA[", 9)) {
            const char *e = fnd(s, i + 9, n, "]]>");
            size_t end = e ? (size_t)(e - s) : n;
            /* CDATA entra como texto escapado para o unent não mudar nada */
            for (size_t k = i + 9; k < end; k++) { if (s[k] == '&') sb_s(&text, "&amp;"); else sb_put(&text, s + k, 1); }
            i = end + 3;
            continue;
        }
        if (i + 2 <= n && (s[i + 1] == '?' || s[i + 1] == '!')) { const char *e = memchr(s + i, '>', n - i); i = e ? (size_t)(e - s) + 1 : n; continue; }
        if (i + 2 <= n && s[i + 1] == '/') {
            const char *e = memchr(s + i, '>', n - i);
            if (cur->parent) {
                if (text.n) { cur->text = unent(text.b, text.n); }
                text.n = 0;
                cur = cur->parent;
            }
            i = e ? (size_t)(e - s) + 1 : n;
            continue;
        }
        /* abre elemento */
        size_t j = i + 1;
        while (j < n && is_name(s[j])) j++;
        xel *e = calloc(1, sizeof *e);
        if (!e) break;
        split_qname(s + i + 1, j - i - 1, &e->prefix, &e->local);
        e->parent = cur;
        text.n = 0;
        int selfclose = 0;
        for (;;) {
            while (j < n && (s[j] == ' ' || s[j] == '\t' || s[j] == '\n' || s[j] == '\r')) j++;
            if (j >= n) break;
            if (s[j] == '/') { selfclose = 1; j++; continue; }
            if (s[j] == '>') { j++; break; }
            size_t a = j;
            while (j < n && is_name(s[j])) j++;
            size_t an = j - a;
            while (j < n && s[j] != '=' && s[j] != '>') j++;
            if (j >= n || s[j] != '=') { if (an == 0) j++; continue; }
            j++;
            while (j < n && (s[j] == ' ' || s[j] == '\t' || s[j] == '\n' || s[j] == '\r')) j++;
            char q = j < n ? s[j] : '"';
            if (q != '"' && q != '\'') break;
            size_t v = ++j;
            while (j < n && s[j] != q) j++;
            char *val = unent(s + v, j - v);
            j++;
            if (an >= 5 && !memcmp(s + a, "xmlns", 5)) {
                void *nn = realloc(e->nsd, (size_t)(e->nnsd + 1) * sizeof *e->nsd);
                if (nn) { e->nsd = nn; e->nsd[e->nnsd].prefix = an > 6 ? sdup(s + a + 6, an - 6) : sdup("", 0); e->nsd[e->nnsd].uri = val; e->nnsd++; }
                else free(val);
                continue;
            }
            xattr *na = realloc(e->a, (size_t)(e->na + 1) * sizeof *na);
            if (!na) { free(val); continue; }
            e->a = na;
            split_qname(s + a, an, &e->a[e->na].prefix, &e->a[e->na].local);
            e->a[e->na].value = val;
            e->na++;
        }
        const char *u = ns_lookup(e, e->prefix);
        e->uri = sdup0(u ? u : "");
        xel **nk = realloc(cur->kids, (size_t)(cur->nk + 1) * sizeof *nk);
        if (!nk) { xel_free(e); break; }
        cur->kids = nk;
        cur->kids[cur->nk++] = e;
        if (!selfclose) cur = e;
        i = j;
    }
    free(text.b);
    return root;
}
#define RDF "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
static int is_rdf(const xel *e, const char *local) { return e && e->uri && e->local && !strcmp(e->uri, RDF) && !strcmp(e->local, local); }
static const char *attr_uri(const xel *e, const xattr *a) { const char *u = ns_lookup(e, a->prefix); return u ? u : ""; }
static const char *get_attr(const xel *e, const char *uri, const char *local) {
    for (int i = 0; i < e->na; i++) if (!strcmp(e->a[i].local, local) && !strcmp(attr_uri(e, &e->a[i]), uri)) return e->a[i].value;
    return 0;
}
static void reg_from(xmp_doc *d, const xel *e, const char *uri) {
    if (!uri || !uri[0] || !strcmp(uri, RDF) || xmp_prefix_for(d, uri)) return;
    /* prefixo como o documento declarou */
    for (const xel *x = e; x; x = x->parent)
        for (int i = 0; i < x->nnsd; i++) if (!strcmp(x->nsd[i].uri, uri) && x->nsd[i].prefix[0]) { xmp_register(d, uri, x->nsd[i].prefix); return; }
}
static xmp_node *prop_of(xmp_doc *d, const xel *e);
/* campos de uma estrutura: atributos e filhos de um elemento (Description ou parseType="Resource") */
static void struct_fields(xmp_doc *d, xmp_node *st, const xel *e) {
    for (int i = 0; i < e->na; i++) {
        const char *u = attr_uri(e, &e->a[i]);
        if (!strcmp(u, RDF) || !strcmp(u, "http://www.w3.org/XML/1998/namespace") || !u[0]) continue;
        reg_from(d, e, u);
        xmp_node_add(st, xmp_node_new(u, xmp_prefix_for(d, u), e->a[i].local, XMP_DEFAULT, e->a[i].value));
    }
    for (int i = 0; i < e->nk; i++) { xmp_node *k = prop_of(d, e->kids[i]); if (k) xmp_node_add(st, k); }
}
/* um elemento de propriedade (ou rdf:li) em nó */
static xmp_node *value_of(xmp_doc *d, const xel *e, const char *uri, const char *name) {
    xmp_node *n = 0;
    const char *pt = get_attr(e, RDF, "parseType"), *res = get_attr(e, RDF, "resource");
    const char *lang = get_attr(e, "http://www.w3.org/XML/1998/namespace", "lang");
    const xel *arr = 0, *desc = 0;
    for (int i = 0; i < e->nk; i++) {
        if (is_rdf(e->kids[i], "Bag") || is_rdf(e->kids[i], "Seq") || is_rdf(e->kids[i], "Alt")) arr = e->kids[i];
        else if (is_rdf(e->kids[i], "Description")) desc = e->kids[i];
    }
    if (arr) {
        int t = is_rdf(arr, "Bag") ? XMP_ARRAY_UNORDERED : is_rdf(arr, "Seq") ? XMP_ARRAY_ORDERED : XMP_ARRAY_ALTERNATE;
        n = xmp_node_new(uri, xmp_prefix_for(d, uri), name, t, 0);
        int all_lang = arr->nk > 0;
        for (int i = 0; i < arr->nk; i++) {
            if (!is_rdf(arr->kids[i], "li")) continue;
            xmp_node *li = value_of(d, arr->kids[i], uri, name);
            if (!li) continue;
            if (!li->lang) all_lang = 0;
            xmp_node_add(n, li);
        }
        if (t == XMP_ARRAY_ALTERNATE && all_lang) n->type = XMP_ALTERNATE_TEXT;
    } else if ((pt && !strcmp(pt, "Resource")) || desc) {
        n = xmp_node_new(uri, xmp_prefix_for(d, uri), name, XMP_STRUCTURE, 0);
        struct_fields(d, n, desc ? desc : e);
    } else {
        int has_fields = 0;
        for (int i = 0; i < e->na; i++) {
            const char *u = attr_uri(e, &e->a[i]);
            if (u[0] && strcmp(u, RDF) && strcmp(u, "http://www.w3.org/XML/1998/namespace")) has_fields = 1;
        }
        if (has_fields) { n = xmp_node_new(uri, xmp_prefix_for(d, uri), name, XMP_STRUCTURE, 0); struct_fields(d, n, e); }
        else n = xmp_node_new(uri, xmp_prefix_for(d, uri), name, XMP_STRING, res ? res : e->text ? e->text : "");
    }
    if (n && lang) n->lang = sdup0(lang);
    return n;
}
static xmp_node *prop_of(xmp_doc *d, const xel *e) {
    if (!e->uri[0] || !strcmp(e->uri, RDF)) return 0;
    reg_from(d, e, e->uri);
    return value_of(d, e, e->uri, e->local);
}
static void walk(xmp_doc *d, const xel *e) {
    if (is_rdf(e, "Description")) {
        for (int i = 0; i < e->na; i++) {
            const char *u = attr_uri(e, &e->a[i]);
            if (!u[0] || !strcmp(u, RDF) || !strcmp(u, "http://www.w3.org/XML/1998/namespace")) continue;
            reg_from(d, e, u);
            xmp_doc_add(d, xmp_node_new(u, xmp_prefix_for(d, u), e->a[i].local, XMP_DEFAULT, e->a[i].value));
        }
        for (int i = 0; i < e->nk; i++) { xmp_node *p = prop_of(d, e->kids[i]); if (p) xmp_doc_add(d, p); }
        return;
    }
    for (int i = 0; i < e->nk; i++) walk(d, e->kids[i]);
}
int xmp_parse(const char *xml, size_t n, xmp_doc *out) {
    memset(out, 0, sizeof *out);
    xel *root = xml_parse(xml, n);
    if (!root) return -1;
    walk(out, root);
    xel_free(root);
    return 0;
}

/* ---------------------------------------------------------------- gravação */
static const char *pfx(const xmp_doc *d, const xmp_node *n) {
    const char *p = n->prefix && n->prefix[0] ? n->prefix : xmp_prefix_for(d, n->ns);
    return p ? p : "ns";
}
static void ser_value(sb *s, const xmp_doc *d, const xmp_node *n, int ind);
static void indent(sb *s, int k) { sb_s(s, "\n"); for (int i = 0; i < k; i++) sb_s(s, " "); }
static void ser_prop(sb *s, const xmp_doc *d, const xmp_node *n, int ind) {
    indent(s, ind);
    sb_s(s, "<"); sb_s(s, pfx(d, n)); sb_s(s, ":"); sb_s(s, n->name);
    if (n->lang) { sb_s(s, " xml:lang=\""); sb_esc(s, n->lang, 1); sb_s(s, "\""); }
    ser_value(s, d, n, ind);
    sb_s(s, "</"); sb_s(s, pfx(d, n)); sb_s(s, ":"); sb_s(s, n->name); sb_s(s, ">");
}
static void ser_li(sb *s, const xmp_doc *d, const xmp_node *n, int ind) {
    indent(s, ind);
    sb_s(s, "<rdf:li");
    if (n->lang) { sb_s(s, " xml:lang=\""); sb_esc(s, n->lang, 1); sb_s(s, "\""); }
    ser_value(s, d, n, ind);
    sb_s(s, "</rdf:li>");
}
static void ser_value(sb *s, const xmp_doc *d, const xmp_node *n, int ind) {
    if (n->type == XMP_STRUCTURE) {
        sb_s(s, " rdf:parseType=\"Resource\">");
        for (int i = 0; i < n->nkids; i++) ser_prop(s, d, n->kids[i], ind + 1);
        indent(s, ind);
        return;
    }
    if (n->type >= XMP_ARRAY_UNORDERED && n->type <= XMP_ALTERNATE_TEXT) {
        const char *k = n->type == XMP_ARRAY_UNORDERED ? "Bag" : n->type == XMP_ARRAY_ORDERED ? "Seq" : "Alt";
        sb_s(s, ">");
        indent(s, ind + 1);
        sb_s(s, "<rdf:"); sb_s(s, k); sb_s(s, ">");
        for (int i = 0; i < n->nkids; i++) ser_li(s, d, n->kids[i], ind + 2);
        indent(s, ind + 1);
        sb_s(s, "</rdf:"); sb_s(s, k); sb_s(s, ">");
        indent(s, ind);
        return;
    }
    sb_s(s, ">");
    sb_esc(s, n->value ? n->value : "", 0);
}
static void collect_ns(const xmp_doc *d, const xmp_node *n, const char **uris, int *nu) {
    if (n->ns && n->ns[0]) {
        int seen = 0;
        for (int i = 0; i < *nu; i++) if (!strcmp(uris[i], n->ns)) seen = 1;
        if (!seen && *nu < 64) uris[(*nu)++] = n->ns;
    }
    for (int i = 0; i < n->nkids; i++) collect_ns(d, n->kids[i], uris, nu);
}
char *xmp_serialize(const xmp_doc *d, size_t *len) {
    sb s = {0};
    const char *uris[64];
    int nu = 0;
    for (int i = 0; i < d->n; i++) collect_ns(d, d->props[i], uris, &nu);
    sb_s(&s, "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"mOS ImageIO\">\n"
             " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n  <rdf:Description rdf:about=\"\"");
    for (int i = 0; i < nu; i++) {
        const char *p = xmp_prefix_for(d, uris[i]);
        char gen[16];
        if (!p) { snprintf(gen, sizeof gen, "ns%d", i + 1); p = gen; }
        sb_s(&s, "\n    xmlns:"); sb_s(&s, p); sb_s(&s, "=\""); sb_esc(&s, uris[i], 1); sb_s(&s, "\"");
    }
    sb_s(&s, ">");
    for (int i = 0; i < d->n; i++) ser_prop(&s, d, d->props[i], 3);
    sb_s(&s, "\n  </rdf:Description>\n </rdf:RDF>\n</x:xmpmeta>\n");
    /* preenchimento para edição no lugar, como os gravadores da Adobe */
    for (int i = 0; i < 20; i++) sb_s(&s, "                                                                                                   \n");
    sb_s(&s, "<?xpacket end=\"w\"?>");
    if (len) *len = s.n;
    return s.b;
}

/* ---------------------------------------------------------------- caminho ("dc:subject[0]", "exif:Flash.Fired", "dc:title[x-default]") */
static int seg_match(const xmp_doc *d, const xmp_node *n, const char *seg, size_t len) {
    const char *c = memchr(seg, ':', len);
    if (!c) return strlen(n->name) == len && !memcmp(n->name, seg, len);
    size_t pl = (size_t)(c - seg);
    char p[64];
    if (pl >= sizeof p) return 0;
    memcpy(p, seg, pl);
    p[pl] = 0;
    const char *u = xmp_uri_for(d, p);
    size_t nl = len - pl - 1;
    return u && n->ns && !strcmp(u, n->ns) && strlen(n->name) == nl && !memcmp(n->name, c + 1, nl);
}
/* nó do caminho dentro de base (NULL = raiz do documento); *parent_out recebe o contêiner (para gravar) */
xmp_node *xmp_find(const xmp_doc *d, xmp_node *base, const char *path) {
    xmp_node **list = base ? base->kids : d->props;
    int n = base ? base->nkids : d->n;
    xmp_node *cur = 0;
    const char *p = path;
    while (p && *p) {
        size_t l = 0;
        while (p[l] && p[l] != '.' && p[l] != '[') l++;
        cur = 0;
        for (int i = 0; i < n; i++) if (seg_match(d, list[i], p, l)) { cur = list[i]; break; }
        if (!cur) return 0;
        p += l;
        while (*p == '[') {
            const char *e = strchr(p, ']');
            if (!e) return 0;
            size_t il = (size_t)(e - p - 1);
            xmp_node *pick = 0;
            if (il && p[1] >= '0' && p[1] <= '9') {
                long idx = strtol(p + 1, 0, 10);
                if (idx >= 0 && idx < cur->nkids) pick = cur->kids[idx];
            } else
                for (int i = 0; i < cur->nkids; i++) if (cur->kids[i]->lang && strlen(cur->kids[i]->lang) == il && !memcmp(cur->kids[i]->lang, p + 1, il)) pick = cur->kids[i];
            if (!pick) return 0;
            cur = pick;
            p = e + 1;
        }
        if (*p == '.') { p++; list = cur->kids; n = cur->nkids; }
    }
    return cur;
}

/* ---------------------------------------------------------------- arquivo: achar e embutir */
static const u8 *find_bytes(const u8 *h, size_t hn, const char *needle) {
    size_t nn = strlen(needle);
    for (size_t i = 0; i + nn <= hn; i++) if (h[i] == (u8)needle[0] && !memcmp(h + i, needle, nn)) return h + i;
    return 0;
}
const u8 *xmp_find_packet(const u8 *f, size_t n, size_t *len) {
    const u8 *a = find_bytes(f, n, "<x:xmpmeta"), *b;
    if (!a) a = find_bytes(f, n, "<rdf:RDF");
    if (!a) return 0;
    b = find_bytes(a, n - (size_t)(a - f), "</x:xmpmeta>");
    size_t tail = 12;
    if (!b) { b = find_bytes(a, n - (size_t)(a - f), "</rdf:RDF>"); tail = 10; }
    if (!b) return 0;
    *len = (size_t)(b - a) + tail;
    return a;
}
static u32 crc_table[256];
static u32 crc32b(const u8 *d, size_t n) {
    if (!crc_table[1]) for (u32 i = 0; i < 256; i++) { u32 c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crc_table[i] = c; }
    u32 c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ d[i]) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
static void be32w(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static u32 rd32(const u8 *p, int le) { return le ? (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24 : (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u32 rd16x(const u8 *p, int le) { return le ? (u32)p[0] | (u32)p[1] << 8 : (u32)p[0] << 8 | p[1]; }
static void wr32(u8 *p, u32 v, int le) { if (le) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); } else be32w(p, v); }
static void wr16(u8 *p, u32 v, int le) { if (le) { p[0] = (u8)v; p[1] = (u8)(v >> 8); } else { p[0] = (u8)(v >> 8); p[1] = (u8)v; } }
/* JPEG: APP1 do XMP logo depois do SOI e dos APP0/APP1 de EXIF; um APP1 de XMP antigo sai */
static int emb_jpeg(const u8 *in, size_t n, const u8 *x, size_t xn, u8 **out, size_t *on) {
    static const char sig[] = "http://ns.adobe.com/xap/1.0/";
    if (n < 4 || in[0] != 0xFF || in[1] != 0xD8 || xn + sizeof sig + 2 > 65535) return -1;
    u8 *o = malloc(n + xn + 64);
    if (!o) return -1;
    size_t k = 0, i = 2;
    o[k++] = 0xFF; o[k++] = 0xD8;
    int done = 0;
    while (i + 4 <= n && in[i] == 0xFF) {
        u8 m = in[i + 1];
        if (m == 0xD9 || m == 0xDA || (m >= 0xD0 && m <= 0xD7)) break;
        size_t L = (size_t)in[i + 2] << 8 | in[i + 3];
        if (i + 2 + L > n) break;
        int is_xmp = m == 0xE1 && L >= sizeof sig + 2 && !memcmp(in + i + 4, sig, sizeof sig);
        if (!done && !(m == 0xE0 || (m == 0xE1 && !is_xmp))) {
            size_t len = xn + sizeof sig + 2;
            o[k++] = 0xFF; o[k++] = 0xE1; o[k++] = (u8)(len >> 8); o[k++] = (u8)len;
            memcpy(o + k, sig, sizeof sig); k += sizeof sig;
            memcpy(o + k, x, xn); k += xn;
            done = 1;
        }
        if (!is_xmp) { memcpy(o + k, in + i, L + 2); k += L + 2; }
        i += L + 2;
    }
    if (!done) {
        size_t len = xn + sizeof sig + 2;
        o[k++] = 0xFF; o[k++] = 0xE1; o[k++] = (u8)(len >> 8); o[k++] = (u8)len;
        memcpy(o + k, sig, sizeof sig); k += sizeof sig;
        memcpy(o + k, x, xn); k += xn;
    }
    memcpy(o + k, in + i, n - i);
    k += n - i;
    *out = o; *on = k;
    return 0;
}
/* PNG: chunk iTXt "XML:com.adobe.xmp" antes do primeiro IDAT; um iTXt de XMP antigo sai */
static int emb_png(const u8 *in, size_t n, const u8 *x, size_t xn, u8 **out, size_t *on) {
    if (n < 8 || memcmp(in, "\x89PNG\r\n\x1a\n", 8)) return -1;
    static const char key[] = "XML:com.adobe.xmp";
    size_t body = sizeof key + 4 + xn;                 /* chave\0, flag, método, idioma\0, traduzida\0, texto */
    u8 *o = malloc(n + body + 32);
    if (!o) return -1;
    size_t k = 8, i = 8;
    memcpy(o, in, 8);
    int done = 0;
    while (i + 12 <= n) {
        u32 L = rd32(in + i, 0);
        if (i + 12 + L > n) break;
        const u8 *t = in + i + 4;
        int is_xmp = !memcmp(t, "iTXt", 4) && L >= sizeof key && !memcmp(in + i + 8, key, sizeof key);
        if (!done && (!memcmp(t, "IDAT", 4) || !memcmp(t, "IEND", 4))) {
            u8 *c = o + k;
            be32w(c, (u32)body);
            memcpy(c + 4, "iTXt", 4);
            memcpy(c + 8, key, sizeof key);
            c[8 + sizeof key] = 0; c[9 + sizeof key] = 0; c[10 + sizeof key] = 0; c[11 + sizeof key] = 0;
            memcpy(c + 12 + sizeof key, x, xn);
            be32w(c + 8 + body, crc32b(c + 4, body + 4));
            k += body + 12;
            done = 1;
        }
        if (!is_xmp) { memcpy(o + k, in + i, L + 12); k += L + 12; }
        i += L + 12;
    }
    *out = o; *on = k;
    return done ? 0 : -1;
}
/* TIFF: a IFD0 regravada no fim com a tag 700 (XMLPacket, BYTE) e os dados do XMP logo antes */
static int emb_tiff(const u8 *in, size_t n, const u8 *x, size_t xn, u8 **out, size_t *on) {
    if (n < 8) return -1;
    int le = in[0] == 'I';
    if ((in[0] != 'I' && in[0] != 'M') || rd16x(in + 2, le) != 42) return -1;
    u32 ifd = rd32(in + 4, le);
    if (ifd + 2 > n) return -1;
    u32 cnt = rd16x(in + ifd, le);
    if (ifd + 2 + cnt * 12 + 4 > n) return -1;
    size_t xoff = (n + 1) & ~(size_t)1, ioff = (xoff + xn + 1) & ~(size_t)1;
    u32 newcnt = cnt + 1;
    for (u32 i = 0; i < cnt; i++) if (rd16x(in + ifd + 2 + i * 12, le) == 700) newcnt = cnt;
    u8 *o = calloc(1, ioff + 2 + newcnt * 12 + 4);
    if (!o) return -1;
    memcpy(o, in, n);
    /* o XMP antigo (tag 700) vira espaço: o novo vai para o fim */
    for (u32 i = 0; i < cnt; i++) {
        const u8 *src = in + ifd + 2 + i * 12;
        if (rd16x(src, le) != 700) continue;
        u32 c = rd32(src + 4, le), off = c > 4 ? rd32(src + 8, le) : (u32)(src + 8 - in);
        if (off + (size_t)c <= n) memset(o + off, ' ', c);
    }
    memcpy(o + xoff, x, xn);
    u8 *e = o + ioff + 2;
    u32 k = 0;
    int put = 0;
    for (u32 i = 0; i < cnt; i++) {
        const u8 *src = in + ifd + 2 + i * 12;
        u32 tag = rd16x(src, le);
        if (!put && tag >= 700) {
            wr16(e + k * 12, 700, le); wr16(e + k * 12 + 2, 1, le); wr32(e + k * 12 + 4, (u32)xn, le); wr32(e + k * 12 + 8, (u32)xoff, le);
            k++;
            put = 1;
            if (tag == 700) continue;
        }
        memcpy(e + k * 12, src, 12);
        k++;
    }
    if (!put) { wr16(e + k * 12, 700, le); wr16(e + k * 12 + 2, 1, le); wr32(e + k * 12 + 4, (u32)xn, le); wr32(e + k * 12 + 8, (u32)xoff, le); k++; }
    wr16(o + ioff, k, le);
    memcpy(o + ioff + 2 + k * 12, in + ifd + 2 + cnt * 12, 4);       /* próxima IFD */
    wr32(o + 4, (u32)ioff, le);
    *out = o; *on = ioff + 2 + k * 12 + 4;
    return 0;
}
/* GIF: extensão de aplicação "XMP DataXMP" com o "trailer mágico" de 258 bytes, antes do primeiro bloco */
static int emb_gif(const u8 *in, size_t n, const u8 *x, size_t xn, u8 **out, size_t *on) {
    if (n < 13 || memcmp(in, "GIF8", 4)) return -1;
    size_t i = 13;
    if (in[10] & 0x80) i += (size_t)3 << ((in[10] & 7) + 1);
    if (i > n) return -1;
    u8 *o = malloc(n + xn + 300);
    if (!o) return -1;
    memcpy(o, in, i);
    size_t k = i;
    o[k++] = 0x21; o[k++] = 0xFF; o[k++] = 11;
    memcpy(o + k, "XMP DataXMP", 11); k += 11;
    memcpy(o + k, x, xn); k += xn;
    o[k++] = 1;
    for (int v = 255; v >= 0; v--) o[k++] = (u8)v;
    o[k++] = 0;
    memcpy(o + k, in + i, n - i);
    k += n - i;
    *out = o; *on = k;
    return 0;
}
int xmp_embed(const char *uti, const u8 *in, size_t n, const u8 *x, size_t xn, u8 **out, size_t *on) {
    if (!uti) return -1;
    if (!strcmp(uti, "public.jpeg")) return emb_jpeg(in, n, x, xn, out, on);
    if (!strcmp(uti, "public.png")) return emb_png(in, n, x, xn, out, on);
    if (!strcmp(uti, "public.tiff")) return emb_tiff(in, n, x, xn, out, on);
    if (!strcmp(uti, "com.compuserve.gif")) return emb_gif(in, n, x, xn, out, on);
    return -1;
}
