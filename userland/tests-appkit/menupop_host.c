/* Teste de host do núcleo do menu flutuante. Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_MP_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_menupop_core.c userland/tests-appkit/menupop_host.c \
 *       -o /tmp/appkit-menupop-test && /tmp/appkit-menupop-test */
#include "appkit_menupop_core.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }

int main(void) {
    mp_item_t it[7];
    memset(it, 0, sizeof it);
    for (int i = 0; i < 7; i++) { it[i].kind = MP_ITEM; it[i].enabled = 1; it[i].title_w = 60; it[i].first = "acdxbcz"[i]; }
    it[2].kind = MP_SEP;
    it[3].enabled = 0;               /* "d" desabilitado */
    it[4].hidden = 1;                /* "x" oculto */
    it[0].key_w = 20;
    it[6].title_w = 140;
    mp_menu_t m;
    memset(&m, 0, sizeof m);
    mp_defaults(&m);
    m.it = it; m.n = 7;
    mp_layout(&m, 0);
    puts("layout");
    check(m.y[0] == 5 && m.y[1] == 27 && m.ih[2] == 11 && m.y[3] == 60, "alturas: item 22, separação 11, margem 5");
    check(m.ih[4] == 0 && m.y[5] == m.y[4], "item oculto não ocupa espaço");
    check(m.h == 5 + 22 * 5 + 11 + 5, "altura total (5 itens visíveis)");
    check(m.w == 22 + 140 + 24 + 20 + 18 + 6, "largura: marca + maior título + atalho + seta");
    mp_layout(&m, 400);
    check(m.w == 400, "largura mínima (a do botão pop-up)");
    puts("acerto do mouse");
    check(mp_hit(&m, 10, 6) == 0 && mp_hit(&m, 10, 30) == 1, "linhas");
    check(mp_hit(&m, 10, 52) == -1, "separação não é item");
    check(mp_hit(&m, -1, 6) == -1 && mp_hit(&m, 401, 6) == -1, "fora da largura");
    check(mp_hit(&m, 10, 62) == 3 && !mp_selectable(&m, 3), "desabilitado é acertado mas não selecionável");
    puts("teclado");
    check(mp_step(&m, -1, 1) == 0 && mp_step(&m, -1, -1) == 6, "primeiro e último");
    check(mp_step(&m, 1, 1) == 5, "↓ pula separação, desabilitado e oculto");
    check(mp_step(&m, 5, -1) == 1, "↑ idem");
    check(mp_step(&m, 6, 1) == 6 && mp_step(&m, 0, -1) == 0, "sem dar a volta");
    check(mp_type_select(&m, -1, 'C') == 1, "letra acha o primeiro");
    check(mp_type_select(&m, 1, 'c') == 5, "a mesma letra vai ao próximo");
    check(mp_type_select(&m, 5, 'c') == 1, "e dá a volta");
    check(mp_type_select(&m, -1, 'd') == -1 && mp_type_select(&m, -1, 'x') == -1, "desabilitado e oculto não contam");
    puts("posição");
    mp_rect s = {0, 0, 1440, 900};
    mp_layout(&m, 0);
    mp_rect r = mp_place(&m, -1, 100, 500, s, MP_PLACE_CONTEXT);
    check(r.x == 100 && r.y + r.h == 500, "contexto: canto superior esquerdo no ponto");
    r = mp_place(&m, -1, 1400, 500, s, MP_PLACE_CONTEXT);
    check(r.x + r.w == 1400, "contexto: sem espaço à direita abre para a esquerda");
    r = mp_place(&m, -1, 100, 50, s, MP_PLACE_CONTEXT);
    check(r.y == 50, "contexto: sem espaço embaixo abre para cima");
    r = mp_place(&m, 5, 300, 400, s, MP_PLACE_POSITION);
    check(r.y + r.h - m.y[5] == 400 && r.x == 300 - m.left, "pop-up: o item escolhido fica sobre o botão");
    r = mp_place(&m, 0, 300, 890, s, MP_PLACE_POSITION);
    check(r.y + r.h <= 896, "pop-up: desliza para caber no topo");
    r = mp_place(&m, 6, 300, 10, s, MP_PLACE_POSITION);
    check(r.y >= 4, "pop-up: desliza para caber embaixo");
    mp_rect pw = {200, 300, 180, 200};
    r = mp_place_sub(&m, pw, 27, s);
    check(r.x == 378 && r.y + r.h == 300 + 200 - 27 + 5, "submenu à direita, 1º item na altura do pai");
    pw.x = 1300;
    r = mp_place_sub(&m, pw, 27, s);
    check(r.x + r.w == 1302, "submenu à esquerda quando não cabe");
    puts("atalhos");
    char b[64];
    mp_key_label("c", 1ull << 20, b, sizeof b); check(!strcmp(b, "\xE2\x8C\x98" "C"), "⌘C");
    mp_key_label("Z", 1ull << 20, b, sizeof b); check(!strcmp(b, "\xE2\x87\xA7\xE2\x8C\x98" "Z"), "maiúscula = ⇧⌘Z");
    mp_key_label("n", (1ull << 20) | (1ull << 19) | (1ull << 18), b, sizeof b); check(!strcmp(b, "\xE2\x8C\x83\xE2\x8C\xA5\xE2\x8C\x98" "N"), "ordem ⌃⌥⌘");
    mp_key_label("\r", 1ull << 20, b, sizeof b); check(!strcmp(b, "\xE2\x8C\x98\xE2\x86\xA9"), "⌘↩");
    mp_key_label("\xEF\x9C\x80", 1ull << 20, b, sizeof b); check(!strcmp(b, "\xE2\x8C\x98\xE2\x86\x91"), "⌘↑ (U+F700)");
    mp_key_label("\xEF\x9C\x8F", 0, b, sizeof b); check(!strcmp(b, "F12"), "F12");
    mp_key_label("", 1ull << 20, b, sizeof b); check(!b[0], "sem tecla, sem rótulo");
    printf("appkit-menupop: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
