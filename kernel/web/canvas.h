/* mOS — Canvas 2D do motor web (kernel/web/canvas.c): bitmap e estado de um <canvas>, usados pelo
 * layout/pintura (layout.c) e pela API JS (canvas_js.c). Implementação própria a partir da especificação HTML. */
#pragma once
#include "web.h"
#include "gfx.h"
#include "raster.h"

/* comandos de caminho (MOVE x y, LINE x y, CUBIC x1 y1 x2 y2 x3 y3, CLOSE), num vetor de floats.
 * base[0..PC_HDR) guarda o cabeçalho para o Path2D, que vive dentro de um ArrayBuffer do JS. */
enum { PC_MOVE, PC_LINE, PC_CUBIC, PC_CLOSE };
#define PC_HDR 8
typedef struct {
    float *base;          /* kmalloc: cabeçalho + comandos */
    int n, cap;           /* floats de comandos usados / capacidade (sem o cabeçalho) */
    float cx, cy, sx, sy; /* ponto atual e início do subcaminho (no espaço gravado) */
    bool has;             /* há subcaminho */
    int last;             /* posição do último comando (-1 = nenhum) */
} pcmd_t;
static inline float *pc_v(const pcmd_t *pc) { return pc->base + PC_HDR; }
void pc_free(pcmd_t *pc);
void pc_reset(pcmd_t *pc);
bool pc_move(pcmd_t *pc, const mat_t *m, float x, float y);
bool pc_line(pcmd_t *pc, const mat_t *m, float x, float y);
bool pc_quad(pcmd_t *pc, const mat_t *m, float cpx, float cpy, float x, float y);
bool pc_cubic(pcmd_t *pc, const mat_t *m, float x1, float y1, float x2, float y2, float x, float y);
void pc_close(pcmd_t *pc);
bool pc_rect(pcmd_t *pc, const mat_t *m, float x, float y, float w, float h);
/* raios em espaço do usuário: [tl.x, tl.y, tr.x, tr.y, br.x, br.y, bl.x, bl.y] */
bool pc_round_rect(pcmd_t *pc, const mat_t *m, float x, float y, float w, float h, const float *r);
bool pc_ellipse(pcmd_t *pc, const mat_t *m, float x, float y, float rx, float ry, float rot, float a0, float a1, bool ccw);
/* arcTo: o ponto atual (gravado em espaço de m) volta ao espaço do usuário por m⁻¹ */
bool pc_arc_to(pcmd_t *pc, const mat_t *m, float x1, float y1, float x2, float y2, float r);
bool pc_append(pcmd_t *pc, const pcmd_t *src, const mat_t *m);   /* Path2D.addPath */
/* reproduz os comandos num path_t, transformados por m; drop_empty tira subcaminhos de um ponto só (traço) */
void pc_build(const pcmd_t *pc, const mat_t *m, float tol, bool drop_empty, path_t *out);

/* ---------------------------------------------------------------- estilos de pintura */
enum { PS_COLOR, PS_LINEAR, PS_RADIAL, PS_CONIC, PS_PATTERN };
typedef struct { float off; u32 c; } cstop_t;            /* c: ARGB não pré-multiplicado */
/* gradiente e padrão moram em blobs (ArrayBuffer do JS) com este cabeçalho */
typedef struct {
    int kind;                       /* PS_LINEAR, PS_RADIAL, PS_CONIC */
    float x0, y0, r0, x1, y1, r1, ang;
    int nstops;
    cstop_t stops[];                /* em ordem de offset (estável) */
} cgrad_t;
enum { REP_REPEAT, REP_X, REP_Y, REP_NONE };
typedef struct {
    int w, h, rep;
    mat_t m;                        /* setTransform do padrão */
    u32 px[];                       /* pré-multiplicado, w*h */
} cpat_t;
typedef struct { int kind; u32 color; const void *obj; } cpaint_t;   /* obj: cgrad_t* ou cpat_t* (vivo no JS) */

enum { OP_SRC_OVER, OP_SRC_IN, OP_SRC_OUT, OP_SRC_ATOP, OP_DST_OVER, OP_DST_IN, OP_DST_OUT, OP_DST_ATOP, OP_LIGHTER,
       OP_COPY, OP_XOR, OP_MULTIPLY, OP_SCREEN, OP_OVERLAY, OP_DARKEN, OP_LIGHTEN, OP_DODGE, OP_BURN, OP_HARD,
       OP_SOFT, OP_DIFF, OP_EXCL, OP_HUE, OP_SAT, OP_COLOR, OP_LUM, OP_COUNT };
extern const char *const canvas_op_names[OP_COUNT];
enum { CTA_START, CTA_END, CTA_LEFT, CTA_RIGHT, CTA_CENTER };
enum { TB_ALPHABETIC, TB_TOP, TB_HANGING, TB_MIDDLE, TB_IDEOGRAPHIC, TB_BOTTOM };

/* ctx.filter: funções de filtro do CSS, na ordem */
enum { CF_BLUR, CF_BRIGHT, CF_CONTRAST, CF_GRAY, CF_HUE, CF_INVERT, CF_OPACITY, CF_SAT, CF_SEPIA, CF_SHADOW };
typedef struct { u8 kind; float v, x, y; u32 color; } cfilt_t;
#define CANVAS_MAX_FILTERS 12

typedef struct {
    mat_t m;
    float alpha;
    int op;
    cpaint_t fill, stroke;
    float lw, miter, dashoff;
    u8 cap, join;
    float *dash;                    /* kmalloc (cópia por estado) */
    int ndash;
    float shx, shy, shblur;
    u32 shcolor;
    char font[160];                 /* forma serializada */
    float fsize;
    int fweight;                    /* 100..900 */
    bool italic;
    u8 align, baseline, rtl;       /* rtl: direction 0 inherit, 1 ltr, 2 rtl */
    bool smooth;
    u8 smoothq;                     /* 0 low, 1 medium, 2 high */
    float letter;                   /* letterSpacing (px) */
    u8 *clip;                       /* máscara W×H (NULL = sem recorte), kmalloc por estado */
    char filter[160];               /* ctx.filter como foi dado ("none" = sem filtro) */
    cfilt_t flt[CANVAS_MAX_FILTERS];
    int nflt;
} cstate_t;

typedef struct canvas2d {
    node_t *node;
    surface_t *s;                   /* bitmap W×H, BGRA pré-multiplicado */
    int w, h;
    bool opaque;                    /* getContext('2d', {alpha:false}) */
    cstate_t st;
    cstate_t *stack;
    int nstack, cstack;
    pcmd_t path;                    /* caminho atual, em pixels do canvas */
    struct canvas2d *next;          /* lista do documento (doc->canvases) */
} canvas2d_t;

canvas2d_t *canvas_of(doc_t *d, node_t *n, bool create);   /* bitmap de um <canvas> (300×150 padrão) */
void canvas_size_attrs(node_t *n, int *w, int *h);
node_t *canvas_detached(doc_t *d, int w, int h);           /* nó <canvas> fora do DOM (OffscreenCanvas) */
void canvas_set_dims(doc_t *d, node_t *n, int w, int h);  /* troca width/height (atributos) e recria o bitmap */
void canvas_resize(canvas2d_t *c, int w, int h);           /* novo bitmap transparente e estado inicial */
void canvas_free_all(doc_t *d);
void canvas_state_init(cstate_t *st);
void canvas_state_copy(cstate_t *dst, const cstate_t *src, int W, int H);
void canvas_state_free(cstate_t *st);
bool canvas_save(canvas2d_t *c);
bool canvas_restore(canvas2d_t *c);                         /* false = pilha vazia */

/* desenho (o estado atual de c decide transformação, recorte, alfa, composição e sombra) */
void canvas_fill(canvas2d_t *c, const pcmd_t *pc, const mat_t *pm, bool evenodd);     /* pm: matriz do caminho */
void canvas_stroke(canvas2d_t *c, const pcmd_t *pc, bool device);                    /* device: caminho em pixels */
void canvas_clip(canvas2d_t *c, const pcmd_t *pc, const mat_t *pm, bool evenodd);
void canvas_clear_rect(canvas2d_t *c, float x, float y, float w, float h);
bool canvas_point_in_path(canvas2d_t *c, const pcmd_t *pc, const mat_t *pm, float x, float y, bool evenodd);
bool canvas_point_in_stroke(canvas2d_t *c, const pcmd_t *pc, bool device, float x, float y);
/* src: pixels pré-multiplicados; retângulo de origem e de destino já normalizados */
void canvas_draw_image(canvas2d_t *c, const u32 *px, int iw, int ih, int stride,
                       float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh);
void canvas_text(canvas2d_t *c, const char *s, int len, float x, float y, float maxw, bool stroke);
typedef struct { float width, l, r, asc, desc, fasc, fdesc, hanging, ideo, alpha; } cmetrics_t;
void canvas_measure(canvas2d_t *c, const char *s, int len, cmetrics_t *m);
bool canvas_set_font(cstate_t *st, const char *v);
/* ImageData: RGBA não pré-multiplicado */
void canvas_get_rgba(canvas2d_t *c, int sx, int sy, int sw, int sh, u8 *out);
void canvas_put_rgba(canvas2d_t *c, const u8 *rgba, int iw, int ih, int dx, int dy, int x0, int y0, int x1, int y1);
/* PNG (RGBA, deflate próprio): devolve bytes kmalloc */
u8 *canvas_png(canvas2d_t *c, usize *len);
u8 *png_encode_rgba(const u8 *rgba, int w, int h, usize *len);
/* cor CSS de canvas: false se inválida; current = cor do elemento <canvas> */
bool canvas_parse_color(const char *s, u32 current, u32 *out);
bool canvas_set_filter(cstate_t *st, const char *s, u32 current);   /* false = valor inválido (o filtro não muda) */
void canvas_color_str(u32 c, char *out, int cap);
