/* mOS — teste de host dos observadores (kernel/web/web_obs.c): MutationObserver (childList com lote de innerHTML e
 * textContent, attributes com oldValue e filtro, characterData, subtree, entrega em microtarefa, takeRecords,
 * disconnect), ResizeObserver (content-box, border-box, só quando muda) e IntersectionObserver (viewport, rolagem,
 * limiares, root elemento, rootMargin). */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } og_t;
static void tick(og_t *g, int n) { for (int i = 0; i < n; i++) js_page_tick(g->js); }
static og_t og_open(const char *html) {
    og_t g;
    g.d = doc_parse(html, strlen(html), "http://teste/o.html");
    g.js = js_page_new2(g.d, "http://teste/o.html", 400, 300, NULL, NULL);
    doc_layout(g.d, 400, 300, 1.0f);
    tick(&g, 3);
    return g;
}
static void og_close(og_t *g) { js_page_free(g->js); doc_free(g->d); }
static const char *run(og_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[700]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)

WEB_HOST_CASE(obs_mutation) {
    og_t g = og_open("<html><body><div id=a class=x><span id=s>t</span></div><script>var r=[];"
                     "function fmt(rs){return rs.map(function(m){return m.type+':'+(m.target.id||m.target.nodeName)+':+'+m.addedNodes.length+'-'+m.removedNodes.length+"
                     "(m.attributeName?':'+m.attributeName:'')+(m.oldValue!==null?':'+m.oldValue:'')}).join(',')}"
                     "var a=document.getElementById('a');</script></body></html>");
    run(&g, "var mo=new MutationObserver(function(rs,o){r.push(fmt(rs)+(o===mo?'':'!'))}); mo.observe(a,{childList:true});"
            "a.appendChild(document.createElement('b')); a.appendChild(document.createElement('i')); r.push('sinc');"
            "Promise.resolve().then(function(){r.push('depois')});");
    tick(&g, 1);
    EQ("r.join('|')", "sinc|childList:a:+1-0,childList:a:+1-0|depois");
    run(&g, "r=[]; a.innerHTML='<p>1</p><p>2</p>'; a.textContent='só texto';");
    tick(&g, 1);
    EQ("r.join('|')", "childList:a:+2-3,childList:a:+1-2");
    run(&g, "r=[]; mo.disconnect(); a.appendChild(document.createElement('u')); mo.observe(a,{attributes:true,attributeOldValue:true,attributeFilter:['class','data-z']});"
            "a.setAttribute('class','y'); a.setAttribute('title','sem filtro'); a.removeAttribute('class'); a.dataset.z='1';");
    tick(&g, 1);
    EQ("r.join('|')", "attributes:a:+0-0:class:x,attributes:a:+0-0:class:y,attributes:a:+0-0:data-z");
    run(&g, "r=[]; var t=a.firstChild; mo.observe(document.body,{characterData:true,characterDataOldValue:true,subtree:true,childList:true});"
            "t.data='novo'; document.getElementById('a').appendChild(document.createTextNode('z')); var tk=mo.takeRecords(); r.push('tomados:'+fmt(tk));");
    tick(&g, 1);
    EQ("r.join('|')", "tomados:characterData:#text:+0-0:só texto,childList:a:+1-0");
    EQ("try{new MutationObserver(function(){}).observe(a,{})}catch(e){e instanceof TypeError}", "true");
    EQ("try{new MutationObserver(function(){}).observe({},{childList:true})}catch(e){e instanceof TypeError}", "true");
    EQ("var m2=new MutationObserver(function(){}); m2.observe(a,{attributeOldValue:true}); 'ok'", "ok");
    og_close(&g);
}

WEB_HOST_CASE(obs_resize) {
    og_t g = og_open("<html><body style='margin:0'><div id=a style='width:100px;height:40px;padding:5px;border:2px solid red'></div>"
                     "<script>var r=[]; var a=document.getElementById('a');</script></body></html>");
    run(&g, "var ro=new ResizeObserver(function(es,o){es.forEach(function(e){r.push(e.contentRect.width+'x'+e.contentRect.height+'@'+e.contentRect.x+','+e.contentRect.y+"
            "' borda '+e.borderBoxSize[0].inlineSize+'x'+e.borderBoxSize[0].blockSize+' '+(e.target===a))})}); ro.observe(a);"
            "var rb=new ResizeObserver(function(es){r.push('bb:'+es[0].contentRect.width)}); rb.observe(a,{box:'border-box'});");
    tick(&g, 2);
    EQ("r.join('|')", "100x40@5,5 borda 114x54 true|bb:100");
    run(&g, "r=[]; a.style.width='150px';");
    tick(&g, 2);
    EQ("r.join('|')", "150x40@5,5 borda 164x54 true|bb:150");
    run(&g, "r=[]; ro.unobserve(a); a.style.height='60px';");
    tick(&g, 2);
    EQ("r.join('|')", "bb:150");
    EQ("try{ro.observe(a,{box:'x'})}catch(e){e instanceof TypeError}", "true");
    og_close(&g);
}

WEB_HOST_CASE(obs_intersection) {
    og_t g = og_open("<html><body style='margin:0'><div id=a style='height:100px'></div><div style='height:900px'></div><div id=b style='height:100px'></div>"
                     "<div id=box style='height:50px'><div id=c style='height:20px'></div></div>"
                     "<script>var r=[]; var a=document.getElementById('a'), b=document.getElementById('b');</script></body></html>");
    run(&g, "var io=new IntersectionObserver(function(es){es.forEach(function(e){r.push(e.target.id+':'+e.isIntersecting+':'+e.intersectionRatio.toFixed(2)+':'+e.boundingClientRect.top)})},{threshold:[0,0.5,1],rootMargin:'10px'});"
            "io.observe(a); io.observe(b); r.push(io.rootMargin+'|'+io.thresholds.join());");
    tick(&g, 2);
    EQ("r.join(' ')", "10px 10px 10px 10px|0,0.5,1 a:true:1.00:0 b:false:0.00:1000");
    /* rola até b ficar meio visível: y da viewport 750..1050 + margem 10 → b (1000..1100) visível em 60% */
    run(&g, "r=[];");
    js_page_viewport(g.js, 400, 300, 750);
    tick(&g, 2);
    EQ("r.join(' ')", "a:false:0.00:-750 b:true:0.60:250");
    run(&g, "r=[];");
    js_page_viewport(g.js, 400, 300, 760);
    tick(&g, 2);
    EQ("r.join(' ')", "");   /* 0.6 → 0.7 não cruza limiar nenhum: nada a entregar */
    run(&g, "r=[]; io.disconnect(); var c=document.getElementById('c'), box=document.getElementById('box');"
            "var io2=new IntersectionObserver(function(es){r.push('raiz:'+es[0].isIntersecting+':'+es[0].rootBounds.height)},{root:box}); io2.observe(c);");
    tick(&g, 2);
    EQ("r.join(' ')", "raiz:true:50");
    EQ("try{new IntersectionObserver(function(){},{threshold:2})}catch(e){e instanceof RangeError}", "true");
    EQ("try{new IntersectionObserver(function(){},{rootMargin:'5em'})}catch(e){e.name}", "SyntaxError");
    og_close(&g);
}

/* MutationObserver: nó tirado de subárvore observada continua observado até a entrega (registro transitório) */
WEB_HOST_CASE(obs_mutation_transitorio) {
    og_t g = og_open("<html><body><div id=a><p id=f><b id=n>x</b></p></div><script>var r=[];"
                     "var a=document.getElementById('a'), f=document.getElementById('f'), n=document.getElementById('n');</script></body></html>");
    run(&g, "var mo=new MutationObserver(function(rs){ r.push(rs.map(function(m){ return m.type+':'+m.target.id+(m.attributeName?':'+m.attributeName:'') }).join(',')) });"
            "mo.observe(a,{subtree:true,childList:true,attributes:true}); a.removeChild(f); n.setAttribute('title','1'); f.appendChild(document.createElement('i'));");
    tick(&g, 1);
    EQ("r.join('|')", "childList:a,attributes:n:title,childList:f");
    run(&g, "r=[]; n.setAttribute('title','2');");
    tick(&g, 1);
    EQ("r.length", "0");
    og_close(&g);
}

/* IntersectionObserver: ancestral com overflow hidden recorta (o alvo fora da caixa dele não intersecta) */
WEB_HOST_CASE(obs_intersection_overflow) {
    og_t g = og_open("<html><body style='margin:0'><div id=c style='overflow:hidden;height:50px'><div style='height:100px'></div>"
                     "<div id=t style='height:20px'></div></div><div id=u style='height:20px'></div><script>var r=[];</script></body></html>");
    run(&g, "var io=new IntersectionObserver(function(es){ es.forEach(function(e){ r.push(e.target.id+':'+e.isIntersecting) }) });"
            "io.observe(document.getElementById('t')); io.observe(document.getElementById('u'));");
    tick(&g, 3);
    EQ("r.sort().join('|')", "t:false|u:true");
    og_close(&g);
}
