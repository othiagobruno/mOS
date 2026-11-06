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
    app_componentes;

/* fonte monoespaçada (simulada em células fixas) para o Terminal e o Console */
int  mono_cell_w(float size);
void mono_draw(surface_t *s, float size, int x, int baseline, u32 color, const char *t, int maxcols);

/* medição de CPU ociosa (Monitor de Atividade) */
extern volatile u64 g_idle_ticks;
