/* mOS — Horizonte: geração de caixas, layout (blocos, linhas, tabelas), pintura e teste de clique */
#include "web.h"
#include "apps.h"

typedef struct {
    doc_t *d;
    arena_t *a;
    item_t *it;
    int ni, ci;
    frag_t *fr;
    int nf, cf;
    line_t *ln;
    int nl, cl;
    u32 cur_bg;
    int depth;
} lay_t;

static int rl(len_t l, int cw) {
    if (l.u == U_PX) return (int)(l.v + (l.v >= 0 ? 0.5f : -0.5f));
    if (l.u == U_PCT) return (int)(l.v * cw / 100.0f + 0.5f);
    return 0;
}
static int pxo(len_t l) { return l.u == U_PX ? (int)(l.v + 0.5f) : 0; }

static font_t *font_of(style_t *st) { return font_get(st->weight); }
static float fsize(style_t *st) { return (float)(int)(st->size * 2 + 0.5f) / 2.0f; }

static int mono_count(const char *t, int len) {
    int n = 0;
    for (int i = 0; i < len; i++) if (((u8)t[i] & 0xC0) != 0x80) n++;
    return n;
}

static int text_w(style_t *st, const char *t, int len) {
    if (st->mono) return mono_count(t, len) * mono_cell_w(fsize(st));
    return text_width_n(font_of(st), fsize(st), t, len);
}

static int line_height_of(style_t *st, int asc, int desc) {
    if (st->lh_mode == 1) return (int)(st->lh * st->size + 0.5f);
    if (st->lh_mode == 2) return (int)(st->lh + 0.5f);
    return (int)((asc + desc) * 1.1f + 0.5f);
}

/* ============================================================ pilhas temporárias */
static item_t *push_item(lay_t *L) {
    if (L->ni == L->ci) {
        int nc = L->ci ? L->ci * 2 : 1024;
        item_t *n = kmalloc(sizeof(item_t) * (usize)nc);
        if (!n) return NULL;
        if (L->it) { memcpy(n, L->it, sizeof(item_t) * (usize)L->ni); kfree(L->it); }
        L->it = n;
        L->ci = nc;
    }
    item_t *it = &L->it[L->ni++];
    memset(it, 0, sizeof *it);
    return it;
}
static frag_t *push_frag(lay_t *L) {
    if (L->nf == L->cf) {
        int nc = L->cf ? L->cf * 2 : 1024;
        frag_t *n = kmalloc(sizeof(frag_t) * (usize)nc);
        if (!n) return NULL;
        if (L->fr) { memcpy(n, L->fr, sizeof(frag_t) * (usize)L->nf); kfree(L->fr); }
        L->fr = n;
        L->cf = nc;
    }
    frag_t *f = &L->fr[L->nf++];
    memset(f, 0, sizeof *f);
    return f;
}
static line_t *push_line(lay_t *L) {
    if (L->nl == L->cl) {
        int nc = L->cl ? L->cl * 2 : 256;
        line_t *n = kmalloc(sizeof(line_t) * (usize)nc);
        if (!n) return NULL;
        if (L->ln) { memcpy(n, L->ln, sizeof(line_t) * (usize)L->nl); kfree(L->ln); }
        L->ln = n;
        L->cl = nc;
    }
    return &L->ln[L->nl++];
}

/* ============================================================ caixas */
static box_t *nbox(lay_t *L, int kind, node_t *n, style_t *st) {
    box_t *b = arena_alloc(L->a, sizeof(box_t));
    if (!b) return NULL;
    b->kind = (u8)kind;
    b->node = n;
    b->st = st;
    b->colspan = b->rowspan = 1;
    return b;
}
static void addc(box_t *p, box_t *c) {
    if (!p || !c) return;
    c->parent = p;
    if (p->last) p->last->next = c; else p->first = c;
    p->last = c;
}

static bool is_block_level(int d) {
    return d == D_BLOCK || d == D_LIST_ITEM || d == D_TABLE || d == D_CAPTION || d == D_ROW_GROUP || d == D_ROW || d == D_CELL;
}
static bool is_ctrl_tag(int t) { return t == T_INPUT || t == T_BUTTON || t == T_SELECT || t == T_TEXTAREA; }

/* ------------------------------------------------------------ imagens e controles */
static img_t *get_img(lay_t *L, const char *src) {
    doc_t *d = L->d;
    char url[512];
    url_resolve(d->base, src, url, sizeof url);
    for (img_t *im = d->images; im; im = im->next) if (!strcmp(im->url, url)) return im;
    if (d->nimages >= 400) return NULL;
    img_t *im = kcalloc(1, sizeof(img_t));
    if (!im) return NULL;
    strncpy(im->url, url, sizeof im->url - 1);
    im->state = IMG_PENDING;
    im->next = d->images;
    d->images = im;
    d->nimages++;
    return im;
}

static void collapse_ws(const char *s, int n, char *out, int cap) {
    int k = 0;
    bool sp = false;
    for (int i = 0; i < n && k < cap - 2; i++) {
        if (isspace((u8)s[i])) { sp = k > 0; continue; }
        if (sp) { out[k++] = ' '; sp = false; }
        out[k++] = s[i];
    }
    out[k] = 0;
}

static node_t *form_of(node_t *n) {
    for (node_t *p = n->parent; p && p->type == N_ELEM; p = p->parent) if (p->tag == T_FORM) return p;
    return NULL;
}

static void collect_opts(lay_t *L, node_t *n, ctrl_t *c, const char **labs, const char **vals, int max) {
    for (node_t *o = n->first; o && c->nopts < max; o = o->next) {
        if (o->type != N_ELEM) continue;
        if (o->tag == T_OPTION) {
            sbuf_t t = {0};
            node_text_content(o, &t);
            char lab[96];
            collapse_ws(t.p ? t.p : "", t.len, lab, sizeof lab);
            sb_free(&t);
            labs[c->nopts] = arena_strndup(&L->d->arena, lab, -1);
            const char *v = node_attr(o, "value");
            vals[c->nopts] = v ? v : labs[c->nopts];
            if (node_attr(o, "selected")) c->sel = c->nopts;
            c->nopts++;
        } else collect_opts(L, o, c, labs, vals, max);
    }
}

static ctrl_t *make_ctrl(lay_t *L, node_t *n) {
    if (n->ctrl) return n->ctrl;
    doc_t *d = L->d;
    ctrl_t *c = arena_alloc(&d->arena, sizeof(ctrl_t));
    if (!c) return NULL;
    c->node = n;
    c->form = form_of(n);
    const char *type = node_attr(n, "type");
    const char *val = node_attr(n, "value");
    c->kind = C_TEXT;
    if (n->tag == T_INPUT) {
        if (!type || !*type) c->kind = C_TEXT;
        else if (!strcasecmp(type, "password")) c->kind = C_PASSWORD;
        else if (!strcasecmp(type, "submit")) c->kind = C_SUBMIT;
        else if (!strcasecmp(type, "reset")) c->kind = C_RESET;
        else if (!strcasecmp(type, "button") || !strcasecmp(type, "file")) c->kind = C_BUTTON;
        else if (!strcasecmp(type, "checkbox")) c->kind = C_CHECK;
        else if (!strcasecmp(type, "radio")) c->kind = C_RADIO;
        else if (!strcasecmp(type, "image")) c->kind = C_IMAGE;
        else if (!strcasecmp(type, "hidden")) c->kind = C_HIDDEN;
        else c->kind = C_TEXT;
        if (c->kind == C_SUBMIT) strncpy(c->label, val ? val : "Enviar", sizeof c->label - 1);
        else if (c->kind == C_RESET) strncpy(c->label, val ? val : "Redefinir", sizeof c->label - 1);
        else if (c->kind == C_BUTTON) strncpy(c->label, val ? val : (type && !strcasecmp(type, "file") ? "Escolher arquivo" : ""), sizeof c->label - 1);
        else if (c->kind == C_IMAGE) { const char *alt = node_attr(n, "alt"); strncpy(c->label, alt ? alt : "Enviar", sizeof c->label - 1); }
        c->checked = node_attr(n, "checked") != NULL;
        const char *sz = node_attr(n, "size");
        c->cols = sz ? atoi(sz) : 0;
    } else if (n->tag == T_BUTTON) {
        c->kind = (type && !strcasecmp(type, "button")) ? C_BUTTON : (type && !strcasecmp(type, "reset")) ? C_RESET : C_SUBMIT;
        sbuf_t t = {0};
        node_text_content(n, &t);
        collapse_ws(t.p ? t.p : "", t.len, c->label, sizeof c->label);
        sb_free(&t);
        if (!c->label[0]) strcpy(c->label, "Enviar");
    } else if (n->tag == T_SELECT) {
        c->kind = C_SELECT;
        const char **labs = arena_alloc(&d->arena, sizeof(char *) * 48);
        const char **vals = arena_alloc(&d->arena, sizeof(char *) * 48);
        c->opts = labs;
        c->vals = vals;
        collect_opts(L, n, c, labs, vals, 48);
    } else if (n->tag == T_TEXTAREA) {
        c->kind = C_TEXTAREA;
        const char *r = node_attr(n, "rows"), *co = node_attr(n, "cols");
        c->rows = r ? CLAMP(atoi(r), 1, 40) : 3;
        c->cols = co ? CLAMP(atoi(co), 4, 200) : 30;
    }
    c->cap = c->kind == C_TEXTAREA ? 4096 : 512;
    c->buf = kcalloc(1, (usize)c->cap);
    if (c->kind == C_TEXTAREA) {
        sbuf_t t = {0};
        node_text_content(n, &t);
        if (t.p) strncpy(c->buf, t.p, (usize)c->cap - 1);
        sb_free(&t);
    } else if (val && (c->kind == C_TEXT || c->kind == C_PASSWORD || c->kind == C_HIDDEN || c->kind == C_CHECK || c->kind == C_RADIO)) {
        strncpy(c->buf, val, (usize)c->cap - 1);
    }
    if (d->nctrls == d->cctrls) {
        int nc = d->cctrls ? d->cctrls * 2 : 16;
        ctrl_t **na = kmalloc(sizeof(ctrl_t *) * (usize)nc);
        if (!na) return c;
        if (d->ctrls) { memcpy(na, d->ctrls, sizeof(ctrl_t *) * (usize)d->nctrls); kfree(d->ctrls); }
        d->ctrls = na;
        d->cctrls = nc;
    }
    c->id = 100000 + d->nctrls;
    d->ctrls[d->nctrls++] = c;
    n->ctrl = c;
    return c;
}

/* tamanho intrínseco de um elemento substituído (conteúdo, sem bordas) */
static void replaced_size(lay_t *L, box_t *b, int cw, int *w, int *h) {
    node_t *n = b->node;
    style_t *st = b->st;
    float z = L->d->zoom;
    if (n->ctrl) {
        ctrl_t *c = n->ctrl;
        int fw = 0, fh = (int)(22 * z);
        font_t *f = font_get(FONT_REGULAR);
        switch (c->kind) {
        case C_TEXT: case C_PASSWORD: fw = c->cols > 0 ? MIN(c->cols * 8 + 14, 600) : 180; fw = (int)(fw * z); break;
        case C_SUBMIT: case C_RESET: case C_BUTTON: case C_IMAGE: fw = text_width(f, FONT_UI, c->label) + 26; fh = 22; break;
        case C_CHECK: case C_RADIO: fw = 16; fh = 16; break;
        case C_SELECT: {
            int mw = 40;
            for (int i = 0; i < c->nopts; i++) mw = MAX(mw, text_width(f, FONT_UI, c->opts[i]));
            fw = MIN(mw + 40, 400);
            fh = 22;
            break;
        }
        case C_TEXTAREA: fw = (int)(c->cols * 7.5f * z) + 12; fh = (int)(c->rows * 17 * z) + 10; break;
        default: break;
        }
        if (st->width.u == U_PX || st->width.u == U_PCT) fw = rl(st->width, cw) - (int)(st->border_box ? 0 : 0);
        if (st->height.u == U_PX && c->kind == C_TEXTAREA) fh = rl(st->height, 0);
        *w = MAX(fw, 8);
        *h = fh;
        return;
    }
    int iw = 0, ih = 0;
    img_t *im = n->img;
    if (im && im->state == IMG_OK && im->surf) { iw = (int)(im->surf->w * z); ih = (int)(im->surf->h * z); }
    bool hw = st->width.u == U_PX || (st->width.u == U_PCT && cw > 0), hh = st->height.u == U_PX;
    int W = hw ? rl(st->width, cw) : 0, H = hh ? rl(st->height, 0) : 0;
    if (hw && hh) { *w = W; *h = H; }
    else if (hw) { *w = W; *h = iw ? (int)((float)W * ih / iw + 0.5f) : (im && im->state == IMG_OK ? W : 0); }
    else if (hh) { *h = H; *w = ih ? (int)((float)H * iw / ih + 0.5f) : 0; }
    else if (iw) { *w = iw; *h = ih; }
    else if (im && (im->state == IMG_ERROR || !im)) {
        const char *alt = node_attr(n, "alt");
        if (alt && *alt) { *w = MIN(text_width(font_get(FONT_REGULAR), 12 * z, alt) + 10, 300); *h = (int)(18 * z); }
        else { *w = 0; *h = 0; }
    } else { *w = 0; *h = 0; }
    if (!im) {
        const char *alt = node_attr(n, "alt");
        if (alt && *alt && !*w) { *w = MIN(text_width(font_get(FONT_REGULAR), 12 * z, alt) + 10, 300); *h = (int)(18 * z); }
    }
    /* max-width */
    if (st->max_w.u == U_PX || (st->max_w.u == U_PCT && cw > 0)) {
        int mx = rl(st->max_w, cw);
        if (*w > mx && mx > 0) { if (!hh) *h = *w ? (int)((float)*h * mx / *w + 0.5f) : *h; *w = mx; }
    }
}

/* ------------------------------------------------------------ itens inline */
static void push_text_item(lay_t *L, int kind, const char *t, int len, style_t *st, node_t *node, bool glue) {
    item_t *it = push_item(L);
    if (!it) return;
    it->kind = (u8)kind;
    it->t = t;
    it->len = len;
    it->st = st;
    it->node = node;
    it->glue = glue;
    it->bg = L->cur_bg;
    if (kind == IT_WORD) it->w = (short)MIN(text_w(st, t, len), 32000);
    else if (kind == IT_SPACE) it->w = (short)(st->mono ? mono_cell_w(fsize(st)) : text_width(font_of(st), fsize(st), " "));
}

static int last_kind(lay_t *L, int start) { return L->ni > start ? L->it[L->ni - 1].kind : -1; }

static const char *transform_text(lay_t *L, style_t *st, const char *t, int len, bool word_start) {
    if (st->transform == TT_NONE) return t;
    char *c = arena_strndup(L->a, t, len);
    for (int i = 0; i < len; i++) {
        bool up = st->transform == TT_UPPER || (st->transform == TT_CAPITALIZE && i == 0 && word_start);
        u8 ch = (u8)c[i];
        if (st->transform == TT_LOWER) c[i] = (char)tolower(ch);
        else if (up) {
            if (ch >= 'a' && ch <= 'z') c[i] = (char)(ch - 32);
            else if (ch == 0xC3 && i + 1 < len && (u8)c[i + 1] >= 0xA0 && (u8)c[i + 1] <= 0xBE && (u8)c[i + 1] != 0xB7) { c[i + 1] = (char)((u8)c[i + 1] - 0x20); i++; }
        }
    }
    return c;
}

static int lines_start_idx;

static void collect_text(lay_t *L, node_t *tn, style_t *st, node_t *owner) {
    const char *s = tn->text;
    int n = tn->len;
    int ws = st->ws;
    if (ws == WS_PRE || ws == WS_PRE_WRAP) {
        int i = 0;
        while (i <= n) {
            int j = i;
            while (j < n && s[j] != '\n') j++;
            if (j > i) {
                const char *t = s + i;
                int len = j - i;
                /* expande tabulações */
                bool tab = false;
                for (int k = 0; k < len; k++) if (t[k] == '\t' || t[k] == '\r') tab = true;
                if (tab) {
                    sbuf_t b = {0};
                    int col = 0;
                    for (int k = 0; k < len; k++) {
                        if (t[k] == '\r') continue;
                        if (t[k] == '\t') { do { sb_putc(&b, ' '); col++; } while (col % 8); }
                        else { sb_putc(&b, t[k]); if (((u8)t[k] & 0xC0) != 0x80) col++; }
                    }
                    t = arena_strndup(L->a, b.p ? b.p : "", b.len);
                    len = b.len;
                    sb_free(&b);
                }
                if (len) push_text_item(L, IT_WORD, t, len, st, owner, last_kind(L, lines_start_idx) == IT_WORD);
            }
            if (j < n) { item_t *it = push_item(L); if (it) { it->kind = IT_BREAK; it->st = st; } }
            i = j + 1;
        }
        return;
    }
    bool nowrap = ws == WS_NOWRAP;
    int i = 0;
    while (i < n) {
        if (isspace((u8)s[i])) {
            bool nl = false;
            while (i < n && isspace((u8)s[i])) { if (s[i] == '\n') nl = true; i++; }
            if (ws == WS_PRE_LINE && nl) { item_t *it = push_item(L); if (it) { it->kind = IT_BREAK; it->st = st; } continue; }
            int lk = last_kind(L, lines_start_idx);
            if (lk != IT_SPACE && lk != -1 && lk != IT_BREAK) push_text_item(L, IT_SPACE, " ", 1, st, owner, nowrap);
            continue;
        }
        int j = i;
        while (j < n && !isspace((u8)s[j])) j++;
        int lk = last_kind(L, lines_start_idx);
        bool glue = lk == IT_WORD || lk == IT_SPACER || (nowrap && lk == IT_SPACE);
        const char *t = transform_text(L, st, s + i, j - i, lk != IT_WORD);
        push_text_item(L, IT_WORD, t, j - i, st, owner, glue);
        i = j;
    }
}

typedef struct { box_t *lines; int start; box_t *cont; } lst_t;

static box_t *gen_block(lay_t *L, node_t *n);
static box_t *gen_atom(lay_t *L, node_t *n);
static void gen_children(lay_t *L, box_t *cont, node_t *n, lst_t *ls);

static void lines_begin(lay_t *L, lst_t *ls) {
    if (ls->lines) return;
    ls->lines = nbox(L, B_LINES, NULL, ls->cont->st);
    ls->lines->is_anon = 1;
    ls->start = L->ni;
    lines_start_idx = L->ni;
}

static void lines_end(lay_t *L, lst_t *ls) {
    if (!ls->lines) return;
    int n = L->ni - ls->start;
    bool content = false;
    for (int i = ls->start; i < L->ni; i++) if (L->it[i].kind != IT_SPACE) { content = true; break; }
    if (content && n > 0) {
        item_t *its = arena_alloc(L->a, sizeof(item_t) * (usize)n);
        if (its) {
            memcpy(its, L->it + ls->start, sizeof(item_t) * (usize)n);
            ls->lines->items = its;
            ls->lines->nitems = n;
            addc(ls->cont, ls->lines);
        }
    }
    L->ni = ls->start;
    ls->lines = NULL;
}

static void push_anchor(lay_t *L, node_t *n) {
    if (!node_attr(n, "id") && !(n->tag == T_A && node_attr(n, "name"))) return;
    item_t *it = push_item(L);
    if (it) { it->kind = IT_ANCHOR; it->node = n; it->st = n->st; }
}

static void inline_node(lay_t *L, node_t *c, lst_t *ls);

static void inline_children(lay_t *L, node_t *n, lst_t *ls) {
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type == N_TEXT) collect_text(L, c, n->st, n);
        else if (c->type == N_ELEM && c->st && c->st->display != D_NONE) inline_node(L, c, ls);
    }
}

static void push_spacer(lay_t *L, style_t *st, node_t *n, int w) {
    item_t *it = push_item(L);
    if (!it) return;
    int lk = L->ni >= 2 ? L->it[L->ni - 2].kind : -1;
    if (L->ni - 1 <= lines_start_idx) lk = -1;
    it->kind = IT_SPACER;
    it->w = (short)w;
    it->st = st;
    it->node = n;
    it->bg = L->cur_bg;
    it->glue = lk == IT_WORD || lk == IT_SPACER;
}

static void inline_node(lay_t *L, node_t *c, lst_t *ls) {
    style_t *st = c->st;
    int d = st->display;
    if (c->tag == T_BR) { item_t *it = push_item(L); if (it) { it->kind = IT_BREAK; it->st = st; it->node = c; } return; }
    if (c->tag == T_WBR) { item_t *it = push_item(L); if (it) { it->kind = IT_SPACE; it->w = 0; it->st = st; } return; }
    if (c->tag == T_IMG || is_ctrl_tag(c->tag) || d == D_INLINE_BLOCK || d == D_INLINE_TABLE) {
        box_t *a = gen_atom(L, c);
        if (!a) return;
        push_anchor(L, c);
        item_t *it = push_item(L);
        if (!it) return;
        it->kind = IT_ATOM;
        it->atom = a;
        it->st = st;
        it->node = c;
        it->bg = L->cur_bg;
        return;
    }
    if (is_block_level(d)) {
        /* bloco dentro de inline sem has_block (não deveria ocorrer): trata como atom */
        box_t *a = gen_block(L, c);
        if (!a) return;
        a->inline_block = 1;
        item_t *it = push_item(L);
        if (it) { it->kind = IT_ATOM; it->atom = a; it->st = st; it->node = c; }
        return;
    }
    push_anchor(L, c);
    u32 old = L->cur_bg;
    if (C_A(st->bg)) L->cur_bg = st->bg;
    int lw = pxo(st->margin[3]) + (int)st->bw[3] + pxo(st->pad[3]);
    int rw = pxo(st->margin[1]) + (int)st->bw[1] + pxo(st->pad[1]);
    if (lw > 0) push_spacer(L, st, c, lw);
    inline_children(L, c, ls);
    if (rw > 0) push_spacer(L, st, c, rw);
    L->cur_bg = old;
}

static void gen_children(lay_t *L, box_t *cont, node_t *n, lst_t *ls) {
    if (++L->depth > 300) { L->depth--; return; }
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type == N_TEXT) {
            /* espaço puro antes de qualquer conteúdo não abre linhas */
            if (!ls->lines) {
                bool ws = true;
                if (n->st->ws == WS_NORMAL || n->st->ws == WS_NOWRAP) { for (int i = 0; i < c->len; i++) if (!isspace((u8)c->text[i])) { ws = false; break; } }
                else ws = false;
                if (ws) continue;
            }
            lines_begin(L, ls);
            collect_text(L, c, n->st, n);
            continue;
        }
        if (c->type != N_ELEM || !c->st || c->st->display == D_NONE) continue;
        int d = c->st->display;
        if (c->tag != T_IMG && !is_ctrl_tag(c->tag) && is_block_level(d)) {
            lines_end(L, ls);
            box_t *b = gen_block(L, c);
            if (b) addc(cont, b);
            continue;
        }
        if ((c->tag == T_IMG || is_ctrl_tag(c->tag)) && is_block_level(d)) {
            lines_end(L, ls);
            box_t *b = gen_atom(L, c);
            if (b) addc(cont, b);
            continue;
        }
        if (d == D_INLINE && c->has_block) {
            /* inline com blocos dentro: transparente */
            push_anchor(L, c);
            gen_children(L, cont, c, ls);
            continue;
        }
        lines_begin(L, ls);
        inline_node(L, c, ls);
    }
    L->depth--;
}

static const char *roman(lay_t *L, int v, bool upper) {
    static const int vals[] = {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
    static const char *syms[] = {"m", "cm", "d", "cd", "c", "xc", "l", "xl", "x", "ix", "v", "iv", "i"};
    char buf[32];
    int k = 0;
    if (v <= 0 || v > 3999) { snprintf(buf, sizeof buf, "%d.", v); return arena_strndup(L->a, buf, -1); }
    for (int i = 0; i < 13; i++) while (v >= vals[i]) { for (const char *s = syms[i]; *s; s++) buf[k++] = upper ? (char)toupper(*s) : *s; v -= vals[i]; }
    buf[k++] = '.';
    buf[k] = 0;
    return arena_strndup(L->a, buf, -1);
}

static const char *list_marker(lay_t *L, node_t *n, style_t *st) {
    if (st->list == LS_NONE) return NULL;
    if (st->list == LS_DISC) return "\x01";
    if (st->list == LS_CIRCLE) return "\x02";
    if (st->list == LS_SQUARE) return "\x03";
    node_t *p = n->parent;
    int idx = 1;
    if (p) { const char *s = node_attr(p, "start"); if (s) idx = atoi(s); }
    for (node_t *q = p ? p->first : NULL; q && q != n; q = q->next) {
        if (q->type != N_ELEM || !q->st || q->st->display != D_LIST_ITEM) continue;
        const char *v = node_attr(q, "value");
        idx = v ? atoi(v) + 1 : idx + 1;
    }
    const char *v = node_attr(n, "value");
    if (v) idx = atoi(v);
    char buf[24];
    if (st->list == LS_LALPHA || st->list == LS_UALPHA) {
        int k = idx - 1;
        if (k < 0) k = 0;
        char a = (char)((st->list == LS_LALPHA ? 'a' : 'A') + k % 26);
        snprintf(buf, sizeof buf, "%c.", a);
    } else if (st->list == LS_LROMAN || st->list == LS_UROMAN) return roman(L, idx, st->list == LS_UROMAN);
    else snprintf(buf, sizeof buf, "%d.", idx);
    return arena_strndup(L->a, buf, -1);
}

static box_t *gen_table(lay_t *L, node_t *n);

static box_t *gen_block(lay_t *L, node_t *n) {
    style_t *st = n->st;
    if (st->display == D_TABLE || st->display == D_INLINE_TABLE) return gen_table(L, n);
    box_t *b = nbox(L, B_BLOCK, n, st);
    if (!b) return NULL;
    if (st->display == D_LIST_ITEM) b->marker = list_marker(L, n, st);
    lst_t ls = {NULL, 0, b};
    int save_start = lines_start_idx;
    gen_children(L, b, n, &ls);
    lines_end(L, &ls);
    lines_start_idx = save_start;
    return b;
}

static box_t *gen_atom(lay_t *L, node_t *n) {
    if (n->tag == T_IMG) {
        box_t *b = nbox(L, B_REPLACED, n, n->st);
        if (!b) return NULL;
        const char *src = node_attr(n, "src");
        if (!src || !*src) src = node_attr(n, "data-src");
        if (!n->img && src && *src) n->img = get_img(L, src);
        return b;
    }
    if (is_ctrl_tag(n->tag)) {
        ctrl_t *c = make_ctrl(L, n);
        if (!c || c->kind == C_HIDDEN) return NULL;
        return nbox(L, B_REPLACED, n, n->st);
    }
    box_t *b = gen_block(L, n);
    if (b) b->inline_block = 1;
    return b;
}

/* ------------------------------------------------------------ tabelas */
static box_t *gen_cell(lay_t *L, node_t *n) {
    box_t *b = nbox(L, B_CELL, n, n->st);
    if (!b) return NULL;
    const char *cs = node_attr(n, "colspan"), *rs = node_attr(n, "rowspan");
    b->colspan = cs ? CLAMP(atoi(cs), 1, 64) : 1;
    b->rowspan = rs ? CLAMP(atoi(rs), 1, 64) : 1;
    lst_t ls = {NULL, 0, b};
    int save_start = lines_start_idx;
    gen_children(L, b, n, &ls);
    lines_end(L, &ls);
    lines_start_idx = save_start;
    return b;
}

static void row_cells(lay_t *L, box_t *r, node_t *n) {
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type != N_ELEM || !c->st || c->st->display == D_NONE) continue;
        if (c->st->display == D_CELL) addc(r, gen_cell(L, c));
        else if (c->tag == T_FORM || c->st->display == D_INLINE) row_cells(L, r, c);
        else {
            /* bloco solto numa linha: vira célula anônima */
            box_t *cell = nbox(L, B_CELL, c, c->st);
            if (!cell) continue;
            box_t *b = gen_block(L, c);
            addc(cell, b);
            addc(r, cell);
        }
    }
}

static void table_rows(lay_t *L, box_t *t, node_t *n) {
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type != N_ELEM || !c->st || c->st->display == D_NONE) continue;
        int d = c->st->display;
        if (d == D_ROW) {
            box_t *r = nbox(L, B_ROW, c, c->st);
            if (!r) continue;
            row_cells(L, r, c);
            addc(t, r);
        } else if (d == D_CAPTION) {
            box_t *cap = nbox(L, B_BLOCK, c, c->st);
            if (!cap) continue;
            cap->is_anon = 2;
            lst_t ls = {NULL, 0, cap};
            int save = lines_start_idx;
            gen_children(L, cap, c, &ls);
            lines_end(L, &ls);
            lines_start_idx = save;
            addc(t, cap);
        } else if (d == D_CELL) {
            box_t *r = t->last;
            if (!r || r->kind != B_ROW || r->is_anon != 3) {
                r = nbox(L, B_ROW, NULL, t->st);
                if (!r) continue;
                r->is_anon = 3;
                addc(t, r);
            }
            addc(r, gen_cell(L, c));
        } else if (d == D_COLUMN) {
            continue;
        } else table_rows(L, t, c);
    }
}

static box_t *gen_table(lay_t *L, node_t *n) {
    box_t *t = nbox(L, B_TABLE, n, n->st);
    if (!t) return NULL;
    table_rows(L, t, n);
    if (n->st->display == D_INLINE_TABLE) t->inline_block = 1;
    return t;
}

/* ============================================================ métricas */
static void metrics(box_t *b, int cw) {
    style_t *st = b->st;
    if (b->kind == B_LINES || b->kind == B_ROW) { memset(b->m, 0, sizeof b->m); memset(b->p, 0, sizeof b->p); memset(b->b, 0, sizeof b->b); return; }
    for (int i = 0; i < 4; i++) {
        b->m[i] = st->margin[i].u == U_AUTO ? 0 : (float)rl(st->margin[i], cw);
        b->p[i] = (float)rl(st->pad[i], cw);
        b->b[i] = st->bw[i];
    }
    if (b->kind == B_CELL) { b->m[0] = b->m[1] = b->m[2] = b->m[3] = 0; }
}

static int hz(box_t *b) { return (int)(b->p[1] + b->p[3] + b->b[1] + b->b[3]); }
static int vt(box_t *b) { return (int)(b->p[0] + b->p[2] + b->b[0] + b->b[2]); }

static void minmax(lay_t *L, box_t *b);

/* unidades inline para min/max */
static void lines_minmax(lay_t *L, box_t *b) {
    int mn = 0, mx = 0, cur = 0, pend = 0;
    bool has = false;
    for (int i = 0; i < b->nitems;) {
        item_t *it = &b->items[i];
        if (it->kind == IT_SPACE) { if (has) pend = it->w; i++; continue; }
        if (it->kind == IT_BREAK) { mx = MAX(mx, cur); cur = 0; pend = 0; has = false; i++; continue; }
        if (it->kind == IT_ANCHOR) { i++; continue; }
        int uw = 0, umax = 0;
        int j = i;
        do {
            item_t *x = &b->items[j];
            if (x->kind == IT_ATOM) {
                box_t *a = x->atom;
                minmax(L, a);
                int ml = pxo(a->st->margin[1]) + pxo(a->st->margin[3]);
                uw += a->minw + ml;
                umax += a->maxw + ml;
            } else { uw += x->w; umax += x->w; }
            j++;
        } while (j < b->nitems && b->items[j].glue && b->items[j].kind != IT_ATOM && b->items[j].kind != IT_BREAK && b->items[j].kind != IT_ANCHOR);
        mn = MAX(mn, uw);
        cur += pend + umax;
        pend = 0;
        has = true;
        i = j;
    }
    mx = MAX(mx, cur);
    b->minw = mn;
    b->maxw = mx;
}

typedef struct { box_t *cell; int row, col; } gcell_t;

static int build_grid(lay_t *L, box_t *t, gcell_t **out, int *nrows, int *ncols) {
    int cap = 64, n = 0;
    gcell_t *g = kmalloc(sizeof(gcell_t) * (usize)cap);
    if (!g) return 0;
    int occ[256];
    memset(occ, 0, sizeof occ);
    int row = 0, maxc = 0;
    for (box_t *r = t->first; r; r = r->next) {
        if (r->kind != B_ROW) continue;
        int col = 0;
        for (box_t *c = r->first; c; c = c->next) {
            while (col < 256 && occ[col] > 0) col++;
            if (col >= 256) break;
            if (n == cap) {
                cap *= 2;
                gcell_t *ng = kmalloc(sizeof(gcell_t) * (usize)cap);
                if (!ng) break;
                memcpy(ng, g, sizeof(gcell_t) * (usize)n);
                kfree(g);
                g = ng;
            }
            g[n++] = (gcell_t){c, row, col};
            int cs = MIN(c->colspan, 256 - col);
            for (int k = 0; k < cs; k++) occ[col + k] = MAX(occ[col + k], c->rowspan);
            col += cs;
            maxc = MAX(maxc, col);
        }
        for (int k = 0; k < 256; k++) if (occ[k] > 0) occ[k]--;
        row++;
    }
    *out = g;
    *nrows = row;
    *ncols = maxc;
    return n;
}

static float table_spacing(box_t *t) { return t->st->collapse ? 0 : t->st->spacing; }

static void table_columns(lay_t *L, box_t *t, gcell_t *g, int n, int ncols, int *cmin, int *cmax, int *cfix, float *cpct) {
    for (int c = 0; c < ncols; c++) { cmin[c] = cmax[c] = 0; cfix[c] = -1; cpct[c] = 0; }
    int sp = (int)table_spacing(t);
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < n; i++) {
            box_t *c = g[i].cell;
            int col = g[i].col, cs = MIN(c->colspan, ncols - col);
            if ((pass == 0) != (cs == 1)) continue;
            minmax(L, c);
            int fix = -1;
            if (c->st->width.u == U_PX) fix = rl(c->st->width, 0) + (c->st->border_box ? 0 : hz(c));
            if (cs == 1) {
                cmin[col] = MAX(cmin[col], c->minw);
                cmax[col] = MAX(cmax[col], c->maxw);
                if (fix >= 0) cfix[col] = MAX(cfix[col], fix);
                if (c->st->width.u == U_PCT) cpct[col] = MAX(cpct[col], c->st->width.v);
            } else {
                int smin = sp * (cs - 1), smax = sp * (cs - 1);
                for (int k = 0; k < cs; k++) { smin += cmin[col + k]; smax += cmax[col + k]; }
                if (c->minw > smin) { int ex = c->minw - smin; for (int k = 0; k < cs; k++) cmin[col + k] += ex / cs + (k < ex % cs); }
                if (c->maxw > smax) { int ex = c->maxw - smax; for (int k = 0; k < cs; k++) cmax[col + k] += ex / cs + (k < ex % cs); }
            }
        }
    }
    for (int c = 0; c < ncols; c++) {
        if (cfix[c] >= 0) { cmax[c] = MAX(cfix[c], cmin[c]); }
        if (cmax[c] < cmin[c]) cmax[c] = cmin[c];
    }
}

static void minmax(lay_t *L, box_t *b) {
    if (b->has_minmax) return;
    b->has_minmax = 1;
    style_t *st = b->st;
    int h = 0;
    if (b->kind != B_LINES && b->kind != B_ROW) {
        for (int i = 1; i <= 3; i += 2) h += pxo(st->pad[i]) + (int)st->bw[i];
    }
    if (b->kind == B_LINES) { lines_minmax(L, b); return; }
    if (b->kind == B_REPLACED) {
        int w, hh;
        replaced_size(L, b, 0, &w, &hh);
        if (st->width.u == U_PCT || st->max_w.u == U_PCT) { b->minw = 0; b->maxw = w + h; }
        else b->minw = b->maxw = w + h;
        if (st->width.u == U_PCT && b->node->img && b->node->img->surf) b->maxw = (int)(b->node->img->surf->w * L->d->zoom) + h;
        return;
    }
    if (b->kind == B_TABLE) {
        gcell_t *g;
        int nr, nc;
        int n = build_grid(L, b, &g, &nr, &nc);
        int cmin[256], cmax[256], cfix[256];
        float cpct[256];
        table_columns(L, b, g, n, nc, cmin, cmax, cfix, cpct);
        kfree(g);
        int sp = (int)table_spacing(b);
        int smin = sp * (nc + 1), smax = sp * (nc + 1);
        for (int c = 0; c < nc; c++) { smin += cmin[c]; smax += cmax[c]; }
        for (box_t *c = b->first; c; c = c->next) if (c->kind == B_BLOCK) { minmax(L, c); smin = MAX(smin, c->minw); smax = MAX(smax, c->maxw); }
        b->minw = smin + h;
        b->maxw = smax + h;
        if (st->width.u == U_PX) { int w = rl(st->width, 0) + (st->border_box ? 0 : h); b->minw = MAX(b->minw, w); b->maxw = b->minw; }
        return;
    }
    int mn = 0, mx = 0;
    for (box_t *c = b->first; c; c = c->next) {
        minmax(L, c);
        int m = 0;
        if (c->kind != B_LINES) m = pxo(c->st->margin[1]) + pxo(c->st->margin[3]);
        mn = MAX(mn, c->minw + m);
        mx = MAX(mx, c->maxw + m);
    }
    if (st->width.u == U_PX && b->kind != B_ROW) {
        int w = rl(st->width, 0) + (st->border_box ? 0 : h);
        b->minw = b->kind == B_CELL ? MAX(w, mn + h) : w;
        b->maxw = b->kind == B_CELL ? MAX(w, mn + h) : w;
        return;
    }
    if (st->max_w.u == U_PX) { int m2 = rl(st->max_w, 0) + h; mx = MIN(mx + h, m2) - h; if (mx < mn) mx = mn; }
    b->minw = mn + h;
    b->maxw = mx + h;
    if (st->min_w.u == U_PX) { int w = rl(st->min_w, 0) + h; b->minw = MAX(b->minw, w); b->maxw = MAX(b->maxw, w); }
}

/* largura usada (caixa de borda) */
static int used_width(lay_t *L, box_t *b, int cw, bool shrink) {
    style_t *st = b->st;
    int h = hz(b);
    int avail = cw - (int)b->m[1] - (int)b->m[3];
    int w;
    if (b->kind == B_REPLACED) {
        int iw, ih;
        replaced_size(L, b, cw, &iw, &ih);
        b->iw = iw;
        b->ih = ih;
        return iw + h;
    }
    if (st->width.u == U_PX || st->width.u == U_PCT) {
        w = rl(st->width, cw);
        if (!st->border_box) w += h;
        if (b->kind == B_TABLE) { minmax(L, b); w = MAX(w, b->minw); }
    } else if (shrink || b->kind == B_TABLE) {
        minmax(L, b);
        w = MAX(b->minw, MIN(b->maxw, avail));
    } else w = avail;
    if (st->max_w.u == U_PX || st->max_w.u == U_PCT) {
        int mx = rl(st->max_w, cw) + (st->border_box ? 0 : h);
        if (w > mx) w = mx;
    }
    if (st->min_w.u == U_PX || st->min_w.u == U_PCT) {
        int mn = rl(st->min_w, cw) + (st->border_box ? 0 : h);
        if (w < mn) w = mn;
    }
    if (w < h) w = h;
    return w;
}

/* ============================================================ translação */
static void translate(box_t *b, int dx, int dy) {
    b->x += dx;
    b->y += dy;
    b->ob += dy;
    if (b->node && b->node->ay >= 0 && b->kind != B_LINES) b->node->ay += dy;
    if (b->kind == B_LINES) {
        for (int i = 0; i < b->nfrags; i++) {
            frag_t *f = &b->frags[i];
            f->x += dx; f->y += dy; f->base += dy;
            if (f->atom) translate(f->atom, dx, dy);
        }
        for (int i = 0; i < b->nlines; i++) b->lines[i].y += dy;
    }
    for (box_t *c = b->first; c; c = c->next) translate(c, dx, dy);
}

/* ============================================================ layout de linhas */
static void layout_block(lay_t *L, box_t *b, int x, int y, int w);

static int atom_layout(lay_t *L, box_t *a, int W) {
    metrics(a, W);
    int w = used_width(L, a, W, true);
    layout_block(L, a, 0, 0, w);
    return (int)a->m[3] + a->w + (int)a->m[1];
}

typedef struct {
    lay_t *L;
    box_t *lb;
    style_t *cst;
    int x0, W, y, fs;
    int lf;         /* primeiro fragmento da linha atual */
    int lx;         /* posição x atual */
    bool has;
    int sasc, sdesc, slh;
} lctx2_t;

static void finish_line(lctx2_t *C) {
    lay_t *L = C->L;
    int above = 0, below = 0;
    {
        int lead = C->slh - (C->sasc + C->sdesc);
        above = C->sasc + lead / 2;
        below = C->sdesc + lead - lead / 2;
    }
    for (int i = C->lf; i < L->nf; i++) {
        frag_t *f = &L->fr[i];
        if (f->atom) {
            box_t *a = f->atom;
            int ah = a->h + (int)a->m[0] + (int)a->m[2];
            int up = ah, dn = 0;
            if (a->st->valign == VA_MIDDLE) { int xh = (int)(f->st ? f->st->size * 0.3f : 5); up = ah / 2 + xh; dn = ah - up; if (dn < 0) { dn = 0; } }
            else if (a->st->valign == VA_TOP || a->st->valign == VA_TEXT_TOP) { up = MIN(ah, C->sasc); dn = ah - up; }
            above = MAX(above, up);
            below = MAX(below, dn);
            continue;
        }
        if (!f->t || !f->st) continue;
        style_t *st = f->st;
        font_t *ft = font_of(st);
        float sz = fsize(st);
        int A = text_ascent(ft, sz), D = text_descent(ft, sz);
        int lh = line_height_of(st, A, D);
        int lead = lh - (A + D);
        int shift = st->valign == VA_SUPER ? -(int)(st->size * 0.45f) : st->valign == VA_SUB ? (int)(st->size * 0.25f) : 0;
        above = MAX(above, A + lead / 2 - shift);
        below = MAX(below, D + lead - lead / 2 + shift);
    }
    int lh = above + below;
    int base = C->y + above;
    /* alinhamento */
    int dx = 0;
    int lw = C->lx;
    if (C->cst->align == TA_CENTER) dx = (C->W - lw) / 2;
    else if (C->cst->align == TA_RIGHT) dx = C->W - lw;
    if (dx < 0) dx = 0;
    for (int i = C->lf; i < L->nf; i++) {
        frag_t *f = &L->fr[i];
        f->x += C->x0 + dx;
        if (f->atom) {
            box_t *a = f->atom;
            int ah = a->h + (int)a->m[0] + (int)a->m[2];
            int top = base - ah;
            if (a->st->valign == VA_MIDDLE) { int xh = (int)(f->st ? f->st->size * 0.3f : 5); top = base - (ah / 2 + xh); }
            else if (a->st->valign == VA_TOP || a->st->valign == VA_TEXT_TOP) top = C->y;
            f->y = top;
            f->h = ah;
            f->base = base;
            translate(a, f->x + (int)a->m[3] - a->x, top + (int)a->m[0] - a->y);
            continue;
        }
        style_t *st = f->st ? f->st : C->cst;
        font_t *ft = font_of(st);
        float sz = fsize(st);
        int A = text_ascent(ft, sz), D = text_descent(ft, sz);
        int shift = st->valign == VA_SUPER ? -(int)(st->size * 0.45f) : st->valign == VA_SUB ? (int)(st->size * 0.25f) : 0;
        f->base = base + shift;
        f->y = f->base - A;
        f->h = A + D;
    }
    line_t *ln = push_line(L);
    if (ln) { ln->y = C->y; ln->h = lh; ln->first = C->lf - C->fs; ln->n = L->nf - C->lf; }
    C->y += lh;
    C->lf = L->nf;
    C->lx = 0;
    C->has = false;
}

static void place_frag(lctx2_t *C, item_t *it, const char *t, int len, int w) {
    frag_t *f = push_frag(C->L);
    if (!f) return;
    f->x = C->lx;
    f->w = w;
    f->t = t;
    f->len = len;
    f->st = it->st;
    f->node = it->node;
    f->bg = it->bg;
    f->atom = it->kind == IT_ATOM ? it->atom : NULL;
    C->lx += w;
    C->has = true;
}

static void layout_lines(lay_t *L, box_t *lb, int x, int y, int W, style_t *cst) {
    lb->x = x;
    lb->y = y;
    lb->w = W;
    lctx2_t C = {0};
    C.L = L;
    C.lb = lb;
    C.cst = cst;
    C.x0 = x;
    C.W = W;
    C.y = y;
    C.fs = L->nf;
    C.lf = L->nf;
    int ls = L->nl;
    font_t *sf = font_of(cst);
    C.sasc = text_ascent(sf, fsize(cst));
    C.sdesc = text_descent(sf, fsize(cst));
    C.slh = line_height_of(cst, C.sasc, C.sdesc);
    C.lx = rl(cst->indent, W);
    int pend = 0;
    int ob = y;
    item_t *its = lb->items;
    int n = lb->nitems;
    for (int i = 0; i < n;) {
        item_t *it = &its[i];
        if (it->kind == IT_SPACE) { if (C.has) { pend = it->w; } i++; continue; }
        if (it->kind == IT_BREAK) { finish_line(&C); pend = 0; i++; continue; }
        if (it->kind == IT_ANCHOR) { if (it->node) it->node->ay = C.y; i++; continue; }
        /* unidade inquebrável */
        int j = i, uw = 0;
        do {
            item_t *x2 = &its[j];
            if (x2->kind == IT_ATOM) uw += atom_layout(L, x2->atom, W);
            else uw += x2->w;
            j++;
        } while (j < n && its[j].glue && its[j].kind != IT_ATOM && its[j].kind != IT_BREAK && its[j].kind != IT_ANCHOR);
        if (C.has && C.lx + pend + uw > W) { finish_line(&C); pend = 0; }
        if (C.has && pend) {
            if (L->nf > C.lf) L->fr[L->nf - 1].sp = pend;
            C.lx += pend;
        }
        pend = 0;
        /* palavra longa demais: quebra por caracteres */
        if (!C.has && uw > W && W > 24 && j == i + 1 && it->kind == IT_WORD && it->st->ws != WS_PRE && it->st->ws != WS_NOWRAP) {
            const char *t = it->t;
            int len = it->len;
            while (len > 0) {
                int k = 0, wk = 0;
                while (k < len) {
                    int nk = k + 1;
                    while (nk < len && ((u8)t[nk] & 0xC0) == 0x80) nk++;
                    int wn = text_w(it->st, t, nk);
                    if (wn > W - C.lx && k > 0) break;
                    k = nk;
                    wk = wn;
                }
                place_frag(&C, it, t, k, wk);
                t += k;
                len -= k;
                if (len > 0) finish_line(&C);
            }
            i = j;
            continue;
        }
        for (int k = i; k < j; k++) {
            item_t *x2 = &its[k];
            if (x2->kind == IT_ATOM) {
                box_t *a = x2->atom;
                place_frag(&C, x2, NULL, 0, (int)a->m[3] + a->w + (int)a->m[1]);
            } else if (x2->kind == IT_SPACE) {
                if (L->nf > C.lf) L->fr[L->nf - 1].sp = x2->w;
                C.lx += x2->w;
            } else place_frag(&C, x2, x2->kind == IT_WORD ? x2->t : NULL, x2->kind == IT_WORD ? x2->len : 0, x2->w);
        }
        i = j;
    }
    if (C.has || L->nf > C.lf) finish_line(&C);
    lb->h = C.y - y;
    /* copia para o arena */
    int nfr = L->nf - C.fs, nln = L->nl - ls;
    if (nfr > 0) {
        lb->frags = arena_alloc(L->a, sizeof(frag_t) * (usize)nfr);
        if (lb->frags) memcpy(lb->frags, L->fr + C.fs, sizeof(frag_t) * (usize)nfr);
        lb->nfrags = lb->frags ? nfr : 0;
    }
    if (nln > 0) {
        lb->lines = arena_alloc(L->a, sizeof(line_t) * (usize)nln);
        if (lb->lines) memcpy(lb->lines, L->ln + ls, sizeof(line_t) * (usize)nln);
        lb->nlines = lb->lines ? nln : 0;
    }
    for (int i = 0; i < lb->nfrags; i++) {
        frag_t *f = &lb->frags[i];
        if (f->atom) ob = MAX(ob, f->atom->ob);
        ob = MAX(ob, f->y + f->h);
    }
    lb->ob = MAX(ob, y + lb->h);
    L->nf = C.fs;
    L->nl = ls;
}

/* ============================================================ blocos */
static int collapse_m(int a, int b) {
    if (a >= 0 && b >= 0) return MAX(a, b);
    if (a < 0 && b < 0) return MIN(a, b);
    return a + b;
}

static int layout_table(lay_t *L, box_t *t, int x, int y, int cw);

static int layout_children(lay_t *L, box_t *b, int cx, int cy, int cw, int *obp) {
    int y = cy, prev_mb = 0, ob = cy;
    bool first = true;
    for (box_t *c = b->first; c; c = c->next) {
        if (c->kind == B_LINES) {
            y += prev_mb;
            prev_mb = 0;
            layout_lines(L, c, cx, y, cw, b->st);
            y += c->h;
            ob = MAX(ob, c->ob);
            first = false;
            continue;
        }
        metrics(c, cw);
        int mt = (int)c->m[0];
        y += first ? mt : collapse_m(prev_mb, mt);
        int w = used_width(L, c, cw, c->inline_block);
        int ml = (int)c->m[3];
        int freew = cw - w - (int)c->m[3] - (int)c->m[1];
        if (freew > 0) {
            bool al = c->st->margin[3].u == U_AUTO, ar = c->st->margin[1].u == U_AUTO;
            if (al && ar) ml += freew / 2;
            else if (al) ml += freew;
            else if (b->st->center_block && (c->kind == B_TABLE || c->st->width.u != U_AUTO || c->kind == B_REPLACED)) ml += freew / 2;
            else if (b->st->align == TA_CENTER && c->kind == B_REPLACED) ml += freew / 2;
            else if (b->st->align == TA_RIGHT && c->kind == B_REPLACED) ml += freew;
        }
        layout_block(L, c, cx + ml, y, w);
        y += c->h;
        prev_mb = (int)c->m[2];
        ob = MAX(ob, c->ob);
        first = false;
    }
    y += prev_mb > 0 ? prev_mb : 0;
    *obp = ob;
    return y - cy;
}

static void layout_block(lay_t *L, box_t *b, int x, int y, int w) {
    style_t *st = b->st;
    b->x = x;
    b->y = y;
    b->w = w;
    if (b->node) b->node->ay = y;
    int cx = x + (int)(b->b[3] + b->p[3]), cy = y + (int)(b->b[0] + b->p[0]);
    int cw = w - hz(b);
    if (cw < 0) cw = 0;
    int h = 0, ob = y;
    if (b->kind == B_TABLE) { h = layout_table(L, b, cx, cy, cw); ob = cy + h; }
    else if (b->kind == B_REPLACED) { h = b->ih; ob = cy + h; }
    else h = layout_children(L, b, cx, cy, cw, &ob);
    if (st->height.u == U_PX && b->kind != B_REPLACED) {
        int eh = rl(st->height, 0);
        if (st->border_box) eh -= vt(b);
        if (eh > h) h = eh;
    }
    if (st->min_h.u == U_PX) { int mh = rl(st->min_h, 0); if (mh > h) h = mh; }
    b->h = h + vt(b);
    b->ob = MAX(ob, b->y + b->h);
}

static int layout_table(lay_t *L, box_t *t, int x, int y, int cw) {
    int cy = y;
    int ob = y;
    /* legendas */
    for (box_t *c = t->first; c; c = c->next) {
        if (c->kind != B_BLOCK) continue;
        metrics(c, cw);
        layout_block(L, c, x, cy, cw);
        cy += c->h;
    }
    gcell_t *g;
    int nr, nc;
    int n = build_grid(L, t, &g, &nr, &nc);
    if (!nc || !n) { kfree(g); return cy - y; }
    int cmin[256], cmax[256], cfix[256], colw[256], colx[257];
    float cpct[256];
    table_columns(L, t, g, n, nc, cmin, cmax, cfix, cpct);
    int sp = (int)table_spacing(t);
    int avail = cw - sp * (nc + 1);
    if (avail < 0) avail = 0;
    for (int c = 0; c < nc; c++) if (cpct[c] > 0) { int tw = (int)(cpct[c] * avail / 100.0f); cmax[c] = MAX(tw, cmin[c]); }
    int smin = 0, smax = 0;
    for (int c = 0; c < nc; c++) { smin += cmin[c]; smax += cmax[c]; }
    if (smax <= avail) {
        int extra = avail - smax;
        /* distribui sobra preferindo colunas sem largura fixa */
        int flex = 0;
        for (int c = 0; c < nc; c++) if (cfix[c] < 0 && cpct[c] <= 0) flex += cmax[c] > 0 ? cmax[c] : 1;
        int nflex = 0;
        for (int c = 0; c < nc; c++) if (cfix[c] < 0 && cpct[c] <= 0) nflex++;
        for (int c = 0; c < nc; c++) {
            colw[c] = cmax[c];
            if (extra > 0 && nflex) {
                if (cfix[c] < 0 && cpct[c] <= 0) colw[c] += (int)((i64)extra * (cmax[c] > 0 ? cmax[c] : 1) / MAX(flex, 1));
            } else if (extra > 0 && smax > 0) colw[c] += (int)((i64)extra * cmax[c] / smax);
            else if (extra > 0) colw[c] += extra / nc;
        }
    } else if (smin >= avail) {
        for (int c = 0; c < nc; c++) colw[c] = cmin[c];
    } else {
        int range = smax - smin;
        for (int c = 0; c < nc; c++) colw[c] = cmin[c] + (int)((i64)(cmax[c] - cmin[c]) * (avail - smin) / MAX(range, 1));
    }
    colx[0] = x + sp;
    for (int c = 0; c < nc; c++) colx[c + 1] = colx[c] + colw[c] + sp;
    /* linhas */
    int *rowh = kcalloc((usize)nr + 1, sizeof(int));
    int *rowy = kcalloc((usize)nr + 1, sizeof(int));
    if (!rowh || !rowy) { kfree(rowh); kfree(rowy); kfree(g); return cy - y; }
    int ry = cy + sp;
    int gi = 0;
    box_t *rbox = t->first;
    for (int r = 0; r < nr; r++) {
        while (rbox && rbox->kind != B_ROW) rbox = rbox->next;
        rowy[r] = ry;
        int rh = 0;
        if (rbox && rbox->st && rbox->st->height.u == U_PX) rh = rl(rbox->st->height, 0);
        int g0 = gi;
        while (gi < n && g[gi].row == r) {
            box_t *c = g[gi].cell;
            int col = g[gi].col, cs = MIN(c->colspan, nc - col);
            int w = colx[col + cs] - colx[col] - sp;
            metrics(c, w);
            c->has_minmax = c->has_minmax;
            layout_block(L, c, colx[col], ry, w);
            if (c->rowspan == 1) rh = MAX(rh, c->h);
            gi++;
        }
        rowh[r] = rh;
        if (rbox) { rbox->x = x + sp; rbox->y = ry; rbox->w = cw - 2 * sp; rbox->h = rh; rbox->ob = ry + rh; }
        (void)g0;
        ry += rh + sp;
        if (rbox) rbox = rbox->next;
    }
    /* altura final das células e alinhamento vertical */
    for (int i = 0; i < n; i++) {
        box_t *c = g[i].cell;
        int r = g[i].row, rs = MIN(c->rowspan, nr - r);
        int h = 0;
        for (int k = 0; k < rs; k++) h += rowh[r + k] + (k ? sp : 0);
        int content = c->h;
        if (h < content) h = content;
        int dy = 0;
        int va = c->st->valign;
        if (va == VA_MIDDLE || va == VA_BASELINE) dy = (h - content) / 2;
        else if (va == VA_BOTTOM) dy = h - content;
        if (dy > 0) for (box_t *ch = c->first; ch; ch = ch->next) translate(ch, 0, dy);
        c->h = h;
        c->ob = MAX(c->ob + dy, c->y + h);
        ob = MAX(ob, c->ob);
    }
    int total = ry - y;
    kfree(rowh);
    kfree(rowy);
    kfree(g);
    (void)ob;
    return total;
}

/* ============================================================ API */
static void gather_css(doc_t *d, sbuf_t *css, node_t *n) {
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type != N_ELEM) continue;
        if (c->tag == T_LINK && c->text) {
            /* folha externa já baixada pelo navegador (texto guardado no próprio nó) */
            const char *media = node_attr(c, "media");
            if (!(media && strstr(media, "print") && !strstr(media, "screen") && !strstr(media, "all"))) { sb_putn(css, c->text, c->len); sb_putc(css, '\n'); }
        } else if (c->tag == T_STYLE) {
            const char *media = node_attr(c, "media");
            if (media && (strstr(media, "print") && !strstr(media, "screen"))) continue;
            for (node_t *t = c->first; t; t = t->next) if (t->type == N_TEXT) { sb_putn(css, t->text, t->len); sb_putc(css, '\n'); }
        } else if (c->tag != T_SVG && c->tag != T_SCRIPT) gather_css(d, css, c);
    }
}

void doc_layout(doc_t *d, int width, int vh, float zoom) {
    if (!d) return;
    u64 t0 = timer_ticks();
    if (zoom != d->styled_zoom) d->need_style = true;
    if (d->need_style) {
        int vw = (int)(width / zoom);
        if (!d->sheet) {
            sbuf_t css = {0};
            gather_css(d, &css, d->root);
            d->sheet = css_parse(css.p ? css.p : "", css.len, vw, NULL);
            sb_free(&css);
        }
        css_style_tree(d->root, css_ua_sheet(), d->sheet, &d->arena, zoom, width, vh);
        d->styled_zoom = zoom;
        d->zoom = zoom;
        d->need_style = false;
        /* cor de fundo da tela: html, senão body */
        u32 bg = 0;
        if (d->html->st && C_A(d->html->st->bg)) bg = d->html->st->bg;
        else if (d->body->st && C_A(d->body->st->bg)) bg = d->body->st->bg;
        d->canvas_bg = C_A(bg) ? (bg | 0xFF000000u) : RGB(255, 255, 255);
    }
    arena_free(&d->larena);
    d->rootbox = NULL;
    lay_t L = {0};
    L.d = d;
    L.a = &d->larena;
    if (d->html->st && d->html->st->display != D_NONE) {
        d->rootbox = gen_block(&L, d->html);
        if (d->rootbox) {
            metrics(d->rootbox, width);
            int w = used_width(&L, d->rootbox, width, false);
            layout_block(&L, d->rootbox, (int)d->rootbox->m[3], (int)d->rootbox->m[0], w);
        }
    }
    kfree(L.it);
    kfree(L.fr);
    kfree(L.ln);
    d->layout_w = width;
    d->vh = vh;
    d->height = d->rootbox ? MAX(d->rootbox->ob, d->rootbox->y + d->rootbox->h + (int)d->rootbox->m[2]) : 0;
    d->need_layout = false;
    d->last_layout = timer_ticks();
    u64 dt = d->last_layout - t0;
    d->layout_ms = (u32)dt;
    if (dt > 50) kprintf("Horizonte: layout em %llu ms (%d nós)\n", dt, d->node_count);
}

/* ============================================================ pintura */
typedef struct {
    ui_t *u;
    surface_t *s;
    doc_t *d;
    int ox, oy;       /* deslocamento documento → superfície */
    int top, bot;     /* faixa visível em coordenadas do documento */
    node_t *hover;
} pctx_t;

static int first_baseline(box_t *b) {
    for (box_t *c = b->first; c; c = c->next) {
        if (c->kind == B_LINES && c->nfrags) {
            for (int i = 0; i < c->nfrags; i++) if (c->frags[i].t) return c->frags[i].base;
            return c->frags[0].base;
        }
        if (c->kind == B_BLOCK) { int r = first_baseline(c); if (r) return r; }
    }
    return 0;
}

static void paint_box(pctx_t *P, box_t *b);

static void paint_bg(pctx_t *P, box_t *b) {
    style_t *st = b->st;
    if (!st || st->hidden) return;
    if (b->kind == B_LINES || b->kind == B_REPLACED) {
        if (b->kind == B_LINES) return;
    }
    int x = b->x + P->ox, y = b->y + P->oy, w = b->w, h = b->h;
    if (w <= 0 || h <= 0) return;
    float r = st->radius;
    if (r > MIN(w, h) / 2.0f) r = MIN(w, h) / 2.0f;
    bool is_root = b->node && (b->node->tag == T_HTML || b->node->tag == T_BODY);
    if (st->shadow) gfx_shadow(P->s, x, y, w, h, r, 10, 46, 3);
    if (!is_root || !(C_A(st->bg) && (st->bg | 0xFF000000u) == P->d->canvas_bg)) {
        if (st->grad_bot) {
            gfx_round_rect_grad(P->s, x, y, w, h, r, st->grad_top, st->grad_bot);
        } else if (C_A(st->bg)) {
            if (r > 0.5f) gfx_round_rect(P->s, x, y, w, h, r, st->bg);
            else gfx_fill(P->s, x, y, w, h, st->bg);
        }
    }
    float *bw = b->b;
    if (bw[0] > 0 || bw[1] > 0 || bw[2] > 0 || bw[3] > 0) {
        bool uniform = bw[0] == bw[1] && bw[1] == bw[2] && bw[2] == bw[3] && st->bc[0] == st->bc[1] && st->bc[1] == st->bc[2] && st->bc[2] == st->bc[3];
        if (uniform && r > 0.5f) gfx_round_rect_stroke(P->s, x, y, w, h, r, bw[0], st->bc[0]);
        else {
            int t = (int)(bw[0] + 0.5f), rr = (int)(bw[1] + 0.5f), bb = (int)(bw[2] + 0.5f), l = (int)(bw[3] + 0.5f);
            if (bw[0] > 0 && !t) t = 1;
            if (bw[1] > 0 && !rr) rr = 1;
            if (bw[2] > 0 && !bb) bb = 1;
            if (bw[3] > 0 && !l) l = 1;
            if (t) gfx_fill(P->s, x, y, w, t, st->bc[0]);
            if (bb) gfx_fill(P->s, x, y + h - bb, w, bb, st->bc[2]);
            if (l) gfx_fill(P->s, x, y + t, l, h - t - bb, st->bc[3]);
            if (rr) gfx_fill(P->s, x + w - rr, y + t, rr, h - t - bb, st->bc[1]);
        }
    }
}

static void paint_marker(pctx_t *P, box_t *b) {
    if (!b->marker || !b->st || b->st->hidden) return;
    style_t *st = b->st;
    font_t *f = font_of(st);
    float sz = fsize(st);
    int base = first_baseline(b);
    if (!base) base = b->y + (int)(b->b[0] + b->p[0]) + text_ascent(f, sz);
    int cx = b->x + (int)(b->b[3] + b->p[3]) + P->ox;
    int by = base + P->oy;
    u32 col = st->color;
    char m = b->marker[0];
    if (m == 1 || m == 2 || m == 3) {
        float r = MAX(2.2f, sz * 0.17f);
        float mx = cx - sz * 0.75f, my = by - sz * 0.33f;
        if (m == 1) gfx_circle(P->s, mx, my, r, col);
        else if (m == 2) gfx_circle_stroke(P->s, mx, my, r, 1.1f, col);
        else gfx_fill(P->s, (int)(mx - r), (int)(my - r), (int)(r * 2), (int)(r * 2), col);
    } else {
        int tw = text_width(f, sz, b->marker);
        text_draw(P->s, f, sz, cx - tw - (int)(sz * 0.4f), by, col, b->marker);
    }
}

static void paint_replaced(pctx_t *P, box_t *b) {
    node_t *n = b->node;
    int x = b->x + (int)(b->b[3] + b->p[3]) + P->ox, y = b->y + (int)(b->b[0] + b->p[0]) + P->oy;
    int w = b->iw, h = b->ih;
    if (n->ctrl) {
        web_paint_control(P->u, P->d, n->ctrl, rect_make(x, y, w, h));
        return;
    }
    if (w <= 0 || h <= 0) return;
    img_t *im = n->img;
    if (im && im->state == IMG_OK && im->surf) {
        surface_t *src = im->surf;
        if ((src->w > w * 2 || src->h > h * 2) && w > 0 && h > 0) {
            if (!im->scaled || im->scaled->w != w || im->scaled->h != h) {
                surface_free(im->scaled);
                im->scaled = image_scale(src, w, h);
            }
            if (im->scaled) src = im->scaled;
        }
        gfx_blit_scaled(P->s, src, x, y, w, h, 255);
        return;
    }
    /* espaço reservado */
    gfx_fill(P->s, x, y, w, h, RGBA(128, 128, 128, im && im->state == IMG_ERROR ? 18 : 30));
    if (im && im->state == IMG_ERROR) {
        gfx_rect(P->s, x, y, w, h, RGBA(128, 128, 128, 90));
        const char *alt = node_attr(n, "alt");
        if (alt && *alt && w > 12 && h > 10) {
            rect_t pc = surface_push_clip(P->s, rect_make(x + 2, y, w - 4, h));
            font_t *f = font_get(FONT_REGULAR);
            float sz = MIN(12.0f * P->d->zoom, (float)h - 4);
            text_draw(P->s, f, sz, x + 5, y + (h + text_ascent(f, sz) - text_descent(f, sz)) / 2, RGB(110, 110, 110), alt);
            surface_pop_clip(P->s, pc);
        }
    }
}

static void paint_lines(pctx_t *P, box_t *lb) {
    if (!lb->nlines) return;
    /* busca binária da primeira linha visível */
    int lo = 0, hi = lb->nlines - 1, first = lb->nlines;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        line_t *ln = &lb->lines[mid];
        if (ln->y + ln->h + 40 >= P->top) { first = mid; hi = mid - 1; }
        else lo = mid + 1;
    }
    for (int li = MAX(0, first - 1); li < lb->nlines; li++) {
        line_t *ln = &lb->lines[li];
        if (ln->y > P->bot + 40) break;
        for (int k = 0; k < ln->n; k++) {
            frag_t *f = &lb->frags[ln->first + k];
            style_t *st = f->st;
            if (f->atom) {
                if (f->bg && C_A(f->bg)) gfx_fill(P->s, f->x + P->ox, f->y + P->oy, f->w + f->sp, f->h, f->bg);
                paint_box(P, f->atom);
                continue;
            }
            if (!st || st->hidden) continue;
            int x = f->x + P->ox, base = f->base + P->oy;
            if (f->bg && C_A(f->bg)) {
                font_t *ft = font_of(st);
                float sz = fsize(st);
                int A = text_ascent(ft, sz), D = text_descent(ft, sz);
                int top = f->t ? f->y : f->base - A;
                int hh = f->t ? f->h : A + D;
                int bw = f->w + (k + 1 < ln->n ? f->sp : 0);
                gfx_fill(P->s, x, top + P->oy - 1, bw, hh + 2, f->bg);
            }
            if (!f->t) continue;
            u32 col = st->color;
            float sz = fsize(st);
            if (st->mono) mono_draw(P->s, sz, x, base, col, f->t, mono_count(f->t, f->len));
            else text_draw_n(P->s, font_of(st), sz, x, base, col, f->t, f->len);
            bool hov = P->hover && f->node && f->node->link == P->hover;
            if (st->underline || hov) {
                int th = MAX(1, (int)(sz / 14 + 0.5f));
                int uw = f->w + ((k + 1 < ln->n && lb->frags[ln->first + k + 1].node == f->node) ? f->sp : 0);
                gfx_fill(P->s, x, base + MAX(1, (int)(sz * 0.12f)), uw, th, col);
            }
            if (st->strike) {
                int th = MAX(1, (int)(sz / 14 + 0.5f));
                gfx_fill(P->s, x, base - (int)(sz * 0.3f), f->w + f->sp, th, col);
            }
        }
    }
}

static void paint_box(pctx_t *P, box_t *b) {
    if (b->y > P->bot + 40) return;
    if (b->ob < P->top - 40) return;
    if (b->kind == B_LINES) { paint_lines(P, b); return; }
    paint_bg(P, b);
    if (b->kind == B_REPLACED) { if (!b->st->hidden) paint_replaced(P, b); return; }
    if (b->marker) paint_marker(P, b);
    for (box_t *c = b->first; c; c = c->next) {
        if (c->y > P->bot + 40 && c->kind != B_ROW) break;
        paint_box(P, c);
    }
}

void doc_paint(doc_t *d, ui_t *u, rect_t view, int scroll, node_t *hover) {
    surface_t *s = u->s;
    gfx_fill(s, view.x, view.y, view.w, view.h, d->canvas_bg);
    if (!d->rootbox) return;
    rect_t pc = surface_push_clip(s, view);
    pctx_t P = {u, s, d, view.x, view.y - scroll, scroll, scroll + view.h, hover};
    paint_box(&P, d->rootbox);
    surface_pop_clip(s, pc);
}

/* ============================================================ teste de clique */
static frag_t *hit_box(box_t *b, int x, int y, box_t **owner) {
    if (y < b->y - 2 || y > b->ob + 2) return NULL;
    if (b->kind == B_LINES) {
        int lo = 0, hi = b->nlines - 1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            line_t *ln = &b->lines[mid];
            if (y < ln->y) hi = mid - 1;
            else if (y >= ln->y + ln->h) lo = mid + 1;
            else {
                for (int k = 0; k < ln->n; k++) {
                    frag_t *f = &b->frags[ln->first + k];
                    if (x >= f->x && x < f->x + f->w + f->sp) {
                        if (f->atom && !(f->atom->kind == B_REPLACED)) {
                            frag_t *inner = hit_box(f->atom, x, y, owner);
                            if (inner) return inner;
                        }
                        if (owner) *owner = b;
                        return f;
                    }
                }
                break;
            }
        }
        /* fragmentos de átomos podem ser mais altos que a linha */
        for (int i = 0; i < b->nfrags; i++) {
            frag_t *f = &b->frags[i];
            if (f->atom && x >= f->x && x < f->x + f->w && y >= f->y && y < f->y + f->h) {
                if (f->atom->kind != B_REPLACED) { frag_t *inner = hit_box(f->atom, x, y, owner); if (inner) return inner; }
                if (owner) *owner = b;
                return f;
            }
        }
        return NULL;
    }
    frag_t *res = NULL;
    for (box_t *c = b->first; c; c = c->next) {
        frag_t *r = hit_box(c, x, y, owner);
        if (r) res = r;
    }
    return res;
}

frag_t *doc_hit(doc_t *d, int x, int y, box_t **owner) {
    if (!d || !d->rootbox) return NULL;
    return hit_box(d->rootbox, x, y, owner);
}

int doc_anchor_y(doc_t *d, const char *name) {
    if (!d || !name || !*name) return -1;
    node_t *n = node_by_id(d->root, name);
    if (!n) return -1;
    for (node_t *p = n; p; ) {
        if (p->ay >= 0) return p->ay;
        if (p->first) p = p->first;
        else {
            while (p && !p->next && p != n) p = p->parent;
            if (!p || p == n) break;
            p = p->next;
        }
    }
    return -1;
}
