/* mOS — Calendário: visões Dia, Semana, Mês e Ano; eventos com horário ou dia inteiro, local, notas,
 * repetição e alerta; vários calendários com cores; feriados nacionais do Brasil (incluindo os móveis,
 * calculados pela Páscoa); aniversários dos Contatos; busca; importação/exportação iCalendar (.ics).
 * Dados em ~/Biblioteca/Calendário/eventos.ics e calendarios.tsv */
#include "kit.h"

extern int g_tz_offset_min;
int contacts_birthdays_on(int m, int d, char *out, int cap);

#define MAXE 800
#define MAXCAL 12
typedef struct {
    bool used, allday;
    int cal, rep, alert;            /* rep: 0 nunca, 1 diário, 2 semanal, 3 mensal, 4 anual; alert: minutos antes (-1 nenhum) */
    i64 start, end;                 /* segundos (hora local) desde 1/1/1970 */
    char title[96], loc[96], notes[256], uid[48];
    i64 notified;                   /* início da última ocorrência avisada */
} ev_t;
typedef struct { char name[40]; int color; bool on; } cal_t;

static const u32 ccolors[8] = {RGB(10, 132, 255), RGB(191, 90, 242), RGB(48, 209, 88), RGB(255, 159, 10),
                               RGB(255, 69, 58), RGB(255, 204, 0), RGB(172, 142, 104), RGB(142, 142, 147)};
static const char *ccolor_names[8] = {"Azul", "Roxo", "Verde", "Laranja", "Vermelho", "Amarelo", "Marrom", "Cinza"};

enum { V_DAY, V_WEEK, V_MONTH, V_YEAR };
static struct {
    window_t *w;
    ev_t e[MAXE];
    cal_t cal[MAXCAL];
    int ncal;
    bool show_hol, show_bday, loaded;
    int view;
    int y, m, d;                    /* dia em foco */
    int sel;                        /* evento selecionado */
    bool pop;                       /* editor aberto */
    rect_t pop_anchor;
    int tscroll;                    /* rolagem da grade de horas */
    bool tscroll_init;
    char search[64];
    int sscroll;
    bool renaming;
    int rename_idx;
    char rename_buf[40];
} K = {.view = V_MONTH, .sel = -1, .show_hol = true, .show_bday = true};

extern app_t app_calendario;

/* ------------------------------------------------------------ tempo */
static i64 day_of(i64 t) { return t >= 0 ? t / 86400 : (t - 86399) / 86400; }
static i64 mk(int y, int m, int d, int hh, int mi) { return kit_days(y, m, d) * 86400 + hh * 3600 + mi * 60; }
static void split(i64 t, int *y, int *m, int *d, int *hh, int *mi) {
    i64 dd = day_of(t);
    kit_from_days(dd, y, m, d);
    int s = (int)(t - dd * 86400);
    if (hh) *hh = s / 3600;
    if (mi) *mi = s / 60 % 60;
}
static i64 focus_day(void) { return kit_days(K.y, K.m, K.d); }
static void set_focus(i64 day) { kit_from_days(day, &K.y, &K.m, &K.d); }

/* ------------------------------------------------------------ feriados nacionais */
static void easter(int y, int *m, int *d) {
    int a = y % 19, b = y / 100, c = y % 100, dd = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
    int h = (19 * a + b - dd - g + 15) % 30, i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
    int mm = (a + 11 * h + 22 * l) / 451;
    *m = (h + l - 7 * mm + 114) / 31;
    *d = (h + l - 7 * mm + 114) % 31 + 1;
}
static const char *holiday(i64 day) {
    int y, m, d;
    kit_from_days(day, &y, &m, &d);
    static const struct { int m, d; const char *n; } fx[] = {
        {1, 1, "Confraternização Universal"}, {4, 21, "Tiradentes"}, {5, 1, "Dia do Trabalho"}, {9, 7, "Independência do Brasil"},
        {10, 12, "Nossa Senhora Aparecida"}, {11, 2, "Finados"}, {11, 15, "Proclamação da República"},
        {11, 20, "Dia da Consciência Negra"}, {12, 25, "Natal"}, {12, 24, "Véspera de Natal"}, {12, 31, "Véspera de Ano-Novo"}};
    for (int i = 0; i < (int)ARRAY_LEN(fx); i++) if (fx[i].m == m && fx[i].d == d) return fx[i].n;
    int em, ed;
    easter(y, &em, &ed);
    i64 e = kit_days(y, em, ed), off = day - e;
    if (off == 0) return "Páscoa";
    if (off == -2) return "Sexta-feira Santa";
    if (off == -47) return "Carnaval";
    if (off == -48) return "Carnaval (segunda-feira)";
    if (off == -46) return "Quarta-feira de Cinzas";
    if (off == 60) return "Corpus Christi";
    /* Dia das Mães (2º domingo de maio) e dos Pais (2º domingo de agosto) */
    if ((m == 5 || m == 8) && kit_dow(y, m, d) == 0 && d > 7 && d <= 14) return m == 5 ? "Dia das Mães" : "Dia dos Pais";
    return NULL;
}

/* ------------------------------------------------------------ ocorrências */
static i64 ev_end_day(ev_t *e) {
    i64 ed = day_of(e->end - 1);
    return ed < day_of(e->start) ? day_of(e->start) : ed;
}
/* início da ocorrência que cobre o dia (ou -1) */
static bool occurs_on(ev_t *e, i64 day, i64 *occ_start) {
    if (!e->used || !K.cal[e->cal].on) return false;
    i64 sd = day_of(e->start), ed = ev_end_day(e), len = ed - sd;
    if (day < sd) return false;
    i64 base = -1;
    if (!e->rep) { if (day <= ed) base = sd; }
    else {
        /* procura uma ocorrência que comece em day-len..day */
        for (i64 k = day - len; k <= day; k++) {
            if (k < sd) continue;
            bool ok = false;
            int y, m, d, sy, sm, sdd;
            kit_from_days(k, &y, &m, &d);
            kit_from_days(sd, &sy, &sm, &sdd);
            switch (e->rep) {
            case 1: ok = true; break;
            case 2: ok = (k - sd) % 7 == 0; break;
            case 3: ok = d == sdd; break;
            case 4: ok = d == sdd && m == sm; break;
            }
            if (ok) { base = k; break; }
        }
    }
    if (base < 0) return false;
    if (occ_start) *occ_start = base * 86400 + (e->start - sd * 86400);
    return true;
}

/* eventos de um dia, ordenados (dia inteiro primeiro, depois por hora) */
static int day_events(i64 day, int *out, i64 *starts, int max) {
    int n = 0;
    for (int i = 0; i < MAXE && n < max; i++) {
        i64 st;
        if (!occurs_on(&K.e[i], day, &st)) continue;
        if (K.search[0]) continue;
        out[n] = i; starts[n] = st; n++;
    }
    for (int i = 1; i < n; i++) {
        int v = out[i];
        i64 sv = starts[i];
        int j = i - 1;
        while (j >= 0 && (K.e[out[j]].allday < K.e[v].allday || (K.e[out[j]].allday == K.e[v].allday && starts[j] > sv))) { out[j + 1] = out[j]; starts[j + 1] = starts[j]; j--; }
        out[j + 1] = v; starts[j + 1] = sv;
    }
    return n;
}

/* ------------------------------------------------------------ iCalendar */
static void ics_time(sbuf_t *b, const char *key, i64 t, bool allday) {
    int y, m, d, hh, mi;
    split(t, &y, &m, &d, &hh, &mi);
    if (allday) sb_printf(b, "%s;VALUE=DATE:%04d%02d%02d\r\n", key, y, m, d);
    else sb_printf(b, "%s:%04d%02d%02dT%02d%02d00\r\n", key, y, m, d, hh, mi);
}
static void ics_text(sbuf_t *b, const char *key, const char *v) {
    if (!v || !v[0]) return;
    sb_puts(b, key);
    sb_putc(b, ':');
    for (; *v; v++) {
        if (*v == '\n') sb_puts(b, "\\n");
        else if (*v == ',' || *v == ';' || *v == '\\') { sb_putc(b, '\\'); sb_putc(b, *v); }
        else if (*v != '\r') sb_putc(b, *v);
    }
    sb_puts(b, "\r\n");
}
static void ics_event(sbuf_t *b, ev_t *e) {
    static const char *freq[5] = {"", "DAILY", "WEEKLY", "MONTHLY", "YEARLY"};
    sb_puts(b, "BEGIN:VEVENT\r\n");
    sb_printf(b, "UID:%s\r\n", e->uid);
    ics_time(b, "DTSTART", e->start, e->allday);
    ics_time(b, "DTEND", e->end, e->allday);
    ics_text(b, "SUMMARY", e->title);
    ics_text(b, "LOCATION", e->loc);
    ics_text(b, "DESCRIPTION", e->notes);
    if (e->rep) sb_printf(b, "RRULE:FREQ=%s\r\n", freq[e->rep]);
    sb_printf(b, "X-MOS-CALENDAR:%d\r\n", e->cal);
    if (e->alert >= 0) sb_printf(b, "BEGIN:VALARM\r\nACTION:DISPLAY\r\nTRIGGER:-PT%dM\r\nEND:VALARM\r\n", e->alert);
    sb_puts(b, "END:VEVENT\r\n");
}
static void save(void) {
    sbuf_t b = {0};
    sb_puts(&b, "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//mOS//Calendario//PT-BR\r\n");
    for (int i = 0; i < MAXE; i++) if (K.e[i].used) ics_event(&b, &K.e[i]);
    sb_puts(&b, "END:VCALENDAR\r\n");
    char p[200];
    kit_save(kit_lib("Calendário", "eventos.ics", p, sizeof p), &b);
    sbuf_t c = {0};
    sb_printf(&c, "# mOS Calendário 1\nH\t%d\t%d\n", K.show_hol, K.show_bday);
    for (int i = 0; i < K.ncal; i++) { char nm[40]; kit_field(nm, 40, K.cal[i].name); sb_printf(&c, "C\t%s\t%d\t%d\n", nm, K.cal[i].color, K.cal[i].on); }
    kit_save(kit_lib("Calendário", "calendarios.tsv", p, sizeof p), &c);
}
static ev_t *ev_new(void) {
    for (int i = 0; i < MAXE; i++)
        if (!K.e[i].used) {
            ev_t *e = &K.e[i];
            memset(e, 0, sizeof *e);
            e->used = true; e->alert = -1;
            snprintf(e->uid, sizeof e->uid, "%08x-%04x-mos", rand32(), rand32() & 0xFFFF);
            return e;
        }
    return NULL;
}
/* "20261107T093000[Z]" ou "20261107" */
static i64 ics_parse_time(const char *v, bool *allday) {
    int y = 0, m = 0, d = 0, hh = 0, mi = 0, ss = 0;
    if (strlen(v) < 8) return 0;
    for (int i = 0; i < 4; i++) y = y * 10 + v[i] - '0';
    m = (v[4] - '0') * 10 + v[5] - '0';
    d = (v[6] - '0') * 10 + v[7] - '0';
    *allday = v[8] != 'T';
    if (!*allday && strlen(v) >= 15) { hh = (v[9] - '0') * 10 + v[10] - '0'; mi = (v[11] - '0') * 10 + v[12] - '0'; ss = (v[13] - '0') * 10 + v[14] - '0'; }
    i64 t = mk(y, CLAMP(m, 1, 12), CLAMP(d, 1, 31), hh, mi) + ss;
    if (!*allday && strlen(v) >= 16 && v[15] == 'Z') t += g_tz_offset_min * 60;   /* UTC → local */
    return t;
}
static void ics_unescape(const char *v, char *out, int n) {
    int k = 0;
    for (; *v && k < n - 1; v++) {
        if (*v == '\\' && v[1]) { v++; out[k++] = (*v == 'n' || *v == 'N') ? '\n' : *v; }
        else out[k++] = *v;
    }
    out[k] = 0;
}
static int ics_import(char *buf, int default_cal) {
    /* desdobra linhas continuadas (começam com espaço) */
    char *o = buf;
    for (char *p = buf; *p; p++) {
        if (*p == '\r') continue;
        if (*p == '\n' && (p[1] == ' ' || p[1] == '\t')) { p++; continue; }
        *o++ = *p;
    }
    *o = 0;
    int n = 0;
    ev_t *e = NULL;
    bool in_alarm = false, has_end = false;
    char *cur = buf, *ln;
    while ((ln = kit_line(&cur))) {
        if (!strcmp(ln, "BEGIN:VEVENT")) { e = ev_new(); if (e) e->cal = default_cal; has_end = false; continue; }
        if (!e) continue;
        if (!strcmp(ln, "BEGIN:VALARM")) { in_alarm = true; continue; }
        if (!strcmp(ln, "END:VALARM")) { in_alarm = false; continue; }
        if (!strcmp(ln, "END:VEVENT")) {
            if (!has_end) e->end = e->allday ? e->start + 86400 : e->start + 3600;
            if (e->end <= e->start) e->end = e->start + (e->allday ? 86400 : 3600);
            n++;
            e = NULL;
            continue;
        }
        char *colon = strchr(ln, ':');
        if (!colon) continue;
        *colon = 0;
        char *key = ln, *val = colon + 1;
        char *semi = strchr(key, ';');
        char kname[32];
        snprintf(kname, sizeof kname, "%.*s", semi ? (int)(semi - key) : (int)strlen(key), key);
        if (in_alarm) {
            if (!strcmp(kname, "TRIGGER")) {
                /* -PT15M, -PT1H, -P1D */
                int v = 0, mult = 1;
                for (char *p = val; *p; p++) {
                    if (isdigit(*p)) v = v * 10 + (*p - '0');
                    else if (*p == 'H') { mult = 60; break; }
                    else if (*p == 'D') { mult = 1440; break; }
                    else if (*p == 'M' && v) break;
                }
                e->alert = v * mult;
            }
            continue;
        }
        bool ad;
        if (!strcmp(kname, "DTSTART")) { e->start = ics_parse_time(val, &ad); e->allday = ad; }
        else if (!strcmp(kname, "DTEND")) { e->end = ics_parse_time(val, &ad); has_end = true; }
        else if (!strcmp(kname, "SUMMARY")) ics_unescape(val, e->title, sizeof e->title);
        else if (!strcmp(kname, "LOCATION")) ics_unescape(val, e->loc, sizeof e->loc);
        else if (!strcmp(kname, "DESCRIPTION")) ics_unescape(val, e->notes, sizeof e->notes);
        else if (!strcmp(kname, "UID")) snprintf(e->uid, sizeof e->uid, "%s", val);
        else if (!strcmp(kname, "X-MOS-CALENDAR")) e->cal = CLAMP(atoi(val), 0, K.ncal - 1);
        else if (!strcmp(kname, "RRULE")) {
            e->rep = strstr(val, "FREQ=DAILY") ? 1 : strstr(val, "FREQ=WEEKLY") ? 2 : strstr(val, "FREQ=MONTHLY") ? 3 : strstr(val, "FREQ=YEARLY") ? 4 : 0;
        }
    }
    if (e) e->used = false;
    return n;
}

static void load(void) {
    if (K.loaded) return;
    K.loaded = true;
    char p[200];
    char *cb = kit_read(kit_lib("Calendário", "calendarios.tsv", p, sizeof p), NULL);
    if (cb) {
        char *cur = cb, *ln, *f[6];
        while ((ln = kit_line(&cur))) {
            int n = kit_split(ln, f, 6);
            if (!strcmp(f[0], "H") && n >= 3) { K.show_hol = atoi(f[1]); K.show_bday = atoi(f[2]); }
            else if (!strcmp(f[0], "C") && n >= 4 && K.ncal < MAXCAL) {
                snprintf(K.cal[K.ncal].name, 40, "%s", f[1]);
                K.cal[K.ncal].color = CLAMP(atoi(f[2]), 0, 7);
                K.cal[K.ncal].on = atoi(f[3]);
                K.ncal++;
            }
        }
        kfree(cb);
    }
    if (!K.ncal) {
        K.ncal = 3;
        K.cal[0] = (cal_t){"Casa", 0, true};
        K.cal[1] = (cal_t){"Trabalho", 1, true};
        K.cal[2] = (cal_t){"Pessoal", 2, true};
    }
    char *buf = kit_read(kit_lib("Calendário", "eventos.ics", p, sizeof p), NULL);
    if (buf) { ics_import(buf, 0); kfree(buf); }
    else {
        int y, m, d;
        kit_today(&y, &m, &d);
        ev_t *e = ev_new();
        snprintf(e->title, sizeof e->title, "Conhecer o Calendário do mOS");
        e->start = mk(y, m, d, 10, 0); e->end = e->start + 3600; e->cal = 2;
        snprintf(e->notes, sizeof e->notes, "Clique duas vezes num dia ou horário para criar um evento.");
        e = ev_new();
        snprintf(e->title, sizeof e->title, "Reunião semanal");
        e->start = mk(y, m, d, 14, 30) + 86400; e->end = e->start + 3600; e->cal = 1; e->rep = 2; e->alert = 10;
        snprintf(e->loc, sizeof e->loc, "Sala 3");
        save();
    }
}

void calendar_import_file(const char *path) {
    load();
    char *buf = kit_read(path, NULL);
    if (!buf) return;
    int n = ics_import(buf, 0);
    kfree(buf);
    save();
    char m[80];
    snprintf(m, sizeof m, "%d evento%s importado%s.", n, n == 1 ? "" : "s", n == 1 ? "" : "s");
    notify("Calendário", m);
    app_launch(&app_calendario);
}

/* ------------------------------------------------------------ ações */
static void open_editor(int idx, rect_t anchor) { K.sel = idx; K.pop = true; K.pop_anchor = anchor; }
static int new_event(i64 start, bool allday, rect_t anchor) {
    ev_t *e = ev_new();
    if (!e) return -1;
    e->allday = allday;
    e->start = start;
    e->end = allday ? start + 86400 : start + 3600;
    for (int i = 0; i < K.ncal; i++) if (K.cal[i].on) { e->cal = i; break; }
    snprintf(e->title, sizeof e->title, "Novo Evento");
    int idx = (int)(e - K.e);
    open_editor(idx, anchor);
    save();
    return idx;
}
static void delete_event(void) {
    if (K.sel < 0) return;
    K.e[K.sel].used = false;
    K.sel = -1;
    K.pop = false;
    save();
}
static void step(int dir) {
    if (K.view == V_DAY) set_focus(focus_day() + dir);
    else if (K.view == V_WEEK) set_focus(focus_day() + 7 * dir);
    else if (K.view == V_MONTH) { K.m += dir; if (K.m < 1) { K.m = 12; K.y--; } if (K.m > 12) { K.m = 1; K.y++; } K.d = MIN(K.d, kit_dim(K.y, K.m)); }
    else K.y += dir;
    K.pop = false;
}

/* ------------------------------------------------------------ desenho: utilidades */
static u32 ev_color(ev_t *e) { return ccolors[K.cal[e->cal].color]; }
static void fmt_hm(char *o, int n, i64 t) { int hh, mi, y, m, d; split(t, &y, &m, &d, &hh, &mi); snprintf(o, n, "%02d:%02d", hh, mi); }

/* pílula de evento no mês */
static void pill(ui_t *u, int id, rect_t r, int idx, i64 st) {
    surface_t *s = u->s;
    ev_t *e = &K.e[idx];
    u32 c = ev_color(e);
    bool sel = K.sel == idx;
    if (e->allday) {
        gfx_round_rect(s, r.x, r.y, r.w, r.h, 4, sel ? c : color_alpha(c, 70));
        text_draw_ellipsis(s, font_get(FONT_MEDIUM), 11.5f, r.x + 6, r.y + r.h - 5, r.w - 10, sel ? RGB(255, 255, 255) : color_mix(c, T.text, 110), e->title);
    } else {
        if (sel) gfx_round_rect(s, r.x, r.y, r.w, r.h, 4, c);
        gfx_circle(s, r.x + 7, r.y + r.h / 2.0f, 3.5f, sel ? RGB(255, 255, 255) : c);
        char hm[8];
        fmt_hm(hm, sizeof hm, st);
        int tw = text_width(font_get(FONT_REGULAR), 11, hm);
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 11.5f, r.x + 15, r.y + r.h - 5, r.w - 20 - tw - 4, sel ? RGB(255, 255, 255) : T.text, e->title);
        text_draw(s, font_get(FONT_REGULAR), 11, r.x + r.w - 4 - tw, r.y + r.h - 5, sel ? RGB(255, 255, 255) : T.text2, hm);
    }
    if (ui_clicked(u, id, r)) { if (u->dblclick || K.sel == idx) open_editor(idx, r); else { K.sel = idx; K.pop = false; } }
}

/* ------------------------------------------------------------ Mês */
static void paint_month(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    int hh = 26;
    int cw = a.w / 7, ch = (a.h - hh) / 6;
    i64 today = kit_now_secs() / 86400;
    for (int i = 0; i < 7; i++) {
        const char *wd = kit_wday_short[i];
        int tw = text_width(font_get(FONT_REGULAR), 12, wd);
        text_draw(s, font_get(FONT_REGULAR), 12, a.x + i * cw + cw - tw - 10, a.y + 18, T.text2, wd);
    }
    i64 first = kit_days(K.y, K.m, 1);
    i64 start = first - kit_dow(K.y, K.m, 1);
    int ids[24];
    i64 sts[24];
    for (int c = 0; c < 42; c++) {
        int col = c % 7, row = c / 7;
        rect_t r = rect_make(a.x + col * cw, a.y + hh + row * ch, col == 6 ? a.w - 6 * cw : cw, ch);
        i64 day = start + c;
        int y, m, d;
        kit_from_days(day, &y, &m, &d);
        bool inm = m == K.m;
        if (col == 0 || col == 6) gfx_fill(s, r.x, r.y, r.w, r.h, T.dark ? RGBA(255, 255, 255, 5) : RGBA(0, 0, 0, 7));
        gfx_fill(s, r.x, r.y, r.w, 1, T.separator);
        if (col) gfx_fill(s, r.x, r.y, 1, r.h, T.separator);
        char ds[16];
        if (d == 1) snprintf(ds, sizeof ds, "%d de %.3s", d, kit_month[m - 1]); else snprintf(ds, sizeof ds, "%d", d);
        int tw = text_width(font_get(FONT_REGULAR), 13, ds);
        int tx = r.x + r.w - tw - 10;
        if (day == today) {
            gfx_round_rect(s, tx - 7, r.y + 4, tw + 14, 22, 11, RGB(255, 69, 58));
            text_draw(s, font_get(FONT_SEMIBOLD), 13, tx, r.y + 20, RGB(255, 255, 255), ds);
        } else text_draw(s, font_get(FONT_REGULAR), 13, tx, r.y + 20, inm ? T.text : T.text3, ds);
        if (day == focus_day() && day != today) gfx_round_rect_stroke(s, tx - 7, r.y + 4, tw + 14, 22, 11, 1.2f, T.accent);
        /* conteúdo: feriado, aniversários e eventos */
        int py = r.y + 30, maxy = r.y + r.h - 4;
        int lines = (maxy - py) / 19;
        int used = 0;
        const char *hol = K.show_hol ? holiday(day) : NULL;
        char bd[200] = "";
        int nb = K.show_bday ? contacts_birthdays_on(m, d, bd, sizeof bd) : 0;
        int n = day_events(day, ids, sts, 24);
        int total = (hol ? 1 : 0) + nb + n;
        if (hol && used < lines) {
            rect_t pr = rect_make(r.x + 4, py, r.w - 8, 17);
            gfx_round_rect(s, pr.x, pr.y, pr.w, pr.h, 4, RGBA(255, 69, 58, 50));
            text_draw_ellipsis(s, font_get(FONT_MEDIUM), 11.5f, pr.x + 6, pr.y + 12, pr.w - 10, color_mix(RGB(255, 69, 58), T.text, 100), hol);
            py += 19; used++;
        }
        char *bp = bd;
        for (int i = 0; i < nb && used < lines - (total - used > lines - used ? 1 : 0); i++) {
            char *nl = strchr(bp, '\n');
            if (nl) *nl = 0;
            char t[80];
            snprintf(t, sizeof t, "Aniversário de %s", bp);
            rect_t pr = rect_make(r.x + 4, py, r.w - 8, 17);
            gfx_round_rect(s, pr.x, pr.y, pr.w, pr.h, 4, RGBA(142, 142, 147, 50));
            text_draw_ellipsis(s, font_get(FONT_MEDIUM), 11.5f, pr.x + 6, pr.y + 12, pr.w - 10, T.text2, t);
            py += 19; used++;
            if (nl) bp = nl + 1;
        }
        for (int i = 0; i < n; i++) {
            if (used >= lines - 1 && total - used > 1) {
                char more[24];
                snprintf(more, sizeof more, "mais %d…", total - used);
                text_draw(s, font_get(FONT_REGULAR), 11, r.x + 10, py + 12, T.text2, more);
                break;
            }
            pill(u, 50000 + c * 24 + i, rect_make(r.x + 4, py, r.w - 8, 17), ids[i], sts[i]);
            py += 19; used++;
        }
        if (ui_clicked(u, 49000 + c, r)) {
            if (u->dblclick) new_event(mk(y, m, d, 9, 0), false, rect_make(r.x, r.y + 30, r.w, 20));
            else { set_focus(day); K.sel = -1; K.pop = false; }
        }
    }
}

/* ------------------------------------------------------------ Semana / Dia */
static void paint_grid(window_t *w, ui_t *u, rect_t a, int ndays, i64 first) {
    surface_t *s = u->s;
    int gutter = 56, hdr = 52, hh = 48;
    int cw = (a.w - gutter) / ndays;
    i64 today = kit_now_secs() / 86400;
    /* cabeçalho dos dias */
    int ids[24];
    i64 sts[24];
    int allday_rows = 0;
    for (int i = 0; i < ndays; i++) {
        int n = day_events(first + i, ids, sts, 24), k = 0;
        for (int j = 0; j < n; j++) if (K.e[ids[j]].allday) k++;
        if (K.show_hol && holiday(first + i)) k++;
        char bd[160];
        if (K.show_bday) { int yy, mm, dd; kit_from_days(first + i, &yy, &mm, &dd); k += contacts_birthdays_on(mm, dd, bd, sizeof bd); }
        allday_rows = MAX(allday_rows, k);
    }
    int adh = allday_rows ? allday_rows * 20 + 6 : 0;
    for (int i = 0; i < ndays; i++) {
        i64 day = first + i;
        int y, m, d;
        kit_from_days(day, &y, &m, &d);
        int x = a.x + gutter + i * cw;
        char t[24];
        snprintf(t, sizeof t, "%s", kit_wday_short[kit_dow(y, m, d)]);
        char dn[4];
        snprintf(dn, sizeof dn, "%d", d);
        if (ndays == 1) {
            char full[64];
            snprintf(full, sizeof full, "%s, %d de %s", kit_wday[kit_dow(y, m, d)], d, kit_month[m - 1]);
            text_draw(s, font_get(FONT_SEMIBOLD), 14, x + 10, a.y + 30, day == today ? RGB(255, 69, 58) : T.text, full);
        } else {
            int tw = text_width(font_get(FONT_REGULAR), 13, t), dw = text_width(font_get(FONT_SEMIBOLD), 18, dn);
            int tx = x + (cw - tw - dw - 14) / 2;
            text_draw(s, font_get(FONT_REGULAR), 13, tx, a.y + 32, T.text2, t);
            if (day == today) gfx_circle(s, tx + tw + 14 + dw / 2.0f, a.y + 26, 14, RGB(255, 69, 58));
            text_draw(s, font_get(FONT_SEMIBOLD), 18, tx + tw + 14, a.y + 33, day == today ? RGB(255, 255, 255) : T.text, dn);
        }
        if (ui_clicked(u, 60000 + i, rect_make(x, a.y, cw, hdr)) && ndays > 1) { set_focus(day); if (u->dblclick) K.view = V_DAY; }
        /* linha de dia inteiro */
        int ay = a.y + hdr;
        const char *hol = K.show_hol ? holiday(day) : NULL;
        if (hol) {
            gfx_round_rect(s, x + 2, ay, cw - 4, 18, 4, RGBA(255, 69, 58, 50));
            text_draw_ellipsis(s, font_get(FONT_MEDIUM), 11.5f, x + 8, ay + 13, cw - 14, color_mix(RGB(255, 69, 58), T.text, 100), hol);
            ay += 20;
        }
        if (K.show_bday) {
            char bd[200];
            int nb = contacts_birthdays_on(m, d, bd, sizeof bd);
            char *bp = bd;
            for (int j = 0; j < nb; j++) {
                char *nl = strchr(bp, '\n');
                if (nl) *nl = 0;
                char t[80];
                snprintf(t, sizeof t, "Aniversário de %s", bp);
                gfx_round_rect(s, x + 2, ay, cw - 4, 18, 4, RGBA(142, 142, 147, 50));
                text_draw_ellipsis(s, font_get(FONT_MEDIUM), 11.5f, x + 8, ay + 13, cw - 14, T.text2, t);
                ay += 20;
                if (nl) bp = nl + 1;
            }
        }
        int n = day_events(day, ids, sts, 24);
        for (int j = 0; j < n; j++)
            if (K.e[ids[j]].allday) { pill(u, 61000 + i * 24 + j, rect_make(x + 2, ay, cw - 4, 18), ids[j], sts[j]); ay += 20; }
    }
    gfx_fill(s, a.x, a.y + hdr + adh, a.w, 1, T.separator);
    /* grade de horas com rolagem */
    rect_t view = rect_make(a.x, a.y + hdr + adh + 1, a.w, a.h - hdr - adh - 1);
    if (!K.tscroll_init) { K.tscroll = 7 * hh; K.tscroll_init = true; }
    rect_t pc = surface_push_clip(s, view);
    int y0 = view.y - K.tscroll + 8;
    for (int h = 0; h <= 24; h++) {
        int y = y0 + h * hh;
        if (h && h < 24) {
            char hl[8];
            snprintf(hl, sizeof hl, "%02d:00", h);
            int tw = text_width(font_get(FONT_REGULAR), 11, hl);
            text_draw(s, font_get(FONT_REGULAR), 11, a.x + gutter - 8 - tw, y + 4, T.text2, hl);
        }
        gfx_fill(s, a.x + gutter, y, a.w - gutter, 1, T.separator);
    }
    for (int i = 0; i <= ndays; i++) gfx_fill(s, a.x + gutter + i * cw, view.y, 1, view.h + 0, T.separator);
    /* cliques duplos criam eventos de 1 h no horário */
    for (int i = 0; i < ndays; i++) {
        rect_t col = rect_make(a.x + gutter + i * cw, y0, cw, 24 * hh);
        if (ui_clicked(u, 62000 + i, col)) {
            int mins = CLAMP((u->my - y0) * 60 / hh / 15 * 15, 0, 23 * 60 + 45);
            if (u->dblclick) {
                int y, m, d;
                kit_from_days(first + i, &y, &m, &d);
                new_event(mk(y, m, d, mins / 60, mins % 60), false, rect_make(col.x, u->my - 10, col.w, 20));
            } else { K.sel = -1; K.pop = false; set_focus(first + i); }
        }
    }
    /* eventos com horário: colunas lado a lado quando se sobrepõem */
    for (int i = 0; i < ndays; i++) {
        int n = day_events(first + i, ids, sts, 24);
        int lane[24], nl = 1;
        i64 lane_end[24];
        for (int j = 0; j < n; j++) {
            ev_t *e = &K.e[ids[j]];
            if (e->allday) { lane[j] = -1; continue; }
            int L = 0;
            while (L < nl && lane_end[L] > sts[j]) L++;
            if (L == nl) nl++;
            lane[j] = L;
            lane_end[L] = sts[j] + (e->end - e->start);
        }
        for (int j = 0; j < n; j++) {
            ev_t *e = &K.e[ids[j]];
            if (lane[j] < 0) continue;
            i64 dstart = (first + i) * 86400;
            i64 st = MAX(sts[j], dstart), en = MIN(sts[j] + (e->end - e->start), dstart + 86400);
            int top = y0 + (int)((st - dstart) * hh / 3600), bot = y0 + (int)((en - dstart) * hh / 3600);
            int lw = (cw - 6) / nl;
            rect_t r = rect_make(a.x + gutter + i * cw + 3 + lane[j] * lw, top + 1, lw - 2, MAX(18, bot - top - 2));
            u32 c = ev_color(e);
            bool sel = K.sel == ids[j];
            gfx_round_rect(s, r.x, r.y, r.w, r.h, 5, sel ? c : color_alpha(c, T.dark ? 90 : 60));
            gfx_round_rect_ex(s, r.x, r.y, 3, r.h, 2, 0, 0, 2, c, c);
            u32 tc = sel ? RGB(255, 255, 255) : color_mix(c, T.text, T.dark ? 60 : 150);
            text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 12, r.x + 7, r.y + 14, r.w - 10, tc, e->title);
            if (r.h > 34) {
                char hm[32], a1[8], a2[8];
                fmt_hm(a1, 8, sts[j]);
                fmt_hm(a2, 8, sts[j] + (e->end - e->start));
                snprintf(hm, sizeof hm, "%s – %s", a1, a2);
                text_draw_ellipsis(s, font_get(FONT_REGULAR), 11, r.x + 7, r.y + 29, r.w - 10, tc, hm);
            }
            if (r.h > 50 && e->loc[0]) text_draw_ellipsis(s, font_get(FONT_REGULAR), 11, r.x + 7, r.y + 44, r.w - 10, tc, e->loc);
            if (ui_clicked(u, 63000 + i * 24 + j, r)) { if (u->dblclick || K.sel == ids[j]) open_editor(ids[j], r); else { K.sel = ids[j]; K.pop = false; } }
        }
        /* linha da hora atual */
        if (first + i == today) {
            i64 ns = kit_now_secs() % 86400;
            int y = y0 + (int)(ns * hh / 3600);
            int x = a.x + gutter + i * cw;
            gfx_fill(s, x, y - 1, cw, 2, RGB(255, 69, 58));
            gfx_circle(s, x, y, 4, RGB(255, 69, 58));
            char hm[8];
            snprintf(hm, sizeof hm, "%02d:%02d", (int)(ns / 3600), (int)(ns / 60 % 60));
            int tw = text_width(font_get(FONT_SEMIBOLD), 11, hm);
            gfx_round_rect(s, a.x + gutter - 10 - tw, y - 8, tw + 6, 16, 4, RGB(255, 69, 58));
            text_draw(s, font_get(FONT_SEMIBOLD), 11, a.x + gutter - 7 - tw, y + 4, RGB(255, 255, 255), hm);
        }
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, 24 * hh + 16, &K.tscroll);
}

/* ------------------------------------------------------------ Ano */
static void paint_year(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    int cols = a.w > 900 ? 4 : 3, rows = 12 / cols;
    int mw = a.w / cols, mh = a.h / rows;
    i64 today = kit_now_secs() / 86400;
    int ids[4];
    i64 sts[4];
    for (int mo = 1; mo <= 12; mo++) {
        int c = (mo - 1) % cols, r = (mo - 1) / cols;
        rect_t mr = rect_make(a.x + c * mw + 14, a.y + r * mh + 8, mw - 28, mh - 16);
        bool cur = mo == K.m;
        rect_t title = rect_make(mr.x, mr.y, mr.w, 22);
        text_draw(s, font_get(FONT_BOLD), 16, mr.x + 4, mr.y + 17, cur ? RGB(255, 69, 58) : T.text, kit_month_cap[mo - 1]);
        if (ui_clicked(u, 70000 + mo, title)) { K.m = mo; K.d = MIN(K.d, kit_dim(K.y, mo)); K.view = V_MONTH; }
        int cw = mr.w / 7, ch = (mr.h - 44) / 6;
        for (int i = 0; i < 7; i++) {
            char l[2] = {"DSTQQSS"[i], 0};
            text_draw_center(s, font_get(FONT_SEMIBOLD), 10, rect_make(mr.x + i * cw, mr.y + 24, cw, 14), T.text3, l);
        }
        int first = kit_dow(K.y, mo, 1), dim = kit_dim(K.y, mo);
        for (int d = 1; d <= dim; d++) {
            int k = first + d - 1;
            rect_t dr = rect_make(mr.x + (k % 7) * cw, mr.y + 40 + (k / 7) * ch, cw, ch);
            i64 day = kit_days(K.y, mo, d);
            char t[4];
            snprintf(t, sizeof t, "%d", d);
            bool has = day_events(day, ids, sts, 4) > 0 || (K.show_hol && holiday(day));
            if (day == today) gfx_circle(s, dr.x + dr.w / 2.0f, dr.y + dr.h / 2.0f, MIN(dr.w, dr.h) / 2.0f - 1, RGB(255, 69, 58));
            else if (day == focus_day()) gfx_circle_stroke(s, dr.x + dr.w / 2.0f, dr.y + dr.h / 2.0f, MIN(dr.w, dr.h) / 2.0f - 1, 1.2f, T.accent);
            text_draw_center(s, font_get(has ? FONT_BOLD : FONT_REGULAR), 11.5f, dr, day == today ? RGB(255, 255, 255) : T.text, t);
            if (ui_clicked(u, 71000 + mo * 40 + d, dr)) { set_focus(day); if (u->dblclick) K.view = V_DAY; }
        }
    }
}

/* ------------------------------------------------------------ busca */
static void paint_search(window_t *w, ui_t *u, rect_t a) {
    surface_t *s = u->s;
    char q[64];
    snprintf(q, sizeof q, "%s", K.search);
    for (char *p = q; *p; p++) *p = (char)tolower(*p);
    static int order[MAXE];
    int n = 0;
    for (int i = 0; i < MAXE; i++) {
        if (!K.e[i].used) continue;
        char h[500];
        snprintf(h, sizeof h, "%s %s %s", K.e[i].title, K.e[i].loc, K.e[i].notes);
        for (char *p = h; *p; p++) *p = (char)tolower(*p);
        if (strstr(h, q)) order[n++] = i;
    }
    for (int i = 1; i < n; i++) { int v = order[i], j = i - 1; while (j >= 0 && K.e[order[j]].start > K.e[v].start) { order[j + 1] = order[j]; j--; } order[j + 1] = v; }
    rect_t pc = surface_push_clip(s, a);
    int y = a.y + 10 - K.sscroll;
    for (int i = 0; i < n; i++) {
        ev_t *e = &K.e[order[i]];
        rect_t r = rect_make(a.x, y, a.w, 54);
        bool sel = K.sel == order[i];
        if (ui_list_row(u, 80000 + i, r, sel, i % 2)) { K.sel = order[i]; set_focus(day_of(e->start)); if (u->dblclick) { K.search[0] = 0; K.view = V_DAY; } }
        gfx_round_rect(s, r.x + 22, r.y + 10, 4, 34, 2, ev_color(e));
        text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 14, r.x + 36, r.y + 24, r.w - 60, sel ? RGB(255, 255, 255) : T.text, e->title);
        int yy, mm, dd, hh, mi;
        split(e->start, &yy, &mm, &dd, &hh, &mi);
        char d[96];
        if (e->allday) snprintf(d, sizeof d, "%d de %s de %d · dia inteiro", dd, kit_month[mm - 1], yy);
        else snprintf(d, sizeof d, "%d de %s de %d · %02d:%02d%s%s", dd, kit_month[mm - 1], yy, hh, mi, e->loc[0] ? " · " : "", e->loc);
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 12, r.x + 36, r.y + 42, r.w - 60, sel ? RGB(255, 255, 255) : T.text2, d);
        y += 56;
    }
    if (!n) ui_label_center(u, rect_make(a.x, a.y + 60, a.w, 24), "Nenhum Resultado", FONT_SEMIBOLD, 17, T.text3);
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, a, n * 56 + 20, &K.sscroll);
}

/* ------------------------------------------------------------ editor (popover) */
static void paint_editor(window_t *w, ui_t *u) {
    if (!K.pop || K.sel < 0 || !K.e[K.sel].used) { K.pop = false; return; }
    ev_t *e = &K.e[K.sel];
    int pw = 330, ph = e->allday ? 410 : 444;
    rect_t p = ui_popover_frame(u, K.pop_anchor, pw, ph);
    u->modal_next = rect_make(0, 0, u->cw, u->ch);
    if (u->pressed && !rect_contains(p, u->mx, u->my)) { K.pop = false; save(); u->repaint = true; return; }
    u->in_overlay = true;
    int x = p.x + 16, fw = pw - 32, y = p.y + 14;
    ui_textfield(u, UI_ID, rect_make(x, y, fw, 28), e->title, sizeof e->title, "Novo Evento");
    y += 34;
    ui_textfield(u, UI_ID, rect_make(x, y, fw, 26), e->loc, sizeof e->loc, "Adicionar Local");
    y += 38;
    int rows = e->allday ? 3 : 4;
    rect_t g = rect_make(p.x + 10, y, pw - 20, rows * 34);
    ui_group_box(u, g);
    rect_t c = ui_form_row(u, g, 0, 34, rows, "Dia inteiro");
    bool ad = e->allday;
    if (ui_switch_ex(u, UI_ID, c.x + c.w - 40, c.y + 8, 18, &ad, true)) {
        i64 sd = day_of(e->start), ed = ev_end_day(e);
        e->allday = ad;
        if (ad) { e->start = sd * 86400; e->end = (ed + 1) * 86400; }
        else { e->start = sd * 86400 + 9 * 3600; e->end = e->start + 3600; }
    }
    /* início */
    int sy, sm, sd, sh, smi, ey, em, ed, eh, emi;
    split(e->start, &sy, &sm, &sd, &sh, &smi);
    i64 dur = e->end - e->start;
    split(e->allday ? e->end - 86400 : e->end, &ey, &em, &ed, &eh, &emi);
    c = ui_form_row(u, g, 1, 34, rows, "Início");
    int ny = sy, nm = sm, nd = sd, nh = sh, nmi = smi;
    ui_date_field(u, UI_ID, rect_make(c.x + c.w - (e->allday ? 0 : 128) - 118, c.y + 4, 118, 26), &ny, &nm, &nd);
    if (!e->allday) {
        ui_stepper(u, UI_ID, rect_make(c.x + c.w - 124, c.y + 4, 58, 26), &nh, 0, 23);
        ui_stepper(u, UI_ID, rect_make(c.x + c.w - 62, c.y + 4, 58, 26), &nmi, 0, 59);
    }
    if (ny != sy || nm != sm || nd != sd || nh != sh || nmi != smi) { e->start = mk(ny, nm, nd, nh, nmi); e->end = e->start + dur; e->notified = 0; }
    c = ui_form_row(u, g, 2, 34, rows, "Término");
    int xy = ey, xm = em, xd = ed, xh = eh, xmi = emi;
    ui_date_field(u, UI_ID, rect_make(c.x + c.w - (e->allday ? 0 : 128) - 118, c.y + 4, 118, 26), &xy, &xm, &xd);
    if (!e->allday) {
        ui_stepper(u, UI_ID, rect_make(c.x + c.w - 124, c.y + 4, 58, 26), &xh, 0, 23);
        ui_stepper(u, UI_ID, rect_make(c.x + c.w - 62, c.y + 4, 58, 26), &xmi, 0, 59);
    }
    if (xy != ey || xm != em || xd != ed || xh != eh || xmi != emi) {
        i64 ne = e->allday ? (kit_days(xy, xm, xd) + 1) * 86400 : mk(xy, xm, xd, xh, xmi);
        if (ne > e->start) e->end = ne;
    }
    if (!e->allday) {
        c = ui_form_row(u, g, 3, 34, rows, "Alerta");
        static const char *al[] = {"Nenhum", "Na hora", "5 min antes", "10 min antes", "15 min antes", "30 min antes", "1 hora antes", "1 dia antes"};
        static const int alv[] = {-1, 0, 5, 10, 15, 30, 60, 1440};
        int ai = 0;
        for (int i = 0; i < 8; i++) if (alv[i] == e->alert) ai = i;
        int na = ui_popup_inline(u, UI_ID, c, al, 8, ai);
        if (na != ai) { e->alert = alv[na]; e->notified = 0; }
    }
    y += rows * 34 + 10;
    rect_t g2 = rect_make(p.x + 10, y, pw - 20, 2 * 34);
    ui_group_box(u, g2);
    c = ui_form_row(u, g2, 0, 34, 2, "Repetir");
    static const char *rp[] = {"Nunca", "Todo dia", "Toda semana", "Todo mês", "Todo ano"};
    e->rep = ui_popup_inline(u, UI_ID, c, rp, 5, e->rep);
    c = ui_form_row(u, g2, 1, 34, 2, "Calendário");
    const char *cn[MAXCAL];
    for (int i = 0; i < K.ncal; i++) cn[i] = K.cal[i].name;
    e->cal = ui_popup_inline(u, UI_ID, c, cn, K.ncal, e->cal);
    y += 2 * 34 + 10;
    ui_textfield(u, UI_ID, rect_make(x, y, fw, 26), e->notes, sizeof e->notes, "Adicionar Notas");
    y += 36;
    if (ui_button_style(u, UI_ID, rect_make(x, p.y + ph - 40, 100, 28), "Apagar", BTN_TINTED_RED, true, NULL)) { delete_event(); u->in_overlay = false; return; }
    if (ui_button_style(u, UI_ID, rect_make(p.x + pw - 16 - 80, p.y + ph - 40, 80, 28), "OK", BTN_FILLED | BTN_DEFAULT, true, NULL)) { K.pop = false; save(); }
    u->in_overlay = false;
}

/* ------------------------------------------------------------ barra lateral */
static void paint_sidebar(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w, y = w->toolbar_h + 4;
    int yy = K.y, mm = K.m, dd = K.d;
    if (ui_calendar(u, UI_ID, rect_make(10, y, sb - 20, 200), &yy, &mm, &dd)) { K.y = yy; K.m = mm; K.d = dd; K.pop = false; }
    y += 212;
    ui_label(u, 18, y, "Calendários", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    for (int i = 0; i < K.ncal; i++) {
        rect_t r = rect_make(10, y, sb - 20, 26);
        u32 c = ccolors[K.cal[i].color];
        rect_t box = rect_make(r.x + 8, r.y + 5, 16, 16);
        gfx_round_rect(s, box.x, box.y, 16, 16, 4, K.cal[i].on ? c : RGBA(0, 0, 0, 0));
        gfx_round_rect_stroke(s, box.x, box.y, 16, 16, 4, 1.5f, c);
        if (K.cal[i].on) glyph_check(s, box.x + 2, box.y + 2, 12, RGB(255, 255, 255));
        if (K.renaming && K.rename_idx == i) {
            if (ui_textfield(u, 85000, rect_make(r.x + 32, r.y, r.w - 36, 24), K.rename_buf, sizeof K.rename_buf, "Nome")) {
                if (K.rename_buf[0]) snprintf(K.cal[i].name, 40, "%s", K.rename_buf);
                K.renaming = false;
                save();
            } else if (u->focus != 85000) u->focus = 85000;
        } else text_draw_ellipsis(s, font_get(FONT_REGULAR), 13, r.x + 32, r.y + 18, r.w - 40, T.text, K.cal[i].name);
        if (ui_clicked(u, 84000 + i, box)) { K.cal[i].on = !K.cal[i].on; save(); }
        else if (ui_clicked(u, 84100 + i, rect_make(r.x + 30, r.y, r.w - 30, r.h)) && u->dblclick) { K.renaming = true; K.rename_idx = i; snprintf(K.rename_buf, 40, "%s", K.cal[i].name); }
        y += 28;
    }
    y += 6;
    ui_label(u, 18, y, "Outros", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    if (ui_checkbox(u, UI_ID, 18, y + 4, "Feriados do Brasil", &K.show_hol)) save();
    y += 26;
    if (ui_checkbox(u, UI_ID, 18, y + 4, "Aniversários", &K.show_bday)) save();
    y += 30;
    if (ui_button_style(u, UI_ID, rect_make(10, u->ch - 36, 150, 28), "Novo Calendário", BTN_PLAIN_GRAY, K.ncal < MAXCAL, glyph_plus)) {
        K.cal[K.ncal] = (cal_t){"Novo Calendário", K.ncal % 8, true};
        K.renaming = true; K.rename_idx = K.ncal; snprintf(K.rename_buf, 40, "Novo Calendário");
        K.ncal++;
        save();
    }
}

/* ------------------------------------------------------------ janela */
static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    if (!K.y) kit_today(&K.y, &K.m, &K.d);
    paint_sidebar(w, u);
    int sb = w->sidebar_w;
    rect_t area = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, area.x, area.y, area.w, area.h, T.content_bg);
    /* barra de ferramentas */
    if (ui_button_icon(u, UI_ID, rect_make(sb + 12, TOOLBAR_Y(28), 32, 28), glyph_plus, true)) {
        i64 st = focus_day() * 86400 + 9 * 3600;
        new_event(st, false, rect_make(sb + 12, 0, 32, 28));
    }
    static const char *vs[4] = {"Dia", "Semana", "Mês", "Ano"};
    int segw = 280;
    int nv = ui_segmented(u, UI_ID, rect_make(sb + (u->cw - sb - segw) / 2, TOOLBAR_Y(28), segw, 28), vs, 4, K.view);
    if (nv != K.view) { K.view = nv; K.pop = false; }
    ui_searchfield(u, UI_ID, rect_make(u->cw - 12 - 180, TOOLBAR_Y(28), 180, 28), K.search, sizeof K.search);
    /* título e navegação */
    int ty = area.y + 8;
    char t1[48], t2[16];
    if (K.view == V_YEAR) { snprintf(t1, sizeof t1, "%d", K.y); t2[0] = 0; }
    else if (K.view == V_DAY) { snprintf(t1, sizeof t1, "%d de %s", K.d, kit_month[K.m - 1]); snprintf(t2, sizeof t2, "%d", K.y); }
    else { snprintf(t1, sizeof t1, "%s", kit_month_cap[K.m - 1]); snprintf(t2, sizeof t2, "%d", K.y); }
    if (!K.search[0]) {
        int x1 = area.x + 20;
        x1 += text_draw(s, font_get(FONT_BOLD), 26, x1, ty + 28, T.text, t1) + 8;
        text_draw(s, font_get(FONT_REGULAR), 26, x1, ty + 28, T.text, t2);
        rect_t nx = rect_make(u->cw - 16 - 28, ty + 6, 28, 26), hj = rect_make(nx.x - 4 - 56, ty + 6, 56, 26), pv = rect_make(hj.x - 4 - 28, ty + 6, 28, 26);
        if (ui_button_style(u, UI_ID, pv, "‹", BTN_GRAY, true, NULL)) step(-1);
        if (ui_button_style(u, UI_ID, hj, "Hoje", BTN_GRAY, true, NULL)) { kit_today(&K.y, &K.m, &K.d); K.tscroll_init = false; K.pop = false; }
        if (ui_button_style(u, UI_ID, nx, "›", BTN_GRAY, true, NULL)) step(1);
    } else ui_label(u, area.x + 20, ty + 4, "Resultados da Busca", FONT_BOLD, 22, T.text);
    rect_t body = rect_make(area.x, ty + 44, area.w, area.h - 52);
    if (K.search[0]) paint_search(w, u, body);
    else if (K.view == V_MONTH) paint_month(w, u, body);
    else if (K.view == V_WEEK) paint_grid(w, u, body, 7, focus_day() - kit_dow(K.y, K.m, K.d));
    else if (K.view == V_DAY) paint_grid(w, u, body, 1, focus_day());
    else paint_year(w, u, body);
    /* teclado */
    if (u->key && !u->focus && !K.pop) {
        if ((u->key == KEY_BACKSPACE || u->key == KEY_DELETE) && K.sel >= 0) { delete_event(); u->key_used = true; }
        else if (u->key == KEY_LEFT) { step(-1); u->key_used = true; }
        else if (u->key == KEY_RIGHT) { step(1); u->key_used = true; }
        else if (u->key == KEY_ENTER && K.sel >= 0) { open_editor(K.sel, rect_make(u->cw / 2, u->ch / 2, 1, 1)); u->key_used = true; }
        else if ((u->mods & MOD_CMD) && u->key >= '1' && u->key <= '4') { K.view = u->key - '1'; u->key_used = true; }
        else if ((u->mods & MOD_CMD) && u->key == 't') { kit_today(&K.y, &K.m, &K.d); u->key_used = true; }
        else if ((u->mods & MOD_CMD) && u->key == 'n') { new_event(focus_day() * 86400 + 9 * 3600, false, rect_make(sb + 12, 0, 32, 28)); u->key_used = true; }
    }
    paint_editor(w, u);
    u->repaint = true;   /* linha da hora atual */
}

/* alertas: verificados a cada 10 s, mesmo com o app fechado */
static void tick(app_t *a) {
    static u64 last;
    if (timer_ticks() - last < 10000) return;
    last = timer_ticks();
    load();
    i64 now = kit_now_secs(), today = now / 86400;
    for (int i = 0; i < MAXE; i++) {
        ev_t *e = &K.e[i];
        if (!e->used || e->alert < 0 || e->allday) continue;
        for (i64 d = today; d <= today + 1; d++) {
            i64 st;
            if (!occurs_on(e, d, &st) || st < now - 600) continue;
            if (now >= st - e->alert * 60 && e->notified != st) {
                e->notified = st;
                char m[160], hm[8];
                fmt_hm(hm, 8, st);
                snprintf(m, sizeof m, "%s às %s%s%s", e->title, hm, e->loc[0] ? " — " : "", e->loc);
                notify("Calendário", m);
                sound_alert();
            }
        }
    }
}

static void on_close(window_t *w) { save(); K.w = NULL; K.pop = false; }
static void launch(app_t *a) {
    if (K.w) { wm_focus(K.w); return; }
    load();
    kit_today(&K.y, &K.m, &K.d);
    K.w = wm_create(&app_calendario, "Calendário", -1, -1, 1040, 680, WIN_RESIZABLE);
    if (!K.w) return;
    K.w->paint = paint;
    K.w->on_close = on_close;
    K.w->toolbar_h = TOOLBAR_H;
    K.w->sidebar_w = 230;
    K.w->min_w = 820;
    K.w->min_h = 520;
    K.w->title_x = -1;
    K.tscroll_init = false;
}

enum { M_NEW = 1, M_DEL, M_TODAY, M_DAY, M_WEEK, M_MONTH, M_YEAR, M_EXPORT, M_IMPORT };
static menu_item_t m_file[] = {{"Novo Evento", "⌘N", M_NEW}, {NULL}, {"Exportar Calendário…", NULL, M_EXPORT}, {"Importar .ics da Mesa", NULL, M_IMPORT}};
static menu_item_t m_edit[] = {{"Apagar Evento", "⌫", M_DEL}};
static menu_item_t m_view[] = {{"Dia", "⌘1", M_DAY}, {"Semana", "⌘2", M_WEEK}, {"Mês", "⌘3", M_MONTH}, {"Ano", "⌘4", M_YEAR}, {NULL}, {"Ir para Hoje", "⌘T", M_TODAY}};
static menu_t menus[] = {{"Arquivo", m_file, 4}, {"Editar", m_edit, 1}, {"Visualizar", m_view, 6}};
static void on_menu(app_t *a, int act) {
    switch (act) {
    case M_NEW: new_event(focus_day() * 86400 + 9 * 3600, false, rect_make(K.w ? K.w->sidebar_w + 12 : 0, 0, 32, 28)); break;
    case M_DEL: delete_event(); break;
    case M_TODAY: kit_today(&K.y, &K.m, &K.d); break;
    case M_DAY: case M_WEEK: case M_MONTH: case M_YEAR: K.view = act - M_DAY; break;
    case M_EXPORT: {
        char p[200];
        snprintf(p, sizeof p, "%s/Mesa/Calendário.ics", CUR_HOME);
        char src[200];
        char *b = kit_read(kit_lib("Calendário", "eventos.ics", src, sizeof src), NULL);
        if (b) { vfs_write_file(p, b, strlen(b)); kfree(b); notify("Calendário exportado", p); }
    } break;
    case M_IMPORT: {
        char p[200];
        snprintf(p, sizeof p, "%s/Mesa", CUR_HOME);
        vnode_t *d = vfs_lookup(p);
        int n = 0;
        for (vnode_t *c = d && d->dir ? d->child : NULL; c; c = c->next) {
            if (c->dir || strcasecmp(vfs_ext(c->name), "ics") || !strcmp(c->name, "Calendário.ics")) continue;
            char q[300];
            vfs_path(c, q, sizeof q);
            char *b = kit_read(q, NULL);
            if (b) { n += ics_import(b, 0); kfree(b); }
        }
        save();
        char m[64];
        snprintf(m, sizeof m, "%d evento(s) importado(s) da Mesa.", n);
        notify("Calendário", m);
    } break;
    }
    (void)ccolor_names;
    if (K.w) wm_invalidate(K.w);
}

app_t app_calendario = {.name = "Calendário", .exec = "calendario", .draw_icon = icon_calendario, .launch = launch, .tick = tick,
                        .on_menu = on_menu, .menus = menus, .nmenus = 3, .pinned = true, .always_tick = true};
