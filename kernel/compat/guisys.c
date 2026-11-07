/*
 * mOS — janelas para processos de usuário (a ponte usada pelo AppKit do mOS).
 *
 * Cada janela de um app tem uma janela "proxy" no window server e uma superfície de fundo no
 * kernel. O app desenha com lotes de comandos (GUI_DRAW) e publica com GUI_PRESENT; a função
 * paint da proxy só copia a superfície e transforma o estado do mouse/teclado em eventos, que
 * o app busca com GUI_POLL. Syscalls rodam com o desktop parado, então tudo aqui é sequencial.
 */
#include "proc.h"
#include "ui.h"
#include "compat_gui.h"
#include "syscall.h"

#define MAX_GWIN 64
#define QLEN 256

typedef struct gapp {
    app_t app;
    int pid;
    char name[32];
    char bundle[160];       /* .app do executável ("" se não veio de um bundle): ícone no Dock */
    int nwin;
    gui_event_t q[QLEN];
    int qh, qt;
    struct gapp *next;
} gapp_t;

typedef struct {
    bool used;
    int id;
    gapp_t *ga;
    window_t *w;
    surface_t *back;        /* o que o app desenhou */
    surface_t *shown;       /* último quadro publicado */
    int lmx, lmy;
    bool linside, ldown;
    char *ime;              /* candidatos do método de entrada (formato de IME_SHOW) ou NULL */
    u32 ime_len;
    int ime_x, ime_y;
} gwin_t;

static gwin_t wins[MAX_GWIN];
static gapp_t *gapps;

/* ------------------------------------------------------------ eventos */
static void push(gapp_t *ga, const gui_event_t *e) {
    int n = (ga->qt + 1) % QLEN;
    if (n == ga->qh) return;                       /* fila cheia: descarta */
    ga->q[ga->qt] = *e;
    ga->qt = n;
}

static void drag_track(void);
static bool drag_is_source(gapp_t *ga);
static void ime_draw(gwin_t *g, surface_t *s);
static void drag_forget_app(gapp_t *ga);

static gwin_t *gwin_of(window_t *w) {
    for (int i = 0; i < MAX_GWIN; i++) if (wins[i].used && wins[i].w == w) return &wins[i];
    return NULL;
}

static void resize_surface(surface_t **sp, int w, int h, u32 fill) {
    surface_t *old = *sp, *n = surface_new(MAX(w, 1), MAX(h, 1));
    if (!n) return;
    gfx_clear(n, fill);
    if (old) { gfx_blit(n, old, 0, 0); surface_free(old); }
    *sp = n;
}

static void proxy_paint(window_t *w, ui_t *u) {
    gwin_t *g = gwin_of(w);
    if (!g) return;
    gui_event_t e;
    memset(&e, 0, sizeof e);
    e.win = (u32)g->id;
    /* redimensionamento: o app redesenha no novo tamanho */
    if (g->shown && (g->shown->w != u->cw || g->shown->h != u->ch)) {
        resize_surface(&g->shown, u->cw, u->ch, T.window_bg);
        resize_surface(&g->back, u->cw, u->ch, T.window_bg);
        e.type = GEV_RESIZE; e.w = u->cw; e.h = u->ch;
        push(g->ga, &e);
    }
    if (g->shown) gfx_blit(u->s, g->shown, 0, 0);
    if (g->ime) ime_draw(g, u->s);                 /* candidatos do IME por cima do conteúdo */
    /* arrasto em andamento: o window server segue o cursor; os cliques e movimentos dele não vão aos apps */
    bool dragging = drag_is_source(NULL);
    drag_track();
    if (dragging) { g->lmx = u->mx; g->lmy = u->my; u->clicked_any = false; return; }
    e.x = u->mx; e.y = u->my; e.buttons = u->down ? 1 : 0;
    e.mods = u->mods;
    if (u->pressed && u->inside) { e.type = GEV_MOUSE_DOWN; e.clicks = u->dblclick ? 2 : 1; push(g->ga, &e); u->active = 1; }
    if (u->released) { e.type = GEV_MOUSE_UP; e.clicks = 1; push(g->ga, &e); }
    if (u->rpressed && u->inside) { e.type = GEV_RIGHT_DOWN; push(g->ga, &e); }
    if (u->wheel && u->inside) { e.type = GEV_SCROLL; e.w = u->wheel; push(g->ga, &e); }
    if ((u->mx != g->lmx || u->my != g->lmy) && (u->inside || u->down)) {
        e.type = GEV_MOUSE_MOVE; e.clicks = 0; push(g->ga, &e);
    }
    g->lmx = u->mx; g->lmy = u->my;
    u->clicked_any = false;
}

static void proxy_key(window_t *w, input_event_t *ev) {
    gwin_t *g = gwin_of(w);
    if (!g || ev->type != EV_KEY_DOWN) return;
    gui_event_t e;
    memset(&e, 0, sizeof e);
    e.type = GEV_KEY_DOWN;
    e.win = (u32)g->id;
    e.key = ev->key;
    e.mods = ev->mods;
    if (ev->key >= 32 && ev->key < 0x100 && ev->key != 127 && !(ev->mods & (MOD_CMD | MOD_CTRL))) utf8_encode((u32)ev->key, e.text);
    push(g->ga, &e);
}
/* tecla solta (wm.c chama para a janela em foco; só as janelas de apps do macOS se interessam) */
void compat_gui_key_up(window_t *w, input_event_t *ev) {
    gwin_t *g = gwin_of(w);
    if (!g || ev->type != EV_KEY_UP) return;
    gui_event_t e;
    memset(&e, 0, sizeof e);
    e.type = GEV_KEY_UP;
    e.win = (u32)g->id;
    e.key = ev->key;
    e.mods = ev->mods;
    if (ev->key >= 32 && ev->key < 0x100 && ev->key != 127 && !(ev->mods & (MOD_CMD | MOD_CTRL))) utf8_encode((u32)ev->key, e.text);
    push(g->ga, &e);
}

static void proxy_close(window_t *w) {
    gwin_t *g = gwin_of(w);
    if (!g) return;
    gui_event_t e;
    memset(&e, 0, sizeof e);
    e.type = GEV_CLOSE;
    e.win = (u32)g->id;
    push(g->ga, &e);
    g->w = NULL;                                   /* a janela do window server deixa de existir */
}
/* botão vermelho, ⌘W do sistema, Dock: como no macOS, o app recebe o pedido (performClose:) e pode recusar
 * (o windowShouldClose: do Chrome recusa e fecha as abas antes); se aceitar, destrói a janela por GUI_WIN_DESTROY.
 * GEV_CLOSE com key = 1 é o pedido; com key = 0, a janela já não existe. */
static bool proxy_close_request(window_t *w) {
    gwin_t *g = gwin_of(w);
    if (!g) return false;
    gui_event_t e;
    memset(&e, 0, sizeof e);
    e.type = GEV_CLOSE;
    e.win = (u32)g->id;
    e.key = 1;
    push(g->ga, &e);
    return true;
}

/* ------------------------------------------------------------ app (menubar/Cmd+Q) */
static void free_menus(app_t *a) {
    for (int i = 0; i < a->nmenus; i++) {
        menu_t *m = &a->menus[i];
        for (int k = 0; k < m->count; k++) { kfree((void *)m->items[k].label); kfree((void *)m->items[k].shortcut); }
        kfree(m->items);
        kfree((void *)m->title);
    }
    kfree(a->menus);
    a->menus = NULL;
    a->nmenus = 0;
}

static void gapp_menu(app_t *a, int action) {
    for (gapp_t *ga = gapps; ga; ga = ga->next)
        if (&ga->app == a) {
            gui_event_t e;
            memset(&e, 0, sizeof e);
            e.type = GEV_MENU;
            /* ações da barra do sistema (menu com o nome do app) */
            e.key = action == 2001 ? GUI_MENU_ABOUT : action == 2004 ? GUI_MENU_PREFS : action == 2003 ? GUI_MENU_HIDE : action;
            push(ga, &e);
        }
}

static char *take_str(const u8 *b, usize n, usize *o) {
    if (*o + 4 > n) return NULL;
    u32 l;
    memcpy(&l, b + *o, 4);
    *o += 4;
    if (l > 256 || *o + l > n) return NULL;
    char *s = kmalloc(l + 1);
    if (!s) return NULL;
    memcpy(s, b + *o, l);
    s[l] = 0;
    *o += l;
    return s;
}

/* lê o formato de GUI_SET_MENU e substitui os menus do app */
static bool set_menus(gapp_t *ga, const u8 *b, usize n) {
    usize o = 0;
    u32 nm;
    if (n < 4) return false;
    memcpy(&nm, b, 4);
    o = 4;
    if (nm > 12) nm = 12;
    menu_t *menus = kcalloc(nm ? nm : 1, sizeof(menu_t));
    if (!menus) return false;
    u32 got = 0;
    for (u32 i = 0; i < nm; i++) {
        char *title = take_str(b, n, &o);
        u32 ni;
        if (!title || o + 4 > n) { kfree(title); break; }
        memcpy(&ni, b + o, 4);
        o += 4;
        if (ni > 64) ni = 64;
        menu_item_t *items = kcalloc(ni ? ni : 1, sizeof(menu_item_t));
        u32 k = 0;
        for (; items && k < ni; k++) {
            u32 act, fl;
            if (o + 8 > n) break;
            memcpy(&act, b + o, 4);
            memcpy(&fl, b + o + 4, 4);
            o += 8;
            char *label = take_str(b, n, &o), *key = take_str(b, n, &o);
            if (!label) { kfree(key); break; }
            if (!act) { kfree(label); label = NULL; }
            items[k].label = label;
            items[k].shortcut = key && key[0] ? key : (kfree(key), (char *)NULL);
            items[k].action = (int)act;
            items[k].disabled = fl & 1;
            items[k].checked = (fl & 2) != 0;
        }
        menus[got].title = title;
        menus[got].items = items;
        menus[got].count = (int)k;
        got++;
    }
    desktop_close_menus();                 /* o desktop guarda ponteiros dos textos enquanto um menu está aberto */
    free_menus(&ga->app);
    ga->app.menus = menus;
    ga->app.nmenus = (int)got;
    wm_damage(rect_make(0, 0, g_screen->w, MENUBAR_H));   /* a barra de menus mostra os novos títulos */
    return true;
}

static void gapp_quit(app_t *a) {
    for (gapp_t *ga = gapps; ga; ga = ga->next)
        if (&ga->app == a) {
            gui_event_t e;
            memset(&e, 0, sizeof e);
            e.type = GEV_QUIT;
            push(ga, &e);
        }
}

static void gapp_icon(surface_t *s, int x, int y, int size) {
    float r = size * 0.22f;
    gfx_round_rect_grad(s, x + size * 0.06f, y + size * 0.06f, size * 0.88f, size * 0.88f, r, RGB(120, 170, 255), RGB(40, 90, 220));
    font_t *f = font_get(FONT_BOLD);
    text_draw_center(s, f, size * 0.42f, rect_make(x, y, size, size), RGB(255, 255, 255), "A");
}

static gapp_t *gapp_for(process_t *p) {
    for (gapp_t *ga = gapps; ga; ga = ga->next) if (ga->pid == p->pid) return ga;
    gapp_t *ga = kcalloc(1, sizeof *ga);
    if (!ga) return NULL;
    ga->pid = p->pid;
    strncpy(ga->name, p->name, sizeof ga->name - 1);
    const char *mo = strstr(p->path, ".app/Contents/MacOS/");
    if (mo && (usize)(mo + 4 - p->path) < sizeof ga->bundle) memcpy(ga->bundle, p->path, (usize)(mo + 4 - p->path));
    ga->app.name = ga->name;
    ga->app.exec = "macapp";
    ga->app.on_quit = gapp_quit;
    ga->app.on_menu = gapp_menu;
    ga->app.draw_icon = gapp_icon;
    ga->app.running = true;
    ga->next = gapps;
    gapps = ga;
    return ga;
}

static gapp_t *gapp_find(process_t *p) {
    for (gapp_t *ga = gapps; ga; ga = ga->next) if (ga->pid == p->pid) return ga;
    return NULL;
}

static gwin_t *win_get(process_t *p, u64 id) {
    if (id >= MAX_GWIN) return NULL;
    gwin_t *g = &wins[id];
    return g->used && g->ga && g->ga->pid == p->pid ? g : NULL;
}

static void destroy(gwin_t *g) {
    if (g->w) { window_t *w = g->w; g->w = NULL; w->on_close = NULL; w->on_close_request = NULL; wm_close(w); }
    surface_free(g->back);
    surface_free(g->shown);
    kfree(g->ime);
    g->ga->nwin--;
    memset(g, 0, sizeof *g);
}

/* para a Dock/Launchpad: o app_t que representa o processo (NULL se ele não tem janelas) */
app_t *compat_gui_app_for_pid(int pid) {
    for (gapp_t *ga = gapps; ga; ga = ga->next) if (ga->pid == pid && ga->nwin > 0) return &ga->app;
    return NULL;
}

/* pid do processo dono de um app proxy (0 se não for proxy) */
int compat_gui_pid_of(app_t *a) {
    for (gapp_t *ga = gapps; ga; ga = ga->next) if (&ga->app == a) return ga->pid;
    return 0;
}

/* caminho do .app de um app_t (atalho "mac:<caminho>" de /Aplicativos ou proxy de um processo); NULL se não houver */
const char *compat_app_bundle(app_t *a) {
    if (!a || !a->exec) return NULL;
    if (!strncmp(a->exec, "mac:", 4)) return a->exec + 4;
    for (gapp_t *ga = gapps; ga; ga = ga->next) if (&ga->app == a) return ga->bundle[0] ? ga->bundle : NULL;
    return NULL;
}

/* chamado quando o processo termina (gancho de proc/process.c) */
void compat_gui_proc_exit(process_t *p) {
    gapp_t *ga = gapp_find(p);
    if (!ga) return;
    drag_forget_app(ga);
    for (int i = 0; i < MAX_GWIN; i++) if (wins[i].used && wins[i].ga == ga) destroy(&wins[i]);
    for (gapp_t **pp = &gapps; *pp; pp = &(*pp)->next)
        if (*pp == ga) { *pp = ga->next; break; }
    ga->app.running = false;
    free_menus(&ga->app);
    kfree(ga);
}

/* ------------------------------------------------------------ desenho */
static bool copy_str(process_t *p, u64 va, char *out, usize cap) {
    usize i = 0;
    for (; i + 1 < cap; i++) {
        if (!uvm_copy_in(p, &out[i], va + i, 1)) return false;
        if (!out[i]) return true;
    }
    out[i] = 0;
    return true;
}

static font_t *font_w(u32 weight) { return font_get(weight >= FONT_COUNT ? FONT_REGULAR : (int)weight); }

static void run_cmds(surface_t *s, const u8 *b, usize n) {
    usize o = 0;
    surface_reset_clip(s);
    while (o + 8 <= n) {
        u32 op, len;
        memcpy(&op, b + o, 4);
        memcpy(&len, b + o + 4, 4);
        if (len < 8 || o + len > n) break;
        const u8 *a = b + o + 8;
        usize al = len - 8;
        i32 iv[8];
        float fv[8];
        u32 uv[8];
#define NEED(k) if (al < (k)) break
        switch (op) {
        case GC_CLEAR: NEED(4); memcpy(uv, a, 4); gfx_fill(s, -s->ox, -s->oy, s->w, s->h, uv[0]); break;
        case GC_FILL: NEED(20); memcpy(iv, a, 16); memcpy(uv, a + 16, 4); gfx_fill(s, iv[0], iv[1], iv[2], iv[3], uv[0]); break;
        case GC_RRECT: NEED(24); memcpy(fv, a, 20); memcpy(uv, a + 20, 4);
            if (fv[4] > 0) gfx_round_rect(s, fv[0], fv[1], fv[2], fv[3], fv[4], uv[0]);
            else gfx_fill(s, (int)fv[0], (int)fv[1], (int)(fv[2] + 0.5f), (int)(fv[3] + 0.5f), uv[0]);
            break;
        case GC_RRECT_STROKE: NEED(28); memcpy(fv, a, 24); memcpy(uv, a + 24, 4); gfx_round_rect_stroke(s, fv[0], fv[1], fv[2], fv[3], fv[4], fv[5], uv[0]); break;
        case GC_LINE: NEED(24); memcpy(fv, a, 20); memcpy(uv, a + 20, 4); gfx_line(s, fv[0], fv[1], fv[2], fv[3], fv[4], uv[0]); break;
        case GC_POLY: {
            NEED(8);
            u32 cnt, col;
            memcpy(&cnt, a, 4); memcpy(&col, a + 4, 4);
            if (cnt < 3 || cnt > 4096 || al < 8 + cnt * 8) break;
            float *xy = kmalloc(cnt * 8);
            if (!xy) break;
            memcpy(xy, a + 8, cnt * 8);
            gfx_polygon(s, xy, (int)cnt, col);
            kfree(xy);
            break;
        }
        case GC_POLYLINE: {
            NEED(12);
            u32 cnt, col;
            float t;
            memcpy(&cnt, a, 4); memcpy(&col, a + 4, 4); memcpy(&t, a + 8, 4);
            if (cnt < 2 || cnt > 4096 || al < 12 + cnt * 8) break;
            for (u32 i = 0; i + 1 < cnt; i++) {
                float p[4];
                memcpy(p, a + 12 + i * 8, 16);
                gfx_line(s, p[0], p[1], p[2], p[3], t, col);
            }
            break;
        }
        case GC_TEXT: {
            NEED(24);
            memcpy(fv, a, 12);
            memcpy(uv, a + 12, 12);
            u32 bytes = uv[2];
            if (al < 24 + bytes) break;
            text_draw_n(s, font_w(uv[0]), fv[2], (int)fv[0], (int)fv[1], uv[1], (const char *)a + 24, (int)bytes);
            break;
        }
        case GC_CLIP: NEED(16); memcpy(iv, a, 16); surface_clip(s, rect_make(iv[0], iv[1], iv[2], iv[3])); break;
        case GC_UNCLIP: surface_reset_clip(s); break;
        case GC_GRAD: NEED(24); memcpy(iv, a, 16); memcpy(uv, a + 16, 8); gfx_grad_v(s, iv[0], iv[1], iv[2], iv[3], uv[0], uv[1]); break;
        case GC_IMAGE: {
            NEED(16);
            memcpy(iv, a, 16);
            if (iv[2] <= 0 || iv[3] <= 0 || iv[2] > 4096 || iv[3] > 4096 || al < 16 + (usize)iv[2] * iv[3] * 4) break;
            surface_t img = {iv[2], iv[3], iv[2], (u32 *)(a + 16), {0, 0, iv[2], iv[3]}, 0, 0};
            /* pixels ARGB não pré-multiplicados: converte linha a linha */
            for (int k = 0; k < iv[2] * iv[3]; k++) img.px[k] = color_premul(img.px[k]);
            gfx_blend(s, &img, iv[0], iv[1], 255);
            break;
        }
        }
#undef NEED
        o += len;
    }
    surface_reset_clip(s);
}

#include "guisys_dragime.inc"
static bool drag_is_source(gapp_t *ga) { return drag.active && (!ga || drag.src == ga); }

/* ------------------------------------------------------------ syscalls */
i64 compat_gui_syscall(process_t *p, u64 n, u64 a, u64 b, u64 c, u64 d, u64 e) {
    (void)e;
    switch (n) {
    case GUI_WIN_CREATE: {
        char title[96];
        if (!copy_str(p, a, title, sizeof title)) return -MOS_EFAULT;
        int w = (int)CLAMP((i64)b, 120, 3000), h = (int)CLAMP((i64)c, 60, 2000);
        gapp_t *ga = gapp_for(p);
        if (!ga) return -MOS_ENOMEM;
        int id = -1;
        for (int i = 1; i < MAX_GWIN; i++) if (!wins[i].used) { id = i; break; }
        if (id < 0) return -MOS_EMFILE;
        gwin_t *g = &wins[id];
        memset(g, 0, sizeof *g);
        g->used = true; g->id = id; g->ga = ga;
        u32 fl = WIN_CENTER;
        if (d & GUI_WF_RESIZABLE) fl |= WIN_RESIZABLE;
        if (d & GUI_WF_NO_TITLE) fl |= WIN_NO_TITLE;
        if (d & GUI_WF_FULLSIZE) fl |= WIN_FULLSIZE;
        g->back = surface_new(w, h);
        g->shown = surface_new(w, h);
        if (!g->back || !g->shown) { surface_free(g->back); surface_free(g->shown); memset(g, 0, sizeof *g); return -MOS_ENOMEM; }
        gfx_clear(g->back, T.window_bg);
        gfx_clear(g->shown, T.window_bg);
        g->w = wm_create(&ga->app, title, 0, 0, w, h + TITLEBAR_H, fl);
        if (!g->w) { surface_free(g->back); surface_free(g->shown); memset(g, 0, sizeof *g); return -MOS_ENOMEM; }
        /* a barra de título do tema pode ter outra altura: ajusta para o conteúdo ficar exato */
        rect_t cr = wm_content_rect(g->w);
        if (cr.h != h || cr.w != w) { g->w->h += h - cr.h; g->w->w += w - cr.w; }
        g->w->paint = proxy_paint;
        g->w->on_close = proxy_close;
        g->w->on_close_request = proxy_close_request;
        g->w->on_key = proxy_key;
        g->w->min_w = 160; g->w->min_h = 90;
        ga->nwin++;
        ga->app.windows = ga->nwin;
        wm_invalidate(g->w);
        return id;
    }
    case GUI_WIN_DESTROY: {
        gwin_t *g = win_get(p, a);
        if (!g) return -MOS_EBADF;
        destroy(g);
        return 0;
    }
    case GUI_WIN_TITLE: {
        gwin_t *g = win_get(p, a);
        char t[96];
        if (!g || !copy_str(p, b, t, sizeof t)) return -MOS_EBADF;
        if (g->w) wm_set_title(g->w, t);
        return 0;
    }
    case GUI_DRAW: {
        gwin_t *g = win_get(p, a);
        if (!g) return -MOS_EBADF;
        if (c > GUI_MAX_BATCH) return -MOS_EINVAL;
        u8 *buf = kmalloc(c ? c : 1);
        if (!buf) return -MOS_ENOMEM;
        if (!uvm_copy_in(p, buf, b, c)) { kfree(buf); return -MOS_EFAULT; }
        run_cmds(g->back, buf, c);
        kfree(buf);
        return 0;
    }
    case GUI_PRESENT: {
        gwin_t *g = win_get(p, a);
        if (!g) return -MOS_EBADF;
        if (g->shown->w != g->back->w || g->shown->h != g->back->h) resize_surface(&g->shown, g->back->w, g->back->h, T.window_bg);
        gfx_blit(g->shown, g->back, 0, 0);
        if (g->w) wm_invalidate(g->w);
        return 0;
    }
    case GUI_POLL: {
        drag_track();                              /* o cursor pode ter saído de todas as janelas do app */
        gapp_t *ga = gapp_find(p);
        if (!ga || ga->qh == ga->qt) return 0;
        if (!uvm_copy_out(p, a, &ga->q[ga->qh], sizeof(gui_event_t))) return -MOS_EFAULT;
        ga->qh = (ga->qh + 1) % QLEN;
        return 1;
    }
    case GUI_MEASURE: {
        if (b > 4096) return -MOS_EINVAL;
        char *t = kmalloc(b + 1);
        if (!t) return -MOS_ENOMEM;
        if (!uvm_copy_in(p, t, a, b)) { kfree(t); return -MOS_EFAULT; }
        t[b] = 0;
        float size = (float)(d & 0xFFFF) / 100.0f;
        font_t *f = font_w((u32)(d >> 16));
        gui_metrics_t m = {text_width_n(f, size, t, (int)b), text_ascent(f, size), text_descent(f, size)};
        kfree(t);
        return uvm_copy_out(p, c, &m, sizeof m) ? 0 : -MOS_EFAULT;
    }
    case GUI_WIN_INFO: {
        gwin_t *g = win_get(p, a);
        if (!g) return -MOS_EBADF;
        gui_wininfo_t i = {g->back->w, g->back->h, g->w && wm_focused() == g->w, g->w != NULL};
        return uvm_copy_out(p, b, &i, sizeof i) ? 0 : -MOS_EFAULT;
    }
    case GUI_SCREEN: {
        /* visible = altura que o Dock reserva embaixo (o NSScreen.visibleFrame dos apps tira isso) */
        int dock = g_screen->h - desktop_dock_top();
        gui_wininfo_t i = {g_screen->w, g_screen->h - MENUBAR_H, 1, dock > 1 ? dock : 2};
        return uvm_copy_out(p, a, &i, sizeof i) ? 0 : -MOS_EFAULT;
    }
    case GUI_SET_MENU: {
        gapp_t *ga = gapp_for(p);
        if (!ga || b > 65536) return -MOS_EINVAL;
        u8 *buf = kmalloc(b ? b : 1);
        if (!buf) return -MOS_ENOMEM;
        bool ok = uvm_copy_in(p, buf, a, b) && set_menus(ga, buf, b);
        kfree(buf);
        return ok ? 0 : -MOS_EINVAL;
    }
    case GUI_DECODE_IMAGE: {
        /* a decodificação usa o mesmo motor de imagens do navegador do mOS */
        extern surface_t *image_decode(const u8 *data, usize len) __attribute__((weak));
        if (!image_decode || b > (32u << 20)) return -MOS_EINVAL;
        u8 *buf = kmalloc(b ? b : 1);
        if (!buf) return -MOS_ENOMEM;
        if (!uvm_copy_in(p, buf, a, b)) { kfree(buf); return -MOS_EFAULT; }
        surface_t *img = image_decode(buf, b);
        kfree(buf);
        if (!img) return 0;
        u64 need = 8 + (u64)img->w * img->h * 4;
        i64 r = (i64)need;
        if (d >= need) {
            u32 hdr[2] = {(u32)img->w, (u32)img->h};
            bool ok = uvm_copy_out(p, c, hdr, 8);
            /* superfícies guardam pré-multiplicado: desfaz para ARGB comum */
            u32 *row = kmalloc((usize)img->w * 4);
            for (int y = 0; ok && row && y < img->h; y++) {
                for (int x = 0; x < img->w; x++) {
                    u32 px = img->px[y * img->stride + x], al = px >> 24;
                    if (al && al < 255) {
                        u32 r_ = ((px >> 16) & 255) * 255 / al, g_ = ((px >> 8) & 255) * 255 / al, b_ = (px & 255) * 255 / al;
                        px = al << 24 | MIN(r_, 255u) << 16 | MIN(g_, 255u) << 8 | MIN(b_, 255u);
                    }
                    row[x] = px;
                }
                ok = uvm_copy_out(p, c + 8 + (u64)y * img->w * 4, row, (usize)img->w * 4);
            }
            kfree(row);
            if (!ok) r = -MOS_EFAULT;
        }
        surface_free(img);
        return r;
    }
    case GUI_FONT: {
        int op = (int)a, weight = (int)b;
        if (op == GF_TABLE) {
            int len = font_table(weight, (u32)c, NULL, 0);
            if (len <= 0) return 0;
            if (!d || !e) return len;
            u32 cap = (u32)e;
            if (cap > (8u << 20)) cap = 8u << 20;
            u8 *tmp = kmalloc((u32)len);
            if (!tmp) return -MOS_ENOMEM;
            font_table(weight, (u32)c, tmp, (u32)len);
            u32 n = (u32)len < cap ? (u32)len : cap;
            bool ok = uvm_copy_out(p, d, tmp, n);
            kfree(tmp);
            return ok ? len : -MOS_EFAULT;
        }
        if (op == GF_CMAP) return font_glyph_id(weight, (u32)c);
        if (op == GF_ADVANCE) {
            i32 upem = 0;
            font_face_metrics(weight, &upem, NULL, NULL, NULL);
            return (i64)(u16)font_advance_units(weight, (u16)c) | ((i64)(u16)upem << 16);
        }
        if (op == GF_METRICS) {
            gui_font_metrics_t m;
            font_face_metrics(weight, &m.upem, &m.ascender, &m.descender, &m.nglyphs);
            return uvm_copy_out(p, d, &m, sizeof m) ? 0 : -MOS_EFAULT;
        }
        if (op == GF_RASTER) {
            gui_glyph_req_t req;
            if (!uvm_copy_in(p, &req, d, sizeof req)) return -MOS_EFAULT;
            if (req.cap > (1u << 20)) req.cap = 1u << 20;
            u8 *tmp = req.cap ? kmalloc(req.cap) : NULL;
            if (req.cap && !tmp) return -MOS_ENOMEM;
            i16 w = 0, h = 0, left = 0, top = 0;
            int n = font_raster_glyph((int)req.weight, (u16)req.glyph, req.size, tmp, req.cap, &w, &h, &left, &top);
            if (n < 0) { kfree(tmp); return -MOS_ENOMEM; }
            req.w = w; req.h = h; req.left = left; req.top = top;
            bool ok = uvm_copy_out(p, d, &req, sizeof req);
            if (ok && tmp && req.buf && n > 0) {
                u32 copy = (u32)n < req.cap ? (u32)n : req.cap;
                ok = uvm_copy_out(p, req.buf, tmp, copy);
            }
            kfree(tmp);
            return ok ? n : -MOS_EFAULT;
        }
        if (op == GF_REGISTER) {
            if (!d || c < 12 || c > (8u << 20)) return -MOS_EINVAL;
            u8 *buf = kmalloc((u32)c);
            if (!buf) return -MOS_ENOMEM;
            if (!uvm_copy_in(p, buf, d, (usize)c)) { kfree(buf); return -MOS_EFAULT; }
            bool kept = false;
            int id = font_register(buf, (u32)c, &kept);
            if (!kept) kfree(buf);
            return id >= 0 ? id : -MOS_EINVAL;
        }
        return -MOS_EINVAL;
    }
    case GUI_DRAG: return drag_syscall(p, a, b, c, d, e);
    case GUI_IME: return ime_syscall(p, a, b, c, d, e);
    case GUI_DEVICE: {
        extern i64 gamepad_syscall(process_t *p, u64 op, u64 b, u64 c, u64 d);
        extern i64 bt_user_syscall(process_t *p, u64 op, u64 b, u64 c, u64 d);
        return a < DEV_BT_STATE ? gamepad_syscall(p, a, b, c, d) : bt_user_syscall(p, a, b, c, d);
    }
    case GUI_WIN_FOCUS: {
        gwin_t *g = win_get(p, a);
        if (!g) return -MOS_EBADF;
        if (b == GUI_FOCUS_MOVE) return g->w && wm_begin_move(g->w) ? 0 : -MOS_EAGAIN;
        if (b == GUI_FOCUS_HIDE || b == GUI_FOCUS_SHOW) { if (g->w) wm_set_hidden(g->w, b == GUI_FOCUS_HIDE); return 0; }
        if (g->w) wm_focus(g->w);
        return 0;
    }
    }
    return -MOS_EINVAL;
}
