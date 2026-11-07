/* mOS — Notas: pastas (subpastas de ~/Documentos/Notas), lista por data com prévia, notas fixadas no topo,
 * busca em todas as notas, listas de verificação (○/●), apagar para "Apagadas Recentemente" e recuperar,
 * nova pasta, renomear pasta; salvamento automático. As notas são arquivos .txt comuns. */
#include "kit.h"

extern app_t app_notas;
#define MAXN 300
#define MAXF 24

typedef struct { char path[220], name[96]; u64 mtime; bool pinned; } note_t;
static struct {
    window_t *w;
    char folders[MAXF][64];
    int nf, folder;           /* -1 = Todas; MAXF = Apagadas Recentemente */
    note_t n[MAXN];
    int nn, sel;
    textarea_t *ta;
    char cur_path[220];
    char search[64];
    char pins[64][220];
    int npins;
    int lscroll;
    bool renaming;
    char rename_buf[64];
    u64 last_save;
} N = {.folder = -1, .sel = -1};

static const char *ndir(void) { static char d[200]; snprintf(d, sizeof d, "%s/Documentos/Notas", CUR_HOME); return d; }
static const char *trash_dir(void) { static char d[220]; snprintf(d, sizeof d, "%s/.Apagadas", ndir()); return d; }

static void pins_save(void) {
    sbuf_t b = {0};
    for (int i = 0; i < N.npins; i++) sb_printf(&b, "%s\n", N.pins[i]);
    char p[200];
    kit_save(kit_lib("Notas", "fixadas.tsv", p, sizeof p), &b);
}
static void pins_load(void) {
    char p[200];
    char *buf = kit_read(kit_lib("Notas", "fixadas.tsv", p, sizeof p), NULL);
    N.npins = 0;
    if (!buf) return;
    char *cur = buf, *ln;
    while ((ln = kit_line(&cur)) && N.npins < 64) if (ln[0]) snprintf(N.pins[N.npins++], 220, "%s", ln);
    kfree(buf);
}
static bool is_pinned(const char *path) { for (int i = 0; i < N.npins; i++) if (!strcmp(N.pins[i], path)) return true; return false; }

static void scan_folders(void) {
    vnode_t *d = vfs_mkdir(ndir());
    N.nf = 0;
    for (vnode_t *c = d->child; c && N.nf < MAXF; c = c->next) if (c->dir && c->name[0] != '.') snprintf(N.folders[N.nf++], 64, "%s", c->name);
}
static void add_notes_from(const char *dir) {
    vnode_t *d = vfs_lookup(dir);
    for (vnode_t *c = d && d->dir ? d->child : NULL; c && N.nn < MAXN; c = c->next) {
        if (c->dir) continue;
        note_t *x = &N.n[N.nn++];
        snprintf(x->path, sizeof x->path, "%s/%s", dir, c->name);
        snprintf(x->name, sizeof x->name, "%s", c->name);
        x->mtime = c->mtime;
        x->pinned = is_pinned(x->path);
    }
}
static void scan_notes(void) {
    N.nn = 0;
    if (N.folder == MAXF) add_notes_from(trash_dir());
    else if (N.folder < 0) {
        add_notes_from(ndir());
        for (int i = 0; i < N.nf; i++) { char d[220]; snprintf(d, sizeof d, "%s/%s", ndir(), N.folders[i]); add_notes_from(d); }
    } else { char d[220]; snprintf(d, sizeof d, "%s/%s", ndir(), N.folders[N.folder]); add_notes_from(d); }
    /* fixadas primeiro, depois as mais recentes */
    for (int i = 1; i < N.nn; i++) {
        note_t t = N.n[i];
        int j = i - 1;
        while (j >= 0 && (N.n[j].pinned < t.pinned || (N.n[j].pinned == t.pinned && N.n[j].mtime < t.mtime))) { N.n[j + 1] = N.n[j]; j--; }
        N.n[j + 1] = t;
    }
}
static void save_current(void) {
    if (!N.cur_path[0] || !textarea_dirty(N.ta, true)) return;
    int len;
    const char *t = textarea_text(N.ta, &len);
    vfs_write_file(N.cur_path, t, len);
}
static void open_note(int i) {
    save_current();
    N.sel = i;
    if (i < 0 || i >= N.nn) { N.cur_path[0] = 0; textarea_set(N.ta, "", 0); return; }
    snprintf(N.cur_path, sizeof N.cur_path, "%s", N.n[i].path);
    char *b = kit_read(N.cur_path, NULL);
    textarea_set(N.ta, b ? b : "", -1);
    if (b) kfree(b);
}
static void new_note(void) {
    save_current();
    char dir[220];
    if (N.folder >= 0 && N.folder < MAXF) snprintf(dir, sizeof dir, "%s/%s", ndir(), N.folders[N.folder]);
    else snprintf(dir, sizeof dir, "%s", ndir());
    datetime_t d;
    rtc_now(&d);
    char p[240];
    snprintf(p, sizeof p, "%s/Nota %04d-%02d-%02d %02d%02d%02d.txt", dir, d.year, d.month, d.day, d.hour, d.minute, d.second);
    vfs_write_file(p, "", 0);
    if (N.folder == MAXF) N.folder = -1;
    N.search[0] = 0;
    scan_notes();
    for (int i = 0; i < N.nn; i++) if (!strcmp(N.n[i].path, p)) open_note(i);
    textarea_focus(N.ta, true);
}
static void delete_note(void) {
    if (N.sel < 0 || N.sel >= N.nn) return;
    vnode_t *v = vfs_lookup(N.n[N.sel].path);
    N.cur_path[0] = 0;
    if (v) {
        if (N.folder == MAXF) vfs_delete(v);
        else { vnode_t *t = vfs_mkdir(trash_dir()); vfs_move(v, t, v->name); }
    }
    scan_notes();
    open_note(MIN(N.sel, N.nn - 1));
}
static void toggle_pin(void) {
    if (N.sel < 0) return;
    const char *p = N.n[N.sel].path;
    bool found = false;
    for (int i = 0; i < N.npins; i++) if (!strcmp(N.pins[i], p)) { memmove(N.pins[i], N.pins[i + 1], 220 * (N.npins - i - 1)); N.npins--; found = true; break; }
    if (!found && N.npins < 64) snprintf(N.pins[N.npins++], 220, "%s", p);
    pins_save();
    char keep[220];
    snprintf(keep, sizeof keep, "%s", p);
    scan_notes();
    for (int i = 0; i < N.nn; i++) if (!strcmp(N.n[i].path, keep)) N.sel = i;
}

static void first_line(const char *src, char *out, int n, int skip) {
    /* skip = 0: primeira linha não vazia; 1: a seguinte */
    const char *p = src;
    for (int k = 0; k <= skip && p; k++) {
        while (*p == '\n' || *p == ' ') p++;
        if (k < skip) { const char *e = strchr(p, '\n'); p = e ? e + 1 : NULL; }
    }
    int i = 0;
    while (p && *p && *p != '\n' && i < n - 1) out[i++] = *p++;
    out[i] = 0;
}

static void g_folder(surface_t *s, int x, int y, int sz, u32 c) { gfx_round_rect_stroke(s, x + sz * 0.12f, y + sz * 0.28f, sz * 0.76f, sz * 0.52f, 2, MAX(1.2f, sz * 0.08f), c); gfx_fill(s, (int)(x + sz * 0.12f), (int)(y + sz * 0.22f), (int)(sz * 0.3f), MAX(1, (int)(sz * 0.1f)), c); }
static void g_trash(surface_t *s, int x, int y, int sz, u32 c) { glyph_sb_trash(s, x, y, sz, c); }
static void g_check(surface_t *s, int x, int y, int sz, u32 c) { gfx_round_rect_stroke(s, x + sz * 0.18f, y + sz * 0.18f, sz * 0.64f, sz * 0.64f, sz * 0.12f, 1.5f, c); glyph_check(s, x + sz * 0.24f, y + sz * 0.24f, (int)(sz * 0.52f), c); }
static void g_pin(surface_t *s, int x, int y, int sz, u32 c) { gfx_circle(s, x + sz * 0.5f, y + sz * 0.35f, sz * 0.2f, c); gfx_line(s, x + sz * 0.5f, y + sz * 0.5f, x + sz * 0.5f, y + sz * 0.9f, 1.5f, c); }
static void g_trashbtn(surface_t *s, int x, int y, int sz, u32 c) { glyph_sb_trash(s, x, y, sz, c); }

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w;
    /* pastas */
    int y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "No Meu Mac", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    if (ui_sidebar_item(u, 128000, rect_make(10, y, sb - 20, 28), "Notas", N.folder == -1, g_folder)) { save_current(); N.folder = -1; scan_notes(); open_note(N.nn ? 0 : -1); }
    y += 30;
    for (int i = 0; i < N.nf; i++) {
        rect_t r = rect_make(10, y, sb - 20, 28);
        if (N.renaming && N.folder == i) {
            if (ui_textfield(u, 128900, rect_make(r.x + 30, r.y + 2, r.w - 34, 24), N.rename_buf, sizeof N.rename_buf, "Nome da pasta")) {
                char a[220];
                snprintf(a, sizeof a, "%s/%s", ndir(), N.folders[i]);
                vnode_t *v = vfs_lookup(a);
                if (v && N.rename_buf[0] && !strchr(N.rename_buf, '/') && vfs_move(v, v->parent, N.rename_buf)) snprintf(N.folders[i], 64, "%s", N.rename_buf);
                N.renaming = false;
                scan_notes();
            } else if (u->focus != 128900) u->focus = 128900;
        } else if (ui_sidebar_item(u, 128100 + i, r, N.folders[i], N.folder == i, g_folder)) {
            if (N.folder == i && u->dblclick) { N.renaming = true; snprintf(N.rename_buf, 64, "%s", N.folders[i]); }
            else if (N.folder != i) { save_current(); N.folder = i; scan_notes(); open_note(N.nn ? 0 : -1); }
        }
        y += 30;
    }
    if (ui_sidebar_item(u, 128050, rect_make(10, y, sb - 20, 28), "Apagadas Recentemente", N.folder == MAXF, g_trash)) { save_current(); N.folder = MAXF; scan_notes(); open_note(N.nn ? 0 : -1); }
    if (ui_button_style(u, UI_ID, rect_make(10, u->ch - 36, 130, 28), "Nova Pasta", BTN_PLAIN_GRAY, N.nf < MAXF, glyph_plus)) {
        char d[220];
        int k = 1;
        do snprintf(d, sizeof d, k == 1 ? "%s/Nova Pasta" : "%s/Nova Pasta %d", ndir(), k); while (vfs_lookup(d) && ++k < 50);
        vfs_mkdir(d);
        scan_folders();
        for (int i = 0; i < N.nf; i++) if (!strcmp(N.folders[i], strrchr(d, '/') + 1)) { N.folder = i; N.renaming = true; snprintf(N.rename_buf, 64, "%s", N.folders[i]); }
        scan_notes();
        open_note(-1);
    }
    /* lista */
    int lw = 270;
    rect_t lv = rect_make(sb, w->toolbar_h, lw, u->ch - w->toolbar_h);
    gfx_fill(s, lv.x, lv.y, lv.w, lv.h, T.content_bg);
    gfx_fill(s, lv.x + lv.w - 1, 0, 1, u->ch, T.separator);
    const char *ft = N.folder == -1 ? "Notas" : N.folder == MAXF ? "Apagadas Recentemente" : N.folders[N.folder];
    char cnt[32];
    snprintf(cnt, sizeof cnt, "%d nota%s", N.nn, N.nn == 1 ? "" : "s");
    ui_title(u, sb + 16, 0, 36, lw - 24, ft, cnt);
    rect_t pc = surface_push_clip(s, lv);
    int ly = lv.y + 4 - N.lscroll, shown = 0;
    char q[64];
    snprintf(q, sizeof q, "%s", N.search);
    for (char *c = q; *c; c++) *c = (char)tolower(*c);
    bool pin_head = false, others_head = false;
    for (int i = 0; i < N.nn; i++) {
        note_t *x = &N.n[i];
        const char *txt;
        char *owned = NULL;
        int tl = 0;
        if (i == N.sel) txt = textarea_text(N.ta, &tl);
        else { vnode_t *v = vfs_lookup(x->path); txt = v && v->data ? (const char *)v->data : ""; tl = v ? (int)v->size : 0; }
        if (q[0]) {
            owned = kmalloc(tl + 1);
            for (int k = 0; k < tl; k++) owned[k] = (char)tolower(txt[k]);
            owned[tl] = 0;
            bool hit = strstr(owned, q) != NULL;
            kfree(owned);
            if (!hit) continue;
        }
        if (x->pinned && !pin_head && !q[0]) { pin_head = true; ui_label(u, lv.x + 18, ly + 6, "Fixadas", FONT_BOLD, 12, T.text2); ly += 26; }
        if (!x->pinned && pin_head && !others_head && !q[0]) { others_head = true; ui_label(u, lv.x + 18, ly + 6, "Notas", FONT_BOLD, 12, T.text2); ly += 26; }
        rect_t r = rect_make(lv.x, ly, lv.w, 62);
        bool sel = N.sel == i;
        if (sel) gfx_round_rect(s, r.x + 8, r.y + 1, r.w - 16, r.h - 2, 9, T.dark ? RGBA(255, 214, 10, 70) : RGBA(255, 204, 0, 110));
        char t1[96], t2[120];
        /* a nota pode estar sendo editada: usa uma cópia terminada em 0 */
        char tmp[400];
        int k = MIN(tl, 399);
        memcpy(tmp, txt, k);
        tmp[k] = 0;
        first_line(tmp, t1, sizeof t1, 0);
        first_line(tmp, t2, sizeof t2, 1);
        text_draw_ellipsis(s, font_get(FONT_BOLD), 13.5f, r.x + 22, r.y + 24, r.w - 44, T.text, t1[0] ? t1 : "Nova Nota");
        /* data relativa + prévia */
        char dt[32];
        u64 age = timer_ticks() - x->mtime;
        if (!x->mtime) snprintf(dt, sizeof dt, "Antiga");
        else if (age < 60000) snprintf(dt, sizeof dt, "Agora");
        else if (age < 3600000) snprintf(dt, sizeof dt, "Há %d min", (int)(age / 60000));
        else snprintf(dt, sizeof dt, "Hoje");
        int dw = text_draw(s, font_get(FONT_SEMIBOLD), 12, r.x + 22, r.y + 44, T.text2, dt);
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 12, r.x + 22 + dw + 8, r.y + 44, r.w - 52 - dw, T.text2, t2[0] ? t2 : "Nenhum texto adicional");
        if (x->pinned) g_pin(s, r.x + r.w - 30, r.y + 10, 14, RGB(255, 159, 10));
        if (!sel) gfx_fill(s, r.x + 22, r.y + r.h - 1, r.w - 40, 1, T.separator);
        if (ui_clicked(u, 128200 + i, r) && N.sel != i) open_note(i);
        ly += 62;
        shown++;
    }
    if (!shown) ui_label_center(u, rect_make(lv.x, lv.y + 40, lv.w, 20), q[0] ? "Nenhum Resultado" : "Nenhuma Nota", FONT_SEMIBOLD, 14, T.text3);
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, lv, ly + N.lscroll - lv.y + 10, &N.lscroll);
    /* barra de ferramentas do editor */
    int tx = sb + lw + 12;
    if (ui_button_icon(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), g_trashbtn, N.sel >= 0)) delete_note();
    ui_tooltip(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), N.folder == MAXF ? "Apagar Definitivamente" : "Apagar");
    if (ui_button_icon(u, UI_ID, rect_make(tx + 42, TOOLBAR_Y(28), 34, 28), glyph_plus, N.folder != MAXF)) new_note();
    ui_tooltip(u, UI_ID, rect_make(tx + 42, TOOLBAR_Y(28), 34, 28), "Nova Nota");
    if (ui_button_icon(u, UI_ID, rect_make(tx + 84, TOOLBAR_Y(28), 34, 28), g_check, N.sel >= 0 && N.folder != MAXF)) { textarea_focus(N.ta, true); textarea_insert(N.ta, "\n○ "); }
    ui_tooltip(u, UI_ID, rect_make(tx + 84, TOOLBAR_Y(28), 34, 28), "Lista de Verificação");
    if (ui_button_icon(u, UI_ID, rect_make(tx + 126, TOOLBAR_Y(28), 34, 28), g_pin, N.sel >= 0 && N.folder != MAXF)) toggle_pin();
    ui_tooltip(u, UI_ID, rect_make(tx + 126, TOOLBAR_Y(28), 34, 28), "Fixar Nota");
    if (N.folder == MAXF && N.sel >= 0 && ui_toolbar_button(u, UI_ID, rect_make(tx + 168, TOOLBAR_Y(28), 90, 28), "Recuperar", true)) {
        vnode_t *v = vfs_lookup(N.n[N.sel].path);
        if (v) { vfs_move(v, vfs_mkdir(ndir()), v->name); N.cur_path[0] = 0; scan_notes(); open_note(N.nn ? 0 : -1); }
    }
    ui_searchfield(u, UI_ID, rect_make(u->cw - 12 - 200, TOOLBAR_Y(28), 200, 28), N.search, sizeof N.search);
    /* editor */
    rect_t ea = rect_make(sb + lw, w->toolbar_h, u->cw - sb - lw, u->ch - w->toolbar_h);
    if (N.sel >= 0 && N.sel < N.nn) {
        /* data no topo, como no Notas */
        gfx_fill(s, ea.x, ea.y, ea.w, ea.h, T.content_bg);
        datetime_t d;
        rtc_now(&d);
        char dl[64];
        snprintf(dl, sizeof dl, "%d de %s de %d às %02d:%02d", d.day, kit_month[d.month - 1], d.year, d.hour, d.minute);
        ui_label_center(u, rect_make(ea.x, ea.y + 8, ea.w, 16), dl, FONT_REGULAR, 11.5f, T.text3);
        textarea_paint(w, u, N.ta, rect_make(ea.x, ea.y + 28, ea.w, ea.h - 28));
        /* ⌘⇧U: marca/desmarca o item da lista onde está o cursor (troca ○↔● na mesma linha) */
        if ((u->mods & MOD_CMD) && (u->mods & MOD_SHIFT) && (u->key == 'u' || u->key == 'U')) {
            int len;
            const char *t = textarea_text(N.ta, &len);
            char *c = kmalloc(len + 1);
            memcpy(c, t, len + 1);
            char *box = strstr(c, "○");
            if (box) memcpy(box, "●", 3); else if ((box = strstr(c, "●"))) memcpy(box, "○", 3);
            if (box) { textarea_set(N.ta, c, len); textarea_dirty(N.ta, false); vfs_write_file(N.cur_path, c, len); }
            kfree(c);
            u->key_used = true;
        }
    } else {
        gfx_fill(s, ea.x, ea.y, ea.w, ea.h, T.content_bg);
        ui_label_center(u, rect_make(ea.x, ea.y + ea.h / 2 - 12, ea.w, 24), "Nenhuma nota selecionada", FONT_SEMIBOLD, 15, T.text3);
    }
    /* salvamento automático */
    if (textarea_dirty(N.ta, false) && timer_ticks() - N.last_save > 1200) { save_current(); N.last_save = timer_ticks(); if (N.sel >= 0) N.n[N.sel].mtime = timer_ticks(); }
    if ((u->mods & MOD_CMD) && u->key == 'n') { new_note(); u->key_used = true; }
    if ((u->mods & MOD_CMD) && u->key == 'l') { textarea_focus(N.ta, true); textarea_insert(N.ta, "\n○ "); u->key_used = true; }
    if ((u->key == KEY_BACKSPACE || u->key == KEY_DELETE) && !u->focus && !textarea_focused(N.ta) && N.sel >= 0) { delete_note(); u->key_used = true; }
}

static void on_close(window_t *w) { save_current(); N.w = NULL; }
static void launch(app_t *a) {
    if (N.w) { wm_focus(N.w); return; }
    if (!vfs_lookup(ndir())) {
        vfs_mkdir(ndir());
        const char *t = "Bem-vindo ao Notas\n\nSuas notas ficam em Documentos/Notas e são salvas automaticamente.\n\n○ Crie uma pasta\n○ Fixe uma nota importante\n● Abra o Notas\n";
        char p[220];
        snprintf(p, sizeof p, "%s/Bem-vindo.txt", ndir());
        vfs_write_file(p, t, strlen(t));
    }
    if (!N.ta) N.ta = textarea_new(15, 0, 0, 22);
    pins_load();
    scan_folders();
    scan_notes();
    N.w = wm_create(&app_notas, "Notas", -1, -1, 980, 620, WIN_RESIZABLE);
    if (!N.w) return;
    N.w->paint = paint;
    N.w->on_close = on_close;
    N.w->toolbar_h = TOOLBAR_H;
    N.w->sidebar_w = 210;
    N.w->min_w = 760;
    N.w->min_h = 440;
    N.w->title_x = -1;
    open_note(N.nn ? 0 : -1);
}

enum { M_NEW = 1, M_FOLDER, M_DEL, M_PIN, M_CHECK, M_TOGGLE };
static menu_item_t m_file[] = {{"Nova Nota", "⌘N", M_NEW}, {"Nova Pasta", "⇧⌘N", M_FOLDER}, {NULL}, {"Fixar Nota", NULL, M_PIN}, {"Apagar", "⌫", M_DEL}};
static menu_item_t m_fmt[] = {{"Lista de Verificação", "⇧⌘L", M_CHECK}, {"Marcar como Concluído", "⇧⌘U", M_TOGGLE}};
static menu_t menus[] = {{"Arquivo", m_file, 5}, {"Formatar", m_fmt, 2}};
static void on_menu(app_t *a, int act) {
    if (act == M_NEW) new_note();
    else if (act == M_DEL) delete_note();
    else if (act == M_PIN) toggle_pin();
    else if (act == M_CHECK) { textarea_focus(N.ta, true); textarea_insert(N.ta, "\n○ "); }
    else if (act == M_FOLDER) { char d[220]; snprintf(d, sizeof d, "%s/Nova Pasta", ndir()); vfs_mkdir(d); scan_folders(); }
    if (N.w) wm_invalidate(N.w);
}

app_t app_notas = {.name = "Notas", .exec = "notas", .draw_icon = icon_notas, .launch = launch, .on_menu = on_menu, .menus = menus, .nmenus = 2, .pinned = true};
