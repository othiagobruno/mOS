/* mOS — Lembretes: listas inteligentes (Hoje, Agendados, Todos, Sinalizados, Concluídos), listas próprias,
 * datas e horários com aviso, sinalização, prioridade, notas e busca. Dados em ~/Biblioteca/Lembretes/dados.tsv */
#include "kit.h"

#define MAXL 24
#define MAXR 600
typedef struct { char name[48]; int color; } rlist_t;
typedef struct {
    int list, prio;                 /* prio 0 nenhuma, 1 baixa, 2 média, 3 alta */
    bool done, flag, has_date, has_time, notified, used;
    int y, m, d, hh, mi;
    char title[128], notes[256];
    i64 done_at;
} rem_t;

static const u32 lcolors[8] = {RGB(255, 69, 58), RGB(255, 159, 10), RGB(255, 204, 0), RGB(48, 209, 88),
                               RGB(10, 132, 255), RGB(191, 90, 242), RGB(172, 142, 104), RGB(142, 142, 147)};
static const char *lcolor_names[8] = {"Vermelho", "Laranja", "Amarelo", "Verde", "Azul", "Roxo", "Marrom", "Cinza"};

static struct {
    window_t *w;
    rlist_t lists[MAXL];
    int nl;
    rem_t r[MAXR];
    int view;                       /* >= 0: lista; < 0: inteligente */
    int sel;                        /* lembrete selecionado (-1 nenhum) */
    bool show_done, pop_open, renaming, loaded;
    char search[64], rename_buf[48];
    int scroll, sb_scroll;
    int focus_new;                  /* lembrete recém-criado: título ganha foco */
} R = {.view = -1, .sel = -1, .focus_new = -1};

enum { V_HOJE = -1, V_AGEND = -2, V_TODOS = -3, V_SINAL = -4, V_CONCL = -5 };
static const char *smart_names[5] = {"Hoje", "Agendados", "Todos", "Sinalizados", "Concluídos"};
static const u32 smart_colors[5] = {RGB(10, 132, 255), RGB(255, 69, 58), RGB(72, 72, 74), RGB(255, 149, 0), RGB(142, 142, 147)};

extern app_t app_lembretes;

/* ------------------------------------------------------------ persistência */
static void save(void) {
    sbuf_t b = {0};
    sb_puts(&b, "# mOS Lembretes 1\n");
    for (int i = 0; i < R.nl; i++) {
        char nm[48];
        kit_field(nm, sizeof nm, R.lists[i].name);
        sb_printf(&b, "L\t%s\t%d\n", nm, R.lists[i].color);
    }
    for (int i = 0; i < MAXR; i++) {
        rem_t *x = &R.r[i];
        if (!x->used) continue;
        char t[128];
        kit_field(t, sizeof t, x->title);
        sb_printf(&b, "R\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%lld\t%s\t", x->list, x->done, x->flag, x->prio, x->has_date,
                  x->y, x->m, x->d, x->has_time, x->hh, x->mi, (long long)x->done_at, t);
        kit_esc(&b, x->notes);
        sb_putc(&b, '\n');
    }
    char p[200];
    kit_save(kit_lib("Lembretes", "dados.tsv", p, sizeof p), &b);
}

static rem_t *rem_new(int list) {
    for (int i = 0; i < MAXR; i++)
        if (!R.r[i].used) { memset(&R.r[i], 0, sizeof R.r[i]); R.r[i].used = true; R.r[i].list = list; return &R.r[i]; }
    return NULL;
}

static void load(void) {
    if (R.loaded) return;
    R.loaded = true;
    char p[200];
    char *buf = kit_read(kit_lib("Lembretes", "dados.tsv", p, sizeof p), NULL);
    if (!buf) {
        /* primeira abertura: listas padrão e exemplos */
        R.nl = 3;
        strcpy(R.lists[0].name, "Lembretes"); R.lists[0].color = 4;
        strcpy(R.lists[1].name, "Casa"); R.lists[1].color = 1;
        strcpy(R.lists[2].name, "Trabalho"); R.lists[2].color = 5;
        int y, m, d;
        kit_today(&y, &m, &d);
        rem_t *a = rem_new(0); strcpy(a->title, "Experimente o mOS"); strcpy(a->notes, "Clique no círculo para concluir");
        a->has_date = true; a->y = y; a->m = m; a->d = d;
        rem_t *b = rem_new(1); strcpy(b->title, "Comprar pão"); b->flag = true;
        rem_t *c = rem_new(2); strcpy(c->title, "Revisar o relatório"); c->prio = 3;
        save();
        return;
    }
    char *cur = buf, *ln, *f[16];
    while ((ln = kit_line(&cur))) {
        if (ln[0] == 'L' && ln[1] == '\t' && R.nl < MAXL) {
            int n = kit_split(ln, f, 16);
            if (n >= 3) { snprintf(R.lists[R.nl].name, 48, "%s", f[1]); R.lists[R.nl].color = CLAMP(atoi(f[2]), 0, 7); R.nl++; }
        } else if (ln[0] == 'R' && ln[1] == '\t') {
            int n = kit_split(ln, f, 16);
            if (n < 15) continue;
            rem_t *x = rem_new(atoi(f[1]));
            if (!x) break;
            x->done = atoi(f[2]); x->flag = atoi(f[3]); x->prio = atoi(f[4]); x->has_date = atoi(f[5]);
            x->y = atoi(f[6]); x->m = atoi(f[7]); x->d = atoi(f[8]); x->has_time = atoi(f[9]); x->hh = atoi(f[10]); x->mi = atoi(f[11]);
            x->done_at = strtol(f[12], NULL, 10);
            snprintf(x->title, sizeof x->title, "%s", f[13]);
            snprintf(x->notes, sizeof x->notes, "%s", f[14]);
            kit_unesc(x->notes);
            if (x->list >= R.nl) x->list = 0;
        }
    }
    kfree(buf);
    if (!R.nl) { R.nl = 1; strcpy(R.lists[0].name, "Lembretes"); R.lists[0].color = 4; }
}

/* ------------------------------------------------------------ filtros */
static i64 rdays(rem_t *x) { return kit_days(x->y, x->m, x->d); }
static i64 today_days(void) { int y, m, d; kit_today(&y, &m, &d); return kit_days(y, m, d); }

static bool matches(rem_t *x, int view) {
    if (!x->used) return false;
    if (R.search[0]) {
        char a[400];
        snprintf(a, sizeof a, "%s %s", x->title, x->notes);
        for (char *p = a; *p; p++) *p = (char)tolower(*p);
        char q[64];
        snprintf(q, sizeof q, "%s", R.search);
        for (char *p = q; *p; p++) *p = (char)tolower(*p);
        return strstr(a, q) != NULL;
    }
    switch (view) {
    case V_HOJE: return !x->done && x->has_date && rdays(x) <= today_days();
    case V_AGEND: return !x->done && x->has_date;
    case V_TODOS: return !x->done || R.show_done;
    case V_SINAL: return x->flag && !x->done;
    case V_CONCL: return x->done;
    default: return x->list == view && (!x->done || R.show_done);
    }
}
static int count(int view) {
    bool sd = R.show_done;
    char s0 = R.search[0];
    R.show_done = false; R.search[0] = 0;
    int n = 0;
    for (int i = 0; i < MAXR; i++) if (matches(&R.r[i], view)) n++;
    R.show_done = sd; R.search[0] = s0;
    return n;
}
static u32 view_color(int v) { return v >= 0 ? lcolors[R.lists[v].color] : smart_colors[-v - 1]; }
static const char *view_name(int v) { return R.search[0] ? "Resultados" : v >= 0 ? R.lists[v].name : smart_names[-v - 1]; }

/* ------------------------------------------------------------ glifos */
static void g_list(surface_t *s, int x, int y, int sz, u32 c) {
    for (int i = 0; i < 3; i++) {
        float yy = y + sz * (0.28f + i * 0.22f);
        gfx_circle(s, x + sz * 0.28f, yy, sz * 0.06f, c);
        gfx_fill(s, (int)(x + sz * 0.42f), (int)(yy - sz * 0.04f), (int)(sz * 0.36f), MAX(1, (int)(sz * 0.08f)), c);
    }
}
static void g_flag(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_line(s, x + sz * 0.3f, y + sz * 0.18f, x + sz * 0.3f, y + sz * 0.86f, MAX(1.2f, sz * 0.09f), c);
    float f[] = {x + sz * 0.3f, y + sz * 0.18f, x + sz * 0.78f, y + sz * 0.32f, x + sz * 0.3f, y + sz * 0.5f};
    gfx_polygon(s, f, 3, c);
}
static void g_cal(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_round_rect_stroke(s, x + sz * 0.18f, y + sz * 0.24f, sz * 0.64f, sz * 0.58f, sz * 0.1f, MAX(1.2f, sz * 0.08f), c);
    gfx_fill(s, (int)(x + sz * 0.18f), (int)(y + sz * 0.36f), (int)(sz * 0.64f), MAX(1, (int)(sz * 0.08f)), c);
}
static void g_tray(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_round_rect_stroke(s, x + sz * 0.16f, y + sz * 0.3f, sz * 0.68f, sz * 0.48f, sz * 0.08f, MAX(1.2f, sz * 0.08f), c);
    gfx_fill(s, (int)(x + sz * 0.16f), (int)(y + sz * 0.55f), (int)(sz * 0.68f), MAX(1, (int)(sz * 0.08f)), c);
}
static void g_info(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_circle_stroke(s, x + sz / 2.0f, y + sz / 2.0f, sz * 0.42f, MAX(1.2f, sz * 0.08f), c);
    gfx_fill(s, x + sz / 2 - 1, (int)(y + sz * 0.44f), 2, (int)(sz * 0.3f), c);
    gfx_circle(s, x + sz / 2.0f, y + sz * 0.3f, sz * 0.07f, c);
}
static void smart_glyph(int i, surface_t *s, int x, int y, int sz, u32 c) {
    if (i == 0) { g_cal(s, x, y, sz, c); char d[4]; int yy, mm, dd; kit_today(&yy, &mm, &dd); snprintf(d, 4, "%d", dd);
                  text_draw_center(s, font_get(FONT_BOLD), sz * 0.36f, rect_make(x, (int)(y + sz * 0.36f), sz, (int)(sz * 0.44f)), c, d); }
    else if (i == 1) g_cal(s, x, y, sz, c);
    else if (i == 2) g_tray(s, x, y, sz, c);
    else if (i == 3) g_flag(s, x, y, sz, c);
    else glyph_check(s, x, y, sz, c);
}

/* ------------------------------------------------------------ ações */
static void new_reminder(void) {
    int l = R.view >= 0 ? R.view : 0;
    rem_t *x = rem_new(l);
    if (!x) return;
    if (R.view == V_HOJE || R.view == V_AGEND) { x->has_date = true; kit_today(&x->y, &x->m, &x->d); }
    if (R.view == V_SINAL) x->flag = true;
    R.search[0] = 0;
    if (R.view == V_CONCL) R.view = l;
    R.sel = (int)(x - R.r);
    R.focus_new = R.sel;
    R.pop_open = false;
    save();
}
static void new_list(void) {
    if (R.nl >= MAXL) return;
    int k = 1;
    char nm[48];
    for (;;) {
        snprintf(nm, sizeof nm, k == 1 ? "Nova Lista" : "Nova Lista %d", k);
        bool dup = false;
        for (int i = 0; i < R.nl; i++) if (!strcmp(R.lists[i].name, nm)) dup = true;
        if (!dup) break;
        k++;
    }
    snprintf(R.lists[R.nl].name, 48, "%s", nm);
    R.lists[R.nl].color = R.nl % 8;
    R.view = R.nl++;
    R.renaming = true;
    snprintf(R.rename_buf, sizeof R.rename_buf, "%s", nm);
    save();
}
static void delete_sel(void) {
    if (R.sel < 0) return;
    R.r[R.sel].used = false;
    R.sel = -1;
    R.pop_open = false;
    save();
}
static void delete_list_cb(int b, void *ctx) {
    int l = (int)(i64)ctx;
    if (b != 1 || l < 0 || l >= R.nl || R.nl <= 1) return;
    for (int i = 0; i < MAXR; i++) {
        if (!R.r[i].used) continue;
        if (R.r[i].list == l) R.r[i].used = false;
        else if (R.r[i].list > l) R.r[i].list--;
    }
    memmove(&R.lists[l], &R.lists[l + 1], sizeof(rlist_t) * (R.nl - l - 1));
    R.nl--;
    R.view = V_HOJE;
    R.sel = -1;
    save();
}
static void delete_list(void) {
    if (R.view < 0 || R.nl <= 1) return;
    static const char *b[] = {"Cancelar", "Apagar"};
    char t[120];
    snprintf(t, sizeof t, "Apagar a lista \"%s\"?", R.lists[R.view].name);
    alert_show(t, "Todos os lembretes desta lista também serão apagados.", b, 2, delete_list_cb, (void *)(i64)R.view);
}

/* ------------------------------------------------------------ desenho */
static void paint_sidebar(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w, y = w->toolbar_h + 2;
    ui_searchfield(u, UI_ID, rect_make(12, y, sb - 24, 28), R.search, sizeof R.search);
    y += 40;
    /* cartões das listas inteligentes: 2 colunas */
    int cw = (sb - 24 - 8) / 2, chh = 62;
    for (int i = 0; i < 5; i++) {
        int col = i % 2, row = i / 2;
        rect_t r = rect_make(12 + col * (cw + 8), y + row * (chh + 8), i == 4 ? cw : cw, chh);
        bool sel = R.view == -i - 1 && !R.search[0];
        bool hov = ui_hit(u, r);
        u32 bg = sel ? smart_colors[i] : (T.dark ? RGBA(255, 255, 255, hov ? 34 : 22) : RGBA(255, 255, 255, hov ? 230 : 190));
        gfx_round_rect(s, r.x, r.y, r.w, r.h, 11, bg);
        if (!sel && !T.dark) gfx_round_rect_stroke(s, r.x, r.y, r.w, r.h, 11, 1, RGBA(0, 0, 0, 14));
        u32 ic = sel ? RGB(255, 255, 255) : smart_colors[i];
        gfx_circle(s, r.x + 22, r.y + 21, 13, sel ? RGBA(255, 255, 255, 60) : ic);
        smart_glyph(i, s, r.x + 12, r.y + 11, 20, RGB(255, 255, 255));
        char n[8];
        snprintf(n, sizeof n, "%d", count(-i - 1));
        int tw = text_width(font_get(FONT_BOLD), 21, n);
        text_draw(s, font_get(FONT_BOLD), 21, r.x + r.w - 12 - tw, r.y + 29, sel ? RGB(255, 255, 255) : T.text, n);
        text_draw(s, font_get(FONT_SEMIBOLD), 13, r.x + 12, r.y + 52, sel ? RGB(255, 255, 255) : T.text2, smart_names[i]);
        if (ui_clicked(u, 11000 + i, r)) { R.view = -i - 1; R.sel = -1; R.search[0] = 0; R.pop_open = false; R.renaming = false; }
    }
    y += 3 * (chh + 8) + 6;
    ui_label(u, 18, y, "Minhas Listas", FONT_BOLD, 11.5f, T.text2);
    y += 20;
    rect_t view = rect_make(0, y, sb, u->ch - y - 40);
    rect_t pc = surface_push_clip(s, view);
    int yy = y - R.sb_scroll;
    for (int i = 0; i < R.nl; i++) {
        rect_t r = rect_make(10, yy, sb - 20, 32);
        bool sel = R.view == i && !R.search[0];
        if (sel) gfx_round_rect(s, r.x, r.y, r.w, r.h, 8, T.sidebar_sel);
        u32 c = lcolors[R.lists[i].color];
        gfx_circle(s, r.x + 18, r.y + 16, 11, c);
        g_list(s, r.x + 9, r.y + 7, 18, RGB(255, 255, 255));
        if (R.renaming && sel) {
            if (ui_textfield(u, 11500, rect_make(r.x + 36, r.y + 4, r.w - 44, 24), R.rename_buf, sizeof R.rename_buf, "Nome da lista") ||
                (u->focus != 11500 && u->pressed && !ui_hit(u, r))) {
                if (R.rename_buf[0]) snprintf(R.lists[i].name, 48, "%s", R.rename_buf);
                R.renaming = false;
                save();
            }
            if (!R.renaming) {} else if (u->focus != 11500 && !u->pressed) u->focus = 11500;
        } else {
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 13.5f, r.x + 36, r.y + 21, r.w - 80, T.text, R.lists[i].name);
            char n[8];
            snprintf(n, sizeof n, "%d", count(i));
            int tw = text_width(font_get(FONT_REGULAR), 13, n);
            text_draw(s, font_get(FONT_REGULAR), 13, r.x + r.w - 10 - tw, r.y + 21, T.text2, n);
        }
        if (ui_clicked(u, 11100 + i, r)) {
            if (R.view == i && u->dblclick) { R.renaming = true; snprintf(R.rename_buf, 48, "%s", R.lists[i].name); }
            else if (R.view != i) R.renaming = false;
            R.view = i; R.sel = -1; R.search[0] = 0; R.pop_open = false;
        }
        yy += 34;
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, R.nl * 34 + 6, &R.sb_scroll);
    if (ui_button_style(u, UI_ID, rect_make(10, u->ch - 36, 140, 28), "Adicionar Lista", BTN_PLAIN_GRAY, true, glyph_plus)) new_list();
}

static void draw_circle_check(surface_t *s, float cx, float cy, bool done, u32 c, bool hov) {
    if (done) {
        gfx_circle(s, cx, cy, 10, c);
        gfx_circle(s, cx, cy, 8.2f, T.content_bg);
        gfx_circle(s, cx, cy, 6.2f, c);
    } else {
        gfx_circle_stroke(s, cx, cy, 10, 1.4f, hov ? c : (T.dark ? RGBA(255, 255, 255, 90) : RGBA(0, 0, 0, 70)));
    }
}

static void paint_popover(window_t *w, ui_t *u, rect_t anchor) {
    if (!R.pop_open || R.sel < 0) return;
    rem_t *x = &R.r[R.sel];
    int pw = 300, ph = 330;
    rect_t p = ui_popover_frame(u, anchor, pw, ph);
    u->modal_next = rect_make(0, 0, u->cw, u->ch);
    if (u->pressed && !rect_contains(p, u->mx, u->my)) { R.pop_open = false; u->repaint = true; return; }
    u->in_overlay = true;
    surface_t *s = u->s;
    int y = p.y + 14;
    ui_textfield(u, UI_ID, rect_make(p.x + 14, y, pw - 28, 26), x->title, sizeof x->title, "Título");
    y += 32;
    ui_textfield(u, UI_ID, rect_make(p.x + 14, y, pw - 28, 26), x->notes, sizeof x->notes, "Notas");
    y += 38;
    rect_t g = rect_make(p.x + 10, y, pw - 20, 4 * 34);
    ui_group_box(u, g);
    rect_t c = ui_form_row(u, g, 0, 34, 4, "Data");
    bool hd = x->has_date;
    if (ui_switch_ex(u, UI_ID, c.x + c.w - 40, c.y + (c.h - 18) / 2, 18, &hd, true)) {
        x->has_date = hd;
        if (hd && !x->y) kit_today(&x->y, &x->m, &x->d);
        x->notified = false;
    }
    if (x->has_date) {
        if (!x->y) kit_today(&x->y, &x->m, &x->d);
        if (ui_date_field(u, UI_ID, rect_make(c.x + c.w - 40 - 8 - 118, c.y + 4, 118, c.h - 8), &x->y, &x->m, &x->d)) x->notified = false;
    }
    c = ui_form_row(u, g, 1, 34, 4, "Horário");
    bool ht = x->has_time;
    if (ui_switch_ex(u, UI_ID, c.x + c.w - 40, c.y + (c.h - 18) / 2, 18, &ht, x->has_date)) { x->has_time = ht; x->notified = false; if (ht && !x->hh) x->hh = 9; }
    if (x->has_time && x->has_date) {
        int hh = x->hh, mi = x->mi;
        ui_stepper(u, UI_ID, rect_make(c.x + c.w - 48 - 2 * 58, c.y + 5, 52, c.h - 10), &hh, 0, 23);
        ui_stepper(u, UI_ID, rect_make(c.x + c.w - 48 - 58, c.y + 5, 52, c.h - 10), &mi, 0, 59);
        if (hh != x->hh || mi != x->mi) { x->hh = hh; x->mi = mi; x->notified = false; }
    }
    c = ui_form_row(u, g, 2, 34, 4, "Sinalizado");
    bool fl = x->flag;
    if (ui_switch_ex(u, UI_ID, c.x + c.w - 40, c.y + (c.h - 18) / 2, 18, &fl, true)) x->flag = fl;
    c = ui_form_row(u, g, 3, 34, 4, "Prioridade");
    static const char *pr[] = {"Nenhuma", "Baixa", "Média", "Alta"};
    x->prio = ui_popup_inline(u, UI_ID, c, pr, 4, x->prio);
    y += 4 * 34 + 12;
    rect_t g2 = rect_make(p.x + 10, y, pw - 20, 34);
    ui_group_box(u, g2);
    c = ui_form_row(u, g2, 0, 34, 1, "Lista");
    const char *ln[MAXL];
    for (int i = 0; i < R.nl; i++) ln[i] = R.lists[i].name;
    x->list = ui_popup_inline(u, UI_ID, c, ln, R.nl, x->list);
    (void)s;
    u->in_overlay = false;
    save();
}

static void paint_main(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w, top = w->toolbar_h;
    rect_t area = rect_make(sb, 0, u->cw - sb, u->ch);
    gfx_fill(s, area.x, top, area.w, u->ch - top, T.content_bg);
    /* barra de ferramentas */
    rect_t bplus = rect_make(u->cw - 12 - 32, TOOLBAR_Y(28), 32, 28);
    if (ui_button_icon(u, UI_ID, bplus, glyph_plus, R.nl > 0)) new_reminder();
    ui_tooltip(u, UI_ID, bplus, "Novo Lembrete");
    rect_t bdone = rect_make(bplus.x - 8 - 132, TOOLBAR_Y(28), 132, 28);
    if (R.view >= 0 && !R.search[0]) {
        int c = ui_popup(u, UI_ID, rect_make(bdone.x - 8 - 110, TOOLBAR_Y(28), 110, 28), lcolor_names, 8, R.lists[R.view].color);
        if (c != R.lists[R.view].color) { R.lists[R.view].color = c; save(); }
    }
    if (R.view != V_CONCL && ui_toolbar_button(u, UI_ID, bdone, R.show_done ? "Ocultar Concluídos" : "Mostrar Concluídos", true)) R.show_done = !R.show_done;
    u32 vc = view_color(R.view);
    const char *vn = view_name(R.view);
    int ty = top + 8;
    text_draw(s, font_get(FONT_BOLD), 28, area.x + 22, ty + 28, R.view == V_TODOS && !R.search[0] ? T.text : vc, vn);
    int n = 0;
    for (int i = 0; i < MAXR; i++) if (matches(&R.r[i], R.view)) n++;
    char cn[8];
    snprintf(cn, sizeof cn, "%d", n);
    int tw = text_width(font_get(FONT_BOLD), 28, cn);
    text_draw(s, font_get(FONT_BOLD), 28, u->cw - 24 - tw, ty + 28, R.view == V_TODOS ? T.text : vc, cn);
    int ly = ty + 48;
    rect_t view = rect_make(area.x, ly, area.w, u->ch - ly);
    rect_t pc = surface_push_clip(s, view);
    int y = ly - R.scroll;
    rect_t anchor = rect_make(0, 0, 0, 0);
    int shown = 0, last_list = -99;
    for (int pass = 0; pass < (R.view == V_TODOS || R.view == V_AGEND ? R.nl : 1); pass++) {
        for (int i = 0; i < MAXR; i++) {
            rem_t *x = &R.r[i];
            if (!matches(x, R.view)) continue;
            if ((R.view == V_TODOS || R.view == V_AGEND) && x->list != pass) continue;
            if ((R.view == V_TODOS || R.view == V_AGEND) && last_list != pass) {
                /* cabeçalho de grupo com o nome da lista */
                last_list = pass;
                text_draw(s, font_get(FONT_BOLD), 17, area.x + 22, y + 26, lcolors[R.lists[pass].color], R.lists[pass].name);
                y += 36;
            }
            bool two = x->notes[0] || x->has_date || (R.view < 0 && R.view != V_TODOS && R.view != V_AGEND);
            int rh = two ? 52 : 36;
            rect_t row = rect_make(area.x, y, area.w, rh);
            bool sel = R.sel == i;
            if (sel) gfx_round_rect(s, row.x + 8, row.y + 1, row.w - 16, row.h - 2, 8, T.dark ? RGBA(255, 255, 255, 22) : RGBA(0, 0, 0, 14));
            u32 lc = lcolors[R.lists[x->list].color];
            rect_t circ = rect_make(row.x + 18, row.y + 6, 24, 24);
            draw_circle_check(s, circ.x + 12, circ.y + 12, x->done, lc, ui_hit(u, circ));
            int tx = row.x + 52;
            /* prioridade: !, !!, !!! */
            char pre[8] = "";
            for (int k = 0; k < x->prio; k++) strcat(pre, "!");
            int px = 0;
            if (pre[0]) { px = text_draw(s, font_get(FONT_SEMIBOLD), 14, tx, row.y + 23, lc, pre) + 5; }
            if (sel && !x->done) {
                rect_t tf = rect_make(tx + px - 4, row.y + 5, row.w - 52 - px - 60, 24);
                if (R.focus_new == i) { u->focus = 12000 + i; u->caret = (int)strlen(x->title); u->sel_anchor = u->caret; R.focus_new = -1; }
                /* campo sem moldura: desenha por cima do fundo da linha */
                if (ui_textfield(u, 12000 + i, tf, x->title, sizeof x->title, "Novo Lembrete")) { save(); if (x->title[0]) new_reminder(); }
            } else {
                text_draw_ellipsis(s, font_get(FONT_REGULAR), 14, tx + px, row.y + 23, row.w - 52 - px - 60, x->done ? T.text3 : T.text, x->title[0] ? x->title : "Novo Lembrete");
            }
            int sy = row.y + 41;
            int sx = tx;
            if (R.view < 0 && R.view != V_TODOS && R.view != V_AGEND) {
                sx += text_draw(s, font_get(FONT_REGULAR), 12, sx, sy, T.text2, R.lists[x->list].name) + 8;
            }
            if (x->has_date) {
                char dt[48];
                i64 dd = rdays(x), td = today_days();
                if (dd == td) snprintf(dt, sizeof dt, "Hoje");
                else if (dd == td + 1) snprintf(dt, sizeof dt, "Amanhã");
                else if (dd == td - 1) snprintf(dt, sizeof dt, "Ontem");
                else kit_fmt_short(dt, sizeof dt, x->y, x->m, x->d);
                if (x->has_time) { usize l = strlen(dt); snprintf(dt + l, sizeof dt - l, ", %02d:%02d", x->hh, x->mi); }
                bool overdue = !x->done && (dd < td || (dd == td && x->has_time && x->hh * 60 + x->mi < (int)((kit_now_secs() % 86400) / 60)));
                sx += text_draw(s, font_get(FONT_REGULAR), 12, sx, sy, overdue ? RGB(255, 69, 58) : T.text2, dt) + 8;
            }
            if (x->notes[0]) {
                char one[96];
                int k = 0;
                for (; x->notes[k] && x->notes[k] != '\n' && k < 95; k++) one[k] = x->notes[k];
                one[k] = 0;
                text_draw_ellipsis(s, font_get(FONT_REGULAR), 12, sx, sy, row.x + row.w - 60 - sx, T.text2, one);
            }
            if (x->flag) g_flag(s, row.x + row.w - 48, row.y + 9, 18, RGB(255, 149, 0));
            if (sel) {
                rect_t ib = rect_make(row.x + row.w - 28, row.y + 8, 20, 20);
                g_info(s, ib.x, ib.y, 20, T.accent);
                if (ui_clicked(u, 13000 + i, ib)) R.pop_open = !R.pop_open;
                anchor = ib;
            }
            gfx_fill(s, tx, row.y + rh - 1, row.x + row.w - tx - 10, 1, T.separator);
            if (ui_clicked(u, 14000 + i, circ)) {
                x->done = !x->done;
                x->done_at = x->done ? kit_now_secs() : 0;
                if (x->done) sound_click();
                save();
            } else if (ui_clicked(u, 15000 + i, row) && R.sel != i) { R.sel = i; R.pop_open = false; }
            y += rh;
            shown++;
        }
    }
    if (!shown) {
        const char *msg = R.search[0] ? "Nenhum resultado" : R.view == V_CONCL ? "Nenhum Lembrete Concluído" : "Nenhum Lembrete";
        ui_label_center(u, rect_make(area.x, ly + 60, area.w, 30), msg, FONT_SEMIBOLD, 17, T.text3);
    }
    /* clicar no vazio abaixo da lista cria um lembrete (como no macOS) */
    rect_t empty = rect_make(area.x, y, area.w, MAX(0, view.y + view.h - y));
    if (empty.h > 0 && ui_clicked(u, UI_ID, empty)) {
        if (R.sel >= 0) { R.sel = -1; R.pop_open = false; }
        else if (R.view != V_CONCL && !R.search[0]) new_reminder();
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, y + R.scroll - ly + 40, &R.scroll);
    /* teclado */
    if (u->key && !u->focus) {
        if ((u->key == KEY_BACKSPACE || u->key == KEY_DELETE) && R.sel >= 0) { delete_sel(); u->key_used = true; }
        else if (u->key == KEY_ESC) { R.sel = -1; R.pop_open = false; u->key_used = true; }
        else if (u->key == KEY_ENTER && R.sel >= 0) { u->focus = 12000 + R.sel; u->key_used = true; }
    }
    if (u->key == 'n' && (u->mods & MOD_CMD)) { if (u->mods & MOD_SHIFT) new_list(); else new_reminder(); u->key_used = true; }
    paint_popover(w, u, anchor);
}

static void paint(window_t *w, ui_t *u) {
    paint_sidebar(w, u);
    paint_main(w, u);
}

/* avisos de lembretes com horário */
static void tick(app_t *a) {
    static u64 last;
    u64 now = timer_ticks();
    if (now - last < 1000) return;
    last = now;
    load();
    i64 ns = kit_now_secs();
    int badge = 0;
    for (int i = 0; i < MAXR; i++) {
        rem_t *x = &R.r[i];
        if (!x->used || x->done || !x->has_date) continue;
        i64 when = kit_days(x->y, x->m, x->d) * 86400 + (x->has_time ? x->hh * 3600 + x->mi * 60 : 9 * 3600);
        if (!x->notified && x->has_time && when <= ns && ns - when < 3600) {
            x->notified = true;
            notify("Lembretes", x->title[0] ? x->title : "Lembrete");
            sound_alert();
        }
    }
    badge = count(V_HOJE);
    if (a->badge != badge) dock_set_badge(a, badge);
}

static void on_close(window_t *w) { save(); R.w = NULL; R.pop_open = false; }

static void launch(app_t *a) {
    if (R.w) { wm_focus(R.w); return; }
    load();
    R.w = wm_create(&app_lembretes, "Lembretes", -1, -1, 880, 580, WIN_RESIZABLE);
    if (!R.w) return;
    R.w->paint = paint;
    R.w->on_close = on_close;
    R.w->toolbar_h = TOOLBAR_H;
    R.w->sidebar_w = 260;
    R.w->min_w = 640;
    R.w->min_h = 420;
    R.w->title_x = -1;
}

enum { M_NEW = 1, M_NEWLIST, M_DEL, M_DELLIST, M_SHOWDONE, M_FLAG, M_TODAY };
static menu_item_t m_file[] = {{"Novo Lembrete", "⌘N", M_NEW}, {"Nova Lista", "⇧⌘N", M_NEWLIST}, {NULL}, {"Apagar Lista…", NULL, M_DELLIST}};
static menu_item_t m_edit[] = {{"Apagar", "⌫", M_DEL}, {"Sinalizar", "⇧⌘F", M_FLAG}, {"Para Hoje", NULL, M_TODAY}};
static menu_item_t m_view[] = {{"Mostrar Concluídos", "⇧⌘H", M_SHOWDONE}};
static menu_t menus[] = {{"Arquivo", m_file, 4}, {"Editar", m_edit, 3}, {"Visualizar", m_view, 1}};

static void on_menu(app_t *a, int act) {
    switch (act) {
    case M_NEW: new_reminder(); break;
    case M_NEWLIST: new_list(); break;
    case M_DEL: delete_sel(); break;
    case M_DELLIST: delete_list(); break;
    case M_SHOWDONE: R.show_done = !R.show_done; m_view[0].checked = R.show_done; break;
    case M_FLAG: if (R.sel >= 0) { R.r[R.sel].flag = !R.r[R.sel].flag; save(); } break;
    case M_TODAY: if (R.sel >= 0) { rem_t *x = &R.r[R.sel]; x->has_date = true; kit_today(&x->y, &x->m, &x->d); save(); } break;
    }
    if (R.w) wm_invalidate(R.w);
}

static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(255, 255, 255), RGB(236, 236, 240));
    static const u32 c[3] = {RGB(10, 132, 255), RGB(255, 69, 58), RGB(255, 159, 10)};
    for (int i = 0; i < 3; i++) {
        float yy = y + (20 + i * 12) * u;
        gfx_circle(s, x + 18 * u, yy, 4.2f * u, c[i]);
        gfx_circle(s, x + 18 * u, yy, 2.6f * u, RGB(255, 255, 255));
        gfx_circle(s, x + 18 * u, yy, 1.8f * u, c[i]);
        gfx_fill(s, (int)(x + 26 * u), (int)(yy - 0.8f * u), (int)(24 * u), MAX(1, (int)(1.6f * u)), RGB(200, 200, 206));
    }
}

app_t app_lembretes = {.name = "Lembretes", .exec = "lembretes", .draw_icon = icon, .launch = launch, .tick = tick,
                       .on_menu = on_menu, .menus = menus, .nmenus = 3, .pinned = true, .always_tick = true};
