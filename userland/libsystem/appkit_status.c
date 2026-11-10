/* mOS — modelo da barra de menus, do NSPopover e da política de ativação.
 * Sem classes: o teste de host compila este arquivo com -DCX_SB_HOST. */
#ifdef CX_SB_HOST
#include <stddef.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include "appkit_status.h"

enum { CX_SB_MAX = 32, CX_POP_MAX = 32, CX_TITLE = 128 };

typedef struct {
    void *item;
    void *menu;
    double length;
    int visible;
    int menu_open;
    int used;
    int token;
    char title[CX_TITLE];
} cx_sb_slot_t;

typedef struct {
    void *pop;
    void *content;
    void *window;
    int behavior;
    int shown;
    int used;
    int laid_out;
    int edge;
    double ax, ay, aw, ah;
    double cw, ch;
    double sw, sh;
    cx_pop_chrome_t chrome;
} cx_pop_slot_t;

static cx_sb_slot_t g_sb[CX_SB_MAX];
static int g_sb_seq = 1;
static cx_pop_slot_t g_pop[CX_POP_MAX];
static int g_policy = CX_ACT_REGULAR;
static int g_policy_explicit;
static int g_active = 1;
static void (*g_pop_hide)(void *pop, void *window);

enum { CX_POP_ARROW = 12, CX_POP_BASE = 22 };

static void pop_hide(cx_pop_slot_t *p) {
    void *win;
    if (!p || !p->shown) return;
    p->shown = 0;
    win = p->window;
    if (g_pop_hide && win) g_pop_hide(p->pop, win);
}

static void copy_title(char *dst, const char *src) {
    int i = 0;
    if (!src) src = "";
    for (; i < CX_TITLE - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

static cx_sb_slot_t *sb_find(const void *item) {
    if (!item) return 0;
    for (int i = 0; i < CX_SB_MAX; i++)
        if (g_sb[i].used && g_sb[i].item == item) return &g_sb[i];
    return 0;
}

static cx_pop_slot_t *pop_slot(void *pop, int create) {
    cx_pop_slot_t *free_slot = 0;
    if (!pop) return 0;
    for (int i = 0; i < CX_POP_MAX; i++) {
        if (g_pop[i].used && g_pop[i].pop == pop) return &g_pop[i];
        if (!g_pop[i].used && !free_slot) free_slot = &g_pop[i];
    }
    if (!create || !free_slot) return 0;
    free_slot->used = 1;
    free_slot->pop = pop;
    free_slot->content = 0;
    free_slot->window = 0;
    free_slot->behavior = CX_POP_APPLICATION;
    free_slot->shown = 0;
    free_slot->laid_out = 0;
    free_slot->cw = 0;
    free_slot->ch = 0;
    return free_slot;
}

int cx_sb_register(void *item, double length) {
    cx_sb_slot_t *s = sb_find(item);
    if (!item) return 0;
    if (s) { s->length = length; return 1; }
    for (int i = 0; i < CX_SB_MAX; i++) {
        if (g_sb[i].used) continue;
        g_sb[i].used = 1;
        g_sb[i].item = item;
        g_sb[i].menu = 0;
        g_sb[i].length = length;
        g_sb[i].visible = 1;
        g_sb[i].menu_open = 0;
        g_sb[i].title[0] = 0;
        g_sb[i].token = g_sb_seq++;
        if (g_sb_seq <= 0) g_sb_seq = 1;
        return 1;
    }
    return 0;
}

int cx_sb_unregister(void *item) {
    cx_sb_slot_t *s = sb_find(item);
    if (!s) return 0;
    s->used = 0;
    s->item = 0;
    s->menu = 0;
    s->menu_open = 0;
    return 1;
}

int cx_sb_count(void) {
    int n = 0;
    for (int i = 0; i < CX_SB_MAX; i++) if (g_sb[i].used) n++;
    return n;
}

int cx_sb_shown_count(void) {
    int n = 0;
    for (int i = 0; i < CX_SB_MAX; i++)
        if (g_sb[i].used && g_sb[i].visible) n++;
    return n;
}

void cx_sb_set_length(void *item, double length) {
    cx_sb_slot_t *s = sb_find(item);
    if (s) s->length = length;
}

double cx_sb_length(const void *item) {
    cx_sb_slot_t *s = sb_find(item);
    return s ? s->length : 0;
}

void cx_sb_set_visible(void *item, int on) {
    cx_sb_slot_t *s = sb_find(item);
    if (!s) return;
    s->visible = on ? 1 : 0;
    if (!s->visible) s->menu_open = 0;
}

int cx_sb_visible(const void *item) {
    cx_sb_slot_t *s = sb_find(item);
    return s ? s->visible : 0;
}

void cx_sb_set_menu(void *item, void *menu) {
    cx_sb_slot_t *s = sb_find(item);
    if (!s) return;
    s->menu = menu;
    if (!menu) s->menu_open = 0;
}

void *cx_sb_menu(const void *item) {
    cx_sb_slot_t *s = sb_find(item);
    return s ? s->menu : 0;
}

void cx_sb_set_title(void *item, const char *title) {
    cx_sb_slot_t *s = sb_find(item);
    if (s) copy_title(s->title, title);
}

const char *cx_sb_title(const void *item) {
    cx_sb_slot_t *s = sb_find(item);
    return s ? s->title : "";
}

int cx_sb_click(void *item) {
    cx_sb_slot_t *s = sb_find(item);
    for (int i = 0; i < CX_SB_MAX; i++)
        if (g_sb[i].used && &g_sb[i] != s) g_sb[i].menu_open = 0;
    if (!s || !s->visible || !s->menu) {
        if (s) s->menu_open = 0;
        return 0;
    }
    s->menu_open = 1;
    return 1;
}

int cx_sb_id(const void *item) {
    cx_sb_slot_t *s = sb_find(item);
    return s ? s->token : 0;
}

void *cx_sb_by_token(int token) {
    int i;
    if (token <= 0) return 0;
    for (i = 0; i < CX_SB_MAX; i++)
        if (g_sb[i].used && g_sb[i].token == token) return g_sb[i].item;
    return 0;
}

int cx_sb_menu_open(const void *item) {
    cx_sb_slot_t *s = sb_find(item);
    return s ? s->menu_open : 0;
}

void cx_pop_set_behavior(void *pop, int behavior) {
    cx_pop_slot_t *p = pop_slot(pop, 1);
    if (!p) return;
    if (behavior < CX_POP_APPLICATION || behavior > CX_POP_SEMTRANSIENT) behavior = CX_POP_APPLICATION;
    p->behavior = behavior;
}

int cx_pop_behavior(const void *pop) {
    cx_pop_slot_t *p = pop_slot((void *)pop, 0);
    return p ? p->behavior : CX_POP_APPLICATION;
}

void cx_pop_set_content(void *pop, void *content) {
    cx_pop_slot_t *p = pop_slot(pop, 1);
    if (p) p->content = content;
}

void *cx_pop_content(const void *pop) {
    cx_pop_slot_t *p = pop_slot((void *)pop, 0);
    return p ? p->content : 0;
}

int cx_pop_is_content(const void *view) {
    if (!view) return 0;
    for (int i = 0; i < CX_POP_MAX; i++)
        if (g_pop[i].used && g_pop[i].shown && g_pop[i].content == view) return 1;
    return 0;
}

void cx_pop_set_anchor(void *pop, double x, double y, double w, double h, int edge) {
    cx_pop_slot_t *p = pop_slot(pop, 1);
    if (!p) return;
    p->ax = x; p->ay = y; p->aw = w > 0 ? w : 1; p->ah = h > 0 ? h : 1;
    if (edge < 0 || edge > 3) edge = 3;
    p->edge = edge;
    p->laid_out = 0;
}

void cx_pop_set_content_size(void *pop, double w, double h) {
    cx_pop_slot_t *p = pop_slot(pop, 1);
    if (!p) return;
    p->cw = w; p->ch = h;
    p->laid_out = 0;
}

void cx_pop_set_screen(void *pop, double w, double h) {
    cx_pop_slot_t *p = pop_slot(pop, 1);
    if (!p) return;
    p->sw = w; p->sh = h;
    p->laid_out = 0;
}

void cx_pop_set_window(void *pop, void *window) {
    cx_pop_slot_t *p = pop_slot(pop, 1);
    if (p) p->window = window;
}

void *cx_pop_window(const void *pop) {
    cx_pop_slot_t *p = pop_slot((void *)pop, 0);
    return p ? p->window : 0;
}

void cx_pop_on_hide(void (*fn)(void *pop, void *window)) { g_pop_hide = fn; }

static int pop_fits(const cx_pop_slot_t *p, double x, double y, double w, double h) {
    if (p->sw <= 0 || p->sh <= 0) return 1;
    return x >= 0 && y >= 0 && x + w <= p->sw && y + h <= p->sh;
}

static void pop_geom(const cx_pop_slot_t *p, int edge, cx_pop_chrome_t *c) {
    double cw = p->cw >= 32 ? p->cw : 160;
    double ch = p->ch >= 32 ? p->ch : 64;
    double ww, wh, wx, wy, tipx, tipy, b0x, b0y, b1x, b1y, cx, cy;
    double mid;
    if (edge == 0 || edge == 2) { ww = cw + CX_POP_ARROW; wh = ch; }
    else { ww = cw; wh = ch + CX_POP_ARROW; }
    if (edge == 3) { wx = p->ax + p->aw / 2 - ww / 2; wy = p->ay + p->ah; }
    else if (edge == 1) { wx = p->ax + p->aw / 2 - ww / 2; wy = p->ay - wh; }
    else if (edge == 0) { wx = p->ax - ww; wy = p->ay + p->ah / 2 - wh / 2; }
    else { wx = p->ax + p->aw; wy = p->ay + p->ah / 2 - wh / 2; }
    if (edge == 3 || edge == 1) {
        mid = p->ax + p->aw / 2;
        if (mid < wx + CX_POP_BASE) mid = wx + CX_POP_BASE;
        if (mid > wx + ww - CX_POP_BASE) mid = wx + ww - CX_POP_BASE;
        tipx = mid - wx;
        if (edge == 3) {
            tipy = 0; cy = CX_POP_ARROW; cx = 0;
            b0x = tipx - CX_POP_BASE / 2; b0y = CX_POP_ARROW;
            b1x = tipx + CX_POP_BASE / 2; b1y = CX_POP_ARROW;
        } else {
            tipy = wh; cy = 0; cx = 0;
            b0x = tipx - CX_POP_BASE / 2; b0y = ch;
            b1x = tipx + CX_POP_BASE / 2; b1y = ch;
        }
    } else {
        mid = p->ay + p->ah / 2;
        if (mid < wy + CX_POP_BASE) mid = wy + CX_POP_BASE;
        if (mid > wy + wh - CX_POP_BASE) mid = wy + wh - CX_POP_BASE;
        tipy = mid - wy;
        if (edge == 0) {
            tipx = ww; cx = 0; cy = 0;
            b0x = cw; b0y = tipy - CX_POP_BASE / 2;
            b1x = cw; b1y = tipy + CX_POP_BASE / 2;
        } else {
            tipx = 0; cx = CX_POP_ARROW; cy = 0;
            b0x = CX_POP_ARROW; b0y = tipy - CX_POP_BASE / 2;
            b1x = CX_POP_ARROW; b1y = tipy + CX_POP_BASE / 2;
        }
    }
    c->win_x = wx; c->win_y = wy; c->win_w = ww; c->win_h = wh;
    c->content_x = cx; c->content_y = cy; c->content_w = cw; c->content_h = ch;
    c->tip_x = tipx; c->tip_y = tipy;
    c->base0_x = b0x; c->base0_y = b0y; c->base1_x = b1x; c->base1_y = b1y;
    c->edge = edge;
}

int cx_pop_layout(void *pop) {
    cx_pop_slot_t *p = pop_slot(pop, 1);
    int edge, flip;
    cx_pop_chrome_t a, b;
    if (!p) return 0;
    edge = p->edge;
    if (edge < 0 || edge > 3) edge = 3;
    pop_geom(p, edge, &a);
    flip = edge ^ 2; /* MinX<->MaxX e MinY<->MaxY: 0^2=2, 1^2=3, 2^2=0, 3^2=1 */
    pop_geom(p, flip, &b);
    if (!pop_fits(p, a.win_x, a.win_y, a.win_w, a.win_h) && pop_fits(p, b.win_x, b.win_y, b.win_w, b.win_h))
        a = b;
    p->chrome = a;
    p->laid_out = 1;
    return 1;
}

int cx_pop_chrome(const void *pop, cx_pop_chrome_t *out) {
    cx_pop_slot_t *p = pop_slot((void *)pop, 0);
    if (!p || !p->laid_out || !out) return 0;
    *out = p->chrome;
    return 1;
}

void cx_pop_show(void *pop) {
    cx_pop_slot_t *p = pop_slot(pop, 1);
    if (!p) return;
    if (!p->laid_out) cx_pop_layout(pop);
    p->shown = 1;
}

void cx_pop_close(void *pop) {
    pop_hide(pop_slot(pop, 0));
}

int cx_pop_shown(const void *pop) {
    cx_pop_slot_t *p = pop_slot((void *)pop, 0);
    return p ? p->shown : 0;
}

void cx_pop_close_transients(void) {
    for (int i = 0; i < CX_POP_MAX; i++)
        if (g_pop[i].used && g_pop[i].shown && g_pop[i].behavior == CX_POP_TRANSIENT)
            pop_hide(&g_pop[i]);
}

void cx_pop_resign_active(void) {
    for (int i = 0; i < CX_POP_MAX; i++) {
        if (!g_pop[i].used || !g_pop[i].shown) continue;
        if (g_pop[i].behavior == CX_POP_TRANSIENT || g_pop[i].behavior == CX_POP_SEMTRANSIENT)
            pop_hide(&g_pop[i]);
    }
}

int cx_act_set_policy(int policy) {
    if (policy != CX_ACT_REGULAR && policy != CX_ACT_ACCESSORY && policy != CX_ACT_PROHIBITED)
        return 0;
    g_policy_explicit = 1;
    g_policy = policy;
    if (policy == CX_ACT_PROHIBITED) {
        g_active = 0;
        cx_pop_resign_active();
    }
    return 1;
}

int cx_act_policy(void) { return g_policy; }
int cx_act_lsuielement(void) { return g_policy == CX_ACT_ACCESSORY; }
int cx_act_hides_dock(void) { return g_policy != CX_ACT_REGULAR; }
int cx_act_note_lsuielement(int present) {
    if (!present || g_policy_explicit) return 0;
    g_policy = CX_ACT_ACCESSORY;
    return 1;
}
int cx_act_active(void) { return g_active; }

int cx_act_activate(void) {
    if (g_policy == CX_ACT_PROHIBITED) return 0;
    g_active = 1;
    return 1;
}

void cx_status_reset(void) {
    for (int i = 0; i < CX_SB_MAX; i++) g_sb[i].used = 0;
    for (int i = 0; i < CX_POP_MAX; i++) g_pop[i].used = 0;
    g_policy = CX_ACT_REGULAR;
    g_policy_explicit = 0;
    g_active = 1;
}
