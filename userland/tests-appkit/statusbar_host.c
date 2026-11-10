/* Teste de host da faixa de status, do selo do Dock, da composição e do modificador sozinho.
 * Não sobe a VM. O desenho real da barra usa estes mesmos retângulos, o hit-test e o source-over.
 *
 *   gcc -std=c11 -Wall -Wextra -DMOS_SB_HOST -Ikernel/include -Ikernel/ui -Ikernel/dev \
 *       kernel/ui/statusdock.c userland/tests-appkit/statusbar_host.c \
 *       -o /tmp/statusbar-host && /tmp/statusbar-host
 *
 * O AppKit traduz GEV_FLAGS em flagsChanged:. Este arquivo não executa appkit.c. */
#include "statusdock.h"
#include "composite.h"
#include "kbd_mod.h"
#include "dockbadge.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    else printf("  FALHA %s\n", what);
}

static void wr_u32(unsigned char *b, unsigned *n, unsigned v) {
    memcpy(b + *n, &v, 4);
    *n += 4;
}
static void wr_bytes(unsigned char *b, unsigned *n, const void *p, unsigned len) {
    if (len) memcpy(b + *n, p, len);
    *n += len;
}

/* id, length, visível, imagem iw x ih (ARGB reto), título, itens (action, mflags, rótulo). */
static unsigned pack_item(unsigned char *b, unsigned cap, unsigned id, int length, int visible,
                          unsigned iw, unsigned ih, const unsigned *px, const char *title,
                          const unsigned *actions, const unsigned *mflags, const char **labels, unsigned nitems) {
    unsigned n = 0, tlen = (unsigned)strlen(title), k;
    if (cap < 64) return 0;
    wr_u32(b, &n, id);
    wr_u32(b, &n, (unsigned)length);
    wr_u32(b, &n, visible ? 1u : 0u);
    wr_u32(b, &n, iw);
    wr_u32(b, &n, ih);
    wr_u32(b, &n, nitems);
    wr_u32(b, &n, tlen);
    wr_bytes(b, &n, title, tlen);
    for (k = 0; k < nitems; k++) {
        unsigned llen = (unsigned)strlen(labels[k]);
        wr_u32(b, &n, actions[k]);
        wr_u32(b, &n, mflags[k]);
        wr_u32(b, &n, llen);
        wr_bytes(b, &n, labels[k], llen);
    }
    if (iw && ih && px) wr_bytes(b, &n, px, iw * ih * 4);
    (void)cap;
    return n;
}

/* a mesma conta de blend_px em kernel/gfx/gfx.c, para o source-over bater bit a bit */
static unsigned blend_px_ref(unsigned d, unsigned s) {
    unsigned a = s >> 24, ia, rb, ag;
    if (a == 255) return s;
    if (a == 0) return d;
    ia = 255 - a;
    rb = (d & 0x00FF00FFu) * ia + 0x00800080u;
    rb = ((rb + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
    ag = ((d >> 8) & 0x00FF00FFu) * ia + 0x00800080u;
    ag = (ag + ((ag >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
    return s + (rb | ag);
}

static int flags_event(unsigned short *mods, unsigned short *phys, unsigned char sc, int ext, int up, int *key) {
    int r = kbd_mod_apply(mods, phys, sc, ext, up, key);
    /* o driver emite o evento; o window server vira GEV_FLAGS e o AppKit chama flagsChanged: */
    return r == 1 && kbd_key_is_modifier(*key);
}

int main(void) {
    unsigned char buf[4096];
    unsigned n, px[16], actions[2], mflags[2];
    const char *labels[2];
    const sb_geom_t *gs;
    int ng, pid, hit, w = 0, h = 0;
    unsigned id = 0;
    unsigned white[800 * 26];
    const unsigned *img;
    int i, j;
    unsigned short mods, phys;
    int key;
    char badge[16];
    unsigned span_d[2], span_s[2];

    sb_reset();
    dock_tile_reset();

    /* Usage: título, imagem 50% vermelha, menu "Abrir" + separador */
    for (i = 0; i < 16; i++) px[i] = 0x80FF0000u;
    actions[0] = 1; mflags[0] = 0; labels[0] = "Abrir";
    actions[1] = 0; mflags[1] = 2; labels[1] = "";
    n = pack_item(buf, sizeof buf, 1, -1, 1, 4, 4, px, "Usage", actions, mflags, labels, 2);
    check(n > 32 && sb_upsert_wire(7, buf, n) == 0, "publica o item Usage");

    actions[0] = 2; mflags[0] = 0; labels[0] = "Abrir";
    n = pack_item(buf, sizeof buf, 2, -1, 1, 0, 0, 0, "Setapp", actions, mflags, labels, 1);
    check(sb_upsert_wire(7, buf, n) == 0, "publica o item Setapp");

    n = pack_item(buf, sizeof buf, 3, -1, 0, 0, 0, 0, "Oculto", 0, 0, 0, 0);
    check(sb_upsert_wire(7, buf, n) == 0, "publica um item oculto");
    check(sb_count() == 3, "três itens na lista, inclusive o oculto");

    ng = sb_layout(800, 26, 700, 0, 0);
    gs = sb_geoms(&ng);
    check(ng == 2, "o item oculto não entra na faixa");
    check(gs[0].id == 1 && gs[0].x + gs[0].w <= 700, "Usage fica à esquerda dos ícones do sistema");
    check(strcmp(gs[0].title, "Usage") == 0 && gs[0].title_x >= 0, "a faixa mostra o título Usage");
    check(gs[0].img_w > 0 && gs[0].img_h > 0, "a faixa reserva o retângulo do ícone");
    check(gs[1].id == 2 && gs[1].x + gs[1].w <= gs[0].x, "Setapp fica à esquerda de Usage");
    hit = sb_hit(gs[0].x + gs[0].w / 2, 13, &pid, &id);
    check(hit == 1 && pid == 7 && id == 1, "hit-test no centro do item");
    check(sb_hit(750, 13, &pid, &id) == 0, "hit-test à direita da faixa erra");

    check(sb_click(gs[0].x + gs[0].w / 2, 13, &pid, &id) == 1, "clique abre o item");
    check(sb_is_open(7, 1) && sb_menu_count(7, 1) == 2, "o menu do item está aberto");
    check(strcmp(sb_menu_label(7, 1, 0, &id, 0, 0), "Abrir") == 0 && id == 1, "o primeiro rótulo é Abrir");
    check(sb_click(gs[0].x + 2, 13, &pid, &id) == 2 && !sb_is_open(7, 1), "segundo clique fecha o menu");

    /* caminho de desenho: source-over dos pixels pré-multiplicados sobre a faixa branca */
    for (i = 0; i < 800 * 26; i++) white[i] = 0xFFFFFFFFu;
    sb_layout(800, 26, 700, 0, 0);
    gs = sb_geoms(&ng);
    img = sb_image(7, 1, &w, &h);
    check(img && w == 4 && h == 4, "o kernel guarda a imagem do item");
    if (img && gs[0].img_w > 0) {
        for (j = 0; j < h && gs[0].img_y + j < 26; j++)
            for (i = 0; i < w && gs[0].img_x + i < 800; i++) {
                unsigned *d = &white[(gs[0].img_y + j) * 800 + gs[0].img_x + i];
                *d = gui_composite_px(*d, img[j * w + i], GUI_COMP_SOURCE_OVER);
            }
    }
    {
        unsigned c = white[gs[0].img_y * 800 + gs[0].img_x];
        check(c != 0xFFFFFFFFu && (c & 0xFF0000) > 0x400000, "o ícone composto não é o branco da barra");
    }
    /* item aberto: o realce é um retângulo preenchido por cima da faixa */
    sb_click(gs[0].x + 2, 4, &pid, &id);
    for (i = gs[0].x; i < gs[0].x + gs[0].w && i < 800; i++)
        white[3 * 800 + i] = gui_composite_px(white[3 * 800 + i], gui_premul(0x1E000000u), GUI_COMP_SOURCE_OVER);
    check(white[3 * 800 + gs[0].x] != 0xFFFFFFFFu && (white[3 * 800 + gs[0].x] & 0xFF) < 250,
          "o realce do item aberto escurece a faixa");

    /* source-over e os modos comuns */
    check(gui_composite_px(0xFF0000FFu, 0xFFFF0000u, GUI_COMP_SOURCE_OVER) == 0xFFFF0000u, "vermelho opaco cobre o azul");
    check(gui_composite_px(0xFF0000FFu, 0, GUI_COMP_SOURCE_OVER) == 0xFF0000FFu, "origem transparente deixa o destino");
    check(gui_composite_px(0xFF0000FFu, 0xFFFF0000u, GUI_COMP_CLEAR) == 0, "clear zera o pixel");
    check(gui_composite_px(0xFF0000FFu, 0xFF00FF00u, GUI_COMP_COPY) == 0xFF00FF00u, "copy troca o pixel");
    check(gui_composite_px(0, 0xFFFF0000u, GUI_COMP_SOURCE_IN) == 0, "source-in sobre destino transparente é zero");
    check(gui_composite_px(0xFF0000FFu, 0xFFFF0000u, GUI_COMP_SOURCE_IN) == 0xFFFF0000u, "source-in de vermelho opaco sobre azul opaco é vermelho");
    check(gui_composite_px(0, 0xFFFF0000u, GUI_COMP_DEST_OVER) == 0xFFFF0000u, "dest-over com destino transparente é a origem");
    check(gui_composite_px(0xFF112233u, 0xFF445566u, GUI_COMP_XOR) == 0, "xor de dois opacos é zero");
    {
        unsigned r = gui_composite_px(0xFF0000FFu, 0x807F0000u, GUI_COMP_SOURCE_OVER);
        unsigned rr = (r >> 16) & 255, gg = (r >> 8) & 255, bb = r & 255;
        check((r >> 24) == 255 && rr > 40 && rr < 200 && bb > 40 && bb < 200 && gg < 8, "source-over de vermelho 50% sobre azul");
        check(r != 0x807F0000u && r != 0xFF0000FFu, "o resultado não é nem a origem nem o destino");
    }
    {
        unsigned pairs[][2] = {
            {0xFF0000FFu, 0xFFFF0000u}, {0xFF0000FFu, 0x807F0000u}, {0x80FFFFFF, 0x40FF00FFu},
            {0, 0x80FF0000u}, {0xFF010203u, 0x01000000u}, {0xFFFFFFFF, 0x01800080u}
        };
        int same = 1;
        for (i = 0; i < 6; i++)
            if (gui_over(pairs[i][0], pairs[i][1]) != blend_px_ref(pairs[i][0], pairs[i][1])) same = 0;
        check(same, "gui_over bate com blend_px");
    }
    span_d[0] = 0xFFFFFFFFu; span_d[1] = 0xFF0000FFu;
    span_s[0] = 0xFFFF0000u; span_s[1] = 0;
    gui_composite_span(span_d, span_s, 2, GUI_COMP_SOURCE_OVER);
    check(span_d[0] == 0xFFFF0000u && span_d[1] == 0xFF0000FFu, "blit de dois pixels em source-over");
    check(gui_composite_px(0xFF808080u, 0xFF808080u, GUI_COMP_PLUS_LIGHTER) == 0xFFFFFFFFu, "plusLighter satura");
    check(gui_composite_px(0xFF0000FFu, 0xFFFF0000u, 99) == gui_over(0xFF0000FFu, 0xFFFF0000u), "op desconhecido cai em source-over");
    {
        unsigned rect[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
        unsigned src = gui_premul(0x80FF0000u);
        for (i = 0; i < 4; i++) rect[i] = gui_composite_px(rect[i], src, GUI_COMP_SOURCE_OVER);
        check(rect[0] == rect[3] && rect[0] != 0xFFFFFFFFu, "preenchimento 2x2 em source-over");
    }

    /* Shift sozinho: pressionar e soltar entregam o evento de flags, sem ser uma letra */
    mods = 0; phys = 0; key = 0;
    check(flags_event(&mods, &phys, 0x2A, 0, 0, &key) && key == KBD_KEY_LSHIFT && (mods & 1), "Shift pressionado gera flagsChanged");
    check(flags_event(&mods, &phys, 0x2A, 0, 1, &key) && key == KBD_KEY_LSHIFT && (mods & 1) == 0, "Shift solto gera flagsChanged e limpa o bit");
    check(kbd_mod_apply(&mods, &phys, 0x1E, 0, 0, &key) == 0, "a letra a não é modificador");
    check(kbd_key_is_modifier(KBD_KEY_LSHIFT) && !kbd_key_is_modifier('a'), "a tecla do evento é modificador");

    mods = 0; phys = 0;
    check(kbd_mod_apply(&mods, &phys, 0x2A, 0, 0, &key) == 1, "Shift esquerdo desce");
    check(kbd_mod_apply(&mods, &phys, 0x36, 0, 0, &key) == 1 && (mods & 1), "Shift direito desce");
    check(kbd_mod_apply(&mods, &phys, 0x2A, 0, 0, &key) == 2 && (mods & 1), "repetição do Shift não gera outro evento");
    check(kbd_mod_apply(&mods, &phys, 0x2A, 0, 1, &key) == 1 && (mods & 1), "soltar o esquerdo com o direito preso mantém Shift");
    check(kbd_mod_apply(&mods, &phys, 0x36, 0, 1, &key) == 1 && (mods & 1) == 0, "soltar o direito limpa Shift");

    mods = 0; phys = 0;
    check(flags_event(&mods, &phys, 0x1D, 0, 0, &key) && key == KBD_KEY_LCTRL && (mods & 2), "Control esquerdo");
    check(flags_event(&mods, &phys, 0x1D, 0, 1, &key) && (mods & 2) == 0, "Control esquerdo solto");
    check(flags_event(&mods, &phys, 0x1D, 1, 0, &key) && key == KBD_KEY_RCTRL, "Control direito");
    check(flags_event(&mods, &phys, 0x1D, 1, 1, &key) && (mods & 2) == 0, "Control direito solto");
    check(flags_event(&mods, &phys, 0x38, 0, 0, &key) && key == KBD_KEY_LALT && (mods & 4), "Option esquerdo");
    check(flags_event(&mods, &phys, 0x38, 0, 1, &key) && (mods & 4) == 0, "Option esquerdo solto");
    check(flags_event(&mods, &phys, 0x38, 1, 0, &key) && key == KBD_KEY_RALT, "Option direito");
    check(flags_event(&mods, &phys, 0x38, 1, 1, &key) && (mods & 4) == 0, "Option direito solto");
    check(flags_event(&mods, &phys, 0x5B, 1, 0, &key) && key == KBD_KEY_LCMD && (mods & 8), "Command esquerdo");
    check(flags_event(&mods, &phys, 0x5B, 1, 1, &key) && (mods & 8) == 0, "Command esquerdo solto");
    check(flags_event(&mods, &phys, 0x5C, 1, 0, &key) && key == KBD_KEY_RCMD, "Command direito");
    check(flags_event(&mods, &phys, 0x5C, 1, 1, &key) && (mods & 8) == 0, "Command direito solto");
    check(kbd_mod_apply(&mods, &phys, 0x5B, 0, 0, &key) == 0, "scancode 5B sem E0 não é Command");

    check(dock_tile_set_visible(7, 0) == 0 && dock_tile_is_hidden(7), "o Dock esconde o app");
    check(dock_tile_set_visible(7, 1) == 0 && !dock_tile_is_hidden(7), "o Dock mostra o app de novo");
    check(!dock_tile_is_hidden(99), "app sem registro continua visível");
    check(dock_tile_set_badge(7, "3") == 3 && dock_tile_badge_n(7) == 3, "selo numérico 3");
    check(dock_badge_paint(dock_tile_badge_n(7), dock_tile_badge_text(7), badge, 16) && strcmp(badge, "3") == 0, "pinta o selo 3");
    check(dock_tile_set_badge(7, "99+") == 0 && dock_tile_badge_n(7) == 0, "selo 99+ não vira número");
    check(dock_badge_paint(0, dock_tile_badge_text(7), badge, 16) && strcmp(badge, "99+") == 0, "pinta o texto 99+");
    check(dock_tile_set_badge(7, "") == 0 && dock_tile_badge_text(7)[0] == 0, "selo vazio limpa");
    check(dock_badge_paint(4, "", badge, 16) && strcmp(badge, "4") == 0, "número antigo sem texto vira 4");
    check(dock_badge_paint(1000, "", badge, 16) && strcmp(badge, "999+") == 0, "número acima de 999 vira 999+");
    check(dock_badge_paint(0, "", badge, 16) == 0, "sem selo não pinta");
    {
        int nb = 0;
        char lab[8];
        dock_badge_apply(&nb, lab, "3");
        check(nb == 3 && strcmp(lab, "3") == 0, "o selo do Dock (I19) guarda 3");
        dock_badge_apply(&nb, lab, "99+");
        check(nb == 1 && strcmp(lab, "99+") == 0, "o selo do Dock guarda o texto 99+");
        dock_badge_apply(&nb, lab, "");
        check(nb == 0 && lab[0] == 0, "o selo do Dock vazio some");
        dock_badge_apply(&nb, lab, "0");
        check(nb == 0 && lab[0] == 0, "o selo zero some");
    }

    printf("statusbar-host: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
