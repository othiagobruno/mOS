/* mOS — teste de host dos Web Workers e do BroadcastChannel (kernel/web/web_worker.c): worker de blob:, escopo sem DOM,
 * mensagens com clone estruturado nos dois sentidos, temporizadores, importScripts, close/terminate, erros, worker de
 * módulo e canais entre página e worker. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } wg_t;
static void tick(wg_t *g, int n) { for (int i = 0; i < n; i++) js_page_tick(g->js); }
static wg_t wg_open(void) {
    wg_t g;
    const char *html = "<html><body><script>var r=[]; function blobw(code,opt){ return new Worker(URL.createObjectURL(new Blob([code],{type:'text/javascript'})),opt) }</script></body></html>";
    g.d = doc_parse(html, strlen(html), "http://teste/w.html");
    g.js = js_page_new2(g.d, "http://teste/w.html", 300, 200, NULL, NULL);
    tick(&g, 3);
    return g;
}
static void wg_close(wg_t *g) { js_page_free(g->js); doc_free(g->d); }
static const char *run(wg_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[700]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)

WEB_HOST_CASE(worker_basico) {
    wg_t g = wg_open();
    run(&g, "var w=blobw(\"onmessage=function(e){ postMessage({eco:e.data.x*2, doc:typeof document, win:typeof window, escopo:self instanceof WorkerGlobalScope,"
            " nome:name, prot:location.protocol, m:e.data.m instanceof Map, cripto:typeof crypto.subtle}) }\",{name:'n1'});"
            "w.onmessage=function(e){ var d=e.data; r.push([d.eco,d.doc,d.win,d.escopo,d.nome,d.prot,d.m,d.cripto,e.target===w].join()) };"
            "w.postMessage({x:21, m:new Map()}); r.push(w instanceof Worker);");
    tick(&g, 4);
    EQ("r.join('|')", "true|42,undefined,undefined,true,n1,blob:,true,object,true");
    /* temporizador e importScripts dentro do worker; addEventListener na dona */
    run(&g, "r=[]; var w2=blobw(\"importScripts('data:text/javascript,function dobro(x){return x*2}'); setTimeout(function(){ postMessage('tempo:'+dobro(5)) },1);"
            " self.addEventListener('message', function(e){ postMessage('ouvinte:'+e.data) })\");"
            "w2.addEventListener('message', function(e){ r.push(e.data) }); w2.postMessage('oi');");
    for (int i = 0; i < 20; i++) { tick(&g, 1); host_sleep_ms(2); }
    EQ("r.sort().join('|')", "ouvinte:oi|tempo:10");
    wg_close(&g);
}

WEB_HOST_CASE(worker_fim_e_erros) {
    wg_t g = wg_open();
    run(&g, "var w=blobw(\"onmessage=function(e){ postMessage('a:'+e.data); if (e.data==2) close() }\"); w.onmessage=function(e){ r.push(e.data) };"
            "w.postMessage(1); w.postMessage(2);");
    tick(&g, 4);
    run(&g, "w.postMessage(3);");
    tick(&g, 4);
    EQ("r.join('|')", "a:1|a:2");
    run(&g, "r=[]; var t=blobw(\"onmessage=function(e){ postMessage('vivo') }\"); t.onmessage=function(e){ r.push(e.data) }; t.terminate(); t.postMessage(1);");
    tick(&g, 4);
    EQ("r.length", "0");
    run(&g, "r=[]; var e1=blobw(\"throw new Error('quebrou no worker')\"); e1.onerror=function(ev){ r.push('erro:'+/quebrou no worker/.test(ev.message)) };"
            "var e2=new Worker('/nao-existe.js'); e2.addEventListener('error', function(){ r.push('404') });");
    tick(&g, 4);
    EQ("r.sort().join('|')", "404|erro:true");
    EQ("try{new Worker('http://outro.site/x.js')}catch(e){e.name}", "SecurityError");
    EQ("try{new Worker('a.js',{type:'nada'})}catch(e){e instanceof TypeError}", "true");
    wg_close(&g);
}

WEB_HOST_CASE(worker_modulo_e_canal) {
    wg_t g = wg_open();
    run(&g, "var m=blobw(\"postMessage('meta:'+typeof import.meta+':'+(typeof importScripts)); export {};\",{type:'module'}); m.onmessage=function(e){ r.push(e.data) };");
    tick(&g, 5);
    EQ("r.join('|')", "meta:object:function");
    run(&g, "r=[]; var a=new BroadcastChannel('c'), b=new BroadcastChannel('c'), z=new BroadcastChannel('outro');"
            "a.onmessage=function(e){ r.push('a:'+e.data) }; b.onmessage=function(e){ r.push('b:'+e.data.v+':'+e.origin) }; z.onmessage=function(){ r.push('z') };"
            "var wb=blobw(\"var c=new BroadcastChannel('c'); c.onmessage=function(e){ postMessage('worker:'+e.data.v) }; postMessage('pronto')\");"
            "wb.onmessage=function(e){ r.push(e.data); if (e.data=='pronto') a.postMessage({v:7}) };");
    tick(&g, 8);
    EQ("r.join('|')", "pronto|b:7:http://teste|worker:7");
    EQ("a.close(); try{a.postMessage(1)}catch(e){e.name}", "InvalidStateError");
    EQ("b.name", "c");
    wg_close(&g);
}

/* SharedWorker: duas páginas da mesma origem ligadas ao mesmo worker (estado compartilhado), outro nome = outro
 * worker, portas entre reinos e erro de carga */
WEB_HOST_CASE(worker_compartilhado) {
    wg_t a = wg_open(), b = wg_open();
    const char *src = "var SRC='data:text/javascript,var n=0; onconnect=function(e){ var p=e.ports[0]; p.onmessage=function(m){ n++; p.postMessage(m.data+\":\"+n+\":\"+name+\":\"+(e.source===p)) } }';";
    run(&a, src);
    run(&b, src);
    run(&a, "var sw=new SharedWorker(SRC,'cont'); sw.port.onmessage=function(e){ r.push(e.data) }; sw.port.postMessage('a');"
            "r.push((sw instanceof SharedWorker)+':'+(sw.port instanceof MessagePort))");
    for (int i = 0; i < 6; i++) { tick(&a, 1); tick(&b, 1); }
    run(&b, "var sw=new SharedWorker(SRC,{name:'cont'}); sw.port.onmessage=function(e){ r.push(e.data) }; sw.port.postMessage('b');"
            "var outro=new SharedWorker(SRC,'outro'); outro.port.onmessage=function(e){ r.push(e.data) }; outro.port.postMessage('c');");
    for (int i = 0; i < 8; i++) { tick(&a, 1); tick(&b, 1); }
    { wg_t g = a; EQ("r.join('|')", "true:true|a:1:cont:true"); }
    { wg_t g = b; EQ("r.sort().join('|')", "b:2:cont:true|c:1:outro:true"); }
    /* erro de carga vai para o SharedWorker */
    run(&b, "r=[]; var ruim=new SharedWorker('/nao-existe.js'); ruim.onerror=function(){ r.push('erro') };");
    for (int i = 0; i < 3; i++) tick(&b, 1);
    { wg_t g = b; EQ("r.join('|')", "erro"); EQ("try{ SharedWorker('x') }catch(e){ e instanceof TypeError }", "true"); }
    wg_close(&a);
    /* a primeira página fechou: a segunda continua falando com o mesmo worker */
    run(&b, "r=[]; sw.port.postMessage('d');");
    for (int i = 0; i < 6; i++) tick(&b, 1);
    { wg_t g = b; EQ("r.join('|')", "d:3:cont:true"); }
    wg_close(&b);
}
