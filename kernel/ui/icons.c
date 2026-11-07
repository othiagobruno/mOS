/* mOS — ícones e glifos desenhados vetorialmente (arte original do projeto) */
#include "ui.h"
#include "compat.h"

/* ============================================================ glifos monocromáticos */
/* logo do mOS: um "m" de hastes arredondadas; com fundo opaco vira um disco com o "m" recortado */
void glyph_mos_logo(surface_t *s, float cx, float cy, float r, u32 c, u32 bg) {
    bool disc = C_A(bg) == 255;
    u32 ink = c;
    float k = disc ? 0.62f : 1.0f; /* tamanho do "m" relativo ao raio */
    if (disc) { gfx_circle(s, cx, cy, r, c); ink = bg; }
    float sw = r * 0.24f * k, half = r * 0.78f * k;
    float x0 = cx - half + sw / 2, x2 = cx + half - sw / 2, x1 = (x0 + x2) / 2;
    float R = (x1 - x0) / 2;
    float archy = cy - r * 0.18f * k, bot = cy + r * 0.62f * k;
    gfx_line(s, x0, archy - R * 0.9f, x0, bot, sw, ink);
    gfx_line(s, x1, archy, x1, bot, sw, ink);
    gfx_line(s, x2, archy, x2, bot, sw, ink);
    for (int a = 0; a < 2; a++) {
        float ax = a ? (x1 + x2) / 2 : (x0 + x1) / 2;
        gfx_ring_arc(s, ax, archy, R + sw / 2, sw, 3 * PI_F / 2, 2 * PI_F, ink);
        gfx_ring_arc(s, ax, archy, R + sw / 2, sw, 0, PI_F / 2, ink);
    }
}

void glyph_search(surface_t *s, int x, int y, int sz, u32 c) {
    float r = sz * 0.34f, t = MAX(1.4f, sz * 0.12f);
    gfx_circle_stroke(s, x + r + 1, y + r + 1, r, t, c);
    gfx_line(s, x + r * 1.7f + 1, y + r * 1.7f + 1, x + sz - 1.0f, y + sz - 1.0f, t * 1.15f, c);
}

void glyph_list(surface_t *s, int x, int y, int sz, u32 c) {
    for (int i = 0; i < 3; i++) {
        float yy = y + sz * (0.2f + i * 0.3f);
        gfx_circle(s, x + 1.5f, yy, 1.4f, c);
        gfx_line(s, x + 5, yy, x + sz, yy, 1.4f, c);
    }
}

void glyph_wifi(surface_t *s, int x, int y, int sz, u32 c, int bars) {
    float cx = x + sz / 2.0f, cy = y + sz * 0.85f;
    for (int i = 0; i < 3; i++) {
        float r = sz * (0.25f + i * 0.22f);
        gfx_ring_arc(s, cx, cy, r, sz * 0.11f, 5.5f, 2 * PI_F, i < bars || bars == 0 ? c : WITH_ALPHA(c, 80));
        gfx_ring_arc(s, cx, cy, r, sz * 0.11f, 0, 0.78f, i < bars || bars == 0 ? c : WITH_ALPHA(c, 80));
    }
    gfx_circle(s, cx, cy - 1, sz * 0.08f, c);
}

void glyph_ethernet(surface_t *s, int x, int y, int sz, u32 c) {
    /* duas setas opostas: troca de dados */
    float t = 1.5f;
    gfx_line(s, x + 2, y + sz * 0.35f, x + sz - 3, y + sz * 0.35f, t, c);
    gfx_line(s, x + sz - 3, y + sz * 0.35f, x + sz - 7, y + sz * 0.35f - 4, t, c);
    gfx_line(s, x + sz - 3, y + sz * 0.35f, x + sz - 7, y + sz * 0.35f + 4, t, c);
    gfx_line(s, x + 3, y + sz * 0.7f, x + sz - 2, y + sz * 0.7f, t, c);
    gfx_line(s, x + 3, y + sz * 0.7f, x + 7, y + sz * 0.7f - 4, t, c);
    gfx_line(s, x + 3, y + sz * 0.7f, x + 7, y + sz * 0.7f + 4, t, c);
}

void glyph_bluetooth(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f, t = 1.4f;
    float top = y + 1, bot = y + sz - 1, q1 = y + sz * 0.3f, q3 = y + sz * 0.7f;
    float l = cx - sz * 0.25f, r = cx + sz * 0.25f;
    gfx_line(s, cx, top, cx, bot, t, c);
    gfx_line(s, cx, top, r, q1, t, c);
    gfx_line(s, r, q1, l, q3, t, c);
    gfx_line(s, cx, bot, r, q3, t, c);
    gfx_line(s, r, q3, l, q1, t, c);
}

void glyph_speaker(surface_t *s, int x, int y, int sz, u32 c, int level) {
    float cy = y + sz / 2.0f;
    float body[] = {x + 1.0f, cy - sz * 0.17f, x + sz * 0.25f, cy - sz * 0.17f, x + sz * 0.5f, cy - sz * 0.4f,
                    x + sz * 0.5f, cy + sz * 0.4f, x + sz * 0.25f, cy + sz * 0.17f, x + 1.0f, cy + sz * 0.17f};
    gfx_polygon(s, body, 6, c);
    if (level < 0) {
        gfx_line(s, x + sz * 0.62f, cy - 3, x + sz * 0.9f, cy + 3, 1.4f, c);
        gfx_line(s, x + sz * 0.62f, cy + 3, x + sz * 0.9f, cy - 3, 1.4f, c);
        return;
    }
    for (int i = 0; i < 3; i++) {
        float r = sz * (0.18f + i * 0.17f);
        u32 cc = i < level ? c : WITH_ALPHA(c, 70);
        gfx_ring_arc(s, x + sz * 0.5f, cy, r, 1.4f, 0.9f, 2.25f, cc);
    }
}

void glyph_chevron(surface_t *s, int x, int y, int sz, u32 c, int dir) {
    float cx = x + sz / 2.0f, cy = y + sz / 2.0f, h = sz * 0.28f, t = MAX(1.4f, sz * 0.13f);
    if (dir == 0) { gfx_line(s, cx - h / 2, cy - h, cx + h / 2, cy, t, c); gfx_line(s, cx + h / 2, cy, cx - h / 2, cy + h, t, c); }
    else if (dir == 1) { gfx_line(s, cx + h / 2, cy - h, cx - h / 2, cy, t, c); gfx_line(s, cx - h / 2, cy, cx + h / 2, cy + h, t, c); }
    else if (dir == 2) { gfx_line(s, cx - h, cy - h / 2, cx, cy + h / 2, t, c); gfx_line(s, cx, cy + h / 2, cx + h, cy - h / 2, t, c); }
    else { gfx_line(s, cx - h, cy + h / 2, cx, cy - h / 2, t, c); gfx_line(s, cx, cy - h / 2, cx + h, cy + h / 2, t, c); }
}

void glyph_plus(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f, cy = y + sz / 2.0f, h = sz * 0.32f;
    gfx_line(s, cx - h, cy, cx + h, cy, 1.6f, c);
    gfx_line(s, cx, cy - h, cx, cy + h, 1.6f, c);
}

void glyph_reload(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f, cy = y + sz / 2.0f, r = sz * 0.32f;
    gfx_ring_arc(s, cx, cy, r + 0.8f, 1.6f, 0.6f, 2 * PI_F - 0.2f, c);
    float tri[] = {cx + 1, cy - r - 4, cx + 5.5f, cy - r + 0.5f, cx + 1, cy - r + 4};
    gfx_polygon(s, tri, 3, c);
}

void glyph_check(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_line(s, x + sz * 0.2f, y + sz * 0.55f, x + sz * 0.42f, y + sz * 0.78f, 1.8f, c);
    gfx_line(s, x + sz * 0.42f, y + sz * 0.78f, x + sz * 0.82f, y + sz * 0.25f, 1.8f, c);
}

void glyph_gear(surface_t *s, float cx, float cy, float r, u32 c, u32 hole) {
    float pts[64];
    int n = 0;
    int teeth = 10;
    for (int i = 0; i < teeth * 2 && n < 62; i++) {
        float a0 = (i * PI_F) / teeth, a1 = ((i + 1) * PI_F) / teeth;
        float rr = (i % 2) ? r * 0.78f : r;
        float am = (a0 + a1) / 2, half = (a1 - a0) * 0.42f;
        pts[n++] = cx + fsin(am - half) * rr; pts[n++] = cy - fcos(am - half) * rr;
    }
    gfx_polygon(s, pts, n / 2, c);
    gfx_circle(s, cx, cy, r * 0.78f, c);
    gfx_circle(s, cx, cy, r * 0.32f, hole);
}

void glyph_grid(surface_t *s, int x, int y, int sz, u32 c) {
    float h = sz * 0.42f;
    for (int i = 0; i < 4; i++) gfx_round_rect(s, x + (i % 2) * (h + 2), y + (i / 2) * (h + 2), h, h, 2, c);
}

void glyph_house(surface_t *s, int x, int y, int sz, u32 c) {
    float roof[] = {x + sz / 2.0f, y + 1.0f, x + sz - 1.0f, y + sz * 0.5f, x + 1.0f, y + sz * 0.5f};
    gfx_polygon(s, roof, 3, c);
    gfx_fill(s, x + sz / 5, y + sz / 2, sz * 3 / 5, sz / 2 - 1, c);
}

void glyph_clock(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f, cy = y + sz / 2.0f, r = sz / 2.0f - 1;
    gfx_circle_stroke(s, cx, cy, r, 1.4f, c);
    gfx_line(s, cx, cy, cx, cy - r * 0.6f, 1.4f, c);
    gfx_line(s, cx, cy, cx + r * 0.45f, cy, 1.4f, c);
}

/* ============================================================ ícones coloridos */
void icon_folder(surface_t *s, int x, int y, int sz, u32 tint) {
    float u = sz / 64.0f;
    u32 back = color_mix(tint, RGB(40, 90, 150), 60), front_t = color_lighten(tint, 30), front_b = tint;
    gfx_round_rect_ex(s, x + 4 * u, y + 10 * u, 24 * u, 10 * u, 3 * u, 3 * u, 0, 0, back, back);
    gfx_round_rect(s, x + 4 * u, y + 14 * u, 56 * u, 42 * u, 4 * u, back);
    gfx_shadow(s, (int)(x + 4 * u), (int)(y + 19 * u), (int)(56 * u), (int)(37 * u), 4 * u, 3 * u, 50, 0);
    gfx_round_rect_grad(s, x + 4 * u, y + 19 * u, 56 * u, 37 * u, 4 * u, front_t, front_b);
    gfx_fill(s, (int)(x + 6 * u), (int)(y + 20 * u), (int)(52 * u), 1, RGBA(255, 255, 255, 120));
}

void icon_document(surface_t *s, int x, int y, int sz, const char *ext) {
    float u = sz / 64.0f;
    float px = x + 12 * u, py = y + 4 * u, w = 40 * u, h = 56 * u, fold = 12 * u;
    float shape[] = {px, py, px + w - fold, py, px + w, py + fold, px + w, py + h, px, py + h};
    float sh[10];
    for (int i = 0; i < 10; i++) sh[i] = shape[i] + (i % 2 ? 1.2f * u : 0);
    gfx_polygon(s, sh, 5, RGBA(0, 0, 0, 60));
    gfx_polygon(s, shape, 5, RGB(252, 252, 252));
    float corner[] = {px + w - fold, py, px + w, py + fold, px + w - fold, py + fold};
    gfx_polygon(s, corner, 3, RGB(214, 214, 214));
    for (int i = 0; i < 6; i++) gfx_fill(s, (int)(px + 6 * u), (int)(py + (18 + i * 6) * u), (int)((i == 5 ? 16 : 28) * u), MAX(1, (int)u), RGB(196, 196, 196));
    if (ext && sz >= 32) {
        font_t *f = font_get(FONT_BOLD);
        float fs = 9 * u + 2;
        int tw = text_width(f, fs, ext);
        text_draw(s, f, fs, (int)(px + (w - tw) / 2), (int)(py + h - 5 * u), RGB(120, 120, 120), ext);
    }
}

void icon_disk(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    gfx_shadow(s, (int)(x + 4 * u), (int)(y + 16 * u), (int)(56 * u), (int)(34 * u), 5 * u, 4 * u, 70, (int)(2 * u));
    gfx_round_rect_grad(s, x + 4 * u, y + 16 * u, 56 * u, 34 * u, 5 * u, RGB(236, 236, 238), RGB(176, 176, 180));
    gfx_round_rect_stroke(s, x + 4 * u, y + 16 * u, 56 * u, 34 * u, 5 * u, 1, RGBA(0, 0, 0, 60));
    gfx_fill(s, (int)(x + 6 * u), (int)(y + 40 * u), (int)(52 * u), 1, RGBA(0, 0, 0, 50));
    gfx_circle(s, x + 52 * u, y + 45 * u, 2 * u, RGB(110, 220, 120));
    for (int i = 0; i < 6; i++) gfx_fill(s, (int)(x + (10 + i * 4) * u), (int)(y + 43 * u), (int)(2 * u), (int)(5 * u), RGBA(0, 0, 0, 50));
}

void icon_trash(surface_t *s, int x, int y, int sz, bool full) {
    float u = sz / 64.0f;
    float body[] = {x + 14 * u, y + 14 * u, x + 50 * u, y + 14 * u, x + 46 * u, y + 60 * u, x + 18 * u, y + 60 * u};
    gfx_polygon(s, body, 4, RGBA(225, 228, 232, 180));
    for (int i = 0; i < 6; i++) {
        float t = i / 5.0f;
        gfx_line(s, x + (17 + t * 30) * u, y + 16 * u, x + (20 + t * 24) * u, y + 58 * u, 1.1f * u, RGBA(120, 124, 130, 200));
    }
    for (int j = 0; j < 7; j++) {
        float yy = y + (20 + j * 6) * u, inset = j * 0.55f * u;
        gfx_line(s, x + 15 * u + inset, yy, x + 49 * u - inset, yy, 0.8f * u, RGBA(150, 154, 160, 160));
    }
    gfx_round_rect(s, x + 11 * u, y + 9 * u, 42 * u, 6 * u, 3 * u, RGB(205, 208, 214));
    gfx_round_rect_stroke(s, x + 11 * u, y + 9 * u, 42 * u, 6 * u, 3 * u, 1, RGBA(0, 0, 0, 60));
    if (full) for (int i = 0; i < 3; i++) gfx_round_rect(s, x + (20 + i * 8) * u, y + (4 + i * 2) * u, 12 * u, 8 * u, 2, RGB(250, 250, 250));
}

/* ------------------------------------------------------------ fundo "squircle" comum */
static void tile(surface_t *s, int x, int y, int sz, u32 top, u32 bot) {
    float u = sz / 64.0f;
    gfx_shadow(s, (int)(x + 5 * u), (int)(y + 5 * u), (int)(54 * u), (int)(54 * u), 12 * u, 4 * u, 70, (int)(1.5f * u));
    gfx_round_rect_grad(s, x + 5 * u, y + 5 * u, 54 * u, 54 * u, 12 * u, top, bot);
    gfx_round_rect_stroke(s, x + 5 * u, y + 5 * u, 54 * u, 54 * u, 12 * u, 1, RGBA(0, 0, 0, 40));
}

void icon_arquivos(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    tile(s, x, y, sz, RGB(96, 196, 255), RGB(18, 110, 228));
    /* pasta branca estilizada */
    gfx_round_rect_ex(s, x + 16 * u, y + 20 * u, 14 * u, 6 * u, 2 * u, 2 * u, 0, 0, RGBA(255, 255, 255, 200), RGBA(255, 255, 255, 200));
    gfx_round_rect(s, x + 16 * u, y + 24 * u, 32 * u, 22 * u, 3 * u, RGBA(255, 255, 255, 200));
    gfx_round_rect_grad(s, x + 16 * u, y + 28 * u, 32 * u, 18 * u, 3 * u, RGB(255, 255, 255), RGB(226, 238, 252));
}

void icon_horizonte(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f, cx = x + 32 * u, cy = y + 32 * u, r = 27 * u;
    gfx_circle(s, cx, cy + 1.5f * u, r, RGBA(0, 0, 0, 60));
    gfx_circle(s, cx, cy, r, RGB(250, 250, 252));
    gfx_round_rect_grad(s, cx - r + 2 * u, cy - r + 2 * u, 2 * r - 4 * u, 2 * r - 4 * u, r - 2 * u, RGB(40, 120, 230), RGB(130, 200, 255));
    /* sol nascendo no horizonte */
    surface_t *sc = s;
    rect_t pc = surface_push_clip(sc, rect_make((int)(cx - r), (int)(cy - r), (int)(2 * r), (int)(r + 6 * u)));
    gfx_circle(s, cx, cy + 6 * u, 11 * u, RGB(255, 196, 70));
    surface_pop_clip(sc, pc);
    rect_t pc2 = surface_push_clip(sc, rect_make((int)(cx - r), (int)(cy + 6 * u), (int)(2 * r), (int)(r)));
    gfx_circle(s, cx, cy, r - 2 * u, RGB(30, 80, 160));
    for (int i = 0; i < 3; i++) gfx_fill(s, (int)(cx - r), (int)(cy + (10 + i * 5) * u), (int)(2 * r), MAX(1, (int)(u)), RGBA(255, 255, 255, 60 - i * 15));
    surface_pop_clip(sc, pc2);
    gfx_line(s, cx - r + 3 * u, cy + 6 * u, cx + r - 3 * u, cy + 6 * u, 1.2f * u, RGBA(255, 255, 255, 220));
    gfx_circle_stroke(s, cx, cy, r, 2 * u, RGB(220, 222, 228));
}

void icon_terminal(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    tile(s, x, y, sz, RGB(70, 70, 74), RGB(30, 30, 32));
    gfx_round_rect(s, x + 9 * u, y + 9 * u, 46 * u, 46 * u, 9 * u, RGB(18, 18, 18));
    gfx_line(s, x + 17 * u, y + 22 * u, x + 25 * u, y + 28 * u, 2.6f * u, RGB(240, 240, 240));
    gfx_line(s, x + 25 * u, y + 28 * u, x + 17 * u, y + 34 * u, 2.6f * u, RGB(240, 240, 240));
    gfx_fill(s, (int)(x + 28 * u), (int)(y + 34 * u), (int)(12 * u), (int)(2.6f * u), RGB(240, 240, 240));
}

void icon_prefs(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    gfx_circle(s, x + 32 * u, y + 33.5f * u, 27 * u, RGBA(0, 0, 0, 60));
    gfx_round_rect_grad(s, x + 5 * u, y + 5 * u, 54 * u, 54 * u, 27 * u, RGB(178, 180, 186), RGB(120, 122, 128));
    glyph_gear(s, x + 32 * u, y + 32 * u, 20 * u, RGB(70, 72, 78), RGB(170, 172, 178));
    glyph_gear(s, x + 32 * u, y + 31 * u, 19 * u, RGB(232, 232, 236), RGB(150, 152, 158));
}

void icon_editor(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    gfx_shadow(s, (int)(x + 12 * u), (int)(y + 6 * u), (int)(40 * u), (int)(52 * u), 3 * u, 3 * u, 60, (int)u);
    gfx_round_rect(s, x + 12 * u, y + 6 * u, 40 * u, 52 * u, 3 * u, RGB(252, 252, 250));
    for (int i = 0; i < 7; i++) gfx_fill(s, (int)(x + 17 * u), (int)(y + (14 + i * 6) * u), (int)((i == 6 ? 18 : 30) * u), MAX(1, (int)(1.2f * u)), RGB(150, 150, 160));
    /* caneta */
    gfx_line(s, x + 52 * u, y + 14 * u, x + 32 * u, y + 46 * u, 6 * u, RGB(40, 100, 220));
    gfx_line(s, x + 52 * u, y + 14 * u, x + 49 * u, y + 19 * u, 6 * u, RGB(30, 30, 30));
    float tip[] = {x + 29.5f * u, y + 44 * u, x + 34.5f * u, y + 47.5f * u, x + 29 * u, y + 52 * u};
    gfx_polygon(s, tip, 3, RGB(230, 200, 150));
}

void icon_notas(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    tile(s, x, y, sz, RGB(255, 232, 120), RGB(250, 205, 60));
    gfx_round_rect_ex(s, x + 5 * u, y + 5 * u, 54 * u, 14 * u, 12 * u, 12 * u, 0, 0, RGB(248, 248, 246), RGB(232, 232, 230));
    for (int i = 0; i < 4; i++) gfx_fill(s, (int)(x + 12 * u), (int)(y + (28 + i * 7) * u), (int)(40 * u), MAX(1, (int)u), RGBA(160, 120, 20, 120));
}

void icon_calc(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    tile(s, x, y, sz, RGB(90, 90, 94), RGB(44, 44, 46));
    gfx_round_rect(s, x + 12 * u, y + 11 * u, 40 * u, 10 * u, 2 * u, RGB(170, 200, 170));
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 4; i++) {
            u32 c = i == 3 ? RGB(255, 149, 0) : RGB(180, 180, 184);
            gfx_round_rect(s, x + (12 + i * 10.5f) * u, y + (26 + j * 9.5f) * u, 8 * u, 7.5f * u, 2 * u, c);
        }
}

void icon_calendario(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    datetime_t dt;
    rtc_now(&dt);
    static const char *mo[12] = {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};
    gfx_shadow(s, (int)(x + 7 * u), (int)(y + 7 * u), (int)(50 * u), (int)(50 * u), 8 * u, 3 * u, 70, (int)u);
    gfx_round_rect(s, x + 7 * u, y + 7 * u, 50 * u, 50 * u, 8 * u, RGB(252, 252, 252));
    gfx_round_rect_ex(s, x + 7 * u, y + 7 * u, 50 * u, 14 * u, 8 * u, 8 * u, 0, 0, RGB(250, 80, 70), RGB(220, 50, 46));
    text_draw_center(s, font_get(FONT_SEMIBOLD), 9 * u + 1, rect_make((int)(x + 7 * u), (int)(y + 7 * u), (int)(50 * u), (int)(14 * u)), RGB(255, 255, 255), mo[dt.month - 1]);
    char d[4];
    snprintf(d, sizeof d, "%d", dt.day);
    text_draw_center(s, font_get(FONT_MEDIUM), 26 * u, rect_make((int)(x + 7 * u), (int)(y + 22 * u), (int)(50 * u), (int)(33 * u)), RGB(30, 30, 30), d);
}

void icon_atividade(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    tile(s, x, y, sz, RGB(56, 56, 60), RGB(18, 18, 20));
    for (int i = 0; i < 4; i++) gfx_fill(s, (int)(x + 10 * u), (int)(y + (16 + i * 10) * u), (int)(44 * u), 1, RGBA(80, 255, 120, 40));
    float pts[] = {10, 40, 18, 38, 23, 26, 29, 44, 35, 20, 41, 36, 47, 30, 54, 34};
    for (int i = 0; i < 7; i++)
        gfx_line(s, x + pts[i * 2] * u, y + pts[i * 2 + 1] * u, x + pts[i * 2 + 2] * u, y + pts[i * 2 + 3] * u, 1.8f * u, RGB(90, 240, 120));
}

void icon_console(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    tile(s, x, y, sz, RGB(240, 240, 240), RGB(200, 200, 204));
    gfx_round_rect(s, x + 11 * u, y + 11 * u, 42 * u, 42 * u, 5 * u, RGB(30, 30, 30));
    static const int lens[6] = {30, 22, 34, 18, 28, 12};
    for (int i = 0; i < 6; i++)
        gfx_fill(s, (int)(x + 15 * u), (int)(y + (16 + i * 6) * u), (int)(lens[i] * u), MAX(1, (int)(2 * u)), i == 2 ? RGB(250, 200, 60) : RGB(200, 200, 200));
}

void icon_musica(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    gfx_circle(s, x + 32 * u, y + 33.5f * u, 27 * u, RGBA(0, 0, 0, 60));
    gfx_round_rect_grad(s, x + 5 * u, y + 5 * u, 54 * u, 54 * u, 27 * u, RGB(255, 110, 140), RGB(200, 40, 120));
    gfx_line(s, x + 27 * u, y + 44 * u, x + 27 * u, y + 18 * u, 2.6f * u, RGB(255, 255, 255));
    gfx_line(s, x + 43 * u, y + 40 * u, x + 43 * u, y + 14 * u, 2.6f * u, RGB(255, 255, 255));
    float beam[] = {x + 25.7f * u, y + 18 * u, x + 44.3f * u, y + 13 * u, x + 44.3f * u, y + 19 * u, x + 25.7f * u, y + 24 * u};
    gfx_polygon(s, beam, 4, RGB(255, 255, 255));
    gfx_round_rect(s, x + 19 * u, y + 40 * u, 10 * u, 8 * u, 4 * u, RGB(255, 255, 255));
    gfx_round_rect(s, x + 35 * u, y + 36 * u, 10 * u, 8 * u, 4 * u, RGB(255, 255, 255));
}

void icon_minas(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    tile(s, x, y, sz, RGB(120, 200, 110), RGB(50, 140, 60));
    for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++)
        gfx_round_rect(s, x + (13 + i * 13) * u, y + (13 + j * 13) * u, 11 * u, 11 * u, 2 * u, (i + j) % 2 ? RGB(170, 220, 140) : RGB(150, 205, 120));
    gfx_circle(s, x + 32 * u, y + 32 * u, 6 * u, RGB(30, 30, 30));
    for (int k = 0; k < 4; k++) {
        float a = k * PI_F / 4;
        gfx_line(s, x + 32 * u - fcos(a) * 9 * u, y + 32 * u - fsin(a) * 9 * u, x + 32 * u + fcos(a) * 9 * u, y + 32 * u + fsin(a) * 9 * u, 1.6f * u, RGB(30, 30, 30));
    }
}

void icon_visualizador(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    gfx_shadow(s, (int)(x + 8 * u), (int)(y + 10 * u), (int)(48 * u), (int)(40 * u), 3 * u, 3 * u, 70, (int)u);
    gfx_round_rect(s, x + 8 * u, y + 10 * u, 48 * u, 40 * u, 3 * u, RGB(250, 250, 250));
    gfx_grad_v(s, (int)(x + 11 * u), (int)(y + 13 * u), (int)(42 * u), (int)(34 * u), RGB(90, 160, 240), RGB(200, 230, 255));
    float m1[] = {x + 11 * u, y + 47 * u, x + 26 * u, y + 26 * u, x + 38 * u, y + 47 * u};
    float m2[] = {x + 28 * u, y + 47 * u, x + 41 * u, y + 32 * u, x + 53 * u, y + 47 * u};
    gfx_polygon(s, m1, 3, RGB(60, 140, 80));
    gfx_polygon(s, m2, 3, RGB(40, 110, 60));
    gfx_circle(s, x + 44 * u, y + 21 * u, 4 * u, RGB(255, 220, 90));
}

/* ------------------------------------------------------------ cache por tamanho */
typedef struct { app_t *a; int size; bool dark; surface_t *s; } icache_t;
static icache_t icache[256];
static int nicache;

/* Variante escura (como os ícones do sistema no modo escuro): se a base do ícone é clara e neutra
 * (papel, vidro branco), os tons claros neutros viram cinza-escuro; cores e traços são mantidos. */
static void icon_make_dark(surface_t *s) {
    int n = s->w * s->h, opaque = 0, lightn = 0;
    for (int i = 0; i < n; i++) {
        u32 p = s->px[i], a = p >> 24;
        if (a < 200) continue;
        opaque++;
        int r = C_R(p), g = C_G(p), b = C_B(p);
        int mx = MAX(r, MAX(g, b)), mn = MIN(r, MIN(g, b));
        if (mn > 200 && mx - mn < 36) lightn++;
    }
    if (!opaque || lightn * 100 / opaque < 30) return; /* ícone de base colorida: não muda */
    /* marca os pixels saturados; claros vizinhos deles (texto sobre faixa colorida) são preservados */
    u8 *sat = kcalloc((usize)n, 1);
    for (int i = 0; i < n; i++) {
        u32 p = s->px[i], a = p >> 24;
        if (a < 128) continue;
        int r = C_R(p) * 255 / a, g = C_G(p) * 255 / a, b = C_B(p) * 255 / a;
        int mx = MAX(r, MAX(g, b)), mn = MIN(r, MIN(g, b));
        if (mx - mn > 90) sat[i] = 1;
    }
    int rad = MAX(2, s->w / 24);
    for (int i = 0; i < n; i++) {
        u32 p = s->px[i], a = p >> 24;
        if (!a) continue;
        int x = i % s->w, y = i / s->w;
        bool near_color = false;
        for (int dy = -rad; dy <= rad && !near_color; dy++)
            for (int dx = -rad; dx <= rad; dx++) {
                int xx = x + dx, yy = y + dy;
                if (xx >= 0 && yy >= 0 && xx < s->w && yy < s->h && sat[yy * s->w + xx]) { near_color = true; break; }
            }
        if (near_color) continue;
        /* despré-multiplica */
        int r = C_R(p) * 255 / a, g = C_G(p) * 255 / a, b = C_B(p) * 255 / a;
        int mx = MAX(r, MAX(g, b)), mn = MIN(r, MIN(g, b));
        if (mx - mn >= 44) continue;
        int L = (r * 77 + g * 150 + b * 29) >> 8;
        int nl;
        if (mx >= 150) nl = CLAMP(268 - L * 86 / 100, 26, 120);   /* base clara: 255 -> ~49, 200 -> ~96 */
        else if (mx < 90) nl = CLAMP(236 - L, 160, 236);           /* texto/traço escuro vira claro */
        else continue;
        int d = nl - L;
        r = CLAMP(r + d, 0, 255); g = CLAMP(g + d, 0, 255); b = CLAMP(b + d, 0, 255);
        s->px[i] = (a << 24) | ((u32)(r * a / 255) << 16) | ((u32)(g * a / 255) << 8) | (u32)(b * a / 255);
    }
    kfree(sat);
}

surface_t *icon_for(app_t *a, int size) {
    size = CLAMP(size, 8, 256);
    bool dark = T.dark;
    for (int i = 0; i < nicache; i++) if (icache[i].a == a && icache[i].size == size && icache[i].dark == dark) return icache[i].s;
    surface_t *s = surface_new(size, size);
    memset(s->px, 0, (usize)size * size * 4);
    /* apps do macOS: o ícone do próprio bundle (.icns), se o mOS conseguir ler */
    const char *bundle = compat_app_bundle(a);
    surface_t *bic = bundle ? macho_bundle_icon(bundle, size) : NULL;
    if (bic) gfx_blend(s, bic, 0, 0, 255);
    else if (tahoe_icon(a->exec, s, 0, 0, size)) {}   /* ícones do sistema no estilo Tahoe (icons_tahoe.c) */
    else if (a->draw_icon) a->draw_icon(s, 0, 0, size);
    /* Padrão do sistema: todo ícone de app é um quadrado arredondado. Ícones de outro formato
     * (redondos, recortados) são colocados, reduzidos, sobre uma placa clara (ou escura). */
    if (a->exec && strcmp(a->exec, "downloads") && size >= 16) {
        /* 15% a partir do canto: dentro de um quadrado arredondado (raio ~22%), fora de um círculo */
        int k = size * 15 / 100;
        u32 c_tl = s->px[(usize)k * s->stride + k] >> 24, c_tr = s->px[(usize)k * s->stride + size - 1 - k] >> 24;
        u32 c_bl = s->px[(usize)(size - 1 - k) * s->stride + k] >> 24, c_br = s->px[(usize)(size - 1 - k) * s->stride + size - 1 - k] >> 24;
        if (c_tl < 60 && c_tr < 60 && c_bl < 60 && c_br < 60) {
            surface_t *t = surface_new(size, size);
            memset(t->px, 0, (usize)size * size * 4);
            float m = size * 0.06f, w = size - 2 * m, r = size * 0.225f;
            if (dark) gfx_round_rect_grad(t, m, m, w, w, r, RGB(64, 64, 68), RGB(44, 44, 48));
            else gfx_round_rect_grad(t, m, m, w, w, r, RGB(255, 255, 255), RGB(236, 236, 240));
            gfx_specular_rim(t, m, m, w, w, r, dark ? 70 : 150);
            int isz = (int)(w * 0.80f), off = (size - isz) / 2;
            gfx_blit_scaled(t, s, off, off, isz, isz, 255);
            surface_free(s);
            s = t;
        }
    }
    if (dark) icon_make_dark(s);
    if (nicache == ARRAY_LEN(icache)) { surface_free(icache[0].s); memmove(icache, icache + 1, sizeof(icache_t) * (nicache - 1)); nicache--; }
    icache[nicache++] = (icache_t){a, size, dark, s};
    return s;
}


/* ------------------------------------------------------------ símbolos de itens de menu */
void glyph_copy(surface_t *s, int x, int y, int sz, u32 c) {
    float t = MAX(1.2f, sz * 0.09f), r = sz * 0.16f;
    /* folha de trás (contorno parcial) e folha da frente com a dobra */
    gfx_round_rect_stroke(s, x + sz * 0.30f, y + sz * 0.06f, sz * 0.58f, sz * 0.66f, r, t, c);
    gfx_round_rect(s, x + sz * 0.10f, y + sz * 0.26f, sz * 0.58f, sz * 0.68f, r, WITH_ALPHA(c, 0));
    gfx_round_rect_stroke(s, x + sz * 0.10f, y + sz * 0.28f, sz * 0.58f, sz * 0.66f, r, t, c);
    gfx_line(s, x + sz * 0.22f, y + sz * 0.52f, x + sz * 0.56f, y + sz * 0.52f, t * 0.8f, c);
    gfx_line(s, x + sz * 0.22f, y + sz * 0.70f, x + sz * 0.48f, y + sz * 0.70f, t * 0.8f, c);
}

void glyph_globe(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f, cy = y + sz / 2.0f, r = sz / 2.0f - 0.8f, t = MAX(1.1f, sz * 0.08f);
    gfx_circle_stroke(s, cx, cy, r, t, c);
    gfx_line(s, cx - r, cy, cx + r, cy, t * 0.8f, c);
    gfx_line(s, cx - r * 0.86f, cy - r * 0.5f, cx + r * 0.86f, cy - r * 0.5f, t * 0.7f, c);
    gfx_line(s, cx - r * 0.86f, cy + r * 0.5f, cx + r * 0.86f, cy + r * 0.5f, t * 0.7f, c);
    gfx_round_rect_stroke(s, cx - r * 0.45f, cy - r, r * 0.9f, r * 2, r * 0.45f, t * 0.8f, c);
    gfx_line(s, cx, cy - r, cx, cy + r, t * 0.8f, c);
}

/* ============================================================ ícones de barra lateral (traço, cor de destaque)
 * Desenhados em uma grade de 16 px; t = espessura do traço proporcional ao tamanho. */
#define SBT(sz) MAX(1.25f, (sz) * 0.085f)

void glyph_sb_recents(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f, cy = y + sz / 2.0f, r = sz / 2.0f - 1, t = SBT(sz);
    gfx_circle_stroke(s, cx, cy, r, t, c);
    gfx_line(s, cx, cy - r * 0.6f, cx, cy, t, c);
    gfx_line(s, cx, cy, cx - r * 0.45f, cy, t, c);
}

void glyph_sb_shared(surface_t *s, int x, int y, int sz, u32 c) {
    float t = SBT(sz);
    /* pasta em contorno */
    float fx = x + 0.5f, fy = y + sz * 0.22f, fw = sz * 0.86f, fh = sz * 0.66f;
    gfx_round_rect_stroke(s, fx, fy + sz * 0.1f, fw, fh - sz * 0.1f, sz * 0.12f, t, c);
    gfx_line(s, fx + sz * 0.05f, fy + sz * 0.1f, fx + sz * 0.05f, fy + sz * 0.02f, t, c);
    gfx_line(s, fx + sz * 0.05f, fy + sz * 0.02f, fx + sz * 0.34f, fy + sz * 0.02f, t, c);
    gfx_line(s, fx + sz * 0.34f, fy + sz * 0.02f, fx + sz * 0.42f, fy + sz * 0.1f, t, c);
    gfx_line(s, fx + sz * 0.12f, fy + sz * 0.28f, fx + sz * 0.5f, fy + sz * 0.28f, t, c);
    /* selo de pessoa (preenchido) no canto */
    float bx = x + sz * 0.82f, by = y + sz * 0.2f, br = sz * 0.2f;
    gfx_circle(s, bx, by, br, c);
    gfx_circle(s, bx, by - br * 0.25f, br * 0.32f, RGB(255, 255, 255));
    gfx_round_rect(s, bx - br * 0.5f, by + br * 0.15f, br, br * 0.5f, br * 0.25f, RGB(255, 255, 255));
}

void glyph_sb_desktop(surface_t *s, int x, int y, int sz, u32 c) {
    float t = SBT(sz);
    gfx_round_rect_stroke(s, x + 0.5f, y + sz * 0.16f, sz - 1, sz * 0.68f, sz * 0.13f, t, c);
    gfx_round_rect(s, x + sz * 0.16f, y + sz * 0.6f, sz * 0.68f, sz * 0.12f, sz * 0.04f, c);
}

void glyph_sb_document(surface_t *s, int x, int y, int sz, u32 c) {
    float t = SBT(sz), l = x + sz * 0.18f, r = x + sz * 0.82f, tp = y + 0.5f, b = y + sz - 0.5f, f = sz * 0.28f;
    gfx_line(s, l, tp + 1, l, b - 1, t, c);
    gfx_line(s, l, b - 0.5f, r, b - 0.5f, t, c);
    gfx_line(s, r, b - 1, r, tp + f, t, c);
    gfx_line(s, l, tp + 0.5f, r - f, tp + 0.5f, t, c);
    gfx_line(s, r - f, tp + 0.5f, r, tp + f, t, c);
    gfx_line(s, r - f, tp + 0.5f, r - f, tp + f, t * 0.9f, c);
    gfx_line(s, r - f, tp + f, r, tp + f, t * 0.9f, c);
}

void glyph_sb_download(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f, cy = y + sz / 2.0f, r = sz / 2.0f - 1, t = SBT(sz);
    gfx_circle_stroke(s, cx, cy, r, t, c);
    gfx_line(s, cx, cy - r * 0.55f, cx, cy + r * 0.5f, t, c);
    gfx_line(s, cx - r * 0.4f, cy + r * 0.12f, cx, cy + r * 0.52f, t, c);
    gfx_line(s, cx + r * 0.4f, cy + r * 0.12f, cx, cy + r * 0.52f, t, c);
}

void glyph_sb_cloud(surface_t *s, int x, int y, int sz, u32 c) {
    float t = SBT(sz), by = y + sz * 0.78f;
    /* três arcos formando a nuvem + base */
    gfx_ring_arc(s, x + sz * 0.28f, y + sz * 0.6f, sz * 0.2f, t, 3.4f, 6.3f, c);
    gfx_ring_arc(s, x + sz * 0.52f, y + sz * 0.46f, sz * 0.26f, t, 4.9f, 6.3f, c);
    gfx_ring_arc(s, x + sz * 0.52f, y + sz * 0.46f, sz * 0.26f, t, 0.0f, 1.25f, c);
    gfx_ring_arc(s, x + sz * 0.78f, y + sz * 0.62f, sz * 0.17f, t, 0.0f, 3.2f, c);
    gfx_line(s, x + sz * 0.26f, by, x + sz * 0.8f, by, t, c);
}

void glyph_sb_home(surface_t *s, int x, int y, int sz, u32 c) {
    float t = SBT(sz), cx = x + sz / 2.0f;
    gfx_line(s, x + sz * 0.06f, y + sz * 0.5f, cx, y + sz * 0.1f, t, c);
    gfx_line(s, cx, y + sz * 0.1f, x + sz * 0.94f, y + sz * 0.5f, t, c);
    gfx_line(s, x + sz * 0.2f, y + sz * 0.4f, x + sz * 0.2f, y + sz * 0.92f, t, c);
    gfx_line(s, x + sz * 0.8f, y + sz * 0.4f, x + sz * 0.8f, y + sz * 0.92f, t, c);
    gfx_line(s, x + sz * 0.2f, y + sz * 0.92f, x + sz * 0.8f, y + sz * 0.92f, t, c);
    gfx_round_rect(s, cx - sz * 0.1f, y + sz * 0.62f, sz * 0.2f, sz * 0.3f, 1, c);
}

void glyph_sb_trash(surface_t *s, int x, int y, int sz, u32 c) {
    float t = SBT(sz);
    gfx_line(s, x + sz * 0.1f, y + sz * 0.2f, x + sz * 0.9f, y + sz * 0.2f, t, c);
    gfx_round_rect_stroke(s, x + sz * 0.36f, y + 0.5f, sz * 0.28f, sz * 0.24f, 1.5f, t, c);
    gfx_line(s, x + sz * 0.2f, y + sz * 0.22f, x + sz * 0.26f, y + sz - 1, t, c);
    gfx_line(s, x + sz * 0.8f, y + sz * 0.22f, x + sz * 0.74f, y + sz - 1, t, c);
    gfx_line(s, x + sz * 0.26f, y + sz - 1, x + sz * 0.74f, y + sz - 1, t, c);
    for (int i = 0; i < 3; i++) {
        float lx = x + sz * (0.38f + i * 0.12f);
        gfx_line(s, lx, y + sz * 0.36f, lx, y + sz * 0.84f, t * 0.8f, c);
    }
}

void glyph_sb_photo(surface_t *s, int x, int y, int sz, u32 c) {
    float t = SBT(sz);
    gfx_round_rect_stroke(s, x + 0.5f, y + sz * 0.14f, sz - 1, sz * 0.72f, sz * 0.12f, t, c);
    float m[] = {x + sz * 0.14f, y + sz * 0.76f, x + sz * 0.42f, y + sz * 0.44f, x + sz * 0.6f, y + sz * 0.62f,
                 x + sz * 0.7f, y + sz * 0.52f, x + sz * 0.88f, y + sz * 0.76f};
    gfx_polygon(s, m, 5, c);
    gfx_circle(s, x + sz * 0.72f, y + sz * 0.33f, sz * 0.08f, c);
}

void glyph_sb_music(surface_t *s, int x, int y, int sz, u32 c) {
    float t = SBT(sz);
    gfx_line(s, x + sz * 0.38f, y + sz * 0.72f, x + sz * 0.38f, y + sz * 0.16f, t, c);
    gfx_line(s, x + sz * 0.86f, y + sz * 0.62f, x + sz * 0.86f, y + sz * 0.06f, t, c);
    gfx_line(s, x + sz * 0.38f, y + sz * 0.16f, x + sz * 0.86f, y + sz * 0.06f, t * 1.4f, c);
    gfx_circle(s, x + sz * 0.26f, y + sz * 0.76f, sz * 0.14f, c);
    gfx_circle(s, x + sz * 0.74f, y + sz * 0.66f, sz * 0.14f, c);
}

void glyph_sb_apps(surface_t *s, int x, int y, int sz, u32 c) {
    float t = SBT(sz), w = sz * 0.4f, g = sz * 0.12f, o = (sz - 2 * w - g) / 2;
    for (int i = 0; i < 4; i++)
        gfx_round_rect_stroke(s, x + o + (i % 2) * (w + g), y + o + (i / 2) * (w + g), w, w, w * 0.3f, t, c);
}

void glyph_sb_disk(surface_t *s, int x, int y, int sz, u32 c) {
    float t = SBT(sz);
    gfx_round_rect_stroke(s, x + 0.5f, y + sz * 0.26f, sz - 1, sz * 0.48f, sz * 0.12f, t, c);
    gfx_line(s, x + sz * 0.1f, y + sz * 0.5f, x + sz * 0.9f, y + sz * 0.5f, t * 0.8f, c);
    gfx_circle(s, x + sz * 0.76f, y + sz * 0.62f, sz * 0.06f, c);
}

/* botão de mostrar/ocultar a barra lateral: quadrado arredondado tracejado */
void glyph_sidebar_toggle(surface_t *s, int x, int y, int sz, u32 c) {
    float t = MAX(1.4f, sz * 0.1f), r = sz * 0.22f, a = x + 0.5f, b = y + 0.5f, e = x + sz - 0.5f, f = y + sz - 0.5f;
    float L = sz * 0.22f;
    /* cantos */
    gfx_ring_arc(s, a + r, b + r, r, t, 4.712f, 6.283f, c);
    gfx_ring_arc(s, e - r, b + r, r, t, 0.0f, 1.571f, c);
    gfx_ring_arc(s, e - r, f - r, r, t, 1.571f, 3.142f, c);
    gfx_ring_arc(s, a + r, f - r, r, t, 3.142f, 4.712f, c);
    /* traços no meio de cada lado */
    float mx = x + sz / 2.0f, my = y + sz / 2.0f;
    gfx_line(s, mx - L / 2, b + t / 2, mx + L / 2, b + t / 2, t, c);
    gfx_line(s, mx - L / 2, f - t / 2, mx + L / 2, f - t / 2, t, c);
    gfx_line(s, a + t / 2, my - L / 2, a + t / 2, my + L / 2, t, c);
    gfx_line(s, e - t / 2, my - L / 2, e - t / 2, my + L / 2, t, c);
}
