/* mOS — motor JavaScript "Brisa": DOM, janela, eventos, temporizadores, console e armazenamento local;
 * implementa a interface pública js.h usada pelo Horizonte. */
#include "js_int.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define TRYV(x) do { if ((x) == JS_EXC) return JS_EXC; } while (0)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

bool js_enabled = true;

typedef struct { int id; u64 due; u32 interval; jsv fn; jsobj *args; bool live; } jtimer_t;
typedef struct { node_t *n; http_req_t *req; char url[512]; char *text; int len; int state; bool module; jsv job; } jscript_t; /* 0 novo, 1 baixando, 2 pronto, 3 feito */
/* requisição de rede de fetch()/XMLHttpRequest, avançada no tick */
typedef struct {
    http_req_t *req;
    jsobj *target;           /* promessa (fetch) ou objeto XHR */
    int kind;                /* 2 módulo ES (fetch e XHR ficam em web_fetch.c) */
    int state;               /* 0 em andamento (http), 1 pronto, 2 erro */
    int status;
    char url[1024];
    char ctype[96];
    char err[128];
    char *body;
    int blen;
} jnet_t;

struct js_page {
    jsrealm *realm;
    doc_t *doc;
    char url[1024];
    char origin[256];
    int vw, vh, scroll;
    node_t **wkeys;
    jsobj **wvals;
    u32 wcap, wn;
    jsobj *document, *location, *winlist, *storage, *session, *keep;
    jsobj *imgdec;              /* img.decode() pendentes: [elemento, promessa, ...] */
    bool img_scan;              /* <img> do documento já registrados para o load da janela */
    doc_t *xdocs[64];           /* documentos de DOMParser e XHR document, vivos até a página fechar */
    /* History API: estados por id (vetor JS); com anfitrião (WebView), as entradas ficam no histórico dele */
    jsobj *hstates;
    jsobj *hobj;                /* o objeto history deste reino (métodos chamados de outra janela acham a página) */
    int hcur;                   /* id do estado atual (-1 = estado nulo) */
    bool hhost, hscroll_manual;
    int hlen;                   /* tamanho do histórico informado pelo anfitrião */
    struct { char url[1024]; int sid; } hl[64];   /* histórico local (iframes, sem anfitrião) */
    int hln, hlpos;
    struct { int op, arg; char url[1024]; } hops[8];   /* 1 push, 2 replace, 3 go */
    int nhops;
    int sbox;                   /* iframe sandbox: JS_SB_* (0 = sem sandbox) */
    jsobj *fpol;                /* document.featurePolicy (Permissions Policy do atributo allow) */
    js_init_fn frame_init;      /* anfitrião: preparo das páginas dos iframes (WKUserScript em todos os quadros...) */
    void *frame_ctx;
    jsobj *xwin;                /* WindowProxy entre origens: só postMessage, location (escrita), frames, parent, top... */
    struct js_page *kids[32];   /* janelas dos iframes, em ordem do documento (window[i], window.length) */
    int nkids;
    char popup[1024];           /* window.open pedido ao anfitrião (\x01 = sem URL) */
    bool popup_pending;
    int nxdocs;
    jtimer_t *timers;
    int ntimers, ctimers, next_timer;
    jnet_t *nets;
    int nnets, cnets;
    jscript_t *scripts;
    int nscripts, cscripts, spos, mpos;
    node_t *cur_script;
    bool dcl, loaded, dirty, in_script;
    sbuf_t console;
    int errors;
    char nav[1024];
    bool nav_pending, nav_reload;
    int scroll_req;
    node_t *submit_req;
    node_t *focus;
    u64 t0;
    char cookie[1024];
    /* WKWebView (kernel/web/webview.c): window.webkit.messageHandlers.<nome>.postMessage(x) chama wk_post */
    void (*wk_post)(void *ctx, const char *name, const char *body, int len, int type);
    void *wk_ctx;
    /* iframes: página do pai e o <iframe> dono; mensagens de postMessage a entregar */
    struct js_page *parent;
    node_t *frame_el;
    struct pmsg *msgs, *msgs_tail;
    js_dialog_fn dialog;
    void *dialog_ctx;
    struct js_page *next;
};
typedef struct pmsg { struct pmsg *next; jsv val; char origin[256]; jsobj *source; } pmsg_t;   /* val: clone estruturado */


static js_page_t *cur_page(void) { return J.realm ? J.realm->page : NULL; }
static jsobj *fpol_of(js_page_t *p);
static bool policy_allows(js_page_t *p, const char *f, const char *o);
static bool sb_denies(js_page_t *p, int allow) { return p && (p->sbox & JS_SB_ON) && !(p->sbox & allow); }

/* ============================================================ console */
void js_console_log(int level, const char *msg) {
    static const char *tags[] = {"log", "info", "aviso", "erro"};
    js_page_t *p = cur_page();
    if (level < 0) { if (p) p->console.len = 0, p->console.p ? (p->console.p[0] = 0) : 0; return; }
    if (level > 3) level = 3;
    kprintf("JS %s: %s\n", tags[level], msg);
    if (!p) return;
    if (p->console.len > 48 * 1024) {
        int cut = p->console.len / 2;
        while (cut < p->console.len && p->console.p[cut] != '\n') cut++;
        memmove(p->console.p, p->console.p + cut, (usize)(p->console.len - cut));
        p->console.len -= cut;
        p->console.p[p->console.len] = 0;
    }
    if (level >= 2) sb_printf(&p->console, "[%s] ", tags[level]);
    sb_puts(&p->console, msg);
    sb_putc(&p->console, '\n');
}

void js_report_exception(void) {
    if (!js_pending()) return;
    jsv e = js_catch();
    jsstr *s = js_exc_string(e);
    char buf[512];
    int line = 0;
    if (jv_isobj(e)) {
        jsprop *lp = js_ownprop(jv_o(e), A[A_lineNumber]);
        if (lp && jv_isnum(lp->v)) line = (int)jv_n(lp->v);
    }
    if (!line) line = J.line;
    snprintf(buf, sizeof buf, "Uncaught %s (%s:%d)", s ? s->d : "?", J.srcname ? J.srcname->d : "script", line);
    js_console_log(3, buf);
    js_page_t *p = cur_page();
    if (p) p->errors++;
}

/* ============================================================ invólucros de nós */
static jsobj *wrap(js_page_t *p, node_t *n) {
    if (!n) return NULL;
    if (p->wn * 2 >= p->wcap) {
        u32 nc = p->wcap ? p->wcap * 2 : 256;
        node_t **nk = kcalloc(nc, sizeof(node_t *));
        jsobj **nv = kcalloc(nc, sizeof(jsobj *));
        if (!nk || !nv) { kfree(nk); kfree(nv); return NULL; }
        for (u32 i = 0; i < p->wcap; i++) {
            if (!p->wkeys[i]) continue;
            u32 h = (u32)(((uptr)p->wkeys[i] >> 3) * 2654435761u) & (nc - 1);
            while (nk[h]) h = (h + 1) & (nc - 1);
            nk[h] = p->wkeys[i];
            nv[h] = p->wvals[i];
        }
        kfree(p->wkeys);
        kfree(p->wvals);
        p->wkeys = nk;
        p->wvals = nv;
        p->wcap = nc;
    }
    u32 m = p->wcap - 1, h = (u32)(((uptr)n >> 3) * 2654435761u) & m;
    while (p->wkeys[h]) {
        if (p->wkeys[h] == n) return p->wvals[h];
        h = (h + 1) & m;
    }
    jsrealm *r = p->realm;
    jsobj *o = js_newobj_cls(CL_NODE, n->type == N_DOC ? r->p_doc : n->type == N_TEXT ? r->p_text :
                                      n->tag == T_CANVAS && r->p_canvas ? r->p_canvas :
                                      n->tag == T_IMG && r->p_img ? r->p_img : r->p_elem);
    o->u.host.page = p;
    o->u.host.node = n;
    o->u.host.extra = JS_UNDEF;
    p->wkeys[h] = n;
    p->wvals[h] = o;
    p->wn++;
    return o;
}

static jsobj *find_wrap(js_page_t *p, node_t *n) {
    if (!p->wcap) return NULL;
    u32 m = p->wcap - 1, h = (u32)(((uptr)n >> 3) * 2654435761u) & m;
    while (p->wkeys[h]) {
        if (p->wkeys[h] == n) return p->wvals[h];
        h = (h + 1) & m;
    }
    return NULL;
}

static jsv wrapv(js_page_t *p, node_t *n) { jsobj *o = n ? wrap(p, n) : NULL; return o ? jv_obj(o) : JS_NULL; }

static node_t *node_of(jsv v) {
    if (jv_iscls(v, CL_NODE)) return jv_o(v)->u.host.node;
    return NULL;
}

static js_page_t *pages;
static js_page_t *page_of(jsv v) {
    if (jv_isobj(v) && jv_o(v)->cls >= CL_NODE && jv_o(v)->cls <= CL_HOST) return jv_o(v)->u.host.page;
    js_page_t *c = cur_page();
    /* objeto global de outra janela (contentWindow.addEventListener...) */
    if (jv_isobj(v) && (!c || !c->realm || jv_obj(c->realm->global) != v))
        for (js_page_t *q = pages; q; q = q->next) if (q->realm && jv_obj(q->realm->global) == v) return q;
    return c;
}

void js_mark_page(js_page_t *p) {
    for (u32 i = 0; i < p->wcap; i++) if (p->wkeys[i]) gc_mark_ptr(p->wvals[i]);
    gc_mark_ptr(p->document);
    gc_mark_ptr(p->location);
    gc_mark_ptr(p->winlist);
    gc_mark_ptr(p->storage);
    gc_mark_ptr(p->session);
    gc_mark_ptr(p->keep);
    gc_mark_ptr(p->imgdec);
    gc_mark_ptr(p->hstates);
    gc_mark_ptr(p->xwin);
    gc_mark_ptr(p->hobj);
    gc_mark_ptr(p->fpol);
    for (int i = 0; i < p->ntimers; i++) { gc_mark_val(p->timers[i].fn); gc_mark_ptr(p->timers[i].args); }
    for (int i = 0; i < p->nnets; i++) gc_mark_ptr(p->nets[i].target);
    for (int i = 0; i < p->nscripts; i++) gc_mark_val(p->scripts[i].job);
    for (pmsg_t *m = p->msgs; m; m = m->next) { gc_mark_ptr(m->source); gc_mark_val(m->val); }
}

void js_mark_pages(void) { for (js_page_t *p = pages; p; p = p->next) js_mark_page(p); }

/* ============================================================ mutações */
static void collapse(const char *s, int n, char *out, int cap) {
    int k = 0;
    bool sp = false;
    for (int i = 0; i < n && k < cap - 2; i++) {
        if (isspace((u8)s[i])) { sp = k > 0; continue; }
        if (sp) { out[k++] = ' '; sp = false; }
        out[k++] = s[i];
    }
    out[k] = 0;
}

static void page_dirty(js_page_t *p, node_t *n) {
    doc_t *d = p->doc;
    d->need_style = true;
    d->need_layout = true;
    p->dirty = true;
    for (node_t *a = n; a; a = a->parent) {
        if (a->type == N_ELEM && a->tag == T_SVG && a->ns) a->svgdirty = 1;   /* SVG inline: redesenha da subárvore */
        if (a->type == N_ELEM && (a->tag == T_STYLE || a->tag == T_LINK)) { if (d->sheet) { css_free(d->sheet); d->sheet = NULL; } }
        if (a->type == N_ELEM && a->tag == T_BUTTON && a->ctrl) {
            sbuf_t t = {0};
            node_text_content(a, &t);
            collapse(t.p ? t.p : "", t.len, a->ctrl->label, sizeof a->ctrl->label);
            sb_free(&t);
        }
        if (a->type == N_ELEM && a->tag == T_TITLE) {
            sbuf_t t = {0};
            node_text_content(a, &t);
            collapse(t.p ? t.p : "", t.len, d->title, sizeof d->title);
            sb_free(&t);
        }
    }
}

static bool contains_style(node_t *n) {
    if (n->type == N_ELEM && (n->tag == T_STYLE || n->tag == T_LINK)) return true;
    for (node_t *c = n->first; c; c = c->next) if (contains_style(c)) return true;
    return false;
}

static void detach(node_t *n) {
    node_t *par = n->parent;
    if (!par) return;
    js_mo_child(par, NULL, n, n->prev, n->next);
    if (n->prev) n->prev->next = n->next; else par->first = n->next;
    if (n->next) n->next->prev = n->prev; else par->last = n->prev;
    n->parent = n->prev = n->next = NULL;
}

static void insert_before(node_t *par, node_t *n, node_t *ref) {
    detach(n);
    n->parent = par;
    if (!ref) {
        n->prev = par->last;
        n->next = NULL;
        if (par->last) par->last->next = n; else par->first = n;
        par->last = n;
    } else {
        n->next = ref;
        n->prev = ref->prev;
        if (ref->prev) ref->prev->next = n; else par->first = n;
        ref->prev = n;
    }
    js_mo_child(par, n, NULL, n->prev, n->next);
}

static node_t *new_node(doc_t *d, int type) {
    node_t *n = arena_alloc(&d->arena, sizeof(node_t));
    if (!n) return NULL;
    n->type = (u8)type;
    n->ay = -1;
    n->index = d->node_count++;
    return n;
}

static node_t *new_elem(doc_t *d, const char *name, int len) {
    node_t *n = new_node(d, N_ELEM);
    if (!n) return NULL;
    char low[64];
    int k = 0;
    for (int i = 0; i < len && k < 63; i++) low[k++] = (char)tolower((u8)name[i]);
    low[k] = 0;
    n->tag = (u16)tag_lookup(low);
    const char *tn = html_tag_name(n->tag);
    n->name = tn ? tn : arena_strndup(&d->arena, low, k);
    return n;
}

static node_t *new_text(doc_t *d, const char *s, int len) {
    node_t *n = new_node(d, N_TEXT);
    if (!n) return NULL;
    n->text = arena_strndup(&d->arena, s, len);
    n->len = len;
    return n;
}

/* <img>: escolhe a imagem de novo; URLs blob: vêm direto do registro da página */
static void img_refresh(js_page_t *p, node_t *n) {
    img_update_node(p->doc, n, p->vw, 1.0f);
    img_t *im = n->img;
    if (!im || im->state != IMG_PENDING || strncmp(im->url, "blob:", 5)) return;
    const u8 *b;
    u32 len;
    if (js_blob_bytes(js_blob_from_url(im->url), &b, &len, NULL) && len) img_set_data(im, b, len);
    im->state = im->surf ? IMG_OK : IMG_ERROR;
}

static void set_attr0(js_page_t *p, node_t *n, const char *name, const char *val, int vlen) {
    doc_t *d = p->doc;
    if (n->tag == T_IFRAME && (!strcmp(name, "src") || !strcmp(name, "srcdoc"))) frame_navigate(n, NULL);
    for (int i = 0; i < n->nattrs; i++) {
        if (!strcmp(n->attrs[i].name, name)) {
            if ((int)strlen(n->attrs[i].value) == vlen && !memcmp(n->attrs[i].value, val, (usize)vlen)) { js_mo_attr(n, name, n->attrs[i].value); return; }
            js_mo_attr(n, name, n->attrs[i].value);
            n->attrs[i].value = arena_strndup(&d->arena, val, vlen);
            page_dirty(p, n);
            return;
        }
    }
    if (n->nattrs == n->acap) {
        int nc = n->acap ? n->acap * 2 : 4;
        attr_t *na = arena_alloc(&d->arena, sizeof(attr_t) * (usize)nc);
        if (!na) return;
        if (n->attrs) memcpy(na, n->attrs, sizeof(attr_t) * (usize)n->nattrs);
        n->attrs = na;
        n->acap = nc;
    }
    js_mo_attr(n, name, NULL);
    n->attrs[n->nattrs].name = arena_strndup(&d->arena, name, (int)strlen(name));
    n->attrs[n->nattrs].value = arena_strndup(&d->arena, val, vlen);
    n->nattrs++;
    page_dirty(p, n);
}

static void set_attr(js_page_t *p, node_t *n, const char *name, const char *val, int vlen) {
    set_attr0(p, n, name, val, vlen);
    /* <canvas>: width/height recriam o bitmap (mesmo com o mesmo valor) */
    if (n->tag == T_CANVAS && (!strcmp(name, "width") || !strcmp(name, "height"))) js_canvas_attr_changed(p, n);
    /* <img>: src/srcset trocados pedem a imagem já, mesmo fora do documento */
    if (n->tag == T_IMG && (!strcmp(name, "src") || !strcmp(name, "srcset") || !strcmp(name, "sizes"))) img_refresh(p, n);
}

static void remove_attr(js_page_t *p, node_t *n, const char *name) {
    for (int i = 0; i < n->nattrs; i++) {
        if (!strcmp(n->attrs[i].name, name)) {
            js_mo_attr(n, name, n->attrs[i].value);
            memmove(&n->attrs[i], &n->attrs[i + 1], sizeof(attr_t) * (usize)(n->nattrs - i - 1));
            n->nattrs--;
            page_dirty(p, n);
            if (n->tag == T_CANVAS && (!strcmp(name, "width") || !strcmp(name, "height"))) js_canvas_attr_changed(p, n);
            if (n->tag == T_IMG && (!strcmp(name, "src") || !strcmp(name, "srcset"))) img_refresh(p, n);
            return;
        }
    }
}

static void remove_children(js_page_t *p, node_t *n) {
    bool st = contains_style(n);
    while (n->first) detach(n->first);
    if (st && p->doc->sheet) { css_free(p->doc->sheet); p->doc->sheet = NULL; }
    page_dirty(p, n);
}

static void set_text(js_page_t *p, node_t *n, jsstr *s) {
    if (n->type == N_TEXT) {
        js_mo_data(n, n->text ? n->text : "");
        n->text = arena_strndup(&p->doc->arena, s->d, (int)s->len);
        n->len = (int)s->len;
        page_dirty(p, n->parent ? n->parent : n);
        return;
    }
    js_mo_batch(1);   /* um registro só, como nos navegadores */
    remove_children(p, n);
    if (s->len) {
        node_t *t = new_text(p->doc, s->d, (int)s->len);
        if (t) insert_before(n, t, NULL);
    }
    js_mo_batch(-1);
    page_dirty(p, n);
}

static void queue_script(js_page_t *p, node_t *n);

static void after_insert(js_page_t *p, node_t *n) {
    if (contains_style(n) && p->doc->sheet) { css_free(p->doc->sheet); p->doc->sheet = NULL; }
    if (n->type == N_ELEM && n->tag == T_SCRIPT) queue_script(p, n);
    page_dirty(p, n);
}

static bool is_connected(js_page_t *p, node_t *n) {
    while (n->parent) n = n->parent;
    return n == p->doc->root;
}

/* ============================================================ serialização */
static bool is_void_tag(int t) {
    return t == T_AREA || t == T_BASE || t == T_BR || t == T_COL || t == T_EMBED || t == T_HR || t == T_IMG || t == T_INPUT ||
           t == T_LINK || t == T_META || t == T_PARAM || t == T_SOURCE || t == T_TRACK || t == T_WBR;
}

static void esc(sbuf_t *sb, const char *s, int n, bool attr) {
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c == '&') sb_puts(sb, "&amp;");
        else if (c == '<' && !attr) sb_puts(sb, "&lt;");
        else if (c == '>' && !attr) sb_puts(sb, "&gt;");
        else if (c == '"' && attr) sb_puts(sb, "&quot;");
        else sb_putc(sb, c);
    }
}

static void serialize(sbuf_t *sb, node_t *n, bool outer) {
    if (n->type == N_TEXT) {
        bool raw = n->parent && n->parent->type == N_ELEM && (n->parent->tag == T_SCRIPT || n->parent->tag == T_STYLE);
        if (raw) sb_putn(sb, n->text, n->len); else esc(sb, n->text, n->len, false);
        return;
    }
    if (outer && n->type == N_ELEM) {
        sb_printf(sb, "<%s", n->name);
        for (int i = 0; i < n->nattrs; i++) {
            sb_printf(sb, " %s=\"", n->attrs[i].name);
            esc(sb, n->attrs[i].value, (int)strlen(n->attrs[i].value), true);
            sb_putc(sb, '"');
        }
        sb_putc(sb, '>');
        if (is_void_tag(n->tag)) return;
    }
    for (node_t *c = n->first; c; c = c->next) serialize(sb, c, true);
    if (outer && n->type == N_ELEM) sb_printf(sb, "</%s>", n->name);
}

/* ============================================================ seletores CSS (subconjunto) */
typedef struct {
    char tag[32];
    char id[64];
    char cls[4][48];
    int ncls;
    struct { char name[32]; char val[64]; char op; } at[3];
    int nat;
    int pseudo[4];      /* 1 first-child, 2 last-child, 3 checked, 4 disabled, 5 empty, 6 nth-child, 7 only-child */
    int nth_a[4], nth_b[4];
    int npseudo;
    int notsel;         /* índice do seletor negado (-1) */
} csimple;

typedef struct { csimple parts[12]; char comb[12]; int n; } cselector;   /* comb[i]: combinador antes da parte i */

static csimple not_pool[8];
static int not_used;

static const char *parse_ident(const char *s, char *out, int cap) {
    int k = 0;
    while (*s && (isalnum((u8)*s) || *s == '-' || *s == '_' || (u8)*s >= 0x80 || *s == '\\')) {
        if (*s == '\\' && s[1]) s++;
        if (k < cap - 1) out[k++] = *s;
        s++;
    }
    out[k] = 0;
    return s;
}

static const char *parse_simple(const char *s, csimple *c, bool *ok) {
    memset(c, 0, sizeof *c);
    c->notsel = -1;
    *ok = true;
    bool any = false;
    if (*s == '*') { s++; any = true; }
    else if (isalpha((u8)*s)) { s = parse_ident(s, c->tag, sizeof c->tag); for (char *q = c->tag; *q; q++) *q = (char)tolower((u8)*q); any = true; }
    for (;;) {
        if (*s == '#') { s = parse_ident(s + 1, c->id, sizeof c->id); any = true; }
        else if (*s == '.') { if (c->ncls < 4) s = parse_ident(s + 1, c->cls[c->ncls++], 48); else s = parse_ident(s + 1, c->cls[3], 48); any = true; }
        else if (*s == '[') {
            s++;
            while (*s == ' ') s++;
            if (c->nat >= 3) { *ok = false; return s; }
            typeof(c->at[0]) *a = &c->at[c->nat++];
            s = parse_ident(s, a->name, sizeof a->name);
            for (char *q = a->name; *q; q++) *q = (char)tolower((u8)*q);
            while (*s == ' ') s++;
            a->op = 0;
            if (*s == '=') { a->op = '='; s++; }
            else if (*s && s[1] == '=' && strchr("^$*~|", *s)) { a->op = *s; s += 2; }
            if (a->op) {
                while (*s == ' ') s++;
                int k = 0;
                if (*s == '"' || *s == '\'') {
                    char q = *s++;
                    while (*s && *s != q) { if (k < 63) a->val[k++] = *s; s++; }
                    if (*s) s++;
                } else while (*s && *s != ']' && *s != ' ') { if (k < 63) a->val[k++] = *s; s++; }
                a->val[k] = 0;
            }
            while (*s == ' ') s++;
            if (*s != ']') { *ok = false; return s; }
            s++;
            any = true;
        } else if (*s == ':') {
            s++;
            if (*s == ':') s++;
            char nm[32];
            s = parse_ident(s, nm, sizeof nm);
            int ps = 0, a = 0, b = 0;
            if (!strcmp(nm, "first-child")) ps = 1;
            else if (!strcmp(nm, "last-child")) ps = 2;
            else if (!strcmp(nm, "checked")) ps = 3;
            else if (!strcmp(nm, "disabled")) ps = 4;
            else if (!strcmp(nm, "empty")) ps = 5;
            else if (!strcmp(nm, "only-child")) ps = 7;
            else if (!strcmp(nm, "nth-child") && *s == '(') {
                s++;
                char arg[32];
                int k = 0;
                while (*s && *s != ')') { if (*s != ' ' && k < 31) arg[k++] = *s; s++; }
                arg[k] = 0;
                if (*s) s++;
                if (!strcmp(arg, "odd")) { a = 2; b = 1; }
                else if (!strcmp(arg, "even")) { a = 2; b = 0; }
                else {
                    char *nn = strchr(arg, 'n');
                    if (nn) {
                        *nn = 0;
                        a = !arg[0] ? 1 : !strcmp(arg, "-") ? -1 : atoi(arg);
                        b = nn[1] ? atoi(nn + 1) : 0;
                    } else { a = 0; b = atoi(arg); }
                }
                ps = 6;
            } else if (!strcmp(nm, "not") && *s == '(') {
                s++;
                if (not_used >= 8) { *ok = false; return s; }
                int idx = not_used++;
                bool ok2;
                s = parse_simple(s, &not_pool[idx], &ok2);
                if (!ok2 || *s != ')') { *ok = false; return s; }
                s++;
                c->notsel = idx;
                any = true;
                continue;
            } else if (!strcmp(nm, "hover") || !strcmp(nm, "focus") || !strcmp(nm, "active") || !strcmp(nm, "visited") || !strcmp(nm, "link")) ps = 99;
            else { *ok = false; return s; }
            if (c->npseudo < 4) { c->pseudo[c->npseudo] = ps; c->nth_a[c->npseudo] = a; c->nth_b[c->npseudo] = b; c->npseudo++; }
            any = true;
        } else break;
    }
    if (!any) *ok = false;
    return s;
}

static bool has_class(node_t *n, const char *c) {
    const char *v = node_attr(n, "class");
    if (!v) return false;
    int l = (int)strlen(c);
    while (*v) {
        while (*v && isspace((u8)*v)) v++;
        const char *st = v;
        while (*v && !isspace((u8)*v)) v++;
        if (v - st == l && !strncmp(st, c, (usize)l)) return true;
    }
    return false;
}

static node_t *prev_el(node_t *n) { for (n = n->prev; n; n = n->prev) if (n->type == N_ELEM) return n; return NULL; }
static node_t *next_el(node_t *n) { for (n = n->next; n; n = n->next) if (n->type == N_ELEM) return n; return NULL; }

static bool match_simple(csimple *c, node_t *n) {
    if (n->type != N_ELEM) return false;
    if (c->tag[0] && strcmp(c->tag, n->name)) return false;
    if (c->id[0]) { const char *v = node_attr(n, "id"); if (!v || strcmp(v, c->id)) return false; }
    for (int i = 0; i < c->ncls; i++) if (!has_class(n, c->cls[i])) return false;
    for (int i = 0; i < c->nat; i++) {
        const char *v = node_attr(n, c->at[i].name);
        if (!v) return false;
        const char *w = c->at[i].val;
        int vl = (int)strlen(v), wl = (int)strlen(w);
        switch (c->at[i].op) {
        case '=': if (strcmp(v, w)) return false; break;
        case '^': if (strncmp(v, w, (usize)wl)) return false; break;
        case '$': if (vl < wl || strcmp(v + vl - wl, w)) return false; break;
        case '*': if (!strstr(v, w)) return false; break;
        case '~': {
            bool f = false;
            const char *q = v;
            while (*q) {
                while (*q == ' ') q++;
                const char *st = q;
                while (*q && *q != ' ') q++;
                if (q - st == wl && !strncmp(st, w, (usize)wl)) f = true;
            }
            if (!f) return false;
            break;
        }
        case '|': if (strcmp(v, w) && !(vl > wl && !strncmp(v, w, (usize)wl) && v[wl] == '-')) return false; break;
        }
    }
    for (int i = 0; i < c->npseudo; i++) {
        switch (c->pseudo[i]) {
        case 1: if (prev_el(n)) return false; break;
        case 2: if (next_el(n)) return false; break;
        case 7: if (prev_el(n) || next_el(n)) return false; break;
        case 3: if (!(n->ctrl ? n->ctrl->checked : node_attr(n, "checked") != NULL) && !(n->tag == T_OPTION && node_attr(n, "selected"))) return false; break;
        case 4: if (!node_attr(n, "disabled")) return false; break;
        case 5: if (n->first) return false; break;
        case 6: {
            int idx = 1;
            for (node_t *q = prev_el(n); q; q = prev_el(q)) idx++;
            int a = c->nth_a[i], b = c->nth_b[i];
            if (a == 0) { if (idx != b) return false; }
            else { int k = idx - b; if (k % a != 0 || k / a < 0) return false; }
            break;
        }
        case 99: return false;
        }
    }
    if (c->notsel >= 0 && match_simple(&not_pool[c->notsel], n)) return false;
    return true;
}

static bool match_from(cselector *s, int i, node_t *n) {
    if (!match_simple(&s->parts[i], n)) return false;
    if (i == 0) return true;
    char comb = s->comb[i];
    if (comb == '>') return n->parent && match_from(s, i - 1, n->parent);
    if (comb == '+') { node_t *p = prev_el(n); return p && match_from(s, i - 1, p); }
    if (comb == '~') { for (node_t *p = prev_el(n); p; p = prev_el(p)) if (match_from(s, i - 1, p)) return true; return false; }
    for (node_t *p = n->parent; p; p = p->parent) if (match_from(s, i - 1, p)) return true;
    return false;
}

typedef struct { cselector *sel; int n; } cselist;

static int parse_selectors(const char *s, cselector *out, int max) {
    not_used = 0;
    int n = 0;
    while (*s) {
        while (*s == ' ' || *s == '\t' || *s == '\n') s++;
        if (n >= max) return -1;
        cselector *cs = &out[n];
        cs->n = 0;
        char comb = ' ';
        for (;;) {
            while (*s == ' ' || *s == '\t' || *s == '\n') { s++; }
            if (*s == '>' || *s == '+' || *s == '~') { comb = *s++; while (*s == ' ') s++; }
            if (!*s || *s == ',') break;
            if (cs->n >= 12) return -1;
            bool ok;
            cs->comb[cs->n] = comb;
            s = parse_simple(s, &cs->parts[cs->n], &ok);
            if (!ok) return -1;
            cs->n++;
            comb = ' ';
            if (*s && *s != ' ' && *s != ',' && *s != '>' && *s != '+' && *s != '~' && *s != '\t' && *s != '\n') return -1;
        }
        if (!cs->n) return -1;
        n++;
        if (*s == ',') s++;
    }
    return n;
}

static bool matches_any(cselector *sel, int ns, node_t *n) {
    for (int i = 0; i < ns; i++) if (match_from(&sel[i], sel[i].n - 1, n)) return true;
    return false;
}

static node_t *qs_walk(node_t *root, cselector *sel, int ns, jsobj *all, js_page_t *p) {
    for (node_t *c = root->first; c; c = c->next) {
        if (c->type != N_ELEM) continue;
        if (matches_any(sel, ns, c)) {
            if (!all) return c;
            js_arr_push(all, jv_obj(wrap(p, c)));
        }
        node_t *r = qs_walk(c, sel, ns, all, p);
        if (r && !all) return r;
    }
    return NULL;
}

static jsv query(js_page_t *p, node_t *root, jsv selv, bool all) {
    jsstr *s = js_tostr(selv);
    if (!s) return JS_EXC;
    static cselector sel[16];
    int ns = parse_selectors(s->d, sel, 16);
    if (ns <= 0) return js_throw_err(E_SYNTAX, "'%s' não é um seletor válido (ou não é suportado)", s->d);
    if (all) {
        jsobj *a = js_newarr(0);
        qs_walk(root, sel, ns, a, p);
        return jv_obj(a);
    }
    return wrapv(p, qs_walk(root, sel, ns, NULL, p));
}

/* ============================================================ geometria */
static void page_navigate(js_page_t *p, const char *rel, bool replace);
typedef struct { int x0, y0, x1, y1; bool any; } rect_acc;

static void acc_add(rect_acc *r, int x, int y, int w, int h) {
    if (!r->any) { r->x0 = x; r->y0 = y; r->x1 = x + w; r->y1 = y + h; r->any = true; return; }
    if (x < r->x0) r->x0 = x;
    if (y < r->y0) r->y0 = y;
    if (x + w > r->x1) r->x1 = x + w;
    if (y + h > r->y1) r->y1 = y + h;
}

static bool is_desc(node_t *a, node_t *n) { for (; a; a = a->parent) if (a == n) return true; return false; }

static void box_rect(box_t *b, node_t *n, rect_acc *r, int depth) {
    if (!b || depth > 300) return;
    if (b->node == n) { acc_add(r, b->x, b->y, b->w, b->h); return; }
    if (b->kind == B_LINES) {
        for (int i = 0; i < b->nfrags; i++) {
            frag_t *f = &b->frags[i];
            if (f->atom) box_rect(f->atom, n, r, depth + 1);
            else if (f->node && is_desc(f->node, n)) acc_add(r, f->x, f->y, f->w, f->h);
        }
    }
    for (box_t *c = b->first; c; c = c->next) box_rect(c, n, r, depth + 1);
}

static void ensure_layout(js_page_t *p) {
    doc_t *d = p->doc;
    if ((d->need_layout || d->need_style) && d->layout_w > 0) doc_layout(d, d->layout_w, d->vh, d->zoom);
}

static rect_acc node_rect(js_page_t *p, node_t *n) {
    rect_acc r = {0};
    ensure_layout(p);
    box_rect(p->doc->rootbox, n, &r, 0);
    return r;
}

static node_t *hit(box_t *b, int x, int y, int depth) {
    if (!b || depth > 300) return NULL;
    node_t *best = NULL;
    if (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h && b->node) best = b->node;
    if (b->kind == B_LINES) {
        for (int i = 0; i < b->nfrags; i++) {
            frag_t *f = &b->frags[i];
            if (x < f->x || x >= f->x + f->w || y < f->y || y >= f->y + f->h) continue;
            if (f->atom) { node_t *r = hit(f->atom, x, y, depth + 1); if (r) best = r; }
            else if (f->node) best = f->node;
        }
    }
    for (box_t *c = b->first; c; c = c->next) {
        node_t *r = hit(c, x, y, depth + 1);
        if (r) best = r;
    }
    return best;
}

node_t *js_hit_node(doc_t *d, int x, int y) {
    if (!d || !d->rootbox) return d ? d->body : NULL;
    node_t *n = hit(d->rootbox, x, y, 0);
    while (n && n->type != N_ELEM) n = n->parent;
    return n ? n : d->body;
}

/* ============================================================ estilo inline */
static void camel_to_kebab(jsstr *k, char *out, int cap) {
    int j = 0;
    for (u32 i = 0; i < k->len && j < cap - 2; i++) {
        char c = k->d[i];
        if (c >= 'A' && c <= 'Z') { out[j++] = '-'; out[j++] = (char)(c + 32); }
        else out[j++] = c;
    }
    out[j] = 0;
    if (!strncmp(out, "webkit-", 7) || !strncmp(out, "moz-", 4) || !strncmp(out, "ms-", 3)) {
        memmove(out + 1, out, (usize)j + 1);
        out[0] = '-';
    }
    if (!strcmp(out, "css-float")) strcpy(out, "float");
}

/* procura "prop: valor" no atributo style; devolve início/fim do valor */
static bool style_find(const char *s, const char *prop, int *vs, int *ve, int *ds, int *de) {
    int i = 0, n = (int)strlen(s), pl = (int)strlen(prop);
    while (i < n) {
        int st = i;
        while (i < n && isspace((u8)s[i])) i++;
        int ns = i;
        while (i < n && s[i] != ':' && s[i] != ';') i++;
        int ne = i;
        while (ne > ns && isspace((u8)s[ne - 1])) ne--;
        if (i >= n || s[i] == ';') { i++; continue; }
        i++;
        int a = i;
        int paren = 0;
        while (i < n && (s[i] != ';' || paren)) { if (s[i] == '(') paren++; else if (s[i] == ')') paren--; i++; }
        int b = i;
        if (i < n) i++;
        while (a < b && isspace((u8)s[a])) a++;
        while (b > a && isspace((u8)s[b - 1])) b--;
        if (ne - ns == pl && !strncasecmp(s + ns, prop, (usize)pl)) { *vs = a; *ve = b; *ds = st; *de = i; return true; }
    }
    return false;
}

static jsv style_get(node_t *n, const char *prop) {
    const char *s = node_attr(n, "style");
    int vs, ve, ds, de;
    if (!s || !style_find(s, prop, &vs, &ve, &ds, &de)) return jv_str(A[A_empty]);
    int e = ve;
    const char *imp = NULL;
    for (int i = vs; i + 9 < ve + 1; i++) if (s[i] == '!') { imp = s + i; break; }
    if (imp) { e = (int)(imp - s); while (e > vs && isspace((u8)s[e - 1])) e--; }
    return jv_str(js_strn(s + vs, e - vs));
}

static void style_set(js_page_t *p, node_t *n, const char *prop, jsstr *val) {
    const char *s = node_attr(n, "style");
    sbuf_t sb = {0};
    int vs, ve, ds, de;
    if (s && style_find(s, prop, &vs, &ve, &ds, &de)) {
        sb_putn(&sb, s, ds);
        if (val->len) { sb_printf(&sb, "%s%s: ", ds && sb.p && sb.p[sb.len - 1] != ' ' ? " " : "", prop); sb_putn(&sb, val->d, (int)val->len); sb_puts(&sb, ";"); }
        sb_puts(&sb, s + de);
    } else {
        if (!val->len) return;
        if (s) {
            sb_puts(&sb, s);
            int l = (int)strlen(s);
            while (l > 0 && isspace((u8)s[l - 1])) l--;
            if (l > 0 && s[l - 1] != ';') sb_putc(&sb, ';');
            if (l > 0) sb_putc(&sb, ' ');
        }
        sb_printf(&sb, "%s: ", prop);
        sb_putn(&sb, val->d, (int)val->len);
        sb_putc(&sb, ';');
    }
    set_attr(p, n, "style", sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
}

static jsobj *mkhost(js_page_t *p, int cls, jsobj *proto, node_t *n) {
    jsobj *o = js_newobj_cls(cls, proto);
    o->u.host.page = p;
    o->u.host.node = n;
    o->u.host.extra = JS_UNDEF;
    return o;
}

/* ============================================================ armazenamento local (em memória, por origem) */
typedef struct sitem { char *k, *v; struct sitem *next; } sitem_t;
typedef struct sarea { char origin[260]; sitem_t *items; int n; struct sarea *next; } sarea_t;
static sarea_t *areas;

static sarea_t *area_for(const char *origin) {
    for (sarea_t *a = areas; a; a = a->next) if (!strcmp(a->origin, origin)) return a;
    sarea_t *a = kcalloc(1, sizeof(sarea_t));
    if (!a) return NULL;
    strncpy(a->origin, origin, sizeof a->origin - 1);
    a->next = areas;
    areas = a;
    return a;
}

static sarea_t *storage_area(jsobj *o) {
    js_page_t *p = o->u.host.page;
    char key[300];
    snprintf(key, sizeof key, "%s%s", o->u.host.extra == JS_TRUE ? "sessao:" : "", p ? p->origin : "");
    return area_for(key);
}

static sitem_t *sfind(sarea_t *a, const char *k) {
    for (sitem_t *i = a->items; i; i = i->next) if (!strcmp(i->k, k)) return i;
    return NULL;
}

static void sset(sarea_t *a, const char *k, const char *v) {
    sitem_t *i = sfind(a, k);
    if (i) { kfree(i->v); i->v = strdup(v); return; }
    i = kcalloc(1, sizeof(sitem_t));
    if (!i) return;
    i->k = strdup(k);
    i->v = strdup(v);
    sitem_t **pp = &a->items;
    while (*pp) pp = &(*pp)->next;
    *pp = i;
    a->n++;
}

static void sdel(sarea_t *a, const char *k) {
    for (sitem_t **pp = &a->items; *pp; pp = &(*pp)->next) {
        if (!strcmp((*pp)->k, k)) {
            sitem_t *i = *pp;
            *pp = i->next;
            kfree(i->k); kfree(i->v); kfree(i);
            a->n--;
            return;
        }
    }
}

NATIVE(stor_op) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_STORAGE)) return js_throw_err(E_TYPE, "Storage: objeto incompatível");
    sarea_t *a = storage_area(jv_o(thisv));
    if (!a) return JS_UNDEF;
    jsstr *k = NULL;
    int op = self->u.nat.magic;
    if (op <= 2) { k = js_tostr(ARG(0)); if (!k) return JS_EXC; }
    switch (op) {
    case 0: { sitem_t *i = sfind(a, k->d); return i ? jv_str(js_strz(i->v)) : JS_NULL; }
    case 1: { jsstr *v = js_tostr(ARG(1)); if (!v) return JS_EXC; sset(a, k->d, v->d); return JS_UNDEF; }
    case 2: sdel(a, k->d); return JS_UNDEF;
    case 3: while (a->items) sdel(a, a->items->k); return JS_UNDEF;
    case 4: {
        double idx = js_tointeger(ARG(0));
        int n = 0;
        for (sitem_t *i = a->items; i; i = i->next, n++) if (n == (int)idx) return jv_str(js_strz(i->k));
        return JS_NULL;
    }
    }
    return JS_UNDEF;
}

/* ============================================================ eventos */
enum { LF_CAPTURE = 1, LF_ONCE = 2, LF_PROP = 4, LF_ATTR = 8 };

static jsobj *listeners_of(js_page_t *p, jsv target, bool create) {
    if (jv_iscls(target, CL_NODE)) {
        jsobj *o = jv_o(target);
        if (o->u.host.extra == JS_UNDEF) { if (!create) return NULL; o->u.host.extra = jv_obj(js_newarr(0)); }
        return jv_o(o->u.host.extra);
    }
    if (!p->winlist && create) p->winlist = js_newarr(0);
    return p->winlist;
}

static void add_listener(jsobj *l, jsstr *type, jsv fn, int fl) {
    for (u32 i = 0; i + 3 < l->alen + 1 && i < l->alen; i += 4)
        if (js_str_eq(jv_s(l->arr[i]), type) && l->arr[i + 1] == fn && ((int)jv_n(l->arr[i + 2]) & LF_CAPTURE) == (fl & LF_CAPTURE)) return;
    js_arr_push(l, jv_str(type));
    js_arr_push(l, fn);
    js_arr_push(l, jv_int(fl));
    js_arr_push(l, JS_UNDEF);
}

static void remove_listener(jsobj *l, jsstr *type, jsv fn, int capflag) {
    for (u32 i = 0; i < l->alen; i += 4) {
        if (js_str_eq(jv_s(l->arr[i]), type) && l->arr[i + 1] == fn && ((int)jv_n(l->arr[i + 2]) & (LF_CAPTURE | LF_PROP)) == capflag) {
            memmove(l->arr + i, l->arr + i + 4, sizeof(jsv) * (l->alen - i - 4));
            l->alen -= 4;
            return;
        }
    }
}

static void set_prop_handler(js_page_t *p, jsv target, jsstr *type, jsv fn) {
    jsobj *l = listeners_of(p, target, true);
    for (u32 i = 0; i < l->alen; i += 4) {
        if (((int)jv_n(l->arr[i + 2]) & LF_PROP) && js_str_eq(jv_s(l->arr[i]), type)) {
            if (js_iscallable(fn)) l->arr[i + 1] = fn;
            else { memmove(l->arr + i, l->arr + i + 4, sizeof(jsv) * (l->alen - i - 4)); l->alen -= 4; }
            return;
        }
    }
    if (js_iscallable(fn)) add_listener(l, type, fn, LF_PROP);
}

static jsv get_prop_handler(js_page_t *p, jsv target, jsstr *type) {
    jsobj *l = listeners_of(p, target, false);
    if (!l) return JS_NULL;
    for (u32 i = 0; i < l->alen; i += 4)
        if (((int)jv_n(l->arr[i + 2]) & LF_PROP) && js_str_eq(jv_s(l->arr[i]), type)) return l->arr[i + 1];
    return JS_NULL;
}

static bool ev_flag(jsobj *ev, const char *name) {
    jsprop *pr = js_ownprop(ev, js_intern(name, -1));
    return pr && js_truthy(pr->v);
}

/* compila (com cache) o atributo on<tipo> de um elemento */
static jsv attr_handler(js_page_t *p, node_t *n, jsstr *type) {
    char an[48];
    snprintf(an, sizeof an, "on%s", type->d);
    const char *code = node_attr(n, an);
    if (!code || sb_denies(p, JS_SB_SCRIPTS)) return JS_UNDEF;
    jsobj *w = wrap(p, n);
    jsobj *l = listeners_of(p, jv_obj(w), true);
    for (u32 i = 0; i < l->alen; i += 4) {
        if (((int)jv_n(l->arr[i + 2]) & LF_ATTR) && js_str_eq(jv_s(l->arr[i]), type)) {
            jsstr *src = jv_isstr(l->arr[i + 3]) ? jv_s(l->arr[i + 3]) : NULL;
            if (src && !strcmp(src->d, code)) return l->arr[i + 1];
            memmove(l->arr + i, l->arr + i + 4, sizeof(jsv) * (l->alen - i - 4));
            l->alen -= 4;
            break;
        }
    }
    sbuf_t sb = {0};
    sb_puts(&sb, "(function(event){\n");
    sb_puts(&sb, code);
    sb_puts(&sb, "\n})");
    jsstr *err = NULL;
    jsrealm *saved_realm = J.realm;
    J.realm = p->realm;   /* o atributo pertence à janela do elemento */
    jsscript *s = js_parse(sb.p, sb.len, an, 0, &err);
    sb_free(&sb);
    if (s) s->realm = p->realm;
    if (!s) {
        J.realm = saved_realm;
        char b[300];
        snprintf(b, sizeof b, "SyntaxError no atributo %s: %s", an, err ? err->d : "?");
        js_console_log(3, b);
        p->errors++;
        return JS_UNDEF;
    }
    jsv f = js_run_script(s);
    J.realm = saved_realm;
    if (f == JS_EXC) { js_report_exception(); return JS_UNDEF; }
    js_arr_push(l, jv_str(type));
    js_arr_push(l, f);
    js_arr_push(l, jv_int(LF_ATTR));
    js_arr_push(l, jv_str(js_strz(code)));
    return f;
}

static void call_handler(jsv fn, jsv thisv, jsobj *ev, bool retfalse_prevents) {
    jsv a[1] = {jv_obj(ev)};
    jsv r;
    J.interrupted = false;
    js_set_budget(2000);
    if (js_iscallable(fn)) r = js_call(fn, thisv, 1, a);
    else if (jv_isobj(fn)) {
        jsv he = js_getk(fn, A[A_handleEvent]);
        r = he == JS_EXC ? JS_EXC : js_iscallable(he) ? js_call(he, fn, 1, a) : JS_UNDEF;
    } else return;
    if (r == JS_EXC) js_report_exception();
    else if (r == JS_FALSE && retfalse_prevents) js_defprop(ev, js_intern("defaultPrevented", -1), JS_TRUE, 0);
}

static void invoke_at(js_page_t *p, jsv target, node_t *n, jsobj *ev, jsstr *type, int phase) {
    /* phase: 1 captura, 2 alvo, 3 borbulha */
    js_defprop(ev, js_intern("currentTarget", -1), target, 0);
    js_defprop(ev, js_intern("eventPhase", -1), jv_int(phase), 0);
    if (phase != 1) {
        if (n) attr_handler(p, n, type);
        else {
            /* window.on<tipo> como propriedade do objeto global */
            char an[48];
            snprintf(an, sizeof an, "on%s", type->d);
            jsprop *pr = js_ownprop(p->realm->global, js_intern(an, -1));
            if (pr && js_iscallable(pr->v)) call_handler(pr->v, target, ev, true);
            if (!strcmp(type->d, "load") && p->doc->body) {
                const char *code = node_attr(p->doc->body, "onload");
                if (code) { jsv f = attr_handler(p, p->doc->body, type); if (f != JS_UNDEF) call_handler(f, target, ev, true); }
            }
        }
    }
    jsobj *l = listeners_of(p, target, false);
    if (!l || !l->alen) return;
    /* copia, pois os tratadores podem alterar a lista */
    jsobj *copy = js_arr_from(l->arr, (int)l->alen);
    for (u32 i = 0; i < copy->alen; i += 4) {
        if (!js_str_eq(jv_s(copy->arr[i]), type)) continue;
        int fl = (int)jv_n(copy->arr[i + 2]);
        bool cap = fl & LF_CAPTURE;
        if ((phase == 1 && !cap) || (phase == 3 && cap)) continue;
        jsv fn = copy->arr[i + 1];
        if (fl & LF_ONCE) remove_listener(l, type, fn, fl & LF_CAPTURE);
        call_handler(fn, target, ev, (fl & (LF_PROP | LF_ATTR)) != 0);
        if (ev_flag(ev, "\x01stopimm")) break;
    }
}

static jsobj *new_event(js_page_t *p, jsstr *type, bool bubbles) {
    jsobj *ev = js_newobj(p->realm->p_event);
    js_defprop(ev, A[A_type], jv_str(type), 0);
    js_defprop(ev, js_intern("bubbles", -1), jv_bool(bubbles), 0);
    js_defprop(ev, js_intern("cancelable", -1), JS_TRUE, 0);
    js_defprop(ev, js_intern("defaultPrevented", -1), JS_FALSE, 0);
    js_defprop(ev, js_intern("timeStamp", -1), jv_num((double)(timer_ticks() - p->t0)), 0);
    js_defprop(ev, js_intern("isTrusted", -1), JS_TRUE, 0);
    return ev;
}

/* despacha ev em target (nó, ou NULL = window); devolve true se preventDefault */
static bool dispatch(js_page_t *p, node_t *target, jsobj *ev) {
    jsprop *tp = js_ownprop(ev, A[A_type]);
    if (!tp || !jv_isstr(tp->v)) return false;
    jsstr *type = jv_s(tp->v);
    bool bubbles = ev_flag(ev, "bubbles");
    jsv win = jv_obj(p->realm->global);
    node_t *path[512];
    int n = 0;
    for (node_t *x = target; x && n < 512; x = x->parent) path[n++] = x;
    js_defprop(ev, A[A_target], target ? jv_obj(wrap(p, target)) : win, 0);
    invoke_at(p, win, NULL, ev, type, 1);
    for (int i = n - 1; i >= 1 && !ev_flag(ev, "\x01stop"); i--) invoke_at(p, jv_obj(wrap(p, path[i])), path[i], ev, type, 1);
    if (!ev_flag(ev, "\x01stop")) {
        if (target) invoke_at(p, jv_obj(wrap(p, target)), target, ev, type, 2);
        else invoke_at(p, win, NULL, ev, type, 2);
    }
    if (bubbles && target) {
        for (int i = 1; i < n && !ev_flag(ev, "\x01stop"); i++) invoke_at(p, jv_obj(wrap(p, path[i])), path[i], ev, type, 3);
        if (!ev_flag(ev, "\x01stop")) invoke_at(p, win, NULL, ev, type, 3);
    }
    js_defprop(ev, js_intern("currentTarget", -1), JS_NULL, 0);
    return ev_flag(ev, "defaultPrevented");
}

NATIVE(ev_method) {
    UNUSED_ARGS;
    if (!jv_isobj(thisv)) return JS_UNDEF;
    jsobj *ev = jv_o(thisv);
    switch (self->u.nat.magic) {
    case 0: js_defprop(ev, js_intern("defaultPrevented", -1), JS_TRUE, 0); break;
    case 2: js_defprop(ev, js_intern("\x01stopimm", -1), JS_TRUE, PF_NOENUM); /* fallthrough */
    case 1: js_defprop(ev, js_intern("\x01stop", -1), JS_TRUE, PF_NOENUM); js_defprop(ev, js_intern("cancelBubble", -1), JS_TRUE, 0); break;
    }
    return JS_UNDEF;
}

NATIVE(ev_ctor) {
    UNUSED_ARGS;
    js_page_t *p = cur_page();
    if (!p) return JS_UNDEF;
    jsstr *type = js_tostr(ARG(0));
    if (!type) return JS_EXC;
    jsv opts = ARG(1);
    bool bub = false;
    jsv detail = JS_NULL;
    if (jv_isobj(opts)) {
        jsv b = js_getn(opts, "bubbles");
        TRYV(b);
        bub = js_truthy(b);
        detail = js_getk(opts, A[A_detail]);
        TRYV(detail);
        if (detail == JS_UNDEF) detail = JS_NULL;
    }
    jsobj *ev = new_event(p, type, bub);
    js_defprop(ev, js_intern("isTrusted", -1), JS_FALSE, 0);
    if (self->u.nat.magic) js_defprop(ev, A[A_detail], detail, 0);
    if (flags & NF_NEW) ev->proto = jv_o(thisv)->proto;
    return jv_obj(ev);
}

NATIVE(et_listen) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    if (!p) return JS_UNDEF;
    jsstr *type = js_tostr(ARG(0));
    if (!type) return JS_EXC;
    jsv fn = ARG(1);
    if (!js_iscallable(fn) && !jv_isobj(fn)) return JS_UNDEF;
    int fl = 0;
    jsv o = ARG(2);
    if (o == JS_TRUE) fl |= LF_CAPTURE;
    else if (jv_isobj(o)) {
        jsv c = js_getk(o, A[A_capture]);
        TRYV(c);
        jsv on = js_getk(o, A[A_once]);
        TRYV(on);
        if (js_truthy(c)) fl |= LF_CAPTURE;
        if (js_truthy(on)) fl |= LF_ONCE;
    }
    jsv target = jv_iscls(thisv, CL_NODE) ? thisv : jv_obj(p->realm->global);
    jsobj *l = listeners_of(p, target, true);
    if (self->u.nat.magic) remove_listener(l, type, fn, fl & LF_CAPTURE);
    else add_listener(l, type, fn, fl);
    return JS_UNDEF;
}

NATIVE(et_dispatch) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    if (!p || !jv_isobj(ARG(0))) return js_throw_err(E_TYPE, "dispatchEvent: o argumento não é um evento");
    bool prevented = dispatch(p, node_of(thisv), jv_o(ARG(0)));
    return jv_bool(!prevented);
}

/* ============================================================ temporizadores */
static int add_timer(js_page_t *p, jsv fn, double delay, bool repeat, int argc, jsv *argv) {
    if (p->ntimers == p->ctimers) {
        int nc = p->ctimers ? p->ctimers * 2 : 16;
        jtimer_t *nt = krealloc(p->timers, sizeof(jtimer_t) * (usize)nc);
        if (!nt) return 0;
        p->timers = nt;
        p->ctimers = nc;
    }
    if (!(delay >= 0)) delay = 0;
    if (delay > 2e9) delay = 2e9;
    u32 ms = (u32)delay;
    if (repeat && ms < 10) ms = 10;
    jtimer_t *t = &p->timers[p->ntimers++];
    t->id = ++p->next_timer;
    t->due = timer_ticks() + ms;
    t->interval = repeat ? ms : 0;
    t->fn = fn;
    t->args = argc > 0 ? js_arr_from(argv, argc) : NULL;
    t->live = true;
    return t->id;
}

NATIVE(w_settimeout) {
    UNUSED_ARGS;
    js_page_t *p = cur_page();
    if (!p) return JS_UNDEF;
    jsv fn = ARG(0);
    if (!js_iscallable(fn)) { jsstr *s = js_tostr(fn); if (!s) return JS_EXC; fn = jv_str(s); }
    double d = js_tonum(ARG(1));
    if (js_pending()) return JS_EXC;
    int mode = self->u.nat.magic;   /* 0 timeout, 1 interval, 2 requestAnimationFrame */
    if (mode == 2) d = 16;
    return jv_int(add_timer(p, fn, d, mode == 1, mode == 2 ? 0 : argc > 2 ? argc - 2 : 0, argv + 2));
}

NATIVE(w_cleartimeout) {
    UNUSED_ARGS;
    js_page_t *p = cur_page();
    if (!p) return JS_UNDEF;
    int id = (int)js_tointeger(ARG(0));
    for (int i = 0; i < p->ntimers; i++) if (p->timers[i].id == id) p->timers[i].live = false;
    return JS_UNDEF;
}

static int run_timers(js_page_t *p) {
    u64 now = timer_ticks();
    int fired = 0;
    for (int round = 0; round < 64; round++) {
        int best = -1;
        for (int i = 0; i < p->ntimers; i++)
            if (p->timers[i].live && p->timers[i].due <= now && (best < 0 || p->timers[i].due < p->timers[best].due)) best = i;
        if (best < 0) break;
        jtimer_t *t = &p->timers[best];
        jsv fn = t->fn;
        jsobj *args = t->args;
        int id = t->id;
        bool raf = false;
        if (t->interval) { t->due += t->interval; if (t->due <= now) t->due = now + t->interval; }
        else t->live = false;
        (void)id;
        J.interrupted = false;
        js_set_budget(2000);
        J.line = 0;
        jsv r;
        if (jv_isstr(fn)) r = js_eval_string(jv_s(fn), "setTimeout");
        else if (raf) { jsv a[1] = {jv_num((double)(now - p->t0))}; r = js_call(fn, JS_UNDEF, 1, a); }
        else r = js_call(fn, JS_UNDEF, args ? (int)args->alen : 0, args ? args->arr : NULL);
        if (r == JS_EXC) js_report_exception();
        js_run_jobs();
        fired++;
    }
    /* compacta */
    int k = 0;
    for (int i = 0; i < p->ntimers; i++) if (p->timers[i].live) p->timers[k++] = p->timers[i];
    p->ntimers = k;
    return fired;
}

/* ============================================================ scripts */
static void queue_script(js_page_t *p, node_t *n) {
    if (sb_denies(p, JS_SB_SCRIPTS)) return;   /* sandbox sem allow-scripts */
    for (int i = 0; i < p->nscripts; i++) if (p->scripts[i].n == n) return;
    const char *type = node_attr(n, "type");
    bool module = type && !strcasecmp(type, "module");
    if (type && *type && !module) {
        char t[64];
        int k = 0;
        for (; type[k] && k < 63; k++) t[k] = (char)tolower((u8)type[k]);
        t[k] = 0;
        if (strcmp(t, "text/javascript") && strcmp(t, "application/javascript") && strcmp(t, "application/x-javascript") &&
            strcmp(t, "text/ecmascript") && strcmp(t, "application/ecmascript") && strcmp(t, "text/jscript")) return;
    }
    if (p->nscripts == p->cscripts) {
        int nc = p->cscripts ? p->cscripts * 2 : 16;
        jscript_t *ns = krealloc(p->scripts, sizeof(jscript_t) * (usize)nc);
        if (!ns) return;
        p->scripts = ns;
        p->cscripts = nc;
    }
    jscript_t *s = &p->scripts[p->nscripts++];
    memset(s, 0, sizeof *s);
    s->n = n;
    s->module = module;
    s->job = JS_UNDEF;
    const char *src = node_attr(n, "src");
    if (src && *src) url_resolve(p->doc->base, src, s->url, sizeof s->url);
}

static void collect_scripts(js_page_t *p, node_t *n) {
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type != N_ELEM) continue;
        if (c->tag == T_SCRIPT) queue_script(p, c);
        else if (c->tag == T_NOSCRIPT) { if (!node_attr(c, "hidden")) set_attr(p, c, "hidden", "", 0); }
        else if (c->tag != T_SVG && c->tag != T_TEMPLATE) collect_scripts(p, c);
    }
}

static void exec_source(js_page_t *p, const char *src, int len, const char *name, node_t *n) {
    jsstr *err = NULL;
    J.interrupted = false;
    jsscript *s = js_parse(src, len, name, 1, &err);
    if (!s) {
        char b[400];
        snprintf(b, sizeof b, "SyntaxError: %s (%s)", err ? err->d : "erro de sintaxe", name);
        js_console_log(3, b);
        p->errors++;
        return;
    }
    p->cur_script = n;
    p->in_script = true;
    js_set_budget(5000);
    jsv r = js_run_script(s);
    p->in_script = false;
    p->cur_script = NULL;
    if (r == JS_EXC) { jsstr *sv = J.srcname; J.srcname = s->name; js_report_exception(); J.srcname = sv; }
    js_run_jobs();
}

static void fire_simple(js_page_t *p, node_t *target, const char *type, bool bubbles) {
    jsobj *ev = new_event(p, js_intern(type, -1), bubbles);
    dispatch(p, target, ev);
    js_run_jobs();
}

/* ---------- iframes: postMessage entre janelas (clone por JSON) */
static js_page_t *page_of_global(jsv g) {
    if (!jv_isobj(g)) return NULL;
    for (js_page_t *q = pages; q; q = q->next) if ((q->realm && jv_obj(q->realm->global) == g) || (q->xwin && jv_obj(q->xwin) == g)) return q;
    return NULL;
}

/* mesma origem: origens opacas ("null", sandbox sem allow-same-origin) nunca são iguais a nada */
static bool same_origin(js_page_t *a, js_page_t *b) {
    return a && b && (a == b || (strcmp(a->origin, "null") && !strcmp(a->origin, b->origin)));
}

static jsv win_for(js_page_t *viewer, js_page_t *t);
static jsobj *xwin_of(js_page_t *t);

/* WindowProxy entre origens: magic 0 window/self/frames 1 parent 2 top 3 length 4 closed 5 location(get) 6 location(set)
 * 7 close/focus/blur; postMessage reaproveita w_postmessage (page_of_global reconhece o proxy) */
NATIVE(xwin_get) {
    UNUSED_ARGS;
    int m = self->u.nat.magic;
    js_page_t *t = page_of_global(m == 8 || m == 9 ? self->u.nat.data : thisv), *v = cur_page();
    if (m == 10) return js_throw_dom("SecurityError", "Bloqueado o acesso a uma janela de outra origem");
    if (!t) return JS_UNDEF;
    switch (m) {
    case 0: return win_for(v, t);
    case 1: return win_for(v, t->parent ? t->parent : t);
    case 2: { js_page_t *top = t; while (top->parent) top = top->parent; return win_for(v, top); }
    case 3: return jv_int(t->nkids);
    case 4: return JS_FALSE;
    case 5: {
        /* Location entre origens: só replace() e href (escrita) */
        jsobj *l = js_newobj(NULL);
        jsv lw = jv_obj(xwin_of(t));
        jsobj *rf = js_newnative(xwin_get, "replace", 1, 8);
        rf->u.nat.data = lw;
        js_defprop(l, js_intern("replace", -1), jv_obj(rf), PF_NOENUM);
        jsobj *hs = js_newnative(xwin_get, "href", 1, 9);
        hs->u.nat.data = lw;
        js_defaccessor(l, js_intern("href", -1), jv_obj(js_newnative(xwin_get, "href", 0, 10)), jv_obj(hs), 0);
        return jv_obj(l);
    }
    case 6: case 8: case 9: {
        if (v && (v->sbox & JS_SB_ON) && !(v->sbox & (JS_SB_TOP_NAV | JS_SB_TOP_NAV_USER)) && !t->parent && v != t)
            return js_throw_dom("SecurityError", "sandbox: o iframe não pode navegar a janela de cima");
        jsstr *u = js_tostr(ARG(0));
        if (!u) return JS_EXC;
        page_navigate(t, u->d, m == 8);
        return JS_UNDEF;
    }
    }
    return JS_UNDEF;
}

static jsobj *xwin_of(js_page_t *t) {
    if (t->xwin) return t->xwin;
    jsobj *o = js_newobj(NULL);
    t->xwin = o;
    static const char *gs[] = { "window", "self", "frames", "parent", "top", "length", "closed" };
    static const int gm[] = { 0, 0, 0, 1, 2, 3, 4 };
    for (u32 i = 0; i < ARRAY_LEN(gs); i++) js_defgetter(o, gs[i], xwin_get, gm[i]);
    js_defaccessor(o, js_intern("location", -1), jv_obj(js_newnative(xwin_get, "location", 0, 5)), jv_obj(js_newnative(xwin_get, "location", 1, 6)), 0);
    js_defprop(o, js_intern("postMessage", -1), js_getn(jv_obj(t->realm->global), "postMessage"), PF_NOENUM);
    static const char *fs[] = { "close", "focus", "blur" };
    for (u32 i = 0; i < ARRAY_LEN(fs); i++) js_defnative(o, fs[i], xwin_get, 0, 7);
    return o;
}

/* o que a janela viewer enxerga de t: o objeto global (mesma origem) ou o WindowProxy restrito */
static jsv win_for(js_page_t *viewer, js_page_t *t) {
    if (!t || !t->realm) return JS_NULL;
    if (!viewer || same_origin(viewer, t)) return jv_obj(t->realm->global);
    return jv_obj(xwin_of(t));
}

/* window[i] e window.length: o hospedeiro informa as páginas dos iframes em ordem do documento */
void js_page_set_frames(js_page_t *p, js_page_t **kids, int n) {
    if (!p || !p->realm) return;
    if (n > (int)ARRAY_LEN(p->kids)) n = (int)ARRAY_LEN(p->kids);
    if (n == p->nkids && !memcmp(p->kids, kids, sizeof(js_page_t *) * (usize)n)) return;
    jsobj *g = p->realm->global;
    char k[12];
    for (int i = n; i < p->nkids; i++) { snprintf(k, sizeof k, "%d", i); js_delprop(g, js_intern(k, -1)); }
    for (int i = 0; i < n; i++) {
        snprintf(k, sizeof k, "%d", i);
        js_defprop(g, js_intern(k, -1), win_for(p, kids[i]), PF_NOENUM | PF_RO);
        p->kids[i] = kids[i];
    }
    p->nkids = n;
}

/* preparo das páginas de iframe pelo anfitrião da página de cima (fases como js_page_new2) */
void js_page_set_frame_init(js_page_t *p, js_init_fn fn, void *ctx) { if (p) { p->frame_init = fn; p->frame_ctx = ctx; } }
void js_page_run_frame_init(js_page_t *c, int phase) {
    js_page_t *t = c ? c->parent : NULL;
    while (t && !t->frame_init) t = t->parent;
    if (t) t->frame_init(c, phase, t->frame_ctx);
}

/* ---------- Permissions Policy (atributo allow do iframe; allowfullscreen) */
static const struct { const char *f; bool all; } pol_feats[] = {   /* all: lista padrão '*'; senão 'self' */
    { "accelerometer", 0 }, { "ambient-light-sensor", 0 }, { "autoplay", 0 }, { "bluetooth", 0 }, { "camera", 0 },
    { "clipboard-read", 0 }, { "clipboard-write", 0 }, { "display-capture", 0 }, { "document-domain", 1 },
    { "encrypted-media", 0 }, { "fullscreen", 0 }, { "gamepad", 0 }, { "geolocation", 0 }, { "gyroscope", 0 },
    { "hid", 0 }, { "idle-detection", 0 }, { "local-fonts", 0 }, { "magnetometer", 0 }, { "microphone", 0 },
    { "midi", 0 }, { "payment", 0 }, { "picture-in-picture", 1 }, { "publickey-credentials-create", 0 },
    { "publickey-credentials-get", 0 }, { "screen-wake-lock", 0 }, { "serial", 0 }, { "storage-access", 1 },
    { "sync-xhr", 1 }, { "usb", 0 }, { "web-share", 0 }, { "window-management", 0 }, { "xr-spatial-tracking", 0 },
};

/* origem de um URL ("https://x.y[:porta]") */
static void origin_of_url(const char *u, char *out, int cap) {
    out[0] = 0;
    const char *s = strstr(u, "://");
    if (!s) return;
    const char *e = s + 3;
    while (*e && *e != '/' && *e != '?' && *e != '#') e++;
    snprintf(out, (usize)cap, "%.*s", (int)(e - u), u);
}

/* a lista do atributo allow para o recurso f: 1 sim, 0 não, -1 não aparece. 'self' = origem do pai, 'src' (padrão) =
 * a do iframe; origem = só ela */
static int allow_attr(js_page_t *p, const char *f) {
    const char *a = node_attr(p->frame_el, "allow");
    if (!strcmp(f, "fullscreen") && node_attr(p->frame_el, "allowfullscreen")) return 1;
    if (!a) return -1;
    usize fl = strlen(f);
    for (const char *d = a; *d;) {
        while (*d == ' ' || *d == ';' || *d == '\t' || *d == '\n') d++;
        const char *de = d;
        while (*de && *de != ';') de++;
        const char *e = d;
        while (e < de && *e != ' ' && *e != '\t') e++;
        if ((usize)(e - d) == fl && !strncasecmp(d, f, fl)) {
            bool any = false;
            for (const char *t = e; t < de;) {
                while (t < de && (*t == ' ' || *t == '\t')) t++;
                const char *te = t;
                while (te < de && *te != ' ' && *te != '\t') te++;
                if (te == t) break;
                any = true;
                int n = (int)(te - t);
                if ((n == 1 && *t == '*') || (n == 5 && !strncmp(t, "'src'", 5))) return 1;
                if (n == 6 && !strncmp(t, "'none'", 6)) return 0;
                if (n == 6 && !strncmp(t, "'self'", 6)) { if (p->parent && !strcmp(p->origin, p->parent->origin)) return 1; }
                else if ((int)strlen(p->origin) == n && !strncmp(t, p->origin, (usize)n)) return 1;
                t = te;
            }
            return any ? 0 : 1;   /* sem lista: 'src' */
        }
        d = de;
    }
    return -1;
}

/* o recurso f vale na página p (para a origem o, se dada) */
static bool policy_allows(js_page_t *p, const char *f, const char *o) {
    int k = -1;
    for (u32 i = 0; i < ARRAY_LEN(pol_feats); i++) if (!strcmp(pol_feats[i].f, f)) k = (int)i;
    if (k < 0) return false;
    if (o && *o && strcmp(o, p->origin) && !pol_feats[k].all) return false;   /* 'self' na própria página */
    if (!p->parent || !p->frame_el) return true;
    if (!policy_allows(p->parent, f, p->parent->origin)) return false;
    int a = allow_attr(p, f);
    if (a >= 0) return a == 1;
    return pol_feats[k].all || same_origin(p, p->parent);
}

/* magic: 0 allowsFeature 1 features 2 allowedFeatures 3 getAllowlistForFeature */
NATIVE(fpol_method) {
    UNUSED_ARGS;
    js_page_t *p = page_of_global(self->u.nat.data);
    if (!p) return JS_UNDEF;
    int m = self->u.nat.magic;
    if (m == 1 || m == 2) {
        jsobj *a = js_newarr(0);
        for (u32 i = 0; i < ARRAY_LEN(pol_feats); i++)
            if (m == 1 || policy_allows(p, pol_feats[i].f, NULL)) js_arr_push(a, jv_str(js_strz(pol_feats[i].f)));
        return jv_obj(a);
    }
    jsstr *f = js_tostr(ARG(0));
    if (!f) return JS_EXC;
    if (m == 0) {
        char o[256] = "";
        if (argc > 1 && !jv_isnullish(argv[1])) {
            jsstr *u = js_tostr(argv[1]);
            if (!u) return JS_EXC;
            origin_of_url(u->d, o, sizeof o);
            if (!o[0]) snprintf(o, sizeof o, "%s", u->d);
        }
        return jv_bool(policy_allows(p, f->d, o[0] ? o : NULL));
    }
    jsobj *a = js_newarr(0);
    int k = -1;
    for (u32 i = 0; i < ARRAY_LEN(pol_feats); i++) if (!strcmp(pol_feats[i].f, f->d)) k = (int)i;
    if (k >= 0 && policy_allows(p, f->d, NULL)) js_arr_push(a, jv_str(js_strz(pol_feats[k].all ? "*" : p->origin)));
    return jv_obj(a);
}

static jsobj *fpol_of(js_page_t *p) {
    if (p->fpol) return p->fpol;
    jsobj *o = js_newobj(J.realm->p_object);
    static const char *nm[] = { "allowsFeature", "features", "allowedFeatures", "getAllowlistForFeature" };
    for (int i = 0; i < 4; i++) {
        jsobj *f = js_newnative(fpol_method, nm[i], i == 0 || i == 3 ? 1 : 0, i);
        f->u.nat.data = jv_obj(p->realm->global);
        js_defprop(o, js_intern(nm[i], -1), jv_obj(f), PF_NOENUM);
    }
    js_defprop(o, A[A_sym_toStringTag], jv_str(js_intern("FeaturePolicy", -1)), PF_NOENUM | PF_RO);
    p->fpol = o;
    return o;
}

/* iframe sandbox: chamado antes dos scripts do filho */
void js_page_set_sandbox(js_page_t *p, int flags) {
    if (!p) return;
    p->sbox = flags;
    if ((flags & JS_SB_ON) && !(flags & JS_SB_SAME_ORIGIN)) snprintf(p->origin, sizeof p->origin, "null");
}
int js_page_sandbox(js_page_t *p) { return p ? p->sbox : 0; }

/* window.open: o anfitrião abre a vista nova (MOS_WEB_EV_NEWWIN); devolve a URL pedida uma vez */
const char *js_page_take_popup(js_page_t *p) {
    if (!p || !p->popup_pending) return NULL;
    p->popup_pending = false;
    return p->popup;
}

NATIVE(w_postmessage) {
    UNUSED_ARGS;
    js_page_t *src = cur_page();
    js_page_t *dst = page_of_global(thisv);
    if (!dst) dst = src;
    if (!dst) return JS_UNDEF;
    jsv t = ARG(1);
    if (jv_isobj(t)) t = js_getn(t, "targetOrigin");
    if (jv_isstr(t)) {
        const char *to = jv_s(t)->d;
        if (strcmp(to, "*") && strcmp(to, "/") && strncmp(to, dst->origin, strlen(dst->origin))) return JS_UNDEF;   /* origem errada: descarta */
        if (!strcmp(to, "/") && src && strcmp(src->origin, dst->origin)) return JS_UNDEF;
    }
    /* clone estruturado agora (instantâneo da mensagem); na entrega, outro clone no reino de destino */
    jsv val = js_structured_clone(ARG(0));
    if (val == JS_EXC) return JS_EXC;
    pmsg_t *m = kcalloc(1, sizeof(pmsg_t));
    if (!m) return JS_UNDEF;
    m->val = val;
    snprintf(m->origin, sizeof m->origin, "%s", src ? src->origin : "null");
    m->source = src && src->realm ? (same_origin(src, dst) ? src->realm->global : xwin_of(src)) : NULL;
    if (dst->msgs_tail) dst->msgs_tail->next = m; else dst->msgs = m;
    dst->msgs_tail = m;
    return JS_UNDEF;
}

static void deliver_messages(js_page_t *p) {
    while (p->msgs) {
        pmsg_t *m = p->msgs;
        p->msgs = m->next;
        if (!p->msgs) p->msgs_tail = NULL;
        jsobj *ev = new_event(p, js_intern("message", -1), false);
        jsv data = js_structured_clone(m->val);
        if (data == JS_EXC) { js_catch(); data = JS_UNDEF; }
        js_defprop(ev, js_intern("data", -1), data, 0);
        js_defprop(ev, js_intern("origin", -1), jv_str(js_strz(m->origin)), 0);
        js_defprop(ev, js_intern("source", -1), m->source ? jv_obj(m->source) : JS_NULL, 0);
        js_defprop(ev, js_intern("lastEventId", -1), jv_str(A[A_empty]), 0);
        js_defprop(ev, js_intern("ports", -1), jv_obj(js_arr_from(NULL, 0)), 0);
        kfree(m);
        dispatch(p, NULL, ev);
        js_run_jobs();
        p->dirty = true;
    }
}

void js_page_fire(js_page_t *p, node_t *target, const char *type, bool bubbles) {
    if (!p) return;
    int frame;
    js_enter(&frame);
    jsrealm *saved = J.realm;
    J.realm = p->realm;
    fire_simple(p, target, type, bubbles);
    J.realm = saved;
    js_leave();
}

void js_page_set_dialog(js_page_t *p, js_dialog_fn fn, void *ctx) { if (p) { p->dialog = fn; p->dialog_ctx = ctx; } }
void *js_suspend_exec(void) { return js_exec_suspend(); }
void js_resume_exec(void *e) { js_exec_resume(e); }
const char *js_page_url(js_page_t *p) { return p ? p->url : ""; }

void js_page_link_frame(js_page_t *c, js_page_t *par, node_t *iframe) {
    if (!c || !par || c == par) return;
    c->parent = par;
    if (!c->dialog) { c->dialog = par->dialog; c->dialog_ctx = par->dialog_ctx; }
    c->frame_el = iframe;
    bool opaque = (c->sbox & JS_SB_ON) && !(c->sbox & JS_SB_SAME_ORIGIN);
    if (!opaque && (!strncmp(c->url, "about:", 6) || !strncmp(c->url, "data:", 5)))
        snprintf(c->origin, sizeof c->origin, "%s", strncmp(c->url, "data:", 5) ? par->origin : "null");
    js_page_t *top = par;
    while (top->parent) top = top->parent;
    jsobj *g = c->realm->global;
    js_defn(g, "parent", win_for(c, par), PF_NOENUM);
    js_defn(g, "top", win_for(c, top), PF_NOENUM);
    js_defn(g, "frameElement", same_origin(c, par) ? jv_obj(wrap(par, iframe)) : JS_NULL, PF_NOENUM);
}

/* executa scripts em ordem; devolve false se está esperando um download */
static void net_start(js_page_t *p, const char *url, const char *method, const char *body, int blen, const char *ctype,
                      int kind, jsobj *target);

static bool pump_scripts(js_page_t *p) {
    while (p->spos < p->nscripts) {
        jscript_t *s = &p->scripts[p->spos];
        if (s->state == 3 || s->module) { p->spos++; continue; }
        if (s->url[0] && s->state < 2) {
            if (!strncmp(s->url, "file:", 5)) {
                char path[512];
                const char *q = s->url + 5;
                if (!strncmp(q, "//", 2)) q += 2;
                url_decode_path(q, path, sizeof path);
                vnode_t *v = vfs_lookup(path);
                if (v && !v->dir) { s->text = kmalloc(v->size + 1); if (s->text) { memcpy(s->text, v->data, v->size); s->len = (int)v->size; } }
                else { char b[600]; snprintf(b, sizeof b, "Falha ao carregar o script %s", s->url); js_console_log(3, b); p->errors++; }
                s->state = 2;
            } else if (!strncmp(s->url, "http", 4)) {
                if (s->state == 0) {
                    s->req = http_get(s->url);
                    s->state = s->req ? 1 : 2;
                    if (!s->req) { char b[600]; snprintf(b, sizeof b, "Falha ao iniciar o download de %s", s->url); js_console_log(3, b); }
                }
                if (s->state == 1) {
                    int st = http_poll(s->req);
                    if (st == 0) return false;
                    if (st > 0 && s->req->body && s->req->status < 400) {
                        usize ul;
                        s->text = charset_to_utf8(s->req->body, s->req->body_len, NULL, &ul);
                        s->len = (int)ul;
                    } else { char b[600]; snprintf(b, sizeof b, "Falha ao baixar o script %s (%s)", s->url, s->req->error); js_console_log(3, b); p->errors++; }
                    http_free(s->req);
                    s->req = NULL;
                    s->state = 2;
                }
            } else s->state = 2;
        }
        s->state = 3;
        p->spos++;
        if (s->url[0]) {
            if (s->text) exec_source(p, s->text, s->len, s->url, s->n);
        } else {
            node_t *t = s->n->first;
            if (t && t->type == N_TEXT) {
                char name[64];
                snprintf(name, sizeof name, "script inline %d", p->spos);
                exec_source(p, t->text, t->len, name, s->n);
            }
        }
        kfree(s->text);
        s->text = NULL;
        /* p->scripts pode ter sido realocado durante a execução */
    }
    /* scripts type="module": adiados (defer) e executados em ordem depois dos clássicos */
    while (p->mpos < p->nscripts) {
        jscript_t *s = &p->scripts[p->mpos];
        if (!s->module || s->state == 3) { p->mpos++; continue; }
        if (s->job == JS_UNDEF) {
            if (s->url[0]) s->job = js_module_start_script(p, s->url, NULL, 0);
            else {
                node_t *t = s->n->first;
                s->job = js_module_start_script(p, p->url, t && t->type == N_TEXT ? t->text : "", t && t->type == N_TEXT ? t->len : 0);
            }
            js_run_jobs();
        }
        if (js_module_job_state(p->scripts[p->mpos].job) == 0) return false;
        p->scripts[p->mpos].state = 3;
        p->mpos++;
    }
    return true;
}

void js_net_fetch_module(js_page_t *p, const char *url, jsobj *mod) {
    if (!p) { js_module_fetched(mod, NULL, 0, 0, "sem página"); return; }
    net_start(p, url, "GET", NULL, 0, NULL, 2, mod);
}

const char *js_page_base(js_page_t *p) { return p ? p->doc->base : ""; }

/* <img> do documento: todos ganham img_t (o pedido começa) para o load da janela esperar por eles */
static void scan_imgs(js_page_t *p, node_t *n) {
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type != N_ELEM) continue;
        if (c->tag == T_IMG && !c->img) img_refresh(p, c);
        if (c->first) scan_imgs(p, c);
    }
}

static bool imgs_pending(js_page_t *p) {
    for (img_t *im = p->doc->images; im; im = im->next) {
        if (im->state != IMG_PENDING && im->state != IMG_LOADING) continue;
        for (int i = 0; i < im->nown; i++) if (im->own[i].n->img == im && is_connected(p, im->own[i].n)) return true;
    }
    return false;
}

/* eventos load/error de cada <img> (no documento ou não) e as promessas de img.decode() */
static void img_events(js_page_t *p) {
    for (img_t *im = p->doc->images; im; im = im->next) {
        if (im->state != IMG_OK && im->state != IMG_ERROR) continue;
        for (int i = 0; i < im->nown; i++) {
            if (im->own[i].fired) continue;
            im->own[i].fired = true;
            node_t *n = im->own[i].n;
            if (n->img == im) fire_simple(p, n, im->state == IMG_OK ? "load" : "error", false);
        }
    }
    if (!p->imgdec || !p->imgdec->alen) return;
    jsobj *keep = js_newarr(0);
    for (u32 i = 0; i + 1 < p->imgdec->alen; i += 2) {
        jsv el = js_arr_get(p->imgdec, i), pr = js_arr_get(p->imgdec, i + 1);
        node_t *n = node_of(el);
        img_t *im = n ? n->img : NULL;
        if (im && (im->state == IMG_PENDING || im->state == IMG_LOADING)) { js_arr_push(keep, el); js_arr_push(keep, pr); continue; }
        if (im && im->state == IMG_OK && im->surf) js_promise_resolve(jv_o(pr), JS_UNDEF);
        else {
            jsobj *e = js_newobj(p->realm->p_domexc ? p->realm->p_domexc : p->realm->p_err[E_ERROR]);
            js_defn(e, "name", jv_str(js_strz("EncodingError")), PF_NOENUM);
            js_defn(e, "message", jv_str(js_strz("a imagem não pôde ser decodificada")), PF_NOENUM);
            js_defn(e, "code", jv_int(0), PF_NOENUM);
            js_promise_reject(jv_o(pr), jv_obj(e));
        }
    }
    p->imgdec = keep;
    js_run_jobs();
}

static void maybe_load_events(js_page_t *p) {
    if (p->spos < p->nscripts || p->mpos < p->nscripts) return;
    if (!p->dcl) {
        p->dcl = true;
        fire_simple(p, p->doc->root, "DOMContentLoaded", true);
    }
    if (!p->img_scan) { p->img_scan = true; scan_imgs(p, p->doc->root); }
    img_events(p);
    if (!p->loaded && imgs_pending(p)) return;   /* o load da janela espera as imagens */
    if (!p->loaded) {
        p->loaded = true;
        fire_simple(p, NULL, "load", false);
    }
}

/* ============================================================ propriedades do DOM */
static bool keyis(jsstr *k, const char *s) { return k->len == strlen(s) && !memcmp(k->d, s, k->len); }

static jsv str_or_empty(const char *s) { return jv_str(s ? js_strz(s) : A[A_empty]); }

static jsv children_array(js_page_t *p, node_t *n, bool elems) {
    jsobj *a = js_newarr(0);
    for (node_t *c = n->first; c; c = c->next) if (!elems || c->type == N_ELEM) js_arr_push(a, jv_obj(wrap(p, c)));
    return jv_obj(a);
}

static node_t *form_of(node_t *n) {
    for (node_t *q = n->parent; q; q = q->parent) if (q->type == N_ELEM && q->tag == T_FORM) return q;
    return NULL;
}

static jsv ctrl_value(node_t *n) {
    ctrl_t *c = n->ctrl;
    if (n->tag == T_SELECT) {
        if (c && c->nopts) return jv_str(js_strz(c->vals[CLAMP(c->sel, 0, c->nopts - 1)]));
        /* sem controle (fora do layout): a <option selected>, senão a primeira */
        node_t *first = NULL;
        for (node_t *o = n->first; o; o = o->next) {
            node_t *opts[2] = { o, NULL };
            if (o->type == N_ELEM && o->tag == T_OPTGROUP) opts[0] = o->first;
            for (node_t *q = opts[0]; q; q = o->tag == T_OPTGROUP ? q->next : NULL) {
                if (q->type != N_ELEM || q->tag != T_OPTION) continue;
                if (!first) first = q;
                if (node_attr(q, "selected")) { first = q; goto achou; }
            }
        }
    achou:
        if (!first) return jv_str(A[A_empty]);
        const char *v = node_attr(first, "value");
        if (v) return jv_str(js_strz(v));
        sbuf_t t = {0};
        node_text_content(first, &t);
        jsv r = jv_str(js_strn(t.p ? t.p : "", t.len));
        sb_free(&t);
        return r;
    }
    if (c && (c->kind == C_TEXT || c->kind == C_PASSWORD || c->kind == C_TEXTAREA || c->kind == C_HIDDEN)) return jv_str(js_strz(c->buf));
    if (n->tag == T_TEXTAREA) {
        sbuf_t t = {0};
        node_text_content(n, &t);
        jsv r = jv_str(js_strn(t.p ? t.p : "", t.len));
        sb_free(&t);
        return r;
    }
    const char *v = node_attr(n, "value");
    if (!v && n->tag == T_INPUT) {
        const char *ty = node_attr(n, "type");
        if (ty && (!strcasecmp(ty, "checkbox") || !strcasecmp(ty, "radio"))) return jv_str(js_strz("on"));
    }
    if (!v && n->tag == T_OPTION) {
        sbuf_t t = {0};
        node_text_content(n, &t);
        jsv r = jv_str(js_strn(t.p ? t.p : "", t.len));
        sb_free(&t);
        return r;
    }
    return str_or_empty(v);
}

static void set_ctrl_value(js_page_t *p, node_t *n, jsstr *v) {
    ctrl_t *c = n->ctrl;
    if (n->tag == T_SELECT) {
        if (c) for (int i = 0; i < c->nopts; i++) if (!strcmp(c->vals[i], v->d)) { c->sel = i; break; }
        for (node_t *o = n->first; o; o = o->next) {}
        p->dirty = true;
        return;
    }
    if (c && (c->kind == C_TEXT || c->kind == C_PASSWORD || c->kind == C_TEXTAREA || c->kind == C_HIDDEN)) {
        strncpy(c->buf, v->d, (usize)c->cap - 1);
        c->buf[c->cap - 1] = 0;
        p->dirty = true;
        return;
    }
    if (c && (c->kind == C_BUTTON || c->kind == C_SUBMIT || c->kind == C_RESET)) {
        strncpy(c->label, v->d, sizeof c->label - 1);
        c->label[sizeof c->label - 1] = 0;
    }
    if (n->tag == T_TEXTAREA) { set_text(p, n, v); return; }
    set_attr(p, n, "value", v->d, (int)v->len);
}

static bool ctrl_checked(node_t *n) { return n->ctrl ? n->ctrl->checked : node_attr(n, "checked") != NULL; }

static void set_checked(js_page_t *p, node_t *n, bool on) {
    ctrl_t *c = n->ctrl;
    if (c) {
        if (on && c->kind == C_RADIO) {
            const char *nm = node_attr(n, "name");
            doc_t *d = p->doc;
            for (int i = 0; i < d->nctrls; i++) {
                ctrl_t *o = d->ctrls[i];
                const char *on2 = node_attr(o->node, "name");
                if (o->kind == C_RADIO && o->form == c->form && nm && on2 && !strcmp(on2, nm)) o->checked = false;
            }
        }
        c->checked = on;
        p->dirty = true;
    } else if (on) set_attr(p, n, "checked", "", 0);
    else remove_attr(p, n, "checked");
}

static jsv geom(js_page_t *p, node_t *n, int what) {
    rect_acc r = node_rect(p, n);
    if (!r.any) return jv_int(0);
    switch (what) {
    case 0: return jv_int(r.x1 - r.x0);
    case 1: return jv_int(r.y1 - r.y0);
    case 2: return jv_int(r.x0);
    case 3: return jv_int(r.y0);
    }
    return jv_int(0);
}

static const char *bool_attrs[] = {"disabled", "hidden", "readonly", "required", "selected", "multiple", "autofocus", "open", NULL};
static const char *str_attrs[] = {"id", "title", "name", "placeholder", "lang", "alt", "rel", "target", "action", "method", "for", "role", NULL};

/* documento dono de um nó: o da página ou um criado por DOMParser/XHR */
static doc_t *doc_for(js_page_t *p, node_t *n) {
    node_t *top = n;
    while (top && top->parent) top = top->parent;
    for (int i = 0; i < p->nxdocs; i++) if (p->xdocs[i]->root == top) return p->xdocs[i];
    return p->doc;
}

static jsv node_get(jsobj *o, jsstr *k, bool *found) {
    js_page_t *p = o->u.host.page;
    node_t *n = o->u.host.node;
    doc_t *d = doc_for(p, n);
    *found = true;
    if (keyis(k, "nodeType")) return jv_int(n->type == N_DOC ? 9 : n->type == N_TEXT ? 3 : 1);
    if (keyis(k, "parentNode")) return n->parent ? wrapv(p, n->parent) : JS_NULL;
    if (keyis(k, "parentElement")) return n->parent && n->parent->type == N_ELEM ? wrapv(p, n->parent) : JS_NULL;
    if (keyis(k, "firstChild")) return wrapv(p, n->first);
    if (keyis(k, "lastChild")) return wrapv(p, n->last);
    if (keyis(k, "nextSibling")) return wrapv(p, n->next);
    if (keyis(k, "previousSibling")) return wrapv(p, n->prev);
    if (keyis(k, "childNodes")) return children_array(p, n, false);
    if (keyis(k, "ownerDocument")) return n->type == N_DOC ? JS_NULL : d == p->doc ? jv_obj(p->document) : wrapv(p, d->root);
    if (n->tag == T_IFRAME && keyis(k, "srcdoc")) return str_or_empty(node_attr(n, "srcdoc"));
    if (n->tag == T_IMG) {
        bool c_ = keyis(k, "complete"), nw = keyis(k, "naturalWidth"), nh = keyis(k, "naturalHeight"), cs = keyis(k, "currentSrc");
        bool w_ = keyis(k, "width"), h_ = keyis(k, "height");
        if (c_ || nw || nh || cs || w_ || h_) {
            if (!n->img && (node_attr(n, "src") || node_attr(n, "srcset"))) img_refresh(p, n);
            img_t *im = n->img;
            bool ok = im && im->state == IMG_OK && im->surf;
            if (c_) return jv_bool(!im || im->state == IMG_OK || im->state == IMG_ERROR);
            if (nw) return jv_int(ok ? im->surf->w : 0);
            if (nh) return jv_int(ok ? im->surf->h : 0);
            if (cs) return jv_str(js_strz(im && strncmp(im->url, "about:invalid", 13) ? im->url : ""));
            /* largura/altura: tamanho exibido se estiver no layout; senão o atributo; senão o natural */
            if (is_connected(p, n)) { rect_acc r = node_rect(p, n); if (r.any) return jv_int(w_ ? r.x1 - r.x0 : r.y1 - r.y0); }
            const char *a = node_attr(n, w_ ? "width" : "height");
            if (a && isdigit((u8)*a)) return jv_int(atoi(a));
            return jv_int(ok ? (w_ ? im->surf->w : im->surf->h) : 0);
        }
        if (keyis(k, "srcset") || keyis(k, "sizes") || keyis(k, "decoding") || keyis(k, "loading")) return str_or_empty(node_attr(n, k->d));
        if (keyis(k, "crossOrigin")) { const char *v = node_attr(n, "crossorigin"); return v ? jv_str(js_strz(v)) : JS_NULL; }
    }
    if (n->tag == T_IFRAME && (keyis(k, "contentWindow") || keyis(k, "contentDocument"))) {
        js_page_t *c = frame_page(n);
        if (!c) return JS_NULL;
        if (keyis(k, "contentWindow")) return win_for(p, c);
        return same_origin(c, p) ? jv_obj(c->document) : JS_NULL;   /* mesma origem */
    }
    if (keyis(k, "isConnected")) return jv_bool(is_connected(p, n));
    if (keyis(k, "textContent") || keyis(k, "innerText")) {
        if (n->type == N_DOC) return JS_NULL;
        if (n->type == N_TEXT) return jv_str(js_strn(n->text, n->len));
        sbuf_t t = {0};
        node_text_content(n, &t);
        jsv r = jv_str(js_strn(t.p ? t.p : "", t.len));
        sb_free(&t);
        return r;
    }
    if (n->type == N_TEXT) {
        if (keyis(k, "data") || keyis(k, "nodeValue") || keyis(k, "wholeText")) return jv_str(js_strn(n->text, n->len));
        if (keyis(k, "nodeName")) return jv_str(js_strz("#text"));
        if (keyis(k, "length")) return jv_int((int)js_strn(n->text, n->len)->ulen);
        *found = false;
        return JS_UNDEF;
    }
    if (n->type == N_DOC) {
        if (keyis(k, "body")) return wrapv(p, d->body);
        if (keyis(k, "head")) return wrapv(p, d->head);
        if (keyis(k, "documentElement")) return wrapv(p, d->html);
        if (keyis(k, "title")) return jv_str(js_strz(d->title));
        if (keyis(k, "URL") || keyis(k, "documentURI")) return jv_str(js_strz(p->url));
        if (keyis(k, "location")) return jv_obj(p->location);
        if (keyis(k, "readyState")) return jv_str(js_strz(p->loaded ? "complete" : p->dcl ? "interactive" : "loading"));
        if (keyis(k, "nodeName")) return jv_str(js_strz("#document"));
        if (keyis(k, "defaultView")) return jv_obj(p->realm->global);
        if (keyis(k, "cookie")) return jv_str(js_strz(p->cookie));
        if (keyis(k, "characterSet") || keyis(k, "charset")) return jv_str(js_strz("UTF-8"));
        if (keyis(k, "referrer")) return jv_str(A[A_empty]);
        if (keyis(k, "domain")) {
            const char *o = strstr(p->origin, "://");
            char h[256] = "";
            if (o) { snprintf(h, sizeof h, "%s", o + 3); char *c = strchr(h, ':'); if (c) *c = 0; }
            return jv_str(js_strz(h));
        }
        if (keyis(k, "featurePolicy") || keyis(k, "permissionsPolicy")) return jv_obj(fpol_of(p));
        if (keyis(k, "fullscreenEnabled")) return jv_bool(policy_allows(p, "fullscreen", NULL));
        if (keyis(k, "currentScript")) return wrapv(p, p->cur_script);
        if (keyis(k, "activeElement")) return wrapv(p, p->focus ? p->focus : d->body);
        if (keyis(k, "children")) return children_array(p, n, true);
        if (keyis(k, "forms") || keyis(k, "images") || keyis(k, "links") || keyis(k, "scripts")) {
            static cselector sel[1];
            const char *q = keyis(k, "forms") ? "form" : keyis(k, "images") ? "img" : keyis(k, "links") ? "a[href]" : "script";
            parse_selectors(q, sel, 1);
            jsobj *a = js_newarr(0);
            qs_walk(n, sel, 1, a, p);
            return jv_obj(a);
        }
        if (keyis(k, "hidden")) return JS_FALSE;
        if (keyis(k, "visibilityState")) return jv_str(js_strz("visible"));
        *found = false;
        return JS_UNDEF;
    }
    /* elementos */
    if ((keyis(k, "tagName") || keyis(k, "nodeName")) && n->ns == 1) return jv_str(js_strz(n->name));   /* SVG: caixa original */
    if (keyis(k, "namespaceURI")) return jv_str(js_strz(n->ns == 1 ? "http://www.w3.org/2000/svg" : "http://www.w3.org/1999/xhtml"));
    if (keyis(k, "tagName") || keyis(k, "nodeName")) {
        char up[64];
        int i = 0;
        for (; n->name[i] && i < 63; i++) up[i] = (char)toupper((u8)n->name[i]);
        up[i] = 0;
        return jv_str(js_strz(up));
    }
    if (keyis(k, "localName")) return jv_str(js_strz(n->name));
    if (keyis(k, "className")) return str_or_empty(node_attr(n, "class"));
    if (keyis(k, "classList")) return jv_obj(mkhost(p, CL_CLASSLIST, p->realm->p_classlist, n));
    if (keyis(k, "style")) return jv_obj(mkhost(p, CL_STYLE, p->realm->p_style, n));
    if (keyis(k, "dataset")) return jv_obj(mkhost(p, CL_DATASET, p->realm->p_object, n));
    if (keyis(k, "innerHTML") || keyis(k, "outerHTML")) {
        sbuf_t sb = {0};
        serialize(&sb, n, keyis(k, "outerHTML"));
        jsv r = jv_str(js_strn(sb.p ? sb.p : "", sb.len));
        sb_free(&sb);
        return r;
    }
    if (keyis(k, "children")) return children_array(p, n, true);
    if (keyis(k, "childElementCount")) { int c = 0; for (node_t *q = n->first; q; q = q->next) if (q->type == N_ELEM) c++; return jv_int(c); }
    if (keyis(k, "firstElementChild")) { for (node_t *q = n->first; q; q = q->next) if (q->type == N_ELEM) return wrapv(p, q); return JS_NULL; }
    if (keyis(k, "lastElementChild")) { for (node_t *q = n->last; q; q = q->prev) if (q->type == N_ELEM) return wrapv(p, q); return JS_NULL; }
    if (keyis(k, "nextElementSibling")) return wrapv(p, next_el(n));
    if (keyis(k, "previousElementSibling")) return wrapv(p, prev_el(n));
    if (keyis(k, "value")) return ctrl_value(n);
    if (keyis(k, "checked")) return jv_bool(ctrl_checked(n));
    if (keyis(k, "defaultValue")) return str_or_empty(node_attr(n, "value"));
    if (keyis(k, "selectedIndex")) return jv_int(n->ctrl ? n->ctrl->sel : 0);
    if (keyis(k, "type")) {
        const char *t = node_attr(n, "type");
        if (n->tag == T_INPUT) return jv_str(js_strz(t && *t ? t : "text"));
        if (n->tag == T_BUTTON) return jv_str(js_strz(t && *t ? t : "submit"));
        if (n->tag == T_SELECT) return jv_str(js_strz("select-one"));
        if (n->tag == T_TEXTAREA) return jv_str(js_strz("textarea"));
        return str_or_empty(t);
    }
    if (keyis(k, "href") || keyis(k, "src")) {
        const char *v = node_attr(n, k->d);
        if (!v) return jv_str(A[A_empty]);
        char abs[1024];
        url_resolve(d->base, v, abs, sizeof abs);
        return jv_str(js_strz(abs));
    }
    if (keyis(k, "form")) return wrapv(p, form_of(n));
    if (keyis(k, "elements") && n->tag == T_FORM) {
        static cselector sel[1];
        parse_selectors("input, select, textarea, button", sel, 1);
        jsobj *a = js_newarr(0);
        qs_walk(n, sel, 1, a, p);
        return jv_obj(a);
    }
    if (keyis(k, "options") && n->tag == T_SELECT) {
        static cselector sel[1];
        parse_selectors("option", sel, 1);
        jsobj *a = js_newarr(0);
        qs_walk(n, sel, 1, a, p);
        return jv_obj(a);
    }
    if (keyis(k, "offsetWidth") || keyis(k, "clientWidth") || keyis(k, "scrollWidth")) return geom(p, n, 0);
    if (keyis(k, "offsetHeight") || keyis(k, "clientHeight") || keyis(k, "scrollHeight")) return geom(p, n, 1);
    if (keyis(k, "offsetLeft")) return geom(p, n, 2);
    if (keyis(k, "offsetTop")) return geom(p, n, 3);
    if (keyis(k, "scrollTop") || keyis(k, "scrollLeft")) return jv_int(0);
    if (keyis(k, "tabIndex")) { const char *v = node_attr(n, "tabindex"); return jv_int(v ? atoi(v) : -1); }
    if (keyis(k, "attributes")) {
        jsobj *a = js_newarr(0);
        for (int i = 0; i < n->nattrs; i++) {
            jsobj *at = js_newobj(p->realm->p_object);
            js_defprop(at, A[A_name], jv_str(js_strz(n->attrs[i].name)), 0);
            js_defprop(at, A[A_value], jv_str(js_strz(n->attrs[i].value)), 0);
            js_arr_push(a, jv_obj(at));
        }
        return jv_obj(a);
    }
    for (int i = 0; bool_attrs[i]; i++) if (keyis(k, bool_attrs[i])) return jv_bool(node_attr(n, bool_attrs[i]) != NULL);
    if (keyis(k, "htmlFor")) return str_or_empty(node_attr(n, "for"));
    for (int i = 0; str_attrs[i]; i++) if (keyis(k, str_attrs[i])) return str_or_empty(node_attr(n, str_attrs[i]));
    if (k->len > 2 && k->d[0] == 'o' && k->d[1] == 'n') return get_prop_handler(p, jv_obj(o), js_strz(k->d + 2));
    *found = false;
    return JS_UNDEF;
}

static int node_set(jsobj *o, jsstr *k, jsv v) {
    js_page_t *p = o->u.host.page;
    node_t *n = o->u.host.node;
    doc_t *d = p->doc;
    if (k->len > 2 && k->d[0] == 'o' && k->d[1] == 'n' && n->type != N_TEXT) {
        set_prop_handler(p, jv_obj(o), js_strz(k->d + 2), v);
        return 1;
    }
    if (keyis(k, "textContent") || keyis(k, "innerText") || (n->type == N_TEXT && (keyis(k, "data") || keyis(k, "nodeValue")))) {
        if (n->type == N_DOC) return 1;
        jsstr *s = jv_isnullish(v) ? A[A_empty] : js_tostr(v);
        if (!s) return -1;
        set_text(p, n, s);
        return 1;
    }
    if (n->type == N_TEXT) return 0;
    if (n->type == N_DOC) {
        if (keyis(k, "title")) {
            jsstr *s = js_tostr(v);
            if (!s) return -1;
            strncpy(d->title, s->d, sizeof d->title - 1);
            d->title[sizeof d->title - 1] = 0;
            node_t *t = node_find_tag(d->head, T_TITLE);
            if (t) set_text(p, t, s);
            p->dirty = true;
            return 1;
        }
        if (keyis(k, "cookie")) {
            jsstr *s = js_tostr(v);
            if (!s) return -1;
            const char *semi = strchr(s->d, ';');
            int l = semi ? (int)(semi - s->d) : (int)s->len;
            int cl = (int)strlen(p->cookie);
            if (cl + l + 3 < (int)sizeof p->cookie) {
                if (cl) { strcat(p->cookie, "; "); cl += 2; }
                memcpy(p->cookie + cl, s->d, (usize)l);
                p->cookie[cl + l] = 0;
            }
            return 1;
        }
        if (keyis(k, "location")) { jsstr *s = js_tostr(v); if (!s) return -1; url_resolve(d->base, s->d, p->nav, sizeof p->nav); p->nav_pending = true; return 1; }
        return 0;
    }
    if (keyis(k, "innerHTML")) {
        jsstr *s = jv_isnullish(v) ? A[A_empty] : js_tostr(v);
        if (!s) return -1;
        js_mo_batch(1);
        remove_children(p, n);
        if (n->tag == T_SCRIPT || n->tag == T_STYLE || n->tag == T_TEXTAREA) {
            if (s->len) { node_t *t = new_text(d, s->d, (int)s->len); if (t) insert_before(n, t, NULL); }
        } else if (n->ns == 1) {
            html_svg_fragment(d, n, s->d, (int)s->len, &d->node_count);   /* conteúdo SVG é XML */
            for (node_t *c = n->first; c; c = c->next) js_mo_child(n, c, NULL, c->prev, c->next);
        } else {
            html_parse_fragment(d, n, s->d, (int)s->len);
            for (node_t *c = n->first; c; c = c->next) js_mo_child(n, c, NULL, c->prev, c->next);   /* o parser liga direto */
        }
        js_mo_batch(-1);
        for (node_t *c = n->first; c; c = c->next) after_insert(p, c);
        page_dirty(p, n);
        return 1;
    }
    if (keyis(k, "outerHTML")) {
        jsstr *s = js_tostr(v);
        if (!s) return -1;
        if (!n->parent) return 1;
        node_t *tmp = new_elem(d, "div", 3);
        if (!tmp) return 1;
        html_parse_fragment(d, tmp, s->d, (int)s->len);
        node_t *par = n->parent;
        js_mo_batch(1);
        while (tmp->first) { node_t *c = tmp->first; insert_before(par, c, n); after_insert(p, c); }
        detach(n);
        js_mo_batch(-1);
        page_dirty(p, par);
        return 1;
    }
    if (keyis(k, "className")) { jsstr *s = js_tostr(v); if (!s) return -1; set_attr(p, n, "class", s->d, (int)s->len); return 1; }
    if (keyis(k, "value")) { jsstr *s = jv_isnullish(v) ? A[A_empty] : js_tostr(v); if (!s) return -1; set_ctrl_value(p, n, s); return 1; }
    if (keyis(k, "checked")) { set_checked(p, n, js_truthy(v)); return 1; }
    if (keyis(k, "selectedIndex")) { if (n->ctrl) { n->ctrl->sel = (int)js_tointeger(v); p->dirty = true; } return 1; }
    if (keyis(k, "style")) { jsstr *s = js_tostr(v); if (!s) return -1; set_attr(p, n, "style", s->d, (int)s->len); return 1; }
    if (keyis(k, "htmlFor")) { jsstr *s = js_tostr(v); if (!s) return -1; set_attr(p, n, "for", s->d, (int)s->len); return 1; }
    for (int i = 0; bool_attrs[i]; i++) if (keyis(k, bool_attrs[i])) {
        if (js_truthy(v)) set_attr(p, n, bool_attrs[i], "", 0); else remove_attr(p, n, bool_attrs[i]);
        return 1;
    }
    if (n->tag == T_IFRAME && keyis(k, "srcdoc")) { jsstr *s = js_tostr(v); if (!s) return -1; set_attr(p, n, "srcdoc", s->d, (int)s->len); return 1; }
    if (n->tag == T_IMG && (keyis(k, "width") || keyis(k, "height"))) {
        double x = js_tonum(v);
        if (js_pending()) return -1;
        char b[16];
        snprintf(b, sizeof b, "%u", x == x && x >= 0 && x < 2147483647.0 ? (u32)x : 0u);
        set_attr(p, n, keyis(k, "width") ? "width" : "height", b, (int)strlen(b));
        return 1;
    }
    if (n->tag == T_IMG && (keyis(k, "srcset") || keyis(k, "sizes") || keyis(k, "crossOrigin") || keyis(k, "decoding") || keyis(k, "loading"))) {
        jsstr *s = js_tostr(v);
        if (!s) return -1;
        char an[16];
        snprintf(an, sizeof an, "%s", keyis(k, "crossOrigin") ? "crossorigin" : k->d);
        set_attr(p, n, an, s->d, (int)s->len);
        return 1;
    }
    if (keyis(k, "href") || keyis(k, "src") || keyis(k, "type") || keyis(k, "tabIndex")) {
        jsstr *s = js_tostr(v);
        if (!s) return -1;
        set_attr(p, n, keyis(k, "tabIndex") ? "tabindex" : k->d, s->d, (int)s->len);
        return 1;
    }
    for (int i = 0; str_attrs[i]; i++) if (keyis(k, str_attrs[i])) {
        jsstr *s = js_tostr(v);
        if (!s) return -1;
        set_attr(p, n, str_attrs[i], s->d, (int)s->len);
        return 1;
    }
    if (keyis(k, "scrollTop") || keyis(k, "scrollLeft")) return 1;
    return 0;
}

static jsv style_obj_get(jsobj *o, jsstr *k, bool *found) {
    node_t *n = o->u.host.node;
    *found = true;
    if (keyis(k, "cssText")) return str_or_empty(node_attr(n, "style"));
    if (keyis(k, "length")) {
        const char *s = node_attr(n, "style");
        int c = 0;
        for (; s && *s; s++) if (*s == ':') c++;
        return jv_int(c);
    }
    if (js_findprop(o->proto, k, NULL)) { *found = false; return JS_UNDEF; }
    if (o->u.host.extra == JS_TRUE) {
        /* getComputedStyle: valores mínimos do estilo calculado */
        style_t *st = n->st;
        char b[64];
        if (keyis(k, "display")) {
            static const char *dn[] = {"inline", "block", "inline-block", "list-item", "none", "table", "inline-table", "table-row-group", "table-row", "table-cell", "table-caption", "table-column"};
            return jv_str(js_strz(st && st->display < ARRAY_LEN(dn) ? dn[st->display] : "inline"));
        }
        if (keyis(k, "color") || keyis(k, "backgroundColor")) {
            u32 c = st ? (keyis(k, "color") ? st->color : st->bg) : 0;
            if (C_A(c) == 0) return jv_str(js_strz("rgba(0, 0, 0, 0)"));
            snprintf(b, sizeof b, "rgb(%u, %u, %u)", (c >> 16) & 255, (c >> 8) & 255, c & 255);
            return jv_str(js_strz(b));
        }
        if (keyis(k, "fontSize")) { snprintf(b, sizeof b, "%dpx", st ? (int)(st->size + 0.5f) : 16); return jv_str(js_strz(b)); }
        if (keyis(k, "visibility")) return jv_str(js_strz("visible"));
        jsv inl = style_get(n, "");
        (void)inl;
    }
    char prop[96];
    camel_to_kebab(k, prop, sizeof prop);
    return style_get(n, prop);
}

static int style_obj_set(jsobj *o, jsstr *k, jsv v) {
    js_page_t *p = o->u.host.page;
    node_t *n = o->u.host.node;
    if (o->u.host.extra == JS_TRUE) return 1;
    jsstr *s = jv_isnullish(v) ? A[A_empty] : js_tostr(v);
    if (!s) return -1;
    if (keyis(k, "cssText")) { set_attr(p, n, "style", s->d, (int)s->len); return 1; }
    char prop[96];
    camel_to_kebab(k, prop, sizeof prop);
    style_set(p, n, prop, s);
    return 1;
}

NATIVE(style_method) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_STYLE)) return js_throw_err(E_TYPE, "CSSStyleDeclaration: objeto incompatível");
    jsobj *o = jv_o(thisv);
    jsstr *prop = js_tostr(ARG(0));
    if (!prop) return JS_EXC;
    switch (self->u.nat.magic) {
    case 0: { jsstr *v = js_tostr(ARG(1)); if (!v) return JS_EXC; style_set(o->u.host.page, o->u.host.node, prop->d, v); return JS_UNDEF; }
    case 1: return style_get(o->u.host.node, prop->d);
    case 2: { jsv old = style_get(o->u.host.node, prop->d); style_set(o->u.host.page, o->u.host.node, prop->d, A[A_empty]); return old; }
    }
    return JS_UNDEF;
}

/* classList */
static int cls_tokens(const char *s, char tok[][64], int max) {
    int n = 0;
    while (s && *s && n < max) {
        while (*s && isspace((u8)*s)) s++;
        if (!*s) break;
        int k = 0;
        while (*s && !isspace((u8)*s)) { if (k < 63) tok[n][k++] = *s; s++; }
        tok[n][k] = 0;
        bool dup = false;
        for (int i = 0; i < n; i++) if (!strcmp(tok[i], tok[n])) dup = true;
        if (!dup) n++;
    }
    return n;
}

static void cls_write(js_page_t *p, node_t *n, char tok[][64], int nt) {
    sbuf_t sb = {0};
    for (int i = 0; i < nt; i++) { if (i) sb_putc(&sb, ' '); sb_puts(&sb, tok[i]); }
    set_attr(p, n, "class", sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
}

NATIVE(cl_method) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_CLASSLIST)) return js_throw_err(E_TYPE, "DOMTokenList: objeto incompatível");
    js_page_t *p = jv_o(thisv)->u.host.page;
    node_t *n = jv_o(thisv)->u.host.node;
    static char tok[64][64];
    int nt = cls_tokens(node_attr(n, "class"), tok, 64);
    int op = self->u.nat.magic;
    if (op == 5) { /* item */
        int i = (int)js_tointeger(ARG(0));
        return i >= 0 && i < nt ? jv_str(js_strz(tok[i])) : JS_NULL;
    }
    if (op == 6) return str_or_empty(node_attr(n, "class"));
    jsstr *c = js_tostr(ARG(0));
    if (!c) return JS_EXC;
    int idx = -1;
    for (int i = 0; i < nt; i++) if (!strcmp(tok[i], c->d)) idx = i;
    switch (op) {
    case 0: /* add */
        for (int a = 0; a < argc; a++) {
            jsstr *x = js_tostr(argv[a]);
            if (!x) return JS_EXC;
            bool have = false;
            for (int i = 0; i < nt; i++) if (!strcmp(tok[i], x->d)) have = true;
            if (!have && nt < 64 && x->len) { strncpy(tok[nt], x->d, 63); tok[nt][63] = 0; nt++; }
        }
        cls_write(p, n, tok, nt);
        return JS_UNDEF;
    case 1: /* remove */
        for (int a = 0; a < argc; a++) {
            jsstr *x = js_tostr(argv[a]);
            if (!x) return JS_EXC;
            for (int i = 0; i < nt; i++) if (!strcmp(tok[i], x->d)) { memmove(tok[i], tok[i + 1], (usize)(nt - i - 1) * 64); nt--; i--; }
        }
        cls_write(p, n, tok, nt);
        return JS_UNDEF;
    case 2: { /* toggle */
        bool want = argc > 1 ? js_truthy(argv[1]) : idx < 0;
        if (want && idx < 0 && nt < 64) { strncpy(tok[nt], c->d, 63); tok[nt][63] = 0; nt++; }
        else if (!want && idx >= 0) { memmove(tok[idx], tok[idx + 1], (usize)(nt - idx - 1) * 64); nt--; }
        cls_write(p, n, tok, nt);
        return jv_bool(want);
    }
    case 3: return jv_bool(idx >= 0);   /* contains */
    case 4: { /* replace */
        jsstr *nw = js_tostr(ARG(1));
        if (!nw) return JS_EXC;
        if (idx < 0) return JS_FALSE;
        strncpy(tok[idx], nw->d, 63);
        tok[idx][63] = 0;
        cls_write(p, n, tok, nt);
        return JS_TRUE;
    }
    }
    return JS_UNDEF;
}

static jsv classlist_get(jsobj *o, jsstr *k, bool *found) {
    *found = false;
    if (keyis(k, "length") || keyis(k, "value")) {
        static char tok[64][64];
        const char *v = node_attr(o->u.host.node, "class");
        *found = true;
        if (keyis(k, "value")) return str_or_empty(v);
        return jv_int(cls_tokens(v, tok, 64));
    }
    u32 idx;
    if (js_str_isindex(k, &idx)) {
        static char tok[64][64];
        int nt = cls_tokens(node_attr(o->u.host.node, "class"), tok, 64);
        *found = true;
        return (int)idx < nt ? jv_str(js_strz(tok[idx])) : JS_UNDEF;
    }
    return JS_UNDEF;
}

/* dataset */
static void data_attr_name(jsstr *k, char *out, int cap) {
    int j = snprintf(out, (usize)cap, "data-");
    for (u32 i = 0; i < k->len && j < cap - 2; i++) {
        char c = k->d[i];
        if (c >= 'A' && c <= 'Z') { out[j++] = '-'; out[j++] = (char)(c + 32); }
        else out[j++] = c;
    }
    out[j] = 0;
}

static jsv dataset_get(jsobj *o, jsstr *k, bool *found) {
    char an[96];
    data_attr_name(k, an, sizeof an);
    const char *v = node_attr(o->u.host.node, an);
    *found = v != NULL;
    return v ? jv_str(js_strz(v)) : JS_UNDEF;
}

/* location */
static void url_parts(const char *url, char *proto, char *host, char *path, char *search, char *hash) {
    proto[0] = host[0] = path[0] = search[0] = hash[0] = 0;
    const char *c = strstr(url, "://");
    const char *rest = url;
    if (c) {
        int l = (int)(c - url) + 1;
        snprintf(proto, 32, "%.*s", l, url);
        rest = c + 3;
        const char *sl = rest;
        while (*sl && *sl != '/' && *sl != '?' && *sl != '#') sl++;
        snprintf(host, 256, "%.*s", (int)(sl - rest), rest);
        rest = sl;
    } else if (strchr(url, ':')) {
        const char *q = strchr(url, ':');
        snprintf(proto, 32, "%.*s", (int)(q - url) + 1, url);
        rest = q + 1;
    }
    const char *h = strchr(rest, '#');
    const char *q = strchr(rest, '?');
    if (q && h && q > h) q = NULL;
    int plen = (int)(q ? q - rest : h ? h - rest : (long long)strlen(rest));
    snprintf(path, 1024, "%.*s", plen, rest);
    if (q) snprintf(search, 1024, "%.*s", (int)(h ? h - q : (long long)strlen(q)), q);
    if (h && h[1]) snprintf(hash, 512, "%s", h);
}

static jsv location_get(jsobj *o, jsstr *k, bool *found) {
    js_page_t *p = o->u.host.page;
    static char proto[32], host[256], path[1024], search[1024], hash[512];
    url_parts(p->url, proto, host, path, search, hash);
    *found = true;
    if (keyis(k, "href")) return jv_str(js_strz(p->url));
    if (keyis(k, "protocol")) return jv_str(js_strz(proto));
    if (keyis(k, "host")) return jv_str(js_strz(host));
    if (keyis(k, "hostname")) { char *c = strchr(host, ':'); if (c) *c = 0; return jv_str(js_strz(host)); }
    if (keyis(k, "port")) { char *c = strchr(host, ':'); return jv_str(js_strz(c ? c + 1 : "")); }
    if (keyis(k, "pathname")) return jv_str(js_strz(path[0] ? path : "/"));
    if (keyis(k, "search")) return jv_str(js_strz(search));
    if (keyis(k, "hash")) return jv_str(js_strz(hash));
    if (keyis(k, "origin")) return jv_str(js_strz(p->origin));
    *found = false;
    return JS_UNDEF;
}

static int location_set(jsobj *o, jsstr *k, jsv v) {
    js_page_t *p = o->u.host.page;
    jsstr *s = js_tostr(v);
    if (!s) return -1;
    if (keyis(k, "href")) { page_navigate(p, s->d, false); return 1; }
    if (keyis(k, "hash")) {
        char base[1024], u[1100];
        strncpy(base, p->url, sizeof base - 1);
        base[sizeof base - 1] = 0;
        char *h = strchr(base, '#');
        if (h) *h = 0;
        snprintf(u, sizeof u, "%s%s%s", base, s->d[0] == '#' ? "" : "#", s->d);
        page_navigate(p, u, false);
        return 1;
    }
    if (keyis(k, "search")) {
        char base[1024];
        strncpy(base, p->url, sizeof base - 1);
        base[sizeof base - 1] = 0;
        char *q = strchr(base, '?');
        if (!q) q = strchr(base, '#');
        if (q) *q = 0;
        snprintf(p->nav, sizeof p->nav, "%s%s%s", base, s->d[0] == '?' ? "" : "?", s->d);
        p->nav_pending = true;
        return 1;
    }
    return 1;
}

/* ============================================================ History API e navegação por fragmento */
static bool same_doc_url(const char *a, const char *b) {
    const char *ha = strchr(a, '#'), *hb = strchr(b, '#');
    usize la = ha ? (usize)(ha - a) : strlen(a), lb = hb ? (usize)(hb - b) : strlen(b);
    return la == lb && !strncmp(a, b, la);
}

static void hist_op(js_page_t *p, int op, int arg, const char *url) {
    if (p->nhops < (int)ARRAY_LEN(p->hops)) {
        p->hops[p->nhops].op = op;
        p->hops[p->nhops].arg = arg;
        snprintf(p->hops[p->nhops].url, sizeof p->hops[0].url, "%s", url ? url : "");
        p->nhops++;
    }
}

/* entrada nova (push) ou trocada (replace) no histórico */
/* histórico local começa com a entrada do documento; chamar antes de trocar p->url */
static void hist_init(js_page_t *p) {
    if (p->hhost || p->hln) return;
    snprintf(p->hl[0].url, sizeof p->hl[0].url, "%s", p->url);
    p->hl[0].sid = -1;
    p->hln = 1;
    p->hlpos = 0;
}

static void hist_record(js_page_t *p, const char *url, int sid, bool replace) {
    if (p->hhost) { hist_op(p, replace ? 2 : 1, sid, url); return; }
    hist_init(p);
    if (!replace) {
        p->hln = p->hlpos + 1;   /* descarta as entradas à frente */
        if (p->hln == (int)ARRAY_LEN(p->hl)) { memmove(p->hl, p->hl + 1, sizeof p->hl[0] * (ARRAY_LEN(p->hl) - 1)); p->hln--; p->hlpos--; }
        p->hlpos = p->hln++;
    }
    snprintf(p->hl[p->hlpos].url, sizeof p->hl[0].url, "%s", url);
    p->hl[p->hlpos].sid = sid;
}

/* rola até a âncora do fragmento (id ou name) */
static void scroll_to_fragment(js_page_t *p, const char *url) {
    const char *h = strchr(url, '#');
    if (!h || !h[1]) { p->scroll_req = 0; return; }
    char frag[256];
    url_decode_path(h + 1, frag, sizeof frag);
    node_t *n = NULL, *byname = NULL;
    for (node_t *c = p->doc->root; c && !n; ) {
        if (c->type == N_ELEM) {
            const char *v = node_attr(c, "id");
            if (v && !strcmp(v, frag)) n = c;
            else if (!byname && c->tag == T_A && (v = node_attr(c, "name")) && !strcmp(v, frag)) byname = c;
        }
        if (c->first) c = c->first;
        else { while (c && !c->next) c = c->parent; if (c) c = c->next; }
    }
    if (!n) n = byname;
    if (!n) return;
    rect_acc r = node_rect(p, n);
    if (r.any) p->scroll_req = r.y0;
}

/* tarefa: popstate e/ou hashchange; data = [estado, url antiga, url nova, popstate?] */
NATIVE(hist_task) {
    UNUSED_ARGS;
    jsobj *a = jv_o(self->u.nat.data);
    js_page_t *p = page_of_global(js_arr_get(a, 4));   /* a janela dona, não a que está rodando */
    if (!p) return JS_UNDEF;
    jsv state = js_arr_get(a, 0), ou = js_arr_get(a, 1), nu = js_arr_get(a, 2);
    if (js_truthy(js_arr_get(a, 3))) {
        jsobj *ev = new_event(p, js_intern("popstate", -1), false);
        js_defprop(ev, js_intern("state", -1), state, 0);
        js_defprop(ev, js_intern("hasUAVisualTransition", -1), JS_FALSE, 0);
        dispatch(p, NULL, ev);
    }
    const char *o = jv_isstr(ou) ? jv_s(ou)->d : "", *n = jv_isstr(nu) ? jv_s(nu)->d : "";
    const char *ho = strchr(o, '#'), *hn = strchr(n, '#');
    if (same_doc_url(o, n) && strcmp(ho ? ho : "", hn ? hn : "")) {
        jsobj *ev = new_event(p, js_intern("hashchange", -1), false);
        js_defprop(ev, js_intern("oldURL", -1), ou, 0);
        js_defprop(ev, js_intern("newURL", -1), nu, 0);
        dispatch(p, NULL, ev);
    }
    js_run_jobs();
    p->dirty = true;
    return JS_UNDEF;
}

static void hist_events(js_page_t *p, const char *old, const char *nu, jsv state, bool pop) {
    jsv a[5] = { state, jv_str(js_strz(old)), jv_str(js_strz(nu)), jv_bool(pop), jv_obj(p->realm->global) };
    jsobj *t = js_newnative(hist_task, "historico", 0, 0);
    t->u.nat.data = jv_obj(js_arr_from(a, 5));
    add_timer(p, jv_obj(t), 0, false, 0, NULL);
}

static jsv state_of(js_page_t *p, int sid) {
    if (sid < 0 || !p->hstates || (u32)sid >= p->hstates->alen) return JS_NULL;
    jsv c = js_structured_clone(js_arr_get(p->hstates, (u32)sid));
    if (c == JS_EXC) { js_catch(); return JS_NULL; }
    return c;
}

/* volta/avança para uma entrada do mesmo documento: URL, estado e eventos */
void js_page_hist_restore(js_page_t *p, const char *url, int sid) {
    if (!p) return;
    jsrealm *saved = J.realm;
    J.realm = p->realm;
    char old[1024];
    snprintf(old, sizeof old, "%s", p->url);
    snprintf(p->url, sizeof p->url, "%s", url);
    p->hcur = sid;
    if (strchr(url, '#') && sid < 0) scroll_to_fragment(p, url);
    hist_events(p, old, url, state_of(p, sid), true);
    J.realm = saved;
}

/* navegação pedida pelo script: fragmento do mesmo documento não recarrega */
static void page_navigate(js_page_t *p, const char *rel, bool replace) {
    char url[1024];
    url_resolve(p->doc->base, rel, url, sizeof url);
    if (strchr(url, '#') && same_doc_url(url, p->url)) {
        char old[1024];
        snprintf(old, sizeof old, "%s", p->url);
        if (!strcmp(old, url)) { scroll_to_fragment(p, url); return; }
        hist_init(p);
        snprintf(p->url, sizeof p->url, "%s", url);
        p->hcur = -1;
        hist_record(p, url, -1, replace);
        scroll_to_fragment(p, url);
        hist_events(p, old, url, JS_NULL, true);
        return;
    }
    snprintf(p->nav, sizeof p->nav, "%s", url);
    p->nav_pending = true;
}

static bool hist_same_origin(js_page_t *p, const char *url) {
    usize n = strlen(p->origin);
    if (!strcmp(p->origin, "null") || !n) return same_doc_url(url, p->url);
    return !strncmp(url, p->origin, n) && (url[n] == '/' || url[n] == 0 || url[n] == '?' || url[n] == '#');
}

/* magic: 0 pushState 1 replaceState 2 go 3 back 4 forward 5 length 6 state 7 scrollRestoration(get) 8 (set) */
NATIVE(hist_method) {
    UNUSED_ARGS;
    js_page_t *p = NULL;
    for (js_page_t *q = pages; q && !p; q = q->next) if (q->hobj && jv_obj(q->hobj) == thisv) p = q;
    if (!p) p = cur_page();
    if (!p) return JS_UNDEF;
    int m = self->u.nat.magic;
    switch (m) {
    case 0: case 1: {
        if (argc < 2) return js_throw_err(E_TYPE, "%s: são necessários 2 argumentos", m ? "replaceState" : "pushState");
        jsv st = js_structured_clone(ARG(0));
        if (st == JS_EXC) return JS_EXC;
        char url[1024];
        snprintf(url, sizeof url, "%s", p->url);
        if (!jv_isnullish(ARG(2))) {
            jsstr *u = js_tostr(ARG(2));
            if (!u) return JS_EXC;
            if (!web_url_parse(u->d, p->url, url, sizeof url)) return js_throw_dom("SecurityError", "history: URL inválido");
            if (!hist_same_origin(p, url)) return js_throw_dom("SecurityError", "history: o URL precisa ser da mesma origem");
        }
        if (!p->hstates) p->hstates = js_newarr(0);
        js_arr_push(p->hstates, st);
        int sid = (int)p->hstates->alen - 1;
        hist_init(p);
        snprintf(p->url, sizeof p->url, "%s", url);
        p->hcur = sid;
        hist_record(p, url, sid, m == 1);
        return JS_UNDEF;
    }
    case 2: case 3: case 4: {
        int delta = m == 3 ? -1 : m == 4 ? 1 : (argc ? js_toint32(ARG(0)) : 0);
        if (js_pending()) return JS_EXC;
        if (!delta) { p->nav_pending = true; p->nav_reload = true; snprintf(p->nav, sizeof p->nav, "%s", p->url); return JS_UNDEF; }
        if (p->hhost) { hist_op(p, 3, delta, NULL); return JS_UNDEF; }
        if (!p->hln) return JS_UNDEF;
        int t = p->hlpos + delta;
        if (t < 0 || t >= p->hln) return JS_UNDEF;
        p->hlpos = t;
        js_page_hist_restore(p, p->hl[t].url, p->hl[t].sid);
        return JS_UNDEF;
    }
    case 5: return jv_int(p->hhost ? (p->hlen > 0 ? p->hlen : 1) : (p->hln ? p->hln : 1));
    case 6: return state_of(p, p->hcur);
    case 7: return jv_str(js_strz(p->hscroll_manual ? "manual" : "auto"));
    case 8: {
        jsstr *v = js_tostr(ARG(0));
        if (!v) return JS_EXC;
        if (!strcmp(v->d, "manual")) p->hscroll_manual = true; else if (!strcmp(v->d, "auto")) p->hscroll_manual = false;
        return JS_UNDEF;
    }
    }
    return JS_UNDEF;
}

void js_page_set_history_host(js_page_t *p, bool host) { if (p) p->hhost = host; }
void js_page_set_hist_len(js_page_t *p, int n) { if (p) p->hlen = n; }
bool js_page_take_hist_op(js_page_t *p, int *op, int *arg, char *url, int cap) {
    if (!p || !p->nhops) return false;
    *op = p->hops[0].op;
    *arg = p->hops[0].arg;
    snprintf(url, (usize)cap, "%s", p->hops[0].url);
    memmove(p->hops, p->hops + 1, sizeof p->hops[0] * (usize)(p->nhops - 1));
    p->nhops--;
    return true;
}

NATIVE(loc_method) {
    UNUSED_ARGS;
    js_page_t *p = jv_isobj(thisv) && jv_o(thisv)->cls == CL_LOCATION ? jv_o(thisv)->u.host.page : cur_page();
    if (!p) return JS_UNDEF;
    int op = self->u.nat.magic;
    if (op == 2) { p->nav_pending = true; p->nav_reload = true; strncpy(p->nav, p->url, sizeof p->nav - 1); return JS_UNDEF; }
    if (op == 3) return jv_str(js_strz(p->url));
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    page_navigate(p, s->d, op == 1);   /* assign / replace */
    return JS_UNDEF;
}

static jsv storage_get(jsobj *o, jsstr *k, bool *found) {
    *found = false;
    if (js_findprop(o->proto, k, NULL)) return JS_UNDEF;
    sarea_t *a = storage_area(o);
    if (!a) return JS_UNDEF;
    if (keyis(k, "length")) { *found = true; return jv_int(a->n); }
    sitem_t *i = sfind(a, k->d);
    if (!i) return JS_UNDEF;
    *found = true;
    return jv_str(js_strz(i->v));
}

static int storage_set(jsobj *o, jsstr *k, jsv v) {
    if (js_findprop(o->proto, k, NULL)) return 0;
    sarea_t *a = storage_area(o);
    jsstr *s = js_tostr(v);
    if (!s) return -1;
    if (a) sset(a, k->d, s->d);
    return 1;
}

jsv js_dom_get(jsobj *o, jsstr *k, bool *found) {
    *found = false;
    if (!o->u.host.page) return JS_UNDEF;
    switch (o->cls) {
    case CL_NODE: return node_get(o, k, found);
    case CL_STYLE: return style_obj_get(o, k, found);
    case CL_CLASSLIST: return classlist_get(o, k, found);
    case CL_DATASET: return dataset_get(o, k, found);
    case CL_LOCATION: return location_get(o, k, found);
    case CL_STORAGE: return storage_get(o, k, found);
    }
    return JS_UNDEF;
}

int js_dom_set(jsobj *o, jsstr *k, jsv v) {
    if (!o->u.host.page) return 0;
    switch (o->cls) {
    case CL_NODE: return node_set(o, k, v);
    case CL_STYLE: return style_obj_set(o, k, v);
    case CL_DATASET: {
        char an[96];
        data_attr_name(k, an, sizeof an);
        jsstr *s = js_tostr(v);
        if (!s) return -1;
        set_attr(o->u.host.page, o->u.host.node, an, s->d, (int)s->len);
        return 1;
    }
    case CL_LOCATION: return location_set(o, k, v);
    case CL_STORAGE: return storage_set(o, k, v);
    }
    return 0;
}

/* ============================================================ métodos de nós */
static node_t *this_node(jsv thisv, const char *m) {
    node_t *n = node_of(thisv);
    if (!n) js_throw_err(E_TYPE, "%s chamado em algo que não é nó", m);
    return n;
}

static node_t *arg_node(jsv v, const char *m) {
    node_t *n = node_of(v);
    if (!n) js_throw_err(E_TYPE, "%s: o argumento não é um Node", m);
    return n;
}

static node_t *to_insertable(js_page_t *p, jsv v) {
    node_t *n = node_of(v);
    if (n) return n;
    jsstr *s = js_tostr(v);
    if (!s) return NULL;
    return new_text(p->doc, s->d, (int)s->len);
}

static bool is_frag(node_t *n) { return n->type == N_ELEM && n->name && !strcmp(n->name, "#fragment"); }

static jsv do_insert(js_page_t *p, node_t *par, node_t *n, node_t *ref) {
    if (n == par || is_desc(par, n)) return js_throw_err(E_ERROR, "HierarchyRequestError: o nó não pode ser inserido aqui");
    if (ref && ref->parent != par) return js_throw_err(E_ERROR, "NotFoundError: o nó de referência não é filho deste nó");
    if (is_frag(n)) {
        while (n->first) { node_t *c = n->first; insert_before(par, c, ref); after_insert(p, c); }
        return JS_UNDEF;
    }
    insert_before(par, n, ref);
    after_insert(p, n);
    return JS_UNDEF;
}

NATIVE(nd_append) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *par = this_node(thisv, "appendChild");
    if (!par) return JS_EXC;
    node_t *n = arg_node(ARG(0), "appendChild");
    if (!n) return JS_EXC;
    TRYV(do_insert(p, par, n, NULL));
    return ARG(0);
}

NATIVE(nd_insertbefore) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *par = this_node(thisv, "insertBefore");
    if (!par) return JS_EXC;
    node_t *n = arg_node(ARG(0), "insertBefore");
    if (!n) return JS_EXC;
    node_t *ref = jv_isnullish(ARG(1)) ? NULL : node_of(ARG(1));
    TRYV(do_insert(p, par, n, ref));
    return ARG(0);
}

NATIVE(nd_removechild) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *par = this_node(thisv, "removeChild");
    if (!par) return JS_EXC;
    node_t *n = arg_node(ARG(0), "removeChild");
    if (!n) return JS_EXC;
    if (n->parent != par) return js_throw_err(E_ERROR, "NotFoundError: o nó não é filho deste nó");
    bool st = contains_style(n);
    detach(n);
    if (st && p->doc->sheet) { css_free(p->doc->sheet); p->doc->sheet = NULL; }
    page_dirty(p, par);
    return ARG(0);
}

NATIVE(nd_replacechild) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *par = this_node(thisv, "replaceChild");
    if (!par) return JS_EXC;
    node_t *nw = arg_node(ARG(0), "replaceChild"), *old = nw ? arg_node(ARG(1), "replaceChild") : NULL;
    if (!old) return JS_EXC;
    if (old->parent != par) return js_throw_err(E_ERROR, "NotFoundError: o nó não é filho deste nó");
    node_t *ref = old->next == nw ? nw->next : old->next;
    detach(old);
    TRYV(do_insert(p, par, nw, ref));
    return ARG(1);
}

NATIVE(nd_remove) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *n = this_node(thisv, "remove");
    if (!n) return JS_EXC;
    node_t *par = n->parent;
    if (!par) return JS_UNDEF;
    bool st = contains_style(n);
    detach(n);
    if (st && p->doc->sheet) { css_free(p->doc->sheet); p->doc->sheet = NULL; }
    page_dirty(p, par);
    return JS_UNDEF;
}

/* append/prepend/before/after/replaceWith (aceitam nós e strings) */
NATIVE(nd_multi) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *n = this_node(thisv, "append");
    if (!n) return JS_EXC;
    int op = self->u.nat.magic;
    node_t *par = op <= 1 ? n : n->parent;
    if (!par) return JS_UNDEF;
    node_t *ref = op == 0 ? NULL : op == 1 ? n->first : op == 2 ? n : n->next;
    for (int i = 0; i < argc; i++) {
        node_t *c = to_insertable(p, argv[i]);
        if (!c) return JS_EXC;
        if (c == ref) { ref = ref->next; }
        TRYV(do_insert(p, par, c, ref));
    }
    if (op == 4) { detach(n); page_dirty(p, par); }
    return JS_UNDEF;
}

static node_t *clone(doc_t *d, node_t *n, bool deep) {
    node_t *c = new_node(d, n->type);
    if (!c) return NULL;
    c->tag = n->tag;
    c->name = n->name;
    if (n->type == N_TEXT) { c->text = arena_strndup(&d->arena, n->text, n->len); c->len = n->len; }
    if (n->nattrs) {
        c->attrs = arena_alloc(&d->arena, sizeof(attr_t) * (usize)n->nattrs);
        if (c->attrs) { memcpy(c->attrs, n->attrs, sizeof(attr_t) * (usize)n->nattrs); c->nattrs = c->acap = n->nattrs; }
    }
    if (deep) for (node_t *k = n->first; k; k = k->next) { node_t *kc = clone(d, k, true); if (kc) insert_before(c, kc, NULL); }
    return c;
}

NATIVE(nd_clone) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *n = this_node(thisv, "cloneNode");
    if (!n) return JS_EXC;
    if (n->type == N_DOC) return js_throw_err(E_ERROR, "NotSupportedError: não é possível clonar o documento");
    return wrapv(p, clone(p->doc, n, js_truthy(ARG(0))));
}

NATIVE(nd_contains) {
    UNUSED_ARGS;
    node_t *n = this_node(thisv, "contains");
    if (!n) return JS_EXC;
    node_t *o = node_of(ARG(0));
    return jv_bool(o && is_desc(o, n));
}

NATIVE(nd_haschild) {
    UNUSED_ARGS;
    node_t *n = this_node(thisv, "hasChildNodes");
    if (!n) return JS_EXC;
    return jv_bool(n->first != NULL);
}

NATIVE(el_attr) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *n = this_node(thisv, "getAttribute");
    if (!n) return JS_EXC;
    if (n->type != N_ELEM) return JS_NULL;
    jsstr *k = js_tostr(ARG(0));
    if (!k) return JS_EXC;
    char low[64];
    int i = 0;
    for (; i < (int)k->len && i < 63; i++) low[i] = (char)tolower((u8)k->d[i]);
    low[i] = 0;
    switch (self->u.nat.magic) {
    case 0: { const char *v = node_attr(n, low); return v ? jv_str(js_strz(v)) : JS_NULL; }
    case 1: {
        jsstr *v = js_tostr(ARG(1));
        if (!v) return JS_EXC;
        set_attr(p, n, low, v->d, (int)v->len);
        if (!strcmp(low, "value") && n->ctrl && (n->ctrl->kind == C_BUTTON || n->ctrl->kind == C_SUBMIT || n->ctrl->kind == C_RESET)) {
            strncpy(n->ctrl->label, v->d, sizeof n->ctrl->label - 1);
        }
        return JS_UNDEF;
    }
    case 2: remove_attr(p, n, low); return JS_UNDEF;
    case 3: return jv_bool(node_attr(n, low) != NULL);
    case 4: {
        bool on = argc > 1 ? js_truthy(argv[1]) : node_attr(n, low) == NULL;
        if (on) set_attr(p, n, low, "", 0); else remove_attr(p, n, low);
        return jv_bool(on);
    }
    }
    return JS_UNDEF;
}

NATIVE(el_query) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *n = this_node(thisv, "querySelector");
    if (!n) return JS_EXC;
    return query(p, n, ARG(0), self->u.nat.magic == 1);
}

NATIVE(el_bytag) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *n = this_node(thisv, "getElementsByTagName");
    if (!n) return JS_EXC;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    sbuf_t q = {0};
    int op = self->u.nat.magic;
    if (op == 0) sb_puts(&q, s->len ? s->d : "*");
    else if (op == 1) {
        static char tok[16][64];
        int nt = cls_tokens(s->d, tok, 16);
        for (int i = 0; i < nt; i++) { sb_putc(&q, '.'); sb_puts(&q, tok[i]); }
        if (!nt) { sb_free(&q); return jv_obj(js_newarr(0)); }
    } else { sb_puts(&q, "[name=\""); sb_puts(&q, s->d); sb_puts(&q, "\"]"); }
    jsv r = query(p, n, jv_str(js_strz(q.p)), true);
    sb_free(&q);
    return r;
}

NATIVE(el_matches) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *n = this_node(thisv, "matches");
    if (!n) return JS_EXC;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    static cselector sel[16];
    int ns = parse_selectors(s->d, sel, 16);
    if (ns <= 0) return js_throw_err(E_SYNTAX, "'%s' não é um seletor válido", s->d);
    if (self->u.nat.magic == 0) return jv_bool(matches_any(sel, ns, n));
    for (node_t *q = n; q; q = q->parent) if (q->type == N_ELEM && matches_any(sel, ns, q)) return wrapv(p, q);
    return JS_NULL;
}

NATIVE(el_rect) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *n = this_node(thisv, "getBoundingClientRect");
    if (!n) return JS_EXC;
    rect_acc r = node_rect(p, n);
    jsobj *o = js_newobj(p->realm->p_object);
    int y0 = r.any ? r.y0 - p->scroll : 0, y1 = r.any ? r.y1 - p->scroll : 0;
    int x0 = r.any ? r.x0 : 0, x1 = r.any ? r.x1 : 0;
    js_defn(o, "x", jv_int(x0), 0);
    js_defn(o, "y", jv_int(y0), 0);
    js_defn(o, "left", jv_int(x0), 0);
    js_defn(o, "top", jv_int(y0), 0);
    js_defn(o, "right", jv_int(x1), 0);
    js_defn(o, "bottom", jv_int(y1), 0);
    js_defn(o, "width", jv_int(x1 - x0), 0);
    js_defn(o, "height", jv_int(y1 - y0), 0);
    return jv_obj(o);
}

NATIVE(el_misc) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *n = this_node(thisv, "focus");
    if (!n) return JS_EXC;
    switch (self->u.nat.magic) {
    case 0: p->focus = n; break;                                  /* focus */
    case 1: if (p->focus == n) p->focus = NULL; break;            /* blur */
    case 2: {                                                     /* click */
        jsobj *ev = new_event(p, js_intern("click", -1), true);
        dispatch(p, n, ev);
        break;
    }
    case 3: {                                                     /* scrollIntoView */
        rect_acc r = node_rect(p, n);
        if (r.any) p->scroll_req = MAX(0, r.y0 - 10);
        break;
    }
    case 4: if (n->tag == T_FORM) p->submit_req = n; break;       /* submit */
    case 5: {                                                     /* reset */
        doc_t *d = p->doc;
        for (int i = 0; i < d->nctrls; i++) {
            ctrl_t *c = d->ctrls[i];
            if (c->form != n) continue;
            const char *v = node_attr(c->node, "value");
            if (c->kind == C_TEXT || c->kind == C_PASSWORD) strncpy(c->buf, v ? v : "", (usize)c->cap - 1);
            if (c->kind == C_CHECK || c->kind == C_RADIO) c->checked = node_attr(c->node, "checked") != NULL;
        }
        p->dirty = true;
        break;
    }
    case 6: {                                                     /* requestSubmit */
        jsobj *ev = new_event(p, js_intern("submit", -1), true);
        if (!dispatch(p, n, ev)) p->submit_req = n;
        break;
    }
    case 7: {                                                     /* select (texto) */
        break;
    }
    }
    return JS_UNDEF;
}

NATIVE(el_adjacent) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    node_t *n = this_node(thisv, "insertAdjacentHTML");
    if (!n) return JS_EXC;
    jsstr *pos = js_tostr(ARG(0));
    if (!pos) return JS_EXC;
    jsv what = ARG(1);
    node_t *tmp = new_elem(p->doc, "div", 3);
    if (!tmp) return JS_UNDEF;
    int op = self->u.nat.magic;   /* 0 HTML, 1 elemento, 2 texto */
    if (op == 0) { jsstr *s = js_tostr(what); if (!s) return JS_EXC; html_parse_fragment(p->doc, tmp, s->d, (int)s->len); }
    else if (op == 1) { node_t *e = arg_node(what, "insertAdjacentElement"); if (!e) return JS_EXC; insert_before(tmp, e, NULL); }
    else { jsstr *s = js_tostr(what); if (!s) return JS_EXC; node_t *t = new_text(p->doc, s->d, (int)s->len); if (t) insert_before(tmp, t, NULL); }
    node_t *par, *ref;
    if (!strcasecmp(pos->d, "beforebegin")) { par = n->parent; ref = n; }
    else if (!strcasecmp(pos->d, "afterbegin")) { par = n; ref = n->first; }
    else if (!strcasecmp(pos->d, "beforeend")) { par = n; ref = NULL; }
    else if (!strcasecmp(pos->d, "afterend")) { par = n->parent; ref = n->next; }
    else return js_throw_err(E_SYNTAX, "Posição inválida: '%s'", pos->d);
    if (!par) return JS_UNDEF;
    while (tmp->first) { node_t *c = tmp->first; insert_before(par, c, ref); after_insert(p, c); }
    page_dirty(p, par);
    return op == 1 ? what : JS_UNDEF;
}

/* document */
NATIVE(doc_byid) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    if (!p) return JS_NULL;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    node_t *root = node_of(thisv);
    if (!root) root = p->doc->root;
    for (node_t *n = root->first; n;) {
        if (n->type == N_ELEM) {
            const char *v = node_attr(n, "id");
            if (v && !strcmp(v, s->d)) return wrapv(p, n);
        }
        if (n->first) n = n->first;
        else {
            while (n && !n->next && n != root) n = n->parent;
            if (!n || n == root) break;
            n = n->next;
        }
    }
    return JS_NULL;
}

NATIVE(doc_create) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    if (!p) return JS_NULL;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    node_t *n;
    switch (self->u.nat.magic) {
    case 0: n = new_elem(p->doc, s->d, (int)s->len); break;
    case 1: n = new_text(p->doc, s->d, (int)s->len); break;
    case 2: n = new_node(p->doc, N_ELEM); if (n) { n->name = "#fragment"; n->tag = T_UNKNOWN; } break;
    default: n = new_node(p->doc, N_TEXT); if (n) { n->text = ""; n->len = 0; } break;   /* comentário: nó de texto vazio */
    }
    return wrapv(p, n);
}

/* new Image(largura?, altura?): <img> fora do documento */
NATIVE(img_ctor) {
    UNUSED_ARGS;
    js_page_t *p = cur_page();
    if (!p) return js_throw_err(E_TYPE, "Image: sem documento");
    node_t *n = new_elem(p->doc, "img", 3);
    if (!n) return js_throw_err(E_RANGE, "sem memória");
    const char *names[2] = { "width", "height" };
    for (int i = 0; i < 2 && i < argc; i++) {
        if (argv[i] == JS_UNDEF) continue;
        double x = js_tonum(argv[i]);
        if (js_pending()) return JS_EXC;
        char b[16];
        snprintf(b, sizeof b, "%u", x == x && x >= 0 && x < 2147483647.0 ? (u32)x : 0u);
        set_attr(p, n, names[i], b, (int)strlen(b));
    }
    return wrapv(p, n);
}

NATIVE(img_decode) {
    UNUSED_ARGS;
    node_t *n = node_of(thisv);
    js_page_t *p = page_of(thisv);
    if (!n || !p || n->tag != T_IMG) return js_throw_err(E_TYPE, "decode: não é um HTMLImageElement");
    jsobj *pr = js_promise_new();
    if (!n->img && (node_attr(n, "src") || node_attr(n, "srcset"))) img_refresh(p, n);
    if (!p->imgdec) p->imgdec = js_newarr(0);
    js_arr_push(p->imgdec, thisv);
    js_arr_push(p->imgdec, jv_obj(pr));
    return jv_obj(pr);
}

/* createElementNS: SVG guarda nome e atributos com a caixa original */
NATIVE(doc_create_ns) {
    UNUSED_ARGS;
    js_page_t *p = page_of(thisv);
    if (!p) return JS_NULL;
    jsstr *ns = jv_isnullish(ARG(0)) ? NULL : js_tostr(ARG(0));
    jsstr *q = js_tostr(ARG(1));
    if (!q) return JS_EXC;
    if (ns && !strcmp(ns->d, "http://www.w3.org/2000/svg")) {
        node_t *n = new_node(p->doc, N_ELEM);
        if (!n) return JS_NULL;
        const char *nm = strchr(q->d, ':') ? strchr(q->d, ':') + 1 : q->d;
        n->name = arena_strndup(&p->doc->arena, nm, (int)strlen(nm));
        n->tag = (u16)(!strcasecmp(nm, "svg") ? T_SVG : T_UNKNOWN);
        n->ns = 1;
        return wrapv(p, n);
    }
    node_t *n = new_elem(p->doc, q->d, (int)q->len);
    return wrapv(p, n);
}

NATIVE(doc_write) {
    UNUSED_ARGS;
    js_page_t *p = cur_page();
    if (!p) return JS_UNDEF;
    sbuf_t sb = {0};
    for (int i = 0; i < argc; i++) { jsstr *s = js_tostr(argv[i]); if (!s) { sb_free(&sb); return JS_EXC; } sb_putn(&sb, s->d, (int)s->len); }
    if (self->u.nat.magic) sb_putc(&sb, '\n');
    if (!p->in_script || !p->cur_script || !p->cur_script->parent) {
        js_console_log(2, "document.write ignorado (fora do carregamento da página)");
        sb_free(&sb);
        return JS_UNDEF;
    }
    node_t *tmp = new_elem(p->doc, "div", 3);
    if (tmp && sb.len) {
        html_parse_fragment(p->doc, tmp, sb.p, sb.len);
        node_t *par = p->cur_script->parent, *ref = p->cur_script->next;
        while (tmp->first) { node_t *c = tmp->first; insert_before(par, c, ref); after_insert(p, c); }
    }
    sb_free(&sb);
    return JS_UNDEF;
}

/* window.length: número de iframes */
NATIVE(w_frames_len) {
    UNUSED_ARGS;
    js_page_t *p = page_of_global(thisv);
    if (!p) p = cur_page();
    return jv_int(p ? p->nkids : 0);
}

/* window.open(url, target, features): iframe com nome → navega o iframe; _self/_parent/_top → navega a janela;
 * senão pede ao anfitrião uma vista nova (createWebViewWithConfiguration) */
NATIVE(w_open) {
    UNUSED_ARGS;
    js_page_t *p = cur_page();
    if (!p) return JS_NULL;
    char url[1024] = "";
    if (argc && !jv_isnullish(argv[0])) {
        jsstr *u = js_tostr(argv[0]);
        if (!u) return JS_EXC;
        if (u->len && !web_url_parse(u->d, p->doc->base, url, sizeof url)) return js_throw_dom("SyntaxError", "window.open: URL inválido");
    }
    char tgt[128] = "_blank";
    if (argc > 1 && !jv_isnullish(argv[1])) {
        jsstr *t = js_tostr(argv[1]);
        if (!t) return JS_EXC;
        if (t->len) snprintf(tgt, sizeof tgt, "%s", t->d);
    }
    bool noopener = false;
    if (argc > 2 && !jv_isnullish(argv[2])) {
        jsstr *f = js_tostr(argv[2]);
        if (!f) return JS_EXC;
        noopener = strstr(f->d, "noopener") || strstr(f->d, "noreferrer");
    }
    js_page_t *dst = NULL;
    if (!strcasecmp(tgt, "_self")) dst = p;
    else if (!strcasecmp(tgt, "_parent")) dst = p->parent ? p->parent : p;
    else if (!strcasecmp(tgt, "_top")) { dst = p; while (dst->parent) dst = dst->parent; }
    else if (strcasecmp(tgt, "_blank")) {
        /* iframe com esse nome, na própria janela ou acima */
        for (js_page_t *q = p; q && !dst; q = q->parent)
            for (int i = 0; i < q->nkids && !dst; i++) {
                const char *nm = q->kids[i]->frame_el ? node_attr(q->kids[i]->frame_el, "name") : NULL;
                if (nm && !strcmp(nm, tgt)) dst = q->kids[i];
            }
    }
    if (dst) {
        if (dst != p && !dst->parent && sb_denies(p, JS_SB_TOP_NAV | JS_SB_TOP_NAV_USER)) return JS_NULL;
        if (url[0]) page_navigate(dst, url, false);
        return noopener ? JS_NULL : win_for(p, dst);
    }
    if (sb_denies(p, JS_SB_POPUPS)) return JS_NULL;   /* sandbox sem allow-popups: bloqueado */
    snprintf(p->popup, sizeof p->popup, "%s", url[0] ? url : "about:blank");
    p->popup_pending = true;
    return JS_NULL;   /* a vista nova é do anfitrião; sem opener entre vistas */
}

/* window */
NATIVE(w_alert) {
    UNUSED_ARGS;
    jsstr *s = argc && argv[0] != JS_UNDEF ? js_tostr(argv[0]) : A[A_empty];
    if (!s) return JS_EXC;
    js_page_t *dp = cur_page();
    if (sb_denies(dp, JS_SB_MODALS)) return self->u.nat.magic == 1 ? JS_FALSE : self->u.nat.magic == 2 ? JS_NULL : JS_UNDEF;
    if (dp && dp->dialog) {
        int kind = self->u.nat.magic == 1 ? JS_DLG_CONFIRM : self->u.nat.magic == 2 ? JS_DLG_PROMPT : JS_DLG_ALERT;
        jsstr *def = NULL;
        if (kind == JS_DLG_PROMPT && argc > 1 && argv[1] != JS_UNDEF) { def = js_tostr(argv[1]); if (!def) return JS_EXC; }
        sbuf_t out = {0};
        /* a string da mensagem fica viva na pilha suspensa (varrida pelo coletor) */
        int ok = dp->dialog(dp->dialog_ctx, dp, kind, s->d, def ? def->d : NULL, &out);
        jsv r = JS_UNDEF;
        if (kind == JS_DLG_CONFIRM) r = jv_bool(ok);
        else if (kind == JS_DLG_PROMPT) r = ok ? jv_str(js_strn(out.p ? out.p : "", out.len)) : JS_NULL;
        sb_free(&out);
        return r;
    }
    char b[400];
    snprintf(b, sizeof b, "%s", s->d);
    js_page_t *p = cur_page();
    char host[128] = "";
    if (p) url_host(p->url, host, sizeof host);
    char title[160];
    snprintf(title, sizeof title, "%s %s", self->u.nat.magic == 1 ? "Confirmação da página" : "Alerta da página", host);
    notify(title, b);
    kprintf("JS alert: %s\n", b);
    if (self->u.nat.magic == 1) return JS_TRUE;
    if (self->u.nat.magic == 2) return argc > 1 ? js_tostrv(argv[1]) : JS_NULL;
    return JS_UNDEF;
}

NATIVE(w_getter) {
    UNUSED_ARGS;
    js_page_t *p = page_of_global(thisv);   /* janela consultada (outra, via contentWindow) */
    if (!p) p = cur_page();
    if (!p) return JS_UNDEF;
    switch (self->u.nat.magic) {
    case 0: return jv_int(p->vw ? p->vw : 1024);
    case 1: return jv_int(p->vh ? p->vh : 640);
    case 2: return jv_int(p->scroll);
    case 3: return jv_int(0);
    case 4: return jv_obj(p->location);
    case 5: return jv_num(1);
    }
    return JS_UNDEF;
}

NATIVE(w_setlocation) {
    UNUSED_ARGS;
    js_page_t *p = page_of_global(thisv);
    if (!p) p = cur_page();
    if (!p) return JS_UNDEF;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    page_navigate(p, s->d, false);
    return JS_UNDEF;
}

NATIVE(w_scrollto) {
    UNUSED_ARGS;
    js_page_t *p = cur_page();
    if (!p) return JS_UNDEF;
    jsv y = argc > 1 ? argv[1] : JS_UNDEF;
    if (argc == 1 && jv_isobj(argv[0])) { y = js_getn(argv[0], "top"); TRYV(y); }
    double d = js_tonum(y);
    if (js_pending()) return JS_EXC;
    if (d == d) p->scroll_req = (int)MAX(0, (self->u.nat.magic ? p->scroll + d : d));
    return JS_UNDEF;
}

NATIVE(w_perfnow) { UNUSED_ARGS; js_page_t *p = cur_page(); return jv_num(p ? (double)(timer_ticks() - p->t0) : 0); }

NATIVE(w_computed) {
    UNUSED_ARGS;
    js_page_t *p = cur_page();
    node_t *n = node_of(ARG(0));
    if (!p || !n) return js_throw_err(E_TYPE, "getComputedStyle: o argumento não é um elemento");
    ensure_layout(p);
    jsobj *o = mkhost(p, CL_STYLE, p->realm->p_style, n);
    o->u.host.extra = JS_TRUE;
    return jv_obj(o);
}

static const char b64c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

NATIVE(w_btoa) {
    UNUSED_ARGS;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    /* converte para Latin-1 */
    sbuf_t raw = {0};
    for (int i = 0; i < (int)s->len;) {
        u32 c = js_utf8_get(s->d, (int)s->len, &i);
        if (c > 255) { sb_free(&raw); return js_throw_err(E_ERROR, "InvalidCharacterError: caractere fora do Latin-1 em btoa"); }
        sb_putc(&raw, (char)c);
    }
    sbuf_t sb = {0};
    const u8 *d = (const u8 *)(raw.p ? raw.p : "");
    for (int i = 0; i < raw.len; i += 3) {
        u32 v = (u32)d[i] << 16 | (i + 1 < raw.len ? (u32)d[i + 1] << 8 : 0) | (i + 2 < raw.len ? d[i + 2] : 0);
        sb_putc(&sb, b64c[v >> 18 & 63]);
        sb_putc(&sb, b64c[v >> 12 & 63]);
        sb_putc(&sb, i + 1 < raw.len ? b64c[v >> 6 & 63] : '=');
        sb_putc(&sb, i + 2 < raw.len ? b64c[v & 63] : '=');
    }
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    sb_free(&raw);
    return jv_str(r);
}

NATIVE(w_atob) {
    UNUSED_ARGS;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    sbuf_t sb = {0};
    u32 acc = 0;
    int bits = 0;
    for (u32 i = 0; i < s->len; i++) {
        char c = s->d[i];
        const char *q = c ? strchr(b64c, c) : NULL;
        if (c == '=' || isspace((u8)c)) continue;
        if (!q) { sb_free(&sb); return js_throw_err(E_ERROR, "InvalidCharacterError: base64 inválido"); }
        acc = acc << 6 | (u32)(q - b64c);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            u32 b = (acc >> bits) & 255;
            char t[4];
            sb_putn(&sb, t, js_utf8_put(t, b));
        }
    }
    jsstr *r = js_strn(sb.p ? sb.p : "", sb.len);
    sb_free(&sb);
    return jv_str(r);
}

NATIVE(w_noop) { UNUSED_ARGS; return JS_UNDEF; }

/* ============================================================ rede: fetch() e XMLHttpRequest */
static jnet_t *net_add(js_page_t *p) {
    if (p->nnets == p->cnets) {
        int nc = p->cnets ? p->cnets * 2 : 8;
        jnet_t *nn = krealloc(p->nets, sizeof(jnet_t) * (usize)nc);
        if (!nn) return NULL;
        p->nets = nn;
        p->cnets = nc;
    }
    jnet_t *n = &p->nets[p->nnets++];
    memset(n, 0, sizeof *n);
    return n;
}

/* inicia uma requisição (URL já resolvida); a conclusão é entregue no próximo tick */
static void net_start(js_page_t *p, const char *url, const char *method, const char *body, int blen, const char *ctype,
                      int kind, jsobj *target) {
    jnet_t *n = net_add(p);
    if (!n) return;
    n->kind = kind;
    n->target = target;
    strncpy(n->url, url, sizeof n->url - 1);
    if (!strncmp(url, "file:", 5)) {
        char path[512];
        const char *q = url + 5;
        if (!strncmp(q, "//", 2)) q += 2;
        char clean[1024];
        strncpy(clean, q, sizeof clean - 1);
        clean[sizeof clean - 1] = 0;
        for (char *cut = clean; *cut; cut++) if (*cut == '?' || *cut == '#') { *cut = 0; break; }
        url_decode_path(clean, path, sizeof path);
        vnode_t *v = vfs_lookup(path);
        n->state = 1;
        if (v && !v->dir) {
            n->status = 200;
            n->body = kmalloc(v->size + 1);
            if (n->body) { memcpy(n->body, v->data, v->size); n->body[v->size] = 0; n->blen = (int)v->size; }
            const char *e = strrchr(v->name, '.');
            strcpy(n->ctype, e && !strcasecmp(e, ".json") ? "application/json" : e && (!strcasecmp(e, ".html") || !strcasecmp(e, ".htm")) ? "text/html" :
                             e && !strcasecmp(e, ".js") ? "text/javascript" : "text/plain");
        } else {
            n->status = 404;
            n->body = NULL;
        }
        return;
    }
    if (!strncmp(url, "http:", 5) || !strncmp(url, "https:", 6)) {
        bool post = method && strcasecmp(method, "GET") && strcasecmp(method, "HEAD");
        n->req = post ? http_post(url, ctype && *ctype ? ctype : "text/plain;charset=UTF-8", body ? body : "", (u32)(body ? blen : 0)) : http_get(url);
        if (!n->req) { n->state = 2; snprintf(n->err, sizeof n->err, "não foi possível iniciar a conexão"); }
        return;
    }
    n->state = 2;
    snprintf(n->err, sizeof n->err, "esquema de URL não suportado");
}

static void net_deliver(js_page_t *p, jnet_t *n) {
    (void)p;
    /* só módulos ES passam por aqui; fetch e XMLHttpRequest ficam em web_fetch.c */
    js_module_fetched(n->target, n->body, n->blen, n->status, n->state == 2 ? n->err : NULL);
}

/* avança as requisições pendentes; devolve true se entregou alguma */
static bool pump_nets(js_page_t *p) {
    bool any = false;
    for (int i = 0; i < p->nnets;) {
        jnet_t *n = &p->nets[i];
        if (n->state == 0 && n->req) {
            int st = http_poll(n->req);
            if (st == 0) { i++; continue; }
            http_req_t *r = n->req;
            if (st > 0) {
                n->state = 1;
                n->status = r->status;
                strncpy(n->ctype, r->content_type, sizeof n->ctype - 1);
                if (r->body && r->body_len) {
                    n->body = kmalloc(r->body_len + 1);
                    if (n->body) { memcpy(n->body, r->body, r->body_len); n->body[r->body_len] = 0; n->blen = (int)r->body_len; }
                }
            } else {
                n->state = 2;
                snprintf(n->err, sizeof n->err, "%s", r->error[0] ? r->error : "erro de rede");
            }
            http_free(r);
            n->req = NULL;
        }
        /* retira da lista antes de entregar (o script pode iniciar novas requisições) */
        jnet_t done = *n;
        memmove(&p->nets[i], &p->nets[i + 1], sizeof(jnet_t) * (usize)(p->nnets - i - 1));
        p->nnets--;
        J.interrupted = false;
        js_set_budget(2000);
        jsobj *volatile keep = done.target;
        net_deliver(p, &done);
        (void)keep;
        kfree(done.body);
        js_run_jobs();
        any = true;
    }
    return any;
}

/* ============================================================ montagem do reino */
static jsv domparser_ctor(jsv thisv, int argc, jsv *argv, jsobj *self, int flags);
static jsv domparser_parse(jsv thisv, int argc, jsv *argv, jsobj *self, int flags);

void js_dom_init(jsrealm *r) {
    js_page_t *p = r->page;
    jsobj *g = r->global, *op = r->p_object;
    jsobj *et = js_newobj(op);   /* EventTarget */
    js_defnative(et, "addEventListener", et_listen, 2, 0);
    js_defnative(et, "removeEventListener", et_listen, 2, 1);
    js_defnative(et, "dispatchEvent", et_dispatch, 1, 0);
    r->p_node = js_newobj(et);
    jsobj *np = r->p_node;
    js_defnative(np, "appendChild", nd_append, 1, 0);
    js_defnative(np, "insertBefore", nd_insertbefore, 2, 0);
    js_defnative(np, "removeChild", nd_removechild, 1, 0);
    js_defnative(np, "replaceChild", nd_replacechild, 2, 0);
    js_defnative(np, "cloneNode", nd_clone, 1, 0);
    js_defnative(np, "contains", nd_contains, 1, 0);
    js_defnative(np, "hasChildNodes", nd_haschild, 0, 0);
    js_defnative(np, "remove", nd_remove, 0, 0);
    js_defnative(np, "append", nd_multi, 1, 0);
    js_defnative(np, "prepend", nd_multi, 1, 1);
    js_defnative(np, "before", nd_multi, 1, 2);
    js_defnative(np, "after", nd_multi, 1, 3);
    js_defnative(np, "replaceWith", nd_multi, 1, 4);
    js_defnative(np, "querySelector", el_query, 1, 0);
    js_defnative(np, "querySelectorAll", el_query, 1, 1);
    js_defnative(np, "getElementsByTagName", el_bytag, 1, 0);
    js_defnative(np, "getElementsByClassName", el_bytag, 1, 1);
    r->p_elem = js_newobj(np);
    jsobj *ep = r->p_elem;
    js_defnative(ep, "getAttribute", el_attr, 1, 0);
    js_defnative(ep, "setAttribute", el_attr, 2, 1);
    js_defnative(ep, "removeAttribute", el_attr, 1, 2);
    js_defnative(ep, "hasAttribute", el_attr, 1, 3);
    js_defnative(ep, "toggleAttribute", el_attr, 1, 4);
    js_defnative(ep, "matches", el_matches, 1, 0);
    js_defnative(ep, "closest", el_matches, 1, 1);
    js_defnative(ep, "getBoundingClientRect", el_rect, 0, 0);
    js_defnative(ep, "focus", el_misc, 0, 0);
    js_defnative(ep, "blur", el_misc, 0, 1);
    js_defnative(ep, "click", el_misc, 0, 2);
    js_defnative(ep, "scrollIntoView", el_misc, 0, 3);
    js_defnative(ep, "submit", el_misc, 0, 4);
    js_defnative(ep, "reset", el_misc, 0, 5);
    js_defnative(ep, "requestSubmit", el_misc, 0, 6);
    js_defnative(ep, "select", el_misc, 0, 7);
    js_defnative(ep, "insertAdjacentHTML", el_adjacent, 2, 0);
    js_defnative(ep, "insertAdjacentElement", el_adjacent, 2, 1);
    js_defnative(ep, "insertAdjacentText", el_adjacent, 2, 2);
    r->p_text = js_newobj(np);
    r->p_doc = js_newobj(np);
    jsobj *dp = r->p_doc;
    js_defnative(dp, "getElementById", doc_byid, 1, 0);
    js_defnative(dp, "createElement", doc_create, 1, 0);
    js_defnative(dp, "createTextNode", doc_create, 1, 1);
    js_defnative(dp, "createDocumentFragment", doc_create, 0, 2);
    js_defnative(dp, "createComment", doc_create, 1, 3);
    js_defnative(dp, "createElementNS", doc_create_ns, 2, 0);
    js_defnative(dp, "getElementsByName", el_bytag, 1, 2);
    js_defnative(dp, "write", doc_write, 1, 0);
    js_defnative(dp, "writeln", doc_write, 1, 1);
    js_defnative(dp, "hasFocus", w_noop, 0, 0);
    r->p_style = js_newobj(op);
    js_defnative(r->p_style, "setProperty", style_method, 2, 0);
    js_defnative(r->p_style, "getPropertyValue", style_method, 1, 1);
    js_defnative(r->p_style, "removeProperty", style_method, 1, 2);
    r->p_classlist = js_newobj(op);
    js_defnative(r->p_classlist, "add", cl_method, 1, 0);
    js_defnative(r->p_classlist, "remove", cl_method, 1, 1);
    js_defnative(r->p_classlist, "toggle", cl_method, 1, 2);
    js_defnative(r->p_classlist, "contains", cl_method, 1, 3);
    js_defnative(r->p_classlist, "replace", cl_method, 2, 4);
    js_defnative(r->p_classlist, "item", cl_method, 1, 5);
    js_defnative(r->p_classlist, "toString", cl_method, 0, 6);
    r->p_event = js_newobj(op);
    js_defnative(r->p_event, "preventDefault", ev_method, 0, 0);
    js_defnative(r->p_event, "stopPropagation", ev_method, 0, 1);
    js_defnative(r->p_event, "stopImmediatePropagation", ev_method, 0, 2);
    jsobj *evc = js_newctor(ev_ctor, "Event", 1, r->p_event);
    js_defn(g, "Event", jv_obj(evc), PF_NOENUM);
    jsobj *cep = js_newobj(r->p_event);
    jsobj *cec = js_newctor(ev_ctor, "CustomEvent", 2, cep);
    cec->u.nat.magic = 1;
    js_defn(g, "CustomEvent", jv_obj(cec), PF_NOENUM);
    js_defn(g, "KeyboardEvent", jv_obj(evc), PF_NOENUM);
    js_defn(g, "MouseEvent", jv_obj(evc), PF_NOENUM);
    /* construtores "de fachada" para instanceof */
    jsobj *nodec = js_newctor(w_noop, "Node", 0, np);
    nodec->is_class = 0;
    js_defn(g, "Node", jv_obj(nodec), PF_NOENUM);
    jsobj *elc = js_newctor(w_noop, "Element", 0, ep);
    elc->is_class = 0;
    js_defn(g, "Element", jv_obj(elc), PF_NOENUM);
    js_defn(g, "HTMLElement", jv_obj(elc), PF_NOENUM);
    /* HTMLImageElement / Image */
    r->p_img = js_newobj(ep);
    js_defnative(r->p_img, "decode", img_decode, 0, 0);
    jsobj *imgk = js_newctor(img_ctor, "Image", 0, r->p_img);
    js_defn(g, "Image", jv_obj(imgk), PF_NOENUM);
    jsobj *imgc = js_newctor(w_noop, "HTMLImageElement", 0, r->p_img);   /* por último: é o constructor do protótipo */
    imgc->is_class = 0;
    js_defn(g, "HTMLImageElement", jv_obj(imgc), PF_NOENUM);
    jsobj *etc = js_newctor(w_noop, "EventTarget", 0, et);
    etc->is_class = 0;
    js_defn(g, "EventTarget", jv_obj(etc), PF_NOENUM);
    /* janela */
    js_defnative(g, "addEventListener", et_listen, 2, 0);
    js_defnative(g, "removeEventListener", et_listen, 2, 1);
    js_defnative(g, "dispatchEvent", et_dispatch, 1, 0);
    js_defn(g, "window", jv_obj(g), PF_NOENUM);
    js_defn(g, "self", jv_obj(g), PF_NOENUM);
    js_defn(g, "top", jv_obj(g), PF_NOENUM);
    js_defn(g, "parent", jv_obj(g), PF_NOENUM);
    js_defn(g, "frames", jv_obj(g), PF_NOENUM);
    js_defn(g, "frameElement", JS_NULL, PF_NOENUM);
    js_defnative(g, "postMessage", w_postmessage, 2, 0);
    js_defnative(g, "open", w_open, 0, 0);
    js_defgetter(g, "length", w_frames_len, 0);
    p->document = wrap(p, p->doc->root);
    js_defn(g, "document", jv_obj(p->document), PF_NOENUM);
    p->location = mkhost(p, CL_LOCATION, js_newobj(op), NULL);
    js_defnative(p->location->proto, "assign", loc_method, 1, 0);
    js_defnative(p->location->proto, "replace", loc_method, 1, 1);
    js_defnative(p->location->proto, "reload", loc_method, 0, 2);
    js_defnative(p->location->proto, "toString", loc_method, 0, 3);
    js_defaccessor(g, js_intern("location", -1), jv_obj(js_newnative(w_getter, "location", 0, 4)),
                   jv_obj(js_newnative(w_setlocation, "location", 1, 0)), PF_NOENUM);
    js_defgetter(g, "innerWidth", w_getter, 0);
    js_defgetter(g, "innerHeight", w_getter, 1);
    js_defgetter(g, "outerWidth", w_getter, 0);
    js_defgetter(g, "outerHeight", w_getter, 1);
    js_defgetter(g, "scrollY", w_getter, 2);
    js_defgetter(g, "pageYOffset", w_getter, 2);
    js_defgetter(g, "scrollX", w_getter, 3);
    js_defgetter(g, "pageXOffset", w_getter, 3);
    js_defgetter(g, "devicePixelRatio", w_getter, 5);
    js_defnative(g, "alert", w_alert, 1, 0);
    js_defnative(g, "confirm", w_alert, 1, 1);
    js_defnative(g, "prompt", w_alert, 2, 2);
    js_defnative(g, "setTimeout", w_settimeout, 2, 0);
    js_defnative(g, "setInterval", w_settimeout, 2, 1);
    js_defnative(g, "requestAnimationFrame", w_settimeout, 1, 2);
    js_defnative(g, "clearTimeout", w_cleartimeout, 1, 0);
    js_defnative(g, "clearInterval", w_cleartimeout, 1, 0);
    js_defnative(g, "cancelAnimationFrame", w_cleartimeout, 1, 0);
    js_defnative(g, "scrollTo", w_scrollto, 2, 0);
    js_defnative(g, "scroll", w_scrollto, 2, 0);
    js_defnative(g, "scrollBy", w_scrollto, 2, 1);
    js_defnative(g, "getComputedStyle", w_computed, 1, 0);
    js_defnative(g, "btoa", w_btoa, 1, 0);
    js_defnative(g, "atob", w_atob, 1, 0);
    js_defnative(g, "focus", w_noop, 0, 0);
    js_defnative(g, "print", w_noop, 0, 0);
    jsobj *perf = js_newobj(op);
    js_defnative(perf, "now", w_perfnow, 0, 0);
    js_defn(g, "performance", jv_obj(perf), PF_NOENUM);
    jsobj *nav = js_newobj(op);
    js_defn(nav, "userAgent", jv_str(js_strz("Mozilla/5.0 (mOS; x86_64) Horizonte/1.1 Brisa/1.0")), 0);
    js_defn(nav, "language", jv_str(js_strz("pt-BR")), 0);
    jsv langs[2] = {jv_str(js_strz("pt-BR")), jv_str(js_strz("pt"))};
    js_defn(nav, "languages", jv_obj(js_arr_from(langs, 2)), 0);
    js_defn(nav, "platform", jv_str(js_strz("mOS")), 0);
    js_defn(nav, "vendor", jv_str(js_strz("mOS")), 0);
    js_defn(nav, "cookieEnabled", JS_TRUE, 0);
    js_defn(nav, "onLine", jv_bool(net_configured()), 0);
    js_defn(g, "navigator", jv_obj(nav), PF_NOENUM);
    jsobj *hp = js_newobj(op);
    js_defnative(hp, "pushState", hist_method, 2, 0);
    js_defnative(hp, "replaceState", hist_method, 2, 1);
    js_defnative(hp, "go", hist_method, 0, 2);
    js_defnative(hp, "back", hist_method, 0, 3);
    js_defnative(hp, "forward", hist_method, 0, 4);
    js_defgetter(hp, "length", hist_method, 5);
    js_defgetter(hp, "state", hist_method, 6);
    js_defaccessor(hp, js_intern("scrollRestoration", -1), jv_obj(js_newnative(hist_method, "scrollRestoration", 0, 7)),
                   jv_obj(js_newnative(hist_method, "scrollRestoration", 1, 8)), 0);
    js_defprop(hp, A[A_sym_toStringTag], jv_str(js_intern("History", -1)), PF_NOENUM | PF_RO);
    js_defn(g, "History", jv_obj(js_newctor(w_noop, "History", 0, hp)), PF_NOENUM);
    jsobj *ho = js_newobj(hp);
    if (r->page) r->page->hobj = ho;
    js_defn(g, "history", jv_obj(ho), PF_NOENUM);
    jsobj *sp = js_newobj(op);
    js_defnative(sp, "getItem", stor_op, 1, 0);
    js_defnative(sp, "setItem", stor_op, 2, 1);
    js_defnative(sp, "removeItem", stor_op, 1, 2);
    js_defnative(sp, "clear", stor_op, 0, 3);
    js_defnative(sp, "key", stor_op, 1, 4);
    r->p_storage = sp;
    p->storage = mkhost(p, CL_STORAGE, sp, NULL);
    p->session = mkhost(p, CL_STORAGE, sp, NULL);
    p->session->u.host.extra = JS_TRUE;
    js_defn(g, "localStorage", jv_obj(p->storage), PF_NOENUM);
    js_defn(g, "sessionStorage", jv_obj(p->session), PF_NOENUM);
    jsobj *scr = js_newobj(op);
    js_defn(scr, "width", jv_int(1280), 0);
    js_defn(scr, "height", jv_int(800), 0);
    js_defn(g, "screen", jv_obj(scr), PF_NOENUM);
    js_blob_init(r);
    js_url_init(r);
    js_fetch_init(r);   /* fetch, Headers, Request, Response, FormData, AbortController, XMLHttpRequest */
    js_plat_init(r);    /* structuredClone, MessageChannel, crypto */
    js_obs_init(r);     /* MutationObserver, ResizeObserver, IntersectionObserver */
    js_worker_init(r);  /* Worker, BroadcastChannel */
    js_stream_init(r);  /* ReadableStream e companhia (carregados na primeira vez) */
    jsobj *dpp = js_newobj(op);
    js_defnative(dpp, "parseFromString", domparser_parse, 2, 0);
    js_defn(g, "DOMParser", jv_obj(js_newctor(domparser_ctor, "DOMParser", 0, dpp)), PF_NOENUM);
    js_canvas_init(r);
}

/* HTML → Document separado (DOMParser, XHR responseType "document") */
jsv js_dom_parse_document(const char *url, jsstr *src, const char *mime) {
    js_page_t *p = cur_page();
    if (!p) return JS_NULL;
    (void)mime;
    if (p->nxdocs >= (int)ARRAY_LEN(p->xdocs)) return js_throw_err(E_RANGE, "documentos demais criados por DOMParser nesta página");
    doc_t *d = doc_parse(src->d, src->len, url && *url ? url : p->url);
    if (!d) return js_throw_err(E_RANGE, "sem memória para o documento");
    p->xdocs[p->nxdocs++] = d;
    jsobj *o = wrap(p, d->root);
    return o ? jv_obj(o) : JS_NULL;
}

NATIVE(domparser_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor DOMParser exige 'new'");
    return jv_obj(js_newobj(jv_isobj(thisv) && jv_o(thisv)->proto ? jv_o(thisv)->proto : J.realm->p_object));
}

NATIVE(domparser_parse) {
    UNUSED_ARGS;
    if (argc < 2) return js_throw_err(E_TYPE, "parseFromString: são necessários 2 argumentos");
    jsstr *src = js_tostr(ARG(0));
    if (!src) return JS_EXC;
    jsstr *t = js_tostr(ARG(1));
    if (!t) return JS_EXC;
    static const char *ok[] = { "text/html", "text/xml", "application/xml", "application/xhtml+xml", "image/svg+xml" };
    bool valid = false;
    for (u32 i = 0; i < ARRAY_LEN(ok); i++) if (!strcmp(t->d, ok[i])) valid = true;
    if (!valid) return js_throw_err(E_TYPE, "parseFromString: tipo '%s' não suportado", t->d);
    js_page_t *p = cur_page();
    return js_dom_parse_document(p ? p->url : "about:blank", src, t->d);
}

/* entradas de um <form> para FormData (ordem do documento, regras de envio do HTML) */
static void form_walk(js_page_t *p, node_t *form, node_t *n, node_t *submitter, jsobj *list) {
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type != N_ELEM) continue;
        if (c->tag == T_INPUT || c->tag == T_SELECT || c->tag == T_TEXTAREA || c->tag == T_BUTTON) {
            const char *name = node_attr(c, "name");
            const char *ty = node_attr(c, "type");
            bool disabled = node_attr(c, "disabled") != NULL;
            for (node_t *a = c->parent; a && a != form; a = a->parent) if (a->tag == T_FIELDSET && node_attr(a, "disabled")) disabled = true;
            bool is_btn = c->tag == T_BUTTON || (ty && (!strcasecmp(ty, "submit") || !strcasecmp(ty, "button") || !strcasecmp(ty, "reset") || !strcasecmp(ty, "image")));
            if (disabled || (is_btn && c != submitter)) { if (c->first) form_walk(p, form, c, submitter, list); continue; }
            if (ty && (!strcasecmp(ty, "checkbox") || !strcasecmp(ty, "radio")) && !ctrl_checked(c)) continue;
            if (c->tag == T_INPUT && ty && !strcasecmp(ty, "image") && name) {
                char nb[300];
                snprintf(nb, sizeof nb, "%s.x", name); js_arr_push(list, jv_str(js_strz(nb))); js_arr_push(list, jv_str(js_strz("0")));
                snprintf(nb, sizeof nb, "%s.y", name); js_arr_push(list, jv_str(js_strz(nb))); js_arr_push(list, jv_str(js_strz("0")));
                continue;
            }
            if (!name || !*name) continue;
            if (c->tag == T_INPUT && ty && !strcasecmp(ty, "file")) {
                /* sem arquivo escolhido: File vazio, como os navegadores */
                jsobj *f = js_blob_new(NULL, 0, "application/octet-stream");
                jsv ctor = js_getn(jv_obj(p->realm->global), "File"), parts = jv_obj(js_arr_from((jsv[]){ jv_obj(f) }, 1));
                jsobj *opt = js_newobj(p->realm->p_object);
                js_defn(opt, "type", jv_str(js_strz("application/octet-stream")), 0);
                jsv a[3] = { parts, jv_str(A[A_empty]), jv_obj(opt) };
                jsv file = js_construct(ctor, 3, a, ctor);
                if (file == JS_EXC) { js_catch(); continue; }
                js_arr_push(list, jv_str(js_strz(name)));
                js_arr_push(list, file);
                continue;
            }
            if (c->tag == T_SELECT && node_attr(c, "multiple") && c->ctrl) {
                /* só a opção marcada no controle; sem controle, as <option selected> */
                js_arr_push(list, jv_str(js_strz(name)));
                js_arr_push(list, ctrl_value(c));
                continue;
            }
            if (c->tag == T_SELECT && node_attr(c, "multiple")) {
                for (node_t *o = c->first; o; o = o->next)
                    if (o->type == N_ELEM && o->tag == T_OPTION && node_attr(o, "selected")) { js_arr_push(list, jv_str(js_strz(name))); js_arr_push(list, ctrl_value(o)); }
                continue;
            }
            jsv v = ctrl_value(c);
            if (c->tag == T_INPUT && ty && !strcasecmp(ty, "hidden") && !strcasecmp(name, "_charset_")) v = jv_str(js_strz("UTF-8"));
            js_arr_push(list, jv_str(js_strz(name)));
            js_arr_push(list, v);
            continue;
        }
        if (c->first) form_walk(p, form, c, submitter, list);
    }
}

bool js_form_entries(jsv form, jsv submitter, jsobj *list) {
    node_t *f = node_of(form);
    js_page_t *p = page_of(form);
    if (!f || !p || f->tag != T_FORM) return false;
    node_t *s = submitter == JS_UNDEF || submitter == JS_NULL ? NULL : node_of(submitter);
    if (s && form_of(s) != f) { js_throw_dom("NotFoundError", "FormData: o submitter não pertence ao formulário"); return false; }
    form_walk(p, f, f, s, list);
    return true;
}

/* ponte para o Canvas 2D (canvas_js.c) */
jsv js_dom_wrapv(js_page_t *p, node_t *n) { return wrapv(p, n); }
doc_t *js_page_doc(js_page_t *p) { return p ? p->doc : NULL; }
void js_page_repaint(js_page_t *p) { if (p) p->dirty = true; }
js_page_t *js_cur_page(void) { return cur_page(); }
node_t *js_page_focus(js_page_t *p) { return p ? p->focus : NULL; }
int js_page_add_timer(js_page_t *p, jsv fn, double ms) { return p ? add_timer(p, fn, ms, false, 0, NULL) : 0; }
const char *js_page_origin(js_page_t *p) { return p ? p->origin : "null"; }
jsrealm *js_page_realm(js_page_t *p) { return p ? p->realm : NULL; }
void js_page_set_origin(js_page_t *p, const char *o) { if (p) snprintf(p->origin, sizeof p->origin, "%s", o); }

/* mensagem para a janela de uma página (worker): val já é um clone; outro clone sai no reino de destino */
void js_page_post_message(js_page_t *dst, jsv val, const char *origin) {
    pmsg_t *m = kcalloc(1, sizeof(pmsg_t));
    if (!m) return;
    m->val = val;
    snprintf(m->origin, sizeof m->origin, "%s", origin ? origin : "");
    if (dst->msgs_tail) dst->msgs_tail->next = m; else dst->msgs = m;
    dst->msgs_tail = m;
}

/* script de módulo com fonte pronta (worker type=module) */
void js_page_add_module_source(js_page_t *p, const char *src, int len) {
    node_t *s = new_elem(p->doc, "script", 6);
    if (!s) return;
    set_attr0(p, s, "type", "module", 6);
    node_t *t = new_text(p->doc, src, len);
    if (t) insert_before(s, t, NULL);
    insert_before(p->doc->root, s, NULL);
    queue_script(p, s);
}

static box_t *find_box(box_t *b, node_t *n, int depth) {
    if (!b || depth > 300) return NULL;
    if (b->node == n) return b;
    for (box_t *c = b->first; c; c = c->next) { box_t *r = find_box(c, n, depth + 1); if (r) return r; }
    if (b->kind == B_LINES) for (int i = 0; i < b->nfrags; i++) if (b->frags[i].atom) { box_t *r = find_box(b->frags[i].atom, n, depth + 1); if (r) return r; }
    return NULL;
}

bool js_node_geom(js_page_t *p, node_t *n, float g[10]) {
    if (!p || !n || !is_connected(p, n)) return false;
    rect_acc r = node_rect(p, n);
    if (!r.any) return false;
    g[0] = (float)r.x0; g[1] = (float)r.y0; g[2] = (float)(r.x1 - r.x0); g[3] = (float)(r.y1 - r.y0);
    box_t *b = find_box(p->doc->rootbox, n, 0);
    float l = 0, t = 0, rr = 0, bt = 0;
    g[8] = b ? b->p[3] : 0; g[9] = b ? b->p[0] : 0;   /* padding esquerdo e de cima */
    if (b) { l = b->b[3] + b->p[3]; t = b->b[0] + b->p[0]; rr = b->b[1] + b->p[1]; bt = b->b[2] + b->p[2]; }
    g[4] = g[0] + l; g[5] = g[1] + t;
    g[6] = g[2] - l - rr; g[7] = g[3] - t - bt;
    if (g[6] < 0) g[6] = 0;
    if (g[7] < 0) g[7] = 0;
    return true;
}

void js_page_viewport_rect(js_page_t *p, int *x, int *y, int *w, int *h) {
    *x = 0; *y = p ? p->scroll : 0;
    *w = p && p->vw ? p->vw : 1024; *h = p && p->vh ? p->vh : 640;
}

/* ============================================================ interface pública */
static void page_origin(const char *url, char *out, int cap) {
    char host[256];
    char sch[16] = "";
    url_scheme(url, sch, sizeof sch);
    if (url_host(url, host, sizeof host)) snprintf(out, (usize)cap, "%s://%s", sch, host);
    else snprintf(out, (usize)cap, "%s://", sch[0] ? sch : "file");
}

js_page_t *js_page_new(doc_t *d, const char *url, int vw, int vh) {
    if (!js_enabled) return NULL;
    return js_page_new2(d, url, vw, vh, NULL, NULL);
}

/* como js_page_new, sem olhar o js_enabled do Horizonte (o WKWebView tem a preferência própria). init é chamado
 * com fase 0 antes dos scripts da página (WKUserScriptInjectionTimeAtDocumentStart, messageHandlers) e com fase 1
 * depois dos scripts inline, antes do load (AtDocumentEnd) */
js_page_t *js_page_new2(doc_t *d, const char *url, int vw, int vh, void (*init)(js_page_t *p, int phase, void *ctx), void *ctx) {
    if (!d) return NULL;
    js_page_t *p = kcalloc(1, sizeof(js_page_t));
    if (!p) return NULL;
    int frame;
    js_enter(&frame);
    js_core_init();
    p->doc = d;
    strncpy(p->url, url, sizeof p->url - 1);
    page_origin(url, p->origin, sizeof p->origin);
    p->vw = vw;
    p->vh = vh;
    p->scroll_req = -1;
    p->hcur = -1;
    p->t0 = timer_ticks();
    p->next = pages;
    pages = p;
    jsrealm *saved = J.realm;
    jsrealm *r = kcalloc(1, sizeof(jsrealm));
    if (!r) { js_leave(); pages = p->next; kfree(p); return NULL; }
    r->page = p;
    p->realm = r;
    r->next = J.realms;
    J.realms = r;
    J.realm = r;
    js_lib_init(r);
    js_dom_init(r);
    if (init) init(p, 0, ctx);
    collect_scripts(p, d->root);
    bool ready = pump_scripts(p);
    if (init) init(p, 1, ctx);
    if (ready) maybe_load_events(p);
    p->dirty = false;
    J.realm = saved ? saved : r;
    js_leave();
    return p;
}

void js_page_free(js_page_t *p) {
    if (!p) return;
    for (js_page_t **pp = &pages; *pp; pp = &(*pp)->next) if (*pp == p) { *pp = p->next; break; }
    for (int i = 0; i < p->nscripts; i++) { if (p->scripts[i].req) http_free(p->scripts[i].req); kfree(p->scripts[i].text); }
    for (int i = 0; i < p->nnets; i++) { if (p->nets[i].req) http_free(p->nets[i].req); kfree(p->nets[i].body); }
    kfree(p->nets);
    web_fetch_drop(p);
    web_worker_drop(p);
    web_bc_realm_gone(p->realm);
    for (int i = 0; i < p->nxdocs; i++) doc_free(p->xdocs[i]);
    while (p->msgs) { pmsg_t *m = p->msgs; p->msgs = m->next; kfree(m); }
    for (js_page_t *q = pages; q; q = q->next) if (q->parent == p) q->parent = NULL;
    kfree(p->scripts);
    kfree(p->timers);
    kfree(p->wkeys);
    kfree(p->wvals);
    sb_free(&p->console);
    jsrealm *r = p->realm;
    r->page = NULL;
    for (jsrealm **pp = &J.realms; *pp; pp = &(*pp)->next) if (*pp == r) { *pp = r->next; break; }
    if (J.realm == r) J.realm = J.realms;
    kfree(r);
    kfree(p);
    if (J.entry == 0) gc_collect();
}

bool js_page_tick(js_page_t *p) {
    if (!p) return false;
    int frame;
    js_enter(&frame);
    jsrealm *saved = J.realm;
    J.realm = p->realm;
    if (p->msgs) deliver_messages(p);
    if (p->spos < p->nscripts || p->mpos < p->nscripts) { if (pump_scripts(p)) maybe_load_events(p); }
    if (p->spos >= p->nscripts && p->mpos >= p->nscripts) maybe_load_events(p);   /* imagens e load da janela */
    run_timers(p);
    if (p->nnets) pump_nets(p);
    web_fetch_pump(p);
    web_worker_pump(p);   /* workers desta página e as mensagens deles */
    js_run_jobs();
    web_obs_tick(p);   /* ResizeObserver e IntersectionObserver, depois do layout */
    js_run_jobs();
    gc_maybe();
    J.realm = saved;
    js_leave();
    bool d = p->dirty;
    p->dirty = false;
    return d;
}

int js_page_next_ms(js_page_t *p) {
    if (!p) return -1;
    if (p->spos < p->nscripts || p->mpos < p->nscripts || p->nnets || web_fetch_pending(p) || web_worker_busy(p)) return 10;
    u64 now = timer_ticks();
    int best = -1;
    for (int i = 0; i < p->ntimers; i++) {
        if (!p->timers[i].live) continue;
        int ms = p->timers[i].due <= now ? 0 : (int)(p->timers[i].due - now);
        if (best < 0 || ms < best) best = ms;
    }
    return best;
}

void js_page_viewport(js_page_t *p, int w, int h, int scroll) {
    if (!p) return;
    p->vw = w;
    p->vh = h;
    p->scroll = scroll;
}

void js_key_name(int key, char *out, int cap) {
    const char *n = NULL;
    switch (key) {
    case KEY_ENTER: case '\r': n = "Enter"; break;
    case KEY_BACKSPACE: n = "Backspace"; break;
    case KEY_TAB: n = "Tab"; break;
    case KEY_ESC: n = "Escape"; break;
    case KEY_UP: n = "ArrowUp"; break;
    case KEY_DOWN: n = "ArrowDown"; break;
    case KEY_LEFT: n = "ArrowLeft"; break;
    case KEY_RIGHT: n = "ArrowRight"; break;
    case KEY_HOME: n = "Home"; break;
    case KEY_END: n = "End"; break;
    case KEY_PGUP: n = "PageUp"; break;
    case KEY_PGDN: n = "PageDown"; break;
    case KEY_DELETE: n = "Delete"; break;
    case KEY_INSERT: n = "Insert"; break;
    case ' ': n = " "; break;
    }
    if (n) { snprintf(out, (usize)cap, "%s", n); return; }
    if (key >= KEY_F1 && key <= KEY_F12) { snprintf(out, (usize)cap, "F%d", key - KEY_F1 + 1); return; }
    if (key > 0 && key < 0x100) { char t[4]; int l = js_utf8_put(t, (u32)key); snprintf(out, (usize)cap, "%.*s", l, t); return; }
    snprintf(out, (usize)cap, "Unidentified");
}

static int key_code(int key) {
    switch (key) {
    case KEY_ENTER: return 13;
    case KEY_BACKSPACE: return 8;
    case KEY_TAB: return 9;
    case KEY_ESC: return 27;
    case KEY_LEFT: return 37;
    case KEY_UP: return 38;
    case KEY_RIGHT: return 39;
    case KEY_DOWN: return 40;
    case KEY_DELETE: return 46;
    }
    if (key >= 'a' && key <= 'z') return key - 32;
    if (key > 0 && key < 128) return key;
    return 0;
}

bool js_page_event(js_page_t *p, node_t *target, int type, int key, u16 mods) {
    if (!p || !target) return false;
    static const char *names[] = {"click", "input", "change", "submit", "keydown", "keyup", "focus", "blur"};
    if (type < 0 || type > JSE_BLUR) return false;
    int frame;
    js_enter(&frame);
    jsrealm *saved = J.realm;
    J.realm = p->realm;
    bool bubbles = type != JSE_FOCUS && type != JSE_BLUR;
    jsobj *ev = new_event(p, js_intern(names[type], -1), bubbles);
    if (type == JSE_KEYDOWN || type == JSE_KEYUP) {
        char kn[32];
        js_key_name(key, kn, sizeof kn);
        js_defprop(ev, A[A_key], jv_str(js_strz(kn)), 0);
        js_defn(ev, "code", jv_str(js_strz(kn)), 0);
        js_defn(ev, "keyCode", jv_int(key_code(key)), 0);
        js_defn(ev, "which", jv_int(key_code(key)), 0);
    }
    if (type == JSE_CLICK) {
        js_defn(ev, "button", jv_int(0), 0);
        js_defn(ev, "detail", jv_int(1), 0);
    }
    if (type == JSE_FOCUS) p->focus = target;
    if (type == JSE_BLUR && p->focus == target) p->focus = NULL;
    js_defn(ev, "shiftKey", jv_bool(mods & MOD_SHIFT), 0);
    js_defn(ev, "ctrlKey", jv_bool(mods & MOD_CTRL), 0);
    js_defn(ev, "altKey", jv_bool(mods & MOD_ALT), 0);
    js_defn(ev, "metaKey", jv_bool(mods & MOD_CMD), 0);
    bool prevented = dispatch(p, target, ev);
    js_run_jobs();
    J.realm = saved;
    js_leave();
    return prevented;
}

void js_page_eval_url(js_page_t *p, const char *code) {
    if (!p || sb_denies(p, JS_SB_SCRIPTS)) return;
    int frame;
    js_enter(&frame);
    jsrealm *saved = J.realm;
    J.realm = p->realm;
    char dec[2048];
    url_decode_path(code, dec, sizeof dec);
    exec_source(p, dec, (int)strlen(dec), "javascript:", NULL);
    J.realm = saved;
    js_leave();
}

/* Editor de Script: avalia um programa inteiro (sem decodificar como URL nem limitar o tamanho) */
void js_page_eval(js_page_t *p, const char *code, int len, const char *name) {
    if (!p || !code) return;
    int frame;
    js_enter(&frame);
    jsrealm *saved = J.realm;
    J.realm = p->realm;
    exec_source(p, code, len < 0 ? (int)strlen(code) : len, name ? name : "script", NULL);
    J.realm = saved;
    js_leave();
}

const char *js_page_take_nav(js_page_t *p, bool *reload) {
    if (!p || !p->nav_pending) return NULL;
    p->nav_pending = false;
    if (reload) *reload = p->nav_reload;
    p->nav_reload = false;
    return p->nav;
}

int js_page_take_scroll(js_page_t *p) {
    if (!p) return -1;
    int s = p->scroll_req;
    p->scroll_req = -1;
    return s;
}

node_t *js_page_take_submit(js_page_t *p) {
    if (!p) return NULL;
    node_t *f = p->submit_req;
    p->submit_req = NULL;
    return f;
}

const char *js_page_console(js_page_t *p) { return p && p->console.p ? p->console.p : ""; }
int js_page_errors(js_page_t *p) { return p ? p->errors : 0; }

/* ============================================================ WKWebView (kernel/web/webview.c)
 * Valores que saem do JavaScript para o app: o tipo (JS_WK_*) e um texto. String e número vão crus, objetos e arrays
 * como JSON (o framework monta NSDictionary/NSArray). Funções, símbolos, nós do DOM e promessas não têm forma
 * serializável: JS_WK_UNSUPPORTED, como o "unsupported type" do WebKit. */
static int wk_value(jsv v, sbuf_t *out) {
    if (v == JS_UNDEF) return JS_WK_UNDEFINED;
    if (v == JS_NULL) return JS_WK_NULL;
    if (jv_isbool(v)) { sb_puts(out, v == JS_TRUE ? "true" : "false"); return JS_WK_BOOL; }
    if (jv_isnum(v) || jv_isbig(v)) {
        jsstr *s = js_tostr(v);
        if (!s) { js_catch(); return JS_WK_UNSUPPORTED; }
        sb_putn(out, s->d, (int)s->len);
        return JS_WK_NUMBER;
    }
    if (jv_isstr(v)) {
        if (jv_s(v)->sym) return JS_WK_UNSUPPORTED;
        sb_putn(out, jv_s(v)->d, (int)jv_s(v)->len);
        return JS_WK_STRING;
    }
    if (!jv_isobj(v)) return JS_WK_UNSUPPORTED;
    int cls = jv_o(v)->cls;
    if (cls == CL_FUNCTION || cls == CL_NATIVE || cls == CL_BOUND || cls == CL_PROMISE || cls == CL_SYMBOL ||
        (cls >= CL_NODE && cls <= CL_HOST))
        return JS_WK_UNSUPPORTED;
    if (cls == CL_STRING || cls == CL_NUMBER || cls == CL_BOOLEAN) {   /* new String(...) e afins: o valor primitivo */
        jsv pv = js_toprim(v, 0);
        if (pv == JS_EXC) { js_catch(); return JS_WK_UNSUPPORTED; }
        return wk_value(pv, out);
    }
    if (cls == CL_DATE) {   /* o WebKit entrega NSDate; aqui vai o número de ms, que o framework converte */
        jsv n = js_tonumv(v);
        if (n == JS_EXC) { js_catch(); return JS_WK_UNSUPPORTED; }
        wk_value(n, out);
        return JS_WK_DATE;
    }
    jsv j = js_json_stringify(v, JS_UNDEF, JS_UNDEF);
    if (j == JS_EXC) { js_catch(); return JS_WK_UNSUPPORTED; }   /* ciclo, BigInt dentro... */
    if (!jv_isstr(j)) return JS_WK_UNSUPPORTED;
    sb_putn(out, jv_s(j)->d, (int)jv_s(j)->len);
    return JS_WK_JSON;
}

/* avalia um programa e devolve o valor da última expressão (evaluateJavaScript:). Exceção: JS_WK_EXCEPTION, com a
 * mensagem ("TypeError: ...") em out. */
int js_page_eval_value(js_page_t *p, const char *code, int len, sbuf_t *out) {
    if (!p || !code) return JS_WK_EXCEPTION;
    int frame;
    js_enter(&frame);
    jsrealm *saved = J.realm;
    J.realm = p->realm;
    jsstr *err = NULL;
    J.interrupted = false;
    int type;
    jsscript *s = js_parse(code, len < 0 ? (int)strlen(code) : len, "evaluateJavaScript", 1, &err);
    if (!s) {
        sb_printf(out, "SyntaxError: %s", err ? err->d : "erro de sintaxe");
        type = JS_WK_EXCEPTION;
    } else {
        p->in_script = true;
        js_set_budget(5000);
        jsv r = js_run_script(s);
        p->in_script = false;
        if (r == JS_EXC) {
            jsv e = js_catch();
            jsstr *m = js_exc_string(e);
            sb_puts(out, m ? m->d : "Error");
            type = JS_WK_EXCEPTION;
        } else type = wk_value(r, out);
        js_run_jobs();
    }
    J.realm = saved;
    js_leave();
    return type;
}

/* postMessage de window.webkit.messageHandlers.<nome>: o nome fica na propriedade escondida __mosName do objeto */
static jsv wk_post_message(jsv thisv, int argc, jsv *argv, jsobj *self, int flags) {
    (void)self; (void)flags;
    js_page_t *p = cur_page();
    if (!p || !p->wk_post) return JS_UNDEF;
    jsv nm = jv_isobj(thisv) ? js_getn(thisv, "__mosName") : JS_UNDEF;
    if (!jv_isstr(nm)) return js_throw_err(E_TYPE, "postMessage: o manipulador não existe mais");
    sbuf_t out = {0};
    int type = wk_value(ARG(0), &out);
    if (type == JS_WK_UNSUPPORTED) { sb_free(&out); return js_throw_err(E_TYPE, "postMessage: o valor não pode ser serializado"); }
    p->wk_post(p->wk_ctx, jv_s(nm)->d, out.p ? out.p : "", out.len, type);
    sb_free(&out);
    return JS_UNDEF;
}

void js_page_wk_bridge(js_page_t *p, void (*post)(void *ctx, const char *name, const char *body, int len, int type), void *ctx) {
    if (!p) return;
    p->wk_post = post;
    p->wk_ctx = ctx;
}

/* acrescenta (on) ou tira window.webkit.messageHandlers.<nome> */
void js_page_wk_handler(js_page_t *p, const char *name, bool on) {
    if (!p || !name || !*name) return;
    int frame;
    js_enter(&frame);
    jsrealm *saved = J.realm;
    J.realm = p->realm;
    jsobj *g = p->realm->global, *op = p->realm->p_object;
    jsv wk = js_getn(jv_obj(g), "webkit");
    if (!jv_isobj(wk)) {
        jsobj *o = js_newobj(op);
        js_defn(o, "messageHandlers", jv_obj(js_newobj(op)), 0);
        js_defn(g, "webkit", jv_obj(o), PF_NOENUM);
        wk = jv_obj(o);
    }
    jsv mh = js_getn(wk, "messageHandlers");
    if (jv_isobj(mh)) {
        if (on) {
            jsobj *h = js_newobj(op);
            js_defnative(h, "postMessage", wk_post_message, 1, 0);
            js_defn(h, "__mosName", jv_str(js_strz(name)), PF_NOENUM);
            js_setn(mh, name, jv_obj(h));
        } else {
            jsv h = js_getn(mh, name);
            if (jv_isobj(h)) js_setn(h, "__mosName", JS_UNDEF);   /* referências guardadas pela página param de mandar */
            js_delprop(jv_o(mh), js_strz(name));
        }
    }
    if (js_pending()) js_catch();
    J.realm = saved;
    js_leave();
}

/* o WKWebView precisa saber se o documento ainda não chegou ao evento load (scripts externos baixando) para o
 * didFinishNavigation; fetch/XHR em andamento não contam, como no WebKit (uma página com long-polling termina) */
bool js_page_busy(js_page_t *p) {
    if (!p) return false;
    if (p->spos < p->nscripts) return true;
    return !p->loaded;
}
