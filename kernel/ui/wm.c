/* mOS — window server: composição, cromo das janelas, arrasto/redimensionamento e roteamento de eventos */
#include "ui.h"

#define MAX_WIN 48
static window_t *wins[MAX_WIN]; /* índice 0 = fundo, n-1 = topo */
static int nwins, next_id = 1;
static int mx = 200, my = 200, mbuttons;
static int cursor_kind = CURSOR_ARROW;

static rect_t dirty[32];
static int ndirty;

enum { DRAG_NONE, DRAG_MOVE, DRAG_RESIZE, DRAG_CONTENT, DRAG_TRAFFIC };
static int drag_mode, drag_dx, drag_dy, drag_edges, traffic_btn;
static window_t *drag_win;
static rect_t drag_start;
static u64 last_click_t;
static int last_click_x, last_click_y;
static bool traffic_hover;
static struct window *green_w;   /* mouse parado no botão verde: abre o menu de organizar */
static u64 green_t;
static bool green_fired;

#define SHADOW_M 58

/* faixa da barra de menus (no monitor principal, que pode estar em qualquer lugar do arranjo) */
static rect_t menubar_rect(void) { rect_t p = display_rect(0); return rect_make(p.x, p.y, p.w, MENUBAR_H); }

/* contorno vermelho no monitor que está sendo arrastado no organizador (Ajustes > Monitores) */
static void draw_identify(surface_t *s) {
    rect_t r;
    if (display_identifying() < 0 || !display_scanout(display_identifying(), &r)) return;
    for (int k = 0; k < 6; k++) gfx_rect(s, r.x + k, r.y + k, r.w - 2 * k, r.h - 2 * k, RGB(235, 50, 45));
}

int wm_mouse_x(void) { return mx; }
int wm_mouse_y(void) { return my; }
int wm_mouse_buttons(void) { return mbuttons; }
window_t *wm_window_list(void) { return nwins ? wins[0] : NULL; }

/* ============================================================ dano */
void wm_damage(rect_t r) {
    r = rect_intersect(r, rect_make(0, 0, g_screen->w, g_screen->h));
    if (rect_empty(r)) return;
    for (int i = 0; i < ndirty; i++) {
        rect_t u = rect_union(dirty[i], r);
        /* funde se a união não desperdiça muita área */
        if ((i64)u.w * u.h <= (i64)dirty[i].w * dirty[i].h + (i64)r.w * r.h + 4096) { dirty[i] = u; return; }
    }
    if (ndirty < (int)ARRAY_LEN(dirty)) { dirty[ndirty++] = r; return; }
    for (int i = 1; i < ndirty; i++) dirty[0] = rect_union(dirty[0], dirty[i]);
    dirty[0] = rect_union(dirty[0], r);
    ndirty = 1;
}
void wm_damage_all(void) { wm_damage(rect_make(0, 0, g_screen->w, g_screen->h)); }

static rect_t win_bounds(window_t *w) { return rect_make(w->x - SHADOW_M, w->y - SHADOW_M + 10, w->w + 2 * SHADOW_M, w->h + 2 * SHADOW_M); }
/* Janela "unificada" (padrão atual): sem linha de título separada. O cabeçalho tem 52 px e tudo nele
 * (semáforos, título, controles em cápsula de 36 px) é centralizado verticalmente em y = 26.
 * O conteúdo começa UNIFIED_PAD (8) px abaixo do topo; os apps usam toolbar_h = TOOLBAR_H (44) e desenham
 * os controles centralizados nos primeiros 36 px (TOOLBAR_Y(h)), então o corpo começa em 8 + 44 = 52. */
#define UNIFIED_PAD 8
#define HEADER_CY (UNIFIED_PAD + 18)
static bool unified(window_t *w) { return w->toolbar_h > 0 && !(w->flags & WIN_NO_TITLE); }
/* abas: faixa de TAB_H no topo (janela unificada: a faixa leva os semáforos, como no layout compacto)
 * ou logo abaixo da linha de título (janela clássica) */
#define TAB_H 34
static int tab_h(window_t *w) { return w->tab_group && !(w->flags & WIN_NO_TITLE) ? TAB_H : 0; }
/* WIN_FULLSIZE: sem faixa de título, o conteúdo começa no topo (os semáforos são desenhados por cima) */
static int title_h(window_t *w) { return (w->flags & (WIN_NO_TITLE | WIN_FULLSIZE)) ? 0 : (unified(w) ? UNIFIED_PAD : TITLEBAR_H) + tab_h(w); }
static int tab_strip_y(window_t *w) { return unified(w) ? 0 : TITLEBAR_H; }

static void traffic_center(window_t *w, int i, float *cx, float *cy) {
    if (unified(w)) {
        *cy = w->tab_group ? TAB_H / 2.0f : HEADER_CY;
        *cx = 26 + i * 23;
    } else {
        *cy = TITLEBAR_H / 2.0f;
        *cx = 18 + i * 20;
    }
}

/* botão de mostrar/ocultar a barra lateral (janelas unificadas que têm barra lateral) */
static bool sidebar_panel(window_t *w, rect_t *r);
static bool toggle_center(window_t *w, float *cx, float *cy) {
    if (!unified(w) || (!w->sidebar_w && !w->sidebar_saved)) return false;
    *cy = HEADER_CY + tab_h(w);
    rect_t sp;
    if (w->sidebar_w && sidebar_panel(w, &sp)) *cx = sp.x + sp.w - 26;   /* canto superior direito da barra */
    else *cx = w->tab_group ? 30 : 26 + 2 * 23 + 32;                    /* recolhida: depois dos semáforos */
    return true;
}

static void sidebar_toggle(window_t *w) {
    wm_damage(rect_make(w->x - 60, w->y - 60, w->w + 120, w->h + 120));
    if (w->sidebar_w) { w->sidebar_saved = w->sidebar_w; w->sidebar_w = 0; }
    else { w->sidebar_w = w->sidebar_saved; w->sidebar_saved = 0; }
    w->needs_paint = true;
}

static void draw_sidebar_toggle(surface_t *s, window_t *w) {
    float cx, cy;
    if (!toggle_center(w, &cx, &cy)) return;
    float r = 17;
    gfx_shadow(s, (int)(cx - r), (int)(cy - r), (int)(2 * r), (int)(2 * r), r, 6, T.dark ? 90 : 34, 1);
    gfx_circle(s, cx, cy, r + 0.5f, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 22));
    gfx_circle(s, cx, cy, r, T.dark ? RGB(44, 44, 48) : RGB(255, 255, 255));
    gfx_specular_rim(s, cx - r, cy - r, 2 * r, 2 * r, r, T.dark ? 50 : 110);
    glyph_sidebar_toggle(s, (int)(cx - 8), (int)(cy - 8), 16, T.dark ? RGB(230, 230, 234) : RGB(40, 40, 44));
}

static int traffic_at(window_t *w, int lx, int ly) {
    if (w->flags & WIN_NO_TITLE) return -1;
    for (int i = 0; i < 3; i++) {
        float cx, cy;
        traffic_center(w, i, &cx, &cy);
        if (lx >= cx - 9 && lx <= cx + 9 && ly >= cy - 10 && ly <= cy + 10) return i;
    }
    return -1;
}

rect_t wm_content_rect(window_t *w) { return rect_make(w->x, w->y + title_h(w), w->w, w->h - title_h(w)); }

/* ============================================================ z-order e foco */
static int index_of(window_t *w) { for (int i = 0; i < nwins; i++) if (wins[i] == w) return i; return -1; }

/* ---------------- Spaces: a mesa (space_win = NULL) e um Space por janela em tela cheia */
static window_t *space_win;
static window_t *spaces[8];          /* ordem dos Spaces de tela cheia (à direita da mesa) */
static int nspaces;
static bool in_space(window_t *w) { return space_win ? w == space_win : !w->fullscreen; }
static void space_switch(window_t *to, int dir);
static void tab_activate(window_t *w);
static int tab_members(window_t *w, window_t **out, int max);
static void tab_frame_from(window_t *dst, window_t *src);
static void tab_repaint_group(window_t *w);
static void tab_leave(window_t *w);
static window_t *tab_pending_src;   /* "+" na faixa de abas: a próxima janela do app entra no grupo */

/* Stage Manager: só as janelas do app em foco ficam no palco; os outros apps viram miniaturas à esquerda */
bool g_stage_manager;
static app_t *stage_app;
static bool on_stage(window_t *w) { return !g_stage_manager || space_win || !stage_app || w->app == stage_app; }

window_t *wm_focused(void) {
    for (int i = nwins - 1; i >= 0; i--) {
        window_t *w = wins[i];
        if (!w->minimized && !w->closing && w->visible && in_space(w)) return w;
    }
    return NULL;
}

app_t *wm_active_app(void) { window_t *w = wm_focused(); return w ? w->app : NULL; }

void wm_invalidate(window_t *w) { if (w) w->needs_paint = true; }

/* -[NSWindow performWindowDragWithEvent:]: o clique que foi para o conteúdo (o app viu que caiu numa
 * região de arrastar, como a barra de título que o VS Code desenha) passa a mover a janela até soltar */
bool wm_begin_move(window_t *w) {
    if (!w || !(mbuttons & 1) || drag_win != w || drag_mode != DRAG_CONTENT || w->fullscreen) return false;
    drag_mode = DRAG_MOVE;
    drag_dx = mx - w->x; drag_dy = my - w->y;
    drag_start = rect_make(mx, my, 0, 0);
    return true;
}

/* -[NSWindow orderOut:] / orderFront: de um app do macOS: a janela some da tela e do foco (wm_focused só olha as
 * visíveis) sem ser destruída, e volta na frente. Janelas em grupo de abas ficam por conta das abas. */
void wm_set_hidden(window_t *w, bool hidden) {
    if (!w || w->tab_group || w->closing) return;
    if (w->visible == !hidden) { if (!hidden) wm_focus(w); return; }
    w->visible = !hidden;
    w->needs_paint = true;
    wm_damage(win_bounds(w));
    if (!hidden) wm_focus(w);
}

/* Janelas filhas (owner, de -[NSWindow addChildWindow:ordered:] pelo GUI_FOCUS_OWNER) ficam acima da dona, como no Cocoa:
 * o diálogo "Abrir o Slack?" do Chrome é outra NSWindow, e um clique na janela do navegador a trazia para cima dele,
 * escondendo-o. Sobe as filhas visíveis na ordem em que estavam, e as filhas delas. */
static void raise_owned(window_t *w, int depth) {
    window_t *list[MAX_WIN];
    int n = 0;
    for (int i = 0; i < nwins; i++) {
        window_t *c = wins[i];
        if (c != w && c->owner == w && c->visible && !c->minimized && !c->closing) list[n++] = c;
    }
    for (int j = 0; j < n; j++) {
        int k = index_of(list[j]);
        if (k < 0) continue;
        for (; k < nwins - 1; k++) wins[k] = wins[k + 1];
        wins[nwins - 1] = list[j];
        list[j]->needs_paint = true;
        wm_damage(win_bounds(list[j]));
        if (depth < 4) raise_owned(list[j], depth + 1);
    }
}

void wm_focus(window_t *w) {
    int i = index_of(w);
    if (i < 0) return;
    if (w->tab_hidden) { tab_activate(w); return; }                 /* aba escondida: vira a da frente */
    if (!in_space(w)) space_switch(w->fullscreen ? w : NULL, w->fullscreen ? 1 : -1);
    if (g_stage_manager && w->app != stage_app) { stage_app = w->app; wm_damage_all(); }
    i = index_of(w);
    window_t *old = wm_focused();
    if (old != w) {
        for (int k = i; k < nwins - 1; k++) wins[k] = wins[k + 1];
        wins[nwins - 1] = w;
        if (old) { old->needs_paint = true; old->ui.focus = 0; }
        w->needs_paint = true;
        wm_damage(win_bounds(w));
        if (old) wm_damage(win_bounds(old));
        wm_damage(menubar_rect()); /* nome do app no menu */
    }
    raise_owned(w, 0);
}

int wm_count_for(app_t *a) {
    int n = 0;
    for (int i = 0; i < nwins; i++) if (wins[i]->app == a && !wins[i]->closing) n++;
    return n;
}

void wm_bring_app(app_t *a) {
    window_t *top = NULL;
    for (int i = 0; i < nwins; i++) if (wins[i]->app == a && !wins[i]->closing) {
        if (wins[i]->minimized) { wins[i]->minimized = false; wins[i]->anim = 0; wins[i]->anim_dir = 1; }
        top = wins[i];
    }
    /* traz todas as janelas do app, preservando a ordem relativa */
    for (int i = 0; i < nwins; i++) if (wins[i]->app == a && wins[i] != top) {}
    if (top) wm_focus(top);
    wm_damage_all();
}

/* ============================================================ criação / destruição */
window_t *wm_create(app_t *app, const char *title, int x, int y, int w, int h, u32 flags) {
    if (nwins >= MAX_WIN) return NULL;
    window_t *win = kcalloc(1, sizeof(window_t));
    win->id = next_id++;
    win->app = app;
    strncpy(win->title, title ? title : "", sizeof(win->title) - 1);
    rect_t pd = display_rect(0);                     /* janelas novas abrem no monitor principal */
    int sw = pd.w, sh = pd.h;
    h += (flags & WIN_NO_TITLE) ? 0 : TITLEBAR_H;
    w = MIN(w, sw - 20);
    h = MIN(h, sh - MENUBAR_H - 20);
    if (x < 0 || (flags & WIN_CENTER)) {
        static int cascade;
        x = pd.x + (sw - w) / 2 + ((flags & WIN_CENTER) ? 0 : (cascade % 6) * 24 - 60);
        y = pd.y + MENUBAR_H + (sh - MENUBAR_H - 90 - h) / 2 + ((flags & WIN_CENTER) ? 0 : (cascade % 6) * 22 - 40);
        cascade++;
    }
    else { x += pd.x; y += pd.y; }                   /* posição pedida pelo app: relativa ao principal */
    y = MAX(y, pd.y + MENUBAR_H + 4);
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->min_w = 260; win->min_h = 140;
    win->flags = flags;
    win->surf = surface_new(w, h);
    win->needs_paint = true;
    win->visible = true;
    win->anim = 0; win->anim_dir = 1;
    win->ui.sel_anchor = -1;
    wins[nwins++] = win;
    if (app) { app->running = true; app->windows++; }
    if (tab_pending_src && app == tab_pending_src->app && index_of(tab_pending_src) >= 0) {
        /* nova aba: mesmo quadro da aba atual, que fica escondida atrás */
        window_t *src = tab_pending_src;
        if (!src->tab_group) src->tab_group = src->id;
        win->tab_group = src->tab_group;
        tab_frame_from(win, src);
        surface_resize(win->surf, win->w, win->h);
        win->anim = 1;
        window_t *m[16];
        int n = tab_members(win, m, 16);
        for (int k = 0; k < n; k++) if (m[k] != win) { m[k]->tab_hidden = true; m[k]->visible = false; }
        tab_repaint_group(win);
        tab_pending_src = NULL;
    }
    if (space_win && !win->fullscreen) space_switch(NULL, -1);       /* janela nova aparece na mesa */
    window_t *old = nwins > 1 ? wins[nwins - 2] : NULL;
    if (old) old->needs_paint = true;
    wm_damage(win_bounds(win));
    wm_damage(menubar_rect());
    return win;
}

static void destroy(window_t *w) {
    int i = index_of(w);
    if (i < 0) return;
    wm_damage(win_bounds(w));
    for (int k = i; k < nwins - 1; k++) wins[k] = wins[k + 1];
    nwins--;
    for (int k = 0; k < nwins; k++) if (wins[k]->owner == w) wins[k]->owner = NULL;   /* filhas perdem a dona */
    if (drag_win == w) { drag_win = NULL; drag_mode = DRAG_NONE; }
    if (w->app) w->app->windows--;
    for (int k = 0; k < nspaces; k++) if (spaces[k] == w) { memmove(spaces + k, spaces + k + 1, (nspaces - k - 1) * sizeof(window_t *)); nspaces--; k--; }
    if (space_win == w) { space_win = NULL; wm_damage_all(); }
    if (tab_pending_src == w) tab_pending_src = NULL;
    surface_free(w->surf);
    kfree(w);
    window_t *f = wm_focused();
    if (f) f->needs_paint = true;
    wm_damage(menubar_rect());
}

void wm_close(window_t *w) {
    if (!w || w->closing) return;
    if (w->on_close_request && w->on_close_request(w)) return;
    if (w->on_close) w->on_close(w);
    if (w->tab_group) tab_leave(w);
    if (w->fullscreen && space_win == w) space_switch(NULL, -1);   /* fechar em tela cheia volta para a mesa */
    w->closing = true;
    w->anim_dir = -1;
    if (w->anim > 1) w->anim = 1;
}

void wm_close_app(app_t *a) {
    for (int i = nwins - 1; i >= 0; i--) if (wins[i]->app == a) { wins[i]->on_close_request = NULL; wm_close(wins[i]); }
}

void wm_set_title(window_t *w, const char *t) {
    if (strcmp(w->title, t)) { strncpy(w->title, t, sizeof(w->title) - 1); w->needs_paint = true; }
}

void wm_minimize(window_t *w) {
    if (!w || (w->flags & WIN_NO_MINIMIZE)) return;
    w->anim_dir = 2;
    w->anim = 1;
    rect_t dr = display_rect(0);
    wm_damage(rect_make(0, dr.h - 220, dr.w, 220));          /* abre espaço para a miniatura no Dock */
}

/* ============================================================ organização de janelas (encaixe nas bordas) */
enum { TILE_NONE, TILE_LEFT, TILE_RIGHT, TILE_FILL, TILE_TOP, TILE_BOTTOM, TILE_TL, TILE_TR, TILE_BL, TILE_BR, TILE_CENTER };
static int snap_zone;

static int tile_disp;                                /* monitor das áreas de encaixe */

/* área útil de um monitor: o principal tem a barra de menus em cima e o Dock embaixo */
static rect_t work_area(int i) {
    rect_t d = display_rect(i);
    if (i == 0) return rect_make(d.x, d.y + MENUBAR_H, d.w, desktop_dock_top() - d.y - MENUBAR_H);
    return d;
}

static rect_t tile_rect(int zone) {
    rect_t a = work_area(tile_disp);
    int sw = a.w, top = a.y + 6, bottom = a.y + a.h - 4;
    int h = bottom - top, half = (sw - 18) / 2, hh = (h - 6) / 2;
    int xl = a.x + 6, xr = a.x + sw - 6 - half, yb = bottom - hh;
    switch (zone) {
    case TILE_LEFT: return rect_make(xl, top, half, h);
    case TILE_RIGHT: return rect_make(xr, top, half, h);
    case TILE_TOP: return rect_make(xl, top, sw - 12, hh);
    case TILE_BOTTOM: return rect_make(xl, yb, sw - 12, hh);
    case TILE_TL: return rect_make(xl, top, half, hh);
    case TILE_TR: return rect_make(xr, top, half, hh);
    case TILE_BL: return rect_make(xl, yb, half, hh);
    case TILE_BR: return rect_make(xr, yb, half, hh);
    default: return rect_make(xl, top, sw - 12, h);
    }
}

void wm_tile(window_t *w, int zone) {
    if (!w || !(w->flags & WIN_RESIZABLE) || zone == TILE_NONE) return;
    wm_damage(win_bounds(w));
    if (!w->zoomed) w->restore = rect_make(w->x, w->y, w->w, w->h);
    if (drag_mode != DRAG_MOVE) tile_disp = display_for(rect_make(w->x, w->y, w->w, w->h));  /* arrastando: o do mouse */
    rect_t r = tile_rect(zone);
    if (zone == TILE_CENTER) {
        /* centralizar: mantém o tamanho (o anterior, se estava encaixada) e põe no meio da área útil */
        rect_t a = work_area(tile_disp);
        int cw = w->zoomed ? w->restore.w : w->w, ch = w->zoomed ? w->restore.h : w->h;
        cw = MIN(cw, a.w - 12); ch = MIN(ch, a.h - 10);
        r = rect_make(a.x + (a.w - cw) / 2, a.y + (a.h - ch) / 2, cw, ch);
        w->x = r.x; w->y = r.y; w->w = MAX(w->min_w, r.w); w->h = MAX(w->min_h, r.h);
        w->zoomed = false;
        surface_resize(w->surf, w->w, w->h);
        w->needs_paint = true;
        wm_damage(win_bounds(w));
        return;
    }
    w->x = r.x; w->y = r.y; w->w = MAX(w->min_w, r.w); w->h = MAX(w->min_h, r.h);
    w->zoomed = true; /* "zoom" de novo (ou arrastar) devolve o tamanho anterior */
    surface_resize(w->surf, w->w, w->h);
    w->needs_paint = true;
    wm_damage(win_bounds(w));
}

/* prévia translúcida da área de encaixe enquanto a janela é arrastada até a borda */
static void draw_snap_preview(surface_t *s) {
    if (drag_mode != DRAG_MOVE || snap_zone == TILE_NONE) return;
    rect_t r = tile_rect(snap_zone);
    gfx_round_rect(s, r.x, r.y, r.w, r.h, WIN_RADIUS + 2, T.dark ? RGBA(255, 255, 255, 34) : RGBA(255, 255, 255, 90));
    gfx_round_rect_stroke(s, r.x, r.y, r.w, r.h, WIN_RADIUS + 2, 1.5f, T.dark ? RGBA(255, 255, 255, 90) : RGBA(255, 255, 255, 200));
    gfx_specular_rim(s, r.x, r.y, r.w, r.h, WIN_RADIUS + 2, 120);
}

/* ============================================================ Mission Control */
static bool mc_open, mc_closing;
static float mc_t;
static int mc_hover = -1;

static int mc_list(window_t **out, int max) {
    int n = 0;
    for (int i = 0; i < nwins && n < max; i++) {
        window_t *w = wins[i];
        if (w->visible && !w->minimized && !w->closing && in_space(w)) out[n++] = w;
    }
    return n;
}

/* grade que preserva a proporção das janelas, centralizada entre a barra de menus e o Dock */
static rect_t mc_target(window_t *w, int i, int n) {
    rect_t pd = display_rect(0);
    int sw = pd.w, top = pd.y + MENUBAR_H + 70, bottom = desktop_dock_top() - 30;
    int cols = 1;
    while (cols * cols < n) cols++;
    int rows = (n + cols - 1) / cols;
    int aw = sw - 120, ah = bottom - top;
    int cw = aw / cols, ch = ah / MAX(1, rows);
    int col = i % cols, row = i / cols;
    float k = MIN((cw - 40) / (float)w->w, (ch - 50) / (float)w->h);
    if (k > 0.8f) k = 0.8f;
    int tw = (int)(w->w * k), th = (int)(w->h * k);
    /* última linha incompleta fica centralizada */
    int in_row = row == rows - 1 ? n - row * cols : cols;
    int x0 = pd.x + 60 + (aw - in_row * cw) / 2;
    return rect_make(x0 + col * cw + (cw - tw) / 2, top + row * ch + (ch - 30 - th) / 2, tw, th);
}

void wm_toggle_mission_control(void) {
    if (mc_open && !mc_closing) mc_closing = true;
    else { mc_open = true; mc_closing = false; mc_t = 0; mc_hover = -1; }
    wm_damage_all();
}

static void mc_step(void) {
    if (!mc_open) return;
    float before = mc_t;
    if (mc_closing) { mc_t -= 0.2f; if (mc_t <= 0) { mc_t = 0; mc_open = false; mc_closing = false; } }
    else if (mc_t < 1) { mc_t += 0.2f; if (mc_t > 1) mc_t = 1; }
    if (mc_t != before || !mc_open) wm_damage_all(); /* inclui o quadro final (t = 1) e o de saída */
}

static void mc_draw(surface_t *s) {
    float k = mc_t;
    k = k * k * (3 - 2 * k);
    surface_t *bl = desktop_wallpaper_blur();
    if (bl) gfx_blend(s, bl, 0, 0, (int)(255 * k));
    gfx_fill(s, 0, 0, g_screen->w, g_screen->h, RGBA(0, 0, 0, (int)((T.dark ? 70 : 40) * k)));
    window_t *list[MAX_WIN];
    int n = mc_list(list, MAX_WIN);
    font_t *f = font_get(FONT_MEDIUM);
    for (int i = 0; i < n; i++) {
        window_t *w = list[i];
        rect_t t = mc_target(w, i, n);
        rect_t r = rect_make((int)(w->x + (t.x - w->x) * k), (int)(w->y + (t.y - w->y) * k),
                             (int)(w->w + (t.w - w->w) * k), (int)(w->h + (t.h - w->h) * k));
        gfx_shadow(s, r.x, r.y, r.w, r.h, WIN_RADIUS * 0.8f, 26, T.dark ? 150 : 80, 10);
        gfx_blit_scaled(s, w->surf, r.x, r.y, r.w, r.h, 255);
        if (k > 0.95f) {
            if (i == mc_hover) gfx_round_rect_stroke(s, r.x - 4, r.y - 4, r.w + 8, r.h + 8, WIN_RADIUS + 3, 3, T.accent);
            const char *title = w->title[0] ? w->title : (w->app ? w->app->name : "");
            int tw = MIN(text_width(f, 13, title) + 20, r.w + 40);
            rect_t lb = rect_make(r.x + r.w / 2 - tw / 2, r.y + r.h + 10, tw, 22);
            gfx_round_rect(s, lb.x, lb.y, lb.w, lb.h, 11, RGBA(0, 0, 0, i == mc_hover ? 150 : 90));
            text_draw_center(s, f, 13, lb, RGB(255, 255, 255), title);
        }
    }
    if (!n && k > 0.9f) text_draw_center(s, f, 16, rect_make(0, g_screen->h / 2 - 20, g_screen->w, 40), RGBA(255, 255, 255, 200), "Nenhuma janela aberta");
}

static bool mc_mouse(bool lp) {
    window_t *list[MAX_WIN];
    int n = mc_list(list, MAX_WIN), h = -1;
    for (int i = 0; i < n; i++) if (rect_contains(rect_inset(mc_target(list[i], i, n), -6), mx, my)) h = i;
    if (h != mc_hover) { mc_hover = h; wm_damage_all(); }
    if (lp && my > display_rect(0).y + MENUBAR_H) {
        if (h >= 0) wm_focus(list[h]);
        wm_toggle_mission_control();
    }
    return true;
}

void wm_zoom(window_t *w) {
    if (!(w->flags & WIN_RESIZABLE)) return;
    if (w->fullscreen) { wm_fullscreen(w); return; }
    wm_damage(win_bounds(w));
    if (w->zoomed) {
        w->x = w->restore.x; w->y = w->restore.y; w->w = w->restore.w; w->h = w->restore.h;
        w->zoomed = false;
    } else {
        w->restore = rect_make(w->x, w->y, w->w, w->h);
        rect_t a = work_area(display_for(rect_make(w->x, w->y, w->w, w->h)));   /* o monitor onde a janela está */
        w->x = a.x + 4; w->y = a.y + 4; w->w = a.w - 8; w->h = a.h - 10;
        w->zoomed = true;
    }
    surface_resize(w->surf, w->w, w->h);
    w->needs_paint = true;
    wm_damage(win_bounds(w));
}

/* ============================================================ pintura de janela */
static void corner_mask(surface_t *s, float r) {
    int ri = (int)fceil(r);
    for (int j = 0; j < ri; j++)
        for (int i = 0; i < ri; i++) {
            float dx = ri - i - 0.5f, dy = ri - j - 0.5f;
            float d = fsqrt(dx * dx + dy * dy) - r + (ri - r);
            float c = 0.5f - d;
            if (c >= 1) continue;
            u32 a = c <= 0 ? 0 : (u32)(c * 255);
            int xs[2] = {i, s->w - 1 - i}, ys[2] = {j, s->h - 1 - j};
            for (int q = 0; q < 4; q++) {
                u32 *p = &s->px[(usize)ys[q >> 1] * s->stride + xs[q & 1]];
                u32 v = *p;
                u32 rb = ((v & 0x00FF00FF) * a >> 8) & 0x00FF00FF, ag = (((v >> 8) & 0x00FF00FF) * a) & 0xFF00FF00;
                *p = rb | ag;
            }
        }
}

/* barra lateral flutuante (vidro): retângulo em coordenadas da janela */
#define SIDEBAR_INSET 6
#define SIDEBAR_RADIUS 14.0f
static bool sidebar_panel(window_t *w, rect_t *r) {
    if (!w->sidebar_w) return false;
    if (unified(w)) {
        /* coluna lateral encostada na borda, de altura total (os cantos da janela a recortam);
         * os semáforos e o botão de recolher ficam no topo dela */
        *r = rect_make(0, 0, w->sidebar_w, w->h);
    } else {
        int top = title_h(w) + w->toolbar_h;
        *r = rect_make(SIDEBAR_INSET, top + 2, w->sidebar_w - SIDEBAR_INSET - 4, w->h - top - 2 - SIDEBAR_INSET);
    }
    return r->w > 20 && r->h > 20;
}

/* torna transparente (com antisserrilhado) um retângulo arredondado da superfície */
static void punch_rrect(surface_t *s, rect_t rc, float rad) {
    float cx = rc.x + rc.w / 2.0f, cy = rc.y + rc.h / 2.0f, hw = rc.w / 2.0f, hh = rc.h / 2.0f;
    rect_t c = rect_intersect(rc, rect_make(0, 0, s->w, s->h));
    for (int j = c.y; j < c.y + c.h; j++) {
        u32 *row = s->px + (usize)j * s->stride;
        float py = j + 0.5f;
        bool mid = py > rc.y + rad + 1 && py < rc.y + rc.h - rad - 1;
        for (int i = c.x; i < c.x + c.w; i++) {
            float px = i + 0.5f;
            if (mid && px > rc.x + 1 && px < rc.x + rc.w - 1) { row[i] = 0; continue; }
            float qx = fabsf_(px - cx) - (hw - rad), qy = fabsf_(py - cy) - (hh - rad);
            float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
            float in = qx > qy ? qx : qy;
            if (in > 0) in = 0;
            float d = fsqrt(ox * ox + oy * oy) + in - rad;
            float cv = 0.5f - d;
            if (cv <= 0) continue;
            if (cv >= 1) { row[i] = 0; continue; }
            u32 a = 255 - (u32)(cv * 255), v = row[i];
            row[i] = ((((v & 0x00FF00FF) * a) >> 8) & 0x00FF00FF) | ((((v >> 8) & 0x00FF00FF) * a) & 0xFF00FF00);
        }
    }
}

static void draw_traffic(surface_t *s, window_t *w, bool focused) {
    static const u32 fill[3] = {RGB(255, 95, 87), RGB(255, 189, 46), RGB(40, 201, 64)};
    static const u32 edge[3] = {RGB(226, 70, 64), RGB(223, 161, 36), RGB(30, 171, 41)};
    bool hover = traffic_hover && wm_focused() == w;
    for (int i = 0; i < 3; i++) {
        float cx, cy;
        traffic_center(w, i, &cx, &cy);
        bool disabled = (i == 1 && (w->flags & WIN_NO_MINIMIZE)) || (i == 2 && (w->flags & (WIN_NO_ZOOM)));
        if (!(w->flags & WIN_RESIZABLE) && i == 2) disabled = true;
        u32 f = fill[i], e = edge[i];
        if (!focused || disabled) {
            f = T.dark ? RGB(88, 88, 88) : RGB(206, 206, 206);
            e = T.dark ? RGB(72, 72, 72) : RGB(188, 188, 188);
        }
        if (drag_mode == DRAG_TRAFFIC && drag_win == w && traffic_btn == i) { f = color_lighten(f, -40); }
        gfx_circle(s, cx, cy, 6.5f, e);
        gfx_circle(s, cx, cy, 5.8f, f);
        if (focused && !disabled) gfx_circle(s, cx, cy - 2.2f, 3.0f, RGBA(255, 255, 255, 46)); /* reflexo */
        if (hover && !disabled) {
            u32 g = RGBA(0, 0, 0, 150);
            if (i == 0) {
                gfx_line(s, cx - 2.6f, cy - 2.6f, cx + 2.6f, cy + 2.6f, 1.2f, g);
                gfx_line(s, cx + 2.6f, cy - 2.6f, cx - 2.6f, cy + 2.6f, 1.2f, g);
            } else if (i == 1) {
                gfx_line(s, cx - 3.2f, cy, cx + 3.2f, cy, 1.3f, g);
            } else {
                float pts1[] = {cx - 3.2f, cy - 3.2f, cx + 1.4f, cy - 3.2f, cx - 3.2f, cy + 1.4f};
                float pts2[] = {cx + 3.2f, cy + 3.2f, cx - 1.4f, cy + 3.2f, cx + 3.2f, cy - 1.4f};
                gfx_polygon(s, pts1, 3, g);
                gfx_polygon(s, pts2, 3, g);
            }
        }
    }
}

/* ============================================================ abas de janela
 * Janelas do mesmo grupo (tab_group ≠ 0) dividem um quadro; só a aba da frente fica visível — as outras
 * ficam com visible = false e herdam posição e tamanho quando voltam para a frente. */
static int tab_members(window_t *w, window_t **out, int max) {
    int n = 0;
    if (!w->tab_group) return 0;
    for (int i = 0; i < nwins && n < max; i++)
        if (wins[i]->tab_group == w->tab_group && !wins[i]->closing) out[n++] = wins[i];
    for (int a = 1; a < n; a++)                               /* ordem estável: criação */
        for (int b = a; b > 0 && out[b - 1]->id > out[b]->id; b--) { window_t *t = out[b]; out[b] = out[b - 1]; out[b - 1] = t; }
    return n;
}

static rect_t tab_rect(window_t *w, int k, int n) {
    int y0 = tab_strip_y(w), x0 = unified(w) ? 92 : 8, x1 = w->w - 8 - 32;
    int tw = (x1 - x0) / MAX(1, n);
    return rect_make(x0 + k * tw, y0 + 5, tw - 2, TAB_H - 10);
}
static rect_t tab_plus_rect(window_t *w) { return rect_make(w->w - 8 - 28, tab_strip_y(w) + 5, 26, TAB_H - 10); }

static void tab_frame_from(window_t *dst, window_t *src) {
    dst->x = src->x; dst->y = src->y; dst->w = src->w; dst->h = src->h;
    dst->zoomed = src->zoomed; dst->restore = src->restore;
}

static void tab_repaint_group(window_t *w) {
    window_t *m[16];
    int n = tab_members(w, m, 16);
    for (int k = 0; k < n; k++) m[k]->needs_paint = true;
}

static void tab_activate(window_t *w) {
    if (!w) return;
    window_t *m[16], *cur = NULL;
    int n = tab_members(w, m, 16);
    for (int k = 0; k < n; k++) if (!m[k]->tab_hidden && m[k] != w) cur = m[k];
    if (cur) {
        tab_frame_from(w, cur);
        cur->tab_hidden = true; cur->visible = false;
        wm_damage(win_bounds(cur));
    }
    w->tab_hidden = false; w->visible = true;
    if (w->surf->w != w->w || w->surf->h != w->h) surface_resize(w->surf, w->w, w->h);
    tab_repaint_group(w);
    wm_focus(w);
    wm_damage(win_bounds(w));
}

/* a janela sai do grupo (fechar ou "mover para nova janela"); se sobrar uma só, o grupo acaba */
static void tab_leave(window_t *w) {
    if (!w->tab_group) return;
    window_t *m[16];
    int n = tab_members(w, m, 16);
    if (!w->tab_hidden) {
        for (int k = 0; k < n; k++) if (m[k] != w) { tab_activate(m[k]); break; }
    }
    int g = w->tab_group;
    w->tab_group = 0; w->tab_hidden = false;
    int left = 0;
    window_t *last = NULL;
    for (int i = 0; i < nwins; i++) if (wins[i]->tab_group == g && !wins[i]->closing) { left++; last = wins[i]; }
    if (left == 1) { last->tab_group = 0; last->tab_hidden = false; last->visible = true; last->needs_paint = true; }
    else if (last) tab_repaint_group(last);
    wm_damage_all();
}

void wm_merge_tabs(window_t *w) {
    if (!w || w->fullscreen) return;
    int g = w->tab_group ? w->tab_group : w->id, n = 0;
    for (int i = 0; i < nwins; i++) {
        window_t *o = wins[i];
        if (o->app != w->app || o->closing || o->minimized || o->fullscreen || (o->flags & (WIN_DIALOG | WIN_NO_TITLE))) continue;
        o->tab_group = g;
        n++;
        if (o != w) { tab_frame_from(o, w); o->tab_hidden = true; o->visible = false; }
    }
    if (n < 2) { w->tab_group = 0; return; }
    w->tab_hidden = false; w->visible = true;
    tab_repaint_group(w);
    wm_focus(w);
    wm_damage_all();
}

void wm_tab_detach(window_t *w) {
    if (!w || !w->tab_group) return;
    tab_leave(w);
    w->visible = true;
    w->x += 30; w->y += 30;
    w->needs_paint = true;
    wm_focus(w);
    wm_damage_all();
}

void wm_tab_step(int dir) {
    window_t *f = wm_focused();
    if (!f || !f->tab_group) return;
    window_t *m[16];
    int n = tab_members(f, m, 16), k = 0;
    for (int i = 0; i < n; i++) if (m[i] == f) k = i;
    tab_activate(m[(k + dir + n) % n]);
}

/* "+" na faixa: o app abre uma janela nova, que entra no grupo como aba da frente */
static void tab_new(window_t *src) {
    if (!src->app || !src->app->launch) return;
    tab_pending_src = src;
    src->app->launch(src->app);
    tab_pending_src = NULL;
}

void wm_new_tab(window_t *w) { if (w) tab_new(w); }

static void draw_tab_bar(surface_t *s, window_t *w, bool focused) {
    window_t *m[16];
    int n = tab_members(w, m, 16);
    if (n < 1) return;
    int y0 = tab_strip_y(w);
    gfx_fill(s, 0, y0, w->w, TAB_H, T.dark ? RGBA(0, 0, 0, 70) : RGBA(0, 0, 0, 16));
    gfx_fill(s, 0, y0 + TAB_H - 1, w->w, 1, T.separator);
    int hx = mx - w->x, hy = my - w->y;
    font_t *f = font_get(FONT_MEDIUM);
    for (int k = 0; k < n; k++) {
        rect_t r = tab_rect(w, k, n);
        bool act = !m[k]->tab_hidden, hov = rect_contains(r, hx, hy);
        float rad = r.h / 2.0f;
        if (act) {
            gfx_shadow(s, r.x, r.y, r.w, r.h, rad, 4, T.dark ? 70 : 26, 1);
            gfx_round_rect(s, r.x, r.y, r.w, r.h, rad, T.dark ? RGB(66, 66, 70) : RGB(255, 255, 255));
        } else if (hov) gfx_round_rect(s, r.x, r.y, r.w, r.h, rad, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 14));
        else if (k > 0 && m[k - 1]->tab_hidden) gfx_fill(s, r.x - 1, r.y + 6, 1, r.h - 12, T.separator);
        const char *t = m[k]->title[0] ? m[k]->title : (m[k]->app ? m[k]->app->name : "");
        int maxw = r.w - 56, tw = MIN(text_width(f, 13, t), maxw);
        int tx = r.x + (r.w - tw) / 2, base = r.y + r.h / 2 + 5;
        text_draw_ellipsis(s, f, 13, tx, base, maxw, act && focused ? T.text : T.text2, t);
        if (hov && n > 1) {
            /* fechar a aba: × à esquerda */
            float cx = r.x + 14, cy = r.y + r.h / 2.0f;
            gfx_line(s, cx - 3.5f, cy - 3.5f, cx + 3.5f, cy + 3.5f, 1.3f, T.text2);
            gfx_line(s, cx + 3.5f, cy - 3.5f, cx - 3.5f, cy + 3.5f, 1.3f, T.text2);
        }
    }
    rect_t p = tab_plus_rect(w);
    if (rect_contains(p, hx, hy)) gfx_round_rect(s, p.x, p.y, p.w, p.h, 6, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 14));
    float pcx = p.x + p.w / 2.0f, pcy = p.y + p.h / 2.0f;
    gfx_line(s, pcx - 5, pcy, pcx + 5, pcy, 1.4f, T.text2);
    gfx_line(s, pcx, pcy - 5, pcx, pcy + 5, 1.4f, T.text2);
}

/* clique na faixa de abas (coordenadas da janela); true se usou o clique */
static bool tab_click(window_t *w, int lx, int ly) {
    window_t *m[16];
    int n = tab_members(w, m, 16);
    if (rect_contains(tab_plus_rect(w), lx, ly)) { tab_new(w); return true; }
    for (int k = 0; k < n; k++) {
        rect_t r = tab_rect(w, k, n);
        if (!rect_contains(r, lx, ly)) continue;
        if (n > 1 && lx < r.x + 26) { wm_close(m[k]); return true; }
        tab_activate(m[k]);
        return true;
    }
    return false;
}

static void paint_window(window_t *w) {
    surface_t *s = w->surf;
    if (s->w != w->w || s->h != w->h) surface_resize(s, w->w, w->h);
    surface_reset_clip(s);
    bool focused = wm_focused() == w;
    int th = title_h(w);
    memset(s->px, 0, (usize)s->stride * s->h * 4);
    u32 body = w->bg ? w->bg : T.window_bg;
    gfx_round_rect(s, 0, 0, w->w, w->h, WIN_RADIUS, body);
    if (th || w->toolbar_h) {
        /* barra de título e de ferramentas unificadas, sem degradê: só a cor da janela */
        int hh = th + w->toolbar_h;
        u32 tb = w->title_bg ? w->title_bg : focused ? T.title_top : T.title_inactive;
        gfx_round_rect_ex(s, 0, 0, w->w, hh, WIN_RADIUS, WIN_RADIUS, 0, 0, tb, tb);
        /* separador fino na base do cabeçalho, só sobre o conteúdo (não atravessa a barra lateral) */
        if (w->toolbar_h) gfx_fill(s, w->sidebar_w, hh, w->w - w->sidebar_w, 1, T.separator);
    }
    rect_t sp;
    if (sidebar_panel(w, &sp)) {
        if (unified(w)) gfx_fill(s, sp.x, sp.y, sp.w, sp.h, T.sidebar_bg); /* coluna sólida; os cantos são recortados no fim */
        else punch_rrect(s, sp, SIDEBAR_RADIUS); /* o vidro é composto por baixo */
    }
    if (unified(w) && w->title_x >= 0) {
        /* título em negrito, centralizado verticalmente no cabeçalho */
        int tx = w->title_x ? w->title_x : (w->sidebar_w ? w->sidebar_w + 18 : 96);
        int maxw = MAX(40, w->w / 2 - tx - 10);
        font_t *f = font_get(FONT_BOLD);
        float fs = 15;
        int base = HEADER_CY + tab_h(w) - (text_ascent(f, fs) + text_descent(f, fs)) / 2 + text_ascent(f, fs);
        text_draw_ellipsis(s, f, fs, tx, base, maxw, focused ? T.text : T.text3, w->title);
    }
    if (th && !unified(w)) {
        draw_traffic(s, w, focused);
        if (tab_h(w)) gfx_fill(s, 0, TITLEBAR_H, w->w, 1, T.separator);
        font_t *f = font_get(FONT_MEDIUM);
        int tw = text_width(f, FONT_UI, w->title);
        int tx = (w->w - tw) / 2;
        if (tx < 84) tx = 84;
        int base = (TITLEBAR_H - (text_ascent(f, FONT_UI) + text_descent(f, FONT_UI))) / 2 + text_ascent(f, FONT_UI);
        text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), FONT_UI, tx, base, w->w - tx - 10, w->title_bg ? RGBA(40, 40, 30, focused ? 230 : 140) : focused ? T.text : T.text3, w->title);
    }
    /* conteúdo */
    ui_t *u = &w->ui;
    u->win = w;
    u->s = s;
    u->now = timer_ticks();
    u->cw = w->w;
    u->ch = w->h - th;
    u->cursor = CURSOR_ARROW;
    s->ox = 0; s->oy = th;
    surface_clip(s, rect_make(0, 0, u->cw, u->ch));
    w->needs_paint = false; /* o app pode pedir nova pintura durante paint (ex.: título mudou) */
    if (w->paint) {
        u->nfl = 0;
        u->kfocus_rect = rect_make(0, 0, 0, 0);
        w->paint(w, u);
        /* navegação por teclado (Tab/⇧Tab) e anel de foco do controle selecionado */
        { extern void ui_focus_navigate(ui_t *u); extern void ui_focus_draw(ui_t *u); ui_focus_navigate(u); ui_focus_draw(u); }
        /* sobreposições adiadas: popover (com entrada própria) e dica de ferramenta, por cima de tudo */
        if (u->overlay_fn) {
            void (*fn)(struct ui *) = u->overlay_fn;
            u->overlay_fn = NULL;
            u->in_overlay = true;
            fn(u);
            u->in_overlay = false;
        }
        if (u->tip_on) { extern void ui_draw_tooltip(ui_t *u); ui_draw_tooltip(u); u->tip_on = false; }
        /* o bloqueio pedido neste quadro (popover/folha) vale a partir do próximo; se ninguém pediu, some */
        u->modal_rect = u->modal_next;
        u->modal_next = rect_make(0, 0, 0, 0);
        u->in_overlay = false;
        if (u->released) u->active = 0;
        if (u->clicked_any || u->key_used || u->released || u->pressed) {
            /* segundo passe sem eventos transitórios: reflete mudanças de estado */
            u->pressed = u->released = u->rpressed = u->dblclick = false;
            u->key = 0; u->text[0] = 0; u->wheel = 0;
            u->clicked_any = u->key_used = false;
            s->ox = 0; s->oy = 0;
            surface_reset_clip(s);
            int keep_cursor = u->cursor;
            memset(s->px, 0, (usize)s->stride * s->h * 4);
            w->needs_paint = false;
            paint_window(w);
            u->cursor = keep_cursor;
            return;
        }
    }
    u->pressed = u->released = u->rpressed = u->dblclick = false;
    u->key = 0; u->text[0] = 0; u->wheel = 0;
    u->clicked_any = u->key_used = false;
    s->ox = s->oy = 0;
    surface_reset_clip(s);
    if (unified(w)) {
        /* separador na base do cabeçalho (52 px), desenhado depois do conteúdo para não ser coberto */
        gfx_fill(s, w->sidebar_w, title_h(w) + w->toolbar_h - 1, w->w - w->sidebar_w, 1, T.separator);
        if (tab_h(w)) draw_tab_bar(s, w, focused);
        draw_traffic(s, w, focused);
        draw_sidebar_toggle(s, w);
    } else if (tab_h(w)) draw_tab_bar(s, w, focused);
    if (w->flags & WIN_FULLSIZE) draw_traffic(s, w, focused);   /* por cima do conteúdo do app */
    /* borda fina */
    gfx_round_rect_stroke(s, 0, 0, w->w, w->h, WIN_RADIUS, 1, T.dark ? RGBA(0, 0, 0, 200) : RGBA(0, 0, 0, 46));
    gfx_specular_rim(s, 1, 1, w->w - 2, w->h - 2, WIN_RADIUS - 1, T.dark ? 70 : 150); /* borda de vidro */
    if (!unified(w) && sidebar_panel(w, &sp)) {
        /* (só no painel flutuante antigo) borda discreta: contorno finíssimo + brilho leve */
        gfx_round_rect_stroke(s, sp.x - 0.5f, sp.y - 0.5f, sp.w + 1, sp.h + 1, SIDEBAR_RADIUS + 0.5f, 1, T.dark ? RGBA(255, 255, 255, 14) : RGBA(0, 0, 0, 12));
        gfx_specular_rim(s, sp.x, sp.y, sp.w, sp.h, SIDEBAR_RADIUS, T.dark ? 30 : 80);
    }
    corner_mask(s, WIN_RADIUS);
    wm_damage(win_bounds(w));
}

/* ============================================================ cursores
 * Desenho vetorial próprio: formas pretas com contorno branco (legíveis sobre qualquer fundo). */
#define CUR_SZ 32
static surface_t *cursors[CURSOR_KINDS];
static u64 wait_phase_t;

/* triângulo apontando na direção (dx, dy) com ponta a s px do centro; contorno branco + miolo preto */
static void cur_tri(surface_t *c, float cx, float cy, float dx, float dy, float s) {
    for (int k = 0; k < 2; k++) {
        float e = k ? 0 : 1.6f, ss = s + e;
        float px = -dy, py = dx;
        float tip_x = cx + dx * ss, tip_y = cy + dy * ss;
        float bx = cx - dx * (s * 0.55f + e), by = cy - dy * (s * 0.55f + e);
        float w = s * 0.95f + e;
        float p[] = {tip_x, tip_y, bx + px * w, by + py * w, bx - px * w, by - py * w};
        gfx_polygon(c, p, 3, k ? RGB(0, 0, 0) : RGB(255, 255, 255));
    }
}
static void cur_bar(surface_t *c, float x, float y, float w, float h) {
    gfx_round_rect(c, x - 1.4f, y - 1.4f, w + 2.8f, h + 2.8f, 2, RGB(255, 255, 255));
    gfx_round_rect(c, x, y, w, h, 1, RGB(0, 0, 0));
}
static void cur_line(surface_t *c, float x0, float y0, float x1, float y1, float t) {
    gfx_line(c, x0, y0, x1, y1, t + 2.4f, RGB(255, 255, 255));
    gfx_line(c, x0, y0, x1, y1, t, RGB(0, 0, 0));
}
static void cur_arrow_shape(surface_t *c) {
    float sh[] = {2, 2.5f, 2, 20, 6.4f, 16.1f, 9.2f, 22.7f, 12.4f, 21.3f, 9.8f, 15.1f, 15.2f, 15.1f};
    for (int i = 0; i < 7; i++) sh[i * 2] += 0.6f, sh[i * 2 + 1] += 1.2f;
    gfx_polygon(c, sh, 7, RGBA(0, 0, 0, 70));
    float o[] = {1.5f, 1, 1.5f, 18.5f, 5.7f, 14.6f, 8.5f, 21.2f, 11.7f, 19.8f, 9.1f, 13.6f, 14.5f, 13.6f};
    gfx_polygon(c, o, 7, RGB(255, 255, 255));
    float i2[] = {2.7f, 3.8f, 2.7f, 15.9f, 5.9f, 12.9f, 8.9f, 19.7f, 10.2f, 19.1f, 7.3f, 12.4f, 11.6f, 12.4f};
    gfx_polygon(c, i2, 7, RGB(0, 0, 0));
}
/* mão: palma + dedos (abertos, fechados ou apontando) */
static void cur_hand_shape(surface_t *c, int mode) {
    u32 W = RGB(255, 255, 255), B = RGB(0, 0, 0);
    if (mode == 0) { /* apontando */
        gfx_round_rect(c, 6.5f, 1, 6, 14, 3, B); gfx_round_rect(c, 7.5f, 2, 4, 13, 2, W);
        gfx_round_rect(c, 3, 11, 19, 13, 6, B); gfx_round_rect(c, 4, 12, 17, 11, 5, W);
        gfx_round_rect(c, 1, 11, 6, 9, 3, B); gfx_round_rect(c, 2, 12, 4, 7, 2, W);
        gfx_line(c, 12.5f, 13, 12.5f, 17, 1, B); gfx_line(c, 16.5f, 13, 16.5f, 17, 1, B);
        return;
    }
    float top = mode == 1 ? 2 : 8; /* dedos abertos ou recolhidos */
    for (int i = 0; i < 4; i++) {
        float fx = 7 + i * 4.2f;
        float ft = top + (i == 0 || i == 3 ? 2 : 0);
        gfx_round_rect(c, fx - 0.5f, ft - 0.5f, 5, 16 - ft + 8, 2.5f, B);
        gfx_round_rect(c, fx + 0.5f, ft + 0.5f, 3, 16 - ft + 7, 1.5f, W);
    }
    if (mode == 1) { gfx_round_rect(c, 1.5f, 12, 7, 5, 2.5f, B); gfx_round_rect(c, 2.5f, 13, 5, 3, 1.5f, W); } /* polegar */
    gfx_round_rect(c, 5.5f, 14, 19, 12, 5, B);
    gfx_round_rect(c, 6.5f, 15, 17, 10, 4, W);
}
static void cur_magnifier(surface_t *c, int sign) {
    float cx = 12, cy = 12, r = 7.5f;
    gfx_circle(c, cx, cy, r + 1.6f, RGB(255, 255, 255));
    gfx_circle_stroke(c, cx, cy, r, 2.4f, RGB(0, 0, 0));
    gfx_circle(c, cx, cy, r - 2.4f, RGB(255, 255, 255));
    cur_line(c, cx + 5.6f, cy + 5.6f, cx + 11, cy + 11, 3);
    gfx_line(c, cx - 3.5f, cy, cx + 3.5f, cy, 2.2f, RGB(0, 0, 0));
    if (sign > 0) gfx_line(c, cx, cy - 3.5f, cx, cy + 3.5f, 2.2f, RGB(0, 0, 0));
}
/* ocupado: anel de progresso girando (cinza + arco na cor de destaque), original */
static void cur_wait_shape(surface_t *c, float phase) {
    float cx = 14, cy = 14, r = 10;
    gfx_circle(c, cx, cy, r + 1.5f, RGB(255, 255, 255));
    gfx_ring_arc(c, cx, cy, r, 4.5f, 0, 2 * PI_F, RGB(210, 210, 214));
    float a0 = phase, a1 = phase + 2.2f;
    if (a1 > 2 * PI_F) { gfx_ring_arc(c, cx, cy, r, 4.5f, a0, 2 * PI_F, T.accent); gfx_ring_arc(c, cx, cy, r, 4.5f, 0, a1 - 2 * PI_F, T.accent); }
    else gfx_ring_arc(c, cx, cy, r, 4.5f, a0, a1, T.accent);
    gfx_circle_stroke(c, cx, cy, r + 0.8f, 0.8f, RGBA(0, 0, 0, 60));
}

static surface_t *make_cursor(int kind) {
    surface_t *c = surface_new(CUR_SZ, CUR_SZ);
    memset(c->px, 0, CUR_SZ * CUR_SZ * 4);
    float m = 13; /* centro dos cursores simétricos */
    switch (kind) {
    case CURSOR_ARROW: cur_arrow_shape(c); break;
    case CURSOR_COPY:
        cur_arrow_shape(c);
        gfx_round_rect(c, 13, 15, 11, 13, 2.5f, RGB(255, 255, 255));
        gfx_round_rect(c, 14.4f, 16.4f, 8.2f, 10.2f, 1.8f, RGB(0, 0, 0));
        gfx_fill(c, 16, 19, 5, 1, RGB(255, 255, 255)); gfx_fill(c, 16, 21, 5, 1, RGB(255, 255, 255)); gfx_fill(c, 16, 23, 3, 1, RGB(255, 255, 255));
        break;
    case CURSOR_IBEAM: {
        u32 cols[2] = {RGB(255, 255, 255), RGB(0, 0, 0)};
        for (int k = 0; k < 2; k++) {
            float t = k ? 1.2f : 3.0f;
            gfx_line(c, 8, 3, 8, 19, t, cols[k]);
            gfx_line(c, 5, 2.5f, 7.2f, 3, t, cols[k]); gfx_line(c, 8.8f, 3, 11, 2.5f, t, cols[k]);
            gfx_line(c, 5, 19.5f, 7.2f, 19, t, cols[k]); gfx_line(c, 8.8f, 19, 11, 19.5f, t, cols[k]);
        }
        break;
    }
    case CURSOR_CROSS:
        cur_line(c, m, m - 10, m, m - 3, 1.2f); cur_line(c, m, m + 3, m, m + 10, 1.2f);
        cur_line(c, m - 10, m, m - 3, m, 1.2f); cur_line(c, m + 3, m, m + 10, m, 1.2f);
        break;
    case CURSOR_HAND: cur_hand_shape(c, 0); break;
    case CURSOR_GRAB: cur_hand_shape(c, 1); break;
    case CURSOR_GRABBING: cur_hand_shape(c, 2); break;
    case CURSOR_ZOOM_IN: cur_magnifier(c, 1); break;
    case CURSOR_ZOOM_OUT: cur_magnifier(c, -1); break;
    case CURSOR_MOVE:
        cur_bar(c, m - 7, m - 1.2f, 14, 2.4f); cur_bar(c, m - 1.2f, m - 7, 2.4f, 14);
        cur_tri(c, m, m - 8, 0, -1, 4.5f); cur_tri(c, m, m + 8, 0, 1, 4.5f);
        cur_tri(c, m - 8, m, -1, 0, 4.5f); cur_tri(c, m + 8, m, 1, 0, 4.5f);
        break;
    case CURSOR_RESIZE_LEFT: cur_bar(c, m + 2, m - 7, 2.6f, 14); cur_tri(c, m - 4, m, -1, 0, 4.5f); break;
    case CURSOR_RESIZE_RIGHT: cur_bar(c, m - 4.6f, m - 7, 2.6f, 14); cur_tri(c, m + 4, m, 1, 0, 4.5f); break;
    case CURSOR_RESIZE_UP: cur_bar(c, m - 7, m + 2, 14, 2.6f); cur_tri(c, m, m - 4, 0, -1, 4.5f); break;
    case CURSOR_RESIZE_DOWN: cur_bar(c, m - 7, m - 4.6f, 14, 2.6f); cur_tri(c, m, m + 4, 0, 1, 4.5f); break;
    case CURSOR_RESIZE_COL: cur_bar(c, m - 1.3f, m - 7, 2.6f, 14); cur_tri(c, m - 6, m, -1, 0, 4.2f); cur_tri(c, m + 6, m, 1, 0, 4.2f); break;
    case CURSOR_RESIZE_ROW: cur_bar(c, m - 7, m - 1.3f, 14, 2.6f); cur_tri(c, m, m - 6, 0, -1, 4.2f); cur_tri(c, m, m + 6, 0, 1, 4.2f); break;
    case CURSOR_RESIZE_H: cur_tri(c, m - 5, m, -1, 0, 4.5f); cur_tri(c, m + 5, m, 1, 0, 4.5f); break;
    case CURSOR_RESIZE_V: cur_tri(c, m, m - 5, 0, -1, 4.5f); cur_tri(c, m, m + 5, 0, 1, 4.5f); break;
    case CURSOR_RESIZE_D: cur_tri(c, m - 4, m - 4, -0.7071f, -0.7071f, 4.5f); cur_tri(c, m + 4, m + 4, 0.7071f, 0.7071f, 4.5f); break;
    case CURSOR_RESIZE_D2: cur_tri(c, m + 4, m - 4, 0.7071f, -0.7071f, 4.5f); cur_tri(c, m - 4, m + 4, -0.7071f, 0.7071f, 4.5f); break;
    case CURSOR_WAIT: cur_wait_shape(c, 0); break;
    }
    return c;
}

static surface_t *cursor_surface(int k, int *hx, int *hy) {
    if (k < 0 || k >= CURSOR_KINDS || !cursors[k]) k = CURSOR_ARROW;
    switch (k) {
    case CURSOR_ARROW: case CURSOR_COPY: *hx = 2; *hy = 1; break;
    case CURSOR_IBEAM: *hx = 8; *hy = 11; break;
    case CURSOR_HAND: *hx = 9; *hy = 1; break;
    case CURSOR_ZOOM_IN: case CURSOR_ZOOM_OUT: *hx = 12; *hy = 12; break;
    case CURSOR_WAIT: *hx = 14; *hy = 14; break;
    default: *hx = 13; *hy = 13; break;
    }
    return cursors[k];
}

static rect_t cursor_rect(void) { return rect_make(mx - 16, my - 16, 48, 48); }

void wm_set_cursor(int c) {
    if (c != cursor_kind) { cursor_kind = c; wm_damage(cursor_rect()); }
}

/* anima o cursor de "ocupado" (redesenha o anel girando) */
static void cursor_tick(void) {
    if (cursor_kind != CURSOR_WAIT) return;
    u64 now = timer_ticks();
    if (now - wait_phase_t < 60) return;
    wait_phase_t = now;
    surface_t *c = cursors[CURSOR_WAIT];
    memset(c->px, 0, CUR_SZ * CUR_SZ * 4);
    cur_wait_shape(c, (float)((now / 60) % 24) * (2 * PI_F / 24));
    wm_damage(cursor_rect());
}

void wm_init(void) {
    for (int k = 0; k < CURSOR_KINDS; k++) cursors[k] = make_cursor(k);
    mx = display_rect(0).x + display_rect(0).w / 2; my = display_rect(0).y + display_rect(0).h / 2;
    wm_damage_all();
}

/* o arranjo dos monitores mudou (gfx.c, display_set_layout): cada janela vai junto com o monitor em que
 * estava, e o mouse também. old/new = o que cada saída de vídeo mostrava antes e mostra agora. */
static int out_under(const rect_t *by_out, int n, rect_t r) {
    int prim = display_output_of(0), best = prim;
    i64 ba = -1;
    for (int o = 0; o < n; o++) {
        rect_t k = rect_intersect(r, by_out[o]);
        i64 a = (i64)k.w * k.h;
        if (a > ba || (a == ba && o == prim)) { ba = a; best = o; }   /* empate (espelho): o principal */
    }
    return best;
}

void wm_displays_changed(const rect_t *old_by_out, const rect_t *new_by_out, int n) {
    for (int i = 0; i < nwins; i++) {
        window_t *w = wins[i];
        int o = out_under(old_by_out, n, rect_make(w->x, w->y, w->w, w->h));
        int dx = new_by_out[o].x - old_by_out[o].x, dy = new_by_out[o].y - old_by_out[o].y;
        w->x += dx; w->y += dy;
        w->restore.x += dx; w->restore.y += dy;
        /* a barra de título precisa continuar em algum monitor; senão a janela volta para o principal */
        if (display_at(w->x + MIN(w->w, 120) / 2, w->y + 10) < 0) {
            rect_t a = work_area(0);
            w->x = a.x + MAX(8, (a.w - w->w) / 2);
            w->y = a.y + 8;
        }
    }
    int o = out_under(old_by_out, n, rect_make(mx, my, 1, 1));
    mx += new_by_out[o].x - old_by_out[o].x;
    my += new_by_out[o].y - old_by_out[o].y;
    display_clamp(&mx, &my);
    tile_disp = 0;
    snap_zone = TILE_NONE;
    ndirty = 0;
    wm_damage_all();
}

/* ============================================================ Spaces e tela cheia
 * Trocar de Space desliza a tela inteira: guarda o quadro atual (sl_a), compõe o novo Space por inteiro
 * (sl_b) e anima os dois lado a lado. Durante a transição nada mais é recomposto. */
static surface_t *sl_a, *sl_b;
static float sl_t;
static int sl_dir, sl_state;   /* 0 parado, 1 compor o novo Space, 2 deslizando */

static void snap_copy(surface_t *dst, surface_t *src) {
    int w = MIN(dst->w, src->w), h = MIN(dst->h, src->h);
    for (int y = 0; y < h; y++) memcpy(dst->px + (usize)y * dst->stride, src->px + (usize)y * src->stride, (usize)w * 4);
}
static void snap_blit(surface_t *dst, surface_t *src, int sx, int dx, int w) {
    if (w <= 0) return;
    for (int y = 0; y < MIN(dst->h, src->h); y++) memcpy(dst->px + (usize)y * dst->stride + dx, src->px + (usize)y * src->stride + sx, (usize)w * 4);
}

static void space_switch(window_t *to, int dir) {
    if (to == space_win) return;
    if (!sl_a) { sl_a = surface_new(g_screen->w, g_screen->h); sl_b = surface_new(g_screen->w, g_screen->h); }
    bool slide = sl_a && sl_b && sl_state == 0 && !mc_open;
    if (slide) snap_copy(sl_a, g_screen);
    space_win = to;
    sl_dir = dir >= 0 ? 1 : -1;
    sl_state = slide ? 1 : 0;
    for (int i = 0; i < nwins; i++) wins[i]->needs_paint = true;
    wm_damage_all();
}

/* desenha um quadro da transição; devolve true enquanto ela durar */
static bool space_slide_frame(void) {
    if (sl_state != 2) return false;
    int W = MIN(g_screen->w, sl_a->w);
    sl_t += 0.075f;
    ndirty = 0;                     /* durante a transição nada é recomposto */
    if (sl_t >= 1) {
        snap_copy(g_screen, sl_b);
        fb_flush(rect_make(0, 0, g_screen->w, g_screen->h));
        sl_state = 0;
        wm_damage_all();            /* volta a compor ao vivo (cursor, animações) */
        return true;
    }
    float u = 1 - sl_t, e = 1 - u * u * u;
    int a = (int)(e * W);
    if (sl_dir > 0) { snap_blit(g_screen, sl_a, a, 0, W - a); snap_blit(g_screen, sl_b, 0, W - a, a); }
    else { snap_blit(g_screen, sl_a, 0, a, W - a); snap_blit(g_screen, sl_b, W - a, 0, a); }
    fb_flush(rect_make(0, 0, g_screen->w, g_screen->h));
    return true;
}

bool wm_space_fullscreen(void) { return space_win != NULL; }

void wm_fullscreen(window_t *w) {
    if (!w || !(w->flags & WIN_RESIZABLE) || w->closing) return;
    if (!w->fullscreen) {
        if (w->tab_group) tab_leave(w), w->visible = true;
        w->fs_restore = rect_make(w->x, w->y, w->w, w->h);
        rect_t d = display_rect(0);
        w->fullscreen = true;
        w->x = d.x; w->y = d.y; w->w = d.w; w->h = d.h;
        surface_resize(w->surf, w->w, w->h);
        w->needs_paint = true;
        if (nspaces < (int)ARRAY_LEN(spaces)) spaces[nspaces++] = w;
        space_switch(w, 1);
        wm_focus(w);
    } else {
        space_switch(NULL, -1);
        for (int k = 0; k < nspaces; k++) if (spaces[k] == w) { memmove(spaces + k, spaces + k + 1, (nspaces - k - 1) * sizeof(window_t *)); nspaces--; break; }
        w->fullscreen = false;
        rect_t r = w->fs_restore;
        w->x = r.x; w->y = r.y; w->w = r.w; w->h = r.h;
        surface_resize(w->surf, w->w, w->h);
        w->needs_paint = true;
        wm_focus(w);
    }
    wm_damage_all();
}

/* ⌃← / ⌃→: a mesa é o Space 0; cada janela em tela cheia é um Space à direita */
void wm_space_step(int dir) {
    int cur = 0;
    for (int k = 0; k < nspaces; k++) if (spaces[k] == space_win) cur = k + 1;
    int nx = CLAMP(cur + dir, 0, nspaces);
    if (nx == cur) return;
    window_t *to = nx ? spaces[nx - 1] : NULL;
    space_switch(to, dir);
    if (to) wm_focus(to);
    else { window_t *f = wm_focused(); if (f) { f->needs_paint = true; } wm_damage_all(); }
}

/* ============================================================ Stage Manager */
#define STAGE_W 150
#define STAGE_MAX 5
typedef struct { app_t *app; window_t *top; rect_t r; } stage_item_t;

static int stage_items(stage_item_t *out) {
    int n = 0;
    if (!g_stage_manager || space_win) return 0;
    if (!stage_app) { window_t *f = wm_focused(); stage_app = f ? f->app : NULL; }
    for (int i = nwins - 1; i >= 0 && n < STAGE_MAX; i--) {
        window_t *w = wins[i];
        if (!w->visible || w->minimized || w->closing || w->fullscreen || w->app == stage_app) continue;
        bool seen = false;
        for (int k = 0; k < n; k++) if (out[k].app == w->app) seen = true;
        if (!seen) out[n++] = (stage_item_t){w->app, w, {0}};
    }
    rect_t d = display_rect(0);
    int bh = 112, gap = 22, total = n * bh + (n - 1) * gap;
    int y0 = MENUBAR_H + (desktop_dock_top() - MENUBAR_H - total) / 2;
    for (int k = 0; k < n; k++) out[k].r = rect_make(d.x + 18, y0 + k * (bh + gap), STAGE_W, bh);
    return n;
}

static void draw_stage_strip(surface_t *s) {
    stage_item_t it[STAGE_MAX];
    int n = stage_items(it);
    for (int k = 0; k < n; k++) {
        window_t *w = it[k].top;
        rect_t b = it[k].r;
        float sc = MIN((float)b.w / w->w, (float)(b.h - 14) / w->h);
        int tw = (int)(w->w * sc), th = (int)(w->h * sc);
        int tx = b.x + (b.w - tw) / 2, ty = b.y + (b.h - 14 - th) / 2;
        bool hov = rect_contains(b, mx, my);
        if (hov) { tx -= 3; ty -= 3; tw += 6; th += 6; }
        gfx_shadow(s, tx, ty, tw, th, 6, 14, T.dark ? 120 : 60, 4);
        gfx_blit_scaled(s, w->surf, tx, ty, tw, th, 255);
        surface_t *ic = icon_for(w->app, 30);
        if (ic) gfx_blit_scaled(s, ic, tx - 6, ty + th - 22, 30, 30, 255);
    }
}

void wm_toggle_stage_manager(void) {
    g_stage_manager = !g_stage_manager;
    window_t *f = wm_focused();
    stage_app = f ? f->app : NULL;
    if (g_stage_manager && stage_app) {
        /* abre espaço para a faixa: janelas do palco que invadem a esquerda andam para a direita */
        for (int i = 0; i < nwins; i++) {
            window_t *w = wins[i];
            if (w->app == stage_app && !w->fullscreen && !w->zoomed && w->x < STAGE_W + 36 && w->x + w->w + (STAGE_W + 36 - w->x) < display_rect(0).w)
                w->x = STAGE_W + 36;
        }
    }
    wm_damage_all();
}

/* ============================================================ composição */
static void anim_rect(window_t *w, rect_t *r, int *alpha) {
    *r = rect_make(w->x, w->y, w->w, w->h);
    *alpha = 255;
    float t = w->anim;
    if (t >= 1 && w->anim_dir != 2) return;
    if (w->anim_dir == 2 || (w->minimized && t < 1)) {
        /* efeito de escala em direção ao Dock */
        rect_t d = dock_window_rect(w);
        float k = t;
        k = k * k * (3 - 2 * k);
        r->x = (int)(d.x + (w->x - d.x) * k);
        r->y = (int)(d.y + (w->y - d.y) * k);
        r->w = (int)(d.w + (w->w - d.w) * k);
        r->h = (int)(d.h + (w->h - d.h) * k);
        *alpha = (int)(80 + 175 * k);
        return;
    }
    /* abertura com mola (ease-out-back): passa um pouco do tamanho final e assenta */
    float u = t - 1, back = 1 + 2.2f * u * u * u + 1.2f * u * u;
    float sc = 0.9f + 0.1f * back;
    r->w = (int)(w->w * sc); r->h = (int)(w->h * sc);
    r->x = w->x + (w->w - r->w) / 2; r->y = w->y + (w->h - r->h) / 2;
    *alpha = (int)(255 * MIN(1.0f, t * 2.5f));
}

/* ---------------- efeito "gênio" ao minimizar/restaurar
 * A janela é cortada em faixas horizontais; cada faixa tem o próprio progresso (as de baixo chegam
 * antes ao Dock), então a janela "escorre" afunilando até o ícone. k = 1 janela inteira, 0 no Dock. */
static bool genie_active(window_t *w) {
    return w->anim_dir == 2 || (w->minimized && w->anim_dir == 1 && w->anim < 1);
}

static void draw_genie(surface_t *s, window_t *w, float k) {
    rect_t d = dock_window_rect(w);
    float dcx = d.x + d.w / 2.0f, wcx = w->x + w->w / 2.0f;
    const int N = 64; /* faixas finas: contorno suave */
    for (int i = 0; i < N; i++) {
        float v0 = (float)i / N, v1 = (float)(i + 1) / N;
        float p0 = CLAMP(k * 1.5f - v0 * 0.5f, 0.0f, 1.0f), p1 = CLAMP(k * 1.5f - v1 * 0.5f, 0.0f, 1.0f);
        p0 = p0 * p0 * (3 - 2 * p0);
        p1 = p1 * p1 * (3 - 2 * p1);
        float y0 = d.y + v0 * d.h + ((w->y + v0 * w->h) - (d.y + v0 * d.h)) * p0;
        float y1 = d.y + v1 * d.h + ((w->y + v1 * w->h) - (d.y + v1 * d.h)) * p1;
        float pm = (p0 + p1) / 2;
        float wid = d.w + (w->w - d.w) * pm, cx = dcx + (wcx - dcx) * pm;
        int sy0 = (int)(v0 * w->h), sy1 = (int)(v1 * w->h);
        if (sy1 <= sy0 || y1 - y0 < 0.5f) continue;
        /* "sub-superfície" que aponta para as linhas da faixa, sem copiar pixels */
        surface_t band = *w->surf;
        band.px = w->surf->px + (usize)sy0 * w->surf->stride;
        band.h = sy1 - sy0;
        gfx_blit_scaled(s, &band, (int)(cx - wid / 2), (int)y0, MAX(1, (int)wid), MAX(1, (int)(y1 - y0 + 0.999f)), pm > 0.25f ? 255 : (int)(255 * pm / 0.25f));
    }
}

static void step_animations(void) {
    for (int i = 0; i < nwins; i++) {
        window_t *w = wins[i];
        if (w->anim_dir == 1 && w->anim < 1) { w->anim += 0.12f; if (w->anim >= 1) w->anim = 1; wm_damage(win_bounds(w)); }
        else if (w->anim_dir == -1) {
            w->anim -= 0.16f;
            wm_damage(win_bounds(w));
            if (w->anim <= 0) { destroy(w); i--; continue; }
        } else if (w->anim_dir == 2) {
            w->anim -= 0.09f;
            rect_t d = dock_window_rect(w);
            wm_damage(rect_union(win_bounds(w), rect_make(d.x - 10, d.y - 10, d.w + 20, d.h + 20)));
            if (w->anim <= 0) {
                w->anim = 0; w->anim_dir = 0; w->minimized = true;
                window_t *f = wm_focused();
                if (f) { f->needs_paint = true; }
                wm_damage_all();
            }
        } else if (w->minimized && w->anim_dir == 1 && w->anim < 1) {
            wm_damage_all();
        }
        if (w->anim_dir == 1 && w->anim >= 1 && w->minimized) {
            w->minimized = false;
            rect_t dr = display_rect(0);
            wm_damage(rect_make(0, dr.h - 220, dr.w, 220));   /* a miniatura sai do Dock */
            /* só agora ela conta como janela em foco: atualiza a barra de menus e o visual das outras */
            for (int k = 0; k < nwins; k++) wins[k]->needs_paint = true;
            wm_damage(menubar_rect());
        }
    }
}

/* O vidro lê o que está atrás dele (mais uma margem). Se só parte dessa área fosse recomposta,
 * o desfoque leria pixels do quadro anterior — inclusive o próprio vidro. Por isso, toda região
 * suja que toca um vidro passa a cobri-lo por inteiro, e regiões que se sobrepõem são fundidas. */
static void expand_for_glass(void) {
    rect_t g[48];
    int ng = desktop_glass_rects(g, 32);
    int reach = GLASS_REACH(&G[GLASS_SIDEBAR]);
    for (int i = 0; i < nwins && ng < (int)ARRAY_LEN(g); i++) {
        window_t *w = wins[i];
        rect_t sp;
        if (!w->visible || w->minimized || unified(w) || !sidebar_panel(w, &sp)) continue; /* coluna sólida não é vidro */
        g[ng++] = rect_make(w->x + sp.x - reach, w->y + sp.y - reach, sp.w + 2 * reach, sp.h + 2 * reach);
    }
    if (!ng) return;
    rect_t scr = rect_make(0, 0, g_screen->w, g_screen->h);
    for (int iter = 0; iter < 6; iter++) {
        bool changed = false;
        for (int d = 0; d < ndirty; d++)
            for (int k = 0; k < ng; k++) {
                if (rect_empty(rect_intersect(dirty[d], g[k]))) continue;
                rect_t u = rect_intersect(rect_union(dirty[d], g[k]), scr);
                if (u.x != dirty[d].x || u.y != dirty[d].y || u.w != dirty[d].w || u.h != dirty[d].h) { dirty[d] = u; changed = true; }
            }
        /* funde regiões sobrepostas para não compor (e desfocar) a mesma área duas vezes */
        for (int a = 0; a < ndirty; a++)
            for (int b = a + 1; b < ndirty; b++)
                if (!rect_empty(rect_intersect(dirty[a], dirty[b]))) {
                    dirty[a] = rect_union(dirty[a], dirty[b]);
                    dirty[b--] = dirty[--ndirty];
                    changed = true;
                }
        if (!changed) break;
    }
}

void wm_compose(void) {
    u64 now = timer_ticks();
    cursor_tick();
    mc_step();
    step_animations();
    if (green_w && !green_fired && now - green_t > 600) {
        green_fired = true;
        if (index_of(green_w) >= 0 && green_w == wm_focused() && !green_w->minimized) {
            float cx, cy;
            traffic_center(green_w, 2, &cx, &cy);
            desktop_tile_popover(green_w, green_w->x + (int)cx, green_w->y + (int)cy);
        }
    }
    for (int i = 0; i < nwins; i++) {
        window_t *w = wins[i];
        if (w->needs_paint && !w->minimized) paint_window(w);
        else if (w->ui.repaint && now - w->ui.caret_blink > 0 && !w->minimized) {
            static u64 last_blink;
            if (now - last_blink > 16) { last_blink = now; w->ui.repaint = false; paint_window(w); } /* ~60 qps para animações */
        }
    }
    if (space_slide_frame()) return;
    if (!ndirty) return;
    if (sl_state == 1) { ndirty = 0; wm_damage_all(); }   /* o novo Space é composto por inteiro */
    expand_for_glass();
    window_t *focused = wm_focused();
    for (int d = 0; d < ndirty; d++) {
        rect_t D = dirty[d];
        surface_t *s = g_screen;
        surface_reset_clip(s);
        surface_clip(s, D);
        if (space_win) gfx_fill(s, D.x, D.y, D.w, D.h, RGB(0, 0, 0));   /* tela cheia: fundo preto */
        else desktop_draw_background(s, D);
        if (mc_open) { mc_draw(s); desktop_draw_overlay(s, D); int hx0, hy0; surface_t *c0 = cursor_surface(cursor_kind, &hx0, &hy0); gfx_blend(s, c0, mx - hx0, my - hy0, 255); surface_reset_clip(s); continue; }
        draw_snap_preview(s);
        for (int i = 0; i < nwins; i++) {
            window_t *w = wins[i];
            if (!w->visible || (w->minimized && w->anim_dir != 1)) continue;
            if (!in_space(w) || !on_stage(w)) continue;
            rect_t r;
            int alpha;
            if (genie_active(w)) { draw_genie(s, w, CLAMP(w->anim, 0.0f, 1.0f)); continue; }
            anim_rect(w, &r, &alpha);
            if (rect_empty(rect_intersect(rect_make(r.x - SHADOW_M, r.y - SHADOW_M + 10, r.w + 2 * SHADOW_M, r.h + 2 * SHADOW_M), D))) continue;
            bool f = w == focused;
            int sh_a = (f ? (T.dark ? 170 : 85) : (T.dark ? 120 : 46)) * alpha / 255;
            gfx_shadow(s, r.x, r.y, r.w, r.h, WIN_RADIUS, f ? 40 : 22, sh_a, f ? 16 : 7);
            if (!f) gfx_shadow(s, r.x, r.y, r.w, r.h, WIN_RADIUS, 3, T.dark ? 60 : 24, 1);
            rect_t sp;
            if (!unified(w) && sidebar_panel(w, &sp)) {
                /* vidro da barra lateral, escalado junto com a janela nas animações */
                float kx = (float)r.w / w->w, ky = (float)r.h / w->h;
                glass_t g = G[GLASS_SIDEBAR];
                g.opacity = alpha;
                if (!f) g.tint = WITH_ALPHA(g.tint, MIN(255, (int)C_A(g.tint) + 50));
                gfx_glass(s, r.x + sp.x * kx, r.y + sp.y * ky, sp.w * kx, sp.h * ky, SIDEBAR_RADIUS * kx, &g);
            }
            if (r.w == w->w && r.h == w->h) gfx_blend(s, w->surf, r.x, r.y, alpha);
            else gfx_blit_scaled(s, w->surf, r.x, r.y, r.w, r.h, alpha);
        }
        draw_stage_strip(s);
        desktop_draw_overlay(s, D);
        draw_identify(s);
        int hx, hy;
        surface_t *c = cursor_surface(cursor_kind, &hx, &hy);
        gfx_blend(s, c, mx - hx, my - hy, 255);
        surface_reset_clip(s);
    }
    if (sl_state == 1) {
        /* novo Space pronto em g_screen: guarda e começa a deslizar a partir do quadro antigo */
        snap_copy(sl_b, g_screen);
        snap_copy(g_screen, sl_a);
        sl_state = 2;
        sl_t = 0;
        fb_flush(rect_make(0, 0, g_screen->w, g_screen->h));
        ndirty = 0;
        return;
    }
    for (int d = 0; d < ndirty; d++) fb_flush(dirty[d]);
    ndirty = 0;
}

/* ============================================================ eventos */
static window_t *window_at(int x, int y) {
    for (int i = nwins - 1; i >= 0; i--) {
        window_t *w = wins[i];
        if (w->minimized || w->closing || !w->visible || !in_space(w) || !on_stage(w)) continue;
        if (rect_contains(rect_make(w->x, w->y, w->w, w->h), x, y)) return w;
    }
    return NULL;
}

window_t *wm_window_at_point(int x, int y) { return window_at(x, y); }
window_t *wm_nth(int i) { return i >= 0 && i < nwins ? wins[i] : NULL; }

static int resize_edges(window_t *w, int x, int y) {
    if (!(w->flags & WIN_RESIZABLE) || w->zoomed || w->fullscreen) return 0;
    int e = 0;
    if (x >= w->x + w->w - 6 && x < w->x + w->w + 4) e |= 1;  /* direita */
    if (y >= w->y + w->h - 6 && y < w->y + w->h + 4) e |= 2;  /* baixo */
    if (x < w->x + 5 && x >= w->x - 4) e |= 4;                /* esquerda */
    if (y < w->y + 3 && y >= w->y - 4) e |= 8;                /* topo (faixa fina: o resto do título arrasta) */
    return e;
}

static void deliver_mouse(window_t *w, bool pressed, bool released, bool rpressed, int wheel) {
    ui_t *u = &w->ui;
    rect_t cr = wm_content_rect(w);
    u->mx = mx - cr.x; u->my = my - cr.y;
    u->inside = rect_contains(cr, mx, my) && window_at(mx, my) == w;
    u->down = mbuttons & 1;
    if (pressed) {
        u->pressed = true;
        u64 t = timer_ticks();
        u->dblclick = t - last_click_t < 400 && ABS_DIFF(mx, last_click_x) < 5 && ABS_DIFF(my, last_click_y) < 5;
        last_click_t = u->dblclick ? 0 : t;
        last_click_x = mx; last_click_y = my;
    }
    if (released) u->released = true;
    if (rpressed) u->rpressed = true;
    u->wheel += wheel;
    paint_window(w);
    if (u->inside) wm_set_cursor(u->cursor);
}

bool wm_handle_event(input_event_t *e) {
    if (e->type == EV_MOUSE || e->type == EV_MOUSE_ABS) {
        int nmx, nmy;
        if (e->type == EV_MOUSE_ABS) {
            /* tablet USB: posição absoluta, sem aceleração */
            nmx = CLAMP(e->dx, 0, g_screen->w - 1);
            nmy = CLAMP(e->dy, 0, g_screen->h - 1);
        } else {
            int odx = e->dx, ody = e->dy;
            /* aceleração suave para movimentos rápidos */
            int ad = ABS_DIFF(odx, 0) + ABS_DIFF(ody, 0);
            if (ad > 6) { odx = odx * 3 / 2; ody = ody * 3 / 2; }
            nmx = CLAMP(mx + odx, 0, g_screen->w - 1);
            nmy = CLAMP(my + ody, 0, g_screen->h - 1);
        }
        display_clamp(&nmx, &nmy);                    /* monitores de alturas diferentes: sem área morta */
        if (nmx != mx || nmy != my) {
            wm_damage(cursor_rect());
            mx = nmx; my = nmy;
            wm_damage(cursor_rect());
        }
        int old = mbuttons;
        mbuttons = e->buttons;
        bool lp = (mbuttons & 1) && !(old & 1), lr = !(mbuttons & 1) && (old & 1);
        bool rp = (mbuttons & 2) && !(old & 2);
        input_event_t ev = *e;
        ev.type = EV_MOUSE;
        ev.dx = mx; ev.dy = my; /* desktop recebe posição absoluta */

        /* arrastos em andamento têm prioridade (o desktop não vê esses eventos: avisa os botões) */
        extern void desktop_sync_buttons(int buttons);
        if ((drag_mode != DRAG_NONE && drag_win) || mc_open) desktop_sync_buttons(mbuttons);
        if (drag_mode != DRAG_NONE && drag_win) {
            window_t *w = drag_win;
            if (drag_mode == DRAG_MOVE) {
                wm_damage(win_bounds(w));
                if (w->zoomed && (ABS_DIFF(mx, drag_start.x) > 3 || ABS_DIFF(my, drag_start.y) > 3)) {
                    /* arrastar uma janela encaixada/ampliada devolve o tamanho anterior, mantendo o ponto pego */
                    float fx = (float)drag_dx / MAX(1, w->w);
                    w->w = w->restore.w; w->h = w->restore.h;
                    drag_dx = (int)(fx * w->w);
                    w->zoomed = false;
                    surface_resize(w->surf, w->w, w->h);
                    w->needs_paint = true;
                }
                w->x = mx - drag_dx;
                { int dm = display_at(mx, my); rect_t dmr = display_rect(dm < 0 ? 0 : dm);   /* a barra de título não passa do topo do monitor */
                  w->y = MAX(dmr.y + (dm <= 0 ? MENUBAR_H : 0), my - drag_dy); }
                wm_damage(win_bounds(w));
                /* zona de encaixe: bordas esquerda/direita (metade) e topo (preencher) */
                int di = display_at(mx, my);
                if (di < 0) di = 0;
                rect_t dr = display_rect(di);
                /* só bordas sem monitor vizinho: na divisa entre dois, o mouse passa direto */
                bool el = mx <= dr.x + 2 && display_at(dr.x - 1, my) < 0;
                bool er = mx >= dr.x + dr.w - 3 && display_at(dr.x + dr.w, my) < 0;
                /* cantos (faixa de 60 px junto ao topo/base de cada borda lateral) encaixam em quartos */
                int ytop = dr.y + (di ? 0 : MENUBAR_H), ybot = dr.y + dr.h;
                bool nt = my <= ytop + 60, nb = my >= ybot - 60;
                int z = el ? (nt ? TILE_TL : nb ? TILE_BL : TILE_LEFT)
                      : er ? (nt ? TILE_TR : nb ? TILE_BR : TILE_RIGHT)
                      : my <= ytop + 2 ? TILE_FILL : TILE_NONE;
                if (!(w->flags & WIN_RESIZABLE)) z = TILE_NONE;
                if (z != snap_zone || (z && di != tile_disp)) {
                    if (snap_zone) wm_damage(rect_inset(tile_rect(snap_zone), -4));
                    snap_zone = z;
                    tile_disp = di;
                    if (snap_zone) wm_damage(rect_inset(tile_rect(snap_zone), -4));
                }
            } else if (drag_mode == DRAG_RESIZE) {
                wm_damage(win_bounds(w));
                rect_t s0 = drag_start;
                int ddx = mx - drag_dx, ddy = my - drag_dy;
                if (drag_edges & 1) w->w = MAX(w->min_w, s0.w + ddx);
                if (drag_edges & 2) w->h = MAX(w->min_h, s0.h + ddy);
                if (drag_edges & 4) { int nw = MAX(w->min_w, s0.w - ddx); w->x = s0.x + s0.w - nw; w->w = nw; }
                if (drag_edges & 8) {
                    int nh = MAX(w->min_h, s0.h - ddy);
                    int ny = MAX(MENUBAR_H, s0.y + s0.h - nh);
                    w->h = s0.y + s0.h - ny; w->y = ny;
                }
                w->needs_paint = true;
                wm_damage(win_bounds(w));
            } else if (drag_mode == DRAG_CONTENT) {
                deliver_mouse(w, false, lr, false, 0);
            } else if (drag_mode == DRAG_TRAFFIC) {
                if (lr) {
                    float tcx, tcy;
                    traffic_center(w, traffic_btn, &tcx, &tcy);
                    rect_t cr = rect_make(w->x + (int)tcx - 9, w->y + (int)tcy - 10, 18, 20);
                    int b = traffic_btn;
                    drag_mode = DRAG_NONE; drag_win = NULL;
                    if (rect_contains(cr, mx, my)) {
                        if (b == 0) wm_close(w);
                        else if (b == 1) wm_minimize(w);
                        else if ((input_mods() & MOD_ALT) && !w->fullscreen) wm_zoom(w);   /* ⌥-clique: ampliar */
                        else wm_fullscreen(w);                                           /* clique: tela cheia */
                    }
                    w->needs_paint = true;
                    return true;
                }
            }
            if (lr) {
                if (drag_mode == DRAG_MOVE && snap_zone) { wm_damage(rect_inset(tile_rect(snap_zone), -4)); wm_tile(w, snap_zone); }
                snap_zone = TILE_NONE;
                drag_mode = DRAG_NONE; drag_win = NULL; wm_set_cursor(CURSOR_ARROW);
            }
            return true;
        }
        if (mc_open) return mc_mouse(lp);
        if (desktop_handle_event(&ev)) return true;

        window_t *w = window_at(mx, my);
        /* hover dos semáforos */
        bool th = false;
        if (w && !(w->flags & WIN_NO_TITLE) && w == wm_focused()) {
            int lx = mx - w->x, ly = my - w->y;
            float c0x, c0y;
            traffic_center(w, 0, &c0x, &c0y);
            th = lx >= c0x - 12 && lx < c0x + 52 && ly >= c0y - 12 && ly < c0y + 12;
        }
        if (th != traffic_hover) { traffic_hover = th; window_t *f = wm_focused(); if (f) f->needs_paint = true; }
        {
            int gb = (w && w == wm_focused() && (w->flags & WIN_RESIZABLE) && !(w->flags & WIN_NO_ZOOM)) ? traffic_at(w, mx - w->x, my - w->y) : -1;
            if (gb == 2 && !(mbuttons & 1)) { if (green_w != w) { green_w = w; green_t = timer_ticks(); green_fired = false; } }
            else green_w = NULL;
        }

        /* Stage Manager: passar o mouse realça; clicar traz o app para o palco */
        if (g_stage_manager && !space_win) {
            static bool stage_hover;
            bool sh = mx < display_rect(0).x + STAGE_W + 40;
            if (sh || stage_hover) wm_damage(rect_make(display_rect(0).x, MENUBAR_H, STAGE_W + 50, display_rect(0).h - MENUBAR_H));
            stage_hover = sh;
            if (lp && sh) {
                stage_item_t it[STAGE_MAX];
                int n = stage_items(it);
                for (int k = 0; k < n; k++) if (rect_contains(it[k].r, mx, my)) { wm_bring_app(it[k].app); return true; }
            }
        }
        /* faixa de abas: realce do item sob o mouse */
        if (w && tab_h(w)) {
            static bool tab_hover;
            int ly2 = my - w->y;
            bool in = ly2 >= tab_strip_y(w) && ly2 < tab_strip_y(w) + TAB_H;
            if (in || tab_hover) w->needs_paint = true;
            tab_hover = in;
        }
        if (lp || rp) {
            if (!w) {
                window_t *f = wm_focused();
                if (f) { f->ui.focus = 0; f->needs_paint = true; }
                return false;
            }
            if (w != wm_focused()) wm_focus(w);
            int lx = mx - w->x, ly = my - w->y;
            int edges = resize_edges(w, mx, my);
            if (lp && edges) {
                drag_mode = DRAG_RESIZE; drag_win = w; drag_edges = edges;
                drag_dx = mx; drag_dy = my;
                drag_start = rect_make(w->x, w->y, w->w, w->h);
                return true;
            }
            float tgx, tgy;
            if (lp && toggle_center(w, &tgx, &tgy) && (lx - tgx) * (lx - tgx) + (ly - tgy) * (ly - tgy) <= 18 * 18) {
                sidebar_toggle(w);
                return true;
            }
            int tb_hit = lp ? traffic_at(w, lx, ly) : -1;
            if (tb_hit >= 0) {
                drag_mode = DRAG_TRAFFIC; drag_win = w; traffic_btn = tb_hit;
                w->needs_paint = true;
                return true;
            }
            /* faixa de abas */
            if (lp && tab_h(w) && ly >= tab_strip_y(w) && ly < tab_strip_y(w) + TAB_H && tab_click(w, lx, ly)) return true;
            if (lp && w->fullscreen && ly < title_h(w) + w->toolbar_h) {
                /* tela cheia: o cabeçalho não arrasta; o clique vai para o conteúdo */
                deliver_mouse(w, true, false, false, 0);
                drag_mode = DRAG_CONTENT; drag_win = w;
                return true;
            }
            if (lp && !(w->flags & WIN_NO_TITLE) && ly < title_h(w)) {
                u64 t = timer_ticks();
                if (t - last_click_t < 400 && ABS_DIFF(mx, last_click_x) < 5) { wm_zoom(w); last_click_t = 0; return true; }
                last_click_t = t; last_click_x = mx; last_click_y = my;
                drag_mode = DRAG_MOVE; drag_win = w; drag_dx = mx - w->x; drag_dy = my - w->y; drag_start = rect_make(mx, my, 0, 0);
                return true;
            }
            if (lp && ly < title_h(w) + w->toolbar_h && !(w->flags & WIN_NO_TITLE)) {
                /* barra de ferramentas: entrega ao conteúdo, mas arrasta se ninguém usar o clique */
                deliver_mouse(w, true, false, false, 0);
                if (w->ui.active == 0) { drag_mode = DRAG_MOVE; drag_win = w; drag_dx = mx - w->x; drag_dy = my - w->y; drag_start = rect_make(mx, my, 0, 0); }
                else { drag_mode = DRAG_CONTENT; drag_win = w; }
                return true;
            }
            if (lp) { drag_mode = DRAG_CONTENT; drag_win = w; }
            deliver_mouse(w, lp, false, rp, 0);
            return true;
        }
        if (w) {
            int edges = resize_edges(w, mx, my);
            if (edges) wm_set_cursor((edges & 3) == 3 || (edges & 12) == 12 ? CURSOR_RESIZE_D
                                     : (edges & 6) == 6 || (edges & 9) == 9 ? CURSOR_RESIZE_D2
                                     : (edges & 10) ? CURSOR_RESIZE_V : CURSOR_RESIZE_H);
            if (e->dz) deliver_mouse(w, false, false, false, e->dz);
            else if (!edges) {
                rect_t cr = wm_content_rect(w);
                if (rect_contains(cr, mx, my)) deliver_mouse(w, false, false, false, 0);
                else wm_set_cursor(CURSOR_ARROW);
            }
            /* janela que perdeu o hover */
            static window_t *last_hover;
            if (last_hover && last_hover != w && index_of(last_hover) >= 0) { last_hover->ui.inside = false; last_hover->needs_paint = true; }
            last_hover = w;
        } else {
            wm_set_cursor(CURSOR_ARROW);
        }
        return w != NULL;
    }
    if (e->type == EV_KEY_UP) {
        /* só os apps do macOS recebem "tecla solta" (keyUp:, GCKeyboard); os apps nativos tratam só o pressionar */
        extern void compat_gui_key_up(window_t *w, input_event_t *e);
        window_t *w = wm_focused();
        if (w) compat_gui_key_up(w, e);
        return w != NULL;
    }
    if (e->type == EV_KEY_DOWN) {
        /* Mission Control: ⌃↑ ou F9; Esc/Enter dentro dele */
        if (((e->mods & MOD_CTRL) && !(e->mods & MOD_CMD) && e->key == KEY_UP) || e->key == KEY_F9) { wm_toggle_mission_control(); return true; }
        if (mc_open) {
            if (e->key == KEY_ESC) wm_toggle_mission_control();
            else if (e->key == KEY_ENTER) {
                window_t *list[MAX_WIN];
                int n = mc_list(list, MAX_WIN);
                if (mc_hover >= 0 && mc_hover < n) wm_focus(list[mc_hover]);
                wm_toggle_mission_control();
            }
            return true;
        }
        /* Spaces: ⌃← / ⌃→; abas: ⌃Tab / ⌃⇧Tab */
        if ((e->mods & MOD_CTRL) && !(e->mods & (MOD_CMD | MOD_ALT))) {
            if (e->key == KEY_LEFT) { wm_space_step(-1); return true; }
            if (e->key == KEY_RIGHT) { wm_space_step(1); return true; }
            if (e->key == KEY_TAB) { window_t *f = wm_focused(); if (f && f->tab_group) { wm_tab_step((e->mods & MOD_SHIFT) ? -1 : 1); return true; } }
        }
        /* organizar janelas: ⌃⌘← metade esquerda, ⌃⌘→ metade direita, ⌃⌘↑ preencher, ⌃⌘↓ tamanho anterior; ⌃⌘F tela cheia */
        if ((e->mods & MOD_CTRL) && (e->mods & MOD_CMD)) {
            window_t *f = wm_focused();
            if (f && (e->key == 'f' || e->key == 'F' || e->key == 6)) { wm_fullscreen(f); return true; }
            if (f && e->key == KEY_LEFT) { wm_tile(f, TILE_LEFT); return true; }
            if (f && e->key == KEY_RIGHT) { wm_tile(f, TILE_RIGHT); return true; }
            if (f && e->key == KEY_UP) { wm_tile(f, TILE_FILL); return true; }
            if (f && e->key == KEY_DOWN && f->zoomed) { wm_zoom(f); return true; }
        }
        /* atalhos globais, Busca, menus, Launchpad e alertas têm prioridade sobre a janela em foco */
        if (desktop_handle_event(e)) return true;
        window_t *w = wm_focused();
        if (!w) return false;
        if (w->on_key) w->on_key(w, e);
        ui_t *u = &w->ui;
        u->key = e->key;
        u->mods = e->mods;
        u->text[0] = 0;
        if (e->key >= 32 && e->key < 0x100 && e->key != 127 && !(e->mods & (MOD_CMD | MOD_CTRL))) {
            int n = utf8_encode((u32)e->key, u->text);
            u->text[n] = 0;
        }
        rect_t cr = wm_content_rect(w);
        u->mx = mx - cr.x; u->my = my - cr.y;
        paint_window(w);
        return true;
    }
    return false;
}
