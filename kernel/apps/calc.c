/* mOS — Calculadora e Calendário */
#include "apps.h"

/* ------------------------------------------------------------------ Calculadora */
static struct {
    double acc, cur;
    char op;
    bool fresh, has_acc;
    char disp[32];
    window_t *w;
} C;

static void fmt_num(double v, char *out) {
    if (v != v) { strcpy(out, "Erro"); return; }
    double av = v < 0 ? -v : v;
    if (av >= 1e15) { strcpy(out, "Erro"); return; }
    if (v == (double)(i64)v) { snprintf(out, 32, "%lld", (i64)v); return; }
    snprintf(out, 32, "%.8f", v);
    char *p = out + strlen(out) - 1;
    while (*p == '0') *p-- = 0;
    if (*p == '.') *p = 0;
    for (char *q = out; *q; q++) if (*q == '.') *q = ',';
}

static double apply(double a, double b, char op) {
    switch (op) {
    case '+': return a + b;
    case '-': return a - b;
    case '*': return a * b;
    case '/': return b == 0 ? (0.0 / 0.0) : a / b;
    }
    return b;
}

static void press(const char *k) {
    char c = k[0];
    if (isdigit(c) || c == ',') {
        if (C.fresh) { strcpy(C.disp, c == ',' ? "0" : ""); C.fresh = false; }
        if (c == ',' && strchr(C.disp, ',')) return;
        if (!strcmp(C.disp, "0") && c != ',') C.disp[0] = 0;
        if (strlen(C.disp) < 15) { int n = (int)strlen(C.disp); C.disp[n] = c; C.disp[n + 1] = 0; }
        char tmp[32];
        strcpy(tmp, C.disp);
        for (char *q = tmp; *q; q++) if (*q == ',') *q = '.';
        const char *p = tmp;
        double v = 0, fr = 0.1;
        bool neg = *p == '-';
        if (neg) p++;
        while (isdigit(*p)) v = v * 10 + (*p++ - '0');
        if (*p == '.') { p++; while (isdigit(*p)) { v += (*p++ - '0') * fr; fr /= 10; } }
        C.cur = neg ? -v : v;
    } else if (c == 'C' || c == 'A') {
        C.acc = C.cur = 0; C.op = 0; C.has_acc = false; strcpy(C.disp, "0"); C.fresh = true;
    } else if (!strcmp(k, "±")) {
        C.cur = -C.cur; fmt_num(C.cur, C.disp);
    } else if (c == '%') {
        C.cur = C.has_acc ? C.acc * C.cur / 100 : C.cur / 100; fmt_num(C.cur, C.disp); C.fresh = true;
    } else if (c == '=') {
        if (C.op) { C.cur = apply(C.acc, C.cur, C.op); C.op = 0; C.has_acc = false; }
        fmt_num(C.cur, C.disp); C.fresh = true;
    } else {
        char op = !strcmp(k, "×") ? '*' : !strcmp(k, "÷") ? '/' : !strcmp(k, "−") ? '-' : '+';
        if (C.op && !C.fresh) C.acc = apply(C.acc, C.cur, C.op);
        else if (!C.has_acc || !C.op) C.acc = C.cur;
        C.has_acc = true;
        C.op = op;
        fmt_num(C.acc, C.disp);
        C.fresh = true;
    }
}

static void paint_calc(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_fill(s, 0, 0, u->cw, u->ch, RGB(56, 56, 58));
    font_t *f = font_get(FONT_REGULAR);
    float fs = strlen(C.disp) > 9 ? 30 : 44;
    int tw = text_width(f, fs, C.disp);
    text_draw(s, f, fs, u->cw - 16 - tw, 66, RGB(255, 255, 255), C.disp);
    static const char *keys[5][4] = {{"C", "±", "%", "÷"}, {"7", "8", "9", "×"}, {"4", "5", "6", "−"}, {"1", "2", "3", "+"}, {"0", "", ",", "="}};
    int top = 84, kw = u->cw / 4, kh = (u->ch - top) / 5;
    for (int j = 0; j < 5; j++)
        for (int i = 0; i < 4; i++) {
            const char *k = keys[j][i];
            if (!k[0]) continue;
            int wdt = (j == 4 && i == 0) ? kw * 2 : kw;
            rect_t r = rect_make(i * kw, top + j * kh, wdt, kh);
            u32 bg = i == 3 ? RGB(255, 149, 0) : j == 0 ? RGB(80, 80, 82) : RGB(110, 110, 112);
            bool hl = (C.op == '*' && !strcmp(k, "×")) || (C.op == '/' && !strcmp(k, "÷")) || (C.op == '-' && !strcmp(k, "−")) || (C.op == '+' && !strcmp(k, "+"));
            bool held = ui_hit(u, r) && u->down;
            if (held) bg = color_lighten(bg, 40);
            gfx_fill(s, r.x, r.y, r.w, r.h, bg);
            gfx_rect(s, r.x, r.y, r.w + 1, r.h + 1, RGB(50, 50, 52));
            if (hl && C.fresh) gfx_rect(s, r.x + 1, r.y + 1, r.w - 1, r.h - 1, RGB(30, 30, 30));
            text_draw_center(s, font_get(FONT_REGULAR), 22, r, RGB(255, 255, 255), k);
            if (ui_clicked(u, 800 + j * 4 + i, r)) press(k);
        }
    if (u->key) {
        char k[4] = {0};
        int c = u->key;
        if (isdigit(c)) k[0] = (char)c;
        else if (c == ',' || c == '.') k[0] = ',';
        else if (c == '+') k[0] = '+';
        else if (c == '-') strcpy(k, "−");
        else if (c == '*') strcpy(k, "×");
        else if (c == '/') strcpy(k, "÷");
        else if (c == KEY_ENTER || c == '=') k[0] = '=';
        else if (c == KEY_ESC || c == 'c') k[0] = 'C';
        else if (c == KEY_BACKSPACE) { int n = (int)strlen(C.disp); if (n > 1) C.disp[n - 1] = 0; else strcpy(C.disp, "0"); }
        if (k[0]) press(k);
        u->key_used = true;
    }
}

static void calc_close(window_t *w) { C.w = NULL; }
static void calc_launch(app_t *a) {
    if (C.w) { wm_focus(C.w); return; }
    C.w = wm_create(&app_calculadora, "", -1, -1, 232, 320, WIN_DARK_CHROME | WIN_NO_ZOOM);
    if (!C.w) return;
    C.w->paint = paint_calc;
    C.w->on_close = calc_close;
    C.w->bg = RGB(56, 56, 58);
    strcpy(C.disp, "0");
    C.fresh = true;
}
app_t app_calculadora = {.name = "Calculadora", .exec = "calculadora", .draw_icon = icon_calc, .launch = calc_launch, .pinned = true};

/* ------------------------------------------------------------------ Calendário */
static window_t *cal_w;
static int cal_month, cal_year;

static int dim(int y, int m) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && ((y % 4 == 0 && y % 100) || y % 400 == 0)) ? 29 : d[m - 1];
}
static int dow(int y, int m, int d) { /* 0 = domingo */
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static void paint_cal(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    static const char *mn[12] = {"Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho", "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro"};
    datetime_t now;
    rtc_now(&now);
    if (!cal_year) { cal_year = now.year; cal_month = now.month; }
    rect_t nx = rect_make(u->cw - 8 - 30, TOOLBAR_Y(28), 30, 28), hj = rect_make(nx.x - 8 - 48, TOOLBAR_Y(28), 48, 28), pr = rect_make(hj.x - 8 - 30, TOOLBAR_Y(28), 30, 28);
    if (ui_toolbar_button(u, UI_ID, pr, "‹", true)) { if (--cal_month < 1) { cal_month = 12; cal_year--; } }
    if (ui_toolbar_button(u, UI_ID, hj, "Hoje", true)) { cal_year = now.year; cal_month = now.month; }
    if (ui_toolbar_button(u, UI_ID, nx, "›", true)) { if (++cal_month > 12) { cal_month = 1; cal_year++; } }
    int top = w->toolbar_h;
    gfx_fill(s, 0, top, u->cw, u->ch - top, T.content_bg);
    char t[48];
    snprintf(t, sizeof t, "%s", mn[cal_month - 1]);
    ui_label(u, 20, top + 12, t, FONT_BOLD, 26, T.text);
    snprintf(t, sizeof t, "%d", cal_year);
    ui_label(u, 24 + text_width(font_get(FONT_BOLD), 26, mn[cal_month - 1]) + 4, top + 12, t, FONT_REGULAR, 26, T.text);
    static const char *wd[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};
    int gy = top + 74, cw = u->cw / 7, ch = (u->ch - gy - 6) / 6;
    for (int i = 0; i < 7; i++) {
        int tw = text_width(font_get(FONT_REGULAR), 12, wd[i]);
        text_draw(s, font_get(FONT_REGULAR), 12, i * cw + cw - tw - 8, gy - 6, T.text2, wd[i]);
    }
    int first = dow(cal_year, cal_month, 1), days = dim(cal_year, cal_month);
    int pm = cal_month == 1 ? 12 : cal_month - 1, py = cal_month == 1 ? cal_year - 1 : cal_year;
    for (int c = 0; c < 42; c++) {
        int col = c % 7, row = c / 7;
        int x = col * cw, y = gy + row * ch;
        int d = c - first + 1;
        bool inm = d >= 1 && d <= days;
        int shown = inm ? d : d < 1 ? dim(py, pm) + d : d - days;
        gfx_fill(s, x, y, cw, 1, T.separator);
        if (col) gfx_fill(s, x, y, 1, ch, T.separator);
        if (col == 0 || col == 6) gfx_fill(s, x + 1, y + 1, cw - 1, ch - 1, T.dark ? RGBA(255, 255, 255, 6) : RGBA(0, 0, 0, 8));
        char ds[4];
        snprintf(ds, sizeof ds, "%d", shown);
        bool today = inm && d == now.day && cal_month == now.month && cal_year == now.year;
        int tw = text_width(font_get(FONT_REGULAR), 14, ds);
        if (today) gfx_circle(s, x + cw - 16 - tw / 2.0f + tw / 2.0f, y + 16, 12, RGB(250, 70, 60));
        text_draw(s, font_get(today ? FONT_SEMIBOLD : FONT_REGULAR), 14, x + cw - 16 - tw / 2 - tw / 2 + 0, y + 21, today ? RGB(255, 255, 255) : inm ? T.text : T.text3, ds);
    }
}

static void cal_close(window_t *w) { cal_w = NULL; }
static void cal_launch(app_t *a) {
    if (cal_w) { wm_focus(cal_w); return; }
    cal_w = wm_create(&app_calendario, "Calendário", -1, -1, 760, 540, WIN_RESIZABLE);
    if (!cal_w) return;
    cal_w->paint = paint_cal;
    cal_w->on_close = cal_close;
    cal_w->toolbar_h = TOOLBAR_H;
    cal_year = 0;
}
app_t app_calendario = {.name = "Calendário", .exec = "calendario", .draw_icon = icon_calendario, .launch = cal_launch, .pinned = true};
