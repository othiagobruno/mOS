/* mOS — app "Instalar mOS": abre o assistente de instalação em tela cheia (kernel/ui/setup.c) já na escolha do destino.
 * Toda a lógica (origem confiável, confirmação digitada, payload verificado, escrita limitada ao alvo) está em
 * fs/install.c e ui/setup.c; este app só o lança. Só funciona no live, com a origem do sistema identificada. */
#include "apps.h"
#include "i18n.h"

extern void setup_open_installer(void);

static void icon_instalador_draw(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    icon_disk(s, x, y + (int)(8 * u), sz);
    /* seta para baixo entrando no disco */
    float cx = x + 32 * u;
    gfx_circle(s, cx, y + 16 * u, 13 * u, RGBA(0, 0, 0, 50));
    gfx_round_rect_grad(s, cx - 13 * u, y + 2 * u, 26 * u, 26 * u, 13 * u, RGB(90, 170, 255), RGB(20, 110, 230));
    gfx_line(s, cx, y + 8 * u, cx, y + 21 * u, 2.6f * u, RGB(255, 255, 255));
    gfx_line(s, cx - 5.5f * u, y + 16 * u, cx, y + 21.5f * u, 2.6f * u, RGB(255, 255, 255));
    gfx_line(s, cx + 5.5f * u, y + 16 * u, cx, y + 21.5f * u, 2.6f * u, RGB(255, 255, 255));
}

static void launch(app_t *a) { setup_open_installer(); }

app_t app_instalador = {.name = "Instalar mOS", .exec = "instalador", .draw_icon = icon_instalador_draw, .launch = launch};
