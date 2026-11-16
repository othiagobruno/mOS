/* mOS — teste de host do Fetch e do XMLHttpRequest (kernel/web/web_fetch.c) e do DOMParser: Headers, Request,
 * Response, FormData (inclusive de <form>), fetch() com http (eco do shim), data:, blob:, redirecionamentos, 404,
 * AbortController/AbortSignal, XHR com eventos, responseType, cabeçalhos, envio, síncrono, abort e timeout. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } fg_t;
static void tick(fg_t *g, int n) { for (int i = 0; i < n; i++) js_page_tick(g->js); }
static fg_t fg_open(const char *html) {
    fg_t g;
    g.d = doc_parse(html, strlen(html), "http://teste/f.html");
    g.js = js_page_new2(g.d, "http://teste/f.html", 300, 200, NULL, NULL);
    tick(&g, 5);
    return g;
}
static void fg_close(fg_t *g) { js_page_free(g->js); doc_free(g->d); }
static const char *run(fg_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[700]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)
static const char *PG = "<html><body><script>var r=[];</script></body></html>";

WEB_HOST_CASE(fetch_headers_request_response) {
    fg_t g = fg_open(PG);
    EQ("var h=new Headers({'X-A':' 1 ','Content-Type':'text/plain'}); h.append('x-a','2'); [h.get('X-A'),h.has('content-type'),h.get('nada')].join('|')", "1, 2|true|");
    EQ("h.set('X-A','3'); h.delete('content-type'); [...h].join(';')+' '+[...h.keys()].join()", "x-a,3 x-a");
    EQ("var h2=new Headers([['b','1'],['a','2'],['Set-Cookie','c=1'],['set-cookie','d=2']]); [...h2.entries()].map(function(e){return e.join('=')}).join('&')+' '+h2.getSetCookie().join()",
       "a=2&b=1&set-cookie=c=1&set-cookie=d=2 c=1,d=2");
    EQ("try{new Headers({'nome inválido':'x'})}catch(e){e instanceof TypeError}", "true");
    EQ("try{new Headers({'a':'x\\ny'})}catch(e){e instanceof TypeError}", "true");
    EQ("var q=new Request('/x?y',{method:'post',headers:{'Host':'mau','X-Ok':'1'},body:'oi'}); [q.url,q.method,String(q.headers.get('host')),q.headers.get('x-ok'),q.headers.get('content-type'),q.mode,q.credentials,q.redirect,q.cache].join('|')",
       "http://teste/x?y|POST|null|1|text/plain;charset=UTF-8|cors|same-origin|follow|default");
    EQ("[new Request('a',{method:'patch'}).method, new Request('a',{method:'Delete'}).method].join()", "patch,DELETE");
    EQ("try{new Request('a',{body:'x'})}catch(e){e instanceof TypeError}", "true");
    EQ("try{new Request('a',{method:'TRACE'})}catch(e){e instanceof TypeError}", "true");
    EQ("try{new Request('http://u:p@h/')}catch(e){e instanceof TypeError}", "true");
    EQ("var q2=q.clone(); q2.signal instanceof AbortSignal && q2.url===q.url && q2.headers!==q.headers", "true");
    run(&g, "q.text().then(function(t){r.push('q:'+t+':'+q.bodyUsed)}); q.text().catch(function(e){r.push('dupla:'+(e instanceof TypeError))});");
    tick(&g, 1);
    EQ("r.join('|')", "q:oi:true|dupla:true");
    EQ("var s=new Response('olá',{status:201,statusText:'Feito',headers:{'X-Z':'z'}}); [s.status,s.ok,s.statusText,s.headers.get('content-type'),s.headers.get('x-z'),s.type,s.url,s.redirected].join('|')",
       "201|true|Feito|text/plain;charset=UTF-8|z|default||false");
    EQ("try{new Response('',{status:99})}catch(e){e instanceof RangeError}", "true");
    EQ("try{new Response('x',{status:204})}catch(e){e instanceof TypeError}", "true");
    EQ("var e1=Response.error(); e1.type+':'+e1.status+' '+Response.redirect('/z',301).headers.get('location')+' '+Response.redirect('/z').status", "error:0 http://teste/z 302");
    EQ("try{Response.redirect('/z',200)}catch(e){e instanceof RangeError}", "true");
    run(&g, "r=[]; Response.json({a:[1,2]},{status:202}).json().then(function(v){r.push('json:'+v.a.join())});"
            "new Response(new Uint8Array([1,2,3])).arrayBuffer().then(function(b){r.push('ab:'+b.byteLength)});"
            "new Response('x',{headers:{'content-type':'Image/PNG'}}).blob().then(function(b){r.push('blob:'+b.type+':'+b.size)});"
            "var c0=new Response('dois'); var c1=c0.clone(); c0.text().then(function(t){r.push('c0:'+t)}); c1.text().then(function(t){r.push('c1:'+t)});"
            "new Response('{x').json().catch(function(e){r.push('ruim:'+(e instanceof SyntaxError))});");
    tick(&g, 1);
    EQ("r.join('|')", "json:1,2|ab:3|blob:image/png:1|c0:dois|c1:dois|ruim:true");
    EQ("s.headers.set('a','b'); s.headers.get('a')", "b");
    fg_close(&g);
}

WEB_HOST_CASE(fetch_rede_e_locais) {
    fg_t g = fg_open(PG);
    run(&g, "fetch('/__eco',{method:'PUT',headers:{'X-Teste':'sim','Content-Type':'application/json'},body:JSON.stringify({a:1})})"
            ".then(function(res){r.push(res.status+':'+res.ok+':'+res.url+':'+res.type+':'+res.headers.get('x-teste')+':'+res.headers.has('set-cookie')); return res.text()})"
            ".then(function(t){r.push(t.replace(/\\r\\n/g,'/').replace(/\\n/g,'#'))});");
    EQ("r.length", "0");
    tick(&g, 3);
    EQ("r.join('|')", "200:true:http://teste/__eco:basic:um, dois:false|PUT#x-teste: sim/content-type: application/json/#{\"a\":1}");
    run(&g, "r=[]; fetch('/__status/404').then(function(res){r.push(res.status+':'+res.ok+':'+res.statusText)});"
            "fetch('/__redir').then(function(res){r.push('seguiu:'+res.redirected+':'+res.status)});"
            "fetch('/__redir',{redirect:'manual'}).then(function(res){r.push('manual:'+res.type+':'+res.status)});"
            "fetch('/__redir',{redirect:'error'}).catch(function(e){r.push('erro:'+(e instanceof TypeError))});"
            "fetch('data:text/plain;charset=utf-8,ol%C3%A1').then(function(res){r.push('data:'+res.headers.get('content-type')); return res.text()}).then(function(t){r.push(t)});"
            "fetch(URL.createObjectURL(new Blob(['bb'],{type:'x/y'}))).then(function(res){return res.blob()}).then(function(b){r.push('blob:'+b.type+':'+b.size)});"
            "fetch('ftp://x/').catch(function(e){r.push('esquema:'+e.message)});");
    tick(&g, 4);
    EQ("r.sort().join('|')", "404:false:Not Found|blob:x/y:2|data:text/plain;charset=utf-8|erro:true|esquema:Failed to fetch|manual:opaqueredirect:0|olá|seguiu:true:200");
    fg_close(&g);
}

WEB_HOST_CASE(fetch_abort) {
    fg_t g = fg_open(PG);
    EQ("var c=new AbortController(); var ev=0; c.signal.onabort=function(e){ev++}; c.signal.addEventListener('abort',function(){ev++},{once:true});"
       "c.abort(); c.abort(); [c.signal.aborted, c.signal.reason.name, c.signal.reason instanceof DOMException, ev].join()", "true,AbortError,true,2");
    EQ("try{c.signal.throwIfAborted()}catch(e){e.name}", "AbortError");
    run(&g, "r=[]; fetch('/__eco',{signal:c.signal}).catch(function(e){r.push('antes:'+e.name)});"
            "var c2=new AbortController(); fetch('/__lento',{signal:c2.signal}).catch(function(e){r.push('durante:'+e.name)}); c2.abort();"
            "var c3=new AbortController(); fetch('/__lento',{signal:c3.signal}).catch(function(e){r.push('razão:'+e)}); c3.abort('minha');"
            "fetch('/__lento',{signal:AbortSignal.timeout(5)}).catch(function(e){r.push('tempo:'+e.name)});"
            "var a1=new AbortController(), any=AbortSignal.any([a1.signal, new AbortController().signal]); a1.abort(); r.push('any:'+any.aborted);"
            "r.push('já:'+AbortSignal.abort().aborted);");
    tick(&g, 2);
    for (int i = 0; i < 20; i++) { tick(&g, 1); host_sleep_ms(2); }
    EQ("r.sort().join('|')", "antes:AbortError|any:true|durante:AbortError|já:true|razão:minha|tempo:TimeoutError");
    fg_close(&g);
}

WEB_HOST_CASE(fetch_formdata) {
    fg_t g = fg_open("<html><body><form id=f><input name=a value=1><input name=b type=checkbox checked><input name=c type=checkbox>"
                     "<select name=s><option value=x>X<option value=y selected>Y</select><textarea name=t>texto</textarea>"
                     "<input name=d disabled value=no><button name=btn value=v>ok</button><input type=file name=arq></form>"
                     "<script>var r=[];</script></body></html>");
    EQ("var fd=new FormData(document.getElementById('f')); [...fd.keys()].join()+' '+fd.get('a')+fd.get('b')+fd.get('s')+fd.get('t')+' '+(fd.get('arq') instanceof File)",
       "a,b,s,t,arq 1onytexto true");
    EQ("var f2=new FormData(document.getElementById('f'), document.querySelector('button')); f2.get('btn')", "v");
    EQ("var g2=new FormData(); g2.append('x','1'); g2.append('x','2'); g2.append('arq', new Blob(['zz'],{type:'a/b'})); g2.set('y','3'); g2.delete('nada');"
       "[g2.getAll('x').join(), g2.get('arq').name, g2.get('arq').type, g2.has('y'), [...g2].length].join('|')", "1,2|blob|a/b|true|4");
    EQ("try{new FormData({})}catch(e){e instanceof TypeError}", "true");
    run(&g, "fetch('/__eco',{method:'POST',body:g2}).then(function(res){return res.text()}).then(function(t){"
            "r.push(/content-type: multipart\\/form-data; boundary=----mOSFormBoundary/.test(t)); r.push(/name=\"arq\"; filename=\"blob\"\\r\\nContent-Type: a\\/b\\r\\n\\r\\nzz/.test(t)) });"
            "new Response(g2).formData().then(function(back){ r.push('volta:'+back.getAll('x').join()+':'+back.get('arq').name+':'+back.get('arq').size) });"
            "new Response('a=1&b=%C3%A9',{headers:{'content-type':'application/x-www-form-urlencoded'}}).formData().then(function(u){ r.push('url:'+u.get('b')) });"
            "fetch('/__eco',{method:'POST',body:new URLSearchParams('k=v w')}).then(function(res){return res.text()}).then(function(t){ r.push(/x-www-form-urlencoded;charset=UTF-8[\\s\\S]*k=v\\+w$/.test(t)) });");
    tick(&g, 4);
    EQ("r.join('|')", "volta:1,2:blob:2|url:é|true|true|true");
    fg_close(&g);
}

WEB_HOST_CASE(xhr_completo) {
    fg_t g = fg_open(PG);
    run(&g, "var x=new XMLHttpRequest(), ev=[]; x.onreadystatechange=function(){ev.push('rs'+x.readyState)};"
            "['loadstart','progress','load','loadend'].forEach(function(t){x.addEventListener(t,function(e){ev.push(t+(e.lengthComputable?':'+e.loaded:''))})});"
            "x.open('post','/__eco'); x.setRequestHeader('X-Um','1'); x.setRequestHeader('x-um','2'); x.setRequestHeader('Cookie','não'); x.send('corpo');");
    EQ("ev.join()+' '+x.readyState", "rs1,loadstart 1");
    tick(&g, 3);
    EQ("ev.join()", "rs1,loadstart,rs2,rs3,progress:63,rs4,load:63,loadend:63");
    EQ("x.status+' '+x.statusText+' '+x.responseURL+' '+x.getResponseHeader('X-TESTE')+' '+x.getResponseHeader('set-cookie')", "200 OK http://teste/__eco um, dois null");
    EQ("x.getAllResponseHeaders()", "content-type: text/plain\r\nx-teste: um, dois\r\n");
    EQ("x.responseText.split('\\n')[0]+'|'+/x-um: 1, 2/.test(x.responseText)+'|'+/cookie/i.test(x.responseText)+'|'+x.responseText.slice(-5)", "POST|true|false|corpo");
    /* responseType */
    run(&g, "r=[]; var y=new XMLHttpRequest(); y.responseType='json'; y.onload=function(){r.push('json:'+y.response.a)}; y.open('GET','data:application/json,{\"a\":7}'); y.send();"
            "var z=new XMLHttpRequest(); z.open('GET','data:,abc'); z.responseType='arraybuffer'; z.onload=function(){r.push('ab:'+z.response.byteLength+':'+(z.response instanceof ArrayBuffer))}; z.send();"
            "var b=new XMLHttpRequest(); b.open('GET','data:image/png;base64,AAA='); b.responseType='blob'; b.onload=function(){r.push('blob:'+b.response.type+':'+b.response.size)}; b.send();"
            "var d=new XMLHttpRequest(); d.open('GET','data:text/html,<title>T</title><p id=p>oi</p>'); d.responseType='document'; d.onload=function(){r.push('doc:'+d.response.title+':'+d.response.getElementById('p').textContent)}; d.send();");
    tick(&g, 3);
    EQ("r.sort().join('|')", "ab:3:true|blob:image/png:2|doc:T:oi|json:7");
    EQ("try{y.responseText}catch(e){e.name}", "InvalidStateError");
    /* síncrono, abort e timeout */
    EQ("var s=new XMLHttpRequest(); s.open('GET','data:,sinc',false); s.send(); s.readyState+':'+s.responseText", "4:sinc");
    EQ("try{var s2=new XMLHttpRequest(); s2.open('GET','/a',false); s2.responseType='json'}catch(e){e.name}", "InvalidAccessError");
    run(&g, "r=[]; var a=new XMLHttpRequest(); a.onabort=function(){r.push('abort:'+a.readyState)}; a.onloadend=function(){r.push('fim')}; a.open('GET','/__lento'); a.send(); a.abort(); r.push('depois:'+a.readyState+':'+a.status);"
            "var t=new XMLHttpRequest(); t.timeout=5; t.ontimeout=function(e){r.push('timeout:'+e.type+':'+t.readyState)}; t.open('GET','/__lento'); t.send();");
    for (int i = 0; i < 20; i++) { tick(&g, 1); host_sleep_ms(2); }
    EQ("r.join('|')", "abort:4|fim|depois:0:0|timeout:timeout:4");
    EQ("try{new XMLHttpRequest().send()}catch(e){e.name}", "InvalidStateError");
    EQ("try{new XMLHttpRequest().open('GET','http://[x')}catch(e){e.name}", "SyntaxError");
    EQ("XMLHttpRequest.DONE+' '+(new XMLHttpRequest().upload instanceof XMLHttpRequestUpload)", "4 true");
    fg_close(&g);
}

WEB_HOST_CASE(domparser) {
    fg_t g = fg_open(PG);
    EQ("var d=new DOMParser().parseFromString('<title>Oi</title><div id=a class=x>um <b>dois</b></div>','text/html');"
       "[d.title, d.getElementById('a').textContent, d.querySelector('.x b').tagName, d.body.childNodes.length, d.documentElement.tagName, d===document].join('|')",
       "Oi|um dois|B|1|HTML|false");
    EQ("var el=d.getElementById('a'); (el.ownerDocument===d)+','+(document.body.ownerDocument===document)", "true,true");
    EQ("try{new DOMParser().parseFromString('x','text/plain')}catch(e){e instanceof TypeError}", "true");
    fg_close(&g);
}
