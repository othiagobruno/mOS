/* mOS — teste de host de Blob, File, FileReader, URLs blob: (img e fetch), Response.arrayBuffer/blob, canvas.toBlob,
 * createImageBitmap/ImageBitmap (kernel/web/web_blob.c, canvas_js.c) e de URL/URLSearchParams (web_url.c), com casos
 * do WHATWG URL Standard. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } bg_t;

static void tick(bg_t *g, int n) { for (int i = 0; i < n; i++) { doc_pump_images(g->d); js_page_tick(g->js); } }
static bg_t bg_open(const char *url) {
    bg_t g;
    const char *html = "<html><body><canvas id=c width=8 height=4></canvas><script>var r=[];</script></body></html>";
    g.d = doc_parse(html, strlen(html), url);
    g.js = js_page_new2(g.d, url, 300, 200, NULL, NULL);
    tick(&g, 10);
    return g;
}
static void bg_close(bg_t *g) { js_page_free(g->js); doc_free(g->d); }
static const char *run(bg_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[600]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)

WEB_HOST_CASE(blob_file) {
    bg_t g = bg_open("http://teste/blob.html");
    EQ("var b=new Blob(['olá ',new Uint8Array([65,66]),new Blob(['!'])],{type:'Text/Plain'}); b.size+' '+b.type+' '+(b instanceof Blob)", "8 text/plain true");
    EQ("Object.prototype.toString.call(b)+' '+new Blob().size+' '+new Blob([],{type:'á'}).type.length", "[object Blob] 0 0");
    EQ("b.slice(-3).size+','+b.slice(1,3,'x/y').type+','+b.slice(5,2).size", "3,x/y,0");
    run(&g, "b.text().then(function(t){r.push(t)}); b.slice(4,6).arrayBuffer().then(function(a){r.push(Array.from(new Uint8Array(a)).join('-'))});"
            "new Blob([new Uint16Array([0x4142]).buffer]).bytes().then(function(u){r.push(u instanceof Uint8Array && u.length)});");
    tick(&g, 2);
    EQ("r.join('|')", "olá AB!|32-65|2");
    EQ("var f=new File(['abc'],'a.txt',{type:'text/plain',lastModified:42}); [f.name,f.size,f.type,f.lastModified,f instanceof Blob,f instanceof File,f.webkitRelativePath].join()", "a.txt,3,text/plain,42,true,true,");
    EQ("typeof new File([],'x').lastModified+' '+(File.prototype instanceof Blob)", "number true");
    EQ("try{new File(['x'])}catch(e){e instanceof TypeError}", "true");
    EQ("try{Blob()}catch(e){e instanceof TypeError}", "true");
    bg_close(&g);
}

WEB_HOST_CASE(blob_filereader) {
    bg_t g = bg_open("http://teste/fr.html");
    run(&g, "var ev=[]; var fr=new FileReader(); fr.onloadstart=function(){ev.push('start:'+fr.readyState)};"
            "fr.addEventListener('progress', function(e){ev.push('prog:'+e.loaded+'/'+e.total)});"
            "fr.onload=function(e){ev.push('load:'+fr.result+':'+fr.readyState+':'+(e.target===fr))}; fr.onloadend=function(){ev.push('end')};"
            "fr.readAsText(new Blob(['oi ç']));");
    EQ("fr.readyState+','+fr.result", "1,null");
    EQ("try{fr.readAsText(new Blob([]))}catch(e){e.name}", "InvalidStateError");
    tick(&g, 2);
    EQ("ev.join('|')", "start:1|prog:5/5|load:oi ç:2:true|end");
    run(&g, "var out=[]; var a=new FileReader(); a.onload=function(){out.push(a.result)}; a.readAsDataURL(new Blob(['hi'],{type:'text/plain'}));"
            "var b2=new FileReader(); b2.onload=function(){out.push(new Uint8Array(b2.result).join())}; b2.readAsArrayBuffer(new Blob([new Uint8Array([1,2,255])]));"
            "var c2=new FileReader(); c2.onload=function(){out.push(c2.result.length+':'+c2.result.charCodeAt(1))}; c2.readAsBinaryString(new Blob([new Uint8Array([0,200])]));"
            "var d2=new FileReader(); d2.onload=function(){out.push(d2.result)}; d2.readAsText(new Blob([new Uint8Array([0xE9])]),'iso-8859-1');");
    tick(&g, 2);
    EQ("out.join('|')", "data:text/plain;base64,aGk=|1,2,255|2:200|é");
    run(&g, "var ab=[]; var x=new FileReader(); x.onabort=function(){ab.push('abort:'+x.readyState+':'+x.result)}; x.onload=function(){ab.push('load')}; x.onloadend=function(){ab.push('end')};"
            "x.readAsText(new Blob(['z'])); x.abort();");
    tick(&g, 2);
    EQ("ab.join('|')+' '+FileReader.DONE", "abort:2:null|end 2");
    bg_close(&g);
}

WEB_HOST_CASE(blob_urls_img_fetch_canvas) {
    bg_t g = bg_open("http://teste/u.html");
    EQ("var u=URL.createObjectURL(new Blob(['dados'],{type:'text/plain'})); /^blob:http:\\/\\/teste\\/[0-9a-f-]{36}$/.test(u)", "true");
    run(&g, "fetch(u).then(function(res){r.push(res.status+':'+res.ok); return res.text()}).then(function(t){r.push(t)});"
            "fetch(u).then(function(res){return res.arrayBuffer()}).then(function(a){r.push(a.byteLength)});"
            "fetch(u).then(function(res){return res.blob()}).then(function(b){r.push(b.type+':'+b.size)});");
    tick(&g, 3);
    EQ("r.join('|')", "200:true|dados|5|text/plain:5");
    run(&g, "r=[]; URL.revokeObjectURL(u); fetch(u).then(function(){r.push('não')}, function(e){r.push(e instanceof TypeError)});");
    tick(&g, 3);
    EQ("r.join()", "true");
    /* canvas → toBlob → blob: → <img> → drawImage */
    run(&g, "r=[]; var cv=document.getElementById('c'), cx=cv.getContext('2d'); cx.fillStyle='#0f0'; cx.fillRect(0,0,8,4);"
            "cv.toBlob(function(b){ r.push(b.type+':'+(b.size>20)); var im=new Image(); im.onload=function(){ r.push('img:'+im.naturalWidth+'x'+im.naturalHeight);"
            "cx.clearRect(0,0,8,4); cx.drawImage(im,0,0); r.push(Array.from(cx.getImageData(3,2,1,1).data).join('-')) }; im.src=URL.createObjectURL(b); });");
    EQ("r.length", "0");
    tick(&g, 4);
    EQ("r.join('|')", "image/png:true|img:8x4|0-255-0-255");
    /* createImageBitmap de canvas, Blob e ImageData, com recorte, redimensionamento e flipY */
    run(&g, "r=[]; cx.fillStyle='#f00'; cx.fillRect(0,0,8,2); cx.fillStyle='#00f'; cx.fillRect(0,2,8,2);"
            "createImageBitmap(cv).then(function(bm){ r.push(bm.width+'x'+bm.height+':'+(bm instanceof ImageBitmap));"
            "  cx.clearRect(0,0,8,4); cx.drawImage(bm,0,0); r.push(Array.from(cx.getImageData(1,3,1,1).data).join('-'));"
            "  bm.close(); r.push(bm.width); try{cx.drawImage(bm,0,0)}catch(e){r.push(e.name)} });"
            "createImageBitmap(cv,{imageOrientation:'flipY',resizeWidth:4}).then(function(bm){ r.push('flip:'+bm.width+'x'+bm.height); var p=cx.createPattern(bm,'no-repeat'); r.push(!!p) });"
            "createImageBitmap(new ImageData(new Uint8ClampedArray([255,255,0,255]),1)).then(function(bm){ r.push('id:'+bm.width) });"
            "createImageBitmap(new Blob(['nada'])).then(function(){r.push('não')}, function(e){r.push('blob:'+e.name)});");
    tick(&g, 3);
    EQ("r.join('|')", "8x4:true|0-0-255-255|0|InvalidStateError|flip:4x2|true|id:1|blob:InvalidStateError");
    bg_close(&g);
}

WEB_HOST_CASE(url_whatwg) {
    bg_t g = bg_open("http://teste/url.html");
    EQ("var a=new URL('HTTPS://User:Pa ss@EXAMPLE.com:443/a/./b/../c%7a?q=1 2#frag ment');"
       "[a.href,a.protocol,a.username,a.password,a.host,a.hostname,a.port,a.pathname,a.search,a.hash,a.origin].join('|')",
       "https://User:Pa%20ss@example.com/a/c%7a?q=1%202#frag%20ment|https:|User|Pa%20ss|example.com|example.com||/a/c%7a|?q=1%202|#frag%20ment|https://example.com");
    EQ("new URL('../x/y?z#w','http://h.com/a/b/c').href", "http://h.com/a/x/y?z#w");
    EQ("[new URL('//o.org/p','https://h.com/').href, new URL('/abs','http://h.com/a/b').href, new URL('?n','http://h.com/a?q#f').href, new URL('#g','http://h.com/a?q#f').href, new URL('','http://h.com/a?q#f').href].join(' ')",
       "https://o.org/p http://h.com/abs http://h.com/a?n http://h.com/a?q#g http://h.com/a?q");
    EQ("[new URL('http:foo','http://h.com/a/b').href, new URL('http://h.com:80/').href, new URL('http://h.com:8080').host, new URL('HTTP://H.COM\\\\a\\\\b').pathname].join(' ')",
       "http://h.com/a/foo http://h.com/ h.com:8080 /a/b");
    EQ("[new URL('http://0x7f.1/').hostname, new URL('http://[0:0:0:0:0:0:0:1]/').host, new URL('http://[2001:db8::0:1]:81/').host, new URL('http://192.168.257').hostname].join(' ')",
       "127.0.0.1 [::1] [2001:db8::1]:81 192.168.1.1");
    EQ("new URL('http://münchen.de/').hostname+' '+new URL('https://例え.テスト/').hostname", "xn--mnchen-3ya.de xn--r8jz45g.xn--zckzah");
    EQ("[new URL('mailto:A@b.c').pathname, new URL('data:text/plain,a b').href, new URL('blob:https://x.org/123').origin, new URL('foo://h/p').origin, new URL('file:///C:/d/../e').href].join(' ')",
       "A@b.c data:text/plain,a b https://x.org null file:///C:/e");
    EQ("[URL.canParse('nada'), URL.canParse('a','http://x/'), URL.parse('::'), String(URL.parse('x','http://y/'))].join()", "false,true,,http://y/x");
    EQ("try{new URL('http://exa mple.com')}catch(e){e instanceof TypeError}", "true");
    EQ("try{new URL('http://h:99999/')}catch(e){e instanceof TypeError}", "true");
    EQ("try{new URL('/sem-base')}catch(e){e instanceof TypeError}", "true");
    /* setters */
    EQ("var s=new URL('http://h.com/p?a=1#x'); s.pathname='/q r'; s.search='b=2'; s.hash=''; s.port='8080'; s.hostname='EX.org'; s.protocol='https'; s.username='u';"
       "s.href", "https://u@ex.org:8080/q%20r?b=2");
    EQ("s.protocol='mailto'; s.port='abc'; s.host='o.net:99'; s.href", "https://u@o.net:99/q%20r?b=2");
    EQ("s.href='http://novo/'; s.toString()+' '+JSON.stringify({u:s})", "http://novo/ {\"u\":\"http://novo/\"}");
    EQ("try{s.href='::'}catch(e){e instanceof TypeError}", "true");
    bg_close(&g);
}

WEB_HOST_CASE(url_searchparams) {
    bg_t g = bg_open("http://teste/usp.html");
    EQ("var p=new URLSearchParams('?a=1&b=x+y&a=%C3%A9&c'); [p.get('a'),p.get('b'),p.getAll('a').join(),p.get('c'),String(p.get('z')),p.has('c'),p.size].join('|')", "1|x y|1,é||null|true|4");
    EQ("p.append('d','e f&g'); p.delete('a'); p.set('b','novo'); p.toString()", "b=novo&c=&d=e+f%26g");
    EQ("var q=new URLSearchParams({z:'1',a:'2'}); q.sort(); q.toString()+' '+new URLSearchParams([['x','1'],['y','2']]).toString()", "a=2&z=1 x=1&y=2");
    EQ("var it=[]; for (var [k,v] of new URLSearchParams('a=1&b=2')) it.push(k+v); it.join()+' '+[...new URLSearchParams('a=1').keys()].join()", "a1,b2 a");
    EQ("var fe=[]; new URLSearchParams('m=1&n=2').forEach(function(v,k){fe.push(k+'='+v)}); fe.join()", "m=1,n=2");
    EQ("new URLSearchParams('a=1&a=2&b=3').has('a','2')+','+(function(){var x=new URLSearchParams('a=1&a=2'); x.delete('a','1'); return x.toString()})()", "true,a=2");
    /* searchParams vivo, ligado ao URL */
    EQ("var u=new URL('http://h/?x=1'); u.searchParams.append('y','2 3'); u.href", "http://h/?x=1&y=2+3");
    EQ("u.search='?k=v'; u.searchParams.get('k')+','+u.searchParams.get('x')", "v,null");
    EQ("u.searchParams.delete('k'); u.href+' '+(u.searchParams===u.searchParams)", "http://h/ true");
    EQ("try{new URLSearchParams([['a']])}catch(e){e instanceof TypeError}", "true");
    bg_close(&g);
}
