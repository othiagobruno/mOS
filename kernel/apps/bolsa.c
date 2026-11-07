/* mOS — Bolsa: lista de ações com preço, variação e minigráfico; gráfico com períodos (1S, 1M, 3M, 6M,
 * 1A, 5A), abertura, máxima, mínima, volume e faixa de 52 semanas. Cotações diárias públicas do Stooq
 * (CSV) com o Yahoo Finance como alternativa; sem conta. Lista em ~/Biblioteca/Bolsa/lista.tsv */
#include "kit.h"
#include "net.h"

extern app_t app_bolsa;
#define MAXS 20
#define MAXPTS 1300

typedef struct {
    char sym[24], name[64];
    float close[MAXPTS];
    i32 day[MAXPTS];
    float open, high, low, vol;
    int n;
    bool have, tried_yahoo;
    http_req_t *req;
    u64 fetched;
    char err[64];
} stock_t;

static struct {
    window_t *w;
    stock_t s[MAXS];
    int n, sel, range;
    bool loaded;
    char add[24];
    int hover;
} B = {.range = 2};

static const char *ranges[6] = {"1S", "1M", "3M", "6M", "1A", "5A"};
static const int range_days[6] = {7, 31, 92, 183, 366, 1830};

static void save(void) {
    sbuf_t b = {0};
    sb_puts(&b, "# mOS Bolsa 1\n");
    for (int i = 0; i < B.n; i++) sb_printf(&b, "S\t%s\n", B.s[i].sym);
    char p[200];
    kit_save(kit_lib("Bolsa", "lista.tsv", p, sizeof p), &b);
}
static void add_sym(const char *sym) {
    if (B.n >= MAXS) return;
    for (int i = 0; i < B.n; i++) if (!strcasecmp(B.s[i].sym, sym)) return;
    stock_t *s = &B.s[B.n++];
    memset(s, 0, sizeof *s);
    for (int i = 0; sym[i] && i < 23; i++) s->sym[i] = (char)toupper(sym[i]);
}
static void load(void) {
    if (B.loaded) return;
    B.loaded = true;
    char p[200];
    char *buf = kit_read(kit_lib("Bolsa", "lista.tsv", p, sizeof p), NULL);
    if (buf) {
        char *cur = buf, *ln, *f[3];
        while ((ln = kit_line(&cur))) { int n = kit_split(ln, f, 3); if (!strcmp(f[0], "S") && n >= 2) add_sym(f[1]); }
        kfree(buf);
    }
    if (!B.n) {
        static const char *def[] = {"AAPL.US", "MSFT.US", "NVDA.US", "GOOGL.US", "AMZN.US", "TSLA.US", "BTCUSD"};
        for (int i = 0; i < (int)ARRAY_LEN(def); i++) add_sym(def[i]);
        save();
    }
}

static void fetch(stock_t *s, bool yahoo) {
    if (s->req) return;
    char url[256], low[24];
    int i = 0;
    for (; s->sym[i]; i++) low[i] = (char)tolower(s->sym[i]);
    low[i] = 0;
    if (!yahoo) {
        int y, m, d;
        kit_today(&y, &m, &d);
        snprintf(url, sizeof url, "https://stooq.com/q/d/l/?s=%s&i=d&d1=%04d%02d%02d&d2=%04d%02d%02d", low, y - 5, m, MIN(d, 28), y, m, d);
    } else {
        /* AAPL.US → AAPL; BTCUSD → BTC-USD */
        char ys[24];
        snprintf(ys, sizeof ys, "%s", s->sym);
        char *dot = strstr(ys, ".US");
        if (dot) *dot = 0;
        else if (!strcmp(ys, "BTCUSD")) strcpy(ys, "BTC-USD");
        snprintf(url, sizeof url, "https://query1.finance.yahoo.com/v8/finance/chart/%s?range=5y&interval=1d", ys);
    }
    s->req = http_get(url);
    s->err[0] = 0;
}
static float parse_f(const char *p, char **end) {
    bool neg = *p == '-';
    if (neg) p++;
    double v = 0, f = 0.1;
    while (isdigit((u8)*p)) v = v * 10 + (*p++ - '0');
    if (*p == '.') { p++; while (isdigit((u8)*p)) { v += (*p++ - '0') * f; f /= 10; } }
    if (*p == 'e' || *p == 'E') { p++; bool en = *p == '-'; if (*p == '-' || *p == '+') p++; int e = 0; while (isdigit((u8)*p)) e = e * 10 + (*p++ - '0'); v *= kit_pow(10, en ? -e : e); }
    if (end) *end = (char *)p;
    return (float)(neg ? -v : v);
}
static bool parse_stooq(stock_t *s, char *csv) {
    /* Date,Open,High,Low,Close,Volume */
    if (strncmp(csv, "Date", 4)) return false;
    char *cur = csv, *ln;
    kit_line(&cur);
    s->n = 0;
    while ((ln = kit_line(&cur))) {
        if (strlen(ln) < 10) continue;
        int y = atoi(ln), m = atoi(ln + 5), d = atoi(ln + 8);
        char *p = strchr(ln, ',');
        if (!p) continue;
        float v[5] = {0};
        for (int k = 0; k < 5 && p; k++) { v[k] = parse_f(p + 1, &p); if (*p != ',') { if (k < 3) p = NULL; break; } }
        if (!p && !v[3]) continue;
        if (s->n == MAXPTS) { memmove(s->close, s->close + 1, sizeof(float) * (MAXPTS - 1)); memmove(s->day, s->day + 1, sizeof(i32) * (MAXPTS - 1)); s->n--; }
        s->close[s->n] = v[3];
        s->day[s->n] = (i32)kit_days(y, m, d);
        s->open = v[0]; s->high = v[1]; s->low = v[2]; s->vol = v[4];
        s->n++;
    }
    return s->n > 1;
}
static bool parse_yahoo(stock_t *s, char *j) {
    const char *ts = kit_json_get(j, "chart.result[0].timestamp");
    const char *cl = kit_json_get(j, "chart.result[0].indicators.quote[0].close");
    if (!ts || !cl || *ts != '[' || *cl != '[') return false;
    kit_json_str(j, "chart.result[0].meta.longName", s->name, sizeof s->name);
    if (!s->name[0]) kit_json_str(j, "chart.result[0].meta.shortName", s->name, sizeof s->name);
    ts++; cl++;
    s->n = 0;
    while (*ts && *cl && s->n < MAXPTS) {
        ts = kit_json_ws(ts); cl = kit_json_ws(cl);
        if (*ts == ']' || *cl == ']') break;
        char *e;
        long t = strtol(ts, &e, 10);
        ts = e;
        float c;
        if (!strncmp(cl, "null", 4)) { cl += 4; c = s->n ? s->close[s->n - 1] : 0; }
        else { c = parse_f(cl, &e); cl = e; }
        s->close[s->n] = c;
        s->day[s->n] = (i32)(t / 86400);
        s->n++;
        ts = kit_json_ws(ts); cl = kit_json_ws(cl);
        if (*ts == ',') ts++;
        if (*cl == ',') cl++;
    }
    s->open = (float)kit_json_num(j, "chart.result[0].meta.regularMarketOpen", s->n ? s->close[s->n - 1] : 0);
    s->high = (float)kit_json_num(j, "chart.result[0].meta.regularMarketDayHigh", 0);
    s->low = (float)kit_json_num(j, "chart.result[0].meta.regularMarketDayLow", 0);
    s->vol = (float)kit_json_num(j, "chart.result[0].meta.regularMarketVolume", 0);
    return s->n > 1;
}
static void poll(void) {
    for (int i = 0; i < B.n; i++) {
        stock_t *s = &B.s[i];
        if (!s->req) {
            if ((!s->fetched || timer_ticks() - s->fetched > 10 * 60 * 1000) && (!s->err[0] || timer_ticks() - s->fetched > 60000)) fetch(s, false);
            continue;
        }
        int st = http_poll(s->req);
        if (!st) continue;
        bool ok = false, was_yahoo = strstr(s->req->url, "yahoo") != NULL;
        if (st > 0 && s->req->status == 200 && s->req->body) {
            char *b = kmalloc(s->req->body_len + 1);
            memcpy(b, s->req->body, s->req->body_len);
            b[s->req->body_len] = 0;
            ok = was_yahoo ? parse_yahoo(s, b) : parse_stooq(s, b);
            kfree(b);
        }
        http_free(s->req);
        s->req = NULL;
        s->fetched = timer_ticks();
        if (ok) { s->have = true; s->err[0] = 0; }
        else if (!was_yahoo) fetch(s, true);
        else snprintf(s->err, sizeof s->err, st < 0 ? "Sem conexão" : "Símbolo não encontrado");
        if (B.w) wm_invalidate(B.w);
    }
}

static void money(char *o, int n, float v) {
    char t[32];
    kit_fmt_num(t, sizeof t, v, v < 10 ? 4 : 2);
    /* sempre 2 casas para valores maiores */
    char *c = strchr(t, ',');
    if (v >= 10) { if (!c) strcat(t, ",00"); else if (strlen(c) == 2) strcat(t, "0"); }
    snprintf(o, n, "%s", t);
}
static float change_pct(stock_t *s, int days) {
    if (s->n < 2) return 0;
    float last = s->close[s->n - 1];
    int k = s->n - 2;
    if (days > 1) { i32 lim = s->day[s->n - 1] - days; k = s->n - 1; while (k > 0 && s->day[k] > lim) k--; }
    float base = s->close[k];
    return base ? (last - base) / base * 100 : 0;
}

static void sparkline(surface_t *s, rect_t r, stock_t *st, int from, u32 c) {
    if (st->n - from < 2) return;
    float lo = 1e30f, hi = -1e30f;
    for (int i = from; i < st->n; i++) { lo = MIN(lo, st->close[i]); hi = MAX(hi, st->close[i]); }
    if (hi <= lo) hi = lo + 1;
    float px = 0, py = 0;
    int cnt = st->n - from;
    for (int i = 0; i < cnt; i++) {
        float x = r.x + (float)i * r.w / (cnt - 1), y = r.y + r.h - (st->close[from + i] - lo) / (hi - lo) * r.h;
        if (i) gfx_line(s, px, py, x, y, 1.5f, c);
        px = x; py = y;
    }
}

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    load();
    poll();
    int sb = w->sidebar_w;
    /* lista */
    int y = w->toolbar_h + 4;
    if (ui_textfield(u, UI_ID, rect_make(12, y, sb - 24, 28), B.add, sizeof B.add, "Adicionar símbolo (ex.: META.US)") && B.add[0]) {
        add_sym(B.add);
        B.sel = B.n - 1;
        B.add[0] = 0;
        save();
    }
    y += 38;
    for (int i = 0; i < B.n; i++) {
        stock_t *st = &B.s[i];
        rect_t r = rect_make(10, y, sb - 20, 56);
        if (B.sel == i) gfx_round_rect(s, r.x, r.y, r.w, r.h, 9, T.sidebar_sel);
        char sym[24];
        snprintf(sym, sizeof sym, "%s", st->sym);
        char *dot = strstr(sym, ".US");
        if (dot) *dot = 0;
        text_draw(s, font_get(FONT_BOLD), 14, r.x + 10, r.y + 22, T.text, sym);
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 11, r.x + 10, r.y + 40, 90, T.text2, st->name[0] ? st->name : st->err[0] ? st->err : st->have ? "" : "Carregando…");
        if (st->have) {
            float pct = change_pct(st, 1);
            u32 c = pct >= 0 ? RGB(48, 199, 89) : RGB(255, 69, 58);
            int from = MAX(0, st->n - 22);
            sparkline(s, rect_make(r.x + 104, r.y + 12, 60, 32), st, from, c);
            char pr[24], pc[16];
            money(pr, sizeof pr, st->close[st->n - 1]);
            int tw = text_width(font_get(FONT_SEMIBOLD), 13.5f, pr);
            text_draw(s, font_get(FONT_SEMIBOLD), 13.5f, r.x + r.w - 10 - tw, r.y + 22, T.text, pr);
            snprintf(pc, sizeof pc, "%s%d,%02d%%", pct >= 0 ? "+" : "−", (int)kit_fabs(pct), (int)(kit_fabs(pct) * 100) % 100);
            int pw = text_width(font_get(FONT_SEMIBOLD), 11.5f, pc) + 12;
            gfx_round_rect(s, r.x + r.w - 10 - pw, r.y + 29, pw, 19, 5, c);
            text_draw(s, font_get(FONT_SEMIBOLD), 11.5f, r.x + r.w - 4 - pw, r.y + 43, RGB(255, 255, 255), pc);
        }
        if (ui_clicked(u, 116000 + i, r)) B.sel = i;
        gfx_fill(s, r.x + 10, r.y + r.h + 1, r.w - 20, 1, T.separator);
        y += 58;
    }
    /* detalhe */
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    if (B.sel >= B.n) B.sel = B.n - 1;
    if (B.sel < 0) return;
    stock_t *st = &B.s[B.sel];
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 90, TOOLBAR_Y(28), 90, 28), "Remover", true)) {
        if (st->req) http_free(st->req);
        memmove(&B.s[B.sel], &B.s[B.sel + 1], sizeof(stock_t) * (B.n - B.sel - 1));
        B.n--;
        B.sel = MAX(0, B.sel - 1);
        save();
        return;
    }
    char sym[24];
    snprintf(sym, sizeof sym, "%s", st->sym);
    char *dot = strstr(sym, ".US");
    if (dot) *dot = 0;
    int x0 = a.x + 26, yy = a.y + 16;
    int sw = text_draw(s, font_get(FONT_BOLD), 26, x0, yy + 26, T.text, sym);
    if (st->name[0]) text_draw_ellipsis(s, font_get(FONT_REGULAR), 14, x0 + sw + 10, yy + 26, a.w - sw - 60, T.text2, st->name);
    yy += 40;
    if (!st->have) {
        ui_label(u, x0, yy + 10, st->err[0] ? st->err : "Carregando cotações…", FONT_SEMIBOLD, 15, T.text2);
        u->repaint = true;
        return;
    }
    float last = st->close[st->n - 1];
    char pr[32];
    money(pr, sizeof pr, last);
    int pw = text_draw(s, font_get(FONT_SEMIBOLD), 30, x0, yy + 32, T.text, pr);
    float pct = change_pct(st, range_days[B.range]);
    float abs_ch = last * pct / (100 + pct);
    char ch[48], a1[24];
    money(a1, sizeof a1, kit_fabs(abs_ch));
    snprintf(ch, sizeof ch, "%s%s (%s%d,%02d%%) · %s", abs_ch >= 0 ? "+" : "−", a1, pct >= 0 ? "+" : "−", (int)kit_fabs(pct), (int)(kit_fabs(pct) * 100) % 100, ranges[B.range]);
    u32 cc = pct >= 0 ? RGB(48, 199, 89) : RGB(255, 69, 58);
    text_draw(s, font_get(FONT_SEMIBOLD), 15, x0 + pw + 14, yy + 30, cc, ch);
    yy += 50;
    B.range = ui_segmented(u, UI_ID, rect_make(x0, yy, MIN(400, a.w - 52), 26), ranges, 6, B.range);
    yy += 40;
    /* gráfico */
    rect_t g = rect_make(x0, yy, a.w - 52 - 60, MAX(140, a.h - (yy - a.y) - 170));
    i32 lim = st->day[st->n - 1] - range_days[B.range];
    int from = st->n - 1;
    while (from > 0 && st->day[from - 1] >= lim) from--;
    int cnt = st->n - from;
    float lo = 1e30f, hi = -1e30f;
    for (int i = from; i < st->n; i++) { lo = MIN(lo, st->close[i]); hi = MAX(hi, st->close[i]); }
    float pad = (hi - lo) * 0.08f + 0.01f;
    lo -= pad; hi += pad;
    for (int k = 0; k <= 4; k++) {
        int gy = g.y + g.h * k / 4;
        gfx_fill(s, g.x, gy, g.w, 1, T.separator);
        char l[24];
        money(l, sizeof l, hi - (hi - lo) * k / 4);
        text_draw(s, font_get(FONT_REGULAR), 11, g.x + g.w + 8, gy + 4, T.text2, l);
    }
    if (cnt >= 2) {
        for (int i = 0; i < cnt; i++) {
            int x = g.x + i * g.w / (cnt - 1);
            int ny = g.y + (int)((hi - st->close[from + i]) / (hi - lo) * g.h);
            /* preenchimento em degradê sob a curva */
            int px0 = g.x + (i - 1) * g.w / (cnt - 1);
            if (i) gfx_grad_v(s, px0, ny, MAX(1, x - px0), g.y + g.h - ny, color_alpha(cc, 70), color_alpha(cc, 5));
        }
        float px = 0, py = 0;
        for (int i = 0; i < cnt; i++) {
            float x = g.x + (float)i * g.w / (cnt - 1), yv = g.y + (hi - st->close[from + i]) / (hi - lo) * g.h;
            if (i) gfx_line(s, px, py, x, yv, 2, cc);
            px = x; py = yv;
        }
        /* cursor: valor e data */
        if (ui_hit(u, g)) {
            int i = CLAMP((u->mx - g.x) * (cnt - 1) / MAX(1, g.w), 0, cnt - 1);
            float x = g.x + (float)i * g.w / (cnt - 1), yv = g.y + (hi - st->close[from + i]) / (hi - lo) * g.h;
            gfx_fill(s, (int)x, g.y, 1, g.h, T.text3);
            gfx_circle(s, x, yv, 5, cc);
            int dy, dm, dd;
            kit_from_days(st->day[from + i], &dy, &dm, &dd);
            char tip[64], v[24];
            money(v, sizeof v, st->close[from + i]);
            snprintf(tip, sizeof tip, "%02d/%02d/%d  %s", dd, dm, dy, v);
            int tw = text_width(font_get(FONT_SEMIBOLD), 12, tip);
            int tx = CLAMP((int)x - tw / 2 - 8, g.x, g.x + g.w - tw - 16);
            gfx_round_rect(s, tx, g.y - 4, tw + 16, 22, 6, T.dark ? RGB(60, 60, 64) : RGB(30, 30, 32));
            text_draw(s, font_get(FONT_SEMIBOLD), 12, tx + 8, g.y + 11, RGB(255, 255, 255), tip);
            u->repaint = true;
        }
    }
    yy = g.y + g.h + 26;
    /* estatísticas */
    float h52 = -1e30f, l52 = 1e30f;
    i32 y1 = st->day[st->n - 1] - 365;
    for (int i = 0; i < st->n; i++) if (st->day[i] >= y1) { h52 = MAX(h52, st->close[i]); l52 = MIN(l52, st->close[i]); }
    struct { const char *k; float v; bool vol; } stats[6] = {{"Abertura", st->open, 0}, {"Máxima", st->high, 0}, {"Mínima", st->low, 0},
                                                            {"Volume", st->vol, 1}, {"Máx. 52 sem.", h52, 0}, {"Mín. 52 sem.", l52, 0}};
    int cw = (a.w - 52) / 3;
    for (int i = 0; i < 6; i++) {
        int sx = x0 + (i % 3) * cw, sy = yy + (i / 3) * 30;
        text_draw(s, font_get(FONT_REGULAR), 13, sx, sy + 16, T.text2, stats[i].k);
        char v[32];
        if (stats[i].vol) {
            float vv = stats[i].v;
            if (vv >= 1e9f) { kit_fmt_num(v, sizeof v, vv / 1e9f, 2); strcat(v, " bi"); }
            else if (vv >= 1e6f) { kit_fmt_num(v, sizeof v, vv / 1e6f, 2); strcat(v, " mi"); }
            else kit_fmt_num(v, sizeof v, vv, 0);
        } else money(v, sizeof v, stats[i].v);
        int tw = text_width(font_get(FONT_SEMIBOLD), 13, v);
        text_draw(s, font_get(FONT_SEMIBOLD), 13, sx + cw - 24 - tw, sy + 16, T.text, v);
        gfx_fill(s, sx, sy + 26, cw - 24, 1, T.separator);
    }
    ui_label(u, x0, a.y + a.h - 24, "Cotações diárias: Stooq / Yahoo Finance. Podem ter atraso.", FONT_REGULAR, 11, T.text3);
}

static void tick(app_t *a) { poll(); }
static void on_close(window_t *w) { B.w = NULL; }
static void launch(app_t *a) {
    if (B.w) { wm_focus(B.w); return; }
    load();
    B.w = wm_create(&app_bolsa, "Bolsa", -1, -1, 1000, 640, WIN_RESIZABLE);
    if (!B.w) return;
    B.w->paint = paint;
    B.w->on_close = on_close;
    B.w->toolbar_h = TOOLBAR_H;
    B.w->sidebar_w = 300;
    B.w->min_w = 760;
    B.w->min_h = 480;
}
static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(40, 40, 44), RGB(10, 10, 12));
    float p[] = {10, 44, 20, 36, 28, 40, 38, 24, 46, 30, 54, 16};
    for (int i = 0; i < 5; i++) gfx_line(s, x + p[i * 2] * u, y + p[i * 2 + 1] * u, x + p[i * 2 + 2] * u, y + p[i * 2 + 3] * u, 2.6f * u, RGB(48, 199, 89));
}
app_t app_bolsa = {.name = "Bolsa", .exec = "bolsa", .draw_icon = icon, .launch = launch, .tick = tick};
