/* mOS — SYS_WEB: o motor web do sistema (o do Horizonte: html.c, css.c, layout.c e o JavaScript "Brisa") para os
 * apps do macOS, por trás do WKWebView do WebKit.framework do mOS (userland/frameworks/WebKit/WebKit.m).
 *
 * Cada vista é uma "aba sem janela": documento, folhas de estilo externas, imagens, JavaScript, histórico e controles
 * de formulário, pintados num surface_t do tamanho da vista. O app manda o tamanho, a entrada (mouse, roda,
 * teclado) e pede a imagem; o MOS_WEB_PUMP avança a rede e os scripts e devolve o que mudou, para os delegates de
 * navegação e o KVO. As vistas pertencem ao processo que as criou (webview_user_exit as libera quando ele sai).
 *
 * Concorrência: toda syscall roda com a trava grande, o laço do desktop também (ele só a solta no hlt), então o
 * motor (que tem estado global: J, o heap do JS, as folhas) nunca roda em dois núcleos ao mesmo tempo.
 * Pilha: as threads de usuário têm 32 KiB de pilha de kernel e o motor JS aceita até 600 KiB de recursão; por isso
 * o trabalho do motor roda numa pilha própria de 1,5 MiB (on_big_stack), que a trava grande deixa compartilhar. */
#include "kernel.h"
#include "web.h"
#include "js.h"
#include "proc.h"
#include "syscall.h"
#include "gfx.h"
#include "ui.h"

bool uvm_copy_in(process_t *p, void *dst, u64 va, usize len);
bool uvm_copy_out(process_t *p, u64 va, const void *src, usize len);

#define WV_MAX      64
#define WV_HIST     64
#define WV_CSS      16
#define WV_HANDLERS 32
#define WV_MSGQ     256          /* mensagens guardadas por vista antes de descartar as mais antigas */
#define WV_STACK    (1536u << 10)
#define WV_MAXDIM   8192
#define WV_IDBASE   0x5700       /* ids dos controles no ui_t da vista */

typedef struct {
    char url[1024];
    char title[256];
    int scroll;
    char *html;                  /* conteúdo de loadHTMLString/loadData (kmalloc), para voltar a ele */
    usize html_len;
    char mime[64];
    int gen;                     /* documento que criou a entrada (pushState e fragmentos voltam sem recarregar) */
    int sid;                     /* estado do history.pushState (-1 = nenhum) */
} wv_hent_t;
typedef struct wmsg { struct wmsg *next; char name[64]; char *body; int len; int type; int subframe; char url[1024]; } wmsg_t;
typedef struct wscript { struct wscript *next; char *src; int len; int at_end; int all_frames; } wscript_t;

typedef struct webview {
    int pid;                     /* 0 = livre */
    int vw, vh;
    float zoom;
    u32 opts;
    surface_t *surf;
    ui_t ui;
    bool dirty;                  /* a imagem precisa ser refeita */
    /* documento */
    doc_t *doc;
    js_page_t *js;
    char url[1024];
    char title[256];
    char mime[64];
    int http_status;
    int scroll;
    char frag[160];
    bool img_dirty;
    u64 img_dirty_t;
    /* carregamento principal */
    http_req_t *req;
    char req_url[1024];
    bool req_record;
    int req_scroll;
    bool loading, committed;
    int nav_seq;
    char error[256];
    int error_code;
    int last_progress;
    /* folhas externas */
    struct { http_req_t *req; node_t *n; } css[WV_CSS];
    int ncss;
    u64 css_t0;
    /* histórico */
    wv_hent_t hist[WV_HIST];
    int nhist, hpos;
    int gen;                     /* geração do documento atual */
    /* política de navegação (decidePolicyForNavigationAction) */
    bool policy;
    bool pend;
    char pend_url[1024];
    int pend_kind;               /* WKNavigationType */
    bool pend_record;
    int pend_scroll;
    char *pend_post;
    int pend_post_len;
    char pend_ctype[64];
    int pend_hist;               /* 2 = voltar/avançar: índice de destino */
    frame_t *pend_frame;         /* a navegação pendente é deste iframe */
    frame_policy_t fpol;         /* política dos iframes (documento principal e filhos apontam para cá) */
    /* scripts e mensagens */
    char handlers[WV_HANDLERS][64];
    int nhandlers;
    wscript_t *scripts;
    wmsg_t *msgs, *msgs_tail;
    int nmsgs;
    /* interação */
    node_t *hover, *press_link;
    bool press_in;
    ctrl_t *js_clicked;
    doc_t *js_clicked_doc;
    struct { ctrl_t *c; int type; doc_t *d; } jsq[16];
    int njsq;
    node_t *submit_form;
    ctrl_t *submitter;
    doc_t *submit_doc;           /* documento do formulário (o principal ou o de um iframe) */
    ctrl_t *select_open;         /* <select> com a lista aberta (desenhada pela vista: ui_popup pede uma janela) */
    rect_t select_r;
    char newwin_url[1024];
    u32 events;
    sbuf_t result;               /* resultado do último EVAL maior que o buffer do app (MOS_WEB_RESULT) */
    /* diálogo JS bloqueante: a operação suspensa (corrotina) e o resultado de um EVAL que esperou */
    struct wvop *dlg;
    int eval_type;
    bool eval_done;
} webview_t;

static webview_t *views[WV_MAX];
static webview_t *g_cur;           /* vista sendo pintada (gancho dos controles) */

/* ============================================================ pilha própria */
static u8 *g_stack;
static bool g_on_stack;

typedef struct { i64 (*fn)(void *); void *arg; i64 r; } bigcall_t;
/* o kernel compila com a ABI da Microsoft (x86_64-uefi-msvc); a entrada usa a System V para receber o argumento em rdi
 * e não precisar do espaço de sombra de 32 bytes */
__attribute__((sysv_abi)) static void bigcall_entry(bigcall_t *c) { c->r = c->fn(c->arg); }

static i64 on_big_stack(i64 (*fn)(void *), void *arg) {
    if (g_on_stack) return fn(arg);
    if (!g_stack) g_stack = kmalloc(WV_STACK);
    if (!g_stack) return fn(arg);
    bigcall_t c = {fn, arg, 0};
    bigcall_t *cp = &c;
    u64 top = ((u64)(uptr)g_stack + WV_STACK) & ~15ull;
    __attribute__((sysv_abi)) void (*entry)(bigcall_t *) = bigcall_entry;
    g_on_stack = true;
    /* rbx guarda o rsp antigo (é preservado pela função chamada); a pilha nova chega alinhada a 16 antes do call */
    __asm__ volatile(
        "mov %%rsp, %%rbx\n\t"
        "mov %[top], %%rsp\n\t"
        "call *%[entry]\n\t"
        "mov %%rbx, %%rsp\n\t"
        : "+D"(cp), [entry] "+a"(entry), [top] "+S"(top)
        :
        : "rbx", "rcx", "rdx", "r8", "r9", "r10", "r11", "memory", "cc",
          "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
    g_on_stack = false;
    return c.r;
}

/* ============================================================ utilidades */
static void wv_strcpy(char *d, const char *s, usize cap) {
    usize n = s ? strlen(s) : 0;
    if (n >= cap) n = cap - 1;
    if (n) memcpy(d, s, n);
    d[n] = 0;
}

static bool is_hex(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

static int b64v(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

/* URL data: -> bytes (kmalloc) e tipo MIME. A parte não-base64 é decodificada por %XX sem limite de tamanho. */
static u8 *data_url(const char *url, usize *len, char *mime, int mcap, char *charset, int ccap) {
    const char *p = url + 5;
    const char *comma = strchr(p, ',');
    if (!comma) return NULL;
    bool b64 = false;
    int k = 0;
    if (charset && ccap) charset[0] = 0;
    const char *q = p;
    for (; q < comma && *q != ';' && k < mcap - 1; q++) mime[k++] = (char)tolower(*q);
    mime[k] = 0;
    while (q < comma) {
        if (*q == ';') {
            q++;
            if (!strncasecmp(q, "base64", 6)) b64 = true;
            else if (!strncasecmp(q, "charset=", 8) && charset) {
                q += 8;
                int j = 0;
                while (q < comma && *q != ';' && j < ccap - 1) charset[j++] = *q++;
                charset[j] = 0;
                continue;
            }
        }
        q++;
    }
    if (!k) strcpy(mime, "text/plain");
    const char *s = comma + 1;
    usize n = strlen(s);
    u8 *out = kmalloc(n + 1);
    if (!out) return NULL;
    usize o = 0;
    if (b64) {
        u32 acc = 0;
        int bits = 0;
        for (usize i = 0; i < n; i++) {
            if (s[i] == '%' && i + 2 < n) {   /* %2B, %2F e %3D aparecem em base64 dentro de URLs */
                int hi = s[i + 1], lo = s[i + 2];
                int h = isdigit(hi) ? hi - '0' : tolower(hi) - 'a' + 10, l = isdigit(lo) ? lo - '0' : tolower(lo) - 'a' + 10;
                int ch = (h << 4) | l;
                i += 2;
                int v = b64v(ch);
                if (v < 0) continue;
                acc = acc << 6 | (u32)v;
                bits += 6;
            } else {
                int v = b64v(s[i]);
                if (v < 0) continue;
                acc = acc << 6 | (u32)v;
                bits += 6;
            }
            if (bits >= 8) { bits -= 8; out[o++] = (u8)(acc >> bits); }
        }
    } else {
        for (usize i = 0; i < n; i++) {
            if (s[i] == '%' && i + 2 < n && is_hex(s[i + 1]) && is_hex(s[i + 2])) {
                int hi = s[i + 1], lo = s[i + 2];
                int h = isdigit(hi) ? hi - '0' : tolower(hi) - 'a' + 10, l = isdigit(lo) ? lo - '0' : tolower(lo) - 'a' + 10;
                out[o++] = (u8)((h << 4) | l);
                i += 2;
            } else out[o++] = (u8)s[i];
        }
    }
    out[o] = 0;
    *len = o;
    return out;
}

static bool is_image_name(const char *n) {
    const char *e = vfs_ext(n);
    return !strcasecmp(e, "png") || !strcasecmp(e, "jpg") || !strcasecmp(e, "jpeg") || !strcasecmp(e, "gif") ||
           !strcasecmp(e, "bmp") || !strcasecmp(e, "ppm") || !strcasecmp(e, "webp");
}

static doc_t *doc_from_html(const u8 *data, usize n, const char *url, const char *ctype) {
    char cs[40] = "";
    if (ctype) {
        const char *c = strstr(ctype, "charset=");
        if (c) { c += 8; int k = 0; while (*c && *c != ';' && *c != ' ' && k < 39) { if (*c != '"') cs[k++] = *c; c++; } cs[k] = 0; }
    }
    if (!cs[0]) html_sniff_charset(data, n, cs, sizeof cs);
    usize ulen;
    char *u = charset_to_utf8(data, n, cs[0] ? cs : NULL, &ulen);
    if (!u) return NULL;
    doc_t *d = doc_parse(u, ulen, url);
    kfree(u);
    return d;
}

static doc_t *doc_from_text(const u8 *data, usize n, const char *url) {
    sbuf_t s = {0};
    sb_puts(&s, "<html><head></head><body style=\"margin:8px\"><pre style=\"white-space:pre-wrap;margin:0\">");
    usize ulen;
    char *u = charset_to_utf8(data, n, NULL, &ulen);
    if (u) {
        for (usize i = 0; i < ulen; i++) {
            char c = u[i];
            if (c == '&') sb_puts(&s, "&amp;");
            else if (c == '<') sb_puts(&s, "&lt;");
            else sb_putc(&s, c);
        }
        kfree(u);
    }
    sb_puts(&s, "</pre></body></html>");
    doc_t *d = doc_parse(s.p ? s.p : "", (usize)s.len, url);
    sb_free(&s);
    return d;
}

static doc_t *doc_from_image(const u8 *data, usize n, const char *url) {
    surface_t *img = image_decode(data, n);
    sbuf_t s = {0};
    sb_puts(&s, "<html><head></head><body style=\"margin:0;text-align:center\">");
    if (img) { sb_puts(&s, "<img style=\"max-width:100%\" src=\""); sb_html(&s, url); sb_puts(&s, "\">"); }
    sb_puts(&s, "</body></html>");
    doc_t *d = doc_parse(s.p ? s.p : "", (usize)s.len, url);
    sb_free(&s);
    if (d && img) {
        img_t *im = kcalloc(1, sizeof(img_t));
        if (im) {
            wv_strcpy(im->url, url, sizeof im->url);
            im->state = IMG_OK;
            im->surf = img;
            d->images = im;
            d->nimages = 1;
        } else surface_free(img);
    } else if (img) surface_free(img);
    return d;
}

/* documento pelo tipo MIME (loadData:MIMEType:, respostas HTTP, file:) */
static doc_t *doc_by_mime(const u8 *data, usize n, const char *url, const char *mime) {
    if (!strncasecmp(mime, "image/", 6)) return doc_from_image(data, n, url);
    if (!strncasecmp(mime, "text/plain", 10) || !strncasecmp(mime, "application/json", 16) || !strncasecmp(mime, "text/css", 8) ||
        !strncasecmp(mime, "application/javascript", 22) || !strncasecmp(mime, "text/javascript", 15) ||
        !strncasecmp(mime, "text/markdown", 13) || !strncasecmp(mime, "text/csv", 8))
        return doc_from_text(data, n, url);
    if (!mime[0] && n > 4 && (data[0] == 0x89 || data[0] == 0xFF || data[0] == 'G' || data[0] == 'B')) {
        surface_t *probe = image_decode(data, n);
        if (probe) { surface_free(probe); return doc_from_image(data, n, url); }
    }
    return doc_from_html(data, n, url, mime);
}

/* ============================================================ mensagens e scripts injetados */
static void wv_post(void *ctx, const char *name, const char *body, int len, int type) {
    webview_t *w = ctx;
    if (!w || !w->pid) return;
    if (w->nmsgs >= WV_MSGQ) {   /* fila cheia: o app não está lendo; descarta a mais antiga */
        wmsg_t *o = w->msgs;
        w->msgs = o->next;
        if (!w->msgs) w->msgs_tail = NULL;
        kfree(o->body);
        kfree(o);
        w->nmsgs--;
    }
    wmsg_t *m = kcalloc(1, sizeof(wmsg_t));
    if (!m) return;
    m->body = kmalloc((usize)len + 1);
    if (!m->body) { kfree(m); return; }
    memcpy(m->body, body, (usize)len);
    m->body[len] = 0;
    m->len = len;
    m->type = type;
    wv_strcpy(m->name, name, sizeof m->name);
    /* quem mandou: a página principal ou um iframe (WKFrameInfo) */
    js_page_t *from = js_cur_page();
    m->subframe = from && from != w->js;
    wv_strcpy(m->url, from && m->subframe ? js_page_url(from) : w->url, sizeof m->url);
    if (w->msgs_tail) w->msgs_tail->next = m;
    else w->msgs = m;
    w->msgs_tail = m;
    w->nmsgs++;
    w->events |= MOS_WEB_EV_MSG;
}

/* alert/confirm/prompt: sem janela para um diálogo modal, viram mensagens do manipulador interno "__mosAlert" (o
 * framework chama o WKUIDelegate). confirm devolve false e prompt devolve null, como o WebKit sem UIDelegate. */
static const char ALERT_SHIM[] =
    "(function(){var h=window.webkit&&window.webkit.messageHandlers&&window.webkit.messageHandlers.__mosAlert;"
    "if(!h)return;window.close=function(){h.postMessage({kind:'close',message:''});};})();";

/* alert/confirm/prompt: com uma operação em corrotina, suspende até o app responder; sem ela (ou com outro diálogo
 * aberto na vista), avisa o app por mensagem e devolve a resposta padrão do WebKit sem WKUIDelegate */
static int wv_dialog(void *ctx, js_page_t *p, int kind, const char *msg, const char *def, sbuf_t *out);

static void wv_frame_init(js_page_t *p, int phase, void *ctx);
static void wv_page_init(js_page_t *p, int phase, void *ctx) {
    webview_t *w = ctx;
    if (phase == 0) js_page_set_dialog(p, wv_dialog, w);
    if (phase == 0) {
        js_page_wk_bridge(p, wv_post, w);
        js_page_wk_handler(p, "__mosAlert", true);
        js_page_wk_handler(p, "__mosAsync", true);   /* resultado de callAsyncJavaScript (a promessa resolve depois) */
        for (int i = 0; i < w->nhandlers; i++) js_page_wk_handler(p, w->handlers[i], true);
        js_page_eval(p, ALERT_SHIM, (int)sizeof ALERT_SHIM - 1, "mos-alert");
    }
    for (wscript_t *s = w->scripts; s; s = s->next)
        if (s->at_end == phase) js_page_eval(p, s->src, s->len, "WKUserScript");
    if (phase == 0) js_page_set_frame_init(p, wv_frame_init, w);
}

/* página de iframe: diálogos, manipuladores de mensagem e os WKUserScript com forMainFrameOnly:NO */
static void wv_frame_init(js_page_t *p, int phase, void *ctx) {
    webview_t *w = ctx;
    if (phase == 0) {
        js_page_set_dialog(p, wv_dialog, w);
        js_page_wk_bridge(p, wv_post, w);
        js_page_wk_handler(p, "__mosAlert", true);
        for (int i = 0; i < w->nhandlers; i++) js_page_wk_handler(p, w->handlers[i], true);
        js_page_eval(p, ALERT_SHIM, (int)sizeof ALERT_SHIM - 1, "mos-alert");
    }
    for (wscript_t *s = w->scripts; s; s = s->next)
        if (s->at_end == phase && s->all_frames) js_page_eval(p, s->src, s->len, "WKUserScript");
}

/* ============================================================ documento */
static void css_cancel(webview_t *w) {
    for (int i = 0; i < w->ncss; i++) if (w->css[i].req) http_free(w->css[i].req);
    w->ncss = 0;
}

static void css_set_text(doc_t *d, node_t *n, const u8 *data, usize len) {
    usize ul;
    char *u = charset_to_utf8(data, len, NULL, &ul);
    if (!u) return;
    n->text = arena_strndup(&d->arena, u, (int)ul);
    n->len = (int)ul;
    kfree(u);
}

static void css_find(webview_t *w, node_t *r) {
    for (node_t *c = r->first; c && w->ncss < WV_CSS; c = c->next) {
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
            url_resolve(w->doc->base, href, url, sizeof url);
            if (!strncmp(url, "file:", 5)) {
                char path[512];
                const char *q = url + 5;
                if (!strncmp(q, "//", 2)) q += 2;
                url_decode_path(q, path, sizeof path);
                vnode_t *v = vfs_lookup(path);
                if (v && !v->dir) css_set_text(w->doc, c, v->data, v->size);
            } else if (!strncmp(url, "data:", 5)) {
                char mime[64];
                usize len;
                u8 *data = data_url(url, &len, mime, sizeof mime, NULL, 0);
                if (data) { css_set_text(w->doc, c, data, len); kfree(data); }
            } else if (!strncmp(url, "http", 4)) {
                http_req_t *rq = http_get(url);
                if (rq) { w->css[w->ncss].req = rq; w->css[w->ncss].n = c; w->ncss++; }
            }
        } else if (c->tag != T_SVG && c->tag != T_SCRIPT && c->tag != T_STYLE) css_find(w, c);
    }
}

static bool css_pump(webview_t *w) {
    if (!w->ncss || !w->doc) return false;
    bool all = true;
    for (int i = 0; i < w->ncss; i++) {
        if (!w->css[i].req) continue;
        int st = http_poll(w->css[i].req);
        if (st == 0) { all = false; continue; }
        http_req_t *r = w->css[i].req;
        if (st > 0 && r->body && r->body_len && r->status < 400) css_set_text(w->doc, w->css[i].n, r->body, r->body_len);
        http_free(r);
        w->css[i].req = NULL;
    }
    if (!all && timer_ticks() - w->css_t0 < 10000) return false;
    css_cancel(w);
    if (w->doc->sheet) { css_free(w->doc->sheet); w->doc->sheet = NULL; }
    w->doc->need_style = true;
    return true;
}

static void set_doc(webview_t *w, doc_t *d) {
    css_cancel(w);
    if (w->js) { js_page_free(w->js); w->js = NULL; }
    if (w->doc) doc_free(w->doc);
    w->doc = d;
    w->img_dirty = false;
    w->hover = w->press_link = NULL;
    w->js_clicked = NULL;
    w->njsq = 0;
    w->submit_form = NULL;
    w->submitter = NULL;
    w->select_open = NULL;
    w->ui.focus = 0;
    w->ui.active = 0;
    w->dirty = true;
}

static void hent_clear(wv_hent_t *e) {
    kfree(e->html);
    e->html = NULL;
    e->html_len = 0;
    e->mime[0] = 0;
}

static void navigate(webview_t *w, const char *url, bool record, int kind, const char *post, int post_len, const char *ctype, int hist_target);
static void push_history_new(webview_t *w, const char *url);
static void push_history(webview_t *w, const char *url) {
    if (w->nhist && w->hpos >= 0 && !strcmp(w->hist[w->hpos].url, url)) { hent_clear(&w->hist[w->hpos]); return; }
    push_history_new(w, url);
}

/* sempre uma entrada nova (loadHTMLString duas vezes com a mesma URL base são duas páginas no histórico) */
static void push_history_new(webview_t *w, const char *url) {
    if (w->hpos < w->nhist - 1) {   /* navegar depois de voltar descarta as entradas à frente */
        for (int i = w->hpos + 1; i < w->nhist; i++) hent_clear(&w->hist[i]);
        w->nhist = w->hpos + 1;
    }
    if (w->nhist == WV_HIST) {
        hent_clear(&w->hist[0]);
        memmove(w->hist, w->hist + 1, sizeof(wv_hent_t) * (WV_HIST - 1));
        w->nhist--;
        if (w->hpos > 0) w->hpos--;
    }
    wv_hent_t *e = &w->hist[w->nhist];
    memset(e, 0, sizeof *e);
    wv_strcpy(e->url, url, sizeof e->url);
    e->gen = w->gen;
    e->sid = -1;
    w->hpos = w->nhist++;
}

/* liga a página ao histórico da vista (History API) */
static void hist_attach(webview_t *w) {
    if (!w->js) return;
    js_page_set_history_host(w->js, true);
    js_page_set_hist_len(w->js, w->nhist);
}

/* descarta a decisão pendente; um iframe que perguntou recebe "não" (não fica esperando para sempre) */
static void pend_drop(webview_t *w) {
    if (w->pend_frame && w->doc && frames_contains(w->doc, w->pend_frame)) frame_policy_reply(w->pend_frame, false);
    w->pend_frame = NULL;
    w->pend = false;
}

/* iframe quer navegar: com a política ligada, o app decide (MOS_WEB_EV_NAVREQ com req_subframe) */
static int wv_frame_policy(void *ctx, frame_t *f, const char *url, int kind) {
    webview_t *w = ctx;
    if (!w->policy) return 1;
    if (w->pend) return 0;   /* outra decisão em curso: o iframe pergunta de novo depois */
    kfree(w->pend_post);
    w->pend_post = NULL;
    w->pend_post_len = 0;
    wv_strcpy(w->pend_url, url, sizeof w->pend_url);
    w->pend_ctype[0] = 0;
    w->pend_kind = kind;
    w->pend_record = false;
    w->pend_hist = -1;
    w->pend_frame = f;
    w->pend = true;
    w->events |= MOS_WEB_EV_NAVREQ;
    return 2;
}

static void commit(webview_t *w, doc_t *d, const char *url, bool record, int scroll, const char *mime, int status) {
    w->fpol.fn = wv_frame_policy;
    w->fpol.ctx = w;
    if (d) d->fpolicy = &w->fpol;
    if (w->pend_frame) pend_drop(w);   /* o iframe da pergunta vai embora com o documento */
    set_doc(w, d);
    if (d) {
        css_find(w, d->root);
        w->css_t0 = timer_ticks();
        if (d->sheet) { css_free(d->sheet); d->sheet = NULL; }
        d->need_style = true;
        d->zoom = w->zoom;
    }
    bool url_changed = strcmp(w->url, url) != 0;
    wv_strcpy(w->url, url, sizeof w->url);
    wv_strcpy(w->mime, mime ? mime : "text/html", sizeof w->mime);
    w->http_status = status;
    w->scroll = scroll;
    const char *fr = url_fragment(url);
    wv_strcpy(w->frag, fr && *fr ? fr : "", sizeof w->frag);
    w->gen++;
    if (record) push_history(w, url);
    else if (w->nhist && w->hpos >= 0) wv_strcpy(w->hist[w->hpos].url, url, sizeof w->hist[0].url);
    else push_history(w, url);
    if (w->hpos >= 0 && w->hpos < w->nhist) { w->hist[w->hpos].gen = w->gen; w->hist[w->hpos].sid = -1; }
    char old_title[256];
    wv_strcpy(old_title, w->title, sizeof old_title);
    wv_strcpy(w->title, d ? d->title : "", sizeof w->title);
    w->committed = true;
    w->events |= MOS_WEB_EV_COMMIT | MOS_WEB_EV_DIRTY;
    if (url_changed) w->events |= MOS_WEB_EV_URL;
    if (strcmp(old_title, w->title)) w->events |= MOS_WEB_EV_TITLE;
    if (d && !(w->opts & MOS_WEB_OPT_NOJS)) w->js = js_page_new2(d, url, w->vw, w->vh, wv_page_init, w);
    hist_attach(w);
    if (d) frames_set_owner_js(d, w->js);
}

static void fail(webview_t *w, const char *msg, int code) {
    wv_strcpy(w->error, msg, sizeof w->error);
    w->error_code = code;
    w->loading = false;
    w->events |= MOS_WEB_EV_FAIL | MOS_WEB_EV_PROGRESS;
}

static void cancel_load(webview_t *w) {
    if (w->req) { http_free(w->req); w->req = NULL; }
}

/* about:, file: e data: carregam na hora; devolve false se a URL é de rede */
static bool load_local(webview_t *w, const char *url, bool record, int scroll) {
    if (!strncmp(url, "about:", 6)) {
        doc_t *d = doc_parse("<html><head></head><body></body></html>", 39, url);
        commit(w, d, url, record, scroll, "text/html", 200);
        return true;
    }
    if (!strncmp(url, "file:", 5)) {
        const char *p = url + 5;
        if (!strncmp(p, "//", 2)) { p += 2; if (*p != '/') { const char *sl = strchr(p, '/'); p = sl ? sl : "/"; } }
        char path[1024];
        url_decode_path(p, path, sizeof path);
        char *hash = strchr(path, '#');
        if (hash) *hash = 0;
        char *qm = strchr(path, '?');
        if (qm) *qm = 0;
        vnode_t *n = vfs_lookup(path[0] ? path : "/");
        if (!n || n->dir) { fail(w, "O arquivo não foi encontrado.", -1100); return true; }
        const char *e = vfs_ext(n->name);
        doc_t *d;
        const char *mime = "text/html";
        if (!strcasecmp(e, "html") || !strcasecmp(e, "htm") || !strcasecmp(e, "xhtml")) d = doc_from_html(n->data, n->size, url, NULL);
        else if (is_image_name(n->name)) { d = doc_from_image(n->data, n->size, url); mime = "image/png"; }
        else { d = doc_from_text(n->data, n->size, url); mime = "text/plain"; }
        if (!d) { fail(w, "Memória insuficiente para exibir a página.", -1); return true; }
        commit(w, d, url, record, scroll, mime, 200);
        return true;
    }
    if (!strncmp(url, "data:", 5)) {
        char mime[64], cs[32];
        usize len;
        u8 *data = data_url(url, &len, mime, sizeof mime, cs, sizeof cs);
        if (!data) { fail(w, "URL de dados inválida.", -1000); return true; }
        char ct[128];
        snprintf(ct, sizeof ct, "%s%s%s", mime, cs[0] ? ";charset=" : "", cs);
        doc_t *d = doc_by_mime(data, len, url, ct);
        kfree(data);
        if (!d) { fail(w, "Memória insuficiente para exibir a página.", -1); return true; }
        commit(w, d, url, record, scroll, mime, 200);
        return true;
    }
    return false;
}

static void begin(webview_t *w) {
    w->nav_seq++;
    w->loading = true;
    w->committed = false;
    w->error[0] = 0;
    w->error_code = 0;
    w->last_progress = 0;
    w->events |= MOS_WEB_EV_START | MOS_WEB_EV_PROGRESS;
}

/* a parte local (about:, file:, data:) ou de rede de uma navegação já começada (begin) */
static void fetch(webview_t *w, const char *url, bool record, int scroll, const char *post, int post_len, const char *ctype) {
    if (load_local(w, url, record, scroll)) return;
    if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) {
        fail(w, "Este tipo de endereço não é suportado.", -1002);   /* NSURLErrorUnsupportedURL */
        return;
    }
    w->req = post ? http_post(url, ctype && *ctype ? ctype : "application/x-www-form-urlencoded", post, (u32)post_len) : http_get(url);
    wv_strcpy(w->req_url, url, sizeof w->req_url);
    w->req_record = record;
    w->req_scroll = scroll;
    if (!w->req) fail(w, "Não foi possível iniciar a conexão.", -1004);
}

/* navegação já permitida */
static void start(webview_t *w, const char *url, bool record, int scroll, const char *post, int post_len, const char *ctype) {
    /* mesma página, só muda o fragmento: rola até a âncora, sem carregar */
    if (!post && w->doc && w->url[0] && url_same_doc(url, w->url) && url_fragment(url) && !w->req) {
        const char *fr = url_fragment(url);
        int y = doc_anchor_y(w->doc, fr);
        char old[1024];
        wv_strcpy(old, w->url, sizeof old);
        if (record) { if (w->nhist) w->hist[w->hpos].scroll = w->scroll; push_history(w, url); }
        wv_strcpy(w->url, url, sizeof w->url);
        w->scroll = y >= 0 ? y : 0;
        w->events |= MOS_WEB_EV_URL | MOS_WEB_EV_DIRTY;
        /* a página vê o novo URL e recebe popstate/hashchange */
        if (w->js && strcmp(old, url)) { js_page_hist_restore(w->js, url, -1); js_page_take_scroll(w->js); hist_attach(w); }
        return;
    }
    if (record && w->nhist) w->hist[w->hpos].scroll = w->scroll;
    cancel_load(w);
    begin(w);
    fetch(w, url, record, scroll, post, post_len, ctype);
}

/* abre a entrada idx do histórico (voltar, avançar, recarregar): o HTML guardado de um loadHTMLString/loadData, que
 * a URL base sozinha não traz de volta, ou a URL */
static void open_entry(webview_t *w, int idx) {
    if (idx < 0 || idx >= w->nhist) return;
    if (w->hpos >= 0 && w->hpos < w->nhist) w->hist[w->hpos].scroll = w->scroll;
    w->hpos = idx;
    wv_hent_t *e = &w->hist[idx];
    char url[1024];
    wv_strcpy(url, e->url, sizeof url);
    int sc = e->scroll;
    cancel_load(w);
    begin(w);
    if (e->html) {
        doc_t *d = doc_by_mime((const u8 *)e->html, e->html_len, url, e->mime[0] ? e->mime : "text/html;charset=utf-8");
        if (!d) { fail(w, "Memória insuficiente para exibir a página.", -1); return; }
        commit(w, d, url, false, sc, e->mime[0] ? e->mime : "text/html", 200);
        return;
    }
    fetch(w, url, false, sc, NULL, 0, NULL);
}

/* voltar/avançar até a entrada idx: do mesmo documento (pushState, fragmento) só troca URL e estado, sem recarregar */
static void go_entry(webview_t *w, int idx) {
    if (idx < 0 || idx >= w->nhist || idx == w->hpos) return;
    wv_hent_t *e = &w->hist[idx];
    if (e->gen != w->gen || !w->doc || w->req) { navigate(w, e->url, false, 2, NULL, 0, NULL, idx); return; }
    if (w->hpos >= 0 && w->hpos < w->nhist) w->hist[w->hpos].scroll = w->scroll;
    w->hpos = idx;
    bool url_changed = strcmp(w->url, e->url) != 0;
    wv_strcpy(w->url, e->url, sizeof w->url);
    const char *fr = url_fragment(e->url);
    wv_strcpy(w->frag, fr && *fr ? fr : "", sizeof w->frag);
    w->scroll = e->scroll;
    if (w->js) { js_page_hist_restore(w->js, e->url, e->sid); js_page_take_scroll(w->js); }
    w->events |= MOS_WEB_EV_DIRTY | (url_changed ? MOS_WEB_EV_URL : 0);
    w->dirty = true;
}

/* toda navegação passa por aqui: com a política ligada, espera o MOS_WEB_DECIDE do app */
static void navigate(webview_t *w, const char *url, bool record, int kind, const char *post, int post_len, const char *ctype, int hist_target) {
    if (w->policy) {
        kfree(w->pend_post);
        w->pend_post = NULL;
        w->pend_post_len = 0;
        if (post && post_len >= 0) {
            w->pend_post = kmalloc((usize)post_len + 1);
            if (w->pend_post) { memcpy(w->pend_post, post, (usize)post_len); w->pend_post[post_len] = 0; w->pend_post_len = post_len; }
        }
        wv_strcpy(w->pend_url, url, sizeof w->pend_url);
        wv_strcpy(w->pend_ctype, ctype ? ctype : "", sizeof w->pend_ctype);
        w->pend_kind = kind;
        w->pend_record = record;
        w->pend_hist = hist_target;
        w->pend_frame = NULL;
        w->pend = true;
        w->events |= MOS_WEB_EV_NAVREQ;
        return;
    }
    if (hist_target >= 0) { open_entry(w, hist_target); return; }
    start(w, url, record, 0, post, post_len, ctype);
}

static int net_error_code(const char *e) {
    char el[128];
    int k = 0;
    for (; e[k] && k < 127; k++) el[k] = (char)tolower(e[k]);
    el[k] = 0;
    if (!net_configured() || strstr(el, "indispon") || strstr(el, "sem rede")) return -1009;   /* NotConnectedToInternet */
    if (strstr(el, "dns") || strstr(el, "resol") || strstr(el, "host")) return -1003;         /* CannotFindHost */
    if (strstr(el, "tls") || strstr(el, "certific") || strstr(el, "segur")) return -1202;     /* ServerCertificateUntrusted */
    if (strstr(el, "tempo") || strstr(el, "timeout")) return -1001;                          /* TimedOut */
    if (strstr(el, "recus") || strstr(el, "refus")) return -1004;                            /* CannotConnectToHost */
    return -1005;                                                                            /* NetworkConnectionLost */
}

static void finish_request(webview_t *w) {
    http_req_t *r = w->req;
    int st = http_poll(r);
    if (st == 0) {
        int pr = (int)(http_progress(r) * 600);   /* a resposta principal vale 60% do progresso */
        if (pr > w->last_progress) { w->last_progress = pr; w->events |= MOS_WEB_EV_PROGRESS; }
        return;
    }
    char url[1024];
    wv_strcpy(url, r->location[0] ? r->location : w->req_url, sizeof url);
    if (r->location[0] && strcmp(r->location, w->req_url)) w->events |= MOS_WEB_EV_REDIRECT;
    const char *fr = url_fragment(w->req_url);
    if (fr && !strchr(url, '#') && strlen(url) + strlen(fr) + 2 < sizeof url) { strcat(url, "#"); strcat(url, fr); }
    bool record = w->req_record;
    int scroll = w->req_scroll;
    w->req = NULL;
    if (st < 0) {
        const char *e = r->error[0] ? r->error : "A conexão com o servidor falhou.";
        fail(w, e, net_error_code(e));
        http_free(r);
        return;
    }
    char mime[64];
    int k = 0;
    for (; r->content_type[k] && r->content_type[k] != ';' && k < 63; k++) mime[k] = (char)tolower(r->content_type[k]);
    mime[k] = 0;
    doc_t *d;
    if (!r->body || !r->body_len) d = doc_parse("", 0, url);
    else d = doc_by_mime(r->body, r->body_len, url, r->content_type);
    if (!d) { fail(w, "Memória insuficiente para exibir a página.", -1); http_free(r); return; }
    /* como o WebKit: uma resposta 4xx/5xx com corpo é exibida e a navegação termina normalmente */
    commit(w, d, url, record, scroll, mime, r->status);
    http_free(r);
}

static bool pump_images(webview_t *w) {
    doc_t *d = w->doc;
    if (!d) return false;
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
            const char *p = im->url + 5;
            if (!strncmp(p, "//", 2)) p += 2;
            char path[512];
            url_decode_path(p, path, sizeof path);
            vnode_t *n = vfs_lookup(path);
            if (n && !n->dir) img_set_data(im, n->data, n->size); else im->surf = NULL;
            im->state = im->surf ? IMG_OK : IMG_ERROR;
            changed = true;
        } else if (!strncmp(im->url, "data:", 5)) {
            char mime[64];
            usize len;
            u8 *data = data_url(im->url, &len, mime, sizeof mime, NULL, 0);
            if (data) img_set_data(im, data, len); else im->surf = NULL;
            kfree(data);
            im->state = im->surf ? IMG_OK : IMG_ERROR;
            changed = true;
        } else if (!strncmp(im->url, "http", 4)) {
            const char *e = vfs_ext(im->url);
            if (!strncasecmp(e, "avif", 4)) { im->state = IMG_ERROR; changed = true; continue; }
            im->req = http_get(im->url);
            if (im->req) { im->state = IMG_LOADING; active++; }
            else { im->state = IMG_ERROR; changed = true; }
        } else { im->state = IMG_ERROR; changed = true; }
    }
    if (changed) { w->img_dirty = true; w->img_dirty_t = timer_ticks(); }
    return changed;
}

static bool images_loading(webview_t *w) {
    if (!w->doc) return false;
    for (img_t *im = w->doc->images; im; im = im->next) if (im->state == IMG_LOADING || im->state == IMG_PENDING) return true;
    return false;
}

/* envio de formulário do documento d (o principal ou o de um iframe, que navega só o iframe) */
static void submit_form_in(webview_t *w, doc_t *d, node_t *form, ctrl_t *submitter) {
    if (!d || !form) return;
    char url[1024];
    sbuf_t body = {0};
    bool post = form_build(d, form, submitter, url, sizeof url, &body);
    if (d != w->doc) {
        frame_t *f = frames_frame_of(w->doc, d);
        if (f && (frame_sandbox(f) & JS_SB_ON) && !(frame_sandbox(f) & JS_SB_FORMS)) f = NULL;   /* sandbox sem allow-forms */
        if (f) frame_load(f, url, post ? (body.p ? body.p : "") : NULL, body.len, NULL);
    } else if (post) navigate(w, url, true, 1, body.p ? body.p : "", body.len, "application/x-www-form-urlencoded", -1);
    else navigate(w, url, true, 1, NULL, 0, NULL, -1);
    sb_free(&body);
}
static void submit_form(webview_t *w, node_t *form, ctrl_t *submitter) { submit_form_in(w, w->doc, form, submitter); }

/* página JS dona do documento d */
static js_page_t *page_for(webview_t *w, doc_t *d) { return !d || d == w->doc ? w->js : frames_page_of(w->doc, d); }

/* ============================================================ controles de formulário */
static doc_t *g_paint_doc;   /* documento do controle sendo pintado (o principal ou o de um iframe) */
static void js_queue(webview_t *w, ctrl_t *c, int type) {
    if (w->njsq < (int)ARRAY_LEN(w->jsq)) { w->jsq[w->njsq].c = c; w->jsq[w->njsq].type = type; w->jsq[w->njsq].d = g_paint_doc; w->njsq++; }
}

static u32 buf_sum(const char *s) { u32 h = 5381; for (; s && *s; s++) h = h * 33 + (u8)*s; return h; }

static void paint_textarea(ui_t *u, ctrl_t *c, rect_t r) {
    bool hover = u->inside && rect_contains(r, u->mx, u->my);
    if (hover) u->cursor = CURSOR_IBEAM;
    if (u->pressed) {
        if (hover) u->focus = (u32)c->id;
        else if (u->focus == (u32)c->id) u->focus = 0;
    }
    bool f = u->focus == (u32)c->id;
    if (f && u->key) {
        int len = (int)strlen(c->buf);
        bool cmd = u->mods & (MOD_CMD | MOD_CTRL);
        if (u->key == KEY_BACKSPACE) { if (len) c->buf[utf8_prev(c->buf, len)] = 0; u->key_used = true; }
        else if (u->key == KEY_ENTER) { if (len + 1 < c->cap) { c->buf[len] = '\n'; c->buf[len + 1] = 0; } u->key_used = true; }
        else if (u->text[0] && !cmd) {
            int tl = (int)strlen(u->text);
            if (len + tl < c->cap) memcpy(c->buf + len, u->text, (usize)tl + 1);
            u->key_used = true;
        }
        u->caret_blink = u->now;
    }
    if (f) ui_focus_ring(u, r, 3);
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, 3, T.field_border);
    gfx_round_rect(u->s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, 2, T.field_bg);
    rect_t pc = surface_push_clip(u->s, rect_make(r.x + 3, r.y + 2, r.w - 6, r.h - 4));
    font_t *fn = font_get(FONT_REGULAR);
    int h = text_wrap(u->s, fn, 13, rect_make(r.x + 6, r.y + 4, r.w - 12, 100000), 17, T.text, c->buf, false);
    int off = h > r.h - 8 ? h - (r.h - 8) : 0;
    text_wrap(u->s, fn, 13, rect_make(r.x + 6, r.y + 4 - off, r.w - 12, 100000), 17, T.text, c->buf, true);
    if (f) {
        const char *last = strrchr(c->buf, '\n');
        const char *ls = last ? last + 1 : c->buf;
        int lw = text_width(fn, 13, ls);
        int cy = r.y + 4 - off + MAX(0, h - 17);
        if (c->buf[0] && c->buf[strlen(c->buf) - 1] == '\n') { lw = 0; cy += 17; }
        if (lw > r.w - 14) lw = r.w - 14;
        gfx_fill(u->s, r.x + 6 + lw, cy + 1, 1, 15, T.dark ? RGB(230, 230, 230) : RGB(20, 20, 20));
    }
    surface_pop_clip(u->s, pc);
}

/* <select>: o campo fechado; a lista aberta é desenhada por cima de tudo, no fim do quadro (draw_select_list) */
static void paint_select(webview_t *w, ui_t *u, ctrl_t *c, rect_t r) {
    bool hover = u->inside && rect_contains(r, u->mx, u->my);
    if (u->pressed && hover && !w->select_open) { w->select_open = c; w->select_r = r; u->clicked_any = true; u->pressed = false; }
    ui_draw_button(u->s, r, c->nopts ? c->opts[CLAMP(c->sel, 0, c->nopts - 1)] : "", 0, hover ? 1 : 0, false);
    /* a seta do menu */
    int ax = r.x + r.w - 14, ay = r.y + r.h / 2;
    for (int i = 0; i < 4; i++) {
        gfx_fill(u->s, ax - 3 + i, ay - 4 + i, 7 - 2 * i, 1, T.text2);
        gfx_fill(u->s, ax - 3 + i, ay + 3 - i, 7 - 2 * i, 1, T.text2);
    }
}

static void draw_select_list(webview_t *w, ui_t *u) {
    ctrl_t *c = w->select_open;
    if (!c) return;
    int ih = 22, n = c->nopts;
    int lw = MAX(w->select_r.w, 120);
    int lh = MIN(n * ih + 8, MAX(ih + 8, w->vh - 8));
    int lx = CLAMP(w->select_r.x, 0, MAX(0, w->vw - lw));
    int ly = w->select_r.y + w->select_r.h + 2;
    if (ly + lh > w->vh) ly = MAX(0, w->select_r.y - lh - 2);
    rect_t lr = rect_make(lx, ly, lw, lh);
    gfx_round_rect(u->s, lr.x, lr.y, lr.w, lr.h, 6, T.separator);
    gfx_round_rect(u->s, lr.x + 1, lr.y + 1, lr.w - 2, lr.h - 2, 5, T.dark ? RGB(44, 44, 46) : RGB(250, 250, 250));
    int first = 0;
    int vis = (lh - 8) / ih;
    if (c->sel >= vis) first = c->sel - vis + 1;
    rect_t pc = surface_push_clip(u->s, rect_make(lr.x + 2, lr.y + 2, lr.w - 4, lr.h - 4));
    for (int i = first; i < n && i < first + vis; i++) {
        rect_t ir = rect_make(lr.x + 4, lr.y + 4 + (i - first) * ih, lr.w - 8, ih);
        bool hot = u->inside && rect_contains(ir, u->mx, u->my);
        if (hot || i == c->sel) gfx_round_rect(u->s, ir.x, ir.y, ir.w, ir.h, 4, hot ? T.accent : T.selection);
        text_draw_ellipsis(u->s, font_get(FONT_REGULAR), 13, ir.x + 8, ir.y + 15, ir.w - 16, hot ? RGB(255, 255, 255) : T.text, c->opts[i]);
        if (hot && u->released) {
            if (c->sel != i) { c->sel = i; js_queue(w, c, JSE_INPUT); js_queue(w, c, JSE_CHANGE); }
            w->select_open = NULL;
            u->clicked_any = true;
        }
    }
    surface_pop_clip(u->s, pc);
    if (u->pressed && !rect_contains(lr, u->mx, u->my) && !rect_contains(w->select_r, u->mx, u->my)) { w->select_open = NULL; u->clicked_any = true; }
    if (u->key == KEY_ESC) { w->select_open = NULL; u->key_used = true; }
}

static void wv_paint_control(ui_t *u, doc_t *d, ctrl_t *c, rect_t r) {
    webview_t *w = g_cur;
    if (!w) return;
    g_paint_doc = d;
    u32 sum0 = c->buf ? buf_sum(c->buf) : 0;
    bool chk0 = c->checked;
    int sel0 = c->sel;
    bool saved_inside = u->inside;
    if (w->select_open) u->inside = false;   /* a lista aberta pega os cliques */
    switch (c->kind) {
    case C_TEXT: case C_PASSWORD: {
        const char *ph = node_attr(c->node, "placeholder");
        if (c->kind == C_PASSWORD && u->focus != (u32)c->id) {
            char masked[512];
            int n = 0;
            for (const char *p = c->buf; *p && n < 500; p++) if (((u8)*p & 0xC0) != 0x80) masked[n++] = '*';
            masked[n] = 0;
            char save[512];
            wv_strcpy(save, c->buf, sizeof save);
            ui_textfield(u, c->id, r, masked, sizeof masked, ph ? ph : "");
            if (u->focus == (u32)c->id) { wv_strcpy(c->buf, save, (usize)c->cap); u->caret = (int)strlen(c->buf); u->sel_anchor = -1; }
            break;
        }
        if (ui_textfield(u, c->id, r, c->buf, c->cap, ph ? ph : "")) {
            if (c->form) { w->submit_form = c->form; w->submitter = NULL; w->submit_doc = d; }
        }
        break;
    }
    case C_SUBMIT: case C_IMAGE:
        if (ui_button(u, c->id, r, c->label, false)) {
            w->js_clicked = c;
            w->js_clicked_doc = d;
            if (c->form) { w->submit_form = c->form; w->submitter = c; w->submit_doc = d; }
        }
        break;
    case C_RESET:
        if (ui_button(u, c->id, r, c->label[0] ? c->label : " ", false)) {
            w->js_clicked = c;
            w->js_clicked_doc = d;
            for (int i = 0; i < d->nctrls; i++) {
                ctrl_t *o = d->ctrls[i];
                if (o->form != c->form) continue;
                if ((o->kind == C_TEXT || o->kind == C_PASSWORD || o->kind == C_TEXTAREA) && o->buf) {
                    const char *v = o->kind == C_TEXTAREA ? NULL : node_attr(o->node, "value");
                    wv_strcpy(o->buf, v ? v : "", (usize)o->cap);
                } else if (o->kind == C_CHECK || o->kind == C_RADIO) o->checked = node_attr(o->node, "checked") != NULL;
            }
        }
        break;
    case C_BUTTON:
        if (ui_button(u, c->id, r, c->label[0] ? c->label : " ", false)) { w->js_clicked = c; w->js_clicked_doc = d; }
        break;
    case C_CHECK: ui_checkbox(u, c->id, r.x, r.y, NULL, &c->checked); break;
    case C_RADIO:
        if (ui_radio(u, c->id, r.x, r.y, NULL, c->checked)) {
            const char *nm = node_attr(c->node, "name");
            for (int i = 0; i < d->nctrls; i++) {
                ctrl_t *o = d->ctrls[i];
                if (o->kind == C_RADIO && o->form == c->form && nm && node_attr(o->node, "name") && !strcmp(node_attr(o->node, "name"), nm)) o->checked = false;
            }
            c->checked = true;
        }
        break;
    case C_SELECT: paint_select(w, u, c, r); break;
    case C_TEXTAREA: paint_textarea(u, c, r); break;
    default: break;
    }
    u->inside = saved_inside;
    if (c->buf && buf_sum(c->buf) != sum0 && (c->kind == C_TEXT || c->kind == C_PASSWORD || c->kind == C_TEXTAREA)) js_queue(w, c, JSE_INPUT);
    if (c->checked != chk0 || c->sel != sel0) { js_queue(w, c, JSE_INPUT); js_queue(w, c, JSE_CHANGE); }
}

void (*web_paint_control_hook)(ui_t *u, doc_t *d, ctrl_t *c, rect_t r) = wv_paint_control;

/* ============================================================ layout e quadro */
static void layout(webview_t *w) {
    doc_t *d = w->doc;
    if (!d || w->vw <= 0 || w->vh <= 0) return;
    u64 gap = MAX(400, (u64)d->layout_ms * 4);
    if (w->img_dirty && timer_ticks() - d->last_layout > gap && (timer_ticks() - w->img_dirty_t > 120 || !images_loading(w))) {
        w->img_dirty = false;
        d->need_layout = true;
    }
    if (d->need_layout || d->layout_w != w->vw || d->zoom != w->zoom || d->need_style || d->vh != w->vh) {
        d->zoom = w->zoom;
        doc_layout(d, w->vw, w->vh, w->zoom);
        if (w->frag[0]) {
            int y = doc_anchor_y(d, w->frag);
            if (y >= 0) w->scroll = y;
            w->frag[0] = 0;
        }
        w->dirty = true;
    }
    if (d->title[0] && strcmp(w->title, d->title)) {
        wv_strcpy(w->title, d->title, sizeof w->title);
        if (w->nhist && w->hpos >= 0) wv_strcpy(w->hist[w->hpos].title, w->title, sizeof w->hist[0].title);
        w->events |= MOS_WEB_EV_TITLE;
    }
}

static int max_scroll(webview_t *w) { return w->doc ? MAX(0, w->doc->height - w->vh) : 0; }

static void ensure_surface(webview_t *w) {
    if (w->vw <= 0 || w->vh <= 0) return;
    if (!w->surf) w->surf = surface_new(w->vw, w->vh);
    else if (w->surf->w != w->vw || w->surf->h != w->vh) surface_resize(w->surf, w->vw, w->vh);
}

/* um quadro do ui_t da vista: pinta o documento e os controles e trata a entrada transitória (clique, tecla).
 * Depois de um quadro com evento, um segundo passe sem os transitórios mostra o estado novo, como o wm faz. */
static void frame(webview_t *w) {
    ensure_surface(w);
    if (!w->surf) return;
    layout(w);
    ui_t *u = &w->ui;
    surface_t *s = w->surf;
    u->win = NULL;
    u->s = s;
    u->now = timer_ticks();
    u->cw = w->vw;
    u->ch = w->vh;
    u->cursor = CURSOR_ARROW;
    u->nfl = 0;
    s->ox = s->oy = 0;
    surface_reset_clip(s);
    g_cur = w;
    doc_t *d = w->doc;
    rect_t view = rect_make(0, 0, w->vw, w->vh);
    w->scroll = CLAMP(w->scroll, 0, max_scroll(w));
    if (w->js) js_page_viewport(w->js, w->vw, w->vh, w->scroll);
    /* keydown para o JavaScript (controle em foco ou corpo) */
    if (d && u->key && !u->key_used) {
        node_t *tg = d->body;
        js_page_t *kp = w->js;
        for (int i = 0; i < d->nctrls; i++) if ((u32)d->ctrls[i]->id == u->focus) { tg = d->ctrls[i]->node; break; }
        doc_t *fd = NULL;
        ctrl_t *fc = u->focus ? frames_ctrl_by_id(d, u->focus, &fd) : NULL;
        if (fc) { tg = fc->node; kp = frames_page_of(d, fd); }
        if (kp && js_page_event(kp, tg, JSE_KEYDOWN, u->key, u->mods)) { u->key = 0; u->text[0] = 0; u->key_used = true; }
        d = w->doc;
    }
    /* rolagem pelo teclado (sem campo em foco) */
    if (u->key && !u->key_used && !u->focus && d) {
        int k = u->key;
        bool used = true;
        if (k == ' ' && !(u->mods & MOD_SHIFT)) w->scroll += w->vh - 40;
        else if (k == ' ' || k == KEY_PGUP) w->scroll -= w->vh - 40;
        else if (k == KEY_PGDN) w->scroll += w->vh - 40;
        else if (k == KEY_DOWN) w->scroll += 40;
        else if (k == KEY_UP) w->scroll -= 40;
        else if (k == KEY_HOME) w->scroll = 0;
        else if (k == KEY_END) w->scroll = max_scroll(w);
        else used = false;
        if (used) { u->key_used = true; u->key = 0; }
    }
    /* iframes: rodinha e clique sob o ponteiro vão para o documento do iframe */
    bool over_frame = false, frame_prevented = false;
    if (d && d->frames && (u->wheel || u->released || u->pressed) && !w->select_open) {
        int ox, oy;
        frame_t *fr = frame_at(d, u->mx, u->my + w->scroll, &ox, &oy);
        if (fr) {
            int fx = u->mx - ox, fy = u->my + w->scroll - oy;
            if (u->wheel && frame_input(fr, FRAME_WHEEL, fx, fy, u->wheel, NULL, 0)) { u->wheel = 0; w->dirty = true; }
            over_frame = true;
            if (u->released) {
                char out[1024];
                frame_prevented = frame_input(fr, FRAME_CLICK, fx, fy, 0, out, sizeof out) == FRAME_PREVENTED;
                if (out[0] == 1) { wv_strcpy(w->newwin_url, out + 1, sizeof w->newwin_url); w->events |= MOS_WEB_EV_NEWWIN; }
                else if (out[0]) navigate(w, out + (out[0] == 2), true, 0, NULL, 0, NULL, -1);
                w->dirty = true;
            }
            w->press_link = NULL;
            w->press_in = false;
        }
    }
    if (u->wheel) { w->scroll += u->wheel; u->wheel = 0; }
    w->scroll = CLAMP(w->scroll, 0, max_scroll(w));
    node_t *hover = NULL;
    if (d && u->inside && !w->select_open) {
        frag_t *f = doc_hit(d, u->mx, u->my + w->scroll, NULL);
        if (f && f->node && f->node->link) hover = f->node->link;
    }
    w->hover = hover;
    if (d) {
        if (w->opts & MOS_WEB_OPT_TRANSPARENT) {
            u32 bg = d->canvas_bg;
            d->canvas_bg = 0;
            memset(s->px, 0, (usize)s->stride * s->h * 4);
            doc_paint(d, u, view, w->scroll, hover);
            d->canvas_bg = bg;
        } else doc_paint(d, u, view, w->scroll, hover);
    } else if (w->opts & MOS_WEB_OPT_TRANSPARENT) memset(s->px, 0, (usize)s->stride * s->h * 4);
    else gfx_fill(s, 0, 0, w->vw, w->vh, RGB(255, 255, 255));
    draw_select_list(w, u);
    if (hover && u->cursor == CURSOR_ARROW && !w->select_open) u->cursor = CURSOR_HAND;
    if (over_frame && u->cursor == CURSOR_ARROW && d) {
        int fc = frame_cursor(d, u->mx, u->my + w->scroll);
        if (fc == FRAME_CUR_HAND) u->cursor = CURSOR_HAND; else if (fc == FRAME_CUR_TEXT) u->cursor = CURSOR_IBEAM;
    }
    if (u->pressed && !w->select_open && !over_frame) { w->press_link = hover; w->press_in = true; }
    /* eventos dos controles e cliques para o JavaScript */
    bool prevented = false;
    if (d) {
        for (int i = 0; i < w->njsq; i++) { js_page_t *jp = page_for(w, w->jsq[i].d); if (jp) js_page_event(jp, w->jsq[i].c->node, w->jsq[i].type, 0, u->mods); }
        if (w->js && u->released && w->press_in) {
            node_t *tg = w->js_clicked ? w->js_clicked->node : js_hit_node(d, u->mx, u->my + w->scroll);
            if (tg && js_page_event(w->js, tg, JSE_CLICK, 0, u->mods)) {
                prevented = true;
                if (w->js_clicked && w->submitter == w->js_clicked) { w->submit_form = NULL; w->submitter = NULL; }
            }
        }
    }
    w->njsq = 0;
    w->js_clicked = NULL;
    if (u->released) w->press_in = false;
    if (prevented) w->press_link = NULL;
    if (u->released && w->press_link && w->press_link == hover && d && w->doc == d) {
        const char *href = node_attr(hover, "href");
        if (href) {
            while (*href == ' ') href++;
            char abs[1024];
            url_resolve(d->base, href, abs, sizeof abs);
            const char *target = node_attr(hover, "target");
            if (!strncasecmp(href, "javascript:", 11)) { if (w->js) js_page_eval_url(w->js, href + 11); }
            else if (!*href || !strncasecmp(abs, "javascript:", 11)) {}
            else if ((target && !strcasecmp(target, "_blank")) || (u->mods & (MOD_CMD | MOD_CTRL))) {
                wv_strcpy(w->newwin_url, abs, sizeof w->newwin_url);
                w->events |= MOS_WEB_EV_NEWWIN;
            } else navigate(w, abs, true, 0, NULL, 0, NULL, -1);
        }
        w->press_link = NULL;
    }
    if (u->released) w->press_link = NULL;
    if (u->released) u->active = 0;
    /* envio de formulário (pode ser cancelado pelo evento submit) */
    if (w->submit_form && w->doc == d) {
        node_t *f = w->submit_form;
        ctrl_t *sb = w->submitter;
        doc_t *sd = w->submit_doc ? w->submit_doc : d;
        w->submit_form = NULL;
        w->submitter = NULL;
        w->submit_doc = NULL;
        /* no iframe, o clique em "enviar" com preventDefault cancela o envio, como no documento principal */
        bool cancel = sd != d && frame_prevented && sb;
        js_page_t *sp = page_for(w, sd);
        if (!cancel && !(sp && js_page_event(sp, f, JSE_SUBMIT, 0, 0))) submit_form_in(w, sd, f, sb);
    }
    g_cur = NULL;
    bool transient = u->clicked_any || u->key_used || u->released || u->pressed || u->key;
    u->pressed = u->released = u->rpressed = u->dblclick = false;
    u->key = 0; u->text[0] = 0; u->wheel = 0;
    u->clicked_any = u->key_used = false;
    w->dirty = false;
    w->events |= MOS_WEB_EV_DIRTY;
    if (transient) frame(w);
}

/* ============================================================ avanço (PUMP) */
static void pump(webview_t *w) {
    if (w->req) finish_request(w);
    if (css_pump(w)) w->dirty = true;
    if (pump_images(w)) w->dirty = true;
    if (w->doc && frames_pump(w->doc)) w->dirty = true;
    if (w->js) {
        doc_t *d0 = w->doc;
        if (js_page_tick(w->js)) w->dirty = true;
        if (w->doc == d0 && w->js) {
            int sc = js_page_take_scroll(w->js);
            if (sc >= 0) { w->scroll = sc; w->dirty = true; }
            node_t *f = js_page_take_submit(w->js);
            if (f) submit_form(w, f, NULL);
        }
        /* History API: pushState/replaceState/go da página */
        int hop, harg;
        char hurl[1024];
        while (w->js && w->doc == d0 && js_page_take_hist_op(w->js, &hop, &harg, hurl, sizeof hurl)) {
            if (hop == 1) {
                if (w->nhist && w->hpos >= 0) w->hist[w->hpos].scroll = w->scroll;
                push_history_new(w, hurl);
                w->hist[w->hpos].sid = harg;
            } else if (hop == 2 && w->hpos >= 0 && w->hpos < w->nhist) {
                wv_strcpy(w->hist[w->hpos].url, hurl, sizeof w->hist[0].url);
                w->hist[w->hpos].sid = harg;
            } else if (hop == 3) {
                go_entry(w, w->hpos + harg);
                continue;
            }
            if (strcmp(w->url, hurl)) { wv_strcpy(w->url, hurl, sizeof w->url); w->events |= MOS_WEB_EV_URL; }
            const char *fr = url_fragment(hurl);
            wv_strcpy(w->frag, fr && *fr ? fr : "", sizeof w->frag);
            js_page_set_hist_len(w->js, w->nhist);
        }
        /* window.open: da página ou de um iframe → createWebViewWithConfiguration no app */
        if (w->js && w->doc == d0) {
            const char *pu = js_page_take_popup(w->js);
            char pbuf[1024];
            if (!pu && frames_take_popup(w->doc, pbuf, sizeof pbuf)) pu = pbuf;
            if (pu) { wv_strcpy(w->newwin_url, pu, sizeof w->newwin_url); w->events |= MOS_WEB_EV_NEWWIN; }
        }
        if (w->js && w->doc == d0) {
            bool reload = false;
            const char *nu = js_page_take_nav(w->js, &reload);
            if (nu) {
                char url[1024];
                if (reload) wv_strcpy(url, w->url, sizeof url);
                else url_resolve(w->doc ? w->doc->base : w->url, nu, url, sizeof url);
                navigate(w, url, !reload, reload ? 3 : -1, NULL, 0, NULL, -1);
            }
        }
    }
    if (w->doc && (w->doc->need_layout || w->doc->need_style || w->img_dirty)) w->dirty = true;
    if (w->dirty && w->vw > 0 && w->vh > 0) layout(w);
    if (w->loading && w->committed && !w->req && !w->ncss && !images_loading(w) && !js_page_busy(w->js) && !frames_busy(w->doc)) {
        w->loading = false;
        w->last_progress = 1000;
        w->events |= MOS_WEB_EV_FINISH | MOS_WEB_EV_PROGRESS;
    } else if (w->loading && w->committed) {
        int total = 0, done = 0;
        if (w->doc) for (img_t *im = w->doc->images; im; im = im->next) { total++; if (im->state == IMG_OK || im->state == IMG_ERROR) done++; }
        int pr = 600 + (total ? 350 * done / total : 350) - (w->ncss ? 50 : 0);
        if (pr > w->last_progress && pr < 1000) { w->last_progress = pr; w->events |= MOS_WEB_EV_PROGRESS; }
    }
    if (w->dirty) w->events |= MOS_WEB_EV_DIRTY;
}

/* ============================================================ vistas */
static void wv_free(webview_t *w) {
    cancel_load(w);
    set_doc(w, NULL);
    if (w->surf) surface_free(w->surf);
    for (wmsg_t *m = w->msgs, *n; m; m = n) { n = m->next; kfree(m->body); kfree(m); }
    for (wscript_t *s = w->scripts, *n; s; s = n) { n = s->next; kfree(s->src); kfree(s); }
    kfree(w->pend_post);
    sb_free(&w->result);
    for (int i = 0; i < w->nhist; i++) hent_clear(&w->hist[i]);
    kfree(w);
}

static webview_t *wv_get(process_t *p, u64 id) {
    if (id < 1 || id > WV_MAX) return NULL;
    webview_t *w = views[id - 1];
    return w && w->pid == p->pid ? w : NULL;
}

static i64 wv_free_call(void *w) { wv_free(w); return 0; }

static void dialog_abort(webview_t *w);
void webview_user_exit(process_t *p) {
    for (int i = 0; i < WV_MAX; i++) if (views[i] && views[i]->pid == p->pid && views[i]->dlg) dialog_abort(views[i]);
    for (int i = 0; i < WV_MAX; i++)
        if (views[i] && views[i]->pid == p->pid) { webview_t *w = views[i]; views[i] = NULL; on_big_stack(wv_free_call, w); }
}

static char *copy_in_str(process_t *p, u64 va, usize len, usize max) {
    if (!va || len > max) return NULL;
    char *s = kmalloc(len + 1);
    if (!s) return NULL;
    if (len && !uvm_copy_in(p, s, va, len)) { kfree(s); return NULL; }
    s[len] = 0;
    return s;
}

/* string de usuário terminada em 0, até cap-1 bytes */
static bool copy_in_cstr(process_t *p, u64 va, char *out, usize cap) {
    if (!va) { out[0] = 0; return true; }
    for (usize i = 0; i < cap - 1; i++) {
        if (!uvm_copy_in(p, &out[i], va + i, 1)) return false;
        if (!out[i]) return true;
    }
    out[cap - 1] = 0;
    return true;
}

static int map_key(int k) {
    switch (k) {
    case MOS_WEB_K_ESC: return KEY_ESC;
    case MOS_WEB_K_BACKSPACE: return KEY_BACKSPACE;
    case MOS_WEB_K_TAB: return KEY_TAB;
    case MOS_WEB_K_ENTER: return KEY_ENTER;
    case MOS_WEB_K_UP: return KEY_UP;
    case MOS_WEB_K_DOWN: return KEY_DOWN;
    case MOS_WEB_K_LEFT: return KEY_LEFT;
    case MOS_WEB_K_RIGHT: return KEY_RIGHT;
    case MOS_WEB_K_HOME: return KEY_HOME;
    case MOS_WEB_K_END: return KEY_END;
    case MOS_WEB_K_PGUP: return KEY_PGUP;
    case MOS_WEB_K_PGDN: return KEY_PGDN;
    case MOS_WEB_K_DELETE: return KEY_DELETE;
    default: return k;
    }
}

static u16 map_mods(int m) {
    u16 r = 0;
    if (m & MOS_WEB_M_SHIFT) r |= MOD_SHIFT;
    if (m & MOS_WEB_M_CTRL) r |= MOD_CTRL;
    if (m & MOS_WEB_M_ALT) r |= MOD_ALT;
    if (m & MOS_WEB_M_CMD) r |= MOD_CMD;
    return r;
}

/* procura texto nos fragmentos diagramados e rola até ele (findString:) */
static bool find_in_box(box_t *b, const char *needle, int nl, int *y) {
    for (; b; b = b->next) {
        if (b->kind == B_LINES) {
            for (int i = 0; i < b->nfrags; i++) {
                frag_t *f = &b->frags[i];
                if (!f->t || f->len <= 0) continue;
                /* junta o fragmento com os seguintes da mesma caixa para achar frases */
                char buf[512];
                int bl = 0;
                for (int j = i; j < b->nfrags && bl < (int)sizeof buf - 2; j++) {
                    frag_t *g = &b->frags[j];
                    if (!g->t) continue;
                    int n = MIN(g->len, (int)sizeof buf - 2 - bl);
                    memcpy(buf + bl, g->t, (usize)n);
                    bl += n;
                    if (g->sp && bl < (int)sizeof buf - 2) buf[bl++] = ' ';
                    if (bl >= nl + 64) break;
                }
                buf[bl] = 0;
                for (int k = 0; k + nl <= bl; k++)
                    if (!strncasecmp(buf + k, needle, (usize)nl)) { if (k < f->len + 1) { *y = f->y; return true; } break; }
            }
        }
        if (b->first && find_in_box(b->first, needle, nl, y)) return true;
    }
    return false;
}

typedef struct { process_t *p; u64 op, id, a, b; } wvcall_t;

/* ============================================================ operações em corrotina (diálogos que bloqueiam)
 * Cada SYS_WEB roda numa costack. Um alert/confirm/prompt suspende a execução JS (js_suspend_exec) e a corrotina; a
 * syscall volta ao app com MOS_WEB_EV_DIALOG, e MOS_WEB_DIALOG REPLY retoma do ponto exato. Enquanto isso a vista só
 * aceita leituras; as outras vistas seguem normais (o estado de execução do motor foi guardado). */
typedef struct wvop {
    costack_t *cs;
    wvcall_t call;
    i64 ret;
    webview_t *w;                /* vista do diálogo (preenchida pelo gancho) */
    bool detached;               /* a syscall original já voltou: nada mais é copiado para o app */
    int kind;
    char *msg, *def, *frame;
    bool subframe;
    int reply_ok;
    char *reply;
    webview_t *saved_cur;
    doc_t *saved_paint;
} wvop_t;
static wvop_t *g_op;             /* operação rodando agora */
static u8 *g_spare_stack;        /* uma pilha guardada para não alocar 1,5 MiB a cada PUMP */

static i64 wv_dispatch(void *arg) {
    wvcall_t *c = arg;
    process_t *p = c->p;
    u64 op = c->op, id = c->id, a = c->a, b = c->b;
    if (op == MOS_WEB_NEW) {
        int slot = -1;
        for (int i = 0; i < WV_MAX; i++) if (!views[i]) { slot = i; break; }
        if (slot < 0) return -MOS_EMFILE;
        webview_t *w = kcalloc(1, sizeof(webview_t));
        if (!w) return -MOS_ENOMEM;
        w->pid = p->pid;
        w->zoom = 1.0f;
        w->hpos = -1;
        w->ui.focus = 0;
        views[slot] = w;
        return slot + 1;
    }
    webview_t *w = wv_get(p, id);
    if (!w) return -MOS_EBADF;
    switch (op) {
    case MOS_WEB_FREE:
        views[id - 1] = NULL;
        wv_free(w);
        return 0;
    case MOS_WEB_LOAD: {
        mos_web_load_t L;
        if (!uvm_copy_in(p, &L, a, sizeof L)) return -MOS_EFAULT;
        L.content_type[sizeof L.content_type - 1] = 0;
        L.mime[sizeof L.mime - 1] = 0;
        if (L.html) {
            if (L.html_len > (64u << 20)) return -MOS_EINVAL;
            char base[1024];
            if (!copy_in_cstr(p, L.base, base, sizeof base)) return -MOS_EFAULT;
            if (!base[0]) strcpy(base, "about:blank");
            char *html = copy_in_str(p, L.html, (usize)L.html_len, 64u << 20);
            if (!html) return -MOS_ENOMEM;
            cancel_load(w);
            pend_drop(w);
            begin(w);
            const char *mime = L.mime[0] ? L.mime : "text/html;charset=utf-8";
            doc_t *d = doc_by_mime((u8 *)html, (usize)L.html_len, base, mime);
            if (!d) { kfree(html); fail(w, "Memória insuficiente para exibir a página.", -1); return 0; }
            if (w->nhist && w->hpos >= 0) w->hist[w->hpos].scroll = w->scroll;
            push_history_new(w, base);
            commit(w, d, base, false, 0, L.mime[0] ? L.mime : "text/html", 200);
            /* a entrada guarda o conteúdo: voltar a ela não pode depender da URL base (about:blank) */
            if (w->hpos >= 0 && w->hpos < w->nhist) {
                wv_hent_t *e = &w->hist[w->hpos];
                hent_clear(e);
                e->html = html;
                e->html_len = (usize)L.html_len;
                wv_strcpy(e->mime, mime, sizeof e->mime);
            } else kfree(html);
            return 0;
        }
        char url[1024];
        if (!copy_in_cstr(p, L.url, url, sizeof url)) return -MOS_EFAULT;
        if (!url[0]) return -MOS_EINVAL;
        char *post = NULL;
        if (L.post) {
            post = copy_in_str(p, L.post, (usize)L.post_len, 16u << 20);
            if (!post) return -MOS_ENOMEM;
        }
        /* o app pediu: com a política ligada o WebKit também pergunta (WKNavigationTypeOther) */
        navigate(w, url, true, -1, post, post ? (int)L.post_len : 0, L.content_type, -1);
        kfree(post);
        return 0;
    }
    case MOS_WEB_SIZE: {
        int nw = (int)a, nh = (int)b;
        if (nw < 0 || nh < 0 || nw > WV_MAXDIM || nh > WV_MAXDIM) return -MOS_EINVAL;
        if (nw != w->vw || nh != w->vh) { w->vw = nw; w->vh = nh; w->dirty = true; w->select_open = NULL; }
        return 0;
    }
    case MOS_WEB_PUMP: {
        pump(w);
        u32 ev = w->events;
        w->events = 0;
        return ev;
    }
    case MOS_WEB_PAINT: {
        if (w->vw <= 0 || w->vh <= 0) return 0;
        usize need = (usize)w->vw * (usize)w->vh * 4;
        if (b < need) return -MOS_EINVAL;
        if (!w->dlg && (w->dirty || !w->surf || w->surf->w != w->vw || w->surf->h != w->vh)) frame(w);
        if (g_op && g_op->detached) return 0;      /* retomado depois de um diálogo: o buffer do app já não é este */
        if (!w->surf) return -MOS_ENOMEM;
        surface_t *s = w->surf;
        if (w->opts & MOS_WEB_OPT_TRANSPARENT) {
            /* o surface é pré-multiplicado e o app compõe ARGB direto (cx_gfx_image_px pré-multiplica de novo) */
            u32 *row = kmalloc((usize)s->w * 4);
            if (!row) return -MOS_ENOMEM;
            for (int y = 0; y < s->h; y++) {
                const u32 *src = s->px + (usize)y * (usize)s->stride;
                for (int x = 0; x < s->w; x++) {
                    u32 c = src[x], al = c >> 24;
                    if (al == 0) row[x] = 0;
                    else if (al == 255) row[x] = c;
                    else row[x] = al << 24 | MIN(255u, ((c >> 16) & 255) * 255 / al) << 16 | MIN(255u, ((c >> 8) & 255) * 255 / al) << 8 | MIN(255u, (c & 255) * 255 / al);
                }
                if (!uvm_copy_out(p, a + (u64)y * (u64)s->w * 4, row, (usize)s->w * 4)) { kfree(row); return -MOS_EFAULT; }
            }
            kfree(row);
        } else if (s->stride == s->w) { if (!uvm_copy_out(p, a, s->px, need)) return -MOS_EFAULT; }
        else for (int y = 0; y < s->h; y++)
            if (!uvm_copy_out(p, a + (u64)y * (u64)s->w * 4, s->px + (usize)y * (usize)s->stride, (usize)s->w * 4)) return -MOS_EFAULT;
        return (i64)need;
    }
    case MOS_WEB_INFO: {
        mos_web_info_t *I = kcalloc(1, sizeof(mos_web_info_t));
        if (!I) return -MOS_ENOMEM;
        wv_strcpy(I->url, w->url, sizeof I->url);
        wv_strcpy(I->title, w->title, sizeof I->title);
        if (w->pend) {
            wv_strcpy(I->req_url, w->pend_url, sizeof I->req_url);
            I->req_kind = w->pend_kind;
            I->req_subframe = w->pend_frame != NULL;
            wv_strcpy(I->req_frame, w->pend_frame ? frame_current_url(w->pend_frame) : w->url, sizeof I->req_frame);
        }
        wv_strcpy(I->newwin, w->newwin_url, sizeof I->newwin);
        w->newwin_url[0] = 0;
        I->nav_seq = w->nav_seq;
        wv_strcpy(I->error, w->error, sizeof I->error);
        I->error_code = w->error_code;
        I->loading = w->loading;
        I->progress = w->loading ? w->last_progress : 1000;
        I->can_back = w->hpos > 0;
        I->can_forward = w->hpos >= 0 && w->hpos < w->nhist - 1;
        I->content_h = w->doc ? w->doc->height : 0;
        I->scroll = w->scroll;
        I->vw = w->vw;
        I->vh = w->vh;
        I->http_status = w->http_status;
        I->js_errors = js_page_errors(w->js);
        I->secure = !strncmp(w->url, "https://", 8);
        I->bg = w->doc ? w->doc->canvas_bg : 0xFFFFFFFFu;
        wv_strcpy(I->mime, w->mime, sizeof I->mime);
        bool ok = uvm_copy_out(p, a, I, sizeof *I);
        kfree(I);
        return ok ? 0 : -MOS_EFAULT;
    }
    case MOS_WEB_INPUT: {
        mos_web_input_t in;
        if (!uvm_copy_in(p, &in, a, sizeof in)) return -MOS_EFAULT;
        in.text[sizeof in.text - 1] = 0;
        ui_t *u = &w->ui;
        u->mods = map_mods(in.mods);
        switch (in.kind) {
        case MOS_WEB_IN_DOWN:
            u->mx = in.x; u->my = in.y; u->inside = true; u->down = true; u->pressed = true;
            u->dblclick = in.clicks >= 2;
            u->drag_x = in.x; u->drag_y = in.y;
            break;
        case MOS_WEB_IN_UP: u->mx = in.x; u->my = in.y; u->inside = true; u->down = false; u->released = true; break;
        case MOS_WEB_IN_MOVE: u->mx = in.x; u->my = in.y; u->inside = true; break;
        case MOS_WEB_IN_EXIT: u->inside = false; break;
        case MOS_WEB_IN_WHEEL: u->mx = in.x; u->my = in.y; u->inside = true; u->wheel += in.wheel; break;
        case MOS_WEB_IN_KEY: {
            int k = map_key(in.key);
            if (!k && in.text[0]) k = (u8)in.text[0];
            u->key = k;
            wv_strcpy(u->text, in.text, sizeof u->text);
            break;
        }
        case MOS_WEB_IN_BLUR: u->focus = 0; w->select_open = NULL; break;
        default: return -MOS_EINVAL;
        }
        node_t *old_hover = w->hover;
        bool moved_only = in.kind == MOS_WEB_IN_MOVE || in.kind == MOS_WEB_IN_EXIT;
        if (moved_only && !u->down && !w->select_open && !u->focus) {
            /* movimento: só o cursor e o realce de link; repinta quando o link sob o mouse muda */
            node_t *hv = NULL;
            int cur = MOS_WEB_CURSOR_ARROW;
            if (w->doc && u->inside) {
                int fc = frame_cursor(w->doc, u->mx, u->my + w->scroll);   /* o iframe sob o ponteiro decide */
                frag_t *f = fc ? NULL : doc_hit(w->doc, u->mx, u->my + w->scroll, NULL);
                if (fc) cur = fc == FRAME_CUR_HAND ? MOS_WEB_CURSOR_HAND : fc == FRAME_CUR_TEXT ? MOS_WEB_CURSOR_IBEAM : MOS_WEB_CURSOR_ARROW;
                else if (f && f->node && f->node->link) { hv = f->node->link; cur = MOS_WEB_CURSOR_HAND; }
                else if (f && f->node) cur = MOS_WEB_CURSOR_IBEAM;
            }
            if (hv != old_hover) { w->dirty = true; frame(w); }
            return cur;
        }
        frame(w);
        return u->cursor == CURSOR_HAND ? MOS_WEB_CURSOR_HAND : u->cursor == CURSOR_IBEAM ? MOS_WEB_CURSOR_IBEAM : MOS_WEB_CURSOR_ARROW;
    }
    case MOS_WEB_EVAL: {
        mos_web_eval_t E;
        if (!uvm_copy_in(p, &E, a, sizeof E)) return -MOS_EFAULT;
        char *src = copy_in_str(p, E.src, E.len, 16u << 20);
        if (!src) return -MOS_ENOMEM;
        sbuf_t out = {0};
        int type;
        if (!w->js) {
            if (!w->doc || (w->opts & MOS_WEB_OPT_NOJS)) { kfree(src); sb_puts(&out, "JavaScript não está ativo nesta vista"); type = JS_WK_EXCEPTION; goto eval_out; }
            w->js = js_page_new2(w->doc, w->url, w->vw, w->vh, wv_page_init, w);
            hist_attach(w);
            frames_set_owner_js(w->doc, w->js);
            if (!w->js) { kfree(src); sb_puts(&out, "sem memória para o JavaScript"); type = JS_WK_EXCEPTION; goto eval_out; }
        }
        {
            doc_t *d0 = w->doc;
            type = js_page_eval_value(w->js, src, (int)E.len, &out);
            kfree(src);
            if (w->doc == d0 && d0) { d0->need_layout = true; w->dirty = true; }
        }
    eval_out:
        if (g_op && g_op->detached) {
            /* este EVAL esperou um diálogo: a syscall já voltou; o app busca com MOS_WEB_EVALDONE */
            sb_free(&w->result);
            w->result = out;
            w->eval_type = type;
            w->eval_done = true;
            w->events |= MOS_WEB_EV_EVALDONE;
            return 0;
        }
        E.type = type;
        {
            u32 n = (u32)out.len;
            E.len = n;
            sb_free(&w->result);
            if (n <= E.cap) {
                if (n && E.out && !uvm_copy_out(p, E.out, out.p, n)) { sb_free(&out); return -MOS_EFAULT; }
                sb_free(&out);
            } else w->result = out;   /* não coube: o framework busca com MOS_WEB_RESULT (rodar de novo repetiria os efeitos) */
        }
        if (!uvm_copy_out(p, a, &E, sizeof E)) return -MOS_EFAULT;
        return E.len;
    }
    case MOS_WEB_GO:
        switch ((int)a) {
        case MOS_WEB_GO_BACK:
            if (w->hpos > 0) go_entry(w, w->hpos - 1);
            return 0;
        case MOS_WEB_GO_FORWARD:
            if (w->hpos >= 0 && w->hpos < w->nhist - 1) go_entry(w, w->hpos + 1);
            return 0;
        case MOS_WEB_GO_RELOAD:
            if (w->hpos >= 0 && w->hpos < w->nhist) navigate(w, w->hist[w->hpos].url, false, 3, NULL, 0, NULL, w->hpos);
            return 0;
        case MOS_WEB_GO_STOP:
            if (w->req) { cancel_load(w); fail(w, "cancelado", -999); }   /* NSURLErrorCancelled */
            pend_drop(w);
            return 0;
        }
        return -MOS_EINVAL;
    case MOS_WEB_HANDLER: {
        char name[64];
        if (!copy_in_cstr(p, a, name, sizeof name) || !name[0]) return -MOS_EINVAL;
        int at = -1;
        for (int i = 0; i < w->nhandlers; i++) if (!strcmp(w->handlers[i], name)) at = i;
        if (b) {
            if (at >= 0) return -MOS_EINVAL;   /* o WebKit lança NSInvalidArgumentException para nome repetido */
            if (w->nhandlers >= WV_HANDLERS) return -MOS_ENOMEM;
            wv_strcpy(w->handlers[w->nhandlers++], name, sizeof w->handlers[0]);
        } else {
            if (at < 0) return 0;
            memmove(w->handlers[at], w->handlers[at + 1], (usize)(w->nhandlers - at - 1) * sizeof w->handlers[0]);
            w->nhandlers--;
        }
        if (w->js) js_page_wk_handler(w->js, name, b != 0);
        return 0;
    }
    case MOS_WEB_TAKEMSG: {
        wmsg_t *m = w->msgs;
        if (!m) return 0;
        mos_web_msg_t *M = kcalloc(1, sizeof(mos_web_msg_t));
        if (!M) return -MOS_ENOMEM;
        if (!uvm_copy_in(p, M, a, sizeof *M)) { kfree(M); return -MOS_EFAULT; }
        u64 body = M->body;
        u32 cap = M->cap;
        memset(M, 0, sizeof *M);
        wv_strcpy(M->name, m->name, sizeof M->name);
        wv_strcpy(M->url, m->url, sizeof M->url);
        M->subframe = m->subframe;
        M->type = m->type;
        M->len = (u32)m->len;
        M->cap = cap;
        M->body = body;
        /* não cabe: devolve o tamanho e deixa a mensagem na fila para o framework tentar com um buffer maior */
        if ((u32)m->len > cap) { bool ok = uvm_copy_out(p, a, M, sizeof *M); kfree(M); return ok ? -MOS_EINVAL : -MOS_EFAULT; }
        bool ok = (!m->len || uvm_copy_out(p, body, m->body, (usize)m->len)) && uvm_copy_out(p, a, M, sizeof *M);
        kfree(M);
        if (!ok) return -MOS_EFAULT;
        w->msgs = m->next;
        if (!w->msgs) w->msgs_tail = NULL;
        w->nmsgs--;
        kfree(m->body);
        kfree(m);
        return 1;
    }
    case MOS_WEB_SCRIPT: {
        mos_web_script_t S;
        if (!uvm_copy_in(p, &S, a, sizeof S)) return -MOS_EFAULT;
        if (!S.src) {
            for (wscript_t *s = w->scripts, *n; s; s = n) { n = s->next; kfree(s->src); kfree(s); }
            w->scripts = NULL;
            return 0;
        }
        wscript_t *s = kcalloc(1, sizeof(wscript_t));
        if (!s) return -MOS_ENOMEM;
        s->src = copy_in_str(p, S.src, S.len, 16u << 20);
        if (!s->src) { kfree(s); return -MOS_ENOMEM; }
        s->len = (int)S.len;
        s->at_end = S.at_end ? 1 : 0;
        s->all_frames = S.all_frames ? 1 : 0;
        wscript_t **pp = &w->scripts;
        while (*pp) pp = &(*pp)->next;
        *pp = s;
        return 0;
    }
    case MOS_WEB_POLICY:
        w->policy = a != 0;
        return 0;
    case MOS_WEB_DECIDE: {
        if (!w->pend) return -MOS_EINVAL;
        w->pend = false;
        if (w->pend_frame) {   /* resposta para um iframe (se ele ainda existe) */
            frame_t *f = w->pend_frame;
            w->pend_frame = NULL;
            if (w->doc && frames_contains(w->doc, f)) frame_policy_reply(f, a != 0);
            w->dirty = true;
            return 0;
        }
        if (!a) { kfree(w->pend_post); w->pend_post = NULL; return 0; }
        char *post = w->pend_post;
        int plen = w->pend_post_len;
        w->pend_post = NULL;
        char url[1024], ct[64];
        wv_strcpy(url, w->pend_url, sizeof url);
        wv_strcpy(ct, w->pend_ctype, sizeof ct);
        if (w->pend_hist >= 0 && w->pend_hist < w->nhist) open_entry(w, w->pend_hist);
        else start(w, url, w->pend_record, 0, post, plen, ct);
        kfree(post);
        return 0;
    }
    case MOS_WEB_SCROLL:
        w->scroll = CLAMP((int)a, 0, max_scroll(w));
        w->dirty = true;
        return 0;
    case MOS_WEB_OPTS:
        if (((u32)a ^ w->opts) & MOS_WEB_OPT_TRANSPARENT) w->dirty = true;
        w->opts = (u32)a;
        return 0;
    case MOS_WEB_ZOOM: {
        float z = (float)(i64)a / 1000.0f;
        if (z < 0.25f || z > 5.0f) return -MOS_EINVAL;
        if (z != w->zoom) { w->zoom = z; w->dirty = true; }
        return 0;
    }
    case MOS_WEB_HISTORY: {
        int idx = w->hpos + (int)(i64)a;
        if (w->hpos < 0 || idx < 0 || idx >= w->nhist) return 0;
        mos_web_hist_t H;
        wv_strcpy(H.url, w->hist[idx].url, sizeof H.url);
        wv_strcpy(H.title, idx == w->hpos ? w->title : w->hist[idx].title, sizeof H.title);
        if (!uvm_copy_out(p, b, &H, sizeof H)) return -MOS_EFAULT;
        return 1;
    }
    case MOS_WEB_FIND: {
        char needle[256];
        if (!copy_in_cstr(p, a, needle, sizeof needle) || !needle[0] || !w->doc || !w->doc->rootbox) return 0;
        int y = 0;
        if (!find_in_box(w->doc->rootbox, needle, (int)strlen(needle), &y)) return 0;
        w->scroll = CLAMP(y - w->vh / 3, 0, max_scroll(w));
        w->dirty = true;
        return 1;
    }
    case MOS_WEB_TEXT: {
        if (!w->doc || !w->doc->body) return 0;
        sbuf_t t = {0};
        node_text_content(w->doc->body, &t);
        u32 n = (u32)t.len;
        u32 cp = MIN(n, (u32)b);
        bool ok = !cp || uvm_copy_out(p, a, t.p, cp);
        sb_free(&t);
        return ok ? (i64)n : -MOS_EFAULT;
    }
    case MOS_WEB_EVALDONE: {
        if (!w->eval_done) return -MOS_ENOENT;
        mos_web_eval_t E;
        if (!uvm_copy_in(p, &E, a, sizeof E)) return -MOS_EFAULT;
        E.type = w->eval_type;
        E.len = (u32)w->result.len;
        if (E.len <= E.cap) {
            if (E.len && E.out && !uvm_copy_out(p, E.out, w->result.p, E.len)) return -MOS_EFAULT;
            sb_free(&w->result);
        }
        w->eval_done = false;
        if (!uvm_copy_out(p, a, &E, sizeof E)) return -MOS_EFAULT;
        return E.len;
    }
    case MOS_WEB_RESULT: {
        u32 n = (u32)w->result.len;
        if (n > (u32)b) return -MOS_EINVAL;
        bool ok = !n || uvm_copy_out(p, a, w->result.p, n);
        sb_free(&w->result);
        return ok ? (i64)n : -MOS_EFAULT;
    }
    }
    return -MOS_EINVAL;
}

static char *dup_str(const char *s) {
    if (!s) return NULL;
    usize n = strlen(s);
    char *d = kmalloc(n + 1);
    if (d) memcpy(d, s, n + 1);
    return d;
}

static void op_entry(void *arg) {
    wvop_t *o = arg;
    o->ret = wv_dispatch(&o->call);
}

static void op_free(wvop_t *o) {
    if (o->cs) { u8 *st = cs_release(o->cs); if (!g_spare_stack) g_spare_stack = st; else kfree(st); }
    kfree(o->msg); kfree(o->def); kfree(o->frame); kfree(o->reply);
    kfree(o);
}

/* roda (ou retoma) a operação; devolve true se terminou */
static bool op_run(wvop_t *o) {
    wvop_t *prev = g_op;
    bool prev_stack = g_on_stack;
    g_op = o;
    g_on_stack = true;                 /* chamadas aninhadas de on_big_stack já estão numa pilha grande */
    bool done = cs_resume(o->cs);
    g_on_stack = prev_stack;
    g_op = prev;
    return done;
}

static int wv_dialog(void *ctx, js_page_t *p, int kind, const char *msg, const char *def, sbuf_t *out) {
    webview_t *w = ctx;
    wvop_t *o = g_op;
    if (!o || !o->cs || cs_current() != o->cs || w->dlg || !w->pid) {
        /* não dá para esperar: o app recebe a mensagem (WKUIDelegate) e a página a resposta padrão */
        sbuf_t j = {0};
        sb_puts(&j, "{\"kind\":\"");
        sb_puts(&j, kind == JS_DLG_CONFIRM ? "confirm" : kind == JS_DLG_PROMPT ? "prompt" : "alert");
        sb_puts(&j, "\",\"message\":\"");
        for (const char *q = msg; q && *q; q++) {
            if (*q == '"' || *q == '\\') { sb_putc(&j, '\\'); sb_putc(&j, *q); }
            else if ((u8)*q < 0x20) sb_printf(&j, "\\u%04x", (u8)*q);
            else sb_putc(&j, *q);
        }
        sb_puts(&j, "\"}");
        wv_post(w, "__mosAlert", j.p, j.len, JS_WK_JSON);
        sb_free(&j);
        return kind == JS_DLG_ALERT;
    }
    o->w = w;
    o->kind = kind;
    o->msg = dup_str(msg ? msg : "");
    o->def = def ? dup_str(def) : NULL;
    o->frame = dup_str(js_page_url(p));
    o->subframe = p != w->js;
    o->reply_ok = kind == JS_DLG_ALERT;
    o->saved_cur = g_cur;
    o->saved_paint = g_paint_doc;
    w->dlg = o;
    w->events |= MOS_WEB_EV_DIALOG;
    void *e = js_suspend_exec();
    cs_yield();
    /* retomado por MOS_WEB_DIALOG REPLY (ou cancelado na saída do processo) */
    js_resume_exec(e);
    g_cur = o->saved_cur;
    g_paint_doc = o->saved_paint;
    w->dlg = NULL;
    int ok = o->reply_ok;
    if (kind == JS_DLG_PROMPT && ok) sb_puts(out, o->reply ? o->reply : "");
    kfree(o->msg); kfree(o->def); kfree(o->frame); kfree(o->reply);
    o->msg = o->def = o->frame = o->reply = NULL;
    return ok;
}

/* responde e retoma; se a operação suspensa era um EVAL, o resultado fica para MOS_WEB_EVALDONE */
static void dialog_reply(webview_t *w, int ok, char *text) {
    wvop_t *o = w->dlg;
    if (!o) { kfree(text); return; }
    kfree(o->reply);
    o->reply = text;
    o->reply_ok = ok;
    if (op_run(o)) op_free(o);
}

/* processo saindo ou vista fechada com diálogo aberto: cancela até a operação terminar */
static void dialog_abort(webview_t *w) {
    for (int i = 0; i < 64 && w->dlg; i++) dialog_reply(w, 0, NULL);
}

static i64 web_dialog_op(process_t *p, webview_t *w, u64 a) {
    mos_web_dialog_t D;
    if (!uvm_copy_in(p, &D, a, sizeof D)) return -MOS_EFAULT;
    wvop_t *o = w->dlg;
    if (!o) return -MOS_ENOENT;
    if (D.op == MOS_WEB_DLG_GET) {
        D.kind = o->kind;
        D.has_default = o->def != NULL;
        D.subframe = o->subframe;
        const char *src[3] = {o->msg ? o->msg : "", o->def ? o->def : "", o->frame ? o->frame : ""};
        unsigned long long dst[3] = {D.msg, D.def, D.frame_url};
        unsigned cap[3] = {D.msg_cap, D.def_cap, D.frame_cap};
        unsigned *len[3] = {&D.msg_len, &D.def_len, &D.frame_len};
        for (int i = 0; i < 3; i++) {
            usize n = strlen(src[i]);
            *len[i] = (unsigned)n;
            if (dst[i] && cap[i]) {
                usize k = n < cap[i] - 1 ? n : cap[i] - 1;
                if (k && !uvm_copy_out(p, dst[i], src[i], k)) return -MOS_EFAULT;
                char z = 0;
                if (!uvm_copy_out(p, dst[i] + k, &z, 1)) return -MOS_EFAULT;
            }
        }
        return uvm_copy_out(p, a, &D, sizeof D) ? 0 : -MOS_EFAULT;
    }
    if (D.op == MOS_WEB_DLG_REPLY) {
        char *t = NULL;
        if (D.ok && D.text && D.text_len) {
            if (D.text_len > (1u << 20)) return -MOS_EINVAL;
            t = copy_in_str(p, D.text, D.text_len, 1u << 20);
            if (!t) return -MOS_EFAULT;
        }
        dialog_reply(w, D.ok ? 1 : 0, t);
        return 0;
    }
    return -MOS_EINVAL;
}

i64 web_user_syscall(process_t *p, u64 op, u64 id, u64 a, u64 b) {
    wvcall_t c = {p, op, id, a, b};
    if (op != MOS_WEB_NEW) {
        webview_t *w = wv_get(p, id);
        if (w && w->dlg) {
            /* vista esperando o app responder o diálogo */
            switch (op) {
            case MOS_WEB_DIALOG: return web_dialog_op(p, w, a);
            case MOS_WEB_FREE: dialog_abort(w); break;      /* cancela e segue para liberar */
            case MOS_WEB_PUMP: { u32 ev = w->events; w->events = 0; return ev; }
            case MOS_WEB_PAINT: case MOS_WEB_INFO: case MOS_WEB_TAKEMSG: case MOS_WEB_HISTORY: case MOS_WEB_TEXT:
            case MOS_WEB_RESULT: case MOS_WEB_EVALDONE: case MOS_WEB_HANDLER: break;
            default: return -MOS_EBUSY;
            }
        } else if (w && op == MOS_WEB_DIALOG) return -MOS_ENOENT;
    }
    wvop_t *o = kcalloc(1, sizeof(wvop_t));
    if (!o) return -MOS_ENOMEM;
    o->call = c;
    u8 *st = g_spare_stack;
    g_spare_stack = NULL;
    o->cs = cs_new_on(st, WV_STACK, op_entry, o);
    if (!o->cs) { kfree(st); kfree(o); return on_big_stack(wv_dispatch, &c); }
    if (op_run(o)) { i64 r = o->ret; op_free(o); return r; }
    /* parou num diálogo: a syscall volta agora, e o resto da operação roda quando o app responder */
    o->detached = true;
    if (op == MOS_WEB_PUMP && o->w) { u32 ev = o->w->events | MOS_WEB_EV_DIALOG; o->w->events = 0; return ev; }
    if (op == MOS_WEB_EVAL) return -MOS_EINPROGRESS;
    return 0;
}
