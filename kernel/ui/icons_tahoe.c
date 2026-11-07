/* mOS — ícones dos apps do sistema na linguagem visual do Tahoe (arte original do projeto, nada da Apple):
 * quadrado arredondado com degradê profundo, brilho no topo e borda especular, e símbolos em camadas de
 * vidro translúcido com realce na borda e sombra suave. Desenhados em vetor numa grade de 64 unidades;
 * o corpo ocupa 6..58 (81%, como a grade de ícones do macOS). icon_for() consulta tahoe_icon() primeiro. */
#include "ui.h"

typedef struct { surface_t *s; float x, y, u; } ic_t;
#define X(v) (c->x + (v) * c->u)
#define Y(v) (c->y + (v) * c->u)
#define U(v) ((v) * c->u)
#define W RGB(255, 255, 255)

/* ------------------------------------------------------------ peças */
static void base(ic_t *c, u32 top, u32 bot) {
    surface_t *s = c->s;
    gfx_shadow(s, (int)X(6), (int)Y(7), (int)U(52), (int)U(52), U(11.7f), U(3.2f), 80, (int)U(1.4f));
    gfx_round_rect_grad(s, X(6), Y(6), U(52), U(52), U(11.7f), top, bot);
    /* brilho difuso na metade de cima */
    gfx_round_rect_ex(s, X(6), Y(6), U(52), U(24), U(11.7f), U(11.7f), 0, 0, RGBA(255, 255, 255, 52), RGBA(255, 255, 255, 0));
    gfx_specular_rim(s, X(6), Y(6), U(52), U(52), U(11.7f), 130);
    gfx_round_rect_stroke(s, X(6), Y(6), U(52), U(52), U(11.7f), MAX(1.0f, U(0.4f)), RGBA(0, 0, 0, 36));
}
/* camada de vidro: preenchimento translúcido em degradê, borda clara e reflexo */
static void glass(ic_t *c, float x, float y, float w, float h, float r, int alpha) {
    surface_t *s = c->s;
    gfx_shadow(s, (int)X(x), (int)Y(y + 0.8f), (int)U(w), (int)U(h), U(r), U(1.6f), 40, (int)U(0.8f));
    gfx_round_rect_grad(s, X(x), Y(y), U(w), U(h), U(r), RGBA(255, 255, 255, alpha), RGBA(255, 255, 255, alpha / 2));
    gfx_specular_rim(s, X(x), Y(y), U(w), U(h), U(r), 160);
    gfx_round_rect_stroke(s, X(x), Y(y), U(w), U(h), U(r), MAX(1.0f, U(0.35f)), RGBA(255, 255, 255, 120));
}
/* forma sólida com degradê (círculos usam r = metade do lado) */
static void solid(ic_t *c, float x, float y, float w, float h, float r, u32 top, u32 bot) {
    gfx_shadow(c->s, (int)X(x), (int)Y(y + 0.6f), (int)U(w), (int)U(h), U(r), U(1.2f), 50, (int)U(0.7f));
    gfx_round_rect_grad(c->s, X(x), Y(y), U(w), U(h), U(r), top, bot);
    gfx_specular_rim(c->s, X(x), Y(y), U(w), U(h), U(r), 110);
}
static void disc(ic_t *c, float cx, float cy, float r, u32 top, u32 bot) { solid(c, cx - r, cy - r, 2 * r, 2 * r, r, top, bot); }
static void line(ic_t *c, float x0, float y0, float x1, float y1, float t, u32 col) { gfx_line(c->s, X(x0), Y(y0), X(x1), Y(y1), U(t), col); }
static void poly(ic_t *c, const float *p, int n, u32 col) {
    float q[32];
    for (int i = 0; i < n && i < 16; i++) { q[i * 2] = X(p[i * 2]); q[i * 2 + 1] = Y(p[i * 2 + 1]); }
    gfx_polygon(c->s, q, n, col);
}
static void text(ic_t *c, float x, float y, float w, float h, float size, int weight, u32 col, const char *t) {
    text_draw_center(c->s, font_get(weight), U(size), rect_make((int)X(x), (int)Y(y), (int)U(w), (int)U(h)), col, t);
}

/* ------------------------------------------------------------ ícones */
static void i_arquivos(ic_t *c) {
    base(c, RGB(120, 200, 255), RGB(20, 100, 230));
    glass(c, 14, 20, 16, 8, 2.5f, 110);
    glass(c, 14, 24, 36, 24, 4, 150);
    line(c, 20, 38, 44, 38, 1.4f, RGBA(255, 255, 255, 170));
}
static void i_horizonte(ic_t *c) {
    base(c, RGB(30, 60, 140), RGB(10, 20, 60));
    glass(c, 12, 12, 40, 40, 20, 70);
    rect_t pc = surface_push_clip(c->s, rect_make((int)X(12), (int)Y(12), (int)U(40), (int)U(22)));
    disc(c, 32, 34, 10, RGB(255, 214, 110), RGB(255, 120, 60));
    surface_pop_clip(c->s, pc);
    line(c, 14, 34, 50, 34, 1.4f, RGBA(255, 255, 255, 220));
    for (int i = 0; i < 3; i++) line(c, 18 + i * 2, 39 + i * 4, 46 - i * 2, 39 + i * 4, 1.0f, RGBA(255, 255, 255, 120 - i * 30));
}
static void i_terminal(ic_t *c) {
    base(c, RGB(70, 72, 80), RGB(22, 22, 26));
    glass(c, 11, 13, 42, 38, 6, 40);
    line(c, 18, 24, 25, 30, 2.6f, RGB(140, 255, 170));
    line(c, 25, 30, 18, 36, 2.6f, RGB(140, 255, 170));
    line(c, 29, 37, 40, 37, 2.6f, W);
}
static void i_notas(ic_t *c) {
    base(c, RGB(255, 255, 255), RGB(236, 236, 240));
    gfx_round_rect_ex(c->s, X(6), Y(6), U(52), U(14), U(11.7f), U(11.7f), 0, 0, RGB(255, 222, 90), RGB(255, 196, 40));
    for (int i = 0; i < 4; i++) line(c, 14, 28 + i * 7, i == 3 ? 36 : 50, 28 + i * 7, 1.2f, RGBA(120, 120, 130, 120));
    disc(c, 15, 28, 1.6f, RGB(255, 196, 40), RGB(240, 170, 20));
}
static void i_editor(ic_t *c) {
    base(c, RGB(232, 236, 244), RGB(196, 202, 214));
    glass(c, 15, 10, 30, 42, 3, 220);
    for (int i = 0; i < 6; i++) line(c, 20, 18 + i * 5.5f, i == 5 ? 32 : 40, 18 + i * 5.5f, 1.1f, RGBA(90, 100, 120, 150));
    line(c, 50, 16, 34, 44, 4.2f, RGB(40, 110, 240));
    float tip[] = {32.2f, 43, 35.8f, 45, 31.5f, 49};
    poly(c, tip, 3, RGB(240, 200, 150));
}
static void i_calc(ic_t *c) {
    base(c, RGB(80, 80, 86), RGB(30, 30, 34));
    glass(c, 12, 11, 40, 11, 3, 70);
    text(c, 12, 11, 38, 11, 7, FONT_MEDIUM, W, "42");
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 4; i++) {
            float x = 12 + i * 10.4f, y = 26 + j * 9.4f;
            if (i == 3) disc(c, x + 4, y + 3.8f, 3.8f, RGB(255, 180, 60), RGB(255, 130, 0));
            else glass(c, x, y, 7.6f, 7.6f, 3.8f, 90);
        }
}
static void i_calendario(ic_t *c) {
    static const char *mo[12] = {"JAN", "FEV", "MAR", "ABR", "MAI", "JUN", "JUL", "AGO", "SET", "OUT", "NOV", "DEZ"};
    static const char *wd[7] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SÁB"};
    datetime_t d;
    rtc_now(&d);
    base(c, RGB(255, 255, 255), RGB(238, 238, 242));
    text(c, 6, 10, 52, 9, 6.5f, FONT_SEMIBOLD, RGB(240, 60, 50), wd[d.weekday % 7]);
    char n[4];
    snprintf(n, sizeof n, "%d", d.day);
    text(c, 6, 18, 52, 30, 27, FONT_REGULAR, RGB(30, 30, 34), n);
    text(c, 6, 46, 52, 8, 5, FONT_SEMIBOLD, RGB(150, 150, 158), mo[(d.month + 11) % 12]);
}
static void i_musica(ic_t *c) {
    base(c, RGB(255, 100, 130), RGB(210, 20, 90));
    line(c, 27, 44, 27, 19, 2.8f, W);
    line(c, 43, 40, 43, 15, 2.8f, W);
    float beam[] = {25.6f, 19, 44.4f, 14, 44.4f, 20, 25.6f, 25};
    poly(c, beam, 4, W);
    glass(c, 18, 39, 11, 9, 4.5f, 230);
    glass(c, 34, 35, 11, 9, 4.5f, 230);
}
static void i_minas(ic_t *c) {
    base(c, RGB(120, 210, 120), RGB(30, 130, 70));
    for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) glass(c, 13 + i * 13, 13 + j * 13, 11, 11, 2.6f, (i + j) % 2 ? 80 : 50);
    disc(c, 32, 32, 6, RGB(70, 70, 76), RGB(20, 20, 24));
    for (int k = 0; k < 4; k++) {
        float a = k * PI_F / 4;
        line(c, 32 - fcos(a) * 9, 32 - fsin(a) * 9, 32 + fcos(a) * 9, 32 + fsin(a) * 9, 1.5f, RGB(30, 30, 34));
    }
    disc(c, 30, 30, 1.6f, W, RGBA(255, 255, 255, 120));
}
static void i_visualizador(ic_t *c) {
    base(c, RGB(110, 180, 255), RGB(30, 100, 220));
    glass(c, 18, 12, 32, 26, 3, 80);
    glass(c, 12, 22, 34, 28, 3, 200);
    float m1[] = {14, 47, 25, 33, 35, 47}, m2[] = {26, 47, 35, 37, 44, 47};
    poly(c, m1, 3, RGB(60, 150, 90));
    poly(c, m2, 3, RGB(40, 120, 70));
    disc(c, 38, 29, 3, RGB(255, 230, 110), RGB(255, 190, 60));
}
static void i_atividade(ic_t *c) {
    base(c, RGB(44, 46, 52), RGB(10, 10, 12));
    glass(c, 11, 14, 42, 36, 5, 26);
    for (int i = 0; i < 3; i++) line(c, 13, 23 + i * 9, 51, 23 + i * 9, 0.6f, RGBA(120, 255, 160, 60));
    float p[] = {12, 40, 19, 38, 24, 26, 30, 44, 36, 21, 42, 36, 47, 30, 52, 33};
    for (int i = 0; i < 7; i++) line(c, p[i * 2], p[i * 2 + 1], p[i * 2 + 2], p[i * 2 + 3], 2, RGB(110, 255, 150));
}
static void i_console(ic_t *c) {
    base(c, RGB(236, 236, 240), RGB(190, 192, 200));
    solid(c, 11, 11, 42, 42, 6, RGB(44, 44, 48), RGB(20, 20, 22));
    static const int lens[6] = {30, 22, 34, 18, 28, 12};
    for (int i = 0; i < 6; i++) line(c, 16, 18 + i * 6, 16 + lens[i], 18 + i * 6, 1.8f, i == 2 ? RGB(255, 200, 60) : RGBA(230, 230, 236, 200));
}
static void i_prefs(ic_t *c) {
    base(c, RGB(170, 172, 180), RGB(100, 102, 110));
    glyph_gear(c->s, X(32), Y(33), U(19), RGBA(0, 0, 0, 60), RGB(120, 122, 130));
    glyph_gear(c->s, X(32), Y(32), U(18), RGB(240, 240, 244), RGB(130, 132, 140));
    glass(c, 26, 26, 12, 12, 6, 90);
}
static void i_sobre(ic_t *c) {
    base(c, RGB(90, 110, 150), RGB(30, 40, 70));
    glass(c, 14, 14, 36, 36, 18, 80);
    glyph_mos_logo(c->s, X(32), Y(32), U(13), W, 0);
}
static void i_componentes(ic_t *c) {
    base(c, RGB(160, 110, 255), RGB(80, 40, 200));
    glass(c, 12, 12, 18, 18, 4, 110);
    glass(c, 34, 12, 18, 18, 9, 110);
    glass(c, 12, 34, 40, 8, 4, 110);
    solid(c, 14, 45, 22, 5, 2.5f, W, RGB(220, 220, 230));
}
static void i_instalador(ic_t *c) {
    base(c, RGB(200, 202, 210), RGB(130, 132, 142));
    glass(c, 12, 34, 40, 16, 4, 160);
    disc(c, 44, 42, 2, RGB(90, 230, 120), RGB(40, 170, 80));
    line(c, 32, 12, 32, 30, 3.4f, RGB(40, 110, 240));
    float a[] = {24, 24, 40, 24, 32, 33};
    poly(c, a, 3, RGB(40, 110, 240));
}
static void i_lembretes(ic_t *c) {
    base(c, RGB(255, 255, 255), RGB(236, 236, 240));
    static const u32 col[3][2] = {{RGB(80, 170, 255), RGB(10, 110, 240)}, {RGB(255, 120, 110), RGB(240, 50, 50)}, {RGB(255, 190, 90), RGB(255, 140, 0)}};
    for (int i = 0; i < 3; i++) {
        float y = 20 + i * 12;
        disc(c, 18, y, 4.4f, col[i][0], col[i][1]);
        disc(c, 18, y, 2.2f, W, RGB(240, 240, 244));
        line(c, 26, y, 50, y, 1.6f, RGBA(140, 140, 150, 150));
    }
}
static void i_contatos(ic_t *c) {
    base(c, RGB(200, 166, 120), RGB(140, 100, 60));
    glass(c, 13, 10, 40, 44, 5, 210);
    gfx_fill(c->s, (int)X(13), (int)Y(14), (int)U(4), (int)U(36), RGBA(150, 110, 70, 120));
    disc(c, 35, 26, 6.6f, RGB(170, 174, 184), RGB(120, 124, 136));
    gfx_ring_arc(c->s, X(35), Y(47), U(11), U(5), PI_F, 2 * PI_F, RGB(140, 144, 156));
}
static void i_relogio(ic_t *c) {
    datetime_t d;
    rtc_now(&d);
    base(c, RGB(50, 50, 56), RGB(14, 14, 16));
    disc(c, 32, 32, 21, RGB(255, 255, 255), RGB(226, 226, 232));
    for (int i = 0; i < 12; i++) {
        float a = i * 2 * PI_F / 12, r0 = i % 3 ? 17.5f : 16;
        line(c, 32 + fsin(a) * r0, 32 - fcos(a) * r0, 32 + fsin(a) * 19, 32 - fcos(a) * 19, i % 3 ? 0.9f : 1.6f, RGB(40, 40, 44));
    }
    float ah = ((d.hour % 12) + d.minute / 60.0f) * 2 * PI_F / 12, am = d.minute * 2 * PI_F / 60;
    line(c, 32, 32, 32 + fsin(ah) * 10, 32 - fcos(ah) * 10, 2.4f, RGB(30, 30, 34));
    line(c, 32, 32, 32 + fsin(am) * 15, 32 - fcos(am) * 15, 1.8f, RGB(30, 30, 34));
    line(c, 32, 32, 32 + fsin(am * 7) * 17, 32 - fcos(am * 7) * 17, 0.8f, RGB(255, 140, 0));
    disc(c, 32, 32, 1.8f, RGB(255, 160, 40), RGB(255, 120, 0));
}
static void i_stickies(ic_t *c) {
    base(c, RGB(255, 236, 150), RGB(250, 200, 70));
    glass(c, 20, 10, 32, 32, 3, 110);
    solid(c, 12, 20, 32, 32, 3, RGB(255, 244, 170), RGB(255, 222, 100));
    for (int i = 0; i < 3; i++) line(c, 17, 31 + i * 6, i == 2 ? 30 : 39, 31 + i * 6, 1.2f, RGBA(150, 110, 20, 150));
}
static void i_xadrez(ic_t *c) {
    base(c, RGB(140, 100, 70), RGB(70, 44, 28));
    for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) if ((i + j) % 2) gfx_fill(c->s, (int)X(12 + i * 10), (int)Y(12 + j * 10), (int)U(10), (int)U(10), RGBA(255, 255, 255, 34));
    glass(c, 22, 47, 22, 5, 2.5f, 220);
    float body[] = {25, 47, 27, 33, 23, 30, 25, 20, 32, 13, 37, 16, 41, 24, 37, 28, 39, 47};
    poly(c, body, 9, RGBA(255, 255, 255, 230));
    disc(c, 32, 19, 1.2f, RGB(60, 40, 28), RGB(60, 40, 28));
}
static void i_sysinfo(ic_t *c) {
    base(c, RGB(70, 90, 140), RGB(20, 28, 54));
    for (int i = 0; i < 4; i++) {
        float o = 18 + i * 8;
        line(c, o, 9, o, 15, 1.6f, RGB(230, 200, 120));
        line(c, o, 49, o, 55, 1.6f, RGB(230, 200, 120));
        line(c, 9, o, 15, o, 1.6f, RGB(230, 200, 120));
        line(c, 49, o, 55, o, 1.6f, RGB(230, 200, 120));
    }
    glass(c, 15, 15, 34, 34, 5, 110);
    text(c, 15, 15, 34, 34, 15, FONT_BOLD, W, "i");
}
static void i_disco(ic_t *c) {
    base(c, RGB(230, 232, 238), RGB(170, 172, 182));
    solid(c, 11, 22, 42, 22, 5, RGB(220, 222, 228), RGB(150, 152, 162));
    line(c, 15, 38, 49, 38, 0.8f, RGBA(80, 80, 90, 120));
    disc(c, 45, 30, 2, RGB(100, 240, 130), RGB(40, 170, 80));
    gfx_ring_arc(c->s, X(28), Y(22), U(9), U(2.2f), PI_F, 2 * PI_F, RGB(60, 60, 70));
    disc(c, 37, 15, 3, RGB(150, 150, 160), RGB(100, 100, 110));
}
static void i_captura(ic_t *c) {
    base(c, RGB(236, 236, 242), RGB(186, 188, 198));
    u32 k = RGB(40, 40, 46);
    float t = 2;
    line(c, 11, 12, 19, 12, t, k); line(c, 11, 12, 11, 20, t, k); line(c, 53, 12, 45, 12, t, k); line(c, 53, 12, 53, 20, t, k);
    line(c, 11, 52, 19, 52, t, k); line(c, 11, 52, 11, 44, t, k); line(c, 53, 52, 45, 52, t, k); line(c, 53, 52, 53, 44, t, k);
    solid(c, 16, 24, 32, 21, 5, RGB(80, 80, 88), RGB(40, 40, 46));
    solid(c, 25, 20, 14, 6, 2, RGB(80, 80, 88), RGB(40, 40, 46));
    disc(c, 32, 34.5f, 7, RGB(230, 230, 236), RGB(180, 180, 190));
    disc(c, 32, 34.5f, 4.5f, RGB(90, 150, 240), RGB(30, 80, 190));
}
static void i_medidor(ic_t *c) {
    base(c, RGB(60, 60, 66), RGB(20, 20, 24));
    gfx_circle(c->s, X(25), Y(26), U(11), RGBA(255, 70, 60, 190));
    gfx_circle(c->s, X(39), Y(26), U(11), RGBA(60, 210, 100, 190));
    gfx_circle(c->s, X(32), Y(38), U(11), RGBA(40, 130, 255, 190));
    gfx_specular_rim(c->s, X(14), Y(15), U(36), U(34), U(17), 90);
    gfx_circle_stroke(c->s, X(32), Y(31), U(5), U(1.2f), W);
    line(c, 32, 22, 32, 26, 1, W); line(c, 32, 36, 32, 40, 1, W); line(c, 23, 31, 27, 31, 1, W); line(c, 37, 31, 41, 31, 1, W);
}
static void i_fontes(ic_t *c) {
    base(c, RGB(255, 255, 255), RGB(232, 232, 238));
    text(c, 6, 10, 52, 40, 27, FONT_BOLD, RGB(30, 30, 36), "Aa");
    line(c, 16, 48, 48, 48, 1, RGBA(200, 60, 50, 200));
}
static void i_grapher(ic_t *c) {
    base(c, RGB(30, 44, 90), RGB(8, 12, 32));
    glass(c, 10, 10, 44, 44, 5, 22);
    for (int i = 1; i < 5; i++) { line(c, 10 + i * 8.8f, 11, 10 + i * 8.8f, 53, 0.5f, RGBA(140, 170, 255, 60)); line(c, 11, 10 + i * 8.8f, 53, 10 + i * 8.8f, 0.5f, RGBA(140, 170, 255, 60)); }
    float px = 0, py = 0;
    for (int i = 0; i <= 40; i++) { float t = i / 40.0f, xx = 11 + 42 * t, yy = 32 - 13 * fsin(t * 2 * PI_F * 1.2f); if (i) line(c, px, py, xx, yy, 2, RGB(110, 220, 255)); px = xx; py = yy; }
    for (int i = 0; i <= 40; i++) { float t = i / 40.0f, xx = 11 + 42 * t, yy = 51 - 38 * t * t; if (i) line(c, px, py, xx, yy, 2, RGB(255, 150, 90)); px = xx; py = yy; }
}
static void i_gravador(ic_t *c) {
    base(c, RGB(44, 44, 48), RGB(12, 12, 14));
    static const float h[] = {6, 12, 20, 30, 16, 26, 12, 20, 8};
    for (int i = 0; i < 9; i++) solid(c, 13.5f + i * 4.4f, 32 - h[i] / 2, 2.6f, h[i], 1.3f, i == 4 ? RGB(255, 90, 80) : W, i == 4 ? RGB(230, 40, 40) : RGB(220, 220, 228));
}
static void i_fotos(ic_t *c) {
    base(c, RGB(255, 180, 90), RGB(230, 70, 120));
    glass(c, 18, 12, 32, 30, 4, 70);
    glass(c, 12, 20, 36, 32, 4, 210);
    float m1[] = {15, 49, 26, 34, 37, 49}, m2[] = {28, 49, 37, 39, 46, 49};
    poly(c, m1, 3, RGB(240, 110, 90));
    poly(c, m2, 3, RGB(220, 70, 110));
    disc(c, 39, 29, 3.4f, RGB(255, 220, 110), RGB(255, 170, 60));
}
static void i_diario(ic_t *c) {
    base(c, RGB(255, 140, 170), RGB(140, 80, 230));
    glass(c, 16, 11, 32, 42, 4, 200);
    gfx_fill(c->s, (int)X(16), (int)Y(14), (int)U(4), (int)U(36), RGBA(140, 80, 200, 110));
    for (int i = 0; i < 4; i++) line(c, 25, 21 + i * 6, i == 3 ? 34 : 42, 21 + i * 6, 1.3f, RGBA(140, 100, 200, 170));
    float bm[] = {38, 11, 44, 11, 44, 22, 41, 19, 38, 22};
    poly(c, bm, 5, RGB(255, 90, 110));
}
static void i_dicas(ic_t *c) {
    base(c, RGB(255, 220, 60), RGB(255, 150, 0));
    glass(c, 20, 12, 24, 24, 12, 190);
    solid(c, 26, 37, 12, 9, 2, W, RGB(230, 230, 236));
    line(c, 27, 41.5f, 37, 41.5f, 0.9f, RGB(255, 170, 0));
    line(c, 32, 30, 32, 22, 1.4f, RGB(255, 170, 0));
}
static void i_freeform(ic_t *c) {
    base(c, RGB(255, 255, 255), RGB(236, 236, 240));
    solid(c, 13, 13, 18, 18, 2, RGB(255, 226, 110), RGB(255, 196, 50));
    disc(c, 42, 23, 9, RGB(90, 170, 255), RGB(20, 110, 240));
    float p[] = {14, 48, 21, 41, 28, 47, 36, 39, 44, 45, 50, 40};
    for (int i = 0; i < 5; i++) line(c, p[i * 2], p[i * 2 + 1], p[i * 2 + 2], p[i * 2 + 3], 2.4f, RGB(255, 80, 70));
}
static void i_tempo(ic_t *c) {
    base(c, RGB(90, 170, 255), RGB(30, 100, 230));
    disc(c, 24, 24, 9, RGB(255, 236, 120), RGB(255, 180, 40));
    glass(c, 16, 30, 34, 16, 8, 200);
    glass(c, 26, 23, 16, 16, 8, 200);
}
static void i_mapas(ic_t *c) {
    base(c, RGB(190, 230, 170), RGB(120, 190, 120));
    gfx_fill(c->s, (int)X(38), (int)Y(36), (int)U(20), (int)U(22), RGBA(120, 190, 255, 160));
    line(c, 6, 46, 58, 22, 5, W);
    line(c, 6, 46, 58, 22, 3, RGB(255, 200, 60));
    line(c, 24, 6, 36, 58, 3.4f, RGBA(255, 255, 255, 230));
    disc(c, 23, 23, 7, RGB(255, 100, 90), RGB(230, 40, 40));
    float t[] = {18, 26, 28, 26, 23, 34};
    poly(c, t, 3, RGB(230, 40, 40));
    disc(c, 23, 23, 2.6f, W, W);
}
static void i_bolsa(ic_t *c) {
    base(c, RGB(44, 44, 50), RGB(8, 8, 10));
    float p[] = {11, 44, 19, 36, 27, 40, 37, 24, 45, 29, 53, 15};
    for (int i = 0; i < 5; i++) line(c, p[i * 2], p[i * 2 + 1], p[i * 2 + 2], p[i * 2 + 3], 2.8f, RGB(60, 220, 110));
    for (int i = 0; i < 6; i++) disc(c, p[i * 2], p[i * 2 + 1], 1.6f, RGB(120, 255, 160), RGB(60, 220, 110));
    glass(c, 11, 48, 42, 4, 2, 50);
}
static void i_script(ic_t *c) {
    base(c, RGB(110, 110, 120), RGB(40, 40, 46));
    glass(c, 14, 10, 30, 44, 3, 190);
    for (int i = 0; i < 5; i++) line(c, 19, 18 + i * 6, i % 2 ? 33 : 39, 18 + i * 6, 1.2f, RGBA(80, 80, 92, 170));
    disc(c, 43, 45, 9, RGB(255, 210, 60), RGB(240, 160, 0));
    text(c, 34, 36, 18, 18, 7.5f, FONT_BOLD, RGB(40, 30, 10), "JS");
}
static void i_senhas(ic_t *c) {
    base(c, RGB(100, 220, 140), RGB(30, 150, 80));
    glass(c, 15, 15, 20, 20, 10, 190);
    disc(c, 25, 25, 4, RGB(40, 150, 80), RGB(30, 120, 60));
    glass(c, 30, 30, 20, 6, 3, 190);
    gfx_line(c->s, X(31), Y(31), X(50), Y(50), U(5), RGBA(255, 255, 255, 230));
    line(c, 43, 43, 47, 39, 3.6f, RGBA(255, 255, 255, 230));
}
static void i_dicionario(ic_t *c) {
    base(c, RGB(210, 70, 60), RGB(130, 20, 20));
    glass(c, 13, 10, 38, 44, 4, 220);
    text(c, 13, 12, 38, 40, 20, FONT_BOLD, RGB(120, 30, 30), "Aa");
}
static void i_chaves(ic_t *c) {
    base(c, RGB(200, 202, 210), RGB(120, 122, 132));
    gfx_ring_arc(c->s, X(32), Y(28), U(9.5f), U(3.6f), PI_F, 2 * PI_F, RGB(230, 230, 236));
    line(c, 22.5f, 28, 22.5f, 31, 3.6f, RGB(230, 230, 236));
    line(c, 41.5f, 28, 41.5f, 31, 3.6f, RGB(230, 230, 236));
    solid(c, 17, 30, 30, 22, 4, RGB(255, 214, 100), RGB(230, 160, 30));
    disc(c, 32, 39, 3, RGB(130, 90, 20), RGB(100, 70, 10));
    line(c, 32, 40, 32, 46, 2, RGB(110, 80, 15));
}
static void i_audiomidi(ic_t *c) {
    base(c, RGB(226, 228, 236), RGB(170, 172, 184));
    glass(c, 11, 30, 42, 22, 3, 230);
    for (int i = 1; i < 7; i++) line(c, 11 + i * 6, 30, 11 + i * 6, 52, 0.6f, RGBA(120, 120, 130, 160));
    for (int i = 0; i < 6; i++) if (i != 2) solid(c, 14.8f + i * 6, 30, 3.2f, 12, 1, RGB(60, 60, 66), RGB(20, 20, 24));
    for (int i = 0; i < 3; i++) disc(c, 20 + i * 12, 20, 4.2f, RGB(90, 90, 100), RGB(50, 50, 58));
}
static void i_paciencia(ic_t *c) {
    base(c, RGB(50, 160, 100), RGB(14, 90, 50));
    glass(c, 13, 13, 22, 30, 3, 210);
    solid(c, 27, 20, 23, 31, 3, W, RGB(236, 236, 240));
    /* copas */
    gfx_circle(c->s, X(35.5f), Y(33), U(3), RGB(225, 35, 45));
    gfx_circle(c->s, X(41.5f), Y(33), U(3), RGB(225, 35, 45));
    float h[] = {32.6f, 34, 44.4f, 34, 38.5f, 41};
    poly(c, h, 3, RGB(225, 35, 45));
    text(c, 15, 14, 8, 8, 6, FONT_BOLD, RGB(30, 30, 36), "A");
}
static void i_jogos(ic_t *c) {
    base(c, RGB(255, 100, 140), RGB(110, 70, 240));
    glass(c, 11, 22, 42, 22, 11, 210);
    line(c, 21, 33, 29, 33, 2.4f, RGB(90, 80, 130));
    line(c, 25, 29, 25, 37, 2.4f, RGB(90, 80, 130));
    disc(c, 40, 30, 2.2f, RGB(255, 100, 120), RGB(230, 50, 80));
    disc(c, 45, 35, 2.2f, RGB(100, 150, 255), RGB(50, 100, 230));
}
static void i_atalhos(ic_t *c) {
    base(c, RGB(60, 60, 110), RGB(16, 16, 40));
    line(c, 20, 22, 32, 32, 2.2f, RGBA(255, 255, 255, 170));
    line(c, 32, 32, 44, 42, 2.2f, RGBA(255, 255, 255, 170));
    disc(c, 20, 22, 7, RGB(255, 120, 170), RGB(240, 60, 120));
    disc(c, 32, 32, 7, RGB(180, 120, 255), RGB(130, 70, 240));
    disc(c, 44, 42, 7, RGB(110, 180, 255), RGB(40, 120, 250));
}
static void i_mail(ic_t *c) {
    base(c, RGB(100, 190, 255), RGB(20, 110, 240));
    glass(c, 11, 18, 42, 28, 4, 230);
    line(c, 13, 21, 32, 35, 2, RGB(40, 130, 240));
    line(c, 51, 21, 32, 35, 2, RGB(40, 130, 240));
}
static void i_livros(ic_t *c) {
    base(c, RGB(255, 170, 70), RGB(240, 110, 20));
    float l[] = {11, 19, 31, 23, 31, 48, 11, 44}, r[] = {33, 23, 53, 19, 53, 44, 33, 48};
    poly(c, l, 4, RGBA(255, 255, 255, 235));
    poly(c, r, 4, RGBA(255, 255, 255, 210));
    gfx_specular_rim(c->s, X(11), Y(19), U(42), U(29), U(2), 100);
}
static void i_quicktime(ic_t *c) {
    base(c, RGB(66, 68, 78), RGB(18, 18, 22));
    gfx_ring_arc(c->s, X(31), Y(31), U(15), U(5), 0, 2 * PI_F, RGBA(120, 180, 255, 230));
    line(c, 42, 42, 51, 51, 5.5f, RGBA(120, 180, 255, 230));
    float t[] = {27, 24, 39, 31, 27, 38};
    poly(c, t, 3, W);
}
static void i_automator(ic_t *c) {
    base(c, RGB(180, 184, 196), RGB(100, 104, 118));
    glyph_gear(c->s, X(27), Y(28), U(13), RGB(240, 240, 244), RGB(150, 154, 166));
    glass(c, 30, 32, 22, 18, 5, 200);
    float a[] = {35, 38, 45, 38, 45, 34, 50, 41, 45, 48, 45, 44, 35, 44};
    poly(c, a, 7, RGB(40, 110, 240));
}
static void i_imagecapture(ic_t *c) {
    base(c, RGB(232, 232, 238), RGB(176, 178, 190));
    glass(c, 30, 12, 22, 16, 3, 160);
    solid(c, 12, 24, 34, 24, 5, RGB(80, 80, 88), RGB(36, 36, 42));
    solid(c, 21, 19, 14, 6, 2, RGB(80, 80, 88), RGB(36, 36, 42));
    disc(c, 29, 36, 8, RGB(230, 230, 236), RGB(170, 170, 180));
    disc(c, 29, 36, 5, RGB(90, 150, 240), RGB(30, 80, 190));
    line(c, 48, 20, 48, 46, 2, RGB(40, 110, 240));
    float t[] = {43, 41, 53, 41, 48, 48};
    poly(c, t, 3, RGB(40, 110, 240));
}

/* ------------------------------------------------------------ tabela */
typedef void (*tahoe_fn)(ic_t *);
static const struct { const char *exec; tahoe_fn fn; } table[] = {
    {"arquivos", i_arquivos}, {"horizonte", i_horizonte}, {"terminal", i_terminal}, {"notas", i_notas}, {"editor", i_editor},
    {"calculadora", i_calc}, {"calendario", i_calendario}, {"musica", i_musica}, {"minas", i_minas}, {"visualizador", i_visualizador},
    {"atividade", i_atividade}, {"console", i_console}, {"preferencias", i_prefs}, {"sobre", i_sobre}, {"componentes", i_componentes},
    {"instalador", i_instalador}, {"lembretes", i_lembretes}, {"contatos", i_contatos}, {"relogio", i_relogio}, {"stickies", i_stickies},
    {"xadrez", i_xadrez}, {"sysinfo", i_sysinfo}, {"disco", i_disco}, {"captura", i_captura}, {"medidor", i_medidor},
    {"fontes", i_fontes}, {"grapher", i_grapher}, {"gravador", i_gravador}, {"fotos", i_fotos}, {"diario", i_diario},
    {"dicas", i_dicas}, {"freeform", i_freeform}, {"tempo", i_tempo}, {"mapas", i_mapas}, {"bolsa", i_bolsa},
    {"script", i_script}, {"senhas", i_senhas}, {"dicionario", i_dicionario}, {"chaves", i_chaves}, {"audiomidi", i_audiomidi},
    {"paciencia", i_paciencia}, {"jogos", i_jogos}, {"atalhos", i_atalhos}, {"mail", i_mail}, {"livros", i_livros},
    {"quicktime", i_quicktime}, {"automator", i_automator}, {"imagecapture", i_imagecapture},
};

/* desenha o ícone Tahoe do app; false se o app não tem um (usa o desenho próprio do app) */
bool tahoe_icon(const char *exec, surface_t *s, int x, int y, int size) {
    if (!exec) return false;
    for (int i = 0; i < (int)ARRAY_LEN(table); i++)
        if (!strcmp(table[i].exec, exec)) {
            ic_t c = {s, (float)x, (float)y, size / 64.0f};
            table[i].fn(&c);
            return true;
        }
    return false;
}
