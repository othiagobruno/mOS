/* mOS — teste de host dos Streams (kernel/web/web_stream.c + web_stream.js): ReadableStream com fila e pull, leitores,
 * cancel, tee, iteração assíncrona, from, bytes com leitor BYOB, WritableStream com contrapressão, abort e erros,
 * TransformStream, pipeTo/pipeThrough com AbortSignal, TextEncoderStream/TextDecoderStream (UTF-8 partido entre pedaços)
 * e Response.body / Blob.stream(). */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } sg_t;
static void tick(sg_t *g, int n) { for (int i = 0; i < n; i++) js_page_tick(g->js); }
static sg_t sg_open(void) {
    sg_t g;
    const char *html = "<html><body><script>var r=[]; function lerTudo(rs){ var rd=rs.getReader(), out=[]; return (function p(){ return rd.read().then(function(x){ if (x.done) return out; out.push(x.value); return p() }) })() }</script></body></html>";
    g.d = doc_parse(html, strlen(html), "http://teste/s.html");
    g.js = js_page_new2(g.d, "http://teste/s.html", 300, 200, NULL, NULL);
    tick(&g, 3);
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
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)
#define ASYNC(code, want) do { run(&g, "r=[];"); run(&g, code); tick(&g, 4); EQ("r.join('|')", want); } while (0)

WEB_HOST_CASE(stream_readable) {
    sg_t g = sg_open();
    EQ("typeof ReadableStream+' '+(new ReadableStream() instanceof ReadableStream)+' '+Object.prototype.toString.call(new ReadableStream())", "function true [object ReadableStream]");
    ASYNC("var n=0; var rs=new ReadableStream({pull:function(c){ n++; if (n>3) c.close(); else c.enqueue('p'+n) }},{highWaterMark:1});"
          "lerTudo(rs).then(function(v){ r.push(v.join()+':'+rs.locked) })", "p1,p2,p3:true");
    ASYNC("var s2=new ReadableStream({start:function(c){ c.enqueue(1); c.enqueue(2); c.close() }}); var rd=s2.getReader();"
          "rd.read().then(function(a){ r.push(a.value+':'+a.done); rd.releaseLock(); r.push('livre:'+!s2.locked); return s2.getReader().read() }).then(function(b){ r.push(b.value) })",
          "1:false|livre:true|2");
    ASYNC("var canc; var s3=new ReadableStream({cancel:function(x){ canc=x }}); s3.cancel('motivo').then(function(){ r.push('cancelado:'+canc) });"
          "try{ s3.getReader(); s3.getReader() }catch(e){ r.push(e instanceof TypeError) }", "true|cancelado:motivo");
    ASYNC("var s4=new ReadableStream({start:function(c){ c.error(new Error('ruim')) }}); s4.getReader().read().catch(function(e){ r.push(e.message) })", "ruim");
    ASYNC("var t=new ReadableStream({start:function(c){ ['a','b'].forEach(function(x){c.enqueue(x)}); c.close() }}).tee();"
          "Promise.all([lerTudo(t[0]),lerTudo(t[1])]).then(function(v){ r.push(v[0].join()+'/'+v[1].join()) })", "a,b/a,b");
    ASYNC("(async function(){ var out=[]; for await (var v of ReadableStream.from([1,2,3])) out.push(v); r.push(out.join()) })()", "1,2,3");
    ASYNC("(async function(){ var s=ReadableStream.from((async function*(){ yield 'x'; yield 'y' })()); var o=[]; for await (var v of s) { o.push(v); break } r.push(o.join()+':'+s.locked) })()", "x:false");
    /* bytes e leitor BYOB */
    ASYNC("var bs=new ReadableStream({type:'bytes', start:function(c){ c.enqueue(new Uint8Array([1,2,3,4,5])); c.close() }}); var br=bs.getReader({mode:'byob'});"
          "br.read(new Uint8Array(3)).then(function(a){ r.push(Array.from(a.value).join()); return br.read(new Uint8Array(8)) }).then(function(b){ r.push(Array.from(b.value).join()); return br.read(new Uint8Array(2)) }).then(function(c){ r.push(c.done) })",
          "1,2,3|4,5|true");
    EQ("try{ new ReadableStream().getReader({mode:'byob'}) }catch(e){ e instanceof TypeError }", "true");
    /* IteratorClose do motor: break, return e exceção fecham o iterador (finally do gerador roda) */
    EQ("var lg=[]; function* gg(){ try { yield 1; yield 2 } finally { lg.push('fim') } } for (var v of gg()) break;"
       "try { for (var w of gg()) throw new Error('x') } catch(e) { lg.push('pegou') } (function(){ for (var z of gg()) return })(); lg.join()", "fim,fim,pegou,fim");
    sg_close(&g);
}

WEB_HOST_CASE(stream_writable_transform) {
    sg_t g = sg_open();
    ASYNC("var got=[]; var ws=new WritableStream({write:function(c){ got.push(c) }, close:function(){ r.push('fechou:'+got.join()) }},{highWaterMark:2});"
          "var w=ws.getWriter(); r.push(w.desiredSize); w.write('a'); w.write('b'); r.push(w.desiredSize); w.close().then(function(){ r.push('ok:'+ws.locked) })",
          "2|0|fechou:a,b|ok:true");
    ASYNC("var ab; var w2=new WritableStream({abort:function(x){ ab=x }}).getWriter(); w2.abort('parou').then(function(){ r.push('abortado:'+ab) }); w2.write(1).catch(function(e){ r.push('escrita:'+e) })",
          "escrita:parou|abortado:parou");
    ASYNC("var w3=new WritableStream({write:function(){ throw new Error('falhou') }}).getWriter(); w3.write(1).catch(function(e){ r.push(e.message) }); w3.closed.catch(function(e){ r.push('closed:'+e.message) })",
          "falhou|closed:falhou");
    ASYNC("var up=new TransformStream({transform:function(c,ctl){ ctl.enqueue(String(c).toUpperCase()) }, flush:function(ctl){ ctl.enqueue('!') }});"
          "lerTudo(ReadableStream.from(['a','b']).pipeThrough(up)).then(function(v){ r.push(v.join('')) })", "AB!");
    ASYNC("var dest=[]; ReadableStream.from([1,2,3]).pipeTo(new WritableStream({write:function(c){ dest.push(c*10) }})).then(function(){ r.push(dest.join()) })", "10,20,30");
    ASYNC("var ac=new AbortController(), dst=new WritableStream({abort:function(){ r.push('destino abortado') }});"
          "var src=new ReadableStream({pull:function(){ return new Promise(function(){}) }, cancel:function(){ r.push('origem cancelada') }});"
          "src.pipeTo(dst,{signal:ac.signal}).catch(function(e){ r.push(e.name) }); ac.abort();", "destino abortado|origem cancelada|AbortError");
    EQ("var id=new TransformStream(); (id.readable instanceof ReadableStream)+','+(id.writable instanceof WritableStream)", "true,true");
    ASYNC("var q=new CountQueuingStrategy({highWaterMark:3}), bl=new ByteLengthQueuingStrategy({highWaterMark:8}); r.push(q.highWaterMark+','+bl.size(new Uint8Array(5)))", "3,5");
    sg_close(&g);
}

WEB_HOST_CASE(stream_texto_e_corpos) {
    sg_t g = sg_open();
    /* "olá €" partido no meio dos bytes de 'á' e do '€' */
    ASYNC("var b=new TextEncoder().encode('olá €'); var parts=[b.slice(0,3), b.slice(3,5), b.slice(5,7), b.slice(7)];"
          "lerTudo(ReadableStream.from(parts).pipeThrough(new TextDecoderStream())).then(function(v){ r.push(v.join('|')) })", "ol|á |€");
    ASYNC("lerTudo(ReadableStream.from(['a','\\uD83D','\\uDE00']).pipeThrough(new TextEncoderStream())).then(function(v){ var n=0; v.forEach(function(x){ n+=x.length }); r.push(n+':'+(v[0] instanceof Uint8Array)) })", "5:true");
    ASYNC("var res=new Response('corpo de resposta'); r.push(res.body instanceof ReadableStream); lerTudo(res.body.pipeThrough(new TextDecoderStream())).then(function(v){ r.push(v.join('')+':'+res.bodyUsed); return res.text() }).catch(function(e){ r.push('dupla:'+(e instanceof TypeError)) })",
          "true|corpo de resposta:true|dupla:true");
    ASYNC("r.push(String(new Response(null).body)); new Blob(['xyz']).stream().getReader().read().then(function(x){ r.push(Array.from(x.value).join()) })", "null|120,121,122");
    ASYNC("var rq=new Request('/x',{method:'POST',body:'abc'}); new Response(rq.body).text().then(function(t){ r.push(t+':'+rq.bodyUsed) })", "abc:true");
    EQ("var Velho=ReadableStream; ReadableStream=1; var v2=ReadableStream; ReadableStream=Velho; v2", "1");
    sg_close(&g);
}
