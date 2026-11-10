/* mOS — barra de menus, popover e política de ativação (sem Objective-C).
 * O AppKit (appkit.c / appkit_extra.c) e o teste de host usam as mesmas funções.
 * O desenho do ícone na barra do sistema é do kernel; aqui fica o modelo que o app lê. */
#pragma once

enum {
    CX_SB_VARIABLE = -1, /* NSStatusItemLength */
    CX_SB_SQUARE = -2    /* NSStatusItemSquareLength */
};

enum {
    CX_ACT_REGULAR = 0,
    CX_ACT_ACCESSORY = 1,   /* LSUIElement: sem ícone no Dock, ainda pode ficar ativo */
    CX_ACT_PROHIBITED = 2
};

enum {
    CX_POP_APPLICATION = 0, /* só fecha com close */
    CX_POP_TRANSIENT = 1,   /* fecha no clique fora e ao resignar */
    CX_POP_SEMTRANSIENT = 2 /* fecha ao resignar; clique dentro do app não fecha */
};

/* Itens. O ponteiro é o objeto (no teste, qualquer endereço estável). */
int cx_sb_register(void *item, double length);
int cx_sb_unregister(void *item);
int cx_sb_count(void);
int cx_sb_shown_count(void);
void cx_sb_set_length(void *item, double length);
double cx_sb_length(const void *item);
void cx_sb_set_visible(void *item, int on);
int cx_sb_visible(const void *item);
void cx_sb_set_menu(void *item, void *menu);
void *cx_sb_menu(const void *item);
void cx_sb_set_title(void *item, const char *title);
const char *cx_sb_title(const void *item);
/* 1 se o menu abriu. Item oculto ou sem menu devolve 0 e fecha os outros menus. */
int cx_sb_click(void *item);
int cx_sb_menu_open(const void *item);

/* Popover, um por ponteiro. */
void cx_pop_set_behavior(void *pop, int behavior);
int cx_pop_behavior(const void *pop);
void cx_pop_set_content(void *pop, void *content);
void *cx_pop_content(const void *pop);
int cx_pop_is_content(const void *view);
void cx_pop_show(void *pop);
void cx_pop_close(void *pop);
int cx_pop_shown(const void *pop);
/* Clique fora do conteúdo: fecha os transient que estão abertos. */
void cx_pop_close_transients(void);
/* App deixou de estar ativo: fecha transient e semitransient. */
void cx_pop_resign_active(void);

/* Política do NSApplication. set devolve 1 se o valor é regular, accessory ou prohibited. */
int cx_act_set_policy(int policy);
int cx_act_policy(void);
int cx_act_lsuielement(void);
int cx_act_hides_dock(void);
int cx_act_activate(void);
int cx_act_active(void);

void cx_status_reset(void);
