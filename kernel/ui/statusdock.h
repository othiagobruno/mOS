/* Faixa de status items da barra de menus e o estado de Dock que o NSDockTile (I19) vai usar.
 * O desenho e o clique ficam no desktop; aqui estão a lista, o layout, o hit-test e o selo.
 * Compila no kernel e no teste de host (-DMOS_SB_HOST). */
#pragma once

#ifdef MOS_SB_HOST
#ifndef MOS_SB_U32
#define MOS_SB_U32
typedef unsigned int u32;
#endif
#ifndef MOS_SB_I32
#define MOS_SB_I32
typedef int i32;
#endif
#endif

#define SB_IMG_MAX 64
#define SB_TITLE_MAX 96
#define SB_LABEL_MAX 80
#define SB_MENU_MAX 16

typedef struct {
    int x, w, y, h;
    int img_x, img_y, img_w, img_h; /* 0 se não há imagem */
    int title_x;                    /* -1 se o título não entra no retângulo */
    int open;
    int pid;
    u32 id;
    char title[SB_TITLE_MAX];
} sb_geom_t;

/* Pacote que o AppKit manda em DEV_STATUS / SB_SET (little-endian):
 *   u32 id; i32 length; u32 flags (bit 0 = visível); u32 iw; u32 ih; u32 nitems;
 *   u32 tlen; char title[tlen];
 *   nitems vezes: u32 action, u32 mflags (bit 0 desabilitado, bit 1 separador), u32 llen, char label[llen];
 *   u32 pixels[iw*ih] ARGB não pré-multiplicado, se iw e ih forem > 0 (teto 64x64).
 * length: -1 variável, -2 quadrado, >0 largura em pixels.
 * O primeiro item registrado fica mais à direita (encostado nos ícones do sistema). */
int sb_upsert_wire(int pid, const void *buf, u32 len);
void sb_remove(int pid, u32 id);
void sb_remove_pid(int pid);
void sb_reset(void);
int sb_count(void);

/* right_x: borda esquerda dos ícones do sistema. text_w mede o título (NULL = 8 px por byte). */
int sb_layout(int screen_w, int bar_h, int right_x, int (*text_w)(const char *s, int n, void *ctx), void *ctx);
const sb_geom_t *sb_geoms(int *n);
int sb_hit(int x, int y, int *pid, u32 *id);
/* 1 = abriu (e fechou os outros), 2 = fechou o que já estava aberto, 0 = errou o clique. */
int sb_click(int x, int y, int *pid, u32 *id);
void sb_close_open(void);
int sb_is_open(int pid, u32 id);
const u32 *sb_image(int pid, u32 id, int *w, int *h); /* pré-multiplicado; NULL se não há */
int sb_menu_count(int pid, u32 id);
/* devolve o rótulo (ponteiro estável até o próximo upsert) ou NULL */
const char *sb_menu_label(int pid, u32 id, int index, u32 *action, int *disabled, int *separator);

/* Dock por processo. visible 0 esconde o ícone (LSUIElement / prohibited). O padrão é visível.
 * O selo: texto vazio tira; inteiro puro também vira o número antigo; "99+" e "•" ficam texto. */
int dock_tile_set_visible(int pid, int visible);
int dock_tile_is_hidden(int pid);
int dock_tile_set_badge(int pid, const char *text);
int dock_tile_badge_n(int pid);
const char *dock_tile_badge_text(int pid);
void dock_tile_remove(int pid);
void dock_tile_reset(void);
/* 1 se há selo para pintar. out recebe o texto ("3", "99+", "•"). */
int dock_badge_paint(int n, const char *text, char *out, int cap);
