/* mOS — Pré-Visualização: abre PNG, JPEG, GIF, BMP e PPM; miniaturas das imagens da mesma pasta, zoom
 * (ajustar, tamanho real, roda/⌘+/⌘−), girar, espelhar, marcação (caneta, retângulo, elipse, seta, texto,
 * com cor e espessura), ajustar tamanho, inspetor de informações e exportar como BMP. */
#include "kit.h"

extern app_t app_visualizador;

enum { MK_NONE, MK_PEN, MK_RECT, MK_ELLIPSE, MK_ARROW, MK_TEXT };
#define MAXSTROKE 64
#define MAXPTS 600
typedef struct { int kind; u32 color; float width; int n; float pts[MAXPTS * 2]; char text[80]; } stroke_t;
typedef struct {
    char path[220], dir[220];
    surface_t *img;           /* imagem já com rotações/espelhos aplicados */
    float zoom;               /* 0 = ajustar à janela */
    int panx, pany;
    bool sidebar, info, markup;
    int tool;
    u32 color;
    float width;
    stroke_t *st;
    int nst;
    bool drawing;
    char thumbs_dir[220];
    char files[64][100];
    int nfiles;
    surface_t *thumb[64];
    bool dirty;
    char textbuf[80];
    bool texting;
    float tx, ty;
    int resize_pct;
    bool resize_sheet;
} pv_t;

static surface_t *load_any(const char *path) {
    surface_t *s = kit_load_image(path);
    if (s) return s;
    /* PPM P6 */
    vnode_t *v = vfs_lookup(path);
    if (!v || v->size < 8 || v->data[0] != 'P' || v->data[1] != '6') return NULL;
    const char *p = (const char *)v->data + 2, *end = (const char *)v->data + v->size;
    int vals[3], k = 0;
    while (k < 3 && p < end) {
        while (p < end && isspace((u8)*p)) p++;
        if (*p == '#') { while (p < end && *p != '\n') p++; continue; }
        vals[k++] = (int)strtol(p, (char **)&p, 10);
    }
    p++;
    int w = vals[0], h = vals[1];
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || p + (usize)w * h * 3 > end) return NULL;
    s = surface_new(w, h);
    const u8 *q = (const u8 *)p;
    for (int i = 0; i < w * h; i++, q += 3) s->px[(i / w) * s->stride + i % w] = RGB(q[0], q[1], q[2]);
    return s;
}
static surface_t *rotate(surface_t *src, int dir) {   /* dir 1 = 90° horário, -1 anti-horário */
    surface_t *o = surface_new(src->h, src->w);
    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++) {
            int nx = dir > 0 ? src->h - 1 - y : y, ny = dir > 0 ? x : src->w - 1 - x;
            o->px[(usize)ny * o->stride + nx] = src->px[(usize)y * src->stride + x];
        }
    return o;
}
static void flip_h(surface_t *s) {
    for (int y = 0; y < s->h; y++) for (int x = 0; x < s->w / 2; x++) {
        u32 *a = &s->px[(usize)y * s->stride + x], *b = &s->px[(usize)y * s->stride + s->w - 1 - x], t = *a;
        *a = *b; *b = t;
    }
}
static void flip_v(surface_t *s) {
    for (int y = 0; y < s->h / 2; y++) for (int x = 0; x < s->w; x++) {
        u32 *a = &s->px[(usize)y * s->stride + x], *b = &s->px[(usize)(s->h - 1 - y) * s->stride + x], t = *a;
        *a = *b; *b = t;
    }
}
static void scan_siblings(pv_t *v) {
    if (!strcmp(v->thumbs_dir, v->dir)) return;
    for (int i = 0; i < v->nfiles; i++) if (v->thumb[i]) { surface_free(v->thumb[i]); v->thumb[i] = NULL; }
    snprintf(v->thumbs_dir, sizeof v->thumbs_dir, "%s", v->dir);
    v->nfiles = 0;
    vnode_t *d = vfs_lookup(v->dir);
    for (vnode_t *c = d && d->dir ? d->child : NULL; c && v->nfiles < 64; c = c->next)
        if (!c->dir && (kit_is_image(c->name) || !strcasecmp(vfs_ext(c->name), "ppm"))) snprintf(v->files[v->nfiles++], 100, "%s", c->name);
}
/* aplica a marcação na imagem (pixels de imagem) */
static void stroke_render(surface_t *s, stroke_t *st, float ox, float oy, float sc) {
    float wdt = MAX(1.0f, st->width * sc);
    if (st->n < 1) return;
    float x0 = ox + st->pts[0] * sc, y0 = oy + st->pts[1] * sc;
    float x1 = ox + st->pts[(st->n - 1) * 2] * sc, y1 = oy + st->pts[(st->n - 1) * 2 + 1] * sc;
    switch (st->kind) {
    case MK_PEN:
        for (int i = 1; i < st->n; i++) gfx_line(s, ox + st->pts[i * 2 - 2] * sc, oy + st->pts[i * 2 - 1] * sc, ox + st->pts[i * 2] * sc, oy + st->pts[i * 2 + 1] * sc, wdt, st->color);
        break;
    case MK_RECT:
        gfx_round_rect_stroke(s, MIN(x0, x1), MIN(y0, y1), kit_fabs(x1 - x0), kit_fabs(y1 - y0), 2, wdt, st->color);
        break;
    case MK_ELLIPSE: {
        float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, rx = kit_fabs(x1 - x0) / 2, ry = kit_fabs(y1 - y0) / 2;
        float px = cx + rx, py = cy;
        for (int i = 1; i <= 64; i++) { float a = i * 2 * PI_F / 64, nx = cx + fcos(a) * rx, ny = cy + fsin(a) * ry; gfx_line(s, px, py, nx, ny, wdt, st->color); px = nx; py = ny; }
    } break;
    case MK_ARROW: {
        gfx_line(s, x0, y0, x1, y1, wdt, st->color);
        float dx = x1 - x0, dy = y1 - y0, l = fsqrt(dx * dx + dy * dy);
        if (l > 1) { dx /= l; dy /= l; float hs = 6 * wdt + 6; float t[] = {x1, y1, x1 - dx * hs - dy * hs * 0.5f, y1 - dy * hs + dx * hs * 0.5f, x1 - dx * hs + dy * hs * 0.5f, y1 - dy * hs - dx * hs * 0.5f}; gfx_polygon(s, t, 3, st->color); }
    } break;
    case MK_TEXT: text_draw(s, font_get(FONT_SEMIBOLD), MAX(8.0f, (10 + st->width * 4) * sc), (int)x0, (int)y0, st->color, st->text); break;
    }
}
static void flatten(pv_t *v) {
    if (!v->img || !v->nst) return;
    for (int i = 0; i < v->nst; i++) stroke_render(v->img, &v->st[i], 0, 0, 1);
    v->nst = 0;
}
static void do_resize(pv_t *v, int pct) {
    if (!v->img || pct <= 0 || pct == 100) return;
    flatten(v);
    int nw = MAX(1, v->img->w * pct / 100), nh = MAX(1, v->img->h * pct / 100);
    surface_t *o = surface_new(nw, nh);
    gfx_blit_scaled(o, v->img, 0, 0, nw, nh, 255);
    surface_free(v->img);
    v->img = o;
    v->dirty = true;
}
static void export_bmp(pv_t *v) {
    if (!v->img) return;
    flatten(v);
    char base[220], p[260];
    snprintf(base, sizeof base, "%s", v->path);
    char *dot = strrchr(base, '.');
    if (dot) *dot = 0;
    int k = 1;
    do snprintf(p, sizeof p, k == 1 ? "%s (editada).bmp" : "%s (editada %d).bmp", base, k); while (vfs_lookup(p) && ++k < 100);
    if (kit_save_bmp(p, v->img)) { notify("Imagem exportada", strrchr(p, '/') + 1); v->dirty = false; }
}
static void open_in(window_t *w, pv_t *v, const char *path) {
    if (v->img) surface_free(v->img);
    snprintf(v->path, sizeof v->path, "%s", path);
    snprintf(v->dir, sizeof v->dir, "%s", path);
    char *sl = strrchr(v->dir, '/');
    if (sl) *sl = 0;
    v->img = load_any(path);
    v->zoom = 0; v->panx = v->pany = 0; v->nst = 0; v->dirty = false;
    wm_set_title(w, strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
}

static void g_pen(surface_t *s, int x, int y, int sz, u32 c) { gfx_line(s, x + sz * 0.2f, y + sz * 0.8f, x + sz * 0.8f, y + sz * 0.2f, 2, c); }
static void g_rect(surface_t *s, int x, int y, int sz, u32 c) { gfx_round_rect_stroke(s, x + sz * 0.18f, y + sz * 0.28f, sz * 0.64f, sz * 0.44f, 2, 1.5f, c); }
static void g_ell(surface_t *s, int x, int y, int sz, u32 c) { gfx_circle_stroke(s, x + sz * 0.5f, y + sz * 0.5f, sz * 0.3f, 1.5f, c); }
static void g_arrow(surface_t *s, int x, int y, int sz, u32 c) { gfx_line(s, x + sz * 0.2f, y + sz * 0.8f, x + sz * 0.8f, y + sz * 0.2f, 1.6f, c); gfx_line(s, x + sz * 0.8f, y + sz * 0.2f, x + sz * 0.55f, y + sz * 0.25f, 1.6f, c); gfx_line(s, x + sz * 0.8f, y + sz * 0.2f, x + sz * 0.75f, y + sz * 0.45f, 1.6f, c); }
static void g_text(surface_t *s, int x, int y, int sz, u32 c) { text_draw_center(s, font_get(FONT_SEMIBOLD), sz * 0.7f, rect_make(x, y, sz, sz), c, "A"); }

static const u32 mk_colors[6] = {RGB(255, 59, 48), RGB(255, 204, 0), RGB(48, 199, 89), RGB(10, 132, 255), RGB(0, 0, 0), RGB(255, 255, 255)};

static void paint(window_t *w, ui_t *u) {
    pv_t *v = w->data;
    surface_t *s = u->s;
    int sb = v->sidebar ? 150 : 0;
    w->sidebar_w = sb;
    /* barra de ferramentas */
    int tx = u->cw - 12;
    tx -= 70;
    if (ui_toolbar_button(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 70, 28), "Info", true)) v->info = !v->info;
    tx -= 8 + 100;
    if (ui_button_style(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 100, 28), "Marcação", v->markup ? BTN_TINTED : BTN_GRAY, v->img != NULL, NULL)) { v->markup = !v->markup; if (!v->tool) v->tool = MK_PEN; }
    tx -= 8 + 34;
    if (ui_button_icon(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), glyph_reload, v->img != NULL)) { flatten(v); surface_t *r = rotate(v->img, (u->mods & MOD_ALT) ? -1 : 1); surface_free(v->img); v->img = r; v->dirty = true; }
    ui_tooltip(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), "Girar (⌥ para a esquerda)");
    tx -= 8 + 34;
    if (ui_toolbar_button(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), "+", v->img != NULL)) v->zoom = (v->zoom ? v->zoom : 1) * 1.25f;
    tx -= 4 + 34;
    if (ui_toolbar_button(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), "−", v->img != NULL)) v->zoom = MAX(0.05f, (v->zoom ? v->zoom : 1) / 1.25f);
    tx -= 4 + 64;
    if (ui_toolbar_button(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 64, 28), v->zoom ? "Ajustar" : "100%", v->img != NULL)) { v->zoom = v->zoom ? 0 : 1; v->panx = v->pany = 0; }
    if (ui_button_icon(u, UI_ID, rect_make(96, TOOLBAR_Y(28), 34, 28), glyph_sidebar_toggle, true)) v->sidebar = !v->sidebar;
    /* barra lateral de miniaturas */
    if (sb) {
        scan_siblings(v);
        int y = w->toolbar_h + 8, decoded = 0;
        for (int i = 0; i < v->nfiles; i++) {
            rect_t r = rect_make(25, y, 100, 80);
            char p[330];
            snprintf(p, sizeof p, "%s/%s", v->dir, v->files[i]);
            if (!v->thumb[i] && decoded < 1) {
                surface_t *im = load_any(p);
                decoded++;
                if (im) {
                    float sc = MIN(100.0f / im->w, 80.0f / im->h);
                    int tw = MAX(1, (int)(im->w * sc)), th = MAX(1, (int)(im->h * sc));
                    v->thumb[i] = surface_new(tw, th);
                    gfx_blit_scaled(v->thumb[i], im, 0, 0, tw, th, 255);
                    surface_free(im);
                } else v->thumb[i] = surface_new(1, 1);
            }
            bool cur = !strcmp(p, v->path);
            if (cur) gfx_round_rect(s, r.x - 6, r.y - 4, r.w + 12, r.h + 24, 8, T.sidebar_sel);
            if (v->thumb[i] && v->thumb[i]->w > 1) gfx_blit(s, v->thumb[i], r.x + (100 - v->thumb[i]->w) / 2, r.y + (80 - v->thumb[i]->h) / 2);
            else gfx_round_rect(s, r.x + 20, r.y + 10, 60, 60, 6, T.alt_row);
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 10.5f, r.x, r.y + 94, 100, T.text2, v->files[i]);
            if (ui_clicked(u, 130000 + i, rect_make(r.x - 6, r.y - 4, r.w + 12, r.h + 24)) && !cur) { if (v->dirty) export_bmp(v); open_in(w, v, p); }
            y += 112;
        }
        if (decoded) u->repaint = true;
    }
    int ip = v->info ? 240 : 0;
    rect_t a = rect_make(sb, w->toolbar_h + (v->markup ? 40 : 0), u->cw - sb - ip, u->ch - w->toolbar_h - (v->markup ? 40 : 0));
    gfx_fill(s, sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h, T.dark ? RGB(30, 30, 32) : RGB(236, 236, 238));
    /* barra de marcação */
    if (v->markup) {
        rect_t mb = rect_make(sb, w->toolbar_h, u->cw - sb, 40);
        gfx_fill(s, mb.x, mb.y, mb.w, mb.h, T.window_bg);
        gfx_fill(s, mb.x, mb.y + mb.h - 1, mb.w, 1, T.separator);
        static glyph_fn tg[5] = {g_pen, g_rect, g_ell, g_arrow, g_text};
        for (int i = 0; i < 5; i++) {
            rect_t r = rect_make(mb.x + 12 + i * 38, mb.y + 6, 32, 28);
            if (v->tool == i + 1) gfx_round_rect(s, r.x, r.y, r.w, r.h, 7, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 22));
            tg[i](s, r.x + 6, r.y + 4, 20, v->tool == i + 1 ? T.accent : T.text);
            if (ui_clicked(u, 130100 + i, r)) v->tool = i + 1;
        }
        for (int i = 0; i < 6; i++) {
            rect_t r = rect_make(mb.x + 220 + i * 24, mb.y + 11, 18, 18);
            gfx_circle(s, r.x + 9, r.y + 9, 9, mk_colors[i]);
            gfx_circle_stroke(s, r.x + 9, r.y + 9, 9, 1, RGBA(0, 0, 0, 60));
            if (v->color == mk_colors[i]) gfx_circle_stroke(s, r.x + 9, r.y + 9, 11.5f, 1.5f, T.accent);
            if (ui_clicked(u, 130200 + i, r)) v->color = mk_colors[i];
        }
        ui_label(u, mb.x + 380, mb.y + 12, "Espessura", FONT_REGULAR, 12, T.text2);
        ui_slider(u, UI_ID, rect_make(mb.x + 446, mb.y + 9, 100, 22), &v->width, 1, 12);
        if (ui_button_style(u, UI_ID, rect_make(mb.x + 560, mb.y + 6, 90, 28), "Desfazer", BTN_GRAY, v->nst > 0, NULL)) v->nst--;
        if (ui_button_style(u, UI_ID, rect_make(mb.x + 658, mb.y + 6, 130, 28), "Ajustar Tamanho…", BTN_GRAY, true, NULL)) { v->resize_sheet = true; v->resize_pct = 50; }
    }
    if (!v->img) { ui_label_center(u, a, "Não foi possível abrir esta imagem.", FONT_SEMIBOLD, 15, T.text2); return; }
    float sc = v->zoom ? v->zoom : MIN(1.0f, MIN((float)(a.w - 40) / v->img->w, (float)(a.h - 40) / v->img->h));
    int dw = (int)(v->img->w * sc), dh = (int)(v->img->h * sc);
    int ox = a.x + (a.w - dw) / 2 + v->panx, oy = a.y + (a.h - dh) / 2 + v->pany;
    rect_t pc = surface_push_clip(s, a);
    gfx_shadow(s, ox, oy, dw, dh, 2, 14, 60, 3);
    gfx_blit_scaled(s, v->img, ox, oy, dw, dh, 255);
    for (int i = 0; i < v->nst; i++) stroke_render(s, &v->st[i], (float)ox, (float)oy, sc);
    if (v->texting) { text_draw(s, font_get(FONT_SEMIBOLD), MAX(8.0f, (10 + v->width * 4) * sc), (int)(ox + v->tx * sc), (int)(oy + v->ty * sc), v->color, v->textbuf); gfx_fill(s, (int)(ox + v->tx * sc + text_width(font_get(FONT_SEMIBOLD), MAX(8.0f, (10 + v->width * 4) * sc), v->textbuf)), (int)(oy + v->ty * sc - 16), 1, 18, v->color); }
    surface_pop_clip(s, pc);
    /* interação: marcação ou arrastar (com zoom) */
    float ix = (u->mx - ox) / sc, iy = (u->my - oy) / sc;
    if (v->markup && ui_hit(u, a)) {
        u->cursor = CURSOR_CROSS;
        if (u->pressed && v->tool == MK_TEXT) {
            if (v->texting && v->textbuf[0] && v->nst < MAXSTROKE) { stroke_t *st = &v->st[v->nst++]; memset(st, 0, sizeof *st); st->kind = MK_TEXT; st->color = v->color; st->width = v->width; st->n = 1; st->pts[0] = v->tx; st->pts[1] = v->ty; snprintf(st->text, 80, "%s", v->textbuf); v->dirty = true; }
            v->texting = true; v->textbuf[0] = 0; v->tx = ix; v->ty = iy;
        } else if (u->pressed && v->nst < MAXSTROKE) {
            stroke_t *st = &v->st[v->nst++];
            memset(st, 0, sizeof *st);
            st->kind = v->tool; st->color = v->color; st->width = v->width;
            st->pts[0] = ix; st->pts[1] = iy; st->n = 1;
            v->drawing = true;
        }
    } else if (u->pressed && ui_hit(u, a) && v->zoom) { v->drawing = false; }
    if (v->drawing && v->nst) {
        stroke_t *st = &v->st[v->nst - 1];
        if (!u->down) { v->drawing = false; v->dirty = true; if (st->n < 2) v->nst--; }
        else if (st->kind == MK_PEN) { if (st->n < MAXPTS) { st->pts[st->n * 2] = ix; st->pts[st->n * 2 + 1] = iy; st->n++; } }
        else { st->pts[2] = ix; st->pts[3] = iy; st->n = 2; }
        u->repaint = true;
    }
    if (v->texting && u->key) {
        if (u->key == KEY_ENTER || u->key == KEY_ESC) {
            if (u->key == KEY_ENTER && v->textbuf[0] && v->nst < MAXSTROKE) { stroke_t *st = &v->st[v->nst++]; memset(st, 0, sizeof *st); st->kind = MK_TEXT; st->color = v->color; st->width = v->width; st->n = 1; st->pts[0] = v->tx; st->pts[1] = v->ty; snprintf(st->text, 80, "%s", v->textbuf); v->dirty = true; }
            v->texting = false;
        } else if (u->key == KEY_BACKSPACE) { int l = (int)strlen(v->textbuf); if (l) v->textbuf[utf8_prev(v->textbuf, l)] = 0; }
        else if (u->text[0] && strlen(v->textbuf) + strlen(u->text) < 79) strcat(v->textbuf, u->text);
        u->key_used = true;
    }
    if (!v->markup && v->zoom && ui_hit(u, a)) { static int lx2, ly2; if (u->down) { v->panx += u->mx - lx2; v->pany += u->my - ly2; u->cursor = CURSOR_GRABBING; } lx2 = u->mx; ly2 = u->my; }
    if (u->wheel && ui_hit(u, a) && (u->mods & MOD_CMD)) { v->zoom = CLAMP((v->zoom ? v->zoom : sc) * (u->wheel > 0 ? 1.15f : 1 / 1.15f), 0.05f, 16.0f); u->wheel = 0; }
    else if (u->wheel && ui_hit(u, a) && v->zoom) { v->pany += u->wheel * 40; u->wheel = 0; }
    /* inspetor */
    if (ip) {
        rect_t r = rect_make(u->cw - ip, w->toolbar_h, ip, u->ch - w->toolbar_h);
        gfx_fill(s, r.x, r.y, r.w, r.h, T.content_bg);
        gfx_fill(s, r.x, r.y, 1, r.h, T.separator);
        int y = r.y + 16;
        ui_label(u, r.x + 16, y, "Informações", FONT_BOLD, 15, T.text);
        y += 32;
        vnode_t *f = vfs_lookup(v->path);
        char t[96];
        struct { const char *k; char v[96]; } rows[5];
        snprintf(rows[0].v, 96, "%s", strrchr(v->path, '/') + 1); rows[0].k = "Nome";
        snprintf(rows[1].v, 96, "%d × %d pixels", v->img->w, v->img->h); rows[1].k = "Dimensões";
        snprintf(rows[2].v, 96, "%u KB", f ? (unsigned)(f->size / 1024) : 0); rows[2].k = "Tamanho do arquivo";
        snprintf(rows[3].v, 96, "%s", vfs_ext(v->path)); rows[3].k = "Formato";
        snprintf(rows[4].v, 96, "%d%%", (int)(sc * 100 + 0.5f)); rows[4].k = "Zoom";
        for (int i = 0; i < 5; i++) {
            ui_label(u, r.x + 16, y, rows[i].k, FONT_REGULAR, 11.5f, T.text2);
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 13, r.x + 16, y + 30, r.w - 32, T.text, rows[i].v);
            y += 46;
        }
        (void)t;
        if (ui_button_style(u, UI_ID, rect_make(r.x + 16, y, r.w - 32, 28), "Espelhar na Horizontal", BTN_GRAY, true, NULL)) { flatten(v); flip_h(v->img); v->dirty = true; }
        y += 34;
        if (ui_button_style(u, UI_ID, rect_make(r.x + 16, y, r.w - 32, 28), "Espelhar na Vertical", BTN_GRAY, true, NULL)) { flatten(v); flip_v(v->img); v->dirty = true; }
        y += 34;
        if (ui_button_style(u, UI_ID, rect_make(r.x + 16, y, r.w - 32, 28), "Exportar como BMP…", BTN_FILLED, true, NULL)) export_bmp(v);
    }
    /* folha de ajustar tamanho */
    if (v->resize_sheet) {
        rect_t r = ui_sheet(u, 320, 170, w->toolbar_h);
        ui_label_center(u, rect_make(r.x, r.y + 16, r.w, 20), "Ajustar Tamanho", FONT_BOLD, 15, T.text);
        char t[64];
        snprintf(t, sizeof t, "%d%% — %d × %d pixels", v->resize_pct, v->img->w * v->resize_pct / 100, v->img->h * v->resize_pct / 100);
        ui_label_center(u, rect_make(r.x, r.y + 48, r.w, 18), t, FONT_REGULAR, 13, T.text2);
        ui_stepper(u, UI_ID, rect_make(r.x + r.w / 2 - 50, r.y + 76, 100, 28), &v->resize_pct, 5, 400);
        if (ui_button_style(u, UI_ID, rect_make(r.x + r.w - 20 - 90, r.y + r.h - 44, 90, 28), "OK", BTN_FILLED | BTN_DEFAULT, true, NULL)) { do_resize(v, v->resize_pct); v->resize_sheet = false; }
        if (ui_button_style(u, UI_ID, rect_make(r.x + r.w - 20 - 90 - 8 - 90, r.y + r.h - 44, 90, 28), "Cancelar", BTN_GRAY, true, NULL)) v->resize_sheet = false;
        ui_sheet_end(u);
    }
    /* teclado */
    if (u->key && !v->texting && !u->focus) {
        if ((u->mods & MOD_CMD) && (u->key == '+' || u->key == '=')) { v->zoom = (v->zoom ? v->zoom : sc) * 1.25f; u->key_used = true; }
        else if ((u->mods & MOD_CMD) && u->key == '-') { v->zoom = MAX(0.05f, (v->zoom ? v->zoom : sc) / 1.25f); u->key_used = true; }
        else if ((u->mods & MOD_CMD) && u->key == '0') { v->zoom = 1; v->panx = v->pany = 0; u->key_used = true; }
        else if ((u->mods & MOD_CMD) && u->key == '9') { v->zoom = 0; v->panx = v->pany = 0; u->key_used = true; }
        else if ((u->mods & MOD_CMD) && u->key == 'z' && v->nst) { v->nst--; u->key_used = true; }
        else if ((u->mods & MOD_CMD) && u->key == 's') { export_bmp(v); u->key_used = true; }
        else if (!(u->mods & MOD_CMD) && (u->key == KEY_LEFT || u->key == KEY_RIGHT) && !v->markup) {
            scan_siblings(v);
            int cur = -1;
            for (int i = 0; i < v->nfiles; i++) { char p[330]; snprintf(p, sizeof p, "%s/%s", v->dir, v->files[i]); if (!strcmp(p, v->path)) cur = i; }
            int nx = cur + (u->key == KEY_RIGHT ? 1 : -1);
            if (cur >= 0 && nx >= 0 && nx < v->nfiles) { char p[330]; snprintf(p, sizeof p, "%s/%s", v->dir, v->files[nx]); open_in(w, v, p); }
            u->key_used = true;
        }
    }
}

static void on_close(window_t *w) {
    pv_t *v = w->data;
    if (!v) return;
    if (v->img) surface_free(v->img);
    for (int i = 0; i < v->nfiles; i++) if (v->thumb[i]) surface_free(v->thumb[i]);
    kfree(v->st);
    kfree(v);
}

void viewer_open_file(const char *path) {
    pv_t *v = kcalloc(1, sizeof(pv_t));
    if (!v) return;
    v->st = kcalloc(MAXSTROKE, sizeof(stroke_t));
    v->color = mk_colors[0];
    v->width = 3;
    surface_t *probe = load_any(path);
    int w = probe ? MIN(MAX(probe->w + 40, 520), 1100) : 600, h = probe ? MIN(MAX(probe->h + 100, 400), 760) : 420;
    if (probe) surface_free(probe);
    window_t *win = wm_create(&app_visualizador, "", -1, -1, w, h, WIN_RESIZABLE);
    if (!win) { kfree(v->st); kfree(v); return; }
    app_visualizador.running = true;
    win->data = v;
    win->paint = paint;
    win->on_close = on_close;
    win->toolbar_h = TOOLBAR_H;
    win->min_w = 480;
    win->min_h = 360;
    win->title_x = -1;
    open_in(win, v, path);
}

static void launch(app_t *a) {
    /* sem documento: abre a imagem mais recente de Imagens, ou o Arquivos nessa pasta */
    char d[200];
    snprintf(d, sizeof d, "%s/Imagens", CUR_HOME);
    vnode_t *dir = vfs_lookup(d), *best = NULL;
    for (vnode_t *c = dir && dir->dir ? dir->child : NULL; c; c = c->next) if (!c->dir && kit_is_image(c->name) && (!best || c->mtime > best->mtime)) best = c;
    if (best) { char p[330]; snprintf(p, sizeof p, "%s/%s", d, best->name); viewer_open_file(p); }
    else { a->running = false; finder_open_path(d); }
}

app_t app_visualizador = {.name = "Pré-Visualização", .exec = "visualizador", .draw_icon = icon_visualizador, .launch = launch};
