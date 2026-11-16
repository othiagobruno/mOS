/* mOS — teste de host do CompressionStream/DecompressionStream (kernel/web/web_zlib.c + web_stream.js): vetores do
 * zlib de referência (gzip com e sem nome no cabeçalho, zlib, deflate cru com blocos dinâmicos, fixos e guardados) em
 * tools/testdata/web/zlib, entregues em pedaços pequenos (bloco partido entre pedaços), ida e volta nos três formatos
 * e os erros (CRC, truncado, dados depois do fim, formato e pedaço inválidos). */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } zg_t;
static zg_t zg_open(void) {
    zg_t g;
    /* o mesmo gerador do script que fez os vetores (LCG de 31 bits, palavra pelos bits 16..19) */
    const char *html = "<html><body><script>var r=[];"
                       "function gerar(n){ var w=['casa','mundo','olá','zebra','navegador','motor','janela','quadro','página','código','teste','ação','fluxo','byte','bloco','fim'], x=7, o=[];"
                       " for (var i=0;i<n;i++){ x=((Math.imul(x,1103515245)+12345)>>>0)&0x7fffffff; o.push(w[(x>>>16)%16]) } return o.join(' ') }"
                       "var TEXTO=gerar(30000), BYTES=new TextEncoder().encode(TEXTO);"
                       "function pedacos(u8,n){ var a=[]; for (var i=0;i<u8.length;i+=n) a.push(u8.slice(i,i+n)); return a }"
                       "function abrir(fmt, partes){ return new Response(ReadableStream.from(partes).pipeThrough(new DecompressionStream(fmt))).arrayBuffer().then(function(b){ return new Uint8Array(b) }) }"
                       "function igual(a,b){ if (a.length!==b.length) return false; for (var i=0;i<a.length;i++) if (a[i]!==b[i]) return false; return true }"
                       "function baixar(n){ return fetch('/zlib/'+n).then(function(x){ return x.arrayBuffer() }).then(function(b){ return new Uint8Array(b) }) }"
                       "</script></body></html>";
    g.d = doc_parse(html, strlen(html), "http://teste/z.html");
    g.js = js_page_new2(g.d, "http://teste/z.html", 300, 200, NULL, NULL);
    for (int i = 0; i < 3; i++) js_page_tick(g.js);
    return g;
}
static void zg_close(zg_t *g) { js_page_free(g->js); doc_free(g->d); }
static const char *run(zg_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[700]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)
/* roda e espera n respostas em r */
static void wait_r(zg_t *g, int n) {
    char q[64];
    snprintf(q, sizeof q, "r.length>=%d", n);
    for (int i = 0; i < 400 && strcmp(run(g, q), "true"); i++) js_page_tick(g->js);
}
#define ASYNC(n, code, want) do { run(&g, "r=[];"); run(&g, code); wait_r(&g, n); EQ("r.join('|')", want); } while (0)

WEB_HOST_CASE(zlib_vetores) {
    zg_t g = zg_open();
    EQ("BYTES.length", "191013");
    EQ("typeof CompressionStream+','+typeof DecompressionStream+','+Object.prototype.toString.call(new DecompressionStream('gzip'))", "function,function,[object DecompressionStream]");
    /* inteiros e em pedaços de 97 bytes (blocos partidos) */
    ASYNC(3, "Promise.all(['gzip:texto.gz','deflate:texto.zz','deflate-raw:texto.raw'].map(function(s){ var p=s.split(':');"
             " return baixar(p[1]).then(function(c){ return Promise.all([abrir(p[0],[c]), abrir(p[0],pedacos(c,97))]) }).then(function(v){ r.push(p[0]+':'+igual(v[0],BYTES)+':'+igual(v[1],BYTES)) }) }))",
          "gzip:true:true|deflate:true:true|deflate-raw:true:true");
    ASYNC(3, "baixar('guardado.raw').then(function(c){ return abrir('deflate-raw', pedacos(c,1000)) }).then(function(v){ r.push(v.length+':'+igual(v, BYTES.slice(0,66000))) });"
             "baixar('fixo.raw').then(function(c){ return abrir('deflate-raw', pedacos(c,5)) }).then(function(v){ r.push(v.length+':'+igual(v, BYTES.slice(0,5000))) });"
             "baixar('nome.gz').then(function(c){ return abrir('gzip', pedacos(c,3)) }).then(function(v){ r.push(new TextDecoder().decode(v)) })",
          "66000:true|5000:true|com nome no cabecalho");
    zg_close(&g);
}

WEB_HOST_CASE(zlib_ida_e_volta) {
    zg_t g = zg_open();
    ASYNC(3, "['gzip','deflate','deflate-raw'].forEach(function(f){ var c=ReadableStream.from(pedacos(BYTES,7000)).pipeThrough(new CompressionStream(f));"
             " new Response(c).arrayBuffer().then(function(b){ var z=new Uint8Array(b); return abrir(f, pedacos(z,333)).then(function(v){ r.push(f+':'+igual(v,BYTES)+':'+(z.length<BYTES.length/2)) }) }) })",
          "gzip:true:true|deflate:true:true|deflate-raw:true:true");
    /* cabeçalhos: gzip 1f 8b 08, zlib 78 9c; vazio também fecha certo */
    ASYNC(2, "new Response(ReadableStream.from([new Uint8Array(0)]).pipeThrough(new CompressionStream('gzip'))).arrayBuffer().then(function(b){ var z=new Uint8Array(b);"
             " r.push([z[0],z[1],z[2],z.length].join()); return abrir('gzip',[z]) }).then(function(v){ r.push(v.length) })",
          "31,139,8,20|0");
    ASYNC(1, "new Response(ReadableStream.from([new TextEncoder().encode('abc')]).pipeThrough(new CompressionStream('deflate'))).arrayBuffer().then(function(b){ var z=new Uint8Array(b); r.push(z[0]+','+z[1]) })",
          "120,156");
    /* DataView e ArrayBuffer também servem de pedaço */
    ASYNC(1, "var u=new TextEncoder().encode('visão'); new Response(ReadableStream.from([u.buffer, new DataView(u.buffer,0,2)]).pipeThrough(new CompressionStream('deflate-raw'))).arrayBuffer()"
             ".then(function(b){ return abrir('deflate-raw',[new Uint8Array(b)]) }).then(function(v){ r.push(new TextDecoder().decode(v)) })",
          "visãovi");
    zg_close(&g);
}

WEB_HOST_CASE(zlib_erros) {
    zg_t g = zg_open();
    EQ("try{ new CompressionStream('brotli') }catch(e){ e instanceof TypeError }", "true");
    EQ("try{ new DecompressionStream() }catch(e){ e instanceof TypeError }", "true");
    EQ("try{ CompressionStream('gzip') }catch(e){ e instanceof TypeError }", "true");
    run(&g, "r=[];");
    run(&g, "baixar('texto.gz').then(function(c){"
             " var crc=c.slice(); crc[crc.length-6]^=1; var curto=c.slice(0,c.length-20), extra=new Uint8Array(c.length+2); extra.set(c);"
             " var lixo=new Uint8Array([1,2,3,4,5,6,7,8,9,10,11,12]);"
             " [['crc',crc],['curto',curto],['extra',extra],['lixo',lixo]].forEach(function(t){ abrir('gzip',[t[1]]).then(function(){ r.push(t[0]+':passou') }, function(e){ r.push(t[0]+':'+e.name) }) }) })");
    wait_r(&g, 4);
    EQ("r.sort().join('|')", "crc:TypeError|curto:TypeError|extra:TypeError|lixo:TypeError");
    run(&g, "r=[]; var cs=new CompressionStream('gzip'), w=cs.writable.getWriter(); cs.readable.getReader().read().catch(function(e){ r.push('leitura:'+e.name) });"
            " w.write('texto').then(function(){ r.push('aceitou') }, function(e){ r.push(e.name) })");
    wait_r(&g, 2);
    EQ("r.sort().join('|')", "TypeError|leitura:TypeError");
    zg_close(&g);
}
