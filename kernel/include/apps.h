/* mOS — interfaces compartilhadas entre os aplicativos */
#pragma once
#include "ui.h"
#include "vfs.h"

void open_path(const char *path);
void finder_open_path(const char *path);
void finder_open_trash(void);
void editor_open_file(const char *path);
void browser_open_url(const char *url);
void prefs_open_pane(int pane);
void about_open(void);
void viewer_open_file(const char *path);

typedef void icon_fn(surface_t *, int, int, int);
icon_fn icon_arquivos, icon_horizonte, icon_terminal, icon_prefs, icon_editor, icon_notas, icon_calc,
    icon_calendario, icon_atividade, icon_console, icon_musica, icon_minas, icon_visualizador;

/* cada app expõe sua definição */
extern app_t app_arquivos, app_horizonte, app_terminal, app_preferencias, app_editor, app_notas,
    app_calculadora, app_calendario, app_atividade, app_console, app_musica, app_minas, app_visualizador, app_sobre, app_instalador,
    app_componentes, app_lembretes, app_contatos;
void contacts_import_file(const char *path);
void calendar_import_file(const char *path);
void books_open_file(const char *path);

/* fonte monoespaçada (simulada em células fixas) para o Terminal e o Console */
int  mono_cell_w(float size);
void mono_draw(surface_t *s, float size, int x, int baseline, u32 color, const char *t, int maxcols);

/* medição de CPU ociosa (Monitor de Atividade) */
extern volatile u64 g_idle_ticks;

/* área de texto multilinha reutilizável (o motor do Editor de Texto), para outros apps.
 * bg 0 = T.content_bg, 1 = transparente; fg 0 = T.text; pad 0 = 14 px */
typedef struct textarea textarea_t;
textarea_t *textarea_new(float size, u32 bg, u32 fg, int pad);
void        textarea_set(textarea_t *t, const char *s, int n);   /* n < 0: até o fim */
const char *textarea_text(textarea_t *t, int *len);
bool        textarea_dirty(textarea_t *t, bool clear);
bool        textarea_focused(textarea_t *t);
void        textarea_focus(textarea_t *t, bool on);
void        textarea_colors(textarea_t *t, u32 bg, u32 fg);
void        textarea_set_size(textarea_t *t, float size);
void        textarea_paint(window_t *w, ui_t *u, textarea_t *t, rect_t area);
void        textarea_insert(textarea_t *t, const char *s);
void        textarea_free(textarea_t *t);
extern app_t app_relogio;
extern app_t app_stickies;
extern app_t app_xadrez;
extern app_t app_sysinfo;
extern app_t app_disco;
extern app_t app_captura;
extern app_t app_medidor;
extern app_t app_fontes;
extern app_t app_grapher;
extern app_t app_gravador;
extern app_t app_fotos;
extern app_t app_diario;
extern app_t app_dicas;
extern app_t app_freeform;
extern app_t app_tempo;
extern app_t app_mapas;
extern app_t app_bolsa;
extern app_t app_script;
extern app_t app_senhas;
extern app_t app_dicionario;
extern app_t app_chaves;
extern app_t app_audiomidi;
extern app_t app_paciencia;
extern app_t app_jogos;
extern app_t app_atalhos;
extern app_t app_mail;
extern app_t app_livros;
