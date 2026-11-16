/* mOS — teste de host da History API (kernel/web/js_dom.c): pushState/replaceState com clone estruturado do estado,
 * length, back/forward/go com popstate, origem obrigatória (SecurityError), navegação por fragmento sem recarregar
 * (hashchange, location.hash/assign/replace) e o modo anfitrião, em que as operações vão para o histórico do WebView. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } hg_t;
static void tick(hg_t *g, int n) { for (int i = 0; i < n; i++) js_page_tick(g->js); }
static hg_t hg_open(void) {
    hg_t g;
    const char *html = "<html><body><script>var r=[]; addEventListener('popstate', function(e){ r.push('pop:'+JSON.stringify(e.state)+':'+location.pathname+location.hash) });"
                       "addEventListener('hashchange', function(e){ r.push('hash:'+e.oldURL.split('/').pop()+'>'+e.newURL.split('/').pop()) });</script>"
                       "<div style='height:2000px'></div><p id=alvo>alvo</p><a name=nomeada>n</a></body></html>";
    g.d = doc_parse(html, strlen(html), "http://teste/h/a.html");
    g.js = js_page_new2(g.d, "http://teste/h/a.html", 300, 200, NULL, NULL);
    doc_layout(g.d, 300, 200, 1.0f);
    tick(&g, 3);
    return g;
}
static void hg_close(hg_t *g) { js_page_free(g->js); doc_free(g->d); }
static const char *run(hg_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[700]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)

WEB_HOST_CASE(history_estado) {
    hg_t g = hg_open();
    EQ("history.length+','+history.state+','+(history instanceof History)+','+Object.prototype.toString.call(history)", "1,null,true,[object History]");
    EQ("var st={n:1, m:new Map([['k',2]])}; history.pushState(st,'','b.html?q=1'); st.n=9; "
       "[history.length, history.state.n, history.state.m.get('k'), location.href, history.state===history.state].join()",
       "2,1,2,http://teste/h/b.html?q=1,false");
    EQ("history.pushState({n:2},'','/h/c.html'); history.replaceState({n:3},''); [history.length, history.state.n, location.pathname].join()", "3,3,/h/c.html");
    CHECK(!js_page_take_nav(g.js, &(bool){ 0 }), "pushState não pode navegar");
    run(&g, "r=[]; history.back();");
    tick(&g, 2);
    EQ("r.join('|')+'|'+history.state.n+'|'+location.pathname", "pop:{\"n\":1,\"m\":{}}:/h/b.html|1|/h/b.html");
    run(&g, "r=[]; history.go(-1);");
    tick(&g, 2);
    EQ("r.join('|')+'|'+history.state+'|'+location.href", "pop:null:/h/a.html|null|http://teste/h/a.html");
    run(&g, "r=[]; history.go(2); history.forward();");
    tick(&g, 2);
    EQ("r.join('|')+'|'+history.length", "pop:{\"n\":3}:/h/c.html|3");
    /* pushState depois de voltar descarta as entradas à frente */
    run(&g, "history.back(); history.pushState(null,'','d.html');");
    tick(&g, 2);
    EQ("history.length+','+location.pathname", "3,/h/d.html");
    EQ("try{ history.pushState(null,'','http://outro/x') }catch(e){ e.name+':'+(e instanceof DOMException) }", "SecurityError:true");
    EQ("try{ history.pushState(function(){},'') }catch(e){ e.name }", "DataCloneError");
    EQ("try{ history.pushState({}) }catch(e){ e instanceof TypeError }", "true");
    EQ("history.scrollRestoration='manual'; var a=history.scrollRestoration; history.scrollRestoration='xx'; a+','+history.scrollRestoration", "manual,manual");
    hg_close(&g);
}

WEB_HOST_CASE(history_fragmento) {
    hg_t g = hg_open();
    run(&g, "r=[]; location.hash='alvo';");
    CHECK(!js_page_take_nav(g.js, &(bool){ 0 }), "mudar só o fragmento não recarrega");
    CHECK(js_page_take_scroll(g.js) > 1000, "rola até a âncora #alvo");
    tick(&g, 2);
    EQ("r.join('|')+'|'+location.hash+'|'+history.length", "pop:null:/h/a.html#alvo|hash:a.html>a.html#alvo|#alvo|2");
    run(&g, "r=[]; location.assign('#nomeada');");
    CHECK(js_page_take_scroll(g.js) > 1000, "rola até <a name=nomeada>");
    run(&g, "location.replace('#x');");
    tick(&g, 2);
    EQ("r.length+'|'+history.length+'|'+location.hash", "4|3|#x");
    run(&g, "r=[]; history.back();");
    tick(&g, 2);
    EQ("r.join('|')", "pop:null:/h/a.html#alvo|hash:a.html#x>a.html#alvo");
    /* outro documento: navega de verdade */
    run(&g, "location.href='outra.html';");
    bool rl = false;
    const char *nu = js_page_take_nav(g.js, &rl);
    CHECK(nu && !strcmp(nu, "http://teste/h/outra.html"), "navegação para outro documento [%s]", nu ? nu : "(nada)");
    hg_close(&g);
}

WEB_HOST_CASE(history_anfitriao) {
    hg_t g = hg_open();
    js_page_set_history_host(g.js, true);
    js_page_set_hist_len(g.js, 4);
    EQ("history.length", "4");
    run(&g, "history.pushState({v:1},'','p1'); history.replaceState({v:2},'','p2'); history.go(-2); location.hash='f';");
    int op, arg;
    char url[1024];
    CHECK(js_page_take_hist_op(g.js, &op, &arg, url, sizeof url) && op == 1 && arg == 0 && !strcmp(url, "http://teste/h/p1"), "push [%d %d %s]", op, arg, url);
    CHECK(js_page_take_hist_op(g.js, &op, &arg, url, sizeof url) && op == 2 && arg == 1 && !strcmp(url, "http://teste/h/p2"), "replace [%d %d %s]", op, arg, url);
    CHECK(js_page_take_hist_op(g.js, &op, &arg, url, sizeof url) && op == 3 && arg == -2, "go [%d %d]", op, arg);
    CHECK(js_page_take_hist_op(g.js, &op, &arg, url, sizeof url) && op == 1 && arg == -1 && !strcmp(url, "http://teste/h/p2#f"), "fragmento [%d %d %s]", op, arg, url);
    CHECK(!js_page_take_hist_op(g.js, &op, &arg, url, sizeof url), "fila vazia");
    /* o anfitrião volta a uma entrada do mesmo documento */
    tick(&g, 2);
    run(&g, "r=[];");
    js_page_hist_restore(g.js, "http://teste/h/p1", 0);
    tick(&g, 2);
    EQ("r.join('|')+'|'+history.state.v", "pop:{\"v\":1}:/h/p1|1");
    hg_close(&g);
}
