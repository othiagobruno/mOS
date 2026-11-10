/* Teste de host da barra de menus, do popover e da política LSUIElement.
 * Compila e roda no Linux, sem machotest e sem bundle de app:
 *   gcc -std=c11 -Wall -Wextra -DCX_SB_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_status.c userland/tests-appkit/status_host.c \
 *       -o /tmp/appkit-status-test && /tmp/appkit-status-test
 * O mesmo appkit_status.c é o que o AppKit chama. */
#include "appkit_status.h"
#include <stdio.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

int main(void) {
    int items[40];
    int menu_a = 1, menu_b = 2;
    int pop_app = 10, pop_tr = 11, pop_semi = 12;
    int content = 20;
    int i;

    cx_status_reset();
    check(CX_SB_VARIABLE == -1 && CX_SB_SQUARE == -2, "comprimentos variável e quadrado");

    check(cx_sb_register(&items[0], CX_SB_SQUARE) == 1, "registra o primeiro item");
    check(cx_sb_register(&items[1], CX_SB_VARIABLE) == 1, "registra o segundo item");
    check(cx_sb_count() == 2 && cx_sb_shown_count() == 2, "dois itens visíveis");
    check(cx_sb_length(&items[0]) == CX_SB_SQUARE && cx_sb_length(&items[1]) == CX_SB_VARIABLE,
          "comprimento de cada item");
    check(cx_sb_visible(&items[0]) == 1, "item nasce visível");

    cx_sb_set_title(&items[0], "Usage");
    check(cx_sb_title(&items[0])[0] == 'U' && cx_sb_title(&items[0])[5] == 0, "título do item");

    cx_sb_set_menu(&items[0], &menu_a);
    cx_sb_set_menu(&items[1], &menu_b);
    check(cx_sb_click(&items[0]) == 1 && cx_sb_menu_open(&items[0]) == 1, "clique abre o menu");
    check(cx_sb_menu(&items[0]) == &menu_a, "o menu aberto é o do item");
    check(cx_sb_click(&items[1]) == 1 && cx_sb_menu_open(&items[1]) == 1 && cx_sb_menu_open(&items[0]) == 0,
          "abrir outro menu fecha o anterior");

    cx_sb_set_menu(&items[1], 0);
    check(cx_sb_click(&items[1]) == 0 && cx_sb_menu_open(&items[1]) == 0, "item sem menu não abre");
    cx_sb_set_menu(&items[0], &menu_a);
    check(cx_sb_click(&items[0]) == 1, "o primeiro menu abre de novo");
    cx_sb_set_visible(&items[0], 0);
    check(cx_sb_visible(&items[0]) == 0 && cx_sb_menu_open(&items[0]) == 0, "ocultar fecha o menu");
    check(cx_sb_click(&items[0]) == 0, "item oculto não abre menu");
    check(cx_sb_count() == 2 && cx_sb_shown_count() == 1, "oculto continua na barra, fora dos visíveis");

    check(cx_sb_unregister(&items[0]) == 1, "remove o item");
    check(cx_sb_count() == 1 && cx_sb_unregister(&items[0]) == 0, "remover de novo não acha o item");
    check(cx_sb_click(&items[0]) == 0, "item removido não recebe clique");

    cx_status_reset();
    {
        int filled = 1;
        for (i = 0; i < 32; i++) if (!cx_sb_register(&items[i], CX_SB_SQUARE)) filled = 0;
        check(filled && cx_sb_count() == 32, "a barra guarda 32 itens");
    }
    {
        int extra = 0;
        check(cx_sb_register(&extra, CX_SB_SQUARE) == 0, "o 33º item é recusado");
    }
    check(cx_sb_register(&items[0], 40) == 1 && cx_sb_length(&items[0]) == 40 && cx_sb_count() == 32,
          "registrar de novo só atualiza o comprimento");

    cx_status_reset();
    cx_pop_set_behavior(&pop_app, CX_POP_APPLICATION);
    cx_pop_set_content(&pop_app, &content);
    cx_pop_show(&pop_app);
    cx_pop_set_behavior(&pop_tr, CX_POP_TRANSIENT);
    cx_pop_show(&pop_tr);
    cx_pop_set_behavior(&pop_semi, CX_POP_SEMTRANSIENT);
    cx_pop_show(&pop_semi);
    check(cx_pop_shown(&pop_app) && cx_pop_shown(&pop_tr) && cx_pop_shown(&pop_semi), "três popovers abertos");
    check(cx_pop_is_content(&content) == 1 && cx_pop_content(&pop_app) == &content, "a view de conteúdo é a do popover");
    cx_pop_close_transients();
    check(cx_pop_shown(&pop_tr) == 0, "clique fora fecha o transient");
    check(cx_pop_shown(&pop_app) == 1 && cx_pop_shown(&pop_semi) == 1,
          "applicationDefined e semitransient ficam no clique fora");
    cx_pop_resign_active();
    check(cx_pop_shown(&pop_semi) == 0 && cx_pop_shown(&pop_app) == 1,
          "resign fecha o semitransient e deixa o applicationDefined");
    cx_pop_close(&pop_app);
    check(cx_pop_shown(&pop_app) == 0, "close fecha o applicationDefined");

    cx_status_reset();
    check(cx_act_policy() == CX_ACT_REGULAR && cx_act_lsuielement() == 0 && cx_act_hides_dock() == 0,
          "app nasce regular, com Dock");
    check(cx_act_set_policy(CX_ACT_ACCESSORY) == 1, "aceita a política accessory");
    check(cx_act_lsuielement() == 1 && cx_act_hides_dock() == 1 && cx_act_active() == 1,
          "LSUIElement esconde o Dock e continua ativo");
    check(cx_act_activate() == 1 && cx_act_active() == 1, "accessory ativa");
    check(cx_act_policy() == CX_ACT_ACCESSORY, "ativar não troca a política");

    cx_pop_set_behavior(&pop_semi, CX_POP_SEMTRANSIENT);
    cx_pop_show(&pop_semi);
    check(cx_act_set_policy(CX_ACT_PROHIBITED) == 1, "aceita a política prohibited");
    check(cx_act_active() == 0 && cx_act_hides_dock() == 1 && cx_pop_shown(&pop_semi) == 0,
          "prohibited desativa, esconde o Dock e fecha o semitransient");
    check(cx_act_activate() == 0 && cx_act_active() == 0, "prohibited recusa activate");
    check(cx_act_set_policy(-1) == 0 && cx_act_policy() == CX_ACT_PROHIBITED, "política inválida é recusada");
    check(cx_act_set_policy(CX_ACT_REGULAR) == 1 && cx_act_hides_dock() == 0 && cx_act_lsuielement() == 0,
          "voltar a regular mostra o Dock");
    check(cx_act_activate() == 1 && cx_act_active() == 1, "regular ativa de novo");

    printf("appkit-status: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
