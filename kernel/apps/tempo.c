/* mOS — Tempo: previsão real via Open-Meteo (sem chave nem conta): condição atual, por hora (24 h),
 * 10 dias, sensação térmica, umidade, vento, pressão, nascer/pôr do sol; busca de cidades (geocodificação),
 * lista de locais salvos em ~/Biblioteca/Tempo/locais.tsv; fundo que muda com o tempo e a hora. */
#include "kit.h"
#include "net.h"

extern app_t app_tempo;
#define MAXLOC 12

typedef struct {
    char name[64], region[64];
    double lat, lon;
    /* dados */
    bool have;
    double temp, feels, hum, wind, wdir, press, precip;
    int code, is_day;
    double hmax[10], hmin[10];
    int dcode[10], ndays;
    double htemp[24];
    int hcode[24], hhour[24], nh;
    char sunrise[8], sunset[8];
    int utc_off;               /* segundos */
    u64 fetched;
    http_req_t *req;
    char err[96];
} loc_t;

static struct {
    window_t *w;
    loc_t l[MAXLOC];
    int n, sel;
    bool loaded;
    char search[64];
    http_req_t *geo;
    char geo_res[5][160];
    double geo_lat[5], geo_lon[5];
    int ngeo;
    int scroll;
} W;

/* ------------------------------------------------------------ dados */
static void save(void) {
    sbuf_t b = {0};
    sb_puts(&b, "# mOS Tempo 1\n");
    for (int i = 0; i < W.n; i++) {
        char n1[64], n2[64];
        kit_field(n1, 64, W.l[i].name);
        kit_field(n2, 64, W.l[i].region);
        sb_printf(&b, "L\t%s\t%s\t%d\t%d\n", n1, n2, (int)(W.l[i].lat * 10000), (int)(W.l[i].lon * 10000));
    }
    char p[200];
    kit_save(kit_lib("Tempo", "locais.tsv", p, sizeof p), &b);
}
static void add_loc(const char *name, const char *region, double lat, double lon) {
    if (W.n >= MAXLOC) return;
    loc_t *l = &W.l[W.n++];
    memset(l, 0, sizeof *l);
    snprintf(l->name, 64, "%s", name);
    snprintf(l->region, 64, "%s", region);
    l->lat = lat; l->lon = lon;
}
static void load(void) {
    if (W.loaded) return;
    W.loaded = true;
    char p[200];
    char *buf = kit_read(kit_lib("Tempo", "locais.tsv", p, sizeof p), NULL);
    if (buf) {
        char *cur = buf, *ln, *f[6];
        while ((ln = kit_line(&cur))) {
            int n = kit_split(ln, f, 6);
            if (!strcmp(f[0], "L") && n >= 5) add_loc(f[1], f[2], atoi(f[3]) / 10000.0, atoi(f[4]) / 10000.0);
        }
        kfree(buf);
    }
    if (!W.n) {
        add_loc("São Paulo", "Brasil", -23.5505, -46.6333);
        add_loc("Rio de Janeiro", "Brasil", -22.9068, -43.1729);
        add_loc("Lisboa", "Portugal", 38.7223, -9.1393);
        save();
    }
}

static void fmt_coord(char *o, int n, double v) {
    /* 4 casas, sem depender de %f com negativos */
    bool neg = v < 0;
    if (neg) v = -v;
    long ip = (long)v, fp = (long)((v - ip) * 10000 + 0.5);
    if (fp >= 10000) { ip++; fp -= 10000; }
    snprintf(o, n, "%s%ld.%04ld", neg ? "-" : "", ip, fp);
}
static void fetch(loc_t *l) {
    if (l->req) return;
    char la[24], lo[24], url[512];
    fmt_coord(la, sizeof la, l->lat);
    fmt_coord(lo, sizeof lo, l->lon);
    snprintf(url, sizeof url,
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
             "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,weather_code,pressure_msl,wind_speed_10m,wind_direction_10m"
             "&hourly=temperature_2m,weather_code&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset"
             "&timezone=auto&forecast_days=10", la, lo);
    l->req = http_get(url);
    l->err[0] = 0;
}
static void parse(loc_t *l, const char *j) {
    l->temp = kit_json_num(j, "current.temperature_2m", 0);
    l->feels = kit_json_num(j, "current.apparent_temperature", l->temp);
    l->hum = kit_json_num(j, "current.relative_humidity_2m", 0);
    l->wind = kit_json_num(j, "current.wind_speed_10m", 0);
    l->wdir = kit_json_num(j, "current.wind_direction_10m", 0);
    l->press = kit_json_num(j, "current.pressure_msl", 0);
    l->precip = kit_json_num(j, "current.precipitation", 0);
    l->code = (int)kit_json_num(j, "current.weather_code", 0);
    l->is_day = (int)kit_json_num(j, "current.is_day", 1);
    l->utc_off = (int)kit_json_num(j, "utc_offset_seconds", 0);
    char ct[24];
    kit_json_str(j, "current.time", ct, sizeof ct);   /* "2026-11-07T15:30" */
    int curh = strlen(ct) >= 13 ? atoi(ct + 11) : 0;
    char curd[12];
    snprintf(curd, sizeof curd, "%.10s", ct);
    l->ndays = 0;
    for (int i = 0; i < 10; i++) {
        char p[64];
        snprintf(p, sizeof p, "daily.temperature_2m_max[%d]", i);
        if (!kit_json_get(j, p)) break;
        l->hmax[i] = kit_json_num(j, p, 0);
        snprintf(p, sizeof p, "daily.temperature_2m_min[%d]", i);
        l->hmin[i] = kit_json_num(j, p, 0);
        snprintf(p, sizeof p, "daily.weather_code[%d]", i);
        l->dcode[i] = (int)kit_json_num(j, p, 0);
        l->ndays++;
    }
    char s[24];
    kit_json_str(j, "daily.sunrise[0]", s, sizeof s);
    snprintf(l->sunrise, 8, "%s", strlen(s) >= 16 ? s + 11 : "--:--");
    kit_json_str(j, "daily.sunset[0]", s, sizeof s);
    snprintf(l->sunset, 8, "%s", strlen(s) >= 16 ? s + 11 : "--:--");
    /* próximas 24 h a partir da hora atual */
    int start = -1;
    for (int i = 0; i < 48; i++) {
        char p[48], t[24];
        snprintf(p, sizeof p, "hourly.time[%d]", i);
        if (!kit_json_str(j, p, t, sizeof t)) break;
        if (!strncmp(t, curd, 10) && atoi(t + 11) == curh) { start = i; break; }
    }
    if (start < 0) start = curh;
    l->nh = 0;
    for (int i = 0; i < 24; i++) {
        char p[48], t[24];
        snprintf(p, sizeof p, "hourly.temperature_2m[%d]", start + i);
        if (!kit_json_get(j, p)) break;
        l->htemp[i] = kit_json_num(j, p, 0);
        snprintf(p, sizeof p, "hourly.weather_code[%d]", start + i);
        l->hcode[i] = (int)kit_json_num(j, p, 0);
        snprintf(p, sizeof p, "hourly.time[%d]", start + i);
        kit_json_str(j, p, t, sizeof t);
        l->hhour[i] = strlen(t) >= 13 ? atoi(t + 11) : 0;
        l->nh++;
    }
    l->have = true;
    l->fetched = timer_ticks();
}
static void poll_all(void) {
    for (int i = 0; i < W.n; i++) {
        loc_t *l = &W.l[i];
        if (!l->req) {
            if (!l->fetched || timer_ticks() - l->fetched > 15 * 60 * 1000) { if (!l->err[0] || timer_ticks() - l->fetched > 60000) fetch(l); }
            continue;
        }
        int st = http_poll(l->req);
        if (!st) continue;
        if (st > 0 && l->req->status == 200 && l->req->body) {
            char *j = kmalloc(l->req->body_len + 1);
            memcpy(j, l->req->body, l->req->body_len);
            j[l->req->body_len] = 0;
            parse(l, j);
            kfree(j);
        } else {
            snprintf(l->err, sizeof l->err, "%s", st < 0 ? l->req->error : "Resposta inesperada do servidor");
            l->fetched = timer_ticks();
        }
        http_free(l->req);
        l->req = NULL;
        if (W.w) wm_invalidate(W.w);
    }
    if (W.geo) {
        int st = http_poll(W.geo);
        if (st) {
            W.ngeo = 0;
            if (st > 0 && W.geo->body) {
                char *j = kmalloc(W.geo->body_len + 1);
                memcpy(j, W.geo->body, W.geo->body_len);
                j[W.geo->body_len] = 0;
                for (int i = 0; i < 5; i++) {
                    char p[48], nm[64], adm[64], co[64];
                    snprintf(p, sizeof p, "results[%d].name", i);
                    if (!kit_json_str(j, p, nm, sizeof nm)) break;
                    snprintf(p, sizeof p, "results[%d].admin1", i);
                    kit_json_str(j, p, adm, sizeof adm);
                    snprintf(p, sizeof p, "results[%d].country", i);
                    kit_json_str(j, p, co, sizeof co);
                    snprintf(W.geo_res[i], 160, "%s\t%s%s%s", nm, adm, adm[0] && co[0] ? ", " : "", co);
                    snprintf(p, sizeof p, "results[%d].latitude", i);
                    W.geo_lat[i] = kit_json_num(j, p, 0);
                    snprintf(p, sizeof p, "results[%d].longitude", i);
                    W.geo_lon[i] = kit_json_num(j, p, 0);
                    W.ngeo++;
                }
                kfree(j);
            }
            http_free(W.geo);
            W.geo = NULL;
        }
    }
}

/* ------------------------------------------------------------ condições */
static const char *cond_name(int c) {
    if (c == 0) return "Céu Limpo";
    if (c == 1) return "Predominantemente Limpo";
    if (c == 2) return "Parcialmente Nublado";
    if (c == 3) return "Nublado";
    if (c == 45 || c == 48) return "Neblina";
    if (c >= 51 && c <= 57) return "Garoa";
    if (c >= 61 && c <= 67) return c >= 65 ? "Chuva Forte" : "Chuva";
    if (c >= 71 && c <= 77) return "Neve";
    if (c >= 80 && c <= 82) return "Pancadas de Chuva";
    if (c >= 85 && c <= 86) return "Pancadas de Neve";
    if (c >= 95) return "Tempestade";
    return "—";
}
/* 0 sol, 1 sol com nuvem, 2 nuvem, 3 chuva, 4 neve, 5 tempestade, 6 neblina */
static int cond_kind(int c) {
    if (c == 0) return 0;
    if (c <= 2) return 1;
    if (c == 3) return 2;
    if (c == 45 || c == 48) return 6;
    if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return 3;
    if ((c >= 71 && c <= 77) || c == 85 || c == 86) return 4;
    if (c >= 95) return 5;
    return 2;
}
static void glyph_cond(surface_t *s, float x, float y, float sz, int code, bool day) {
    int k = cond_kind(code);
    float u = sz / 32.0f;
    u32 sun = RGB(255, 204, 0), moon = RGB(235, 235, 245), cloud = RGB(245, 245, 250), dark_cloud = RGB(190, 195, 205);
    if (k == 0 || k == 1) {
        float cx = x + (k ? 12 : 16) * u, cy = y + (k ? 12 : 16) * u, r = (k ? 6.5f : 8) * u;
        if (day) {
            gfx_circle(s, cx, cy, r, sun);
            for (int i = 0; i < 8; i++) { float a = i * PI_F / 4; gfx_line(s, cx + fcos(a) * r * 1.4f, cy + fsin(a) * r * 1.4f, cx + fcos(a) * r * 1.85f, cy + fsin(a) * r * 1.85f, 1.8f * u, sun); }
        } else {
            gfx_circle(s, cx, cy, r * 1.2f, moon);
            gfx_circle(s, cx + r * 0.6f, cy - r * 0.4f, r * 1.0f, RGBA(0, 0, 0, 0));
        }
    }
    if (k >= 1) {
        u32 cc = k >= 3 ? dark_cloud : cloud;
        float ox = x + (k == 1 ? 4 : 2) * u, oy = y + (k == 1 ? 6 : 2) * u;
        gfx_circle(s, ox + 11 * u, oy + 15 * u, 6 * u, cc);
        gfx_circle(s, ox + 18 * u, oy + 12 * u, 7.5f * u, cc);
        gfx_round_rect(s, ox + 5 * u, oy + 14 * u, 22 * u, 9 * u, 4.5f * u, cc);
    }
    if (k == 3) for (int i = 0; i < 3; i++) gfx_line(s, x + (10 + i * 6) * u, y + 26 * u, x + (8 + i * 6) * u, y + 31 * u, 1.8f * u, RGB(90, 170, 255));
    if (k == 4) for (int i = 0; i < 3; i++) gfx_circle(s, x + (10 + i * 6) * u, y + 29 * u, 1.6f * u, RGB(255, 255, 255));
    if (k == 5) { float b[] = {x + 16 * u, y + 22 * u, x + 12 * u, y + 29 * u, x + 15 * u, y + 29 * u, x + 13 * u, y + 34 * u, x + 20 * u, y + 26 * u, x + 17 * u, y + 26 * u}; gfx_polygon(s, b, 6, sun); }
    if (k == 6) for (int i = 0; i < 3; i++) gfx_fill(s, (int)(x + 6 * u), (int)(y + (25 + i * 3) * u), (int)(20 * u), MAX(1, (int)(1.6f * u)), RGB(220, 220, 230));
}
static void bg_colors(loc_t *l, u32 *top, u32 *bot) {
    int k = l->have ? cond_kind(l->code) : 1;
    bool day = !l->have || l->is_day;
    if (!day) { *top = RGB(20, 28, 60); *bot = RGB(50, 60, 100); if (k >= 2) { *top = RGB(40, 44, 56); *bot = RGB(70, 74, 86); } return; }
    switch (k) {
    case 0: case 1: *top = RGB(40, 120, 220); *bot = RGB(110, 180, 245); break;
    case 2: case 6: *top = RGB(110, 130, 150); *bot = RGB(160, 175, 190); break;
    default: *top = RGB(70, 85, 105); *bot = RGB(110, 125, 145); break;
    }
}
static void deg(char *o, int n, double t) { snprintf(o, n, "%d°", (int)(t < 0 ? t - 0.5 : t + 0.5)); }

/* ------------------------------------------------------------ desenho */
static void card(surface_t *s, rect_t r) {
    gfx_round_rect(s, r.x, r.y, r.w, r.h, 16, RGBA(255, 255, 255, 38));
    gfx_round_rect_stroke(s, r.x, r.y, r.w, r.h, 16, 1, RGBA(255, 255, 255, 50));
}
static void card_title(ui_t *u, rect_t r, const char *t) {
    ui_label(u, r.x + 16, r.y + 12, t, FONT_SEMIBOLD, 11.5f, RGBA(255, 255, 255, 170));
    gfx_fill(u->s, r.x + 16, r.y + 32, r.w - 32, 1, RGBA(255, 255, 255, 50));
}

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    load();
    poll_all();
    int sb = w->sidebar_w;
    /* barra lateral: busca e locais */
    int y = w->toolbar_h + 4;
    bool enter = ui_searchfield(u, UI_ID, rect_make(12, y, sb - 24, 28), W.search, sizeof W.search);
    static char last_q[64];
    if ((enter || (strlen(W.search) >= 3 && strcmp(last_q, W.search))) && !W.geo) {
        snprintf(last_q, sizeof last_q, "%s", W.search);
        sbuf_t q = {0};
        sb_puts(&q, "https://geocoding-api.open-meteo.com/v1/search?count=5&language=pt&name=");
        sb_urlenc(&q, W.search);
        W.geo = http_get(q.p);
        sb_free(&q);
    }
    if (!W.search[0]) { W.ngeo = 0; last_q[0] = 0; }
    y += 38;
    if (W.search[0]) {
        for (int i = 0; i < W.ngeo; i++) {
            rect_t r = rect_make(10, y, sb - 20, 44);
            char nm[64], rg[96];
            char *tab = strchr(W.geo_res[i], '\t');
            snprintf(nm, sizeof nm, "%.*s", tab ? (int)(tab - W.geo_res[i]) : 63, W.geo_res[i]);
            snprintf(rg, sizeof rg, "%s", tab ? tab + 1 : "");
            if (ui_hit(u, r)) gfx_round_rect(s, r.x, r.y, r.w, r.h, 8, T.sidebar_sel);
            text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 13, r.x + 10, r.y + 18, r.w - 20, T.text, nm);
            text_draw_ellipsis(s, font_get(FONT_REGULAR), 11.5f, r.x + 10, r.y + 35, r.w - 20, T.text2, rg);
            if (ui_clicked(u, 113000 + i, r)) { add_loc(nm, rg, W.geo_lat[i], W.geo_lon[i]); W.sel = W.n - 1; W.search[0] = 0; save(); }
            y += 46;
        }
        if (!W.ngeo) ui_label(u, 20, y, W.geo ? "Buscando…" : "Nenhum resultado", FONT_REGULAR, 12.5f, T.text2);
    } else {
        for (int i = 0; i < W.n; i++) {
            loc_t *l = &W.l[i];
            rect_t r = rect_make(10, y, sb - 20, 74);
            u32 t1, t2;
            bg_colors(l, &t1, &t2);
            gfx_round_rect_grad(s, r.x, r.y, r.w, r.h, 12, t1, t2);
            if (W.sel == i) gfx_round_rect_stroke(s, r.x - 1, r.y - 1, r.w + 2, r.h + 2, 13, 2, T.accent);
            text_draw_ellipsis(s, font_get(FONT_BOLD), 15, r.x + 12, r.y + 24, r.w - 70, RGB(255, 255, 255), l->name);
            if (l->have) {
                char t[8];
                deg(t, sizeof t, l->temp);
                int tw = text_width(font_get(FONT_REGULAR), 30, t);
                text_draw(s, font_get(FONT_REGULAR), 30, r.x + r.w - 12 - tw, r.y + 34, RGB(255, 255, 255), t);
                text_draw_ellipsis(s, font_get(FONT_REGULAR), 11.5f, r.x + 12, r.y + 62, r.w / 2, RGBA(255, 255, 255, 220), cond_name(l->code));
                char hl[24], a1[8], a2[8];
                deg(a1, 8, l->hmax[0]); deg(a2, 8, l->hmin[0]);
                snprintf(hl, sizeof hl, "Máx.: %s Mín.: %s", a1, a2);
                int hw = text_width(font_get(FONT_REGULAR), 11, hl);
                text_draw(s, font_get(FONT_REGULAR), 11, r.x + r.w - 12 - hw, r.y + 62, RGBA(255, 255, 255, 220), hl);
            } else text_draw(s, font_get(FONT_REGULAR), 11.5f, r.x + 12, r.y + 62, RGBA(255, 255, 255, 220), l->err[0] ? "Sem conexão" : "Carregando…");
            if (ui_clicked(u, 113100 + i, r)) W.sel = i;
            y += 82;
        }
    }
    /* área principal */
    rect_t a = rect_make(sb, 0, u->cw - sb, u->ch);
    if (W.sel >= W.n) W.sel = W.n - 1;
    if (W.sel < 0) { gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg); return; }
    loc_t *l = &W.l[W.sel];
    u32 t1, t2;
    bg_colors(l, &t1, &t2);
    gfx_grad_v(s, a.x, a.y, a.w, a.h, t1, t2);
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 90, TOOLBAR_Y(28), 90, 28), "Remover", W.n > 1)) {
        if (l->req) http_free(l->req);
        memmove(&W.l[W.sel], &W.l[W.sel + 1], sizeof(loc_t) * (W.n - W.sel - 1));
        W.n--;
        W.sel = MAX(0, W.sel - 1);
        save();
        return;
    }
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 90 - 8 - 100, TOOLBAR_Y(28), 100, 28), "Atualizar", !l->req)) fetch(l);
    rect_t view = rect_make(a.x, w->toolbar_h, a.w, a.h - w->toolbar_h);
    rect_t pc = surface_push_clip(s, view);
    int cy = view.y + 20 - W.scroll, cx = a.x + a.w / 2;
    ui_label_center(u, rect_make(a.x, cy, a.w, 34), l->name, FONT_REGULAR, 30, RGB(255, 255, 255));
    cy += 36;
    if (!l->have) {
        ui_label_center(u, rect_make(a.x, cy + 40, a.w, 24), l->err[0] ? l->err : "Carregando a previsão…", FONT_SEMIBOLD, 15, RGB(255, 255, 255));
        if (l->err[0]) ui_label_center(u, rect_make(a.x, cy + 70, a.w, 20), "Verifique a conexão com a internet.", FONT_REGULAR, 13, RGBA(255, 255, 255, 200));
        surface_pop_clip(s, pc);
        u->repaint = l->req != NULL;
        return;
    }
    char t[16];
    deg(t, sizeof t, l->temp);
    ui_label_center(u, rect_make(a.x + 14, cy, a.w, 90), t, FONT_REGULAR, 84, RGB(255, 255, 255));
    cy += 92;
    ui_label_center(u, rect_make(a.x, cy, a.w, 22), cond_name(l->code), FONT_SEMIBOLD, 17, RGBA(255, 255, 255, 230));
    cy += 24;
    char hl[40], a1[8], a2[8];
    deg(a1, 8, l->hmax[0]); deg(a2, 8, l->hmin[0]);
    snprintf(hl, sizeof hl, "Máx.: %s  Mín.: %s", a1, a2);
    ui_label_center(u, rect_make(a.x, cy, a.w, 22), hl, FONT_SEMIBOLD, 16, RGB(255, 255, 255));
    cy += 40;
    int cw = MIN(720, a.w - 40), x0 = cx - cw / 2;
    /* por hora */
    rect_t hr = rect_make(x0, cy, cw, 130);
    card(s, hr);
    card_title(u, hr, "PREVISÃO POR HORA");
    int cols = MIN(l->nh, MAX(6, (cw - 32) / 56));
    int colw = (cw - 32) / MAX(1, cols);
    for (int i = 0; i < cols; i++) {
        int x = hr.x + 16 + i * colw;
        char hh[8];
        if (i == 0) snprintf(hh, sizeof hh, "Agora"); else snprintf(hh, sizeof hh, "%02d", l->hhour[i]);
        ui_label_center(u, rect_make(x, hr.y + 42, colw, 16), hh, FONT_SEMIBOLD, 12, RGB(255, 255, 255));
        glyph_cond(s, x + colw / 2.0f - 13, hr.y + 62, 26, l->hcode[i], l->hhour[i] >= 6 && l->hhour[i] < 18);
        char tt[8];
        deg(tt, 8, l->htemp[i]);
        ui_label_center(u, rect_make(x, hr.y + 100, colw, 18), tt, FONT_SEMIBOLD, 15, RGB(255, 255, 255));
    }
    cy += 144;
    /* 10 dias */
    rect_t dr = rect_make(x0, cy, cw, 44 + l->ndays * 40);
    card(s, dr);
    card_title(u, dr, "PREVISÃO PARA 10 DIAS");
    double lo = 1e9, hi = -1e9;
    for (int i = 0; i < l->ndays; i++) { lo = MIN(lo, l->hmin[i]); hi = MAX(hi, l->hmax[i]); }
    int ty, tm, td;
    kit_today(&ty, &tm, &td);
    i64 today = kit_days(ty, tm, td);
    for (int i = 0; i < l->ndays; i++) {
        int yy = dr.y + 40 + i * 40;
        int dy, dm, dd;
        kit_from_days(today + i, &dy, &dm, &dd);
        char dn[16];
        snprintf(dn, sizeof dn, "%s", i == 0 ? "Hoje" : kit_wday_short[kit_dow(dy, dm, dd)]);
        if (i) dn[0] = (char)toupper(dn[0]);
        ui_label(u, dr.x + 18, yy + 10, dn, FONT_SEMIBOLD, 15, RGB(255, 255, 255));
        glyph_cond(s, dr.x + 90, yy + 4, 28, l->dcode[i], true);
        char mn[8], mx[8];
        deg(mn, 8, l->hmin[i]); deg(mx, 8, l->hmax[i]);
        int bx = dr.x + 190, bw = cw - 190 - 70;
        ui_label(u, dr.x + 150, yy + 10, mn, FONT_SEMIBOLD, 15, RGBA(255, 255, 255, 170));
        ui_label(u, bx + bw + 14, yy + 10, mx, FONT_SEMIBOLD, 15, RGB(255, 255, 255));
        gfx_round_rect(s, bx, yy + 18, bw, 5, 2.5f, RGBA(0, 0, 0, 40));
        float f0 = hi > lo ? (float)((l->hmin[i] - lo) / (hi - lo)) : 0, f1 = hi > lo ? (float)((l->hmax[i] - lo) / (hi - lo)) : 1;
        int ga = bx + (int)(f0 * bw), gb = bx + (int)(f1 * bw);
        gfx_round_rect_ex(s, ga, yy + 18, MAX(6, gb - ga), 5, 2.5f, 2.5f, 2.5f, 2.5f, RGB(120, 220, 160), RGB(255, 180, 60));
        gfx_grad_h(s, ga + 2, yy + 18, MAX(2, gb - ga - 4), 5, RGB(110, 210, 200), RGB(255, 170, 50));
        if (i < l->ndays - 1) gfx_fill(s, dr.x + 16, yy + 38, cw - 32, 1, RGBA(255, 255, 255, 40));
    }
    cy += dr.h + 14;
    /* blocos de detalhe */
    struct { const char *t; char v[24]; char d[48]; } tiles[6];
    int nt = 0;
    tiles[nt].t = "SENSAÇÃO TÉRMICA"; deg(tiles[nt].v, 24, l->feels); snprintf(tiles[nt].d, 48, "%s", l->feels > l->temp + 1 ? "Parece mais quente." : l->feels < l->temp - 1 ? "O vento deixa mais frio." : "Parecida com a real."); nt++;
    tiles[nt].t = "UMIDADE"; snprintf(tiles[nt].v, 24, "%d%%", (int)l->hum); snprintf(tiles[nt].d, 48, "%s", l->hum > 75 ? "Ar bem úmido." : l->hum < 35 ? "Ar seco." : "Umidade confortável."); nt++;
    static const char *dirs[8] = {"N", "NE", "L", "SE", "S", "SO", "O", "NO"};
    tiles[nt].t = "VENTO"; snprintf(tiles[nt].v, 24, "%d km/h", (int)(l->wind + 0.5)); snprintf(tiles[nt].d, 48, "Direção %s", dirs[((int)(l->wdir + 22.5) / 45) % 8]); nt++;
    tiles[nt].t = "PRESSÃO"; snprintf(tiles[nt].v, 24, "%d hPa", (int)(l->press + 0.5)); snprintf(tiles[nt].d, 48, "Ao nível do mar"); nt++;
    tiles[nt].t = "NASCER DO SOL"; snprintf(tiles[nt].v, 24, "%s", l->sunrise); snprintf(tiles[nt].d, 48, "Pôr do sol: %s", l->sunset); nt++;
    tiles[nt].t = "PRECIPITAÇÃO"; snprintf(tiles[nt].v, 24, "%d mm", (int)(l->precip + 0.5)); snprintf(tiles[nt].d, 48, "Na última hora"); nt++;
    int tcols = cw >= 600 ? 3 : 2, tw = (cw - (tcols - 1) * 14) / tcols;
    for (int i = 0; i < nt; i++) {
        rect_t r = rect_make(x0 + (i % tcols) * (tw + 14), cy + (i / tcols) * 134, tw, 120);
        card(s, r);
        ui_label(u, r.x + 16, r.y + 12, tiles[i].t, FONT_SEMIBOLD, 11.5f, RGBA(255, 255, 255, 170));
        ui_label(u, r.x + 16, r.y + 36, tiles[i].v, FONT_REGULAR, 30, RGB(255, 255, 255));
        ui_label(u, r.x + 16, r.y + 90, tiles[i].d, FONT_REGULAR, 12.5f, RGBA(255, 255, 255, 220));
    }
    cy += (nt + tcols - 1) / tcols * 134 + 10;
    ui_label_center(u, rect_make(a.x, cy, a.w, 16), "Dados meteorológicos: Open-Meteo.com (CC BY 4.0)", FONT_REGULAR, 11, RGBA(255, 255, 255, 170));
    cy += 30;
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, cy + W.scroll - view.y, &W.scroll);
    if (l->req || W.geo) u->repaint = true;
}

static void tick(app_t *a) { poll_all(); }
static void on_close(window_t *w) { W.w = NULL; }
static void launch(app_t *a) {
    if (W.w) { wm_focus(W.w); return; }
    load();
    W.w = wm_create(&app_tempo, "Tempo", -1, -1, 1020, 700, WIN_RESIZABLE);
    if (!W.w) return;
    W.w->paint = paint;
    W.w->on_close = on_close;
    W.w->toolbar_h = TOOLBAR_H;
    W.w->sidebar_w = 280;
    W.w->min_w = 760;
    W.w->min_h = 520;
    W.w->title_x = -1;
    for (int i = 0; i < W.n; i++) if (!W.l[i].have && !W.l[i].req) fetch(&W.l[i]);
}
static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(60, 150, 240), RGB(30, 90, 200));
    glyph_cond(s, x + 12 * u, y + 12 * u, 40 * u, 2, true);
    gfx_circle(s, x + 24 * u, y + 24 * u, 8 * u, RGB(255, 204, 0));
    glyph_cond(s, x + 14 * u, y + 16 * u, 38 * u, 3, true);
}
app_t app_tempo = {.name = "Tempo", .exec = "tempo", .draw_icon = icon, .launch = launch, .tick = tick};
