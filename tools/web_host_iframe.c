/* mOS — teste de host dos iframes (kernel/web/frame.c + JS): carga por src/srcdoc, CSS externo do filho, pintura,
 * parent/top/frameElement, contentWindow/contentDocument, postMessage nos dois sentidos, evento load, clique em link
 * dentro do iframe, rodinha e src trocado pelo JavaScript. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; surface_t *s; } pg_t;

static void tick(pg_t *g, int n) {
    for (int i = 0; i < n; i++) {
        if (g->js) js_page_tick(g->js);
        doc_pump_images(g->d);
        if (g->d->need_layout || g->d->need_style || !g->d->rootbox) doc_layout(g->d, g->s->w, g->s->h, 1.0f);
        frames_pump(g->d);
    }
    doc_layout(g->d, g->s->w, g->s->h, 1.0f);
    gfx_fill(g->s, 0, 0, g->s->w, g->s->h, RGB(255, 255, 255));
    ui_t u;
    memset(&u, 0, sizeof u);
    u.s = g->s;
    doc_paint(g->d, &u, rect_make(0, 0, g->s->w, g->s->h), 0, NULL);
}

static pg_t open_page(const char *html, int w, int h) {
    pg_t g = { 0 };
    g.d = doc_parse(html, strlen(html), "http://teste/iframe/pai.html");
    g.s = surface_new(w, h);
    g.js = js_page_new2(g.d, "http://teste/iframe/pai.html", w, h, NULL, NULL);
    frames_set_owner_js(g.d, g.js);
    doc_layout(g.d, w, h, 1.0f);
    tick(&g, 30);
    return g;
}
static void close_page(pg_t *g) { js_page_free(g->js); doc_free(g->d); surface_free(g->s); }

static const char *ev(pg_t *g, const char *code) {
    static sbuf_t out;
    if (out.p) sb_free(&out);
    memset(&out, 0, sizeof out);
    js_page_eval_value(g->js, code, (int)strlen(code), &out);
    return out.p ? out.p : "";
}
static u32 px(pg_t *g, int x, int y) { return g->s->px[(usize)y * g->s->stride + x] & 0xFFFFFF; }

WEB_HOST_CASE(iframe_src_pintura_js) {
    pg_t g = open_page("<html><body style='margin:0'>"
                       "<iframe id=f src='filho.html' width=100 height=60 style='border:0;display:block'></iframe>"
                       "<iframe id=s srcdoc=\"<body style='margin:0;background:#0000ff'><b id=x>oi</b><script>window.v=parent.document.title</script></body>\" "
                       "width=50 height=30 style='border:0;display:block'></iframe>"
                       "<script>var cargas=0; document.getElementById('f').onload=function(){cargas++};"
                       "document.title='Pai';"
                       "window.addEventListener('message', function(e){ window.resp = e.data.resposta; window.ori = e.origin; });</script>"
                       "</body></html>", 200, 200);
    /* pintura: fundo verde do filho (CSS dele), azul do srcdoc */
    CHECK(px(&g, 90, 50) == 0x00FF00, "fundo do iframe src: %06x", px(&g, 90, 50));
    CHECK(px(&g, 45, 75) == 0x0000FF, "fundo do srcdoc: %06x", px(&g, 45, 75));
    CHECK(px(&g, 150, 30) == 0xFFFFFF, "fora do iframe: %06x", px(&g, 150, 30));
    /* link azul do filho em (0..40, ~altura do texto) */
    const char *r = ev(&g, "document.getElementById('f').contentWindow.ehFilho");
    CHECK(!strcmp(r, "true"), "parent/top/frameElement no filho: %s", r);
    r = ev(&g, "document.getElementById('f').contentDocument.getElementById('t').textContent");
    CHECK(!strcmp(r, "filho"), "contentDocument (mesma origem): %s", r);
    r = ev(&g, "cargas");
    CHECK(!strcmp(r, "1"), "load do iframe no pai: %s", r);
    r = ev(&g, "document.getElementById('s').contentWindow.v");
    CHECK(!strcmp(r, "Pai"), "srcdoc lê o pai (origem herdada): %s", r);
    /* postMessage pai → filho → pai */
    ev(&g, "document.getElementById('f').contentWindow.postMessage({n: 21}, '*')");
    tick(&g, 5);
    r = ev(&g, "document.getElementById('f').contentDocument.getElementById('t').textContent");
    CHECK(!strcmp(r, "recebi:21:true"), "mensagem chegou ao filho: %s", r);
    r = ev(&g, "window.resp + ' ' + window.ori");
    CHECK(!strcmp(r, "42 http://teste"), "resposta do filho: %s", r);
    /* targetOrigin errado: descartada */
    ev(&g, "window.resp = 0; document.getElementById('f').contentWindow.postMessage({n: 1}, 'https://outra.example')");
    tick(&g, 5);
    r = ev(&g, "window.resp");
    CHECK(!strcmp(r, "0"), "targetOrigin diferente descarta: %s", r);
    close_page(&g);
}

WEB_HOST_CASE(iframe_clique_rodinha_src) {
    pg_t g = open_page("<html><body style='margin:0'><iframe id=f src='filho.html' width=100 height=60 style='border:0;display:block'></iframe></body></html>", 200, 200);
    int ox, oy;
    /* acha o link azul dentro do iframe */
    int ly = -1;
    for (int y = 0; y < 60 && ly < 0; y++) if (px(&g, 5, y) == 0x0000FF) ly = y;
    CHECK(ly >= 0, "link azul do filho não apareceu");
    frame_t *f = frame_at(g.d, 5, ly + 2, &ox, &oy);
    CHECK(f != NULL && ox == 0 && oy == 0, "frame_at");
    if (f && ly >= 0) {
        char out[256];
        CHECK(frame_input(f, FRAME_CLICK, 5, ly + 2, 0, out, sizeof out) && !out[0], "clique tratado no iframe");
        tick(&g, 20);
        CHECK(!strcmp(frame_url(f), "http://teste/iframe/outro.html"), "o link navegou o iframe: %s", frame_url(f));
        CHECK(px(&g, 90, 50) == 0xFF00FF, "pintou o documento novo: %06x", px(&g, 90, 50));
    }
    /* src trocado pelo JS */
    ev(&g, "document.getElementById('f').src = 'filho.html'");
    tick(&g, 20);
    CHECK(!strcmp(frame_url(f), "http://teste/iframe/filho.html"), "src pelo JS: %s", frame_url(f));
    /* rodinha: documento alto rola dentro do iframe */
    ev(&g, "document.getElementById('f').srcdoc = \"<body style='margin:0'><div style='height:500px;background:#ff0000'></div><div style='height:100px;background:#000000'></div></body>\"");
    tick(&g, 10);
    CHECK(px(&g, 50, 30) == 0xFF0000, "srcdoc alto: %06x", px(&g, 50, 30));
    CHECK(frame_input(f, FRAME_WHEEL, 10, 10, 480, NULL, 0), "rodinha rolou");
    tick(&g, 1);
    CHECK(px(&g, 50, 40) == 0x000000, "rolou para o fim: %06x", px(&g, 50, 40));
    CHECK(px(&g, 150, 40) == 0xFFFFFF, "pai não rolou: %06x", px(&g, 150, 40));
    close_page(&g);
}

WEB_HOST_CASE(iframe_aninhado_e_limites) {
    /* iframe dentro de iframe (srcdoc), e recursão infinita cortada pela profundidade */
    pg_t g = open_page("<html><body style='margin:0'><iframe width=120 height=80 style='border:0' srcdoc=\""
                       "<body style='margin:0'><iframe width=60 height=40 style='border:0' srcdoc='<body style=background:#123456></body>'></iframe></body>\"></iframe>"
                       "<iframe src='pai.html'></iframe></body></html>", 200, 200);
    CHECK(px(&g, 30, 20) == 0x123456, "neto pintado: %06x", px(&g, 30, 20));
    close_page(&g);
    CHECK(true, "sobreviveu");
}

WEB_HOST_CASE(iframe_formularios) {
    pg_t g = open_page("<html><body style='margin:0'><form><input name=a></form>"
                       "<iframe id=f src='form.html' width=200 height=100 style='border:0;display:block'></iframe></body></html>", 300, 200);
    int ox, oy;
    frame_t *f = NULL;
    for (int y = 0; y < 200 && !f; y += 5) f = frame_at(g.d, 10, y, &ox, &oy);
    CHECK(f != NULL, "iframe com formulário");
    if (!f) { close_page(&g); return; }
    doc_t *cd = frame_document(f);
    CHECK(cd && cd->nctrls >= 4 && g.d->nctrls >= 1, "controles: pai %d, filho %d", g.d->nctrls, cd ? cd->nctrls : -1);
    if (!cd || cd->nctrls < 4) { close_page(&g); return; }
    /* ids únicos entre o pai e o iframe; achar o controle do iframe pelo id */
    CHECK(g.d->ctrls[0]->id != cd->ctrls[0]->id, "ids iguais: %d", cd->ctrls[0]->id);
    doc_t *own = NULL;
    ctrl_t *c = frames_ctrl_by_id(g.d, (u32)cd->ctrls[0]->id, &own);
    CHECK(c == cd->ctrls[0] && own == cd, "frames_ctrl_by_id");
    CHECK(frames_page_of(g.d, cd) != NULL && frames_frame_of(g.d, cd) == f, "página e frame do documento do iframe");
    /* envio GET montado com os valores dos controles do iframe */
    node_t *form = cd->ctrls[0]->form;
    char url[1024];
    sbuf_t body = { 0 };
    bool post = form_build(cd, form, cd->ctrls[2], url, sizeof url, &body);
    CHECK(!post && !strcmp(url, "http://teste/iframe/recebe.html?q=ola+mundo&c=on"), "GET: %s", url);
    sb_free(&body);
    frame_load(f, url, NULL, 0, NULL);
    tick(&g, 20);
    CHECK(!strncmp(frame_url(f), "http://teste/iframe/recebe.html?q=", 34), "iframe navegou com o GET: %s", frame_url(f));
    CHECK(px(&g, 150, 50) == 0x00FFFF, "página recebida pintada no iframe: %06x", px(&g, 150, 50));
    /* POST: corpo codificado */
    frame_load(f, "form.html", NULL, 0, NULL);
    tick(&g, 20);
    cd = frame_document(f);
    node_t *fp = NULL;
    for (int i = 0; i < cd->nctrls; i++) if (cd->ctrls[i]->form && node_attr(cd->ctrls[i]->form, "method") && !strcmp(node_attr(cd->ctrls[i]->form, "method"), "post")) fp = cd->ctrls[i]->form;
    CHECK(fp != NULL, "form POST");
    if (fp) {
        sbuf_t b2 = { 0 };
        post = form_build(cd, fp, NULL, url, sizeof url, &b2);
        CHECK(post && !strcmp(url, "http://teste/iframe/recebe.html") && b2.len == 7 && !memcmp(b2.p, "u=x%26y", 7), "POST: %s [%.*s]", url, b2.len, b2.p ? b2.p : "");
        sb_free(&b2);
    }
    /* form.submit() pelo JS do filho navega o próprio iframe */
    sbuf_t o = { 0 };
    js_page_eval_value(frames_page_of(g.d, cd), "document.getElementById('fm').submit()", 38, &o);
    sb_free(&o);
    tick(&g, 20);
    CHECK(!strncmp(frame_url(f), "http://teste/iframe/recebe.html?q=ola+mundo", 42), "form.submit() no filho: %s", frame_url(f));
    close_page(&g);
}
