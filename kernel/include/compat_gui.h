/*
 * mOS — protocolo gráfico entre apps de usuário (AppKit do mOS) e o window server.
 * Compartilhado entre o kernel (kernel/compat/guisys.c) e o userland (userland/libsystem/).
 *
 * Syscalls (int 0x80, números 64..79, roteados por proc_syscall para compat_gui_syscall):
 *   GUI_WIN_CREATE (title, w, h, flags)       -> id da janela (conteúdo w x h, coordenadas
 *                                              com origem no canto superior esquerdo)
 *   GUI_WIN_DESTROY(id)
 *   GUI_WIN_TITLE  (id, title)
 *   GUI_DRAW       (id, cmds, bytes)          executa um lote de comandos na superfície da janela
 *   GUI_PRESENT    (id)                       mostra o que foi desenhado
 *   GUI_POLL       (gui_event_t *ev)          1 = evento copiado, 0 = nenhum (não bloqueia)
 *   GUI_MEASURE    (text, bytes, gui_metrics_t *m, size_x100 | weight << 16)
 *   GUI_WIN_INFO   (id, gui_wininfo_t *out)   tamanho atual, foco
 *   GUI_SCREEN     (gui_wininfo_t *out)       tamanho da tela
 *   GUI_WIN_FOCUS  (id)
 *   GUI_SET_MENU   (buf, bytes)              menus do app na barra do mOS (ver formato abaixo)
 *   GUI_DECODE_IMAGE(data, bytes, out, cap)   PNG/JPEG/BMP/GIF -> {u32 w, h; u32 argb[w*h]} (0 ou bytes necessários)
 */
#pragma once

enum {
    GUI_WIN_CREATE = 64, GUI_WIN_DESTROY, GUI_WIN_TITLE, GUI_DRAW, GUI_PRESENT, GUI_POLL,
    GUI_MEASURE, GUI_WIN_INFO, GUI_SCREEN, GUI_WIN_FOCUS, GUI_SET_MENU, GUI_DECODE_IMAGE,
    GUI_FONT,           /* CoreText: tabelas da fonte do sistema e raster de glifos (ver abaixo) */
    GUI_DRAG,           /* arrastar e soltar entre janelas (de qualquer app): rdi = DG_* (ver abaixo) */
    GUI_IME,            /* janela de candidatos do método de entrada: rdi = IME_* (ver abaixo) */
    GUI_LAST = 79
};

/* GUI_DRAG
 *   DG_BEGIN  (win, payload, bytes, mask)  começa uma sessão; devolve o número de sequência (> 0)
 *   DG_DATA   (out, cap)                   cópia do conteúdo do arrasto; devolve o tamanho total
 *   DG_STATUS (seq, op)                    o destino diz qual operação aceita onde o cursor está (0 = nenhuma)
 *   DG_FINISH (seq, op)                    o destino terminou de soltar (op 0 = falhou); a fonte recebe GEV_DRAG_END
 *   DG_CANCEL (seq)                        a fonte desiste do arrasto
 * Conteúdo: u32 ntipos; por tipo: u32 tlen, tipo UTF-8, u32 kind ('S' texto, 'D' dados, 'P' plist XML), u32 dlen, dados.
 * Eventos: GEV_DRAG_ENTER/MOVE/EXIT/DROP para a janela sob o cursor (x, y no conteúdo; w, h na tela; key = máscara
 * de operações da fonte; clicks = sequência); GEV_DRAG_SRC_MOVE e GEV_DRAG_END para a janela da fonte
 * (GEV_DRAG_END: key = operação feita). */
#define DG_BEGIN  0
#define DG_DATA   1
#define DG_STATUS 2
#define DG_FINISH 3
#define DG_CANCEL 4
#define GUI_DRAG_MAX (16u << 20)

/* GUI_IME
 *   IME_SHOW (win, buf, bytes, x | y << 32)  mostra a composição e os candidatos perto de (x, y) no conteúdo.
 *            buf: u32 selecionado, u32 n, u32 flags (1 = desenhar a composição: o cliente não mostra marked text),
 *            depois n+1 strings terminadas em 0 (a primeira é a composição).
 *   IME_HIDE (win)
 *   IME_SOURCE_GET () / IME_SOURCE_SET (índice)  fonte de entrada global (0 = teclado, 1 japonês, 2 chinês, 3 coreano) */
#define IME_SHOW 0
#define IME_HIDE 1
#define IME_SOURCE_GET 2
#define IME_SOURCE_SET 3
#define GUI_IME_MAX 8192

/* flags de janela */
#define GUI_WF_RESIZABLE 1
#define GUI_WF_NO_TITLE  2

/* eventos */
enum { GEV_NONE, GEV_MOUSE_DOWN, GEV_MOUSE_UP, GEV_MOUSE_MOVE, GEV_RIGHT_DOWN, GEV_SCROLL,
       GEV_KEY_DOWN, GEV_CLOSE, GEV_RESIZE, GEV_FOCUS, GEV_QUIT, GEV_MENU,     /* GEV_MENU: key = ação */
       GEV_DRAG_ENTER, GEV_DRAG_MOVE, GEV_DRAG_EXIT, GEV_DRAG_DROP, GEV_DRAG_SRC_MOVE, GEV_DRAG_END,
       GEV_INPUT_SOURCE };  /* GEV_INPUT_SOURCE: key = nova fonte de entrada (⌃Espaço troca) */

typedef struct {
    unsigned int type, win;
    int x, y;               /* posição no conteúdo da janela */
    int buttons, clicks;    /* botões pressionados; 2 = duplo clique */
    int key;                /* tecla (ASCII ou KEY_* do mOS) */
    unsigned int mods;      /* MOD_SHIFT 1, CTRL 2, ALT 4, CMD 8 */
    int w, h;               /* GEV_RESIZE: novo tamanho; GEV_SCROLL: delta em w */
    char text[8];           /* caractere digitado (UTF-8) */
} gui_event_t;

typedef struct { int width, ascent, descent; } gui_metrics_t;
typedef struct { int w, h, focused, visible; } gui_wininfo_t;

/* comandos de desenho: cabeçalho {u32 op, u32 bytes} + carga; coordenadas em pixels do conteúdo */
enum {
    GC_CLEAR = 1,       /* u32 color */
    GC_FILL,            /* i32 x, y, w, h; u32 color */
    GC_RRECT,           /* f32 x, y, w, h, r; u32 color */
    GC_RRECT_STROKE,    /* f32 x, y, w, h, r, t; u32 color */
    GC_LINE,            /* f32 x0, y0, x1, y1, t; u32 color */
    GC_POLY,            /* u32 n; u32 color; f32 xy[2n]          preenchimento */
    GC_POLYLINE,        /* u32 n; u32 color; f32 t; f32 xy[2n]   contorno */
    GC_TEXT,            /* f32 x, baseline, size; u32 weight, color, bytes; char text[] */
    GC_CLIP,            /* i32 x, y, w, h */
    GC_UNCLIP,
    GC_GRAD,            /* i32 x, y, w, h; u32 top, bottom */
    GC_IMAGE,           /* i32 x, y, w, h; u32 pixels[w*h] (ARGB) */
};

#define GUI_MAX_BATCH (512u << 10)

/* GUI_SET_MENU: u32 nmenus; por menu: u32 nlen, título; u32 nitems; por item:
 *   u32 action (0 = separador), u32 flags (1 desabilitado, 2 marcado), u32 nlen, rótulo, u32 klen, atalho.
 * Ações especiais vindas da barra: GUI_MENU_ABOUT, GUI_MENU_PREFS, GUI_MENU_HIDE. */
/* GUI_FONT: rdi = operação, rsi = peso (0 regular .. 3 bold).
 *   0 tabela: rdx = tag fourcc, r10 = buffer, r8 = capacidade. Devolve o tamanho (0 = não existe).
 *   1 cmap:   rdx = codepoint. Devolve o índice do glifo.
 *   2 avanço: rdx = glifo. Devolve unidades | (upem << 16).
 *   3 métricas: r10 = gui_font_metrics_t. Devolve 0.
 *   4 raster: r10 = gui_glyph_req_t (preenchida). Devolve w*h, ou 0 se o glifo é vazio. */
#define GF_TABLE 0
#define GF_CMAP 1
#define GF_ADVANCE 2
#define GF_METRICS 3
#define GF_RASTER 4
typedef struct { i32 upem, ascender, descender, nglyphs; } gui_font_metrics_t;
typedef struct {
    u32 weight, glyph;
    float size;
    u32 cap;
    u64 buf;                 /* alfa 8 bits, do chamador */
    i32 w, h, left, top;
} gui_glyph_req_t;

#define GUI_MENU_ABOUT 60001
#define GUI_MENU_PREFS 60002
#define GUI_MENU_HIDE  60003