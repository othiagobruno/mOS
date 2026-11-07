/* mOS — Atalhos: galeria de atalhos coloridos, editor de ações (abrir app, abrir site, notificação, esperar,
 * volume, aparência clara/escura, criar nota, criar lembrete, copiar texto, som, captura de tela) e
 * execução passo a passo com progresso. Dados em ~/Biblioteca/Atalhos/atalhos.tsv */
#include "kit.h"

extern app_t app_atalhos;
void desktop_screenshot_ui(int mode);
bool reminders_add(const char *title);

enum { A_APP, A_URL, A_NOTIFY, A_WAIT, A_VOLUME, A_THEME, A_NOTE, A_REMINDER, A_COPY, A_SOUND, A_SHOT, A_KINDS };
static const char *act_names[A_KINDS] = {"Abrir App", "Abrir URL", "Mostrar Notificação", "Esperar", "Definir Volume", "Definir Aparência",
                                         "Criar Nota", "Criar Lembrete", "Copiar para a Área de Transferência", "Tocar Som", "Tirar Captura de Tela"};
static const u32 act_colors[A_KINDS] = {RGB(10, 132, 255), RGB(10, 132, 255), RGB(255, 59, 48), RGB(142, 142, 147), RGB(255, 45, 85), RGB(88, 86, 214),
                                        RGB(255, 204, 0), RGB(255, 149, 0), RGB(48, 199, 89), RGB(255, 45, 85), RGB(142, 142, 147)};
#define MAXSC 24
#define MAXA 16
typedef struct { int kind; char arg[160]; int num; } action_t;
typedef struct { bool used; char name[48]; int color; action_t a[MAXA]; int na; } shortcut_t;
static const u32 sc_colors[8] = {RGB(10, 132, 255), RGB(255, 59, 48), RGB(255, 149, 0), RGB(48, 199, 89), RGB(175, 82, 222), RGB(255, 45, 85), RGB(90, 200, 250), RGB(142, 142, 147)};

static struct {
    window_t *w;
    shortcut_t s[MAXSC];
    bool loaded;
    int edit;                 /* atalho aberto no editor (-1 = galeria) */
    int running, step;        /* execução */
    u64 wait_until;
    int scroll;
    int add_kind;
} AT = {.edit = -1, .running = -1};

static void save(void) {
    sbuf_t b = {0};
    sb_puts(&b, "# mOS Atalhos 1\n");
    for (int i = 0; i < MAXSC; i++) {
        shortcut_t *s = &AT.s[i];
        if (!s->used) continue;
        char nm[48];
        kit_field(nm, 48, s->name);
        sb_printf(&b, "S\t%s\t%d\n", nm, s->color);
        for (int k = 0; k < s->na; k++) {
            char a[160];
            kit_field(a, 160, s->a[k].arg);
            sb_printf(&b, "A\t%d\t%d\t%s\n", s->a[k].kind, s->a[k].num, a);
        }
    }
    char p[200];
    kit_save(kit_lib("Atalhos", "atalhos.tsv", p, sizeof p), &b);
}
static shortcut_t *sc_new(const char *name, int color) {
    for (int i = 0; i < MAXSC; i++) if (!AT.s[i].used) { memset(&AT.s[i], 0, sizeof AT.s[i]); AT.s[i].used = true; snprintf(AT.s[i].name, 48, "%s", name); AT.s[i].color = color; return &AT.s[i]; }
    return NULL;
}
static void add_action(shortcut_t *s, int kind, const char *arg, int num) {
    if (s->na >= MAXA) return;
    s->a[s->na].kind = kind;
    snprintf(s->a[s->na].arg, 160, "%s", arg ? arg : "");
    s->a[s->na].num = num;
    s->na++;
}
static void load(void) {
    if (AT.loaded) return;
    AT.loaded = true;
    char p[200];
    char *buf = kit_read(kit_lib("Atalhos", "atalhos.tsv", p, sizeof p), NULL);
    if (!buf) {
        shortcut_t *s = sc_new("Modo Foco", 4);
        add_action(s, A_THEME, "", 1); add_action(s, A_VOLUME, "", 20); add_action(s, A_NOTIFY, "Modo Foco ativado", 0);
        s = sc_new("Pausa de 5 minutos", 3);
        add_action(s, A_NOTIFY, "Pausa iniciada", 0); add_action(s, A_WAIT, "", 300); add_action(s, A_SOUND, "", 0); add_action(s, A_NOTIFY, "Fim da pausa", 0);
        s = sc_new("Início do Dia", 2);
        add_action(s, A_APP, "calendario", 0); add_action(s, A_APP, "lembretes", 0); add_action(s, A_APP, "tempo", 0);
        s = sc_new("Anotar Ideia", 5);
        add_action(s, A_NOTE, "Nova ideia", 0); add_action(s, A_APP, "notas", 0);
        save();
        return;
    }
    char *cur = buf, *ln, *f[5];
    shortcut_t *s = NULL;
    while ((ln = kit_line(&cur))) {
        int n = kit_split(ln, f, 5);
        if (!strcmp(f[0], "S") && n >= 3) s = sc_new(f[1], CLAMP(atoi(f[2]), 0, 7));
        else if (!strcmp(f[0], "A") && n >= 4 && s) add_action(s, CLAMP(atoi(f[1]), 0, A_KINDS - 1), f[3], atoi(f[2]));
    }
    kfree(buf);
}

/* ------------------------------------------------------------ execução */
static void run(int i) { if (AT.running >= 0) return; AT.running = i; AT.step = 0; AT.wait_until = 0; }
static void exec_step(void) {
    if (AT.running < 0) return;
    shortcut_t *s = &AT.s[AT.running];
    if (AT.wait_until) { if (timer_ticks() < AT.wait_until) return; AT.wait_until = 0; AT.step++; }
    while (AT.step < s->na) {
        action_t *a = &s->a[AT.step];
        switch (a->kind) {
        case A_APP: { app_t *ap = apps_find(a->arg); if (ap) app_launch(ap); } break;
        case A_URL: app_launch(&app_horizonte); browser_open_url(a->arg[0] ? a->arg : "https://www.openstreetmap.org"); break;
        case A_NOTIFY: notify(s->name, a->arg); break;
        case A_WAIT: AT.wait_until = timer_ticks() + (u64)MAX(1, a->num) * 1000; return;
        case A_VOLUME: audio_set_volume(CLAMP(a->num, 0, 100)); break;
        case A_THEME: theme_set(a->num == 2 ? !T.dark : a->num == 1, g_accent_index); wm_damage_all(); break;
        case A_NOTE: {
            char p[240];
            datetime_t d;
            rtc_now(&d);
            snprintf(p, sizeof p, "%s/Documentos/Notas/Atalho %02d-%02d %02d%02d%02d.txt", CUR_HOME, d.day, d.month, d.hour, d.minute, d.second);
            vfs_write_file(p, a->arg, strlen(a->arg));
        } break;
        case A_REMINDER: reminders_add(a->arg); break;
        case A_COPY: clipboard_set(a->arg, -1); break;
        case A_SOUND: sound_alert(); break;
        case A_SHOT: desktop_screenshot_ui(1); break;
        }
        AT.step++;
    }
    AT.running = -1;
}

/* ------------------------------------------------------------ desenho */
static void paint_gallery(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    ui_label(u, a.x + 24, a.y + 16, "Todos os Atalhos", FONT_BOLD, 22, T.text);
    int cw = 200, ch = 112, gap = 16;
    int cols = MAX(1, (a.w - 48 + gap) / (cw + gap));
    cw = (a.w - 48 - gap * (cols - 1)) / cols;
    rect_t view = rect_make(a.x, a.y + 54, a.w, a.h - 54);
    rect_t pc = surface_push_clip(s, view);
    int k = 0;
    for (int i = 0; i < MAXSC; i++) {
        shortcut_t *sc = &AT.s[i];
        if (!sc->used) continue;
        rect_t r = rect_make(a.x + 24 + (k % cols) * (cw + gap), view.y + 6 + (k / cols) * (ch + gap) - AT.scroll, cw, ch);
        k++;
        u32 c = sc_colors[sc->color];
        bool run_now = AT.running == i;
        gfx_shadow(s, r.x, r.y, r.w, r.h, 16, 8, 40, 2);
        gfx_round_rect_grad(s, r.x, r.y, r.w, r.h, 16, color_lighten(c, 20), c);
        gfx_specular_rim(s, r.x, r.y, r.w, r.h, 16, 60);
        text_wrap(s, font_get(FONT_BOLD), 15, rect_make(r.x + 14, r.y + r.h - 46, r.w - 28, 40), 19, RGB(255, 255, 255), sc->name, true);
        /* botão de executar e de editar */
        rect_t pb = rect_make(r.x + 12, r.y + 12, 30, 30);
        gfx_circle(s, pb.x + 15, pb.y + 15, 15, RGBA(255, 255, 255, 70));
        if (run_now) ui_progress_ring(u, pb.x + 15, pb.y + 15, 22, (float)AT.step / MAX(1, sc->na), false);
        else { float t[] = {pb.x + 11.0f, pb.y + 8.0f, pb.x + 23.0f, pb.y + 15.0f, pb.x + 11.0f, pb.y + 22.0f}; gfx_polygon(s, t, 3, RGB(255, 255, 255)); }
        rect_t eb = rect_make(r.x + r.w - 40, r.y + 12, 28, 28);
        gfx_circle(s, eb.x + 14, eb.y + 14, 14, RGBA(255, 255, 255, 50));
        for (int d = 0; d < 3; d++) gfx_circle(s, eb.x + 8 + d * 6, eb.y + 14, 1.8f, RGB(255, 255, 255));
        if (ui_clicked(u, 123000 + i, eb)) { AT.edit = i; AT.scroll = 0; }
        else if (ui_clicked(u, 123100 + i, r)) run(i);
    }
    /* cartão de novo atalho */
    rect_t r = rect_make(a.x + 24 + (k % cols) * (cw + gap), view.y + 6 + (k / cols) * (ch + gap) - AT.scroll, cw, ch);
    gfx_round_rect_stroke(s, r.x, r.y, r.w, r.h, 16, 1.5f, T.text3);
    glyph_plus(s, r.x + r.w / 2 - 12, r.y + r.h / 2 - 22, 24, T.text2);
    ui_label_center(u, rect_make(r.x, r.y + r.h / 2 + 8, r.w, 18), "Novo Atalho", FONT_SEMIBOLD, 13, T.text2);
    if (ui_clicked(u, UI_ID, r)) { shortcut_t *n = sc_new("Novo Atalho", k % 8); if (n) { AT.edit = (int)(n - AT.s); save(); } }
    k++;
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, (k + cols - 1) / cols * (ch + gap) + 20, &AT.scroll);
}

static void paint_editor(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    shortcut_t *sc = &AT.s[AT.edit];
    if (!sc->used) { AT.edit = -1; return; }
    if (ui_button_style(u, UI_ID, rect_make(a.x + 16, a.y + 14, 34, 28), "‹", BTN_GRAY, true, NULL)) { AT.edit = -1; save(); return; }
    ui_textfield(u, UI_ID, rect_make(a.x + 60, a.y + 14, 260, 28), sc->name, sizeof sc->name, "Nome do atalho");
    for (int i = 0; i < 8; i++) {
        rect_t cr = rect_make(a.x + 334 + i * 24, a.y + 19, 18, 18);
        gfx_circle(s, cr.x + 9, cr.y + 9, 9, sc_colors[i]);
        if (sc->color == i) gfx_circle_stroke(s, cr.x + 9, cr.y + 9, 11.5f, 1.5f, T.text);
        if (ui_clicked(u, 123300 + i, cr)) sc->color = i;
    }
    if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 16 - 100, a.y + 14, 100, 28), "Executar", BTN_FILLED, AT.running < 0, NULL)) { save(); run(AT.edit); }
    if (ui_button_style(u, UI_ID, rect_make(a.x + a.w - 16 - 100 - 8 - 90, a.y + 14, 90, 28), "Apagar", BTN_TINTED_RED, true, NULL)) { sc->used = false; AT.edit = -1; save(); return; }
    rect_t view = rect_make(a.x, a.y + 56, a.w, a.h - 56 - 56);
    rect_t pc = surface_push_clip(s, view);
    int y = view.y + 8 - AT.scroll, cw = MIN(560, a.w - 48), cx = a.x + (a.w - cw) / 2;
    int del = -1, up = -1;
    for (int i = 0; i < sc->na; i++) {
        action_t *ac = &sc->a[i];
        bool has_text = ac->kind == A_APP || ac->kind == A_URL || ac->kind == A_NOTIFY || ac->kind == A_NOTE || ac->kind == A_REMINDER || ac->kind == A_COPY;
        bool has_num = ac->kind == A_WAIT || ac->kind == A_VOLUME || ac->kind == A_THEME;
        int h = has_text || has_num ? 84 : 52;
        rect_t r = rect_make(cx, y, cw, h);
        bool active = AT.running == AT.edit && AT.step == i;
        gfx_shadow(s, r.x, r.y, r.w, r.h, 12, 6, 30, 2);
        gfx_round_rect(s, r.x, r.y, r.w, r.h, 12, T.dark ? RGB(48, 48, 52) : RGB(255, 255, 255));
        if (active) gfx_round_rect_stroke(s, r.x, r.y, r.w, r.h, 12, 2, T.accent);
        gfx_round_rect(s, r.x + 12, r.y + 12, 26, 26, 7, act_colors[ac->kind]);
        char num[4];
        snprintf(num, sizeof num, "%d", i + 1);
        text_draw_center(s, font_get(FONT_BOLD), 12, rect_make(r.x + 12, r.y + 12, 26, 26), RGB(255, 255, 255), num);
        text_draw(s, font_get(FONT_SEMIBOLD), 14, r.x + 48, r.y + 30, T.text, act_names[ac->kind]);
        if (ui_button_style(u, 123400 + i, rect_make(r.x + r.w - 40, r.y + 10, 28, 28), "×", BTN_PLAIN_GRAY, true, NULL)) del = i;
        if (i > 0 && ui_button_style(u, 123500 + i, rect_make(r.x + r.w - 72, r.y + 10, 28, 28), "↑", BTN_PLAIN_GRAY, true, NULL)) up = i;
        if (ac->kind == A_APP) {
            const char *names[64];
            int ex[64], n = 0, sel = 0;
            for (int k = 0; k < apps_count() && n < 64; k++) { app_t *ap = apps_get(k); if (!ap || !strncmp(ap->exec, "mac:", 4)) continue; if (!strcmp(ap->exec, ac->arg)) sel = n; names[n] = ap->name; ex[n++] = k; }
            int ns = ui_popup(u, 123600 + i, rect_make(r.x + 48, r.y + 44, MIN(300, r.w - 64), 28), names, n, sel);
            if (n) snprintf(ac->arg, 160, "%s", apps_get(ex[ns])->exec);
        } else if (has_text) {
            const char *ph = ac->kind == A_URL ? "https://" : "Texto";
            ui_textfield(u, 123700 + i, rect_make(r.x + 48, r.y + 44, r.w - 64, 28), ac->arg, 160, ph);
        } else if (ac->kind == A_THEME) {
            static const char *th[3] = {"Clara", "Escura", "Alternar"};
            ac->num = ui_segmented(u, 123800 + i, rect_make(r.x + 48, r.y + 44, 260, 26), th, 3, ac->num);
        } else if (has_num) {
            ui_stepper(u, 123900 + i, rect_make(r.x + 48, r.y + 44, 90, 28), &ac->num, 0, ac->kind == A_VOLUME ? 100 : 3600);
            ui_label(u, r.x + 148, r.y + 50, ac->kind == A_VOLUME ? "%" : "segundos", FONT_REGULAR, 13, T.text2);
        }
        y += h + 12;
    }
    if (!sc->na) ui_label_center(u, rect_make(a.x, y + 20, a.w, 20), "Adicione ações abaixo.", FONT_REGULAR, 14, T.text3);
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, y + AT.scroll - view.y + 20, &AT.scroll);
    if (del >= 0) { memmove(&sc->a[del], &sc->a[del + 1], sizeof(action_t) * (sc->na - del - 1)); sc->na--; save(); }
    if (up > 0) { action_t t = sc->a[up]; sc->a[up] = sc->a[up - 1]; sc->a[up - 1] = t; save(); }
    /* barra de adicionar */
    int by = a.y + a.h - 44;
    gfx_fill(s, a.x, by - 10, a.w, 1, T.separator);
    AT.add_kind = ui_popup(u, UI_ID, rect_make(a.x + 16, by, 280, 28), act_names, A_KINDS, AT.add_kind);
    if (ui_button_style(u, UI_ID, rect_make(a.x + 304, by, 130, 28), "Adicionar Ação", BTN_TINTED, sc->na < MAXA, glyph_plus)) {
        add_action(sc, AT.add_kind, AT.add_kind == A_URL ? "https://" : "", AT.add_kind == A_WAIT ? 5 : AT.add_kind == A_VOLUME ? 50 : 0);
        if (AT.add_kind == A_APP) snprintf(sc->a[sc->na - 1].arg, 160, "calculadora");
        save();
    }
}

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    load();
    exec_step();
    rect_t a = rect_make(0, w->toolbar_h, u->cw, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    if (AT.edit >= 0) paint_editor(w, u, a); else paint_gallery(w, u, a);
    if (AT.running >= 0) u->repaint = true;
}
static void tick(app_t *a) { exec_step(); }
static void on_close(window_t *w) { save(); AT.w = NULL; }
static void launch(app_t *a) {
    if (AT.w) { wm_focus(AT.w); return; }
    load();
    AT.w = wm_create(&app_atalhos, "Atalhos", -1, -1, 900, 620, WIN_RESIZABLE);
    if (!AT.w) return;
    AT.w->paint = paint;
    AT.w->on_close = on_close;
    AT.w->toolbar_h = TOOLBAR_H;
    AT.w->min_w = 640;
    AT.w->min_h = 460;
}
static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(40, 40, 60), RGB(14, 14, 24));
    gfx_round_rect(s, x + 14 * u, y + 14 * u, 26 * u, 26 * u, 7 * u, RGBA(255, 70, 120, 230));
    gfx_round_rect(s, x + 24 * u, y + 24 * u, 26 * u, 26 * u, 7 * u, RGBA(60, 140, 255, 230));
}
app_t app_atalhos = {.name = "Atalhos", .exec = "atalhos", .draw_icon = icon, .launch = launch, .tick = tick, .always_tick = true};
