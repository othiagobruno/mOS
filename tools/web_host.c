/* mOS — teste de host do motor web (kernel/web): HTML → estilo → layout → pintura, JavaScript e imagens.
 * Compilado pelo tools/web_host.sh com os headers do kernel; o shim (web_host_shim.c) liga o resto à libc. */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "js.h"

int  host_printf(const char *fmt, ...);
u8  *host_read_file(const char *path, usize *n);
int  host_write_ppm(const char *path, const u32 *px, int w, int h, int stride);
void exit(int);
char *getenv(const char *);

static int ok, fail;
#define CHECK(c, ...) do { if (c) ok++; else { fail++; host_printf("FALHOU %s:%d: ", __FILE__, __LINE__); host_printf(__VA_ARGS__); host_printf("\n"); } } while (0)

/* documento pintado num surface w×h */
typedef struct { doc_t *d; js_page_t *js; surface_t *s; } page_t;

static void page_paint(page_t *p, int w, int h) {
    doc_layout(p->d, w, h, 1.0f);
    if (!p->s) p->s = surface_new(w, h);
    ui_t u;
    memset(&u, 0, sizeof u);
    u.s = p->s;
    u.cw = w; u.ch = h;
    gfx_fill(p->s, 0, 0, w, h, RGB(255, 255, 255));
    doc_paint(p->d, &u, rect_make(0, 0, w, h), 0, NULL);
}

static page_t page_load(const char *html, const char *url, bool js, int w, int h) {
    page_t p = { 0 };
    p.d = doc_parse(html, strlen(html), url);
    if (js) {
        p.js = js_page_new2(p.d, url, w, h, NULL, NULL);
        for (int i = 0; i < 200 && js_page_busy(p.js); i++) js_page_tick(p.js);
        js_page_tick(p.js);
    }
    page_paint(&p, w, h);
    return p;
}

static void page_free(page_t *p) {
    if (p->js) js_page_free(p->js);
    if (p->d) doc_free(p->d);
    if (p->s) surface_free(p->s);
}

static u32 px_at(surface_t *s, int x, int y) { return s->px[y * s->stride + x] & 0xFFFFFF; }

static void dump(page_t *p, const char *name) {
    const char *dir = getenv("WEB_HOST_OUT");
    if (!dir) return;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    host_write_ppm(path, p->s->px, p->s->w, p->s->h, p->s->stride);
}

static const char *eval(page_t *p, const char *code, int *type) {
    static sbuf_t out;
    if (out.p) sb_free(&out);
    memset(&out, 0, sizeof out);
    *type = js_page_eval_value(p->js, code, strlen(code), &out);
    return out.p ? out.p : "";
}

/* ------------------------------------------------------------ casos */
static void t_basico(void) {
    page_t p = page_load("<html><body style='margin:0'><div style='background:#ff0000;width:50px;height:20px'></div>"
                         "<p>Olá mundo</p></body></html>", "http://teste/a.html", false, 200, 100);
    CHECK(px_at(p.s, 10, 10) == 0xFF0000, "div vermelha: %06x", px_at(p.s, 10, 10));
    CHECK(px_at(p.s, 100, 10) == 0xFFFFFF, "fora da div: %06x", px_at(p.s, 100, 10));
    dump(&p, "basico");
    page_free(&p);
}

static void t_js(void) {
    page_t p = page_load("<html><body><div id=x>a</div><script>document.getElementById('x').textContent='b'+(1+2);"
                         "var g = [1,2,3].map(function(v){return v*2}).join(',');</script></body></html>",
                         "http://teste/b.html", true, 200, 100);
    int t;
    const char *r = eval(&p, "document.getElementById('x').textContent", &t);
    CHECK(t == JS_WK_STRING && !strcmp(r, "b3"), "textContent: %d %s", t, r);
    r = eval(&p, "g", &t);
    CHECK(t == JS_WK_STRING && !strcmp(r, "2,4,6"), "map: %s", r);
    r = eval(&p, "({a:1,b:[true,null]})", &t);
    CHECK(t == JS_WK_JSON, "objeto vira JSON: %d %s", t, r);
    page_free(&p);
}

extern void web_host_extra_tests(int *ok, int *fail);   /* testes das partes novas (web_host_novos.c) */
__attribute__((weak)) void web_host_extra_tests(int *o, int *f) { (void)o; (void)f; }

int main(void) {
    font_init();
    t_basico();
    t_js();
    web_host_extra_tests(&ok, &fail);
    host_printf("web_host: %d/%d ok\n", ok, ok + fail);
    return fail ? 1 : 0;
}
