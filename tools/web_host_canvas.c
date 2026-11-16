/* mOS — teste de host do Canvas 2D (kernel/web/canvas.c + canvas_js.c): <canvas> no layout e na pintura,
 * propriedades e serialização, caminhos, transformações, gradientes, padrões, composição, recorte, sombra,
 * drawImage, texto, ImageData, toDataURL (PNG lido de volta pelo decodificador do motor), Path2D e DOMException. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"
#include "tools/web_host.h"

typedef struct { doc_t *d; js_page_t *js; surface_t *s; } cg_t;

static void cg_paint(cg_t *g) {
    for (int i = 0; i < 4; i++) { js_page_tick(g->js); doc_pump_images(g->d); }
    doc_layout(g->d, g->s->w, g->s->h, 1.0f);
    gfx_fill(g->s, 0, 0, g->s->w, g->s->h, RGB(255, 255, 255));
    ui_t u;
    memset(&u, 0, sizeof u);
    u.s = g->s;
    doc_paint(g->d, &u, rect_make(0, 0, g->s->w, g->s->h), 0, NULL);
}

static cg_t cg_open(const char *html, int w, int h) {
    cg_t g = { 0 };
    g.d = doc_parse(html, strlen(html), "http://teste/canvas.html");
    g.s = surface_new(w, h);
    g.js = js_page_new2(g.d, "http://teste/canvas.html", w, h, NULL, NULL);
    doc_layout(g.d, w, h, 1.0f);
    for (int i = 0; i < 50 && js_page_busy(g.js); i++) js_page_tick(g.js);
    cg_paint(&g);
    return g;
}
static void cg_close(cg_t *g) { js_page_free(g->js); doc_free(g->d); surface_free(g->s); }

static const char *run(cg_t *g, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(g->js, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[600]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
#define EQ(code, want) do { const char *r_ = run(&g, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)
static u32 spx(cg_t *g, int x, int y) { return g->s->px[(usize)y * g->s->stride + x] & 0xFFFFFF; }

/* pixel RGBA de getImageData como "r,g,b,a" */
#define PX(x, y) "Array.from(c.getImageData(" #x "," #y ",1,1).data).join()"

static const char *PAGE = "<html><body style='margin:0'><canvas id=c width=100 height=50>reserva <b>não aparece</b></canvas>"
                          "<canvas id=d></canvas><script>var c=document.getElementById('c').getContext('2d');"
                          "var cv=document.getElementById('c'), d=document.getElementById('d');</script></body></html>";

WEB_HOST_CASE(canvas_elemento_layout) {
    cg_t g = cg_open(PAGE, 400, 300);
    EQ("cv.width+'x'+cv.height+' '+d.width+'x'+d.height", "100x50 300x150");
    EQ("cv instanceof HTMLCanvasElement && c instanceof CanvasRenderingContext2D && c.canvas===cv", "true");
    EQ("cv.getContext('2d')===c && cv.getContext('webgl')===null", "true");
    EQ("Object.prototype.toString.call(c)", "[object CanvasRenderingContext2D]");
    EQ("cv.getBoundingClientRect().width+','+d.getBoundingClientRect().width", "100,300");
    run(&g, "c.fillStyle='red'; c.fillRect(10,10,20,20);");
    cg_paint(&g);
    /* as duas telas ficam na mesma linha, alinhadas pela base: #c (50 de altura) começa em y = 100 */
    EQ("cv.getBoundingClientRect().top", "100");
    CHECK(spx(&g, 15, 115) == 0xFF0000, "canvas pintado na página: %06x", spx(&g, 15, 115));
    CHECK(spx(&g, 50, 140) == 0xFFFFFF, "canvas transparente mostra o fundo: %06x", spx(&g, 50, 140));
    /* o conteúdo de reserva não é desenhado: nada escuro na faixa do canvas fora do retângulo */
    int dark = 0;
    for (int y = 100; y < 150; y++) for (int x = 35; x < 100; x++) if ((spx(&g, x, y) & 0xFF) < 128) dark++;
    CHECK(dark == 0, "reserva não deve aparecer: %d pixels escuros", dark);
    EQ(PX(15, 15) "+'|'+" PX(5, 5), "255,0,0,255|0,0,0,0");
    /* width novo limpa o bitmap e o estado */
    EQ("cv.width=100; c.fillStyle+' '+" PX(15, 15), "#000000 0,0,0,0");
    EQ("cv.setAttribute('height','20'); cv.height+' '+c.getImageData(0,0,1,30).data.length", "20 120");
    EQ("cv.width=-5; cv.width", "300");
    cg_close(&g);
}

WEB_HOST_CASE(canvas_propriedades) {
    cg_t g = cg_open(PAGE, 200, 100);
    EQ("[c.fillStyle,c.strokeStyle,c.lineWidth,c.lineCap,c.lineJoin,c.miterLimit,c.globalAlpha,c.globalCompositeOperation].join()",
       "#000000,#000000,1,butt,miter,10,1,source-over");
    EQ("[c.font,c.textAlign,c.textBaseline,c.direction,c.imageSmoothingEnabled,c.imageSmoothingQuality,c.shadowColor,c.shadowBlur].join('|')",
       "10px sans-serif|start|alphabetic|inherit|true|low|rgba(0, 0, 0, 0)|0");
    EQ("c.fillStyle='rgba(255,0,0,0.5)'; c.fillStyle", "rgba(255, 0, 0, 0.5)");
    EQ("c.fillStyle='#ABC'; c.fillStyle", "#aabbcc");
    EQ("c.fillStyle='nada'; c.fillStyle", "#aabbcc");
    EQ("c.strokeStyle='hsl(120,100%,50%)'; c.strokeStyle", "#00ff00");
    EQ("c.lineWidth=-1; c.lineWidth=NaN; c.lineWidth=0; c.lineWidth", "1");
    EQ("c.lineWidth=2.5; c.lineCap='round'; c.lineCap='x'; c.lineJoin='bevel'; [c.lineWidth,c.lineCap,c.lineJoin].join()", "2.5,round,bevel");
    EQ("c.globalAlpha=2; c.globalAlpha=0.25; c.globalCompositeOperation='xor'; c.globalCompositeOperation='nada'; c.globalAlpha+c.globalCompositeOperation", "0.25xor");
    EQ("c.font='bold 20px Arial'; c.font", "bold 20px Arial");
    EQ("c.font='italic 600 1.5em  \"Helvetica Neue\",  serif'; c.font", "italic 600 15px \"Helvetica Neue\", serif");
    EQ("c.font='12pt monospace'; c.font", "16px monospace");
    EQ("c.font='inválida'; c.font='20px'; c.font", "16px monospace");
    EQ("c.textAlign='center'; c.textBaseline='middle'; c.textAlign='x'; c.textAlign+c.textBaseline", "centermiddle");
    EQ("c.setLineDash([5,3,2]); c.getLineDash().join()", "5,3,2,5,3,2");
    EQ("c.setLineDash([1,-1]); c.getLineDash().length", "6");
    EQ("c.letterSpacing='2px'; c.letterSpacing", "2px");
    EQ("JSON.stringify(c.getContextAttributes())", "{\"alpha\":true,\"colorSpace\":\"srgb\",\"desynchronized\":false,\"willReadFrequently\":false}");
    EQ("c.save(); c.fillStyle='blue'; c.lineWidth=7; c.translate(5,5); c.restore(); c.fillStyle+' '+c.lineWidth+' '+c.getTransform().e", "#aabbcc 2.5 0");
    EQ("c.restore(); c.restore(); c.fillStyle", "#aabbcc");
    EQ("c.reset(); c.fillStyle+' '+c.lineWidth+' '+c.font", "#000000 1 10px sans-serif");
    cg_close(&g);
}

WEB_HOST_CASE(canvas_caminhos) {
    cg_t g = cg_open(PAGE, 200, 100);
    /* círculo cheio */
    run(&g, "c.fillStyle='#00f'; c.beginPath(); c.arc(25,25,10,0,Math.PI*2); c.fill();");
    EQ(PX(25, 25) "+'|'+" PX(25, 37) "+'|'+" PX(25, 16), "0,0,255,255|0,0,0,0|0,0,255,255");
    EQ("c.isPointInPath(25,25)+','+c.isPointInPath(40,40)+','+c.isPointInPath(34.5,25)", "true,false,true");
    /* par-ímpar: dois retângulos aninhados deixam um furo */
    run(&g, "c.clearRect(0,0,100,50); c.beginPath(); c.rect(0,0,40,40); c.rect(10,10,20,20); c.fill('evenodd');");
    EQ(PX(5, 5) "+'|'+" PX(20, 20), "0,0,255,255|0,0,0,0");
    EQ("c.isPointInPath(20,20,'evenodd')+','+c.isPointInPath(20,20)", "false,true");
    /* traço horizontal de 4px: cobre y 18..22 */
    run(&g, "c.clearRect(0,0,100,50); c.beginPath(); c.moveTo(10,20); c.lineTo(90,20); c.lineWidth=4; c.strokeStyle='#0f0'; c.stroke();");
    EQ(PX(50, 18) "+'|'+" PX(50, 21) "+'|'+" PX(50, 23) "+'|'+" PX(8, 20), "0,255,0,255|0,255,0,255|0,0,0,0|0,0,0,0");
    EQ("c.isPointInStroke(50,21)+','+c.isPointInStroke(50,25)", "true,false");
    /* tampa redonda estende o traço */
    run(&g, "c.lineCap='round'; c.stroke();");
    EQ(PX(9, 20), "0,255,0,255");
    /* tracejado: 10 ligado, 10 desligado */
    run(&g, "c.clearRect(0,0,100,50); c.lineCap='butt'; c.setLineDash([10,10]); c.stroke(); c.setLineDash([]);");
    EQ(PX(15, 20) "+'|'+" PX(25, 20) "+'|'+" PX(35, 20), "0,255,0,255|0,0,0,0|0,255,0,255");
    /* quadrática, bézier, arcTo, ellipse e roundRect fecham áreas esperadas */
    run(&g, "c.clearRect(0,0,100,50); c.fillStyle='#f00'; c.beginPath(); c.ellipse(50,25,30,10,0,0,Math.PI*2); c.fill();");
    EQ(PX(75, 25) "+'|'+" PX(50, 38), "255,0,0,255|0,0,0,0");
    run(&g, "c.clearRect(0,0,100,50); c.beginPath(); c.roundRect(10,10,40,30,10); c.fill();");
    EQ(PX(11, 11) "+'|'+" PX(30, 25) "+'|'+" PX(11, 25), "0,0,0,0|255,0,0,255|255,0,0,255");
    run(&g, "c.clearRect(0,0,100,50); c.beginPath(); c.moveTo(10,40); c.arcTo(10,10,40,10,15); c.lineTo(40,40); c.closePath(); c.fill();");
    EQ(PX(11, 11) "+'|'+" PX(20, 30), "0,0,0,0|255,0,0,255");
    EQ("try{c.arc(0,0,-1,0,1);'não'}catch(e){e instanceof DOMException && e.name+e.code}", "IndexSizeError1");
    EQ("c.moveTo(NaN,1); c.lineTo(Infinity,2); 'ok'", "ok");
    /* Path2D: texto SVG e addPath com transformação */
    run(&g, "c.clearRect(0,0,100,50); var p=new Path2D('M10 10 h 20 v 20 h -20 Z'); c.fillStyle='#0ff'; c.fill(p);"
            "var q=new Path2D(); q.addPath(p,{e:50,f:0}); c.fillStyle='#ff0'; c.fill(q);");
    EQ(PX(20, 20) "+'|'+" PX(70, 20) "+'|'+" PX(45, 20), "0,255,255,255|255,255,0,255|0,0,0,0");
    EQ("c.isPointInPath(p,20,20)+','+c.isPointInPath(q,20,20)+','+(new Path2D(p) instanceof Path2D)", "true,false,true");
    cg_close(&g);
}

WEB_HOST_CASE(canvas_transformacoes) {
    cg_t g = cg_open(PAGE, 200, 100);
    run(&g, "c.translate(50,25); c.rotate(Math.PI/4); c.fillStyle='#f00'; c.fillRect(-5,-5,10,10);");
    /* quadrado girado: a ponta em (50, 25-7) cobre, o canto (45,20) não */
    EQ(PX(50, 19) "+'|'+" PX(44, 19), "255,0,0,255|0,0,0,0");
    EQ("var m=c.getTransform(); [m.a.toFixed(4),m.b.toFixed(4),m.e,m.f,m.is2D,m instanceof DOMMatrix].join()", "0.7071,0.7071,50,25,true,true");
    EQ("c.setTransform(2,0,0,2,0,0); var t=c.getTransform(); t.a+','+t.isIdentity", "2,false");
    EQ("c.setTransform({a:1,d:1}); c.getTransform().isIdentity", "true");
    run(&g, "c.clearRect(0,0,100,50); c.scale(2,1); c.fillRect(10,10,5,5); c.resetTransform();");
    EQ(PX(29, 12) "+'|'+" PX(31, 12), "255,0,0,255|0,0,0,0");
    /* o caminho guarda a transformação de quando foi feito */
    run(&g, "c.clearRect(0,0,100,50); c.beginPath(); c.translate(60,0); c.rect(0,0,10,10); c.resetTransform(); c.fill();");
    EQ(PX(65, 5) "+'|'+" PX(5, 5), "255,0,0,255|0,0,0,0");
    EQ("new DOMMatrix([1,0,0,1,3,4]).inverse().e+','+new DOMMatrix([2,0,0,2,0,0]).transformPoint({x:1,y:2}).y", "-3,4");
    cg_close(&g);
}

WEB_HOST_CASE(canvas_gradientes_padroes) {
    cg_t g = cg_open(PAGE, 200, 100);
    run(&g, "var gr=c.createLinearGradient(0,0,100,0); gr.addColorStop(0,'#f00'); gr.addColorStop(1,'#00f'); c.fillStyle=gr; c.fillRect(0,0,100,10);");
    EQ("c.fillStyle===gr && gr instanceof CanvasGradient", "true");
    const char *a = run(&g, PX(0, 5)), *m = NULL;
    CHECK(!strncmp(a, "25", 2), "início do gradiente vermelho: %s", a);
    m = run(&g, "var d_=c.getImageData(50,5,1,1).data; (d_[0]>100&&d_[0]<155&&d_[2]>100&&d_[2]<155)+''");
    CHECK(!strcmp(m, "true"), "meio do gradiente: %s", run(&g, PX(50, 5)));
    EQ("try{gr.addColorStop(2,'red')}catch(e){e.name}", "IndexSizeError");
    EQ("try{gr.addColorStop(0.5,'nada')}catch(e){e.name}", "SyntaxError");
    EQ("try{c.createRadialGradient(0,0,-1,0,0,1)}catch(e){e.name}", "IndexSizeError");
    /* radial: centro branco, borda preta */
    run(&g, "var r=c.createRadialGradient(50,30,0,50,30,15); r.addColorStop(0,'#fff'); r.addColorStop(1,'#000'); c.fillStyle=r; c.fillRect(30,15,40,30);");
    EQ("var r_=c.getImageData(50,30,1,1).data; (r_[0]>=240&&r_[3]==255)+'|'+" PX(68, 30), "true|0,0,0,255");
    /* cônico: começa vermelho à direita, azul na metade */
    run(&g, "c.clearRect(0,0,100,50); var k=c.createConicGradient(0,50,25); k.addColorStop(0,'#f00'); k.addColorStop(0.5,'#00f'); k.addColorStop(1,'#f00'); c.fillStyle=k; c.fillRect(0,0,100,50);");
    a = run(&g, PX(90, 26));
    CHECK(!strncmp(a, "2", 1) && strstr(a, ",255") != NULL, "cônico à direita vermelho: %s", a);
    a = run(&g, PX(10, 25));
    CHECK(!strncmp(a, "0,0,25", 6) || !strncmp(a, "1,0,25", 6) || !strncmp(a, "2,0,25", 6), "cônico à esquerda azul: %s", a);
    /* padrão a partir de outro canvas */
    run(&g, "var dc=d.getContext('2d'); d.width=2; d.height=2; dc.fillStyle='#0f0'; dc.fillRect(0,0,1,1); dc.fillStyle='#f0f'; dc.fillRect(1,0,1,1);"
            "dc.fillRect(0,1,1,1); dc.fillStyle='#0f0'; dc.fillRect(1,1,1,1);"
            "c.clearRect(0,0,100,50); c.imageSmoothingEnabled=false; c.fillStyle=c.createPattern(d,'repeat'); c.fillRect(0,0,10,10);");
    EQ(PX(0, 0) "+'|'+" PX(1, 0) "+'|'+" PX(2, 0) "+'|'+" PX(3, 1), "0,255,0,255|255,0,255,255|0,255,0,255|0,255,0,255");
    run(&g, "c.clearRect(0,0,100,50); c.fillStyle=c.createPattern(d,'no-repeat'); c.fillRect(0,0,10,10);");
    EQ(PX(1, 1) "+'|'+" PX(5, 5), "0,255,0,255|0,0,0,0");
    EQ("try{c.createPattern(d,'x')}catch(e){e.name}", "SyntaxError");
    cg_close(&g);
}

WEB_HOST_CASE(canvas_composicao_recorte_sombra) {
    cg_t g = cg_open(PAGE, 200, 100);
    run(&g, "c.fillStyle='#f00'; c.fillRect(0,0,50,50); c.globalCompositeOperation='destination-out'; c.fillRect(0,0,10,10); c.globalCompositeOperation='source-over';");
    EQ(PX(5, 5) "+'|'+" PX(15, 15), "0,0,0,0|255,0,0,255");
    /* source-in apaga fora da forma (operação ilimitada) */
    run(&g, "c.globalCompositeOperation='source-in'; c.fillStyle='#00f'; c.fillRect(20,20,10,10); c.globalCompositeOperation='source-over';");
    EQ(PX(25, 25) "+'|'+" PX(40, 40), "0,0,255,255|0,0,0,0");
    /* multiply sobre branco mantém a cor */
    run(&g, "c.clearRect(0,0,100,50); c.fillStyle='#fff'; c.fillRect(0,0,20,20); c.globalCompositeOperation='multiply'; c.fillStyle='#808080'; c.fillRect(0,0,20,20); c.globalCompositeOperation='source-over';");
    EQ(PX(10, 10), "128,128,128,255");
    /* globalAlpha */
    run(&g, "c.clearRect(0,0,100,50); c.globalAlpha=0.5; c.fillStyle='#f00'; c.fillRect(0,0,10,10); c.globalAlpha=1;");
    EQ(PX(5, 5), "255,0,0,128");
    /* recorte */
    run(&g, "c.clearRect(0,0,100,50); c.save(); c.beginPath(); c.rect(10,10,10,10); c.clip(); c.fillStyle='#0f0'; c.fillRect(0,0,100,50); c.restore(); c.fillRect(60,0,5,5);");
    EQ(PX(15, 15) "+'|'+" PX(5, 5) "+'|'+" PX(62, 2), "0,255,0,255|0,0,0,0|255,0,0,255");
    /* sombra deslocada, sem desfoque e com desfoque */
    run(&g, "c.clearRect(0,0,100,50); c.shadowColor='#00f'; c.shadowOffsetX=20; c.shadowOffsetY=0; c.fillStyle='#f00'; c.fillRect(5,5,10,10);");
    EQ(PX(10, 10) "+'|'+" PX(30, 10) "+'|'+" PX(45, 10), "255,0,0,255|0,0,255,255|0,0,0,0");
    run(&g, "c.clearRect(0,0,100,50); c.shadowBlur=6; c.fillRect(5,5,10,10); c.shadowColor='transparent';");
    const char *b = run(&g, "var s_=c.getImageData(36,10,1,1).data; (s_[2]>0 && s_[3]>0 && s_[3]<255)+''");
    CHECK(!strcmp(b, "true"), "borda desfocada da sombra: %s", run(&g, PX(36, 10)));
    /* clearRect respeita transformação e recorte */
    run(&g, "c.fillStyle='#f00'; c.fillRect(0,0,100,50); c.translate(50,0); c.clearRect(0,0,10,10); c.resetTransform();");
    EQ(PX(55, 5) "+'|'+" PX(45, 5), "0,0,0,0|255,0,0,255");
    cg_close(&g);
}

WEB_HOST_CASE(canvas_imagens_e_dados) {
    cg_t g = cg_open(PAGE, 200, 100);
    /* drawImage de outro canvas: 1:1, escalado e recortado (9 argumentos) */
    run(&g, "var dc=d.getContext('2d'); d.width=4; d.height=4; dc.fillStyle='#f00'; dc.fillRect(0,0,2,4); dc.fillStyle='#00f'; dc.fillRect(2,0,2,4);"
            "c.imageSmoothingEnabled=false; c.drawImage(d,0,0); c.drawImage(d,10,0,8,8); c.drawImage(d,2,0,2,4,30,0,10,10);");
    EQ(PX(1, 1) "+'|'+" PX(3, 1) "+'|'+" PX(13, 4) "+'|'+" PX(15, 4) "+'|'+" PX(35, 5), "255,0,0,255|0,0,255,255|255,0,0,255|0,0,255,255|0,0,255,255");
    EQ("try{c.drawImage(d)}catch(e){e instanceof TypeError}", "true");
    EQ("try{c.drawImage({},0,0)}catch(e){e instanceof TypeError}", "true");
    EQ("d.width=0; try{c.drawImage(d,0,0)}catch(e){e.name}", "InvalidStateError");
    /* desenhar o próprio canvas */
    run(&g, "c.clearRect(0,0,100,50); c.fillStyle='#0f0'; c.fillRect(0,0,5,5); c.drawImage(cv,10,0);");
    EQ(PX(12, 2), "0,255,0,255");
    /* ImageData: ida e volta com alfa (sem pré-multiplicação) */
    EQ("var im=c.createImageData(2,1); im.data.set([10,20,30,128, 200,100,50,255]); c.putImageData(im,40,40);"
       "Array.from(c.getImageData(40,40,2,1).data).join()", "10,20,30,128,200,100,50,255");
    EQ("var im2=new ImageData(3,2); im2.width+'x'+im2.height+' '+im2.data.length+' '+(im2.data instanceof Uint8ClampedArray)+' '+im2.colorSpace", "3x2 24 true srgb");
    EQ("new ImageData(new Uint8ClampedArray(16),2).height", "2");
    EQ("try{new ImageData(0,1)}catch(e){e.name}", "IndexSizeError");
    EQ("try{c.getImageData(0,0,0,1)}catch(e){e.name}", "IndexSizeError");
    EQ("c.getImageData(98,48,4,4).data.length+','+c.getImageData(-1,-1,1,1).data[3]", "64,0");
    /* putImageData com retângulo sujo, sem composição */
    EQ("var z=new ImageData(2,2); z.data.fill(255); c.putImageData(z,0,20,1,1,1,1);" PX(0, 20) "+'|'+" PX(1, 21), "0,0,0,0|255,255,255,255");
    /* toDataURL: PNG lido de volta pelo decodificador do motor */
    run(&g, "c.clearRect(0,0,100,50); c.fillStyle='rgba(0,128,255,0.5)'; c.fillRect(0,0,30,30); c.fillStyle='#123456'; c.fillRect(50,10,20,20);");
    const char *url = run(&g, "cv.toDataURL()");
    CHECK(!strncmp(url, "data:image/png;base64,", 22), "toDataURL: %.40s", url);
    static const char b64c[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    usize n = strlen(url + 22), k = 0;
    u8 *bin = kmalloc(n);
    u32 acc = 0;
    int nb = 0;
    for (usize i = 0; i < n; i++) {
        const char *q = strchr(b64c, url[22 + i]);
        if (!q || url[22 + i] == '=') break;
        acc = acc << 6 | (u32)(q - b64c);
        nb += 6;
        if (nb >= 8) { bin[k++] = (u8)(acc >> (nb - 8)); nb -= 8; }
    }
    surface_t *dec = image_decode(bin, k);
    CHECK(dec && dec->w == 100 && dec->h == 50, "PNG decodificado: %dx%d", dec ? dec->w : 0, dec ? dec->h : 0);
    if (dec) {
        u32 p0 = dec->px[5 * dec->stride + 5], p1 = dec->px[20 * dec->stride + 60], p2 = dec->px[45 * dec->stride + 90];
        CHECK((p0 >> 24) >= 127 && (p0 >> 24) <= 128 && (p0 & 0xFF) >= 126 && (p0 & 0xFF) <= 128, "PNG semitransparente: %08x", p0);
        CHECK(p1 == 0xFF123456u, "PNG opaco: %08x", p1);
        CHECK(p2 == 0, "PNG transparente: %08x", p2);
        surface_free(dec);
    }
    kfree(bin);
    EQ("d.width=0; d.toDataURL()", "data:,");
    cg_close(&g);
}

WEB_HOST_CASE(canvas_texto) {
    cg_t g = cg_open(PAGE, 300, 100);
    run(&g, "c.font='20px sans-serif'; c.fillStyle='#000'; c.fillText('Olá',5,30);");
    int ink = atoi(run(&g, "var t_=c.getImageData(0,0,100,50).data, n_=0; for (var i=3;i<t_.length;i+=4) if (t_[i]>128) n_++; n_"));
    CHECK(ink > 60, "fillText desenhou %d pixels", ink);
    char w1[64], w2[64];
    snprintf(w1, sizeof w1, "%s", run(&g, "c.measureText('Olá').width"));
    snprintf(w2, sizeof w2, "%s", run(&g, "c.font='40px sans-serif'; c.measureText('Olá').width"));
    double a = 0, b = 0;
    for (const char *p = w1; *p; p++) if (*p == '.') break; else a = a * 10 + (*p - '0');
    for (const char *p = w2; *p; p++) if (*p == '.') break; else b = b * 10 + (*p - '0');
    CHECK(a > 20 && b >= 2 * a - 2 && b <= 2 * a + 2, "measureText proporcional ao tamanho: %s %s", w1, w2);
    EQ("var m_=c.measureText('Hg'); (m_.actualBoundingBoxAscent>20 && m_.actualBoundingBoxDescent>3 && m_.fontBoundingBoxAscent>=m_.actualBoundingBoxAscent && m_ instanceof TextMetrics)+''", "true");
    EQ("c.textAlign='center'; var m2=c.measureText('ab'); (Math.abs(m2.actualBoundingBoxLeft-m2.actualBoundingBoxRight)<4)+''", "true");
    EQ("c.textAlign='start'; c.textBaseline='top'; (c.measureText('A').actualBoundingBoxAscent<2)+''", "true");
    /* textBaseline top: o texto fica abaixo de y */
    run(&g, "c.clearRect(0,0,100,50); c.font='20px sans-serif'; c.fillText('H',10,5);");
    EQ("var t2=c.getImageData(0,0,40,5).data, a2=0; for (var i=3;i<t2.length;i+=4) a2+=t2[i]; a2", "0");
    /* maxWidth comprime; strokeText desenha só o contorno */
    run(&g, "c.textBaseline='alphabetic'; c.clearRect(0,0,100,50); c.fillText('MMMMMMMM',0,30,20);");
    EQ("var t3=c.getImageData(25,0,75,50).data, a3=0; for (var i=3;i<t3.length;i+=4) a3+=t3[i]; a3", "0");
    run(&g, "c.clearRect(0,0,100,50); c.font='40px sans-serif'; c.lineWidth=1; c.strokeStyle='#f00'; c.strokeText('O',10,40);");
    ink = atoi(run(&g, "var t4=c.getImageData(0,0,100,50).data, n4=0; for (var i=0;i<t4.length;i+=4) if (t4[i+3]>100 && t4[i]>200) n4++; n4"));
    CHECK(ink > 20, "strokeText desenhou %d pixels", ink);
    /* texto girado continua desenhando */
    run(&g, "c.clearRect(0,0,100,50); c.translate(50,25); c.rotate(Math.PI/2); c.fillStyle='#000'; c.fillText('ABC',-15,5); c.resetTransform();");
    ink = atoi(run(&g, "var t5=c.getImageData(0,0,100,50).data, n5=0; for (var i=3;i<t5.length;i+=4) if (t5[i]>128) n5++; n5"));
    CHECK(ink > 30, "texto girado: %d pixels", ink);
    cg_close(&g);
}

WEB_HOST_CASE(canvas_lixo) {
    /* chamadas inválidas não podem derrubar o motor */
    cg_t g = cg_open(PAGE, 200, 100);
    run(&g, "try{CanvasRenderingContext2D.prototype.fillRect.call({},0,0,1,1)}catch(e){}"
            "try{c.fill(new Path2D(),'x')}catch(e){} c.drawImage(cv,1e30,-1e30,1e30,1e30);"
            "c.setTransform(0,0,0,0,0,0); c.fillRect(0,0,10,10); c.stroke(); c.fillText('x',0,0); c.resetTransform();"
            "c.lineWidth=1e6; c.beginPath(); c.moveTo(0,0); c.lineTo(10,10); c.stroke();"
            "c.shadowBlur=1e4; c.shadowColor='red'; c.fillRect(0,0,2,2); c.shadowColor='transparent';"
            "for (var i=0;i<2000;i++) c.save(); for (var i=0;i<3000;i++) c.restore();"
            "c.putImageData(new ImageData(5,5),-1e9,1e9); c.getImageData(-1e6,-1e6,3,3);"
            "var p=new Path2D('M 1 1 L x'); p.addPath(p); c.fill(p); new Path2D(p);");
    EQ("try{new CanvasRenderingContext2D()}catch(e){e instanceof TypeError}", "true");
    EQ("typeof c.fillRect", "function");
    cg_close(&g);
}

WEB_HOST_CASE(canvas_filter) {
    cg_t g = cg_open(PAGE, 400, 300);
    EQ("c.filter", "none");
    EQ("c.filter='grayscale(100%) blur(2px)'; c.filter", "grayscale(100%) blur(2px)");
    EQ("c.filter='url(#x)'; c.filter", "grayscale(100%) blur(2px)");          /* inválido: não muda */
    EQ("c.filter='blur(-1px)'; c.filter+'|'; c.filter='nada(1)'; c.filter", "grayscale(100%) blur(2px)");
    EQ("c.save(); c.filter='invert(1)'; c.restore(); c.filter", "grayscale(100%) blur(2px)");
    /* cor: grayscale do vermelho puro (luminância 0.2126) */
    EQ("c.reset(); c.filter='grayscale(1)'; c.fillStyle='#f00'; c.fillRect(0,0,10,10); " PX(5, 5), "54,54,54,255");
    EQ("c.reset(); c.filter='invert(100%)'; c.fillStyle='#fff'; c.fillRect(0,0,10,10); " PX(5, 5), "0,0,0,255");
    EQ("c.reset(); c.filter='opacity(0.5)'; c.fillStyle='#00f'; c.fillRect(0,0,10,10); " PX(5, 5), "0,0,255,128");
    EQ("c.reset(); c.filter='brightness(0.5)'; c.fillStyle='#fff'; c.fillRect(0,0,10,10); " PX(5, 5), "128,128,128,255");
    EQ("c.reset(); c.filter='sepia(1)'; c.fillStyle='#fff'; c.fillRect(0,0,10,10); " PX(5, 5), "255,255,239,255");
    EQ("c.reset(); c.filter='hue-rotate(180deg)'; c.fillStyle='#f00'; c.fillRect(0,0,10,10); var p=c.getImageData(5,5,1,1).data; (p[0]<20 && p[1]>100 && p[2]>100)+''", "true");
    /* desfoque: borda suavizada, centro intacto, vaza para fora do retângulo */
    EQ("c.reset(); c.filter='blur(3px)'; c.fillStyle='#000'; c.fillRect(20,10,30,30); var a=c.getImageData(35,25,1,1).data[3], b=c.getImageData(20,25,1,1).data[3], o=c.getImageData(17,25,1,1).data[3]; [a, b>90&&b<170, o>0&&o<90].join()",
       "255,true,true");
    /* drop-shadow: sombra azul deslocada por baixo da forma */
    EQ("c.reset(); c.filter='drop-shadow(10px 5px 0 blue)'; c.fillStyle='red'; c.fillRect(10,10,20,20); [" PX(15, 15) "," PX(35, 25) "," PX(45, 45) "].join('|')",
       "255,0,0,255|0,0,255,255|0,0,0,0");
    EQ("c.reset(); c.filter='drop-shadow(rgb(0, 128, 0) 4px 4px)'; c.fillStyle='#000'; c.fillRect(0,0,10,10); " PX(12, 12), "0,128,0,255");
    /* drawImage e texto também passam pelo filtro */
    EQ("c.reset(); var o=document.createElement('canvas'); o.width=o.height=10; var oc=o.getContext('2d'); oc.fillStyle='#f00'; oc.fillRect(0,0,10,10);"
       "c.filter='grayscale(1)'; c.drawImage(o,0,0); " PX(5, 5), "54,54,54,255");
    cg_close(&g);
}

WEB_HOST_CASE(canvas_offscreen) {
    cg_t g = cg_open(PAGE, 400, 300);
    EQ("var oc=new OffscreenCanvas(20,10), ox=oc.getContext('2d'); [oc.width, oc.height, ox.canvas===oc, oc.getContext('2d')===ox, Object.prototype.toString.call(oc), oc instanceof OffscreenCanvas].join()",
       "20,10,true,true,[object OffscreenCanvas],true");
    EQ("try{ OffscreenCanvas(1,1) }catch(e){ e instanceof TypeError }", "true");
    EQ("try{ oc.getContext('xyz') }catch(e){ e instanceof TypeError }", "true");
    EQ("String(oc.getContext('webgl'))", "null");
    /* desenhar no offscreen e usar como origem no canvas da página */
    EQ("ox.fillStyle='#0f0'; ox.fillRect(0,0,20,10); c.reset(); c.drawImage(oc,5,5); " PX(10, 10) "+'|'+" PX(30, 30), "0,255,0,255|0,0,0,0");
    EQ("c.reset(); c.fillStyle=c.createPattern(oc,'repeat'); c.fillRect(0,0,50,50); " PX(45, 45), "0,255,0,255");
    /* transferToImageBitmap: ImageBitmap com o desenho e o offscreen limpo */
    EQ("var bm=oc.transferToImageBitmap(); [bm.width, bm.height, Array.from(ox.getImageData(3,3,1,1).data).join(' ')].join()", "20,10,0 0 0 0");
    EQ("c.reset(); c.drawImage(bm,0,0); " PX(3, 3), "0,255,0,255");
    /* mudar o tamanho zera o bitmap e o estado */
    EQ("ox.fillStyle='red'; oc.width=7; [oc.width, ox.fillStyle, ox.getImageData(0,0,1,1).data[3]].join()", "7,#000000,0");
    /* convertToBlob: PNG que volta pelo createImageBitmap */
    run(&g, "window.res=''; ox.fillStyle='#00f'; ox.fillRect(0,0,7,10); oc.convertToBlob().then(function(b){ res=b.type+':'+(b.size>20); return createImageBitmap(b) }).then(function(i){ res+=':'+i.width+'x'+i.height })");
    for (int i = 0; i < 10; i++) js_page_tick(g.js);
    EQ("res", "image/png:true:7x10");
    /* transferControlToOffscreen: o elemento mostra o que o offscreen desenha */
    run(&g, "var oc2=d.transferControlToOffscreen(), o2=oc2.getContext('2d'); o2.fillStyle='#f0f'; o2.fillRect(0,0,300,150);");
    cg_paint(&g);
    CHECK(spx(&g, 150, 120) == 0xFF00FF, "elemento transferido: %06x", spx(&g, 150, 120));
    EQ("try{ d.getContext('2d') }catch(e){ e.name }", "InvalidStateError");
    EQ("try{ d.transferControlToOffscreen() }catch(e){ e.name }", "InvalidStateError");
    EQ("oc2.width+'x'+oc2.height", "300x150");
    cg_close(&g);
}
