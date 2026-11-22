/*
 * mOS libSystem — ncurses (libncurses.5.4 / libpanel.5.4 do macOS): o _curses do Python embutido (blender, calibre,
 * deluge), o CrossOver e os utilitários de terminal (docs/lacunas/leonardo.md: 191 + 14 símbolos).
 * Código original do mOS (MIT). O terminal é tratado como xterm-256color (o Terminal do mOS e os do macOS):
 *   - janelas com buffer de células (chtype = caractere | atributos | par de cor, e o ponto de código largo à parte);
 *   - wnoutrefresh copia para a tela virtual; doupdate manda só as linhas mudadas em sequências ANSI (cup, SGR, el);
 *   - entrada em modo cbreak/raw pelo termios; keypad decodifica as sequências de seta, F1..F12, Home/End/PgUp...;
 *   - cores (8/256, pares, use_default_colors), ACS como os traços do Unicode, painéis em pilha;
 *   - terminfo/termcap embutidos: tigetstr/tigetnum/tigetflag, tparm (interpretador de %-parâmetros), tgoto, tputs.
 * Sem mouse (mousemask devolve 0). Teste: tools/curses_mos_test.sh
 */
#include "sys.h"
#include <stdarg.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strdup(const char *s);
char *getenv(const char *n);
char *strstr(const char *h, const char *n);
int snprintf(char *b, size_t n, const char *f, ...);
int vsnprintf(char *b, size_t n, const char *f, va_list ap);
long read(int fd, void *b, unsigned long n);
long write(int fd, const void *b, unsigned long n);
int ioctl(int fd, unsigned long req, ...);
int isatty(int fd);
int usleep(unsigned us);
typedef struct { unsigned long iflag, oflag, cflag, lflag; unsigned char cc[20]; unsigned long ispeed, ospeed; } tio_t;   /* termios do macOS */
int tcgetattr(int fd, tio_t *t);
int tcsetattr(int fd, int act, const tio_t *t);
struct pollfd_ { int fd; short events, revents; };
int poll(struct pollfd_ *p, unsigned n, int ms);
typedef void FILE;
long fread(void *b, size_t s, size_t n, FILE *f);
long fwrite(const void *b, size_t s, size_t n, FILE *f);

typedef unsigned int chtype;
typedef chtype attr_t;
#define OK 0
#define ERR (-1)
#define A_CHARTEXT 0x000000ffu
#define A_COLOR 0x0000ff00u
#define A_STANDOUT (1u << 16)
#define A_UNDERLINE (1u << 17)
#define A_REVERSE (1u << 18)
#define A_BLINK (1u << 19)
#define A_DIM (1u << 20)
#define A_BOLD (1u << 21)
#define A_ALTCHARSET (1u << 22)
#define A_INVIS (1u << 23)
#define A_ATTRS 0xffffff00u
#define PAIR_OF(a) (((a) & A_COLOR) >> 8)
/* termios do macOS */
#define T_ECHO 0x8
#define T_ICANON 0x100
#define T_ISIG 0x80
#define T_IEXTEN 0x400
#define T_ICRNL 0x100
#define T_IXON 0x200
#define T_OPOST 0x1
#define T_ONLCR 0x2
#define VMIN_ 16
#define VTIME_ 17

/* ---------------------------------------------------------------- janelas */
typedef struct _win_st {
    short _cury, _curx, _maxy, _maxx, _begy, _begx;     /* o mesmo começo do WINDOW do ncurses */
    short _flags;
    attr_t _attrs;
    chtype _bkgd;
    unsigned char _notimeout, _clear, _leaveok, _scroll, _idlok, _idcok, _immed, _sync, _use_keypad;
    int _delay;
    void *_line;
    short _regtop, _regbottom;
    int _parx, _pary;
    struct _win_st *_parent;
    /* do mOS */
    chtype *cell;
    unsigned *wide;                 /* ponto de código quando não cabe em 8 bits */
    unsigned char *touched;
    int pad, own, stride;           /* sub-janela: aponta para as células da mãe (stride = largura da mãe) */
    void *userptr;
} WINDOW;
WINDOW *stdscr, *curscr, *newscr;
int LINES = 24, COLS = 80, COLORS = 0, COLOR_PAIRS = 0, TABSIZE = 8, ESCDELAY = 1000;
chtype acs_map[128];
char PC;
char *UP = "", *BC = "";
short ospeed;
static int g_init, g_echo = 1, g_cbreak, g_raw, g_nl = 1, g_cursor = 1, g_endwin;
static tio_t g_shell, g_prog, g_saved;
static int g_have_shell;
#define MAXPAIRS 256
static short pair_fg[MAXPAIRS], pair_bg[MAXPAIRS];
static int g_default_colors;
#define W(w, y, x) ((w)->cell[(size_t)(y) * (size_t)(w)->stride + (size_t)(x)])
#define WW(w, y, x) ((w)->wide[(size_t)(y) * (size_t)(w)->stride + (size_t)(x)])

static WINDOW *win_alloc(int h, int w, int y, int x) {
    if (h <= 0 || w <= 0) return 0;
    WINDOW *win = calloc(1, sizeof *win);
    if (!win) return 0;
    win->cell = calloc((size_t)h * (size_t)w, sizeof(chtype));
    win->wide = calloc((size_t)h * (size_t)w, sizeof(unsigned));
    win->touched = calloc((size_t)h, 1);
    if (!win->cell || !win->wide || !win->touched) { free(win->cell); free(win->wide); free(win->touched); free(win); return 0; }
    for (size_t i = 0; i < (size_t)h * (size_t)w; i++) win->cell[i] = ' ';
    win->_maxy = (short)(h - 1); win->_maxx = (short)(w - 1); win->_begy = (short)y; win->_begx = (short)x;
    win->_regtop = 0; win->_regbottom = (short)(h - 1);
    win->_delay = -1; win->own = 1; win->stride = w; win->_bkgd = ' ';
    memset(win->touched, 1, (size_t)h);
    return win;
}
WINDOW *newwin(int h, int w, int y, int x) {
    if (!h) h = LINES - y;
    if (!w) w = COLS - x;
    return win_alloc(h, w, y, x);
}
WINDOW *newpad(int h, int w) { WINDOW *p = win_alloc(h, w, 0, 0); if (p) p->pad = 1; return p; }
static WINDOW *sub_of(WINDOW *o, int h, int w, int y, int x, int rel) {
    if (!o) return 0;
    int py = rel ? y : y - o->_begy, px = rel ? x : x - o->_begx;
    if (!h) h = o->_maxy + 1 - py;
    if (!w) w = o->_maxx + 1 - px;
    if (py < 0 || px < 0 || py + h > o->_maxy + 1 || px + w > o->_maxx + 1) return 0;
    WINDOW *s = calloc(1, sizeof *s);
    if (!s) return 0;
    *s = *o;
    s->cell = &W(o, py, px);
    s->wide = &WW(o, py, px);
    s->touched = calloc((size_t)h, 1);
    if (!s->touched) { free(s); return 0; }
    memset(s->touched, 1, (size_t)h);
    s->_maxy = (short)(h - 1); s->_maxx = (short)(w - 1);
    s->_begy = (short)(o->_begy + py); s->_begx = (short)(o->_begx + px);
    s->_cury = s->_curx = 0; s->_regtop = 0; s->_regbottom = (short)(h - 1);
    s->_parent = o; s->_pary = py; s->_parx = px; s->own = 0;
    return s;
}
WINDOW *subwin(WINDOW *o, int h, int w, int y, int x) { return sub_of(o, h, w, y, x, 0); }
WINDOW *derwin(WINDOW *o, int h, int w, int y, int x) { return sub_of(o, h, w, y, x, 1); }
WINDOW *subpad(WINDOW *o, int h, int w, int y, int x) { WINDOW *s = sub_of(o, h, w, y, x, 1); if (s) s->pad = 1; return s; }
int delwin(WINDOW *w) {
    if (!w) return ERR;
    if (w->own) { free(w->cell); free(w->wide); }
    free(w->touched);
    free(w);
    return OK;
}
int mvwin(WINDOW *w, int y, int x) {
    if (!w || w->pad) return ERR;
    if (y < 0 || x < 0 || y + w->_maxy >= LINES || x + w->_maxx >= COLS) return ERR;
    w->_begy = (short)y; w->_begx = (short)x;
    memset(w->touched, 1, (size_t)w->_maxy + 1);
    return OK;
}
int mvderwin(WINDOW *w, int y, int x) {
    if (!w || !w->_parent) return ERR;
    WINDOW *p = w->_parent;
    if (y < 0 || x < 0 || y + w->_maxy > p->_maxy || x + w->_maxx > p->_maxx) return ERR;
    w->cell = &W(p, y, x); w->wide = &WW(p, y, x);
    w->_pary = y; w->_parx = x;
    w->_begy = (short)(p->_begy + y); w->_begx = (short)(p->_begx + x);
    memset(w->touched, 1, (size_t)w->_maxy + 1);
    return OK;
}
int wresize(WINDOW *w, int h, int nw) {
    if (!w || h <= 0 || nw <= 0 || !w->own) return ERR;
    chtype *c = calloc((size_t)h * (size_t)nw, sizeof(chtype));
    unsigned *wd = calloc((size_t)h * (size_t)nw, sizeof(unsigned));
    unsigned char *t = calloc((size_t)h, 1);
    if (!c || !wd || !t) { free(c); free(wd); free(t); return ERR; }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < nw; x++) {
            int in = y <= w->_maxy && x <= w->_maxx;
            c[(size_t)y * (size_t)nw + (size_t)x] = in ? W(w, y, x) : w->_bkgd;
            wd[(size_t)y * (size_t)nw + (size_t)x] = in ? WW(w, y, x) : 0;
        }
    free(w->cell); free(w->wide); free(w->touched);
    w->cell = c; w->wide = wd; w->touched = t; w->stride = nw;
    memset(t, 1, (size_t)h);
    w->_maxy = (short)(h - 1); w->_maxx = (short)(nw - 1);
    if (w->_cury > w->_maxy) w->_cury = w->_maxy;
    if (w->_curx > w->_maxx) w->_curx = w->_maxx;
    w->_regtop = 0; w->_regbottom = w->_maxy;
    return OK;
}
static void touch(WINDOW *w, int y) {
    if (y >= 0 && y <= w->_maxy) w->touched[y] = 1;
    for (WINDOW *p = w->_parent; p; y += w->_pary, w = p, p = p->_parent)
        if (y + w->_pary <= p->_maxy) p->touched[y + w->_pary] = 1;
}

/* ---------------------------------------------------------------- escrita nas células */
static chtype render(WINDOW *w, chtype ch) {
    chtype a = (ch & A_ATTRS) | w->_attrs;
    if (!(a & A_COLOR)) a |= w->_bkgd & A_COLOR;
    a |= w->_bkgd & (A_ATTRS & ~A_COLOR);
    if ((ch & A_CHARTEXT) == ' ' && (w->_bkgd & A_CHARTEXT) != ' ') ch = (ch & ~A_CHARTEXT) | (w->_bkgd & A_CHARTEXT);
    return (ch & A_CHARTEXT) | a;
}
int wscrl(WINDOW *w, int n);
static void newline(WINDOW *w) {
    w->_curx = 0;
    if (w->_cury == w->_regbottom) { if (w->_scroll) wscrl(w, 1); }
    else if (w->_cury < w->_maxy) w->_cury++;
}
static int put_cell(WINDOW *w, chtype ch, unsigned wide) {
    unsigned c = ch & A_CHARTEXT;
    if (!wide) {
        if (c == '\n') { int y = w->_cury; for (int x = w->_curx; x <= w->_maxx; x++) { W(w, y, x) = render(w, ' '); WW(w, y, x) = 0; } touch(w, y); newline(w); return OK; }
        if (c == '\r') { w->_curx = 0; return OK; }
        if (c == '\b') { if (w->_curx > 0) w->_curx--; return OK; }
        if (c == '\t') { int nx = (w->_curx / TABSIZE + 1) * TABSIZE; while (w->_curx < nx && w->_curx <= w->_maxx) { if (put_cell(w, ' ' | (ch & A_ATTRS), 0)) return ERR; if (!w->_curx) break; } return OK; }
        if (c < 32 || c == 127) {                    /* controle: ^X */
            put_cell(w, '^' | (ch & A_ATTRS), 0);
            return put_cell(w, (c == 127 ? '?' : c + 64) | (ch & A_ATTRS), 0);
        }
    }
    W(w, w->_cury, w->_curx) = render(w, ch);
    WW(w, w->_cury, w->_curx) = wide;
    touch(w, w->_cury);
    if (++w->_curx > w->_maxx) {
        if (w->_cury == w->_regbottom && !w->_scroll) { w->_curx = w->_maxx; return ERR; }
        newline(w);
    }
    return OK;
}
int waddch(WINDOW *w, chtype ch) { return w ? put_cell(w, ch, 0) : ERR; }
int wechochar(WINDOW *w, chtype ch) { int r = waddch(w, ch); extern int wrefresh(WINDOW *); wrefresh(w); return r; }
int pechochar(WINDOW *p, chtype ch) { return waddch(p, ch); }
/* UTF-8 de entrada vira pontos de código */
static int utf8_next(const unsigned char **p, const unsigned char *end, unsigned *out) {
    const unsigned char *s = *p;
    unsigned c = *s++;
    int n = c < 0x80 ? 0 : c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : -1;
    if (n < 0) { *p = s; *out = 0xFFFD; return 1; }
    c &= n == 0 ? 0x7F : n == 1 ? 0x1F : n == 2 ? 0x0F : 0x07;
    for (int i = 0; i < n; i++) { if (s >= end || (*s & 0xC0) != 0x80) { *p = s; *out = 0xFFFD; return 1; } c = c << 6 | (*s++ & 0x3F); }
    *p = s; *out = c;
    return 1;
}
int waddnstr(WINDOW *w, const char *s, int n) {
    if (!w || !s) return ERR;
    size_t len = n < 0 ? strlen(s) : (size_t)n;
    const unsigned char *p = (const unsigned char *)s, *end = p + len;
    while (p < end && *p) {
        unsigned c;
        utf8_next(&p, end, &c);
        if (put_cell(w, c < 0x80 ? c : '?', c < 0x80 ? 0 : c) && c != '\n') return ERR;
    }
    return OK;
}
int waddstr(WINDOW *w, const char *s) { return waddnstr(w, s, -1); }
int mvwaddnstr(WINDOW *w, int y, int x, const char *s, int n) { extern int wmove(WINDOW *, int, int); return wmove(w, y, x) ? ERR : waddnstr(w, s, n); }
int mvwaddstr(WINDOW *w, int y, int x, const char *s) { return mvwaddnstr(w, y, x, s, -1); }
int waddnwstr(WINDOW *w, const int *s, int n) {
    if (!w || !s) return ERR;
    for (int i = 0; (n < 0 || i < n) && s[i]; i++) put_cell(w, s[i] < 0x80 ? (unsigned)s[i] : '?', s[i] < 0x80 ? 0 : (unsigned)s[i]);
    return OK;
}
typedef struct { attr_t attr; int chars[5]; int ext_color; } cchar_t;
int setcchar(cchar_t *c, const int *wch, attr_t a, short pair, const void *opts) {
    (void)opts;
    if (!c) return ERR;
    memset(c, 0, sizeof *c);
    for (int i = 0; i < 5 && wch && wch[i]; i++) c->chars[i] = wch[i];
    c->attr = (a & ~A_COLOR) | ((attr_t)(pair & 0xff) << 8);
    c->ext_color = pair;
    return OK;
}
int getcchar(const cchar_t *c, int *wch, attr_t *a, short *pair, void *opts) {
    (void)opts;
    if (!c) return ERR;
    if (!wch) { int n = 0; while (n < 5 && c->chars[n]) n++; return n + 1; }
    for (int i = 0; i < 5; i++) { wch[i] = c->chars[i]; if (!c->chars[i]) break; }
    if (a) *a = c->attr & ~A_COLOR;
    if (pair) *pair = (short)PAIR_OF(c->attr);
    return OK;
}
int wadd_wch(WINDOW *w, const cchar_t *c) {
    if (!w || !c) return ERR;
    unsigned ch = (unsigned)c->chars[0];
    return put_cell(w, (ch < 0x80 ? ch : '?') | (c->attr & A_ATTRS), ch < 0x80 ? 0 : ch);
}
int win_wch(WINDOW *w, cchar_t *c) {
    if (!w || !c) return ERR;
    memset(c, 0, sizeof *c);
    chtype x = W(w, w->_cury, w->_curx);
    unsigned wd = WW(w, w->_cury, w->_curx);
    c->chars[0] = (int)(wd ? wd : (x & A_CHARTEXT));
    c->attr = x & A_ATTRS;
    return OK;
}
chtype winch(WINDOW *w) { return w ? W(w, w->_cury, w->_curx) : (chtype)ERR; }
int winnstr(WINDOW *w, char *s, int n) {
    if (!w || !s) return ERR;
    int k = 0;
    for (int x = w->_curx; x <= w->_maxx && (n < 0 || k < n); x++) s[k++] = (char)(W(w, w->_cury, x) & A_CHARTEXT);
    s[k] = 0;
    return k;
}
int winsch(WINDOW *w, chtype ch) {
    if (!w) return ERR;
    int y = w->_cury;
    for (int x = w->_maxx; x > w->_curx; x--) { W(w, y, x) = W(w, y, x - 1); WW(w, y, x) = WW(w, y, x - 1); }
    W(w, y, w->_curx) = render(w, ch);
    WW(w, y, w->_curx) = 0;
    touch(w, y);
    return OK;
}
int winsnstr(WINDOW *w, const char *s, int n) {
    if (!w || !s) return ERR;
    int len = (int)strlen(s);
    if (n >= 0 && n < len) len = n;
    for (int i = len - 1; i >= 0; i--) winsch(w, (unsigned char)s[i]);
    return OK;
}
int wins_wch(WINDOW *w, const cchar_t *c) {
    if (!w || !c) return ERR;
    unsigned ch = (unsigned)c->chars[0];
    winsch(w, (ch < 0x80 ? ch : '?') | (c->attr & A_ATTRS));
    if (ch >= 0x80) WW(w, w->_cury, w->_curx) = ch;
    return OK;
}
int wins_nwstr(WINDOW *w, const int *s, int n) {
    if (!w || !s) return ERR;
    int len = 0;
    while (s[len] && (n < 0 || len < n)) len++;
    for (int i = len - 1; i >= 0; i--) { cchar_t c; int t[2] = {s[i], 0}; setcchar(&c, t, 0, 0, 0); wins_wch(w, &c); }
    return OK;
}
int wdelch(WINDOW *w) {
    if (!w) return ERR;
    int y = w->_cury;
    for (int x = w->_curx; x < w->_maxx; x++) { W(w, y, x) = W(w, y, x + 1); WW(w, y, x) = WW(w, y, x + 1); }
    W(w, y, w->_maxx) = w->_bkgd;
    WW(w, y, w->_maxx) = 0;
    touch(w, y);
    return OK;
}
int wmove(WINDOW *w, int y, int x) {
    if (!w || y < 0 || x < 0 || y > w->_maxy || x > w->_maxx) return ERR;
    w->_cury = (short)y; w->_curx = (short)x;
    return OK;
}
int wclrtoeol(WINDOW *w) {
    if (!w) return ERR;
    for (int x = w->_curx; x <= w->_maxx; x++) { W(w, w->_cury, x) = render(w, ' '); WW(w, w->_cury, x) = 0; }
    touch(w, w->_cury);
    return OK;
}
int wclrtobot(WINDOW *w) {
    if (!w) return ERR;
    wclrtoeol(w);
    for (int y = w->_cury + 1; y <= w->_maxy; y++) { for (int x = 0; x <= w->_maxx; x++) { W(w, y, x) = render(w, ' '); WW(w, y, x) = 0; } touch(w, y); }
    return OK;
}
int werase(WINDOW *w) { if (!w) return ERR; w->_cury = w->_curx = 0; return wclrtobot(w); }
int wclear(WINDOW *w) { if (!w) return ERR; werase(w); w->_clear = 1; return OK; }
int erase(void) { return werase(stdscr); }
int clearok(WINDOW *w, unsigned char b) { if (!w) return ERR; w->_clear = b; return OK; }
int wscrl(WINDOW *w, int n) {
    if (!w || !w->_scroll) return ERR;
    int top = w->_regtop, bot = w->_regbottom, h = bot - top + 1;
    while (n > 0) {
        for (int y = top; y < bot; y++) { memmove(&W(w, y, 0), &W(w, y + 1, 0), (size_t)(w->_maxx + 1) * sizeof(chtype)); memmove(&WW(w, y, 0), &WW(w, y + 1, 0), (size_t)(w->_maxx + 1) * sizeof(unsigned)); }
        for (int x = 0; x <= w->_maxx; x++) { W(w, bot, x) = w->_bkgd; WW(w, bot, x) = 0; }
        n--;
    }
    while (n < 0) {
        for (int y = bot; y > top; y--) { memmove(&W(w, y, 0), &W(w, y - 1, 0), (size_t)(w->_maxx + 1) * sizeof(chtype)); memmove(&WW(w, y, 0), &WW(w, y - 1, 0), (size_t)(w->_maxx + 1) * sizeof(unsigned)); }
        for (int x = 0; x <= w->_maxx; x++) { W(w, top, x) = w->_bkgd; WW(w, top, x) = 0; }
        n++;
    }
    for (int y = top; y < top + h; y++) touch(w, y);
    return OK;
}
int wsetscrreg(WINDOW *w, int top, int bot) {
    if (!w || top < 0 || bot > w->_maxy || top > bot) return ERR;
    w->_regtop = (short)top; w->_regbottom = (short)bot;
    return OK;
}
int scrollok(WINDOW *w, unsigned char b) { if (!w) return ERR; w->_scroll = b; return OK; }
int winsdelln(WINDOW *w, int n) {
    if (!w) return ERR;
    short t = w->_regtop, s = w->_scroll;
    w->_regtop = w->_cury; w->_scroll = 1;
    int r = wscrl(w, -n);
    w->_regtop = t; w->_scroll = (unsigned char)s;
    return r;
}
static chtype acs(chtype c) { return (c & A_ALTCHARSET) || (c & A_CHARTEXT) >= 128 ? c : (c ? c : 0); }
int wborder(WINDOW *w, chtype ls, chtype rs, chtype ts, chtype bs, chtype tl, chtype tr, chtype bl, chtype br) {
    if (!w) return ERR;
    if (!ls) ls = acs_map['x']; if (!rs) rs = acs_map['x']; if (!ts) ts = acs_map['q']; if (!bs) bs = acs_map['q'];
    if (!tl) tl = acs_map['l']; if (!tr) tr = acs_map['k']; if (!bl) bl = acs_map['m']; if (!br) br = acs_map['j'];
    int H = w->_maxy, X = w->_maxx;
    for (int x = 1; x < X; x++) { W(w, 0, x) = render(w, acs(ts)); W(w, H, x) = render(w, acs(bs)); WW(w, 0, x) = WW(w, H, x) = 0; }
    for (int y = 1; y < H; y++) { W(w, y, 0) = render(w, acs(ls)); W(w, y, X) = render(w, acs(rs)); WW(w, y, 0) = WW(w, y, X) = 0; touch(w, y); }
    W(w, 0, 0) = render(w, tl); W(w, 0, X) = render(w, tr); W(w, H, 0) = render(w, bl); W(w, H, X) = render(w, br);
    touch(w, 0); touch(w, H);
    return OK;
}
int whline(WINDOW *w, chtype ch, int n) {
    if (!w) return ERR;
    if (!ch) ch = acs_map['q'];
    for (int x = w->_curx; x <= w->_maxx && n-- > 0; x++) { W(w, w->_cury, x) = render(w, ch); WW(w, w->_cury, x) = 0; }
    touch(w, w->_cury);
    return OK;
}
int wvline(WINDOW *w, chtype ch, int n) {
    if (!w) return ERR;
    if (!ch) ch = acs_map['x'];
    for (int y = w->_cury; y <= w->_maxy && n-- > 0; y++) { W(w, y, w->_curx) = render(w, ch); WW(w, y, w->_curx) = 0; touch(w, y); }
    return OK;
}
int wchgat(WINDOW *w, int n, attr_t a, short pair, const void *opts) {
    (void)opts;
    if (!w) return ERR;
    for (int x = w->_curx; x <= w->_maxx && (n < 0 || n-- > 0); x++)
        W(w, w->_cury, x) = (W(w, w->_cury, x) & A_CHARTEXT) | (a & ~A_COLOR) | ((chtype)(pair & 0xff) << 8);
    touch(w, w->_cury);
    return OK;
}
int wattrset(WINDOW *w, int a) { if (!w) return ERR; w->_attrs = (attr_t)a; return OK; }
int wattron(WINDOW *w, int a) { if (!w) return ERR; if (a & A_COLOR) w->_attrs &= ~A_COLOR; w->_attrs |= (attr_t)a; return OK; }
int wattroff(WINDOW *w, int a) { if (!w) return ERR; w->_attrs &= ~(attr_t)a; return OK; }
int wattr_on(WINDOW *w, attr_t a, void *o) { (void)o; return wattron(w, (int)a); }
int wattr_off(WINDOW *w, attr_t a, void *o) { (void)o; return wattroff(w, (int)a); }
int wstandout(WINDOW *w) { return wattron(w, A_STANDOUT); }
int wstandend(WINDOW *w) { return wattrset(w, 0); }
attr_t getattrs(const WINDOW *w) { return w ? w->_attrs : (attr_t)ERR; }
void wbkgdset(WINDOW *w, chtype c) { if (w) w->_bkgd = (c & A_CHARTEXT) ? c : (c | ' '); }
int wbkgd(WINDOW *w, chtype c) {
    if (!w) return ERR;
    chtype old = w->_bkgd;
    wbkgdset(w, c);
    for (int y = 0; y <= w->_maxy; y++) {
        for (int x = 0; x <= w->_maxx; x++) {
            chtype v = W(w, y, x);
            if ((v & A_CHARTEXT) == (old & A_CHARTEXT)) v = (v & ~A_CHARTEXT) | (w->_bkgd & A_CHARTEXT);
            W(w, y, x) = (v & ~(old & A_ATTRS)) | (w->_bkgd & A_ATTRS);
        }
        touch(w, y);
    }
    return OK;
}
chtype getbkgd(WINDOW *w) { return w ? w->_bkgd : (chtype)ERR; }
int getcury(const WINDOW *w) { return w ? w->_cury : ERR; }
int getcurx(const WINDOW *w) { return w ? w->_curx : ERR; }
int getmaxy(const WINDOW *w) { return w ? w->_maxy + 1 : ERR; }
int getmaxx(const WINDOW *w) { return w ? w->_maxx + 1 : ERR; }
int getbegy(const WINDOW *w) { return w ? w->_begy : ERR; }
int getbegx(const WINDOW *w) { return w ? w->_begx : ERR; }
int getpary(const WINDOW *w) { return w && w->_parent ? w->_pary : ERR; }
int getparx(const WINDOW *w) { return w && w->_parent ? w->_parx : ERR; }
int leaveok(WINDOW *w, unsigned char b) { if (!w) return ERR; w->_leaveok = b; return OK; }
unsigned char is_leaveok(const WINDOW *w) { return w && w->_leaveok; }
int idlok(WINDOW *w, unsigned char b) { if (!w) return ERR; w->_idlok = b; return OK; }
void idcok(WINDOW *w, unsigned char b) { if (w) w->_idcok = b; }
void immedok(WINDOW *w, unsigned char b) { if (w) w->_immed = b; }
int syncok(WINDOW *w, unsigned char b) { if (!w) return ERR; w->_sync = b; return OK; }
void wsyncup(WINDOW *w) { for (int y = 0; w && y <= w->_maxy; y++) if (w->touched[y]) touch(w, y); }
void wsyncdown(WINDOW *w) { if (w && w->_parent) for (int y = 0; y <= w->_maxy; y++) if (w->_parent->touched[y + w->_pary]) w->touched[y] = 1; }
void wcursyncup(WINDOW *w) { for (WINDOW *p = w ? w->_parent : 0; p; w = p, p = p->_parent) { p->_cury = (short)(w->_cury + w->_pary); p->_curx = (short)(w->_curx + w->_parx); } }
int wtouchln(WINDOW *w, int y, int n, int changed) {
    if (!w) return ERR;
    for (int i = y; i < y + n && i <= w->_maxy; i++) if (i >= 0) w->touched[i] = changed ? 1 : 0;
    return OK;
}
unsigned char is_linetouched(WINDOW *w, int y) { return w && y >= 0 && y <= w->_maxy && w->touched[y]; }
unsigned char is_wintouched(WINDOW *w) { for (int y = 0; w && y <= w->_maxy; y++) if (w->touched[y]) return 1; return 0; }
int redrawwin(WINDOW *w) { return w ? wtouchln(w, 0, w->_maxy + 1, 1) : ERR; }
int wredrawln(WINDOW *w, int y, int n) { return wtouchln(w, y, n, 1); }
static int copy_win(const WINDOW *s, WINDOW *d, int sy, int sx, int dy0, int dx0, int dy1, int dx1, int over) {
    if (!s || !d) return ERR;
    for (int y = dy0; y <= dy1 && y <= d->_maxy; y++) {
        int yy = sy + y - dy0;
        if (yy > s->_maxy) break;
        for (int x = dx0; x <= dx1 && x <= d->_maxx; x++) {
            int xx = sx + x - dx0;
            if (xx > s->_maxx) break;
            chtype v = W(s, yy, xx);
            if (over && (v & A_CHARTEXT) == ' ') continue;
            W(d, y, x) = v; WW(d, y, x) = WW(s, yy, xx);
        }
        touch(d, y);
    }
    return OK;
}
int copywin(const WINDOW *s, WINDOW *d, int sy, int sx, int dy0, int dx0, int dy1, int dx1, int over) { return copy_win(s, d, sy, sx, dy0, dx0, dy1, dx1, over); }
static int ov(const WINDOW *s, WINDOW *d, int over) {
    if (!s || !d) return ERR;
    int y0 = s->_begy > d->_begy ? s->_begy : d->_begy, x0 = s->_begx > d->_begx ? s->_begx : d->_begx;
    int y1 = (s->_begy + s->_maxy) < (d->_begy + d->_maxy) ? s->_begy + s->_maxy : d->_begy + d->_maxy;
    int x1 = (s->_begx + s->_maxx) < (d->_begx + d->_maxx) ? s->_begx + s->_maxx : d->_begx + d->_maxx;
    if (y0 > y1 || x0 > x1) return OK;
    return copy_win(s, d, y0 - s->_begy, x0 - s->_begx, y0 - d->_begy, x0 - d->_begx, y1 - d->_begy, x1 - d->_begx, over);
}
int overlay(const WINDOW *s, WINDOW *d) { return ov(s, d, 1); }
int overwrite(const WINDOW *s, WINDOW *d) { return ov(s, d, 0); }

/* ---------------------------------------------------------------- saída para o terminal */
static char obuf[16384];
static size_t on;
static void oflush(void) {
    size_t p = 0;
    while (p < on) { long k = write(1, obuf + p, on - p); if (k <= 0) break; p += (size_t)k; }
    on = 0;
}
static void oput(const char *s, size_t n) {
    if (on + n > sizeof obuf) oflush();
    if (n > sizeof obuf) { long k = write(1, s, n); (void)k; return; }
    memcpy(obuf + on, s, n);
    on += n;
}
static void ostr(const char *s) { oput(s, strlen(s)); }
static void ofmt(const char *f, ...) { char b[128]; va_list ap; va_start(ap, f); int n = vsnprintf(b, sizeof b, f, ap); va_end(ap); if (n > 0) oput(b, (size_t)n); }
static void out_utf8(unsigned c) {
    char b[4];
    int n;
    if (c < 0x80) { b[0] = (char)c; n = 1; }
    else if (c < 0x800) { b[0] = (char)(0xC0 | c >> 6); b[1] = (char)(0x80 | (c & 63)); n = 2; }
    else if (c < 0x10000) { b[0] = (char)(0xE0 | c >> 12); b[1] = (char)(0x80 | ((c >> 6) & 63)); b[2] = (char)(0x80 | (c & 63)); n = 3; }
    else { b[0] = (char)(0xF0 | c >> 18); b[1] = (char)(0x80 | ((c >> 12) & 63)); b[2] = (char)(0x80 | ((c >> 6) & 63)); b[3] = (char)(0x80 | (c & 63)); n = 4; }
    oput(b, (size_t)n);
}
/* traços do conjunto alternativo (VT100) como os caracteres do Unicode */
static unsigned acs_unicode(unsigned c) {
    switch (c) {
    case 'j': return 0x2518; case 'k': return 0x2510; case 'l': return 0x250C; case 'm': return 0x2514; case 'n': return 0x253C;
    case 'q': return 0x2500; case 't': return 0x251C; case 'u': return 0x2524; case 'v': return 0x2534; case 'w': return 0x252C;
    case 'x': return 0x2502; case 'a': return 0x2592; case '`': return 0x25C6; case 'f': return 0x00B0; case 'g': return 0x00B1;
    case '~': return 0x00B7; case ',': return 0x2190; case '+': return 0x2192; case '.': return 0x2193; case '-': return 0x2191;
    case 'h': return 0x2591; case 'i': return 0x2603; case '0': return 0x2588; case 'y': return 0x2264; case 'z': return 0x2265;
    case '{': return 0x03C0; case '|': return 0x2260; case '}': return 0x00A3; case 'o': return 0x23BA; case 's': return 0x23BD;
    default: return c;
    }
}
static chtype *scr_cell;
static unsigned *scr_wide;
static int scr_h, scr_w;
static attr_t cur_attr = (attr_t)-1;
static void sgr(attr_t a) {
    if (a == cur_attr) return;
    cur_attr = a;
    ostr("\x1b[0");
    if (a & (A_BOLD | A_STANDOUT)) ostr(";1");
    if (a & A_DIM) ostr(";2");
    if (a & A_UNDERLINE) ostr(";4");
    if (a & A_BLINK) ostr(";5");
    if (a & (A_REVERSE | A_STANDOUT)) ostr(";7");
    if (a & A_INVIS) ostr(";8");
    int p = (int)PAIR_OF(a);
    if (p && COLORS) {
        int fg = pair_fg[p], bg = pair_bg[p];
        if (fg >= 0) { if (fg < 8) ofmt(";%d", 30 + fg); else if (fg < 16) ofmt(";%d", 90 + fg - 8); else ofmt(";38;5;%d", fg); }
        if (bg >= 0) { if (bg < 8) ofmt(";%d", 40 + bg); else if (bg < 16) ofmt(";%d", 100 + bg - 8); else ofmt(";48;5;%d", bg); }
    }
    ostr("m");
}
static void scr_alloc(void) {
    free(scr_cell); free(scr_wide);
    scr_h = LINES; scr_w = COLS;
    scr_cell = calloc((size_t)scr_h * (size_t)scr_w, sizeof(chtype));
    scr_wide = calloc((size_t)scr_h * (size_t)scr_w, sizeof(unsigned));
    for (int i = 0; scr_cell && i < scr_h * scr_w; i++) scr_cell[i] = (chtype)-1;   /* tudo diferente: a primeira atualização desenha tudo */
}
int wnoutrefresh(WINDOW *w) {
    if (!w || !newscr) return ERR;
    if (w->_clear) { newscr->_clear = 1; w->_clear = 0; }
    for (int y = 0; y <= w->_maxy; y++) {
        int sy = w->_begy + y;
        if (sy < 0 || sy > newscr->_maxy) continue;
        if (!w->touched[y] && !newscr->_clear) continue;
        for (int x = 0; x <= w->_maxx; x++) {
            int sx = w->_begx + x;
            if (sx < 0 || sx > newscr->_maxx) continue;
            W(newscr, sy, sx) = W(w, y, x);
            WW(newscr, sy, sx) = WW(w, y, x);
        }
        newscr->touched[sy] = 1;
        w->touched[y] = 0;
    }
    if (!w->_leaveok) { newscr->_cury = (short)(w->_begy + w->_cury); newscr->_curx = (short)(w->_begx + w->_curx); }
    return OK;
}
int doupdate(void) {
    if (!newscr) return ERR;
    if (g_endwin) { extern int reset_prog_mode(void); reset_prog_mode(); g_endwin = 0; ostr("\x1b[?1049h"); newscr->_clear = 1; }
    if (!scr_cell || scr_h != LINES || scr_w != COLS) scr_alloc();
    if (newscr->_clear) { ostr("\x1b[0m\x1b[H\x1b[2J"); cur_attr = (attr_t)-1; for (int i = 0; i < scr_h * scr_w; i++) scr_cell[i] = (chtype)-1; newscr->_clear = 0; }
    ostr("\x1b[?25l");
    for (int y = 0; y <= newscr->_maxy && y < scr_h; y++) {
        if (!newscr->touched[y]) continue;
        int x = 0, last = -2;
        for (; x <= newscr->_maxx && x < scr_w; x++) {
            chtype c = W(newscr, y, x);
            unsigned wd = WW(newscr, y, x);
            size_t i = (size_t)y * (size_t)scr_w + (size_t)x;
            if (scr_cell[i] == c && scr_wide[i] == wd) continue;
            if (last != x - 1) ofmt("\x1b[%d;%dH", y + 1, x + 1);
            sgr(c & A_ATTRS & ~A_ALTCHARSET);
            unsigned ch = wd ? wd : (c & A_CHARTEXT);
            if (c & A_ALTCHARSET) ch = acs_unicode(ch);
            if (ch < 32) ch = ' ';
            out_utf8(ch);
            scr_cell[i] = c; scr_wide[i] = wd;
            last = x;
        }
        newscr->touched[y] = 0;
    }
    sgr(0);
    ofmt("\x1b[%d;%dH", newscr->_cury + 1, newscr->_curx + 1);
    if (g_cursor) ostr("\x1b[?25h");
    oflush();
    return OK;
}
int wrefresh(WINDOW *w) {
    if (!w) return ERR;
    if (w == curscr) { if (newscr) newscr->_clear = 1; return doupdate(); }
    return wnoutrefresh(w) ? ERR : doupdate();
}
static int pad_copy(WINDOW *p, int py, int px, int sy0, int sx0, int sy1, int sx1) {
    if (!p || !newscr) return ERR;
    if (py < 0) py = 0;
    if (px < 0) px = 0;
    for (int y = sy0; y <= sy1 && y <= newscr->_maxy; y++) {
        int yy = py + y - sy0;
        if (yy > p->_maxy) break;
        for (int x = sx0; x <= sx1 && x <= newscr->_maxx; x++) {
            int xx = px + x - sx0;
            if (xx > p->_maxx) break;
            W(newscr, y, x) = W(p, yy, xx); WW(newscr, y, x) = WW(p, yy, xx);
        }
        newscr->touched[y] = 1;
        p->touched[yy] = 0;
    }
    newscr->_cury = (short)(sy0 + p->_cury - py); newscr->_curx = (short)(sx0 + p->_curx - px);
    return OK;
}
int pnoutrefresh(WINDOW *p, int py, int px, int sy0, int sx0, int sy1, int sx1) { return pad_copy(p, py, px, sy0, sx0, sy1, sx1); }
int prefresh(WINDOW *p, int py, int px, int sy0, int sx0, int sy1, int sx1) { return pad_copy(p, py, px, sy0, sx0, sy1, sx1) ? ERR : doupdate(); }

/* ---------------------------------------------------------------- início e fim */
static void term_size(void) {
    struct { unsigned short row, col, x, y; } ws = {0, 0, 0, 0};
    if (ioctl(1, 0x40087468, &ws) == 0 && ws.row && ws.col) { LINES = ws.row; COLS = ws.col; }   /* TIOCGWINSZ do macOS */
    const char *l = getenv("LINES"), *c = getenv("COLUMNS");
    if (l && *l) { int v = 0; for (; *l >= '0' && *l <= '9'; l++) v = v * 10 + (*l - '0'); if (v) LINES = v; }
    if (c && *c) { int v = 0; for (; *c >= '0' && *c <= '9'; c++) v = v * 10 + (*c - '0'); if (v) COLS = v; }
}
static void acs_init(void) {
    const char *k = "jklmnqtuvwxa`fg~,+.-hi0yz{|}os";
    for (const char *p = k; *p; p++) acs_map[(unsigned char)*p] = (chtype)(unsigned char)*p | A_ALTCHARSET;
}
typedef struct { int fd; char name[32]; } TERMINAL;
TERMINAL *cur_term;
static TERMINAL g_term;
int setupterm(const char *term, int fd, int *err) {
    (void)fd;
    const char *t = term ? term : getenv("TERM");
    if (!t) t = "xterm-256color";
    if (!strcmp(t, "dumb")) { if (err) *err = 0; return ERR; }
    g_term.fd = fd;
    snprintf(g_term.name, sizeof g_term.name, "%s", t);
    cur_term = &g_term;
    if (err) *err = 1;
    term_size();
    return OK;
}
TERMINAL *set_curterm(TERMINAL *t) { TERMINAL *o = cur_term; cur_term = t; return o; }
int del_curterm(TERMINAL *t) { if (t == cur_term) cur_term = 0; return OK; }
int restartterm(const char *term, int fd, int *err) { return setupterm(term, fd, err); }
WINDOW *initscr(void) {
    if (g_init) return stdscr;
    setupterm(0, 1, 0);
    term_size();
    acs_init();
    if (tcgetattr(0, &g_shell) == 0) g_have_shell = 1;
    g_prog = g_shell;
    stdscr = newwin(LINES, COLS, 0, 0);
    newscr = newwin(LINES, COLS, 0, 0);
    curscr = newwin(LINES, COLS, 0, 0);
    if (!stdscr || !newscr || !curscr) return 0;
    scr_alloc();
    g_init = 1;
    ostr("\x1b[?1049h\x1b[H\x1b[2J");             /* tela alternativa (smcup) */
    oflush();
    return stdscr;
}
void *newterm(const char *type, FILE *out, FILE *in) { (void)out; (void)in; setupterm(type, 1, 0); return initscr(); }
void *set_term(void *s) { return s; }
void delscreen(void *s) { (void)s; }
int endwin(void) {
    if (!g_init) return ERR;
    ostr("\x1b[0m\x1b[?25h\x1b[?1049l");          /* rmcup */
    oflush();
    if (g_have_shell) { tcgetattr(0, &g_prog); tcsetattr(0, 0, &g_shell); }
    g_endwin = 1;
    return OK;
}
unsigned char isendwin(void) { return (unsigned char)g_endwin; }
int is_term_resized(int l, int c) { return l != LINES || c != COLS; }
int resize_term(int l, int c) {
    if (l <= 0 || c <= 0) return ERR;
    LINES = l; COLS = c;
    if (stdscr) wresize(stdscr, l, c);
    if (newscr) wresize(newscr, l, c);
    if (curscr) wresize(curscr, l, c);
    scr_alloc();
    if (newscr) newscr->_clear = 1;
    return OK;
}
int resizeterm(int l, int c) { return resize_term(l, c); }
void use_env(unsigned char b) { (void)b; }
void filter(void) { LINES = 1; }
int def_prog_mode(void) { return tcgetattr(0, &g_prog) == 0 ? OK : ERR; }
int def_shell_mode(void) { return tcgetattr(0, &g_shell) == 0 ? (g_have_shell = 1, OK) : ERR; }
int reset_prog_mode(void) { return tcsetattr(0, 0, &g_prog) == 0 ? OK : ERR; }
int reset_shell_mode(void) { return g_have_shell && tcsetattr(0, 0, &g_shell) == 0 ? OK : ERR; }
int savetty(void) { return tcgetattr(0, &g_saved) == 0 ? OK : ERR; }
int resetty(void) { return tcsetattr(0, 0, &g_saved) == 0 ? OK : ERR; }
static int tmode(void) {
    tio_t t;
    if (tcgetattr(0, &t)) return ERR;
    if (g_raw) { t.lflag &= ~(unsigned long)(T_ICANON | T_ISIG | T_IEXTEN); t.iflag &= ~(unsigned long)(T_IXON | T_ICRNL); }
    else if (g_cbreak) { t.lflag &= ~(unsigned long)T_ICANON; t.lflag |= T_ISIG; t.iflag |= T_ICRNL; }
    else t.lflag |= T_ICANON | T_ISIG;
    if (g_echo) t.lflag |= T_ECHO; else t.lflag &= ~(unsigned long)T_ECHO;
    if (!g_nl) t.iflag &= ~(unsigned long)T_ICRNL;
    t.cc[VMIN_] = 1; t.cc[VTIME_] = 0;
    return tcsetattr(0, 0, &t) == 0 ? OK : ERR;
}
int cbreak(void) { g_cbreak = 1; return tmode(); }
int nocbreak(void) { g_cbreak = 0; g_raw = 0; return tmode(); }
int raw(void) { g_raw = 1; return tmode(); }
int noraw(void) { g_raw = 0; return tmode(); }
int echo(void) { g_echo = 1; return tmode(); }
int noecho(void) { g_echo = 0; return tmode(); }
int nl(void) { g_nl = 1; return tmode(); }
int nonl(void) { g_nl = 0; return tmode(); }
int halfdelay(int t) { if (t < 1 || t > 255) return ERR; g_cbreak = 1; if (stdscr) stdscr->_delay = t * 100; return tmode(); }
void qiflush(void) {}
void noqiflush(void) {}
int intrflush(WINDOW *w, unsigned char b) { (void)w; (void)b; return OK; }
int meta(WINDOW *w, unsigned char b) { (void)w; (void)b; return OK; }
int typeahead(int fd) { (void)fd; return OK; }
int notimeout(WINDOW *w, unsigned char b) { if (!w) return ERR; w->_notimeout = b; return OK; }
int keypad(WINDOW *w, unsigned char b) { if (!w) return ERR; w->_use_keypad = b; ostr(b ? "\x1b[?1h\x1b=" : "\x1b[?1l\x1b>"); return OK; }
int nodelay(WINDOW *w, unsigned char b) { if (!w) return ERR; w->_delay = b ? 0 : -1; return OK; }
void wtimeout(WINDOW *w, int ms) { if (w) w->_delay = ms; }
int curs_set(int v) {
    int old = g_cursor;
    if (v < 0 || v > 2) return ERR;
    g_cursor = v;
    ostr(v ? "\x1b[?25h" : "\x1b[?25l");
    oflush();
    return old;
}
int napms(int ms) { usleep((unsigned)ms * 1000u); return OK; }
int delay_output(int ms) { return napms(ms); }
int beep(void) { ostr("\a"); oflush(); return OK; }
int flash(void) { ostr("\x1b[?5h"); oflush(); usleep(100000); ostr("\x1b[?5l"); oflush(); return OK; }
int baudrate(void) { return 38400; }
char erasechar(void) { return g_have_shell ? (char)g_shell.cc[3] : 127; }          /* VERASE */
char killchar(void) { return g_have_shell ? (char)g_shell.cc[5] : 21; }           /* VKILL */
char *longname(void) { return "xterm-256color (mOS)"; }
char *termname(void) { return cur_term ? cur_term->name : "xterm-256color"; }
attr_t termattrs(void) { return A_STANDOUT | A_UNDERLINE | A_REVERSE | A_BLINK | A_DIM | A_BOLD | A_INVIS | A_ALTCHARSET; }
unsigned char has_ic(void) { return 1; }
unsigned char has_il(void) { return 1; }
const char *curses_version(void) { return "ncurses 5.4.20040208 (mOS)"; }
int set_escdelay(int v) { ESCDELAY = v; return OK; }
int set_tabsize(int v) { if (v <= 0) return ERR; TABSIZE = v; return OK; }

/* ---------------------------------------------------------------- cores */
unsigned char has_colors(void) { const char *t = getenv("TERM"); return !t || strcmp(t, "dumb"); }
unsigned char can_change_color(void) { return 0; }
int start_color(void) {
    const char *t = getenv("TERM");
    COLORS = t && (strstr(t, "256")) ? 256 : 8;
    COLOR_PAIRS = MAXPAIRS;
    for (int i = 0; i < MAXPAIRS; i++) { pair_fg[i] = 7; pair_bg[i] = 0; }
    return OK;
}
int use_default_colors(void) { g_default_colors = 1; pair_fg[0] = pair_bg[0] = -1; return OK; }
int assume_default_colors(int fg, int bg) { g_default_colors = 1; pair_fg[0] = (short)fg; pair_bg[0] = (short)bg; return OK; }
int init_pair(short p, short fg, short bg) {
    if (p < 1 || p >= MAXPAIRS || fg >= COLORS || bg >= COLORS) return ERR;
    if ((fg < 0 || bg < 0) && !g_default_colors) return ERR;
    pair_fg[p] = fg; pair_bg[p] = bg;
    if (newscr) newscr->_clear = 1;
    return OK;
}
int pair_content(short p, short *fg, short *bg) {
    if (p < 0 || p >= MAXPAIRS) return ERR;
    if (fg) *fg = pair_fg[p];
    if (bg) *bg = pair_bg[p];
    return OK;
}
int init_color(short c, short r, short g, short b) { (void)c; (void)r; (void)g; (void)b; return ERR; }
static const short base8[8][3] = {{0, 0, 0}, {680, 0, 0}, {0, 680, 0}, {680, 680, 0}, {0, 0, 680}, {680, 0, 680}, {0, 680, 680}, {680, 680, 680}};
int color_content(short c, short *r, short *g, short *b) {
    if (c < 0 || c >= (COLORS ? COLORS : 8)) return ERR;
    short rr, gg, bb;
    if (c < 8) { rr = base8[c][0]; gg = base8[c][1]; bb = base8[c][2]; }
    else if (c < 16) { rr = c & 1 ? 1000 : 333; gg = c & 2 ? 1000 : 333; bb = c & 4 ? 1000 : 333; }
    else if (c < 232) { int k = c - 16; rr = (short)(k / 36 * 200); gg = (short)(k / 6 % 6 * 200); bb = (short)(k % 6 * 200); }
    else { short v = (short)((c - 232) * 1000 / 23); rr = gg = bb = v; }
    if (r) *r = rr;
    if (g) *g = gg;
    if (b) *b = bb;
    return OK;
}
int COLOR_PAIR(int n) { return (n & 0xff) << 8; }
int PAIR_NUMBER(int a) { return (int)(((unsigned)a & A_COLOR) >> 8); }

/* ---------------------------------------------------------------- entrada */
#define KEY_DOWN 0402
#define KEY_UP 0403
#define KEY_LEFT 0404
#define KEY_RIGHT 0405
#define KEY_HOME 0406
#define KEY_BACKSPACE 0407
#define KEY_F0 0410
#define KEY_DC 0512
#define KEY_IC 0513
#define KEY_NPAGE 0522
#define KEY_PPAGE 0523
#define KEY_BTAB 0541
#define KEY_END 0550
#define KEY_RESIZE 0632
static int pending[32], npend;
int ungetch(int c) { if (npend >= 32) return ERR; pending[npend++] = c; return OK; }
int unget_wch(int c) { return ungetch(c); }
static int rd_byte(int ms) {
    if (ms >= 0) {
        struct pollfd_ p = {0, 1, 0};
        if (poll(&p, 1, ms) <= 0) return -1;
    }
    unsigned char c;
    return read(0, &c, 1) == 1 ? c : -1;
}
int flushinp(void) { npend = 0; while (rd_byte(0) >= 0) {} return OK; }
static int decode_esc(void) {
    int c = rd_byte(ESCDELAY > 0 ? (ESCDELAY < 50 ? ESCDELAY : 50) : 0);
    if (c < 0) return 27;
    if (c != '[' && c != 'O') { ungetch(c); return 27; }
    int intro = c, n = 0, params[4] = {0}, np = 0;
    for (;;) {
        c = rd_byte(50);
        if (c < 0) return 27;
        if (c >= '0' && c <= '9') { n = n * 10 + (c - '0'); continue; }
        if (c == ';') { if (np < 4) params[np++] = n; n = 0; continue; }
        break;
    }
    if (np < 4) params[np++] = n;
    switch (c) {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    case 'H': return KEY_HOME;
    case 'F': return KEY_END;
    case 'Z': return KEY_BTAB;
    case 'P': return KEY_F0 + 1;
    case 'Q': return KEY_F0 + 2;
    case 'R': return KEY_F0 + 3;
    case 'S': return KEY_F0 + 4;
    case '~':
        switch (params[0]) {
        case 1: case 7: return KEY_HOME;
        case 2: return KEY_IC;
        case 3: return KEY_DC;
        case 4: case 8: return KEY_END;
        case 5: return KEY_PPAGE;
        case 6: return KEY_NPAGE;
        case 15: return KEY_F0 + 5;
        case 17: return KEY_F0 + 6; case 18: return KEY_F0 + 7; case 19: return KEY_F0 + 8; case 20: return KEY_F0 + 9;
        case 21: return KEY_F0 + 10; case 23: return KEY_F0 + 11; case 24: return KEY_F0 + 12;
        }
    }
    (void)intro;
    return 27;
}
int wgetch(WINDOW *w) {
    if (!w) return ERR;
    if (npend) { int c = pending[0]; npend--; memmove(pending, pending + 1, (size_t)npend * sizeof(int)); return c; }
    if (!w->pad && is_wintouched(w)) wrefresh(w);
    oflush();
    int c = rd_byte(w->_delay);
    if (c < 0) return ERR;
    if (c == 27 && w->_use_keypad) return decode_esc();
    if (c == 127 && w->_use_keypad) return KEY_BACKSPACE;
    if (g_echo && c >= 32 && c < 127) { waddch(w, (chtype)c); wrefresh(w); }
    return c;
}
int wget_wch(WINDOW *w, int *wc) {
    int c = wgetch(w);
    if (c == ERR) return ERR;
    if (c >= 0400) { if (wc) *wc = c; return 0400; }   /* KEY_CODE_YES */
    if (c >= 0xC0 && c < 0xF8) {                       /* resto do UTF-8 */
        int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
        unsigned v = (unsigned)c & (n == 1 ? 0x1F : n == 2 ? 0x0F : 0x07);
        for (int i = 0; i < n; i++) { int d = rd_byte(100); if (d < 0) break; v = v << 6 | ((unsigned)d & 0x3F); }
        c = (int)v;
    }
    if (wc) *wc = c;
    return OK;
}
int wgetnstr(WINDOW *w, char *s, int n) {
    if (!w || !s) return ERR;
    int k = 0;
    for (;;) {
        int c = wgetch(w);
        if (c == ERR) { if (w->_delay >= 0) break; continue; }
        if (c == '\n' || c == '\r' || c == 0527) break;      /* KEY_ENTER */
        if ((c == 127 || c == 8 || c == KEY_BACKSPACE) && k) { k--; continue; }
        if (c < 256 && (n < 0 || k < n)) s[k++] = (char)c;
    }
    s[k] = 0;
    return OK;
}
unsigned char has_key(int k) { return k == KEY_UP || k == KEY_DOWN || k == KEY_LEFT || k == KEY_RIGHT || k == KEY_HOME || k == KEY_END ||
                                      k == KEY_PPAGE || k == KEY_NPAGE || k == KEY_DC || k == KEY_IC || k == KEY_BACKSPACE || (k > KEY_F0 && k <= KEY_F0 + 12); }
const char *keyname(int k) {
    static char b[16];
    switch (k) {
    case KEY_UP: return "KEY_UP"; case KEY_DOWN: return "KEY_DOWN"; case KEY_LEFT: return "KEY_LEFT"; case KEY_RIGHT: return "KEY_RIGHT";
    case KEY_HOME: return "KEY_HOME"; case KEY_END: return "KEY_END"; case KEY_BACKSPACE: return "KEY_BACKSPACE";
    case KEY_DC: return "KEY_DC"; case KEY_IC: return "KEY_IC"; case KEY_NPAGE: return "KEY_NPAGE"; case KEY_PPAGE: return "KEY_PPAGE";
    case KEY_BTAB: return "KEY_BTAB"; case KEY_RESIZE: return "KEY_RESIZE";
    }
    if (k > KEY_F0 && k <= KEY_F0 + 63) { snprintf(b, sizeof b, "KEY_F(%d)", k - KEY_F0); return b; }
    if (k >= 0 && k < 32) { b[0] = '^'; b[1] = (char)(k + 64); b[2] = 0; return b; }
    if (k == 127) return "^?";
    if (k >= 32 && k < 127) { b[0] = (char)k; b[1] = 0; return b; }
    if (k >= 128 && k < 256) { snprintf(b, sizeof b, "M-%c", k - 128 >= 32 ? k - 128 : '?'); return b; }
    return 0;
}
char *unctrl(chtype c) {
    static char b[8];
    unsigned ch = c & A_CHARTEXT;
    if (ch < 32) { b[0] = '^'; b[1] = (char)(ch + 64); b[2] = 0; }
    else if (ch == 127) { b[0] = '^'; b[1] = '?'; b[2] = 0; }
    else if (ch >= 128) snprintf(b, sizeof b, "~%c", (ch & 127) >= 32 ? (char)(ch & 127) : '?');
    else { b[0] = (char)ch; b[1] = 0; }
    return b;
}
/* mouse: sem eventos de mouse no terminal do mOS */
typedef struct { short id; int x, y, z; unsigned long bstate; } MEVENT;
unsigned long mousemask(unsigned long m, unsigned long *old) { (void)m; if (old) *old = 0; return 0; }
int getmouse(MEVENT *e) { (void)e; return ERR; }
int ungetmouse(MEVENT *e) { (void)e; return ERR; }
int mouseinterval(int ms) { (void)ms; return 166; }
unsigned char wenclose(const WINDOW *w, int y, int x) {
    return w && y >= w->_begy && x >= w->_begx && y <= w->_begy + w->_maxy && x <= w->_begx + w->_maxx;
}
/* putwin/getwin: dimensões e as células */
int putwin(WINDOW *w, FILE *f) {
    if (!w || !f) return ERR;
    int hdr[4] = {0x4d4f5357, w->_maxy + 1, w->_maxx + 1, (int)w->_attrs};
    if (fwrite(hdr, sizeof hdr, 1, f) != 1) return ERR;
    for (int y = 0; y <= w->_maxy; y++)
        if (fwrite(&W(w, y, 0), sizeof(chtype), (size_t)w->_maxx + 1, f) != w->_maxx + 1 || fwrite(&WW(w, y, 0), sizeof(unsigned), (size_t)w->_maxx + 1, f) != w->_maxx + 1) return ERR;
    return OK;
}
WINDOW *getwin(FILE *f) {
    int hdr[4];
    if (!f || fread(hdr, sizeof hdr, 1, f) != 1 || hdr[0] != 0x4d4f5357 || hdr[1] <= 0 || hdr[2] <= 0 || hdr[1] > 10000 || hdr[2] > 10000) return 0;
    WINDOW *w = newwin(hdr[1], hdr[2], 0, 0);
    if (!w) return 0;
    w->_attrs = (attr_t)hdr[3];
    for (int y = 0; y < hdr[1]; y++)
        if (fread(&W(w, y, 0), sizeof(chtype), (size_t)hdr[2], f) != hdr[2] || fread(&WW(w, y, 0), sizeof(unsigned), (size_t)hdr[2], f) != hdr[2]) { delwin(w); return 0; }
    return w;
}

/* ---------------------------------------------------------------- terminfo e termcap embutidos (xterm-256color) */
typedef struct { const char *ti, *tc, *val; } cap_t;
static const cap_t caps[] = {
    {"clear", "cl", "\x1b[H\x1b[2J"}, {"cup", "cm", "\x1b[%i%p1%d;%p2%dH"}, {"el", "ce", "\x1b[K"}, {"ed", "cd", "\x1b[J"},
    {"home", "ho", "\x1b[H"}, {"smcup", "ti", "\x1b[?1049h"}, {"rmcup", "te", "\x1b[?1049l"}, {"bold", "md", "\x1b[1m"},
    {"dim", "mh", "\x1b[2m"}, {"rev", "mr", "\x1b[7m"}, {"smul", "us", "\x1b[4m"}, {"rmul", "ue", "\x1b[24m"},
    {"blink", "mb", "\x1b[5m"}, {"sgr0", "me", "\x1b[0m"}, {"smso", "so", "\x1b[7m"}, {"rmso", "se", "\x1b[27m"},
    {"civis", "vi", "\x1b[?25l"}, {"cnorm", "ve", "\x1b[?12l\x1b[?25h"}, {"cvvis", "vs", "\x1b[?12;25h"},
    {"setaf", "AF", "\x1b[%?%p1%{8}%<%t3%p1%d%e%p1%{16}%<%t9%p1%{8}%-%d%e38;5;%p1%d%;m"},
    {"setab", "AB", "\x1b[%?%p1%{8}%<%t4%p1%d%e%p1%{16}%<%t10%p1%{8}%-%d%e48;5;%p1%d%;m"},
    {"op", "op", "\x1b[39;49m"}, {"bel", "bl", "\a"}, {"cr", "cr", "\r"}, {"cub1", "le", "\b"}, {"cuf1", "nd", "\x1b[C"},
    {"cuu1", "up", "\x1b[A"}, {"cud1", "do", "\n"}, {"cub", "LE", "\x1b[%p1%dD"}, {"cuf", "RI", "\x1b[%p1%dC"},
    {"cuu", "UP", "\x1b[%p1%dA"}, {"cud", "DO", "\x1b[%p1%dB"}, {"hpa", "ch", "\x1b[%i%p1%dG"}, {"vpa", "cv", "\x1b[%i%p1%dd"},
    {"il1", "al", "\x1b[L"}, {"dl1", "dl", "\x1b[M"}, {"il", "AL", "\x1b[%p1%dL"}, {"dl", "DL", "\x1b[%p1%dM"},
    {"ich1", "ic", "\x1b[@"}, {"dch1", "dc", "\x1b[P"}, {"ech", "ec", "\x1b[%p1%dX"}, {"csr", "cs", "\x1b[%i%p1%d;%p2%dr"},
    {"ind", "sf", "\n"}, {"ri", "sr", "\x1bM"}, {"smkx", "ks", "\x1b[?1h\x1b="}, {"rmkx", "ke", "\x1b[?1l\x1b>"},
    {"kcuu1", "ku", "\x1bOA"}, {"kcud1", "kd", "\x1bOB"}, {"kcub1", "kl", "\x1bOD"}, {"kcuf1", "kr", "\x1bOC"},
    {"khome", "kh", "\x1bOH"}, {"kend", "@7", "\x1bOF"}, {"kbs", "kb", "\x7f"}, {"kdch1", "kD", "\x1b[3~"},
    {"kich1", "kI", "\x1b[2~"}, {"knp", "kN", "\x1b[6~"}, {"kpp", "kP", "\x1b[5~"}, {"kf1", "k1", "\x1bOP"},
    {"kf2", "k2", "\x1bOQ"}, {"kf3", "k3", "\x1bOR"}, {"kf4", "k4", "\x1bOS"}, {"kf5", "k5", "\x1b[15~"},
    {"kf6", "k6", "\x1b[17~"}, {"kf7", "k7", "\x1b[18~"}, {"kf8", "k8", "\x1b[19~"}, {"kf9", "k9", "\x1b[20~"},
    {"kf10", "k;", "\x1b[21~"}, {"kf11", "F1", "\x1b[23~"}, {"kf12", "F2", "\x1b[24~"}, {"acsc", "ac", "``aaffggiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz{{||}}~~"},
    {"smacs", "as", "\x1b(0"}, {"rmacs", "ae", "\x1b(B"}, {"tsl", "ts", "\x1b]2;"}, {"fsl", "fs", "\a"}, {"flash", "vb", "\x1b[?5h\x1b[?5l"},
    {0, 0, 0}};
static const char *const bool_ti[] = {"am", "bce", "km", "mir", "msgr", "xenl", "ccc", "hs", 0};
static const char *const bool_tc[] = {"am", "ut", "km", "mi", "ms", "xn", "cc", "hs", 0};
char *const boolnames[] = {"bw", "am", "xsb", "xhp", "xenl", "eo", "gn", "hc", "km", "hs", "in", "da", "db", "mir", "msgr", "os",
                           "eslok", "xt", "hz", "ul", "xon", "nxon", "mc5i", "chts", "nrrmc", "npc", "ndscr", "ccc", "bce", "hls",
                           "xhpa", "crxm", "daisy", "xvpa", "sam", "cpix", "lpix", 0};
char *const numnames[] = {"cols", "it", "lines", "lm", "xmc", "pb", "vt", "wsl", "nlab", "lh", "lw", "ma", "wnum", "colors", "pairs", "ncv", 0};
char *const strnames[] = {"cbt", "bel", "cr", "csr", "tbc", "clear", "el", "ed", "hpa", "cmdch", "cup", "cud1", "home", "civis",
                          "cub1", "mrcup", "cnorm", "cuf1", "ll", "cuu1", "cvvis", "dch1", "dl1", "dsl", "hd", "smacs", "blink",
                          "bold", "smcup", "smdc", "dim", "smir", "invis", "prot", "rev", "smso", "smul", "ech", "rmacs", "sgr0",
                          "rmcup", "rmdc", "rmir", "rmso", "rmul", "flash", "ff", "fsl", "is1", "is2", "is3", "if", "ich1", "il1",
                          "ip", "kbs", 0};
static const char *cap_find(const char *name, int tc) {
    for (const cap_t *c = caps; c->ti; c++) if (!strcmp(tc ? c->tc : c->ti, name)) return c->val;
    return 0;
}
char *tigetstr(const char *name) { const char *v = name ? cap_find(name, 0) : 0; return v ? (char *)v : (char *)-1; }   /* -1: não é string */
int tigetnum(const char *name) {
    if (!name) return -2;
    if (!strcmp(name, "cols")) return COLS;
    if (!strcmp(name, "lines")) return LINES;
    if (!strcmp(name, "colors")) { const char *t = getenv("TERM"); return t && strstr(t, "256") ? 256 : 8; }
    if (!strcmp(name, "pairs")) return 32767;
    if (!strcmp(name, "it")) return 8;
    return -1;
}
int tigetflag(const char *name) { for (int i = 0; name && bool_ti[i]; i++) if (!strcmp(bool_ti[i], name)) return 1; return 0; }
int tgetent(char *bp, const char *name) { (void)bp; return setupterm(name, 1, 0) == OK ? 1 : 0; }
int tgetflag(const char *id) { for (int i = 0; id && bool_tc[i]; i++) if (!strncmp(bool_tc[i], id, 2)) return 1; return 0; }
int tgetnum(const char *id) {
    if (!id) return -1;
    if (!strncmp(id, "co", 2)) return COLS;
    if (!strncmp(id, "li", 2)) return LINES;
    if (!strncmp(id, "Co", 2)) return tigetnum("colors");
    if (!strncmp(id, "it", 2)) return 8;
    return -1;
}
char *tgetstr(const char *id, char **area) {
    char k[3] = {id ? id[0] : 0, id && id[0] ? id[1] : 0, 0};
    const char *v = cap_find(k, 1);
    if (!v) return 0;
    if (area && *area) { size_t n = strlen(v) + 1; memcpy(*area, v, n); char *r = *area; *area += n; return r; }
    return (char *)v;
}
/* interpretador de parâmetros do terminfo (%p %d %c %s %i %{n} %'c' + - * / m & | ^ = < > ! ~ A O %? %t %e %;) */
static char tp_out[512];
static long tparm_core(const char *s, const long *p, int np) {
    long st[32];
    int sp = 0, inc = 0;
    long par[9];
    for (int i = 0; i < 9; i++) par[i] = i < np ? p[i] : 0;
    size_t o = 0;
#define PUSH(v) do { if (sp < 32) st[sp++] = (v); } while (0)
#define POP() (sp ? st[--sp] : 0)
    for (; *s && o + 32 < sizeof tp_out; s++) {
        if (*s != '%') { tp_out[o++] = *s; continue; }
        s++;
        switch (*s) {
        case '%': tp_out[o++] = '%'; break;
        case 'i': if (!inc) { par[0]++; par[1]++; inc = 1; } break;
        case 'p': s++; if (*s >= '1' && *s <= '9') PUSH(par[*s - '1']); break;
        case 'd': o += (size_t)snprintf(tp_out + o, sizeof tp_out - o, "%ld", POP()); break;
        case 'c': tp_out[o++] = (char)POP(); break;
        case 's': { const char *x = (const char *)(uintptr_t)POP(); if (x) { size_t n = strlen(x); if (o + n + 1 < sizeof tp_out) { memcpy(tp_out + o, x, n); o += n; } } break; }
        case '{': { long v = 0; s++; while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'); PUSH(v); break; }
        case '\'': s++; PUSH((unsigned char)*s); s++; break;
        case '+': { long b = POP(), a = POP(); PUSH(a + b); break; }
        case '-': { long b = POP(), a = POP(); PUSH(a - b); break; }
        case '*': { long b = POP(), a = POP(); PUSH(a * b); break; }
        case '/': { long b = POP(), a = POP(); PUSH(b ? a / b : 0); break; }
        case 'm': { long b = POP(), a = POP(); PUSH(b ? a % b : 0); break; }
        case '&': { long b = POP(), a = POP(); PUSH(a & b); break; }
        case '|': { long b = POP(), a = POP(); PUSH(a | b); break; }
        case '^': { long b = POP(), a = POP(); PUSH(a ^ b); break; }
        case '=': { long b = POP(), a = POP(); PUSH(a == b); break; }
        case '<': { long b = POP(), a = POP(); PUSH(a < b); break; }
        case '>': { long b = POP(), a = POP(); PUSH(a > b); break; }
        case 'A': { long b = POP(), a = POP(); PUSH(a && b); break; }
        case 'O': { long b = POP(), a = POP(); PUSH(a || b); break; }
        case '!': { long a = POP(); PUSH(!a); break; }
        case '~': { long a = POP(); PUSH(~a); break; }
        case '?': break;
        case 't': if (!POP()) {                        /* pula até o %e ou %; do mesmo nível */
                int lvl = 0;
                for (s++; *s; s++) {
                    if (*s != '%') continue;
                    s++;
                    if (*s == '?') lvl++;
                    else if (*s == ';') { if (!lvl) break; lvl--; }
                    else if (*s == 'e' && !lvl) break;
                }
            }
            break;
        case 'e': {                                    /* veio do ramo verdadeiro: pula até o %; */
            int lvl = 0;
            for (s++; *s; s++) {
                if (*s != '%') continue;
                s++;
                if (*s == '?') lvl++;
                else if (*s == ';') { if (!lvl) break; lvl--; }
            }
            break;
        }
        case ';': break;
        default: break;
        }
        if (!*s) break;
    }
    tp_out[o] = 0;
#undef PUSH
#undef POP
    return 0;
}
char *tparm(const char *s, long p1, long p2, long p3, long p4, long p5, long p6, long p7, long p8, long p9) {
    if (!s) return 0;
    long p[9] = {p1, p2, p3, p4, p5, p6, p7, p8, p9};
    tparm_core(s, p, 9);
    return tp_out;
}
char *tiparm(const char *s, ...) {
    if (!s) return 0;
    long p[9] = {0};
    va_list ap; va_start(ap, s);
    for (int i = 0; i < 9; i++) p[i] = va_arg(ap, long);
    va_end(ap);
    tparm_core(s, p, 9);
    return tp_out;
}
/* tgoto(cm, coluna, linha): o termcap passa a coluna primeiro; o cm daqui é terminfo (linha em %p1) */
char *tgoto(const char *cm, int col, int row) { return tparm(cm, row, col, 0, 0, 0, 0, 0, 0, 0); }
int tputs(const char *s, int affcnt, int (*putc_)(int)) {
    (void)affcnt;
    if (!s || !putc_) return ERR;
    for (; *s; s++) {
        if (s[0] == '$' && s[1] == '<') { while (*s && *s != '>') s++; if (!*s) break; continue; }   /* atraso: ignorado */
        putc_((unsigned char)*s);
    }
    return OK;
}
static int put_out(int c) { char ch = (char)c; oput(&ch, 1); oflush(); return c; }
int putp(const char *s) { return tputs(s, 1, put_out); }

/* ---------------------------------------------------------------- painéis */
typedef struct panel { WINDOW *win; struct panel *above, *below; const void *user; int hidden; } PANEL;
static PANEL *p_bottom, *p_top;
static void p_unlink(PANEL *p) {
    if (p->above) p->above->below = p->below; else if (p_top == p) p_top = p->below;
    if (p->below) p->below->above = p->above; else if (p_bottom == p) p_bottom = p->above;
    p->above = p->below = 0;
}
static void p_push_top(PANEL *p) { p->below = p_top; p->above = 0; if (p_top) p_top->above = p; p_top = p; if (!p_bottom) p_bottom = p; }
PANEL *new_panel(WINDOW *w) { if (!w) return 0; PANEL *p = calloc(1, sizeof *p); if (!p) return 0; p->win = w; p_push_top(p); return p; }
int del_panel(PANEL *p) { if (!p) return ERR; if (!p->hidden) p_unlink(p); free(p); return OK; }
int top_panel(PANEL *p) { if (!p) return ERR; if (!p->hidden) p_unlink(p); p->hidden = 0; p_push_top(p); return OK; }
int bottom_panel(PANEL *p) {
    if (!p) return ERR;
    if (!p->hidden) p_unlink(p);
    p->hidden = 0;
    p->above = p_bottom; p->below = 0;
    if (p_bottom) p_bottom->below = p;
    p_bottom = p;
    if (!p_top) p_top = p;
    return OK;
}
int hide_panel(PANEL *p) { if (!p) return ERR; if (!p->hidden) { p_unlink(p); p->hidden = 1; } return OK; }
int show_panel(PANEL *p) { return top_panel(p); }
int panel_hidden(const PANEL *p) { return p ? p->hidden : ERR; }
PANEL *panel_above(const PANEL *p) { return p ? p->above : p_bottom; }
PANEL *panel_below(const PANEL *p) { return p ? p->below : p_top; }
WINDOW *panel_window(const PANEL *p) { return p ? p->win : 0; }
int replace_panel(PANEL *p, WINDOW *w) { if (!p || !w) return ERR; p->win = w; return OK; }
int move_panel(PANEL *p, int y, int x) { return p ? mvwin(p->win, y, x) : ERR; }
int set_panel_userptr(PANEL *p, const void *u) { if (!p) return ERR; p->user = u; return OK; }
const void *panel_userptr(const PANEL *p) { return p ? p->user : 0; }
void update_panels(void) {
    for (PANEL *p = p_bottom; p; p = p->above) { redrawwin(p->win); wnoutrefresh(p->win); }
}
