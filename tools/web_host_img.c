/* mOS — teste de host do HTMLImageElement no JS (kernel/web/js_dom.c + layout.c): new Image(), imagem pedida fora
 * do documento, eventos load/error (propriedade e atributo), complete, naturalWidth/Height, width/height, currentSrc,
 * srcset, decode(), src vazio, o load da janela esperando as imagens e drawImage de uma imagem carregada pelo JS. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } ig_t;

static void tick(ig_t *g, int n) {
    for (int i = 0; i < n; i++) { doc_pump_images(g->d); js_page_tick(g->js); }
}
static ig_t ig_open(const char *html) {
    ig_t g;
    g.d = doc_parse(html, strlen(html), "http://teste/img.html");
    g.js = js_page_new2(g.d, "http://teste/img.html", 400, 300, NULL, NULL);
    tick(&g, 10);
    doc_layout(g.d, 400, 300, 1.0f);
    tick(&g, 10);
    return g;
}
static void ig_close(ig_t *g) { js_page_free(g->js); doc_free(g->d); }

static const char *run(ig_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[600]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)

#define SVG10x5 "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' width='10' height='5'><rect width='10' height='5' fill='red'/></svg>"

WEB_HOST_CASE(img_js_new_image_eventos) {
    ig_t g = ig_open("<html><body><script>var ev=[];</script></body></html>");
    EQ("var im=new Image(); [im instanceof HTMLImageElement, Image.prototype===HTMLImageElement.prototype, "
       "im.constructor===HTMLImageElement, im.tagName, im.complete, im.naturalWidth].join()", "true,true,true,IMG,true,0");
    EQ("im.onload=function(){ev.push('load:'+im.naturalWidth+'x'+im.naturalHeight+':'+im.complete)};"
       "im.onerror=function(){ev.push('erro')}; im.src=\"" SVG10x5 "\"; im.complete+','+ev.length", "false,0");
    tick(&g, 5);
    EQ("ev.join()+'|'+im.width+'x'+im.height", "load:10x5:true|10x5");
    EQ("im.currentSrc.slice(0,19)", "data:image/svg+xml,");
    /* imagem quebrada: error, complete, natural 0 */
    EQ("var im2=new Image(); im2.addEventListener('error', function(e){ev.push('erro2:'+e.type+':'+im2.complete+':'+im2.naturalWidth)});"
       "im2.src='data:image/png;base64,AAAA'; 'ok'", "ok");
    tick(&g, 5);
    EQ("ev[1]", "erro2:error:true:0");
    /* src vazio também dispara error, sem pedido */
    EQ("var im3=new Image(); im3.onerror=function(){ev.push('vazio')}; im3.src=''; 'ok'", "ok");
    tick(&g, 3);
    EQ("ev[2]+','+im3.currentSrc.length", "vazio,0");
    /* trocar o src dispara de novo; a mesma URL em outro elemento também recebe o seu load */
    EQ("var n_=0; var a=new Image(); a.onload=function(){n_++}; a.src=im.src; var b=new Image(); b.onload=function(){n_++}; b.src=im.src; 'ok'", "ok");
    tick(&g, 3);
    EQ("n_", "2");
    /* tamanho pelos atributos do construtor e pelas propriedades */
    EQ("var c=new Image(20,30); c.width+'x'+c.height+' '+c.getAttribute('width')", "20x30 20");
    EQ("c.width=7; c.getAttribute('width')+','+c.width", "7,7");
    EQ("c.srcset='a.png 1x'; c.srcset+'|'+c.getAttribute('srcset')+'|'+c.crossOrigin", "a.png 1x|a.png 1x|null");
    ig_close(&g);
}

WEB_HOST_CASE(img_js_decode) {
    ig_t g = ig_open("<html><body><script>var r=[];</script></body></html>");
    run(&g, "var im=new Image(); im.src=\"" SVG10x5 "\"; im.decode().then(function(){r.push('ok:'+im.naturalWidth)});"
            "var q=new Image(); q.src='data:image/png;base64,AAAA'; q.decode().then(function(){r.push('não')}, function(e){r.push(e.name+':'+(e instanceof DOMException))});");
    EQ("r.length", "0");
    tick(&g, 5);
    EQ("r.sort().join()", "EncodingError:true,ok:10");
    ig_close(&g);
}

WEB_HOST_CASE(img_js_load_da_janela) {
    ig_t g = ig_open("<html><body><img id=a onload=\"ordem.push('img')\" src=\"" SVG10x5 "\">"
                     "<script>var ordem=[]; addEventListener('load', function(){ var a=document.getElementById('a');"
                     "ordem.push('janela:'+a.complete+':'+a.naturalWidth) });</script></body></html>");
    EQ("ordem.join()", "img,janela:true:10");
    /* no layout, width é o tamanho exibido */
    EQ("document.getElementById('a').width+','+document.getElementById('a').naturalHeight", "10,5");
    ig_close(&g);
}

WEB_HOST_CASE(img_js_drawimage_carregada) {
    ig_t g = ig_open("<html><body><canvas id=c width=20 height=10></canvas><script>var ok_=0;"
                     "var src=document.createElement('canvas'); src.width=4; src.height=2; var s=src.getContext('2d');"
                     "s.fillStyle='#00f'; s.fillRect(0,0,4,2); var url=src.toDataURL();"
                     "var im=new Image(); im.onload=function(){ var c=document.getElementById('c').getContext('2d');"
                     "c.drawImage(im,0,0,20,10); ok_=1 }; im.src=url;</script></body></html>");
    tick(&g, 5);
    EQ("ok_+' '+Array.from(document.getElementById('c').getContext('2d').getImageData(10,5,1,1).data).join()", "1 0,0,255,255");
    /* imagem ainda não carregada: drawImage não desenha nem lança */
    EQ("var p=new Image(); p.src='http://exemplo.invalido/x.png'; document.getElementById('c').getContext('2d').drawImage(p,0,0); 'ok'", "ok");
    ig_close(&g);
}
