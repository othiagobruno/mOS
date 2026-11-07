/* mOS — Fotos: biblioteca (Imagens, Mesa e Downloads), favoritos, capturas de tela, álbuns próprios,
 * grade com zoom, visualizador com navegação, informações, girar, apagar e edição (luz, contraste,
 * cor, nitidez de filtros: Vívido, Dramático, Mono, Sépia, Noir) salva como cópia.
 * Favoritos e álbuns em ~/Biblioteca/Fotos/dados.tsv */
#include "kit.h"

extern app_t app_fotos;
#define MAXP 400
#define MAXALB 16

typedef struct { char path[200], name[96]; u64 mtime; usize size; surface_t *thumb; bool fav, failed; u32 albums; } photo_t;
static struct {
    window_t *w;
    photo_t p[MAXP];
    int n;
    char alb[MAXALB][40];
    int nalb;
    int view;                 /* 0 biblioteca, 1 favoritos, 2 capturas, 3 recentes, 10+ álbum */
    int sel, scroll, zoom;    /* zoom: tamanho da miniatura */
    bool viewer, info, edit;
    surface_t *full;          /* imagem aberta no visualizador */
    int full_for;
    /* edição */
    float light, contrast, color;
    int filter, rot;
    surface_t *edited;
    bool dirty_edit;
    bool loaded, renaming;
    char rename_buf[40];
    char search[64];
} P = {.sel = -1, .zoom = 150, .full_for = -1};

static const char *filters[6] = {"Original", "Vívido", "Dramático", "Mono", "Sépia", "Noir"};

/* ------------------------------------------------------------ dados */
static void save(void) {
    sbuf_t b = {0};
    sb_puts(&b, "# mOS Fotos 1\n");
    for (int i = 0; i < P.nalb; i++) { char nm[40]; kit_field(nm, 40, P.alb[i]); sb_printf(&b, "A\t%s\n", nm); }
    for (int i = 0; i < P.n; i++) if (P.p[i].fav || P.p[i].albums) sb_printf(&b, "P\t%d\t%u\t%s\n", P.p[i].fav, P.p[i].albums, P.p[i].path);
    char p[200];
    kit_save(kit_lib("Fotos", "dados.tsv", p, sizeof p), &b);
}
static void scan_dir(const char *path, int depth) {
    vnode_t *d = vfs_lookup(path);
    if (!d || !d->dir || depth > 4) return;
    for (vnode_t *c = d->child; c && P.n < MAXP; c = c->next) {
        char sub[220];
        snprintf(sub, sizeof sub, "%s/%s", path, c->name);
        if (c->dir) { if (c->name[0] != '.') scan_dir(sub, depth + 1); continue; }
        if (!kit_is_image(c->name)) continue;
        bool dup = false;
        for (int i = 0; i < P.n; i++) if (!strcmp(P.p[i].path, sub)) { dup = true; break; }
        if (dup) continue;
        photo_t *ph = &P.p[P.n++];
        memset(ph, 0, sizeof *ph);
        snprintf(ph->path, sizeof ph->path, "%s", sub);
        snprintf(ph->name, sizeof ph->name, "%s", c->name);
        ph->mtime = c->mtime;
        ph->size = c->size;
    }
}
static void load(void) {
    for (int i = 0; i < P.n; i++) if (P.p[i].thumb) surface_free(P.p[i].thumb);
    P.n = 0;
    char d[200];
    snprintf(d, sizeof d, "%s/Imagens", CUR_HOME); scan_dir(d, 0);
    snprintf(d, sizeof d, "%s/Mesa", CUR_HOME); scan_dir(d, 0);
    snprintf(d, sizeof d, "%s/Downloads", CUR_HOME); scan_dir(d, 0);
    /* mais recentes primeiro */
    for (int i = 1; i < P.n; i++) { photo_t t = P.p[i]; int j = i - 1; while (j >= 0 && P.p[j].mtime < t.mtime) { P.p[j + 1] = P.p[j]; j--; } P.p[j + 1] = t; }
    char p[200];
    char *buf = kit_read(kit_lib("Fotos", "dados.tsv", p, sizeof p), NULL);
    P.nalb = 0;
    if (buf) {
        char *cur = buf, *ln, *f[5];
        while ((ln = kit_line(&cur))) {
            int n = kit_split(ln, f, 5);
            if (!strcmp(f[0], "A") && n >= 2 && P.nalb < MAXALB) snprintf(P.alb[P.nalb++], 40, "%s", f[1]);
            else if (!strcmp(f[0], "P") && n >= 4)
                for (int i = 0; i < P.n; i++) if (!strcmp(P.p[i].path, f[3])) { P.p[i].fav = atoi(f[1]); P.p[i].albums = (u32)strtol(f[2], NULL, 10); }
        }
        kfree(buf);
    }
    P.loaded = true;
}

static bool in_view(photo_t *ph) {
    if (P.search[0]) {
        char a[100], q[64];
        snprintf(a, sizeof a, "%s", ph->name);
        snprintf(q, sizeof q, "%s", P.search);
        for (char *x = a; *x; x++) *x = (char)tolower(*x);
        for (char *x = q; *x; x++) *x = (char)tolower(*x);
        if (!strstr(a, q)) return false;
    }
    switch (P.view) {
    case 0: return true;
    case 1: return ph->fav;
    case 2: return !strncasecmp(ph->name, "Captura", 7);
    case 3: return timer_ticks() - ph->mtime < 7ull * 24 * 3600 * 1000 && ph->mtime;
    default: return (ph->albums >> (P.view - 10)) & 1;
    }
}

/* miniatura quadrada (recorte central) */
static void make_thumb(photo_t *ph) {
    surface_t *img = kit_load_image(ph->path);
    if (!img) { ph->failed = true; return; }
    int ts = 220;
    surface_t *t = surface_new(ts, ts);
    int side = MIN(img->w, img->h), ox = (img->w - side) / 2, oy = (img->h - side) / 2;
    for (int y = 0; y < ts; y++)
        for (int x = 0; x < ts; x++) {
            int sx = ox + x * side / ts, sy = oy + y * side / ts;
            t->px[(usize)y * t->stride + x] = img->px[(usize)sy * img->stride + sx] | 0xFF000000;
        }
    surface_free(img);
    ph->thumb = t;
}

/* ------------------------------------------------------------ edição */
static u32 apply_px(u32 c) {
    float r = C_R(c), g = C_G(c), b = C_B(c);
    /* luz e contraste */
    float L = P.light * 80, k = 1 + P.contrast;
    r = (r - 128) * k + 128 + L; g = (g - 128) * k + 128 + L; b = (b - 128) * k + 128 + L;
    /* cor (saturação) */
    float gray = r * 0.299f + g * 0.587f + b * 0.114f, sat = 1 + P.color;
    switch (P.filter) {
    case 1: sat *= 1.35f; break;
    case 2: sat *= 0.8f; r = (r - 128) * 1.25f + 118; g = (g - 128) * 1.25f + 118; b = (b - 128) * 1.25f + 128; break;
    case 3: sat = 0; break;
    case 4: sat = 0; break;
    case 5: sat = 0; gray = (gray - 128) * 1.4f + 118; break;
    }
    r = gray + (r - gray) * sat; g = gray + (g - gray) * sat; b = gray + (b - gray) * sat;
    if (P.filter == 4) { float t = gray; r = t * 1.07f + 20; g = t * 0.94f + 10; b = t * 0.74f; }
    if (P.filter == 5) { r = g = b = gray; }
    return RGB(CLAMP((int)r, 0, 255), CLAMP((int)g, 0, 255), CLAMP((int)b, 0, 255));
}
static surface_t *render_edit(surface_t *src) {
    if (!src) return NULL;
    bool swap = P.rot % 2;
    int w = swap ? src->h : src->w, h = swap ? src->w : src->h;
    surface_t *o = surface_new(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int sx, sy;
            switch (P.rot & 3) {
            case 1: sx = y; sy = src->h - 1 - x; break;
            case 2: sx = src->w - 1 - x; sy = src->h - 1 - y; break;
            case 3: sx = src->w - 1 - y; sy = x; break;
            default: sx = x; sy = y;
            }
            o->px[(usize)y * o->stride + x] = apply_px(src->px[(usize)sy * src->stride + sx]);
        }
    return o;
}
static void edit_reset(void) { P.light = P.contrast = P.color = 0; P.filter = 0; P.rot = 0; P.dirty_edit = true; }
static void save_edit(void) {
    if (P.sel < 0 || !P.full) return;
    surface_t *o = render_edit(P.full);
    if (!o) return;
    char base[200], path[240];
    snprintf(base, sizeof base, "%s", P.p[P.sel].path);
    char *dot = strrchr(base, '.');
    if (dot) *dot = 0;
    int k = 1;
    do snprintf(path, sizeof path, k == 1 ? "%s (editada).bmp" : "%s (editada %d).bmp", base, k); while (vfs_lookup(path) && ++k < 100);
    if (kit_save_bmp(path, o)) notify("Foto salva", strrchr(path, '/') + 1);
    surface_free(o);
    P.edit = false;
    load();
    for (int i = 0; i < P.n; i++) if (!strcmp(P.p[i].path, path)) P.sel = i;
    P.full_for = -1;
}

static void delete_sel(void) {
    if (P.sel < 0) return;
    vnode_t *v = vfs_lookup(P.p[P.sel].path);
    if (v) { vfs_mkdir(CUR_TRASH); vfs_move(v, vfs_lookup(CUR_TRASH), v->name); }
    P.viewer = false;
    P.sel = -1;
    load();
}

/* ------------------------------------------------------------ desenho */
static void g_heart(surface_t *s, int x, int y, int sz, u32 c) { glyph_heart(s, x, y, sz, c); }

static void paint_sidebar(window_t *w, ui_t *u) {
    int sb = w->sidebar_w, y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Fotos", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    static const char *names[4] = {"Biblioteca", "Favoritos", "Capturas de Tela", "Recentes"};
    static glyph_fn icons[4] = {glyph_sb_photo, g_heart, glyph_sb_desktop, glyph_sb_recents};
    for (int i = 0; i < 4; i++) {
        if (ui_sidebar_item(u, 106000 + i, rect_make(10, y, sb - 20, 28), names[i], P.view == i, icons[i])) { P.view = i; P.viewer = false; P.scroll = 0; }
        y += 30;
    }
    y += 8;
    ui_label(u, 18, y + 2, "Álbuns", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    for (int i = 0; i < P.nalb; i++) {
        rect_t r = rect_make(10, y, sb - 20, 28);
        if (P.renaming && P.view == 10 + i) {
            if (ui_textfield(u, 106500, rect_make(r.x + 30, r.y + 2, r.w - 36, 24), P.rename_buf, sizeof P.rename_buf, "Nome do álbum")) {
                if (P.rename_buf[0]) snprintf(P.alb[i], 40, "%s", P.rename_buf);
                P.renaming = false;
                save();
            } else if (u->focus != 106500) u->focus = 106500;
        } else if (ui_sidebar_item(u, 106100 + i, r, P.alb[i], P.view == 10 + i, glyph_sb_photo)) {
            if (P.view == 10 + i && u->dblclick) { P.renaming = true; snprintf(P.rename_buf, 40, "%s", P.alb[i]); }
            P.view = 10 + i; P.viewer = false; P.scroll = 0;
        }
        y += 30;
    }
    if (ui_button_style(u, UI_ID, rect_make(10, u->ch - 36, 130, 28), "Novo Álbum", BTN_PLAIN_GRAY, P.nalb < MAXALB, glyph_plus)) {
        snprintf(P.alb[P.nalb], 40, "Novo Álbum");
        P.view = 10 + P.nalb;
        P.nalb++;
        P.renaming = true;
        snprintf(P.rename_buf, 40, "Novo Álbum");
        save();
    }
}

static void paint_grid(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    int ts = P.zoom, gap = 3;
    int cols = MAX(1, (a.w - 16 + gap) / (ts + gap));
    ts = (a.w - 16 - gap * (cols - 1)) / cols;
    rect_t pc = surface_push_clip(s, a);
    int k = 0, decoded = 0;
    for (int i = 0; i < P.n; i++) {
        photo_t *ph = &P.p[i];
        if (!in_view(ph)) continue;
        int x = a.x + 8 + (k % cols) * (ts + gap), y = a.y + 8 + (k / cols) * (ts + gap) - P.scroll;
        k++;
        if (y + ts < a.y || y > a.y + a.h) continue;
        rect_t r = rect_make(x, y, ts, ts);
        if (!ph->thumb && !ph->failed && decoded < 2) { make_thumb(ph); decoded++; }
        if (ph->thumb) gfx_blit_scaled(s, ph->thumb, x, y, ts, ts, 255);
        else { gfx_fill(s, x, y, ts, ts, T.alt_row); if (!ph->failed) u->repaint = true; else text_draw_center(s, font_get(FONT_REGULAR), 11, r, T.text3, "Sem prévia"); }
        if (ph->fav) { gfx_circle(s, x + 16, y + ts - 16, 11, RGBA(0, 0, 0, 90)); glyph_heart(s, x + 9, y + ts - 23, 14, RGB(255, 255, 255)); }
        if (P.sel == i) { gfx_rect(s, x, y, ts, ts, T.accent); gfx_rect(s, x + 1, y + 1, ts - 2, ts - 2, T.accent); gfx_rect(s, x + 2, y + 2, ts - 4, ts - 4, RGB(255, 255, 255)); }
        if (ui_clicked(u, 107000 + i, r)) { P.sel = i; if (u->dblclick) { P.viewer = true; P.edit = false; } }
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, a, (k + cols - 1) / cols * (ts + gap) + 16, &P.scroll);
    if (!k) {
        const char *m = P.view == 1 ? "Nenhum Favorito" : P.view >= 10 ? "Álbum Vazio" : "Nenhuma Foto";
        ui_label_center(u, rect_make(a.x, a.y + a.h / 2 - 30, a.w, 30), m, FONT_SEMIBOLD, 20, T.text3);
        const char *h = P.view >= 10 ? "Selecione fotos na Biblioteca e use Imagem > Adicionar ao Álbum." : "Fotos de Imagens, Mesa e Downloads aparecem aqui.";
        ui_label_center(u, rect_make(a.x, a.y + a.h / 2 + 4, a.w, 20), h, FONT_REGULAR, 13, T.text3);
    }
}

static void paint_viewer(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    if (P.sel < 0 || P.sel >= P.n) { P.viewer = false; return; }
    photo_t *ph = &P.p[P.sel];
    if (P.full_for != P.sel) {
        if (P.full) surface_free(P.full);
        P.full = kit_load_image(ph->path);
        P.full_for = P.sel;
        P.dirty_edit = true;
        if (P.edited) { surface_free(P.edited); P.edited = NULL; }
        if (!P.edit) edit_reset();
    }
    gfx_fill(s, a.x, a.y, a.w, a.h, T.dark ? RGB(18, 18, 20) : RGB(246, 246, 248));
    int panel = P.edit ? 260 : P.info ? 250 : 0;
    rect_t img = rect_make(a.x + 20, a.y + 20, a.w - 40 - panel, a.h - 40);
    surface_t *show = P.full;
    if (P.edit || P.rot) {
        if (P.dirty_edit) { if (P.edited) surface_free(P.edited); P.edited = render_edit(P.full); P.dirty_edit = false; }
        show = P.edited;
    }
    if (show) {
        float sc = MIN((float)img.w / show->w, (float)img.h / show->h);
        sc = MIN(sc, 4.0f);
        int dw = (int)(show->w * sc), dh = (int)(show->h * sc);
        gfx_shadow(s, img.x + (img.w - dw) / 2, img.y + (img.h - dh) / 2, dw, dh, 2, 16, 70, 4);
        gfx_blit_scaled(s, show, img.x + (img.w - dw) / 2, img.y + (img.h - dh) / 2, dw, dh, 255);
    } else ui_label_center(u, img, "Não foi possível abrir esta imagem", FONT_SEMIBOLD, 16, T.text3);
    /* painel lateral */
    if (P.info && !P.edit) {
        rect_t r = rect_make(a.x + a.w - panel, a.y, panel, a.h);
        gfx_fill(s, r.x, r.y, r.w, r.h, T.content_bg);
        gfx_fill(s, r.x, r.y, 1, r.h, T.separator);
        int y = r.y + 20;
        text_draw_ellipsis(s, font_get(FONT_BOLD), 15, r.x + 16, y + 14, r.w - 32, T.text, ph->name);
        y += 34;
        char t[64];
        if (P.full) { snprintf(t, sizeof t, "%d × %d pixels", P.full->w, P.full->h); ui_label(u, r.x + 16, y, t, FONT_REGULAR, 12.5f, T.text2); y += 22; }
        snprintf(t, sizeof t, "%u KB", (unsigned)(ph->size / 1024));
        ui_label(u, r.x + 16, y, t, FONT_REGULAR, 12.5f, T.text2);
        y += 22;
        ui_label(u, r.x + 16, y, vfs_ext(ph->name), FONT_REGULAR, 12.5f, T.text2);
        y += 30;
        text_wrap(s, font_get(FONT_REGULAR), 11.5f, rect_make(r.x + 16, y, r.w - 32, 80), 16, T.text3, ph->path, true);
        y += 60;
        ui_label(u, r.x + 16, y, "Álbuns", FONT_SEMIBOLD, 12, T.text);
        y += 22;
        for (int i = 0; i < P.nalb; i++) {
            bool on = (ph->albums >> i) & 1;
            if (ui_checkbox(u, 108000 + i, r.x + 16, y, P.alb[i], &on)) { ph->albums = on ? ph->albums | (1u << i) : ph->albums & ~(1u << i); save(); }
            y += 24;
        }
    }
    if (P.edit) {
        rect_t r = rect_make(a.x + a.w - panel, a.y, panel, a.h);
        gfx_fill(s, r.x, r.y, r.w, r.h, T.content_bg);
        gfx_fill(s, r.x, r.y, 1, r.h, T.separator);
        int y = r.y + 16;
        ui_label(u, r.x + 16, y, "Ajustar", FONT_BOLD, 15, T.text);
        y += 30;
        const char *lab[3] = {"Luz", "Contraste", "Cor"};
        float *val[3] = {&P.light, &P.contrast, &P.color};
        for (int i = 0; i < 3; i++) {
            ui_label(u, r.x + 16, y, lab[i], FONT_SEMIBOLD, 12.5f, T.text);
            float old = *val[i];
            ui_slider(u, 108100 + i, rect_make(r.x + 16, y + 18, r.w - 32, 22), val[i], -1, 1);
            if (old != *val[i]) P.dirty_edit = true;
            y += 50;
        }
        ui_label(u, r.x + 16, y, "Filtros", FONT_BOLD, 15, T.text);
        y += 28;
        for (int i = 0; i < 6; i++) {
            rect_t b = rect_make(r.x + 16 + (i % 2) * ((r.w - 40) / 2 + 8), y + (i / 2) * 34, (r.w - 40) / 2, 28);
            if (ui_button_style(u, 108200 + i, b, filters[i], P.filter == i ? BTN_FILLED : BTN_GRAY, true, NULL)) { P.filter = i; P.dirty_edit = true; }
        }
        y += 3 * 34 + 12;
        if (ui_button_style(u, UI_ID, rect_make(r.x + 16, y, r.w - 32, 28), "Girar 90°", BTN_GRAY, true, NULL)) { P.rot = (P.rot + 1) & 3; P.dirty_edit = true; }
        y += 36;
        if (ui_button_style(u, UI_ID, rect_make(r.x + 16, y, r.w - 32, 28), "Reverter ao Original", BTN_GRAY, true, NULL)) edit_reset();
        if (ui_button_style(u, UI_ID, rect_make(r.x + 16, r.y + r.h - 44, (r.w - 40) / 2, 28), "Cancelar", BTN_GRAY, true, NULL)) { P.edit = false; edit_reset(); }
        if (ui_button_style(u, UI_ID, rect_make(r.x + 24 + (r.w - 40) / 2, r.y + r.h - 44, (r.w - 40) / 2, 28), "Salvar Cópia", BTN_FILLED, true, NULL)) save_edit();
    }
    /* navegação por teclado */
    if (u->key && !u->focus) {
        int dir = u->key == KEY_LEFT ? -1 : u->key == KEY_RIGHT ? 1 : 0;
        if (dir && !P.edit) {
            for (int i = P.sel + dir; i >= 0 && i < P.n; i += dir) if (in_view(&P.p[i])) { P.sel = i; break; }
            u->key_used = true;
        }
        if (u->key == KEY_ESC || (u->key == ' ' && !P.edit)) { if (P.edit) { P.edit = false; edit_reset(); } else P.viewer = false; u->key_used = true; }
    }
}

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    if (!P.loaded) load();
    paint_sidebar(w, u);
    int sb = w->sidebar_w;
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    int tx = u->cw - 12;
    if (P.viewer) {
        if (ui_button_style(u, UI_ID, rect_make(sb + 12, TOOLBAR_Y(28), 34, 28), "‹", BTN_GRAY, true, NULL)) { P.viewer = false; P.edit = false; }
        photo_t *ph = P.sel >= 0 ? &P.p[P.sel] : NULL;
        if (ph) ui_title(u, sb + 58, 0, 36, 300, ph->name, NULL);
        tx -= 90;
        if (ui_toolbar_button(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 90, 28), P.edit ? "Concluir" : "Editar", ph != NULL)) { if (P.edit) save_edit(); else { P.edit = true; edit_reset(); } }
        tx -= 8 + 34;
        if (ui_button_icon(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), glyph_reload, ph && !P.edit)) { P.rot = (P.rot + 1) & 3; P.dirty_edit = true; }
        tx -= 8 + 34;
        if (ph && ui_button_icon(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), ph->fav ? glyph_heart : glyph_heart_slash, true)) { ph->fav = !ph->fav; save(); }
        tx -= 8 + 60;
        if (ui_toolbar_button(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 60, 28), "Info", !P.edit)) P.info = !P.info;
        paint_viewer(w, u, a);
    } else {
        tx -= 180;
        ui_searchfield(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 180, 28), P.search, sizeof P.search);
        tx -= 8 + 120;
        float z = (float)P.zoom;
        ui_slider(u, UI_ID, rect_make(tx, TOOLBAR_Y(22), 120, 22), &z, 70, 320);
        P.zoom = (int)z;
        static const char *vn[4] = {"Biblioteca", "Favoritos", "Capturas de Tela", "Recentes"};
        ui_title(u, sb + 18, 0, 36, 300, P.view < 10 ? vn[P.view] : P.alb[P.view - 10], NULL);
        paint_grid(w, u, a);
        if (u->key && !u->focus) {
            if ((u->key == ' ' || u->key == KEY_ENTER) && P.sel >= 0) { P.viewer = true; u->key_used = true; }
            if ((u->key == KEY_BACKSPACE || u->key == KEY_DELETE) && P.sel >= 0) { delete_sel(); u->key_used = true; }
            if (u->key == '.' && P.sel >= 0) { P.p[P.sel].fav = !P.p[P.sel].fav; save(); u->key_used = true; }
        }
    }
}

static void on_close(window_t *w) {
    P.w = NULL;
    if (P.full) { surface_free(P.full); P.full = NULL; }
    if (P.edited) { surface_free(P.edited); P.edited = NULL; }
    P.full_for = -1;
}
static void launch(app_t *a) {
    if (P.w) { wm_focus(P.w); return; }
    load();
    P.w = wm_create(&app_fotos, "Fotos", -1, -1, 1000, 660, WIN_RESIZABLE);
    if (!P.w) return;
    P.w->paint = paint;
    P.w->on_close = on_close;
    P.w->toolbar_h = TOOLBAR_H;
    P.w->sidebar_w = 220;
    P.w->min_w = 720;
    P.w->min_h = 480;
    P.w->title_x = -1;
}

enum { M_FAV = 1, M_DEL, M_ALB0 = 100, M_REFRESH = 50, M_ROT, M_EDIT, M_INFO };
static menu_item_t m_img[MAXALB + 8];
static menu_t menus[1] = {{"Imagem", m_img, 0}};
static void build_menu(void) {
    int k = 0;
    m_img[k++] = (menu_item_t){"Favoritar", ".", M_FAV};
    m_img[k++] = (menu_item_t){"Girar", "⌘R", M_ROT};
    m_img[k++] = (menu_item_t){"Editar", "↩", M_EDIT};
    m_img[k++] = (menu_item_t){"Mostrar Informações", "⌘I", M_INFO};
    m_img[k++] = (menu_item_t){NULL};
    for (int i = 0; i < P.nalb && k < MAXALB + 6; i++) m_img[k++] = (menu_item_t){P.alb[i], NULL, M_ALB0 + i};
    m_img[k++] = (menu_item_t){"Apagar Foto", "⌫", M_DEL};
    menus[0].count = k;
}
static void on_menu(app_t *a, int act) {
    photo_t *ph = P.sel >= 0 && P.sel < P.n ? &P.p[P.sel] : NULL;
    if (act == M_FAV && ph) { ph->fav = !ph->fav; save(); }
    else if (act == M_DEL) delete_sel();
    else if (act == M_ROT && ph) { P.viewer = true; P.rot = (P.rot + 1) & 3; P.dirty_edit = true; }
    else if (act == M_EDIT && ph) { P.viewer = true; P.edit = true; edit_reset(); }
    else if (act == M_INFO) { P.info = !P.info; if (ph) P.viewer = true; }
    else if (act >= M_ALB0 && act < M_ALB0 + MAXALB && ph) {
        int i = act - M_ALB0;
        ph->albums ^= 1u << i;
        notify((ph->albums >> i) & 1 ? "Adicionada ao álbum" : "Removida do álbum", P.alb[i]);
        save();
    }
    build_menu();
    if (P.w) wm_invalidate(P.w);
}
static void tick(app_t *a) { static int last = -1; if (last != P.nalb) { last = P.nalb; build_menu(); } }

static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(255, 255, 255), RGB(236, 236, 240));
    static const u32 c[8] = {RGB(255, 149, 0), RGB(255, 204, 0), RGB(140, 210, 60), RGB(48, 199, 120), RGB(50, 170, 230), RGB(90, 110, 230), RGB(175, 82, 222), RGB(255, 59, 90)};
    for (int i = 0; i < 8; i++) {
        float a = i * PI_F / 4, cx = x + 32 * u + fcos(a) * 10 * u, cy = y + 32 * u + fsin(a) * 10 * u;
        gfx_circle(s, cx, cy, 9 * u, color_alpha(c[i], 200));
    }
}

app_t app_fotos = {.name = "Fotos", .exec = "fotos", .draw_icon = icon, .launch = launch, .tick = tick, .on_menu = on_menu,
                   .menus = menus, .nmenus = 1, .pinned = true};
