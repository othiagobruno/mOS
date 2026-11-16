/* Teste de host dos núcleos de controles do AppKit (NSSegmentedControl). Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_SG_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_segment_core.c userland/tests-appkit/controls_host.c \
 *       -o /tmp/appkit-controls-test && /tmp/appkit-controls-test */
#include "appkit_segment_core.h"
#include <stdio.h>

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }
static int near(double a, double b) { return a - b < 0.001 && b - a < 0.001; }

int main(void) {
    sg_t s;
    sg_init(&s);
    sg_set_count(&s, 3);
    s.natural[0] = 30; s.natural[1] = 2; s.natural[2] = 50;
    puts("larguras");
    check(near(sg_auto_width(30), 46) && near(sg_auto_width(2), 20), "automática: conteúdo + 16, mínimo 20");
    check(near(sg_intrinsic_width(&s), 46 + 20 + 66), "intrínseca = soma");
    double used = sg_layout(&s, 300);
    check(near(used, 132) && near(s.x[1], 46) && near(s.x[2], 66), "fit: não estica");
    s.distribution = SG_FILL;
    sg_layout(&s, 300);
    check(near(s.w[0], 46 + 56) && near(s.x[2] + s.w[2], 300), "fill: sobra dividida entre os automáticos");
    s.fixed[1] = 40;
    sg_layout(&s, 300);
    check(near(s.w[1], 40) && near(s.x[2] + s.w[2], 300), "fill: largura fixa não estica");
    s.distribution = SG_FILL_EQUALLY;
    sg_layout(&s, 300);
    check(near(s.w[0], 100) && near(s.w[2], 100), "fillEqually");
    s.distribution = SG_FILL_PROPORTIONALLY;
    sg_layout(&s, 304);
    check(near(s.w[0], 46 * 2) && near(s.x[2] + s.w[2], 304), "fillProportionally");
    s.distribution = SG_FIT; s.fixed[1] = 0;
    sg_layout(&s, 300);
    puts("acerto");
    check(sg_hit(&s, 0) == 0 && sg_hit(&s, 46) == 1 && sg_hit(&s, 131) == 2 && sg_hit(&s, 132) == -1 && sg_hit(&s, -1) == -1, "por x");
    puts("seleção");
    check(sg_selected_segment(&s) == -1, "começa sem seleção");
    check(sg_click(&s, 1) && sg_selected_segment(&s) == 1 && s.selected[1], "selectOne: clica");
    check(!sg_click(&s, 1), "selectOne: clicar no mesmo não muda");
    check(sg_click(&s, 2) && !s.selected[1] && s.selected[2], "selectOne: troca");
    s.enabled[0] = 0;
    check(!sg_click(&s, 0) && sg_selected_segment(&s) == 2, "segmento desabilitado ignora");
    s.enabled[0] = 1;
    s.tracking = SG_SELECT_ANY;
    sg_click(&s, 0);
    check(s.selected[0] && s.selected[2], "selectAny: acumula");
    sg_click(&s, 2);
    check(!s.selected[2] && s.selected[0], "selectAny: alterna");
    sg_set_selected(&s, 1, 1);
    check(s.selected[1] && s.selected[0], "selectAny: setSelected não limpa os outros");
    s.tracking = SG_MOMENTARY;
    check(sg_click(&s, 0) == 1 && sg_click(&s, 0) == 1, "momentâneo: toda vez manda a ação");
    s.tracking = SG_SELECT_ONE;
    sg_set_selected(&s, 2, 1);
    sg_set_count(&s, 2);
    check(s.n == 2 && sg_selected_segment(&s) == -1, "diminuir a contagem tira a seleção que sumiu");
    sg_set_count(&s, 100);
    check(s.n == SG_MAX, "teto de segmentos");
    printf("appkit-controls: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
