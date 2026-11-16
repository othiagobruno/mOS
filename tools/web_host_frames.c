/* mOS — teste de host dos iframes, parte 2 (kernel/web/frame.c + js_dom.c): window[i]/length, window.open (iframe com
 * nome e vista nova), WindowProxy entre origens, sandbox (scripts, origem opaca, formulários, popups), histórico do
 * iframe (pushState, voltar sem recarregar e entre documentos) e cursor sobre link do iframe. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; surface_t *s; } fg_t;

static void tick(fg_t *g, int n) {
    for (int i = 0; i < n; i++) {
        if (g->js) js_page_tick(g->js);
        doc_pump_images(g->d);
        if (g->d->need_layout || g->d->need_style || !g->d->rootbox) doc_layout(g->d, g->s->w, g->s->h, 1.0f);
        frames_pump(g->d);
    }
    doc_layout(g->d, g->s->w, g->s->h, 1.0f);
    ui_t u;
    memset(&u, 0, sizeof u);
    u.s = g->s;
    doc_paint(g->d, &u, rect_make(0, 0, g->s->w, g->s->h), 0, NULL);
}

static fg_t open_page(const char *html) {
    fg_t g = { 0 };
    g.d = doc_parse(html, strlen(html), "http://teste/iframe/pai.html");
    g.s = surface_new(300, 400);
    g.js = js_page_new2(g.d, "http://teste/iframe/pai.html", 300, 400, NULL, NULL);
    frames_set_owner_js(g.d, g.js);
    doc_layout(g.d, 300, 400, 1.0f);
    tick(&g, 30);
    return g;
}
static void close_page(fg_t *g) { js_page_free(g->js); doc_free(g->d); surface_free(g->s); }

static const char *run(fg_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[700]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)

WEB_HOST_CASE(frames_indice_e_open) {
    fg_t g = open_page("<html><body style='margin:0'>"
                       "<iframe id=a name=primeiro src='filho.html' width=100 height=60 style='border:0;display:block'></iframe>"
                       "<div><iframe id=b srcdoc='<p>b</p>' width=50 height=30></iframe></div>"
                       "<script>var r=[], a=document.getElementById('a'), b=document.getElementById('b');</script></body></html>");
    EQ("[window.length, frames.length, frames[0]===a.contentWindow, window[1]===b.contentWindow, frames[2]].join()", "2,2,true,true,");
    EQ("b.remove(); 0", "0");
    tick(&g, 3);
    EQ("[window.length, window[1]].join()", "1,");
    /* window.open com o nome de um iframe navega o iframe e devolve a janela dele */
    EQ("var w=window.open('outro.html','primeiro'); w===a.contentWindow", "true");
    tick(&g, 20);
    EQ("/outro\\.html$/.test(a.contentWindow.location.href)", "true");
    /* sem nome conhecido: vista nova pelo anfitrião */
    EQ("String(window.open('/x.html'))", "null");
    const char *pu = js_page_take_popup(g.js);
    CHECK(pu && !strcmp(pu, "http://teste/x.html"), "window.open pediu a vista nova: %s", pu ? pu : "(nada)");
    CHECK(!js_page_take_popup(g.js), "o pedido sai uma vez só");
    EQ("try{ window.open('http://[x') }catch(e){ e.name }", "SyntaxError");
    /* cursor de link sobre o iframe: o link azul do filho fica no topo do iframe */
    run(&g, "a.src='filho.html'");
    tick(&g, 20);
    CHECK(frame_cursor(g.d, 14, 36) == FRAME_CUR_HAND, "cursor sobre o link do iframe: %d", frame_cursor(g.d, 14, 36));
    CHECK(frame_cursor(g.d, 80, 50) == FRAME_CUR_ARROW, "cursor no fundo do iframe: %d", frame_cursor(g.d, 80, 50));
    CHECK(frame_cursor(g.d, 200, 300) == 0, "fora do iframe: %d", frame_cursor(g.d, 200, 300));
    close_page(&g);
}

WEB_HOST_CASE(frames_entre_origens) {
    fg_t g = open_page("<html><body>"
                       "<iframe id=x src=\"data:text/html,<script>onmessage=function(e){ e.source.postMessage([typeof parent.document, typeof parent.postMessage, e.origin, parent===top, String(frameElement)].join(), '*') }</script>\"></iframe>"
                       "<script>var r=[], x=document.getElementById('x'); onmessage=function(e){ r.push(e.data+'|'+e.origin+'|'+(e.source===x.contentWindow)) }</script></body></html>");
    EQ("var cw=x.contentWindow; [typeof cw, cw.document, typeof cw.postMessage, cw.length, cw.closed, cw.window===cw, cw.parent===window, x.contentDocument].join()",
       "object,,function,0,false,true,true,");
    EQ("try{ cw.location.href } catch(e){ e.name }", "SecurityError");
    run(&g, "cw.postMessage('oi','*')");
    tick(&g, 5);
    EQ("r.join()", "undefined,function,http://teste,true,null|null|true");
    close_page(&g);
}

WEB_HOST_CASE(frames_sandbox) {
    fg_t g = open_page("<html><body><script>var r=[]; onmessage=function(e){ r.push(e.data+'|'+e.origin) }</script>"
                       "<iframe id=s0 sandbox srcdoc='<script>parent.rodou0=1</script><b onclick=\"parent.rodou0=2\">x</b>'></iframe>"
                       "<iframe id=s1 sandbox='allow-scripts' srcdoc=\"<script>parent.postMessage([typeof parent.document, String(window.open(&quot;/p&quot;)), confirm(&quot;?&quot;)].join(),'*')</script>\"></iframe>"
                       "<iframe id=s2 sandbox='allow-scripts allow-same-origin allow-popups' srcdoc='<script>parent.rodou2=location.origin; window.open(\"/pop.html\")</script>'></iframe>"
                       "<iframe id=s3 sandbox='allow-scripts allow-same-origin' srcdoc='<form id=f action=\"outro.html\"></form><script>setTimeout(function(){ document.getElementById(\"f\").submit() },1)</script>'></iframe>"
                       "<iframe id=s4 sandbox='allow-scripts allow-same-origin allow-forms' srcdoc='<form id=f action=\"outro.html\"></form><script>setTimeout(function(){ document.getElementById(\"f\").submit() },1)</script>'></iframe>"
                       "<script>var s0=document.getElementById('s0'), s2=document.getElementById('s2'), s3=document.getElementById('s3'), s4=document.getElementById('s4');</script></body></html>");
    EQ("typeof rodou0+','+(s0.contentDocument!==null)", "undefined,false");
    EQ("r.join()", "undefined,null,false|null");
    EQ("rodou2+','+(s2.contentDocument.body!==null)", "http://teste,true");
    char pu[1024] = "";
    CHECK(frames_take_popup(g.d, pu, sizeof pu) && !strcmp(pu, "http://teste/pop.html"), "popup liberado por allow-popups: %s", pu);
    CHECK(!frames_take_popup(g.d, pu, sizeof pu), "o popup de s1 foi bloqueado");
    /* sem allow-forms: submit não navega; com allow-forms, navega */
    for (int i = 0; i < 15; i++) { tick(&g, 1); host_sleep_ms(2); }
    EQ("s3.contentWindow.location.href+','+/outro\\.html\\?$/.test(s4.contentWindow.location.href)", "about:srcdoc,true");
    /* o atributo trocado vale na próxima navegação do iframe */
    EQ("s2.setAttribute('sandbox','allow-same-origin'); s2.srcdoc='<b>sem scripts</b><script>parent.rodou3=1</script>'; 0", "0");
    tick(&g, 15);
    EQ("typeof rodou3", "undefined");
    close_page(&g);
}

WEB_HOST_CASE(frames_historico) {
    fg_t g = open_page("<html><body><iframe id=f src='filho.html'></iframe><script>var r=[], f=document.getElementById('f');</script></body></html>");
    run(&g, "var cw=f.contentWindow; cw.addEventListener('popstate', function(e){ r.push('pop:'+JSON.stringify(e.state)+':'+cw.location.search) });"
            "cw.history.pushState({a:1},'','?p=1'); cw.history.pushState({a:2},'','?p=2');");
    tick(&g, 3);
    EQ("cw.history.length+','+cw.location.search+','+history.length", "3,?p=2,1");
    run(&g, "cw.history.go(-2)");
    tick(&g, 3);
    EQ("r.join()+','+cw.location.search+','+(f.contentWindow===cw)", "pop:null:,,true");
    run(&g, "cw.history.forward()");
    tick(&g, 3);
    EQ("r.join('|')+','+cw.history.state.a", "pop:null:|pop:{\"a\":1}:?p=1,1");
    /* outro documento descarta as entradas à frente; voltar recarrega o documento anterior */
    run(&g, "cw.location.href='outro.html'");
    tick(&g, 20);
    EQ("var c2=f.contentWindow; [c2!==cw, /outro\\.html$/.test(c2.location.href), c2.history.length].join()", "true,true,3");
    run(&g, "c2.history.back()");
    tick(&g, 20);
    EQ("f.contentWindow.location.search+','+f.contentWindow.history.length+','+(f.contentWindow!==c2)", "?p=1,3,true");
    close_page(&g);
}

/* gancho do anfitrião para as páginas de iframe (WKUserScript forMainFrameOnly:NO, manipuladores) e quem mandou cada
 * mensagem (WKFrameInfo.isMainFrame / request) */
static js_page_t *g_top;
static int g_fases;
static char g_msgs[512];
static void fi_post(void *ctx, const char *name, const char *body, int len, int type) {
    (void)ctx; (void)type;
    js_page_t *from = js_cur_page();
    usize n = strlen(g_msgs);
    snprintf(g_msgs + n, sizeof g_msgs - n, "%s%s:%.*s:%s", n ? "|" : "", name, len, body, from == g_top ? "principal" : js_page_url(from));
}
static void fi_init(js_page_t *p, int phase, void *ctx) {
    (void)ctx;
    g_fases |= 1 << phase;
    if (phase == 0) {
        js_page_wk_bridge(p, fi_post, NULL);
        js_page_wk_handler(p, "canal", true);
        js_page_eval(p, "window.injetado = 'sim'", -1, "WKUserScript");
    }
}

WEB_HOST_CASE(frames_quadros_do_anfitriao) {
    fg_t g = { 0 };
    const char *html = "<html><body><iframe id=f srcdoc=\"<script>webkit.messageHandlers.canal.postMessage('de '+window.injetado)</script>\"></iframe>"
                       "<script>webkit.messageHandlers.canal.postMessage('topo')</script></body></html>";
    g_msgs[0] = 0;
    g_fases = 0;
    g.d = doc_parse(html, strlen(html), "http://teste/iframe/pai.html");
    g.s = surface_new(300, 400);
    g.js = js_page_new2(g.d, "http://teste/iframe/pai.html", 300, 400, NULL, NULL);
    g_top = g.js;
    js_page_wk_bridge(g.js, fi_post, NULL);
    js_page_wk_handler(g.js, "canal", true);
    js_page_set_frame_init(g.js, fi_init, NULL);
    run(&g, "webkit.messageHandlers.canal.postMessage('topo')");
    frames_set_owner_js(g.d, g.js);
    tick(&g, 20);
    CHECK(g_fases == 3, "fases do gancho no iframe: %d", g_fases);
    CHECK(!strcmp(g_msgs, "canal:topo:principal|canal:de sim:about:srcdoc"), "mensagens: %s", g_msgs);
    close_page(&g);
}
