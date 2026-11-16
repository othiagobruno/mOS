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

#define SVGH "<svg xmlns='http://www.w3.org/2000/svg' width='20' height='20'>"

static int ink(surface_t *s, int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) if ((P(s, x, y) >> 24) > 100) n++;
    return n;
}

WEB_HOST_CASE(svg_padrao_mascara) {
    surface_t *s = R(SVGH "<defs><pattern id='p' width='10' height='10' patternUnits='userSpaceOnUse'><rect width='5' height='5' fill='red'/></pattern></defs>"
                     "<rect width='20' height='20' fill='url(#p)'/></svg>", 20, 20);
    PX(s, 2, 2, 0xFFFF0000u, 2); PX(s, 7, 2, 0, 2); PX(s, 12, 2, 0xFFFF0000u, 2); PX(s, 12, 12, 0xFFFF0000u, 2); PX(s, 2, 7, 0, 2);
    surface_free(s);
    /* padrão em unidades da caixa: 2×2 ladrilhos sobre o retângulo, conteúdo pela viewBox */
    s = R(SVGH "<pattern id='q' width='0.5' height='0.5' viewBox='0 0 2 2'><rect width='1' height='1' fill='blue'/></pattern>"
          "<rect x='0' y='0' width='20' height='20' fill='url(#q)'/></svg>", 20, 20);
    PX(s, 2, 2, 0xFF0000FFu, 2); PX(s, 7, 7, 0, 2); PX(s, 12, 2, 0xFF0000FFu, 2);
    surface_free(s);
    /* máscara: metade esquerda branca, e cinza 50% → alfa ~128 */
    s = R(SVGH "<mask id='m'><rect width='10' height='20' fill='white'/><rect x='10' width='10' height='10' fill='#808080'/></mask>"
          "<rect width='20' height='20' fill='blue' mask='url(#m)'/></svg>", 20, 20);
    PX(s, 5, 10, 0xFF0000FFu, 2); PX(s, 15, 15, 0, 2);
    u32 c = P(s, 15, 5);
    CHECK((c >> 24) > 100 && (c >> 24) < 150, "máscara cinza: %08x", c);
    surface_free(s);
    /* região da máscara (padrão −10%..120%) recorta conteúdo fora dela; mask-type alpha */
    s = R(SVGH "<mask id='a' style='mask-type:alpha'><rect width='20' height='20' fill='black'/></mask>"
          "<rect x='2' y='2' width='10' height='10' fill='red' mask='url(#a)'/></svg>", 20, 20);
    PX(s, 6, 6, 0xFFFF0000u, 2);
    surface_free(s);
}

WEB_HOST_CASE(svg_filtros) {
    surface_t *s = R(SVGH "<filter id='o' filterUnits='userSpaceOnUse' x='0' y='0' width='20' height='20'><feOffset dx='10' dy='0'/></filter>"
                     "<rect width='5' height='5' fill='red' filter='url(#o)'/></svg>", 20, 20);
    PX(s, 2, 2, 0, 2); PX(s, 12, 2, 0xFFFF0000u, 2);
    surface_free(s);
    s = R(SVGH "<filter id='b' filterUnits='userSpaceOnUse' x='0' y='0' width='20' height='20'><feGaussianBlur stdDeviation='2'/></filter>"
          "<rect x='5' y='5' width='10' height='10' fill='black' filter='url(#b)'/></svg>", 20, 20);
    u32 e = P(s, 5, 10), m = P(s, 10, 10);
    CHECK((e >> 24) > 40 && (e >> 24) < 200 && (m >> 24) > 230, "blur: borda %08x meio %08x", e, m);
    surface_free(s);
    s = R(SVGH "<filter id='g'><feColorMatrix type='saturate' values='0'/></filter><rect width='20' height='20' fill='red' filter='url(#g)'/></svg>", 20, 20);
    u32 g = P(s, 10, 10);
    CHECK(((g >> 16) & 255) == ((g >> 8) & 255) && ((g >> 8) & 255) == (g & 255) && (g >> 24) == 255, "saturate 0 vira cinza: %08x", g);
    surface_free(s);
    /* flood + composite in + merge: sombra verde atrás do original */
    s = R(SVGH "<filter id='f' filterUnits='userSpaceOnUse' x='0' y='0' width='20' height='20'><feFlood flood-color='lime' result='cor'/>"
          "<feComposite in='cor' in2='SourceAlpha' operator='in' result='som'/><feOffset in='som' dx='5' dy='5' result='desl'/>"
          "<feMerge><feMergeNode in='desl'/><feMergeNode in='SourceGraphic'/></feMerge></filter>"
          "<rect width='10' height='10' fill='red' filter='url(#f)'/></svg>", 20, 20);
    PX(s, 2, 2, 0xFFFF0000u, 2); PX(s, 12, 12, 0xFF00FF00u, 2); PX(s, 17, 17, 0, 2);
    surface_free(s);
    s = R(SVGH "<filter id='d' x='-1' y='-1' width='3' height='3'><feDropShadow dx='3' dy='3' stdDeviation='0' flood-color='blue'/></filter><rect x='2' y='2' width='8' height='8' fill='red' filter='url(#d)'/></svg>", 20, 20);
    PX(s, 5, 5, 0xFFFF0000u, 2); PX(s, 11, 11, 0xFF0000FFu, 2);
    surface_free(s);
}

WEB_HOST_CASE(svg_marcadores_texto) {
    surface_t *s = R(SVGH "<marker id='q' markerWidth='4' markerHeight='4' refX='2' refY='2' markerUnits='userSpaceOnUse'><rect width='4' height='4' fill='red'/></marker>"
                     "<polyline points='2,10 10,10 18,10' stroke='black' stroke-width='1' fill='none' marker-start='url(#q)' marker-mid='url(#q)' marker-end='url(#q)'/></svg>", 20, 20);
    PX(s, 1, 8, 0xFFFF0000u, 2); PX(s, 10, 8, 0xFFFF0000u, 2); PX(s, 19, 8, 0xFFFF0000u, 2); PX(s, 5, 5, 0, 2);
    surface_free(s);
    /* orient=auto numa linha vertical: o marcador triangular aponta para baixo */
    s = R(SVGH "<marker id='t' markerWidth='6' markerHeight='6' refX='0' refY='3' orient='auto' markerUnits='userSpaceOnUse'><path d='M0,0 L6,3 L0,6 Z' fill='blue'/></marker>"
          "<path d='M10,2 L10,10' stroke='black' marker-end='url(#t)'/></svg>", 20, 20);
    CHECK(ink(s, 7, 11, 14, 16) > 6, "seta para baixo desenhada: %d", ink(s, 7, 11, 14, 16));
    surface_free(s);
    /* texto com gradiente, girado por rotate e em textPath */
    surface_t *t = R("<svg xmlns='http://www.w3.org/2000/svg' width='120' height='60'><linearGradient id='lg'><stop offset='0' stop-color='red'/><stop offset='1' stop-color='blue'/></linearGradient>"
                     "<text x='5' y='30' font-size='24' fill='url(#lg)'>MMMM</text></svg>", 120, 60);
    int red = 0, blue = 0;
    for (int y = 0; y < 60; y++) for (int x = 0; x < 120; x++) { u32 c = P(t, x, y); if ((c >> 24) > 200) { if (((c >> 16) & 255) > 150) red++; if ((c & 255) > 150) blue++; } }
    CHECK(red > 10 && blue > 10, "texto com gradiente: %d vermelhos %d azuis", red, blue);
    surface_free(t);
    t = R("<svg xmlns='http://www.w3.org/2000/svg' width='60' height='60'><text x='20' y='40' font-size='30' rotate='90'>I</text></svg>", 60, 60);
    int wide = 0;
    for (int x = 0; x < 60; x++) { int col = 0; for (int y = 0; y < 60; y++) if ((P(t, x, y) >> 24) > 100) col++; if (col) wide++; }
    CHECK(wide > 12, "I girado 90° fica largo: %d colunas", wide);
    surface_free(t);
    t = R("<svg xmlns='http://www.w3.org/2000/svg' width='100' height='100'><path id='c' d='M10,90 Q50,0 90,90' fill='none'/>"
          "<text font-size='14'><textPath href='#c'>curva curva</textPath></text></svg>", 100, 100);
    CHECK(ink(t, 0, 0, 100, 100) > 40, "textPath desenhou %d pixels", ink(t, 0, 0, 100, 100));
    CHECK(ink(t, 40, 30, 60, 50) > 3, "texto acompanha a curva no topo: %d", ink(t, 40, 30, 60, 50));
    surface_free(t);
}

/* SVG inline como subárvore do DOM: atributos, caixa dos nomes, mudanças pelo JS redesenham */
#include "js.h"
static const char *svgjs(js_page_t *p, const char *code) {
    static sbuf_t o;
    if (o.p) sb_free(&o);
    memset(&o, 0, sizeof o);
    int t = js_page_eval_value(p, code, (int)strlen(code), &o);
    if (t == JS_WK_EXCEPTION) { static char b[600]; snprintf(b, sizeof b, "EXC:%s", o.p ? o.p : ""); return b; }
    return o.p ? o.p : "";
}
static u32 svg_page_px(doc_t *d, surface_t *s, int x, int y) {
    doc_layout(d, 100, 60, 1.0f);
    gfx_fill(s, 0, 0, 100, 60, RGB(255, 255, 255));
    ui_t u;
    memset(&u, 0, sizeof u);
    u.s = s;
    doc_paint(d, &u, rect_make(0, 0, 100, 60), 0, NULL);
    return P(s, x, y);
}
#define SEQ(code, want) do { const char *r_ = svgjs(js, code); CHECK(!strcmp(r_, want), "%s -> [%s], esperado [%s]", code, r_, want); } while (0)

WEB_HOST_CASE(svg_dom_js) {
    const char *html = "<html><body style='margin:0'><svg id=s width='20' height='20' viewBox='0 0 20 20' style='display:block'>"
                       "<defs><linearGradient id='lg'><stop offset='0' stop-color='#000'/></linearGradient></defs>"
                       "<rect id='r' width='10' height='10' fill='red'/><text id='t' x='0' y='19'>oi &amp; tchau</text></svg></body></html>";
    doc_t *d = doc_parse(html, strlen(html), "http://teste/sd.html");
    js_page_t *js = js_page_new2(d, "http://teste/sd.html", 100, 60, NULL, NULL);
    surface_t *s = surface_new(100, 60);
    CHECK((svg_page_px(d, s, 5, 5) & 0xFFFFFF) == 0xFF0000, "rect vermelho inicial: %08x", P(s, 5, 5));
    SEQ("var r=document.getElementById('r'); [r.tagName, r.getAttribute('fill'), r.namespaceURI, document.getElementById('lg').tagName, document.querySelector('#s rect').id].join()",
        "rect,red,http://www.w3.org/2000/svg,linearGradient,r");
    SEQ("document.getElementById('t').textContent+'|'+document.getElementById('s').getAttribute('viewBox')+'|'+document.getElementById('s').children.length", "oi & tchau|0 0 20 20|3");
    svgjs(js, "r.setAttribute('fill','blue')");
    js_page_tick(js);
    CHECK((svg_page_px(d, s, 5, 5) & 0xFFFFFF) == 0x0000FF, "setAttribute redesenhou: %08x", P(s, 5, 5));
    svgjs(js, "var c=document.createElementNS('http://www.w3.org/2000/svg','circle'); c.setAttribute('cx','15'); c.setAttribute('cy','15'); c.setAttribute('r','3'); c.setAttribute('fill','lime');"
              "document.getElementById('s').appendChild(c);");
    js_page_tick(js);
    CHECK((svg_page_px(d, s, 15, 15) & 0xFFFFFF) == 0x00FF00, "createElementNS + appendChild: %08x", P(s, 15, 15));
    SEQ("c.tagName+','+c.namespaceURI.slice(-3)+','+(c.parentNode.id)", "circle,svg,s");
    svgjs(js, "r.remove();");
    js_page_tick(js);
    CHECK((svg_page_px(d, s, 5, 5) & 0xFFFFFF) == 0xFFFFFF, "remove tirou o rect: %08x", P(s, 5, 5));
    svgjs(js, "document.getElementById('s').innerHTML=\"<rect width='20' height='5' fill='#ff00ff'/><linearGradient id='g2'/>\"");
    js_page_tick(js);
    CHECK((svg_page_px(d, s, 10, 2) & 0xFFFFFF) == 0xFF00FF, "innerHTML em SVG: %08x", P(s, 10, 2));
    SEQ("document.getElementById('g2').tagName", "linearGradient");
    js_page_free(js);
    doc_free(d);
    surface_free(s);
}

/* filtros que faltavam: sub-região, turbulência, convolução, deslocamento, iluminação, ladrilho e imagem */
WEB_HOST_CASE(svg_filtros_2) {
#define SVG40 "<svg xmlns='http://www.w3.org/2000/svg' width='40' height='40'>"
#define FU "filterUnits='userSpaceOnUse' x='0' y='0' width='40' height='40'"
    /* sub-região: o flood só pinta o quadrado pedido */
    surface_t *s = R(SVG40 "<filter id='f' " FU "><feFlood flood-color='blue' x='10' y='10' width='10' height='10'/></filter>"
                     "<rect width='40' height='40' fill='red' filter='url(#f)'/></svg>", 40, 40);
    PX(s, 15, 15, 0xFF0000FFu, 2); PX(s, 5, 5, 0, 2); PX(s, 25, 25, 0, 2);
    surface_free(s);
    /* turbulência: ruído variado, determinístico pelo seed, diferente com outro seed; fractalNoise fica perto do meio */
    const char *tb = SVG40 "<filter id='t' " FU "><feTurbulence baseFrequency='0.1' numOctaves='2' seed='%d' type='%s'/></filter>"
                     "<rect width='40' height='40' filter='url(#t)'/></svg>";
    char b1[512], b2[512], b3[512];
    snprintf(b1, sizeof b1, tb, 3, "turbulence");
    snprintf(b2, sizeof b2, tb, 3, "turbulence");
    snprintf(b3, sizeof b3, tb, 9, "fractalNoise");
    surface_t *t1 = R(b1, 40, 40), *t2 = R(b2, 40, 40), *t3 = R(b3, 40, 40);
    int distinct = 0, same = 1, diff = 0;
    for (int i = 1; i < 40 * 40; i++) {
        if (t1->px[i] != t1->px[i - 1]) distinct++;
        if (t1->px[i] != t2->px[i]) same = 0;
        if (t1->px[i] != t3->px[i]) diff++;
    }
    CHECK(distinct > 1000 && same && diff > 1000, "turbulência: distintos %d, igual %d, diferentes %d", distinct, same, diff);
    u32 fa = P(t3, 20, 20) >> 24;
    CHECK(fa > 40 && fa < 220, "fractalNoise: alfa no meio %u", fa);
    surface_free(t1); surface_free(t2); surface_free(t3);
    /* convolução identidade não muda; 3×3 de média suaviza a borda */
    s = R(SVG40 "<filter id='c' " FU "><feConvolveMatrix order='3' kernelMatrix='0 0 0 0 1 0 0 0 0'/></filter>"
          "<rect x='10' y='10' width='20' height='20' fill='red' filter='url(#c)'/></svg>", 40, 40);
    PX(s, 15, 15, 0xFFFF0000u, 1); PX(s, 5, 5, 0, 1);
    surface_free(s);
    s = R(SVG40 "<filter id='c' " FU "><feConvolveMatrix order='3' kernelMatrix='1 1 1 1 1 1 1 1 1'/></filter>"
          "<rect x='10' y='10' width='20' height='20' fill='black' filter='url(#c)'/></svg>", 40, 40);
    u32 ce = P(s, 10, 20) >> 24, cm = P(s, 20, 20) >> 24;
    CHECK(ce > 120 && ce < 220 && cm == 255, "convolução de média: borda %u meio %u", ce, cm);
    surface_free(s);
    /* deslocamento: mapa cinza neutro não move; mapa com R=1 move scale/2 para a direita */
    s = R(SVG40 "<filter id='d' " FU "><feFlood flood-color='rgb(255,128,128)' result='m'/><feDisplacementMap in='SourceGraphic' in2='m' scale='20' xChannelSelector='R' yChannelSelector='G'/></filter>"
          "<rect x='10' y='10' width='10' height='10' fill='red' filter='url(#d)'/></svg>", 40, 40);
    PX(s, 15, 15, 0, 2); PX(s, 5, 15, 0xFFFF0000u, 2);
    surface_free(s);
    /* iluminação difusa de cima (elevação 90): superfície plana fica com a cor da luz */
    s = R(SVG40 "<filter id='l' " FU "><feDiffuseLighting lighting-color='#ff8000' surfaceScale='1' diffuseConstant='1'><feDistantLight azimuth='0' elevation='90'/></feDiffuseLighting></filter>"
          "<rect width='40' height='40' fill='black' filter='url(#l)'/></svg>", 40, 40);
    PX(s, 20, 20, 0xFFFF8000u, 2);
    surface_free(s);
    /* especular com luz pontual alta: brilho máximo no centro, menor na borda */
    s = R(SVG40 "<filter id='l' " FU "><feSpecularLighting specularExponent='20' lighting-color='white'><fePointLight x='20' y='20' z='30'/></feSpecularLighting></filter>"
          "<rect width='40' height='40' fill='black' filter='url(#l)'/></svg>", 40, 40);
    u32 sc = P(s, 20, 20) >> 24, sb = P(s, 2, 2) >> 24;
    CHECK(sc > 200 && sb < sc, "especular: centro %u borda %u", sc, sb);
    surface_free(s);
    /* ladrilho: quadrado 10×10 (verde em cima à esquerda) repetido */
    s = R(SVG40 "<filter id='t' " FU "><feFlood flood-color='lime' x='0' y='0' width='5' height='5' result='a'/>"
          "<feOffset in='a' dx='0' dy='0' x='0' y='0' width='10' height='10' result='b'/><feTile in='b'/></filter>"
          "<rect width='40' height='40' filter='url(#t)'/></svg>", 40, 40);
    PX(s, 2, 2, 0xFF00FF00u, 2); PX(s, 22, 32, 0xFF00FF00u, 2); PX(s, 7, 7, 0, 2); PX(s, 27, 17, 0, 2);
    surface_free(s);
    /* imagem data: (PNG azul 2×2) ocupando a sub-região, e elemento por #id */
    s = R(SVG40 "<filter id='i' " FU "><feImage x='10' y='10' width='20' height='20' preserveAspectRatio='none' "
          "href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAEElEQVR4nGNgYPj/H4KhDAA/0gf5tBJPzQAAAABJRU5ErkJggg=='/></filter>"
          "<rect width='40' height='40' fill='red' filter='url(#i)'/></svg>", 40, 40);
    PX(s, 20, 20, 0xFF0000FFu, 2); PX(s, 5, 5, 0, 2);
    surface_free(s);
    s = R(SVG40 "<defs><circle id='bola' cx='20' cy='20' r='8' fill='lime'/></defs><filter id='i' " FU "><feImage href='#bola'/></filter>"
          "<rect width='40' height='40' fill='red' filter='url(#i)'/></svg>", 40, 40);
    PX(s, 20, 20, 0xFF00FF00u, 2); PX(s, 3, 3, 0, 2);
    surface_free(s);
#undef FU
#undef SVG40
}
