/* mOS — Editor de Texto (multilinha, com quebra automática, abrir/salvar no VFS) e Notas */
#include "apps.h"

typedef struct {
    char *buf;
    int len, cap, caret, sel;
    int scroll;
    char path[192];
    bool dirty;
    float size;
    int goal_x;
    bool notes;
    int note_idx;
    /* desfazer/refazer: fotos do texto antes de cada grupo de edição */
    struct { char *buf; int len, caret; } undo[24], redo[24];
    int nundo, nredo;
    u64 last_snap;
    int last_kind;
    /* área de texto embutida em outro app (textarea_*): cores próprias, sem ⌘S */
    bool embedded;
    u32 bg, fg;
    int pad;
    bool focused;
} ed_t;

enum { ED_TYPE = 1, ED_DEL, ED_OTHER };
static void snap_free(char **p) { if (*p) { kfree(*p); *p = NULL; } }
static void ed_snapshot(ed_t *e, int kind) {
    u64 now = timer_ticks();
    if (kind == e->last_kind && kind != ED_OTHER && now - e->last_snap < 1000) { e->last_snap = now; return; }
    e->last_snap = now;
    e->last_kind = kind;
    if (e->nundo == 24) { snap_free(&e->undo[0].buf); memmove(e->undo, e->undo + 1, sizeof e->undo[0] * 23); e->nundo--; }
    char *c = kmalloc(e->len + 1);
    if (!c) return;
    memcpy(c, e->buf, e->len + 1);
    e->undo[e->nundo].buf = c; e->undo[e->nundo].len = e->len; e->undo[e->nundo].caret = e->caret;
    e->nundo++;
    for (int i = 0; i < e->nredo; i++) snap_free(&e->redo[i].buf);
    e->nredo = 0;
}
/* troca o texto atual por uma foto (a atual vai para a outra pilha) */
static void ed_swap(ed_t *e, bool undo) {
    int *n = undo ? &e->nundo : &e->nredo, *m = undo ? &e->nredo : &e->nundo;
    typeof(e->undo[0]) *from = undo ? e->undo : e->redo, *to = undo ? e->redo : e->undo;
    if (!*n || *m == 24) return;
    char *c = kmalloc(e->len + 1);
    if (!c) return;
    memcpy(c, e->buf, e->len + 1);
    to[*m].buf = c; to[*m].len = e->len; to[*m].caret = e->caret;
    (*m)++;
    (*n)--;
    if (from[*n].len + 1 > e->cap) { e->cap = from[*n].len + 1024; e->buf = krealloc(e->buf, e->cap); }
    memcpy(e->buf, from[*n].buf, from[*n].len + 1);
    e->len = from[*n].len;
    e->caret = MIN(from[*n].caret, e->len);
    e->sel = -1;
    snap_free(&from[*n].buf);
    e->dirty = true;
    e->last_kind = 0;
}
static void ed_copy(ed_t *e) {
    if (e->sel < 0 || e->sel == e->caret) return;
    int a = MIN(e->sel, e->caret), b = MAX(e->sel, e->caret);
    clipboard_set(e->buf + a, b - a);
}

static void ed_ensure(ed_t *e, int extra) {
    if (e->len + extra + 1 > e->cap) {
        e->cap = MAX(e->cap * 2, e->len + extra + 1024);
        e->buf = krealloc(e->buf, e->cap);
    }
}

static void ed_insert(ed_t *e, const char *t, int n) {
    if (e->sel >= 0 && e->sel != e->caret) {
        int a = MIN(e->sel, e->caret), b = MAX(e->sel, e->caret);
        memmove(e->buf + a, e->buf + b, e->len - b + 1);
        e->len -= b - a;
        e->caret = a;
    }
    e->sel = -1;
    ed_ensure(e, n);
    memmove(e->buf + e->caret + n, e->buf + e->caret, e->len - e->caret + 1);
    memcpy(e->buf + e->caret, t, n);
    e->len += n;
    e->caret += n;
    e->dirty = true;
}

static void ed_load(ed_t *e, const char *path) {
    vnode_t *n = vfs_lookup(path);
    e->len = 0;
    if (n && !n->dir) {
        ed_ensure(e, (int)n->size);
        memcpy(e->buf, n->data, n->size);
        e->len = (int)n->size;
        for (int i = 0; i < e->len; i++) if (e->buf[i] == '\r') e->buf[i] = ' ';
    }
    e->buf[e->len] = 0;
    strncpy(e->path, path, sizeof e->path - 1);
    e->caret = 0; e->sel = -1; e->dirty = false; e->scroll = 0;
}

static void ed_save(ed_t *e) {
    if (!e->path[0]) {
        int k = 1;
        do snprintf(e->path, sizeof e->path, home_sub("/Documentos/Sem título %d.txt"), k++); while (vfs_lookup(e->path));
    }
    if (!vfs_write_file(e->path, e->buf, e->len)) {
        notify("Não foi possível salvar", "O local é somente leitura (imagem de disco). Salve em Documentos.");
        char nm[160];
        strncpy(nm, strrchr(e->path, '/') ? strrchr(e->path, '/') + 1 : "Sem título.txt", sizeof nm - 1);
        nm[sizeof nm - 1] = 0;
        snprintf(e->path, sizeof e->path, home_sub("/Documentos/%s"), nm);
        return;
    }
    e->dirty = false;
    notify("Documento salvo", e->path);
}

/* documento sem nome: pergunta onde salvar (diálogo do sistema); com nome: salva direto */
static const char *ed_formats[] = {"Texto Simples (.txt)", "Markdown (.md)", "Página Web (.html)"};
static const char *ed_exts[] = {".txt", ".md", ".html"};

static void ed_save_cb(int button, const char *dir, const char *name, int fmt, void *ctx) {
    ed_t *e = ctx;
    if (button != 2 || !name[0]) return;
    const char *ext = ed_exts[CLAMP(fmt, 0, 2)];
    usize nl = strlen(name), el = strlen(ext);
    bool has_ext = nl > el && !strcasecmp(name + nl - el, ext);
    snprintf(e->path, sizeof e->path, "%s/%s%s", dir, name, has_ext ? "" : ext);
    ed_save(e);
}

static void ed_save_or_ask(ed_t *e) {
    if (e->path[0]) { ed_save(e); return; }
    save_dialog_show("Deseja salvar este novo documento?", "Escolha um nome e um local. Você poderá abri-lo depois pelo Arquivos.",
                     "Sem título", ed_formats, 3, false, ed_save_cb, e);
}

/* layout com quebra: calcula posições (início de cada linha visual) */
typedef struct { int start, end; } vline_t;

static int layout(ed_t *e, font_t *f, int width, vline_t *out, int max) {
    int n = 0, i = 0;
    while (i <= e->len && n < max) {
        int ls = i, last_space = -1;
        int j = i;
        while (j < e->len && e->buf[j] != '\n') {
            int nj = utf8_next(e->buf, j);
            if (e->buf[j] == ' ') last_space = j;
            if (text_width_n(f, e->size, e->buf + ls, nj - ls) > width && j > ls) {
                if (last_space > ls) j = last_space + 1;
                break;
            }
            j = nj;
        }
        out[n].start = ls;
        out[n].end = j;
        n++;
        if (j >= e->len) break;
        i = e->buf[j] == '\n' ? j + 1 : j;
        if (e->buf[j] == '\n' && i == e->len) { out[n].start = out[n].end = e->len; n++; break; }
    }
    return n;
}

static vline_t lines[8192];

static void paint_editor(window_t *w, ui_t *u, ed_t *e, rect_t area) {
    surface_t *s = u->s;
    font_t *f = font_get(FONT_REGULAR);
    if (e->bg != 1) gfx_fill(s, area.x, area.y, area.w, area.h, e->bg ? e->bg : T.content_bg);   /* bg 1 = transparente */
    u32 fg = e->fg ? e->fg : T.text;
    int pad = e->pad ? e->pad : 14, lh = (int)(e->size * 1.45f);
    int n = layout(e, f, area.w - pad * 2 - 12, lines, 8192);
    /* linha do caret */
    int cl = 0;
    for (int i = 0; i < n; i++) if (e->caret >= lines[i].start && (e->caret < lines[i].end || (e->caret == lines[i].end && (i == n - 1 || lines[i + 1].start > e->caret)))) { cl = i; break; }
    if (e->caret == e->len) cl = n - 1;
    /* teclado */
    if (e->embedded) {
        if (u->pressed) e->focused = ui_hit(u, area);
        if (e->focused && u->pressed) u->focus = 0;
        if (u->focus) e->focused = false;
    }
    bool keys = !e->embedded || e->focused;
    if (keys && u->key && !(u->mods & MOD_CMD)) {
        int k = u->key;
        if (k == KEY_BACKSPACE || k == KEY_DELETE) ed_snapshot(e, ED_DEL);
        else if (k == KEY_ENTER || k == KEY_TAB || u->text[0]) ed_snapshot(e, (e->sel >= 0 && e->sel != e->caret) ? ED_OTHER : ED_TYPE);
        if ((u->mods & MOD_SHIFT) && (k == KEY_LEFT || k == KEY_RIGHT || k == KEY_HOME || k == KEY_END)) {
            /* ⇧ + setas estendem a seleção */
            if (e->sel < 0) e->sel = e->caret;
            if (k == KEY_LEFT) e->caret = utf8_prev(e->buf, e->caret);
            else if (k == KEY_RIGHT) e->caret = utf8_next(e->buf, e->caret);
            else if (k == KEY_HOME) e->caret = lines[cl].start;
            else e->caret = lines[cl].end;
            u->key_used = true;
            k = 0;
        }
        if (!k) {}
        else if (k == KEY_BACKSPACE) {
            if (e->sel >= 0 && e->sel != e->caret) ed_insert(e, "", 0);
            else if (e->caret > 0) { int p = utf8_prev(e->buf, e->caret); memmove(e->buf + p, e->buf + e->caret, e->len - e->caret + 1); e->len -= e->caret - p; e->caret = p; e->dirty = true; }
        } else if (k == KEY_DELETE) {
            if (e->caret < e->len) { int q = utf8_next(e->buf, e->caret); memmove(e->buf + e->caret, e->buf + q, e->len - q + 1); e->len -= q - e->caret; e->dirty = true; }
        } else if (k == KEY_LEFT) { e->caret = utf8_prev(e->buf, e->caret); e->sel = -1; }
        else if (k == KEY_RIGHT) { e->caret = utf8_next(e->buf, e->caret); e->sel = -1; }
        else if (k == KEY_UP || k == KEY_DOWN || k == KEY_PGUP || k == KEY_PGDN) {
            int step = (k == KEY_PGUP || k == KEY_PGDN) ? area.h / lh : 1;
            int tl = CLAMP(cl + ((k == KEY_UP || k == KEY_PGUP) ? -step : step), 0, n - 1);
            int gx = text_width_n(f, e->size, e->buf + lines[cl].start, e->caret - lines[cl].start);
            char tmp[1024];
            int ll = MIN(lines[tl].end - lines[tl].start, 1023);
            memcpy(tmp, e->buf + lines[tl].start, ll);
            tmp[ll] = 0;
            e->caret = lines[tl].start + text_index_at(f, e->size, tmp, gx);
            e->sel = -1;
        } else if (k == KEY_HOME) e->caret = lines[cl].start;
        else if (k == KEY_END) e->caret = lines[cl].end;
        else if (k == KEY_ENTER) ed_insert(e, "\n", 1);
        else if (k == KEY_TAB) ed_insert(e, "    ", 4);
        else if (u->text[0]) ed_insert(e, u->text, (int)strlen(u->text));
        u->key_used = true;
        u->caret_blink = u->now;
        n = layout(e, f, area.w - pad * 2 - 12, lines, 8192);
        for (int i = 0; i < n; i++) if (e->caret >= lines[i].start && e->caret <= lines[i].end) { cl = i; if (e->caret < lines[i].end) break; }
        /* mantém o caret visível */
        int cy = cl * lh;
        if (cy < e->scroll) e->scroll = cy;
        if (cy + lh > e->scroll + area.h - pad * 2) e->scroll = cy + lh - area.h + pad * 2;
    }
    if (keys && u->key && (u->mods & MOD_CMD)) {
        int ck = tolower(u->key);
        if (ck == 'a') { e->sel = 0; e->caret = e->len; u->key_used = true; }
        if (ck == 's' && !e->embedded) { ed_save_or_ask(e); u->key_used = true; }
        if (ck == 'c') { ed_copy(e); u->key_used = true; }
        if (ck == 'x' && e->sel >= 0 && e->sel != e->caret) { ed_snapshot(e, ED_OTHER); ed_copy(e); ed_insert(e, "", 0); u->key_used = true; }
        if (ck == 'v') { const char *c = clipboard_get(); if (*c) { ed_snapshot(e, ED_OTHER); ed_insert(e, c, (int)strlen(c)); } u->key_used = true; }
        if (ck == 'z') { ed_swap(e, !(u->mods & MOD_SHIFT)); u->key_used = true; }
    }
    /* mouse */
    if (ui_hit(u, area)) {
        u->cursor = CURSOR_IBEAM;
        if (u->pressed || (u->down && u->active == 9999)) {
            int li = CLAMP((u->my - area.y - pad + e->scroll) / lh, 0, n - 1);
            char tmp[1024];
            int ll = MIN(lines[li].end - lines[li].start, 1023);
            memcpy(tmp, e->buf + lines[li].start, ll);
            tmp[ll] = 0;
            int pos = lines[li].start + text_index_at(f, e->size, tmp, u->mx - area.x - pad);
            if (u->pressed) { e->caret = pos; e->sel = pos; u->active = 9999; }
            else e->caret = pos;
            u->caret_blink = u->now;
        }
    }
    int content_h = n * lh + pad * 2;
    rect_t pc = surface_push_clip(s, area);
    int y0 = area.y + pad - e->scroll;
    int sa = e->sel >= 0 ? MIN(e->sel, e->caret) : -1, sb = e->sel >= 0 ? MAX(e->sel, e->caret) : -1;
    for (int i = 0; i < n; i++) {
        int y = y0 + i * lh;
        if (y + lh < area.y || y > area.y + area.h) continue;
        int base = y + (int)(e->size);
        if (sa >= 0 && sa != sb && sb > lines[i].start && sa <= lines[i].end) {
            int a = MAX(sa, lines[i].start), b = MIN(sb, lines[i].end);
            int xa = text_width_n(f, e->size, e->buf + lines[i].start, a - lines[i].start);
            int xb = text_width_n(f, e->size, e->buf + lines[i].start, b - lines[i].start);
            gfx_fill(s, area.x + pad + xa, y, MAX(4, xb - xa), lh, T.selection);
        }
        text_draw_n(s, f, e->size, area.x + pad, base, fg, e->buf + lines[i].start, lines[i].end - lines[i].start);
        if (i == cl && wm_focused() == w && keys && ((u->now - u->caret_blink) / 530) % 2 == 0) {
            int cx = text_width_n(f, e->size, e->buf + lines[i].start, e->caret - lines[i].start);
            gfx_fill(s, area.x + pad + cx, y + 1, 1, lh - 2, fg);
        }
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, area, content_h, &e->scroll);
    u->repaint = true;
}

/* ------------------------------------------------------------------ Editor de Texto */
static void paint_ed(window_t *w, ui_t *u) {
    ed_t *e = w->data;
    static const char *sizes[] = {"12", "14", "16", "18", "24"};
    static const float sz[] = {12, 14, 16, 18, 24};
    int si = 1;
    for (int i = 0; i < 5; i++) if (sz[i] == e->size) si = i;
    /* à direita: os semáforos e o título ocupam o começo da barra unificada */
    si = ui_popup(u, UI_ID, rect_make(u->cw - 8 - 84 - 8 - 72, TOOLBAR_Y(28), 72, 28), sizes, 5, si);
    e->size = sz[si];
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 8 - 84, TOOLBAR_Y(28), 84, 28), "Salvar", true)) ed_save_or_ask(e);
    char title[160];
    const char *nm = e->path[0] ? strrchr(e->path, '/') + 1 : "Sem título";
    snprintf(title, sizeof title, "%s%s", nm, e->dirty ? " — Editado" : "");
    wm_set_title(w, title);
    paint_editor(w, u, e, rect_make(0, w->toolbar_h, u->cw, u->ch - w->toolbar_h));
}

static void ed_close(window_t *w) {
    ed_t *e = w->data;
    if (e->dirty && e->len) ed_save(e);
    kfree(e->buf);
    kfree(e);
}

static window_t *ed_new(const char *path) {
    window_t *w = wm_create(&app_editor, "Sem título", -1, -1, 640, 480, WIN_RESIZABLE);
    if (!w) return NULL;
    ed_t *e = kcalloc(1, sizeof(ed_t));
    e->size = 14;
    e->sel = -1;
    ed_ensure(e, 0);
    e->buf[0] = 0;
    if (path) ed_load(e, path);
    w->data = e;
    w->paint = paint_ed;
    w->on_close = ed_close;
    w->toolbar_h = TOOLBAR_H;
    w->ui.focus = 1;
    return w;
}

void editor_open_file(const char *path) { app_editor.running = true; ed_new(path); }

static void ed_launch(app_t *a) { ed_new(NULL); }

static menu_item_t m_file[] = {{"Novo", "⌘N", 1}, {"Salvar", "⌘S", 2}, {NULL}, {"Fechar", "⌘W", 3}};
static menu_item_t m_edit[] = {
    {"Desfazer", "⌘Z", 10}, {"Refazer", "⇧⌘Z", 11}, {NULL},
    {"Recortar", "⌘X", 12}, {"Copiar", "⌘C", 13}, {"Colar", "⌘V", 14}, {"Selecionar Tudo", "⌘A", 15},
};
static menu_t ed_menus[] = {{"Arquivo", m_file, ARRAY_LEN(m_file)}, {"Editar", m_edit, ARRAY_LEN(m_edit)}};

static void ed_menu(app_t *a, int act) {
    window_t *w = wm_focused();
    ed_t *e = w && w->app == a ? w->data : NULL;
    if (act == 1) ed_new(NULL);
    if (act == 2 && e) ed_save_or_ask(e);
    if (act == 3 && w) wm_close(w);
    if (!e) return;
    if (act == 10) ed_swap(e, true);
    if (act == 11) ed_swap(e, false);
    if (act == 12 && e->sel >= 0 && e->sel != e->caret) { ed_snapshot(e, ED_OTHER); ed_copy(e); ed_insert(e, "", 0); }
    if (act == 13) ed_copy(e);
    if (act == 14) { const char *c = clipboard_get(); if (*c) { ed_snapshot(e, ED_OTHER); ed_insert(e, c, (int)strlen(c)); } }
    if (act == 15) { e->sel = 0; e->caret = e->len; }
    w->needs_paint = true;
}

app_t app_editor = {.name = "Editor de Texto", .exec = "editor", .draw_icon = icon_editor, .launch = ed_launch, .on_menu = ed_menu,
                    .menus = ed_menus, .nmenus = ARRAY_LEN(ed_menus), .pinned = true};

/* ------------------------------------------------------------------ Notas */
#define NOTES_DIR home_sub("/Documentos/Notas")
static window_t *notes_w;

static void notes_select(ed_t *e, int idx) {
    if (e->dirty && e->path[0]) vfs_write_file(e->path, e->buf, e->len);
    vnode_t *d = vfs_mkdir(NOTES_DIR);
    int i = 0;
    for (vnode_t *c = d->child; c; c = c->next, i++) {
        if (i == idx) { char p[192]; vfs_path(c, p, sizeof p); ed_load(e, p); e->note_idx = idx; return; }
    }
}

static void paint_notes(window_t *w, ui_t *u) {
    ed_t *e = w->data;
    surface_t *s = u->s;
    int sb = w->sidebar_w;
    if (ui_button_icon(u, UI_ID, rect_make(sb + 16, TOOLBAR_Y(28), 34, 28), glyph_plus, true)) {
        if (e->dirty && e->path[0]) vfs_write_file(e->path, e->buf, e->len);
        char p[192];
        datetime_t d;
        rtc_now(&d);
        snprintf(p, sizeof p, "%s/Nota %02d-%02d %02d%02d%02d.txt", NOTES_DIR, d.day, d.month, d.hour, d.minute, d.second);
        vfs_write_file(p, "Nova nota\n", 10);
        ed_load(e, p);
        vnode_t *dir = vfs_lookup(NOTES_DIR);
        int i = 0;
        for (vnode_t *c = dir->child; c; c = c->next, i++) if (!strcmp(c->name, strrchr(p, '/') + 1)) e->note_idx = i;
    }
    /* lista de notas */
    vnode_t *d = vfs_mkdir(NOTES_DIR);
    int y = w->toolbar_h, i = 0;
    /* a lista fica sobre a barra lateral de vidro (desenhada pelo window server) */
    y += 6;
    for (vnode_t *c = d->child; c; c = c->next, i++) {
        rect_t r = rect_make(10, y, sb - 22, 54);
        bool sel = i == e->note_idx;
        if (sel) gfx_round_rect(s, r.x, r.y + 2, r.w, r.h - 4, 9, T.dark ? RGBA(255, 200, 60, 90) : RGBA(255, 200, 40, 120));
        char first[64] = "";
        const char *src = (char *)c->data;
        int k = 0;
        if (sel) src = e->buf;
        for (; src && src[k] && src[k] != '\n' && k < 60; k++) first[k] = src[k];
        first[k] = 0;
        text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 13, 22, y + 23, sb - 44, T.text, first[0] ? first : "Nota vazia");
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 11.5f, 22, y + 40, sb - 44, T.text2, c->name);
        if (ui_clicked(u, 700 + i, r) && !sel) notes_select(e, i);
        y += 54;
    }
    paint_editor(w, u, e, rect_make(sb + 1, w->toolbar_h, u->cw - sb - 1, u->ch - w->toolbar_h));
}

static void notes_close(window_t *w) {
    ed_t *e = w->data;
    if (e->dirty && e->path[0]) vfs_write_file(e->path, e->buf, e->len);
    kfree(e->buf);
    kfree(e);
    notes_w = NULL;
}

static void notes_launch(app_t *a) {
    if (notes_w) { wm_focus(notes_w); return; }
    if (!vfs_lookup(NOTES_DIR)) {
        vfs_mkdir(NOTES_DIR);
        const char *t = "Bem-vindo ao Notas\n\nSuas notas ficam em Documentos/Notas.\nUse o botão + para criar uma nova nota.\n";
        vfs_write_file(home_sub("/Documentos/Notas/Bem-vindo.txt"), t, strlen(t));
    }
    notes_w = wm_create(&app_notas, "Notas", -1, -1, 720, 460, WIN_RESIZABLE);
    if (!notes_w) return;
    ed_t *e = kcalloc(1, sizeof(ed_t));
    e->size = 15;
    e->sel = -1;
    e->notes = true;
    ed_ensure(e, 0);
    notes_w->data = e;
    notes_w->paint = paint_notes;
    notes_w->on_close = notes_close;
    notes_w->toolbar_h = TOOLBAR_H;
    notes_w->sidebar_w = 220;
    notes_select(e, 0);
}

app_t app_notas = {.name = "Notas", .exec = "notas", .draw_icon = icon_notas, .launch = notes_launch, .pinned = true};

/* ------------------------------------------------------------------ área de texto para outros apps */
textarea_t *textarea_new(float size, u32 bg, u32 fg, int pad) {
    ed_t *e = kcalloc(1, sizeof(ed_t));
    if (!e) return NULL;
    e->size = size; e->sel = -1; e->embedded = true; e->bg = bg; e->fg = fg; e->pad = pad;
    ed_ensure(e, 0);
    e->buf[0] = 0;
    return (textarea_t *)e;
}
void textarea_set(textarea_t *t, const char *s, int n) {
    ed_t *e = (ed_t *)t;
    if (!e) return;
    if (n < 0) n = s ? (int)strlen(s) : 0;
    e->len = 0;
    ed_ensure(e, n);
    if (n) memcpy(e->buf, s, n);
    e->len = n;
    e->buf[n] = 0;
    e->caret = MIN(e->caret, n); e->sel = -1; e->dirty = false; e->scroll = 0;
    for (int i = 0; i < e->nundo; i++) snap_free(&e->undo[i].buf);
    for (int i = 0; i < e->nredo; i++) snap_free(&e->redo[i].buf);
    e->nundo = e->nredo = 0;
}
const char *textarea_text(textarea_t *t, int *len) { ed_t *e = (ed_t *)t; if (len) *len = e ? e->len : 0; return e ? e->buf : ""; }
bool textarea_dirty(textarea_t *t, bool clear) { ed_t *e = (ed_t *)t; bool d = e && e->dirty; if (e && clear) e->dirty = false; return d; }
bool textarea_focused(textarea_t *t) { ed_t *e = (ed_t *)t; return e && e->focused; }
void textarea_focus(textarea_t *t, bool on) { ed_t *e = (ed_t *)t; if (e) { e->focused = on; if (on) e->caret = e->len; } }
void textarea_colors(textarea_t *t, u32 bg, u32 fg) { ed_t *e = (ed_t *)t; if (e) { e->bg = bg; e->fg = fg; } }
void textarea_set_size(textarea_t *t, float size) { ed_t *e = (ed_t *)t; if (e) e->size = size; }
void textarea_paint(window_t *w, ui_t *u, textarea_t *t, rect_t area) { if (t) paint_editor(w, u, (ed_t *)t, area); }
void textarea_insert(textarea_t *t, const char *s) { ed_t *e = (ed_t *)t; if (e && s) ed_insert(e, s, (int)strlen(s)); }
void textarea_free(textarea_t *t) {
    ed_t *e = (ed_t *)t;
    if (!e) return;
    for (int i = 0; i < e->nundo; i++) snap_free(&e->undo[i].buf);
    for (int i = 0; i < e->nredo; i++) snap_free(&e->redo[i].buf);
    kfree(e->buf);
    kfree(e);
}
