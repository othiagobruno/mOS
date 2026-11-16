/* Teste de host dos núcleos de controles do AppKit (NSSegmentedControl). Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_SG_HOST -DCX_DT_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_segment_core.c userland/libsystem/appkit_date_core.c userland/tests-appkit/controls_host.c \
 *       -o /tmp/appkit-controls-test && /tmp/appkit-controls-test */
#include "appkit_segment_core.h"
#include "appkit_date_core.h"
#include <string.h>
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
    puts("datas");
    dt_civil_t c;
    dt_from_seconds(0, &c);
    check(c.y == 1970 && c.mo == 1 && c.d == 1 && c.h == 0, "época");
    dt_from_seconds(1794783600LL, &c);
    check(c.y == 2026 && c.mo == 11 && c.d == 15 && c.h == 23 && c.mi == 0, "15/11/2026 23:00");
    check(dt_to_seconds(&c) == 1794783600LL, "ida e volta");
    dt_from_seconds(-86400, &c);
    check(c.y == 1969 && c.mo == 12 && c.d == 31, "antes da época");
    check(dt_days_in_month(2024, 2) == 29 && dt_days_in_month(1900, 2) == 28 && dt_days_in_month(2000, 2) == 29, "bissextos");
    check(dt_weekday(2026, 11, 15) == 0 && dt_weekday(1970, 1, 1) == 4, "dia da semana (domingo = 0)");
    c = (dt_civil_t){2024, 1, 31, 23, 59, 59};
    dt_step(&c, DT_MONTH, 1);
    check(c.mo == 2 && c.d == 29, "31/jan + 1 mês = 29/fev (o dia se ajusta)");
    dt_step(&c, DT_DAY, 1);
    check(c.d == 1 && c.mo == 2, "dia dá a volta dentro do mês");
    dt_step(&c, DT_MINUTE, 1);
    check(c.mi == 0 && c.h == 23, "minuto dá a volta sem mexer na hora");
    dt_step(&c, DT_MONTH, -2);
    check(c.mo == 12 && c.y == 2024, "mês dá a volta sem mexer no ano");
    int acc = 0, na = 0;
    check(dt_type_digit(&c, DT_DAY, 0, &acc, &na) == 0 && dt_type_digit(&c, DT_DAY, 7, &acc, &na) == 1 && c.d == 7, "digitar 0 7 no dia");
    check(dt_type_digit(&c, DT_DAY, 4, &acc, &na) == 1 && c.d == 4, "4 no dia já completa (40 não existe)");
    check(dt_type_digit(&c, DT_HOUR, 1, &acc, &na) == 0 && dt_type_digit(&c, DT_HOUR, 9, &acc, &na) == 1 && c.h == 19, "hora 19");
    acc = na = 0;
    dt_type_digit(&c, DT_YEAR, 2, &acc, &na); dt_type_digit(&c, DT_YEAR, 0, &acc, &na); dt_type_digit(&c, DT_YEAR, 3, &acc, &na);
    check(dt_type_digit(&c, DT_YEAR, 0, &acc, &na) == 1 && c.y == 2030, "ano com 4 dígitos");
    char buf[64]; int sp[DT_NFIELDS][2];
    c = (dt_civil_t){2026, 11, 5, 9, 7, 3};
    int nf = dt_format(&c, DT_EL_YEAR_MONTH_DAY | DT_EL_HOUR_MINUTE, buf, sizeof buf, sp);
    check(nf == 5 && !strcmp(buf, "05/11/2026 09:07") && sp[2][0] == 6 && sp[2][1] == 10, "formato pt-BR e posições dos campos");
    dt_format(&c, DT_EL_HOUR_MINUTE_SECOND, buf, sizeof buf, sp);
    check(!strcmp(buf, "09:07:03"), "só hora com segundos");
    int g[42];
    dt_month_grid(2026, 11, g);
    check(g[0] == 1 && g[29] == 30 && g[30] == -1, "novembro/2026 começa no domingo");
    dt_month_grid(2026, 12, g);
    check(g[0] == -29 && g[1] == -30 && g[2] == 1, "dezembro/2026 (começa na terça): 29 e 30 de novembro antes");
    check(dt_clamp(5, 10, 20, 1, 1) == 10 && dt_clamp(25, 10, 20, 1, 1) == 20 && dt_clamp(25, 10, 20, 1, 0) == 25, "mínimo e máximo");
    printf("appkit-controls: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
