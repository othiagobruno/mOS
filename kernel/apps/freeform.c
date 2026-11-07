/* mOS — Freeform: quadros infinitos com notas adesivas, formas (retângulo, elipse, triângulo, losango),
 * texto, desenho à mão livre e linhas; selecionar, mover, redimensionar, mudar cor, editar texto (clique
 * duplo), apagar, duplicar, trazer para frente; zoom e deslocamento. Vários quadros em ~/Biblioteca/Freeform */
#include "kit.h"

extern app_t app_freeform;

enum { K_NOTE, K_RECT, K_ELLIPSE, K_TRI, K_DIAMOND, K_TEXT, K_PEN, K_LINE };
enum { TL_SELECT, TL_NOTE, TL_SHAPE, TL_TEXT, TL_PEN, TL_LINE };
#define MAXO 400
#define MAXPTS 512
typedef struct {
    bool used;
    int kind;
    float x, y, w, h;           /* coordenadas do quadro */
    u32 color;
    char text[240];
    float *pts;                 /* caneta: pares x,y relativos a (x,y) */
    int npts;
} obj_t;

static const u32 note_colors[6] = {RGB(255, 236, 140), RGB(255, 196, 160), RGB(190, 236, 170), RGB(170, 214, 255), RGB(220, 196, 255), RGB(255, 190, 210)};
static const u32 ink_colors[8] = {RGB(30, 30, 32), RGB(10, 132, 255), RGB(255, 69, 58), RGB(48, 199, 89), RGB(255, 159, 10), RGB(175, 82, 222), RGB(142, 142, 147), RGB(255, 255, 255)};

static struct {
    window_t *w;
    char boards[32][48];
    int nb, cur;
    obj_t o[MAXO];
    int sel, tool, shape;
    float ox, oy, zoom;         /* deslocamento e zoom da vista */
    int drag;                   /* 0 nada, 1 mover, 2 redimensionar, 3 criar, 4 deslocar vista, 5 caneta */
    float dx, dy, sx, sy, sw, sh;
    bool editing;
    textarea_t *ta;
    int edit_obj;
    u32 color_note, color_ink;
    bool renaming, loaded, dirty;
    char rename_buf[48];
    u64 last_save;
} F = {.sel = -1, .zoom = 1, .color_note = RGB(255, 236, 140), .color_ink = RGB(30, 30, 32), .edit_obj = -1};

/* ------------------------------------------------------------ arquivos */
static void board_path(int i, char *out, int n) {
    char f[80];
    snprintf(f, sizeof f, "%s.freeform", F.boards[i]);
    kit_lib("Freeform", f, out, n);
}
static void clear_objs(void) {
    for (int i = 0; i < MAXO; i++) { if (F.o[i].pts) kfree(F.o[i].pts); memset(&F.o[i], 0, sizeof F.o[i]); }
    F.sel = -1;
}
static void save_board(void) {
    if (F.cur < 0 || F.cur >= F.nb) return;
    sbuf_t b = {0};
    sb_printf(&b, "# mOS Freeform 1\nV\t%d\t%d\t%d\n", (int)F.ox, (int)F.oy, (int)(F.zoom * 100));
    for (int i = 0; i < MAXO; i++) {
        obj_t *o = &F.o[i];
        if (!o->used) continue;
        sb_printf(&b, "O\t%d\t%d\t%d\t%d\t%d\t%u\t", o->kind, (int)o->x, (int)o->y, (int)o->w, (int)o->h, o->color);
        kit_esc(&b, o->text);
        sb_putc(&b, '\t');
        for (int k = 0; k < o->npts; k++) sb_printf(&b, "%s%d,%d", k ? ";" : "", (int)o->pts[k * 2], (int)o->pts[k * 2 + 1]);
        sb_putc(&b, '\n');
    }
    char p[200];
    board_path(F.cur, p, sizeof p);
    kit_save(p, &b);
    F.dirty = false;
}
static obj_t *obj_new(int kind) {
    for (int i = 0; i < MAXO; i++) if (!F.o[i].used) { memset(&F.o[i], 0, sizeof F.o[i]); F.o[i].used = true; F.o[i].kind = kind; return &F.o[i]; }
    return NULL;
}
static void load_board(int i) {
    clear_objs();
    F.cur = i;
    F.ox = F.oy = 0; F.zoom = 1;
    char p[200];
    board_path(i, p, sizeof p);
    char *buf = kit_read(p, NULL);
    if (!buf) return;
    char *cur = buf, *ln, *f[10];
    while ((ln = kit_line(&cur))) {
        int n = kit_split(ln, f, 10);
        if (!strcmp(f[0], "V") && n >= 4) { F.ox = (float)atoi(f[1]); F.oy = (float)atoi(f[2]); F.zoom = atoi(f[3]) / 100.0f; if (F.zoom < 0.1f) F.zoom = 1; }
        else if (!strcmp(f[0], "O") && n >= 8) {
            obj_t *o = obj_new(atoi(f[1]));
            if (!o) break;
            o->x = (float)atoi(f[2]); o->y = (float)atoi(f[3]); o->w = (float)atoi(f[4]); o->h = (float)atoi(f[5]);
            o->color = (u32)strtol(f[6], NULL, 10);
            kit_unesc(f[7]);
            snprintf(o->text, sizeof o->text, "%s", f[7]);
            if (n >= 9 && f[8][0]) {
                o->pts = kmalloc(sizeof(float) * 2 * MAXPTS);
                char *q = f[8];
                while (*q && o->npts < MAXPTS) {
                    o->pts[o->npts * 2] = (float)strtol(q, &q, 10);
                    if (*q == ',') q++;
                    o->pts[o->npts * 2 + 1] = (float)strtol(q, &q, 10);
                    o->npts++;
                    if (*q == ';') q++;
                    else break;
                }
            }
        }
    }
    kfree(buf);
}
static void load_list(void) {
    F.nb = 0;
    char d[200];
    kit_lib("Freeform", "x", d, sizeof d);
    *strrchr(d, '/') = 0;
    vnode_t *v = vfs_lookup(d);
    for (vnode_t *c = v && v->dir ? v->child : NULL; c && F.nb < 32; c = c->next) {
        usize l = strlen(c->name);
        if (l > 9 && !strcmp(c->name + l - 9, ".freeform")) { snprintf(F.boards[F.nb], 48, "%.*s", (int)(l - 9), c->name); F.nb++; }
    }
    if (!F.nb) {
        snprintf(F.boards[0], 48, "Ideias");
        F.nb = 1;
        F.cur = 0;
        clear_objs();
        obj_t *o = obj_new(K_NOTE); o->x = 80; o->y = 80; o->w = 180; o->h = 180; o->color = note_colors[0];
        snprintf(o->text, sizeof o->text, "Bem-vindo ao Freeform!\n\nClique duas vezes para editar.");
        o = obj_new(K_NOTE); o->x = 300; o->y = 120; o->w = 180; o->h = 180; o->color = note_colors[3];
        snprintf(o->text, sizeof o->text, "Use a barra de ferramentas para criar notas, formas, texto e desenhos.");
        o = obj_new(K_ELLIPSE); o->x = 540; o->y = 140; o->w = 160; o->h = 110; o->color = RGB(255, 159, 10);
        snprintf(o->text, sizeof o->text, "Formas");
        o = obj_new(K_LINE); o->x = 480; o->y = 210; o->w = 60; o->h = -10; o->color = RGB(30, 30, 32);
        save_board();
    }
}

/* ------------------------------------------------------------ geometria */
static float sx_(float x) { return (x - F.ox) * F.zoom; }
static float sy_(float y) { return (y - F.oy) * F.zoom; }
static float bx_(float x) { return x / F.zoom + F.ox; }
static float by_(float y) { return y / F.zoom + F.oy; }
static rect_t obj_screen(obj_t *o, int ax, int ay) {
    float x0 = MIN(o->x, o->x + o->w), y0 = MIN(o->y, o->y + o->h);
    return rect_make(ax + (int)sx_(x0), ay + (int)sy_(y0), (int)(kit_fabs(o->w) * F.zoom), (int)(kit_fabs(o->h) * F.zoom));
}
static bool obj_hit(obj_t *o, float bx, float by) {
    float x0 = MIN(o->x, o->x + o->w), y0 = MIN(o->y, o->y + o->h), x1 = MAX(o->x, o->x + o->w), y1 = MAX(o->y, o->y + o->h);
    float pad = (o->kind == K_LINE || o->kind == K_PEN) ? 8 / F.zoom : 0;
    if (o->kind == K_LINE) {
        float lx = o->w, ly = o->h, len2 = lx * lx + ly * ly;
        float t = len2 ? ((bx - o->x) * lx + (by - o->y) * ly) / len2 : 0;
        t = CLAMP(t, 0.0f, 1.0f);
        float px = o->x + t * lx - bx, py = o->y + t * ly - by;
        return px * px + py * py < pad * pad;
    }
    return bx >= x0 - pad && bx <= x1 + pad && by >= y0 - pad && by <= y1 + pad;
}

/* ------------------------------------------------------------ desenho dos objetos */
static void draw_obj(surface_t *s, obj_t *o, int ax, int ay, bool sel) {
    rect_t r = obj_screen(o, ax, ay);
    float z = F.zoom;
    switch (o->kind) {
    case K_NOTE:
        gfx_shadow(s, r.x, r.y, r.w, r.h, 2, 8 * z, 50, (int)(3 * z));
        gfx_fill(s, r.x, r.y, r.w, r.h, o->color);
        break;
    case K_RECT: gfx_round_rect(s, r.x, r.y, r.w, r.h, 8 * z, o->color); break;
    case K_ELLIPSE: {
        /* elipse por varredura de linhas */
        float cx = r.x + r.w / 2.0f, cy = r.y + r.h / 2.0f, rx = r.w / 2.0f, ry = r.h / 2.0f;
        for (int yy = r.y; yy < r.y + r.h; yy++) {
            float dy = (yy + 0.5f - cy) / ry, k = 1 - dy * dy;
            if (k <= 0) continue;
            float hw = rx * fsqrt(k);
            gfx_fill(s, (int)(cx - hw), yy, (int)(2 * hw), 1, o->color);
        }
    } break;
    case K_TRI: { float t[] = {r.x + r.w / 2.0f, (float)r.y, (float)r.x + r.w, (float)r.y + r.h, (float)r.x, (float)r.y + r.h}; gfx_polygon(s, t, 3, o->color); } break;
    case K_DIAMOND: { float t[] = {r.x + r.w / 2.0f, (float)r.y, (float)r.x + r.w, r.y + r.h / 2.0f, r.x + r.w / 2.0f, (float)r.y + r.h, (float)r.x, r.y + r.h / 2.0f}; gfx_polygon(s, t, 4, o->color); } break;
    case K_LINE: {
        float x0 = ax + sx_(o->x), y0 = ay + sy_(o->y), x1 = ax + sx_(o->x + o->w), y1 = ay + sy_(o->y + o->h);
        gfx_line(s, x0, y0, x1, y1, MAX(1.5f, 3 * z), o->color);
        /* ponta de seta */
        float dx = x1 - x0, dy = y1 - y0, l = fsqrt(dx * dx + dy * dy);
        if (l > 1) {
            dx /= l; dy /= l;
            float hs = 12 * z;
            float t[] = {x1, y1, x1 - dx * hs - dy * hs * 0.5f, y1 - dy * hs + dx * hs * 0.5f, x1 - dx * hs + dy * hs * 0.5f, y1 - dy * hs - dx * hs * 0.5f};
            gfx_polygon(s, t, 3, o->color);
        }
    } break;
    case K_PEN:
        for (int k = 1; k < o->npts; k++)
            gfx_line(s, ax + sx_(o->x + o->pts[k * 2 - 2]), ay + sy_(o->y + o->pts[k * 2 - 1]), ax + sx_(o->x + o->pts[k * 2]), ay + sy_(o->y + o->pts[k * 2 + 1]), MAX(1.5f, 3 * z), o->color);
        break;
    case K_TEXT: break;
    }
    if (o->text[0] && !(F.editing && F.edit_obj == (int)(o - F.o))) {
        bool dark_bg = o->kind != K_NOTE && o->kind != K_TEXT && (C_R(o->color) * 3 + C_G(o->color) * 6 + C_B(o->color)) / 10 < 150;
        u32 tc = o->kind == K_TEXT ? o->color : dark_bg ? RGB(255, 255, 255) : RGB(30, 30, 32);
        float fs = (o->kind == K_TEXT ? 22 : 15) * z;
        if (fs >= 5) {
            rect_t tr = rect_inset(r, (int)(10 * z));
            rect_t pc = surface_push_clip(s, r);
            if (o->kind == K_NOTE || o->kind == K_TEXT) text_wrap(s, font_get(o->kind == K_TEXT ? FONT_SEMIBOLD : FONT_REGULAR), fs, tr, (int)(fs * 1.35f), tc, o->text, true);
            else if (!strchr(o->text, '\n') && text_width(font_get(FONT_SEMIBOLD), fs, o->text) <= tr.w) {
                text_draw_center(s, font_get(FONT_SEMIBOLD), fs, r, tc, o->text);
            } else {
                int th = text_wrap(s, font_get(FONT_SEMIBOLD), fs, tr, (int)(fs * 1.35f), tc, o->text, false);
                text_wrap(s, font_get(FONT_SEMIBOLD), fs, rect_make(tr.x, r.y + (r.h - th) / 2, tr.w, th + 4), (int)(fs * 1.35f), tc, o->text, true);
            }
            surface_pop_clip(s, pc);
        }
    }
    if (sel) {
        if (o->kind == K_LINE) {
            gfx_circle(s, ax + sx_(o->x), ay + sy_(o->y), 5, T.accent);
            gfx_circle(s, ax + sx_(o->x + o->w), ay + sy_(o->y + o->h), 5, T.accent);
        } else {
            gfx_round_rect_stroke(s, r.x - 3, r.y - 3, r.w + 6, r.h + 6, 4, 1.5f, T.accent);
            gfx_circle(s, r.x + r.w + 3, r.y + r.h + 3, 6, RGB(255, 255, 255));
            gfx_circle_stroke(s, r.x + r.w + 3, r.y + r.h + 3, 6, 1.5f, T.accent);
        }
    }
}

/* ------------------------------------------------------------ ações */
static void delete_sel(void) {
    if (F.sel < 0) return;
    obj_t *o = &F.o[F.sel];
    if (o->pts) kfree(o->pts);
    memset(o, 0, sizeof *o);
    F.sel = -1;
    F.dirty = true;
}
static void duplicate_sel(void) {
    if (F.sel < 0) return;
    obj_t *src = &F.o[F.sel], *o = obj_new(src->kind);
    if (!o) return;
    float *pts = o->pts;
    *o = *src;
    o->pts = pts;
    if (src->pts) { o->pts = kmalloc(sizeof(float) * 2 * MAXPTS); memcpy(o->pts, src->pts, sizeof(float) * 2 * src->npts); }
    o->x += 24; o->y += 24;
    F.sel = (int)(o - F.o);
    F.dirty = true;
}
static void bring_front(void) {
    if (F.sel < 0) return;
    for (int i = MAXO - 1; i > F.sel; i--)
        if (!F.o[i].used) { F.o[i] = F.o[F.sel]; memset(&F.o[F.sel], 0, sizeof F.o[0]); F.sel = i; F.dirty = true; return; }
}
static void begin_edit(int idx) {
    if (idx < 0 || F.o[idx].kind == K_LINE || F.o[idx].kind == K_PEN) return;
    if (!F.ta) F.ta = textarea_new(15, 1, RGB(30, 30, 32), 8);
    F.edit_obj = idx;
    F.editing = true;
    textarea_set(F.ta, F.o[idx].text, -1);
    textarea_focus(F.ta, true);
}
static void end_edit(void) {
    if (!F.editing) return;
    if (F.edit_obj >= 0 && F.o[F.edit_obj].used) {
        int n;
        const char *t = textarea_text(F.ta, &n);
        n = MIN(n, (int)sizeof F.o[0].text - 1);
        memcpy(F.o[F.edit_obj].text, t, n);
        F.o[F.edit_obj].text[n] = 0;
        if (F.o[F.edit_obj].kind == K_TEXT && !n) { F.sel = F.edit_obj; delete_sel(); }
    }
    F.editing = false;
    F.edit_obj = -1;
    F.dirty = true;
}

/* ------------------------------------------------------------ desenho da janela */
static void g_note(surface_t *s, int x, int y, int sz, u32 c) { gfx_round_rect_stroke(s, x + sz * 0.18f, y + sz * 0.18f, sz * 0.64f, sz * 0.64f, 2, 1.5f, c); gfx_line(s, x + sz * 0.6f, y + sz * 0.82f, x + sz * 0.82f, y + sz * 0.6f, 1.5f, c); }
static void g_shape(surface_t *s, int x, int y, int sz, u32 c) { gfx_circle_stroke(s, x + sz * 0.62f, y + sz * 0.4f, sz * 0.22f, 1.5f, c); gfx_round_rect_stroke(s, x + sz * 0.16f, y + sz * 0.46f, sz * 0.36f, sz * 0.36f, 2, 1.5f, c); }
static void g_text(surface_t *s, int x, int y, int sz, u32 c) { text_draw_center(s, font_get(FONT_SEMIBOLD), sz * 0.7f, rect_make(x, y, sz, sz), c, "A"); }
static void g_pen(surface_t *s, int x, int y, int sz, u32 c) { gfx_line(s, x + sz * 0.2f, y + sz * 0.8f, x + sz * 0.78f, y + sz * 0.22f, 2, c); gfx_line(s, x + sz * 0.2f, y + sz * 0.8f, x + sz * 0.3f, y + sz * 0.62f, 2, c); }
static void g_line(surface_t *s, int x, int y, int sz, u32 c) { gfx_line(s, x + sz * 0.2f, y + sz * 0.8f, x + sz * 0.8f, y + sz * 0.2f, 1.6f, c); gfx_line(s, x + sz * 0.8f, y + sz * 0.2f, x + sz * 0.56f, y + sz * 0.26f, 1.6f, c); gfx_line(s, x + sz * 0.8f, y + sz * 0.2f, x + sz * 0.74f, y + sz * 0.44f, 1.6f, c); }
static void g_cursor(surface_t *s, int x, int y, int sz, u32 c) { float t[] = {x + sz * 0.3f, y + sz * 0.16f, x + sz * 0.74f, y + sz * 0.56f, x + sz * 0.5f, y + sz * 0.58f, x + sz * 0.36f, y + sz * 0.84f}; gfx_polygon(s, t, 4, c); }

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    if (!F.loaded) { load_list(); load_board(0); F.loaded = true; }
    int sb = w->sidebar_w;
    /* barra lateral: quadros */
    int y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Todos os Quadros", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    for (int i = 0; i < F.nb; i++) {
        rect_t r = rect_make(10, y, sb - 20, 28);
        if (F.renaming && F.cur == i) {
            if (ui_textfield(u, 112900, rect_make(r.x + 4, r.y + 2, r.w - 8, 24), F.rename_buf, sizeof F.rename_buf, "Nome do quadro")) {
                if (F.rename_buf[0] && !strchr(F.rename_buf, '/')) {
                    char a[200], b[200];
                    board_path(i, a, sizeof a);
                    snprintf(F.boards[i], 48, "%s", F.rename_buf);
                    board_path(i, b, sizeof b);
                    vnode_t *v = vfs_lookup(a);
                    if (v) vfs_move(v, v->parent, strrchr(b, '/') + 1);
                    save_board();
                }
                F.renaming = false;
            } else if (u->focus != 112900) u->focus = 112900;
        } else if (ui_sidebar_item(u, 112000 + i, r, F.boards[i], F.cur == i, glyph_sb_document)) {
            if (F.cur == i && u->dblclick) { F.renaming = true; snprintf(F.rename_buf, 48, "%s", F.boards[i]); }
            else if (F.cur != i) { end_edit(); if (F.dirty) save_board(); load_board(i); }
        }
        y += 30;
    }
    if (ui_button_style(u, UI_ID, rect_make(10, u->ch - 36, 130, 28), "Novo Quadro", BTN_PLAIN_GRAY, F.nb < 32, glyph_plus)) {
        end_edit();
        if (F.dirty) save_board();
        int k = F.nb + 1;
        do snprintf(F.boards[F.nb], 48, "Quadro %d", k++); while (0);
        F.nb++;
        clear_objs();
        F.cur = F.nb - 1;
        F.ox = F.oy = 0; F.zoom = 1;
        save_board();
        F.renaming = true;
        snprintf(F.rename_buf, 48, "%s", F.boards[F.cur]);
    }
    /* barra de ferramentas */
    static glyph_fn tg[6] = {g_cursor, g_note, g_shape, g_text, g_pen, g_line};
    static const char *tn[6] = {"Selecionar", "Nota Adesiva", "Forma", "Caixa de Texto", "Desenhar", "Linha"};
    int tx = sb + (u->cw - sb) / 2 - 3 * 40;
    ui_tool_group(u, rect_make(tx - 4, TOOLBAR_Y(32), 6 * 40 + 8, 32));
    for (int i = 0; i < 6; i++) {
        rect_t r = rect_make(tx + i * 40, TOOLBAR_Y(28), 36, 28);
        if (F.tool == i) gfx_round_rect(s, r.x, r.y, r.w, r.h, 8, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 22));
        tg[i](s, r.x + 8, r.y + 4, 20, F.tool == i ? T.accent : T.text);
        ui_tooltip(u, 112100 + i, r, tn[i]);
        if (ui_clicked(u, 112200 + i, r)) { F.tool = i; end_edit(); }
    }
    if (F.tool == TL_SHAPE) {
        static const char *sh[4] = {"Retângulo", "Elipse", "Triângulo", "Losango"};
        F.shape = ui_popup(u, UI_ID, rect_make(tx + 6 * 40 + 14, TOOLBAR_Y(28), 112, 28), sh, 4, F.shape);
    }
    /* cores: notas ou tinta conforme a seleção/ferramenta */
    bool note_ctx = (F.sel >= 0 && F.o[F.sel].kind == K_NOTE) || (F.sel < 0 && F.tool == TL_NOTE);
    int cx = u->cw - 12 - (note_ctx ? 6 : 8) * 22;
    for (int i = 0; i < (note_ctx ? 6 : 8); i++) {
        u32 c = note_ctx ? note_colors[i] : ink_colors[i];
        rect_t r = rect_make(cx + i * 22, TOOLBAR_Y(18), 18, 18);
        gfx_circle(s, r.x + 9, r.y + 9, 9, c);
        gfx_circle_stroke(s, r.x + 9, r.y + 9, 9, 1, RGBA(0, 0, 0, 50));
        bool cur = F.sel >= 0 ? F.o[F.sel].color == c : (note_ctx ? F.color_note : F.color_ink) == c;
        if (cur) gfx_circle_stroke(s, r.x + 9, r.y + 9, 11.5f, 1.5f, T.accent);
        if (ui_clicked(u, 112300 + i, r)) {
            if (note_ctx) F.color_note = c; else F.color_ink = c;
            if (F.sel >= 0) { F.o[F.sel].color = c; F.dirty = true; }
        }
    }
    /* quadro */
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.dark ? RGB(30, 30, 32) : RGB(250, 250, 250));
    /* grade de pontos */
    float step = 24 * F.zoom;
    if (step >= 8) {
        float gx0 = a.x - kit_fmod(F.ox * F.zoom, step), gy0 = a.y - kit_fmod(F.oy * F.zoom, step);
        u32 dc = T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 30);
        for (float gy = gy0; gy < a.y + a.h; gy += step)
            for (float gx = gx0; gx < a.x + a.w; gx += step) gfx_fill(s, (int)gx, (int)gy, 2, 2, dc);
    }
    rect_t pc = surface_push_clip(s, a);
    for (int i = 0; i < MAXO; i++) if (F.o[i].used) draw_obj(s, &F.o[i], a.x, a.y, i == F.sel);
    /* edição de texto sobre o objeto */
    if (F.editing && F.edit_obj >= 0) {
        obj_t *o = &F.o[F.edit_obj];
        rect_t r = obj_screen(o, a.x, a.y);
        if (o->kind == K_TEXT) gfx_round_rect_stroke(s, r.x - 2, r.y - 2, r.w + 4, r.h + 4, 4, 1, T.accent);
        bool dark_bg = o->kind != K_NOTE && o->kind != K_TEXT && (C_R(o->color) * 3 + C_G(o->color) * 6 + C_B(o->color)) / 10 < 150;
        textarea_colors(F.ta, 1, o->kind == K_TEXT ? o->color : dark_bg ? RGB(255, 255, 255) : RGB(30, 30, 32));
        textarea_set_size(F.ta, (o->kind == K_TEXT ? 22 : 15) * F.zoom);
        textarea_paint(w, u, F.ta, rect_inset(r, 2));
    }
    surface_pop_clip(s, pc);
    /* mouse */
    float mbx = bx_(u->mx - a.x), mby = by_(u->my - a.y);
    bool in = ui_hit(u, a) && !(F.editing && F.edit_obj >= 0 && rect_contains(obj_screen(&F.o[F.edit_obj], a.x, a.y), u->mx, u->my));
    if (in && u->pressed) {
        if (F.editing) end_edit();
        bool space_pan = u->rpressed;
        if (F.tool == TL_SELECT && !space_pan) {
            /* alça de redimensionar do selecionado */
            if (F.sel >= 0 && F.o[F.sel].kind != K_LINE) {
                rect_t r = obj_screen(&F.o[F.sel], a.x, a.y);
                if (kit_fabs(u->mx - (r.x + r.w + 3)) < 9 && kit_fabs(u->my - (r.y + r.h + 3)) < 9) { F.drag = 2; F.sw = F.o[F.sel].w; F.sh = F.o[F.sel].h; F.dx = mbx; F.dy = mby; goto pressed_done; }
            }
            int hit = -1;
            for (int i = MAXO - 1; i >= 0; i--) if (F.o[i].used && obj_hit(&F.o[i], mbx, mby)) { hit = i; break; }
            if (hit >= 0) {
                if (u->dblclick && hit == F.sel) begin_edit(hit);
                F.sel = hit; F.drag = 1; F.dx = mbx; F.dy = mby; F.sx = F.o[hit].x; F.sy = F.o[hit].y;
            } else { F.sel = -1; F.drag = 4; F.dx = (float)u->mx; F.dy = (float)u->my; F.sx = F.ox; F.sy = F.oy; }
        } else if (F.tool == TL_PEN) {
            obj_t *o = obj_new(K_PEN);
            if (o) {
                o->x = mbx; o->y = mby; o->color = F.color_ink;
                o->pts = kmalloc(sizeof(float) * 2 * MAXPTS);
                o->pts[0] = o->pts[1] = 0; o->npts = 1;
                F.sel = (int)(o - F.o); F.drag = 5;
            }
        } else if (F.tool == TL_LINE) {
            obj_t *o = obj_new(K_LINE);
            if (o) { o->x = mbx; o->y = mby; o->color = F.color_ink; F.sel = (int)(o - F.o); F.drag = 3; }
        } else {
            int kind = F.tool == TL_NOTE ? K_NOTE : F.tool == TL_TEXT ? K_TEXT : K_RECT + F.shape;
            obj_t *o = obj_new(kind);
            if (o) {
                o->x = mbx; o->y = mby;
                o->w = kind == K_NOTE ? 180 : kind == K_TEXT ? 220 : 140;
                o->h = kind == K_NOTE ? 180 : kind == K_TEXT ? 44 : 100;
                o->x -= o->w / 2; o->y -= o->h / 2;
                o->color = kind == K_NOTE ? F.color_note : kind == K_TEXT ? F.color_ink : (F.color_ink == RGB(30, 30, 32) ? RGB(10, 132, 255) : F.color_ink);
                F.sel = (int)(o - F.o);
                F.tool = TL_SELECT;
                F.dirty = true;
                if (kind == K_NOTE || kind == K_TEXT) begin_edit(F.sel);
            }
        }
    }
pressed_done:
    if (F.drag && u->down) {
        if (F.drag == 1 && F.sel >= 0) { F.o[F.sel].x = F.sx + mbx - F.dx; F.o[F.sel].y = F.sy + mby - F.dy; u->cursor = CURSOR_MOVE; }
        else if (F.drag == 2 && F.sel >= 0) { F.o[F.sel].w = MAX(30.0f, F.sw + mbx - F.dx); F.o[F.sel].h = MAX(20.0f, F.sh + mby - F.dy); }
        else if (F.drag == 3 && F.sel >= 0) { F.o[F.sel].w = mbx - F.o[F.sel].x; F.o[F.sel].h = mby - F.o[F.sel].y; }
        else if (F.drag == 4) { F.ox = F.sx - (u->mx - F.dx) / F.zoom; F.oy = F.sy - (u->my - F.dy) / F.zoom; u->cursor = CURSOR_GRABBING; }
        else if (F.drag == 5 && F.sel >= 0) {
            obj_t *o = &F.o[F.sel];
            float px = mbx - o->x, py = mby - o->y;
            float lx = o->pts[(o->npts - 1) * 2], ly = o->pts[(o->npts - 1) * 2 + 1];
            if ((px - lx) * (px - lx) + (py - ly) * (py - ly) > 9 && o->npts < MAXPTS) { o->pts[o->npts * 2] = px; o->pts[o->npts * 2 + 1] = py; o->npts++; }
        }
        u->repaint = true;
    } else if (F.drag) {
        if (F.drag == 5 && F.sel >= 0) {
            /* normaliza a caixa do traço */
            obj_t *o = &F.o[F.sel];
            float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
            for (int k = 0; k < o->npts; k++) { mnx = MIN(mnx, o->pts[k * 2]); mny = MIN(mny, o->pts[k * 2 + 1]); mxx = MAX(mxx, o->pts[k * 2]); mxy = MAX(mxy, o->pts[k * 2 + 1]); }
            for (int k = 0; k < o->npts; k++) { o->pts[k * 2] -= mnx; o->pts[k * 2 + 1] -= mny; }
            o->x += mnx; o->y += mny; o->w = MAX(4.0f, mxx - mnx); o->h = MAX(4.0f, mxy - mny);
            if (o->npts < 2) delete_sel();
        }
        if (F.drag == 3 && F.sel >= 0 && kit_fabs(F.o[F.sel].w) + kit_fabs(F.o[F.sel].h) < 6) delete_sel();
        if (F.drag != 4) F.dirty = true;
        F.drag = 0;
    }
    if (in && u->wheel) {
        float nz = CLAMP(F.zoom * (u->wheel > 0 ? 1.1f : 1 / 1.1f), 0.2f, 4.0f);
        /* zoom em torno do ponteiro */
        F.ox = mbx - (u->mx - a.x) / nz;
        F.oy = mby - (u->my - a.y) / nz;
        F.zoom = nz;
        u->wheel = 0;
    }
    /* zoom no rodapé */
    char zl[16];
    snprintf(zl, sizeof zl, "%d%%", (int)(F.zoom * 100 + 0.5f));
    rect_t zb = rect_make(a.x + a.w - 120, a.y + a.h - 40, 108, 28);
    glass_panel(s, zb.x, zb.y, zb.w, zb.h, 14, GLASS_CONTROL);
    if (ui_clicked(u, UI_ID, rect_make(zb.x, zb.y, 30, 28))) F.zoom = MAX(0.2f, F.zoom / 1.25f);
    if (ui_clicked(u, UI_ID, rect_make(zb.x + zb.w - 30, zb.y, 30, 28))) F.zoom = MIN(4.0f, F.zoom * 1.25f);
    text_draw_center(s, font_get(FONT_SEMIBOLD), 15, rect_make(zb.x, zb.y, 30, 28), T.text, "−");
    text_draw_center(s, font_get(FONT_SEMIBOLD), 15, rect_make(zb.x + zb.w - 30, zb.y, 30, 28), T.text, "+");
    text_draw_center(s, font_get(FONT_MEDIUM), 12, rect_make(zb.x + 30, zb.y, zb.w - 60, 28), T.text, zl);
    /* teclado */
    if (u->key && !u->focus && !F.editing) {
        if ((u->key == KEY_BACKSPACE || u->key == KEY_DELETE) && F.sel >= 0) { delete_sel(); u->key_used = true; }
        else if (u->key == KEY_ENTER && F.sel >= 0) { begin_edit(F.sel); u->key_used = true; }
        else if (u->key == KEY_ESC) { F.sel = -1; F.tool = TL_SELECT; u->key_used = true; }
        else if ((u->mods & MOD_CMD) && u->key == 'd') { duplicate_sel(); u->key_used = true; }
        else if (!(u->mods & MOD_CMD) && (u->key == 'n' || u->key == 's' || u->key == 't' || u->key == 'p' || u->key == 'l' || u->key == 'v')) {
            F.tool = u->key == 'n' ? TL_NOTE : u->key == 's' ? TL_SHAPE : u->key == 't' ? TL_TEXT : u->key == 'p' ? TL_PEN : u->key == 'l' ? TL_LINE : TL_SELECT;
            u->key_used = true;
        }
    }
    if (F.editing && u->key == KEY_ESC) { end_edit(); u->key_used = true; }
    if (F.dirty && timer_ticks() - F.last_save > 1500 && !F.drag) { save_board(); F.last_save = timer_ticks(); }
}

static void on_close(window_t *w) { end_edit(); save_board(); F.w = NULL; }
static void launch(app_t *a) {
    if (F.w) { wm_focus(F.w); return; }
    F.w = wm_create(&app_freeform, "Freeform", -1, -1, 1060, 700, WIN_RESIZABLE);
    if (!F.w) return;
    F.w->paint = paint;
    F.w->on_close = on_close;
    F.w->toolbar_h = TOOLBAR_H;
    F.w->sidebar_w = 220;
    F.w->min_w = 760;
    F.w->min_h = 480;
    F.w->title_x = -1;
}

enum { M_DEL = 1, M_DUP, M_FRONT, M_NEWB, M_DELB };
static menu_item_t m_edit[] = {{"Duplicar", "⌘D", M_DUP}, {"Trazer para a Frente", NULL, M_FRONT}, {NULL}, {"Apagar", "⌫", M_DEL}};
static menu_item_t m_file[] = {{"Novo Quadro", "⌘N", M_NEWB}, {"Apagar Quadro", NULL, M_DELB}};
static menu_t menus[] = {{"Arquivo", m_file, 2}, {"Editar", m_edit, 4}};
static void on_menu(app_t *a, int act) {
    if (act == M_DEL) delete_sel();
    else if (act == M_DUP) duplicate_sel();
    else if (act == M_FRONT) bring_front();
    else if (act == M_NEWB && F.nb < 32) {
        end_edit(); save_board();
        snprintf(F.boards[F.nb], 48, "Quadro %d", F.nb + 1);
        F.nb++;
        clear_objs();
        F.cur = F.nb - 1;
        save_board();
    } else if (act == M_DELB && F.nb > 1) {
        char p[200];
        board_path(F.cur, p, sizeof p);
        vnode_t *v = vfs_lookup(p);
        if (v) vfs_delete(v);
        memmove(F.boards[F.cur], F.boards[F.cur + 1], sizeof F.boards[0] * (F.nb - F.cur - 1));
        F.nb--;
        load_board(0);
    }
    if (F.w) wm_invalidate(F.w);
}

static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(255, 255, 255), RGB(236, 236, 240));
    gfx_fill(s, (int)(x + 14 * u), (int)(y + 14 * u), (int)(18 * u), (int)(18 * u), RGB(255, 214, 90));
    gfx_circle(s, x + 42 * u, y + 24 * u, 9 * u, RGB(10, 132, 255));
    float pts[] = {14, 48, 22, 40, 30, 46, 38, 38, 48, 44};
    for (int i = 0; i < 4; i++) gfx_line(s, x + pts[i * 2] * u, y + pts[i * 2 + 1] * u, x + pts[i * 2 + 2] * u, y + pts[i * 2 + 3] * u, 2.4f * u, RGB(255, 69, 58));
}

app_t app_freeform = {.name = "Freeform", .exec = "freeform", .draw_icon = icon, .launch = launch, .on_menu = on_menu,
                      .menus = menus, .nmenus = 2, .pinned = true};
