/* mOS — Mapas: mapa do OpenStreetMap em tiles (arrastar, roda/± para zoom, cache em memória), busca de
 * lugares (Nominatim), cartão do lugar, favoritos (~/Biblioteca/Mapas/favoritos.tsv) e rotas de carro,
 * a pé ou de bicicleta (OSRM) com distância e tempo. Dados © colaboradores do OpenStreetMap. */
#include "kit.h"
#include "net.h"

extern app_t app_mapas;
#define NTILE 120
#define MAXFAV 32
#define MAXRP 2000

typedef struct { int z, x, y; surface_t *s; http_req_t *req; u64 used; bool failed; } tile_t;
typedef struct { char name[96], addr[160]; double lat, lon; } place_t;

static struct {
    window_t *w;
    tile_t t[NTILE];
    int z;
    double cx, cy;            /* centro em pixels do mundo no zoom z */
    bool dragging;
    int dmx, dmy;
    double dcx, dcy;
    char search[96];
    http_req_t *sreq;
    place_t res[8];
    int nres;
    place_t pin;
    bool has_pin;
    place_t fav[MAXFAV];
    int nfav;
    bool loaded;
    /* rota */
    place_t from;
    bool has_from;
    http_req_t *rreq;
    float *route;             /* pares lat,lon */
    int nroute;
    double rdist, rtime;
    int mode;                 /* 0 carro, 1 a pé, 2 bicicleta */
    char status[96];
} M = {.z = 12};

/* ------------------------------------------------------------ projeção */
static double world(int z) { return 256.0 * (double)(1u << z); }
static double lon2x(double lon, int z) { return (lon + 180.0) / 360.0 * world(z); }
static double lat2y(double lat, int z) {
    double r = lat * KIT_PI / 180;
    return (1 - kit_log(kit_tan(r) + 1 / kit_cos(r)) / KIT_PI) / 2 * world(z);
}
static double x2lon(double x, int z) { return x / world(z) * 360.0 - 180.0; }
static double y2lat(double y, int z) {
    double n = KIT_PI - 2 * KIT_PI * y / world(z);
    return 180.0 / KIT_PI * kit_atan(0.5 * (kit_exp(n) - kit_exp(-n)));
}
static void center_on(double lat, double lon, int z) { M.z = CLAMP(z, 2, 18); M.cx = lon2x(lon, M.z); M.cy = lat2y(lat, M.z); }

/* ------------------------------------------------------------ tiles */
static int inflight(void) { int n = 0; for (int i = 0; i < NTILE; i++) if (M.t[i].req) n++; return n; }
static tile_t *tile_get(int z, int x, int y) {
    int n = 1 << z;
    x = ((x % n) + n) % n;
    if (y < 0 || y >= n) return NULL;
    tile_t *lru = NULL;
    for (int i = 0; i < NTILE; i++) {
        tile_t *t = &M.t[i];
        if ((t->s || t->req || t->failed) && t->z == z && t->x == x && t->y == y) { t->used = timer_ticks(); return t; }
        if (!t->req && (!lru || t->used < lru->used)) lru = t;
    }
    if (!lru || inflight() >= 6) return NULL;
    if (lru->s) surface_free(lru->s);
    memset(lru, 0, sizeof *lru);
    lru->z = z; lru->x = x; lru->y = y; lru->used = timer_ticks();
    char url[128];
    snprintf(url, sizeof url, "https://tile.openstreetmap.org/%d/%d/%d.png", z, x, y);
    lru->req = http_get(url);
    return lru;
}
static void tiles_poll(void) {
    for (int i = 0; i < NTILE; i++) {
        tile_t *t = &M.t[i];
        if (!t->req) continue;
        int st = http_poll(t->req);
        if (!st) continue;
        if (st > 0 && t->req->status == 200 && t->req->body) t->s = image_decode(t->req->body, t->req->body_len);
        if (!t->s) t->failed = true;
        http_free(t->req);
        t->req = NULL;
        if (M.w) wm_invalidate(M.w);
    }
}

/* ------------------------------------------------------------ busca, favoritos e rotas */
static void fav_save(void) {
    sbuf_t b = {0};
    sb_puts(&b, "# mOS Mapas 1\n");
    for (int i = 0; i < M.nfav; i++) {
        char n[96], a[160];
        kit_field(n, 96, M.fav[i].name);
        kit_field(a, 160, M.fav[i].addr);
        sb_printf(&b, "F\t%s\t%s\t%d\t%d\n", n, a, (int)(M.fav[i].lat * 1e6), (int)(M.fav[i].lon * 1e6));
    }
    char p[200];
    kit_save(kit_lib("Mapas", "favoritos.tsv", p, sizeof p), &b);
}
static void load(void) {
    if (M.loaded) return;
    M.loaded = true;
    center_on(-23.5505, -46.6333, 12);
    char p[200];
    char *buf = kit_read(kit_lib("Mapas", "favoritos.tsv", p, sizeof p), NULL);
    if (!buf) return;
    char *cur = buf, *ln, *f[6];
    while ((ln = kit_line(&cur)) && M.nfav < MAXFAV) {
        int n = kit_split(ln, f, 6);
        if (strcmp(f[0], "F") || n < 5) continue;
        place_t *pl = &M.fav[M.nfav++];
        snprintf(pl->name, 96, "%s", f[1]);
        snprintf(pl->addr, 160, "%s", f[2]);
        pl->lat = atoi(f[3]) / 1e6; pl->lon = atoi(f[4]) / 1e6;
    }
    kfree(buf);
}
static bool is_fav(place_t *p) { for (int i = 0; i < M.nfav; i++) if (kit_fabs(M.fav[i].lat - p->lat) < 1e-5 && kit_fabs(M.fav[i].lon - p->lon) < 1e-5) return true; return false; }
static void fmt_c(char *o, int n, double v) {
    bool neg = v < 0;
    if (neg) v = -v;
    long ip = (long)v, fp = (long)((v - ip) * 1000000 + 0.5);
    if (fp >= 1000000) { ip++; fp -= 1000000; }
    snprintf(o, n, "%s%ld.%06ld", neg ? "-" : "", ip, fp);
}
static void do_search(void) {
    if (M.sreq || !M.search[0]) return;
    sbuf_t q = {0};
    sb_puts(&q, "https://nominatim.openstreetmap.org/search?format=jsonv2&limit=8&accept-language=pt-BR&q=");
    sb_urlenc(&q, M.search);
    M.sreq = http_get(q.p);
    sb_free(&q);
    M.nres = 0;
    snprintf(M.status, sizeof M.status, "Buscando…");
}
static void do_route(void) {
    if (M.rreq || !M.has_from || !M.has_pin) return;
    static const char *prof[3] = {"driving", "foot", "bike"};
    char a1[24], a2[24], b1[24], b2[24], url[400];
    fmt_c(a1, 24, M.from.lon); fmt_c(a2, 24, M.from.lat); fmt_c(b1, 24, M.pin.lon); fmt_c(b2, 24, M.pin.lat);
    snprintf(url, sizeof url, "https://router.project-osrm.org/route/v1/%s/%s,%s;%s,%s?overview=full&geometries=geojson", prof[M.mode], a1, a2, b1, b2);
    M.rreq = http_get(url);
    snprintf(M.status, sizeof M.status, "Calculando a rota…");
}
static void maps_poll(void) {
    tiles_poll();
    if (M.sreq) {
        int st = http_poll(M.sreq);
        if (st) {
            M.nres = 0;
            if (st > 0 && M.sreq->body) {
                char *j = kmalloc(M.sreq->body_len + 1);
                memcpy(j, M.sreq->body, M.sreq->body_len);
                j[M.sreq->body_len] = 0;
                for (int i = 0; i < 8; i++) {
                    char p[48], lat[32], lon[32], full[400];
                    snprintf(p, sizeof p, "[%d].display_name", i);
                    if (!kit_json_str(j, p, full, sizeof full)) break;
                    place_t *pl = &M.res[M.nres++];
                    char *comma = strchr(full, ',');
                    snprintf(pl->name, 96, "%.*s", comma ? (int)(comma - full) : 95, full);
                    snprintf(pl->addr, 160, "%s", comma ? comma + 2 : "");
                    snprintf(p, sizeof p, "[%d].lat", i); kit_json_str(j, p, lat, sizeof lat);
                    snprintf(p, sizeof p, "[%d].lon", i); kit_json_str(j, p, lon, sizeof lon);
                    /* "-23.55" → double */
                    char *e;
                    double v = (double)strtol(lat, &e, 10), f = 0.1;
                    bool ng = lat[0] == '-';
                    if (*e == '.') for (e++; isdigit((u8)*e); e++, f /= 10) v += (ng ? -1 : 1) * (*e - '0') * f;
                    pl->lat = v;
                    v = (double)strtol(lon, &e, 10); f = 0.1; ng = lon[0] == '-';
                    if (*e == '.') for (e++; isdigit((u8)*e); e++, f /= 10) v += (ng ? -1 : 1) * (*e - '0') * f;
                    pl->lon = v;
                }
                kfree(j);
                snprintf(M.status, sizeof M.status, M.nres ? "" : "Nenhum resultado");
            } else snprintf(M.status, sizeof M.status, "Sem conexão");
            http_free(M.sreq);
            M.sreq = NULL;
        }
    }
    if (M.rreq) {
        int st = http_poll(M.rreq);
        if (st) {
            if (M.route) { kfree(M.route); M.route = NULL; }
            M.nroute = 0;
            if (st > 0 && M.rreq->body) {
                char *j = kmalloc(M.rreq->body_len + 1);
                memcpy(j, M.rreq->body, M.rreq->body_len);
                j[M.rreq->body_len] = 0;
                M.rdist = kit_json_num(j, "routes[0].distance", 0);
                M.rtime = kit_json_num(j, "routes[0].duration", 0);
                const char *c = kit_json_get(j, "routes[0].geometry.coordinates");
                if (c && *c == '[') {
                    M.route = kmalloc(sizeof(float) * 2 * MAXRP);
                    c++;
                    while (*c && M.nroute < MAXRP) {
                        c = kit_json_ws(c);
                        if (*c != '[') break;
                        c++;
                        char *e;
                        double v[2];
                        for (int k = 0; k < 2; k++) {
                            c = kit_json_ws(c);
                            bool ng = *c == '-';
                            double x = (double)strtol(c, &e, 10), f = 0.1;
                            c = e;
                            if (*c == '.') for (c++; isdigit((u8)*c); c++, f /= 10) x += (ng ? -1 : 1) * (*c - '0') * f;
                            v[k] = x;
                            c = kit_json_ws(c);
                            if (*c == ',') c++;
                        }
                        while (*c && *c != ']') c++;
                        if (*c) c++;
                        M.route[M.nroute * 2] = (float)v[1];
                        M.route[M.nroute * 2 + 1] = (float)v[0];
                        M.nroute++;
                        c = kit_json_ws(c);
                        if (*c == ',') c++;
                    }
                }
                kfree(j);
                M.status[0] = 0;
                if (!M.nroute) snprintf(M.status, sizeof M.status, "Nenhuma rota encontrada");
            } else snprintf(M.status, sizeof M.status, "Sem conexão");
            http_free(M.rreq);
            M.rreq = NULL;
        }
    }
}

/* ------------------------------------------------------------ desenho */
static void pin(surface_t *s, float x, float y, u32 c) {
    gfx_shadow(s, (int)x - 10, (int)y - 34, 20, 34, 10, 6, 70, 3);
    gfx_circle(s, x, y - 24, 13, RGB(255, 255, 255));
    gfx_circle(s, x, y - 24, 11, c);
    gfx_circle(s, x, y - 24, 4, RGB(255, 255, 255));
    float t[] = {x - 6, y - 15, x + 6, y - 15, x, y};
    gfx_polygon(s, t, 3, c);
}
static void place_row(ui_t *u, int id, rect_t r, place_t *pl, bool sel) {
    surface_t *s = u->s;
    if (sel || ui_hit(u, r)) gfx_round_rect(s, r.x, r.y, r.w, r.h, 8, T.sidebar_sel);
    gfx_circle(s, r.x + 18, r.y + r.h / 2.0f, 12, RGB(255, 59, 48));
    gfx_circle(s, r.x + 18, r.y + r.h / 2.0f, 4, RGB(255, 255, 255));
    text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 13, r.x + 38, r.y + 19, r.w - 46, T.text, pl->name);
    text_draw_ellipsis(s, font_get(FONT_REGULAR), 11.5f, r.x + 38, r.y + 36, r.w - 46, T.text2, pl->addr);
    if (ui_clicked(u, id, r)) { M.pin = *pl; M.has_pin = true; center_on(pl->lat, pl->lon, MAX(M.z, 15)); }
}

static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    load();
    maps_poll();
    int sb = w->sidebar_w;
    /* barra lateral */
    int y = w->toolbar_h + 4;
    if (ui_searchfield(u, UI_ID, rect_make(12, y, sb - 24, 28), M.search, sizeof M.search)) do_search();
    if (!M.search[0]) M.nres = 0;
    y += 40;
    if (M.search[0] && (M.nres || M.sreq || M.status[0])) {
        ui_label(u, 18, y, "Resultados", FONT_BOLD, 11.5f, T.text2);
        y += 20;
        for (int i = 0; i < M.nres; i++) { place_row(u, 114000 + i, rect_make(10, y, sb - 20, 46), &M.res[i], false); y += 48; }
        if (!M.nres) ui_label(u, 18, y, M.sreq ? "Buscando…" : M.status, FONT_REGULAR, 12.5f, T.text2);
    } else {
        ui_label(u, 18, y, "Favoritos", FONT_BOLD, 11.5f, T.text2);
        y += 20;
        for (int i = 0; i < M.nfav; i++) { place_row(u, 114100 + i, rect_make(10, y, sb - 20, 46), &M.fav[i], false); y += 48; }
        if (!M.nfav) ui_label(u, 18, y, "Busque um lugar e toque em Favoritar.", FONT_REGULAR, 12, T.text3);
    }
    /* mapa */
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, RGB(242, 239, 233));
    rect_t pc = surface_push_clip(s, a);
    double ox = M.cx - a.w / 2.0, oy = M.cy - a.h / 2.0;
    int tx0 = (int)kit_floor(ox / 256), ty0 = (int)kit_floor(oy / 256);
    int tx1 = (int)kit_floor((ox + a.w) / 256), ty1 = (int)kit_floor((oy + a.h) / 256);
    bool loading = false;
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) {
            int px = a.x + (int)(tx * 256 - ox), py = a.y + (int)(ty * 256 - oy);
            tile_t *t = tile_get(M.z, tx, ty);
            if (t && t->s) { gfx_blit(s, t->s, px, py); continue; }
            /* sem o tile: usa o do zoom anterior ampliado, se estiver no cache */
            bool drawn = false;
            for (int i = 0; i < NTILE && M.z > 0; i++) {
                tile_t *p = &M.t[i];
                int n = 1 << M.z, wx = ((tx % n) + n) % n;
                if (!p->s || p->z != M.z - 1 || p->x != wx / 2 || p->y != ty / 2) continue;
                surface_t tmp = *p->s;
                (void)tmp;
                rect_t src = rect_make((wx % 2) * 128, (ty % 2) * 128, 128, 128);
                surface_t *sub = surface_new(128, 128);
                gfx_blit_rect(sub, p->s, src, 0, 0);
                gfx_blit_scaled(s, sub, px, py, 256, 256, 255);
                surface_free(sub);
                drawn = true;
                break;
            }
            if (!drawn) {
                for (int k = 0; k < 256; k += 32) { gfx_fill(s, px + k, py, 1, 256, RGB(228, 224, 218)); gfx_fill(s, px, py + k, 256, 1, RGB(228, 224, 218)); }
            }
            if (!t || t->req) loading = true;
        }
    /* rota */
    if (M.nroute > 1) {
        for (int i = 1; i < M.nroute; i++) {
            float x0 = (float)(a.x + lon2x(M.route[i * 2 - 1], M.z) - ox), y0 = (float)(a.y + lat2y(M.route[i * 2 - 2], M.z) - oy);
            float x1 = (float)(a.x + lon2x(M.route[i * 2 + 1], M.z) - ox), y1 = (float)(a.y + lat2y(M.route[i * 2], M.z) - oy);
            gfx_line(s, x0, y0, x1, y1, 7, RGB(255, 255, 255));
            gfx_line(s, x0, y0, x1, y1, 5, RGB(10, 132, 255));
        }
    }
    if (M.has_from) {
        float fx = (float)(a.x + lon2x(M.from.lon, M.z) - ox), fy = (float)(a.y + lat2y(M.from.lat, M.z) - oy);
        gfx_circle(s, fx, fy, 9, RGB(255, 255, 255));
        gfx_circle(s, fx, fy, 6.5f, RGB(48, 199, 89));
    }
    if (M.has_pin) pin(s, (float)(a.x + lon2x(M.pin.lon, M.z) - ox), (float)(a.y + lat2y(M.pin.lat, M.z) - oy), RGB(255, 59, 48));
    else for (int i = 0; i < M.nres; i++) pin(s, (float)(a.x + lon2x(M.res[i].lon, M.z) - ox), (float)(a.y + lat2y(M.res[i].lat, M.z) - oy), RGB(255, 120, 90));
    /* atribuição */
    const char *attr = "© colaboradores do OpenStreetMap";
    int aw = text_width(font_get(FONT_REGULAR), 10.5f, attr);
    gfx_round_rect(s, a.x + a.w - aw - 14, a.y + a.h - 20, aw + 10, 16, 4, RGBA(255, 255, 255, 200));
    text_draw(s, font_get(FONT_REGULAR), 10.5f, a.x + a.w - aw - 9, a.y + a.h - 8, RGB(60, 60, 66), attr);
    /* controles de zoom */
    rect_t zc = rect_make(a.x + a.w - 50, a.y + 14, 36, 72);
    glass_panel(s, zc.x, zc.y, zc.w, zc.h, 10, GLASS_CONTROL);
    text_draw_center(s, font_get(FONT_SEMIBOLD), 18, rect_make(zc.x, zc.y, 36, 36), T.text, "+");
    text_draw_center(s, font_get(FONT_SEMIBOLD), 18, rect_make(zc.x, zc.y + 36, 36, 36), T.text, "−");
    gfx_fill(s, zc.x + 6, zc.y + 36, 24, 1, T.separator);
    int zdir = 0;
    if (ui_clicked(u, UI_ID, rect_make(zc.x, zc.y, 36, 36))) zdir = 1;
    if (ui_clicked(u, UI_ID, rect_make(zc.x, zc.y + 36, 36, 36))) zdir = -1;
    /* bússola */
    gfx_circle(s, a.x + a.w - 32, a.y + 112, 16, T.dark ? RGBA(40, 40, 44, 220) : RGBA(255, 255, 255, 230));
    float nt[] = {a.x + a.w - 32.0f, a.y + 100.0f, a.x + a.w - 27.0f, a.y + 113.0f, a.x + a.w - 37.0f, a.y + 113.0f};
    gfx_polygon(s, nt, 3, RGB(255, 59, 48));
    text_draw_center(s, font_get(FONT_BOLD), 8, rect_make(a.x + a.w - 40, a.y + 112, 16, 12), T.text, "N");
    /* cartão do lugar */
    if (M.has_pin) {
        rect_t c = rect_make(a.x + 14, a.y + 14, MIN(340, a.w - 90), 178);
        glass_panel(s, c.x, c.y, c.w, c.h, 16, GLASS_REGULAR);
        text_draw_ellipsis(s, font_get(FONT_BOLD), 17, c.x + 16, c.y + 30, c.w - 32, T.glass_text, M.pin.name);
        text_draw_ellipsis(s, font_get(FONT_REGULAR), 12, c.x + 16, c.y + 50, c.w - 32, T.glass_text2, M.pin.addr);
        char la[24], lo[24], co[64];
        fmt_c(la, 24, M.pin.lat); fmt_c(lo, 24, M.pin.lon);
        snprintf(co, sizeof co, "%s, %s", la, lo);
        text_draw(s, font_get(FONT_REGULAR), 11, c.x + 16, c.y + 68, T.glass_text2, co);
        bool fav = is_fav(&M.pin);
        int bw = (c.w - 32 - 16) / 3;
        if (ui_button_style(u, UI_ID, rect_make(c.x + 16, c.y + 82, bw, 28), fav ? "Favorito" : "Favoritar", fav ? BTN_TINTED : BTN_GRAY, true, NULL)) {
            if (fav) { for (int i = 0; i < M.nfav; i++) if (kit_fabs(M.fav[i].lat - M.pin.lat) < 1e-5 && kit_fabs(M.fav[i].lon - M.pin.lon) < 1e-5) { memmove(&M.fav[i], &M.fav[i + 1], sizeof(place_t) * (M.nfav - i - 1)); M.nfav--; break; } }
            else if (M.nfav < MAXFAV) M.fav[M.nfav++] = M.pin;
            fav_save();
        }
        if (ui_button_style(u, UI_ID, rect_make(c.x + 24 + bw, c.y + 82, bw, 28), "Partida", M.has_from && kit_fabs(M.from.lat - M.pin.lat) < 1e-6 ? BTN_TINTED : BTN_GRAY, true, NULL)) { M.from = M.pin; M.has_from = true; M.nroute = 0; }
        if (ui_button_style(u, UI_ID, rect_make(c.x + 32 + 2 * bw, c.y + 82, bw, 28), "Rotas", BTN_FILLED, M.has_from, NULL)) do_route();
        static const char *modes[3] = {"Carro", "A pé", "Bicicleta"};
        int nm = ui_segmented(u, UI_ID, rect_make(c.x + 16, c.y + 118, c.w - 32, 24), modes, 3, M.mode);
        if (nm != M.mode) { M.mode = nm; if (M.nroute) do_route(); }
        char info[96] = "";
        if (M.nroute > 1) {
            int mins = (int)(M.rtime / 60 + 0.5);
            char d[24];
            kit_fmt_num(d, sizeof d, M.rdist / 1000, 1);
            if (mins >= 60) snprintf(info, sizeof info, "%s km · %d h %02d min", d, mins / 60, mins % 60);
            else snprintf(info, sizeof info, "%s km · %d min", d, mins);
        } else if (M.status[0]) snprintf(info, sizeof info, "%s", M.status);
        else if (!M.has_from) snprintf(info, sizeof info, "Defina a partida em outro lugar para traçar a rota.");
        text_draw_ellipsis(s, font_get(M.nroute > 1 ? FONT_SEMIBOLD : FONT_REGULAR), 12.5f, c.x + 16, c.y + 164, c.w - 50, T.glass_text, info);
        rect_t xb = rect_make(c.x + c.w - 30, c.y + 10, 20, 20);
        gfx_circle(s, xb.x + 10, xb.y + 10, 10, RGBA(128, 128, 128, 80));
        gfx_line(s, xb.x + 6, xb.y + 6, xb.x + 14, xb.y + 14, 1.5f, T.glass_text);
        gfx_line(s, xb.x + 14, xb.y + 6, xb.x + 6, xb.y + 14, 1.5f, T.glass_text);
        if (ui_clicked(u, UI_ID, xb)) { M.has_pin = false; M.nroute = 0; }
    }
    surface_pop_clip(s, pc);
    /* interação com o mapa */
    bool over_ui = M.has_pin && rect_contains(rect_make(a.x + 14, a.y + 14, MIN(340, a.w - 90), 178), u->mx, u->my);
    over_ui |= rect_contains(zc, u->mx, u->my);
    if (u->pressed && ui_hit(u, a) && !over_ui) { M.dragging = true; M.dmx = u->mx; M.dmy = u->my; M.dcx = M.cx; M.dcy = M.cy; }
    if (M.dragging) {
        if (!u->down) {
            M.dragging = false;
            /* clique sem arrastar com duplo clique: aproxima ali */
        } else { M.cx = M.dcx - (u->mx - M.dmx); M.cy = M.dcy - (u->my - M.dmy); u->cursor = CURSOR_GRABBING; }
    }
    if (u->dblclick && ui_hit(u, a) && !over_ui) zdir = 1;
    if (u->wheel && ui_hit(u, a)) { zdir = u->wheel > 0 ? 1 : -1; u->wheel = 0; }
    if (zdir) {
        int nz = CLAMP(M.z + zdir, 2, 19);
        if (nz != M.z) {
            /* mantém o ponto sob o cursor (ou o centro) parado */
            double fx = (zdir && ui_hit(u, a) && !over_ui) ? u->mx - a.x - a.w / 2.0 : 0, fy = (zdir && ui_hit(u, a) && !over_ui) ? u->my - a.y - a.h / 2.0 : 0;
            double wx = M.cx + fx, wy = M.cy + fy, k = nz > M.z ? 2.0 : 0.5;
            M.cx = wx * k - fx; M.cy = wy * k - fy;
            M.z = nz;
        }
    }
    double wmax = world(M.z);
    M.cy = CLAMP(M.cy, a.h / 2.0, wmax - a.h / 2.0);
    M.cx = kit_fmod(M.cx, wmax);
    if (loading || M.sreq || M.rreq) u->repaint = true;
    if (u->key && !u->focus) {
        double step = 120;
        if (u->key == KEY_LEFT) M.cx -= step; else if (u->key == KEY_RIGHT) M.cx += step;
        else if (u->key == KEY_UP) M.cy -= step; else if (u->key == KEY_DOWN) M.cy += step;
        else if (u->key == '+' || u->key == '=') { M.cx *= 2; M.cy *= 2; M.z = MIN(19, M.z + 1); }
        else if (u->key == '-') { M.cx /= 2; M.cy /= 2; M.z = MAX(2, M.z - 1); }
        u->key_used = true;
    }
}

static void tick(app_t *a) { maps_poll(); }
static void on_close(window_t *w) { M.w = NULL; }
static void launch(app_t *a) {
    if (M.w) { wm_focus(M.w); return; }
    load();
    M.w = wm_create(&app_mapas, "Mapas", -1, -1, 1080, 700, WIN_RESIZABLE);
    if (!M.w) return;
    M.w->paint = paint;
    M.w->on_close = on_close;
    M.w->toolbar_h = TOOLBAR_H;
    M.w->sidebar_w = 280;
    M.w->min_w = 720;
    M.w->min_h = 480;
}
static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(170, 220, 150), RGB(120, 190, 120));
    gfx_line(s, x + 6 * u, y + 46 * u, x + 58 * u, y + 20 * u, 6 * u, RGB(255, 255, 255));
    gfx_line(s, x + 6 * u, y + 46 * u, x + 58 * u, y + 20 * u, 3.5f * u, RGB(255, 204, 0));
    gfx_line(s, x + 26 * u, y + 6 * u, x + 38 * u, y + 58 * u, 4 * u, RGB(255, 255, 255));
    gfx_fill(s, (int)(x + 40 * u), (int)(y + 36 * u), (int)(14 * u), (int)(18 * u), RGB(130, 190, 240));
    gfx_circle(s, x + 22 * u, y + 24 * u, 7 * u, RGB(10, 132, 255));
    gfx_circle(s, x + 22 * u, y + 24 * u, 3 * u, RGB(255, 255, 255));
}
app_t app_mapas = {.name = "Mapas", .exec = "mapas", .draw_icon = icon, .launch = launch, .tick = tick, .pinned = true};
