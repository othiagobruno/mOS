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
typedef struct frame frame_t;

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
    frame_t *frame;        /* <iframe>: documento filho (kernel/web/frame.c) */
    struct canvas2d *canvas; /* <canvas>: bitmap e estado 2D (kernel/web/canvas.c) */
    u8 ns;                 /* 0 HTML, 1 SVG, 2 outro XML (nomes e atributos com a caixa original) */
    const char *nsuri;     /* documento XML: namespace do elemento ("" = nenhum; NULL = o padrão de ns) */
    u8 svgdirty;           /* <svg> inline mudado pelo JS: re-serializar e redesenhar */
    int index;             /* ordem no documento */
} node_t;

const char *node_attr(node_t *n, const char *name);
node_t *node_find_tag(node_t *root, int tag);
node_t *node_by_id(node_t *root, const char *id);
void    node_text_content(node_t *n, sbuf_t *out);
int     tag_lookup(const char *name);
const char *html_tag_name(int tag);
/* SVG inline: subárvore XML (nomes/atributos como vieram) sob parent; counter = contador de índices de nós */
struct doc;
void html_svg_fragment(struct doc *d, node_t *parent, const char *src, int len, int *counter);
/* documento XML (DOMParser, responseXML): namespaces, entidades predefinidas e numéricas, CDATA; dados mal formados
 * viram um documento com <parsererror> (como no navegador) */
struct doc *xml_parse_doc(const char *src, int len, const char *url);
void node_xml(node_t *n, sbuf_t *out);   /* serializa uma subárvore SVG como XML */

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
    u8 clip_overflow;              /* overflow diferente de visible (IntersectionObserver recorta por ele) */
} style_t;

typedef struct sheet sheet_t;
sheet_t *css_parse(const char *src, int len, int viewport_w, sheet_t *append);
void     css_free(sheet_t *s);
void     css_style_tree(node_t *root, sheet_t *ua, sheet_t *author, arena_t *a, float zoom, int vw, int vh);
sheet_t *css_ua_sheet(void);
bool     css_color(const char *s, u32 *out);
bool     css_media_ok(const char *query, int viewport_w);          /* NULL ou vazio = vale */
/* imagem a carregar para um <img>: <picture><source type/media/srcset>, srcset (x e w) e src; NULL se nenhuma */
const char *img_pick_src(node_t *img, int viewport_w, float dpr, char *buf, int cap);

/* ============================================================ imagens */
enum { IMG_PENDING, IMG_LOADING, IMG_OK, IMG_ERROR };
struct img {
    char url[512];
    int state;
    http_req_t *req;
    surface_t *surf;
    surface_t *scaled;     /* cache redimensionado */
    char *svg;             /* fonte SVG (kmalloc): redesenhada no tamanho exibido */
    int svglen;
    u32 svgcolor;          /* SVG inline: cor CSS do elemento (currentColor); 0 = preto */
    /* elementos <img> que usam esta imagem: cada um recebe o seu evento load/error (kernel/web/js_dom.c) */
    struct { node_t *n; bool fired; } *own;
    int nown, cown;
    struct img *next;
};
surface_t *image_decode(const u8 *d, usize n);
bool raw_inflate(const u8 *d, usize n, u8 *out, usize cap, usize *olen);   /* deflate cru (zip) */
surface_t *image_scale(const surface_t *src, int w, int h); /* filtro de área / bilinear */
/* WebP (kernel/web/webp.c): VP8, VP8L, ALPH, VP8X; animação devolve o 1º quadro */
surface_t *webp_decode(const u8 *d, usize n);
u32 *webp_decode_argb(const u8 *d, usize n, int *w, int *h);   /* ARGB não pré-multiplicado, kfree no resultado */
/* SVG (kernel/web/svg.c) */
bool svg_is(const u8 *d, usize n);                                   /* tem um elemento <svg> no começo */
bool svg_intrinsic(const char *src, int len, float *w, float *h);
surface_t *svg_render(const char *src, int len, int w, int h);       /* w/h 0 = tamanho intrínseco */
surface_t *svg_render_ex(const char *src, int len, int w, int h, u32 color);   /* color: currentColor herdado */
/* imagem de um documento por URL (resolvida na base, reaproveitada, carregada pelo pump de imagens) */
struct doc;
img_t *doc_image(struct doc *d, const char *src);
/* <img>: escolhe src/srcset/<picture>, liga node->img e registra o elemento como dono (para os eventos) */
void   img_update_node(struct doc *d, node_t *n, int viewport_w, float dpr);
/* põe os bytes baixados numa imagem do documento: decodifica, e guarda a fonte se for SVG */
void img_set_data(img_t *im, const u8 *d, usize n);

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
    /* iframes (kernel/web/frame.c) */
    frame_t *frames;
    struct js_page *frames_js;  /* página JS deste documento, dona dos iframes */
    struct frame_policy *fpolicy;  /* anfitrião decide as navegações dos iframes (herdado pelos documentos filhos) */
    int frame_depth;            /* 0 = documento principal */
    bool transparent;           /* iframe: sem fundo declarado, a tela fica transparente */
    bool xml;                   /* documento XML (DOMParser/responseXML): nomes com caixa, sem body/head */
    char ctype[48];             /* document.contentType */
    int ctrl_base;              /* ids dos controles no ui_t (0 = 100000); cada iframe tem a sua faixa */
    struct canvas2d *canvases;  /* bitmaps dos <canvas> (kernel/web/canvas.c) */
} doc_t;

/* <canvas> (kernel/web/canvas.c): tamanho intrínseco (atributos width/height) e bitmap atual (NULL se vazio) */
void       canvas_intrinsic(node_t *n, int *w, int *h);
surface_t *canvas_surface(node_t *n);
void       canvas_free_all(doc_t *d);

doc_t *doc_parse(const char *html, usize len, const char *url);
void   html_parse_fragment(doc_t *d, node_t *parent, const char *html, int len);  /* innerHTML */
bool   html_sniff_charset(const u8 *d, usize n, char *out, int cap);
void   doc_free(doc_t *d);
void   doc_layout(doc_t *d, int width, int vh, float zoom);
void   doc_paint(doc_t *d, ui_t *u, rect_t view, int scroll, node_t *hover_link);
frag_t *doc_hit(doc_t *d, int x, int y, box_t **owner);  /* coordenadas do documento */
box_t *doc_hit_atom(doc_t *d, int x, int y);
int    doc_anchor_y(doc_t *d, const char *name);

/* ============================================================ corrotinas (kernel/web/costack.c) */
typedef struct costack costack_t;
costack_t *cs_new(usize stack_size, void (*fn)(void *), void *arg);
costack_t *cs_new_on(u8 *stack, usize stack_size, void (*fn)(void *), void *arg);   /* pilha (kmalloc) já pronta */
u8        *cs_release(costack_t *c);       /* libera a corrotina terminada; devolve a pilha */
bool       cs_resume(costack_t *c);        /* roda até terminar (true) ou ceder (false) */
void       cs_yield(void);                 /* de dentro da corrotina atual */
costack_t *cs_current(void);
bool       cs_done(costack_t *c);
void       cs_free(costack_t *c);

/* ============================================================ iframes (kernel/web/frame.c) */
enum { FRAME_CLICK, FRAME_WHEEL };
enum { FRAME_HANDLED = 1, FRAME_PREVENTED = 2 };   /* frame_input: 0 = não tratou */
bool    frames_pump(doc_t *d);                  /* carrega, roda o JS e faz o layout dos iframes; true se mudou */
bool    frames_busy(doc_t *d);
int     frames_next_ms(doc_t *d);
void    frames_free(doc_t *d);                  /* doc_free chama */
void    frames_set_owner_js(doc_t *d, struct js_page *js);
frame_t *frame_at(doc_t *d, int x, int y, int *ox, int *oy);   /* coords do documento; ox/oy: canto do conteúdo */
int     frame_input(frame_t *f, int kind, int x, int y, int wheel, char *out_url, int cap);
void    frame_paint(frame_t *f, ui_t *u, rect_t r);
void    frame_navigate(node_t *iframe, const char *url);
struct js_page *frame_page(node_t *iframe);
doc_t  *frame_document(frame_t *f);
const char *frame_url(frame_t *f);
bool    doc_pump_images(doc_t *d);              /* imagens http/data:/file: de um documento */
frame_t *frames_frame_of(doc_t *root, doc_t *d);           /* iframe (em qualquer nível) cujo documento é d */
struct js_page *frames_page_of(doc_t *root, doc_t *d);     /* página JS de d (a principal se d == root: NULL) */
ctrl_t *frames_ctrl_by_id(doc_t *root, u32 id, doc_t **owner);  /* controle com esse id nos iframes */
void    frame_load(frame_t *f, const char *url, const char *post, int post_len, const char *ctype);
/* envio de formulário: URL (com a query no GET) e corpo (POST); devolve true se é POST */
bool    form_build(doc_t *d, node_t *form, ctrl_t *submitter, char *url, int cap, sbuf_t *body);
u8     *frame_data_url(const char *url, usize *len, char *mime, int mcap);
enum { FRAME_CUR_ARROW = 1, FRAME_CUR_HAND, FRAME_CUR_TEXT };
/* política de navegação dos iframes: fn devolve 1 = segue agora, 2 = perguntou (frame_policy_reply responde depois),
 * 0 = ocupado (pergunta de novo no próximo pump) */
typedef struct frame_policy { int (*fn)(void *ctx, frame_t *f, const char *url, int kind); void *ctx; } frame_policy_t;
void    frame_policy_reply(frame_t *f, bool allow);
bool    frames_contains(doc_t *root, frame_t *f);
const char *frame_current_url(frame_t *f);
int     frame_cursor(doc_t *d, int x, int y);              /* 0 = fora de iframe */
bool    frames_take_popup(doc_t *d, char *out, int cap);  /* window.open de algum iframe */
int     frame_sandbox(frame_t *f);                         /* JS_SB_* do documento do iframe */

/* chamado pela pintura para controles de formulário (implementado em horizonte.c) */
void web_paint_control(ui_t *u, doc_t *d, ctrl_t *c, rect_t r);
/* fora de uma janela do Horizonte, quem pinta os controles (kernel/web/webview.c, durante a pintura de um WKWebView) */
extern void (*web_paint_control_hook)(ui_t *u, doc_t *d, ctrl_t *c, rect_t r);
