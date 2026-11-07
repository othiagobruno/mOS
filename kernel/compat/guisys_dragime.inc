/* incluído por guisys.c — arrastar e soltar entre janelas e o método de entrada (CJK) */

/* ------------------------------------------------------------ arrastar e soltar entre janelas
 * Uma sessão por vez, de qualquer app: a fonte entrega o conteúdo (tipos do pasteboard serializados) e o
 * window server segue o cursor. A janela de app do macOS que estiver embaixo recebe ENTER/MOVE/EXIT e, ao
 * soltar, DROP; ela responde com DG_STATUS (operação aceita) e DG_FINISH (operação feita), que vira
 * GEV_DRAG_END na fonte. O cursor mostra "copiar" quando o destino aceita. */
static struct {
    bool active;                 /* botão ainda pressionado: seguindo o cursor */
    bool waiting;                /* soltou num destino que ainda não respondeu DG_FINISH */
    int seq;
    gapp_t *src;
    int src_win;
    u8 *data;
    u32 len, mask;
    int tgt_win;                 /* janela sob o cursor (0 = nenhuma de app do macOS) */
    u32 reply;                   /* operação que o destino aceita */
    int lx, ly;
} drag;

static void drag_push(gapp_t *ga, int win, u32 type, int x, int y, int sx, int sy, u32 key) {
    if (!ga) return;
    gui_event_t e;
    memset(&e, 0, sizeof e);
    e.type = type; e.win = (u32)win;
    e.x = x; e.y = y; e.w = sx; e.h = sy;
    e.key = (int)key; e.clicks = drag.seq;
    push(ga, &e);
}
static void drag_end(u32 op) {
    if (drag.src) {
        int x = 0, y = 0;
        gwin_t *s = drag.src_win > 0 && drag.src_win < MAX_GWIN && wins[drag.src_win].used ? &wins[drag.src_win] : NULL;
        if (s && s->w) { rect_t cr = wm_content_rect(s->w); x = drag.lx - cr.x; y = drag.ly - cr.y; }
        drag_push(drag.src, drag.src_win, GEV_DRAG_END, x, y, drag.lx, drag.ly, op);
    }
    drag.active = drag.waiting = false;
    drag.tgt_win = 0;
    drag.reply = 0;
    wm_set_cursor(CURSOR_ARROW);
}
/* janela de app do macOS sob (x, y) e a posição dentro do conteúdo */
static gwin_t *gwin_at(int x, int y, int *cx, int *cy) {
    window_t *w = wm_window_at_point(x, y);
    gwin_t *g = w ? gwin_of(w) : NULL;
    if (!g) return NULL;
    rect_t cr = wm_content_rect(w);
    if (!rect_contains(cr, x, y)) return NULL;
    *cx = x - cr.x; *cy = y - cr.y;
    return g;
}
static void drag_track(void) {
    if (!drag.active) return;
    int x = wm_mouse_x(), y = wm_mouse_y(), cx = 0, cy = 0;
    bool down = wm_mouse_buttons() & 1, moved = x != drag.lx || y != drag.ly;
    gwin_t *g = gwin_at(x, y, &cx, &cy);
    int tgt = g ? g->id : 0;
    if (tgt != drag.tgt_win) {
        if (drag.tgt_win && wins[drag.tgt_win].used)
            drag_push(wins[drag.tgt_win].ga, drag.tgt_win, GEV_DRAG_EXIT, 0, 0, x, y, drag.mask);
        drag.tgt_win = tgt;
        drag.reply = 0;
        if (g) drag_push(g->ga, tgt, GEV_DRAG_ENTER, cx, cy, x, y, drag.mask);
        wm_set_cursor(CURSOR_ARROW);
    } else if (moved && g) drag_push(g->ga, tgt, GEV_DRAG_MOVE, cx, cy, x, y, drag.mask);
    if (moved && drag.src) {
        int sx = 0, sy = 0;
        gwin_t *s = &wins[drag.src_win];
        if (s->used && s->w) { rect_t cr = wm_content_rect(s->w); sx = x - cr.x; sy = y - cr.y; }
        drag_push(drag.src, drag.src_win, GEV_DRAG_SRC_MOVE, sx, sy, x, y, 0);
    }
    drag.lx = x; drag.ly = y;
    if (!down) {
        drag.active = false;
        if (g && drag.reply) {
            drag_push(g->ga, tgt, GEV_DRAG_DROP, cx, cy, x, y, drag.mask);
            drag.waiting = true;           /* a fonte fica sabendo quando o destino chamar DG_FINISH */
        } else {
            if (g) drag_push(g->ga, tgt, GEV_DRAG_EXIT, 0, 0, x, y, drag.mask);
            drag_end(0);
        }
    }
}
/* processo que sai no meio de um arrasto: a outra ponta não fica esperando */
static void drag_forget_app(gapp_t *ga) {
    if (!drag.active && !drag.waiting) return;
    if (drag.src == ga) {
        drag.src = NULL;
        if (drag.tgt_win && wins[drag.tgt_win].used && wins[drag.tgt_win].ga != ga)
            drag_push(wins[drag.tgt_win].ga, drag.tgt_win, GEV_DRAG_EXIT, 0, 0, drag.lx, drag.ly, drag.mask);
        drag_end(0);
    } else if (drag.tgt_win && wins[drag.tgt_win].used && wins[drag.tgt_win].ga == ga) {
        drag.tgt_win = 0;
        drag.reply = 0;
        if (drag.waiting) drag_end(0);
    }
}

static i64 drag_syscall(process_t *p, u64 op, u64 b, u64 c, u64 d, u64 e) {
    switch (op) {
    case DG_BEGIN: {
        gwin_t *g = win_get(p, b);
        if (!g || d > GUI_DRAG_MAX) return -MOS_EINVAL;
        u8 *buf = kmalloc(d ? (usize)d : 1);
        if (!buf) return -MOS_ENOMEM;
        if (d && !uvm_copy_in(p, buf, c, d)) { kfree(buf); return -MOS_EFAULT; }
        if (drag.active || drag.waiting) drag_end(0);   /* uma sessão nova encerra a anterior */
        kfree(drag.data);
        drag.data = buf; drag.len = (u32)d; drag.mask = (u32)e;
        drag.src = g->ga; drag.src_win = g->id;
        drag.seq++;
        drag.active = true; drag.waiting = false;
        drag.tgt_win = 0; drag.reply = 0;
        drag.lx = -1; drag.ly = -1;
        drag_track();
        return drag.seq;
    }
    case DG_DATA:
        if (!drag.data) return -MOS_ENOENT;
        if (b && c) {
            u32 n = drag.len < (u32)c ? drag.len : (u32)c;
            if (!uvm_copy_out(p, b, drag.data, n)) return -MOS_EFAULT;
        }
        return drag.len;
    case DG_STATUS: {
        if ((int)b != drag.seq || !drag.active || !drag.tgt_win) return -MOS_EINVAL;
        gwin_t *t = &wins[drag.tgt_win];
        if (!t->used || t->ga->pid != p->pid) return -MOS_EINVAL;
        drag.reply = (u32)c & (drag.mask ? drag.mask : ~0u);
        wm_set_cursor(drag.reply ? CURSOR_COPY : CURSOR_ARROW);
        return 0;
    }
    case DG_FINISH:
        if ((int)b != drag.seq || !drag.waiting) return -MOS_EINVAL;
        drag_end((u32)c);
        return 0;
    case DG_CANCEL:
        if ((int)b != drag.seq || !drag.src || drag.src->pid != p->pid) return -MOS_EINVAL;
        if (drag.tgt_win && wins[drag.tgt_win].used)
            drag_push(wins[drag.tgt_win].ga, drag.tgt_win, GEV_DRAG_EXIT, 0, 0, drag.lx, drag.ly, drag.mask);
        drag_end(0);
        return 0;
    }
    return -MOS_EINVAL;
}

/* ------------------------------------------------------------ método de entrada (CJK)
 * A composição e a escolha de candidatos acontecem no processo do app (NSTextInputContext do libSystem);
 * aqui fica só o que é global: a fonte de entrada escolhida (⌃Espaço) e a janelinha de candidatos, desenhada
 * por cima do conteúdo da janela, perto do cursor de texto. */
static int ime_source;                         /* 0 teclado, 1 japonês, 2 chinês (pinyin), 3 coreano */
static const char *ime_names[] = {"Teclado (ABNT2)", "Japonês – Romaji", "Chinês Simplificado – Pinyin", "Coreano – 2-Set"};
#define IME_SOURCES 4
void compat_ime_cycle(int dir) {
    ime_source = ((ime_source + dir) % IME_SOURCES + IME_SOURCES) % IME_SOURCES;
    gui_event_t e;
    memset(&e, 0, sizeof e);
    e.type = GEV_INPUT_SOURCE;
    e.key = ime_source;
    for (gapp_t *ga = gapps; ga; ga = ga->next) push(ga, &e);
    notify("Fonte de entrada", ime_names[ime_source]);
}

static void ime_draw(gwin_t *g, surface_t *s) {
    if (!g->ime || g->ime_len < 12) return;
    u32 sel, n, fl;
    memcpy(&sel, g->ime, 4); memcpy(&n, g->ime + 4, 4); memcpy(&fl, g->ime + 8, 4);
    const char *p = g->ime + 12, *end = g->ime + g->ime_len;
    font_t *f = font_get(FONT_REGULAR), *fb = font_get(FONT_SEMIBOLD);
    const float sz = 15;
    const int lh = 24, pad = 6;
    const char *comp = p;
    p += strlen(p) + 1;
    if (n > 9) n = 9;
    int wmax = (fl & 1) ? text_width(f, sz, comp) : 0;
    const char *q = p;
    for (u32 i = 0; i < n && q < end; i++) { int tw = text_width(f, sz, q) + 22; if (tw > wmax) wmax = tw; q += strlen(q) + 1; }
    int rows = (int)n + ((fl & 1) ? 1 : 0);
    if (!rows) return;
    int w = wmax + 2 * pad + 10, h = rows * lh + 2 * pad;
    int x = g->ime_x, y = g->ime_y + 4;
    if (x + w > s->w) x = s->w - w;
    if (y + h > s->h) y = g->ime_y - h - 24;       /* sem espaço embaixo: acima da linha */
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    gfx_round_rect(s, x + 1, y + 2, w, h, 9, RGBA(0, 0, 0, 40));
    gfx_round_rect(s, x, y, w, h, 9, T.dark ? RGB(44, 44, 46) : RGB(250, 250, 252));
    gfx_round_rect_stroke(s, x, y, w, h, 9, 1, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 36));
    u32 tc = T.dark ? RGB(240, 240, 245) : RGB(28, 28, 30);
    int ry = y + pad;
    if (fl & 1) {
        int tw = text_draw(s, f, sz, x + pad + 4, ry + 17, tc, comp);
        gfx_fill(s, x + pad + 4, ry + 20, tw, 1, tc);   /* sublinhado da composição */
        ry += lh;
    }
    for (u32 i = 0; i < n && p < end; i++) {
        bool on = i == sel;
        if (on) gfx_round_rect(s, x + 3, ry, w - 6, lh, 6, T.accent);
        char num[2] = {(char)('1' + i), 0};
        text_draw(s, fb, 12, x + pad + 4, ry + 16, on ? RGB(255, 255, 255) : RGB(128, 128, 136), num);
        text_draw(s, f, sz, x + pad + 20, ry + 17, on ? RGB(255, 255, 255) : tc, p);
        p += strlen(p) + 1;
        ry += lh;
    }
}

static i64 ime_syscall(process_t *p, u64 op, u64 b, u64 c, u64 d, u64 e) {
    switch (op) {
    case IME_SHOW: {
        gwin_t *g = win_get(p, b);
        if (!g || d < 12 || d > GUI_IME_MAX) return -MOS_EINVAL;
        char *buf = kmalloc((usize)d + 1);
        if (!buf) return -MOS_ENOMEM;
        if (!uvm_copy_in(p, buf, c, d)) { kfree(buf); return -MOS_EFAULT; }
        buf[d] = 0;
        kfree(g->ime);
        g->ime = buf; g->ime_len = (u32)d;
        g->ime_x = (int)(i32)(e & 0xFFFFFFFF); g->ime_y = (int)(i32)(e >> 32);
        if (g->w) wm_invalidate(g->w);
        return 0;
    }
    case IME_HIDE: {
        gwin_t *g = win_get(p, b);
        if (!g) return -MOS_EBADF;
        if (g->ime) { kfree(g->ime); g->ime = NULL; g->ime_len = 0; if (g->w) wm_invalidate(g->w); }
        return 0;
    }
    case IME_SOURCE_GET: return ime_source;
    case IME_SOURCE_SET:
        if (b >= IME_SOURCES) return -MOS_EINVAL;
        if ((int)b != ime_source) compat_ime_cycle((int)b - ime_source);
        return 0;
    }
    return -MOS_EINVAL;
}
