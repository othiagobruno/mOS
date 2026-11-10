/* Lista de status items e o estado de visibilidade/selo do Dock. Sem desenho. */
#ifdef MOS_SB_HOST
#include <string.h>
#include <stdlib.h>
typedef unsigned char u8;
#else
#include "kernel.h"
#endif
#include "statusdock.h"
#include "composite.h"

#define SB_MAX 32
#define DOCK_MAX 64

typedef struct {
    int used;
    int pid;
    u32 id;
    i32 length;
    int visible;
    int open;
    int iw, ih;
    u32 image[SB_IMG_MAX * SB_IMG_MAX];
    char title[SB_TITLE_MAX];
    int nitems;
    u32 action[SB_MENU_MAX];
    int disabled[SB_MENU_MAX];
    int separator[SB_MENU_MAX];
    char label[SB_MENU_MAX][SB_LABEL_MAX];
} sb_slot_t;

typedef struct {
    int used;
    int pid;
    int hidden;
    int badge_n;
    char badge[16];
} dock_tile_t;

static sb_slot_t slots[SB_MAX];
static sb_geom_t geoms[SB_MAX];
static int ngeoms, geom_h;
static dock_tile_t tiles[DOCK_MAX];

static int rd_u32(const u8 *b, u32 n, u32 *o, u32 *out) {
    if (*o + 4 > n) return 0;
    memcpy(out, b + *o, 4);
    *o += 4;
    return 1;
}

static sb_slot_t *sb_find(int pid, u32 id, int make) {
    int i, free_i = -1;
    for (i = 0; i < SB_MAX; i++) {
        if (slots[i].used && slots[i].pid == pid && slots[i].id == id) return &slots[i];
        if (!slots[i].used && free_i < 0) free_i = i;
    }
    if (!make || free_i < 0) return 0;
    memset(&slots[free_i], 0, sizeof slots[free_i]);
    slots[free_i].used = 1;
    slots[free_i].pid = pid;
    slots[free_i].id = id;
    slots[free_i].visible = 1;
    slots[free_i].length = -1;
    return &slots[free_i];
}

static void copy_cap(char *dst, int cap, const char *src, int n) {
    int i = 0;
    if (n > cap - 1) n = cap - 1;
    if (n < 0) n = 0;
    for (; i < n; i++) dst[i] = src[i];
    dst[i] = 0;
}

static void sb_tmp_free(sb_slot_t *t) {
#ifdef MOS_SB_HOST
    free(t);
#else
    kfree(t);
#endif
}

int sb_upsert_wire(int pid, const void *buf, u32 len) {
    const u8 *b = buf;
    u32 o = 0, id = 0, flags = 0, iw = 0, ih = 0, nitems = 0, tlen = 0;
    u32 length_u = 0;
    i32 length;
    sb_slot_t *tmp, *s;
    int k, rc = -1;
    /* a imagem cabe em 16 KiB: fora da pilha do kernel (32 KiB) */
#ifdef MOS_SB_HOST
    tmp = (sb_slot_t *)calloc(1, sizeof *tmp);
#else
    tmp = (sb_slot_t *)kmalloc(sizeof *tmp);
#endif
    if (!tmp) return -1;
    if (!b || pid <= 0) goto out;
    if (!rd_u32(b, len, &o, &id) || !rd_u32(b, len, &o, &length_u)) goto out;
    length = (i32)length_u;
    if (!rd_u32(b, len, &o, &flags) || !rd_u32(b, len, &o, &iw) || !rd_u32(b, len, &o, &ih)) goto out;
    if (!rd_u32(b, len, &o, &nitems) || !rd_u32(b, len, &o, &tlen)) goto out;
    if (id == 0 || tlen > 4096 || nitems > SB_MENU_MAX) goto out;
    if (iw > SB_IMG_MAX || ih > SB_IMG_MAX) goto out;
    if (o + tlen > len) goto out;
    memset(tmp, 0, sizeof *tmp);
    tmp->used = 1;
    tmp->pid = pid;
    tmp->id = id;
    tmp->length = length;
    tmp->visible = (flags & 1) ? 1 : 0;
    copy_cap(tmp->title, SB_TITLE_MAX, (const char *)b + o, (int)tlen);
    o += tlen;
    for (k = 0; k < (int)nitems; k++) {
        u32 act = 0, mf = 0, llen = 0;
        if (!rd_u32(b, len, &o, &act) || !rd_u32(b, len, &o, &mf) || !rd_u32(b, len, &o, &llen)) goto out;
        if (llen > 4096 || o + llen > len) goto out;
        tmp->action[k] = act;
        tmp->disabled[k] = (mf & 1) ? 1 : 0;
        tmp->separator[k] = (mf & 2) ? 1 : 0;
        copy_cap(tmp->label[k], SB_LABEL_MAX, (const char *)b + o, (int)llen);
        o += llen;
    }
    tmp->nitems = (int)nitems;
    if (iw && ih) {
        u32 need = iw * ih * 4;
        u32 i;
        if (o + need > len) goto out;
        tmp->iw = (int)iw;
        tmp->ih = (int)ih;
        for (i = 0; i < iw * ih; i++) {
            u32 px;
            memcpy(&px, b + o + i * 4, 4);
            tmp->image[i] = gui_premul(px);
        }
    }
    s = sb_find(pid, id, 1);
    if (!s) goto out;
    tmp->open = s->open && tmp->visible;
    *s = *tmp;
    rc = 0;
out:
    sb_tmp_free(tmp);
    return rc;
}

void sb_remove(int pid, u32 id) {
    sb_slot_t *s = sb_find(pid, id, 0);
    if (s) memset(s, 0, sizeof *s);
}

void sb_remove_pid(int pid) {
    int i;
    for (i = 0; i < SB_MAX; i++)
        if (slots[i].used && slots[i].pid == pid) memset(&slots[i], 0, sizeof slots[i]);
}

void sb_reset(void) { memset(slots, 0, sizeof slots); ngeoms = 0; }

int sb_count(void) {
    int i, n = 0;
    for (i = 0; i < SB_MAX; i++) if (slots[i].used) n++;
    return n;
}

static int default_tw(const char *s, int n, void *ctx) {
    (void)s;
    (void)ctx;
    return n * 8;
}

static int item_width(const sb_slot_t *s, int bar_h, int (*text_w)(const char *, int, void *), void *ctx) {
    int side, w, n;
    if (!s->visible) return 0;
    side = bar_h - 8;
    if (side < 14) side = 14;
    if (s->length == -2) return bar_h > 4 ? bar_h - 4 : bar_h;
    w = 12;
    if (s->iw > 0) w += (s->iw < side ? s->iw : side) + 4;
    n = 0;
    while (s->title[n] && n < SB_TITLE_MAX) n++;
    if (n) w += text_w(s->title, n, ctx) + 8;
    if (s->length > 0) w = s->length;
    if (w < side) w = side;
    if (w > 280) w = 280;
    return w;
}

int sb_layout(int screen_w, int bar_h, int right_x, int (*text_w)(const char *, int, void *), void *ctx) {
    int order[SB_MAX], n = 0, i, x;
    if (!text_w) text_w = default_tw;
    if (bar_h < 8) bar_h = 8;
    if (right_x > screen_w) right_x = screen_w;
    for (i = 0; i < SB_MAX; i++)
        if (slots[i].used && slots[i].visible) order[n++] = i;
    x = right_x - 6;
    ngeoms = 0;
    geom_h = bar_h;
    for (i = 0; i < n; i++) {
        sb_slot_t *s = &slots[order[i]];
        int w = item_width(s, bar_h, text_w, ctx);
        int side = bar_h - 10;
        sb_geom_t *g;
        if (side < 12) side = 12;
        if (x - w < 8) break;
        x -= w;
        g = &geoms[ngeoms++];
        memset(g, 0, sizeof *g);
        g->x = x;
        g->w = w;
        g->y = 0;
        g->h = bar_h;
        g->open = s->open;
        g->pid = s->pid;
        g->id = s->id;
        g->title_x = -1;
        memcpy(g->title, s->title, SB_TITLE_MAX);
        if (s->iw > 0 && s->ih > 0) {
            int dw = s->iw < side ? s->iw : side;
            int dh = s->ih < side ? s->ih : side;
            if (s->length == -2) {
                g->img_w = dw;
                g->img_h = dh;
                g->img_x = g->x + (w - dw) / 2;
                g->img_y = (bar_h - dh) / 2;
            } else {
                g->img_w = dw;
                g->img_h = dh;
                g->img_x = g->x + 6;
                g->img_y = (bar_h - dh) / 2;
                if (s->title[0]) g->title_x = g->img_x + dw + 4;
            }
        } else if (s->title[0] && s->length != -2) {
            g->title_x = g->x + 8;
        } else if (s->title[0]) {
            g->title_x = g->x + 4;
        }
        x -= 2;
    }
    return ngeoms;
}

const sb_geom_t *sb_geoms(int *n) {
    if (n) *n = ngeoms;
    return geoms;
}

static int geom_at(int x, int y) {
    int i;
    if (y < 0 || y >= geom_h) return -1;
    for (i = 0; i < ngeoms; i++)
        if (x >= geoms[i].x && x < geoms[i].x + geoms[i].w) return i;
    return -1;
}

int sb_hit(int x, int y, int *pid, u32 *id) {
    int i = geom_at(x, y);
    if (i < 0) return 0;
    if (pid) *pid = geoms[i].pid;
    if (id) *id = geoms[i].id;
    return 1;
}

static void close_all(void) {
    int i;
    for (i = 0; i < SB_MAX; i++) slots[i].open = 0;
    for (i = 0; i < ngeoms; i++) geoms[i].open = 0;
}

int sb_click(int x, int y, int *pid, u32 *id) {
    int gi = geom_at(x, y);
    sb_slot_t *s;
    if (gi < 0) return 0;
    s = sb_find(geoms[gi].pid, geoms[gi].id, 0);
    if (!s) return 0;
    if (s->open) {
        close_all();
        if (pid) *pid = s->pid;
        if (id) *id = s->id;
        return 2;
    }
    close_all();
    s->open = 1;
    geoms[gi].open = 1;
    if (pid) *pid = s->pid;
    if (id) *id = s->id;
    return 1;
}

void sb_close_open(void) { close_all(); }

int sb_is_open(int pid, u32 id) {
    sb_slot_t *s = sb_find(pid, id, 0);
    return s && s->open;
}

const u32 *sb_image(int pid, u32 id, int *w, int *h) {
    sb_slot_t *s = sb_find(pid, id, 0);
    if (!s || s->iw <= 0 || s->ih <= 0) return 0;
    if (w) *w = s->iw;
    if (h) *h = s->ih;
    return s->image;
}

int sb_menu_count(int pid, u32 id) {
    sb_slot_t *s = sb_find(pid, id, 0);
    return s ? s->nitems : 0;
}

const char *sb_menu_label(int pid, u32 id, int index, u32 *action, int *disabled, int *separator) {
    sb_slot_t *s = sb_find(pid, id, 0);
    if (!s || index < 0 || index >= s->nitems) return 0;
    if (action) *action = s->action[index];
    if (disabled) *disabled = s->disabled[index];
    if (separator) *separator = s->separator[index];
    return s->separator[index] ? "" : s->label[index];
}

static dock_tile_t *tile_find(int pid, int make) {
    int i, free_i = -1;
    if (pid <= 0) return 0;
    for (i = 0; i < DOCK_MAX; i++) {
        if (tiles[i].used && tiles[i].pid == pid) return &tiles[i];
        if (!tiles[i].used && free_i < 0) free_i = i;
    }
    if (!make || free_i < 0) return 0;
    memset(&tiles[free_i], 0, sizeof tiles[free_i]);
    tiles[free_i].used = 1;
    tiles[free_i].pid = pid;
    return &tiles[free_i];
}

int dock_tile_set_visible(int pid, int visible) {
    dock_tile_t *t = tile_find(pid, 1);
    if (!t) return -1;
    t->hidden = visible ? 0 : 1;
    return 0;
}

int dock_tile_is_hidden(int pid) {
    dock_tile_t *t = tile_find(pid, 0);
    return t && t->hidden;
}

static int parse_int(const char *s, int *out) {
    int n = 0, sign = 1, any = 0;
    if (!s || !s[0]) return 0;
    if (*s == '+') s++;
    else if (*s == '-') { sign = -1; s++; }
    if (!*s) return 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return 0;
        any = 1;
        if (n > 100000) return 0;
        n = n * 10 + (*s - '0');
    }
    if (!any) return 0;
    *out = sign < 0 ? -n : n;
    return 1;
}

int dock_tile_set_badge(int pid, const char *text) {
    dock_tile_t *t = tile_find(pid, 1);
    int n = 0, i;
    if (!t) return -1;
    if (!text) text = "";
    for (i = 0; i < 15 && text[i]; i++) t->badge[i] = text[i];
    t->badge[i] = 0;
    if (!t->badge[0]) t->badge_n = 0;
    else if (parse_int(t->badge, &n) && n > 0) t->badge_n = n;
    else t->badge_n = 0;
    return t->badge_n;
}

int dock_tile_badge_n(int pid) {
    dock_tile_t *t = tile_find(pid, 0);
    return t ? t->badge_n : 0;
}

const char *dock_tile_badge_text(int pid) {
    dock_tile_t *t = tile_find(pid, 0);
    return t ? t->badge : "";
}

void dock_tile_remove(int pid) {
    dock_tile_t *t = tile_find(pid, 0);
    if (t) memset(t, 0, sizeof *t);
}

void dock_tile_reset(void) { memset(tiles, 0, sizeof tiles); }

int dock_badge_paint(int n, const char *text, char *out, int cap) {
    int i;
    if (!out || cap < 2) return 0;
    out[0] = 0;
    if (text && text[0]) {
        for (i = 0; i < cap - 1 && text[i]; i++) out[i] = text[i];
        out[i] = 0;
        return 1;
    }
    if (n <= 0) return 0;
    if (n > 999) {
        if (cap < 5) return 0;
        out[0] = '9'; out[1] = '9'; out[2] = '9'; out[3] = '+'; out[4] = 0;
        return 1;
    }
    {
        char tmp[8];
        int k = 0, v = n;
        if (!v) tmp[k++] = '0';
        while (v && k < 7) { tmp[k++] = (char)('0' + v % 10); v /= 10; }
        if (k >= cap) return 0;
        for (i = 0; i < k; i++) out[i] = tmp[k - 1 - i];
        out[k] = 0;
    }
    return 1;
}
