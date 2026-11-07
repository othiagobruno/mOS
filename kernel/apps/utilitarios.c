/* mOS — utilitários: Captura de Tela, Medidor de Cor Digital, Livro de Fontes e Grapher */
#include "kit.h"

void desktop_screenshot_ui(int mode);
extern app_t app_captura, app_medidor, app_fontes, app_grapher;

/* ================================================================== Captura de Tela */
static void cap_launch(app_t *a) { a->running = false; desktop_screenshot_ui(3); }
static void cap_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(232, 232, 236), RGB(190, 190, 198));
    /* câmera */
    gfx_round_rect(s, x + 14 * u, y + 24 * u, 36 * u, 24 * u, 5 * u, RGB(60, 60, 66));
    gfx_round_rect(s, x + 24 * u, y + 19 * u, 14 * u, 7 * u, 2 * u, RGB(60, 60, 66));
    gfx_circle(s, x + 32 * u, y + 36 * u, 8 * u, RGB(230, 230, 236));
    gfx_circle(s, x + 32 * u, y + 36 * u, 5.5f * u, RGB(70, 120, 200));
    /* cantos de seleção */
    u32 c = RGB(40, 40, 46);
    float t = 2 * u;
    gfx_line(s, x + 10 * u, y + 12 * u, x + 18 * u, y + 12 * u, t, c); gfx_line(s, x + 10 * u, y + 12 * u, x + 10 * u, y + 20 * u, t, c);
    gfx_line(s, x + 54 * u, y + 12 * u, x + 46 * u, y + 12 * u, t, c); gfx_line(s, x + 54 * u, y + 12 * u, x + 54 * u, y + 20 * u, t, c);
    gfx_line(s, x + 10 * u, y + 54 * u, x + 18 * u, y + 54 * u, t, c); gfx_line(s, x + 10 * u, y + 54 * u, x + 10 * u, y + 46 * u, t, c);
    gfx_line(s, x + 54 * u, y + 54 * u, x + 46 * u, y + 54 * u, t, c); gfx_line(s, x + 54 * u, y + 54 * u, x + 54 * u, y + 46 * u, t, c);
}
app_t app_captura = {.name = "Captura de Tela", .exec = "captura", .draw_icon = cap_icon, .launch = cap_launch};

/* ================================================================== Medidor de Cor Digital */
static struct { window_t *w; int mode, zoom; bool locked; int lx, ly; u32 color; } M = {.zoom = 8};

static void hsv(u32 c, int *h, int *s, int *v) {
    int r = C_R(c), g = C_G(c), b = C_B(c), mx = MAX(r, MAX(g, b)), mn = MIN(r, MIN(g, b)), d = mx - mn;
    *v = mx * 100 / 255;
    *s = mx ? d * 100 / mx : 0;
    int hh = 0;
    if (d) {
        if (mx == r) hh = 60 * (g - b) / d;
        else if (mx == g) hh = 120 + 60 * (b - r) / d;
        else hh = 240 + 60 * (r - g) / d;
    }
    *h = (hh + 360) % 360;
}
static void med_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_fill(s, 0, 0, u->cw, u->ch, T.window_bg);
    int mx = wm_mouse_x(), my = wm_mouse_y();
    if (M.locked) { mx = M.lx; my = M.ly; }
    /* lupa */
    int n = 11, cell = 10, gx = 16, gy = 16;
    int half = n / 2;
    for (int j = 0; j < n; j++)
        for (int i = 0; i < n; i++) {
            int sx = mx - half + i, sy = my - half + j;
            u32 c = 0;
            if (g_screen && sx >= 0 && sy >= 0 && sx < g_screen->w && sy < g_screen->h) c = g_screen->px[(usize)sy * g_screen->stride + sx] | 0xFF000000;
            gfx_fill(s, gx + i * cell, gy + j * cell, cell, cell, c);
        }
    gfx_rect(s, gx + half * cell, gy + half * cell, cell, cell, RGB(255, 255, 255));
    gfx_rect(s, gx + half * cell - 1, gy + half * cell - 1, cell + 2, cell + 2, RGB(0, 0, 0));
    gfx_rect(s, gx - 1, gy - 1, n * cell + 2, n * cell + 2, T.border);
    u32 c = 0xFF000000;
    if (g_screen && mx >= 0 && my >= 0 && mx < g_screen->w && my < g_screen->h) c = g_screen->px[(usize)my * g_screen->stride + mx] | 0xFF000000;
    M.color = c;
    /* valores */
    int vx = gx + n * cell + 18;
    static const char *modes[] = {"RGB decimal", "RGB hexadecimal", "RGB porcentagem", "HSV"};
    M.mode = ui_popup(u, UI_ID, rect_make(vx, gy, u->cw - vx - 14, 26), modes, 4, M.mode);
    char l[3][24];
    const char *lab[3] = {"Vermelho", "Verde", "Azul"};
    int r = C_R(c), g = C_G(c), b = C_B(c);
    if (M.mode == 0) { snprintf(l[0], 24, "%d", r); snprintf(l[1], 24, "%d", g); snprintf(l[2], 24, "%d", b); }
    else if (M.mode == 1) { snprintf(l[0], 24, "%02X", r); snprintf(l[1], 24, "%02X", g); snprintf(l[2], 24, "%02X", b); }
    else if (M.mode == 2) { snprintf(l[0], 24, "%d,%d%%", r * 100 / 255, r * 1000 / 255 % 10); snprintf(l[1], 24, "%d,%d%%", g * 100 / 255, g * 1000 / 255 % 10); snprintf(l[2], 24, "%d,%d%%", b * 100 / 255, b * 1000 / 255 % 10); }
    else { int h, sa, v; hsv(c, &h, &sa, &v); snprintf(l[0], 24, "%d°", h); snprintf(l[1], 24, "%d%%", sa); snprintf(l[2], 24, "%d%%", v); lab[0] = "Matiz"; lab[1] = "Saturação"; lab[2] = "Valor"; }
    for (int i = 0; i < 3; i++) {
        text_draw(s, font_get(FONT_REGULAR), 13, vx, gy + 52 + i * 22, T.text2, lab[i]);
        int tw = text_width(font_get(FONT_MEDIUM), 14, l[i]);
        text_draw(s, font_get(FONT_MEDIUM), 14, u->cw - 14 - tw, gy + 52 + i * 22, T.text, l[i]);
    }
    gfx_round_rect(s, vx, gy + 104, u->cw - vx - 14, 14, 4, c);
    char pos[48];
    snprintf(pos, sizeof pos, "Posição: (%d, %d)%s", mx, my, M.locked ? " — travado" : "");
    ui_label(u, 16, gy + n * cell + 12, pos, FONT_REGULAR, 12, T.text2);
    if (ui_button_style(u, UI_ID, rect_make(16, u->ch - 38, 130, 26), "Copiar Cor", BTN_GRAY, true, NULL)) {
        char hx[16];
        snprintf(hx, sizeof hx, "#%02X%02X%02X", r, g, b);
        clipboard_set(hx, -1);
        notify("Medidor de Cor Digital", hx);
    }
    if (ui_button_style(u, UI_ID, rect_make(156, u->ch - 38, u->cw - 172, 26), M.locked ? "Destravar (⌘L)" : "Travar Posição (⌘L)", BTN_GRAY, true, NULL) ||
        ((u->mods & MOD_CMD) && u->key == 'l')) {
        M.locked = !M.locked; M.lx = wm_mouse_x(); M.ly = wm_mouse_y(); u->key_used = true;
    }
    u->repaint = true;
}
static void med_close(window_t *w) { M.w = NULL; }
static void med_launch(app_t *a) {
    if (M.w) { wm_focus(M.w); return; }
    M.w = wm_create(&app_medidor, "Medidor de Cor Digital", -1, -1, 360, 230, WIN_NO_ZOOM);
    if (!M.w) return;
    M.w->paint = med_paint;
    M.w->on_close = med_close;
}
static void med_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(70, 70, 76), RGB(30, 30, 34));
    static const u32 c[3] = {RGB(255, 70, 60), RGB(60, 210, 100), RGB(40, 130, 255)};
    for (int i = 0; i < 3; i++) gfx_circle(s, x + (23 + (i % 2) * 18) * u - (i == 2 ? 9 * u : 0), y + (25 + (i == 2) * 15) * u, 12 * u, color_alpha(c[i], 200));
    gfx_circle_stroke(s, x + 32 * u, y + 32 * u, 6 * u, 1.5f * u, RGB(255, 255, 255));
}
app_t app_medidor = {.name = "Medidor de Cor Digital", .exec = "medidor", .draw_icon = med_icon, .launch = med_launch};

/* ================================================================== Livro de Fontes */
static struct { window_t *w; int font, view, scroll; float size; char custom[200]; u32 sel_cp; } F = {.size = 36, .sel_cp = 'A'};
static const char *font_names[5] = {"Inter Regular", "Inter Medium", "Inter Semibold", "Inter Bold", "Fonte de fallback (CJK)"};

static void fb_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w, y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Todas as Fontes", FONT_BOLD, 11.5f, T.text2);
    y += 22;
    int nf = font_has_fallback() ? 5 : 4;
    for (int i = 0; i < nf; i++) {
        if (ui_sidebar_item(u, 102000 + i, rect_make(10, y, sb - 20, 26), font_names[i], F.font == i, NULL)) { F.font = i; F.scroll = 0; }
        y += 27;
    }
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    static const char *views[4] = {"Amostra", "Repertório", "Personalizado", "Informações"};
    F.view = ui_segmented(u, UI_ID, rect_make(u->cw - 12 - 380, TOOLBAR_Y(28), 380, 28), views, 4, F.view);
    font_t *f = F.font < 4 ? font_get(F.font) : font_get(FONT_FALLBACK_WEIGHT);
    rect_t view = rect_make(a.x, a.y, a.w, a.h - (F.view == 0 || F.view == 2 ? 44 : 0));
    rect_t pc = surface_push_clip(s, view);
    int yy = view.y + 20 - F.scroll;
    if (F.view == 0) {
        static const char *lines[] = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz", "0123456789 ÁÉÍÓÚ ÂÊÔ ÃÕ Ç à",
                                      "A rápida raposa marrom pula sobre o cão preguiçoso."};
        for (int i = 0; i < 4; i++) {
            text_wrap(s, f, F.size, rect_make(a.x + 24, yy, a.w - 48, 4000), (int)(F.size * 1.3f), T.text, lines[i], true);
            yy += text_wrap(s, f, F.size, rect_make(a.x + 24, yy, a.w - 48, 4000), (int)(F.size * 1.3f), T.text, lines[i], false) + 12;
        }
    } else if (F.view == 1) {
        int cell = 64, cols = MAX(1, (a.w - 40) / cell), k = 0;
        static const u32 ranges[][2] = {{0x20, 0x7E}, {0xA1, 0x17F}, {0x2010, 0x2044}, {0x20AC, 0x20AC}, {0x2190, 0x21FF}, {0x2200, 0x22FF}, {0x2300, 0x23FF}, {0x25A0, 0x25FF}, {0x2600, 0x26FF}, {0x3040, 0x30FF}};
        for (int ri = 0; ri < (int)ARRAY_LEN(ranges); ri++)
            for (u32 cp = ranges[ri][0]; cp <= ranges[ri][1]; cp++) {
                if (!font_has_glyph(f, cp) && !(F.font == 4 && font_has_fallback())) continue;
                if (F.font < 4 && !font_has_glyph(f, cp)) continue;
                int cx = a.x + 20 + (k % cols) * cell, cy = yy + (k / cols) * cell;
                k++;
                if (cy + cell < view.y || cy > view.y + view.h) continue;
                rect_t r = rect_make(cx, cy, cell - 4, cell - 4);
                bool sel = F.sel_cp == cp;
                gfx_round_rect(s, r.x, r.y, r.w, r.h, 6, sel ? T.accent : (ui_hit(u, r) ? T.alt_row : RGBA(0, 0, 0, 0)));
                gfx_round_rect_stroke(s, r.x, r.y, r.w, r.h, 6, 1, T.separator);
                char g[8];
                g[utf8_encode(cp, g)] = 0;
                text_draw_center(s, f, 28, r, sel ? RGB(255, 255, 255) : T.text, g);
                if (ui_clicked(u, 103000 + (int)(cp & 0xFFFF), r)) F.sel_cp = cp;
            }
        yy += (k + cols - 1) / cols * cell + 20;
        char info[64], g[8];
        g[utf8_encode(F.sel_cp, g)] = 0;
        snprintf(info, sizeof info, "%s  U+%04X", g, F.sel_cp);
        (void)info;
    } else if (F.view == 2) {
        if (!F.custom[0]) snprintf(F.custom, sizeof F.custom, "Digite seu texto aqui");
        yy += text_wrap(s, f, F.size, rect_make(a.x + 24, yy, a.w - 48, 4000), (int)(F.size * 1.3f), T.text, F.custom, true);
    } else {
        i32 upem = 0, asc = 0, desc = 0, ng = 0;
        int wt = F.font < 4 ? F.font : FONT_FALLBACK_WEIGHT;
        font_face_metrics(wt, &upem, &asc, &desc, &ng);
        text_draw(s, font_get(FONT_BOLD), 28, a.x + 24, yy + 30, T.text, font_names[F.font]);
        yy += 56;
        const char *k[] = {"Família", "Estilo", "Tipo", "Unidades por em", "Ascendente", "Descendente", "Glifos", "Licença", "Local"};
        char v[9][96];
        snprintf(v[0], 96, "%s", F.font < 4 ? "Inter" : "Fallback do sistema");
        static const char *st[5] = {"Regular", "Medium", "Semibold", "Bold", "Regular"};
        snprintf(v[1], 96, "%s", st[F.font]);
        snprintf(v[2], 96, "TrueType (glyf), rasterizador próprio com kerning GPOS");
        snprintf(v[3], 96, "%d", upem);
        snprintf(v[4], 96, "%d", asc);
        snprintf(v[5], 96, "%d", desc);
        snprintf(v[6], 96, "%d", ng);
        snprintf(v[7], 96, "%s", F.font < 4 ? "SIL Open Font License 1.1" : "conforme o arquivo instalado");
        snprintf(v[8], 96, "%s", F.font < 4 ? "/Sistema/Biblioteca/Fontes" : "/System/Library/Fonts");
        for (int i = 0; i < 9; i++) {
            int tw = text_width(font_get(FONT_REGULAR), 13, k[i]);
            text_draw(s, font_get(FONT_REGULAR), 13, a.x + 24 + 150 - tw, yy + 16, T.text2, k[i]);
            text_draw(s, font_get(FONT_REGULAR), 13, a.x + 24 + 162, yy + 16, T.text, v[i]);
            yy += 26;
        }
    }
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, view, yy + F.scroll - view.y + 20, &F.scroll);
    if (F.view == 0 || F.view == 2) {
        int by = a.y + a.h - 36;
        gfx_fill(s, a.x, by - 8, a.w, 1, T.separator);
        if (F.view == 2) ui_textfield(u, UI_ID, rect_make(a.x + 16, by, a.w - 260, 26), F.custom, sizeof F.custom, "Texto");
        char sz[16];
        snprintf(sz, sizeof sz, "%d pt", (int)F.size);
        ui_label(u, u->cw - 230, by + 5, sz, FONT_REGULAR, 12, T.text2);
        ui_slider(u, UI_ID, rect_make(u->cw - 180, by + 2, 164, 22), &F.size, 9, 144);
    }
}
static void fb_close(window_t *w) { F.w = NULL; }
static void fb_launch(app_t *a) {
    if (F.w) { wm_focus(F.w); return; }
    F.w = wm_create(&app_fontes, "Livro de Fontes", -1, -1, 900, 580, WIN_RESIZABLE);
    if (!F.w) return;
    F.w->paint = fb_paint;
    F.w->on_close = fb_close;
    F.w->toolbar_h = TOOLBAR_H;
    F.w->sidebar_w = 230;
}
static void fb_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    gfx_shadow(s, (int)(x + 10 * u), (int)(y + 6 * u), (int)(44 * u), (int)(52 * u), 4 * u, 3 * u, 70, (int)u);
    gfx_round_rect_grad(s, x + 10 * u, y + 6 * u, 44 * u, 52 * u, 4 * u, RGB(250, 250, 250), RGB(226, 226, 230));
    gfx_fill(s, (int)(x + 10 * u), (int)(y + 6 * u), (int)(5 * u), (int)(52 * u), RGB(200, 60, 50));
    text_draw_center(s, font_get(FONT_BOLD), 26 * u, rect_make((int)(x + 14 * u), (int)(y + 10 * u), (int)(40 * u), (int)(44 * u)), RGB(40, 40, 44), "Aa");
}
app_t app_fontes = {.name = "Livro de Fontes", .exec = "fontes", .draw_icon = fb_icon, .launch = fb_launch};

/* ================================================================== Grapher */
#define NEQ 6
static struct {
    window_t *w;
    char eq[NEQ][96];
    bool on[NEQ];
    double cx, cy, scale;     /* centro do gráfico e pixels por unidade */
    bool dragging;
    int dx, dy;
    double dcx, dcy;
    bool deg;
} GR = {.on = {true, true, true, true, true, true}, .scale = 40};
static const u32 gcolors[NEQ] = {RGB(10, 132, 255), RGB(255, 69, 58), RGB(48, 209, 88), RGB(255, 159, 10), RGB(191, 90, 242), RGB(100, 210, 255)};

typedef struct { const char *p; double x; bool err; } gp_t;
static double g_expr(gp_t *s);
static void g_skip(gp_t *s) { while (*s->p == ' ') s->p++; }
static bool g_eat(gp_t *s, const char *t) { g_skip(s); usize n = strlen(t); if (!strncmp(s->p, t, n)) { s->p += n; return true; } return false; }
static double g_prim(gp_t *s) {
    g_skip(s);
    if (g_eat(s, "(")) { double v = g_expr(s); g_eat(s, ")"); return v; }
    if (g_eat(s, "-") || g_eat(s, "−")) return -g_prim(s);
    if (g_eat(s, "pi") || g_eat(s, "π")) return KIT_PI;
    static const char *fn[] = {"sqrt", "sin", "cos", "tan", "asin", "acos", "atan", "ln", "log", "abs", "exp", "floor", "sinh", "cosh", "tanh", "√"};
    for (int i = 0; i < (int)ARRAY_LEN(fn); i++) {
        if (i == 1 && !strncmp(s->p, "sinh", 4)) continue;
        if (i == 2 && !strncmp(s->p, "cosh", 4)) continue;
        if (i == 3 && !strncmp(s->p, "tanh", 4)) continue;
        if (!g_eat(s, fn[i])) continue;
        double v = g_prim(s);
        switch (i) {
        case 0: case 15: return v < 0 ? 0.0 / 0.0 : kit_sqrt(v);
        case 1: return kit_sin(v);
        case 2: return kit_cos(v);
        case 3: return kit_tan(v);
        case 4: return kit_fabs(v) > 1 ? 0.0 / 0.0 : kit_asin(v);
        case 5: return kit_fabs(v) > 1 ? 0.0 / 0.0 : kit_acos(v);
        case 6: return kit_atan(v);
        case 7: return v <= 0 ? 0.0 / 0.0 : kit_log(v);
        case 8: return v <= 0 ? 0.0 / 0.0 : kit_log(v) / kit_log(10);
        case 9: return kit_fabs(v);
        case 10: return kit_exp(v);
        case 11: return kit_floor(v);
        case 12: return (kit_exp(v) - kit_exp(-v)) / 2;
        case 13: return (kit_exp(v) + kit_exp(-v)) / 2;
        case 14: { double a = kit_exp(v), b = kit_exp(-v); return (a - b) / (a + b); }
        }
    }
    if (*s->p == 'x') { s->p++; return s->x; }
    if (*s->p == 'e' && !isalpha(s->p[1])) { s->p++; return 2.718281828459045; }
    double v = 0, fr = 0.1;
    int dig = 0;
    while (isdigit(*s->p)) { v = v * 10 + (*s->p++ - '0'); dig++; }
    if (*s->p == '.' || *s->p == ',') { s->p++; while (isdigit(*s->p)) { v += (*s->p++ - '0') * fr; fr /= 10; dig++; } }
    if (!dig) s->err = true;
    return v;
}
static double g_pow(gp_t *s) {
    double b = g_prim(s);
    g_skip(s);
    /* multiplicação implícita: 2x, 3sin(x), x(x+1) */
    while (*s->p == 'x' || *s->p == '(' || (isalpha(*s->p) && *s->p != 'e') || !strncmp(s->p, "π", 2)) b *= g_prim(s);
    if (g_eat(s, "^")) return kit_pow(b, g_pow(s));
    if (g_eat(s, "²")) return b * b;
    return b;
}
static double g_term(gp_t *s) {
    double v = g_pow(s);
    for (;;) {
        if (g_eat(s, "*") || g_eat(s, "×")) v *= g_pow(s);
        else if (g_eat(s, "/") || g_eat(s, "÷")) v /= g_pow(s);
        else break;
    }
    return v;
}
static double g_expr(gp_t *s) {
    double v = g_term(s);
    for (;;) {
        if (g_eat(s, "+")) v += g_term(s);
        else if (g_eat(s, "-") || g_eat(s, "−")) v -= g_term(s);
        else break;
    }
    return v;
}
static const char *g_rhs(const char *eq) {
    const char *e = strchr(eq, '=');
    return e ? e + 1 : eq;
}
static bool g_eval(const char *eq, double x, double *out) {
    gp_t s = {g_rhs(eq), x, false};
    double v = g_expr(&s);
    g_skip(&s);
    if (s.err || *s.p) return false;
    *out = v;
    return v == v && kit_fabs(v) < 1e12;
}

static void nice_step(double range, double *step) {
    double raw = range / 8, mag = 1;
    while (mag * 10 <= raw) mag *= 10;
    while (mag > raw) mag /= 10;
    double n = raw / mag;
    *step = (n < 2 ? 1 : n < 5 ? 2 : 5) * mag;
}

static void gr_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    int sb = w->sidebar_w;
    /* lista de equações na barra lateral */
    int y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, "Equações", FONT_BOLD, 11.5f, T.text2);
    y += 24;
    for (int i = 0; i < NEQ; i++) {
        rect_t box = rect_make(14, y + 6, 14, 14);
        gfx_circle(s, box.x + 7, box.y + 7, 7, GR.on[i] ? gcolors[i] : RGBA(0, 0, 0, 0));
        gfx_circle_stroke(s, box.x + 7, box.y + 7, 7, 1.5f, gcolors[i]);
        if (ui_clicked(u, 104000 + i, box)) GR.on[i] = !GR.on[i];
        ui_textfield(u, 104100 + i, rect_make(34, y, sb - 46, 26), GR.eq[i], 96, "y = f(x)");
        double t;
        if (GR.eq[i][0] && !g_eval(GR.eq[i], 1.2345, &t) && !g_eval(GR.eq[i], 0.5, &t) && !g_eval(GR.eq[i], 2, &t))
            ui_label(u, 36, y + 28, "Expressão inválida", FONT_REGULAR, 10.5f, RGB(255, 69, 58));
        y += 44;
    }
    ui_label(u, 18, y + 4, "Funções: sin cos tan asin acos atan", FONT_REGULAR, 10.5f, T.text3);
    ui_label(u, 18, y + 20, "sqrt ln log abs exp floor, π, e, x^n", FONT_REGULAR, 10.5f, T.text3);
    /* área do gráfico */
    rect_t a = rect_make(sb, w->toolbar_h, u->cw - sb, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 120, TOOLBAR_Y(28), 120, 28), "Enquadrar", true)) { GR.cx = GR.cy = 0; GR.scale = 40; }
    if (ui_button_icon(u, UI_ID, rect_make(u->cw - 12 - 120 - 8 - 32, TOOLBAR_Y(28), 32, 28), glyph_plus, true)) GR.scale *= 1.25;
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 120 - 8 - 32 - 8 - 32, TOOLBAR_Y(28), 32, 28), "−", true)) GR.scale /= 1.25;
    rect_t pc = surface_push_clip(s, a);
    double ox = a.x + a.w / 2.0 - GR.cx * GR.scale, oy = a.y + a.h / 2.0 + GR.cy * GR.scale;
    double xmin = (a.x - ox) / GR.scale, xmax = (a.x + a.w - ox) / GR.scale;
    double ymax = (oy - a.y) / GR.scale, ymin = (oy - a.y - a.h) / GR.scale;
    double step;
    nice_step(xmax - xmin, &step);
    /* grade */
    for (double gx = kit_floor(xmin / step) * step; gx <= xmax; gx += step) {
        int px = (int)(ox + gx * GR.scale);
        gfx_fill(s, px, a.y, 1, a.h, T.dark ? RGBA(255, 255, 255, 16) : RGBA(0, 0, 0, 14));
        if (kit_fabs(gx) > step / 2) {
            char l[24];
            kit_fmt_num(l, sizeof l, gx, 4);
            int ly = CLAMP((int)oy + 14, a.y + 14, a.y + a.h - 4);
            text_draw(s, font_get(FONT_REGULAR), 10.5f, px + 3, ly, T.text2, l);
        }
    }
    for (double gy = kit_floor(ymin / step) * step; gy <= ymax; gy += step) {
        int py = (int)(oy - gy * GR.scale);
        gfx_fill(s, a.x, py, a.w, 1, T.dark ? RGBA(255, 255, 255, 16) : RGBA(0, 0, 0, 14));
        if (kit_fabs(gy) > step / 2) {
            char l[24];
            kit_fmt_num(l, sizeof l, gy, 4);
            int lx = CLAMP((int)ox + 4, a.x + 4, a.x + a.w - 40);
            text_draw(s, font_get(FONT_REGULAR), 10.5f, lx, py - 3, T.text2, l);
        }
    }
    gfx_fill(s, a.x, (int)oy, a.w, 1, T.text2);
    gfx_fill(s, (int)ox, a.y, 1, a.h, T.text2);
    /* curvas */
    for (int i = 0; i < NEQ; i++) {
        if (!GR.on[i] || !GR.eq[i][0]) continue;
        double py0 = 0;
        bool have = false;
        for (int px = a.x; px <= a.x + a.w; px++) {
            double x = (px - ox) / GR.scale, yv;
            if (!g_eval(GR.eq[i], x, &yv)) { have = false; continue; }
            double py = oy - yv * GR.scale;
            if (have && kit_fabs(py - py0) < a.h * 2) gfx_line(s, px - 1, (float)py0, px, (float)py, 2.0f, gcolors[i]);
            py0 = py;
            have = true;
        }
    }
    /* cursor: coordenadas e valores */
    if (ui_hit(u, a) && !GR.dragging) {
        double x = (u->mx - ox) / GR.scale, yy = (oy - u->my) / GR.scale;
        char t[64], xs[24], ys[24];
        kit_fmt_num(xs, 24, x, 3);
        kit_fmt_num(ys, 24, yy, 3);
        snprintf(t, sizeof t, "x = %s   y = %s", xs, ys);
        int tw = text_width(font_get(FONT_MEDIUM), 12, t);
        gfx_round_rect(s, a.x + a.w - tw - 30, a.y + a.h - 34, tw + 18, 24, 8, T.dark ? RGBA(40, 40, 44, 230) : RGBA(255, 255, 255, 235));
        text_draw(s, font_get(FONT_MEDIUM), 12, a.x + a.w - tw - 21, a.y + a.h - 17, T.text, t);
        for (int i = 0; i < NEQ; i++) {
            double yv;
            if (!GR.on[i] || !GR.eq[i][0] || !g_eval(GR.eq[i], x, &yv)) continue;
            gfx_circle(s, (float)u->mx, (float)(oy - yv * GR.scale), 4, gcolors[i]);
        }
        u->cursor = CURSOR_CROSS;
        u->repaint = true;
    }
    surface_pop_clip(s, pc);
    /* arrastar move, roda dá zoom */
    if (u->pressed && ui_hit(u, a)) { GR.dragging = true; GR.dx = u->mx; GR.dy = u->my; GR.dcx = GR.cx; GR.dcy = GR.cy; }
    if (GR.dragging) {
        if (!u->down) GR.dragging = false;
        else { GR.cx = GR.dcx - (u->mx - GR.dx) / GR.scale; GR.cy = GR.dcy + (u->my - GR.dy) / GR.scale; u->cursor = CURSOR_GRABBING; }
    }
    if (u->wheel && ui_hit(u, a)) { GR.scale *= u->wheel > 0 ? 1.15 : 1 / 1.15; GR.scale = CLAMP(GR.scale, 0.01, 100000.0); u->wheel = 0; }
}
static void gr_close(window_t *w) { GR.w = NULL; }
static void gr_launch(app_t *a) {
    if (GR.w) { wm_focus(GR.w); return; }
    if (!GR.eq[0][0]) { strcpy(GR.eq[0], "y = sin(x)"); strcpy(GR.eq[1], "y = x^2/4 - 2"); }
    GR.w = wm_create(&app_grapher, "Grapher", -1, -1, 960, 620, WIN_RESIZABLE);
    if (!GR.w) return;
    GR.w->paint = gr_paint;
    GR.w->on_close = gr_close;
    GR.w->toolbar_h = TOOLBAR_H;
    GR.w->sidebar_w = 260;
}
static void gr_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(30, 40, 70), RGB(12, 16, 30));
    for (int i = 0; i < 5; i++) {
        gfx_fill(s, (int)(x + (12 + i * 10) * u), (int)(y + 10 * u), 1, (int)(44 * u), RGBA(120, 160, 255, 50));
        gfx_fill(s, (int)(x + 10 * u), (int)(y + (12 + i * 10) * u), (int)(44 * u), 1, RGBA(120, 160, 255, 50));
    }
    float px = 0, py = 0;
    for (int i = 0; i <= 40; i++) {
        float t = i / 40.0f, xx = x + (10 + 44 * t) * u, yy = y + (32 - 14 * fsin(t * 2 * PI_F * 1.2f)) * u;
        if (i) gfx_line(s, px, py, xx, yy, 2.2f * u, RGB(120, 220, 255));
        px = xx; py = yy;
    }
    for (int i = 0; i <= 40; i++) {
        float t = i / 40.0f, xx = x + (10 + 44 * t) * u, yy = y + (52 - 40 * t * t) * u;
        if (i) gfx_line(s, px, py, xx, yy, 2.2f * u, RGB(255, 140, 90));
        px = xx; py = yy;
    }
}
app_t app_grapher = {.name = "Grapher", .exec = "grapher", .draw_icon = gr_icon, .launch = gr_launch};
