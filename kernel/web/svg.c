/* mOS — renderizador SVG do motor web (implementação própria a partir da especificação SVG 1.1/2):
 *   - parser XML próprio (entidades, CDATA, comentários, prefixos de nome);
 *   - estilo: atributos de apresentação, style="", <style> com seletores simples (tag, .classe, #id, *),
 *     herança, currentColor, display/visibility;
 *   - geometria: path (M L H V C S Q T A Z, relativos), rect (rx/ry), circle, ellipse, line, polyline, polygon,
 *     transform (matrix translate scale rotate skewX skewY), viewBox + preserveAspectRatio, <svg> aninhado;
 *   - pintura: cor, linearGradient e radialGradient (foco, units, gradientTransform, spreadMethod, href),
 *     fill-rule, opacidades, opacidade de grupo em camada, clip-path (clipPath em userSpaceOnUse/objectBoundingBox);
 *   - traço: largura, junções miter/round/bevel com miterlimit, tampas butt/round/square, tracejado com deslocamento;
 *   - <use>/<symbol>/<defs>, <text>/<tspan> com x/y/dx/dy, text-anchor e peso; <image> com data: (PNG/JPEG/GIF/WebP);
 *   - rasterizador de cobertura com 8 subamostras verticais e cobertura horizontal exata (não-zero e par-ímpar).
 * Saída: surface_t BGRA pré-multiplicado. */
#include "web.h"
#include "gfx.h"
#include "raster.h"
/* nomes curtos para o rasterizador comum (kernel/web/raster.c) */
#define num_start rs_num_start
#define parse_f rs_parse_f
#define path_free rs_path_free
#define path_pt rs_path_pt
#define path_move rs_path_move
#define path_line rs_path_line
#define path_close rs_path_close
#define cont_len rs_cont_len
#define path_cubic rs_path_cubic
#define path_quad rs_path_quad
#define path_arc rs_path_arc
#define parse_path_d rs_parse_path_d
#define edges_from_path rs_edges_from_path
#define rasterize rs_rasterize
#define poly_add rs_poly_add
#define circle_poly rs_circle_poly

/* ================================================================ XML */
typedef struct xel {
    const char *name;            /* NULL = nó de texto */
    const char *text;            /* nós de texto */
    int tlen;
    const char **an, **av;
    int na;
    struct xel *parent, *first, *last, *next;
} xel_t;

typedef struct {
    arena_t A;
    xel_t *root;
} xdoc_t;

static void put_utf8(sbuf_t *b, u32 cp) {
    char o[4];
    int n;
    if (cp < 0x80) { o[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { o[0] = (char)(0xC0 | cp >> 6); o[1] = (char)(0x80 | (cp & 63)); n = 2; }
    else if (cp < 0x10000) { o[0] = (char)(0xE0 | cp >> 12); o[1] = (char)(0x80 | ((cp >> 6) & 63)); o[2] = (char)(0x80 | (cp & 63)); n = 3; }
    else { o[0] = (char)(0xF0 | cp >> 18); o[1] = (char)(0x80 | ((cp >> 12) & 63)); o[2] = (char)(0x80 | ((cp >> 6) & 63)); o[3] = (char)(0x80 | (cp & 63)); n = 4; }
    sb_putn(b, o, n);
}

/* texto com entidades → string na arena */
static const char *xdecode(xdoc_t *X, const char *s, const char *e, int *olen) {
    sbuf_t b = { 0 };
    while (s < e) {
        if (*s == '&') {
            const char *semi = s;
            while (semi < e && *semi != ';' && semi - s < 12) semi++;
            if (semi < e && *semi == ';') {
                int n = (int)(semi - s - 1);
                const char *nm = s + 1;
                u32 cp = 0;
                bool ok = true;
                if (n == 2 && !strncmp(nm, "lt", 2)) cp = '<';
                else if (n == 2 && !strncmp(nm, "gt", 2)) cp = '>';
                else if (n == 3 && !strncmp(nm, "amp", 3)) cp = '&';
                else if (n == 4 && !strncmp(nm, "quot", 4)) cp = '"';
                else if (n == 4 && !strncmp(nm, "apos", 4)) cp = '\'';
                else if (n == 4 && !strncmp(nm, "nbsp", 4)) cp = 0xA0;
                else if (n > 1 && nm[0] == '#') {
                    if (nm[1] == 'x' || nm[1] == 'X') { for (int i = 2; i < n; i++) { char c = (char)tolower(nm[i]); cp = cp * 16 + (u32)(isdigit(c) ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : 0); } }
                    else for (int i = 1; i < n; i++) cp = cp * 10 + (u32)(nm[i] - '0');
                } else ok = false;
                if (ok) { put_utf8(&b, cp ? cp : 0xFFFD); s = semi + 1; continue; }
            }
        }
        sb_putc(&b, *s++);
    }
    const char *r = arena_strndup(&X->A, b.p ? b.p : "", b.len);
    if (olen) *olen = b.len;
    sb_free(&b);
    return r;
}

static xel_t *xnew(xdoc_t *X, xel_t *parent) {
    xel_t *n = arena_alloc(&X->A, sizeof(xel_t));
    if (!n) return NULL;
    n->parent = parent;
    if (parent) {
        if (parent->last) parent->last->next = n; else parent->first = n;
        parent->last = n;
    }
    return n;
}

static const char *find_str(const char *s, const char *e, const char *pat) {
    int n = (int)strlen(pat);
    for (; s + n <= e; s++) if (!memcmp(s, pat, n)) return s;
    return NULL;
}

static bool xml_parse(xdoc_t *X, const char *s, int len) {
    const char *p = s, *e = s + len;
    xel_t *cur = NULL;
    int depth = 0;
    while (p < e) {
        if (*p != '<') {
            const char *t = p;
            while (p < e && *p != '<') p++;
            if (cur) {
                xel_t *tn = xnew(X, cur);
                if (tn) tn->text = xdecode(X, t, p, &tn->tlen);
            }
            continue;
        }
        if (p + 4 <= e && !memcmp(p, "<!--", 4)) { const char *q = find_str(p + 4, e, "-->"); p = q ? q + 3 : e; continue; }
        if (p + 9 <= e && !memcmp(p, "<![CDATA[", 9)) {
            const char *q = find_str(p + 9, e, "]]>");
            const char *end = q ? q : e;
            if (cur) { xel_t *tn = xnew(X, cur); if (tn) { tn->text = arena_strndup(&X->A, p + 9, (int)(end - p - 9)); tn->tlen = (int)(end - p - 9); } }
            p = q ? q + 3 : e;
            continue;
        }
        if (p + 1 < e && (p[1] == '!' || p[1] == '?')) {
            /* DOCTYPE pode ter subconjunto interno [...] */
            int br = 0;
            p += 2;
            while (p < e && (br || *p != '>')) { if (*p == '[') br++; else if (*p == ']') br--; p++; }
            p++;
            continue;
        }
        if (p + 1 < e && p[1] == '/') {
            const char *q = p + 2, *ns = q;
            while (q < e && *q != '>' && !isspace(*q)) q++;
            const char *ne = q, *colon = NULL;
            for (const char *c = ns; c < ne; c++) if (*c == ':') colon = c;
            if (colon) ns = colon + 1;
            while (q < e && *q != '>') q++;
            p = q < e ? q + 1 : e;
            /* fecha até o ancestral com esse nome */
            for (xel_t *a = cur; a; a = a->parent)
                if (a->name && (int)strlen(a->name) == (int)(ne - ns) && !memcmp(a->name, ns, ne - ns)) { cur = a->parent; depth--; break; }
            continue;
        }
        /* abertura */
        const char *q = p + 1, *ns = q;
        while (q < e && !isspace(*q) && *q != '>' && *q != '/') q++;
        const char *ne = q, *colon = NULL;
        for (const char *c = ns; c < ne; c++) if (*c == ':') colon = c;
        if (colon) ns = colon + 1;
        xel_t *n = xnew(X, cur);
        if (!n) return false;
        n->name = arena_strndup(&X->A, ns, (int)(ne - ns));
        const char *an[64], *av[64];
        int na = 0;
        bool selfclose = false;
        while (q < e) {
            while (q < e && isspace(*q)) q++;
            if (q >= e) break;
            if (*q == '>') { q++; break; }
            if (*q == '/') { selfclose = true; q++; continue; }
            const char *as = q;
            while (q < e && !isspace(*q) && *q != '=' && *q != '>' && *q != '/') q++;
            const char *ae = q;
            while (q < e && isspace(*q)) q++;
            const char *vs = NULL, *ve = NULL;
            if (q < e && *q == '=') {
                q++;
                while (q < e && isspace(*q)) q++;
                if (q < e && (*q == '"' || *q == '\'')) {
                    char quote = *q++;
                    vs = q;
                    while (q < e && *q != quote) q++;
                    ve = q;
                    if (q < e) q++;
                } else { vs = q; while (q < e && !isspace(*q) && *q != '>') q++; ve = q; }
            }
            if (ae > as && na < 64) {
                an[na] = arena_strndup(&X->A, as, (int)(ae - as));
                av[na] = vs ? xdecode(X, vs, ve, NULL) : "";
                na++;
            }
        }
        if (na) {
            n->an = arena_alloc(&X->A, sizeof(char *) * na);
            n->av = arena_alloc(&X->A, sizeof(char *) * na);
            if (!n->an || !n->av) return false;
            memcpy(n->an, an, sizeof(char *) * na);
            memcpy(n->av, av, sizeof(char *) * na);
            n->na = na;
        }
        if (!X->root && !strcmp(n->name, "svg")) X->root = n;
        p = q;
        if (!selfclose && depth < 256) { cur = n; depth++; }
        if (!strcmp(n->name, "style") || !strcmp(n->name, "script")) {
            /* conteúdo bruto até o fechamento */
            if (!selfclose) {
                const char *close = find_str(p, e, !strcmp(n->name, "style") ? "</style" : "</script");
                if (!close) close = find_str(p, e, "</");
                const char *end = close ? close : e;
                xel_t *tn = xnew(X, n);
                if (tn) {
                    const char *cs = p, *ce = end;
                    const char *cd = find_str(cs, ce, "<![CDATA[");
                    if (cd) { cs = cd + 9; const char *cde = find_str(cs, ce, "]]>"); if (cde) ce = cde; }
                    tn->text = arena_strndup(&X->A, cs, (int)(ce - cs));
                    tn->tlen = (int)(ce - cs);
                }
                p = end;
            }
        }
    }
    return X->root != NULL;
}

static const char *xattr(const xel_t *n, const char *name) {
    for (int i = 0; i < n->na; i++) if (!strcmp(n->an[i], name)) return n->av[i];
    /* xlink:href e outras com prefixo */
    for (int i = 0; i < n->na; i++) { const char *c = strchr(n->an[i], ':'); if (c && !strcmp(c + 1, name)) return n->av[i]; }
    return NULL;
}

/* ================================================================ números, comprimentos e matrizes */


/* comprimento: px pt pc mm cm in em ex %; ref = base de % */
static float parse_len(const char *s, float ref, float fs, float def) {
    if (!s) return def;
    const char *p = s;
    float v;
    if (!parse_f(&p, &v)) return def;
    while (isspace(*p)) p++;
    if (*p == '%') return v * ref / 100.0f;
    if (!strncmp(p, "pt", 2)) return v * 4.0f / 3.0f;
    if (!strncmp(p, "pc", 2)) return v * 16.0f;
    if (!strncmp(p, "mm", 2)) return v * 96.0f / 25.4f;
    if (!strncmp(p, "cm", 2)) return v * 96.0f / 2.54f;
    if (!strncmp(p, "in", 2)) return v * 96.0f;
    if (!strncmp(p, "em", 2) || !strncmp(p, "rem", 3)) return v * fs;
    if (!strncmp(p, "ex", 2)) return v * fs * 0.5f;
    return v;
}


static mat_t parse_transform(const char *s) {
    mat_t m = MID;
    if (!s) return m;
    const char *p = s;
    while (*p) {
        while (*p && (isspace(*p) || *p == ',')) p++;
        if (!*p) break;
        const char *nm = p;
        while (isalpha(*p)) p++;
        int nl = (int)(p - nm);
        while (isspace(*p)) p++;
        if (*p != '(') break;
        p++;
        float v[6] = { 0 };
        int n = 0;
        while (n < 6 && parse_f(&p, &v[n])) n++;
        while (*p && *p != ')') p++;
        if (*p) p++;
        mat_t t = MID;
        if (nl == 6 && !strncmp(nm, "matrix", 6) && n == 6) { t.a = v[0]; t.b = v[1]; t.c = v[2]; t.d = v[3]; t.e = v[4]; t.f = v[5]; }
        else if (nl == 9 && !strncmp(nm, "translate", 9)) { t.e = v[0]; t.f = n > 1 ? v[1] : 0; }
        else if (nl == 5 && !strncmp(nm, "scale", 5)) { t.a = v[0]; t.d = n > 1 ? v[1] : v[0]; }
        else if (nl == 6 && !strncmp(nm, "rotate", 6)) {
            float r = v[0] * PI_F / 180.0f, cs = fcos(r), sn = fsin(r);
            t.a = cs; t.b = sn; t.c = -sn; t.d = cs;
            if (n >= 3) {
                mat_t a = MID, b = MID;
                a.e = v[1]; a.f = v[2]; b.e = -v[1]; b.f = -v[2];
                t = mmul(mmul(a, t), b);
            }
        } else if (nl == 5 && !strncmp(nm, "skewX", 5)) { float r = v[0] * PI_F / 180.0f; t.c = fsin(r) / fcos(r); }
        else if (nl == 5 && !strncmp(nm, "skewY", 5)) { float r = v[0] * PI_F / 180.0f; t.b = fsin(r) / fcos(r); }
        m = mmul(m, t);
    }
    return m;
}

/* ================================================================ caminhos achatados */



/* ================================================================ rasterizador */




/* ================================================================ estilo */
enum { PT_NONE, PT_COLOR, PT_URL, PT_CURRENT };
typedef struct { u8 kind; u32 color; char id[64]; u32 fallback; bool has_fb; } paint_t;
enum { ANCHOR_START, ANCHOR_MIDDLE, ANCHOR_END };

typedef struct {
    paint_t fill, stroke;
    float fill_op, stroke_op, opacity, sw, miter, dashoff, font_size;
    float dash[16];
    int ndash;
    u8 cap, join, evenodd, clip_evenodd, hidden, anchor, weight;
    bool none;            /* display:none */
    u32 color;
    char clip[64];
    u32 stop_color;
    float stop_op;
} sst_t;

typedef struct { char sel[64]; int spec; char decl[512]; } rule_t;

typedef struct ctx {
    xdoc_t X;
    surface_t *s;         /* camada atual */
    int W, H;
    rule_t *rules;
    int nrules;
    xel_t **ids;          /* elementos com id */
    int nids;
    int depth;            /* use/recursão */
    float vw, vh;         /* viewport atual (para %) */
    u8 *clipm;            /* máscara de recorte do canvas (NULL = nenhuma) */
} ctx_t;

static void set_paint(paint_t *pt, const char *v, const sst_t *par, bool is_fill) {
    while (isspace(*v)) v++;
    if (!strncmp(v, "none", 4)) { pt->kind = PT_NONE; return; }
    if (!strncasecmp(v, "currentcolor", 12)) { pt->kind = PT_CURRENT; return; }
    if (!strncmp(v, "inherit", 7)) { *pt = is_fill ? par->fill : par->stroke; return; }
    if (!strncmp(v, "url(", 4)) {
        const char *s = v + 4;
        while (*s == ' ' || *s == '\'' || *s == '"') s++;
        if (*s == '#') s++;
        int n = 0;
        pt->kind = PT_URL;
        while (*s && *s != ')' && *s != '\'' && *s != '"' && n < 63) pt->id[n++] = *s++;
        pt->id[n] = 0;
        const char *fb = strchr(v, ')');
        pt->has_fb = false;
        if (fb) {
            fb++;
            while (isspace(*fb)) fb++;
            if (*fb && strncmp(fb, "none", 4)) { u32 c; if (css_color(fb, &c)) { pt->fallback = c; pt->has_fb = true; } }
        }
        return;
    }
    u32 c;
    if (css_color(v, &c)) { pt->kind = PT_COLOR; pt->color = c; }
}

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static float parse_op(const char *v) {
    const char *p = v;
    float f;
    if (!parse_f(&p, &f)) return 1;
    if (*p == '%') f /= 100;
    return clamp01(f);
}

static void apply_prop(ctx_t *C, sst_t *st, const sst_t *par, const char *k, int kl, const char *v) {
#define K(s) (kl == (int)sizeof(s) - 1 && !strncmp(k, s, kl))
    while (isspace(*v)) v++;
    if (!*v) return;
    bool inh = !strncmp(v, "inherit", 7);
    if (K("fill")) set_paint(&st->fill, v, par, true);
    else if (K("stroke")) set_paint(&st->stroke, v, par, false);
    else if (K("color")) { if (inh) st->color = par->color; else { u32 c; if (css_color(v, &c)) st->color = c; } }
    else if (K("fill-opacity")) st->fill_op = inh ? par->fill_op : parse_op(v);
    else if (K("stroke-opacity")) st->stroke_op = inh ? par->stroke_op : parse_op(v);
    else if (K("opacity")) st->opacity = parse_op(v);
    else if (K("stroke-width")) st->sw = inh ? par->sw : parse_len(v, sqrtf_((C->vw * C->vw + C->vh * C->vh) / 2), st->font_size, st->sw);
    else if (K("stroke-miterlimit")) { const char *p = v; float f; if (parse_f(&p, &f) && f >= 1) st->miter = f; }
    else if (K("stroke-linecap")) st->cap = !strncmp(v, "round", 5) ? CAP_ROUND : !strncmp(v, "square", 6) ? CAP_SQUARE : CAP_BUTT;
    else if (K("stroke-linejoin")) st->join = !strncmp(v, "round", 5) ? JOIN_ROUND : !strncmp(v, "bevel", 5) ? JOIN_BEVEL : JOIN_MITER;
    else if (K("stroke-dasharray")) {
        st->ndash = 0;
        if (strncmp(v, "none", 4)) {
            const char *p = v;
            float f;
            while (st->ndash < 16 && parse_f(&p, &f)) {
                while (*p && *p != ',' && !isspace(*p)) p++;   /* unidades */
                st->dash[st->ndash++] = f < 0 ? 0 : f;
            }
            float sum = 0;
            for (int i = 0; i < st->ndash; i++) sum += st->dash[i];
            if (sum <= 0) st->ndash = 0;
        }
    } else if (K("stroke-dashoffset")) st->dashoff = parse_len(v, 100, st->font_size, 0);
    else if (K("fill-rule")) st->evenodd = !strncmp(v, "evenodd", 7);
    else if (K("clip-rule")) st->clip_evenodd = !strncmp(v, "evenodd", 7);
    else if (K("display")) st->none = !strncmp(v, "none", 4);
    else if (K("visibility")) st->hidden = !strncmp(v, "hidden", 6) || !strncmp(v, "collapse", 8);
    else if (K("font-size")) st->font_size = parse_len(v, par->font_size, par->font_size, st->font_size);
    else if (K("font-weight")) {
        if (!strncmp(v, "bold", 4) || !strncmp(v, "bolder", 6)) st->weight = FONT_BOLD;
        else if (!strncmp(v, "normal", 6) || !strncmp(v, "lighter", 7)) st->weight = FONT_REGULAR;
        else { int w = atoi(v); st->weight = w >= 700 ? FONT_BOLD : w >= 600 ? FONT_SEMIBOLD : w >= 500 ? FONT_MEDIUM : FONT_REGULAR; }
    } else if (K("text-anchor")) st->anchor = !strncmp(v, "middle", 6) ? ANCHOR_MIDDLE : !strncmp(v, "end", 3) ? ANCHOR_END : ANCHOR_START;
    else if (K("clip-path")) {
        st->clip[0] = 0;
        if (!strncmp(v, "url(", 4)) {
            const char *s = v + 4;
            while (*s == ' ' || *s == '\'' || *s == '"') s++;
            if (*s == '#') s++;
            int n = 0;
            while (*s && *s != ')' && *s != '\'' && *s != '"' && n < 63) st->clip[n++] = *s++;
            st->clip[n] = 0;
        }
    } else if (K("stop-color")) { if (!strncasecmp(v, "currentcolor", 12)) st->stop_color = st->color; else { u32 c; if (css_color(v, &c)) st->stop_color = c; } }
    else if (K("stop-opacity")) st->stop_op = parse_op(v);
#undef K
}

static void apply_decls(ctx_t *C, sst_t *st, const sst_t *par, const char *d) {
    const char *p = d;
    while (*p) {
        while (*p && (isspace(*p) || *p == ';')) p++;
        const char *k = p;
        while (*p && *p != ':' && *p != ';') p++;
        if (*p != ':') { if (*p) p++; continue; }
        const char *ke = p++;
        while (ke > k && isspace(ke[-1])) ke--;
        const char *v = p;
        while (*p && *p != ';') p++;
        char val[256];
        int n = (int)(p - v);
        if (n > 255) n = 255;
        memcpy(val, v, n);
        val[n] = 0;
        char *imp = strstr(val, "!important");
        if (imp) *imp = 0;
        while (n > 0 && isspace(val[n - 1])) val[--n] = 0;
        apply_prop(C, st, par, k, (int)(ke - k), val);
    }
}

static const char *presentation[] = {
    "fill", "stroke", "color", "fill-opacity", "stroke-opacity", "opacity", "stroke-width", "stroke-miterlimit",
    "stroke-linecap", "stroke-linejoin", "stroke-dasharray", "stroke-dashoffset", "fill-rule", "clip-rule", "display",
    "visibility", "font-size", "font-weight", "text-anchor", "clip-path", "stop-color", "stop-opacity",
};

/* seletor composto simples casa com o elemento? ("tag", ".a", "#i", "tag.a.b", "*") */
static bool sel_match(const char *sel, const xel_t *n) {
    const char *p = sel;
    /* só o último composto de um seletor descendente */
    const char *last = p;
    for (const char *q = p; *q; q++) if ((*q == ' ' || *q == '>') && q[1] && q[1] != ' ' && q[1] != '>') last = q + 1;
    p = last;
    if (*p == '*') p++;
    else if (isalpha(*p)) {
        const char *s = p;
        while (*p && *p != '.' && *p != '#' && *p != ':' && *p != '[') p++;
        if ((int)strlen(n->name) != (int)(p - s) || strncmp(n->name, s, p - s)) return false;
    }
    while (*p) {
        if (*p == '.' || *p == '#') {
            char kind = *p++;
            const char *s = p;
            while (*p && *p != '.' && *p != '#' && *p != ':' && *p != '[') p++;
            int l = (int)(p - s);
            if (kind == '#') { const char *id = xattr(n, "id"); if (!id || (int)strlen(id) != l || strncmp(id, s, l)) return false; }
            else {
                const char *cl = xattr(n, "class");
                bool found = false;
                while (cl && *cl && !found) {
                    while (isspace(*cl)) cl++;
                    const char *cs = cl;
                    while (*cl && !isspace(*cl)) cl++;
                    if ((int)(cl - cs) == l && !strncmp(cs, s, l)) found = true;
                }
                if (!found) return false;
            }
        } else return false;   /* pseudo-classes e atributos: não casam */
    }
    return true;
}

static int sel_spec(const char *sel) {
    int s = 0;
    for (const char *p = sel; *p; p++) { if (*p == '#') s += 100; else if (*p == '.') s += 10; else if (isalpha(*p) && (p == sel || p[-1] == ' ' || p[-1] == '>')) s += 1; }
    return s;
}

static void parse_css(ctx_t *C, const char *css) {
    const char *p = css;
    while (*p) {
        while (*p && isspace(*p)) p++;
        if (!*p) break;
        if (p[0] == '/' && p[1] == '*') { const char *e = strstr(p + 2, "*/"); p = e ? e + 2 : p + strlen(p); continue; }
        if (*p == '@') {  /* @media, @font-face...: pula o bloco */
            int dep = 0;
            while (*p && !(*p == ';' && !dep)) { if (*p == '{') dep++; else if (*p == '}') { if (--dep <= 0) { p++; break; } } p++; }
            if (*p == ';') p++;
            continue;
        }
        const char *ss = p;
        while (*p && *p != '{') p++;
        if (!*p) break;
        const char *se = p++;
        const char *ds = p;
        while (*p && *p != '}') p++;
        const char *de = p;
        if (*p) p++;
        /* cada seletor da lista */
        const char *a = ss;
        while (a < se) {
            const char *b = a;
            while (b < se && *b != ',') b++;
            const char *x = a, *y = b;
            while (x < y && isspace(*x)) x++;
            while (y > x && isspace(y[-1])) y--;
            if (y > x && (int)(y - x) < 63 && (int)(de - ds) < 511) {
                rule_t *nr = krealloc(C->rules, sizeof(rule_t) * (C->nrules + 1));
                if (!nr) return;
                C->rules = nr;
                rule_t *r = &C->rules[C->nrules++];
                memcpy(r->sel, x, y - x); r->sel[y - x] = 0;
                memcpy(r->decl, ds, de - ds); r->decl[de - ds] = 0;
                r->spec = sel_spec(r->sel);
            }
            a = b + 1;
        }
    }
}

static void compute_style(ctx_t *C, const xel_t *n, const sst_t *par, sst_t *st) {
    *st = *par;
    st->opacity = 1;
    st->clip[0] = 0;
    st->none = false;
    st->stop_color = 0xFF000000u;
    st->stop_op = 1;
    for (unsigned i = 0; i < ARRAY_LEN(presentation); i++) {
        const char *v = xattr(n, presentation[i]);
        if (v) apply_prop(C, st, par, presentation[i], (int)strlen(presentation[i]), v);
    }
    /* regras da folha, já ordenadas por especificidade (estável) em collect */
    for (int i = 0; i < C->nrules; i++) if (sel_match(C->rules[i].sel, n)) apply_decls(C, st, par, C->rules[i].decl);
    const char *sa = xattr(n, "style");
    if (sa) apply_decls(C, st, par, sa);
}

static xel_t *by_id(ctx_t *C, const char *id) {
    if (!id) return NULL;
    if (*id == '#') id++;
    for (int i = 0; i < C->nids; i++) { const char *v = xattr(C->ids[i], "id"); if (v && !strcmp(v, id)) return C->ids[i]; }
    return NULL;
}

static void collect(ctx_t *C, xel_t *n) {
    for (xel_t *c = n; c; c = c->next) {
        if (!c->name) continue;
        if (xattr(c, "id")) {
            xel_t **ni = krealloc(C->ids, sizeof(xel_t *) * (C->nids + 1));
            if (ni) { C->ids = ni; C->ids[C->nids++] = c; }
        }
        if (!strcmp(c->name, "style")) for (xel_t *t = c->first; t; t = t->next) if (!t->name && t->text) parse_css(C, t->text);
        if (c->first) collect(C, c->first);
    }
}

/* ================================================================ pintura */
typedef struct { float off; u32 c; } stop_t;
typedef struct {
    int kind;             /* 0 cor, 1 linear, 2 radial */
    u32 color;            /* pré-multiplicado com a opacidade */
    u32 lut[256];
    int spread;           /* 0 pad, 1 reflect, 2 repeat */
    mat_t inv;            /* canvas → espaço do gradiente */
    float x1, y1, x2, y2, cx, cy, r, fx, fy;
} paintsrc_t;


static int grad_stops(ctx_t *C, xel_t *g, stop_t *st, int max, const sst_t *base, int depth) {
    int n = 0;
    for (xel_t *c = g->first; c && n < max; c = c->next) {
        if (!c->name || strcmp(c->name, "stop")) continue;
        sst_t s;
        compute_style(C, c, base, &s);
        float off = 0;
        const char *o = xattr(c, "offset");
        if (o) { const char *p = o; if (parse_f(&p, &off) && *p == '%') off /= 100; }
        off = clamp01(off);
        if (n && off < st[n - 1].off) off = st[n - 1].off;
        st[n].off = off;
        u32 col = s.stop_color;
        st[n].c = (col & 0xFFFFFF) | ((u32)((col >> 24) * s.stop_op + 0.5f) << 24);
        n++;
    }
    if (!n && depth < 8) {
        const char *h = xattr(g, "href");
        xel_t *r = by_id(C, h);
        if (r && r != g) return grad_stops(C, r, st, max, base, depth + 1);
    }
    return n;
}

static const char *gattr(ctx_t *C, xel_t *g, const char *name) {
    for (int d = 0; g && d < 8; d++) {
        const char *v = xattr(g, name);
        if (v) return v;
        g = by_id(C, xattr(g, "href"));
    }
    return NULL;
}

/* bbox: caixa do objeto (espaço do usuário) para objectBoundingBox */
static bool make_paint(ctx_t *C, const paint_t *pt, const sst_t *st, float op, const mat_t *ctm, const float *bbox, paintsrc_t *P) {
    memset(P, 0, sizeof *P);
    if (pt->kind == PT_NONE) return false;
    if (pt->kind == PT_COLOR || pt->kind == PT_CURRENT) {
        P->kind = 0;
        P->color = premul(pt->kind == PT_CURRENT ? st->color : pt->color, op);
        return (P->color >> 24) != 0;
    }
    xel_t *g = by_id(C, pt->id);
    if (!g || (strcmp(g->name, "linearGradient") && strcmp(g->name, "radialGradient"))) {
        if (pt->has_fb) { P->kind = 0; P->color = premul(pt->fallback, op); return true; }
        return false;
    }
    stop_t stops[32];
    sst_t base = *st;
    int ns = grad_stops(C, g, stops, 32, &base, 0);
    if (!ns) return false;
    if (ns == 1) { P->kind = 0; P->color = premul(stops[0].c, op); return true; }
    for (int i = 0; i < 256; i++) {
        float t = i / 255.0f;
        int k = 0;
        while (k < ns - 1 && stops[k + 1].off < t) k++;
        u32 c;
        if (t <= stops[0].off) c = stops[0].c;
        else if (k >= ns - 1) c = stops[ns - 1].c;
        else {
            float span = stops[k + 1].off - stops[k].off, f = span > 0 ? (t - stops[k].off) / span : 1;
            u32 a = stops[k].c, b = stops[k + 1].c;
            c = 0;
            for (int sh = 0; sh < 32; sh += 8) {
                int va = (int)((a >> sh) & 255), vb = (int)((b >> sh) & 255);
                c |= (u32)(va + (vb - va) * f + 0.5f) << sh;
            }
        }
        P->lut[i] = premul(c, op);
    }
    const char *sp = gattr(C, g, "spreadMethod");
    P->spread = sp && !strcmp(sp, "reflect") ? 1 : sp && !strcmp(sp, "repeat") ? 2 : 0;
    const char *units = gattr(C, g, "gradientUnits");
    bool obb = !units || strcmp(units, "userSpaceOnUse");
    mat_t gm = *ctm;
    if (obb) {
        if (bbox[2] <= 0 || bbox[3] <= 0) return false;
        mat_t b = { bbox[2], 0, 0, bbox[3], bbox[0], bbox[1] };
        gm = mmul(gm, b);
    }
    gm = mmul(gm, parse_transform(gattr(C, g, "gradientTransform")));
    if (!minv(gm, &P->inv)) return false;
    float rw = obb ? 1 : C->vw, rh = obb ? 1 : C->vh, rd = obb ? 1 : sqrtf_((C->vw * C->vw + C->vh * C->vh) / 2);
    if (!strcmp(g->name, "linearGradient")) {
        P->kind = 1;
        P->x1 = parse_len(gattr(C, g, "x1"), rw, 16, 0);
        P->y1 = parse_len(gattr(C, g, "y1"), rh, 16, 0);
        P->x2 = parse_len(gattr(C, g, "x2"), rw, 16, obb ? 1 : C->vw);
        P->y2 = parse_len(gattr(C, g, "y2"), rh, 16, 0);
    } else {
        P->kind = 2;
        P->cx = parse_len(gattr(C, g, "cx"), rw, 16, obb ? .5f : C->vw / 2);
        P->cy = parse_len(gattr(C, g, "cy"), rh, 16, obb ? .5f : C->vh / 2);
        P->r = parse_len(gattr(C, g, "r"), rd, 16, obb ? .5f : rd / 2);
        P->fx = parse_len(gattr(C, g, "fx"), rw, 16, P->cx);
        P->fy = parse_len(gattr(C, g, "fy"), rh, 16, P->cy);
        if (P->r <= 0) { P->kind = 0; P->color = P->lut[255]; }
    }
    return true;
}

static inline u32 grad_at(const paintsrc_t *P, float px, float py) {
    float x, y;
    mapp(&P->inv, px, py, &x, &y);
    float t;
    if (P->kind == 1) {
        float dx = P->x2 - P->x1, dy = P->y2 - P->y1, l2 = dx * dx + dy * dy;
        t = l2 > 0 ? ((x - P->x1) * dx + (y - P->y1) * dy) / l2 : 0;
    } else {
        float cdx = P->cx - P->fx, cdy = P->cy - P->fy, pdx = x - P->fx, pdy = y - P->fy;
        float A = cdx * cdx + cdy * cdy - P->r * P->r, B = pdx * cdx + pdy * cdy, Cc = pdx * pdx + pdy * pdy;
        if (fabs_(A) < 1e-9f) t = B != 0 ? Cc / (2 * B) : 0;
        else { float disc = B * B - A * Cc; t = disc < 0 ? 0 : (B - sqrtf_(disc)) / A; }
    }
    if (P->spread == 2) t -= ffloor(t);
    else if (P->spread == 1) { t = fabs_(t); int k = (int)t; t -= k; if (k & 1) t = 1 - t; }
    t = clamp01(t);
    return P->lut[(int)(t * 255 + 0.5f)];
}


static void composite(ctx_t *C, const mask_t *M, const paintsrc_t *P) {
    surface_t *s = C->s;
    for (int y = 0; y < M->h; y++) {
        int cy = M->y + y;
        if (cy < 0 || cy >= s->h) continue;
        const u8 *mr = M->a + (usize)y * M->w;
        u32 *dr = s->px + (usize)cy * s->stride;
        const u8 *cr = C->clipm ? C->clipm + (usize)cy * C->W : NULL;
        for (int x = 0; x < M->w; x++) {
            u32 m = mr[x];
            if (!m) continue;
            int cx = M->x + x;
            if (cx < 0 || cx >= s->w) continue;
            if (cr) { m = m * cr[cx] / 255; if (!m) continue; }
            u32 src = P->kind == 0 ? P->color : grad_at(P, cx + 0.5f, cy + 0.5f);
            dr[cx] = over(dr[cx], mul_px(src, m));
        }
    }
}

/* ================================================================ traço */

/* ================================================================ desenho de formas */
static void bbox_of(const path_t *p, float *bb) {
    if (!p->n) { bb[0] = bb[1] = bb[2] = bb[3] = 0; return; }
    float x0 = p->xy[0], y0 = p->xy[1], x1 = x0, y1 = y0;
    for (int i = 1; i < p->n; i++) {
        float x = p->xy[2 * i], y = p->xy[2 * i + 1];
        if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y;
    }
    bb[0] = x0; bb[1] = y0; bb[2] = x1 - x0; bb[3] = y1 - y0;
}

static void fill_path(ctx_t *C, const path_t *p, const mat_t *ctm, bool evenodd, const paintsrc_t *P) {
    edges_t E = { 0 };
    edges_from_path(&E, p, ctm);
    mask_t M = rasterize(&E, evenodd, 0, 0, C->W, C->H);
    if (M.a) { composite(C, &M, P); kfree(M.a); }
    if (E.e) kfree(E.e);
}

static void draw_path(ctx_t *C, const path_t *p, const sst_t *st, const mat_t *ctm, bool can_fill) {
    if (st->hidden || !p->n) return;
    float bb[4];
    bbox_of(p, bb);
    paintsrc_t P;
    if (can_fill && make_paint(C, &st->fill, st, st->fill_op, ctm, bb, &P)) fill_path(C, p, ctm, st->evenodd, &P);
    if (st->sw > 0 && make_paint(C, &st->stroke, st, st->stroke_op, ctm, bb, &P)) {
        path_t o = { 0 };
        o.tol = p->tol;
        rs_stroke_t rs;
        rs.cap = st->cap; rs.join = st->join; rs.miter = st->miter; rs.dashoff = st->dashoff;
        rs.ndash = st->ndash > 16 ? 16 : st->ndash;
        for (int i = 0; i < rs.ndash; i++) rs.dash[i] = st->dash[i];
        rs_stroke_path(&o, p, &rs, st->sw / 2, mscale(ctm), 1.0f);
        fill_path(C, &o, ctm, false, &P);
        path_free(&o);
    }
}

static void shape_path(ctx_t *C, xel_t *n, const sst_t *st, path_t *p, bool *fillable) {
    const char *nm = n->name;
    float fs = st->font_size, vw = C->vw, vh = C->vh, vd = sqrtf_((vw * vw + vh * vh) / 2);
    *fillable = true;
    if (!strcmp(nm, "path")) { const char *d = xattr(n, "d"); if (d) parse_path_d(p, d); }
    else if (!strcmp(nm, "rect")) {
        float x = parse_len(xattr(n, "x"), vw, fs, 0), y = parse_len(xattr(n, "y"), vh, fs, 0);
        float w = parse_len(xattr(n, "width"), vw, fs, 0), h = parse_len(xattr(n, "height"), vh, fs, 0);
        if (w <= 0 || h <= 0) return;
        const char *rxa = xattr(n, "rx"), *rya = xattr(n, "ry");
        float rx = parse_len(rxa, vw, fs, -1), ry = parse_len(rya, vh, fs, -1);
        if (rx < 0 && ry < 0) rx = ry = 0;
        else if (rx < 0) rx = ry;
        else if (ry < 0) ry = rx;
        if (rx > w / 2) rx = w / 2;
        if (ry > h / 2) ry = h / 2;
        if (rx <= 0 || ry <= 0) { path_move(p, x, y); path_line(p, x + w, y); path_line(p, x + w, y + h); path_line(p, x, y + h); path_close(p); }
        else {
            path_move(p, x + rx, y);
            path_line(p, x + w - rx, y); path_arc(p, rx, ry, 0, 0, 1, x + w, y + ry);
            path_line(p, x + w, y + h - ry); path_arc(p, rx, ry, 0, 0, 1, x + w - rx, y + h);
            path_line(p, x + rx, y + h); path_arc(p, rx, ry, 0, 0, 1, x, y + h - ry);
            path_line(p, x, y + ry); path_arc(p, rx, ry, 0, 0, 1, x + rx, y);
            path_close(p);
        }
    } else if (!strcmp(nm, "circle") || !strcmp(nm, "ellipse")) {
        float cx = parse_len(xattr(n, "cx"), vw, fs, 0), cy = parse_len(xattr(n, "cy"), vh, fs, 0), rx, ry;
        if (nm[0] == 'c') rx = ry = parse_len(xattr(n, "r"), vd, fs, 0);
        else {
            rx = parse_len(xattr(n, "rx"), vw, fs, -1); ry = parse_len(xattr(n, "ry"), vh, fs, -1);
            if (rx < 0) rx = ry;
            if (ry < 0) ry = rx;
        }
        if (rx <= 0 || ry <= 0) return;
        path_move(p, cx + rx, cy);
        path_arc(p, rx, ry, 0, 0, 1, cx, cy + ry);
        path_arc(p, rx, ry, 0, 0, 1, cx - rx, cy);
        path_arc(p, rx, ry, 0, 0, 1, cx, cy - ry);
        path_arc(p, rx, ry, 0, 0, 1, cx + rx, cy);
        path_close(p);
    } else if (!strcmp(nm, "line")) {
        path_move(p, parse_len(xattr(n, "x1"), vw, fs, 0), parse_len(xattr(n, "y1"), vh, fs, 0));
        path_line(p, parse_len(xattr(n, "x2"), vw, fs, 0), parse_len(xattr(n, "y2"), vh, fs, 0));
        *fillable = false;
    } else if (!strcmp(nm, "polyline") || !strcmp(nm, "polygon")) {
        const char *s = xattr(n, "points");
        float x, y;
        bool first = true;
        while (s && parse_f(&s, &x) && parse_f(&s, &y)) { if (first) path_move(p, x, y); else path_line(p, x, y); first = false; }
        if (nm[4] == 'g' && !first) path_close(p);
    }
}

/* viewBox + preserveAspectRatio → matriz do viewport (x,y,w,h) */
static mat_t viewbox_mat(const char *vb, const char *par, float x, float y, float w, float h, bool *ok) {
    mat_t m = MID;
    m.e = x; m.f = y;
    *ok = true;
    if (!vb) return m;
    const char *p = vb;
    float v[4];
    for (int i = 0; i < 4; i++) if (!parse_f(&p, &v[i])) return m;
    if (v[2] <= 0 || v[3] <= 0) { *ok = false; return m; }
    float sx = w / v[2], sy = h / v[3];
    char align[16] = "xMidYMid";
    bool slice = false;
    if (par) {
        const char *q = par;
        while (isspace(*q)) q++;
        if (!strncmp(q, "defer", 5)) { q += 5; while (isspace(*q)) q++; }
        int k = 0;
        while (*q && !isspace(*q) && k < 15) align[k++] = *q++;
        align[k] = 0;
        if (strstr(q, "slice")) slice = true;
    }
    if (strcmp(align, "none")) {
        float s = slice ? MAX(sx, sy) : MIN(sx, sy);
        sx = sy = s;
    }
    float tx = x - v[0] * sx, ty = y - v[1] * sy;
    if (strcmp(align, "none")) {
        float ew = v[2] * sx, eh = v[3] * sy;
        if (strstr(align, "xMid")) tx += (w - ew) / 2; else if (strstr(align, "xMax")) tx += w - ew;
        if (strstr(align, "YMid")) ty += (h - eh) / 2; else if (strstr(align, "YMax")) ty += h - eh;
    }
    m.a = sx; m.d = sy; m.e = tx; m.f = ty;
    return m;
}

static void render_node(ctx_t *C, xel_t *n, const sst_t *par, mat_t ctm);

static void render_children(ctx_t *C, xel_t *n, const sst_t *st, mat_t ctm) {
    for (xel_t *c = n->first; c; c = c->next) if (c->name) render_node(C, c, st, ctm);
}

/* máscara de recorte de um clipPath, na resolução do canvas */
static u8 *clip_mask(ctx_t *C, const char *id, const sst_t *st, mat_t ctm, const float *bb) {
    xel_t *cp = by_id(C, id);
    if (!cp || strcmp(cp->name, "clipPath") || C->depth > 8) return NULL;
    u8 *m = kcalloc((usize)C->W * C->H, 1);
    if (!m) return NULL;
    mat_t cm = mmul(ctm, parse_transform(xattr(cp, "transform")));
    const char *u = xattr(cp, "clipPathUnits");
    if (u && !strcmp(u, "objectBoundingBox")) { mat_t b = { bb[2], 0, 0, bb[3], bb[0], bb[1] }; cm = mmul(cm, b); }
    sst_t cs;
    compute_style(C, cp, st, &cs);
    C->depth++;
    for (xel_t *c = cp->first; c; c = c->next) {
        if (!c->name) continue;
        xel_t *g = c;
        mat_t em = mmul(cm, parse_transform(xattr(c, "transform")));
        if (!strcmp(c->name, "use")) {
            g = by_id(C, xattr(c, "href"));
            if (!g) continue;
            mat_t t = MID;
            t.e = parse_len(xattr(c, "x"), C->vw, 16, 0); t.f = parse_len(xattr(c, "y"), C->vh, 16, 0);
            em = mmul(mmul(em, t), parse_transform(xattr(g, "transform")));
        }
        sst_t es;
        compute_style(C, g, &cs, &es);
        if (es.none || es.hidden) continue;
        path_t p = { 0 };
        p.tol = 0.2f / MAX(mscale(&em), 1e-3f);
        bool f;
        shape_path(C, g, &es, &p, &f);
        edges_t E = { 0 };
        edges_from_path(&E, &p, &em);
        mask_t M = rasterize(&E, es.clip_evenodd, 0, 0, C->W, C->H);
        if (M.a) {
            for (int y = 0; y < M.h; y++)
                for (int x = 0; x < M.w; x++) {
                    u8 *d = m + (usize)(M.y + y) * C->W + M.x + x;
                    u32 a = M.a[y * M.w + x];
                    *d = (u8)(255 - (255 - *d) * (255 - a) / 255);
                }
            kfree(M.a);
        }
        if (E.e) kfree(E.e);
        path_free(&p);
    }
    C->depth--;
    return m;
}

static int b64v(char c) { return c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' || c == '-' ? 62 : c == '/' || c == '_' ? 63 : -1; }

static void draw_image(ctx_t *C, xel_t *n, const sst_t *st, const mat_t *ctm) {
    const char *h = xattr(n, "href");
    if (!h || strncmp(h, "data:", 5)) return;
    const char *comma = strchr(h, ',');
    if (!comma) return;
    bool b64 = false;
    for (const char *q = h; q < comma; q++) if (!strncmp(q, ";base64", 7)) b64 = true;
    const char *src = comma + 1;
    usize sl = strlen(src);
    u8 *data = kmalloc(sl + 1);
    if (!data) return;
    usize dl = 0;
    if (b64) {
        u32 acc = 0;
        int bits = 0;
        for (usize i = 0; i < sl; i++) {
            int v = b64v(src[i]);
            if (v < 0) continue;
            acc = acc << 6 | (u32)v; bits += 6;
            if (bits >= 8) { bits -= 8; data[dl++] = (u8)(acc >> bits); }
        }
    } else { memcpy(data, src, sl); dl = sl; }
    surface_t *img = image_decode(data, dl);
    kfree(data);
    if (!img) return;
    float fs = st->font_size;
    float x = parse_len(xattr(n, "x"), C->vw, fs, 0), y = parse_len(xattr(n, "y"), C->vh, fs, 0);
    float w = parse_len(xattr(n, "width"), C->vw, fs, (float)img->w), hh = parse_len(xattr(n, "height"), C->vh, fs, (float)img->h);
    char vb[64];
    snprintf(vb, sizeof vb, "0 0 %d %d", img->w, img->h);
    bool ok;
    mat_t m = mmul(*ctm, viewbox_mat(vb, xattr(n, "preserveAspectRatio"), x, y, w, hh, &ok));
    /* só escala + translação (o caso comum); rotação desenha a caixa alinhada */
    float x0, y0, x1, y1;
    mapp(&m, 0, 0, &x0, &y0);
    mapp(&m, (float)img->w, (float)img->h, &x1, &y1);
    int dx = (int)(MIN(x0, x1) + 0.5f), dy = (int)(MIN(y0, y1) + 0.5f), dw = (int)(fabs_(x1 - x0) + 0.5f), dh = (int)(fabs_(y1 - y0) + 0.5f);
    if (dw > 0 && dh > 0) gfx_blit_scaled(C->s, img, dx, dy, dw, dh, (int)(255 * st->opacity));
    surface_free(img);
}

static void draw_text(ctx_t *C, xel_t *n, const sst_t *st, const mat_t *ctm, float *cx, float *cy) {
    float fs = st->font_size;
    const char *xa = xattr(n, "x"), *ya = xattr(n, "y");
    if (xa) *cx = parse_len(xa, C->vw, fs, *cx);
    if (ya) *cy = parse_len(ya, C->vh, fs, *cy);
    *cx += parse_len(xattr(n, "dx"), C->vw, fs, 0);
    *cy += parse_len(xattr(n, "dy"), C->vh, fs, 0);
    for (xel_t *c = n->first; c; c = c->next) {
        if (c->name) {
            if (!strcmp(c->name, "tspan")) {
                sst_t ts;
                compute_style(C, c, st, &ts);
                if (!ts.none) draw_text(C, c, &ts, ctm, cx, cy);
            }
            continue;
        }
        /* espaços colapsados */
        char buf[512];
        int k = 0;
        bool sp = false;
        for (int i = 0; i < c->tlen && k < 510; i++) {
            char ch = c->text[i];
            if (isspace(ch)) { if (!sp && k) buf[k++] = ' '; sp = true; }
            else { buf[k++] = ch; sp = false; }
        }
        if (c == n->last && k && buf[k - 1] == ' ') k--;
        buf[k] = 0;
        if (!k || st->hidden) continue;
        float scale = mscale(ctm);
        float size = fs * scale;
        font_t *f = font_get(st->weight);
        int tw = text_width_n(f, size, buf, k);
        float dxu = (float)tw / (scale > 0 ? scale : 1);
        float ax = *cx;
        if (st->anchor == ANCHOR_MIDDLE) ax -= dxu / 2;
        else if (st->anchor == ANCHOR_END) ax -= dxu;
        float px, py;
        mapp(ctm, ax, *cy, &px, &py);
        paintsrc_t P;
        float bb[4] = { ax, *cy - fs, dxu, fs };
        if (make_paint(C, &st->fill, st, st->fill_op, ctm, bb, &P)) {
            u32 col = P.kind == 0 ? P.color : P.lut[128];
            /* text_draw recebe cor não pré-multiplicada */
            u32 a = col >> 24;
            u32 un = a ? ((((col >> 16) & 255) * 255 / a) << 16) | ((((col >> 8) & 255) * 255 / a) << 8) | ((col & 255) * 255 / a) : 0;
            text_draw_n(C->s, f, size, (int)(px + 0.5f), (int)(py + 0.5f), (a << 24) | un, buf, k);
        }
        *cx += dxu;
    }
}

static void render_node(ctx_t *C, xel_t *n, const sst_t *par, mat_t ctm) {
    const char *nm = n->name;
    if (!strcmp(nm, "defs") || !strcmp(nm, "style") || !strcmp(nm, "script") || !strcmp(nm, "title") || !strcmp(nm, "desc") ||
        !strcmp(nm, "metadata") || !strcmp(nm, "linearGradient") || !strcmp(nm, "radialGradient") || !strcmp(nm, "clipPath") ||
        !strcmp(nm, "mask") || !strcmp(nm, "pattern") || !strcmp(nm, "marker") || !strcmp(nm, "symbol") || !strcmp(nm, "filter"))
        return;
    sst_t st;
    compute_style(C, n, par, &st);
    if (st.none) return;
    ctm = mmul(ctm, parse_transform(xattr(n, "transform")));
    bool group = !strcmp(nm, "g") || !strcmp(nm, "svg") || !strcmp(nm, "use") || !strcmp(nm, "a") || !strcmp(nm, "switch");
    /* camada para opacidade de grupo e recorte */
    surface_t *saved = C->s;
    u8 *saved_clip = C->clipm, *own_clip = NULL;
    bool layer = group && st.opacity < 1;
    float sv_vw = C->vw, sv_vh = C->vh;
    if (st.clip[0]) {
        float bb[4] = { 0, 0, C->vw, C->vh };
        if (!group) { path_t tp = { 0 }; tp.tol = 1; bool f; shape_path(C, n, &st, &tp, &f); bbox_of(&tp, bb); path_free(&tp); }
        own_clip = clip_mask(C, st.clip, &st, ctm, bb);
        if (own_clip) {
            if (saved_clip) for (usize i = 0, e = (usize)C->W * C->H; i < e; i++) own_clip[i] = (u8)(own_clip[i] * saved_clip[i] / 255);
            C->clipm = own_clip;
        }
    }
    if (layer) {
        surface_t *ls = surface_new(C->W, C->H);
        if (ls && ls->px) { memset(ls->px, 0, (usize)ls->stride * ls->h * 4); C->s = ls; }
        else { surface_free(ls); layer = false; }
    }
    if (!strcmp(nm, "g") || !strcmp(nm, "a") || !strcmp(nm, "switch")) render_children(C, n, &st, ctm);
    else if (!strcmp(nm, "svg")) {
        float fs = st.font_size;
        float x = parse_len(xattr(n, "x"), C->vw, fs, 0), y = parse_len(xattr(n, "y"), C->vh, fs, 0);
        float w = parse_len(xattr(n, "width"), C->vw, fs, C->vw), h = parse_len(xattr(n, "height"), C->vh, fs, C->vh);
        bool ok;
        mat_t vm = viewbox_mat(xattr(n, "viewBox"), xattr(n, "preserveAspectRatio"), x, y, w, h, &ok);
        if (ok && w > 0 && h > 0) {
            const char *vb = xattr(n, "viewBox");
            float v[4];
            const char *p = vb;
            if (vb && parse_f(&p, &v[0]) && parse_f(&p, &v[1]) && parse_f(&p, &v[2]) && parse_f(&p, &v[3])) { C->vw = v[2]; C->vh = v[3]; }
            else { C->vw = w; C->vh = h; }
            render_children(C, n, &st, mmul(ctm, vm));
        }
    } else if (!strcmp(nm, "use")) {
        xel_t *t = by_id(C, xattr(n, "href"));
        if (t && C->depth < 16 && t != n) {
            float fs = st.font_size;
            mat_t tr = MID;
            tr.e = parse_len(xattr(n, "x"), C->vw, fs, 0);
            tr.f = parse_len(xattr(n, "y"), C->vh, fs, 0);
            mat_t m = mmul(ctm, tr);
            C->depth++;
            if (!strcmp(t->name, "symbol")) {
                float w = parse_len(xattr(n, "width"), C->vw, fs, parse_len(xattr(t, "width"), C->vw, fs, C->vw));
                float h = parse_len(xattr(n, "height"), C->vh, fs, parse_len(xattr(t, "height"), C->vh, fs, C->vh));
                bool ok;
                mat_t vm = viewbox_mat(xattr(t, "viewBox"), xattr(t, "preserveAspectRatio"), 0, 0, w, h, &ok);
                sst_t ss;
                compute_style(C, t, &st, &ss);
                if (ok && !ss.none) {
                    const char *vb = xattr(t, "viewBox");
                    float v[4];
                    const char *p = vb;
                    if (vb && parse_f(&p, &v[0]) && parse_f(&p, &v[1]) && parse_f(&p, &v[2]) && parse_f(&p, &v[3])) { C->vw = v[2]; C->vh = v[3]; }
                    render_children(C, t, &ss, mmul(mmul(m, vm), parse_transform(xattr(t, "transform"))));
                }
            } else render_node(C, t, &st, m);
            C->depth--;
        }
    } else if (!strcmp(nm, "text")) {
        float cx = 0, cy = 0;
        draw_text(C, n, &st, &ctm, &cx, &cy);
    } else if (!strcmp(nm, "image")) draw_image(C, n, &st, &ctm);
    else if (!strcmp(nm, "path") || !strcmp(nm, "rect") || !strcmp(nm, "circle") || !strcmp(nm, "ellipse") || !strcmp(nm, "line") ||
             !strcmp(nm, "polyline") || !strcmp(nm, "polygon")) {
        path_t p = { 0 };
        float sc = mscale(&ctm);
        p.tol = 0.15f / (sc > 1e-3f ? sc : 1e-3f);
        bool fillable;
        shape_path(C, n, &st, &p, &fillable);
        if (st.opacity < 1) { st.fill_op *= st.opacity; st.stroke_op *= st.opacity; }
        draw_path(C, &p, &st, &ctm, fillable);
        path_free(&p);
    }
    C->vw = sv_vw; C->vh = sv_vh;
    if (layer) {
        surface_t *ls = C->s;
        C->s = saved;
        u32 op = (u32)(st.opacity * 255 + 0.5f);
        for (int y = 0; y < C->H; y++)
            for (int x = 0; x < C->W; x++) {
                u32 c = ls->px[(usize)y * ls->stride + x];
                if (c) { u32 *d = &saved->px[(usize)y * saved->stride + x]; *d = over(*d, mul_px(c, op)); }
            }
        surface_free(ls);
    }
    if (own_clip) { kfree(own_clip); C->clipm = saved_clip; }
}

/* ================================================================ API */
static void root_size(xel_t *r, float *w, float *h) {
    const char *wa = xattr(r, "width"), *ha = xattr(r, "height"), *vb = xattr(r, "viewBox");
    float v[4] = { 0, 0, 0, 0 };
    bool hv = false;
    if (vb) { const char *p = vb; hv = parse_f(&p, &v[0]) && parse_f(&p, &v[1]) && parse_f(&p, &v[2]) && parse_f(&p, &v[3]) && v[2] > 0 && v[3] > 0; }
    bool pw = wa && strchr(wa, '%'), ph = ha && strchr(ha, '%');
    float W = wa && !pw ? parse_len(wa, 0, 16, 0) : 0, H = ha && !ph ? parse_len(ha, 0, 16, 0) : 0;
    if (W <= 0 && H <= 0) { if (hv) { W = v[2]; H = v[3]; } else { W = 300; H = 150; } }
    else if (W <= 0) W = hv ? H * v[2] / v[3] : 300;
    else if (H <= 0) H = hv ? W * v[3] / v[2] : 150;
    *w = W; *h = H;
}

bool svg_is(const u8 *d, usize n) {
    usize lim = n < 4096 ? n : 4096;
    for (usize i = 0; i + 4 < lim; i++)
        if (d[i] == '<' && (d[i + 1] == 's' || d[i + 1] == 'S') && (d[i + 2] == 'v' || d[i + 2] == 'V') && (d[i + 3] == 'g' || d[i + 3] == 'G') &&
            (isspace(d[i + 4]) || d[i + 4] == '>' || d[i + 4] == ':' || d[i + 4] == '/'))
            return true;
    return false;
}

bool svg_intrinsic(const char *src, int len, float *w, float *h) {
    xdoc_t X = { 0 };
    bool ok = xml_parse(&X, src, len);
    if (ok) root_size(X.root, w, h);
    arena_free(&X.A);
    return ok;
}

/* desenha o SVG num surface novo de w×h (0 = tamanho intrínseco); color = cor CSS herdada (currentColor) */
surface_t *svg_render_ex(const char *src, int len, int w, int h, u32 color) {
    ctx_t C;
    memset(&C, 0, sizeof C);
    if (!xml_parse(&C.X, src, len)) { arena_free(&C.X.A); return NULL; }
    xel_t *r = C.X.root;
    float iw, ih;
    root_size(r, &iw, &ih);
    if (w <= 0 || h <= 0) { w = (int)(iw + 0.5f); h = (int)(ih + 0.5f); }
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || (u64)w * h > 32u * 1024 * 1024) { arena_free(&C.X.A); return NULL; }
    surface_t *s = surface_new(w, h);
    if (!s || !s->px) { surface_free(s); arena_free(&C.X.A); return NULL; }
    memset(s->px, 0, (usize)s->stride * s->h * 4);
    C.s = s; C.W = w; C.H = h;
    collect(&C, r);
    for (int i = 1; i < C.nrules; i++) {   /* ordem estável por especificidade */
        rule_t t = C.rules[i];
        int j = i;
        while (j > 0 && C.rules[j - 1].spec > t.spec) { C.rules[j] = C.rules[j - 1]; j--; }
        C.rules[j] = t;
    }
    sst_t st;
    memset(&st, 0, sizeof st);
    st.fill.kind = PT_COLOR; st.fill.color = 0xFF000000u;
    st.stroke.kind = PT_NONE;
    st.fill_op = st.stroke_op = st.opacity = 1;
    st.sw = 1; st.miter = 4; st.font_size = 16; st.weight = FONT_REGULAR;
    st.color = color;
    /* o elemento raiz ocupa o surface inteiro: tamanho próprio mapeado para w×h */
    sst_t rs;
    compute_style(&C, r, &st, &rs);
    if (!rs.none) {
        bool ok;
        mat_t vm;
        const char *vb = xattr(r, "viewBox");
        float v[4];
        const char *p = vb;
        if (vb && parse_f(&p, &v[0]) && parse_f(&p, &v[1]) && parse_f(&p, &v[2]) && parse_f(&p, &v[3]) && v[2] > 0 && v[3] > 0) {
            vm = viewbox_mat(vb, xattr(r, "preserveAspectRatio"), 0, 0, (float)w, (float)h, &ok);
            C.vw = v[2]; C.vh = v[3];
        } else {
            vm = MID;
            vm.a = w / iw; vm.d = h / ih;
            C.vw = iw; C.vh = ih;
            ok = true;
        }
        mat_t ctm = mmul(vm, parse_transform(xattr(r, "transform")));
        if (ok) {
            surface_t *base = C.s;
            bool layer = rs.opacity < 1;
            if (layer) { C.s = surface_new(w, h); if (C.s) memset(C.s->px, 0, (usize)C.s->stride * h * 4); else { C.s = base; layer = false; } }
            render_children(&C, r, &rs, ctm);
            if (layer) {
                u32 op = (u32)(rs.opacity * 255 + 0.5f);
                for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++) base->px[(usize)y * base->stride + x] = mul_px(C.s->px[(usize)y * C.s->stride + x], op);
                surface_free(C.s);
                C.s = base;
            }
        }
    }
    if (C.rules) kfree(C.rules);
    if (C.ids) kfree(C.ids);
    arena_free(&C.X.A);
    return s;
}

surface_t *svg_render(const char *src, int len, int w, int h) { return svg_render_ex(src, len, w, h, 0xFF000000u); }
