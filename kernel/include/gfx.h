/* mOS — motor gráfico: superfícies, primitivas antialiasadas e texto TrueType */
#pragma once
#include "kernel.h"

/* cores em 0xAARRGGBB não pré-multiplicadas na API; superfícies guardam pré-multiplicado */
#define RGB(r, g, b)      (0xFF000000u | ((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b))
#define RGBA(r, g, b, a)  (((u32)(a) << 24) | ((u32)(r) << 16) | ((u32)(g) << 8) | (u32)(b))
#define C_A(c) ((c) >> 24)
#define C_R(c) (((c) >> 16) & 0xFF)
#define C_G(c) (((c) >> 8) & 0xFF)
#define C_B(c) ((c) & 0xFF)
#define WITH_ALPHA(c, a) (((c) & 0xFFFFFF) | ((u32)(a) << 24))

typedef struct { int x, y, w, h; } rect_t;

typedef struct {
    int w, h, stride;
    u32 *px;
    rect_t clip;
    int ox, oy; /* translação aplicada às coordenadas de desenho */
} surface_t;

surface_t *surface_new(int w, int h);
void       surface_free(surface_t *s);
void       surface_resize(surface_t *s, int w, int h);
void       surface_clip(surface_t *s, rect_t r);          /* define o clip (coords locais) */
void       surface_reset_clip(surface_t *s);
rect_t     surface_push_clip(surface_t *s, rect_t r);     /* intersecta; devolve o anterior */
void       surface_pop_clip(surface_t *s, rect_t prev);

rect_t rect_make(int x, int y, int w, int h);
rect_t rect_intersect(rect_t a, rect_t b);
rect_t rect_union(rect_t a, rect_t b);
bool   rect_empty(rect_t r);
bool   rect_contains(rect_t r, int x, int y);
rect_t rect_inset(rect_t r, int d);

u32  color_premul(u32 c);
u32  color_mix(u32 a, u32 b, int t /*0..255*/);
u32  color_lighten(u32 c, int amt);
u32  color_alpha(u32 c, int a);   /* multiplica o alfa */

void gfx_clear(surface_t *s, u32 c);
void gfx_fill(surface_t *s, int x, int y, int w, int h, u32 c);
void gfx_pixel(surface_t *s, int x, int y, u32 c, int cover);
void gfx_hline(surface_t *s, int x, int y, int w, u32 c);
void gfx_vline(surface_t *s, int x, int y, int h, u32 c);
void gfx_rect(surface_t *s, int x, int y, int w, int h, u32 c);           /* contorno 1px */
void gfx_round_rect(surface_t *s, float x, float y, float w, float h, float r, u32 c);
void gfx_round_rect_stroke(surface_t *s, float x, float y, float w, float h, float r, float t, u32 c);
void gfx_round_rect_grad(surface_t *s, float x, float y, float w, float h, float r, u32 top, u32 bottom);
void gfx_round_rect_ex(surface_t *s, float x, float y, float w, float h, float rtl, float rtr, float rbr, float rbl, u32 top, u32 bottom);
void gfx_circle(surface_t *s, float cx, float cy, float r, u32 c);
void gfx_circle_stroke(surface_t *s, float cx, float cy, float r, float t, u32 c);
void gfx_ring_arc(surface_t *s, float cx, float cy, float r, float t, float a0, float a1, u32 c);
void gfx_line(surface_t *s, float x0, float y0, float x1, float y1, float t, u32 c);
void gfx_grad_v(surface_t *s, int x, int y, int w, int h, u32 top, u32 bottom);
void gfx_grad_h(surface_t *s, int x, int y, int w, int h, u32 left, u32 right);
void gfx_shadow(surface_t *s, int x, int y, int w, int h, float r, float blur, int alpha, int dy);
void gfx_polygon(surface_t *s, const float *xy, int n, u32 c); /* preenchimento AA, regra não-zero */

void gfx_blit(surface_t *d, const surface_t *src, int dx, int dy);            /* opaco */
void gfx_blit_rect(surface_t *d, const surface_t *src, rect_t sr, int dx, int dy);
void gfx_blend(surface_t *d, const surface_t *src, int dx, int dy, int alpha); /* com alfa */
void gfx_blend_rect(surface_t *d, const surface_t *src, rect_t sr, int dx, int dy, int alpha);
void gfx_blit_scaled(surface_t *d, const surface_t *src, int dx, int dy, int dw, int dh, int alpha);
void gfx_blur(surface_t *s, rect_t r, int radius);
void gfx_mask(surface_t *s, int x, int y, const u8 *mask, int mw, int mh, int mstride, u32 c);

/* ---------------------------------------------------------------- material de vidro ("Liquid Glass")
 * Lê o que já está desenhado atrás do retângulo arredondado, desfoca (com subamostragem 2x),
 * realça a saturação, curva o fundo perto da borda (refração de lente), aplica a tinta e
 * acende a borda com um brilho especular que depende do ângulo da luz (topo-esquerda).
 * O fundo lido se estende GLASS_REACH(g) pixels além do retângulo: o compositor garante
 * que essa faixa esteja atualizada antes de desenhar o vidro. */
typedef struct {
    int   blur;        /* raio do desfoque em px */
    int   saturation;  /* 256 = neutro; 320 = +25% */
    u32   tint;        /* tinta (0xAARRGGBB, não pré-multiplicada) sobre o fundo desfocado */
    float refract;     /* deslocamento máximo do fundo na borda, em px */
    float bezel;       /* largura da faixa que refrata, em px */
    int   rim;         /* brilho especular da borda 0..255 */
    int   sheen;       /* brilho difuso no topo 0..255 */
    int   opacity;     /* 0..255 (0 = 255) */
    float chroma;      /* aberração cromática na faixa que refrata: 0 = nenhuma; 1 = vermelho desloca
                        * +22% e verde +11% além do azul (franja colorida na borda, como numa lente) */
} glass_t;
#define GLASS_CHROMA_R 0.22f
#define GLASS_CHROMA_G 0.11f
#define GLASS_REACH(g) ((g)->blur * 2 + (int)((g)->refract * (1 + GLASS_CHROMA_R * (g)->chroma)) + 3)
void gfx_glass(surface_t *s, float x, float y, float w, float h, float r, const glass_t *g);
/* brilho especular só (para controles desenhados dentro de janelas, sem fundo para desfocar) */
void gfx_specular_rim(surface_t *s, float x, float y, float w, float h, float r, int strength);

/* ---------------------------------------------------------------- texto */
enum { FONT_REGULAR, FONT_MEDIUM, FONT_SEMIBOLD, FONT_BOLD, FONT_COUNT };
typedef struct font font_t;
void    font_init(void);
font_t *font_get(int weight);
int     text_width(font_t *f, float size, const char *utf8);
int     text_width_n(font_t *f, float size, const char *utf8, int bytes);
int     text_draw(surface_t *s, font_t *f, float size, int x, int baseline, u32 color, const char *utf8);
int     text_draw_n(surface_t *s, font_t *f, float size, int x, int baseline, u32 color, const char *utf8, int bytes);
void    text_draw_center(surface_t *s, font_t *f, float size, rect_t r, u32 color, const char *utf8);
int     text_draw_ellipsis(surface_t *s, font_t *f, float size, int x, int baseline, int maxw, u32 color, const char *utf8);
int     text_ascent(font_t *f, float size);
int     text_descent(font_t *f, float size);
int     text_index_at(font_t *f, float size, const char *utf8, int px); /* deslocamento em bytes */
bool    font_has_glyph(font_t *f, u32 cp);
int     text_wrap(surface_t *s, font_t *f, float size, rect_t r, int line_h, u32 color, const char *utf8, bool draw);
u32     utf8_decode(const char **p);
int     utf8_encode(u32 cp, char *out);
int     utf8_prev(const char *s, int i);
int     utf8_next(const char *s, int i);

/* ---------------------------------------------------------------- framebuffer */
extern surface_t *g_screen; /* backbuffer do compositor */
void fb_init(void);
void fb_flush(rect_t r);

/* ---------------------------------------------------------------- monitores
 * Todos os monitores formam um só espaço de coordenadas (o g_screen), arranjados livremente como no
 * organizador do Mac. O monitor 0 é sempre o principal (barra de menus e Dock), em qualquer posição.
 * Os drivers de GPU registram saídas extras com display_add() antes do fb_init(). */
#define MAX_DISPLAYS 4
int         display_count(void);
rect_t      display_rect(int i);
const char *display_name(int i);
int         display_output_of(int i);        /* saída de vídeo que mostra o monitor i */
int         display_at(int x, int y);        /* -1: fora de qualquer monitor */
int         display_for(rect_t r);           /* monitor com a maior parte de r (0 se nenhum) */
void        display_clamp(int *x, int *y);   /* traz o ponto para dentro de algum monitor */
bool        display_add(int w, int h, const char *name);

/* arranjo (Ajustes > Monitores): posição de cada saída, qual é a principal e o modo espelho */
typedef struct { int n; int x[MAX_DISPLAYS], y[MAX_DISPLAYS]; int primary; bool mirror; } display_layout_t;
int         display_output_count(void);
bool        display_output_info(int out, int *w, int *h, const char **name);
void        display_get_layout(display_layout_t *L);
bool        display_layout_valid(const display_layout_t *L);  /* sem sobreposição, todos encostados */
bool        display_set_layout(const display_layout_t *L);    /* aplica na hora e salva */
bool        display_scanout(int out, rect_t *r);              /* pedaço do g_screen que a saída mostra */
void        display_identify(int out);                       /* contorno vermelho na saída (-1 = nenhum) */
int         display_identifying(void);
