/*
 * mOS libSystem — NSXMLDocument / NSXMLElement / NSXMLNode (a árvore XML do Foundation do macOS). O Sparkle lê o
 * appcast assim: initWithData:options:error:, nodesForXPath:@"/rss/channel/item", depois children, name,
 * stringValue, attributeForName: e elementsForName: em cada item. O parser cobre o XML que aparece na prática:
 * elementos, atributos com aspas simples ou duplas, texto, CDATA, comentários, instruções de processamento, DOCTYPE
 * (ignorado) e as entidades predefinidas e numéricas. Prefixos de namespace ficam no nome ("sparkle:version"), como no
 * NSXMLNode da Apple (name = qualificado, localName = sem prefixo, prefix = o prefixo).
 * XPath: caminhos com /, // (descendentes), ., .., *, nome, @atributo e text() — sem predicados nem funções.
 */
#include "foundation.h"

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);
void *memcpy(void *d, const void *s, size_t n);
void *malloc(size_t n);
void *realloc(void *p, size_t n);
void free(void *p);
extern id objc_retain(id), objc_autorelease(id);
void objc_release(id);

enum { K_INVALID, K_DOCUMENT, K_ELEMENT, K_ATTRIBUTE, K_NAMESPACE, K_PI, K_COMMENT, K_TEXT };   /* NSXMLNodeKind */

typedef struct xnode {
    cx_class_t *isa;
    int kind;
    char *name;                 /* elementos, atributos, PI */
    char *value;                /* texto, comentário, CDATA, valor de atributo */
    struct xnode *parent;       /* sem retenção: o pai vive enquanto o documento vive */
    struct xnode **kids; u32 nkids, capk;
    struct xnode **attrs; u32 nattrs, capa;
} xnode_t;

cx_class_t NSXMLNode_c __asm__("OBJC_CLASS_$_NSXMLNode"), NSXMLNode_m __asm__("OBJC_METACLASS_$_NSXMLNode");
cx_class_t NSXMLElement_c __asm__("OBJC_CLASS_$_NSXMLElement"), NSXMLElement_m __asm__("OBJC_METACLASS_$_NSXMLElement");
cx_class_t NSXMLDocument_c __asm__("OBJC_CLASS_$_NSXMLDocument"), NSXMLDocument_m __asm__("OBJC_METACLASS_$_NSXMLDocument");

static char *dupn(const char *s, u64 n) { char *r = malloc(n + 1); if (r) { memcpy(r, s, n); r[n] = 0; } return r; }
static xnode_t *mk(int kind) {
    cx_class_t *c = kind == K_ELEMENT ? &NSXMLElement_c : kind == K_DOCUMENT ? &NSXMLDocument_c : &NSXMLNode_c;
    xnode_t *n = (xnode_t *)cx_alloc_instance(c);
    n->kind = kind;
    return n;
}
static void add_kid(xnode_t *p, xnode_t *k) {
    if (p->nkids == p->capk) { p->capk = p->capk ? p->capk * 2 : 4; p->kids = realloc(p->kids, p->capk * sizeof *p->kids); }
    p->kids[p->nkids++] = k;
    k->parent = p;
}
static void add_attr(xnode_t *p, xnode_t *a) {
    if (p->nattrs == p->capa) { p->capa = p->capa ? p->capa * 2 : 4; p->attrs = realloc(p->attrs, p->capa * sizeof *p->attrs); }
    p->attrs[p->nattrs++] = a;
    a->parent = p;
}

/* ---- texto com entidades -> UTF-8 */
static u64 put_utf8(char *o, u32 cp) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xC0 | cp >> 6); o[1] = (char)(0x80 | (cp & 63)); return 2; }
    if (cp < 0x10000) { o[0] = (char)(0xE0 | cp >> 12); o[1] = (char)(0x80 | ((cp >> 6) & 63)); o[2] = (char)(0x80 | (cp & 63)); return 3; }
    o[0] = (char)(0xF0 | cp >> 18); o[1] = (char)(0x80 | ((cp >> 12) & 63)); o[2] = (char)(0x80 | ((cp >> 6) & 63)); o[3] = (char)(0x80 | (cp & 63));
    return 4;
}
static char *decode(const char *s, u64 n) {
    char *o = malloc(n + 1);
    if (!o) return 0;
    u64 k = 0;
    for (u64 i = 0; i < n;) {
        if (s[i] != '&') { o[k++] = s[i++]; continue; }
        u64 e = i + 1;
        while (e < n && e - i < 12 && s[e] != ';') e++;
        if (e >= n || s[e] != ';') { o[k++] = s[i++]; continue; }
        const char *t = s + i + 1; u64 tl = e - i - 1;
        u32 cp = 0; int ok = 1;
        if (tl == 2 && !strncmp(t, "lt", 2)) cp = '<';
        else if (tl == 2 && !strncmp(t, "gt", 2)) cp = '>';
        else if (tl == 3 && !strncmp(t, "amp", 3)) cp = '&';
        else if (tl == 4 && !strncmp(t, "quot", 4)) cp = '"';
        else if (tl == 4 && !strncmp(t, "apos", 4)) cp = '\'';
        else if (tl > 1 && t[0] == '#') {
            int hex = t[1] == 'x' || t[1] == 'X';
            for (u64 j = hex ? 2 : 1; j < tl; j++) {
                char ch = t[j];
                int d = ch >= '0' && ch <= '9' ? ch - '0' : hex && ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : hex && ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
                if (d < 0) { ok = 0; break; }
                cp = cp * (hex ? 16 : 10) + (u32)d;
                if (cp > 0x10FFFF) { ok = 0; break; }
            }
        } else ok = 0;
        if (!ok || !cp) { o[k++] = s[i++]; continue; }
        k += put_utf8(o + k, cp);       /* uma entidade nunca ocupa menos bytes que o caractere: cabe */
        i = e + 1;
    }
    o[k] = 0;
    return o;
}

/* ---- parser: devolve o documento ou NULL (XML malformado) */
static int is_sp(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static xnode_t *parse_xml(const char *s, u64 n) {
    xnode_t *doc = mk(K_DOCUMENT), *cur = doc;
    u64 i = 0;
    if (n >= 3 && (u8)s[0] == 0xEF && (u8)s[1] == 0xBB && (u8)s[2] == 0xBF) i = 3;   /* BOM */
    int depth = 0;
    while (i < n) {
        if (s[i] != '<') {
            u64 j = i;
            while (j < n && s[j] != '<') j++;
            if (cur != doc) {                                   /* texto fora da raiz (espaços) não entra */
                xnode_t *t = mk(K_TEXT);
                t->value = decode(s + i, j - i);
                add_kid(cur, t);
            }
            i = j;
            continue;
        }
        if (i + 4 <= n && !strncmp(s + i, "<!--", 4)) {
            u64 j = i + 4;
            while (j + 3 <= n && strncmp(s + j, "-->", 3)) j++;
            if (j + 3 > n) goto bad;
            xnode_t *c = mk(K_COMMENT); c->value = dupn(s + i + 4, j - i - 4); add_kid(cur, c);
            i = j + 3;
            continue;
        }
        if (i + 9 <= n && !strncmp(s + i, "<![CDATA[", 9)) {
            u64 j = i + 9;
            while (j + 3 <= n && strncmp(s + j, "]]>", 3)) j++;
            if (j + 3 > n) goto bad;
            xnode_t *t = mk(K_TEXT); t->value = dupn(s + i + 9, j - i - 9); add_kid(cur, t);
            i = j + 3;
            continue;
        }
        if (i + 2 <= n && s[i + 1] == '?') {                     /* <?xml ...?> e outras PIs */
            u64 j = i + 2;
            while (j + 2 <= n && strncmp(s + j, "?>", 2)) j++;
            if (j + 2 > n) goto bad;
            u64 k = i + 2;
            while (k < j && !is_sp(s[k])) k++;
            if (k - i - 2 != 3 || strncmp(s + i + 2, "xml", 3)) {
                xnode_t *p = mk(K_PI); p->name = dupn(s + i + 2, k - i - 2);
                while (k < j && is_sp(s[k])) k++;
                p->value = dupn(s + k, j - k); add_kid(cur, p);
            }
            i = j + 2;
            continue;
        }
        if (i + 2 <= n && s[i + 1] == '!') {                     /* DOCTYPE: pula, inclusive [ ... ] */
            u64 j = i + 2; int br = 0;
            while (j < n && (br || s[j] != '>')) { if (s[j] == '[') br++; else if (s[j] == ']') br--; j++; }
            i = j + 1;
            continue;
        }
        if (i + 2 <= n && s[i + 1] == '/') {                     /* fechamento */
            u64 j = i + 2, k;
            while (j < n && s[j] != '>') j++;
            if (j >= n || cur == doc) goto bad;
            k = j; while (k > i + 2 && is_sp(s[k - 1])) k--;
            if (strlen(cur->name) != k - i - 2 || strncmp(cur->name, s + i + 2, k - i - 2)) goto bad;
            cur = cur->parent; depth--;
            i = j + 1;
            continue;
        }
        /* abertura: <nome atr="v" ...> ou <nome .../> */
        u64 j = i + 1;
        while (j < n && !is_sp(s[j]) && s[j] != '>' && s[j] != '/') j++;
        if (j == i + 1 || j >= n) goto bad;
        xnode_t *e = mk(K_ELEMENT);
        e->name = dupn(s + i + 1, j - i - 1);
        for (;;) {
            while (j < n && is_sp(s[j])) j++;
            if (j >= n) goto bad;
            if (s[j] == '>' || s[j] == '/') break;
            u64 a = j;
            while (j < n && !is_sp(s[j]) && s[j] != '=' && s[j] != '>') j++;
            u64 an = j - a;
            while (j < n && is_sp(s[j])) j++;
            if (j >= n || s[j] != '=') goto bad;
            j++;
            while (j < n && is_sp(s[j])) j++;
            if (j >= n || (s[j] != '"' && s[j] != '\'')) goto bad;
            char q = s[j++];
            u64 v = j;
            while (j < n && s[j] != q) j++;
            if (j >= n) goto bad;
            xnode_t *at = mk(K_ATTRIBUTE);
            at->name = dupn(s + a, an);
            at->value = decode(s + v, j - v);
            add_attr(e, at);
            j++;
        }
        add_kid(cur, e);
        if (s[j] == '/') { if (j + 1 >= n || s[j + 1] != '>') goto bad; i = j + 2; }
        else { cur = e; depth++; i = j + 1; if (depth > 512) goto bad; }
    }
    if (cur != doc) goto bad;
    return doc;
bad:
    return 0;                                                   /* a árvore fica para o coletor do processo (pequena) */
}

/* ---- NSString <-> C */
static const char *cs(id s) { return s ? cx_cstr(s) : ""; }
static id str(const char *s) { return s ? cx_str(s) : 0; }

/* stringValue: texto do nó; de elemento/documento, o texto de todos os descendentes em ordem */
static void collect(xnode_t *n, char **buf, u64 *len, u64 *cap) {
    if (n->kind == K_TEXT && n->value) {
        u64 l = strlen(n->value);
        if (*len + l + 1 > *cap) { *cap = (*len + l + 1) * 2; *buf = realloc(*buf, *cap); }
        memcpy(*buf + *len, n->value, l); *len += l; (*buf)[*len] = 0;
        return;
    }
    for (u32 i = 0; i < n->nkids; i++) collect(n->kids[i], buf, len, cap);
}
static id n_string_value(id self, SEL c) {
    (void)c;
    xnode_t *n = (xnode_t *)self;
    if (n->kind == K_TEXT || n->kind == K_COMMENT || n->kind == K_ATTRIBUTE || n->kind == K_PI) return str(n->value ? n->value : "");
    char *b = malloc(1); u64 l = 0, cap = 1; b[0] = 0;
    collect(n, &b, &l, &cap);
    id r = str(b); free(b);
    return r;
}
static id n_name(id self, SEL c) { (void)c; xnode_t *n = (xnode_t *)self; return n->name ? str(n->name) : 0; }
static id n_local(id self, SEL c) { (void)c; xnode_t *n = (xnode_t *)self; if (!n->name) return 0; const char *p = strchr(n->name, ':'); return str(p ? p + 1 : n->name); }
static id n_prefix(id self, SEL c) { (void)c; xnode_t *n = (xnode_t *)self; if (!n->name) return str(""); const char *p = strchr(n->name, ':'); return p ? cx_str_n(n->name, (u64)(p - n->name)) : str(""); }
static u64 n_kind(id self, SEL c) { (void)c; return (u64)((xnode_t *)self)->kind; }
static id n_parent(id self, SEL c) { (void)c; return (id)((xnode_t *)self)->parent; }
static u64 n_child_count(id self, SEL c) { (void)c; return ((xnode_t *)self)->nkids; }
static id n_child_at(id self, SEL c, u64 i) { (void)c; xnode_t *n = (xnode_t *)self; return i < n->nkids ? (id)n->kids[i] : 0; }
static id arr_of(xnode_t **v, u32 nv) {
    id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    for (u32 i = 0; i < nv; i++) MSG(void, a, "addObject:", (id)v[i]);
    return a;
}
static id n_children(id self, SEL c) { (void)c; xnode_t *n = (xnode_t *)self; return n->nkids ? arr_of(n->kids, n->nkids) : 0; }
static id n_root_doc(id self, SEL c) { (void)c; xnode_t *n = (xnode_t *)self; while (n->parent) n = n->parent; return n->kind == K_DOCUMENT ? (id)n : 0; }
static u64 n_level(id self, SEL c) { (void)c; u64 l = 0; for (xnode_t *n = ((xnode_t *)self)->parent; n; n = n->parent) l++; return l; }
static u64 n_index(id self, SEL c) { (void)c; xnode_t *n = (xnode_t *)self, *p = n->parent; if (p) for (u32 i = 0; i < p->nkids; i++) if (p->kids[i] == n) return i; return 0; }

/* XMLString: serialização com escape mínimo */
static void out_s(char **b, u64 *l, u64 *cap, const char *s, int esc) {
    for (; *s; s++) {
        const char *r = 0;
        if (esc && *s == '<') r = "&lt;"; else if (esc && *s == '>') r = "&gt;"; else if (esc && *s == '&') r = "&amp;"; else if (esc == 2 && *s == '"') r = "&quot;";
        u64 rl = r ? strlen(r) : 1;
        if (*l + rl + 1 > *cap) { *cap = (*l + rl + 1) * 2; *b = realloc(*b, *cap); }
        if (r) { memcpy(*b + *l, r, rl); *l += rl; } else (*b)[(*l)++] = *s;
        (*b)[*l] = 0;
    }
}
static void ser(xnode_t *n, char **b, u64 *l, u64 *cap) {
    switch (n->kind) {
    case K_TEXT: out_s(b, l, cap, n->value ? n->value : "", 1); return;
    case K_COMMENT: out_s(b, l, cap, "<!--", 0); out_s(b, l, cap, n->value ? n->value : "", 0); out_s(b, l, cap, "-->", 0); return;
    case K_PI: out_s(b, l, cap, "<?", 0); out_s(b, l, cap, n->name, 0); out_s(b, l, cap, " ", 0); out_s(b, l, cap, n->value ? n->value : "", 0); out_s(b, l, cap, "?>", 0); return;
    case K_ATTRIBUTE: out_s(b, l, cap, n->name, 0); out_s(b, l, cap, "=\"", 0); out_s(b, l, cap, n->value ? n->value : "", 2); out_s(b, l, cap, "\"", 0); return;
    case K_ELEMENT:
        out_s(b, l, cap, "<", 0); out_s(b, l, cap, n->name, 0);
        for (u32 i = 0; i < n->nattrs; i++) { out_s(b, l, cap, " ", 0); ser(n->attrs[i], b, l, cap); }
        if (!n->nkids) { out_s(b, l, cap, "/>", 0); return; }
        out_s(b, l, cap, ">", 0);
        for (u32 i = 0; i < n->nkids; i++) ser(n->kids[i], b, l, cap);
        out_s(b, l, cap, "</", 0); out_s(b, l, cap, n->name, 0); out_s(b, l, cap, ">", 0);
        return;
    default:
        for (u32 i = 0; i < n->nkids; i++) ser(n->kids[i], b, l, cap);
    }
}
static id n_xml_string(id self, SEL c) {
    (void)c;
    char *b = malloc(1); u64 l = 0, cap = 1; b[0] = 0;
    ser((xnode_t *)self, &b, &l, &cap);
    id r = str(b); free(b);
    return r;
}
static id n_xml_string_opts(id self, SEL c, u64 o) { (void)o; return n_xml_string(self, c); }

/* ---- XPath (subconjunto) */
typedef struct { xnode_t **v; u32 n, cap; } nset_t;
static void ns_add(nset_t *s, xnode_t *x) {
    for (u32 i = 0; i < s->n; i++) if (s->v[i] == x) return;
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 16; s->v = realloc(s->v, s->cap * sizeof *s->v); }
    s->v[s->n++] = x;
}
static int name_match(xnode_t *x, const char *step, u64 sl) {
    if (x->kind != K_ELEMENT) return 0;
    if (sl == 1 && step[0] == '*') return 1;
    return strlen(x->name) == sl && !strncmp(x->name, step, sl);
}
static void desc_or_self(xnode_t *x, nset_t *o) { ns_add(o, x); for (u32 i = 0; i < x->nkids; i++) desc_or_self(x->kids[i], o); }
static id n_xpath(id self, SEL c, id path, id *err) {
    (void)c;
    if (err) *err = 0;
    const char *p = cs(path);
    nset_t cur = {0}, nxt = {0};
    xnode_t *ctx = (xnode_t *)self;
    if (*p == '/') { while (ctx->parent) ctx = ctx->parent; }
    ns_add(&cur, ctx);
    while (*p) {
        int deep = 0;
        if (*p == '/') { p++; if (*p == '/') { deep = 1; p++; } }
        const char *st = p;
        while (*p && *p != '/') p++;
        u64 sl = (u64)(p - st);
        if (!sl) continue;
        nxt.n = 0;
        for (u32 i = 0; i < cur.n; i++) {
            nset_t base = {0};
            if (deep) desc_or_self(cur.v[i], &base); else ns_add(&base, cur.v[i]);
            for (u32 b = 0; b < base.n; b++) {
                xnode_t *x = base.v[b];
                if (sl == 1 && st[0] == '.') ns_add(&nxt, x);
                else if (sl == 2 && st[0] == '.' && st[1] == '.') { if (x->parent) ns_add(&nxt, x->parent); }
                else if (sl == 6 && !strncmp(st, "text()", 6)) { for (u32 k = 0; k < x->nkids; k++) if (x->kids[k]->kind == K_TEXT) ns_add(&nxt, x->kids[k]); }
                else if (st[0] == '@') {
                    for (u32 k = 0; k < x->nattrs; k++)
                        if ((sl == 2 && st[1] == '*') || (strlen(x->attrs[k]->name) == sl - 1 && !strncmp(x->attrs[k]->name, st + 1, sl - 1))) ns_add(&nxt, x->attrs[k]);
                } else for (u32 k = 0; k < x->nkids; k++) if (name_match(x->kids[k], st, sl)) ns_add(&nxt, x->kids[k]);
            }
            free(base.v);
        }
        nset_t t = cur; cur = nxt; nxt = t;
    }
    id r = arr_of(cur.v, cur.n);
    free(cur.v); free(nxt.v);
    return r;
}

/* ---- NSXMLElement */
static id e_attributes(id self, SEL c) { (void)c; xnode_t *n = (xnode_t *)self; return n->nattrs ? arr_of(n->attrs, n->nattrs) : 0; }
static id e_attr_named(id self, SEL c, id name) {
    (void)c;
    xnode_t *n = (xnode_t *)self;
    const char *nm = cs(name);
    for (u32 i = 0; i < n->nattrs; i++) if (!strcmp(n->attrs[i]->name, nm)) return (id)n->attrs[i];
    return 0;
}
static id e_attr_local(id self, SEL c, id local, id uri) {
    (void)c; (void)uri;
    xnode_t *n = (xnode_t *)self;
    const char *nm = cs(local);
    for (u32 i = 0; i < n->nattrs; i++) { const char *p = strchr(n->attrs[i]->name, ':'); if (!strcmp(p ? p + 1 : n->attrs[i]->name, nm)) return (id)n->attrs[i]; }
    return 0;
}
static id e_elements_named(id self, SEL c, id name) {
    (void)c;
    xnode_t *n = (xnode_t *)self;
    const char *nm = cs(name);
    id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    for (u32 i = 0; i < n->nkids; i++) if (n->kids[i]->kind == K_ELEMENT && !strcmp(n->kids[i]->name, nm)) MSG(void, a, "addObject:", (id)n->kids[i]);
    return a;
}
static id e_elements_local(id self, SEL c, id local, id uri) {
    (void)c; (void)uri;
    xnode_t *n = (xnode_t *)self;
    const char *nm = cs(local);
    id a = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    for (u32 i = 0; i < n->nkids; i++) {
        xnode_t *k = n->kids[i];
        if (k->kind != K_ELEMENT) continue;
        const char *p = strchr(k->name, ':');
        if (!strcmp(p ? p + 1 : k->name, nm)) MSG(void, a, "addObject:", (id)k);
    }
    return a;
}
static id e_init_name(id self, SEL c, id name) { (void)c; xnode_t *n = (xnode_t *)self; n->kind = K_ELEMENT; n->name = dupn(cs(name), strlen(cs(name))); return self; }
static id e_init_name_str(id self, SEL c, id name, id value) {
    e_init_name(self, c, name);
    if (value) { xnode_t *t = mk(K_TEXT); t->value = dupn(cs(value), strlen(cs(value))); add_kid((xnode_t *)self, t); }
    return self;
}
static void e_add_child(id self, SEL c, id child) { (void)c; if (child) add_kid((xnode_t *)self, (xnode_t *)objc_retain(child)); }
static void e_add_attr(id self, SEL c, id a) { (void)c; if (a) add_attr((xnode_t *)self, (xnode_t *)objc_retain(a)); }
static id node_attr_with(id cls, SEL c, id name, id value) {
    (void)cls; (void)c;
    xnode_t *a = mk(K_ATTRIBUTE);
    a->name = dupn(cs(name), strlen(cs(name)));
    a->value = dupn(cs(value), strlen(cs(value)));
    return objc_autorelease((id)a);
}
static id node_elem_with(id cls, SEL c, id name) { (void)cls; return objc_autorelease(e_init_name((id)mk(K_ELEMENT), c, name)); }
static id node_text_with(id cls, SEL c, id value) { (void)cls; (void)c; xnode_t *t = mk(K_TEXT); t->value = dupn(cs(value), strlen(cs(value))); return objc_autorelease((id)t); }

/* ---- NSXMLDocument */
static id doc_take(id self, xnode_t *d) {
    xnode_t *me = (xnode_t *)self;
    me->kind = K_DOCUMENT;
    for (u32 i = 0; i < d->nkids; i++) add_kid(me, d->kids[i]);
    free(d->kids);                                                 /* d (a casca) não é mais usada */
    return self;
}
static id parse_error(const char *why) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(why), cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSXMLParserErrorDomain"), (long)1, ui);
}
static id doc_init_bytes(id self, const char *s, u64 n, id *err) {
    xnode_t *d = s ? parse_xml(s, n) : 0;
    if (!d) { if (err) *err = parse_error("XML malformado"); objc_release(self); return 0; }
    if (err) *err = 0;
    return doc_take(self, d);
}
static id doc_init_data(id self, SEL c, id data, u64 opts, id *err) {
    (void)c; (void)opts;
    const char *b = data ? MSG0(const char *, data, "bytes") : 0;
    u64 n = data ? MSG0(u64, data, "length") : 0;
    return doc_init_bytes(self, b, n, err);
}
static id doc_init_string(id self, SEL c, id s, u64 opts, id *err) { (void)c; (void)opts; const char *t = cs(s); return doc_init_bytes(self, t, strlen(t), err); }
static id doc_init_url(id self, SEL c, id url, u64 opts, id *err) {
    id data = url ? MSG(id, (id)cx_class_named("NSData"), "dataWithContentsOfURL:", url) : 0;
    return doc_init_data(self, c, data, opts, err);
}
static id doc_init_root(id self, SEL c, id root) { (void)c; xnode_t *me = (xnode_t *)self; me->kind = K_DOCUMENT; if (root) add_kid(me, (xnode_t *)objc_retain(root)); return self; }
static id doc_root(id self, SEL c) {
    (void)c;
    xnode_t *me = (xnode_t *)self;
    for (u32 i = 0; i < me->nkids; i++) if (me->kids[i]->kind == K_ELEMENT) return (id)me->kids[i];
    return 0;
}
static void doc_set_root(id self, SEL c, id root) {
    (void)c;
    xnode_t *me = (xnode_t *)self;
    for (u32 i = 0; i < me->nkids; i++) if (me->kids[i]->kind == K_ELEMENT) { me->kids[i] = (xnode_t *)objc_retain(root); ((xnode_t *)root)->parent = me; return; }
    if (root) add_kid(me, (xnode_t *)objc_retain(root));
}
static id doc_xml_data(id self, SEL c) {
    id s = n_xml_string(self, c);
    return MSG(id, s, "dataUsingEncoding:", (u64)4);   /* NSUTF8StringEncoding */
}
static id doc_xml_data_opts(id self, SEL c, u64 o) { (void)o; return doc_xml_data(self, c); }

void cx_xmldom_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&NSXMLNode_c, &NSXMLNode_m, "NSXMLNode", ob, (u32)(sizeof(xnode_t) - 8));
    cx_define_class(&NSXMLElement_c, &NSXMLElement_m, "NSXMLElement", &NSXMLNode_c, 0);
    cx_define_class(&NSXMLDocument_c, &NSXMLDocument_m, "NSXMLDocument", &NSXMLNode_c, 0);
    static const struct { const char *sel; void *fn; } nm[] = {
        {"stringValue", (void *)n_string_value}, {"objectValue", (void *)n_string_value}, {"name", (void *)n_name},
        {"localName", (void *)n_local}, {"prefix", (void *)n_prefix}, {"kind", (void *)n_kind}, {"parent", (void *)n_parent},
        {"childCount", (void *)n_child_count}, {"childAtIndex:", (void *)n_child_at}, {"children", (void *)n_children},
        {"rootDocument", (void *)n_root_doc}, {"level", (void *)n_level}, {"index", (void *)n_index},
        {"XMLString", (void *)n_xml_string}, {"XMLStringWithOptions:", (void *)n_xml_string_opts},
        {"description", (void *)n_xml_string}, {"nodesForXPath:error:", (void *)n_xpath},
    };
    for (unsigned i = 0; i < sizeof nm / sizeof nm[0]; i++) cx_add_method(&NSXMLNode_c, nm[i].sel, nm[i].fn);
    cx_add_cmethod(&NSXMLNode_c, "attributeWithName:stringValue:", (void *)node_attr_with);
    cx_add_cmethod(&NSXMLNode_c, "elementWithName:", (void *)node_elem_with);
    cx_add_cmethod(&NSXMLNode_c, "textWithStringValue:", (void *)node_text_with);
    static const struct { const char *sel; void *fn; } em[] = {
        {"attributes", (void *)e_attributes}, {"attributeForName:", (void *)e_attr_named},
        {"attributeForLocalName:URI:", (void *)e_attr_local}, {"elementsForName:", (void *)e_elements_named},
        {"elementsForLocalName:URI:", (void *)e_elements_local}, {"initWithName:", (void *)e_init_name},
        {"initWithName:stringValue:", (void *)e_init_name_str}, {"addChild:", (void *)e_add_child},
        {"addAttribute:", (void *)e_add_attr},
    };
    for (unsigned i = 0; i < sizeof em / sizeof em[0]; i++) cx_add_method(&NSXMLElement_c, em[i].sel, em[i].fn);
    static const struct { const char *sel; void *fn; } dm[] = {
        {"initWithData:options:error:", (void *)doc_init_data}, {"initWithXMLString:options:error:", (void *)doc_init_string},
        {"initWithContentsOfURL:options:error:", (void *)doc_init_url}, {"initWithRootElement:", (void *)doc_init_root},
        {"rootElement", (void *)doc_root}, {"setRootElement:", (void *)doc_set_root},
        {"XMLData", (void *)doc_xml_data}, {"XMLDataWithOptions:", (void *)doc_xml_data_opts},
    };
    for (unsigned i = 0; i < sizeof dm / sizeof dm[0]; i++) cx_add_method(&NSXMLDocument_c, dm[i].sel, dm[i].fn);
}
