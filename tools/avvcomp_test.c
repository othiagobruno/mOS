/* Teste de host do userland/libsystem/avvcomp_core.c. */
#include <stdio.h>
#include <string.h>
#include "../userland/libsystem/avvcomp_core.h"
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static int near(int a, int b, int t) { return a - b <= t && b - a <= t; }
int main(void) {
    vc_layer l;
    memset(&l, 0, sizeof l);
    vc_aff id = vc_layer_transform(&l, 3);
    CHECK(id.a == 1 && id.d == 1 && id.tx == 0 && vc_layer_opacity(&l, 0) == 1);
    /* rampa de opacidade 1 -> 0 de 2 s a 4 s; antes vale 1, no meio 0,5, depois 0 */
    l.op[0] = (vc_f_ramp){2, 4, 1, 0}; l.nop = 1;
    CHECK(vc_layer_opacity(&l, 0) == 1 && vc_layer_opacity(&l, 3) == 0.5f && vc_layer_opacity(&l, 9) == 0);
    /* setOpacity:0.3 atTime:5 depois da rampa */
    l.op[1] = (vc_f_ramp){5, 5, 0.3f, 0.3f}; l.nop = 2;
    CHECK(vc_layer_opacity(&l, 4.5) == 0 && vc_layer_opacity(&l, 6) == 0.3f);
    /* rampa de translação 0 -> 100 em x */
    l.tr[0] = (vc_aff_ramp){0, 10, {1, 0, 0, 1, 0, 0}, {1, 0, 0, 1, 100, 0}}; l.ntr = 1;
    CHECK(vc_layer_transform(&l, 5).tx == 50);
    vc_rect cr;
    CHECK(!vc_layer_crop(&l, 1, &cr));
    l.cr[0] = (vc_rect_ramp){0, 0, {1, 1, 2, 2}, {1, 1, 2, 2}}; l.ncr = 1;
    CHECK(vc_layer_crop(&l, 1, &cr) && cr.w == 2);

    /* composição: 2x2 vermelho deslocado (+1,+1) numa tela 4x4 preta */
    uint32_t src[4] = {0xFFFF0000, 0xFFFF0000, 0xFFFF0000, 0xFFFF0000}, dst[16];
    for (int i = 0; i < 16; i++) dst[i] = 0xFF000000;
    vc_aff t = {1, 0, 0, 1, 1, 1};
    vc_composite(dst, 4, 4, src, 2, 2, t, 1, 0);
    CHECK(dst[0] == 0xFF000000 && dst[5] == 0xFFFF0000 && dst[10] == 0xFFFF0000 && dst[15] == 0xFF000000);
    /* opacidade 0,5 sobre preto: vermelho pela metade */
    for (int i = 0; i < 16; i++) dst[i] = 0xFF000000;
    vc_composite(dst, 4, 4, src, 2, 2, t, 0.5f, 0);
    CHECK(near((dst[5] >> 16) & 255, 128, 2));
    /* escala 2x: cobre a tela inteira */
    for (int i = 0; i < 16; i++) dst[i] = 0xFF000000;
    vc_aff s2 = {2, 0, 0, 2, 0, 0};
    vc_composite(dst, 4, 4, src, 2, 2, s2, 1, 0);
    int all = 1;
    for (int i = 0; i < 16; i++) if (dst[i] != 0xFFFF0000) all = 0;
    CHECK(all);
    /* rotação de 90°: (x, y) -> (-y, x) + (2, 0): a coluna da esquerda da origem vira a linha de cima */
    uint32_t g[4] = {0xFF00FF00, 0xFF0000FF, 0xFF00FF00, 0xFF0000FF};   /* esquerda verde, direita azul */
    uint32_t d2[4] = {0, 0, 0, 0};
    vc_aff r90 = {0, 1, -1, 0, 2, 0};
    vc_composite(d2, 2, 2, g, 2, 2, r90, 1, 0);
    CHECK(d2[0] == 0xFF00FF00 && d2[1] == 0xFF00FF00 && d2[2] == 0xFF0000FF && d2[3] == 0xFF0000FF);
    /* recorte: só o pixel (0,0) da origem passa */
    for (int i = 0; i < 16; i++) dst[i] = 0xFF000000;
    vc_rect c1 = {0, 0, 1, 1};
    vc_composite(dst, 4, 4, src, 2, 2, s2, 1, &c1);
    CHECK(dst[0] == 0xFFFF0000 && dst[5] == 0xFFFF0000 && dst[2] == 0xFF000000 && dst[10] == 0xFF000000);
    printf(fails ? "avvcomp: %d falhas\n" : "avvcomp: ok\n", fails);
    return fails != 0;
}
