/* mOS — geometria do hosting SwiftUI (rolagem, recorte, popover, vidro de fallback).
 * Funções puras, sem libc: o libSystem exporta os mesmos cálculos e o teste do host
 * (userland/tests-swiftui) roda a cena inteira. O vidro NÃO amostra o fundo. */
#ifndef MOS_SUI_GEOM_H
#define MOS_SUI_GEOM_H

static inline double mos_sui_clamp_offset_inline(double offset, double delta, double viewport, double content) {
    double max_off = content - viewport;
    double n;
    if (max_off < 0) max_off = 0;
    n = offset + delta;
    if (n < 0) n = 0;
    if (n > max_off) n = max_off;
    return n;
}

/* Interseção de dois retângulos. Devolve 1 se a área é positiva. */
static inline int mos_sui_intersect_inline(
    double ax, double ay, double aw, double ah,
    double bx, double by, double bw, double bh,
    double *x, double *y, double *w, double *h) {
    double x1 = ax > bx ? ax : bx;
    double y1 = ay > by ? ay : by;
    double x2 = (ax + aw) < (bx + bw) ? (ax + aw) : (bx + bw);
    double y2 = (ay + ah) < (by + bh) ? (ay + ah) : (by + bh);
    if (aw <= 0 || ah <= 0 || bw <= 0 || bh <= 0 || x2 <= x1 || y2 <= y1) {
        *x = *y = *w = *h = 0;
        return 0;
    }
    *x = x1;
    *y = y1;
    *w = x2 - x1;
    *h = y2 - y1;
    return 1;
}

/* Popover logo abaixo da âncora; se não cabe, acima. Sempre dentro do contêiner. */
static inline void mos_sui_popover_frame_inline(
    double ax, double ay, double aw, double ah,
    double pw, double ph,
    double bx, double by, double bw, double bh,
    double *x, double *y, double *w, double *h) {
    double px = ax + (aw - pw) / 2;
    double py = ay + ah + 4;
    if (px + pw > bx + bw) px = bx + bw - pw;
    if (py + ph > by + bh) py = ay - ph - 4;
    if (px < bx) px = bx;
    if (py < by) py = by;
    *x = px;
    *y = py;
    *w = pw;
    *h = ph;
}

/* Branco translúcido. fallback=1, samples_backdrop=0: não há desfoque nem leitura do fundo. */
static inline void mos_sui_glass_fallback_inline(
    double *r, double *g, double *b, double *a, double *radius, int *fallback, int *samples_backdrop) {
    *r = 1;
    *g = 1;
    *b = 1;
    *a = 0.62;
    *radius = 12;
    *fallback = 1;
    *samples_backdrop = 0;
}

/* ---- cena de prova do NSHostingView (lista, rolagem, abas, popover, vidro) ---- */

enum { MOS_SUI_W = 240, MOS_SUI_H = 200, MOS_SUI_ROWS = 4, MOS_SUI_ITEMS = 8 };

typedef struct mos_sui_scene {
    int tab;       /* 0 lista, 1 rolagem, 2 mais */
    int row;       /* -1 nenhuma */
    double offset; /* rolagem vertical, positivo desce o conteúdo */
    int popover;
} mos_sui_scene;

static inline void mos_sui_scene_init(mos_sui_scene *s) {
    s->tab = 0;
    s->row = -1;
    s->offset = 0;
    s->popover = 0;
}

static inline void mos_sui_rep_add(char **p, char *end, const char *s) {
    if (!s) return;
    while (*s && *p + 1 < end) *(*p)++ = *s++;
    **p = 0;
}

static inline void mos_sui_rep_num(char **p, char *end, int v) {
    char rev[16];
    int n = 0;
    int neg = v < 0;
    if (neg) v = -v;
    if (v == 0) rev[n++] = '0';
    while (v && n < 15) {
        rev[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    if (neg && *p + 1 < end) *(*p)++ = '-';
    while (n > 0 && *p + 1 < end) *(*p)++ = rev[--n];
    **p = 0;
}

static inline void mos_sui_pix_put(unsigned char *px, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    unsigned char *p;
    int dr, dg, db;
    if (x < 0 || y < 0 || x >= MOS_SUI_W || y >= MOS_SUI_H) return;
    p = px + ((y * MOS_SUI_W + x) * 4);
    if (a == 255 || p[3] == 0) {
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = 255;
        return;
    }
    dr = p[0];
    dg = p[1];
    db = p[2];
    p[0] = (unsigned char)((r * a + dr * (255 - a) + 127) / 255);
    p[1] = (unsigned char)((g * a + dg * (255 - a) + 127) / 255);
    p[2] = (unsigned char)((b * a + db * (255 - a) + 127) / 255);
    p[3] = 255;
}

static inline void mos_sui_fill(unsigned char *px, int x, int y, int w, int h, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    int iy, ix;
    for (iy = 0; iy < h; iy++)
        for (ix = 0; ix < w; ix++)
            mos_sui_pix_put(px, x + ix, y + iy, r, g, b, a);
}

static inline void mos_sui_item_rgb(int i, unsigned char *r, unsigned char *g, unsigned char *b) {
    static const unsigned char c[8][3] = {
        {200, 48, 48}, {48, 96, 210}, {40, 160, 72}, {210, 160, 40},
        {140, 60, 180}, {40, 160, 170}, {90, 90, 200}, {200, 100, 140},
    };
    if (i < 0 || i >= 8) {
        *r = *g = *b = 255;
        return;
    }
    *r = c[i][0];
    *g = c[i][1];
    *b = c[i][2];
}

static inline void mos_sui_scene_click(mos_sui_scene *s, double x, double y) {
    double px, py, pw, ph;
    if (s->popover) {
        mos_sui_popover_frame_inline(16, 40, 80, 24, 150, 64, 0, 0, MOS_SUI_W, MOS_SUI_H, &px, &py, &pw, &ph);
        if (x >= px && x < px + pw && y >= py && y < py + ph) return;
        s->popover = 0;
        return;
    }
    if (y < 28) {
        int t = (int)(x / 80);
        if (t < 0) t = 0;
        if (t > 2) t = 2;
        s->tab = t;
        return;
    }
    if (s->tab == 0) {
        int row = (int)((y - 28) / 32);
        if (row >= 0 && row < MOS_SUI_ROWS) s->row = row;
        return;
    }
    if (s->tab == 1) {
        if (x >= 8 && x < 72 && y >= 172 && y < 194)
            s->offset = mos_sui_clamp_offset_inline(s->offset, 40, 140, MOS_SUI_ITEMS * 36.0);
        return;
    }
    if (x >= 16 && x < 96 && y >= 40 && y < 64) s->popover = 1;
}

static inline void mos_sui_scene_scroll(mos_sui_scene *s, double dy) {
    if (s->tab != 1 || s->popover) return;
    s->offset = mos_sui_clamp_offset_inline(s->offset, dy, 140, MOS_SUI_ITEMS * 36.0);
}

static inline void mos_sui_scene_draw(const mos_sui_scene *s, unsigned char *px) {
    int i;
    double gr, gg, gb, ga, rad;
    int fb, samp;
    unsigned char ga8;
    mos_sui_fill(px, 0, 0, MOS_SUI_W, MOS_SUI_H, 255, 255, 255, 255);
    for (i = 0; i < 3; i++) {
        if (i == s->tab) mos_sui_fill(px, i * 80, 0, 80, 28, 0, 122, 255, 255);
        else mos_sui_fill(px, i * 80, 0, 80, 28, 210, 210, 214, 255);
    }
    if (s->tab == 0) {
        for (i = 0; i < MOS_SUI_ROWS; i++) {
            if (i == s->row) mos_sui_fill(px, 0, 28 + i * 32, MOS_SUI_W, 32, 0, 122, 255, 255);
            else mos_sui_fill(px, 0, 28 + i * 32, MOS_SUI_W, 32, 232, 232, 236, 255);
        }
    } else if (s->tab == 1) {
        for (i = 0; i < MOS_SUI_ITEMS; i++) {
            unsigned char r, g, b;
            int y = (int)(28 - s->offset + i * 36);
            int x0 = 0, y0 = y, w = MOS_SUI_W, h = 36;
            int iy, ix;
            mos_sui_item_rgb(i, &r, &g, &b);
            /* recorte no viewport da rolagem (y 28, altura 140) */
            if (y0 < 28) {
                h -= 28 - y0;
                y0 = 28;
            }
            if (y0 + h > 168) h = 168 - y0;
            if (h > 0) {
                for (iy = 0; iy < h; iy++)
                    for (ix = 0; ix < w; ix++)
                        mos_sui_pix_put(px, x0 + ix, y0 + iy, r, g, b, 255);
            }
        }
        mos_sui_fill(px, 8, 172, 64, 22, 40, 40, 44, 255); /* botão Descer */
    } else {
        mos_sui_fill(px, 0, 28, 120, MOS_SUI_H - 28, 180, 24, 24, 255);
        mos_sui_fill(px, 120, 28, 120, MOS_SUI_H - 28, 20, 220, 40, 255);
        mos_sui_fill(px, 16, 40, 80, 24, 60, 60, 64, 255); /* Abrir */
        mos_sui_glass_fallback_inline(&gr, &gg, &gb, &ga, &rad, &fb, &samp);
        (void)rad;
        (void)fb;
        (void)samp;
        ga8 = (unsigned char)(ga * 255 + 0.5);
        mos_sui_fill(px, 16, 100, 80, 36, (unsigned char)(gr * 255), (unsigned char)(gg * 255), (unsigned char)(gb * 255), ga8);
    }
    if (s->popover) {
        double x, y, w, h;
        mos_sui_popover_frame_inline(16, 40, 80, 24, 150, 64, 0, 0, MOS_SUI_W, MOS_SUI_H, &x, &y, &w, &h);
        mos_sui_fill(px, (int)x, (int)y, (int)w, (int)h, 250, 250, 252, 255);
    }
}

static inline void mos_sui_scene_tree(const mos_sui_scene *s, char *buf, int n) {
    char *p = buf;
    char *end = buf + (n > 0 ? n : 0);
    double x, y, w, h;
    if (n <= 0) return;
    buf[0] = 0;
    mos_sui_rep_add(&p, end, "NSHostingView 240x200\n");
    mos_sui_rep_add(&p, end, "TabView sel=");
    mos_sui_rep_num(&p, end, s->tab);
    mos_sui_rep_add(&p, end, "\n");
    if (s->tab == 0) {
        mos_sui_rep_add(&p, end, "List rows=4 selected=");
        mos_sui_rep_num(&p, end, s->row);
        mos_sui_rep_add(&p, end, " row1.y=60\n");
    } else if (s->tab == 1) {
        mos_sui_rep_add(&p, end, "ScrollView offset=");
        mos_sui_rep_num(&p, end, (int)s->offset);
        mos_sui_rep_add(&p, end, " viewport=140 content=288\n");
    } else {
        mos_sui_popover_frame_inline(16, 40, 80, 24, 150, 64, 0, 0, MOS_SUI_W, MOS_SUI_H, &x, &y, &w, &h);
        mos_sui_rep_add(&p, end, "popover=");
        mos_sui_rep_num(&p, end, s->popover);
        mos_sui_rep_add(&p, end, " x=");
        mos_sui_rep_num(&p, end, (int)x);
        mos_sui_rep_add(&p, end, " y=");
        mos_sui_rep_num(&p, end, (int)y);
        mos_sui_rep_add(&p, end, "\nglass fallback=1 samples_backdrop=0\n");
    }
}

static inline int mos_sui_has(const char *s, const char *n) {
    int i, j;
    if (!s || !n) return 0;
    for (i = 0; s[i]; i++) {
        for (j = 0; n[j] && s[i + j] == n[j]; j++) {
        }
        if (!n[j]) return 1;
    }
    return 0;
}

static inline int mos_sui_pix(const unsigned char *px, int x, int y, int ch) {
    if (x < 0 || y < 0 || x >= MOS_SUI_W || y >= MOS_SUI_H) return -1;
    return px[(y * MOS_SUI_W + x) * 4 + ch];
}

/* Devolve o número de falhas. Escreve PASS/FAIL linha a linha em err. */
static int mos_sui_selftest_run(char *err, int errlen) {
    char *p = err;
    char *end = err ? err + errlen : 0;
    int checks = 0, fails = 0;
    unsigned char *px;
    static unsigned char frame[MOS_SUI_W * MOS_SUI_H * 4];
    mos_sui_scene sc;
    double x, y, w, h;
    double gr, gg, gb, ga, rad;
    int fb, samp;
    unsigned char er, eg, eb;
    int pr, pg, pb;
    char tree[512];
    if (!err || errlen < 8) return 1;
    err[0] = 0;
#define SUI_CHECK(ok, msg)                                                         \
    do {                                                                           \
        checks++;                                                                  \
        if (!(ok)) fails++;                                                        \
        mos_sui_rep_add(&p, end, (ok) ? "PASS " : "FAIL ");                        \
        mos_sui_rep_add(&p, end, msg);                                             \
        mos_sui_rep_add(&p, end, "\n");                                            \
    } while (0)

    SUI_CHECK(mos_sui_clamp_offset_inline(0, 50, 100, 400) == 50, "clamp desce 50");
    SUI_CHECK(mos_sui_clamp_offset_inline(380, 50, 100, 400) == 300, "clamp no fim do conteúdo");
    SUI_CHECK(mos_sui_clamp_offset_inline(10, -40, 100, 400) == 0, "clamp não passa de zero");
    SUI_CHECK(mos_sui_clamp_offset_inline(0, 20, 200, 80) == 0, "conteúdo menor que o viewport");
    SUI_CHECK(mos_sui_intersect_inline(0, -10, 40, 30, 0, 0, 100, 50, &x, &y, &w, &h) == 1, "recorte parcial existe");
    SUI_CHECK(y == 0 && h == 20 && x == 0 && w == 40, "recorte parcial 0,0 40x20");
    SUI_CHECK(mos_sui_intersect_inline(0, 60, 10, 10, 0, 0, 100, 50, &x, &y, &w, &h) == 0, "fora do viewport");
    mos_sui_popover_frame_inline(16, 40, 80, 24, 150, 64, 0, 0, 240, 200, &x, &y, &w, &h);
    SUI_CHECK(x == 0 && y == 68 && w == 150 && h == 64, "popover centrado na âncora e dentro da janela");
    mos_sui_popover_frame_inline(200, 180, 30, 16, 80, 40, 0, 0, 240, 200, &x, &y, &w, &h);
    SUI_CHECK(x >= 0 && y >= 0 && x + w <= 240 && y + h <= 200, "popover preso no contêiner");
    mos_sui_glass_fallback_inline(&gr, &gg, &gb, &ga, &rad, &fb, &samp);
    SUI_CHECK(fb == 1 && samp == 0, "vidro é fallback e não amostra o fundo");
    SUI_CHECK(gr == 1 && gg == 1 && gb == 1 && ga > 0.5 && ga < 0.7, "vidro branco translúcido");

    px = frame;
    mos_sui_scene_init(&sc);
    mos_sui_scene_draw(&sc, px);
    mos_sui_scene_tree(&sc, tree, (int)sizeof tree);
    SUI_CHECK(sc.tab == 0 && sc.row == -1, "árvore inicial na lista sem seleção");
    SUI_CHECK(mos_sui_pix(px, 8, 36, 0) == 232, "linha 0 sem seleção");
    SUI_CHECK(mos_sui_pix(px, 20, 10, 2) == 255, "aba Lista ativa");

    mos_sui_scene_click(&sc, 16, 70);
    mos_sui_scene_draw(&sc, px);
    mos_sui_scene_tree(&sc, tree, (int)sizeof tree);
    SUI_CHECK(sc.row == 1 && mos_sui_has(tree, "selected=1") && mos_sui_has(tree, "row1.y=60"), "clique seleciona a linha 1 na árvore");
    SUI_CHECK(mos_sui_pix(px, 8, 68, 2) == 255 && mos_sui_pix(px, 8, 68, 0) == 0, "pixel da linha 1 é o destaque");
    SUI_CHECK(mos_sui_pix(px, 8, 36, 0) == 232, "linha 0 continua sem destaque");
    SUI_CHECK(tree[0] != 0, "árvore da lista foi emitida");

    mos_sui_scene_click(&sc, 100, 10);
    mos_sui_scene_draw(&sc, px);
    SUI_CHECK(sc.tab == 1, "clique troca para Rolagem");
    SUI_CHECK(mos_sui_pix(px, 12, 40, 0) == 200 && mos_sui_pix(px, 12, 40, 2) == 48, "topo da rolagem é o item 0");
    mos_sui_scene_scroll(&sc, 40);
    mos_sui_scene_draw(&sc, px);
    mos_sui_scene_tree(&sc, tree, (int)sizeof tree);
    SUI_CHECK(sc.offset == 40 && mos_sui_has(tree, "ScrollView offset=40") && mos_sui_has(tree, "viewport=140"), "scroll +40 muda o deslocamento na árvore");
    SUI_CHECK(mos_sui_pix(px, 12, 40, 0) == 48 && mos_sui_pix(px, 12, 40, 2) == 210, "depois do scroll o pixel é o item 1");
    mos_sui_scene_click(&sc, 20, 180);
    mos_sui_scene_draw(&sc, px);
    SUI_CHECK(sc.offset == 80, "clique em Descer soma 40");
    SUI_CHECK(mos_sui_pix(px, 12, 40, 1) == 160, "depois do clique o pixel é o item 2");

    mos_sui_scene_click(&sc, 180, 10);
    mos_sui_scene_draw(&sc, px);
    mos_sui_scene_tree(&sc, tree, (int)sizeof tree);
    SUI_CHECK(sc.tab == 2 && sc.popover == 0 && mos_sui_has(tree, "samples_backdrop=0"), "aba Mais sem popover e sem amostrar o fundo");
    /* blend independente do branco 0.62 sobre o vermelho (180,24,24); alfa 158 */
    er = (unsigned char)((255 * 158 + 180 * 97 + 127) / 255);
    eg = (unsigned char)((255 * 158 + 24 * 97 + 127) / 255);
    eb = (unsigned char)((255 * 158 + 24 * 97 + 127) / 255);
    pr = mos_sui_pix(px, 40, 110, 0);
    pg = mos_sui_pix(px, 40, 110, 1);
    pb = mos_sui_pix(px, 40, 110, 2);
    SUI_CHECK(pr == er && pg == eg && pb == eb, "vidro é o blend chapado, sem ler o vizinho");
    SUI_CHECK(mos_sui_pix(px, 200, 110, 1) == 220, "vizinho verde intacto");
    SUI_CHECK(pg + 40 < mos_sui_pix(px, 200, 110, 1), "o verde do vizinho não vazou para o vidro");

    mos_sui_scene_click(&sc, 40, 52);
    mos_sui_scene_draw(&sc, px);
    mos_sui_scene_tree(&sc, tree, (int)sizeof tree);
    SUI_CHECK(sc.popover == 1, "clique abre o popover");
    SUI_CHECK(mos_sui_pix(px, 40, 80, 0) == 250 && mos_sui_pix(px, 40, 80, 2) == 252, "pixel do popover cobre o fundo");
    mos_sui_scene_click(&sc, 200, 180);
    mos_sui_scene_draw(&sc, px);
    SUI_CHECK(sc.popover == 0, "clique fora fecha o popover");
    SUI_CHECK(mos_sui_pix(px, 40, 80, 0) == 180, "o fundo volta depois de fechar");

    mos_sui_rep_add(&p, end, fails ? "HOSTING FAIL " : "HOSTING PASS ");
    mos_sui_rep_num(&p, end, checks - fails);
    mos_sui_rep_add(&p, end, "/");
    mos_sui_rep_num(&p, end, checks);
    mos_sui_rep_add(&p, end, "\n");
    (void)tree;
#undef SUI_CHECK
    return fails;
}

#endif
