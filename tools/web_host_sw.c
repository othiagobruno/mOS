/* mOS — teste de host dos Service Workers e da Cache API (kernel/web/web_sw.c + web_sw.js): registro, install com
 * cache.addAll, skipWaiting, clients.claim, fetch interceptado (respondWith com Response, com o cache, rejeitado e sem
 * resposta indo para a rede), página aberta depois já controlada, mensagens nos dois sentidos, CacheStorage na janela. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } sg_t;
static sg_t sg_open(const char *url) {
    sg_t g;
    const char *html = "<html><body><script>var r=[];</script></body></html>";
    g.d = doc_parse(html, strlen(html), url);
    g.js = js_page_new2(g.d, url, 300, 200, NULL, NULL);
    for (int i = 0; i < 3; i++) js_page_tick(g.js);
    return g;
}
static void sg_close(sg_t *g) { js_page_free(g->js); doc_free(g->d); }
static const char *run(sg_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[700]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
static void spin(sg_t *g, int n) { for (int i = 0; i < n; i++) js_page_tick(g->js); }
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)

WEB_HOST_CASE(sw_ciclo_e_fetch) {
    sg_t g = sg_open("http://teste/sw/a.html");
    EQ("typeof navigator.serviceWorker+','+(navigator.serviceWorker instanceof ServiceWorkerContainer)+','+navigator.serviceWorker.controller", "object,true,null");
    run(&g, "navigator.serviceWorker.register('sw.js').then(function(reg){ r.push('reg:'+reg.scope+':'+(reg.installing?reg.installing.state:'-')) });"
            "navigator.serviceWorker.ready.then(function(reg){ r.push('ready:'+reg.active.state) });"
            "navigator.serviceWorker.oncontrollerchange=function(){ r.push('controller') };");
    spin(&g, 30);
    EQ("r.join('|')", "reg:http://teste/sw/:installing|ready:activated|controller");
    EQ("navigator.serviceWorker.controller.scriptURL", "http://teste/sw/sw.js");
    /* fetch passando pelo worker */
    run(&g, "r=[]; Promise.all([fetch('virtual',{method:'POST',body:'x'}).then(function(x){ return x.text().then(function(t){ return t+':'+x.headers.get('x-sw') }) }),"
            " fetch('dado.txt').then(function(x){ return x.text() }), fetch('rede.txt').then(function(x){ return x.text() }),"
            " fetch('quebrado').then(function(){ return 'passou' }, function(e){ return e.name })]).then(function(v){ r.push(v.join('|')) })");
    spin(&g, 30);
    EQ("r.join('|')", "do worker:POST:1|conteúdo guardado|da rede|TypeError");
    /* Cache API na janela (mesmo armazenamento da origem) */
    run(&g, "r=[]; caches.keys().then(function(k){ r.push(k.join()); return caches.match('dado.txt') }).then(function(x){ return x.text() }).then(function(t){ r.push(t);"
            " return caches.open('outro') }).then(function(c){ return c.put('http://teste/sw/novo', new Response('novo corpo',{status:201})).then(function(){ return c.match('novo') }) })"
            ".then(function(x){ return x.text().then(function(t){ r.push(x.status+':'+t) }) }).then(function(){ return caches.delete('outro') }).then(function(d){ r.push('apagado:'+d); return caches.has('outro') })"
            ".then(function(h){ r.push('existe:'+h) })");
    spin(&g, 20);
    EQ("r.join('|')", "v1|conteúdo guardado|201:novo corpo|apagado:true|existe:false");
    /* mensagens: página → worker → página */
    run(&g, "r=[]; navigator.serviceWorker.onmessage=function(e){ r.push(e.data) }; navigator.serviceWorker.controller.postMessage('oi');");
    spin(&g, 20);
    EQ("r.join('|')", "eco:oi:http://teste/sw/");
    /* página aberta depois: controlada desde o primeiro script */
    sg_t b = sg_open("http://teste/sw/b.html");
    run(&b, "fetch('virtual').then(function(x){ return x.text() }).then(function(t){ r.push(t) })");
    for (int i = 0; i < 20; i++) { spin(&g, 1); spin(&b, 1); }
    { sg_t g = b; EQ("r.join('|')+'|'+(navigator.serviceWorker.controller!==null)", "do worker:GET|true"); }
    /* fora do escopo: sem controle */
    sg_t c = sg_open("http://teste/outra.html");
    { sg_t g = c; EQ("String(navigator.serviceWorker.controller)", "null"); }
    /* escopo além da pasta do script é recusado */
    run(&g, "r=[]; navigator.serviceWorker.register('sw.js',{scope:'/'}).catch(function(e){ r.push(e.name) })");
    spin(&g, 5);
    EQ("r.join('|')", "SecurityError");
    sg_close(&c);
    sg_close(&b);
    sg_close(&g);
}
