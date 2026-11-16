/* mOS — iframes do motor web (Horizonte e WKWebView): cada <iframe> tem documento, folhas externas, imagens,
 * página JavaScript (reino próprio, ligado ao pai: parent/top/frameElement/contentWindow/postMessage), rolagem e
 * navegação próprias. O hospedeiro chama frames_pump a cada tick e frame_input para cliques/rodinha no iframe;
 * a pintura sai do doc_paint (layout.c, paint_replaced).
 * Origens: src http(s)/file/data:/about:blank e srcdoc. */
#include "web.h"
#include "js.h"

#define FRAME_MAX_DEPTH 4
#define FRAME_MAX_PER_DOC 32
#define FRAME_CSS 8
#define FRAME_GRAVE 8
#define FRAME_HIST 16

struct frame {
    node_t *el;                 /* o <iframe> no documento pai */
    doc_t *owner;               /* documento pai */
    js_page_t *owner_js;
    doc_t *doc;
    js_page_t *js;
    http_req_t *req;
    char url[1024];             /* documento atual */
    char pending[1024];         /* navegação pedida */
    char *pend_post;            /* corpo do POST pendente (kmalloc) */
    int pend_post_len;
    char pend_ctype[64];
    int w, h, scroll;
    float zoom;
    int depth;
    bool started, load_fired, dirty;
    struct { http_req_t *req; node_t *n; } css[FRAME_CSS];
    int ncss;
    u64 css_t0;
    /* documentos antigos: o JS do pai pode guardar objetos deles (contentWindow), então vivem até o pai sair */
    struct { doc_t *d; js_page_t *js; } grave[FRAME_GRAVE];
    int ngrave;
    int sbox;                   /* sandbox do documento atual (JS_SB_*) */
    /* histórico do iframe: documentos e entradas de pushState/fragmento (gen = documento que as criou) */
    struct { char url[1024]; int gen, sid; } hist[FRAME_HIST];
    int nh, hp, gen, trav;      /* trav: entrada a abrir na próxima carga (-1 = navegação nova) */
    char asked[1024];           /* navegação à espera da política do anfitrião */
    int verdict;                /* resposta: 1 permite, -1 nega, 0 nenhuma */
    int nav_kind;               /* WKNavigationType da navegação pedida (0 link, 1 formulário, 2 histórico, 3 recarga, -1 outra) */
    struct frame *next;
};

static void frame_free(frame_t *f);
static void link_init(js_page_t *p, int phase, void *ctx);

/* ---------------------------------------------------------------- data: */
static int b64v(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}
static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

u8 *frame_data_url(const char *url, usize *len, char *mime, int mcap) {
    const char *comma = strchr(url, ',');
    if (!comma || strncmp(url, "data:", 5)) return NULL;
    bool b64 = false;
    int mn = 0;
    for (const char *q = url + 5; q < comma; q++) {
        if (!strncmp(q, ";base64", 7)) { b64 = true; break; }
        if (*q == ';') break;
        if (mn < mcap - 1) mime[mn++] = (char)tolower(*q);
    }
    mime[mn] = 0;
    if (!mn && mcap > 10) strcpy(mime, "text/plain");
    const char *s = comma + 1;
    usize sl = strlen(s);
    u8 *o = kmalloc(sl + 1);
    if (!o) return NULL;
    usize n = 0;
    if (b64) {
        u32 acc = 0;
        int bits = 0;
        for (usize i = 0; i < sl; i++) {
            int c = (u8)s[i];
            if (c == '%' && i + 2 < sl && hexv(s[i + 1]) >= 0 && hexv(s[i + 2]) >= 0) { c = hexv(s[i + 1]) * 16 + hexv(s[i + 2]); i += 2; }
            int v = b64v(c);
            if (v < 0) continue;
            acc = acc << 6 | (u32)v;
            bits += 6;
            if (bits >= 8) { bits -= 8; o[n++] = (u8)(acc >> bits); }
        }
    } else {
        for (usize i = 0; i < sl; i++) {
            if (s[i] == '%' && i + 2 < sl && hexv(s[i + 1]) >= 0 && hexv(s[i + 2]) >= 0) { o[n++] = (u8)(hexv(s[i + 1]) * 16 + hexv(s[i + 2])); i += 2; }
            else o[n++] = (u8)s[i];
        }
    }
    o[n] = 0;
    *len = n;
    return o;
}

static vnode_t *file_node(const char *url) {
    char path[512];
    const char *q = url + 5;
    if (!strncmp(q, "//", 2)) q += 2;
    url_decode_path(q, path, sizeof path);
    vnode_t *v = vfs_lookup(path);
    return v && !v->dir ? v : NULL;
}

/* ---------------------------------------------------------------- documento */
static doc_t *frame_make_doc(const u8 *data, usize n, const char *url, const char *ctype) {
    char cs[48] = { 0 };
    const char *c = ctype ? strstr(ctype, "charset=") : NULL;
    if (c) { int k = 0; c += 8; while (*c && *c != ';' && *c != ' ' && k < 47) cs[k++] = *c++; }
    else html_sniff_charset(data, n, cs, sizeof cs);
    if (ctype && *ctype && strncasecmp(ctype, "text/html", 9) && strncasecmp(ctype, "application/xhtml", 17)) {
        if (!strncasecmp(ctype, "image/", 6)) {
            /* imagem direto no iframe: página com um <img> */
            sbuf_t s = { 0 };
            sb_puts(&s, "<html><body style='margin:0'><img src=\"");
            sb_html(&s, url);
            sb_puts(&s, "\"></body></html>");
            doc_t *d = doc_parse(s.p, (usize)s.len, url);
            sb_free(&s);
            return d;
        }
        if (!strncasecmp(ctype, "text/", 5) || !strncasecmp(ctype, "application/json", 16)) {
            sbuf_t s = { 0 };
            sb_puts(&s, "<html><body><pre>");
            usize ul;
            char *u = charset_to_utf8(data, n, cs[0] ? cs : NULL, &ul);
            if (u) { sb_html(&s, u); kfree(u); }
            sb_puts(&s, "</pre></body></html>");
            doc_t *d = doc_parse(s.p, (usize)s.len, url);
            sb_free(&s);
            return d;
        }
    }
    usize ul;
    char *u = charset_to_utf8(data, n, cs[0] ? cs : NULL, &ul);
    if (!u) return NULL;
    doc_t *d = doc_parse(u, ul, url);
    kfree(u);
    return d;
}

static void css_set_text(doc_t *d, node_t *n, const u8 *data, usize len) {
    usize ul;
    char *u = charset_to_utf8(data, len, NULL, &ul);
    if (!u) return;
    n->text = arena_strndup(&d->arena, u, (int)ul);
    n->len = (int)ul;
    kfree(u);
}

static void css_find(frame_t *f, node_t *r) {
    for (node_t *c = r->first; c && f->ncss < FRAME_CSS; c = c->next) {
        if (c->type != N_ELEM) continue;
        if (c->tag == T_LINK) {
            const char *rel = node_attr(c, "rel"), *href = node_attr(c, "href");
            if (!rel || !href || !*href || strstr(rel, "alternate")) continue;
            char low[64];
            int k = 0;
            for (; rel[k] && k < 63; k++) low[k] = (char)tolower(rel[k]);
            low[k] = 0;
            if (!strstr(low, "stylesheet")) continue;
            char url[1024];
            url_resolve(f->doc->base, href, url, sizeof url);
            if (!strncmp(url, "file:", 5)) { vnode_t *v = file_node(url); if (v) css_set_text(f->doc, c, v->data, v->size); }
            else if (!strncmp(url, "data:", 5)) {
                char mime[64];
                usize len;
                u8 *data = frame_data_url(url, &len, mime, sizeof mime);
                if (data) { css_set_text(f->doc, c, data, len); kfree(data); }
            } else if (!strncmp(url, "http", 4)) {
                http_req_t *rq = http_get(url);
                if (rq) { f->css[f->ncss].req = rq; f->css[f->ncss].n = c; f->ncss++; }
            }
        } else if (c->tag != T_SVG && c->tag != T_SCRIPT && c->tag != T_STYLE) css_find(f, c);
    }
}

static bool css_pump(frame_t *f) {
    if (!f->ncss) return false;
    bool all = true;
    for (int i = 0; i < f->ncss; i++) {
        if (!f->css[i].req) continue;
        int st = http_poll(f->css[i].req);
        if (st == 0) { all = false; continue; }
        http_req_t *r = f->css[i].req;
        if (st > 0 && r->body && r->body_len && r->status < 400) css_set_text(f->doc, f->css[i].n, r->body, r->body_len);
        http_free(r);
        f->css[i].req = NULL;
    }
    if (!all && timer_ticks() - f->css_t0 < 10000) return false;
    for (int i = 0; i < f->ncss; i++) if (f->css[i].req) http_free(f->css[i].req);
    f->ncss = 0;
    if (f->doc->sheet) { css_free(f->doc->sheet); f->doc->sheet = NULL; }
    f->doc->need_style = true;
    return true;
}

/* imagens de um documento (http, data:, file:); devolve true se alguma mudou */
bool doc_pump_images(doc_t *d) {
    bool changed = false;
    int active = 0;
    for (img_t *im = d->images; im; im = im->next) {
        if (im->state != IMG_LOADING || !im->req) continue;
        int st = http_poll(im->req);
        if (st == 0) { active++; continue; }
        if (st > 0 && im->req->body && im->req->body_len && im->req->status < 400) img_set_data(im, im->req->body, im->req->body_len);
        im->state = im->surf ? IMG_OK : IMG_ERROR;
        http_free(im->req);
        im->req = NULL;
        changed = true;
    }
    for (img_t *im = d->images; im && active < 4; im = im->next) {
        if (im->state != IMG_PENDING) continue;
        if (!strncmp(im->url, "file:", 5)) {
            vnode_t *v = file_node(im->url);
            if (v) img_set_data(im, v->data, v->size);
            im->state = im->surf ? IMG_OK : IMG_ERROR;
            changed = true;
        } else if (!strncmp(im->url, "data:", 5)) {
            char mime[64];
            usize len;
            u8 *data = frame_data_url(im->url, &len, mime, sizeof mime);
            if (data) { img_set_data(im, data, len); kfree(data); }
            im->state = im->surf ? IMG_OK : IMG_ERROR;
            changed = true;
        } else if (!strncmp(im->url, "http", 4)) {
            im->req = http_get(im->url);
            if (im->req) { im->state = IMG_LOADING; active++; }
            else { im->state = IMG_ERROR; changed = true; }
        } else { im->state = IMG_ERROR; changed = true; }
    }
    return changed;
}

/* ---------------------------------------------------------------- ciclo de vida */
static void bury(frame_t *f) {
    if (!f->doc && !f->js) return;
    if (f->ngrave == FRAME_GRAVE) {
        /* o mais antigo sai de vez */
        frames_free(f->grave[0].d);
        if (f->grave[0].js) js_page_free(f->grave[0].js);
        if (f->grave[0].d) doc_free(f->grave[0].d);
        memmove(f->grave, f->grave + 1, sizeof f->grave[0] * (FRAME_GRAVE - 1));
        f->ngrave--;
    }
    f->grave[f->ngrave].d = f->doc;
    f->grave[f->ngrave].js = f->js;
    f->ngrave++;
    f->doc = NULL;
    f->js = NULL;
}

/* antes dos scripts do filho: parent/top/frameElement já valem no primeiro script */
static void link_init(js_page_t *p, int phase, void *ctx) {
    frame_t *f = ctx;
    if (phase != 0) { js_page_run_frame_init(p, phase); return; }
    js_page_set_sandbox(p, f->sbox);
    js_page_set_history_host(p, true);
    js_page_set_hist_len(p, f->nh);
    if (f->owner_js) js_page_link_frame(p, f->owner_js, f->el);
    js_page_run_frame_init(p, 0);
}

/* atributo sandbox: sem ele, herda as restrições do documento pai; com ele, só o que os tokens liberam */
static int sandbox_flags(frame_t *f) {
    int parent = f->owner_js ? js_page_sandbox(f->owner_js) : 0;
    const char *a = node_attr(f->el, "sandbox");
    if (!a) return parent;
    static const struct { const char *t; int f; } tok[] = {
        { "allow-scripts", JS_SB_SCRIPTS }, { "allow-same-origin", JS_SB_SAME_ORIGIN }, { "allow-forms", JS_SB_FORMS },
        { "allow-popups", JS_SB_POPUPS }, { "allow-top-navigation", JS_SB_TOP_NAV }, { "allow-modals", JS_SB_MODALS },
        { "allow-top-navigation-by-user-activation", JS_SB_TOP_NAV_USER }, { "allow-downloads", JS_SB_DOWNLOADS },
    };
    int fl = JS_SB_ON;
    for (const char *p = a; *p;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f') p++;
        const char *e = p;
        while (*e && *e != ' ' && *e != '\t' && *e != '\n' && *e != '\r' && *e != '\f') e++;
        for (u32 i = 0; i < ARRAY_LEN(tok); i++)
            if ((usize)(e - p) == strlen(tok[i].t) && !strncasecmp(p, tok[i].t, (usize)(e - p))) fl |= tok[i].f;
        p = e;
    }
    if (parent & JS_SB_ON) fl &= parent | JS_SB_ON;   /* não pode liberar o que o pai já não tem */
    return fl;
}

static void hist_sync(frame_t *f) { if (f->js) js_page_set_hist_len(f->js, f->nh); }

static void hist_push(frame_t *f, const char *url, int sid) {
    if (f->hp < f->nh - 1) f->nh = f->hp + 1;   /* descarta as entradas à frente */
    if (f->nh == FRAME_HIST) { memmove(f->hist, f->hist + 1, sizeof f->hist[0] * (FRAME_HIST - 1)); f->nh--; }
    f->hp = f->nh++;
    snprintf(f->hist[f->hp].url, sizeof f->hist[0].url, "%s", url);
    f->hist[f->hp].gen = f->gen;
    f->hist[f->hp].sid = sid;
}

/* o anfitrião decide a navegação do iframe (decidePolicyForNavigationAction com targetFrame de iframe):
 * 1 segue, 0 espera a resposta (ou o anfitrião ocupado), -1 negada */
static int policy_check(frame_t *f, const char *url) {
    frame_policy_t *fp = f->owner->fpolicy;
    if (!fp || !fp->fn || !url || !strncmp(url, "about:", 6) || !strncmp(url, "javascript:", 11)) return 1;
    if (f->verdict && !strcmp(f->asked, url)) {
        int v = f->verdict;
        f->verdict = 0;
        f->asked[0] = 0;
        return v;
    }
    if (f->asked[0] && !strcmp(f->asked, url)) return 0;
    int r = fp->fn(fp->ctx, f, url, f->nav_kind);
    if (r == 1) return 1;
    if (r == 2) { snprintf(f->asked, sizeof f->asked, "%s", url); f->verdict = 0; }
    return 0;
}

void frame_policy_reply(frame_t *f, bool allow) { if (f && f->asked[0]) f->verdict = allow ? 1 : -1; }
const char *frame_current_url(frame_t *f) { return f ? f->url : ""; }
bool frames_contains(doc_t *root, frame_t *f) {
    for (frame_t *q = root ? root->frames : NULL; q; q = q->next) if (q == f || (q->doc && frames_contains(q->doc, f))) return true;
    return false;
}

static void set_doc(frame_t *f, doc_t *d, const char *url) {
    for (int i = 0; i < f->ncss; i++) if (f->css[i].req) http_free(f->css[i].req);
    f->ncss = 0;
    bury(f);
    f->doc = d;
    f->scroll = 0;
    f->load_fired = false;
    f->dirty = true;
    if (!d) return;
    snprintf(f->url, sizeof f->url, "%s", url);
    f->gen++;
    if (f->trav >= 0 && f->trav < f->nh) {
        f->hp = f->trav;
        snprintf(f->hist[f->hp].url, sizeof f->hist[0].url, "%s", url);
        f->hist[f->hp].gen = f->gen;
        f->hist[f->hp].sid = -1;
    } else hist_push(f, url, -1);
    f->trav = -1;
    f->sbox = sandbox_flags(f);
    d->fpolicy = f->owner->fpolicy;
    f->nav_kind = -1;
    static int seq;
    d->transparent = true;
    d->frame_depth = f->depth;
    d->ctrl_base = 1000000 + (seq++ % 3000) * 1000;
    css_find(f, d->root);
    f->css_t0 = timer_ticks();
    if (f->ncss == 0) d->need_style = true;
    f->js = js_page_new2(d, url, f->w > 0 ? f->w : 300, f->h > 0 ? f->h : 150, link_init, f);
}

static void start_load_post(frame_t *f, const char *url, const char *post, int plen, const char *ctype) {
    if (f->req) { http_free(f->req); f->req = NULL; }
    f->req = http_post(url, ctype && *ctype ? ctype : "application/x-www-form-urlencoded", post, (u32)plen);
    snprintf(f->url, sizeof f->url, "%s", url);
}

static void start_load(frame_t *f, const char *url) {
    if (f->req) { http_free(f->req); f->req = NULL; }
    if (!url || !*url || !strcmp(url, "about:blank")) {
        set_doc(f, doc_parse("<html><body></body></html>", 26, "about:blank"), "about:blank");
        return;
    }
    if (!strncmp(url, "javascript:", 11)) { set_doc(f, doc_parse("<html><body></body></html>", 26, "about:blank"), "about:blank"); return; }
    if (!strncmp(url, "data:", 5)) {
        char mime[64];
        usize len;
        u8 *data = frame_data_url(url, &len, mime, sizeof mime);
        set_doc(f, data ? frame_make_doc(data, len, url, mime) : NULL, url);
        kfree(data);
        return;
    }
    if (!strncmp(url, "file:", 5)) {
        vnode_t *v = file_node(url);
        set_doc(f, v ? frame_make_doc(v->data, v->size, url, NULL) : NULL, url);
        return;
    }
    if (!strncmp(url, "http", 4)) {
        f->req = http_get(url);
        snprintf(f->url, sizeof f->url, "%s", url);
        return;
    }
    set_doc(f, NULL, url);
}

/* primeira carga pelos atributos; false se a política ainda não respondeu */
static bool frame_begin(frame_t *f) {
    const char *sd = node_attr(f->el, "srcdoc");
    if (sd) {
        doc_t *d = doc_parse(sd, strlen(sd), "about:srcdoc");
        if (d) snprintf(d->base, sizeof d->base, "%s", f->owner->base);
        f->started = true;
        set_doc(f, d, "about:srcdoc");
        return true;
    }
    const char *src = node_attr(f->el, "src");
    char url[1024] = "about:blank";
    if (src && *src) url_resolve(f->owner->base, src, url, sizeof url);
    int r = policy_check(f, url);
    if (r == 0) return false;   /* tenta de novo no próximo pump */
    f->started = true;
    if (r > 0) start_load(f, url);
    return true;
}

static frame_t *find_frame(doc_t *d, node_t *el) {
    for (frame_t *f = d->frames; f; f = f->next) if (f->el == el) return f;
    return NULL;
}

static int count_frames(doc_t *d) { int n = 0; for (frame_t *f = d->frames; f; f = f->next) n++; return n; }

static void scan(doc_t *d, node_t *n, js_page_t *js) {
    for (node_t *c = n->first; c; c = c->next) {
        if (c->type != N_ELEM) continue;
        if (c->tag == T_IFRAME) {
            if (!find_frame(d, c) && count_frames(d) < FRAME_MAX_PER_DOC && d->frame_depth < FRAME_MAX_DEPTH) {
                frame_t *f = kcalloc(1, sizeof(frame_t));
                if (!f) return;
                f->el = c;
                f->owner = d;
                f->owner_js = js;
                f->depth = d->frame_depth + 1;
                f->w = 300; f->h = 150; f->zoom = d->zoom > 0 ? d->zoom : 1;
                f->hp = -1;
                f->trav = -1;
                f->nav_kind = -1;
                f->next = d->frames;
                d->frames = f;
                c->frame = f;
            }
        } else if (c->tag != T_SVG && c->tag != T_TEMPLATE) scan(d, c, js);
    }
}

static frame_t *frame_of(node_t *el) { return el ? el->frame : NULL; }

/* liga o documento pai à sua página JS (contentWindow/parent): o hospedeiro chama depois de criar a página */
void frames_set_owner_js(doc_t *d, js_page_t *js) {
    d->frames_js = js;
    for (frame_t *f = d->frames; f; f = f->next) {
        f->owner_js = js;
        if (f->js && js) js_page_link_frame(f->js, js, f->el);
    }
}

js_page_t *frame_page(node_t *el) {
    frame_t *f = frame_of(el);
    return f ? f->js : NULL;
}

/* tamanho e rolagem atuais do iframe (o layout do pai passa em paint_replaced) */
void frame_resize(frame_t *f, int w, int h, float zoom) {
    if (w == f->w && h == f->h && zoom == f->zoom) return;
    f->w = w; f->h = h; f->zoom = zoom;
    if (f->doc) { f->doc->need_layout = true; f->dirty = true; }
}

static int frame_max_scroll(frame_t *f) { return f->doc ? MAX(0, f->doc->height - f->h) : 0; }

/* janelas dos iframes em ordem do documento (window[i]) */
static void collect_kids(node_t *n, js_page_t **out, int *k, int cap) {
    for (node_t *c = n->first; c && *k < cap; c = c->next) {
        if (c->type != N_ELEM) continue;
        if (c->tag == T_IFRAME) { if (c->frame && c->frame->js) out[(*k)++] = c->frame->js; }
        else if (c->tag != T_SVG && c->tag != T_TEMPLATE) collect_kids(c, out, k, cap);
    }
}

/* voltar/avançar no iframe: entrada do mesmo documento troca URL e estado; outra recarrega */
static void frame_go(frame_t *f, int idx) {
    if (idx < 0 || idx >= f->nh || idx == f->hp) return;
    if (f->hist[idx].gen == f->gen && f->js) {
        f->hp = idx;
        snprintf(f->url, sizeof f->url, "%s", f->hist[idx].url);
        js_page_hist_restore(f->js, f->hist[idx].url, f->hist[idx].sid);
        f->dirty = true;
        return;
    }
    f->trav = idx;
    snprintf(f->pending, sizeof f->pending, "%s", f->hist[idx].url);
    f->nav_kind = 2;   /* voltar/avançar */
}

static void hist_ops(frame_t *f) {
    int op, arg;
    char url[1024];
    while (f->js && js_page_take_hist_op(f->js, &op, &arg, url, sizeof url)) {
        if (op == 1) { hist_push(f, url, arg); snprintf(f->url, sizeof f->url, "%s", url); }
        else if (op == 2 && f->hp >= 0) { snprintf(f->hist[f->hp].url, sizeof f->hist[0].url, "%s", url); f->hist[f->hp].sid = arg; snprintf(f->url, sizeof f->url, "%s", url); }
        else if (op == 3) frame_go(f, f->hp + arg);
        hist_sync(f);
    }
}

bool frames_pump(doc_t *d) {
    if (!d) return false;
    bool changed = false;
    scan(d, d->root, d->frames_js);
    if (d->frames_js) {
        js_page_t *kids[32];
        int nk = 0;
        collect_kids(d->root, kids, &nk, 32);
        js_page_set_frames(d->frames_js, kids, nk);
    }
    for (frame_t *f = d->frames; f; f = f->next) {
        if (!f->el->parent) continue;                       /* removido do DOM */
        if (!f->started && frame_begin(f)) changed = true;
        int pr = f->pending[0] ? policy_check(f, f->pending) : 1;
        if (pr < 0) {   /* negada: a navegação some */
            f->pending[0] = 0;
            f->trav = -1;
            if (f->pend_post) { kfree(f->pend_post); f->pend_post = NULL; }
        }
        if (f->pending[0] && pr > 0) {
            char u[1024];
            snprintf(u, sizeof u, "%s", f->pending);
            f->pending[0] = 0;
            if (f->pend_post && !strncmp(u, "http", 4)) start_load_post(f, u, f->pend_post, f->pend_post_len, f->pend_ctype);
            else start_load(f, u);
            if (f->pend_post) { kfree(f->pend_post); f->pend_post = NULL; }
            changed = true;
        }
        if (f->req) {
            int st = http_poll(f->req);
            if (st != 0) {
                http_req_t *r = f->req;
                f->req = NULL;
                const char *u = r->location[0] ? r->location : r->url;
                char url[1024];
                snprintf(url, sizeof url, "%s", u);
                doc_t *nd = NULL;
                if (st > 0 && r->body) nd = frame_make_doc(r->body, r->body_len, url, r->content_type);
                else if (st > 0) nd = doc_parse("<html><body></body></html>", 26, url);
                if (!nd) {
                    sbuf_t s = { 0 };
                    sb_puts(&s, "<html><body style='font-family:sans-serif;color:#888'><p>Não foi possível abrir ");
                    sb_html(&s, url);
                    sb_puts(&s, "</p></body></html>");
                    nd = doc_parse(s.p, (usize)s.len, url);
                    sb_free(&s);
                }
                http_free(r);
                set_doc(f, nd, url);
                changed = true;
            }
        }
        if (!f->doc) continue;
        if (css_pump(f)) changed = true;
        if (doc_pump_images(f->doc)) { f->doc->need_layout = true; changed = true; }
        if (f->js) {
            if (js_page_tick(f->js)) { f->doc->need_layout = true; changed = true; }
            hist_ops(f);
            if (!f->js) continue;
            bool rl = false;
            const char *nav = js_page_take_nav(f->js, &rl);
            if (nav && *nav) {
                char u[1024];
                url_resolve(f->doc->base, nav, u, sizeof u);
                snprintf(f->pending, sizeof f->pending, "%s", u);
                f->nav_kind = -1;
            } else if (rl) { snprintf(f->pending, sizeof f->pending, "%s", f->url); f->trav = f->hp; f->nav_kind = 3; }
            int sc = js_page_take_scroll(f->js);
            if (sc >= 0) { f->scroll = sc; changed = true; }
            node_t *sf = js_page_take_submit(f->js);   /* form.submit() no filho */
            if (sf && (f->sbox & JS_SB_ON) && !(f->sbox & JS_SB_FORMS)) sf = NULL;   /* sandbox sem allow-forms */
            if (sf) {
                char u[1024];
                sbuf_t body = { 0 };
                bool post = form_build(f->doc, sf, NULL, u, sizeof u, &body);
                frame_load(f, u, post ? (body.p ? body.p : "") : NULL, body.len, NULL);
                sb_free(&body);
            }
        }
        if (f->doc->need_layout || f->doc->need_style) {
            if (!f->ncss) { doc_layout(f->doc, f->w, f->h, f->zoom); f->doc->need_layout = false; changed = true; }
        }
        f->scroll = CLAMP(f->scroll, 0, frame_max_scroll(f));
        if (f->js) js_page_viewport(f->js, f->w, f->h, f->scroll);
        /* load do <iframe> no pai quando o filho terminou */
        if (!f->load_fired && !f->req && !f->ncss && (!f->js || !js_page_busy(f->js))) {
            f->load_fired = true;
            if (f->owner_js) js_page_fire(f->owner_js, f->el, "load", false);
            changed = true;
        }
        if (frames_pump(f->doc)) changed = true;
        if (f->dirty) { f->dirty = false; changed = true; }
    }
    return changed;
}

bool frames_busy(doc_t *d) {
    for (frame_t *f = d ? d->frames : NULL; f; f = f->next) {
        if (!f->started || f->req || f->ncss || (f->js && js_page_busy(f->js))) return true;
        for (img_t *im = f->doc ? f->doc->images : NULL; im; im = im->next) if (im->state == IMG_PENDING || im->state == IMG_LOADING) return true;
        if (f->doc && frames_busy(f->doc)) return true;
    }
    return false;
}

int frames_next_ms(doc_t *d) {
    int best = -1;
    for (frame_t *f = d ? d->frames : NULL; f; f = f->next) {
        int ms = f->req || f->ncss ? 10 : f->js ? js_page_next_ms(f->js) : -1;
        int sub = f->doc ? frames_next_ms(f->doc) : -1;
        if (sub >= 0 && (ms < 0 || sub < ms)) ms = sub;
        if (ms >= 0 && (best < 0 || ms < best)) best = ms;
    }
    return best;
}

/* pintura do iframe na caixa r (coordenadas do surface) */
void frame_paint(frame_t *f, ui_t *u, rect_t r) {
    if (!f || !f->doc) return;
    if (r.w != f->w || r.h != f->h) frame_resize(f, r.w, r.h, f->owner->zoom > 0 ? f->owner->zoom : 1);
    if (f->doc->need_layout || f->doc->need_style || !f->doc->rootbox) { doc_layout(f->doc, f->w, f->h, f->zoom); f->doc->need_layout = false; }
    f->scroll = CLAMP(f->scroll, 0, frame_max_scroll(f));
    doc_paint(f->doc, u, r, f->scroll, NULL);
}

/* entrada dentro do iframe (x, y relativos ao canto do iframe). Devolve true se tratou.
 * Clique em link navega o próprio iframe; target=_top/_parent/_blank devolve a URL em out_url (o hospedeiro navega). */
int frame_input(frame_t *f, int kind, int x, int y, int wheel, char *out_url, int cap) {
    if (out_url && cap) out_url[0] = 0;
    if (!f || !f->doc) return false;
    int dy = y + f->scroll;
    /* iframe dentro do iframe */
    box_t *a = doc_hit_atom(f->doc, x, dy);
    if (a && a->node && a->node->tag == T_IFRAME) {
        frame_t *sub = find_frame(f->doc, a->node);
        int ox = a->x + (int)(a->b[3] + a->p[3]), oy = a->y + (int)(a->b[0] + a->p[0]) - f->scroll;
        if (sub) {
            int r = frame_input(sub, kind, x - ox, y - oy, wheel, out_url, cap);
            /* target=_parent do neto navega este iframe */
            if (out_url && out_url[0] == 2) { snprintf(f->pending, sizeof f->pending, "%s", out_url + 1); out_url[0] = 0; }
            return r;
        }
    }
    if (kind == FRAME_WHEEL) {
        int old = f->scroll;
        f->scroll = CLAMP(f->scroll + wheel, 0, frame_max_scroll(f));
        if (f->js) js_page_viewport(f->js, f->w, f->h, f->scroll);
        f->dirty = f->scroll != old;
        return f->scroll != old;
    }
    if (kind != FRAME_CLICK) return false;
    node_t *tg = js_hit_node(f->doc, x, dy);
    bool prevented = false;
    if (f->js && tg) prevented = js_page_event(f->js, tg, JSE_CLICK, 0, 0);
    f->dirty = true;
    if (prevented) return FRAME_PREVENTED;
    frag_t *fr = doc_hit(f->doc, x, dy, NULL);
    node_t *link = fr && fr->node ? fr->node->link : NULL;
    if (!link && tg) for (node_t *q = tg; q; q = q->parent) if (q->tag == T_A && node_attr(q, "href")) { link = q; break; }
    if (!link) return true;
    const char *href = node_attr(link, "href");
    if (!href || !*href) return true;
    char url[1024];
    url_resolve(f->doc->base, href, url, sizeof url);
    if (!strncmp(url, "javascript:", 11)) { if (f->js) js_page_eval_url(f->js, url + 11); return true; }
    const char *t = node_attr(link, "target");
    if (href[0] == '#' && (!t || !*t || !strcasecmp(t, "_self"))) {
        int ay = doc_anchor_y(f->doc, href + 1);
        if (ay >= 0) f->scroll = CLAMP(ay, 0, frame_max_scroll(f));
        if (strcmp(url, f->url)) {
            if (f->hp >= 0) hist_push(f, url, -1);
            snprintf(f->url, sizeof f->url, "%s", url);
            if (f->js) { js_page_hist_restore(f->js, url, -1); js_page_take_scroll(f->js); }
            hist_sync(f);
        }
        return true;
    }
    if (t && (!strcasecmp(t, "_top") || !strcasecmp(t, "_parent") || !strcasecmp(t, "_blank"))) {
        bool blank = !strcasecmp(t, "_blank");
        /* sandbox: popups e navegação da janela de cima só com os tokens (clique conta como ativação do usuário) */
        if ((f->sbox & JS_SB_ON) && (blank ? !(f->sbox & JS_SB_POPUPS) : !(f->sbox & (JS_SB_TOP_NAV | JS_SB_TOP_NAV_USER)))) return true;
        bool parent = !strcasecmp(t, "_parent") && f->depth > 1;
        if (out_url) snprintf(out_url, cap, "%s%s", blank ? "\x01" : parent ? "\x02" : "", url);
        return true;
    }
    snprintf(f->pending, sizeof f->pending, "%s", url);
    f->nav_kind = 0;   /* link */
    return true;
}

/* iframe sob o ponto (coordenadas do documento d); devolve o frame e a posição da caixa de conteúdo */
frame_t *frame_at(doc_t *d, int x, int y, int *ox, int *oy) {
    if (!d || !d->frames) return NULL;
    box_t *a = doc_hit_atom(d, x, y);
    if (!a || !a->node || a->node->tag != T_IFRAME) return NULL;
    frame_t *f = find_frame(d, a->node);
    if (!f) return NULL;
    if (ox) *ox = a->x + (int)(a->b[3] + a->p[3]);
    if (oy) *oy = a->y + (int)(a->b[0] + a->p[0]);
    return f;
}

const char *frame_url(frame_t *f) { return f ? f->url : ""; }
doc_t *frame_document(frame_t *f) { return f ? f->doc : NULL; }

/* navegação pedida de fora (atributo src mudou pelo JS, location do filho) */
void frame_navigate(node_t *el, const char *url) {
    frame_t *f = frame_of(el);
    if (!f) return;
    if (!url) { f->started = false; return; }   /* src/srcdoc mudou: recomeça pelos atributos */
    char u[1024];
    url_resolve(f->owner->base, url && *url ? url : "about:blank", u, sizeof u);
    snprintf(f->pending, sizeof f->pending, "%s", u);
    f->nav_kind = -1;
    f->started = true;
}

static void frame_free(frame_t *f) {
    if (f->el && f->el->frame == f) f->el->frame = NULL;
    if (f->pend_post) kfree(f->pend_post);
    if (f->req) http_free(f->req);
    for (int i = 0; i < f->ncss; i++) if (f->css[i].req) http_free(f->css[i].req);
    for (int i = f->ngrave - 1; i >= 0; i--) {
        if (f->grave[i].d) frames_free(f->grave[i].d);
        if (f->grave[i].js) js_page_free(f->grave[i].js);
        if (f->grave[i].d) doc_free(f->grave[i].d);
    }
    if (f->doc) frames_free(f->doc);
    if (f->js) js_page_free(f->js);
    if (f->doc) doc_free(f->doc);
    kfree(f);
}

/* chamado pelo doc_free antes de liberar o documento pai */
void frames_free(doc_t *d) {
    if (!d) return;
    frame_t *f = d->frames;
    d->frames = NULL;
    while (f) { frame_t *n = f->next; frame_free(f); f = n; }
}

/* ---------------------------------------------------------------- formulários e controles dos iframes */
void frame_load(frame_t *f, const char *url, const char *post, int post_len, const char *ctype) {
    if (!f) return;
    char u[1024];
    url_resolve(f->doc ? f->doc->base : f->owner->base, url, u, sizeof u);
    snprintf(f->pending, sizeof f->pending, "%s", u);
    if (f->pend_post) { kfree(f->pend_post); f->pend_post = NULL; }
    if (post) {
        f->pend_post = kmalloc((usize)post_len + 1);
        if (f->pend_post) { memcpy(f->pend_post, post, (usize)post_len); f->pend_post[post_len] = 0; f->pend_post_len = post_len; }
        snprintf(f->pend_ctype, sizeof f->pend_ctype, "%s", ctype && *ctype ? ctype : "application/x-www-form-urlencoded");
    }
    f->nav_kind = 1;   /* formulário */
    f->started = true;
}

frame_t *frames_frame_of(doc_t *root, doc_t *d) {
    for (frame_t *f = root ? root->frames : NULL; f; f = f->next) {
        if (f->doc == d) return f;
        frame_t *r = f->doc ? frames_frame_of(f->doc, d) : NULL;
        if (r) return r;
    }
    return NULL;
}

js_page_t *frames_page_of(doc_t *root, doc_t *d) {
    frame_t *f = frames_frame_of(root, d);
    return f ? f->js : NULL;
}

ctrl_t *frames_ctrl_by_id(doc_t *root, u32 id, doc_t **owner) {
    for (frame_t *f = root ? root->frames : NULL; f; f = f->next) {
        doc_t *d = f->doc;
        if (!d) continue;
        for (int i = 0; i < d->nctrls; i++) if ((u32)d->ctrls[i]->id == id) { if (owner) *owner = d; return d->ctrls[i]; }
        ctrl_t *c = frames_ctrl_by_id(d, id, owner);
        if (c) return c;
    }
    return NULL;
}

bool form_build(doc_t *d, node_t *form, ctrl_t *submitter, char *out, int cap, sbuf_t *body) {
    const char *action = node_attr(form, "action");
    const char *method = node_attr(form, "method");
    bool post = method && !strcasecmp(method, "post");
    char url[1024];
    url_resolve(d->base, action && *action ? action : d->url, url, sizeof url);
    sbuf_t q = { 0 };
    for (int i = 0; i < d->nctrls; i++) {
        ctrl_t *c = d->ctrls[i];
        if (c->form != form) continue;
        const char *name = node_attr(c->node, "name");
        if (!name || !*name || node_attr(c->node, "disabled")) continue;
        const char *val = NULL;
        switch (c->kind) {
        case C_TEXT: case C_PASSWORD: case C_HIDDEN: case C_TEXTAREA: val = c->buf; break;
        case C_CHECK: case C_RADIO: if (c->checked) val = c->buf && c->buf[0] ? c->buf : "on"; break;
        case C_SELECT: if (c->nopts) val = c->vals[CLAMP(c->sel, 0, c->nopts - 1)]; break;
        case C_SUBMIT: if (c == submitter) { const char *v = node_attr(c->node, "value"); val = v ? v : c->label; } break;
        case C_IMAGE:
            if (c == submitter) {
                if (q.len) sb_putc(&q, '&');
                sb_urlenc(&q, name); sb_puts(&q, ".x=1&"); sb_urlenc(&q, name); sb_puts(&q, ".y=1");
            }
            break;
        default: break;
        }
        if (!val) continue;
        if (q.len) sb_putc(&q, '&');
        sb_urlenc(&q, name);
        sb_putc(&q, '=');
        sb_urlenc(&q, val);
    }
    if (post && !strncmp(url, "http", 4)) {
        snprintf(out, cap, "%s", url);
        if (q.len) sb_putn(body, q.p, q.len);
        sb_free(&q);
        return true;
    }
    char *qm = strchr(url, '?');
    if (qm) *qm = 0;
    char *hm = strchr(url, '#');
    if (hm) *hm = 0;
    snprintf(out, cap, "%s?%s", url, q.p ? q.p : "");
    sb_free(&q);
    return false;
}

/* cursor sobre o iframe (coords do documento d): 0 fora de iframe, senão MOS_WEB_CURSOR_* + 1 */
int frame_cursor(doc_t *d, int x, int y) {
    int ox, oy;
    frame_t *f = frame_at(d, x, y, &ox, &oy);
    if (!f || !f->doc) return 0;
    int fx = x - ox, fy = y - oy + f->scroll;
    int sub = frame_cursor(f->doc, fx, fy);
    if (sub) return sub;
    frag_t *fr = doc_hit(f->doc, fx, fy, NULL);
    if (fr && fr->node && fr->node->link) return FRAME_CUR_HAND;
    for (node_t *q = js_hit_node(f->doc, fx, fy); q; q = q->parent)   /* link em bloco, sem texto */
        if (q->type == N_ELEM && q->tag == T_A && node_attr(q, "href")) return FRAME_CUR_HAND;
    if (fr && fr->node) return FRAME_CUR_TEXT;
    return FRAME_CUR_ARROW;
}

/* window.open pedido por um iframe (em qualquer nível); \x01 + URL, como target=_blank */
bool frames_take_popup(doc_t *d, char *out, int cap) {
    for (frame_t *f = d ? d->frames : NULL; f; f = f->next) {
        const char *u = f->js ? js_page_take_popup(f->js) : NULL;
        if (u) { snprintf(out, (usize)cap, "%s", u); return true; }
        if (f->doc && frames_take_popup(f->doc, out, cap)) return true;
    }
    return false;
}

int frame_sandbox(frame_t *f) { return f ? f->sbox : 0; }
