/* Teste de host do núcleo de appkit_more.c (gestos, NSMatrix, slider/stepper, curvas, EPS). Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -Iuserland/libsystem userland/libsystem/appkit_more_core.c \
 *       userland/tests-appkit/more_host.c -o /tmp/appkit-more-test && /tmp/appkit-more-test */
#include "appkit_more_core.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }
static int near(double a, double b) { return a - b < 0.001 && b - a < 0.001; }

int main(void) {
    /* curvas */
    check(near(amc_curve(3, 0.3), 0.3) && near(amc_curve(0, 0.5), 0.5), "linear e meio do easeInOut");
    check(amc_curve(1, 0.5) < 0.5 && amc_curve(2, 0.5) > 0.5, "easeIn atrasa, easeOut adianta");
    check(near(amc_curve(0, 1.7), 1) && near(amc_curve(2, -1), 0), "progresso fora de 0..1 é limitado");

    /* pan */
    gr_core g; gr_init(&g, GR_K_PAN);
    check(gr_feed(&g, GE_DOWN, 10, 10, 0, 0, 1, 0, 0) == 0 && g.state == GR_POSSIBLE, "pan: apertar não começa");
    check(gr_feed(&g, GE_DRAG, 11, 10, 0.1, 0, 1, 0, 0) == 0, "pan: 1 pt ainda não começa");
    check(gr_feed(&g, GE_DRAG, 15, 10, 0.2, 0, 1, 0, 0) == 1 && g.state == GR_BEGAN, "pan: começa ao passar de 2 pt");
    check(gr_feed(&g, GE_DRAG, 25, 12, 0.3, 0, 1, 0, 0) == 1 && g.state == GR_CHANGED, "pan: muda");
    check(gr_feed(&g, GE_UP, 25, 12, 0.4, 0, 1, 0, 0) == 1 && g.state == GR_ENDED, "pan: termina ao soltar");
    gr_reset_if_done(&g);
    check(g.state == GR_POSSIBLE, "pan: volta a possível");
    gr_feed(&g, GE_DOWN, 0, 0, 1, 0, 1, 0, 0);
    check(gr_feed(&g, GE_UP, 0, 0, 1.1, 0, 1, 0, 0) == 0 && g.state == GR_FAILED, "pan: clique parado falha");
    gr_reset_if_done(&g);
    check(gr_feed(&g, GE_DOWN, 0, 0, 2, 1, 1, 0, 0) == 0 && !g.down, "pan: botão direito fora da máscara é ignorado");

    /* press */
    gr_init(&g, GR_K_PRESS);
    gr_feed(&g, GE_DOWN, 50, 50, 0, 0, 1, 0, 0);
    check(gr_feed(&g, GE_TICK, 50, 50, 0.3, 0, 1, 0, 0) == 0, "press: antes do tempo, nada");
    check(gr_feed(&g, GE_TICK, 50, 50, 0.6, 0, 1, 0, 0) == 1 && g.state == GR_BEGAN, "press: começa em 0,5 s");
    check(gr_feed(&g, GE_UP, 50, 50, 0.9, 0, 1, 0, 0) == 1 && g.state == GR_ENDED, "press: termina ao soltar");
    gr_reset_if_done(&g);
    gr_feed(&g, GE_DOWN, 50, 50, 0, 0, 1, 0, 0);
    check(gr_feed(&g, GE_DRAG, 80, 50, 0.1, 0, 1, 0, 0) == 0 && g.state == GR_FAILED, "press: mexer além da tolerância falha");
    gr_reset_if_done(&g);
    gr_feed(&g, GE_DOWN, 0, 0, 0, 0, 1, 0, 0);
    check(gr_feed(&g, GE_UP, 0, 0, 0.2, 0, 1, 0, 0) == 0 && g.state == GR_FAILED, "press: soltar cedo falha");

    /* click */
    gr_init(&g, GR_K_CLICK); g.clicks_required = 2;
    gr_feed(&g, GE_DOWN, 5, 5, 0, 0, 1, 0, 0);
    check(gr_feed(&g, GE_UP, 5, 5, 0.1, 0, 1, 0, 0) == 0 && g.state == GR_POSSIBLE, "click duplo: o primeiro clique espera");
    gr_feed(&g, GE_DOWN, 5, 5, 0.2, 0, 2, 0, 0);
    check(gr_feed(&g, GE_UP, 6, 5, 0.25, 0, 2, 0, 0) == 1 && g.state == GR_ENDED, "click duplo: reconhece no segundo");

    /* magnify / rotate */
    gr_init(&g, GR_K_MAGNIFY);
    check(gr_feed(&g, GE_MAG, 1, 1, 0, 0, 0, 0.1, 0) == 1 && g.state == GR_BEGAN, "magnify: começa");
    check(gr_feed(&g, GE_MAG, 1, 1, 0, 0, 0, 0.15, 0) == 1 && near(g.magnification, 0.25), "magnify: acumula");
    check(gr_feed(&g, GE_MAG, 1, 1, 0, 0, 0, 0, 1) == 1 && g.state == GR_ENDED, "magnify: fase final termina");
    check(gr_feed(&g, GE_ROT, 1, 1, 0, 0, 0, 5, 0) == 0, "magnify ignora rotação");
    gr_init(&g, GR_K_ROTATE);
    gr_feed(&g, GE_ROT, 0, 0, 0, 0, 0, 10, 0); gr_feed(&g, GE_ROT, 0, 0, 0, 0, 0, -4, 0);
    check(near(g.rotation, 6) && g.state == GR_CHANGED, "rotate: acumula graus");

    /* NSMatrix */
    mx_geom m = {2, 3, 40, 20, 4, 2};
    int r = -1, c = -1;
    check(mx_hit(&m, 50, 25, &r, &c) && r == 1 && c == 1, "matriz: acerta linha 1 coluna 1");
    check(!mx_hit(&m, 42, 5, &r, &c), "matriz: espaçamento não é célula");
    check(!mx_hit(&m, 200, 5, &r, &c) && !mx_hit(&m, 5, 100, &r, &c), "matriz: fora não é célula");
    double fx, fy, fw, fh; mx_cell_frame(&m, 1, 2, &fx, &fy, &fw, &fh);
    check(near(fx, 88) && near(fy, 22) && near(fw, 40) && near(fh, 20), "matriz: moldura da célula");
    unsigned char sel[6] = {0}; int anchor = -1;
    mx_click(0, 0, sel, 6, 2, 0, &anchor); mx_click(0, 0, sel, 6, 4, 0, &anchor);
    check(!sel[2] && sel[4], "rádio: só uma selecionada");
    mx_click(0, 0, sel, 6, 4, 0, &anchor);
    check(sel[4], "rádio sem seleção vazia: clicar de novo mantém");
    mx_click(0, 1, sel, 6, 4, 0, &anchor);
    check(!sel[4], "rádio com seleção vazia: clicar de novo desmarca");
    memset(sel, 0, 6); anchor = -1;
    mx_click(2, 0, sel, 6, 1, 0, &anchor); mx_click(2, 0, sel, 6, 4, 1, &anchor);
    check(!sel[0] && sel[1] && sel[2] && sel[3] && sel[4] && !sel[5], "lista: shift estende da âncora");
    mx_click(2, 0, sel, 6, 2, 2, &anchor);
    check(!sel[2] && sel[1] && sel[3], "lista: ⌘ alterna uma");
    memset(sel, 0, 6); sel[3] = 1;
    mx_click(2, 0, sel, 6, 3, 2, &anchor);
    check(sel[3], "lista sem seleção vazia: ⌘ não tira a última");

    /* slider / stepper */
    check(near(sl_value_at(60, 10, 100, 0, 10), 5), "slider: valor no meio do trilho");
    check(near(sl_value_at(-5, 10, 100, 0, 10), 0) && near(sl_value_at(500, 10, 100, 0, 10), 10), "slider: limita às pontas");
    check(near(sl_closest_tick(3.4, 0, 10, 5), 2.5) && near(sl_closest_tick(3.9, 0, 10, 5), 5), "slider: marca mais próxima");
    check(near(sl_closest_tick(7, 0, 10, 1), 5), "slider: uma marca fica no meio");
    check(near(sl_knob_pos(2.5, 0, 200, 0, 10), 50), "slider: posição do botão");
    check(near(st_step(9.5, 1, 0, 10, 1, 0), 10) && near(st_step(10, 1, 0, 10, 1, 1), 0), "stepper: limita ou dá a volta");
    check(near(st_step(0, 1, 0, 10, 0, 1), 10) && near(st_step(0, 1, 0, 10, 0, 0), 0), "stepper: para baixo");

    /* EPS */
    const char *ps = "%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: (atend)\n%%Title: x\n%%BoundingBox: 10 20 110 70\n%%HiResBoundingBox: 10.5 20 110 70.25\n";
    double ex, ey, ew, eh;
    check(eps_bbox((const unsigned char *)ps, strlen(ps), &ex, &ey, &ew, &eh) && near(ex, 10.5) && near(ew, 99.5) && near(eh, 50.25), "EPS: HiResBoundingBox vence");
    const char *ps2 = "%!PS\n%%BoundingBox: (atend)\nshowpage\n%%Trailer\n%%BoundingBox: 0 0 72 36\n";
    check(eps_bbox((const unsigned char *)ps2, strlen(ps2), &ex, &ey, &ew, &eh) && near(ew, 72) && near(eh, 36), "EPS: (atend) usa o valor do fim");
    unsigned char dos[64] = {0xC5, 0xD0, 0xD3, 0xC6, 30, 0, 0, 0, 10, 0, 0, 0};
    dos[20] = 40; dos[24] = 20;
    unsigned long po, pl, to, tl;
    check(eps_dos_header(dos, 64, &po, &pl, &to, &tl) && po == 30 && pl == 10 && to == 40 && tl == 20, "EPS DOS: offsets do PostScript e do TIFF");
    dos[24] = 60;
    check(eps_dos_header(dos, 64, &po, &pl, &to, &tl) && to == 0 && tl == 0, "EPS DOS: TIFF fora do arquivo é ignorado");
    check(!eps_dos_header((const unsigned char *)ps, strlen(ps), &po, &pl, &to, &tl), "EPS em texto não é DOS");

    printf("%d/%d\n", ok, total);
    return ok == total ? 0 : 1;
}
