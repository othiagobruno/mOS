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
    char mask[64], filter[64];                 /* não herdados */
    char mk_start[64], mk_mid[64], mk_end[64]; /* marcadores (herdados) */
    u32 flood_color;                           /* feFlood/feDropShadow (não herdados) */
    float flood_op;
    u8 mask_alpha;                             /* mask-type: alpha */
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
    sst_t base;           /* estilo da raiz: marcadores, padrões e máscaras herdam dos ancestrais deles, não de quem usa */
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

/* url(#id) → id (vazio se não for url) */
static void url_ref(const char *v, char *out) {
    out[0] = 0;
    if (strncmp(v, "url(", 4)) return;
    const char *s = v + 4;
    while (*s == ' ' || *s == '\'' || *s == '"') s++;
    if (*s == '#') s++;
    int n = 0;
    while (*s && *s != ')' && *s != '\'' && *s != '"' && n < 63) out[n++] = *s++;
    out[n] = 0;
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
    } else if (K("mask")) url_ref(v, st->mask);
    else if (K("filter")) url_ref(v, st->filter);
    else if (K("marker-start")) url_ref(v, st->mk_start);
    else if (K("marker-mid")) url_ref(v, st->mk_mid);
    else if (K("marker-end")) url_ref(v, st->mk_end);
    else if (K("marker")) { url_ref(v, st->mk_start); strcpy(st->mk_mid, st->mk_start); strcpy(st->mk_end, st->mk_start); }
    else if (K("flood-color")) { if (!strncasecmp(v, "currentcolor", 12)) st->flood_color = st->color; else { u32 c; if (css_color(v, &c)) st->flood_color = c; } }
    else if (K("flood-opacity")) st->flood_op = parse_op(v);
    else if (K("mask-type")) st->mask_alpha = !strncmp(v, "alpha", 5);
    else if (K("stop-color")) { if (!strncasecmp(v, "currentcolor", 12)) st->stop_color = st->color; else { u32 c; if (css_color(v, &c)) st->stop_color = c; } }
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
    "mask", "filter", "marker", "marker-start", "marker-mid", "marker-end", "flood-color", "flood-opacity", "mask-type",
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
    st->mask[0] = st->filter[0] = 0;
    st->flood_color = 0xFF000000u;
    st->flood_op = 1;
    st->mask_alpha = 0;
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
    mat_t inv;            /* canvas → espaço do gradiente (ou pixels do ladrilho do padrão) */
    float x1, y1, x2, y2, cx, cy, r, fx, fy;
    surface_t *tile;      /* kind 3: ladrilho do <pattern> já desenhado (liberar com paint_free) */
    u32 op8;              /* opacidade aplicada ao padrão */
} paintsrc_t;

static void render_children(ctx_t *C, xel_t *n, const sst_t *st, mat_t ctm);
static mat_t viewbox_mat(const char *vb, const char *par, float x, float y, float w, float h, bool *ok);
static void paint_free(paintsrc_t *P) { if (P->tile) { surface_free(P->tile); P->tile = NULL; } }


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
/* número de unidade de caixa: "0.5" ou "50%" → fração */
static float frac_len(const char *v, float def) {
    if (!v) return def;
    const char *q = v;
    float f;
    if (!parse_f(&q, &f)) return def;
    return *q == '%' ? f / 100 : f;
}

/* <pattern>: desenha um ladrilho na resolução do dispositivo; a amostragem repete pelos pixels do ladrilho */
static bool make_pattern(ctx_t *C, xel_t *g, const sst_t *st, float op, const mat_t *ctm, const float *bb, paintsrc_t *P) {
    if (C->depth > 8) return false;
    const char *units = gattr(C, g, "patternUnits"), *cunits = gattr(C, g, "patternContentUnits");
    bool obb = !units || strcmp(units, "userSpaceOnUse");
    bool cobb = cunits && !strcmp(cunits, "objectBoundingBox");
    float x, y, w, h;
    if (obb) {
        x = bb[0] + frac_len(gattr(C, g, "x"), 0) * bb[2]; y = bb[1] + frac_len(gattr(C, g, "y"), 0) * bb[3];
        w = frac_len(gattr(C, g, "width"), 0) * bb[2]; h = frac_len(gattr(C, g, "height"), 0) * bb[3];
    } else {
        x = parse_len(gattr(C, g, "x"), C->vw, 16, 0); y = parse_len(gattr(C, g, "y"), C->vh, 16, 0);
        w = parse_len(gattr(C, g, "width"), C->vw, 16, 0); h = parse_len(gattr(C, g, "height"), C->vh, 16, 0);
    }
    if (!(w > 0) || !(h > 0)) return false;
    mat_t pm = mmul(*ctm, parse_transform(gattr(C, g, "patternTransform")));
    float sc = mscale(&pm);
    int tw = (int)fceil(w * sc), th = (int)fceil(h * sc);
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    if (tw > 2048) tw = 2048;
    if (th > 2048) th = 2048;
    float sx = tw / w, sy = th / h;
    /* conteúdo: elementos do próprio pattern ou do que ele herda por href */
    xel_t *content = g;
    for (int d = 0; d < 8 && content && !content->first; d++) content = by_id(C, xattr(content, "href"));
    surface_t *tile = surface_new(tw, th);
    if (!tile || !tile->px) { surface_free(tile); return false; }
    memset(tile->px, 0, (usize)tile->stride * th * 4);
    if (content) {
        mat_t tm = { sx, 0, 0, sy, 0, 0 };
        bool okv;
        const char *vb = gattr(C, g, "viewBox");
        mat_t vm = viewbox_mat(vb, gattr(C, g, "preserveAspectRatio"), 0, 0, w, h, &okv);
        if (vb && okv) tm = mmul(tm, vm);
        else if (cobb) { mat_t b = { bb[2], 0, 0, bb[3], 0, 0 }; tm = mmul(tm, b); }
        surface_t *ss = C->s;
        int sw = C->W, sh = C->H;
        u8 *sclip = C->clipm;
        float svw = C->vw, svh = C->vh;
        C->s = tile; C->W = tw; C->H = th; C->clipm = NULL;
        sst_t ps;
        compute_style(C, g, &C->base, &ps);
        C->depth++;
        render_children(C, content, &ps, tm);
        C->depth--;
        C->s = ss; C->W = sw; C->H = sh; C->clipm = sclip; C->vw = svw; C->vh = svh;
    }
    mat_t t2u = { 1 / sx, 0, 0, 1 / sy, x, y };
    if (!minv(mmul(pm, t2u), &P->inv)) { surface_free(tile); return false; }
    P->kind = 3;
    P->tile = tile;
    P->op8 = (u32)(clamp01(op) * 255 + 0.5f);
    return true;
}

static bool make_paint(ctx_t *C, const paint_t *pt, const sst_t *st, float op, const mat_t *ctm, const float *bbox, paintsrc_t *P) {
    memset(P, 0, sizeof *P);
    if (pt->kind == PT_NONE) return false;
    if (pt->kind == PT_COLOR || pt->kind == PT_CURRENT) {
        P->kind = 0;
        P->color = premul(pt->kind == PT_CURRENT ? st->color : pt->color, op);
        return (P->color >> 24) != 0;
    }
    xel_t *g = by_id(C, pt->id);
    if (g && !strcmp(g->name, "pattern")) {
        if (make_pattern(C, g, st, op, ctm, bbox, P)) return true;
        if (pt->has_fb) { P->kind = 0; P->color = premul(pt->fallback, op); return true; }
        return false;
    }
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

static inline u32 pat_at(const paintsrc_t *P, float px, float py) {
    float u, v;
    mapp(&P->inv, px, py, &u, &v);
    int tw = P->tile->w, th = P->tile->h;
    int iu = (int)ffloor(u), iv = (int)ffloor(v);
    iu %= tw; if (iu < 0) iu += tw;
    iv %= th; if (iv < 0) iv += th;
    u32 c = P->tile->px[(usize)iv * P->tile->stride + iu];
    return P->op8 == 255 ? c : mul_px(c, P->op8);
}

static inline u32 grad_at(const paintsrc_t *P, float px, float py) {
    if (P->kind == 3) return pat_at(P, px, py);
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
    if (can_fill && make_paint(C, &st->fill, st, st->fill_op, ctm, bb, &P)) { fill_path(C, p, ctm, st->evenodd, &P); paint_free(&P); }
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
        paint_free(&P);
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

/* ================================================================ texto */
/* glifo com matriz completa: m leva o espaço do glifo (unidades do usuário, origem na caneta, y para baixo) ao canvas */
static void draw_glyph(ctx_t *C, int face, u16 gid, float fs, const mat_t *m, const paintsrc_t *P) {
    float sc = mscale(m);
    float gs = fs * sc;
    if (gs < 1) gs = 1;
    if (gs > 512) gs = 512;
    float k = gs / fs;   /* pixels do bitmap por unidade do usuário */
    i16 w, h, l, t;
    if (font_raster_glyph(face, gid, gs, NULL, 0, &w, &h, &l, &t) <= 0 || w <= 0 || h <= 0) return;
    u8 *bm = kmalloc((usize)w * h);
    if (!bm) return;
    font_raster_glyph(face, gid, gs, bm, (u32)w * h, &w, &h, &l, &t);
    /* bitmap → espaço do glifo → canvas */
    mat_t b2g = { 1 / k, 0, 0, 1 / k, l / k, t / k };
    mat_t b2d = mmul(*m, b2g), d2b;
    if (!minv(b2d, &d2b)) { kfree(bm); return; }
    float xs[4], ys[4];
    mapp(&b2d, 0, 0, &xs[0], &ys[0]); mapp(&b2d, w, 0, &xs[1], &ys[1]);
    mapp(&b2d, 0, h, &xs[2], &ys[2]); mapp(&b2d, w, h, &xs[3], &ys[3]);
    float x0 = xs[0], x1 = xs[0], y0 = ys[0], y1 = ys[0];
    for (int i = 1; i < 4; i++) { x0 = MIN(x0, xs[i]); x1 = MAX(x1, xs[i]); y0 = MIN(y0, ys[i]); y1 = MAX(y1, ys[i]); }
    int X0 = MAX((int)ffloor(x0), 0), Y0 = MAX((int)ffloor(y0), 0), X1 = MIN((int)fceil(x1) + 1, C->W), Y1 = MIN((int)fceil(y1) + 1, C->H);
    if (X1 > X0 && Y1 > Y0) {
        mask_t M = { X0, Y0, X1 - X0, Y1 - Y0, kcalloc((usize)(X1 - X0) * (Y1 - Y0), 1) };
        if (M.a) {
            for (int yy = 0; yy < M.h; yy++)
                for (int xx = 0; xx < M.w; xx++) {
                    float u, v;
                    mapp(&d2b, X0 + xx + 0.5f, Y0 + yy + 0.5f, &u, &v);
                    u -= 0.5f; v -= 0.5f;
                    int iu = (int)ffloor(u), iv = (int)ffloor(v);
                    if (iu < -1 || iv < -1 || iu >= w || iv >= h) continue;
                    float fu = u - iu, fv = v - iv;
                    float a00 = (iu >= 0 && iv >= 0) ? bm[iv * w + iu] : 0, a10 = (iu + 1 < w && iv >= 0) ? bm[iv * w + iu + 1] : 0;
                    float a01 = (iu >= 0 && iv + 1 < h) ? bm[(iv + 1) * w + iu] : 0, a11 = (iu + 1 < w && iv + 1 < h) ? bm[(iv + 1) * w + iu + 1] : 0;
                    float c = (a00 * (1 - fu) + a10 * fu) * (1 - fv) + (a01 * (1 - fu) + a11 * fu) * fv;
                    M.a[(usize)yy * M.w + xx] = (u8)(c > 255 ? 255 : c + 0.5f);
                }
            composite(C, &M, P);
            kfree(M.a);
        }
    }
    kfree(bm);
}

/* fonte e avanço de um ponto de código (com a fonte de reserva) */
static u16 glyph_for(int weight, u32 cp, int *face, float fs, float *adv) {
    int f = weight;
    u16 g = font_glyph_id(f, cp);
    if (!g && cp != ' ') { u16 g2 = font_glyph_id(FONT_FALLBACK_WEIGHT, cp); if (g2) { g = g2; f = FONT_FALLBACK_WEIGHT; } }
    i32 upem = 0;
    font_face_metrics(f, &upem, NULL, NULL, NULL);
    if (!upem) upem = 2048;
    *face = f;
    *adv = font_advance_units(f, g) * fs / upem;
    return g;
}

/* texto em caminho: posição e ângulo na distância d de um caminho achatado */
typedef struct { float *xy; float *len; int n; float total; } pathlen_t;
static bool pathlen_at(const pathlen_t *pl, float d, float *x, float *y, float *ang) {
    if (d < 0 || d > pl->total || pl->n < 2) return false;
    int i = 1;
    while (i < pl->n - 1 && pl->len[i] < d) i++;
    float seg = pl->len[i] - pl->len[i - 1], t = seg > 0 ? (d - pl->len[i - 1]) / seg : 0;
    float ax = pl->xy[2 * (i - 1)], ay = pl->xy[2 * (i - 1) + 1], bx = pl->xy[2 * i], by = pl->xy[2 * i + 1];
    *x = ax + (bx - ax) * t;
    *y = ay + (by - ay) * t;
    *ang = atan2_(by - ay, bx - ax);
    return true;
}

typedef struct { float *rot; int nrot, idx; } rotlist_t;

/* uma corrida de texto já colapsada: posições, rotate e (em textPath) o caminho */
static void text_run(ctx_t *C, const char *buf, int k, const sst_t *st, const mat_t *ctm, float *cx, float *cy, rotlist_t *rl, const pathlen_t *pl) {
    float fs = st->font_size;
    /* largura para a âncora */
    float width = 0;
    for (const char *q = buf; q < buf + k;) { u32 cp = utf8_decode(&q); int f; float adv; glyph_for(st->weight, cp, &f, fs, &adv); width += adv; }
    float ax = *cx;
    if (st->anchor == ANCHOR_MIDDLE) ax -= width / 2;
    else if (st->anchor == ANCHOR_END) ax -= width;
    paintsrc_t P;
    float bb[4] = { ax, *cy - fs, width, fs };
    bool paint = make_paint(C, &st->fill, st, st->fill_op, ctm, bb, &P);
    float pen = ax;
    for (const char *q = buf; q < buf + k;) {
        u32 cp = utf8_decode(&q);
        int face;
        float adv;
        u16 g = glyph_for(st->weight, cp, &face, fs, &adv);
        float r = 0;
        if (rl && rl->nrot) { r = rl->rot[rl->idx < rl->nrot ? rl->idx : rl->nrot - 1]; rl->idx++; }
        mat_t gm;
        bool draw = true;
        if (pl) {
            /* o meio do glifo fica no caminho, girado pela tangente */
            float px = 0, py = 0, ang = 0;
            draw = pathlen_at(pl, pen + adv / 2, &px, &py, &ang);
            mat_t t1 = MID; t1.e = px; t1.f = py;
            float rr = ang + r * PI_F / 180;
            mat_t ro = { fcos(rr), fsin(rr), -fsin(rr), fcos(rr), 0, 0 };
            mat_t t2 = MID; t2.e = -adv / 2; t2.f = *cy;
            gm = mmul(mmul(mmul(*ctm, t1), ro), t2);
        } else {
            mat_t t1 = MID; t1.e = pen; t1.f = *cy;
            float rr = r * PI_F / 180;
            mat_t ro = { fcos(rr), fsin(rr), -fsin(rr), fcos(rr), 0, 0 };
            gm = mmul(mmul(*ctm, t1), ro);
        }
        if (draw && paint && cp != ' ' && g) draw_glyph(C, face, g, fs, &gm, &P);
        pen += adv;
    }
    if (paint) paint_free(&P);
    *cx = pen;
}

static void collapse_text(const xel_t *c, const xel_t *parent, char *buf, int cap, int *len) {
    int k = 0;
    bool sp = false;
    for (int i = 0; i < c->tlen && k < cap - 2; i++) {
        char ch = c->text[i];
        if (isspace(ch)) { if (!sp && k) buf[k++] = ' '; sp = true; }
        else { buf[k++] = ch; sp = false; }
    }
    if (c == parent->last && k && buf[k - 1] == ' ') k--;
    buf[k] = 0;
    *len = k;
}

static void read_rotate(const char *v, rotlist_t *rl) {
    if (!v) return;
    static float pool[256];
    rl->rot = pool;
    rl->nrot = 0;
    rl->idx = 0;
    float f;
    while (rl->nrot < 256 && parse_f(&v, &f)) pool[rl->nrot++] = f;
}

static void draw_text(ctx_t *C, xel_t *n, const sst_t *st, const mat_t *ctm, float *cx, float *cy) {
    float fs = st->font_size;
    const char *xa = xattr(n, "x"), *ya = xattr(n, "y");
    if (xa) *cx = parse_len(xa, C->vw, fs, *cx);
    if (ya) *cy = parse_len(ya, C->vh, fs, *cy);
    *cx += parse_len(xattr(n, "dx"), C->vw, fs, 0);
    *cy += parse_len(xattr(n, "dy"), C->vh, fs, 0);
    rotlist_t rl = { 0 };
    read_rotate(xattr(n, "rotate"), &rl);
    for (xel_t *c = n->first; c; c = c->next) {
        if (c->name) {
            sst_t ts;
            compute_style(C, c, st, &ts);
            if (ts.none) continue;
            if (!strcmp(c->name, "tspan")) draw_text(C, c, &ts, ctm, cx, cy);
            else if (!strcmp(c->name, "textPath")) {
                xel_t *pe = by_id(C, xattr(c, "href"));
                if (!pe || strcmp(pe->name, "path") || !xattr(pe, "d")) continue;
                path_t pp = { 0 };
                pp.tol = 0.1f;
                parse_path_d(&pp, xattr(pe, "d"));
                mat_t pt = parse_transform(xattr(pe, "transform"));
                if (pp.n < 2 || !pp.nc) { path_free(&pp); continue; }
                /* só o primeiro subcaminho, no espaço do <text> */
                int np = rs_cont_len(&pp, 0);
                float *xy = kmalloc(sizeof(float) * 2 * np), *len = kmalloc(sizeof(float) * np);
                if (xy && len) {
                    float tot = 0;
                    for (int i = 0; i < np; i++) {
                        mapp(&pt, pp.xy[2 * i], pp.xy[2 * i + 1], &xy[2 * i], &xy[2 * i + 1]);
                        if (i) { float dx = xy[2 * i] - xy[2 * i - 2], dy = xy[2 * i + 1] - xy[2 * i - 1]; tot += sqrtf_(dx * dx + dy * dy); }
                        len[i] = tot;
                    }
                    pathlen_t pl = { xy, len, np, tot };
                    const char *so = xattr(c, "startOffset");
                    float start = so ? (strchr(so, '%') ? frac_len(so, 0) * tot : parse_len(so, tot, fs, 0)) : 0;
                    float pen = start;
                    float y0 = 0;
                    for (xel_t *t = c->first; t; t = t->next) {
                        if (t->name) continue;
                        char buf[512];
                        int k;
                        collapse_text(t, c, buf, sizeof buf, &k);
                        if (k && !ts.hidden) text_run(C, buf, k, &ts, ctm, &pen, &y0, NULL, &pl);
                    }
                }
                kfree(xy); kfree(len);
                path_free(&pp);
            }
            continue;
        }
        char buf[512];
        int k;
        collapse_text(c, n, buf, sizeof buf, &k);
        if (!k || st->hidden) continue;
        text_run(C, buf, k, st, ctm, cx, cy, rl.nrot ? &rl : NULL, NULL);
    }
}

/* ================================================================ caixa, máscara, filtros e marcadores */
/* caixa do elemento no espaço m (filhos com as próprias transformações) */
static void bbox_acc(ctx_t *C, xel_t *n, const sst_t *par, mat_t m, float *bb, bool *any, int depth) {
    if (!n->name || depth > 16) return;
    sst_t st;
    compute_style(C, n, par, &st);
    if (st.none) return;
    const char *nm = n->name;
    if (!strcmp(nm, "g") || !strcmp(nm, "a") || !strcmp(nm, "switch") || !strcmp(nm, "svg")) {
        mat_t cm = depth ? mmul(m, parse_transform(xattr(n, "transform"))) : m;
        for (xel_t *c = n->first; c; c = c->next) bbox_acc(C, c, &st, cm, bb, any, depth + 1);
        return;
    }
    if (!strcmp(nm, "use")) {
        xel_t *t = by_id(C, xattr(n, "href"));
        if (!t || t == n) return;
        mat_t tr = depth ? parse_transform(xattr(n, "transform")) : MID;
        tr.e += parse_len(xattr(n, "x"), C->vw, st.font_size, 0);
        tr.f += parse_len(xattr(n, "y"), C->vh, st.font_size, 0);
        bbox_acc(C, t, &st, mmul(m, tr), bb, any, depth + 1);
        return;
    }
    path_t p = { 0 };
    p.tol = 0.5f;
    bool f;
    if (!strcmp(nm, "text")) {
        float x = parse_len(xattr(n, "x"), C->vw, st.font_size, 0), y = parse_len(xattr(n, "y"), C->vh, st.font_size, 0);
        path_move(&p, x, y - st.font_size); path_line(&p, x + st.font_size * 4, y + st.font_size * 0.25f);
    } else shape_path(C, n, &st, &p, &f);
    mat_t em = depth ? mmul(m, parse_transform(xattr(n, "transform"))) : m;
    for (int i = 0; i < p.n; i++) {
        float x, y;
        mapp(&em, p.xy[2 * i], p.xy[2 * i + 1], &x, &y);
        if (!*any) { bb[0] = bb[2] = x; bb[1] = bb[3] = y; *any = true; }
        else { bb[0] = MIN(bb[0], x); bb[1] = MIN(bb[1], y); bb[2] = MAX(bb[2], x); bb[3] = MAX(bb[3], y); }
    }
    path_free(&p);
}
static void elem_bbox(ctx_t *C, xel_t *n, const sst_t *par, float *out) {
    float b[4] = { 0, 0, 0, 0 };
    bool any = false;
    bbox_acc(C, n, par, MID, b, &any, 0);
    out[0] = b[0]; out[1] = b[1]; out[2] = b[2] - b[0]; out[3] = b[3] - b[1];
}

/* região de máscara/filtro: unidades objectBoundingBox (padrão −10% −10% 120% 120%) ou do usuário */
static void fx_region(ctx_t *C, xel_t *e, const char *unit_attr, const float *bb, float *r) {
    const char *u = gattr(C, e, unit_attr);
    if (!u || strcmp(u, "userSpaceOnUse")) {
        r[0] = bb[0] + frac_len(gattr(C, e, "x"), -0.1f) * bb[2]; r[1] = bb[1] + frac_len(gattr(C, e, "y"), -0.1f) * bb[3];
        r[2] = frac_len(gattr(C, e, "width"), 1.2f) * bb[2]; r[3] = frac_len(gattr(C, e, "height"), 1.2f) * bb[3];
    } else {
        r[0] = parse_len(gattr(C, e, "x"), C->vw, 16, -0.1f * C->vw); r[1] = parse_len(gattr(C, e, "y"), C->vh, 16, -0.1f * C->vh);
        r[2] = parse_len(gattr(C, e, "width"), C->vw, 16, 1.2f * C->vw); r[3] = parse_len(gattr(C, e, "height"), C->vh, 16, 1.2f * C->vh);
    }
}

/* cobertura de um retângulo do usuário no canvas */
static u8 *rect_cov(ctx_t *C, const float *r, const mat_t *ctm) {
    path_t p = { 0 };
    p.tol = 1;
    path_move(&p, r[0], r[1]); path_line(&p, r[0] + r[2], r[1]); path_line(&p, r[0] + r[2], r[1] + r[3]); path_line(&p, r[0], r[1] + r[3]); path_close(&p);
    edges_t E = { 0 };
    edges_from_path(&E, &p, ctm);
    mask_t M = rasterize(&E, false, 0, 0, C->W, C->H);
    if (E.e) kfree(E.e);
    path_free(&p);
    u8 *out = kcalloc((usize)C->W * C->H, 1);
    if (out && M.a) for (int y = 0; y < M.h; y++) memcpy(out + (usize)(M.y + y) * C->W + M.x, M.a + (usize)y * M.w, (usize)M.w);
    if (M.a) kfree(M.a);
    return out;
}

/* <mask>: luminância (ou alfa) do conteúdo, recortada à região */
static u8 *mask_values(ctx_t *C, const char *id, const sst_t *st, const mat_t *ctm, const float *bb) {
    xel_t *mk = by_id(C, id);
    if (!mk || strcmp(mk->name, "mask") || C->depth > 8) return NULL;
    float reg[4];
    fx_region(C, mk, "maskUnits", bb, reg);
    surface_t *ms = surface_new(C->W, C->H);
    if (!ms || !ms->px) { surface_free(ms); return NULL; }
    memset(ms->px, 0, (usize)ms->stride * ms->h * 4);
    surface_t *saved = C->s;
    u8 *sclip = C->clipm;
    C->s = ms; C->clipm = NULL;
    mat_t cm = *ctm;
    const char *cu = gattr(C, mk, "maskContentUnits");
    if (cu && !strcmp(cu, "objectBoundingBox")) { mat_t b = { bb[2], 0, 0, bb[3], bb[0], bb[1] }; cm = mmul(cm, b); }
    sst_t ms_st;
    compute_style(C, mk, &C->base, &ms_st);
    C->depth++;
    render_children(C, mk, &ms_st, cm);
    C->depth--;
    C->s = saved; C->clipm = sclip;
    u8 *cov = rect_cov(C, reg, ctm);
    u8 *out = kmalloc((usize)C->W * C->H);
    if (out && cov) {
        for (int y = 0; y < C->H; y++)
            for (int x = 0; x < C->W; x++) {
                u32 c = ms->px[(usize)y * ms->stride + x];
                u32 v;
                if (ms_st.mask_alpha) v = c >> 24;
                else v = (u32)(0.2125f * ((c >> 16) & 255) + 0.7154f * ((c >> 8) & 255) + 0.0721f * (c & 255) + 0.5f);   /* luminância já pré-multiplicada */
                out[(usize)y * C->W + x] = (u8)(v * cov[(usize)y * C->W + x] / 255);
            }
    } else { kfree(out); out = NULL; }
    kfree(cov);
    surface_free(ms);
    return out;
}

/* ---------------------------------------------------------------- filtros */
typedef struct { u32 *px; } fimg_t;   /* imagem W×H pré-multiplicada */

static u32 *fimg_new(ctx_t *C) { return kcalloc((usize)C->W * C->H, 4); }

static void blur_channel_pass(u8 *a, u8 *tmp, int w, int h, int r, bool horiz) {
    if (r <= 0) return;
    int n = horiz ? w : h, m = horiz ? h : w, step = horiz ? 1 : w, div = 2 * r + 1;
    for (int j = 0; j < m; j++) {
        u8 *src = horiz ? a + (usize)j * w : a + j, *dst = horiz ? tmp + (usize)j * w : tmp + j;
        int acc = 0;
        for (int i = -r; i <= r; i++) if (i >= 0 && i < n) acc += src[i * step];
        for (int i = 0; i < n; i++) {
            dst[i * step] = (u8)((acc + div / 2) / div);
            int ad = i + r + 1, sb = i - r;
            if (ad < n) acc += src[ad * step];
            if (sb >= 0) acc -= src[sb * step];
        }
    }
    memcpy(a, tmp, (usize)w * h);
}

static void box3(float sigma, int b[3]) {
    b[0] = b[1] = b[2] = 0;
    if (!(sigma > 0)) return;
    if (sigma > 200) sigma = 200;
    float wi = sqrtf_(12 * sigma * sigma / 3 + 1);
    int wl = (int)ffloor(wi);
    if (!(wl & 1)) wl--;
    int wu = wl + 2;
    float mi = (12 * sigma * sigma - 3 * wl * wl - 12 * wl - 9) / (-4.0f * wl - 4);
    int mm = (int)(mi + (mi >= 0 ? 0.5f : -0.5f));
    for (int i = 0; i < 3; i++) b[i] = ((i < mm ? wl : wu) - 1) / 2;
}

static void fe_blur(ctx_t *C, u32 *img, float sx, float sy) {
    int W = C->W, H = C->H, bx[3], by[3];
    box3(sx, bx); box3(sy, by);
    u8 *ch = kmalloc((usize)W * H), *tmp = kmalloc((usize)W * H);
    if (!ch || !tmp) { kfree(ch); kfree(tmp); return; }
    for (int c = 0; c < 4; c++) {
        int sh = c * 8;
        for (usize i = 0; i < (usize)W * H; i++) ch[i] = (u8)(img[i] >> sh);
        for (int k = 0; k < 3; k++) { blur_channel_pass(ch, tmp, W, H, bx[k], true); blur_channel_pass(ch, tmp, W, H, by[k], false); }
        for (usize i = 0; i < (usize)W * H; i++) img[i] = (img[i] & ~(0xFFu << sh)) | ((u32)ch[i] << sh);
    }
    kfree(ch); kfree(tmp);
}

static void fe_offset(ctx_t *C, u32 *img, int dx, int dy) {
    int W = C->W, H = C->H;
    u32 *o = fimg_new(C);
    if (!o) return;
    for (int y = 0; y < H; y++) {
        int sy = y - dy;
        if (sy < 0 || sy >= H) continue;
        for (int x = 0; x < W; x++) { int sx = x - dx; if (sx >= 0 && sx < W) o[(usize)y * W + x] = img[(usize)sy * W + sx]; }
    }
    memcpy(img, o, (usize)W * H * 4);
    kfree(o);
}

static u32 unpre(u32 c, float *r, float *g, float *b, float *a) {
    *a = (c >> 24) / 255.0f;
    if (*a <= 0) { *r = *g = *b = 0; return 0; }
    *r = ((c >> 16) & 255) / 255.0f / *a; *g = ((c >> 8) & 255) / 255.0f / *a; *b = (c & 255) / 255.0f / *a;
    return 1;
}
static u32 repre(float r, float g, float b, float a) {
    a = clamp01(a); r = clamp01(r); g = clamp01(g); b = clamp01(b);
    return (u32)(a * 255 + 0.5f) << 24 | (u32)(r * a * 255 + 0.5f) << 16 | (u32)(g * a * 255 + 0.5f) << 8 | (u32)(b * a * 255 + 0.5f);
}

static void fe_colormatrix(ctx_t *C, u32 *img, xel_t *e) {
    const char *type = xattr(e, "type"), *vals = xattr(e, "values");
    float m[20] = { 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0 };
    if (!type || !strcmp(type, "matrix")) {
        const char *q = vals;
        float v[20];
        int n = 0;
        while (q && n < 20 && parse_f(&q, &v[n])) n++;
        if (n == 20) memcpy(m, v, sizeof m);
    } else if (!strcmp(type, "saturate")) {
        float sat = frac_len(vals, 1);
        float t[20] = { 0.213f + 0.787f * sat, 0.715f - 0.715f * sat, 0.072f - 0.072f * sat, 0, 0,
                        0.213f - 0.213f * sat, 0.715f + 0.285f * sat, 0.072f - 0.072f * sat, 0, 0,
                        0.213f - 0.213f * sat, 0.715f - 0.715f * sat, 0.072f + 0.928f * sat, 0, 0, 0, 0, 0, 1, 0 };
        memcpy(m, t, sizeof m);
    } else if (!strcmp(type, "hueRotate")) {
        float deg = 0;
        const char *q = vals;
        if (q) parse_f(&q, &deg);
        float a = deg * PI_F / 180, cs = fcos(a), sn = fsin(a);
        float t[20] = { 0.213f + cs * 0.787f - sn * 0.213f, 0.715f - cs * 0.715f - sn * 0.715f, 0.072f - cs * 0.072f + sn * 0.928f, 0, 0,
                        0.213f - cs * 0.213f + sn * 0.143f, 0.715f + cs * 0.285f + sn * 0.140f, 0.072f - cs * 0.072f - sn * 0.283f, 0, 0,
                        0.213f - cs * 0.213f - sn * 0.787f, 0.715f - cs * 0.715f + sn * 0.715f, 0.072f + cs * 0.928f + sn * 0.072f, 0, 0, 0, 0, 0, 1, 0 };
        memcpy(m, t, sizeof m);
    } else if (!strcmp(type, "luminanceToAlpha")) {
        float t[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2125f, 0.7154f, 0.0721f, 0, 0 };
        memcpy(m, t, sizeof m);
    }
    for (usize i = 0; i < (usize)C->W * C->H; i++) {
        float r, g, b, a;
        unpre(img[i], &r, &g, &b, &a);
        float o[4];
        for (int k = 0; k < 4; k++) o[k] = m[5 * k] * r + m[5 * k + 1] * g + m[5 * k + 2] * b + m[5 * k + 3] * a + m[5 * k + 4];
        img[i] = repre(o[0], o[1], o[2], o[3]);
    }
}

/* feComponentTransfer: feFuncR/G/B/A */
static float transfer(xel_t *f, float c) {
    const char *t = xattr(f, "type");
    if (!t || !strcmp(t, "identity")) return c;
    if (!strcmp(t, "table") || !strcmp(t, "discrete")) {
        float v[64];
        int n = 0;
        const char *q = xattr(f, "tableValues");
        while (q && n < 64 && parse_f(&q, &v[n])) n++;
        if (!n) return c;
        if (!strcmp(t, "discrete")) { int k = (int)ffloor(c * n); if (k >= n) k = n - 1; return v[k]; }
        if (n == 1) return v[0];
        float pos = c * (n - 1);
        int k = (int)ffloor(pos);
        if (k >= n - 1) return v[n - 1];
        return v[k] + (pos - k) * (v[k + 1] - v[k]);
    }
    if (!strcmp(t, "linear")) return frac_len(xattr(f, "slope"), 1) * c + frac_len(xattr(f, "intercept"), 0);
    if (!strcmp(t, "gamma")) return frac_len(xattr(f, "amplitude"), 1) * fpow(c, frac_len(xattr(f, "exponent"), 1)) + frac_len(xattr(f, "offset"), 0);
    return c;
}
static void fe_transfer(ctx_t *C, u32 *img, xel_t *e) {
    xel_t *fn[4] = { NULL, NULL, NULL, NULL };
    for (xel_t *c = e->first; c; c = c->next) {
        if (!c->name) continue;
        if (!strcmp(c->name, "feFuncR")) fn[0] = c; else if (!strcmp(c->name, "feFuncG")) fn[1] = c;
        else if (!strcmp(c->name, "feFuncB")) fn[2] = c; else if (!strcmp(c->name, "feFuncA")) fn[3] = c;
    }
    for (usize i = 0; i < (usize)C->W * C->H; i++) {
        float v[4];
        unpre(img[i], &v[0], &v[1], &v[2], &v[3]);
        for (int k = 0; k < 4; k++) if (fn[k]) v[k] = transfer(fn[k], v[k]);
        img[i] = repre(v[0], v[1], v[2], v[3]);
    }
}

static void fe_morph(ctx_t *C, u32 *img, int rx, int ry, bool dilate) {
    int W = C->W, H = C->H;
    if (rx <= 0 && ry <= 0) return;
    if (rx > 64) rx = 64;
    if (ry > 64) ry = 64;
    u32 *o = fimg_new(C);
    if (!o) return;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            u32 best[4] = { dilate ? 0u : 255u, dilate ? 0u : 255u, dilate ? 0u : 255u, dilate ? 0u : 255u };
            for (int dy = -ry; dy <= ry; dy++) {
                int yy = y + dy;
                for (int dx = -rx; dx <= rx; dx++) {
                    int xx = x + dx;
                    u32 c = (yy < 0 || yy >= H || xx < 0 || xx >= W) ? 0 : img[(usize)yy * W + xx];
                    for (int k = 0; k < 4; k++) { u32 v = (c >> (8 * k)) & 255; if (dilate ? v > best[k] : v < best[k]) best[k] = v; }
                }
            }
            o[(usize)y * W + x] = best[0] | best[1] << 8 | best[2] << 16 | best[3] << 24;
        }
    memcpy(img, o, (usize)W * H * 4);
    kfree(o);
}

static u32 fe_comp_px(const char *op, u32 a, u32 b, const float *k) {
    u32 aa = a >> 24, ba = b >> 24;
    if (!op || !strcmp(op, "over")) return over(b, a);
    if (!strcmp(op, "in")) return mul_px(a, ba);
    if (!strcmp(op, "out")) return mul_px(a, 255 - ba);
    if (!strcmp(op, "atop")) return mul_px(a, ba) + mul_px(b, 255 - aa);
    if (!strcmp(op, "xor")) return mul_px(a, 255 - ba) + mul_px(b, 255 - aa);
    if (!strcmp(op, "arithmetic")) {
        u32 out = 0;
        for (int sh = 0; sh < 32; sh += 8) {
            float i1 = ((a >> sh) & 255) / 255.0f, i2 = ((b >> sh) & 255) / 255.0f;
            float r = clamp01(k[0] * i1 * i2 + k[1] * i1 + k[2] * i2 + k[3]);
            out |= (u32)(r * 255 + 0.5f) << sh;
        }
        /* cores não passam do alfa */
        u32 al = out >> 24;
        for (int sh = 0; sh < 24; sh += 8) if (((out >> sh) & 255) > al) out = (out & ~(0xFFu << sh)) | al << sh;
        return out;
    }
    return over(b, a);
}

static u32 fe_blend_px(const char *mode, u32 a, u32 b) {
    if (!mode || !strcmp(mode, "normal")) return over(b, a);
    float ar, ag, ab_, aa, br, bg, bb2, ba;
    unpre(a, &ar, &ag, &ab_, &aa); unpre(b, &br, &bg, &bb2, &ba);
    float s[3] = { ar, ag, ab_ }, d[3] = { br, bg, bb2 }, r[3];
    for (int i = 0; i < 3; i++) {
        float x = s[i], y = d[i], f;
        if (!strcmp(mode, "multiply")) f = x * y;
        else if (!strcmp(mode, "screen")) f = x + y - x * y;
        else if (!strcmp(mode, "darken")) f = MIN(x, y);
        else if (!strcmp(mode, "lighten")) f = MAX(x, y);
        else if (!strcmp(mode, "difference")) f = fabs_(x - y);
        else if (!strcmp(mode, "exclusion")) f = x + y - 2 * x * y;
        else if (!strcmp(mode, "overlay")) f = y <= 0.5f ? 2 * x * y : 1 - 2 * (1 - x) * (1 - y);
        else f = x;
        r[i] = aa * (1 - ba) * x + ba * (1 - aa) * y + aa * ba * f;
    }
    float ra = aa + ba - aa * ba;
    return ra > 0 ? repre(r[0] / ra, r[1] / ra, r[2] / ra, ra) : 0;
}

typedef struct { char name[32]; u32 *img; } fres_t;

static u32 *fe_input(ctx_t *C, const char *in, u32 *src, u32 *last, fres_t *res, int nres) {
    u32 *o = fimg_new(C);
    if (!o) return NULL;
    usize n = (usize)C->W * C->H;
    if (!in || !*in) memcpy(o, last ? last : src, n * 4);
    else if (!strcmp(in, "SourceGraphic")) memcpy(o, src, n * 4);
    else if (!strcmp(in, "SourceAlpha")) for (usize i = 0; i < n; i++) o[i] = src[i] & 0xFF000000u;
    else {
        bool found = false;
        for (int i = nres - 1; i >= 0; i--) if (!strcmp(res[i].name, in)) { memcpy(o, res[i].img, n * 4); found = true; break; }
        if (!found) memcpy(o, last ? last : src, n * 4);
    }
    return o;
}

/* aplica <filter> à camada (W×H) do elemento; devolve a nova camada (ou a mesma) */
static void apply_filter(ctx_t *C, surface_t *ls, const char *id, const sst_t *st, const mat_t *ctm, const float *bb) {
    xel_t *f = by_id(C, id);
    if (!f || strcmp(f->name, "filter")) return;
    int W = C->W, H = C->H;
    usize n = (usize)W * H;
    u32 *src = fimg_new(C);
    if (!src) return;
    for (int y = 0; y < H; y++) memcpy(src + (usize)y * W, ls->px + (usize)y * ls->stride, (usize)W * 4);
    float reg[4];
    fx_region(C, f, "filterUnits", bb, reg);
    u8 *cov = rect_cov(C, reg, ctm);
    const char *pu = gattr(C, f, "primitiveUnits");
    bool pobb = pu && !strcmp(pu, "objectBoundingBox");
    float ux = sqrtf_(ctm->a * ctm->a + ctm->b * ctm->b), uy = sqrtf_(ctm->c * ctm->c + ctm->d * ctm->d);
    fres_t res[16];
    int nres = 0;
    u32 *last = NULL;
    for (xel_t *e = f->first; e; e = e->next) {
        if (!e->name || strncmp(e->name, "fe", 2)) continue;
        const char *nm = e->name;
        u32 *img = NULL;
        sst_t es;
        compute_style(C, e, st, &es);
        float scx = pobb ? bb[2] * ux : ux, scy = pobb ? bb[3] * uy : uy;
        if (!strcmp(nm, "feFlood")) {
            img = fimg_new(C);
            if (img) { u32 c = premul(es.flood_color, es.flood_op); for (usize i = 0; i < n; i++) img[i] = c; }
        } else if (!strcmp(nm, "feMerge")) {
            img = fimg_new(C);
            for (xel_t *mn = e->first; img && mn; mn = mn->next) {
                if (!mn->name || strcmp(mn->name, "feMergeNode")) continue;
                u32 *in = fe_input(C, xattr(mn, "in"), src, last, res, nres);
                if (in) { for (usize i = 0; i < n; i++) img[i] = over(img[i], in[i]); kfree(in); }
            }
        } else if (!strcmp(nm, "feComposite") || !strcmp(nm, "feBlend")) {
            u32 *a = fe_input(C, xattr(e, "in"), src, last, res, nres), *b = fe_input(C, xattr(e, "in2"), src, last, res, nres);
            if (a && b) {
                float k[4] = { frac_len(xattr(e, "k1"), 0), frac_len(xattr(e, "k2"), 0), frac_len(xattr(e, "k3"), 0), frac_len(xattr(e, "k4"), 0) };
                bool comp = !strcmp(nm, "feComposite");
                const char *op = xattr(e, comp ? "operator" : "mode");
                for (usize i = 0; i < n; i++) a[i] = comp ? fe_comp_px(op, a[i], b[i], k) : fe_blend_px(op, a[i], b[i]);
                img = a; a = NULL;
            }
            kfree(a); kfree(b);
        } else {
            img = fe_input(C, xattr(e, "in"), src, last, res, nres);
            if (!img) break;
            if (!strcmp(nm, "feGaussianBlur")) {
                float sx = 0, sy;
                const char *q = xattr(e, "stdDeviation");
                if (q) parse_f(&q, &sx);
                sy = sx;
                if (q) parse_f(&q, &sy);
                fe_blur(C, img, sx * scx, sy * scy);
            } else if (!strcmp(nm, "feOffset")) {
                fe_offset(C, img, (int)ffloor(frac_len(xattr(e, "dx"), 0) * scx + 0.5f), (int)ffloor(frac_len(xattr(e, "dy"), 0) * scy + 0.5f));
            } else if (!strcmp(nm, "feColorMatrix")) fe_colormatrix(C, img, e);
            else if (!strcmp(nm, "feComponentTransfer")) fe_transfer(C, img, e);
            else if (!strcmp(nm, "feMorphology")) {
                float rx = 0, ry;
                const char *q = xattr(e, "radius");
                if (q) parse_f(&q, &rx);
                ry = rx;
                if (q) parse_f(&q, &ry);
                const char *op = xattr(e, "operator");
                fe_morph(C, img, (int)(rx * scx + 0.5f), (int)(ry * scy + 0.5f), op && !strcmp(op, "dilate"));
            } else if (!strcmp(nm, "feDropShadow")) {
                u32 *sh = fimg_new(C);
                if (sh) {
                    u32 col = premul(es.flood_color, es.flood_op);
                    for (usize i = 0; i < n; i++) sh[i] = mul_px(col, img[i] >> 24);
                    float sx = 2, sy;
                    const char *q = xattr(e, "stdDeviation");
                    if (q) parse_f(&q, &sx);
                    sy = sx;
                    if (q) parse_f(&q, &sy);
                    fe_blur(C, sh, sx * scx, sy * scy);
                    fe_offset(C, sh, (int)ffloor(frac_len(xattr(e, "dx"), 2) * scx + 0.5f), (int)ffloor(frac_len(xattr(e, "dy"), 2) * scy + 0.5f));
                    for (usize i = 0; i < n; i++) img[i] = over(sh[i], img[i]);
                    kfree(sh);
                }
            }
            /* outros primitivos (turbulência, iluminação, convolução, imagem) passam a entrada adiante */
        }
        if (!img) break;
        if (cov) for (usize i = 0; i < n; i++) if (cov[i] != 255) img[i] = mul_px(img[i], cov[i]);
        const char *rn = xattr(e, "result");
        if (nres < 16) { snprintf(res[nres].name, sizeof res[nres].name, "%s", rn ? rn : ""); res[nres].img = img; nres++; }
        last = img;
    }
    if (last) for (int y = 0; y < H; y++) memcpy(ls->px + (usize)y * ls->stride, last + (usize)y * W, (usize)W * 4);
    for (int i = 0; i < nres; i++) kfree(res[i].img);
    kfree(src);
    kfree(cov);
}

/* ---------------------------------------------------------------- marcadores */
typedef struct { float x, y, ain, aout; bool has_in, has_out; } vert_t;

static int path_vertices(ctx_t *C, xel_t *n, const sst_t *st, vert_t *v, int cap) {
    const char *nm = n->name;
    int k = 0;
    float fs = st->font_size;
#define ADDV(X, Y) do { if (k < cap) { v[k].x = (X); v[k].y = (Y); v[k].has_in = v[k].has_out = false; k++; } } while (0)
    if (!strcmp(nm, "line")) {
        ADDV(parse_len(xattr(n, "x1"), C->vw, fs, 0), parse_len(xattr(n, "y1"), C->vh, fs, 0));
        ADDV(parse_len(xattr(n, "x2"), C->vw, fs, 0), parse_len(xattr(n, "y2"), C->vh, fs, 0));
    } else if (!strcmp(nm, "polyline") || !strcmp(nm, "polygon")) {
        const char *s = xattr(n, "points");
        float x, y;
        while (s && parse_f(&s, &x) && parse_f(&s, &y)) ADDV(x, y);
        if (nm[4] == 'g' && k) ADDV(v[0].x, v[0].y);
    } else if (!strcmp(nm, "path")) {
        /* vértices = pontos finais dos comandos, com as tangentes das curvas */
        const char *p = xattr(n, "d");
        char cmd = 0, prev = 0;
        float cx = 0, cy = 0, sx = 0, sy = 0, lcx = 0, lcy = 0;
        int sub0 = 0;
        while (p && *p && k < cap) {
            while (*p && (isspace(*p) || *p == ',')) p++;
            if (!*p) break;
            if (isalpha(*p)) cmd = *p++;
            else if (!cmd) break;
            else if (!num_start(p)) { p++; continue; }
            bool rel = cmd >= 'a';
            char Cc = (char)toupper(cmd);
            float ox = rel ? cx : 0, oy = rel ? cy : 0, a[7];
            int need = Cc == 'M' || Cc == 'L' || Cc == 'T' ? 2 : Cc == 'H' || Cc == 'V' ? 1 : Cc == 'C' ? 6 : Cc == 'S' || Cc == 'Q' ? 4 : Cc == 'A' ? 7 : 0;
            if (Cc == 'Z') {
                float ang = atan2_(sy - cy, sx - cx);
                if (k) { v[k - 1].aout = ang; v[k - 1].has_out = true; }
                ADDV(sx, sy);
                v[k - 1].ain = ang; v[k - 1].has_in = true;
                if (sub0 < k - 1) { v[sub0].ain = ang; v[sub0].has_in = true; }
                cx = sx; cy = sy; cmd = 0; prev = 'Z';
                continue;
            }
            bool ok = true;
            for (int i = 0; i < need && ok; i++) {
                if (Cc == 'A' && (i == 3 || i == 4)) { while (*p && (isspace(*p) || *p == ',')) p++; if (*p == '0' || *p == '1') a[i] = (float)(*p++ - '0'); else ok = false; }
                else ok = parse_f(&p, &a[i]);
            }
            if (!ok) break;
            float ex = cx, ey = cy, c1x = cx, c1y = cy, c2x = cx, c2y = cy;
            switch (Cc) {
            case 'M': ex = ox + a[0]; ey = oy + a[1]; ADDV(ex, ey); sub0 = k - 1; sx = ex; sy = ey; cx = ex; cy = ey; cmd = rel ? 'l' : 'L'; prev = 'M'; continue;
            /* reta: tangente de saída = fim − início, de chegada = fim − início */
            case 'L': ex = ox + a[0]; ey = oy + a[1]; c1x = ex; c1y = ey; c2x = cx; c2y = cy; break;
            case 'H': ex = ox + a[0]; c1x = ex; c1y = ey; c2x = cx; c2y = cy; break;
            case 'V': ey = oy + a[0]; c1x = ex; c1y = ey; c2x = cx; c2y = cy; break;
            case 'C': c1x = ox + a[0]; c1y = oy + a[1]; c2x = ox + a[2]; c2y = oy + a[3]; ex = ox + a[4]; ey = oy + a[5]; break;
            case 'S':
                c1x = (prev == 'C' || prev == 'S') ? 2 * cx - lcx : cx; c1y = (prev == 'C' || prev == 'S') ? 2 * cy - lcy : cy;
                c2x = ox + a[0]; c2y = oy + a[1]; ex = ox + a[2]; ey = oy + a[3]; break;
            case 'Q': c1x = c2x = ox + a[0]; c1y = c2y = oy + a[1]; ex = ox + a[2]; ey = oy + a[3]; break;
            case 'T':
                c1x = c2x = (prev == 'Q' || prev == 'T') ? 2 * cx - lcx : cx; c1y = c2y = (prev == 'Q' || prev == 'T') ? 2 * cy - lcy : cy;
                ex = ox + a[0]; ey = oy + a[1]; break;
            case 'A': {
                ex = ox + a[5]; ey = oy + a[6];
                path_t tp = { 0 };
                tp.tol = 0.01f;
                path_move(&tp, cx, cy);
                path_arc(&tp, a[0], a[1], a[2], (int)a[3], (int)a[4], ex, ey);
                if (tp.n >= 2) { c1x = tp.xy[2]; c1y = tp.xy[3]; c2x = tp.xy[2 * (tp.n - 2)]; c2y = tp.xy[2 * (tp.n - 2) + 1]; }
                path_free(&tp);
                break;
            }
            default: break;
            }
            float tox = c1x - cx, toy = c1y - cy;
            if (fabs_(tox) + fabs_(toy) < 1e-9f) { tox = (c2x != cx || c2y != cy) ? c2x - cx : ex - cx; toy = (c2x != cx || c2y != cy) ? c2y - cy : ey - cy; }
            float tix = ex - c2x, tiy = ey - c2y;
            if (fabs_(tix) + fabs_(tiy) < 1e-9f) { tix = ex - c1x; tiy = ey - c1y; }
            if (k) { v[k - 1].aout = atan2_(toy, tox); v[k - 1].has_out = true; }
            ADDV(ex, ey);
            v[k - 1].ain = atan2_(tiy, tix); v[k - 1].has_in = true;
            lcx = (Cc == 'Q' || Cc == 'T') ? c1x : c2x; lcy = (Cc == 'Q' || Cc == 'T') ? c1y : c2y;
            cx = ex; cy = ey; prev = Cc;
        }
    }
#undef ADDV
    /* polilinhas: ângulos pelos vizinhos */
    if (strcmp(nm, "path"))
        for (int i = 0; i < k; i++) {
            if (i > 0) { v[i].ain = atan2_(v[i].y - v[i - 1].y, v[i].x - v[i - 1].x); v[i].has_in = true; }
            if (i + 1 < k) { v[i].aout = atan2_(v[i + 1].y - v[i].y, v[i + 1].x - v[i].x); v[i].has_out = true; }
        }
    return k;
}

static void draw_marker(ctx_t *C, const char *id, const sst_t *st, const mat_t *ctm, const vert_t *v, int which) {
    xel_t *mk = by_id(C, id);
    if (!mk || strcmp(mk->name, "marker") || C->depth > 8) return;
    float ang;
    const char *orient = xattr(mk, "orient");
    if (orient && (!strcmp(orient, "auto") || !strcmp(orient, "auto-start-reverse"))) {
        if (v->has_in && v->has_out) {
            float a = v->ain, b = v->aout, d = b - a;
            while (d > PI_F) d -= 2 * PI_F;
            while (d < -PI_F) d += 2 * PI_F;
            ang = a + d / 2;
        } else ang = v->has_out ? v->aout : v->ain;
        if (which == 0 && !strcmp(orient, "auto-start-reverse")) ang += PI_F;
    } else { float deg = 0; const char *q = orient; if (q) parse_f(&q, &deg); ang = deg * PI_F / 180; }
    float fs = st->font_size;
    float mw = parse_len(xattr(mk, "markerWidth"), C->vw, fs, 3), mh = parse_len(xattr(mk, "markerHeight"), C->vh, fs, 3);
    if (mw <= 0 || mh <= 0) return;
    const char *mu = xattr(mk, "markerUnits");
    float sc = (!mu || strcmp(mu, "userSpaceOnUse")) ? st->sw : 1;
    bool okv;
    const char *vb = xattr(mk, "viewBox");
    mat_t vm = viewbox_mat(vb, xattr(mk, "preserveAspectRatio"), 0, 0, mw, mh, &okv);
    if (!vb || !okv) vm = MID;
    float rx, ry;
    mapp(&vm, frac_len(xattr(mk, "refX"), 0), frac_len(xattr(mk, "refY"), 0), &rx, &ry);
    mat_t t1 = MID; t1.e = v->x; t1.f = v->y;
    mat_t ro = { fcos(ang), fsin(ang), -fsin(ang), fcos(ang), 0, 0 };
    mat_t s1 = { sc, 0, 0, sc, 0, 0 };
    mat_t t2 = MID; t2.e = -rx; t2.f = -ry;
    mat_t m = mmul(mmul(mmul(mmul(mmul(*ctm, t1), ro), s1), t2), vm);
    sst_t ms;
    compute_style(C, mk, &C->base, &ms);
    /* o marcador não herda os marcadores do caminho */
    ms.mk_start[0] = ms.mk_mid[0] = ms.mk_end[0] = 0;
    C->depth++;
    render_children(C, mk, &ms, m);
    C->depth--;
}

static void draw_markers(ctx_t *C, xel_t *n, const sst_t *st, const mat_t *ctm) {
    if (!st->mk_start[0] && !st->mk_mid[0] && !st->mk_end[0]) return;
    vert_t v[512];
    int k = path_vertices(C, n, st, v, 512);
    for (int i = 0; i < k; i++) {
        const char *id = i == 0 ? st->mk_start : i == k - 1 ? st->mk_end : st->mk_mid;
        if (id[0]) draw_marker(C, id, st, ctm, &v[i], i == 0 ? 0 : i == k - 1 ? 2 : 1);
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
    /* camada isolada: opacidade de grupo, máscara ou filtro (para formas também) */
    bool fx = st.mask[0] || st.filter[0];
    bool layer = (group && st.opacity < 1) || fx;
    float ebb[4] = { 0, 0, 0, 0 };
    if (fx) elem_bbox(C, n, par, ebb);
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
        sst_t ds = st;
        if (st.opacity < 1 && !layer) { ds.fill_op *= st.opacity; ds.stroke_op *= st.opacity; }
        draw_path(C, &p, &ds, &ctm, fillable);
        path_free(&p);
        if (!st.hidden && strcmp(nm, "rect") && strcmp(nm, "circle") && strcmp(nm, "ellipse")) draw_markers(C, n, &st, &ctm);
    }
    C->vw = sv_vw; C->vh = sv_vh;
    if (layer) {
        surface_t *ls = C->s;
        C->s = saved;
        if (st.filter[0]) apply_filter(C, ls, st.filter, &st, &ctm, ebb);
        u8 *mv = st.mask[0] ? mask_values(C, st.mask, &st, &ctm, ebb) : NULL;
        bool masked_out = st.mask[0] && !mv && by_id(C, st.mask);
        u32 op = (u32)(st.opacity * 255 + 0.5f);
        for (int y = 0; y < C->H && !masked_out; y++)
            for (int x = 0; x < C->W; x++) {
                u32 c = ls->px[(usize)y * ls->stride + x];
                if (!c) continue;
                u32 a = op;
                if (mv) a = a * mv[(usize)y * C->W + x] / 255;
                if (a) { u32 *d = &saved->px[(usize)y * saved->stride + x]; *d = over(*d, mul_px(c, a)); }
            }
        kfree(mv);
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
    C.base = rs;
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
