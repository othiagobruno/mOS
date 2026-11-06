/* mOS — Horizonte: CSS (parser, seletores, cascata, estilo computado) */
#include "web.h"

/* ============================================================ propriedades */
enum {
    P_NONE, P_DISPLAY, P_COLOR, P_BACKGROUND, P_BACKGROUND_COLOR, P_BACKGROUND_IMAGE, P_FONT, P_FONT_SIZE, P_FONT_WEIGHT,
    P_FONT_STYLE, P_FONT_FAMILY, P_TEXT_ALIGN, P_TEXT_DECORATION, P_TEXT_TRANSFORM, P_TEXT_INDENT, P_WHITE_SPACE,
    P_LINE_HEIGHT, P_LIST_STYLE, P_LIST_STYLE_TYPE, P_MARGIN, P_MARGIN_TOP, P_MARGIN_RIGHT, P_MARGIN_BOTTOM, P_MARGIN_LEFT,
    P_PADDING, P_PADDING_TOP, P_PADDING_RIGHT, P_PADDING_BOTTOM, P_PADDING_LEFT, P_BORDER, P_BORDER_TOP, P_BORDER_RIGHT,
    P_BORDER_BOTTOM, P_BORDER_LEFT, P_BORDER_WIDTH, P_BORDER_COLOR, P_BORDER_STYLE, P_BORDER_TOP_WIDTH, P_BORDER_RIGHT_WIDTH,
    P_BORDER_BOTTOM_WIDTH, P_BORDER_LEFT_WIDTH, P_BORDER_TOP_COLOR, P_BORDER_RIGHT_COLOR, P_BORDER_BOTTOM_COLOR,
    P_BORDER_LEFT_COLOR, P_BORDER_TOP_STYLE, P_BORDER_RIGHT_STYLE, P_BORDER_BOTTOM_STYLE, P_BORDER_LEFT_STYLE,
    P_WIDTH, P_HEIGHT, P_MIN_WIDTH, P_MAX_WIDTH, P_MIN_HEIGHT, P_MAX_HEIGHT, P_VISIBILITY, P_VERTICAL_ALIGN,
    P_BORDER_RADIUS, P_BOX_SHADOW, P_BORDER_SPACING, P_BORDER_COLLAPSE, P_POSITION, P_LEFT, P_TOP, P_OVERFLOW, P_OPACITY,
    P_FLOAT, P_FLEX_DIRECTION, P_CLIP, P_TEXT_DECORATION_LINE, P_BOX_SIZING, P_CONTENT_VIS,
    P_COUNT
};
static const char *prop_names[P_COUNT] = {
    "", "display", "color", "background", "background-color", "background-image", "font", "font-size", "font-weight",
    "font-style", "font-family", "text-align", "text-decoration", "text-transform", "text-indent", "white-space",
    "line-height", "list-style", "list-style-type", "margin", "margin-top", "margin-right", "margin-bottom", "margin-left",
    "padding", "padding-top", "padding-right", "padding-bottom", "padding-left", "border", "border-top", "border-right",
    "border-bottom", "border-left", "border-width", "border-color", "border-style", "border-top-width", "border-right-width",
    "border-bottom-width", "border-left-width", "border-top-color", "border-right-color", "border-bottom-color",
    "border-left-color", "border-top-style", "border-right-style", "border-bottom-style", "border-left-style",
    "width", "height", "min-width", "max-width", "min-height", "max-height", "visibility", "vertical-align",
    "border-radius", "box-shadow", "border-spacing", "border-collapse", "position", "left", "top", "overflow", "opacity",
    "float", "flex-direction", "clip", "text-decoration-line", "box-sizing", "content-visibility",
};

static int prop_lookup(const char *n) {
    for (int i = 1; i < P_COUNT; i++) if (prop_names[i][0] == n[0] && !strcmp(prop_names[i], n)) return i;
    return 0;
}

/* ============================================================ estruturas */
enum { PS_LINK = 1, PS_FIRST = 2, PS_LAST = 4, PS_ROOT = 8, PS_NEVER = 16, PS_EMPTY = 32, PS_NTH = 64, PS_ONLY = 128,
       PS_CHECKED = 256 };
enum { AOP_EXISTS, AOP_EQ, AOP_WORD, AOP_PREFIX, AOP_SUFFIX, AOP_SUBSTR, AOP_DASH };

typedef struct { const char *name, *val; u8 op; } aselt_t;
typedef struct spart {
    int tag;               /* 0 = qualquer; -1 = nome desconhecido em tagname */
    const char *tagname;
    const char *id;
    const char **cls;
    int ncls;
    aselt_t *attrs;
    int nattrs;
    u16 pseudo;
    short nth_a, nth_b;
    char comb;             /* combinador com a parte anterior */
    struct spart *neg;     /* :not(...) */
} spart_t;

typedef struct { u16 prop; u8 important; const char *val; } decl_t;
typedef struct {
    spart_t *parts;
    int nparts, spec, order;
    decl_t *decls;
    int ndecls;
} rule_t;

typedef struct { u8 type; int tag; const char *name; int rule; int next; } ientry_t;
#define IBUCKETS 1024

struct sheet {
    arena_t a;
    rule_t *rules;
    int n, cap;
    int vw;
    ientry_t *ix;
    int nix, cix;
    int buckets[IBUCKETS];
    bool indexed;
};

/* ============================================================ utilidades de texto */
static const char *skip_ws(const char *p, const char *e) { while (p < e && isspace(*p)) p++; return p; }
static bool is_ident(char c) { return isalnum(c) || c == '-' || c == '_' || (u8)c >= 0x80 || c == '\\'; }

static u32 hash_str(u8 type, const char *s) {
    u32 h = 2166136261u ^ type;
    while (*s) { h ^= (u8)*s++; h *= 16777619u; }
    return h;
}

/* ============================================================ cores */
typedef struct { const char *n; u32 c; } ncolor_t;
static const ncolor_t named_colors[] = {
    {"black", 0x000000}, {"white", 0xFFFFFF}, {"red", 0xFF0000}, {"green", 0x008000}, {"blue", 0x0000FF},
    {"yellow", 0xFFFF00}, {"gray", 0x808080}, {"grey", 0x808080}, {"silver", 0xC0C0C0}, {"maroon", 0x800000},
    {"purple", 0x800080}, {"fuchsia", 0xFF00FF}, {"magenta", 0xFF00FF}, {"lime", 0x00FF00}, {"olive", 0x808000},
    {"navy", 0x000080}, {"teal", 0x008080}, {"aqua", 0x00FFFF}, {"cyan", 0x00FFFF}, {"orange", 0xFFA500},
    {"pink", 0xFFC0CB}, {"brown", 0xA52A2A}, {"gold", 0xFFD700}, {"beige", 0xF5F5DC}, {"ivory", 0xFFFFF0},
    {"khaki", 0xF0E68C}, {"lavender", 0xE6E6FA}, {"lightgray", 0xD3D3D3}, {"lightgrey", 0xD3D3D3},
    {"darkgray", 0xA9A9A9}, {"darkgrey", 0xA9A9A9}, {"dimgray", 0x696969}, {"dimgrey", 0x696969},
    {"gainsboro", 0xDCDCDC}, {"whitesmoke", 0xF5F5F5}, {"lightblue", 0xADD8E6}, {"skyblue", 0x87CEEB},
    {"steelblue", 0x4682B4}, {"royalblue", 0x4169E1}, {"darkblue", 0x00008B}, {"midnightblue", 0x191970},
    {"cornflowerblue", 0x6495ED}, {"dodgerblue", 0x1E90FF}, {"deepskyblue", 0x00BFFF}, {"lightgreen", 0x90EE90},
    {"darkgreen", 0x006400}, {"forestgreen", 0x228B22}, {"seagreen", 0x2E8B57}, {"limegreen", 0x32CD32},
    {"darkred", 0x8B0000}, {"crimson", 0xDC143C}, {"firebrick", 0xB22222}, {"tomato", 0xFF6347}, {"coral", 0xFF7F50},
    {"salmon", 0xFA8072}, {"orangered", 0xFF4500}, {"darkorange", 0xFF8C00}, {"chocolate", 0xD2691E},
    {"tan", 0xD2B48C}, {"wheat", 0xF5DEB3}, {"linen", 0xFAF0E6}, {"snow", 0xFFFAFA}, {"honeydew", 0xF0FFF0},
    {"mintcream", 0xF5FFFA}, {"azure", 0xF0FFFF}, {"aliceblue", 0xF0F8FF}, {"ghostwhite", 0xF8F8FF},
    {"seashell", 0xFFF5EE}, {"oldlace", 0xFDF5E6}, {"floralwhite", 0xFFFAF0}, {"lightyellow", 0xFFFFE0},
    {"lemonchiffon", 0xFFFACD}, {"cornsilk", 0xFFF8DC}, {"violet", 0xEE82EE}, {"indigo", 0x4B0082},
    {"orchid", 0xDA70D6}, {"plum", 0xDDA0DD}, {"slategray", 0x708090}, {"slategrey", 0x708090},
    {"lightslategray", 0x778899}, {"darkslategray", 0x2F4F4F}, {"turquoise", 0x40E0D0}, {"darkcyan", 0x008B8B},
    {"cadetblue", 0x5F9EA0}, {"rebeccapurple", 0x663399}, {"hotpink", 0xFF69B4}, {"deeppink", 0xFF1493},
    {"goldenrod", 0xDAA520}, {"darkgoldenrod", 0xB8860B}, {"sienna", 0xA0522D}, {"peru", 0xCD853F},
    {"mediumblue", 0x0000CD}, {"darkviolet", 0x9400D3}, {"darkmagenta", 0x8B008B}, {"lightpink", 0xFFB6C1},
    {"lightcyan", 0xE0FFFF}, {"palegreen", 0x98FB98}, {"yellowgreen", 0x9ACD32}, {"olivedrab", 0x6B8E23},
    {"darkolivegreen", 0x556B2F}, {"mediumseagreen", 0x3CB371}, {"springgreen", 0x00FF7F}, {"teal", 0x008080},
    {"lightsteelblue", 0xB0C4DE}, {"powderblue", 0xB0E0E6}, {"mistyrose", 0xFFE4E1}, {"antiquewhite", 0xFAEBD7},
    {"papayawhip", 0xFFEFD5}, {"blanchedalmond", 0xFFEBCD}, {"bisque", 0xFFE4C4}, {"moccasin", 0xFFE4B5},
    {"navajowhite", 0xFFDEAD}, {"peachpuff", 0xFFDAB9}, {"burlywood", 0xDEB887}, {"rosybrown", 0xBC8F8F},
    {"indianred", 0xCD5C5C}, {"lightcoral", 0xF08080}, {"darksalmon", 0xE9967A}, {"lightsalmon", 0xFFA07A},
};

static int hexd(int c) {
    c = tolower(c);
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static float parse_num(const char **pp, bool *ok) {
    const char *p = *pp;
    bool neg = false;
    if (*p == '-') { neg = true; p++; } else if (*p == '+') p++;
    float v = 0;
    bool any = false;
    while (isdigit(*p)) { v = v * 10 + (*p - '0'); p++; any = true; }
    if (*p == '.') {
        p++;
        float f = 0.1f;
        while (isdigit(*p)) { v += (*p - '0') * f; f *= 0.1f; p++; any = true; }
    }
    if ((*p == 'e' || *p == 'E') && (isdigit(p[1]) || ((p[1] == '-' || p[1] == '+') && isdigit(p[2])))) {
        p++;
        int es = 1, ev = 0;
        if (*p == '-') { es = -1; p++; } else if (*p == '+') p++;
        while (isdigit(*p)) { ev = ev * 10 + (*p - '0'); p++; }
        for (int i = 0; i < ev; i++) v = es > 0 ? v * 10 : v / 10;
    }
    *ok = any;
    *pp = p;
    return neg ? -v : v;
}

static float hue2rgb(float p, float q, float t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0f / 6) return p + (q - p) * 6 * t;
    if (t < 0.5f) return q;
    if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
    return p;
}

/* interpreta uma cor CSS (s pode conter espaços nas pontas) */
bool css_color(const char *s, u32 *out) {
    while (isspace(*s)) s++;
    char buf[64];
    int n = 0;
    while (s[n] && n < 63) { buf[n] = (char)tolower(s[n]); n++; }
    while (n && isspace(buf[n - 1])) n--;
    buf[n] = 0;
    if (!n) return false;
    if (!strncmp(buf, "light-dark(", 11)) {
        /* páginas são exibidas em aparência clara: usa o primeiro valor */
        char inner[64];
        int k = 0, depth = 0;
        for (const char *p = buf + 11; *p && k < 63; p++) {
            if (*p == '(') depth++;
            if (*p == ')') { if (!depth) break; depth--; }
            if (*p == ',' && !depth) break;
            inner[k++] = *p;
        }
        inner[k] = 0;
        return css_color(inner, out);
    }
    if (buf[0] == '#') {
        int h[8], k = 0;
        for (int i = 1; buf[i] && k < 8; i++) { int v = hexd(buf[i]); if (v < 0) return false; h[k++] = v; }
        if (k == 3) { *out = RGB(h[0] * 17, h[1] * 17, h[2] * 17); return true; }
        if (k == 4) { *out = RGBA(h[0] * 17, h[1] * 17, h[2] * 17, h[3] * 17); return true; }
        if (k == 6) { *out = RGB(h[0] * 16 + h[1], h[2] * 16 + h[3], h[4] * 16 + h[5]); return true; }
        if (k == 8) { *out = RGBA(h[0] * 16 + h[1], h[2] * 16 + h[3], h[4] * 16 + h[5], h[6] * 16 + h[7]); return true; }
        return false;
    }
    if (!strncmp(buf, "rgb", 3) || !strncmp(buf, "hsl", 3)) {
        bool hsl = buf[0] == 'h';
        const char *p = strchr(buf, '(');
        if (!p) return false;
        p++;
        float v[4] = {0, 0, 0, 1};
        int k = 0;
        while (*p && k < 4) {
            while (*p == ' ' || *p == ',' || *p == '/') p++;
            if (*p == ')' || !*p) break;
            bool ok;
            float x = parse_num(&p, &ok);
            if (!ok) return false;
            if (*p == '%') { p++; x = (hsl && k > 0 && k < 3) ? x / 100.0f : (k == 3 ? x / 100.0f : x * 2.55f); }
            else if (hsl && k == 0) { while (isalpha(*p)) p++; }
            v[k++] = x;
        }
        if (k < 3) return false;
        int r, g, b;
        if (hsl) {
            float hh = v[0] / 360.0f, ss = v[1], ll = v[2];
            hh -= ffloor(hh);
            float q = ll < 0.5f ? ll * (1 + ss) : ll + ss - ll * ss, pp2 = 2 * ll - q;
            r = (int)(hue2rgb(pp2, q, hh + 1.0f / 3) * 255 + 0.5f);
            g = (int)(hue2rgb(pp2, q, hh) * 255 + 0.5f);
            b = (int)(hue2rgb(pp2, q, hh - 1.0f / 3) * 255 + 0.5f);
        } else { r = (int)(v[0] + 0.5f); g = (int)(v[1] + 0.5f); b = (int)(v[2] + 0.5f); }
        int a = (int)(v[3] * 255 + 0.5f);
        *out = RGBA(CLAMP(r, 0, 255), CLAMP(g, 0, 255), CLAMP(b, 0, 255), CLAMP(a, 0, 255));
        return true;
    }
    if (!strcmp(buf, "transparent")) { *out = 0; return true; }
    for (u32 i = 0; i < ARRAY_LEN(named_colors); i++)
        if (!strcmp(named_colors[i].n, buf)) { *out = 0xFF000000u | named_colors[i].c; return true; }
    /* cores de sistema comuns */
    if (!strcmp(buf, "canvas") || !strcmp(buf, "window")) { *out = RGB(255, 255, 255); return true; }
    if (!strcmp(buf, "canvastext") || !strcmp(buf, "windowtext")) { *out = RGB(0, 0, 0); return true; }
    return false;
}

/* ============================================================ parser de folhas */
static void sheet_add(sheet_t *s, rule_t *r) {
    if (s->n == s->cap) {
        int nc = s->cap ? s->cap * 2 : 64;
        rule_t *nr = kmalloc(sizeof(rule_t) * (usize)nc);
        if (!nr) return;
        if (s->rules) { memcpy(nr, s->rules, sizeof(rule_t) * (usize)s->n); kfree(s->rules); }
        s->rules = nr;
        s->cap = nc;
    }
    r->order = s->n;
    s->rules[s->n++] = *r;
    s->indexed = false;
}

static char *dupn(sheet_t *s, const char *p, int n) { return arena_strndup(&s->a, p, n); }

/* lê um identificador (com escapes simples) */
static const char *read_ident(const char *p, const char *e, char *out, int cap) {
    int n = 0;
    while (p < e && is_ident(*p)) {
        char c = *p++;
        if (c == '\\' && p < e) c = *p++;
        if (n < cap - 1) out[n++] = c;
    }
    out[n] = 0;
    return p;
}

static bool parse_compound(sheet_t *s, const char **pp, const char *e, spart_t *sp, int *spec) {
    const char *p = *pp;
    bool any = false;
    char id[128];
    const char *cls[16];
    int ncls = 0;
    aselt_t at[8];
    int nat = 0;
    if (p < e && *p == '*') { p++; any = true; }
    else if (p < e && is_ident(*p) && *p != '-') {
        p = read_ident(p, e, id, sizeof id);
        for (char *q = id; *q; q++) *q = (char)tolower(*q);
        sp->tag = tag_lookup(id);
        if (!sp->tag) { sp->tag = -1; sp->tagname = dupn(s, id, (int)strlen(id)); }
        *spec += 1;
        any = true;
    }
    while (p < e) {
        if (*p == '#') {
            p = read_ident(p + 1, e, id, sizeof id);
            sp->id = dupn(s, id, (int)strlen(id));
            *spec += 10000;
        } else if (*p == '.') {
            p = read_ident(p + 1, e, id, sizeof id);
            if (ncls < 16) cls[ncls++] = dupn(s, id, (int)strlen(id));
            *spec += 100;
        } else if (*p == '[') {
            p++;
            p = skip_ws(p, e);
            p = read_ident(p, e, id, sizeof id);
            for (char *q = id; *q; q++) *q = (char)tolower(*q);
            aselt_t a = {dupn(s, id, (int)strlen(id)), NULL, AOP_EXISTS};
            p = skip_ws(p, e);
            if (p < e && *p != ']') {
                char op = *p;
                if (op == '=') { a.op = AOP_EQ; p++; }
                else if (p + 1 < e && p[1] == '=') {
                    a.op = op == '~' ? AOP_WORD : op == '^' ? AOP_PREFIX : op == '$' ? AOP_SUFFIX : op == '*' ? AOP_SUBSTR : AOP_DASH;
                    p += 2;
                } else return false;
                p = skip_ws(p, e);
                const char *vs = p, *ve;
                if (p < e && (*p == '"' || *p == '\'')) { char q = *p++; vs = p; while (p < e && *p != q) p++; ve = p; if (p < e) p++; }
                else { while (p < e && *p != ']' && !isspace(*p)) p++; ve = p; }
                a.val = dupn(s, vs, (int)(ve - vs));
                p = skip_ws(p, e);
                while (p < e && *p != ']') p++; /* flags " i" */
            }
            if (p < e && *p == ']') p++;
            if (nat < 8) at[nat++] = a;
            *spec += 100;
        } else if (*p == ':') {
            p++;
            bool elem = false;
            if (p < e && *p == ':') { p++; elem = true; }
            p = read_ident(p, e, id, sizeof id);
            for (char *q = id; *q; q++) *q = (char)tolower(*q);
            const char *args = NULL, *args_e = NULL;
            if (p < e && *p == '(') {
                int depth = 1;
                args = ++p;
                while (p < e && depth) { if (*p == '(') depth++; else if (*p == ')') depth--; if (depth) p++; }
                args_e = p;
                if (p < e) p++;
            }
            *spec += elem ? 1 : 100;
            if (elem) sp->pseudo |= PS_NEVER;
            else if (!strcmp(id, "link") || !strcmp(id, "any-link") || !strcmp(id, "visited")) sp->pseudo |= PS_LINK;
            else if (!strcmp(id, "first-child")) sp->pseudo |= PS_FIRST;
            else if (!strcmp(id, "last-child")) sp->pseudo |= PS_LAST;
            else if (!strcmp(id, "only-child")) sp->pseudo |= PS_ONLY;
            else if (!strcmp(id, "root")) sp->pseudo |= PS_ROOT;
            else if (!strcmp(id, "empty")) sp->pseudo |= PS_EMPTY;
            else if (!strcmp(id, "checked")) sp->pseudo |= PS_CHECKED;
            else if (!strcmp(id, "first-of-type")) sp->pseudo |= PS_FIRST;
            else if (!strcmp(id, "last-of-type")) sp->pseudo |= PS_LAST;
            else if (!strcmp(id, "nth-child") && args) {
                char a[24];
                int k = 0;
                for (const char *q = args; q < args_e && k < 23; q++) if (!isspace(*q)) a[k++] = (char)tolower(*q);
                a[k] = 0;
                sp->pseudo |= PS_NTH;
                if (!strcmp(a, "odd")) { sp->nth_a = 2; sp->nth_b = 1; }
                else if (!strcmp(a, "even")) { sp->nth_a = 2; sp->nth_b = 0; }
                else {
                    char *nn = strchr(a, 'n');
                    if (nn) {
                        *nn = 0;
                        sp->nth_a = (short)(a[0] == 0 || !strcmp(a, "+") ? 1 : !strcmp(a, "-") ? -1 : atoi(a));
                        sp->nth_b = (short)atoi(nn + 1);
                    } else { sp->nth_a = 0; sp->nth_b = (short)atoi(a); }
                }
            } else if (!strcmp(id, "not") && args) {
                spart_t *neg = arena_alloc(&s->a, sizeof(spart_t));
                const char *q = skip_ws(args, args_e);
                int dummy = 0;
                if (parse_compound(s, &q, args_e, neg, &dummy) && skip_ws(q, args_e) == args_e) sp->neg = neg;
                else sp->pseudo |= PS_NEVER;
            } else if (!strcmp(id, "is") || !strcmp(id, "where") || !strcmp(id, "lang") || !strcmp(id, "dir")) {
                sp->pseudo |= PS_NEVER;
            } else sp->pseudo |= PS_NEVER; /* hover, focus, active, before, after... */
        } else break;
        any = true;
    }
    if (ncls) {
        sp->cls = arena_alloc(&s->a, sizeof(char *) * (usize)ncls);
        memcpy(sp->cls, cls, sizeof(char *) * (usize)ncls);
        sp->ncls = ncls;
    }
    if (nat) {
        sp->attrs = arena_alloc(&s->a, sizeof(aselt_t) * (usize)nat);
        memcpy(sp->attrs, at, sizeof(aselt_t) * (usize)nat);
        sp->nattrs = nat;
    }
    *pp = p;
    return any;
}

static bool parse_selector(sheet_t *s, const char *p, const char *e, rule_t *r) {
    spart_t parts[24];
    int n = 0, spec = 0;
    char comb = 0;
    p = skip_ws(p, e);
    while (p < e) {
        if (n == 24) return false;
        memset(&parts[n], 0, sizeof(spart_t));
        if (!parse_compound(s, &p, e, &parts[n], &spec)) return false;
        parts[n].comb = comb;
        n++;
        const char *q = skip_ws(p, e);
        if (q >= e) break;
        if (*q == '>' || *q == '+' || *q == '~') { comb = *q; p = skip_ws(q + 1, e); }
        else if (q > p) { comb = ' '; p = q; }
        else return false;
    }
    if (!n) return false;
    r->parts = arena_alloc(&s->a, sizeof(spart_t) * (usize)n);
    memcpy(r->parts, parts, sizeof(spart_t) * (usize)n);
    r->nparts = n;
    r->spec = spec;
    return true;
}

/* lê declarações "a: b; c: d" para um vetor no arena */
static int parse_decls(arena_t *a, const char *p, const char *e, decl_t **out) {
    decl_t tmp[96];
    int n = 0;
    while (p < e && n < 96) {
        p = skip_ws(p, e);
        const char *ns = p;
        while (p < e && *p != ':' && *p != ';') p++;
        if (p >= e || *p == ';') { p++; continue; }
        const char *ne = p;
        while (ne > ns && isspace(ne[-1])) ne--;
        p++;
        const char *vs = skip_ws(p, e);
        int depth = 0;
        char quote = 0;
        while (p < e) {
            if (quote) { if (*p == quote) quote = 0; }
            else if (*p == '"' || *p == '\'') quote = *p;
            else if (*p == '(') depth++;
            else if (*p == ')') depth--;
            else if (*p == ';' && depth <= 0) break;
            p++;
        }
        const char *ve = p;
        if (p < e) p++;
        while (ve > vs && isspace(ve[-1])) ve--;
        char name[40];
        int nl = 0;
        for (const char *q = ns; q < ne && nl < 39; q++) name[nl++] = (char)tolower(*q);
        name[nl] = 0;
        int prop = prop_lookup(name);
        if (!prop) {
            if (!strcmp(name, "-webkit-box-shadow")) prop = P_BOX_SHADOW;
            else continue;
        }
        bool imp = false;
        const char *bang = NULL;
        for (const char *q = vs; q < ve; q++) if (*q == '!') bang = q;
        if (bang) {
            const char *q = skip_ws(bang + 1, ve);
            if (ve - q >= 9 && !strncasecmp(q, "important", 9)) { imp = true; ve = bang; while (ve > vs && isspace(ve[-1])) ve--; }
        }
        tmp[n].prop = (u16)prop;
        tmp[n].important = imp;
        tmp[n].val = arena_strndup(a, vs, (int)(ve - vs));
        n++;
    }
    if (!n) { *out = NULL; return 0; }
    *out = arena_alloc(a, sizeof(decl_t) * (usize)n);
    memcpy(*out, tmp, sizeof(decl_t) * (usize)n);
    return n;
}

static float mq_len(const char *p) {
    bool ok;
    float v = parse_num(&p, &ok);
    if (!strncmp(p, "em", 2) || !strncmp(p, "rem", 3)) v *= 16;
    return v;
}

/* avalia uma media query simples */
static bool media_ok(const char *p, const char *e, int vw) {
    char q[256];
    int n = 0;
    for (; p < e && n < 255; p++) q[n++] = (char)tolower(*p);
    q[n] = 0;
    char *save = q;
    bool any_true = false;
    while (save && *save) {
        char *comma = strchr(save, ',');
        if (comma) *comma = 0;
        char *part = save;
        save = comma ? comma + 1 : NULL;
        bool neg = false, res = true;
        while (isspace(*part)) part++;
        if (!strncmp(part, "not ", 4)) { neg = true; part += 4; }
        if (strstr(part, "print") || strstr(part, "speech") || strstr(part, "tv")) res = false;
        char *f = part;
        while ((f = strchr(f, '(')) != NULL) {
            f++;
            char *cl = strchr(f, ')');
            if (cl) *cl = 0;
            char *colon = strchr(f, ':');
            char feat[48] = {0};
            int k = 0;
            for (char *x = f; *x && x != colon && k < 47; x++) if (!isspace(*x)) feat[k++] = *x;
            const char *val = colon ? colon + 1 : "";
            while (isspace(*val)) val++;
            if (!strcmp(feat, "min-width")) { if (vw < mq_len(val)) res = false; }
            else if (!strcmp(feat, "max-width")) { if (vw > mq_len(val)) res = false; }
            else if (!strcmp(feat, "prefers-color-scheme")) { if (strncmp(val, "light", 5)) res = false; }
            else if (strstr(feat, "resolution") || strstr(feat, "pixel-ratio") || strstr(feat, "reduced-motion") ||
                     strstr(feat, "max-height") || strstr(feat, "forced-colors") || strstr(feat, "contrast")) res = false;
            if (!cl) break;
            f = cl + 1;
        }
        if (neg) res = !res;
        if (res) any_true = true;
    }
    return any_true;
}

static const char *match_brace(const char *p, const char *e) {
    int depth = 0;
    char quote = 0;
    for (; p < e; p++) {
        if (quote) { if (*p == quote) quote = 0; continue; }
        if (*p == '"' || *p == '\'') quote = *p;
        else if (*p == '{') depth++;
        else if (*p == '}') { depth--; if (depth == 0) return p; }
    }
    return e;
}

static void parse_rules(sheet_t *s, const char *p, const char *e) {
    while (p < e) {
        p = skip_ws(p, e);
        if (p >= e) break;
        if (*p == '}' || *p == ';') { p++; continue; }
        if (*p == '<' || (*p == '-' && p + 2 < e && p[1] == '-' && p[2] == '>')) { p++; continue; } /* <!-- --> */
        if (*p == '@') {
            char name[32];
            const char *q = read_ident(p + 1, e, name, sizeof name);
            const char *semi = q, *brace = q;
            while (semi < e && *semi != ';' && *semi != '{') semi++;
            brace = semi;
            if (brace < e && *brace == '{') {
                const char *close = match_brace(brace, e);
                if (!strcasecmp(name, "media") && media_ok(q, brace, s->vw)) parse_rules(s, brace + 1, close);
                else if (!strcasecmp(name, "supports") || !strcasecmp(name, "layer") || !strcasecmp(name, "document")) parse_rules(s, brace + 1, close);
                p = close < e ? close + 1 : e;
            } else p = semi < e ? semi + 1 : e;
            continue;
        }
        const char *brace = p;
        while (brace < e && *brace != '{') brace++;
        if (brace >= e) break;
        const char *close = match_brace(brace, e);
        rule_t base = {0};
        base.ndecls = parse_decls(&s->a, brace + 1, close, &base.decls);
        if (base.ndecls) {
            /* separa a lista de seletores por vírgulas */
            const char *ss = p;
            int depth = 0;
            for (const char *q = p; q <= brace; q++) {
                if (q < brace && (*q == '(' || *q == '[')) depth++;
                else if (q < brace && (*q == ')' || *q == ']')) depth--;
                if (q == brace || (*q == ',' && depth == 0)) {
                    rule_t r = base;
                    if (parse_selector(s, ss, q, &r)) sheet_add(s, &r);
                    ss = q + 1;
                }
            }
        }
        p = close < e ? close + 1 : e;
    }
}

sheet_t *css_parse(const char *src, int len, int vw, sheet_t *s) {
    if (!s) { s = kcalloc(1, sizeof(sheet_t)); if (!s) return NULL; }
    s->vw = vw;
    /* remove comentários */
    char *buf = kmalloc((usize)len + 1);
    if (!buf) return s;
    int n = 0;
    for (int i = 0; i < len; i++) {
        if (src[i] == '/' && i + 1 < len && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < len && !(src[i] == '*' && src[i + 1] == '/')) i++;
            i++;
            buf[n++] = ' ';
            continue;
        }
        buf[n++] = src[i];
    }
    buf[n] = 0;
    parse_rules(s, buf, buf + n);
    kfree(buf);
    return s;
}

void css_free(sheet_t *s) {
    if (!s) return;
    kfree(s->rules);
    kfree(s->ix);
    arena_free(&s->a);
    kfree(s);
}

/* ============================================================ índice de regras */
static u32 bucket_of(u8 type, const char *name, int tag) {
    return (type == 2 ? (u32)tag * 2654435761u : hash_str(type, name)) % IBUCKETS;
}

static void index_add(sheet_t *s, u8 type, const char *name, int rule, int tag) {
    if (s->nix == s->cix) {
        int nc = s->cix ? s->cix * 2 : 256;
        ientry_t *ni = kmalloc(sizeof(ientry_t) * (usize)nc);
        if (!ni) return;
        if (s->ix) { memcpy(ni, s->ix, sizeof(ientry_t) * (usize)s->nix); kfree(s->ix); }
        s->ix = ni;
        s->cix = nc;
    }
    u32 b = bucket_of(type, name, tag);
    ientry_t *en = &s->ix[s->nix];
    en->type = type; en->name = name; en->rule = rule; en->next = -1; en->tag = tag;
    /* mantém a ordem das regras: insere no fim da cadeia */
    if (s->buckets[b] < 0) s->buckets[b] = s->nix;
    else {
        int k = s->buckets[b];
        while (s->ix[k].next >= 0) k = s->ix[k].next;
        s->ix[k].next = s->nix;
    }
    s->nix++;
}

static void build_index(sheet_t *s) {
    s->nix = 0;
    for (int i = 0; i < IBUCKETS; i++) s->buckets[i] = -1;
    for (int i = 0; i < s->n; i++) {
        spart_t *r = &s->rules[i].parts[s->rules[i].nparts - 1];
        if (r->id) index_add(s, 0, r->id, i, 0);
        else if (r->ncls) index_add(s, 1, r->cls[0], i, 0);
        else if (r->tag > 0) index_add(s, 2, "", i, r->tag);
        else index_add(s, 3, "*", i, 0);
    }
    s->indexed = true;
}

/* ============================================================ casamento */
static bool has_class(node_t *n, const char *c) {
    const char *v = node_attr(n, "class");
    if (!v) return false;
    int cl = (int)strlen(c);
    const char *p = v;
    while (*p) {
        while (*p && isspace(*p)) p++;
        const char *s = p;
        while (*p && !isspace(*p)) p++;
        if (p - s == cl && !strncmp(s, c, (usize)cl)) return true;
    }
    return false;
}

static node_t *prev_elem(node_t *n) { for (n = n->prev; n; n = n->prev) if (n->type == N_ELEM) return n; return NULL; }
static node_t *next_elem(node_t *n) { for (n = n->next; n; n = n->next) if (n->type == N_ELEM) return n; return NULL; }

static bool attr_match(node_t *n, aselt_t *a) {
    const char *v = node_attr(n, a->name);
    if (!v) return false;
    if (a->op == AOP_EXISTS) return true;
    int vl = (int)strlen(a->val), l = (int)strlen(v);
    switch (a->op) {
    case AOP_EQ: return !strcasecmp(v, a->val) && (!strcmp(a->name, "type") || !strcmp(v, a->val));
    case AOP_PREFIX: return vl && l >= vl && !strncmp(v, a->val, (usize)vl);
    case AOP_SUFFIX: return vl && l >= vl && !strcmp(v + l - vl, a->val);
    case AOP_SUBSTR: return vl && strstr(v, a->val);
    case AOP_DASH: return !strcmp(v, a->val) || (l > vl && !strncmp(v, a->val, (usize)vl) && v[vl] == '-');
    case AOP_WORD: {
        const char *p = v;
        while (*p) {
            while (*p && isspace(*p)) p++;
            const char *s = p;
            while (*p && !isspace(*p)) p++;
            if (p - s == vl && !strncmp(s, a->val, (usize)vl)) return true;
        }
        return false;
    }
    }
    return false;
}

static bool match_part(spart_t *p, node_t *n) {
    if (n->type != N_ELEM) return false;
    if (p->pseudo & PS_NEVER) return false;
    if (p->tag > 0 && n->tag != p->tag) return false;
    if (p->tag < 0 && (n->tag != T_UNKNOWN || strcmp(n->name, p->tagname))) return false;
    if (p->id) { const char *v = node_attr(n, "id"); if (!v || strcmp(v, p->id)) return false; }
    for (int i = 0; i < p->ncls; i++) if (!has_class(n, p->cls[i])) return false;
    for (int i = 0; i < p->nattrs; i++) if (!attr_match(n, &p->attrs[i])) return false;
    if (p->pseudo) {
        if ((p->pseudo & PS_LINK) && !((n->tag == T_A || n->tag == T_AREA) && node_attr(n, "href"))) return false;
        if ((p->pseudo & PS_FIRST) && prev_elem(n)) return false;
        if ((p->pseudo & PS_LAST) && next_elem(n)) return false;
        if ((p->pseudo & PS_ONLY) && (prev_elem(n) || next_elem(n))) return false;
        if ((p->pseudo & PS_ROOT) && n->tag != T_HTML) return false;
        if ((p->pseudo & PS_EMPTY) && n->first) return false;
        if ((p->pseudo & PS_CHECKED) && !node_attr(n, "checked") && !node_attr(n, "selected")) return false;
        if (p->pseudo & PS_NTH) {
            int idx = 1;
            for (node_t *q = prev_elem(n); q; q = prev_elem(q)) idx++;
            int k = idx - p->nth_b;
            if (p->nth_a == 0) { if (k) return false; }
            else if (k % p->nth_a || k / p->nth_a < 0) return false;
        }
    }
    if (p->neg && match_part(p->neg, n)) return false;
    return true;
}

static bool match_from(rule_t *r, int i, node_t *n) {
    if (!match_part(&r->parts[i], n)) return false;
    if (i == 0) return true;
    char c = r->parts[i].comb;
    if (c == '>') { node_t *p = n->parent; return p && p->type == N_ELEM && match_from(r, i - 1, p); }
    if (c == ' ') {
        for (node_t *p = n->parent; p && p->type == N_ELEM; p = p->parent) if (match_from(r, i - 1, p)) return true;
        return false;
    }
    if (c == '+') { node_t *p = prev_elem(n); return p && match_from(r, i - 1, p); }
    if (c == '~') { for (node_t *p = prev_elem(n); p; p = prev_elem(p)) if (match_from(r, i - 1, p)) return true; return false; }
    return false;
}

/* coleta regras candidatas e testa */
static int collect(sheet_t *s, node_t *n, rule_t **out, int max) {
    if (!s || !s->n) return 0;
    if (!s->indexed) build_index(s);
    int k = 0;
    const char *id = node_attr(n, "id");
    const char *cls = node_attr(n, "class");
    /* percorre cadeias relevantes */
    for (int pass = 0; pass < 4; pass++) {
        if (pass == 0 && !id) continue;
        if (pass == 1 && !cls) continue;
        if (pass == 1) {
            const char *p = cls;
            while (*p) {
                while (*p && isspace(*p)) p++;
                const char *st = p;
                while (*p && !isspace(*p)) p++;
                int l = (int)(p - st);
                if (!l || l > 120) continue;
                char c[128];
                memcpy(c, st, (usize)l);
                c[l] = 0;
                u32 b = bucket_of(1, c, 0);
                for (int e = s->buckets[b]; e >= 0; e = s->ix[e].next) {
                    ientry_t *en = &s->ix[e];
                    if (en->type != 1 || strcmp(en->name, c)) continue;
                    rule_t *r = &s->rules[en->rule];
                    /* evita duplicar se a classe aparece duas vezes */
                    bool dup = false;
                    for (int q = 0; q < k; q++) if (out[q] == r) { dup = true; break; }
                    if (!dup && k < max && match_from(r, r->nparts - 1, n)) out[k++] = r;
                }
            }
            continue;
        }
        if (pass == 2) {
            if (!n->tag) continue;
            u32 b = bucket_of(2, "", n->tag);
            for (int e = s->buckets[b]; e >= 0; e = s->ix[e].next) {
                ientry_t *en = &s->ix[e];
                if (en->type != 2 || en->tag != n->tag) continue;
                rule_t *r = &s->rules[en->rule];
                if (k < max && match_from(r, r->nparts - 1, n)) out[k++] = r;
            }
            continue;
        }
        const char *key = pass == 0 ? id : "*";
        u8 type = pass == 0 ? 0 : 3;
        u32 b = bucket_of(type, key, 0);
        for (int e = s->buckets[b]; e >= 0; e = s->ix[e].next) {
            ientry_t *en = &s->ix[e];
            if (en->type != type || strcmp(en->name, key)) continue;
            rule_t *r = &s->rules[en->rule];
            if (k < max && match_from(r, r->nparts - 1, n)) out[k++] = r;
        }
    }
    /* ordena por especificidade e ordem (inserção; listas curtas) */
    for (int i = 1; i < k; i++) {
        rule_t *r = out[i];
        int j = i - 1;
        while (j >= 0 && (out[j]->spec > r->spec || (out[j]->spec == r->spec && out[j]->order > r->order))) { out[j + 1] = out[j]; j--; }
        out[j + 1] = r;
    }
    return k;
}

/* ============================================================ valores */
typedef struct {
    float fs, rootfs, zoom;
    int vw, vh;
} lctx_t;

#define BC_CURRENT 0x00000001u

static bool parse_len(const char *s, lctx_t *c, len_t *out, bool allow_neg) {
    while (isspace(*s)) s++;
    if (!strncasecmp(s, "auto", 4)) { out->u = U_AUTO; out->v = 0; return true; }
    if (!strncasecmp(s, "none", 4)) { out->u = U_NONE; out->v = 0; return true; }
    if (!strncasecmp(s, "calc(", 5)) {
        /* aproximação: usa o primeiro termo */
        s += 5;
        while (*s == '(' || isspace(*s)) s++;
    }
    const char *p = s;
    bool ok;
    float v = parse_num(&p, &ok);
    if (!ok) return false;
    char u[8];
    int n = 0;
    while ((isalpha(*p) || *p == '%') && n < 7) u[n++] = (char)tolower(*p++);
    u[n] = 0;
    if (!allow_neg && v < 0) return false;
    if (!n) { if (v != 0 && n == 0) { out->u = U_PX; out->v = v * c->zoom; return true; } out->u = U_PX; out->v = 0; return true; }
    if (!strcmp(u, "px")) { out->u = U_PX; out->v = v * c->zoom; }
    else if (!strcmp(u, "%")) { out->u = U_PCT; out->v = v; }
    else if (!strcmp(u, "em")) { out->u = U_PX; out->v = v * c->fs; }
    else if (!strcmp(u, "rem")) { out->u = U_PX; out->v = v * c->rootfs; }
    else if (!strcmp(u, "ex") || !strcmp(u, "ch")) { out->u = U_PX; out->v = v * c->fs * 0.5f; }
    else if (!strcmp(u, "pt")) { out->u = U_PX; out->v = v * 4.0f / 3.0f * c->zoom; }
    else if (!strcmp(u, "pc")) { out->u = U_PX; out->v = v * 16 * c->zoom; }
    else if (!strcmp(u, "in")) { out->u = U_PX; out->v = v * 96 * c->zoom; }
    else if (!strcmp(u, "cm")) { out->u = U_PX; out->v = v * 37.8f * c->zoom; }
    else if (!strcmp(u, "mm")) { out->u = U_PX; out->v = v * 3.78f * c->zoom; }
    else if (!strcmp(u, "vw")) { out->u = U_PX; out->v = v * c->vw / 100.0f; }
    else if (!strcmp(u, "vh")) { out->u = U_PX; out->v = v * c->vh / 100.0f; }
    else if (!strcmp(u, "vmin")) { out->u = U_PX; out->v = v * MIN(c->vw, c->vh) / 100.0f; }
    else if (!strcmp(u, "vmax")) { out->u = U_PX; out->v = v * MAX(c->vw, c->vh) / 100.0f; }
    else return false;
    return true;
}

/* divide um valor em até n tokens separados por espaço (respeitando parênteses) */
static int split_tokens(const char *s, char tok[][64], int max) {
    int n = 0;
    while (*s && n < max) {
        while (isspace(*s) || *s == ',') s++;
        if (!*s) break;
        int k = 0, depth = 0;
        while (*s && (depth || (!isspace(*s) && *s != ',')) && k < 63) {
            if (*s == '(') depth++;
            else if (*s == ')') depth--;
            tok[n][k++] = *s++;
        }
        while (*s && (depth || !isspace(*s))) { if (*s == '(') depth++; else if (*s == ')') depth--; s++; }
        tok[n][k] = 0;
        n++;
    }
    return n;
}

static int weight_of(const char *v, int cur) {
    if (!strcasecmp(v, "bold") || !strcasecmp(v, "bolder")) return FONT_BOLD;
    if (!strcasecmp(v, "normal") || !strcasecmp(v, "lighter")) return FONT_REGULAR;
    if (isdigit(v[0])) {
        int w = atoi(v);
        return w >= 700 ? FONT_BOLD : w >= 600 ? FONT_SEMIBOLD : w >= 500 ? FONT_MEDIUM : FONT_REGULAR;
    }
    return cur;
}

static bool family_mono(const char *v) {
    char b[160];
    int n = 0;
    for (; v[n] && n < 159; n++) b[n] = (char)tolower(v[n]);
    b[n] = 0;
    /* primeira família decide */
    char *comma = strchr(b, ',');
    if (comma) *comma = 0;
    return strstr(b, "mono") || strstr(b, "courier") || strstr(b, "consol") || strstr(b, "menlo") || strstr(b, "monaco") ||
           strstr(b, "code") || strstr(b, "fixed");
}

static bool set_font_size(style_t *st, const char *v, lctx_t *c, float parent_fs) {
    static const struct { const char *k; float px; } kw[] = {
        {"xx-small", 9}, {"x-small", 10}, {"small", 13}, {"medium", 16}, {"large", 18}, {"x-large", 24}, {"xx-large", 32}, {"xxx-large", 48}};
    for (u32 i = 0; i < ARRAY_LEN(kw); i++) if (!strcasecmp(v, kw[i].k)) { st->size = kw[i].px * c->zoom; return true; }
    if (!strcasecmp(v, "smaller")) { st->size = parent_fs / 1.2f; return true; }
    if (!strcasecmp(v, "larger")) { st->size = parent_fs * 1.2f; return true; }
    lctx_t pc = *c;
    pc.fs = parent_fs;
    len_t l;
    if (!parse_len(v, &pc, &l, false)) return false;
    if (l.u == U_PCT) st->size = parent_fs * l.v / 100.0f;
    else if (l.u == U_PX) st->size = l.v;
    else return false;
    if (st->size < 1) st->size = 1;
    if (st->size > 400) st->size = 400;
    return true;
}

static void set_lh(style_t *st, const char *v, lctx_t *c) {
    if (!strcasecmp(v, "normal")) { st->lh_mode = 0; return; }
    const char *p = v;
    bool ok;
    float x = parse_num(&p, &ok);
    if (!ok) return;
    while (isspace(*p)) p++;
    if (!*p) { st->lh_mode = 1; st->lh = x; return; }
    if (*p == '%') { st->lh_mode = 1; st->lh = x / 100.0f; return; }
    len_t l;
    if (parse_len(v, c, &l, false) && l.u == U_PX) { st->lh_mode = 2; st->lh = l.v; }
}

static int display_of(const char *v) {
    static const struct { const char *k; int d; } t[] = {
        {"inline", D_INLINE}, {"block", D_BLOCK}, {"inline-block", D_INLINE_BLOCK}, {"list-item", D_LIST_ITEM},
        {"none", D_NONE}, {"table", D_TABLE}, {"inline-table", D_INLINE_TABLE}, {"table-row-group", D_ROW_GROUP},
        {"table-header-group", D_ROW_GROUP}, {"table-footer-group", D_ROW_GROUP}, {"table-row", D_ROW},
        {"table-cell", D_CELL}, {"table-caption", D_CAPTION}, {"table-column", D_COLUMN}, {"table-column-group", D_COLUMN},
        {"flex", D_BLOCK}, {"grid", D_BLOCK}, {"inline-flex", D_INLINE_BLOCK}, {"inline-grid", D_INLINE_BLOCK},
        {"flow-root", D_BLOCK}, {"contents", D_INLINE}, {"run-in", D_BLOCK},
    };
    for (u32 i = 0; i < ARRAY_LEN(t); i++) if (!strcasecmp(v, t[i].k)) return t[i].d;
    return -1;
}

static int list_of(const char *v) {
    static const struct { const char *k; int d; } t[] = {
        {"disc", LS_DISC}, {"circle", LS_CIRCLE}, {"square", LS_SQUARE}, {"decimal", LS_DECIMAL},
        {"decimal-leading-zero", LS_DECIMAL}, {"lower-alpha", LS_LALPHA}, {"lower-latin", LS_LALPHA},
        {"upper-alpha", LS_UALPHA}, {"upper-latin", LS_UALPHA}, {"lower-roman", LS_LROMAN}, {"upper-roman", LS_UROMAN},
        {"none", LS_NONE},
    };
    for (u32 i = 0; i < ARRAY_LEN(t); i++) if (!strcasecmp(v, t[i].k)) return t[i].d;
    return -1;
}

static bool border_style_none(const char *v) { return !strcasecmp(v, "none") || !strcasecmp(v, "hidden"); }
static bool is_border_style(const char *v) {
    static const char *k[] = {"none", "hidden", "solid", "dotted", "dashed", "double", "groove", "ridge", "inset", "outset"};
    for (u32 i = 0; i < ARRAY_LEN(k); i++) if (!strcasecmp(v, k[i])) return true;
    return false;
}

static float border_width_of(const char *v, lctx_t *c, bool *ok) {
    *ok = true;
    if (!strcasecmp(v, "thin")) return 1 * c->zoom;
    if (!strcasecmp(v, "medium")) return 3 * c->zoom;
    if (!strcasecmp(v, "thick")) return 5 * c->zoom;
    len_t l;
    if (parse_len(v, c, &l, false) && l.u == U_PX) return l.v < 0.5f && l.v > 0 ? 1 : l.v;
    *ok = false;
    return 0;
}

/* aplica "border" ou "border-<lado>" a lados [a..b] */
static void apply_border(style_t *st, const char *v, lctx_t *c, int s0, int s1) {
    char tok[6][64];
    int n = split_tokens(v, tok, 6);
    float w = 3 * c->zoom;
    u32 col = BC_CURRENT;
    bool none = n == 0;
    for (int i = 0; i < n; i++) {
        bool ok;
        float bw = border_width_of(tok[i], c, &ok);
        if (ok) { w = bw; continue; }
        if (is_border_style(tok[i])) { if (border_style_none(tok[i])) none = true; continue; }
        u32 cc;
        if (css_color(tok[i], &cc)) col = cc;
    }
    if (!strcmp(v, "0")) none = true;
    bool has_style = false;
    for (int i = 0; i < n; i++) if (is_border_style(tok[i])) has_style = true;
    if (!has_style) none = true;
    for (int i = s0; i <= s1; i++) { st->bw[i] = none ? 0 : w; st->bc[i] = col; }
}

/* atalho de 1 a 4 valores → 4 lados */
static void four(const char *v, lctx_t *c, len_t out[4], bool allow_neg) {
    char tok[4][64];
    int n = split_tokens(v, tok, 4);
    len_t l[4];
    int k = 0;
    for (int i = 0; i < n; i++) if (parse_len(tok[i], c, &l[k], allow_neg)) k++;
    if (!k) return;
    if (k == 1) { out[0] = out[1] = out[2] = out[3] = l[0]; }
    else if (k == 2) { out[0] = out[2] = l[0]; out[1] = out[3] = l[1]; }
    else if (k == 3) { out[0] = l[0]; out[1] = out[3] = l[1]; out[2] = l[2]; }
    else { out[0] = l[0]; out[1] = l[1]; out[2] = l[2]; out[3] = l[3]; }
}

typedef struct {
    int position;   /* 0 static, 1 relative, 2 absolute, 3 fixed */
    float left, top;
    bool has_left, has_top, overflow_hidden, opacity0, flt, column, flex;
    bool set_align, set_color;
} extra_t;

static void bg_value(style_t *st, const char *v) {
    char tok[8][64];
    int n = split_tokens(v, tok, 8);
    for (int i = 0; i < n; i++) {
        if (!strncasecmp(tok[i], "linear-gradient", 15)) {
            /* usa a primeira e a última cor */
            const char *p = strchr(tok[i], '(');
            u32 first = 0, last = 0;
            int found = 0;
            if (p) {
                char inner[64];
                int depth = 0, k = 0;
                for (p++; *p; p++) {
                    if (*p == '(') depth++;
                    if (*p == ')') { if (!depth) break; depth--; }
                    if (*p == ',' && !depth) {
                        inner[k] = 0;
                        char t2[2][64];
                        if (split_tokens(inner, t2, 2) >= 1) { u32 cc; if (css_color(t2[0], &cc)) { if (!found) first = cc; last = cc; found++; } }
                        k = 0;
                        continue;
                    }
                    if (k < 63) inner[k++] = *p;
                }
                inner[k] = 0;
                char t2[2][64];
                if (split_tokens(inner, t2, 2) >= 1) { u32 cc; if (css_color(t2[0], &cc)) { if (!found) first = cc; last = cc; found++; } }
            }
            if (found >= 2) { st->grad_top = first; st->grad_bot = last; st->bg = first; }
            else if (found == 1) st->bg = first;
            continue;
        }
        if (!strcasecmp(tok[i], "none")) continue;
        u32 c;
        if (css_color(tok[i], &c)) st->bg = c;
    }
    if (!strcasecmp(v, "none") || !strcasecmp(v, "transparent")) { st->bg = 0; st->grad_bot = 0; }
}

static void apply_decl(style_t *st, const style_t *ps, int prop, const char *v, lctx_t *c, extra_t *x) {
    bool inherit = !strcasecmp(v, "inherit");
    bool initial = !strcasecmp(v, "initial") || !strcasecmp(v, "unset") || !strcasecmp(v, "revert");
    len_t l;
    u32 col;
    switch (prop) {
    case P_DISPLAY: { int d = display_of(v); if (d >= 0) st->display = (u8)d; x->flex = !strcasecmp(v, "flex") || !strcasecmp(v, "inline-flex"); break; }
    case P_COLOR:
        if (inherit) st->color = ps->color;
        else if (!strcasecmp(v, "currentcolor")) {}
        else if (css_color(v, &col)) { st->color = col; x->set_color = true; }
        break;
    case P_BACKGROUND: case P_BACKGROUND_IMAGE:
        if (initial) { st->bg = 0; st->grad_bot = 0; }
        else bg_value(st, v);
        break;
    case P_BACKGROUND_COLOR:
        if (css_color(v, &col)) { st->bg = col; st->grad_bot = 0; }
        else if (initial) st->bg = 0;
        break;
    case P_FONT_WEIGHT: st->weight = (u8)(inherit ? ps->weight : weight_of(v, st->weight)); break;
    case P_FONT_STYLE: st->italic = !strcasecmp(v, "italic") || !strcasecmp(v, "oblique"); break;
    case P_FONT_FAMILY: st->mono = family_mono(v); break;
    case P_TEXT_ALIGN:
        x->set_align = true;
        if (!strcasecmp(v, "center") || !strcasecmp(v, "-webkit-center") || !strcasecmp(v, "-moz-center")) st->align = TA_CENTER;
        else if (!strcasecmp(v, "right") || !strcasecmp(v, "end") || !strcasecmp(v, "-webkit-right")) st->align = TA_RIGHT;
        else if (!strcasecmp(v, "justify")) st->align = TA_JUSTIFY;
        else if (inherit) st->align = ps->align;
        else st->align = TA_LEFT;
        st->center_block = !strcasecmp(v, "-webkit-center") || !strcasecmp(v, "-moz-center");
        break;
    case P_TEXT_DECORATION: case P_TEXT_DECORATION_LINE:
        if (strstr(v, "none") || initial) { st->underline = 0; st->strike = 0; }
        if (strstr(v, "underline")) st->underline = 1;
        if (strstr(v, "line-through")) st->strike = 1;
        break;
    case P_TEXT_TRANSFORM:
        st->transform = !strcasecmp(v, "uppercase") ? TT_UPPER : !strcasecmp(v, "lowercase") ? TT_LOWER :
                        !strcasecmp(v, "capitalize") ? TT_CAPITALIZE : TT_NONE;
        break;
    case P_TEXT_INDENT: if (parse_len(v, c, &l, true)) st->indent = l; break;
    case P_WHITE_SPACE:
        if (!strcasecmp(v, "pre")) st->ws = WS_PRE;
        else if (!strcasecmp(v, "nowrap")) st->ws = WS_NOWRAP;
        else if (!strcasecmp(v, "pre-wrap") || !strcasecmp(v, "break-spaces")) st->ws = WS_PRE_WRAP;
        else if (!strcasecmp(v, "pre-line")) st->ws = WS_PRE_LINE;
        else if (inherit) st->ws = ps->ws;
        else st->ws = WS_NORMAL;
        break;
    case P_LINE_HEIGHT: if (inherit) { st->lh = ps->lh; st->lh_mode = ps->lh_mode; } else set_lh(st, v, c); break;
    case P_LIST_STYLE: case P_LIST_STYLE_TYPE: {
        char tok[4][64];
        int n = split_tokens(v, tok, 4);
        for (int i = 0; i < n; i++) { int ls = list_of(tok[i]); if (ls >= 0) st->list = (u8)ls; }
        break;
    }
    case P_MARGIN: if (initial) { for (int i = 0; i < 4; i++) st->margin[i] = (len_t){0, U_PX}; } else four(v, c, st->margin, true); break;
    case P_MARGIN_TOP: case P_MARGIN_RIGHT: case P_MARGIN_BOTTOM: case P_MARGIN_LEFT:
        if (parse_len(v, c, &l, true)) st->margin[prop - P_MARGIN_TOP] = l;
        break;
    case P_PADDING: four(v, c, st->pad, false); break;
    case P_PADDING_TOP: case P_PADDING_RIGHT: case P_PADDING_BOTTOM: case P_PADDING_LEFT:
        if (parse_len(v, c, &l, false)) st->pad[prop - P_PADDING_TOP] = l;
        break;
    case P_BORDER: apply_border(st, v, c, 0, 3); break;
    case P_BORDER_TOP: case P_BORDER_RIGHT: case P_BORDER_BOTTOM: case P_BORDER_LEFT:
        apply_border(st, v, c, prop - P_BORDER_TOP, prop - P_BORDER_TOP);
        break;
    case P_BORDER_WIDTH: {
        char tok[4][64];
        int n = split_tokens(v, tok, 4);
        float w[4];
        int k = 0;
        for (int i = 0; i < n; i++) { bool ok; float bw = border_width_of(tok[i], c, &ok); if (ok) w[k++] = bw; }
        if (k == 1) for (int i = 0; i < 4; i++) st->bw[i] = w[0];
        else if (k == 2) { st->bw[0] = st->bw[2] = w[0]; st->bw[1] = st->bw[3] = w[1]; }
        else if (k == 3) { st->bw[0] = w[0]; st->bw[1] = st->bw[3] = w[1]; st->bw[2] = w[2]; }
        else if (k == 4) for (int i = 0; i < 4; i++) st->bw[i] = w[i];
        break;
    }
    case P_BORDER_COLOR: {
        char tok[4][64];
        int n = split_tokens(v, tok, 4);
        u32 cc[4];
        int k = 0;
        for (int i = 0; i < n; i++) if (css_color(tok[i], &cc[k])) k++;
        if (k == 1) for (int i = 0; i < 4; i++) st->bc[i] = cc[0];
        else if (k == 2) { st->bc[0] = st->bc[2] = cc[0]; st->bc[1] = st->bc[3] = cc[1]; }
        else if (k == 3) { st->bc[0] = cc[0]; st->bc[1] = st->bc[3] = cc[1]; st->bc[2] = cc[2]; }
        else if (k == 4) for (int i = 0; i < 4; i++) st->bc[i] = cc[i];
        break;
    }
    case P_BORDER_STYLE: {
        char tok[4][64];
        int n = split_tokens(v, tok, 4);
        if (n == 1 && border_style_none(tok[0])) for (int i = 0; i < 4; i++) st->bw[i] = 0;
        else if (n == 1) for (int i = 0; i < 4; i++) { if (st->bw[i] == 0) st->bw[i] = 3 * c->zoom; }
        break;
    }
    case P_BORDER_TOP_WIDTH: case P_BORDER_RIGHT_WIDTH: case P_BORDER_BOTTOM_WIDTH: case P_BORDER_LEFT_WIDTH: {
        bool ok;
        float bw = border_width_of(v, c, &ok);
        if (ok) st->bw[prop - P_BORDER_TOP_WIDTH] = bw;
        break;
    }
    case P_BORDER_TOP_COLOR: case P_BORDER_RIGHT_COLOR: case P_BORDER_BOTTOM_COLOR: case P_BORDER_LEFT_COLOR:
        if (css_color(v, &col)) st->bc[prop - P_BORDER_TOP_COLOR] = col;
        break;
    case P_BORDER_TOP_STYLE: case P_BORDER_RIGHT_STYLE: case P_BORDER_BOTTOM_STYLE: case P_BORDER_LEFT_STYLE:
        if (border_style_none(v)) st->bw[prop - P_BORDER_TOP_STYLE] = 0;
        else if (st->bw[prop - P_BORDER_TOP_STYLE] == 0) st->bw[prop - P_BORDER_TOP_STYLE] = 3 * c->zoom;
        break;
    case P_WIDTH: if (parse_len(v, c, &l, false)) st->width = l; break;
    case P_HEIGHT: if (parse_len(v, c, &l, false)) st->height = l; break;
    case P_MIN_WIDTH: if (parse_len(v, c, &l, false)) st->min_w = l; break;
    case P_MAX_WIDTH: if (parse_len(v, c, &l, false)) st->max_w = l; break;
    case P_MIN_HEIGHT: if (parse_len(v, c, &l, false)) st->min_h = l; break;
    case P_MAX_HEIGHT: break;
    case P_VISIBILITY: st->hidden = !strcasecmp(v, "hidden") || !strcasecmp(v, "collapse"); break;
    case P_VERTICAL_ALIGN:
        st->valign = !strcasecmp(v, "middle") ? VA_MIDDLE : !strcasecmp(v, "top") ? VA_TOP : !strcasecmp(v, "bottom") ? VA_BOTTOM :
                     !strcasecmp(v, "sub") ? VA_SUB : !strcasecmp(v, "super") ? VA_SUPER : !strcasecmp(v, "text-top") ? VA_TEXT_TOP :
                     !strcasecmp(v, "text-bottom") ? VA_TEXT_BOTTOM : VA_BASELINE;
        break;
    case P_BORDER_RADIUS: {
        char tok[4][64];
        if (split_tokens(v, tok, 4) >= 1 && parse_len(tok[0], c, &l, false)) st->radius = l.u == U_PCT ? (l.v >= 50 ? 9999 : l.v * 0.4f) : l.v;
        break;
    }
    case P_BOX_SHADOW: st->shadow = strcasecmp(v, "none") != 0 && !strstr(v, "inset"); break;
    case P_BORDER_SPACING: if (parse_len(v, c, &l, false) && l.u == U_PX) st->spacing = l.v; break;
    case P_BORDER_COLLAPSE: st->collapse = !strcasecmp(v, "collapse"); break;
    case P_POSITION:
        x->position = !strcasecmp(v, "relative") ? 1 : !strcasecmp(v, "absolute") ? 2 : !strcasecmp(v, "fixed") ? 3 :
                      !strcasecmp(v, "sticky") ? 1 : 0;
        break;
    case P_LEFT: if (parse_len(v, c, &l, true) && l.u == U_PX) { x->left = l.v / c->zoom; x->has_left = true; } break;
    case P_TOP: if (parse_len(v, c, &l, true) && l.u == U_PX) { x->top = l.v / c->zoom; x->has_top = true; } break;
    case P_OVERFLOW: x->overflow_hidden = !strcasecmp(v, "hidden") || !strcasecmp(v, "clip"); break;
    case P_OPACITY: { const char *p = v; bool ok; float o = parse_num(&p, &ok); x->opacity0 = ok && o < 0.05f; break; }
    case P_FLOAT: x->flt = !strcasecmp(v, "left") || !strcasecmp(v, "right"); break;
    case P_FLEX_DIRECTION: x->column = !strncasecmp(v, "column", 6); break;
    case P_CLIP: break;
    case P_BOX_SIZING: st->border_box = !strcasecmp(v, "border-box"); break;
    case P_CONTENT_VIS: break;
    }
}

static void apply_font(style_t *st, const style_t *ps, int prop, const char *v, lctx_t *c) {
    if (prop == P_FONT_SIZE) {
        if (!strcasecmp(v, "inherit")) st->size = ps->size;
        else set_font_size(st, v, c, ps->size);
        return;
    }
    /* font: [style] [variant] [weight] size[/lh] family */
    char tok[12][64];
    int n = split_tokens(v, tok, 12);
    if (n == 1 && (!strcasecmp(tok[0], "inherit") || !strcasecmp(tok[0], "caption") || !strcasecmp(tok[0], "menu"))) return;
    st->weight = FONT_REGULAR;
    st->italic = 0;
    for (int i = 0; i < n; i++) {
        char *t = tok[i];
        if (!strcasecmp(t, "italic") || !strcasecmp(t, "oblique")) { st->italic = 1; continue; }
        if (!strcasecmp(t, "bold") || !strcasecmp(t, "bolder") || (isdigit(t[0]) && strlen(t) == 3 && t[1] == '0' && t[2] == '0')) {
            st->weight = (u8)weight_of(t, st->weight);
            continue;
        }
        if (!strcasecmp(t, "normal") || !strcasecmp(t, "small-caps")) continue;
        char *slash = strchr(t, '/');
        if (slash) *slash = 0;
        if (set_font_size(st, t, c, ps->size)) {
            lctx_t c2 = *c;
            c2.fs = st->size;
            if (slash) set_lh(st, slash + 1, &c2);
            else if (i + 1 < n && tok[i + 1][0] == '/') set_lh(st, tok[i + 1] + 1, &c2), i++;
            /* o resto é a família */
            sbuf_t fam = {0};
            for (int k = i + 1; k < n; k++) { sb_puts(&fam, tok[k]); sb_putc(&fam, ','); }
            st->mono = fam.p ? family_mono(fam.p) : false;
            sb_free(&fam);
            break;
        }
    }
}

/* ============================================================ folha do agente de usuário */
static const char ua_css[] =
    "html,address,blockquote,body,dd,div,dl,dt,fieldset,form,frame,frameset,h1,h2,h3,h4,h5,h6,noframes,ol,p,ul,center,"
    "dir,hr,menu,pre,article,aside,footer,header,main,nav,section,figure,figcaption,details,summary,hgroup,legend,"
    "listing,xmp,plaintext,dialog,search,optgroup{display:block}"
    "li{display:list-item}"
    "head,script,style,title,meta,link,base,template,noembed,noframes,iframe,svg,math,object,embed,video,audio,canvas,"
    "param,source,track,datalist,area,map,frameset,frame,option,col,colgroup,[hidden],input[type=hidden]{display:none}"
    "table{display:table;border-spacing:2px;border-collapse:separate;text-align:start}"
    "tr{display:table-row;vertical-align:inherit}thead,tbody,tfoot{display:table-row-group;vertical-align:middle}"
    "td,th{display:table-cell;padding:1px;vertical-align:inherit}th{font-weight:bold;text-align:center}"
    "caption{display:table-caption;text-align:center}"
    "body{margin:8px;line-height:normal}"
    "h1{font-size:2em;margin:.67em 0;font-weight:bold}h2{font-size:1.5em;margin:.83em 0;font-weight:bold}"
    "h3{font-size:1.17em;margin:1em 0;font-weight:bold}h4{margin:1.33em 0;font-weight:bold}"
    "h5{font-size:.83em;margin:1.67em 0;font-weight:bold}h6{font-size:.67em;margin:2.33em 0;font-weight:bold}"
    "p,dl,ul,ol,menu,dir{margin:1em 0}blockquote,figure{margin:1em 40px}"
    "ul,ol,menu,dir{padding-left:40px}ul,menu,dir{list-style-type:disc}ol{list-style-type:decimal}"
    "ul ul,ol ul,menu ul{list-style-type:circle}ul ul ul,ol ul ul,ol ol ul,ul ol ul{list-style-type:square}"
    "ul ul,ul ol,ol ul,ol ol,ul menu,ol menu{margin-top:0;margin-bottom:0}"
    "dd{margin-left:40px}"
    "b,strong{font-weight:bold}i,em,cite,var,dfn,address{font-style:italic}"
    "pre,code,kbd,samp,tt,xmp,listing,plaintext{font-family:monospace}pre,xmp,listing,plaintext{white-space:pre;margin:1em 0}"
    "code,kbd,samp,tt{font-size:.9em}pre{font-size:.9em}"
    "big{font-size:larger}small,sub,sup{font-size:smaller}sub{vertical-align:sub}sup{vertical-align:super}"
    "u,ins{text-decoration:underline}s,strike,del{text-decoration:line-through}"
    "a:link{color:#0a64d6;text-decoration:underline}"
    "center{text-align:-webkit-center}"
    "hr{border:1px solid #cfcfcf;margin:.5em auto;height:0}"
    "mark{background:#ffef7a;color:black}"
    "button,input,select,textarea{display:inline-block}"
    "nobr{white-space:nowrap}"
    "fieldset{margin:0 2px;padding:.35em .75em .625em;border:2px groove #c0c0c0}legend{padding:0 2px}"
    "summary{display:block;font-weight:bold}"
    "abbr[title]{text-decoration:none}"
    "img{vertical-align:baseline}";

static sheet_t *ua_sheet;
sheet_t *css_ua_sheet(void) {
    if (!ua_sheet) ua_sheet = css_parse(ua_css, (int)sizeof ua_css - 1, 1000, NULL);
    return ua_sheet;
}

/* ============================================================ cálculo do estilo */
static const float font_sizes[8] = {0, 10, 13, 16, 18, 24, 32, 48};

typedef struct {
    sheet_t *ua, *author;
    arena_t *a;
    lctx_t c;
    rule_t **match;
    int maxmatch;
    u32 link_color;
    bool has_link_color;
    arena_t scratch;       /* declarações de style="" (descartadas ao fim) */
} sctx_t;

static bool attr_len(const char *v, len_t *out, float zoom) {
    if (!v || !*v) return false;
    const char *p = v;
    bool ok;
    float x = parse_num(&p, &ok);
    if (!ok || x < 0) return false;
    if (*p == '%') { out->u = U_PCT; out->v = x; }
    else { out->u = U_PX; out->v = x * zoom; }
    return true;
}

static void set_align_attr(style_t *st, const char *v, extra_t *x) {
    if (!v) return;
    if (!strcasecmp(v, "center") || !strcasecmp(v, "middle")) { st->align = TA_CENTER; st->center_block = 1; }
    else if (!strcasecmp(v, "right")) st->align = TA_RIGHT;
    else if (!strcasecmp(v, "left")) st->align = TA_LEFT;
    else if (!strcasecmp(v, "justify")) st->align = TA_JUSTIFY;
    (void)x;
}

static node_t *ancestor_table(node_t *n) {
    for (node_t *p = n->parent; p && p->type == N_ELEM; p = p->parent) if (p->tag == T_TABLE) return p;
    return NULL;
}

/* atributos de apresentação (abaixo das regras do autor) */
static void presentational(sctx_t *S, node_t *n, style_t *st, const style_t *ps, bool font_pass, extra_t *x) {
    float z = S->c.zoom;
    const char *v;
    u32 col;
    if (font_pass) {
        if (n->tag == T_FONT && (v = node_attr(n, "size"))) {
            int k = atoi(v);
            if (v[0] == '+' || v[0] == '-') k = 3 + k;
            k = CLAMP(k, 1, 7);
            st->size = font_sizes[k] * z;
        }
        return;
    }
    switch (n->tag) {
    case T_BODY:
        if ((v = node_attr(n, "bgcolor")) && css_color(v, &col)) st->bg = col;
        if ((v = node_attr(n, "text")) && css_color(v, &col)) st->color = col;
        if ((v = node_attr(n, "link")) && css_color(v, &col)) { S->link_color = col; S->has_link_color = true; }
        if ((v = node_attr(n, "marginwidth")) || (v = node_attr(n, "leftmargin"))) { len_t l; if (attr_len(v, &l, z)) st->margin[1] = st->margin[3] = l; }
        if ((v = node_attr(n, "marginheight")) || (v = node_attr(n, "topmargin"))) { len_t l; if (attr_len(v, &l, z)) st->margin[0] = st->margin[2] = l; }
        break;
    case T_A:
        if (S->has_link_color && node_attr(n, "href")) st->color = S->link_color;
        break;
    case T_FONT:
        if ((v = node_attr(n, "color")) && css_color(v, &col)) st->color = col;
        if ((v = node_attr(n, "face"))) st->mono = family_mono(v);
        break;
    case T_TABLE: {
        if ((v = node_attr(n, "bgcolor")) && css_color(v, &col)) st->bg = col;
        len_t l;
        if (attr_len(node_attr(n, "width"), &l, z)) st->width = l;
        if (attr_len(node_attr(n, "height"), &l, z)) st->height = l;
        st->cellpad = -1;
        st->border_attr = 0;
        if ((v = node_attr(n, "cellspacing"))) st->spacing = (float)atoi(v) * z;
        if ((v = node_attr(n, "cellpadding"))) st->cellpad = (float)atoi(v) * z;
        if ((v = node_attr(n, "border"))) {
            int b = *v ? atoi(v) : 1;
            if (b > 0) {
                for (int i = 0; i < 4; i++) { st->bw[i] = (float)b * z; st->bc[i] = RGB(128, 128, 128); }
                st->border_attr = 1;
            }
        }
        if ((v = node_attr(n, "align")) && !strcasecmp(v, "center")) { st->margin[1].u = st->margin[3].u = U_AUTO; }
        break;
    }
    case T_TR: case T_TBODY: case T_THEAD: case T_TFOOT:
        if ((v = node_attr(n, "bgcolor")) && css_color(v, &col)) st->bg = col;
        set_align_attr(st, node_attr(n, "align"), x);
        if ((v = node_attr(n, "valign"))) st->valign = !strcasecmp(v, "top") ? VA_TOP : !strcasecmp(v, "bottom") ? VA_BOTTOM : VA_MIDDLE;
        break;
    case T_TD: case T_TH: {
        if ((v = node_attr(n, "bgcolor")) && css_color(v, &col)) st->bg = col;
        len_t l;
        if (attr_len(node_attr(n, "width"), &l, z)) st->width = l;
        if (attr_len(node_attr(n, "height"), &l, z)) st->height = l;
        set_align_attr(st, node_attr(n, "align"), x);
        if ((v = node_attr(n, "valign"))) st->valign = !strcasecmp(v, "top") ? VA_TOP : !strcasecmp(v, "bottom") ? VA_BOTTOM : VA_MIDDLE;
        if (node_attr(n, "nowrap")) st->ws = WS_NOWRAP;
        node_t *t = ancestor_table(n);
        if (t && t->st) {
            if (t->st->cellpad >= 0) for (int i = 0; i < 4; i++) st->pad[i] = (len_t){t->st->cellpad, U_PX};
            if (t->st->border_attr) for (int i = 0; i < 4; i++) { st->bw[i] = 1 * z; st->bc[i] = RGB(160, 160, 160); }
        }
        break;
    }
    case T_IMG: case T_INPUT: case T_IFRAME: case T_CANVAS: case T_EMBED: case T_OBJECT: case T_VIDEO: {
        len_t l;
        if (attr_len(node_attr(n, "width"), &l, z)) st->width = l;
        if (attr_len(node_attr(n, "height"), &l, z)) st->height = l;
        if (n->tag == T_IMG) {
            if ((v = node_attr(n, "border"))) { float b = (float)atoi(v) * z; for (int i = 0; i < 4; i++) { st->bw[i] = b; st->bc[i] = BC_CURRENT; } }
            if ((v = node_attr(n, "hspace"))) { float h = (float)atoi(v) * z; st->margin[1] = st->margin[3] = (len_t){h, U_PX}; }
            if ((v = node_attr(n, "vspace"))) { float h = (float)atoi(v) * z; st->margin[0] = st->margin[2] = (len_t){h, U_PX}; }
            if ((v = node_attr(n, "align"))) {
                if (!strcasecmp(v, "middle") || !strcasecmp(v, "absmiddle") || !strcasecmp(v, "center")) st->valign = VA_MIDDLE;
                else if (!strcasecmp(v, "top") || !strcasecmp(v, "texttop")) st->valign = VA_TOP;
            }
        }
        break;
    }
    case T_HR: {
        len_t l;
        if (attr_len(node_attr(n, "width"), &l, z)) st->width = l;
        if ((v = node_attr(n, "size"))) { float h = (float)atoi(v) * z; st->height = (len_t){MAX(0, h - 2), U_PX}; }
        if ((v = node_attr(n, "color")) && css_color(v, &col)) { for (int i = 0; i < 4; i++) st->bc[i] = col; st->bg = col; }
        if ((v = node_attr(n, "align"))) {
            if (!strcasecmp(v, "left")) st->margin[3] = (len_t){0, U_PX};
            else if (!strcasecmp(v, "right")) st->margin[1] = (len_t){0, U_PX};
        }
        break;
    }
    case T_P: case T_DIV: case T_H1: case T_H2: case T_H3: case T_H4: case T_H5: case T_H6: case T_CAPTION: case T_LEGEND:
        set_align_attr(st, node_attr(n, "align"), x);
        break;
    case T_UL: case T_OL: case T_LI:
        if ((v = node_attr(n, "type"))) {
            int ls = -1;
            if (!strcmp(v, "1")) ls = LS_DECIMAL;
            else if (!strcmp(v, "a")) ls = LS_LALPHA;
            else if (!strcmp(v, "A")) ls = LS_UALPHA;
            else if (!strcmp(v, "i")) ls = LS_LROMAN;
            else if (!strcmp(v, "I")) ls = LS_UROMAN;
            else ls = list_of(v);
            if (ls >= 0) st->list = (u8)ls;
        }
        break;
    case T_PRE: case T_LISTING: case T_XMP:
        break;
    case T_SELECT: case T_TEXTAREA: case T_BUTTON:
        break;
    }
}

static void reset_noninherited(style_t *st) {
    st->display = D_INLINE;
    st->bg = 0;
    st->grad_top = st->grad_bot = 0;
    for (int i = 0; i < 4; i++) {
        st->margin[i] = (len_t){0, U_PX};
        st->pad[i] = (len_t){0, U_PX};
        st->bw[i] = 0;
        st->bc[i] = BC_CURRENT;
    }
    st->width = st->height = (len_t){0, U_AUTO};
    st->min_w = st->min_h = (len_t){0, U_PX};
    st->max_w = (len_t){0, U_NONE};
    st->radius = 0;
    st->shadow = 0;
    st->valign = VA_BASELINE;
    st->indent = (len_t){0, U_PX};
    st->flexrow = 0;
    st->border_box = 0;
}

static void apply_phase(sctx_t *S, node_t *n, style_t *st, const style_t *ps, rule_t **m, int nm, bool important, bool font_pass, extra_t *x) {
    for (int i = 0; i < nm; i++) {
        rule_t *r = m[i];
        for (int k = 0; k < r->ndecls; k++) {
            decl_t *d = &r->decls[k];
            if (d->important != important) continue;
            bool is_font = d->prop == P_FONT || d->prop == P_FONT_SIZE;
            if (is_font != font_pass) continue;
            if (font_pass) apply_font(st, ps, d->prop, d->val, &S->c);
            else apply_decl(st, ps, d->prop, d->val, &S->c, x);
        }
    }
}

static void apply_inline(sctx_t *S, decl_t *d, int nd, style_t *st, const style_t *ps, bool important, bool font_pass, extra_t *x) {
    for (int k = 0; k < nd; k++) {
        if (d[k].important != important) continue;
        bool is_font = d[k].prop == P_FONT || d[k].prop == P_FONT_SIZE;
        if (is_font != font_pass) continue;
        if (font_pass) apply_font(st, ps, d[k].prop, d[k].val, &S->c);
        else apply_decl(st, ps, d[k].prop, d[k].val, &S->c, x);
    }
}

static bool is_blockish(int d) { return d != D_INLINE && d != D_INLINE_BLOCK && d != D_INLINE_TABLE && d != D_NONE; }

static void style_node(sctx_t *S, node_t *n, const style_t *ps) {
    /* reaproveita o estilo já alocado (reestilizações frequentes vindas do JavaScript) */
    style_t *st = n->st ? n->st : arena_alloc(S->a, sizeof(style_t));
    *st = *ps;
    reset_noninherited(st);
    st->cellpad = ps->cellpad;
    st->border_attr = ps->border_attr;
    n->st = st;
    extra_t x = {0};
    /* regras */
    rule_t **mu = S->match;
    int nu = collect(S->ua, n, mu, S->maxmatch / 2);
    rule_t **ma = S->match + nu;
    int na = collect(S->author, n, ma, S->maxmatch - nu);
    decl_t *sd = NULL;
    int nsd = 0;
    const char *sattr = node_attr(n, "style");
    if (sattr && *sattr) nsd = parse_decls(&S->scratch, sattr, sattr + strlen(sattr), &sd);
    for (int pass = 1; pass >= 0; pass--) {
        bool fp = pass == 1;
        S->c.fs = fp ? ps->size : st->size;
        apply_phase(S, n, st, ps, mu, nu, false, fp, &x);
        presentational(S, n, st, ps, fp, &x);
        apply_phase(S, n, st, ps, ma, na, false, fp, &x);
        apply_inline(S, sd, nsd, st, ps, false, fp, &x);
        apply_phase(S, n, st, ps, ma, na, true, fp, &x);
        apply_inline(S, sd, nsd, st, ps, true, fp, &x);
        if (fp && n->tag == T_HTML) S->c.rootfs = st->size;
    }
    /* pós-processamento */
    for (int i = 0; i < 4; i++) if (st->bc[i] == BC_CURRENT) st->bc[i] = st->color;
    if (n->tag == T_HTML) st->display = D_BLOCK;
    if ((x.position >= 2 && ((x.has_left && x.left < -400) || (x.has_top && x.top < -400))) || x.opacity0) st->display = D_NONE;
    if (x.overflow_hidden && st->width.u == U_PX && st->height.u == U_PX && st->width.v <= 2 && st->height.v <= 2) st->display = D_NONE;
    if (x.position == 3 && st->display != D_NONE) {
        /* barras fixas (cookies, avisos) ficam no fluxo normal */
    }
    if (x.flt && (st->display == D_BLOCK || st->display == D_LIST_ITEM || st->display == D_TABLE)) st->display = D_INLINE_BLOCK;
    st->flexrow = x.flex && !x.column && st->display != D_NONE;
    /* filhos de flex em linha viram inline-block (aproximação do flexbox) */
    if (ps->flexrow && (st->display == D_BLOCK || st->display == D_LIST_ITEM || st->display == D_TABLE)) {
        st->display = D_INLINE_BLOCK;
        st->valign = VA_MIDDLE;
    }
    if (n->tag == T_TD || n->tag == T_TH) st->collapse = ps->collapse;
}

static void style_rec(sctx_t *S, node_t *n, const style_t *ps, int depth) {
    style_node(S, n, ps);
    if (n->link == NULL) n->link = (n->tag == T_A && node_attr(n, "href")) ? n : (n->parent ? n->parent->link : NULL);
    if (n->st->display == D_NONE) return;
    bool hb = false;
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type == N_TEXT) { c->link = n->link; c->st = n->st; continue; }
        if (c->type != N_ELEM) continue;
        c->link = NULL;
        if (depth > 400) { c->st = NULL; continue; }
        style_rec(S, c, n->st, depth + 1);
        if (c->st && (is_blockish(c->st->display) || c->has_block)) hb = true;
    }
    n->has_block = hb;
}

void css_style_tree(node_t *root, sheet_t *ua, sheet_t *author, arena_t *a, float zoom, int vw, int vh) {
    sctx_t S = {0};
    S.ua = ua;
    S.author = author;
    S.a = a;
    S.c.zoom = zoom;
    S.c.fs = 16 * zoom;
    S.c.rootfs = 16 * zoom;
    S.c.vw = vw;
    S.c.vh = vh;
    S.maxmatch = 512;
    S.match = kmalloc(sizeof(rule_t *) * (usize)S.maxmatch);
    if (!S.match) return;
    style_t init = {0};
    init.display = D_BLOCK;
    init.size = 16 * zoom;
    init.color = RGB(0, 0, 0);
    init.weight = FONT_REGULAR;
    init.list = LS_DISC;
    init.cellpad = -1;
    init.spacing = 2 * zoom;
    reset_noninherited(&init);
    init.display = D_BLOCK;
    for (node_t *c = root->first; c; c = c->next) if (c->type == N_ELEM) style_rec(&S, c, &init, 0);
    kfree(S.match);
    arena_free(&S.scratch);
}
