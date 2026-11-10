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
    GUI_DEVICE,         /* 79: controles de jogo, vibração e Bluetooth LE para os frameworks: rdi = DEV_* (ver abaixo) */
    GUI_LAST = 79
};

/* GUI_DEVICE
 *   DEV_PAD_GET    (índice, gui_gamepad_t *)     1 = há esse slot (conectado ou não), 0 = acabou a lista
 *   DEV_PAD_RUMBLE (índice, forte | fraco << 8, ms)  vibração (0..255 cada motor); 0, 0 para
 *   DEV_AUDIO_PLAY (amostras, quadros)            toca PCM 48 kHz estéreo i16 no mixer do sistema
 *   DEV_BT_STATE   ()                             0 sem adaptador, 1 desligado, 2 ligado
 *   DEV_BT_SCAN    (ligar)                        varredura LE
 *   DEV_BT_DEVICE  (índice, gui_bt_device_t *)    1 = preenchido, 0 = acabou
 *   DEV_BT_CONNECT (índice)                       conexão LE com canal ATT cru (o GATT fica no CoreBluetooth)
 *   DEV_BT_DISCONNECT ()
 *   DEV_BT_ATT_SEND (pdu, bytes)                  PDU ATT para o periférico conectado
 *   DEV_BT_ATT_RECV (buf, cap)                    próxima PDU ATT recebida (bytes) ou 0; < 0 = desconectado
 *   DEV_BT_LINK    (gui_bt_link_t *)              estado da conexão crua */
#define DEV_PAD_GET 0
#define DEV_PAD_RUMBLE 1
#define DEV_AUDIO_PLAY 2      /* (amostras i16 estéreo 48 kHz, quadros) -> quadros aceitos; mixer do sistema */
#define DEV_INPUT_POST 3      /* (tipo EV_* | botões << 8 | mods << 16 | roda << 32, tecla, x | y << 32) evento sintético
                               * na fila de entrada global (CGEventPost); tipos EV_KEY_DOWN/UP e EV_MOUSE_ABS */
#define DEV_SCREEN_CAPTURE 4  /* (x | y << 32, w | h << 32, buf) retângulo da tela em ARGB; w = 0: a tela toda; buf = 0: só
                               * o tamanho. Devolve w | h << 32 (já recortado), < 0 erro */
/* streams de áudio por processo (CoreAudio/AudioUnit/AudioQueue/AVFoundation, userland/libsystem/coreaudio.c), somados
 * no mixer do sistema: 48 kHz, estéreo, i16 intercalado. O app escreve à frente (até 0,5 s) e se ritma por queued. */
#define DEV_AUDIO_OPEN 5      /* () -> stream | -errno */
#define DEV_AUDIO_WRITE 6     /* (stream, amostras, quadros) -> quadros aceitos (0 = anel cheio) */
#define DEV_AUDIO_CLOSE 7     /* (stream) -> 0 */
#define DEV_AUDIO_STATE 17    /* (stream, gui_audio_state_t *) -> 0 */
#define DEV_AUDIO_CTL 18      /* (stream, ganho 0..65536 (-1 = mantém) | pausa (-1/0/1) << 32, descartar) -> 0 */
#define DEV_AUDIO_INFO 19     /* (gui_audio_state_t *) -> 1 se há saída de som de verdade (senão os streams andam no relógio) */
#define DEV_WINDOW_LIST 20    /* (índice, gui_cgwindow_t *) 1 = preencheu, 0 = acabou. Da frente para o fundo.
                               * CGWindowListCopyWindowInfo / SCShareableContent. A permissão de gravar a tela fica no userland (TCC). */
typedef struct {
    unsigned int number;           /* kCGWindowNumber */
    int pid;                       /* kCGWindowOwnerPID; 0 se a janela não é de um processo convidado */
    int layer;                     /* kCGWindowLayer (0 = janela normal) */
    int onscreen;                  /* kCGWindowIsOnscreen */
    int x, y, w, h;                /* kCGWindowBounds, origem no canto superior esquerdo do compositor */
    char owner[64];                /* kCGWindowOwnerName */
    char name[96];                 /* kCGWindowName */
} gui_cgwindow_t;
int compat_gui_cgwindow(int index, gui_cgwindow_t *out);
typedef struct {
    unsigned int queued, capacity, rate, channels;
    unsigned long long played, underruns;
    int available, volume;      /* volume mestre do sistema 0..100 */
    char name[64];
} gui_audio_state_t;
#define DEV_BT_STATE 8
#define DEV_BT_SCAN 9
#define DEV_BT_DEVICE 10
#define DEV_BT_CONNECT 11
#define DEV_BT_DISCONNECT 12
#define DEV_BT_ATT_SEND 13
#define DEV_BT_ATT_RECV 14
#define DEV_BT_LINK 15
#define DEV_BT_ADVERTISE 16   /* (dados AD, bytes <= 31, ligar) papel periférico: anuncia; a conexão de entrada vira o canal ATT cru */

/* botões no layout do GCExtendedGamepad */
#define PAD_A        (1u << 0)
#define PAD_B        (1u << 1)
#define PAD_X        (1u << 2)
#define PAD_Y        (1u << 3)
#define PAD_LB       (1u << 4)
#define PAD_RB       (1u << 5)
#define PAD_LS       (1u << 6)   /* clique do analógico esquerdo */
#define PAD_RS       (1u << 7)
#define PAD_MENU     (1u << 8)   /* Start / Options */
#define PAD_OPTIONS  (1u << 9)   /* Back / Share / View */
#define PAD_HOME     (1u << 10)
#define PAD_UP       (1u << 11)
#define PAD_DOWN     (1u << 12)
#define PAD_LEFT     (1u << 13)
#define PAD_RIGHT    (1u << 14)
#define PAD_TOUCHPAD (1u << 15)
#define PAD_CAP_RUMBLE 1u
#define PAD_KIND_HID 1
#define PAD_KIND_XBOX360 2
#define PAD_KIND_XBOXONE 3
#define PAD_KIND_SONY 4
typedef struct {
    int connected;
    unsigned int seq;              /* muda a cada relatório */
    unsigned int buttons;          /* PAD_* */
    float lx, ly, rx, ry;          /* -1..1, y para cima positivo */
    float lt, rt;                  /* 0..1 */
    char name[48];
    unsigned short vid, pid;
    unsigned int kind, caps;
} gui_gamepad_t;
typedef struct {
    char name[32];
    char addr[18];                 /* "AA:BB:CC:DD:EE:FF" */
    int rssi;
    int hid;                       /* anuncia HID (teclado/mouse/controle) */
    int connectable;
} gui_bt_device_t;
typedef struct {
    int state;                     /* 0 livre, 1 conectando, 2 pronto (ATT cru), 3 falhou */
    int mtu;
    char addr[18];
    char err[64];
    int role;                      /* 0 central (nós conectamos), 1 periférico (conectaram no nosso anúncio) */
    int advertising;
} gui_bt_link_t;

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
/* NSWindowStyleMaskFullSizeContentView: o conteúdo ocupa a janela inteira (w x h é a janela toda) e o
 * window server só desenha os semáforos por cima; quem arrasta a janela é o app (GUI_WIN_FOCUS com
 * GUI_FOCUS_MOVE, de -[NSWindow performWindowDragWithEvent:]). */
#define GUI_WF_FULLSIZE  4
/* GUI_WIN_FOCUS (id, GUI_FOCUS_MOVE): passa o arrasto do botão esquerdo, que está apertado sobre o
 * conteúdo dessa janela, para mover a janela. -EAGAIN se não há botão apertado nela. */
#define GUI_FOCUS_MOVE 0x4D4F5645
/* GUI_WIN_FOCUS (id, GUI_FOCUS_HIDE / GUI_FOCUS_SHOW): -[NSWindow orderOut:] esconde a janela sem destruí-la (conteúdo,
 * posição e o ícone do app no Dock ficam) e orderFront: a mostra de novo, na frente. */
#define GUI_FOCUS_HIDE 0x48494445
#define GUI_FOCUS_SHOW 0x53484F57
/* GUI_WIN_FOCUS (id, GUI_FOCUS_OWNER, dona): a janela passa a ficar sempre acima da dona (outra janela do mesmo processo;
 * 0 desfaz), como as filhas do Cocoa (-[NSWindow addChildWindow:ordered:]). Não muda o foco agora. */
#define GUI_FOCUS_OWNER 0x4F574E52

/* eventos */
enum { GEV_NONE, GEV_MOUSE_DOWN, GEV_MOUSE_UP, GEV_MOUSE_MOVE, GEV_RIGHT_DOWN, GEV_SCROLL,
       GEV_KEY_DOWN, GEV_CLOSE, GEV_RESIZE, GEV_FOCUS, GEV_QUIT, GEV_MENU,     /* GEV_MENU: key = ação */
       GEV_DRAG_ENTER, GEV_DRAG_MOVE, GEV_DRAG_EXIT, GEV_DRAG_DROP, GEV_DRAG_SRC_MOVE, GEV_DRAG_END,
       GEV_INPUT_SOURCE,    /* key = nova fonte de entrada (⌃Espaço troca) */
       GEV_KEY_UP };        /* tecla solta (mesmos campos de GEV_KEY_DOWN) */

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
/* GUI_WIN_INFO: tamanho do conteúdo, foco e visibilidade da janela. GUI_SCREEN: w x h é a tela abaixo da barra de
 * menus, focused = 1 e visible = altura que o Dock reserva embaixo (2 com o Dock oculto; kernels antigos: 1). */
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
 *   4 raster: r10 = gui_glyph_req_t (preenchida). Devolve w*h, ou 0 se o glifo é vazio.
 *   5 registrar: rdx = tamanho, r10 = bytes de uma fonte TrueType (glyf, até 8 MiB). Devolve o id da
 *     fonte (32 a 79), que serve de peso nas outras operações; o mesmo arquivo devolve o mesmo id.
 *     -EINVAL se a fonte não é TrueType válida ou não há mais vaga. */
#define GF_TABLE 0
#define GF_CMAP 1
#define GF_ADVANCE 2
#define GF_METRICS 3
#define GF_RASTER 4
#define GF_REGISTER 5
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