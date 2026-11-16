/* mOS — teste de host do renderizador SVG (kernel/web/svg.c): sondas de pixel por recurso, SVG inline numa página,
 * <img src=.svg> e entradas quebradas. Cores comparadas com tolerância pequena (antisserrilhado nas bordas). */
#include "kernel.h"
#include "gfx.h"
#include "web.h"
#include "tools/web_host.h"

static surface_t *R(const char *svg, int w, int h) { return svg_render(svg, (int)strlen(svg), w, h); }
static u32 P(surface_t *s, int x, int y) { return s->px[(usize)y * s->stride + x]; }
static bool near(u32 a, u32 b, int tol) {
    for (int sh = 0; sh < 32; sh += 8) { int d = (int)((a >> sh) & 255) - (int)((b >> sh) & 255); if (d < -tol || d > tol) return false; }
    return true;
}
#define PX(s, x, y, cor, tol) CHECK(near(P(s, x, y), cor, tol), "(%d,%d) = %08x, esperado %08x", x, y, P(s, x, y), (u32)(cor))

WEB_HOST_CASE(svg_formas) {
    surface_t *s = R("<svg xmlns='http://www.w3.org/2000/svg' width='100' height='100'>"
                     "<rect x='10' y='10' width='30' height='20' fill='red'/>"
                     "<circle cx='70' cy='30' r='15' fill='#00ff00'/>"
                     "<ellipse cx='30' cy='70' rx='20' ry='8' fill='blue'/>"
                     "<polygon points='60,60 90,60 90,90' fill='black'/>"
                     "<line x1='0' y1='99' x2='100' y2='99' stroke='blue' stroke-width='2'/></svg>", 0, 0);
    CHECK(s && s->w == 100 && s->h == 100, "tamanho intrínseco");
    if (!s) return;
    PX(s, 20, 20, 0xFFFF0000, 0);
    PX(s, 5, 5, 0x00000000, 0);
    PX(s, 70, 30, 0xFF00FF00, 0);
    PX(s, 70, 13, 0x00000000, 0);        /* fora do círculo (r=15 → topo em 15) */
    PX(s, 30, 70, 0xFF0000FF, 0);
    PX(s, 30, 80, 0x00000000, 0);        /* elipse achatada (ry=8) */
    PX(s, 30, 77, 0xFF0000FF, 0);
    PX(s, 85, 70, 0xFF000000, 0);        /* dentro do triângulo */
    PX(s, 65, 85, 0x00000000, 0);        /* fora */
    PX(s, 50, 99, 0xFF0000FF, 0);
    /* borda do círculo antisserrilhada: alfa parcial */
    u32 e = P(s, 70, 15);
    CHECK((e >> 24) > 0 && (e >> 24) < 255, "borda AA do círculo: %08x", e);
    surface_free(s);
}

WEB_HOST_CASE(svg_path_comandos) {
    /* mesmo quadrado 20..60 por M/L/H/V/Z absolutos e relativos, C, Q e A */
    const char *ds[] = {
        "M20 20 L60 20 L60 60 L20 60 Z",
        "m20,20 h40 v40 h-40 z",
        "M20 20H60V60H20Z",
        "M20,20 60,20 60,60 20,60z",
        "M20 20 C33 20 47 20 60 20 L60 60 Q40 60 20 60 Z",
        "M20 20 L60 20 A0 0 0 0 1 60 60 L20 60 Z",
        "M2e1 20 L.6e2 20L60 60L20 6e1Z",
    };
    for (unsigned i = 0; i < ARRAY_LEN(ds); i++) {
        char svg[512];
        snprintf(svg, sizeof svg, "<svg width='80' height='80'><path d='%s' fill='#123456'/></svg>", ds[i]);
        surface_t *s = R(svg, 0, 0);
        CHECK(s != NULL, "path %u", i);
        if (!s) continue;
        int bad = 0;
        for (int y = 0; y < 80; y++) for (int x = 0; x < 80; x++) {
            bool in = x >= 20 && x < 60 && y >= 20 && y < 60;
            if (P(s, x, y) != (in ? 0xFF123456u : 0)) bad++;
        }
        CHECK(bad == 0, "path %u (%s): %d pixels errados", i, ds[i], bad);
        surface_free(s);
    }
    /* arco: semicírculo */
    surface_t *s = R("<svg width='100' height='60'><path d='M10 50 A40 40 0 0 1 90 50 Z' fill='red'/></svg>", 0, 0);
    PX(s, 50, 30, 0xFFFF0000, 0);
    PX(s, 50, 12, 0xFFFF0000, 0);
    PX(s, 50, 8, 0x00000000, 0);
    PX(s, 15, 15, 0x00000000, 0);
    surface_free(s);
}

WEB_HOST_CASE(svg_regras_e_traco) {
    /* evenodd: buraco no meio; nonzero: cheio */
    surface_t *s = R("<svg width='100' height='50'>"
                     "<path fill-rule='evenodd' d='M0 0H40V40H0Z M10 10H30V30H10Z' fill='black'/>"
                     "<path d='M50 0H90V40H50Z M60 10H80V30H60Z' fill='black'/></svg>", 0, 0);
    PX(s, 5, 5, 0xFF000000, 0);
    PX(s, 20, 20, 0x00000000, 0);
    PX(s, 70, 20, 0xFF000000, 0);
    surface_free(s);
    /* traço: largura, junção miter, tampas e tracejado */
    s = R("<svg width='120' height='120'>"
          "<polyline points='20,20 100,20 100,100' fill='none' stroke='red' stroke-width='10'/>"
          "<line x1='10' y1='60' x2='60' y2='60' stroke='blue' stroke-width='10' stroke-linecap='square'/>"
          "<line x1='10' y1='90' x2='80' y2='90' stroke='green' stroke-width='6' stroke-dasharray='10 10'/></svg>", 0, 0);
    PX(s, 60, 20, 0xFFFF0000, 0);
    PX(s, 60, 16, 0xFFFF0000, 0);
    PX(s, 60, 26, 0x00000000, 0);
    PX(s, 103, 17, 0xFFFF0000, 0);       /* canto em miter */
    PX(s, 6, 60, 0xFF0000FF, 0);         /* tampa quadrada além do início */
    PX(s, 15, 90, 0xFF008000, 0);        /* traço */
    PX(s, 25, 90, 0x00000000, 0);        /* lacuna */
    PX(s, 35, 90, 0xFF008000, 0);
    surface_free(s);
    /* junção bevel corta o canto; round arredonda */
    s = R("<svg width='120' height='60'><polyline points='10,50 50,10 90,50' fill='none' stroke='black' stroke-width='16' stroke-linejoin='bevel'/></svg>", 0, 0);
    PX(s, 50, 1, 0x00000000, 0);
    PX(s, 50, 14, 0xFF000000, 0);
    surface_free(s);
}

WEB_HOST_CASE(svg_viewbox_transform) {
    /* viewBox 0 0 10 10 em 100x100: um rect 0..5 cobre 0..50 */
    surface_t *s = R("<svg viewBox='0 0 10 10'><rect width='5' height='5' fill='red'/></svg>", 100, 100);
    PX(s, 25, 25, 0xFFFF0000, 0);
    PX(s, 75, 75, 0x00000000, 0);
    surface_free(s);
    /* preserveAspectRatio xMidYMid meet: conteúdo quadrado centrado em 200x100 */
    s = R("<svg viewBox='0 0 10 10'><rect width='10' height='10' fill='blue'/></svg>", 200, 100);
    PX(s, 100, 50, 0xFF0000FF, 0);
    PX(s, 20, 50, 0x00000000, 0);
    surface_free(s);
    s = R("<svg viewBox='0 0 10 10' preserveAspectRatio='none'><rect width='10' height='10' fill='blue'/></svg>", 200, 100);
    PX(s, 20, 50, 0xFF0000FF, 0);
    surface_free(s);
    /* transformações */
    s = R("<svg width='100' height='100'><g transform='translate(50,50)'><rect transform='rotate(45)' x='-10' y='-10' width='20' height='20' fill='red'/></g>"
          "<rect transform='matrix(1 0 0 1 80 0) scale(2)' width='5' height='5' fill='lime'/></svg>", 0, 0);
    PX(s, 50, 37, 0xFFFF0000, 0);        /* losango: ponta de cima em 50-14 */
    PX(s, 40, 40, 0x00000000, 0);        /* canto do quadrado original virou fora */
    PX(s, 85, 5, 0xFF00FF00, 0);
    surface_free(s);
    /* intrínseco: só viewBox, width com unidade */
    float w, h;
    const char *a = "<svg viewBox='0 0 24 12'/>", *b = "<svg width='2in' viewBox='0 0 2 1'/>", *c = "<svg/>";
    CHECK(svg_intrinsic(a, (int)strlen(a), &w, &h) && w == 24 && h == 12, "intrínseco viewBox: %g %g", w, h);
    CHECK(svg_intrinsic(b, (int)strlen(b), &w, &h) && w == 192 && h == 96, "intrínseco in: %g %g", w, h);
    CHECK(svg_intrinsic(c, (int)strlen(c), &w, &h) && w == 300 && h == 150, "intrínseco padrão: %g %g", w, h);
}

WEB_HOST_CASE(svg_gradientes) {
    surface_t *s = R("<svg width='100' height='40'><defs>"
                     "<linearGradient id='g'><stop offset='0' stop-color='#000'/><stop offset='1' stop-color='#fff'/></linearGradient>"
                     "<linearGradient id='h' href='#g' x1='0' y1='0' x2='0' y2='1'/>"
                     "<radialGradient id='r'><stop offset='0%' stop-color='red'/><stop offset='100%' stop-color='blue' stop-opacity='0'/></radialGradient>"
                     "</defs><rect width='100' height='20' fill='url(#g)'/><rect x='0' y='20' width='40' height='20' fill='url(#h)'/>"
                     "<circle cx='80' cy='30' r='10' fill='url(#r)'/></svg>", 0, 0);
    u32 l = P(s, 1, 10), m = P(s, 50, 10), r = P(s, 98, 10);
    CHECK((l & 255) < 10 && (r & 255) > 245 && (m & 255) > 110 && (m & 255) < 145, "linear: %08x %08x %08x", l, m, r);
    u32 t = P(s, 20, 21), b = P(s, 20, 38);
    CHECK((t & 255) < 30 && (b & 255) > 225, "vertical via href: %08x %08x", t, b);
    u32 cc = P(s, 80, 30);
    CHECK((cc >> 24) > 225 && ((cc >> 16) & 255) > 200 && (cc & 255) < 40, "radial no centro: %08x", cc);
    u32 o = P(s, 80, 39);
    CHECK((o >> 24) < 60, "radial transparente na borda: %08x", o);
    surface_free(s);
}

WEB_HOST_CASE(svg_estilo_use_clip) {
    surface_t *s = R("<svg width='100' height='100'><style>.a{fill:#00f} #b { fill: red } rect.c{fill:lime}</style>"
                     "<defs><symbol id='sq' viewBox='0 0 2 2'><rect width='2' height='2'/></symbol>"
                     "<clipPath id='cp'><circle cx='75' cy='75' r='10'/></clipPath></defs>"
                     "<rect class='a' width='20' height='20'/><rect id='b' x='25' width='20' height='20' class='a'/>"
                     "<rect class='c' x='50' width='20' height='20'/>"
                     "<g fill='orange'><use href='#sq' x='0' y='30' width='20' height='20'/></g>"
                     "<rect x='50' y='50' width='50' height='50' fill='black' clip-path='url(#cp)'/>"
                     "<g opacity='0.5'><rect x='0' y='60' width='20' height='20' fill='red'/><rect x='10' y='60' width='20' height='20' fill='red'/></g>"
                     "<g style='display:none'><rect width='100' height='100' fill='red'/></g>"
                     "<rect x='30' y='30' width='10' height='10' fill='currentColor' color='#ff00ff'/>"
                     "</svg>", 0, 0);
    PX(s, 10, 10, 0xFF0000FF, 0);
    PX(s, 35, 10, 0xFFFF0000, 0);        /* #id vence .classe */
    PX(s, 60, 10, 0xFF00FF00, 0);
    PX(s, 10, 40, 0xFFFFA500, 0);        /* symbol via use herda fill */
    PX(s, 75, 75, 0xFF000000, 0);        /* dentro do recorte */
    PX(s, 55, 55, 0x00000000, 0);        /* fora do recorte */
    /* opacidade de grupo: a sobreposição não escurece (camada) */
    u32 a = P(s, 5, 70), b = P(s, 15, 70);
    CHECK(near(a, b, 2) && (a >> 24) > 120 && (a >> 24) < 136, "camada de grupo: %08x %08x", a, b);
    PX(s, 35, 35, 0xFFFF00FF, 0);
    PX(s, 90, 10, 0x00000000, 0);        /* display:none */
    surface_free(s);
}

WEB_HOST_CASE(svg_texto_imagem) {
    surface_t *s = R("<svg width='200' height='40'><text x='10' y='30' font-size='24' fill='black'>Olá SVG</text></svg>", 0, 0);
    int ink = 0;
    for (int y = 0; y < 40; y++) for (int x = 0; x < 200; x++) if (P(s, x, y) >> 24) ink++;
    CHECK(ink > 100, "texto desenhou %d pixels", ink);
    surface_free(s);
    /* <image> com PNG em data: (1x1 vermelho) esticado para 10x10 */
    s = R("<svg width='20' height='20'><image x='5' y='5' width='10' height='10' href='data:image/png;base64,"
          "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8DwHwAFBQIAX8jx0gAAAABJRU5ErkJggg=='/></svg>", 0, 0);
    PX(s, 10, 10, 0xFFFF0000, 2);
    PX(s, 2, 2, 0x00000000, 0);
    surface_free(s);
}

WEB_HOST_CASE(svg_na_pagina) {
    const char *html = "<html><body style='margin:0'><div style='color:#ff0000'><svg width='12' height='4'><rect width='12' height='4' fill='currentColor'/></svg></div>"
                       "<div><svg width='30' height='10'><rect width='30' height='10' fill='#00ff00'/></svg></div>"
                       "<img src='svg/icone.svg' style='display:block'><img src='svg/icone.svg' width='80' style='display:block'></body></html>";
    doc_t *d = doc_parse(html, strlen(html), "http://teste/p.html");
    doc_layout(d, 300, 200, 1.0f);
    for (img_t *im = d->images; im; im = im->next) {
        if (im->state != IMG_PENDING) continue;
        http_req_t *q = http_get(im->url);
        if (q && q->status == 200) img_set_data(im, q->body, q->body_len);
        im->state = im->surf ? IMG_OK : IMG_ERROR;
        http_free(q);
    }
    d->need_layout = true;
    doc_layout(d, 300, 200, 1.0f);
    surface_t *s = surface_new(300, 200);
    gfx_fill(s, 0, 0, 300, 200, RGB(255, 255, 255));
    ui_t u;
    memset(&u, 0, sizeof u);
    u.s = s;
    doc_paint(d, &u, rect_make(0, 0, 300, 200), 0, NULL);
    if (getenv("WEB_HOST_VERBOSE")) for (img_t *im = d->images; im; im = im->next) host_printf("  img %s estado %d surf %p svg %d\n", im->url, im->state, (void *)im->surf, im->svglen);
    /* currentColor herdado da cor CSS da página */
    int red = 0;
    for (int y = 0; y < 30; y++) for (int x = 0; x < 60; x++) if (P(s, x, y) == 0xFFFF0000u) red++;
    CHECK(red == 48, "currentColor do svg inline: %d pixels vermelhos", red);
    /* inline: na linha de base do texto (acima dele fica a entrelinha), 30x10 verde */
    int yi = -1;
    for (int y = 0; y < 60 && yi < 0; y++) if (P(s, 5, y) == 0xFF00FF00u) yi = y;
    CHECK(yi >= 0 && yi < 40, "svg inline não apareceu (y=%d)", yi);
    if (yi >= 0) { PX(s, 29, yi + 9, 0xFF00FF00, 0); PX(s, 31, yi + 5, 0xFFFFFFFF, 0); }
    /* acha a primeira linha azul do <img> 40x20 e confere as duas metades */
    int y0 = -1;
    for (int y = 0; y < 200 && y0 < 0; y++) if ((P(s, 5, y) & 0xFFFFFF) == 0x0000FF) y0 = y;
    CHECK(y0 >= 10, "img svg não apareceu (y0=%d)", y0);
    if (y0 >= 0) {
        PX(s, 5, y0 + 5, 0xFF0000FF, 0);
        PX(s, 30, y0 + 5, 0xFF00FF00, 0);
        PX(s, 50, y0 + 5, 0xFFFFFFFF, 0);
        /* o segundo, width=80: redesenhado nítido em 80x40 */
        PX(s, 39, y0 + 20 + 20, 0xFF0000FF, 0);
        PX(s, 41, y0 + 20 + 20, 0xFF00FF00, 0);
    }
    const char *dir = getenv("WEB_HOST_OUT");
    if (dir) { char p[512]; snprintf(p, sizeof p, "%s/svg_pagina.ppm", dir); extern int host_write_ppm(const char *, const u32 *, int, int, int); host_write_ppm(p, s->px, s->w, s->h, s->stride); }
    surface_free(s);
    doc_free(d);
}

WEB_HOST_CASE(svg_sprite_da_pagina) {
    const char *html = "<html><body style='margin:0'><svg style='display:none'><symbol id='ic' viewBox='0 0 2 2'><rect width='2' height='2' fill='#0000ff'/></symbol></svg>"
                       "<div style='line-height:0'><svg width='20' height='20'><use href='#ic'/></svg></div></body></html>";
    doc_t *d = doc_parse(html, strlen(html), "http://teste/p.html");
    doc_layout(d, 100, 100, 1.0f);
    surface_t *s = surface_new(100, 100);
    gfx_fill(s, 0, 0, 100, 100, RGB(255, 255, 255));
    ui_t u;
    memset(&u, 0, sizeof u);
    u.s = s;
    doc_paint(d, &u, rect_make(0, 0, 100, 100), 0, NULL);
    int blue = 0;
    for (int y = 0; y < 100; y++) for (int x = 0; x < 100; x++) if (P(s, x, y) == 0xFF0000FFu) blue++;
    CHECK(blue == 400, "símbolo de outro <svg> da página: %d pixels azuis", blue);
    surface_free(s);
    doc_free(d);
}

WEB_HOST_CASE(svg_lixo) {
    const char *casos[] = {
        "", "<svg", "<svg><path d='M 10 10 L'/>", "<svg><path d='M1e999 1e999 L-1e999 0Z' fill='red'/></svg>",
        "<svg width='99999' height='99999'/>", "<svg viewBox='0 0 0 0'><rect width='1' height='1'/></svg>",
        "<svg><use href='#a' id='a'/></svg>", "<svg><g id='x'><use href='#x'/></g></svg>",
        "<svg><linearGradient id='g' href='#g'/><rect width='5' height='5' fill='url(#g)'/></svg>",
        "<svg><rect width='10' height='10' stroke='red' stroke-width='1e30' stroke-dasharray='0.0001'/></svg>",
        "<svg><circle r='-5'/><ellipse rx='1e-30' ry='4'/><polygon points='1'/></svg>",
        "<svg><text>&#xFFFFFFFF;&#0;&lt;</text></svg>", "<svg><![CDATA[ x", "<svg><!-- nunca fecha",
    };
    for (unsigned i = 0; i < ARRAY_LEN(casos); i++) { surface_t *s = R(casos[i], 0, 0); surface_free(s); }
    /* bytes aleatórios dentro de um svg */
    u32 seed = 5;
    for (int k = 0; k < 60; k++) {
        char b[300] = "<svg width='50' height='50'><path d='";
        int n = (int)strlen(b);
        for (int i = 0; i < 200; i++) { seed = seed * 1103515245 + 12345; b[n++] = "MLHVCSQTAZmlhvcsqtaz0123456789.-, e"[(seed >> 16) % 36]; }
        strcpy(b + n, "' fill='red' stroke='blue' stroke-dasharray='3 1'/></svg>");
        surface_t *s = R(b, 0, 0);
        surface_free(s);
    }
    CHECK(true, "sobreviveu");
}
