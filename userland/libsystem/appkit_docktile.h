/* NSDockTile do app (I19). O teste de host compila appkit_docktile.c com -DCX_DOCK_HOST.
 * O desenho do selo no Dock é do kernel (dock_set_badge_label). A view de conteúdo
 * fica guardada aqui: o Dock ainda não a pinta no ícone (K19). */
#pragma once

void cx_dock_reset(void);
void cx_dock_set_badge(const char *label);
const char *cx_dock_badge(void);
void cx_dock_set_content(void *view);
void *cx_dock_content(void);
void cx_dock_display(void);
int cx_dock_displayed(void);
void cx_dock_set_shows_app_badge(int on);
int cx_dock_shows_app_badge(void);
/* Texto publicado para o Dock. Vazio quando o selo está oculto. */
const char *cx_dock_shown(void);
void cx_dock_size(double *w, double *h);
void cx_dock_set_owner(void *owner);
void *cx_dock_owner(void);

typedef void (*cx_dock_sink_fn)(const char *label, void *ctx);
void cx_dock_set_sink(cx_dock_sink_fn fn, void *ctx);

/* 1 se o NSDockTile implementa o seletor. Seletor desconhecido devolve 0 e não aborta. */
int cx_dock_selector(const char *name);
int cx_dock_unknown(const char *name);
int cx_dock_unknown_count(void);

/* Chama setDockTile: se o ponteiro existir. Sem o método, o plugin carrega e ninguém é chamado. */
typedef void (*cx_dock_plugin_fn)(void *plugin, void *tile);
int cx_dock_load_plugin(cx_dock_plugin_fn set_dock_tile, void *plugin);
int cx_dock_load_count(void);

/* Caminho ausente ou vazio: 0, sem abortar. O binário real entra por cx_docktile_load_path. */
int cx_dock_load_path(const char *path);
