/* mOS — Relógio: Relógio Mundial, Alarmes (tocam mesmo com o app fechado), Cronômetro com voltas e Timer.
 * Dados em ~/Biblioteca/Relógio/dados.tsv */
#include "kit.h"

void synth_note(float freq, int ms, int wave, float vol);
extern int g_tz_offset_min;
extern app_t app_relogio;

typedef struct { const char *name, *country; int off; } city_t;   /* off = minutos em relação ao UTC (horário padrão) */
static const city_t cities[] = {
    {"São Paulo", "Brasil", -180}, {"Rio de Janeiro", "Brasil", -180}, {"Manaus", "Brasil", -240}, {"Rio Branco", "Brasil", -300},
    {"Fernando de Noronha", "Brasil", -120}, {"Buenos Aires", "Argentina", -180}, {"Santiago", "Chile", -240}, {"Lima", "Peru", -300},
    {"Bogotá", "Colômbia", -300}, {"Cidade do México", "México", -360}, {"Nova York", "EUA", -300}, {"Chicago", "EUA", -360},
    {"Denver", "EUA", -420}, {"Los Angeles", "EUA", -480}, {"Honolulu", "EUA", -600}, {"Toronto", "Canadá", -300},
    {"Londres", "Reino Unido", 0}, {"Lisboa", "Portugal", 0}, {"Paris", "França", 60}, {"Madri", "Espanha", 60},
    {"Berlim", "Alemanha", 60}, {"Roma", "Itália", 60}, {"Cairo", "Egito", 120}, {"Joanesburgo", "África do Sul", 120},
    {"Moscou", "Rússia", 180}, {"Istambul", "Turquia", 180}, {"Dubai", "Emirados Árabes", 240}, {"Nova Délhi", "Índia", 330},
    {"Bangkok", "Tailândia", 420}, {"Singapura", "Singapura", 480}, {"Pequim", "China", 480}, {"Hong Kong", "China", 480},
    {"Tóquio", "Japão", 540}, {"Seul", "Coreia do Sul", 540}, {"Sydney", "Austrália", 600}, {"Auckland", "Nova Zelândia", 720},
};
#define NCITIES ((int)ARRAY_LEN(cities))

typedef struct { bool used, on; int hh, mi; u8 days; char label[40]; int fired_day; } alarm_t;   /* days: bit 0 = domingo */
#define MAXA 24

static struct {
    window_t *w;
    int tab;
    int wc[16], nwc;
    alarm_t al[MAXA];
    bool loaded;
    /* cronômetro */
    bool sw_run;
    u64 sw_start, sw_acc;
    u64 laps[64];
    int nlaps;
    /* timer */
    int t_h, t_m, t_s;
    bool t_run, t_paused, t_active;
    u64 t_end, t_left_ms, t_total;
    /* folha de novo alarme */
    bool sheet;
    int edit;                   /* índice em edição (-1 = novo) */
    alarm_t tmp;
    bool city_pick;
    int lscroll, ascroll;
    /* alarme tocando */
    int ringing;                /* -1 nenhum; MAXA = timer */
    u64 ring_until, ring_next;
} C = {.ringing = -1, .t_m = 5};

/* ------------------------------------------------------------ dados */
static void save(void) {
    sbuf_t b = {0};
    sb_puts(&b, "# mOS Relógio 1\n");
    for (int i = 0; i < C.nwc; i++) sb_printf(&b, "C\t%s\n", cities[C.wc[i]].name);
    for (int i = 0; i < MAXA; i++) {
        alarm_t *a = &C.al[i];
        if (!a->used) continue;
        char l[40];
        kit_field(l, sizeof l, a->label);
        sb_printf(&b, "A\t%d\t%d\t%d\t%d\t%s\n", a->on, a->hh, a->mi, a->days, l);
    }
    char p[200];
    kit_save(kit_lib("Relógio", "dados.tsv", p, sizeof p), &b);
}
static void load(void) {
    if (C.loaded) return;
    C.loaded = true;
    char p[200];
    char *buf = kit_read(kit_lib("Relógio", "dados.tsv", p, sizeof p), NULL);
    if (!buf) {
        static const int def[] = {0, 10, 16, 32};   /* São Paulo, Nova York, Londres, Tóquio */
        for (int i = 0; i < 4; i++) C.wc[C.nwc++] = def[i];
        C.al[0] = (alarm_t){.used = true, .on = false, .hh = 7, .mi = 0, .days = 0x3E, .fired_day = -1};
        strcpy(C.al[0].label, "Despertar");
        save();
        return;
    }
    char *cur = buf, *ln, *f[8];
    int na = 0;
    while ((ln = kit_line(&cur))) {
        int n = kit_split(ln, f, 8);
        if (!strcmp(f[0], "C") && n >= 2 && C.nwc < 16) {
            for (int i = 0; i < NCITIES; i++) if (!strcmp(cities[i].name, f[1])) { C.wc[C.nwc++] = i; break; }
        } else if (!strcmp(f[0], "A") && n >= 6 && na < MAXA) {
            alarm_t *a = &C.al[na++];
            a->used = true; a->on = atoi(f[1]); a->hh = atoi(f[2]); a->mi = atoi(f[3]); a->days = (u8)atoi(f[4]);
            snprintf(a->label, sizeof a->label, "%s", f[5]);
            a->fired_day = -1;
        }
    }
    kfree(buf);
}

static void ring_start(int who) {
    C.ringing = who;
    C.ring_until = timer_ticks() + 60000;
    C.ring_next = 0;
    if (C.w) wm_focus(C.w);
    dock_request_attention(&app_relogio);
}
static void ring_stop(void) { C.ringing = -1; }

static void tick(app_t *a) {
    load();
    u64 now = timer_ticks();
    /* toque: arpejo repetido até parar ou 1 min */
    if (C.ringing >= 0) {
        if (now > C.ring_until) ring_stop();
        else if (now >= C.ring_next) {
            static const float n[4] = {880.0f, 1108.7f, 1318.5f, 1760.0f};
            for (int i = 0; i < 4; i++) synth_note(n[i], 140, 0, 0.35f);
            C.ring_next = now + 1400;
        }
    }
    /* timer */
    if (C.t_run && !C.t_paused && now >= C.t_end) {
        C.t_run = false;
        C.t_active = false;
        notify("Timer", "O timer terminou.");
        ring_start(MAXA);
    }
    /* alarmes: verificados uma vez por segundo */
    static u64 last;
    if (now - last < 1000) return;
    last = now;
    datetime_t t;
    rtc_now(&t);
    int dayno = (int)kit_days(t.year, t.month, t.day);
    for (int i = 0; i < MAXA; i++) {
        alarm_t *x = &C.al[i];
        if (!x->used || !x->on || x->fired_day == dayno) continue;
        if (x->hh != t.hour || x->mi != t.minute) continue;
        int wd = kit_dow(t.year, t.month, t.day);
        if (x->days && !((x->days >> wd) & 1)) continue;
        x->fired_day = dayno;
        if (!x->days) { x->on = false; save(); }
        char m[80];
        snprintf(m, sizeof m, "%s — %02d:%02d", x->label[0] ? x->label : "Alarme", x->hh, x->mi);
        notify("Alarme", m);
        ring_start(i);
    }
    if (C.w && (C.tab == 0 || C.sw_run || C.t_run)) wm_invalidate(C.w);
}

/* ------------------------------------------------------------ desenho comum */
static void clock_face(surface_t *s, float cx, float cy, float r, int h, int m, int sec, bool night, bool ticks60) {
    u32 face = night ? RGB(30, 30, 32) : RGB(250, 250, 252), ink = night ? RGB(245, 245, 245) : RGB(28, 28, 30);
    gfx_circle(s, cx, cy + 1.5f, r, RGBA(0, 0, 0, 40));
    gfx_circle(s, cx, cy, r, face);
    for (int i = 0; i < (ticks60 ? 60 : 12); i++) {
        float a = i * 2 * PI_F / (ticks60 ? 60 : 12);
        bool major = !ticks60 || i % 5 == 0;
        float r0 = r * (major ? 0.82f : 0.88f), r1 = r * 0.93f;
        gfx_line(s, cx + fsin(a) * r0, cy - fcos(a) * r0, cx + fsin(a) * r1, cy - fcos(a) * r1, major ? MAX(1.2f, r * 0.035f) : 1, major ? ink : color_alpha(ink, 120));
    }
    if (r > 50) {
        for (int i = 1; i <= 12; i++) {
            char t[4];
            snprintf(t, sizeof t, "%d", i);
            float a = i * 2 * PI_F / 12;
            int fr = (int)(r * 0.66f);
            rect_t rr = rect_make((int)(cx + fsin(a) * fr - 14), (int)(cy - fcos(a) * fr - 12), 28, 24);
            text_draw_center(s, font_get(FONT_MEDIUM), r * 0.16f, rr, ink, t);
        }
    }
    float ah = ((h % 12) + m / 60.0f) * 2 * PI_F / 12, am = (m + sec / 60.0f) * 2 * PI_F / 60, as = sec * 2 * PI_F / 60;
    gfx_line(s, cx, cy, cx + fsin(ah) * r * 0.5f, cy - fcos(ah) * r * 0.5f, MAX(2.0f, r * 0.06f), ink);
    gfx_line(s, cx, cy, cx + fsin(am) * r * 0.76f, cy - fcos(am) * r * 0.76f, MAX(1.6f, r * 0.045f), ink);
    if (sec >= 0) gfx_line(s, cx - fsin(as) * r * 0.14f, cy + fcos(as) * r * 0.14f, cx + fsin(as) * r * 0.84f, cy - fcos(as) * r * 0.84f, MAX(1.0f, r * 0.018f), RGB(255, 149, 0));
    gfx_circle(s, cx, cy, MAX(2.0f, r * 0.05f), sec >= 0 ? RGB(255, 149, 0) : ink);
}

/* botão circular do cronômetro/timer */
static bool round_btn(ui_t *u, int id, float cx, float cy, float r, const char *label, u32 bg, u32 fg, bool enabled) {
    rect_t rr = rect_make((int)(cx - r), (int)(cy - r), (int)(2 * r), (int)(2 * r));
    bool hov = enabled && ui_hit(u, rr), held = hov && u->down;
    gfx_circle(u->s, cx, cy, r, enabled ? (held ? color_lighten(bg, 30) : bg) : RGBA(128, 128, 128, 40));
    gfx_circle_stroke(u->s, cx, cy, r - 3, 1.5f, T.content_bg);
    text_draw_center(u->s, font_get(FONT_MEDIUM), 14, rr, enabled ? fg : T.text3, label);
    return enabled && ui_clicked(u, id, rr);
}

static void fmt_ms(char *out, int n, u64 ms, bool hundredths) {
    u64 cs = ms / 10, s = ms / 1000;
    int h = (int)(s / 3600), m = (int)(s / 60 % 60), sec = (int)(s % 60);
    if (hundredths) {
        if (h) snprintf(out, n, "%d:%02d:%02d,%02d", h, m, sec, (int)(cs % 100));
        else snprintf(out, n, "%02d:%02d,%02d", m, sec, (int)(cs % 100));
    } else {
        if (h) snprintf(out, n, "%d:%02d:%02d", h, m, sec);
        else snprintf(out, n, "%02d:%02d", m, sec);
    }
}

/* ------------------------------------------------------------ Relógio Mundial */
static void paint_world(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    datetime_t lt;
    rtc_now(&lt);
    i64 now_utc = kit_now_secs() - g_tz_offset_min * 60;
    int local_day = (int)kit_days(lt.year, lt.month, lt.day);
    int cw = 200, ch = 250, gap = 20;
    int cols = MAX(1, (a.w - gap) / (cw + gap));
    int x0 = a.x + (a.w - cols * (cw + gap) + gap) / 2;
    rect_t view = a;
    rect_t pc = surface_push_clip(s, view);
    int del = -1;
    for (int i = 0; i < C.nwc; i++) {
        const city_t *c = &cities[C.wc[i]];
        int col = i % cols, row = i / cols;
        rect_t r = rect_make(x0 + col * (cw + gap), a.y + 20 + row * (ch + gap) - C.lscroll, cw, ch);
        i64 t = now_utc + c->off * 60;
        i64 day = t >= 0 ? t / 86400 : (t - 86399) / 86400;
        int sod = (int)(t - day * 86400);
        int h = sod / 3600, m = sod / 60 % 60, sec = sod % 60;
        bool night = h < 6 || h >= 18;
        clock_face(s, r.x + cw / 2.0f, r.y + 80, 72, h, m, sec, night, false);
        ui_label_center(u, rect_make(r.x, r.y + 166, cw, 22), c->name, FONT_SEMIBOLD, 15, T.text);
        int diff = (c->off - g_tz_offset_min) / 60, dm = (c->off - g_tz_offset_min) % 60; if (dm < 0) dm = -dm;
        const char *dd = day == local_day ? "Hoje" : day > local_day ? "Amanhã" : "Ontem";
        char rel[48];
        if (diff == 0 && !dm) snprintf(rel, sizeof rel, "%s, mesmo horário", dd);
        else if (dm) snprintf(rel, sizeof rel, "%s, %+d:%02d h", dd, diff, dm);
        else snprintf(rel, sizeof rel, "%s, %+d h", dd, diff);
        ui_label_center(u, rect_make(r.x, r.y + 188, cw, 18), rel, FONT_REGULAR, 12, T.text2);
        char tt[8];
        snprintf(tt, sizeof tt, "%02d:%02d", h, m);
        ui_label_center(u, rect_make(r.x, r.y + 208, cw, 30), tt, FONT_MEDIUM, 24, T.text);
        if (ui_hit(u, r)) {
            rect_t xb = rect_make(r.x + cw / 2 + 52, r.y + 6, 22, 22);
            gfx_circle(s, xb.x + 11, xb.y + 11, 11, T.dark ? RGB(90, 90, 94) : RGB(120, 120, 126));
            gfx_line(s, xb.x + 7, xb.y + 7, xb.x + 15, xb.y + 15, 1.6f, RGB(255, 255, 255));
            gfx_line(s, xb.x + 15, xb.y + 7, xb.x + 7, xb.y + 15, 1.6f, RGB(255, 255, 255));
            if (ui_clicked(u, 30000 + i, xb)) del = i;
        }
    }
    int rows = (C.nwc + cols - 1) / cols;
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, rows * (ch + gap) + 40, &C.lscroll);
    if (!C.nwc) ui_label_center(u, rect_make(a.x, a.y + a.h / 2 - 20, a.w, 30), "Clique em + para adicionar uma cidade", FONT_SEMIBOLD, 16, T.text3);
    if (del >= 0) { memmove(&C.wc[del], &C.wc[del + 1], sizeof(int) * (C.nwc - del - 1)); C.nwc--; save(); }
}

/* ------------------------------------------------------------ Alarmes */
static void days_text(u8 days, char *out, int n) {
    if (!days) { snprintf(out, n, "Nunca"); return; }
    if (days == 0x7F) { snprintf(out, n, "Todos os dias"); return; }
    if (days == 0x3E) { snprintf(out, n, "Dias da semana"); return; }
    if (days == 0x41) { snprintf(out, n, "Fins de semana"); return; }
    out[0] = 0;
    for (int i = 0; i < 7; i++)
        if ((days >> i) & 1) { usize l = strlen(out); snprintf(out + l, n - l, "%s%s", l ? ", " : "", kit_wday_short[i]); }
}

static void paint_alarms(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    int n = 0;
    rect_t view = a;
    rect_t pc = surface_push_clip(s, view);
    int y = a.y + 12 - C.ascroll;
    int del = -1;
    for (int i = 0; i < MAXA; i++) {
        alarm_t *x = &C.al[i];
        if (!x->used) continue;
        rect_t r = rect_make(a.x + 24, y, a.w - 48, 88);
        u32 tc = x->on ? T.text : T.text3;
        char t[8];
        snprintf(t, sizeof t, "%02d:%02d", x->hh, x->mi);
        text_draw(s, font_get(FONT_REGULAR), 46, r.x + 4, r.y + 54, tc, t);
        char dd[80], line[140];
        days_text(x->days, dd, sizeof dd);
        snprintf(line, sizeof line, "%s%s%s", x->label[0] ? x->label : "Alarme", x->days ? ", " : "", x->days ? dd : "");
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 13, r.x + 6, r.y + 76, r.w - 140, tc, line);
        bool on = x->on;
        if (ui_switch_ex(u, 31000 + i, r.x + r.w - 54, r.y + 34, 22, &on, true)) { x->on = on; x->fired_day = -1; save(); }
        if (ui_hit(u, r)) {
            if (ui_button_style(u, 31100 + i, rect_make(r.x + r.w - 54 - 8 - 70, r.y + 33, 70, 24), "Apagar", BTN_TINTED_RED, true, NULL)) del = i;
            if (ui_button_style(u, 31200 + i, rect_make(r.x + r.w - 54 - 16 - 140, r.y + 33, 62, 24), "Editar", BTN_GRAY, true, NULL)) {
                C.sheet = true; C.edit = i; C.tmp = *x;
            }
        }
        gfx_fill(s, r.x, r.y + r.h - 1, r.w, 1, T.separator);
        y += 88;
        n++;
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, n * 88 + 30, &C.ascroll);
    if (!n) ui_label_center(u, rect_make(a.x, a.y + a.h / 2 - 20, a.w, 30), "Nenhum Alarme", FONT_SEMIBOLD, 17, T.text3);
    if (del >= 0) { C.al[del].used = false; save(); }
}

static void paint_sheet(window_t *w, ui_t *u) {
    rect_t r = ui_sheet(u, 380, 330, w->toolbar_h);
    surface_t *s = u->s;
    alarm_t *x = &C.tmp;
    ui_label_center(u, rect_make(r.x, r.y + 14, r.w, 22), C.edit < 0 ? "Novo Alarme" : "Editar Alarme", FONT_BOLD, 15, T.text);
    /* hora grande com steppers */
    int cx = r.x + r.w / 2;
    char t[8];
    snprintf(t, sizeof t, "%02d:%02d", x->hh, x->mi);
    ui_label_center(u, rect_make(r.x, r.y + 44, r.w, 56), t, FONT_REGULAR, 48, T.text);
    ui_stepper(u, UI_ID, rect_make(cx - 120, r.y + 104, 64, 26), &x->hh, 0, 23);
    ui_stepper(u, UI_ID, rect_make(cx + 56, r.y + 104, 64, 26), &x->mi, 0, 59);
    ui_label(u, cx - 120, r.y + 100, "hora", FONT_REGULAR, 11, T.text2);
    ui_label(u, cx + 56, r.y + 100, "minuto", FONT_REGULAR, 11, T.text2);
    ui_textfield(u, UI_ID, rect_make(r.x + 24, r.y + 146, r.w - 48, 26), x->label, sizeof x->label, "Rótulo (Alarme)");
    ui_label(u, r.x + 24, r.y + 184, "Repetir", FONT_SEMIBOLD, 12, T.text2);
    int bx = r.x + 24, bw = (r.w - 48 - 6 * 6) / 7;
    static const char *dl[7] = {"D", "S", "T", "Q", "Q", "S", "S"};
    for (int i = 0; i < 7; i++) {
        rect_t b = rect_make(bx + i * (bw + 6), r.y + 204, bw, 30);
        bool on = (x->days >> i) & 1;
        gfx_round_rect(s, b.x, b.y, b.w, b.h, 15, on ? T.accent : (T.dark ? RGBA(255, 255, 255, 24) : RGBA(0, 0, 0, 12)));
        text_draw_center(s, font_get(FONT_SEMIBOLD), 13, b, on ? RGB(255, 255, 255) : T.text, dl[i]);
        if (ui_clicked(u, 32000 + i, b)) x->days ^= (u8)(1 << i);
    }
    if (ui_button_style(u, UI_ID, rect_make(r.x + r.w - 24 - 90, r.y + r.h - 46, 90, 28), "Salvar", BTN_FILLED | BTN_DEFAULT, true, NULL)) {
        int k = C.edit;
        if (k < 0) for (int i = 0; i < MAXA; i++) if (!C.al[i].used) { k = i; break; }
        if (k >= 0) { C.al[k] = *x; C.al[k].used = true; C.al[k].on = true; C.al[k].fired_day = -1; save(); }
        C.sheet = false;
    }
    if (ui_button_style(u, UI_ID, rect_make(r.x + r.w - 24 - 90 - 8 - 90, r.y + r.h - 46, 90, 28), "Cancelar", BTN_GRAY, true, NULL) || u->key == KEY_ESC)
        C.sheet = false;
    ui_sheet_end(u);
}

/* ------------------------------------------------------------ Cronômetro */
static u64 sw_elapsed(void) { return C.sw_acc + (C.sw_run ? timer_ticks() - C.sw_start : 0); }

static void paint_stopwatch(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    u64 el = sw_elapsed();
    float r = MIN(a.h * 0.32f, 150.0f);
    float cx = a.x + a.w / 2.0f, cy = a.y + 24 + r;
    /* mostrador de 60 s com ponteiro de segundos e subdial de minutos */
    gfx_circle(s, cx, cy, r, T.dark ? RGB(28, 28, 30) : RGB(248, 248, 250));
    gfx_circle_stroke(s, cx, cy, r, 1, T.separator);
    for (int i = 0; i < 240; i++) {
        float ang = i * 2 * PI_F / 240;
        bool maj = i % 20 == 0, mid = i % 4 == 0;
        float r0 = r * (maj ? 0.86f : mid ? 0.9f : 0.93f);
        gfx_line(s, cx + fsin(ang) * r0, cy - fcos(ang) * r0, cx + fsin(ang) * r * 0.96f, cy - fcos(ang) * r * 0.96f, maj ? 1.6f : 1, maj ? T.text : color_alpha(T.text2, mid ? 200 : 110));
    }
    for (int i = 1; i <= 12; i++) {
        char t[4];
        snprintf(t, sizeof t, "%d", i * 5);
        float ang = i * 2 * PI_F / 12, fr = r * 0.74f;
        text_draw_center(s, font_get(FONT_MEDIUM), r * 0.1f, rect_make((int)(cx + fsin(ang) * fr - 16), (int)(cy - fcos(ang) * fr - 10), 32, 20), T.text, t);
    }
    /* subdial de minutos (30 min) */
    float sr = r * 0.2f, scx = cx, scy = cy - r * 0.36f;
    gfx_circle_stroke(s, scx, scy, sr, 1, T.separator);
    float am = (el / 60000.0f) * 2 * PI_F / 30;
    gfx_line(s, scx, scy, scx + fsin(am) * sr * 0.85f, scy - fcos(am) * sr * 0.85f, 1.6f, RGB(255, 149, 0));
    char dig[24];
    fmt_ms(dig, sizeof dig, el, true);
    ui_label_center(u, rect_make((int)(cx - r), (int)(cy + r * 0.3f), (int)(2 * r), (int)(r * 0.22f)), dig, FONT_MEDIUM, r * 0.15f, T.text);
    float as = (el % 60000) / 60000.0f * 2 * PI_F;
    gfx_line(s, cx - fsin(as) * r * 0.16f, cy + fcos(as) * r * 0.16f, cx + fsin(as) * r * 0.92f, cy - fcos(as) * r * 0.92f, 1.6f, RGB(255, 149, 0));
    gfx_circle(s, cx, cy, 4, RGB(255, 149, 0));
    /* botões */
    float by = cy + r + 50;
    if (C.sw_run) {
        if (round_btn(u, UI_ID, cx - 150, by, 36, "Volta", T.dark ? RGB(70, 70, 74) : RGB(220, 220, 224), T.text, true) && C.nlaps < 64) C.laps[C.nlaps++] = el;
        if (round_btn(u, UI_ID, cx + 150, by, 36, "Parar", RGB(255, 69, 58), RGB(255, 255, 255), true)) { C.sw_acc = el; C.sw_run = false; }
    } else {
        if (round_btn(u, UI_ID, cx - 150, by, 36, "Zerar", T.dark ? RGB(70, 70, 74) : RGB(220, 220, 224), T.text, el > 0)) { C.sw_acc = 0; C.nlaps = 0; }
        if (round_btn(u, UI_ID, cx + 150, by, 36, "Iniciar", RGB(48, 209, 88), RGB(255, 255, 255), true)) { C.sw_start = timer_ticks(); C.sw_run = true; }
    }
    /* voltas (a atual no topo) */
    int ly = (int)(by + 50), lw = MIN(420, a.w - 40), lx = (int)(cx - lw / 2);
    if (C.nlaps || C.sw_run) {
        u64 best = ~0ull, worst = 0;
        for (int i = 0; i < C.nlaps; i++) { u64 d = C.laps[i] - (i ? C.laps[i - 1] : 0); if (d < best) best = d; if (d > worst) worst = d; }
        int k = 0;
        for (int i = C.nlaps; i >= 0 && ly + 26 < a.y + a.h; i--, k++) {
            if (i == C.nlaps && !C.sw_run && C.nlaps) continue;
            u64 d = (i == C.nlaps ? el : C.laps[i]) - (i ? C.laps[i - 1] : 0);
            u32 c = T.text;
            if (i < C.nlaps && C.nlaps > 1) { if (d == best) c = RGB(48, 209, 88); else if (d == worst) c = RGB(255, 69, 58); }
            char l[24], v[24];
            snprintf(l, sizeof l, "Volta %d", i + 1);
            fmt_ms(v, sizeof v, d, true);
            gfx_fill(s, lx, ly, lw, 1, T.separator);
            text_draw(s, font_get(FONT_REGULAR), 14, lx + 4, ly + 20, c, l);
            int tw = text_width(font_get(FONT_REGULAR), 14, v);
            text_draw(s, font_get(FONT_REGULAR), 14, lx + lw - 4 - tw, ly + 20, c, v);
            ly += 28;
        }
    }
    if (C.sw_run) u->repaint = true;
}

/* ------------------------------------------------------------ Timer */
static void paint_timer(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    float cx = a.x + a.w / 2.0f;
    if (!C.t_active) {
        /* seleção da duração */
        char t[16];
        snprintf(t, sizeof t, "%02d:%02d:%02d", C.t_h, C.t_m, C.t_s);
        ui_label_center(u, rect_make(a.x, a.y + 30, a.w, 70), t, FONT_REGULAR, 64, T.text);
        int sx = (int)cx - 170;
        static const char *lb[3] = {"horas", "minutos", "segundos"};
        int *v[3] = {&C.t_h, &C.t_m, &C.t_s};
        for (int i = 0; i < 3; i++) {
            ui_label_center(u, rect_make(sx + i * 120, a.y + 108, 100, 16), lb[i], FONT_REGULAR, 11, T.text2);
            ui_stepper(u, 33000 + i, rect_make(sx + i * 120 + 18, a.y + 126, 64, 26), v[i], 0, i ? 59 : 23);
        }
        /* atalhos */
        static const int pm[6] = {1, 3, 5, 10, 15, 30};
        int bw = 74, bx = (int)cx - (6 * bw + 5 * 8) / 2;
        for (int i = 0; i < 6; i++) {
            char l[12];
            snprintf(l, sizeof l, "%d min", pm[i]);
            if (ui_button_style(u, 33100 + i, rect_make(bx + i * (bw + 8), a.y + 180, bw, 28), l, BTN_GRAY | BTN_CAPSULE, true, NULL)) { C.t_h = 0; C.t_m = pm[i]; C.t_s = 0; }
        }
        u64 total = ((u64)C.t_h * 3600 + C.t_m * 60 + C.t_s) * 1000;
        if (round_btn(u, UI_ID, cx, a.y + 280, 40, "Iniciar", RGB(48, 209, 88), RGB(255, 255, 255), total > 0)) {
            C.t_total = total; C.t_end = timer_ticks() + total; C.t_run = true; C.t_paused = false; C.t_active = true;
        }
        return;
    }
    u64 left = C.t_paused ? C.t_left_ms : (C.t_end > timer_ticks() ? C.t_end - timer_ticks() : 0);
    float r = MIN(a.h * 0.34f, 150.0f), cy = a.y + 30 + r;
    float frac = C.t_total ? (float)left / C.t_total : 0;
    gfx_ring_arc(s, cx, cy, r, 8, 0, 2 * PI_F, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 20));
    if (frac > 0) gfx_ring_arc(s, cx, cy, r, 8, -PI_F / 2, -PI_F / 2 + frac * 2 * PI_F, RGB(255, 149, 0));
    char t[16];
    fmt_ms(t, sizeof t, left + 999, false);
    ui_label_center(u, rect_make((int)(cx - r), (int)(cy - 34), (int)(2 * r), 60), t, FONT_REGULAR, r * 0.36f, T.text);
    datetime_t d;
    rtc_now(&d);
    int endm = d.hour * 60 + d.minute + (int)((left + 59999) / 60000);
    char e[32];
    snprintf(e, sizeof e, "Termina às %02d:%02d", (endm / 60) % 24, endm % 60);
    ui_label_center(u, rect_make((int)(cx - r), (int)(cy + 32), (int)(2 * r), 20), e, FONT_REGULAR, 13, T.text2);
    float by = cy + r + 50;
    if (round_btn(u, UI_ID, cx - 150, by, 36, "Cancelar", T.dark ? RGB(70, 70, 74) : RGB(220, 220, 224), T.text, true)) { C.t_active = C.t_run = false; }
    if (C.t_paused) {
        if (round_btn(u, UI_ID, cx + 150, by, 36, "Retomar", RGB(48, 209, 88), RGB(255, 255, 255), true)) { C.t_end = timer_ticks() + C.t_left_ms; C.t_paused = false; }
    } else if (round_btn(u, UI_ID, cx + 150, by, 36, "Pausar", RGB(255, 149, 0), RGB(255, 255, 255), true)) { C.t_left_ms = left; C.t_paused = true; }
    u->repaint = true;
}

/* ------------------------------------------------------------ janela */
static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_fill(s, 0, w->toolbar_h, u->cw, u->ch - w->toolbar_h, T.content_bg);
    static const char *tabs[4] = {"Relógio Mundial", "Alarmes", "Cronômetro", "Timer"};
    int sw = 440;
    C.tab = ui_segmented(u, UI_ID, rect_make((u->cw - sw) / 2, TOOLBAR_Y(28), sw, 28), tabs, 4, C.tab);
    rect_t a = rect_make(0, w->toolbar_h, u->cw, u->ch - w->toolbar_h);
    if (C.tab == 0 || C.tab == 1) {
        rect_t pb = rect_make(u->cw - 12 - 32, TOOLBAR_Y(28), 32, 28);
        if (ui_button_icon(u, UI_ID, pb, glyph_plus, C.tab == 1 || C.nwc < 16)) {
            if (C.tab == 1) { C.sheet = true; C.edit = -1; memset(&C.tmp, 0, sizeof C.tmp); C.tmp.hh = 8; }
            else C.city_pick = !C.city_pick;
        }
        if (C.tab == 0 && C.city_pick) {
            /* lista de cidades num popover */
            rect_t p = ui_popover_frame(u, pb, 260, 360);
            u->modal_next = rect_make(0, 0, u->cw, u->ch);
            if (u->pressed && !rect_contains(p, u->mx, u->my)) C.city_pick = false;
            u->in_overlay = true;
            static int cs;
            rect_t lv = rect_make(p.x + 6, p.y + 8, p.w - 12, p.h - 16);
            rect_t pc = surface_push_clip(s, lv);
            int y = lv.y - cs;
            for (int i = 0; i < NCITIES; i++) {
                rect_t r = rect_make(lv.x, y, lv.w, 30);
                bool have = false;
                for (int k = 0; k < C.nwc; k++) if (C.wc[k] == i) have = true;
                if (ui_list_row(u, 34000 + i, r, false, false) && !have && C.nwc < 16) { C.wc[C.nwc++] = i; save(); C.city_pick = false; }
                if (ui_hit(u, r)) gfx_round_rect(s, r.x + 4, r.y, r.w - 8, r.h, 6, T.accent);
                u32 tc = ui_hit(u, r) ? RGB(255, 255, 255) : have ? T.text3 : T.text;
                text_draw(s, font_get(FONT_REGULAR), 13, r.x + 14, r.y + 20, tc, cities[i].name);
                int tw = text_width(font_get(FONT_REGULAR), 11, cities[i].country);
                text_draw(s, font_get(FONT_REGULAR), 11, r.x + r.w - 14 - tw, r.y + 20, ui_hit(u, r) ? RGB(255, 255, 255) : T.text2, cities[i].country);
                y += 30;
            }
            surface_pop_clip(s, pc);
            ui_scroll(u, UI_ID, lv, NCITIES * 30, &cs);
            u->in_overlay = false;
        }
    }
    if (C.tab == 0) paint_world(w, u, a);
    else if (C.tab == 1) paint_alarms(w, u, a);
    else if (C.tab == 2) paint_stopwatch(w, u, a);
    else paint_timer(w, u, a);
    if (C.sheet) paint_sheet(w, u);
    /* alerta de alarme/timer tocando */
    if (C.ringing >= 0) {
        rect_t r = ui_sheet(u, 320, 170, w->toolbar_h);
        const char *t = C.ringing == MAXA ? "Timer" : (C.al[C.ringing].label[0] ? C.al[C.ringing].label : "Alarme");
        ui_label_center(u, rect_make(r.x, r.y + 22, r.w, 24), t, FONT_BOLD, 17, T.text);
        datetime_t d;
        rtc_now(&d);
        char hm[8];
        snprintf(hm, sizeof hm, "%02d:%02d", d.hour, d.minute);
        ui_label_center(u, rect_make(r.x, r.y + 52, r.w, 44), hm, FONT_REGULAR, 38, T.text);
        if (C.ringing < MAXA && ui_button_style(u, UI_ID, rect_make(r.x + 20, r.y + r.h - 48, 130, 30), "Adiar 9 min", BTN_GRAY, true, NULL)) {
            alarm_t *x = &C.al[C.ringing];
            int m = x->hh * 60 + x->mi + 9;
            for (int i = 0; i < MAXA; i++) if (!C.al[i].used) {
                C.al[i] = (alarm_t){.used = true, .on = true, .hh = (m / 60) % 24, .mi = m % 60, .fired_day = -1};
                snprintf(C.al[i].label, sizeof C.al[i].label, "%s (adiado)", x->label[0] ? x->label : "Alarme");
                break;
            }
            save();
            ring_stop();
        }
        if (ui_button_style(u, UI_ID, rect_make(r.x + r.w - 20 - 130, r.y + r.h - 48, 130, 30), "Parar", BTN_FILLED | BTN_DEFAULT, true, NULL)) ring_stop();
        ui_sheet_end(u);
        u->repaint = true;
    }
    if (u->key == ' ' && !u->focus && C.tab == 2 && !C.sheet) {
        if (C.sw_run) { C.sw_acc = sw_elapsed(); C.sw_run = false; } else { C.sw_start = timer_ticks(); C.sw_run = true; }
        u->key_used = true;
    }
}

static void on_close(window_t *w) { C.w = NULL; C.sheet = false; C.city_pick = false; }
static void launch(app_t *a) {
    if (C.w) { wm_focus(C.w); return; }
    load();
    C.w = wm_create(&app_relogio, "Relógio", -1, -1, 900, 600, WIN_RESIZABLE);
    if (!C.w) return;
    C.w->paint = paint;
    C.w->on_close = on_close;
    C.w->toolbar_h = TOOLBAR_H;
    C.w->min_w = 640;
    C.w->min_h = 520;
}

static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(44, 44, 46), RGB(20, 20, 22));
    datetime_t d;
    rtc_now(&d);
    float cx = x + 32 * u, cy = y + 32 * u, r = 22 * u;
    gfx_circle(s, cx, cy, r, RGB(250, 250, 250));
    for (int i = 0; i < 12; i++) {
        float a = i * 2 * PI_F / 12;
        gfx_line(s, cx + fsin(a) * r * 0.8f, cy - fcos(a) * r * 0.8f, cx + fsin(a) * r * 0.92f, cy - fcos(a) * r * 0.92f, MAX(1.0f, 1.4f * u), RGB(30, 30, 30));
    }
    float ah = ((d.hour % 12) + d.minute / 60.0f) * 2 * PI_F / 12, am = d.minute * 2 * PI_F / 60;
    gfx_line(s, cx, cy, cx + fsin(ah) * r * 0.5f, cy - fcos(ah) * r * 0.5f, 2.4f * u, RGB(30, 30, 30));
    gfx_line(s, cx, cy, cx + fsin(am) * r * 0.75f, cy - fcos(am) * r * 0.75f, 1.8f * u, RGB(30, 30, 30));
    gfx_line(s, cx, cy, cx + fsin(am * 3) * r * 0.82f, cy - fcos(am * 3) * r * 0.82f, 1.0f * u, RGB(255, 149, 0));
    gfx_circle(s, cx, cy, 1.8f * u, RGB(255, 149, 0));
}

app_t app_relogio = {.name = "Relógio", .exec = "relogio", .draw_icon = icon, .launch = launch, .tick = tick, .always_tick = true};
