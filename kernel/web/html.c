/* mOS — Horizonte: tokenizador HTML5 (tolerante) e construção da árvore DOM */
#include "web.h"

static const char *tag_names[T_COUNT] = {
    "", "html", "head", "body", "title", "meta", "link", "style", "script", "base", "noscript",
    "div", "p", "span", "a", "b", "strong", "i", "em", "u", "s", "strike", "del", "ins", "small", "big",
    "sub", "sup", "code", "kbd", "samp", "tt", "var", "pre", "xmp", "listing", "plaintext", "br", "wbr", "hr",
    "h1", "h2", "h3", "h4", "h5", "h6", "ul", "ol", "li", "dl", "dt", "dd", "menu", "dir",
    "table", "caption", "thead", "tbody", "tfoot", "tr", "td", "th", "col", "colgroup",
    "img", "form", "input", "button", "select", "option", "optgroup", "textarea", "label", "fieldset", "legend",
    "blockquote", "center", "font", "nobr", "address", "article", "aside", "footer", "header", "main", "nav",
    "section", "figure", "figcaption", "details", "summary", "hgroup", "mark", "cite", "abbr", "q", "dfn",
    "iframe", "svg", "math", "object", "embed", "video", "audio", "canvas", "template", "area", "map",
    "param", "source", "track", "noembed", "noframes", "frameset", "frame", "picture", "time", "data", "dialog",
};

int tag_lookup(const char *name) {
    for (int i = 1; i < T_COUNT; i++)
        if (tag_names[i][0] == name[0] && !strcmp(tag_names[i], name)) return i;
    return T_UNKNOWN;
}

static bool is_void(int t) {
    switch (t) {
    case T_BR: case T_WBR: case T_HR: case T_IMG: case T_INPUT: case T_META: case T_LINK: case T_BASE: case T_COL:
    case T_AREA: case T_EMBED: case T_PARAM: case T_SOURCE: case T_TRACK: case T_FRAME: return true;
    }
    return false;
}

/* elementos cuja abertura fecha um <p> aberto */
static bool closes_p(int t) {
    switch (t) {
    case T_ADDRESS: case T_ARTICLE: case T_ASIDE: case T_BLOCKQUOTE: case T_CENTER: case T_DETAILS: case T_DIALOG:
    case T_DIR: case T_DIV: case T_DL: case T_FIELDSET: case T_FIGCAPTION: case T_FIGURE: case T_FOOTER: case T_FORM:
    case T_H1: case T_H2: case T_H3: case T_H4: case T_H5: case T_H6: case T_HEADER: case T_HGROUP: case T_HR:
    case T_MAIN: case T_MENU: case T_NAV: case T_OL: case T_P: case T_PRE: case T_SECTION: case T_TABLE: case T_UL:
    case T_LI: case T_DD: case T_DT: case T_XMP: case T_LISTING: case T_PLAINTEXT: case T_SUMMARY: return true;
    }
    return false;
}

static bool is_heading(int t) { return t >= T_H1 && t <= T_H6; }
static bool scope_boundary(int t) {
    return t == T_TABLE || t == T_TD || t == T_TH || t == T_CAPTION || t == T_HTML || t == T_TEMPLATE || t == T_OBJECT;
}

/* ============================================================ entidades */
typedef struct { const char *n; u16 cp; } ent_t;
static const ent_t ents[] = {
    {"amp", '&'}, {"lt", '<'}, {"gt", '>'}, {"quot", '"'}, {"apos", '\''}, {"nbsp", 0xA0}, {"copy", 0xA9}, {"reg", 0xAE},
    {"trade", 0x2122}, {"hellip", 0x2026}, {"mdash", 0x2014}, {"ndash", 0x2013}, {"lsquo", 0x2018}, {"rsquo", 0x2019},
    {"sbquo", 0x201A}, {"ldquo", 0x201C}, {"rdquo", 0x201D}, {"bdquo", 0x201E}, {"laquo", 0xAB}, {"raquo", 0xBB},
    {"lsaquo", 0x2039}, {"rsaquo", 0x203A}, {"bull", 0x2022}, {"middot", 0xB7}, {"deg", 0xB0}, {"plusmn", 0xB1},
    {"times", 0xD7}, {"divide", 0xF7}, {"euro", 0x20AC}, {"pound", 0xA3}, {"yen", 0xA5}, {"cent", 0xA2}, {"sect", 0xA7},
    {"para", 0xB6}, {"iexcl", 0xA1}, {"iquest", 0xBF}, {"shy", 0xAD}, {"ordf", 0xAA}, {"ordm", 0xBA}, {"sup1", 0xB9},
    {"sup2", 0xB2}, {"sup3", 0xB3}, {"frac14", 0xBC}, {"frac12", 0xBD}, {"frac34", 0xBE}, {"micro", 0xB5},
    {"not", 0xAC}, {"macr", 0xAF}, {"acute", 0xB4}, {"cedil", 0xB8}, {"uml", 0xA8}, {"curren", 0xA4}, {"brvbar", 0xA6},
    {"aacute", 0xE1}, {"Aacute", 0xC1}, {"agrave", 0xE0}, {"Agrave", 0xC0}, {"atilde", 0xE3}, {"Atilde", 0xC3},
    {"acirc", 0xE2}, {"Acirc", 0xC2}, {"auml", 0xE4}, {"Auml", 0xC4}, {"aring", 0xE5}, {"Aring", 0xC5}, {"aelig", 0xE6},
    {"AElig", 0xC6}, {"ccedil", 0xE7}, {"Ccedil", 0xC7}, {"eacute", 0xE9}, {"Eacute", 0xC9}, {"egrave", 0xE8},
    {"Egrave", 0xC8}, {"ecirc", 0xEA}, {"Ecirc", 0xCA}, {"euml", 0xEB}, {"Euml", 0xCB}, {"iacute", 0xED}, {"Iacute", 0xCD},
    {"igrave", 0xEC}, {"Igrave", 0xCC}, {"icirc", 0xEE}, {"Icirc", 0xCE}, {"iuml", 0xEF}, {"Iuml", 0xCF}, {"ntilde", 0xF1},
    {"Ntilde", 0xD1}, {"oacute", 0xF3}, {"Oacute", 0xD3}, {"ograve", 0xF2}, {"Ograve", 0xD2}, {"ocirc", 0xF4},
    {"Ocirc", 0xD4}, {"otilde", 0xF5}, {"Otilde", 0xD5}, {"ouml", 0xF6}, {"Ouml", 0xD6}, {"oslash", 0xF8},
    {"Oslash", 0xD8}, {"uacute", 0xFA}, {"Uacute", 0xDA}, {"ugrave", 0xF9}, {"Ugrave", 0xD9}, {"ucirc", 0xFB},
    {"Ucirc", 0xDB}, {"uuml", 0xFC}, {"Uuml", 0xDC}, {"yacute", 0xFD}, {"Yacute", 0xDD}, {"yuml", 0xFF}, {"szlig", 0xDF},
    {"eth", 0xF0}, {"ETH", 0xD0}, {"thorn", 0xFE}, {"THORN", 0xDE}, {"oelig", 0x153}, {"OElig", 0x152},
    {"scaron", 0x161}, {"Scaron", 0x160}, {"fnof", 0x192}, {"circ", 0x2C6}, {"tilde", 0x2DC}, {"ensp", 0x2002},
    {"emsp", 0x2003}, {"thinsp", 0x2009}, {"zwnj", 0x200C}, {"zwj", 0x200D}, {"lrm", 0x200E}, {"rlm", 0x200F},
    {"dagger", 0x2020}, {"Dagger", 0x2021}, {"permil", 0x2030}, {"prime", 0x2032}, {"Prime", 0x2033}, {"oline", 0x203E},
    {"larr", 0x2190}, {"uarr", 0x2191}, {"rarr", 0x2192}, {"darr", 0x2193}, {"harr", 0x2194}, {"crarr", 0x21B5},
    {"lArr", 0x21D0}, {"rArr", 0x21D2}, {"hArr", 0x21D4}, {"forall", 0x2200}, {"part", 0x2202}, {"exist", 0x2203},
    {"empty", 0x2205}, {"nabla", 0x2207}, {"isin", 0x2208}, {"notin", 0x2209}, {"prod", 0x220F}, {"sum", 0x2211},
    {"minus", 0x2212}, {"radic", 0x221A}, {"infin", 0x221E}, {"and", 0x2227}, {"or", 0x2228}, {"cap", 0x2229},
    {"cup", 0x222A}, {"int", 0x222B}, {"there4", 0x2234}, {"sim", 0x223C}, {"asymp", 0x2248}, {"ne", 0x2260},
    {"equiv", 0x2261}, {"le", 0x2264}, {"ge", 0x2265}, {"sub", 0x2282}, {"sup", 0x2283}, {"loz", 0x25CA},
    {"spades", 0x2660}, {"clubs", 0x2663}, {"hearts", 0x2665}, {"diams", 0x2666}, {"alpha", 0x3B1}, {"beta", 0x3B2},
    {"gamma", 0x3B3}, {"delta", 0x3B4}, {"epsilon", 0x3B5}, {"lambda", 0x3BB}, {"mu", 0x3BC}, {"pi", 0x3C0},
    {"sigma", 0x3C3}, {"omega", 0x3C9}, {"Delta", 0x394}, {"Omega", 0x3A9}, {"Sigma", 0x3A3}, {"check", 0x2713},
    {"star", 0x2606}, {"starf", 0x2605}, {"rang", 0x27E9}, {"lang", 0x27E8}, {"vert", '|'}, {"colon", ':'},
    {"comma", ','}, {"period", '.'}, {"excl", '!'}, {"quest", '?'}, {"num", '#'}, {"dollar", '$'}, {"percnt", '%'},
    {"lpar", '('}, {"rpar", ')'}, {"ast", '*'}, {"plus", '+'}, {"sol", '/'}, {"semi", ';'}, {"equals", '='},
    {"lsqb", '['}, {"rsqb", ']'}, {"lowbar", '_'}, {"grave", '`'}, {"lcub", '{'}, {"rcub", '}'}, {"Tab", 9},
    {"NewLine", 10}, {"bsol", '\\'}, {"commat", '@'}, {"hyphen", 0x2010}, {"dash", 0x2010},
};
static const u16 c1_map[32] = {
    0x20AC, 0x81, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x8D, 0x017D, 0x8F,
    0x90, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x9D, 0x017E, 0x0178};

/* decodifica uma entidade em s (após '&'); devolve bytes consumidos (0 = não é entidade) */
static int decode_entity(const char *s, const char *e, bool in_attr, u32 *cp) {
    if (s < e && *s == '#') {
        const char *p = s + 1;
        u32 v = 0;
        int nd = 0;
        if (p < e && (*p == 'x' || *p == 'X')) {
            p++;
            while (p < e && nd < 8) {
                int c = tolower(*p);
                if (c >= '0' && c <= '9') v = v * 16 + (u32)(c - '0');
                else if (c >= 'a' && c <= 'f') v = v * 16 + (u32)(c - 'a' + 10);
                else break;
                p++; nd++;
            }
        } else {
            while (p < e && isdigit(*p) && nd < 9) { v = v * 10 + (u32)(*p - '0'); p++; nd++; }
        }
        if (!nd) return 0;
        if (p < e && *p == ';') p++;
        if (v >= 0x80 && v < 0xA0) v = c1_map[v - 0x80];
        if (v == 0 || v > 0x10FFFF || (v >= 0xD800 && v < 0xE000)) v = 0xFFFD;
        *cp = v;
        return (int)(p - s);
    }
    char name[12];
    int n = 0;
    while (s + n < e && n < 11 && isalnum(s[n])) { name[n] = s[n]; n++; }
    name[n] = 0;
    if (!n) return 0;
    bool semi = s + n < e && s[n] == ';';
    /* busca o nome mais longo que casa (entidades legadas sem ';') */
    for (int len = n; len >= 2; len--) {
        for (u32 i = 0; i < ARRAY_LEN(ents); i++) {
            if ((int)strlen(ents[i].n) != len || strncmp(ents[i].n, name, (usize)len)) continue;
            if (len == n && semi) { *cp = ents[i].cp; return len + 1; }
            if (in_attr) return 0;
            if (len == n || !isalnum(s[len])) {
                /* aceita legado só para as entidades mais comuns */
                if (len == n && !semi && strcmp(name, "amp") && strcmp(name, "lt") && strcmp(name, "gt") && strcmp(name, "nbsp") &&
                    strcmp(name, "quot") && strcmp(name, "copy") && strcmp(name, "reg")) return 0;
                *cp = ents[i].cp;
                return len;
            }
            if (!semi && len < n) {
                if (strcmp(ents[i].n, "amp") && strcmp(ents[i].n, "lt") && strcmp(ents[i].n, "gt") && strcmp(ents[i].n, "nbsp") &&
                    strcmp(ents[i].n, "copy")) continue;
                *cp = ents[i].cp;
                return len;
            }
        }
        if (semi) break; /* com ';' só aceita o nome completo */
    }
    return 0;
}

/* acrescenta texto decodificando entidades */
static void put_decoded(sbuf_t *b, const char *s, const char *e, bool in_attr) {
    while (s < e) {
        const char *amp = s;
        while (amp < e && *amp != '&' && *amp != '\r' && *amp != 0) amp++;
        sb_putn(b, s, (int)(amp - s));
        if (amp >= e) break;
        if (*amp == '\r') { if (amp + 1 >= e || amp[1] != '\n') sb_putc(b, '\n'); s = amp + 1; continue; }
        if (*amp == 0) { s = amp + 1; continue; }
        u32 cp;
        int k = decode_entity(amp + 1, e, in_attr, &cp);
        if (k) {
            char u[4];
            int n = utf8_encode(cp, u);
            sb_putn(b, u, n);
            s = amp + 1 + k;
        } else {
            sb_putc(b, '&');
            s = amp + 1;
        }
    }
}

/* ============================================================ DOM helpers */
const char *node_attr(node_t *n, const char *name) {
    if (!n || n->type != N_ELEM) return NULL;
    for (int i = 0; i < n->nattrs; i++) if (!strcmp(n->attrs[i].name, name)) return n->attrs[i].value;
    return NULL;
}

node_t *node_find_tag(node_t *r, int tag) {
    for (node_t *c = r ? r->first : NULL; c; c = c->next) {
        if (c->type == N_ELEM && c->tag == tag) return c;
        node_t *f = node_find_tag(c, tag);
        if (f) return f;
    }
    return NULL;
}

node_t *node_by_id(node_t *r, const char *id) {
    for (node_t *c = r ? r->first : NULL; c; c = c->next) {
        if (c->type != N_ELEM) continue;
        const char *v = node_attr(c, "id");
        if (v && !strcmp(v, id)) return c;
        if (c->tag == T_A) { v = node_attr(c, "name"); if (v && !strcmp(v, id)) return c; }
        node_t *f = node_by_id(c, id);
        if (f) return f;
    }
    return NULL;
}

void node_text_content(node_t *n, sbuf_t *out) {
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type == N_TEXT) sb_putn(out, c->text, c->len);
        else if (c->type == N_ELEM && c->tag != T_SCRIPT && c->tag != T_STYLE) node_text_content(c, out);
    }
}

/* ============================================================ construtor da árvore */
typedef struct {
    doc_t *d;
    const char *s, *e;
    node_t *stack[512];
    int sp;
    bool in_body;
    sbuf_t text;
    sbuf_t tmp;
    int count;
    bool frag;             /* analisando fragmento (innerHTML) */
} parser_t;

static node_t *new_node(parser_t *P, int type) {
    node_t *n = arena_alloc(&P->d->arena, sizeof(node_t));
    n->type = (u8)type;
    n->ay = -1;
    n->index = P->count++;
    return n;
}

static void append(node_t *parent, node_t *n) {
    n->parent = parent;
    n->prev = parent->last;
    if (parent->last) parent->last->next = n; else parent->first = n;
    parent->last = n;
}

static node_t *top(parser_t *P) { return P->stack[P->sp - 1]; }

static void flush_text(parser_t *P) {
    if (!P->text.len) return;
    bool ws = true;
    for (int i = 0; i < P->text.len; i++) if (!isspace((u8)P->text.p[i])) { ws = false; break; }
    if (!P->in_body && ws) { P->text.len = 0; return; }
    if (!ws) P->in_body = true;
    node_t *parent = top(P);
    /* texto solto dentro de tabela/linha: só espaço é descartado */
    if (ws && (parent->tag == T_TABLE || parent->tag == T_TR || parent->tag == T_TBODY || parent->tag == T_THEAD ||
               parent->tag == T_TFOOT || parent->tag == T_SELECT || parent->tag == T_UL || parent->tag == T_OL)) {
        P->text.len = 0;
        return;
    }
    node_t *last = parent->last;
    if (last && last->type == N_TEXT) {
        char *nt = arena_alloc(&P->d->arena, (usize)last->len + P->text.len + 1);
        memcpy(nt, last->text, (usize)last->len);
        memcpy(nt + last->len, P->text.p, (usize)P->text.len);
        last->text = nt;
        last->len += P->text.len;
    } else {
        node_t *t = new_node(P, N_TEXT);
        t->text = arena_strndup(&P->d->arena, P->text.p, P->text.len);
        t->len = P->text.len;
        append(parent, t);
    }
    P->text.len = 0;
}

static void add_attr(parser_t *P, node_t *n, const char *name, int nl, const char *val) {
    for (int i = 0; i < n->nattrs; i++) if ((int)strlen(n->attrs[i].name) == nl && !strncmp(n->attrs[i].name, name, (usize)nl)) return;
    if (n->nattrs == n->acap) {
        int nc = n->acap ? n->acap * 2 : 4;
        attr_t *na = arena_alloc(&P->d->arena, sizeof(attr_t) * (usize)nc);
        if (n->attrs) memcpy(na, n->attrs, sizeof(attr_t) * (usize)n->nattrs);
        n->attrs = na;
        n->acap = nc;
    }
    n->attrs[n->nattrs].name = arena_strndup(&P->d->arena, name, nl);
    n->attrs[n->nattrs].value = val;
    n->nattrs++;
}

static void pop_to(parser_t *P, int idx) { if (idx >= 2) P->sp = idx; }

static int find_in_scope(parser_t *P, int tag, const char *name) {
    for (int i = P->sp - 1; i >= 2; i--) {
        node_t *n = P->stack[i];
        if (tag ? n->tag == tag : (n->tag == T_UNKNOWN && !strcmp(n->name, name))) return i;
        if (scope_boundary(n->tag) && tag != n->tag) return -1;
    }
    return -1;
}

static void close_p(parser_t *P) {
    for (int i = P->sp - 1; i >= 2; i--) {
        int t = P->stack[i]->tag;
        if (t == T_P) { pop_to(P, i); return; }
        if (scope_boundary(t) || t == T_BUTTON) return;
    }
}

/* índice do elemento de tabela mais próximo (ou -1) */
static int nearest_table(parser_t *P) {
    for (int i = P->sp - 1; i >= 2; i--) if (P->stack[i]->tag == T_TABLE) return i;
    return -1;
}

static void insert_implicit(parser_t *P, int tag) {
    node_t *n = new_node(P, N_ELEM);
    n->tag = (u16)tag;
    n->name = tag_names[tag];
    append(top(P), n);
    if (P->sp < (int)ARRAY_LEN(P->stack)) P->stack[P->sp++] = n;
}

static void handle_start(parser_t *P, node_t *n, bool selfclose) {
    int t = n->tag;
    if (P->frag && (t == T_HTML || t == T_BODY || t == T_HEAD)) return;
    if (P->frag) goto body_mode;
    if (t == T_HTML || t == T_BODY || t == T_HEAD) {
        node_t *target = t == T_HTML ? P->d->html : t == T_BODY ? P->d->body : P->d->head;
        for (int i = 0; i < n->nattrs; i++) add_attr(P, target, n->attrs[i].name, (int)strlen(n->attrs[i].name), n->attrs[i].value);
        if (t == T_BODY) P->in_body = true;
        return;
    }
    bool headish = t == T_TITLE || t == T_META || t == T_LINK || t == T_STYLE || t == T_SCRIPT || t == T_BASE || t == T_TEMPLATE;
    if (!P->in_body && headish) { append(P->d->head, n); return; }
    if (t == T_TITLE) { append(P->d->head, n); return; }
body_mode:
    P->in_body = true;
    if (closes_p(t)) close_p(P);
    switch (t) {
    case T_LI:
        for (int i = P->sp - 1; i >= 2; i--) {
            int k = P->stack[i]->tag;
            if (k == T_LI) { pop_to(P, i); break; }
            if (k == T_UL || k == T_OL || k == T_MENU || k == T_DIR || scope_boundary(k)) break;
        }
        break;
    case T_DT: case T_DD:
        for (int i = P->sp - 1; i >= 2; i--) {
            int k = P->stack[i]->tag;
            if (k == T_DT || k == T_DD) { pop_to(P, i); break; }
            if (k == T_DL || scope_boundary(k)) break;
        }
        break;
    case T_OPTION:
        if (top(P)->tag == T_OPTION) P->sp--;
        break;
    case T_OPTGROUP:
        if (top(P)->tag == T_OPTION) P->sp--;
        if (top(P)->tag == T_OPTGROUP) P->sp--;
        break;
    case T_TR: case T_THEAD: case T_TBODY: case T_TFOOT: {
        int tb = nearest_table(P);
        if (tb >= 0) {
            for (int i = P->sp - 1; i > tb; i--) {
                int k = P->stack[i]->tag;
                if (k == T_TR || k == T_TD || k == T_TH || k == T_CAPTION || (t != T_TR && (k == T_THEAD || k == T_TBODY || k == T_TFOOT))) {
                    pop_to(P, i);
                }
            }
        }
        break;
    }
    case T_TD: case T_TH: {
        int tb = nearest_table(P);
        if (tb >= 0) {
            int tr = -1;
            for (int i = P->sp - 1; i > tb; i--) {
                int k = P->stack[i]->tag;
                if (k == T_TD || k == T_TH) { pop_to(P, i); }
            }
            for (int i = P->sp - 1; i > tb; i--) if (P->stack[i]->tag == T_TR) { tr = i; break; }
            if (tr < 0) {
                /* fecha o que estiver aberto abaixo da seção e cria <tr> implícito */
                for (int i = P->sp - 1; i > tb; i--) {
                    int k = P->stack[i]->tag;
                    if (k != T_THEAD && k != T_TBODY && k != T_TFOOT) { pop_to(P, i); }
                }
                insert_implicit(P, T_TR);
            } else if (tr < P->sp - 1) {
                pop_to(P, tr + 1);
            }
        }
        break;
    }
    case T_CAPTION: case T_COLGROUP: {
        int tb = nearest_table(P);
        if (tb >= 0) pop_to(P, tb + 1);
        break;
    }
    case T_A: {
        int i = find_in_scope(P, T_A, NULL);
        if (i >= 0) pop_to(P, i);
        break;
    }
    case T_BUTTON: {
        int i = find_in_scope(P, T_BUTTON, NULL);
        if (i >= 0) pop_to(P, i);
        break;
    }
    case T_NOBR: {
        int i = find_in_scope(P, T_NOBR, NULL);
        if (i >= 0) pop_to(P, i);
        break;
    }
    case T_FORM: break;
    default:
        if (is_heading(t) && is_heading(top(P)->tag)) P->sp--;
        break;
    }
    node_t *parent = top(P);
    if (parent->tag == T_SELECT && t != T_OPTION && t != T_OPTGROUP) {
        if (t == T_INPUT || t == T_TEXTAREA || t == T_SELECT) { P->sp--; parent = top(P); }
        else return;
    }
    append(parent, n);
    bool known = t != T_UNKNOWN;
    if (is_void(t) || (selfclose && !known)) return;
    if (P->sp < (int)ARRAY_LEN(P->stack)) P->stack[P->sp++] = n;
}

static void handle_end(parser_t *P, int t, const char *name) {
    switch (t) {
    case T_HTML: case T_BODY: case T_HEAD: return;
    case T_P: { int i = find_in_scope(P, T_P, NULL); if (i >= 0) pop_to(P, i); return; }
    case T_LI: case T_DT: case T_DD: {
        for (int i = P->sp - 1; i >= 2; i--) {
            int k = P->stack[i]->tag;
            if (k == t) { pop_to(P, i); return; }
            if (k == T_UL || k == T_OL || k == T_DL || scope_boundary(k)) return;
        }
        return;
    }
    case T_H1: case T_H2: case T_H3: case T_H4: case T_H5: case T_H6:
        for (int i = P->sp - 1; i >= 2; i--) {
            int k = P->stack[i]->tag;
            if (is_heading(k)) { pop_to(P, i); return; }
            if (scope_boundary(k)) return;
        }
        return;
    case T_TABLE:
        for (int i = P->sp - 1; i >= 2; i--) if (P->stack[i]->tag == T_TABLE) { pop_to(P, i); return; }
        return;
    case T_TR: case T_TD: case T_TH: case T_THEAD: case T_TBODY: case T_TFOOT: case T_CAPTION: {
        int tb = nearest_table(P);
        for (int i = P->sp - 1; i > tb && i >= 2; i--) if (P->stack[i]->tag == t) { pop_to(P, i); return; }
        return;
    }
    default: {
        int i = find_in_scope(P, t, name);
        if (i >= 0) pop_to(P, i);
    }
    }
}

/* lê o conteúdo bruto até </name>; devolve ponteiro após o fechamento */
static const char *raw_until(parser_t *P, const char *p, const char *name, const char **content_end) {
    int nl = (int)strlen(name);
    const char *q = p;
    while (q < P->e) {
        if (q[0] == '<' && q + 1 < P->e && q[1] == '/' && q + 2 + nl <= P->e && !strncasecmp(q + 2, name, (usize)nl)) {
            char c = q + 2 + nl < P->e ? q[2 + nl] : '>';
            if (c == '>' || isspace(c) || c == '/') {
                *content_end = q;
                q += 2 + nl;
                while (q < P->e && *q != '>') q++;
                return q < P->e ? q + 1 : q;
            }
        }
        q++;
    }
    *content_end = P->e;
    return P->e;
}

static void parse(parser_t *P) {
    const char *p = P->s;
    while (p < P->e) {
        if (*p != '<') {
            const char *q = p;
            while (q < P->e && *q != '<') q++;
            put_decoded(&P->text, p, q, false);
            p = q;
            continue;
        }
        const char *q = p + 1;
        if (q >= P->e) { sb_putc(&P->text, '<'); break; }
        if (*q == '!') {
            flush_text(P);
            if (q + 2 < P->e && q[1] == '-' && q[2] == '-') {
                const char *c = q + 3;
                while (c + 2 < P->e && !(c[0] == '-' && c[1] == '-' && c[2] == '>')) c++;
                p = c + 2 < P->e ? c + 3 : P->e;
            } else if (q + 7 < P->e && !strncmp(q + 1, "[CDATA[", 7)) {
                const char *c = q + 8;
                while (c + 2 < P->e && !(c[0] == ']' && c[1] == ']' && c[2] == '>')) c++;
                p = c + 2 < P->e ? c + 3 : P->e;
            } else {
                while (q < P->e && *q != '>') q++;
                p = q < P->e ? q + 1 : q;
            }
            continue;
        }
        if (*q == '?') {
            flush_text(P);
            while (q < P->e && *q != '>') q++;
            p = q < P->e ? q + 1 : q;
            continue;
        }
        bool end = false;
        if (*q == '/') { end = true; q++; }
        if (q >= P->e || !isalpha(*q)) {
            if (end) { /* "</ >" ou similar: ignora até '>' */
                while (q < P->e && *q != '>') q++;
                p = q < P->e ? q + 1 : q;
                continue;
            }
            sb_putc(&P->text, '<');
            p++;
            continue;
        }
        char name[32];
        int nl = 0;
        while (q < P->e && !isspace(*q) && *q != '/' && *q != '>') {
            if (nl < 31) name[nl++] = (char)tolower(*q);
            q++;
        }
        name[nl] = 0;
        flush_text(P);
        int tag = tag_lookup(name);
        if (end) {
            while (q < P->e && *q != '>') q++;
            p = q < P->e ? q + 1 : q;
            if (tag == T_BR) {
                node_t *n = new_node(P, N_ELEM);
                n->tag = T_BR; n->name = "br";
                handle_start(P, n, false);
            } else handle_end(P, tag, name);
            continue;
        }
        node_t *n = new_node(P, N_ELEM);
        n->tag = (u16)tag;
        n->name = tag ? tag_names[tag] : arena_strndup(&P->d->arena, name, nl);
        bool selfclose = false;
        /* atributos */
        for (;;) {
            while (q < P->e && (isspace(*q) || *q == '/')) {
                if (*q == '/' && q + 1 < P->e && q[1] == '>') selfclose = true;
                q++;
            }
            if (q >= P->e || *q == '>') break;
            const char *an = q;
            while (q < P->e && !isspace(*q) && *q != '/' && *q != '>' && *q != '=') q++;
            if (q == an) q++; /* '=' solto */
            int anl = (int)(q - an);
            char lname[48];
            int ln = 0;
            for (int i = 0; i < anl && ln < 47; i++) lname[ln++] = (char)tolower(an[i]);
            lname[ln] = 0;
            while (q < P->e && isspace(*q)) q++;
            const char *val = "";
            if (q < P->e && *q == '=') {
                q++;
                while (q < P->e && isspace(*q)) q++;
                const char *vs, *ve;
                if (q < P->e && (*q == '"' || *q == '\'')) {
                    char quote = *q++;
                    vs = q;
                    while (q < P->e && *q != quote) q++;
                    ve = q;
                    if (q < P->e) q++;
                } else {
                    vs = q;
                    while (q < P->e && !isspace(*q) && *q != '>') q++;
                    ve = q;
                }
                P->tmp.len = 0;
                put_decoded(&P->tmp, vs, ve, true);
                val = arena_strndup(&P->d->arena, P->tmp.p ? P->tmp.p : "", P->tmp.len);
            }
            if (ln) add_attr(P, n, lname, ln, val);
        }
        p = q < P->e ? q + 1 : q;
        /* elementos de texto bruto */
        const char *raw = NULL;
        switch (tag) {
        case T_SCRIPT: case T_STYLE: case T_XMP: case T_IFRAME: case T_NOEMBED: case T_NOFRAMES: case T_TEXTAREA:
        case T_TITLE: case T_SVG: case T_MATH: case T_TEMPLATE: case T_PLAINTEXT:
            raw = name;
            break;
        }
        if (raw && !selfclose) {
            const char *ce;
            const char *after = tag == T_PLAINTEXT ? (ce = P->e, P->e) : raw_until(P, p, raw, &ce);
            handle_start(P, n, false);
            if (tag == T_STYLE || tag == T_XMP || tag == T_TEXTAREA || tag == T_TITLE || tag == T_PLAINTEXT || (tag == T_SCRIPT && ce > p)) {
                node_t *t = new_node(P, N_TEXT);
                if (tag == T_STYLE || tag == T_XMP || tag == T_PLAINTEXT || tag == T_SCRIPT) {
                    const char *cs = p;
                    if (tag != T_STYLE && cs < ce && *cs == '\n') cs++;
                    t->text = arena_strndup(&P->d->arena, cs, (int)(ce - cs));
                    t->len = (int)(ce - cs);
                } else {
                    P->tmp.len = 0;
                    const char *cs = p;
                    if (tag == T_TEXTAREA && cs < ce && *cs == '\n') cs++;
                    put_decoded(&P->tmp, cs, ce, false);
                    t->text = arena_strndup(&P->d->arena, P->tmp.p ? P->tmp.p : "", P->tmp.len);
                    t->len = P->tmp.len;
                }
                append(n, t);
            }
            /* sai do elemento */
            if (P->sp > 2 && top(P) == n) P->sp--;
            p = after;
            if (tag == T_TITLE && !P->d->title[0] && !P->frag) {
                sbuf_t tb = {0};
                node_text_content(n, &tb);
                int k = 0;
                bool sp = false;
                for (int i = 0; i < tb.len && k < (int)sizeof(P->d->title) - 4; i++) {
                    char c = tb.p[i];
                    if (isspace((u8)c)) { sp = k > 0; continue; }
                    if (sp) { P->d->title[k++] = ' '; sp = false; }
                    P->d->title[k++] = c;
                }
                P->d->title[k] = 0;
                sb_free(&tb);
            }
            continue;
        }
        handle_start(P, n, selfclose);
        if (tag == T_PRE || tag == T_LISTING) {
            /* ignora a primeira quebra de linha após <pre> */
            if (p < P->e && *p == '\n') p++;
            else if (p + 1 < P->e && p[0] == '\r' && p[1] == '\n') p += 2;
        }
        if (tag == T_BASE && !P->d->base[0] && !P->frag) {
            const char *h = node_attr(n, "href");
            if (h) url_resolve(P->d->url, h, P->d->base, sizeof P->d->base);
        }
    }
    flush_text(P);
}

doc_t *doc_parse(const char *html, usize len, const char *url) {
    doc_t *d = kcalloc(1, sizeof(doc_t));
    if (!d) return NULL;
    strncpy(d->url, url, sizeof d->url - 1);
    parser_t *P = kcalloc(1, sizeof(parser_t));
    P->d = d;
    P->s = html;
    P->e = html + len;
    d->root = new_node(P, N_DOC);
    d->html = new_node(P, N_ELEM); d->html->tag = T_HTML; d->html->name = "html";
    d->head = new_node(P, N_ELEM); d->head->tag = T_HEAD; d->head->name = "head";
    d->body = new_node(P, N_ELEM); d->body->tag = T_BODY; d->body->name = "body";
    append(d->root, d->html);
    append(d->html, d->head);
    append(d->html, d->body);
    P->stack[0] = d->html;
    P->stack[1] = d->body;
    P->sp = 2;
    parse(P);
    d->node_count = P->count;
    sb_free(&P->text);
    sb_free(&P->tmp);
    kfree(P);
    if (!d->base[0]) strcpy(d->base, d->url);
    d->zoom = 1;
    d->need_style = true;
    d->need_layout = true;
    return d;
}

/* procura <meta charset> nos primeiros bytes */
bool html_sniff_charset(const u8 *d, usize n, char *out, int cap) {
    usize lim = MIN(n, (usize)4096);
    for (usize i = 0; i + 8 < lim; i++) {
        if (d[i] != '<' || (d[i + 1] | 32) != 'm' || strncasecmp((const char *)d + i + 1, "meta", 4)) continue;
        usize j = i + 5;
        usize end = j;
        while (end < lim && d[end] != '>') end++;
        for (usize k = j; k + 7 < end; k++) {
            if (!strncasecmp((const char *)d + k, "charset", 7)) {
                usize v = k + 7;
                while (v < end && (d[v] == ' ' || d[v] == '=' || d[v] == '"' || d[v] == '\'')) v++;
                int o = 0;
                while (v < end && o < cap - 1 && (isalnum(d[v]) || d[v] == '-' || d[v] == '_')) out[o++] = (char)d[v++];
                out[o] = 0;
                if (o) return true;
            }
        }
    }
    return false;
}

void doc_free(doc_t *d) {
    if (!d) return;
    for (img_t *im = d->images, *nx; im; im = nx) {
        nx = im->next;
        if (im->req) http_free(im->req);
        surface_free(im->surf);
        surface_free(im->scaled);
        kfree(im);
    }
    for (int i = 0; i < d->nctrls; i++) kfree(d->ctrls[i]->buf);
    kfree(d->ctrls);
    css_free(d->sheet);
    arena_free(&d->larena);
    arena_free(&d->arena);
    kfree(d);
}

/* analisa um fragmento HTML (innerHTML) acrescentando os nós como filhos de parent (memória do documento) */
void html_parse_fragment(doc_t *d, node_t *parent, const char *html, int len) {
    parser_t *P = kcalloc(1, sizeof(parser_t));
    if (!P) return;
    P->d = d;
    P->s = html;
    P->e = html + len;
    P->frag = true;
    P->in_body = true;
    P->count = d->node_count;
    P->stack[0] = parent;
    P->stack[1] = parent;
    P->sp = 2;
    parse(P);
    d->node_count = P->count;
    sb_free(&P->text);
    sb_free(&P->tmp);
    kfree(P);
}

const char *html_tag_name(int tag) { return tag > 0 && tag < T_COUNT ? tag_names[tag] : NULL; }