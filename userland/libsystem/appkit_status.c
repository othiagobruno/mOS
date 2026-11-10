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
    char title[CX_TITLE];
} cx_sb_slot_t;

typedef struct {
    void *pop;
    void *content;
    int behavior;
    int shown;
    int used;
} cx_pop_slot_t;

static cx_sb_slot_t g_sb[CX_SB_MAX];
static cx_pop_slot_t g_pop[CX_POP_MAX];
static int g_policy = CX_ACT_REGULAR;
static int g_active = 1;

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
    free_slot->behavior = CX_POP_APPLICATION;
    free_slot->shown = 0;
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

void cx_pop_show(void *pop) {
    cx_pop_slot_t *p = pop_slot(pop, 1);
    if (p) p->shown = 1;
}

void cx_pop_close(void *pop) {
    cx_pop_slot_t *p = pop_slot(pop, 0);
    if (p) p->shown = 0;
}

int cx_pop_shown(const void *pop) {
    cx_pop_slot_t *p = pop_slot((void *)pop, 0);
    return p ? p->shown : 0;
}

void cx_pop_close_transients(void) {
    for (int i = 0; i < CX_POP_MAX; i++)
        if (g_pop[i].used && g_pop[i].shown && g_pop[i].behavior == CX_POP_TRANSIENT)
            g_pop[i].shown = 0;
}

void cx_pop_resign_active(void) {
    for (int i = 0; i < CX_POP_MAX; i++) {
        if (!g_pop[i].used || !g_pop[i].shown) continue;
        if (g_pop[i].behavior == CX_POP_TRANSIENT || g_pop[i].behavior == CX_POP_SEMTRANSIENT)
            g_pop[i].shown = 0;
    }
}

int cx_act_set_policy(int policy) {
    if (policy != CX_ACT_REGULAR && policy != CX_ACT_ACCESSORY && policy != CX_ACT_PROHIBITED)
        return 0;
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
    g_active = 1;
}
