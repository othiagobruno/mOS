/* mOS — motor web do Horizonte: DOM, CSS, layout, pintura e decodificadores de imagem */
#pragma once
#include "ui.h"
#include "vfs.h"
#include "net.h"

/* ============================================================ utilidades */
typedef struct achunk { struct achunk *next; usize used, cap; } achunk_t;
typedef struct { achunk_t *head; usize total; } arena_t;
void *arena_alloc(arena_t *a, usize n);                 /* memória zerada, alinhada a 8 */
char *arena_strndup(arena_t *a, const char *s, int n);
void  arena_free(arena_t *a);

typedef struct { char *p; int len, cap; } sbuf_t;
void sb_putc(sbuf_t *b, char c);
void sb_putn(sbuf_t *b, const char *s, int n);
void sb_puts(sbuf_t *b, const char *s);
void sb_printf(sbuf_t *b, const char *fmt, ...);
void sb_html(sbuf_t *b, const char *s);                 /* escapa &<>" */
void sb_urlenc(sbuf_t *b, const char *s);               /* application/x-www-form-urlencoded */
void sb_free(sbuf_t *b);

/* URLs */
void url_resolve(const char *base, const char *rel, char *out, int cap);
bool url_host(const char *url, char *out, int cap);
bool url_scheme(const char *url, char *out, int cap);
void url_decode_path(const char *in, char *out, int cap); /* %20 → espaço */
bool url_same_doc(const char *a, const char *b);          /* iguais exceto pelo #fragmento */
const char *url_fragment(const char *url);                /* aponta após '#', ou NULL */

/* conjuntos de caracteres: converte para UTF-8 (devolve buffer kmalloc) */
char *charset_to_utf8(const u8 *data, usize n, const char *hint, usize *outlen);
bool  utf8_valid(const u8 *d, usize n);

/* ============================================================ DOM */
enum { N_DOC, N_ELEM, N_TEXT };

enum {
    T_UNKNOWN, T_HTML, T_HEAD, T_BODY, T_TITLE, T_META, T_LINK, T_STYLE, T_SCRIPT, T_BASE, T_NOSCRIPT,
    T_DIV, T_P, T_SPAN, T_A, T_B, T_STRONG, T_I, T_EM, T_U, T_S, T_STRIKE, T_DEL, T_INS, T_SMALL, T_BIG,
    T_SUB, T_SUP, T_CODE, T_KBD, T_SAMP, T_TT, T_VAR, T_PRE, T_XMP, T_LISTING, T_PLAINTEXT, T_BR, T_WBR, T_HR,
    T_H1, T_H2, T_H3, T_H4, T_H5, T_H6, T_UL, T_OL, T_LI, T_DL, T_DT, T_DD, T_MENU, T_DIR,
    T_TABLE, T_CAPTION, T_THEAD, T_TBODY, T_TFOOT, T_TR, T_TD, T_TH, T_COL, T_COLGROUP,
    T_IMG, T_FORM, T_INPUT, T_BUTTON, T_SELECT, T_OPTION, T_OPTGROUP, T_TEXTAREA, T_LABEL, T_FIELDSET, T_LEGEND,
    T_BLOCKQUOTE, T_CENTER, T_FONT, T_NOBR, T_ADDRESS, T_ARTICLE, T_ASIDE, T_FOOTER, T_HEADER, T_MAIN, T_NAV,
    T_SECTION, T_FIGURE, T_FIGCAPTION, T_DETAILS, T_SUMMARY, T_HGROUP, T_MARK, T_CITE, T_ABBR, T_Q, T_DFN,
    T_IFRAME, T_SVG, T_MATH, T_OBJECT, T_EMBED, T_VIDEO, T_AUDIO, T_CANVAS, T_TEMPLATE, T_AREA, T_MAP,
    T_PARAM, T_SOURCE, T_TRACK, T_NOEMBED, T_NOFRAMES, T_FRAMESET, T_FRAME, T_PICTURE, T_TIME, T_DATA, T_DIALOG,
    T_COUNT
};

typedef struct { const char *name, *value; } attr_t;

struct style;
struct box;
typedef struct ctrl ctrl_t;
typedef struct img img_t;

typedef struct node {
    u8 type;
    u8 has_block;          /* descendente com display de bloco (para inline transparente) */
    u16 tag;
    const char *name;      /* nome em minúsculas (elementos) */
    char *text;            /* texto (nós de texto) */
    int len;
    attr_t *attrs;
    int nattrs, acap;
    struct node *parent, *first, *last, *next, *prev;
    struct style *st;
    struct node *link;     /* <a href> mais próximo (inclusive) */
    ctrl_t *ctrl;          /* estado de controle de formulário */
    img_t *img;            /* imagem associada (<img>) */
    int ay;                /* posição y (para âncoras), -1 se desconhecida */
    int index;             /* ordem no documento */
} node_t;

const char *node_attr(node_t *n, const char *name);
node_t *node_find_tag(node_t *root, int tag);
node_t *node_by_id(node_t *root, const char *id);
void    node_text_content(node_t *n, sbuf_t *out);
int     tag_lookup(const char *name);
const char *html_tag_name(int tag);

/* ============================================================ CSS */
enum { U_NONE, U_PX, U_PCT, U_AUTO };
typedef struct { float v; u8 u; } len_t;

enum { D_INLINE, D_BLOCK, D_INLINE_BLOCK, D_LIST_ITEM, D_NONE, D_TABLE, D_INLINE_TABLE, D_ROW_GROUP, D_ROW,
       D_CELL, D_CAPTION, D_COLUMN };
enum { WS_NORMAL, WS_PRE, WS_NOWRAP, WS_PRE_WRAP, WS_PRE_LINE };
enum { TA_LEFT, TA_CENTER, TA_RIGHT, TA_JUSTIFY };
enum { LS_DISC, LS_CIRCLE, LS_SQUARE, LS_DECIMAL, LS_LALPHA, LS_UALPHA, LS_LROMAN, LS_UROMAN, LS_NONE };
enum { VA_BASELINE, VA_MIDDLE, VA_TOP, VA_BOTTOM, VA_SUB, VA_SUPER, VA_TEXT_TOP, VA_TEXT_BOTTOM };
enum { TT_NONE, TT_UPPER, TT_LOWER, TT_CAPITALIZE };

typedef struct style {
    u8 display, ws, align, weight, list, valign, transform;
    u8 italic, underline, strike, mono, hidden, collapse, nowrap_cell, center_block, flexrow, border_box;
    float size;            /* px (já com zoom) */
    float lh;              /* altura de linha: lh_mode 0 normal, 1 fator, 2 px */
    u8 lh_mode;
    u32 color, bg;         /* 0xAARRGGBB, bg alfa 0 = transparente */
    u32 grad_top, grad_bot;/* gradiente linear vertical (se grad_bot != 0) */
    len_t margin[4], pad[4];       /* topo, direita, baixo, esquerda */
    float bw[4];                   /* larguras das bordas (px) */
    u32 bc[4];
    len_t width, height, min_w, max_w, min_h, indent;
    float radius;
    float spacing;                 /* border-spacing */
    u8 shadow;
    u8 border_attr;                /* <table border> (bordas nas células) */
    float cellpad;                 /* cellpadding (-1 = não definido) */
} style_t;

typedef struct sheet sheet_t;
sheet_t *css_parse(const char *src, int len, int viewport_w, sheet_t *append);
void     css_free(sheet_t *s);
void     css_style_tree(node_t *root, sheet_t *ua, sheet_t *author, arena_t *a, float zoom, int vw, int vh);
sheet_t *css_ua_sheet(void);
bool     css_color(const char *s, u32 *out);

/* ============================================================ imagens */
enum { IMG_PENDING, IMG_LOADING, IMG_OK, IMG_ERROR };
struct img {
    char url[512];
    int state;
    http_req_t *req;
    surface_t *surf;
    surface_t *scaled;     /* cache redimensionado */
    struct img *next;
};
surface_t *image_decode(const u8 *d, usize n);
bool raw_inflate(const u8 *d, usize n, u8 *out, usize cap, usize *olen);   /* deflate cru (zip) */
surface_t *image_scale(const surface_t *src, int w, int h); /* filtro de área / bilinear */

/* ============================================================ controles de formulário */
enum { C_TEXT, C_PASSWORD, C_SUBMIT, C_RESET, C_BUTTON, C_CHECK, C_RADIO, C_SELECT, C_TEXTAREA, C_IMAGE, C_HIDDEN };
struct ctrl {
    int kind, id;
    char *buf;
    int cap;
    bool checked;
    int sel, nopts;
    const char **opts;     /* rótulos (select) */
    const char **vals;
    node_t *form;
    node_t *node;
    char label[96];
    int cols, rows;
};

/* ============================================================ layout */
enum { B_BLOCK, B_LINES, B_TABLE, B_ROW, B_CELL, B_REPLACED };
enum { IT_WORD, IT_SPACE, IT_BREAK, IT_ATOM, IT_SPACER, IT_ANCHOR, IT_TAB };

typedef struct item {
    u8 kind, glue;
    short w;
    int len;
    const char *t;
    style_t *st;
    node_t *node;          /* elemento de origem */
    struct box *atom;
    u32 bg;                /* fundo de elemento inline ancestral */
} item_t;

typedef struct frag {
    int x, y, w, h;        /* relativos ao documento */
    int base;              /* linha de base absoluta */
    int sp;                /* largura do espaço seguinte (decorações) */
    const char *t;
    int len;
    style_t *st;
    node_t *node;
    struct box *atom;
    u32 bg;
} frag_t;

typedef struct line { int y, h, first, n; } line_t;

typedef struct box {
    u8 kind, inline_block, has_minmax, is_anon;
    node_t *node;
    style_t *st;
    int x, y, w, h;        /* caixa de borda, relativa ao documento */
    int ob;                /* limite inferior incluindo transbordo */
    float m[4], p[4], b[4];
    struct box *parent, *first, *last, *next;
    /* B_LINES */
    item_t *items;
    int nitems;
    frag_t *frags;
    int nfrags;
    line_t *lines;
    int nlines;
    /* tabelas */
    int colspan, rowspan, ncols;
    int *colw;
    /* tamanho intrínseco */
    int minw, maxw;
    /* marcador de lista */
    const char *marker;
    /* B_REPLACED */
    int iw, ih;
} box_t;

/* ============================================================ documento */
typedef struct doc {
    arena_t arena;         /* DOM e estilos */
    arena_t larena;        /* caixas e fragmentos */
    node_t *root, *html, *head, *body;
    char title[160];
    char url[1024];
    char base[1024];
    sheet_t *sheet;
    img_t *images;
    int nimages;
    ctrl_t **ctrls;
    int nctrls, cctrls;
    box_t *rootbox;
    int layout_w, height, vw, vh;
    float zoom, styled_zoom;
    bool need_layout, need_style;
    u32 canvas_bg;
    u64 last_layout;
    u32 layout_ms;          /* duração do último layout */
    int node_count;
} doc_t;

doc_t *doc_parse(const char *html, usize len, const char *url);
void   html_parse_fragment(doc_t *d, node_t *parent, const char *html, int len);  /* innerHTML */
bool   html_sniff_charset(const u8 *d, usize n, char *out, int cap);
void   doc_free(doc_t *d);
void   doc_layout(doc_t *d, int width, int vh, float zoom);
void   doc_paint(doc_t *d, ui_t *u, rect_t view, int scroll, node_t *hover_link);
frag_t *doc_hit(doc_t *d, int x, int y, box_t **owner);  /* coordenadas do documento */
box_t *doc_hit_atom(doc_t *d, int x, int y);
int    doc_anchor_y(doc_t *d, const char *name);

/* chamado pela pintura para controles de formulário (implementado em horizonte.c) */
void web_paint_control(ui_t *u, doc_t *d, ctrl_t *c, rect_t r);
