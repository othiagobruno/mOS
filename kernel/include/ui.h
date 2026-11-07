/* mOS — "Cupertino": design system, window server e toolkit de widgets */
#pragma once
#include "gfx.h"

/* ============================================================ tema */
typedef struct {
    bool dark;
    u32 accent, accent_hi, selection, focus_ring;
    u32 text, text2, text3, text_inv;
    u32 window_bg, content_bg, alt_row, separator, border;
    u32 title_top, title_bot, title_text, title_inactive;
    u32 control_top, control_bot, control_border, control_text;
    u32 field_bg, field_border;
    u32 sidebar_bg, sidebar_text, sidebar_head;
    u32 menu_bg, menu_text, menu_sep, menubar_tint;
    u32 dock_tint, dock_border;
    u32 scroll_thumb;
    u32 tooltip_bg;
    /* vidro líquido */
    u32 glass_text, glass_text2;     /* texto sobre vidro */
    u32 glass_hairline;              /* contorno externo fino (contraste sobre fundos claros) */
    u32 glass_fill;                  /* preenchimento de controles sobre vidro (botões, blocos) */
    u32 glass_fill_hi;               /* idem, ao passar o mouse */
    u32 glass_sel;                   /* seleção em menus de vidro */
    u32 knob;                        /* botão (thumb) de sliders e switches */
    u32 tool_bg, tool_border;        /* grupos em cápsula da barra de ferramentas */
    u32 sidebar_sel;                 /* seleção na barra lateral */
} theme_t;

/* estilos de vidro */
enum { GLASS_REGULAR, GLASS_CLEAR, GLASS_THICK, GLASS_SIDEBAR, GLASS_CONTROL, GLASS_WIDGET, GLASS_KINDS };
extern glass_t G[GLASS_KINDS];
extern int g_glass_level;   /* opacidade global do vidro 0..100 (50 = padrão); aplicada em theme_set */
/* painel de vidro completo: sombra + vidro + contorno */
void glass_panel(surface_t *s, float x, float y, float w, float h, float r, int kind);
void glass_capsule_fill(surface_t *s, float x, float y, float w, float h, float r, u32 fill, int rim);

extern theme_t T;            /* tema ativo */
extern int g_accent_index;   /* 0..7 */
extern const u32 g_accent_colors[8];
extern const char *g_accent_names[8];
void theme_set(bool dark, int accent);

/* métricas (pontos = pixels; mOS usa escala 1x) */
#define MENUBAR_H   26
#define TITLEBAR_H  28
#define WIN_RADIUS  10.0f   /* cantos mais fechados (padrão atual) */
#define MENU_RADIUS 12.0f
/* janelas unificadas: cabeçalho de 52 px; o app usa toolbar_h = TOOLBAR_H e centraliza seus controles
 * de altura h na faixa de 36 px do topo do conteúdo com y = TOOLBAR_Y(h) */
#define TOOLBAR_H   44
#define TOOLBAR_Y(h) ((36 - (h)) / 2)
#define CTRL_RADIUS 7.0f
#define FONT_UI     13.0f
#define FONT_SMALL  11.0f

/* ============================================================ menus */
struct menu;
typedef struct {
    const char *label;   /* NULL = separador */
    const char *shortcut; /* ex.: "⌘C", "⇧⌘S", "⌥⌘⎋" — desenhado como "⌘ C" */
    int action;
    bool disabled, checked;
    /* campos novos (opcionais; inicializadores posicionais antigos continuam válidos) */
    void (*icon)(surface_t *, int, int, int, u32); /* símbolo à esquerda do rótulo */
    bool header;                                    /* título de seção (não clicável) */
    struct menu *submenu;                           /* abre um menu em cascata */
} menu_item_t;

typedef struct menu {
    const char *title;
    menu_item_t *items;
    int count;
} menu_t;

/* ícones de barra lateral (traço, desenhados na cor recebida) */
void glyph_sb_recents(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_shared(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_desktop(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_document(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_download(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_cloud(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_home(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_trash(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_photo(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_music(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_apps(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sb_disk(surface_t *s, int x, int y, int sz, u32 c);
void glyph_sidebar_toggle(surface_t *s, int x, int y, int sz, u32 c);

/* símbolos para itens de menu */
void glyph_copy(surface_t *s, int x, int y, int sz, u32 c);
void glyph_globe(surface_t *s, int x, int y, int sz, u32 c);

/* ============================================================ immediate-mode UI */
struct window;
typedef struct ui {
    struct window *win;
    surface_t *s;
    int mx, my;              /* mouse em coordenadas do conteúdo */
    bool inside, down, pressed, released, rpressed, dblclick;
    int wheel;
    u32 hot, active, focus;
    i32 key;                 /* tecla pressionada neste quadro (0 se nenhuma) */
    u16 mods;
    char text[16];           /* caractere digitado em UTF-8 */
    int cw, ch;              /* tamanho da área de conteúdo */
    u64 now;
    bool clicked_any, repaint, key_used;
    int drag_x, drag_y;      /* posição do mouse ao iniciar o arrasto */
    int cursor;              /* cursor desejado */
    /* estado de campo de texto em foco */
    int caret, sel_anchor;
    u64 caret_blink;
    /* sobreposições adiadas (popover, dica de ferramenta): desenhadas depois do conteúdo da janela;
     * enquanto um popover está aberto, modal_rect bloqueia os cliques nos controles que ficam embaixo */
    void (*overlay_fn)(struct ui *u);
    rect_t modal_rect, modal_next;   /* modal_next é pedido durante o quadro e vira modal_rect no fim dele */
    bool in_overlay;
    char tip[96];
    int tip_x, tip_y;
    bool tip_on;
    /* navegação por teclado: a cada quadro os controles focáveis se registram (em ordem de desenho);
     * Tab/⇧Tab percorre a lista; Espaço aciona o controle com foco (kfocus). Campos de texto usam `focus`. */
    int kfocus;
    rect_t kfocus_rect;
    int fl_ids[128];
    u8 fl_text[128];
    int nfl;
} ui_t;

enum {
    CURSOR_ARROW, CURSOR_IBEAM, CURSOR_HAND, CURSOR_RESIZE_H, CURSOR_RESIZE_V, CURSOR_RESIZE_D, CURSOR_WAIT,
    CURSOR_RESIZE_D2,                       /* diagonal ↗↙ (canto inferior esquerdo) */
    CURSOR_CROSS, CURSOR_MOVE, CURSOR_ZOOM_IN, CURSOR_ZOOM_OUT,
    CURSOR_GRAB, CURSOR_GRABBING, CURSOR_COPY,
    CURSOR_RESIZE_LEFT, CURSOR_RESIZE_RIGHT, CURSOR_RESIZE_UP, CURSOR_RESIZE_DOWN,
    CURSOR_RESIZE_COL, CURSOR_RESIZE_ROW,
    CURSOR_KINDS
};

/* ============================================================ janelas e apps */
enum {
    WIN_RESIZABLE = 1, WIN_NO_TITLE = 2, WIN_UNIFIED = 4, WIN_DIALOG = 8, WIN_NO_MINIMIZE = 16,
    WIN_NO_ZOOM = 32, WIN_DARK_CHROME = 64, WIN_TRANSPARENT = 128, WIN_CENTER = 256
};

struct app;
typedef struct window {
    int id;
    struct app *app;
    char title[96];
    int x, y, w, h;
    int min_w, min_h;
    u32 flags;
    int toolbar_h;              /* altura extra de barra de ferramentas unificada */
    int sidebar_w;              /* largura de barra lateral translúcida (0 = nenhuma) */
    surface_t *surf;
    bool needs_paint, minimized, zoomed, visible, closing;
    rect_t restore;
    void (*paint)(struct window *w, ui_t *u);
    void (*on_close)(struct window *w);
    void (*on_key)(struct window *w, input_event_t *e);
    void *data;
    ui_t ui;
    float anim;                 /* animação de abertura/fechamento 0..1 */
    int anim_dir;               /* +1 abrindo, -1 fechando, 2 minimizando */
    u32 bg;                     /* cor de fundo do conteúdo (0 = T.window_bg) */
    int title_x;                /* janelas unificadas: x do título (0 = padrão, -1 = o app desenha) */
    int sidebar_saved;          /* largura da barra lateral quando recolhida (sidebar_w = 0) */
    struct window *next;
    bool fullscreen;            /* tela cheia: a janela vira um Space próprio */
    rect_t fs_restore;
    int tab_group;              /* abas: janelas com o mesmo grupo (≠ 0) dividem um quadro; só uma aparece */
    bool tab_hidden;            /* aba que não é a da frente */
} window_t;

typedef struct app {
    const char *name;
    const char *exec;           /* identificador curto */
    void (*draw_icon)(surface_t *s, int x, int y, int size);
    void (*launch)(struct app *a);
    void (*tick)(struct app *a);
    void (*on_menu)(struct app *a, int action);
    void (*on_quit)(struct app *a);
    menu_t *menus;
    int nmenus;
    bool running, pinned;
    int windows;
    float bounce;               /* animação de pulo no Dock */
    surface_t *icon64, *icon128;
    void *state;
    /* Dock */
    int badge;                  /* número no selo vermelho do ícone (0 = sem selo) */
    bool attention;             /* pula no Dock até o app ficar ativo */
    bool open_at_login;         /* Opções > Abrir ao Iniciar */
} app_t;

/* window server */
void      wm_init(void);
window_t *wm_create(app_t *app, const char *title, int x, int y, int w, int h, u32 flags);
void      wm_close(window_t *w);
void      wm_invalidate(window_t *w);
void      wm_focus(window_t *w);
window_t *wm_focused(void);
app_t    *wm_active_app(void);
void      wm_set_title(window_t *w, const char *t);
void      wm_minimize(window_t *w);
void      wm_zoom(window_t *w);
void      wm_tile(window_t *w, int zone);        /* 1 metade esquerda, 2 direita, 3 preencher, 4 metade de cima, 5 de baixo,
                                                    6..9 quartos (↖ ↗ ↙ ↘), 10 centralizar */
void      wm_toggle_mission_control(void);
void      wm_fullscreen(window_t *w);            /* entra/sai da tela cheia (Space próprio) */
bool      wm_space_fullscreen(void);             /* o Space atual é uma janela em tela cheia */
void      wm_space_step(int dir);                /* ⌃← / ⌃→: Space anterior/seguinte */
void      wm_merge_tabs(window_t *w);            /* Janela > Mesclar Todas as Janelas (do mesmo app) */
void      wm_tab_detach(window_t *w);            /* Janela > Mover Aba para Nova Janela */
void      wm_tab_step(int dir);                  /* ⌃Tab / ⌃⇧Tab: aba seguinte/anterior */
void      wm_new_tab(window_t *w);               /* ⌘T: o app abre uma janela nova que entra como aba */
void      wm_toggle_stage_manager(void);
extern bool g_stage_manager;
void      wm_damage(rect_t r);
void      wm_damage_all(void);
rect_t    wm_content_rect(window_t *w);   /* em coordenadas de tela */
window_t *wm_window_list(void);
void      wm_close_app(app_t *a);
void      wm_bring_app(app_t *a);
int       wm_count_for(app_t *a);
void      wm_compose(void);
bool      wm_handle_event(input_event_t *e);
int       wm_mouse_x(void);
int       wm_mouse_y(void);
int       wm_mouse_buttons(void);         /* bit 0 esquerdo, bit 1 direito */
window_t *wm_window_at_point(int x, int y);
void      wm_set_cursor(int c);

/* desktop: menubar, dock, wallpaper, notificações, busca */
void desktop_init(void);
void desktop_draw_background(surface_t *s, rect_t r);
void desktop_draw_overlay(surface_t *s, rect_t r);
bool desktop_handle_event(input_event_t *e);
void desktop_tick(void);
int  desktop_dock_top(void);
rect_t desktop_dock_rect(void);
void desktop_rebuild_wallpaper(void);
void notify(const char *title, const char *body);
void desktop_open_menu(int index);
void desktop_close_menus(void);
bool desktop_menu_open(void);
void desktop_show_about(void);
void desktop_shutdown_dialog(int kind); /* 0 desligar, 1 reiniciar */
void desktop_toggle_spotlight(void);
void desktop_toggle_control_center(void);
void desktop_toggle_launcher(void);
int  desktop_glass_rects(rect_t *out, int max); /* regiões de vidro sobrepostas (já com a margem de leitura) */
surface_t *desktop_wallpaper_blur(void);        /* papel de parede inteiro desfocado */
rect_t dock_icon_rect(app_t *a);
rect_t dock_window_rect(window_t *w);           /* destino do gênio: a miniatura da janela no Dock */
void dock_set_badge(app_t *a, int n);           /* selo vermelho com número (0 remove) */
void dock_request_attention(app_t *a);          /* o ícone pula até o app ficar ativo */
void desktop_tile_popover(window_t *w, int bx, int by); /* menu do botão verde (bx,by = centro do botão na tela) */
bool desktop_tile_popover_open(void);
extern int g_wallpaper_style;
extern bool g_dock_magnify;
extern bool g_dock_autohide;
extern int g_dock_size;

/* registro de apps */
void   apps_register_all(void);
int    apps_count(void);
app_t *apps_get(int i);
app_t *apps_find(const char *exec);
app_t *apps_shortcut_for(app_t *proxy);   /* atalho de /Aplicativos dono das janelas de um processo */
void   app_launch(app_t *a);
void   app_quit(app_t *a);

/* ícones desenhados vetorialmente */
surface_t *icon_for(app_t *a, int size);
void icon_folder(surface_t *s, int x, int y, int size, u32 tint);
void icon_document(surface_t *s, int x, int y, int size, const char *ext);
void icon_disk(surface_t *s, int x, int y, int size);
void icon_trash(surface_t *s, int x, int y, int size, bool full);
void glyph_mos_logo(surface_t *s, float cx, float cy, float r, u32 c, u32 bg);
void glyph_wifi(surface_t *s, int x, int y, int sz, u32 c, int bars);
void glyph_ethernet(surface_t *s, int x, int y, int sz, u32 c);
void glyph_bluetooth(surface_t *s, int x, int y, int sz, u32 c);
void glyph_speaker(surface_t *s, int x, int y, int sz, u32 c, int level);
void glyph_search(surface_t *s, int x, int y, int sz, u32 c);
void glyph_list(surface_t *s, int x, int y, int sz, u32 c);
void glyph_chevron(surface_t *s, int x, int y, int sz, u32 c, int dir); /* 0 dir,1 esq,2 baixo,3 cima */
void glyph_plus(surface_t *s, int x, int y, int sz, u32 c);
void glyph_reload(surface_t *s, int x, int y, int sz, u32 c);
void glyph_check(surface_t *s, int x, int y, int sz, u32 c);
void glyph_gear(surface_t *s, float cx, float cy, float r, u32 c, u32 hole);
void glyph_grid(surface_t *s, int x, int y, int sz, u32 c);
void glyph_house(surface_t *s, int x, int y, int sz, u32 c);
void glyph_clock(surface_t *s, int x, int y, int sz, u32 c);

/* ============================================================ widgets */
#define UI_ID (__LINE__)
void ui_begin(ui_t *u, surface_t *s);
bool ui_button(ui_t *u, int id, rect_t r, const char *label, bool primary);
bool ui_button_icon(ui_t *u, int id, rect_t r, void (*glyph)(surface_t *, int, int, int, u32), bool enabled);
bool ui_toolbar_button(ui_t *u, int id, rect_t r, const char *label, bool enabled);
bool ui_checkbox(ui_t *u, int id, int x, int y, const char *label, bool *v);
bool ui_radio(ui_t *u, int id, int x, int y, const char *label, bool on);
bool ui_switch(ui_t *u, int id, int x, int y, bool *v);
/* switch em qualquer tamanho (h = altura do trilho, largura 2,25 h); pressionado vira lente de vidro */
bool ui_switch_ex(ui_t *u, int id, int x, int y, int h, bool *v, bool enabled);
/* ------------------------------------------------------------ UI kit: componentes adicionais */
void   ui_tooltip(ui_t *u, int id, rect_t r, const char *text);           /* dica após ~700 ms sobre r */
rect_t ui_popover_frame(ui_t *u, rect_t anchor, int w, int h);           /* painel com seta ancorado */
int    ui_tabview(ui_t *u, int id, rect_t r, const char **labels, int n, int sel); /* painel + abas na borda */
bool   ui_table_header(ui_t *u, int id, rect_t r, const char **cols, const int *xs, int ncols, int *sort_col, bool *asc);
bool   ui_outline_row(ui_t *u, int id, rect_t r, int depth, bool expandable, bool *open, const char *label,
                      void (*icon)(surface_t *, int, int, int, u32), bool selected);
bool   ui_calendar(ui_t *u, int id, rect_t r, int *y, int *m, int *d); /* mês com navegação ‹ › */
bool   ui_date_field(ui_t *u, int id, rect_t r, int *y, int *m, int *d); /* abre o calendário num popover */
bool   ui_color_well(ui_t *u, int id, rect_t r, u32 *color);              /* abre uma paleta num popover */
bool   ui_token_field(ui_t *u, int id, rect_t r, char tokens[][24], int *n, int max, char *buf, int cap);
rect_t ui_sheet(ui_t *u, int w, int h, int top);                         /* folha modal; termine com ui_sheet_end */
void   ui_sheet_end(ui_t *u);

/* animação de widget: devolve o valor atual, que se aproxima de target em ~dur_ms (pede repintura até chegar) */
float ui_anim(ui_t *u, int id, float target, float dur_ms);
/* pré-visualização em galerias: força o estado visual de switch/checkbox (-1 = real) */
extern int ui_force_state, ui_force_key;
/* checkbox com estado 0 desmarcado, 1 marcado, 2 misto (–) e opção de desativado */
bool ui_checkbox_ex(ui_t *u, int id, int x, int y, const char *label, int *state, bool enabled);
bool ui_slider(ui_t *u, int id, rect_t r, float *v, float lo, float hi);
int  ui_segmented(ui_t *u, int id, rect_t r, const char **labels, int n, int sel);
bool ui_textfield(ui_t *u, int id, rect_t r, char *buf, int cap, const char *placeholder);
/* área de transferência do sistema (⌘C/⌘X/⌘V entre apps) */
void clipboard_set(const char *t, int n);   /* n < 0: até o fim da string */
const char *clipboard_get(void);
bool ui_searchfield(ui_t *u, int id, rect_t r, char *buf, int cap);
int  ui_popup(ui_t *u, int id, rect_t r, const char **items, int n, int sel);
void ui_progress(ui_t *u, rect_t r, float v);
/* barra de progresso em cápsula: v em 0..1, ou v < 0 para indeterminado (faixa animada);
 * tinted = azul (ativo), senão cinza (janela inativa / pausado) */
void ui_progress_bar(ui_t *u, rect_t r, float v, bool tinted);
/* progresso circular de diâmetro d: anel fino (d >= 28) ou "pizza" grossa (d < 28) */
void ui_progress_ring(ui_t *u, int cx, int cy, int d, float v, bool tinted);
void ui_spinner(ui_t *u, int cx, int cy, int r);
void ui_label(ui_t *u, int x, int y, const char *t, int weight, float size, u32 color);
void ui_label_center(ui_t *u, rect_t r, const char *t, int weight, float size, u32 color);
void ui_separator(ui_t *u, int x, int y, int w);
void ui_box(ui_t *u, rect_t r, const char *title);
bool ui_list_row(ui_t *u, int id, rect_t r, bool selected, bool alt);
int  ui_scroll(ui_t *u, int id, rect_t view, int content_h, int *offset);
bool ui_sidebar_item(ui_t *u, int id, rect_t r, const char *label, bool selected, void (*icon)(surface_t *, int, int, int, u32));
bool ui_hit(ui_t *u, rect_t r);
bool ui_clicked(ui_t *u, int id, rect_t r);
void ui_focus_ring(ui_t *u, rect_t r, float radius);
void ui_disclosure(ui_t *u, int x, int y, bool open);
void ui_sidebar_header(ui_t *u, rect_t r, const char *label); /* r.h = altura da faixa do título */
/* título (e subtítulo opcional) alinhado à esquerda na barra de ferramentas unificada; y = topo da linha */
void ui_title(ui_t *u, int x, int y, int h, int maxw, const char *title, const char *subtitle);
/* grupo em cápsula da barra de ferramentas (fundo branco com contorno e sombra suave) */
void ui_tool_group(ui_t *u, rect_t r);

/* ------------------------------------------------------------ UI kit: botões com estilo */
typedef void (*glyph_fn)(surface_t *, int, int, int, u32);
enum {
    BTN_GRAY,          /* cinza (padrão) */
    BTN_TINTED,        /* fundo de destaque translúcido, texto na cor de destaque */
    BTN_TINTED_RED,    /* idem, destrutivo */
    BTN_FILLED,        /* preenchido com a cor de destaque (ação principal) */
    BTN_FILLED_RED,    /* preenchido vermelho (ação destrutiva) */
    BTN_PLAIN,         /* sem fundo, só texto na cor de destaque */
    BTN_SUBTLE,        /* fundo de destaque bem claro (botões com ícone, ex.: Favoritar) */
    BTN_GRAY_ACCENT,   /* fundo cinza, texto/ícone na cor de destaque */
    BTN_PLAIN_GRAY,    /* sem fundo, texto secundário (cinza) */
    BTN_STYLE_MASK = 0xFF,
    BTN_SHOW_PRESSED = 0x100,  /* força a aparência pressionada (galerias/documentação) */
    BTN_CAPSULE = 0x200,       /* cantos totalmente arredondados */
    BTN_DEFAULT = 0x400,       /* botão principal: responde a Enter */
};
/* estilo: BTN_* (pode combinar com BTN_SHOW_PRESSED/BTN_CAPSULE); icon opcional à esquerda do rótulo.
 * Em janelas que não estão em foco, os estilos coloridos viram cinza (como no macOS). */
bool ui_button_style(ui_t *u, int id, rect_t r, const char *label, int style, bool enabled, glyph_fn icon);
/* botão circular só com ícone (ex.: seletor ⌃⌄), diâmetro d */
bool ui_circle_button(ui_t *u, int id, int cx, int cy, int d, glyph_fn icon, bool enabled, bool show_pressed);
/* combo box: campo de texto editável + botão de lista embutido; devolve true se o valor mudou pela lista */
bool ui_combobox(ui_t *u, int id, rect_t r, char *buf, int cap, const char **items, int n, bool enabled);
/* desenho puro de um botão estilizado (para superfícies fora de janelas: diálogos do sistema).
 * state: 0 normal, 1 pressionado, 2 desativado */
void ui_draw_button(surface_t *s, rect_t r, const char *label, int style, int state, bool key);

/* ------------------------------------------------------------ UI kit: formulários (inspetor) */
void   ui_group_box(ui_t *u, rect_t r);   /* painel arredondado que agrupa linhas */
/* linha i de um grupo: desenha o rótulo à esquerda e o separador; devolve a área do controle (à direita) */
rect_t ui_form_row(ui_t *u, rect_t group, int i, int row_h, int nrows, const char *label);
enum { DETAIL_NONE, DETAIL_DISCLOSURE, DETAIL_INFO, DETAIL_MENU };
/* valor secundário alinhado à direita, com acessório opcional (›, ⓘ, …); devolve true no clique */
bool ui_detail(ui_t *u, int id, rect_t row, const char *value, int accessory);
/* botão pop-up compacto: rótulo + círculo ⌃⌄ alinhados à direita da linha */
int  ui_popup_inline(ui_t *u, int id, rect_t row, const char **items, int n, int sel);
/* stepper: campo numérico com setas ▲▼ à direita (r inclui as setas) */
bool ui_stepper(ui_t *u, int id, rect_t r, int *v, int lo, int hi);

/* ------------------------------------------------------------ diálogo de salvar (modal do sistema) */
/* button: 0 apagar, 1 cancelar, 2 salvar; dir = pasta escolhida em "Onde" */
typedef void (*save_cb)(int button, const char *dir, const char *name, int format, void *ctx);
void save_dialog_show(const char *title, const char *msg, const char *name, const char **formats, int nformats,
                      bool allow_delete, save_cb cb, void *ctx);
bool save_dialog_open(void);

void glyph_updown(surface_t *s, int x, int y, int sz, u32 c);
void glyph_chevron_down(surface_t *s, int x, int y, int sz, u32 c);
void glyph_heart(surface_t *s, int x, int y, int sz, u32 c);
void glyph_heart_slash(surface_t *s, int x, int y, int sz, u32 c);

/* diálogo modal simples (alerta estilo folha) */
typedef void (*alert_cb)(int button, void *ctx);
void alert_show(const char *title, const char *msg, const char **buttons, int nbuttons, alert_cb cb, void *ctx);
