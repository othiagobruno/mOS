/* mOS — núcleo do menu flutuante do AppKit (menu de contexto, NSPopUpButton, NSComboBox, submenus), sem
 * Objective-C. A cola (appkit_menupop.c) e o teste de host (userland/tests-appkit/menupop_host.c) usam as mesmas
 * funções. Coordenadas do menu: y cresce para baixo a partir do topo do menu. Tela: Cocoa (origem embaixo). */
#pragma once

enum { MP_ITEM, MP_SEP, MP_HEADER };
enum { MP_PLACE_POSITION, MP_PLACE_CONTEXT };

typedef struct {
    int kind;
    int enabled, hidden, has_sub;
    int state;              /* 0 desligado, 1 ligado (✓), -1 misto (–) */
    int indent;             /* indentationLevel */
    double title_w, key_w;  /* larguras medidas pela cola */
    char first;             /* 1ª letra do título, minúscula (ASCII; 0 se não há) */
} mp_item_t;

typedef struct {
    mp_item_t *it;
    int n;
    double row_h, sep_h, head_h, pad_v, left, right_min, gap_key;
    double w, h;
    double y[512];          /* topo de cada item (itens ocultos: altura 0) */
    double ih[512];         /* altura de cada item */
} mp_menu_t;

typedef struct { double x, y, w, h; } mp_rect;

/* métricas padrão do mOS (pontos) */
void mp_defaults(mp_menu_t *m);
/* calcula y/altura de cada item, a largura (no mínimo min_w) e a altura total */
void mp_layout(mp_menu_t *m, double min_w);
/* item sob o ponto (y desde o topo do menu), -1 fora ou numa separação/cabeçalho */
int mp_hit(const mp_menu_t *m, double x, double y);
int mp_selectable(const mp_menu_t *m, int i);
/* próximo selecionável a partir de from (dir +1/-1); from -1 com dir +1 = primeiro; sem volta ao início */
int mp_step(const mp_menu_t *m, int from, int dir);
/* digitar a 1ª letra: o próximo selecionável depois de from que começa com c (dá a volta); -1 se nenhum */
int mp_type_select(const mp_menu_t *m, int from, char c);

/* Onde a janela do menu fica na tela (retângulo Cocoa da janela).
 * POSITION (popUpMenuPositioningItem:atLocation:inView:): o canto superior esquerdo do item pos fica em (x, y);
 *   sem item, o topo do menu. O menu desliza para caber na tela.
 * CONTEXT (menu de contexto): o canto superior esquerdo em (x, y); se não cabe embaixo, abre para cima; se não
 *   cabe à direita, abre para a esquerda do ponto. */
mp_rect mp_place(const mp_menu_t *m, int pos, double x, double y, mp_rect screen, int mode);
/* submenu ao lado do item parent_item (retângulo da janela-mãe na tela) */
mp_rect mp_place_sub(const mp_menu_t *sub, mp_rect parent_win, double item_top, mp_rect screen);

/* texto do atalho no estilo do Mac: ⌃⌥⇧⌘ e a tecla (letras em maiúscula, teclas especiais com símbolo). */
void mp_key_label(const char *key, unsigned long long mask, char *out, int cap);
