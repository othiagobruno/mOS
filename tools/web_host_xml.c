/* mOS — teste de host do parser XML (kernel/web/html.c, xml_parse_doc) pelo DOMParser, XMLSerializer e responseXML:
 * namespaces e prefixos, entidades, CDATA, caixa dos nomes, SVG como documento, erros de boa formação com
 * <parsererror>, serialização e o tipo MIME da resposta do XHR. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; } xg_t;
static xg_t xg_open(void) {
    xg_t g;
    const char *html = "<html><body><script>var r=[]; function P(s,t){ return new DOMParser().parseFromString(s, t||'application/xml') }"
                       "function erro(s){ var d=P(s); return d.documentElement.localName==='parsererror' }</script></body></html>";
    g.d = doc_parse(html, strlen(html), "http://teste/x.html");
    g.js = js_page_new2(g.d, "http://teste/x.html", 300, 200, NULL, NULL);
    for (int i = 0; i < 3; i++) js_page_tick(g.js);
    return g;
}
static void xg_close(xg_t *g) { js_page_free(g->js); doc_free(g->d); }
static const char *run(xg_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[700]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)

WEB_HOST_CASE(xml_documento) {
    xg_t g = xg_open();
    EQ("var d=P('<?xml version=\"1.0\"?><!DOCTYPE rss [<!ELEMENT rss ANY>]><rss version=\"2.0\"><Canal Nome=\"x\"><item>A &amp; B &lt;3 &#65;&#x42;</item><item><![CDATA[<b>cru</b> & tal]]></item></Canal></rss>');"
       "[d.documentElement.tagName, d.contentType, d.querySelector('Canal').getAttribute('Nome'), d.getElementsByTagName('item').length, d.getElementsByTagName('item')[0].textContent,"
       " d.getElementsByTagName('item')[1].textContent, d.body, d.xmlVersion].join('|')",
       "rss|application/xml|x|2|A & B <3 AB|<b>cru</b> & tal||1.0");
    /* namespaces: padrão, prefixo, xml:lang e atributos com prefixo */
    EQ("var d=P('<a:raiz xmlns:a=\"urn:a\" xmlns=\"urn:padrao\"><filho a:id=\"1\" xml:lang=\"pt\"/><a:outro/><sem xmlns=\"\"/></a:raiz>'); var e=d.documentElement;"
       "[e.namespaceURI, e.prefix, e.localName, e.tagName, e.firstChild.namespaceURI, e.firstChild.prefix, e.children[1].namespaceURI, e.children[2].namespaceURI].join('|')",
       "urn:a|a|raiz|a:raiz|urn:padrao||urn:a|");
    /* SVG como documento */
    EQ("var s=P('<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 10 10\"><linearGradient id=\"g\"/><circle r=\"4\"/></svg>','image/svg+xml');"
       "[s.documentElement.namespaceURI, s.documentElement.getAttribute('viewBox'), s.querySelector('linearGradient').tagName, s.contentType].join('|')",
       "http://www.w3.org/2000/svg|0 0 10 10|linearGradient|image/svg+xml");
    /* text/html continua HTML */
    EQ("var h=P('<p>oi','text/html'); [h.body.firstChild.tagName, h.contentType].join()", "P,text/html");
    xg_close(&g);
}

WEB_HOST_CASE(xml_erros) {
    xg_t g = xg_open();
    EQ("[erro('<a><b></a>'), erro('<a>'), erro('<a/><b/>'), erro('texto<a/>'), erro('<a x=1/>'), erro('<a x=\"1\" x=\"2\"/>'),"
       " erro('<a>&nbsp;</a>'), erro('<p:a/>'), erro('<a><!-- x -- y --></a>'), erro(''), erro('<a>]]></a>'), erro('<a/>  ')].join()",
       "true,true,true,true,true,true,true,true,true,true,true,false");
    EQ("var d=P('<a>\\n<b></c></a>'); var pe=d.documentElement; [pe.namespaceURI, /Linha 2/.test(pe.textContent), d.getElementsByTagName('parsererror').length].join('|')",
       "http://www.mozilla.org/newlayout/xml/parsererror.xml|true|1");
    xg_close(&g);
}

WEB_HOST_CASE(xml_serializar) {
    xg_t g = xg_open();
    EQ("var d=P('<r xmlns=\"urn:x\"><v a=\"1 &amp; 2\">t&lt;</v><vazio/></r>'); new XMLSerializer().serializeToString(d)",
       "<r xmlns=\"urn:x\"><v a=\"1 &amp; 2\">t&lt;</v><vazio/></r>");
    EQ("var d=P('<r><v/></r>'); var v=d.querySelector('v'); v.innerHTML='<Novo k=\"1\">x</Novo>'; [d.documentElement.outerHTML, v.firstChild.tagName].join('|')",
       "<r><v><Novo k=\"1\">x</Novo></v></r>|Novo");
    EQ("var e=document.createElement('div'); e.innerHTML='<br>a'; new XMLSerializer().serializeToString(e)", "<div xmlns=\"http://www.w3.org/1999/xhtml\"><br/>a</div>");
    EQ("try{ new XMLSerializer().serializeToString({}) }catch(e){ e instanceof TypeError }", "true");
    xg_close(&g);
}

WEB_HOST_CASE(xml_xhr) {
    xg_t g = xg_open();
    /* o shim serve .svg como image/svg+xml e .html como text/html */
    run(&g, "r=[]; var x=new XMLHttpRequest(); x.open('GET','/svg/icone.svg'); x.onload=function(){ r.push(x.responseXML && x.responseXML.documentElement.localName) }; x.send();"
            "var y=new XMLHttpRequest(); y.open('GET','/iframe/filho.html'); y.onload=function(){ r.push(String(y.responseXML)) }; y.send();"
            "var z=new XMLHttpRequest(); z.open('GET','/iframe/filho.html'); z.responseType='document'; z.onload=function(){ r.push(z.response.getElementById('t').textContent) }; z.send();"
            "var w=new XMLHttpRequest(); w.open('GET','/iframe/filho.html'); w.overrideMimeType('text/xml'); w.onload=function(){ r.push(w.responseXML.documentElement.localName) }; w.send();");
    for (int i = 0; i < 20; i++) js_page_tick(g.js);
    EQ("r.sort().join('|')", "filho|null|parsererror|svg");
    xg_close(&g);
}
