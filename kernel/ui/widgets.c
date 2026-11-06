/* mOS — widgets do Cupertino (modo imediato) */
#include "ui.h"

extern void popup_open(int sx, int sy, int w, const char **items, int n, int sel, window_t *win, int id);
extern bool popup_take_result(window_t *win, int id, int *val);

#define F_UI font_get(FONT_REGULAR)
#define F_MED font_get(FONT_MEDIUM)

void ui_begin(ui_t *u, surface_t *s) { u->s = s; }

bool ui_hit(ui_t *u, rect_t r) {
    if (!u->inside || !rect_contains(r, u->mx, u->my)) return false;
    /* um popover aberto "engole" o ponteiro: controles embaixo dele não respondem */
    if (!u->in_overlay && !rect_empty(u->modal_rect) && rect_contains(u->modal_rect, u->mx, u->my)) return false;
    return true;
}

/* lógica comum de botão: devolve true no clique (soltar dentro) */
static bool behave(ui_t *u, int id, rect_t r, bool *held) {
    bool hover = ui_hit(u, r);
    if (hover && u->pressed) { u->active = id; u->drag_x = u->mx; u->drag_y = u->my; }
    bool click = false;
    if (u->active == id && u->released) {
        if (hover) { click = true; u->clicked_any = true; }
        u->active = 0;
    }
    if (held) *held = u->active == id && hover && u->down;
    return click;
}

bool ui_clicked(ui_t *u, int id, rect_t r) { return behave(u, id, r, NULL); }

/* ------------------------------------------------------------ cadeia de foco por teclado */
static void focus_register(ui_t *u, int id, rect_t r, bool text) {
    if (u->in_overlay && rect_empty(u->modal_next)) return;
    if (u->nfl < (int)ARRAY_LEN(u->fl_ids)) { u->fl_ids[u->nfl] = id; u->fl_text[u->nfl] = text; u->nfl++; }
    if (!text && u->kfocus == id) u->kfocus_rect = r;
}

/* behave para controles focáveis: registra na cadeia de Tab e aciona com Espaço quando focado */
static bool behave_f(ui_t *u, int id, rect_t r, bool *held) {
    focus_register(u, id, r, false);
    bool c = behave(u, id, r, held);
    if (u->pressed && ui_hit(u, r)) u->kfocus = 0; /* clique com o mouse some com o anel do teclado */
    if (!c && u->kfocus == id && u->key == ' ' && !u->key_used) { c = true; u->key_used = true; u->clicked_any = true; }
    return c;
}

/* chamado pelo window server depois da pintura: Tab/⇧Tab movem o foco; desenha o anel do controle focado */
void ui_focus_navigate(ui_t *u) {
    if (u->key == KEY_TAB && !u->key_used && u->nfl > 0) {
        int cur = -1;
        for (int i = 0; i < u->nfl; i++)
            if ((u->fl_text[i] && u->focus == u->fl_ids[i]) || (!u->fl_text[i] && u->kfocus == u->fl_ids[i])) cur = i;
        int d = (u->mods & MOD_SHIFT) ? -1 : 1;
        int nx = cur < 0 ? (d > 0 ? 0 : u->nfl - 1) : (cur + d + u->nfl) % u->nfl;
        if (u->fl_text[nx]) { u->focus = u->fl_ids[nx]; u->kfocus = 0; u->sel_anchor = 0; u->caret = 9999; u->caret_blink = u->now; }
        else { u->kfocus = u->fl_ids[nx]; u->focus = 0; }
        u->key_used = true;
        u->repaint = true;   /* o anel aparece já no quadro seguinte, mesmo sem outro evento */
    }
}

void ui_focus_draw(ui_t *u) {
    if (!u->kfocus || rect_empty(u->kfocus_rect)) return;
    bool alive = false;
    for (int i = 0; i < u->nfl; i++) if (!u->fl_text[i] && u->fl_ids[i] == u->kfocus) alive = true;
    if (!alive) { u->kfocus = 0; return; }
    rect_t r = u->kfocus_rect;
    ui_focus_ring(u, r, MIN(r.h / 2.0f, MAX(6.0f, r.h * 0.3f)));
}

void ui_focus_ring(ui_t *u, rect_t r, float rad) {
    gfx_round_rect_stroke(u->s, r.x - 3.5f, r.y - 3.5f, r.w + 7, r.h + 7, rad + 3.5f, 3.5f, WITH_ALPHA(T.accent, 120));
}

void ui_label(ui_t *u, int x, int y, const char *t, int weight, float size, u32 color) {
    font_t *f = font_get(weight);
    text_draw(u->s, f, size, x, y + text_ascent(f, size), color, t);
}

void ui_label_center(ui_t *u, rect_t r, const char *t, int weight, float size, u32 color) {
    text_draw_center(u->s, font_get(weight), size, r, color, t);
}

void ui_separator(ui_t *u, int x, int y, int w) { gfx_fill(u->s, x, y, w, 1, T.separator); }

void ui_box(ui_t *u, rect_t r, const char *title) {
    int ty = r.y;
    if (title) {
        ui_label(u, r.x + 2, r.y, title, FONT_REGULAR, FONT_UI, T.text);
        ty += 20;
    }
    u32 bg = T.dark ? RGBA(255, 255, 255, 12) : RGBA(0, 0, 0, 10);
    gfx_round_rect(u->s, r.x, ty, r.w, r.h - (ty - r.y), 5, bg);
    gfx_round_rect_stroke(u->s, r.x, ty, r.w, r.h - (ty - r.y), 5, 1, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 22));
}

/* ------------------------------------------------------------ botões */
/* botão de diálogo: cápsula plana cinza-clara; o principal é a cor de destaque, também plana */
static void draw_push(ui_t *u, rect_t r, bool primary, bool held, bool enabled) {
    float rad = r.h / 2.0f;
    bool hover = enabled && ui_hit(u, r);
    if (primary && enabled) {
        u32 c = T.accent;
        if (hover) c = color_lighten(c, 10);
        if (held) c = color_lighten(T.accent, -24);
        gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, c);
    } else {
        u32 c = T.control_top;
        if (hover) c = T.dark ? RGBA(255, 255, 255, 46) : RGB(220, 220, 225);
        if (held) c = T.dark ? RGBA(255, 255, 255, 64) : RGB(206, 206, 212);
        if (!enabled) c = color_alpha(c, 140);
        gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, c);
    }
}

/* grupo em cápsula da barra de ferramentas: branco, contorno finíssimo e sombra difusa */
void ui_tool_group(ui_t *u, rect_t r) {
    float rad = r.h / 2.0f;
    gfx_shadow(u->s, r.x, r.y, r.w, r.h, rad, 6, T.dark ? 70 : 22, 1);
    gfx_round_rect(u->s, r.x - 0.5f, r.y - 0.5f, r.w + 1, r.h + 1, rad + 0.5f, T.tool_border);
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, T.dark ? RGB(58, 58, 62) : T.tool_bg);
    gfx_specular_rim(u->s, r.x, r.y, r.w, r.h, rad, T.dark ? 50 : 120);
}

/* botão de barra de ferramentas: cápsula própria, ou só um realce quando está dentro de um grupo */
static void draw_tool(ui_t *u, rect_t r, bool held, bool enabled) {
    ui_tool_group(u, r);
    bool hover = enabled && ui_hit(u, r);
    if (held || hover) gfx_round_rect(u->s, r.x + 2, r.y + 2, r.w - 4, r.h - 4, (r.h - 4) / 2.0f, T.dark ? RGBA(255, 255, 255, held ? 40 : 20) : RGBA(0, 0, 0, held ? 30 : 12));
}

void ui_title(ui_t *u, int x, int y, int h, int maxw, const char *title, const char *subtitle) {
    font_t *fb = font_get(FONT_BOLD), *fr = font_get(FONT_REGULAR);
    if (maxw < 20) return;
    if (subtitle && subtitle[0]) {
        text_draw_ellipsis(u->s, fb, 14, x, y + h / 2 - 2, maxw, T.text, title);
        text_draw_ellipsis(u->s, fr, 11, x, y + h / 2 + 12, maxw, T.text2, subtitle);
    } else {
        int base = y + (h - (text_ascent(fb, 14) + text_descent(fb, 14))) / 2 + text_ascent(fb, 14);
        text_draw_ellipsis(u->s, fb, 14, x, base, maxw, T.text, title);
    }
}

bool ui_button(ui_t *u, int id, rect_t r, const char *label, bool primary) {
    return ui_button_style(u, id, r, label, primary ? BTN_FILLED : BTN_GRAY, true, NULL);
}

bool ui_toolbar_button(ui_t *u, int id, rect_t r, const char *label, bool enabled) {
    bool held = false;
    bool c = enabled ? behave_f(u, id, r, &held) : false;
    draw_tool(u, r, held, enabled);
    text_draw_center(u->s, F_UI, FONT_UI, rect_make(r.x, r.y - 1, r.w, r.h), enabled ? T.control_text : T.text3, label);
    return c;
}

bool ui_button_icon(ui_t *u, int id, rect_t r, void (*glyph)(surface_t *, int, int, int, u32), bool enabled) {
    bool held = false;
    bool c = enabled ? behave_f(u, id, r, &held) : false;
    draw_tool(u, r, held, enabled);
    int sz = MIN(r.w, r.h) - 8;
    glyph(u->s, r.x + (r.w - sz) / 2, r.y + (r.h - sz) / 2, sz, enabled ? T.control_text : T.text3);
    return c;
}

/* ------------------------------------------------------------ checkbox / rádio / switch */
/* ------------------------------------------------------------ animações de widgets
 * Cada controle (janela + id) guarda um valor que se aproxima do alvo com amortecimento exponencial;
 * enquanto não chega, pede nova pintura (u->repaint). */
typedef struct { void *win; int id; float v; u64 t; } anim_t;
static anim_t anims[128];
static int nanims;

float ui_anim(ui_t *u, int id, float target, float dur_ms) {
    anim_t *a = NULL;
    for (int i = 0; i < nanims; i++) if (anims[i].win == u->win && anims[i].id == id) { a = &anims[i]; break; }
    if (!a) {
        if (nanims == (int)ARRAY_LEN(anims)) { memmove(anims, anims + 1, sizeof(anim_t) * (nanims - 1)); nanims--; }
        a = &anims[nanims++];
        a->win = u->win; a->id = id; a->v = target; a->t = u->now;
        return target;
    }
    float dt = (float)(u->now - a->t);
    a->t = u->now;
    if (dt > 0) {
        float k = 1 - fexp(-dt / MAX(1.0f, dur_ms / 3.5f));
        a->v += (target - a->v) * k;
    }
    if (fabsf_(target - a->v) < 0.004f) a->v = target;
    else u->repaint = true;
    return a->v;
}

/* pré-visualização (galerias): força o estado visual (0 normal, 1 pressionado, 2 desativado) e o foco da janela */
int ui_force_state = -1, ui_force_key = -1;
static bool win_key(ui_t *u) { if (ui_force_key >= 0) return ui_force_key; return !u->win || wm_focused() == u->win; }
static u32 sys_blue(void) {
    if (g_accent_index != 0) return T.dark ? color_lighten(T.accent, 20) : T.accent;
    return T.dark ? RGB(20, 160, 255) : RGB(0, 136, 255);
}

/* checkbox: quadrado arredondado sem borda; marcado = azul com ✓ branco; misto = traço; desmarcado = cinza.
 * Em janela sem foco, o marcado vira cinza com o símbolo escuro (como no sistema). */
bool ui_checkbox_ex(ui_t *u, int id, int x, int y, const char *label, int *state, bool enabled) {
    int lw = label ? text_width(F_UI, FONT_UI, label) + 8 : 0;
    rect_t hit = rect_make(x, y, 18 + lw, 18);
    bool held = false;
    bool c = enabled ? behave_f(u, id, hit, &held) : false;
    if (c) *state = *state == 1 ? 0 : 1;
    bool d = T.dark, key = win_key(u);
    int st = !enabled ? 2 : held ? 1 : 0;
    if (ui_force_state >= 0) st = ui_force_state;
    if (st == 2) enabled = false;
    float sz = 16, rad = 4.5f, bx = x, by = y + 1;
    u32 bg, fg;
    if (*state && key) {
        u32 b = sys_blue();
        bg = st == 0 ? b : st == 1 ? (d ? color_lighten(b, 22) : color_lighten(b, -26)) : (d ? color_mix(b, RGB(30, 30, 32), 130) : color_mix(b, RGB(255, 255, 255), 110));
        fg = st == 2 ? (d ? RGBA(255, 255, 255, 150) : RGBA(255, 255, 255, 220)) : RGB(255, 255, 255);
    } else if (*state) {
        bg = d ? (st == 0 ? RGB(52, 52, 54) : st == 1 ? RGB(74, 74, 78) : RGB(40, 40, 42)) : (st == 0 ? RGB(226, 226, 228) : st == 1 ? RGB(206, 206, 210) : RGB(243, 243, 244));
        fg = st == 2 ? (d ? RGB(90, 90, 94) : RGB(184, 184, 188)) : (d ? RGB(240, 240, 242) : RGB(34, 34, 36));
    } else {
        bg = d ? (st == 0 ? RGB(52, 52, 54) : st == 1 ? RGB(74, 74, 78) : RGB(40, 40, 42)) : (st == 0 ? RGB(229, 229, 231) : st == 1 ? RGB(208, 208, 212) : RGB(243, 243, 244));
        fg = 0;
    }
    /* ao marcar, a cor e o símbolo entram com um fade curto (~140 ms) sobre a base cinza */
    float ta = ui_force_state >= 0 ? 1.0f : ui_anim(u, id, *state ? 1.0f : 0.0f, 140);
    if (*state && ta < 1.0f) {
        gfx_round_rect(u->s, bx, by, sz, sz, rad, d ? RGB(52, 52, 54) : RGB(229, 229, 231));
        bg = color_alpha(bg, (int)(ta * 255));
        fg = color_alpha(fg, (int)(ta * 255));
    }
    gfx_round_rect(u->s, bx, by, sz, sz, rad, bg);
    if (*state == 1) {
        gfx_line(u->s, bx + 4.0f, by + 8.6f, bx + 6.8f, by + 11.6f, 2.1f, fg);
        gfx_line(u->s, bx + 6.8f, by + 11.6f, bx + 12.2f, by + 4.6f, 2.1f, fg);
    } else if (*state == 2) {
        gfx_round_rect(u->s, bx + 4.5f, by + sz / 2 - 1.3f, sz - 9, 2.6f, 1.3f, fg);
    }
    if (label) ui_label(u, x + 24, y + 1, label, FONT_REGULAR, FONT_UI, enabled ? T.text : T.text3);
    return c;
}

bool ui_checkbox(ui_t *u, int id, int x, int y, const char *label, bool *v) {
    int st = *v ? 1 : 0;
    bool c = ui_checkbox_ex(u, id, x, y, label, &st, true);
    *v = st != 0;
    return c;
}

bool ui_radio(ui_t *u, int id, int x, int y, const char *label, bool on) {
    int lw = label ? text_width(F_UI, FONT_UI, label) + 8 : 0;
    bool held;
    bool c = behave_f(u, id, rect_make(x, y, 18 + lw, 18), &held);
    float cx = x + 8, cy = y + 9;
    if (on) {
        u32 b = win_key(u) ? sys_blue() : (T.dark ? RGB(74, 74, 78) : RGB(150, 150, 154));
        gfx_circle(u->s, cx, cy, 8, held ? color_lighten(b, T.dark ? 22 : -26) : b);
        gfx_circle(u->s, cx, cy, 3, RGB(255, 255, 255));
    } else {
        gfx_circle(u->s, cx, cy, 8, T.dark ? (held ? RGB(74, 74, 78) : RGB(52, 52, 54)) : (held ? RGB(208, 208, 212) : RGB(229, 229, 231)));
    }
    if (label) ui_label(u, x + 24, y + 1, label, FONT_REGULAR, FONT_UI, T.text);
    return c;
}

/* Lente de vidro líquido: o "knob" pressionado vira uma pílula de vidro maior que o trilho,
 * que mostra o que está embaixo desfocado e curvado nas bordas, com contorno e sombra. */
static void liquid_lens(surface_t *s, float x, float y, float w, float h) {
    float rad = h / 2;
    gfx_shadow(s, (int)x, (int)y, (int)w, (int)h, rad, h * 0.35f, T.dark ? 120 : 60, (int)(h * 0.12f) + 1);
    glass_t g = {3, 270, T.dark ? RGBA(255, 255, 255, 22) : RGBA(255, 255, 255, 80), h * 0.16f, h * 0.38f, 230, 26, 0, 1.0f};
    gfx_glass(s, x, y, w, h, rad, &g);
    gfx_round_rect_stroke(s, x - 0.5f, y - 0.5f, w + 1, h + 1, rad + 0.5f, 1, T.dark ? RGBA(0, 0, 0, 160) : RGBA(0, 0, 0, 70));
    gfx_round_rect_stroke(s, x + 1, y + 1, w - 2, h - 2, rad - 1, 1.2f, RGBA(255, 255, 255, T.dark ? 40 : 120));
}

/* knob branco em pílula (estado normal) */
static void white_pill(surface_t *s, float x, float y, float w, float h, bool hairline) {
    if (hairline) gfx_round_rect(s, x - 0.5f, y - 0.5f, w + 1, h + 1, h / 2 + 0.5f, T.dark ? RGBA(0, 0, 0, 90) : RGBA(0, 0, 0, 26));
    gfx_round_rect(s, x, y, w, h, h / 2, RGB(255, 255, 255));
}

/* switch em 5 tamanhos (h = altura do trilho; largura = 2,25 h). Pressionado: o knob vira lente de vidro. */
bool ui_switch_ex(ui_t *u, int id, int x, int y, int h, bool *v, bool enabled) {
    int W = (int)(h * 2.25f + 0.5f);
    rect_t r = rect_make(x, y, W, h);
    bool held = false;
    bool c = enabled ? behave_f(u, id, r, &held) : false;
    if (c) *v = !*v;
    if (ui_force_state >= 0) { held = ui_force_state == 1; enabled = ui_force_state != 2; }
    bool on = *v, d = T.dark;
    /* transição animada: o knob desliza e o trilho muda de cor (~180 ms) */
    float t = ui_force_state >= 0 ? (on ? 1.0f : 0.0f) : ui_anim(u, id, on ? 1.0f : 0.0f, 180);
    u32 b = sys_blue();
    u32 t_on = enabled ? b : (d ? color_mix(b, RGB(30, 30, 32), 110) : color_mix(b, RGB(255, 255, 255), 110));
    u32 t_off = d ? (enabled ? RGB(52, 52, 54) : RGB(40, 40, 42)) : (enabled ? RGB(233, 233, 235) : RGB(243, 243, 244));
    u32 track = color_mix(t_off, t_on, (int)(t * 255));
    float rad = h / 2.0f;
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, track);
    float inset = MAX(2.0f, h * 0.09f), kh = h - 2 * inset, kw = W * 0.6f;
    float kx = x + inset + (W - 2 * inset - kw) * t, ky = y + inset;
    on = t > 0.5f;
    if (held) {
        /* lente maior que o trilho, um pouco deslocada para o centro */
        float lw = kw * 1.32f, lh = h * 1.32f;
        float lcx = kx + kw / 2 + (on ? -kw * 0.1f : kw * 0.1f), lcy = y + h / 2.0f;
        liquid_lens(u->s, lcx - lw / 2, lcy - lh / 2, lw, lh);
    } else {
        white_pill(u->s, kx, ky, kw, kh, !on);
        if (!enabled && !d) gfx_round_rect(u->s, kx, ky, kw, kh, kh / 2, RGBA(255, 255, 255, 90)); /* no escuro o knob segue branco */
    }
    return c;
}

bool ui_switch(ui_t *u, int id, int x, int y, bool *v) { return ui_switch_ex(u, id, x, y, 22, v, true); }

/* knob de slider: pílula branca; ao arrastar, vira lente de vidro */
static void glass_knob(surface_t *s, float cx, float cy, bool held) {
    float kw = 24, kh = 16;
    if (held) {
        float lw = kw * 1.4f, lh = kh * 1.5f;
        liquid_lens(s, cx - lw / 2, cy - lh / 2, lw, lh);
        return;
    }
    gfx_shadow(s, (int)(cx - kw / 2), (int)(cy - kh / 2), (int)kw, (int)kh, kh / 2, 4, T.dark ? 90 : 40, 1);
    white_pill(s, cx - kw / 2, cy - kh / 2, kw, kh, true);
}

/* ------------------------------------------------------------ slider */
bool ui_slider(ui_t *u, int id, rect_t r, float *v, float lo, float hi) {
    /* teclado: com foco (Tab), ← → ajustam 5% da faixa */
    focus_register(u, id, rect_make(r.x, r.y + r.h / 2 - 9, r.w, 18), false);
    if (u->kfocus == id && !u->key_used && (u->key == KEY_LEFT || u->key == KEY_RIGHT)) {
        *v = CLAMP(*v + (u->key == KEY_RIGHT ? 1 : -1) * (hi - lo) * 0.05f, MIN(lo, hi), MAX(lo, hi));
        u->key_used = true;
        u->clicked_any = true;
    }
    bool hover = ui_hit(u, rect_make(r.x - 6, r.y, r.w + 12, r.h));
    if (hover && u->pressed) u->active = id;
    bool changed = false;
    if (u->active == id) {
        if (u->down) {
            float t = (float)(u->mx - r.x - 12) / MAX(1, r.w - 24);
            t = CLAMP(t, 0.0f, 1.0f);
            float nv = lo + t * (hi - lo);
            if (nv != *v) { *v = nv; changed = true; }
        }
        if (u->released) { u->active = 0; u->clicked_any = true; }
    }
    float t = (*v - lo) / (hi - lo);
    int cy = r.y + r.h / 2;
    gfx_round_rect(u->s, r.x, cy - 3, r.w, 6, 3, T.dark ? RGBA(255, 255, 255, 36) : RGBA(0, 0, 0, 28));
    float kx = r.x + 12 + (r.w - 24) * t;
    gfx_round_rect(u->s, r.x, cy - 3, kx - r.x + 3, 6, 3, T.accent);
    glass_knob(u->s, kx, cy, u->active == id);
    return changed;
}

/* ------------------------------------------------------------ segmented control */
int ui_segmented(ui_t *u, int id, rect_t r, const char **labels, int n, int sel) {
    /* trilho em cápsula + segmento selecionado "flutuando" como uma gota de vidro */
    ui_tool_group(u, r);
    int res = sel;
    for (int i = 0; i < n; i++) {
        int x0 = r.x + r.w * i / n, x1 = r.x + r.w * (i + 1) / n;
        rect_t sr = rect_make(x0, r.y, x1 - x0, r.h);
        bool held;
        if (behave_f(u, id * 64 + i, sr, &held)) res = i;
        /* teclado: com um segmento focado, ← → mudam a seleção e o foco acompanha */
        if (u->kfocus == id * 64 + i && !u->key_used && (u->key == KEY_LEFT || u->key == KEY_RIGHT)) {
            int ns = CLAMP(sel + (u->key == KEY_RIGHT ? 1 : -1), 0, n - 1);
            res = ns;
            u->kfocus = id * 64 + ns;
            u->key_used = true;
            u->clicked_any = true;
        }
        if (i == sel) {
            float kx = sr.x + 2, kw = sr.w - 4, kh = r.h - 4;
            gfx_round_rect(u->s, kx, r.y + 2, kw, kh, kh / 2, T.dark ? RGBA(255, 255, 255, 40) : RGB(230, 230, 234));
        } else {
            if (held) gfx_round_rect(u->s, sr.x + 2, r.y + 2, sr.w - 4, r.h - 4, (r.h - 4) / 2.0f, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 14));
            if (i > 0 && i - 1 != sel) gfx_fill(u->s, x0, r.y + 6, 1, r.h - 12, T.dark ? RGBA(255, 255, 255, 34) : RGBA(0, 0, 0, 30));
        }
        text_draw_center(u->s, i == sel ? F_MED : F_UI, FONT_UI, rect_make(sr.x, sr.y - 1, sr.w, sr.h), T.control_text, labels[i]);
    }
    return res;
}

/* ------------------------------------------------------------ área de transferência (do sistema todo) */
static char *g_clip;
static int g_clip_len;
void clipboard_set(const char *t, int n) {
    if (n < 0) n = (int)strlen(t);
    char *nb = kmalloc(n + 1);
    if (!nb) return;
    memcpy(nb, t, n);
    nb[n] = 0;
    if (g_clip) kfree(g_clip);
    g_clip = nb;
    g_clip_len = n;
}
const char *clipboard_get(void) { return g_clip ? g_clip : ""; }

/* desfazer dos campos de uma linha: uma foto antes de cada grupo de edição (⌘Z volta, ⇧⌘Z refaz) */
static struct { char *owner; char text[256]; int caret; u64 t; bool redo; char redo_text[256]; } fu;
static void field_snapshot(ui_t *u, char *buf) {
    if (fu.owner == buf && u->now - fu.t < 900) { fu.t = u->now; return; }   /* digitação contínua = um grupo */
    fu.owner = buf;
    strncpy(fu.text, buf, sizeof fu.text - 1);
    fu.text[sizeof fu.text - 1] = 0;
    fu.caret = u->caret;
    fu.t = u->now;
    fu.redo = false;
}
static void sel_delete(ui_t *u, char *buf) {
    int len = (int)strlen(buf);
    if (u->sel_anchor < 0 || u->sel_anchor == u->caret) return;
    int a = MIN(u->sel_anchor, u->caret), b = MAX(u->sel_anchor, u->caret);
    if (b > len) b = len;
    memmove(buf + a, buf + b, len - b + 1);
    u->caret = a;
    u->sel_anchor = -1;
}
static void insert_text(ui_t *u, char *buf, int cap, const char *t, int tl) {
    sel_delete(u, buf);
    int len = (int)strlen(buf);
    if (len + tl >= cap) tl = cap - 1 - len;
    if (tl <= 0) return;
    memmove(buf + u->caret + tl, buf + u->caret, len - u->caret + 1);
    memcpy(buf + u->caret, t, tl);
    for (int i = 0; i < tl; i++) if (buf[u->caret + i] == '\n' || buf[u->caret + i] == '\r' || buf[u->caret + i] == '\t') buf[u->caret + i] = ' ';
    u->caret += tl;
}

/* ------------------------------------------------------------ campo de texto */
static bool edit_text(ui_t *u, char *buf, int cap) {
    int len = (int)strlen(buf);
    if (u->caret > len) u->caret = len;
    bool enter = false;
    if (u->key) {
        bool cmd = u->mods & (MOD_CMD | MOD_CTRL);
        bool shift = u->mods & MOD_SHIFT;
        int k = u->key;
        bool has_sel = u->sel_anchor >= 0 && u->sel_anchor != u->caret;
        int a = MIN(u->sel_anchor, u->caret), b = MAX(u->sel_anchor, u->caret);
        if (cmd && (k == 'a' || k == 'A')) { u->sel_anchor = 0; u->caret = len; u->key_used = true; }
        else if (cmd && (k == 'c' || k == 'C')) { if (has_sel) clipboard_set(buf + a, b - a); u->key_used = true; }
        else if (cmd && (k == 'x' || k == 'X')) {
            if (has_sel) { field_snapshot(u, buf); fu.t = 0; clipboard_set(buf + a, b - a); sel_delete(u, buf); }
            u->key_used = true;
        } else if (cmd && (k == 'v' || k == 'V')) {
            field_snapshot(u, buf); fu.t = 0;
            insert_text(u, buf, cap, clipboard_get(), g_clip_len);
            u->key_used = true;
        } else if (cmd && (k == 'z' || k == 'Z')) {
            if (fu.owner == buf) {
                if (!shift && !fu.redo) {
                    strncpy(fu.redo_text, buf, sizeof fu.redo_text - 1);
                    strncpy(buf, fu.text, cap - 1); buf[cap - 1] = 0;
                    fu.redo = true;
                } else if (shift && fu.redo) {
                    strncpy(buf, fu.redo_text, cap - 1); buf[cap - 1] = 0;
                    fu.redo = false;
                }
                u->caret = (int)strlen(buf); u->sel_anchor = -1;
                fu.t = 0;
            }
            u->key_used = true;
        }
        else if ((k == KEY_LEFT || k == KEY_RIGHT || k == KEY_HOME || k == KEY_END || k == KEY_UP || k == KEY_DOWN) && shift) {
            /* ⇧ + setas: estende a seleção a partir da âncora */
            if (u->sel_anchor < 0) u->sel_anchor = u->caret;
            if (k == KEY_LEFT) u->caret = utf8_prev(buf, u->caret);
            else if (k == KEY_RIGHT) u->caret = utf8_next(buf, u->caret);
            else if (k == KEY_HOME || k == KEY_UP) u->caret = 0;
            else u->caret = len;
            u->key_used = true;
        }
        else if (k == KEY_BACKSPACE) {
            field_snapshot(u, buf);
            if (u->sel_anchor >= 0 && u->sel_anchor != u->caret) {
                int a = MIN(u->sel_anchor, u->caret), b = MAX(u->sel_anchor, u->caret);
                memmove(buf + a, buf + b, len - b + 1);
                u->caret = a;
            } else if (u->caret > 0) {
                int p = utf8_prev(buf, u->caret);
                memmove(buf + p, buf + u->caret, len - u->caret + 1);
                u->caret = p;
            }
            u->sel_anchor = -1;
            u->key_used = true;
        } else if (k == KEY_DELETE) {
            field_snapshot(u, buf);
            if (u->caret < len) {
                int n = utf8_next(buf, u->caret);
                memmove(buf + u->caret, buf + n, len - n + 1);
            }
            u->key_used = true;
        } else if (k == KEY_LEFT) { u->caret = utf8_prev(buf, u->caret); u->sel_anchor = -1; u->key_used = true; }
        else if (k == KEY_RIGHT) { u->caret = utf8_next(buf, u->caret); u->sel_anchor = -1; u->key_used = true; }
        else if (k == KEY_HOME || k == KEY_UP) { u->caret = 0; u->sel_anchor = -1; u->key_used = true; }
        else if (k == KEY_END || k == KEY_DOWN) { u->caret = len; u->sel_anchor = -1; u->key_used = true; }
        else if (k == KEY_ENTER) { enter = true; u->key_used = true; }
        else if (u->text[0] && !cmd) {
            field_snapshot(u, buf);
            if (u->sel_anchor >= 0 && u->sel_anchor != u->caret) {
                int a = MIN(u->sel_anchor, u->caret), b = MAX(u->sel_anchor, u->caret);
                memmove(buf + a, buf + b, len - b + 1);
                u->caret = a;
                len = (int)strlen(buf);
            }
            u->sel_anchor = -1;
            int tl = (int)strlen(u->text);
            if (len + tl < cap) {
                memmove(buf + u->caret + tl, buf + u->caret, len - u->caret + 1);
                memcpy(buf + u->caret, u->text, tl);
                u->caret += tl;
            }
            u->key_used = true;
        }
        u->caret_blink = u->now;
    }
    return enter;
}

/* parâmetros extras do campo (o combo box reserva espaço à direita e escala a fonte) */
static int fl_pad_r = 0;
static float fl_fs = FONT_UI;

static bool field_common(ui_t *u, int id, rect_t r, char *buf, int cap, const char *ph, int pad_l, bool round) {
    focus_register(u, id, r, true);
    bool hover = ui_hit(u, r);
    if (hover) u->cursor = CURSOR_IBEAM;
    static int drag_id;   /* campo em que o botão foi pressionado: arrastar seleciona */
    if (u->pressed) {
        if (hover) {
            if (u->focus != id) { u->focus = id; u->sel_anchor = 0; u->caret = (int)strlen(buf); drag_id = 0; }
            else {
                u->caret = text_index_at(F_UI, fl_fs, buf, u->mx - r.x - pad_l);
                u->sel_anchor = u->caret;
                drag_id = id;
                if (u->dblclick) {
                    /* clique duplo: seleciona a palavra */
                    int a = u->caret, b = u->caret, len = (int)strlen(buf);
                    while (a > 0 && buf[a - 1] != ' ') a--;
                    while (b < len && buf[b] != ' ') b++;
                    u->sel_anchor = a; u->caret = b;
                    drag_id = 0;
                }
            }
            u->caret_blink = u->now;
        } else if (u->focus == id) {
            u->focus = 0;
        }
    } else if (u->down && drag_id == id && u->focus == id) {
        u->caret = text_index_at(F_UI, fl_fs, buf, u->mx - r.x - pad_l);
        u->repaint = true;
    } else if (!u->down && drag_id == id) drag_id = 0;
    bool focused = u->focus == id;
    bool enter = focused ? edit_text(u, buf, cap) : false;
    float rad = round ? r.h / 2.0f : MAX(CTRL_RADIUS, r.h * 0.26f);
    if (focused) ui_focus_ring(u, r, rad);
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 36));
    gfx_round_rect(u->s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, rad - 1, T.field_bg);
    gfx_round_rect(u->s, r.x + 1, r.y + 1, r.w - 2, 4, 2, T.dark ? RGBA(0, 0, 0, 40) : RGBA(0, 0, 0, 8)); /* sombra interna */
    rect_t prev = surface_push_clip(u->s, rect_make(r.x + 2, r.y + 1, r.w - 4 - fl_pad_r, r.h - 2));
    font_t *f = F_UI;
    int base = r.y + (r.h - (text_ascent(f, fl_fs) + text_descent(f, fl_fs))) / 2 + text_ascent(f, fl_fs);
    int tx = r.x + pad_l;
    int cx = text_width_n(f, fl_fs, buf, focused ? u->caret : 0);
    int scroll = 0;
    if (focused && cx > r.w - pad_l - fl_pad_r - 8) scroll = cx - (r.w - pad_l - fl_pad_r - 8);
    tx -= scroll;
    if (!buf[0] && ph && !focused) text_draw(u->s, f, fl_fs, tx, base, T.text3, ph);
    else if (!buf[0] && ph && focused) text_draw(u->s, f, fl_fs, tx, base, T.text3, ph);
    if (focused && u->sel_anchor >= 0 && u->sel_anchor != u->caret) {
        int a = MIN(u->sel_anchor, u->caret), b = MAX(u->sel_anchor, u->caret);
        int xa = text_width_n(f, fl_fs, buf, a), xb = text_width_n(f, fl_fs, buf, b);
        gfx_fill(u->s, tx + xa, r.y + 3, xb - xa, r.h - 6, T.selection);
    }
    text_draw(u->s, f, fl_fs, tx, base, T.text, buf);
    if (focused && ((u->now - u->caret_blink) / 530) % 2 == 0)
        gfx_fill(u->s, tx + cx, r.y + 4, 1, r.h - 8, T.dark ? RGB(230, 230, 230) : RGB(20, 20, 20));
    if (focused) u->repaint = true;
    surface_pop_clip(u->s, prev);
    return enter;
}

bool ui_textfield(ui_t *u, int id, rect_t r, char *buf, int cap, const char *ph) {
    return field_common(u, id, r, buf, cap, ph, 6, false);
}

/* busca em cápsula: lupa à esquerda e botão ⊗ para limpar quando há texto (ou o campo está em foco) */
bool ui_searchfield(ui_t *u, int id, rect_t r, char *buf, int cap) {
    int gs = CLAMP(r.h / 2, 12, 16);
    int d = CLAMP(r.h - 10, 14, 20);
    rect_t xr = rect_make(r.x + r.w - d - (r.h - d) / 2 - 2, r.y + (r.h - d) / 2, d, d);
    bool show_x = buf[0] || u->focus == id;
    bool on_x = show_x && ui_hit(u, xr);
    bool saved = u->pressed;
    if (on_x) u->pressed = false; /* o clique no ⊗ não move o cursor do campo */
    fl_pad_r = show_x ? d + 8 : 0;
    bool e = field_common(u, id, r, buf, cap, "Buscar", gs + 14, true);
    fl_pad_r = 0;
    u->pressed = saved;
    glyph_search(u->s, r.x + 9, r.y + (r.h - gs) / 2, gs, T.dark ? RGB(200, 200, 204) : RGB(60, 60, 64));
    if (show_x) {
        bool held;
        if (behave(u, id + 0x30000, xr, &held)) { buf[0] = 0; u->caret = 0; u->sel_anchor = -1; u->clicked_any = true; }
        float cx = xr.x + d / 2.0f, cy = xr.y + d / 2.0f, rr = d / 2.0f;
        gfx_circle(u->s, cx, cy, rr, T.dark ? (held ? RGB(190, 190, 194) : RGB(150, 150, 154)) : (held ? RGB(100, 100, 104) : RGB(132, 132, 136)));
        u32 xc = T.dark ? RGB(40, 40, 42) : RGB(255, 255, 255);
        float k = rr * 0.38f;
        gfx_line(u->s, cx - k, cy - k, cx + k, cy + k, 1.6f, xc);
        gfx_line(u->s, cx + k, cy - k, cx - k, cy + k, 1.6f, xc);
    }
    return e;
}

/* ------------------------------------------------------------ pop-up */
int ui_popup(ui_t *u, int id, rect_t r, const char **items, int n, int sel) {
    int v;
    if (popup_take_result(u->win, id, &v)) { sel = v; u->clicked_any = true; }
    bool held;
    if (behave_f(u, id, r, &held) || (held && u->pressed)) {
        rect_t cr = wm_content_rect(u->win);
        popup_open(cr.x + r.x - 8, cr.y + r.y - 7 - sel * 24, r.w + 14, items, n, sel, u->win, id);
        u->active = 0;
    }
    draw_push(u, r, false, false, true);
    rect_t pr = surface_push_clip(u->s, rect_make(r.x, r.y, r.w - 22, r.h));
    if (sel >= 0 && sel < n) {
        font_t *f = F_UI;
        int base = r.y + (r.h - (text_ascent(f, FONT_UI) + text_descent(f, FONT_UI))) / 2 + text_ascent(f, FONT_UI) - 1;
        text_draw(u->s, f, FONT_UI, r.x + 9, base, T.control_text, items[sel]);
    }
    surface_pop_clip(u->s, pr);
    /* setas duplas discretas (⌃⌄) à direita da cápsula */
    float ax = r.x + r.w - r.h / 2.0f - 2, ay = r.y + r.h / 2.0f;
    u32 wc = T.control_text;
    gfx_line(u->s, ax - 2.6f, ay - 1.4f, ax, ay - 4.0f, 1.4f, wc);
    gfx_line(u->s, ax, ay - 4.0f, ax + 2.6f, ay - 1.4f, 1.4f, wc);
    gfx_line(u->s, ax - 2.6f, ay + 1.4f, ax, ay + 4.0f, 1.4f, wc);
    gfx_line(u->s, ax, ay + 4.0f, ax + 2.6f, ay + 1.4f, 1.4f, wc);
    return sel;
}

/* ------------------------------------------------------------ progresso */
void ui_progress(ui_t *u, rect_t r, float v) { ui_progress_bar(u, r, v, true); }

static u32 progress_fill(bool tinted) {
    if (tinted) return T.dark ? RGB(10, 140, 255) : RGB(0, 136, 255);
    return T.dark ? RGB(88, 88, 92) : RGB(208, 208, 212);
}
static u32 progress_track(void) { return T.dark ? RGB(46, 46, 48) : RGB(234, 234, 236); }

void ui_progress_bar(ui_t *u, rect_t r, float v, bool tinted) {
    float rad = r.h / 2.0f;
    u32 fill = progress_fill(tinted);
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, progress_track());
    gfx_round_rect_stroke(u->s, r.x, r.y, r.w, r.h, rad, 1, T.dark ? RGBA(255, 255, 255, 10) : RGBA(0, 0, 0, 12));
    if (v < 0) {
        /* indeterminado: faixa que desliza, com cauda em degradê transparente → cor */
        int seg = r.w * 2 / 5;
        int phase = (int)((u->now / 6) % (u64)(r.w + seg));
        int x1 = r.x - seg + phase;
        rect_t pc = surface_push_clip(u->s, r);
        for (int i = 0; i < seg; i++) {
            int a = 255 * i / seg;
            gfx_fill(u->s, x1 + i, r.y, 1, r.h, WITH_ALPHA(fill, a * a / 255));
        }
        surface_pop_clip(u->s, pc);
        gfx_round_rect_stroke(u->s, r.x - 1, r.y - 1, r.w + 2, r.h + 2, rad + 1, 1.5f, T.dark ? RGB(30, 30, 32) : RGB(255, 255, 255));
        u->repaint = true;
        return;
    }
    v = CLAMP(v, 0.0f, 1.0f);
    if (v > 0) gfx_round_rect(u->s, r.x, r.y, MAX((float)r.h, r.w * v), r.h, rad, fill);
}

void ui_progress_ring(ui_t *u, int cx, int cy, int d, float v, bool tinted) {
    float r = d / 2.0f;
    u32 fill = progress_fill(tinted);
    bool pie = d < 28;
    float t = pie ? r * 0.62f : MAX(3.0f, d * 0.1f);
    gfx_ring_arc(u->s, cx, cy, r, t, 0, 2 * PI_F, progress_track());
    if (v < 0) { ui_spinner(u, cx, cy, (int)r); return; }
    v = CLAMP(v, 0.0f, 1.0f);
    if (v <= 0) return;
    float a1 = v * 2 * PI_F;
    gfx_ring_arc(u->s, cx, cy, r, t, 0, a1, fill);
    /* pontas arredondadas */
    float mr = r - t / 2;
    gfx_circle(u->s, cx, cy - mr, t / 2, fill);
    gfx_circle(u->s, cx + fsin(a1) * mr, cy - fcos(a1) * mr, t / 2, fill);
}

void ui_spinner(ui_t *u, int cx, int cy, int r) {
    /* 8 raios grossos e arredondados; o mais recente é o mais escuro */
    int phase = (int)(u->now / 110) % 8;
    u32 base = T.dark ? RGB(170, 170, 174) : RGB(120, 120, 124);
    for (int i = 0; i < 8; i++) {
        float a = i * PI_F / 4;
        int age = (phase - i + 8) % 8;
        int al = 255 - age * 26;
        gfx_line(u->s, cx + fsin(a) * r * 0.45f, cy - fcos(a) * r * 0.45f, cx + fsin(a) * r * 0.92f, cy - fcos(a) * r * 0.92f,
                 MAX(1.6f, r * 0.24f), WITH_ALPHA(base, CLAMP(al, 50, 255)));
    }
    u->repaint = true;
}

/* ------------------------------------------------------------ listas, rolagem, sidebar */
bool ui_list_row(ui_t *u, int id, rect_t r, bool selected, bool alt) {
    bool c = behave(u, id, r, NULL);
    /* linhas em "pílula": alternadas em cinza-claro, seleção em azul arredondado */
    if (alt && !selected) gfx_round_rect(u->s, r.x + 8, r.y, r.w - 16, r.h, 6, T.alt_row);
    if (selected) gfx_round_rect(u->s, r.x + 8, r.y, r.w - 16, r.h, 6, T.accent);
    return c;
}

/* rolagem com inércia e borda elástica: a roda dá um impulso que desacelera aos poucos; passar do
 * início/fim estica com resistência e volta com mola. O estado fica por (janela, id). */
static struct { struct window *win; int id; float pos, v; u64 t; } scr[32];

int ui_scroll(ui_t *u, int id, rect_t view, int content_h, int *off) {
    int maxo = MAX(0, content_h - view.h);
    int k = -1, freek = 0;
    for (int i = 0; i < 32; i++) {
        if (scr[i].win == u->win && scr[i].id == id) { k = i; break; }
        if (!scr[i].win || scr[i].t + 30000 < u->now) freek = i;
    }
    if (k < 0) { k = freek; scr[k].win = u->win; scr[k].id = id; scr[k].pos = (float)*off; scr[k].v = 0; scr[k].t = u->now; }
    if ((int)scr[k].pos != *off) { scr[k].pos = (float)*off; scr[k].v = 0; }      /* o app mudou a posição (ex.: navegou) */
    float dt = CLAMP((float)(u->now - scr[k].t), 0.0f, 50.0f) / 16.0f;
    scr[k].t = u->now;
    if (ui_hit(u, view) && u->wheel) {
        bool beyond = scr[k].pos < 0 || scr[k].pos > maxo;
        scr[k].v += u->wheel * (beyond ? 7.0f : 8.0f);
        u->wheel = 0;
    }
    if (u->active != id && (scr[k].v != 0 || scr[k].pos < 0 || scr[k].pos > maxo)) {
        scr[k].pos += scr[k].v * dt;
        scr[k].v *= fpow(0.90f, dt);                       /* atrito */
        float lim = view.h * 0.25f;
        if (scr[k].pos < 0) {
            scr[k].v *= fpow(0.78f, dt);                   /* fora da borda: freia forte */
            if (scr[k].pos < -lim) scr[k].pos = -lim;
            if (fabsf_(scr[k].v) < 2) scr[k].pos += (0 - scr[k].pos) * MIN(1.0f, 0.22f * dt);   /* mola */
            if (scr[k].pos > -0.5f && fabsf_(scr[k].v) < 0.5f) { scr[k].pos = 0; scr[k].v = 0; }
        } else if (scr[k].pos > maxo) {
            scr[k].v *= fpow(0.78f, dt);
            if (scr[k].pos > maxo + lim) scr[k].pos = maxo + lim;
            if (fabsf_(scr[k].v) < 2) scr[k].pos += (maxo - scr[k].pos) * MIN(1.0f, 0.22f * dt);
            if (scr[k].pos < maxo + 0.5f && fabsf_(scr[k].v) < 0.5f) { scr[k].pos = (float)maxo; scr[k].v = 0; }
        }
        if (fabsf_(scr[k].v) < 0.2f) scr[k].v = 0;
        u->repaint = true;
    }
    *off = (int)scr[k].pos;
    rect_t track = rect_make(view.x + view.w - 11, view.y + 2, 9, view.h - 4);
    if (maxo > 0) {
        int th = MAX(24, track.h * view.h / content_h);
        int ty = track.y + (track.h - th) * (*off) / maxo;
        rect_t thumb = rect_make(track.x + 1, ty, 7, th);
        if (ui_hit(u, track) && u->pressed) { u->active = id; u->drag_y = u->my - ty; }
        if (u->active == id) {
            if (u->down) { *off = CLAMP((u->my - u->drag_y - track.y) * maxo / MAX(1, track.h - th), 0, maxo); scr[k].pos = (float)*off; scr[k].v = 0; }
            if (u->released) u->active = 0;
        }
        ty = track.y + (track.h - th) * CLAMP(*off, 0, maxo) / maxo;
        /* esticando além da borda, a barra encolhe como no sistema */
        int over = *off < 0 ? -*off : *off > maxo ? *off - maxo : 0;
        if (over) { th = MAX(10, th - over); if (*off > maxo) ty = track.y + track.h - th; }
        thumb.y = ty;
        thumb.h = th;
        /* barra sobreposta: fina e discreta; engrossa (com trilho) quando o ponteiro chega perto ou ao arrastar */
        bool show = ui_hit(u, view) || u->active == id;
        bool near = u->active == id || ui_hit(u, rect_make(track.x - 6, track.y, track.w + 6, track.h));
        float wv = ui_anim(u, id + 0x70000, near ? 1.0f : 0.0f, 120);
        float tw = 6 + 4 * wv, tx = view.x + view.w - 3 - tw;
        if (wv > 0.05f) gfx_round_rect(u->s, tx - 1, track.y, tw + 2, track.h, (tw + 2) / 2, T.dark ? RGBA(255, 255, 255, (int)(16 * wv)) : RGBA(0, 0, 0, (int)(10 * wv)));
        if (show) gfx_round_rect(u->s, tx, thumb.y, tw, thumb.h, tw / 2, T.scroll_thumb);
    }
    /* conteúdo menor que a área: ainda estica e volta (mas sem barra) */
    return *off;
}

bool ui_sidebar_item(ui_t *u, int id, rect_t r, const char *label, bool selected, void (*icon)(surface_t *, int, int, int, u32)) {
    /* padrão do sistema: pílula de seleção cinza bem clara (sem destaque azul), ícone em traço na
     * cor de destaque, rótulo em texto normal; o tamanho do ícone/texto acompanha a altura da linha */
    bool c = behave_f(u, id, r, NULL);
    bool held = u->active == id && ui_hit(u, r) && u->down;
    if (selected || held) gfx_round_rect(u->s, r.x, r.y, r.w, r.h, MIN(10.0f, r.h * 0.3f), T.sidebar_sel);
    float fs = CLAMP(r.h * 0.54f, 13.0f, 17.0f);
    int isz = (int)(fs * 1.2f + 0.5f);
    int pad = MAX(8, r.h / 3);
    int tx = r.x + pad;
    if (icon) {
        u32 ic = T.dark ? color_lighten(T.accent, 40) : T.accent;
        icon(u->s, r.x + pad, r.y + (r.h - isz) / 2, isz, ic);
        tx += isz + MAX(8, isz / 2);
    }
    font_t *f = F_UI;
    int base = r.y + (r.h - (text_ascent(f, fs) + text_descent(f, fs))) / 2 + text_ascent(f, fs);
    text_draw_ellipsis(u->s, f, fs, tx, base, r.w - (tx - r.x) - 6, T.sidebar_text, label);
    return c;
}

/* título de seção da barra lateral ("Favoritos", "Locais"): cinza, seminegrito, alinhado aos ícones */
void ui_sidebar_header(ui_t *u, rect_t r, const char *label) {
    float fs = CLAMP(r.h * 0.5f, 11.5f, 15.0f);
    font_t *f = font_get(FONT_SEMIBOLD);
    int pad = MAX(8, r.h / 3);
    int base = r.y + r.h - 6;
    text_draw_ellipsis(u->s, f, fs, r.x + pad, base, r.w - pad, T.dark ? RGB(150, 150, 156) : RGB(134, 134, 140), label);
}

void ui_disclosure(ui_t *u, int x, int y, bool open) {
    glyph_chevron(u->s, x, y, 10, T.text2, open ? 2 : 0);
}

/* ============================================================ UI kit: botões com estilo, circulares e combo box */
#define RED_SYS RGB(255, 56, 60)

/* paleta por estilo e estado (0 normal, 1 pressionado, 2 desativado), nos dois temas */
static void style_colors(int style, int state, bool key, u32 *bg, u32 *fg) {
    u32 acc = T.accent, red = RED_SYS;
    if (T.dark) { acc = color_lighten(acc, 26); red = RGB(255, 96, 96); }
    /* janela fora de foco: estilos coloridos passam a cinza (o texto simples e o sutil mantêm a cor) */
    if (!key && (style == BTN_TINTED || style == BTN_TINTED_RED || style == BTN_FILLED || style == BTN_FILLED_RED)) style = BTN_GRAY;
    bool d = T.dark;
    switch (style) {
    case BTN_TINTED:
    case BTN_TINTED_RED: {
        u32 c = style == BTN_TINTED ? acc : red;
        int a[3] = {d ? 70 : 50, d ? 90 : 74, d ? 34 : 26};
        *bg = WITH_ALPHA(c, a[state]);
        *fg = state == 2 ? WITH_ALPHA(c, d ? 110 : 120) : state == 1 && !d ? color_lighten(c, -16) : c;
        break;
    }
    case BTN_FILLED:
    case BTN_FILLED_RED: {
        u32 c = style == BTN_FILLED ? (d ? RGB(26, 170, 255) : RGB(0, 136, 255)) : (d ? RGB(255, 96, 96) : red);
        if (style == BTN_FILLED && g_accent_index != 0) c = d ? color_lighten(T.accent, 26) : T.accent;
        *bg = state == 0 ? c : state == 1 ? (d ? color_lighten(c, 30) : color_lighten(c, -26)) : (d ? color_mix(c, RGB(30, 30, 32), 120) : color_mix(c, RGB(255, 255, 255), 100));
        *fg = state == 2 ? (d ? RGBA(255, 255, 255, 140) : RGBA(255, 255, 255, 230)) : RGB(255, 255, 255);
        break;
    }
    case BTN_PLAIN:
        *bg = 0;
        *fg = state == 0 ? acc : state == 1 ? (d ? color_lighten(acc, 30) : color_lighten(acc, -50)) : WITH_ALPHA(acc, d ? 110 : 130);
        break;
    case BTN_SUBTLE: {
        int a[3] = {d ? 34 : 18, d ? 60 : 40, d ? 18 : 12};
        *bg = WITH_ALPHA(acc, a[state]);
        *fg = state == 2 ? WITH_ALPHA(acc, d ? 100 : 110) : acc;
        break;
    }
    case BTN_PLAIN_GRAY:
        *bg = 0;
        *fg = state == 2 ? T.text3 : state == 1 ? T.text : T.text2;
        break;
    case BTN_GRAY_ACCENT:
        style_colors(BTN_GRAY, state, key, bg, fg);
        *fg = state == 2 ? WITH_ALPHA(acc, d ? 100 : 110) : acc;
        break;
    default: /* cinza */
        *bg = state == 0 ? (d ? RGB(52, 52, 54) : RGB(236, 236, 238)) : state == 1 ? (d ? RGB(76, 76, 80) : RGB(220, 220, 224)) : (d ? RGB(40, 40, 42) : RGB(246, 246, 247));
        *fg = state == 2 ? (d ? RGB(96, 96, 100) : RGB(176, 176, 180)) : (d ? RGB(228, 228, 232) : RGB(24, 24, 26));
        break;
    }
}

bool ui_button_style(ui_t *u, int id, rect_t r, const char *label, int style, bool enabled, glyph_fn icon) {
    bool held = false;
    bool c = enabled ? behave_f(u, id, r, &held) : false;
    /* botão principal (BTN_DEFAULT): Enter aciona quando nenhum campo de texto está em foco */
    if (!c && enabled && (style & BTN_DEFAULT) && u->key == KEY_ENTER && !u->key_used && !u->focus) { c = true; u->key_used = true; u->clicked_any = true; }
    bool key = !u->win || wm_focused() == u->win;
    int base = style & BTN_STYLE_MASK;
    int state = !enabled ? 2 : (held || (style & BTN_SHOW_PRESSED)) ? 1 : 0;
    u32 bg, fg;
    style_colors(base, state, key, &bg, &fg);
    float rad = (style & BTN_CAPSULE) ? r.h / 2.0f : MAX(5.0f, r.h * 0.26f);
    if (C_A(bg)) gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, bg);
    if (state == 0 && enabled && ui_hit(u, r) && base != BTN_PLAIN)
        gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, T.dark ? RGBA(255, 255, 255, 10) : RGBA(0, 0, 0, 8));
    /* conteúdo centralizado: ícone + rótulo */
    float fs = CLAMP(r.h * 0.48f, 11.0f, 16.0f); /* 28 px → ~13,5 */
    font_t *f = F_UI;
    int isz = (int)(fs * 1.15f);
    int tw = label ? text_width(f, fs, label) : 0;
    int gap = icon && label ? 6 : 0;
    int total = (icon ? isz : 0) + gap + tw;
    int x = r.x + (r.w - total) / 2;
    if (icon) { icon(u->s, x, r.y + (r.h - isz) / 2, isz, fg); x += isz + gap; }
    if (label) {
        int bl = r.y + (r.h - (text_ascent(f, fs) + text_descent(f, fs))) / 2 + text_ascent(f, fs);
        text_draw(u->s, f, fs, x, bl, fg, label);
    }
    return c;
}

bool ui_circle_button(ui_t *u, int id, int cx, int cy, int d, glyph_fn icon, bool enabled, bool show_pressed) {
    rect_t r = rect_make(cx - d / 2, cy - d / 2, d, d);
    bool held = false;
    bool c = enabled ? behave_f(u, id, r, &held) : false;
    int state = !enabled ? 2 : (held || show_pressed) ? 1 : 0;
    u32 bg = T.dark ? (state == 0 ? RGB(56, 56, 58) : state == 1 ? RGB(84, 84, 88) : RGB(42, 42, 44))
                    : (state == 0 ? RGB(230, 230, 232) : state == 1 ? RGB(206, 206, 210) : RGB(243, 243, 244));
    u32 fg = state == 2 ? (T.dark ? RGB(96, 96, 100) : RGB(180, 180, 184)) : (T.dark ? RGB(226, 226, 230) : RGB(28, 28, 30));
    gfx_circle(u->s, cx, cy, d / 2.0f, bg);
    if (state == 0 && enabled && ui_hit(u, r)) gfx_circle(u->s, cx, cy, d / 2.0f, T.dark ? RGBA(255, 255, 255, 12) : RGBA(0, 0, 0, 8));
    int gs = MAX(8, d * 2 / 5);
    if (icon) icon(u->s, cx - gs / 2, cy - gs / 2, gs, fg);
    return c;
}

bool ui_combobox(ui_t *u, int id, rect_t r, char *buf, int cap, const char **items, int n, bool enabled) {
    bool changed = false;
    int v;
    if (popup_take_result(u->win, id, &v) && v >= 0 && v < n) {
        strncpy(buf, items[v], cap - 1);
        buf[cap - 1] = 0;
        changed = true;
        u->clicked_any = true;
    }
    int bs = r.h - 8;                      /* botão de lista quadrado, embutido à direita */
    rect_t br = rect_make(r.x + r.w - bs - 4, r.y + 4, bs, bs);
    float fs = CLAMP(r.h * 0.5f, 11.0f, 18.0f);
    if (enabled) {
        fl_pad_r = bs + 6;
        fl_fs = fs;
        /* o clique no botão não deve focar o campo */
        bool on_btn = ui_hit(u, br);
        bool saved = u->pressed;
        if (on_btn) u->pressed = false;
        field_common(u, id, r, buf, cap, NULL, (int)(r.h * 0.34f), false);
        u->pressed = saved;
        fl_pad_r = 0;
        fl_fs = FONT_UI;
    } else {
        float rad = MAX(CTRL_RADIUS, r.h * 0.26f);
        gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 22));
        gfx_round_rect(u->s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, rad - 1, T.field_bg);
        int bl = r.y + (r.h - (text_ascent(F_UI, fs) + text_descent(F_UI, fs))) / 2 + text_ascent(F_UI, fs);
        text_draw_ellipsis(u->s, F_UI, fs, r.x + (int)(r.h * 0.34f), bl, r.w - bs - 20, T.text, buf);
    }
    bool held = false;
    bool click = enabled ? behave_f(u, id + 0x40000, br, &held) : false;
    int state = !enabled ? 2 : held ? 1 : 0;
    u32 bg = T.dark ? (state == 0 ? RGB(52, 52, 56) : state == 1 ? RGB(80, 80, 84) : RGBA(255, 255, 255, 8))
                    : (state == 0 ? RGB(232, 232, 235) : state == 1 ? RGB(210, 210, 214) : RGB(246, 246, 247));
    u32 fg = state == 2 ? (T.dark ? RGB(90, 90, 94) : RGB(186, 186, 190)) : (T.dark ? RGB(226, 226, 230) : RGB(36, 36, 40));
    gfx_round_rect(u->s, br.x, br.y, br.w, br.h, MAX(4.0f, bs * 0.24f), bg);
    int gs = MAX(8, bs / 2);
    glyph_chevron_down(u->s, br.x + (bs - gs) / 2, br.y + (bs - gs) / 2, gs, fg);
    if (click && n > 0) {
        int sel = 0;
        for (int i = 0; i < n; i++) if (!strcmp(items[i], buf)) sel = i;
        rect_t cr = wm_content_rect(u->win);
        popup_open(cr.x + r.x - 6, cr.y + r.y + r.h + 4, r.w + 12, items, n, sel, u->win, id);
        u->active = 0;
    }
    return changed;
}

/* ------------------------------------------------------------ glifos do UI kit */
void glyph_chevron_down(surface_t *s, int x, int y, int sz, u32 c) {
    float t = MAX(1.4f, sz * 0.16f);
    gfx_line(s, x + sz * 0.18f, y + sz * 0.36f, x + sz * 0.5f, y + sz * 0.66f, t, c);
    gfx_line(s, x + sz * 0.5f, y + sz * 0.66f, x + sz * 0.82f, y + sz * 0.36f, t, c);
}

void glyph_updown(surface_t *s, int x, int y, int sz, u32 c) {
    float t = MAX(1.4f, sz * 0.15f);
    gfx_line(s, x + sz * 0.2f, y + sz * 0.38f, x + sz * 0.5f, y + sz * 0.12f, t, c);
    gfx_line(s, x + sz * 0.5f, y + sz * 0.12f, x + sz * 0.8f, y + sz * 0.38f, t, c);
    gfx_line(s, x + sz * 0.2f, y + sz * 0.62f, x + sz * 0.5f, y + sz * 0.88f, t, c);
    gfx_line(s, x + sz * 0.5f, y + sz * 0.88f, x + sz * 0.8f, y + sz * 0.62f, t, c);
}

/* coração em contorno (curva paramétrica clássica) */
void glyph_heart(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f, cy = y + sz * 0.46f, k = sz / 36.0f;
    float px = 0, py = 0;
    for (int i = 0; i <= 48; i++) {
        float a = i * 2 * PI_F / 48;
        float sa = fsin(a);
        float hx = 16 * sa * sa * sa;
        float hy = 13 * fcos(a) - 5 * fcos(2 * a) - 2 * fcos(3 * a) - fcos(4 * a);
        float qx = cx + hx * k, qy = cy - hy * k;
        if (i) gfx_line(s, px, py, qx, qy, MAX(1.3f, sz * 0.08f), c);
        px = qx; py = qy;
    }
}

void glyph_heart_slash(surface_t *s, int x, int y, int sz, u32 c) {
    glyph_heart(s, x, y, sz, c);
    gfx_line(s, x + sz * 0.08f, y + sz * 0.1f, x + sz * 0.92f, y + sz * 0.9f, MAX(1.3f, sz * 0.08f), c);
}

/* ============================================================ UI kit: desenho público de botão e formulários */
void ui_draw_button(surface_t *s, rect_t r, const char *label, int style, int state, bool key) {
    u32 bg, fg;
    int base = style & BTN_STYLE_MASK;
    style_colors(base, state, key, &bg, &fg);
    float rad = (style & BTN_CAPSULE) ? r.h / 2.0f : MAX(5.0f, r.h * 0.26f);
    if (C_A(bg)) gfx_round_rect(s, r.x, r.y, r.w, r.h, rad, bg);
    float fs = CLAMP(r.h * 0.48f, 11.0f, 16.0f);
    if (label) text_draw_center(s, F_UI, fs, rect_make(r.x, r.y - 1, r.w, r.h), fg, label);
}

void ui_group_box(ui_t *u, rect_t r) {
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, 12, T.dark ? RGB(38, 38, 40) : RGB(246, 246, 247));
}

rect_t ui_form_row(ui_t *u, rect_t g, int i, int rh, int nrows, const char *label) {
    int y = g.y + 6 + i * rh;
    if (i > 0) gfx_fill(u->s, g.x + 20, y, g.w - 40, 1, T.separator);
    font_t *f = F_UI;
    float fs = 14;
    int bl = y + (rh - (text_ascent(f, fs) + text_descent(f, fs))) / 2 + text_ascent(f, fs);
    if (label) text_draw_ellipsis(u->s, f, fs, g.x + 20, bl, g.w / 2 - 24, T.text, label);
    (void)nrows;
    return rect_make(g.x + g.w / 2, y, g.w / 2 - 20, rh);
}

static void glyph_info(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f, cy = y + sz / 2.0f, r = sz / 2.0f - 0.5f;
    gfx_circle_stroke(s, cx, cy, r, 1.3f, c);
    gfx_circle(s, cx, cy - r * 0.42f, 1.2f, c);
    gfx_line(s, cx, cy - r * 0.08f, cx, cy + r * 0.5f, 1.6f, c);
}
static void glyph_more(surface_t *s, int x, int y, int sz, u32 c) {
    for (int i = 0; i < 3; i++) gfx_circle(s, x + sz * (0.18f + i * 0.32f), y + sz / 2.0f, MAX(1.2f, sz * 0.09f), c);
}

bool ui_detail(ui_t *u, int id, rect_t row, const char *value, int accessory) {
    font_t *f = F_UI;
    float fs = 14;
    int acc_w = accessory == DETAIL_NONE ? 0 : accessory == DETAIL_DISCLOSURE ? 16 : 26;
    int tw = value ? text_width(f, fs, value) : 0;
    int right = row.x + row.w;
    rect_t hit = rect_make(right - acc_w - tw - 8, row.y, acc_w + tw + 8, row.h);
    bool c = behave_f(u, id, hit, NULL);
    bool hov = ui_hit(u, hit);
    u32 col = hov && accessory != DETAIL_NONE ? T.text : T.text2;
    int bl = row.y + (row.h - (text_ascent(f, fs) + text_descent(f, fs))) / 2 + text_ascent(f, fs);
    if (value) text_draw(u->s, f, fs, right - acc_w - tw, bl, col, value);
    int cy = row.y + row.h / 2;
    if (accessory == DETAIL_DISCLOSURE) glyph_chevron(u->s, right - 12, cy - 6, 12, T.text3, 0);
    else if (accessory == DETAIL_INFO) glyph_info(u->s, right - 18, cy - 9, 18, col);
    else if (accessory == DETAIL_MENU) glyph_more(u->s, right - 18, cy - 9, 18, col);
    return c;
}

int ui_popup_inline(ui_t *u, int id, rect_t row, const char **items, int n, int sel) {
    int v;
    if (popup_take_result(u->win, id, &v)) { sel = v; u->clicked_any = true; }
    font_t *f = F_UI;
    float fs = 14;
    const char *lab = sel >= 0 && sel < n ? items[sel] : "";
    int tw = text_width(f, fs, lab), d = 22;
    int right = row.x + row.w;
    rect_t hit = rect_make(right - d - 8 - tw, row.y + (row.h - d) / 2, d + 8 + tw, d);
    bool held;
    if (behave_f(u, id, hit, &held)) {
        rect_t cr = wm_content_rect(u->win);
        popup_open(cr.x + hit.x - 8, cr.y + hit.y - 7 - sel * 24, MAX(hit.w + 24, 150), items, n, sel, u->win, id);
        u->active = 0;
    }
    int bl = row.y + (row.h - (text_ascent(f, fs) + text_descent(f, fs))) / 2 + text_ascent(f, fs);
    text_draw(u->s, f, fs, right - d - 8 - tw, bl, T.text, lab);
    float cx = right - d / 2.0f, cy = row.y + row.h / 2.0f;
    gfx_circle(u->s, cx, cy, d / 2.0f, T.dark ? (held ? RGB(84, 84, 88) : RGB(58, 58, 62)) : (held ? RGB(210, 210, 214) : RGB(230, 230, 233)));
    glyph_updown(u->s, (int)cx - 6, (int)cy - 6, 12, T.text);
    return sel;
}

bool ui_stepper(ui_t *u, int id, rect_t r, int *v, int lo, int hi) {
    int aw = 26;
    rect_t fr = rect_make(r.x, r.y, r.w - aw + 6, r.h), ar = rect_make(r.x + r.w - aw, r.y, aw, r.h);
    float rad = MAX(CTRL_RADIUS, r.h * 0.26f);
    /* setas num bloco cinza arredondado; o campo branco fica por cima, à esquerda */
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, T.dark ? RGB(52, 52, 56) : RGB(236, 236, 238));
    gfx_round_rect(u->s, fr.x, fr.y, fr.w, fr.h, rad, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 30));
    gfx_round_rect(u->s, fr.x + 1, fr.y + 1, fr.w - 2, fr.h - 2, rad - 1, T.dark ? RGB(24, 24, 26) : RGB(255, 255, 255));
    char num[16];
    snprintf(num, sizeof num, "%d", *v);
    float fs = CLAMP(r.h * 0.5f, 12.0f, 16.0f);
    text_draw_center(u->s, F_UI, fs, rect_make(fr.x, fr.y - 1, fr.w - 4, fr.h), T.text, num);
    rect_t up = rect_make(ar.x, ar.y, ar.w, ar.h / 2), dn = rect_make(ar.x, ar.y + ar.h / 2, ar.w, ar.h - ar.h / 2);
    bool hu, hd;
    bool changed = false;
    if (behave_f(u, id, up, &hu) && *v < hi) { (*v)++; changed = true; }
    if (behave_f(u, id + 0x20000, dn, &hd) && *v > lo) { (*v)--; changed = true; }
    if (hu) gfx_round_rect(u->s, up.x + 2, up.y + 2, up.w - 4, up.h - 2, 4, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 20));
    if (hd) gfx_round_rect(u->s, dn.x + 2, dn.y, dn.w - 4, dn.h - 2, 4, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 20));
    u32 c = T.text;
    float cx = ar.x + ar.w / 2.0f, t = 1.5f, s = 4.5f;
    float uy = up.y + up.h / 2.0f + 1.5f, dy = dn.y + dn.h / 2.0f - 1.5f;
    gfx_line(u->s, cx - s, uy + s * 0.4f, cx, uy - s * 0.6f, t, *v < hi ? c : T.text3);
    gfx_line(u->s, cx, uy - s * 0.6f, cx + s, uy + s * 0.4f, t, *v < hi ? c : T.text3);
    gfx_line(u->s, cx - s, dy - s * 0.4f, cx, dy + s * 0.6f, t, *v > lo ? c : T.text3);
    gfx_line(u->s, cx, dy + s * 0.6f, cx + s, dy - s * 0.4f, t, *v > lo ? c : T.text3);
    if (changed) u->clicked_any = true;
    return changed;
}

/* ============================================================ UI kit: dica, popover, abas, tabela, árvore,
 * calendário, data, cor, tokens, folha */

/* ------------------------------------------------------------ dica de ferramenta (aparece após ~700 ms parado) */
static struct { void *win; int id; u64 t0; } tipst;

void ui_tooltip(ui_t *u, int id, rect_t r, const char *text) {
    if (ui_hit(u, r) && !u->down) {
        if (tipst.win != u->win || tipst.id != id) { tipst.win = u->win; tipst.id = id; tipst.t0 = u->now; }
        if (u->now - tipst.t0 >= 700) {
            strncpy(u->tip, text, sizeof u->tip - 1);
            u->tip[sizeof u->tip - 1] = 0;
            u->tip_x = u->mx + 2;
            u->tip_y = u->my + 22;
            u->tip_on = true;
        } else {
            u->repaint = true; /* continua contando o tempo */
        }
    } else if (tipst.win == u->win && tipst.id == id) {
        tipst.win = NULL;
    }
}

void ui_draw_tooltip(ui_t *u) {
    font_t *f = F_UI;
    float fs = 12;
    int tw = text_width(f, fs, u->tip) + 16, th = 22;
    int x = CLAMP(u->tip_x, 2, u->cw - tw - 2), y = u->tip_y;
    if (y + th > u->ch - 2) y = u->tip_y - 44;
    gfx_shadow(u->s, x, y, tw, th, 6, 8, T.dark ? 110 : 50, 2);
    gfx_round_rect(u->s, x - 0.5f, y - 0.5f, tw + 1, th + 1, 6.5f, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 30));
    gfx_round_rect(u->s, x, y, tw, th, 6, T.dark ? RGB(50, 50, 54) : RGB(252, 252, 252));
    text_draw_center(u->s, f, fs, rect_make(x, y - 1, tw, th), T.text, u->tip);
}

/* ------------------------------------------------------------ popover (painel com seta, ancorado num controle) */
rect_t ui_popover_frame(ui_t *u, rect_t anchor, int w, int h) {
    int arrow = 8;
    int x = CLAMP(anchor.x + anchor.w / 2 - w / 2, 6, u->cw - w - 6);
    bool below = anchor.y + anchor.h + arrow + h + 6 <= u->ch || anchor.y - arrow - h < 6;
    int y = below ? anchor.y + anchor.h + arrow + 2 : anchor.y - arrow - 2 - h;
    rect_t r = rect_make(x, y, w, h);
    u32 bg = T.dark ? RGB(44, 44, 48) : RGB(255, 255, 255);
    gfx_shadow(u->s, r.x, r.y, r.w, r.h, 12, 22, T.dark ? 150 : 60, 8);
    gfx_round_rect(u->s, r.x - 0.5f, r.y - 0.5f, r.w + 1, r.h + 1, 12.5f, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 30));
    float ax = CLAMP(anchor.x + anchor.w / 2.0f, r.x + 18.0f, r.x + r.w - 18.0f);
    if (below) {
        float t1[] = {ax - arrow - 1, r.y + 0.5f, ax + arrow + 1, r.y + 0.5f, ax, r.y - arrow - 1.0f};
        gfx_polygon(u->s, t1, 3, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 30));
        float t2[] = {ax - arrow, r.y + 1.0f, ax + arrow, r.y + 1.0f, ax, r.y - arrow + 0.0f};
        gfx_polygon(u->s, t2, 3, bg);
    } else {
        float t1[] = {ax - arrow - 1, r.y + r.h - 0.5f, ax + arrow + 1, r.y + r.h - 0.5f, ax, r.y + r.h + arrow + 1.0f};
        gfx_polygon(u->s, t1, 3, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 30));
        float t2[] = {ax - arrow, r.y + r.h - 1.0f, ax + arrow, r.y + r.h - 1.0f, ax, r.y + r.h + arrow + 0.0f};
        gfx_polygon(u->s, t2, 3, bg);
    }
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, 12, bg);
    return r;
}

/* estado do popover aberto (um por vez): quem abriu, onde, e o que mostra */
enum { POP_NONE, POP_DATE, POP_COLOR };
static struct {
    void *win; int id, kind; rect_t anchor, panel;
    int *y, *m, *d; u32 *color; bool changed;
} pop;

static bool pop_is(ui_t *u, int id) { return pop.kind != POP_NONE && pop.win == u->win && pop.id == id; }
static void pop_close(ui_t *u) { pop.kind = POP_NONE; pop.win = NULL; u->modal_next = rect_make(0, 0, 0, 0); u->repaint = true; }

/* clique fora do painel (e fora do controle que o abriu) fecha; Esc também */
static void pop_outside_close(ui_t *u) {
    if ((u->pressed && !rect_contains(pop.panel, u->mx, u->my) && !rect_contains(pop.anchor, u->mx, u->my)) || u->key == KEY_ESC) {
        if (u->key == KEY_ESC) u->key_used = true;
        pop_close(u);
    }
}

/* ------------------------------------------------------------ abas (segmentado centrado na borda de um painel) */
int ui_tabview(ui_t *u, int id, rect_t r, const char **labels, int n, int sel) {
    gfx_round_rect(u->s, r.x - 0.5f, r.y - 0.5f, r.w + 1, r.h + 1, 10.5f, T.dark ? RGBA(255, 255, 255, 16) : RGBA(0, 0, 0, 14));
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, 10, T.dark ? RGB(38, 38, 40) : RGB(246, 246, 247));
    int sw = 0;
    for (int i = 0; i < n; i++) sw += text_width(F_UI, FONT_UI, labels[i]) + 28;
    sw = MIN(sw, r.w - 20);
    return ui_segmented(u, id, rect_make(r.x + (r.w - sw) / 2, r.y - 13, sw, 26), labels, n, sel);
}

/* ------------------------------------------------------------ cabeçalho de tabela com ordenação */
bool ui_table_header(ui_t *u, int id, rect_t r, const char **cols, const int *xs, int ncols, int *sort_col, bool *asc) {
    bool changed = false;
    font_t *f = font_get(FONT_SEMIBOLD);
    float fs = 12;
    int bl = r.y + (r.h - (text_ascent(f, fs) + text_descent(f, fs))) / 2 + text_ascent(f, fs);
    for (int i = 0; i < ncols; i++) {
        int x0 = xs[i], x1 = i + 1 < ncols ? xs[i + 1] : r.x + r.w;
        rect_t cr = rect_make(x0 - 6, r.y, x1 - x0, r.h);
        bool held;
        if (behave(u, id * 32 + i, cr, &held)) {
            if (*sort_col == i) *asc = !*asc; else { *sort_col = i; *asc = true; }
            changed = true;
        }
        bool hov = ui_hit(u, cr);
        if (held || hov) gfx_round_rect(u->s, cr.x + 2, cr.y + 3, cr.w - 4, cr.h - 6, 5, T.dark ? RGBA(255, 255, 255, held ? 24 : 12) : RGBA(0, 0, 0, held ? 18 : 8));
        u32 c = *sort_col == i ? T.text : T.text2;
        int tw = text_draw(u->s, f, fs, x0, bl, c, cols[i]);
        if (*sort_col == i) glyph_chevron(u->s, x0 + tw + 4, r.y + (r.h - 10) / 2, 10, T.text2, *asc ? 3 : 2);
        if (i > 0) gfx_fill(u->s, x0 - 8, r.y + 6, 1, r.h - 12, T.separator);
    }
    gfx_fill(u->s, r.x, r.y + r.h - 1, r.w, 1, T.separator);
    return changed;
}

/* ------------------------------------------------------------ linha de lista em árvore (disclosure ›/⌄) */
bool ui_outline_row(ui_t *u, int id, rect_t r, int depth, bool expandable, bool *open, const char *label, glyph_fn icon, bool selected) {
    int dx = r.x + 10 + depth * 18;
    rect_t dr = rect_make(dx - 2, r.y, 18, r.h);
    if (expandable && open && behave(u, id + 0x50000, dr, NULL)) *open = !*open;
    bool c = behave(u, id, r, NULL);
    if (selected) gfx_round_rect(u->s, r.x + 8, r.y, r.w - 16, r.h, 6, T.accent);
    u32 tc = selected ? RGB(255, 255, 255) : T.text;
    if (expandable) {
        float a = ui_anim(u, id + 0x60000, open && *open ? 1.0f : 0.0f, 150);
        /* chevron que gira de › para ⌄ */
        float cx = dx + 6, cy = r.y + r.h / 2.0f, s = 3.6f;
        float ang = a * PI_F / 2, ca = fcos(ang), sa = fsin(ang);
        float p[3][2] = {{-s * 0.5f, -s}, {s * 0.6f, 0}, {-s * 0.5f, s}};
        float q[3][2];
        for (int i = 0; i < 3; i++) { q[i][0] = cx + p[i][0] * ca - p[i][1] * sa; q[i][1] = cy + p[i][0] * sa + p[i][1] * ca; }
        u32 cc = selected ? RGB(255, 255, 255) : T.text2;
        gfx_line(u->s, q[0][0], q[0][1], q[1][0], q[1][1], 1.6f, cc);
        gfx_line(u->s, q[1][0], q[1][1], q[2][0], q[2][1], 1.6f, cc);
    }
    int tx = dx + 18;
    if (icon) { icon(u->s, tx, r.y + (r.h - 16) / 2, 16, selected ? RGB(255, 255, 255) : T.accent); tx += 22; }
    font_t *f = F_UI;
    int bl = r.y + (r.h - (text_ascent(f, 13.5f) + text_descent(f, 13.5f))) / 2 + text_ascent(f, 13.5f);
    text_draw_ellipsis(u->s, f, 13.5f, tx, bl, r.x + r.w - tx - 12, tc, label);
    return c;
}

/* ------------------------------------------------------------ calendário mensal */
static int days_in_month(int y, int m) {
    static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return d[(m - 1) % 12];
}
static int weekday_of(int y, int m, int d) { /* 0 = domingo (Zeller/Sakamoto) */
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
static const char *mes_nome[12] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};

/* r: área do calendário (recomendado 224 × 220). y/m/d: data selecionada; o mês exibido segue a seleção. */
bool ui_calendar(ui_t *u, int id, rect_t r, int *y, int *m, int *d) {
    bool changed = false;
    font_t *fb = font_get(FONT_SEMIBOLD), *fr = F_UI;
    char head[48];
    snprintf(head, sizeof head, "%s de %d", mes_nome[*m - 1], *y);
    text_draw(u->s, fb, 14, r.x + 8, r.y + 20, T.text, head);
    rect_t pv = rect_make(r.x + r.w - 56, r.y + 4, 24, 22), nx = rect_make(r.x + r.w - 28, r.y + 4, 24, 22);
    bool hp, hn;
    if (behave(u, id + 1, pv, &hp)) { if (--*m < 1) { *m = 12; --*y; } *d = MIN(*d, days_in_month(*y, *m)); changed = true; }
    if (behave(u, id + 2, nx, &hn)) { if (++*m > 12) { *m = 1; ++*y; } *d = MIN(*d, days_in_month(*y, *m)); changed = true; }
    if (hp || ui_hit(u, pv)) gfx_round_rect(u->s, pv.x, pv.y, pv.w, pv.h, 6, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 10));
    if (hn || ui_hit(u, nx)) gfx_round_rect(u->s, nx.x, nx.y, nx.w, nx.h, 6, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 10));
    glyph_chevron(u->s, pv.x + 5, pv.y + 4, 14, T.accent, 1);
    glyph_chevron(u->s, nx.x + 5, nx.y + 4, 14, T.accent, 0);
    static const char *wd[7] = {"D", "S", "T", "Q", "Q", "S", "S"};
    int cw = r.w / 7, top = r.y + 34;
    for (int i = 0; i < 7; i++) text_draw_center(u->s, font_get(FONT_SEMIBOLD), 11, rect_make(r.x + i * cw, top, cw, 16), T.text2, wd[i]);
    int first = weekday_of(*y, *m, 1), nd = days_in_month(*y, *m);
    datetime_t now;
    rtc_now(&now);
    int ch = (r.h - 54) / 6;
    for (int day = 1; day <= nd; day++) {
        int slot = first + day - 1, col = slot % 7, row = slot / 7;
        rect_t c = rect_make(r.x + col * cw, top + 20 + row * ch, cw, ch);
        float cx = c.x + c.w / 2.0f, cy = c.y + c.h / 2.0f, rr = MIN(c.w, c.h) / 2.0f - 1;
        bool sel = day == *d;
        bool today = now.year == *y && now.month == *m && now.day == day;
        bool held;
        if (behave(u, id * 64 + 100 + day, c, &held)) { *d = day; changed = true; sel = true; }
        if (sel) gfx_circle(u->s, cx, cy, rr, T.accent);
        else if (held || ui_hit(u, c)) gfx_circle(u->s, cx, cy, rr, T.dark ? RGBA(255, 255, 255, 22) : RGBA(0, 0, 0, 12));
        char num[4];
        snprintf(num, sizeof num, "%d", day);
        u32 tc = sel ? RGB(255, 255, 255) : today ? T.accent : (col == 0 || col == 6) ? T.text2 : T.text;
        text_draw_center(u->s, today && !sel ? fb : fr, 13, rect_make(c.x, c.y - 1, c.w, c.h), tc, num);
    }
    if (changed) u->clicked_any = true;
    return changed;
}

/* ------------------------------------------------------------ campo de data (abre o calendário num popover) */
static void glyph_calendar_small(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_round_rect_stroke(s, x + 0.5f, y + sz * 0.16f, sz - 1, sz * 0.8f, sz * 0.16f, 1.3f, c);
    gfx_fill(s, x + 1, y + (int)(sz * 0.36f), sz - 2, 1, c);
    gfx_line(s, x + sz * 0.3f, y + 0.5f, x + sz * 0.3f, y + sz * 0.26f, 1.3f, c);
    gfx_line(s, x + sz * 0.7f, y + 0.5f, x + sz * 0.7f, y + sz * 0.26f, 1.3f, c);
}

static void pop_date_overlay(ui_t *u) {
    rect_t p = ui_popover_frame(u, pop.anchor, 240, 236);
    pop.panel = p;
    u->modal_next = p;
    if (ui_calendar(u, pop.id * 8 + 7, rect_make(p.x + 8, p.y + 8, p.w - 16, p.h - 16), pop.y, pop.m, pop.d)) pop.changed = true;
    pop_outside_close(u);
}

bool ui_date_field(ui_t *u, int id, rect_t r, int *y, int *m, int *d) {
    bool held;
    bool c = behave_f(u, id, r, &held);
    bool open = pop_is(u, id);
    if (c) {
        if (open) pop_close(u);
        else { pop.win = u->win; pop.id = id; pop.kind = POP_DATE; pop.anchor = r; pop.y = y; pop.m = m; pop.d = d; pop.changed = false; open = true; }
    }
    float rad = MAX(CTRL_RADIUS, r.h * 0.26f);
    if (open) ui_focus_ring(u, r, rad);
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 36));
    gfx_round_rect(u->s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, rad - 1, T.field_bg);
    char txt[24];
    snprintf(txt, sizeof txt, "%02d/%02d/%04d", *d, *m, *y);
    font_t *f = F_UI;
    int bl = r.y + (r.h - (text_ascent(f, 13.5f) + text_descent(f, 13.5f))) / 2 + text_ascent(f, 13.5f);
    text_draw(u->s, f, 13.5f, r.x + 10, bl, T.text, txt);
    glyph_calendar_small(u->s, r.x + r.w - 24, r.y + (r.h - 15) / 2, 15, T.accent);
    bool changed = false;
    if (open) {
        u->overlay_fn = pop_date_overlay;
        if (pop.changed) { changed = true; pop.changed = false; }
    }
    return changed;
}

/* ------------------------------------------------------------ poço de cor (abre uma paleta num popover) */
static u32 palette_color(int col, int row) {
    if (col == 11) { int g = 255 - row * 255 / 5; return RGB(g, g, g); } /* coluna de cinzas */
    /* matiz por coluna, claridade por linha (HSL simplificado) */
    float h = col / 11.0f * 6.0f, l = 0.82f - row * 0.13f, sat = 0.85f;
    float c = (1 - fabsf_(2 * l - 1)) * sat, x = c * (1 - fabsf_(h - 2 * (int)(h / 2) - 1)), mm = l - c / 2;
    float r = 0, g = 0, b = 0;
    int hi = (int)h;
    if (hi == 0) { r = c; g = x; } else if (hi == 1) { r = x; g = c; } else if (hi == 2) { g = c; b = x; }
    else if (hi == 3) { g = x; b = c; } else if (hi == 4) { r = x; b = c; } else { r = c; b = x; }
    return RGB((int)((r + mm) * 255), (int)((g + mm) * 255), (int)((b + mm) * 255));
}

static void pop_color_overlay(ui_t *u) {
    int cs = 18, gap = 3, cols = 12, rows = 6;
    int w = 16 + cols * (cs + gap) - gap, h = 16 + rows * (cs + gap) - gap;
    rect_t p = ui_popover_frame(u, pop.anchor, w, h);
    pop.panel = p;
    u->modal_next = p;
    for (int row = 0; row < rows; row++)
        for (int col = 0; col < cols; col++) {
            rect_t c = rect_make(p.x + 8 + col * (cs + gap), p.y + 8 + row * (cs + gap), cs, cs);
            u32 color = palette_color(col, row);
            gfx_round_rect(u->s, c.x, c.y, c.w, c.h, 4, color);
            if (color == (*pop.color | 0xFF000000u)) gfx_round_rect_stroke(u->s, c.x - 2, c.y - 2, c.w + 4, c.h + 4, 6, 2, T.text);
            else if (ui_hit(u, c)) gfx_round_rect_stroke(u->s, c.x - 1.5f, c.y - 1.5f, c.w + 3, c.h + 3, 5.5f, 1.5f, T.text2);
            if (behave(u, pop.id * 128 + 1000 + row * 16 + col, c, NULL)) { *pop.color = color; pop.changed = true; }
        }
    pop_outside_close(u);
}

bool ui_color_well(ui_t *u, int id, rect_t r, u32 *color) {
    bool held;
    bool c = behave_f(u, id, r, &held);
    bool open = pop_is(u, id);
    if (c) {
        if (open) pop_close(u);
        else { pop.win = u->win; pop.id = id; pop.kind = POP_COLOR; pop.anchor = r; pop.color = color; pop.changed = false; open = true; }
    }
    float rad = r.h / 2.0f;
    u32 bg = T.dark ? (held ? RGB(74, 74, 78) : RGB(52, 52, 54)) : (held ? RGB(214, 214, 218) : RGB(236, 236, 238));
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, bg);
    if (open) ui_focus_ring(u, r, rad);
    gfx_round_rect(u->s, r.x + 4, r.y + 4, r.w - 8, r.h - 8, rad - 4, *color | 0xFF000000u);
    gfx_round_rect_stroke(u->s, r.x + 4, r.y + 4, r.w - 8, r.h - 8, rad - 4, 1, RGBA(0, 0, 0, 30));
    bool changed = false;
    if (open) {
        u->overlay_fn = pop_color_overlay;
        if (pop.changed) { changed = true; pop.changed = false; }
    }
    return changed;
}

/* ------------------------------------------------------------ campo de tokens (etiquetas em cápsula) */
/* tokens: matriz de nomes (tamanho máximo 24 cada); Enter, vírgula ou Tab confirmam; ⌫ com texto vazio remove o último */
bool ui_token_field(ui_t *u, int id, rect_t r, char tokens[][24], int *n, int max, char *buf, int cap) {
    bool changed = false;
    bool hover = ui_hit(u, r);
    if (hover) u->cursor = CURSOR_IBEAM;
    if (u->pressed) { if (hover) u->focus = id; else if (u->focus == id) u->focus = 0; }
    bool focused = u->focus == id;
    if (focused && u->key) {
        int len = (int)strlen(buf);
        if ((u->key == KEY_ENTER || u->key == KEY_TAB || u->key == ',') && len && *n < max) {
            strncpy(tokens[*n], buf, 23); tokens[*n][23] = 0; (*n)++; buf[0] = 0; changed = true; u->key_used = true;
        } else if (u->key == KEY_BACKSPACE) {
            if (len) buf[utf8_prev(buf, len)] = 0; else if (*n) { (*n)--; changed = true; }
            u->key_used = true;
        } else if (u->text[0] && u->key != ',' && len + (int)strlen(u->text) < cap - 1) {
            strcat(buf, u->text); u->key_used = true;
        }
        u->caret_blink = u->now;
    }
    float rad = CTRL_RADIUS;
    if (focused) ui_focus_ring(u, r, rad);
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, rad, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 36));
    gfx_round_rect(u->s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, rad - 1, T.field_bg);
    rect_t pc = surface_push_clip(u->s, rect_make(r.x + 2, r.y + 1, r.w - 4, r.h - 2));
    font_t *f = F_UI;
    float fs = 13;
    int x = r.x + 6, th = r.h - 8;
    for (int i = 0; i < *n; i++) {
        int tw = text_width(f, fs, tokens[i]) + 16;
        gfx_round_rect(u->s, x, r.y + 4, tw, th, th / 2.0f, WITH_ALPHA(T.accent, T.dark ? 70 : 46));
        text_draw_center(u->s, f, fs, rect_make(x, r.y + 3, tw, th), T.dark ? color_lighten(T.accent, 60) : color_lighten(T.accent, -30), tokens[i]);
        x += tw + 4;
    }
    int bl = r.y + (r.h - (text_ascent(f, fs) + text_descent(f, fs))) / 2 + text_ascent(f, fs);
    if (!buf[0] && !*n) text_draw(u->s, f, fs, x + 2, bl, T.text3, "Adicionar…");
    int tw = text_draw(u->s, f, fs, x + 2, bl, T.text, buf);
    if (focused && ((u->now - u->caret_blink) / 530) % 2 == 0) gfx_fill(u->s, x + 3 + tw, r.y + 5, 1, r.h - 10, T.text);
    if (focused) u->repaint = true;
    surface_pop_clip(u->s, pc);
    if (changed) u->clicked_any = true;
    return changed;
}

/* ------------------------------------------------------------ folha (painel modal preso ao topo da janela) */
/* Escurece o conteúdo e desenha o painel centralizado; devolve a área interna. O app deve desenhá-la por
 * último e ignorar a interação com o resto enquanto ela estiver aberta. */
rect_t ui_sheet(ui_t *u, int w, int h, int top) {
    gfx_fill(u->s, 0, 0, u->cw, u->ch, T.dark ? RGBA(0, 0, 0, 90) : RGBA(0, 0, 0, 40));
    rect_t r = rect_make((u->cw - w) / 2, top, w, h);
    gfx_shadow(u->s, r.x, r.y, r.w, r.h, 18, 30, T.dark ? 160 : 70, 10);
    gfx_round_rect(u->s, r.x - 0.5f, r.y - 0.5f, r.w + 1, r.h + 1, 18.5f, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 30));
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, 18, T.dark ? RGB(40, 40, 44) : RGB(255, 255, 255));
    /* bloqueia o que está embaixo a partir do próximo quadro; o conteúdo da folha (desenhado em seguida) responde */
    u->modal_next = rect_make(0, 0, u->cw, u->ch);
    u->in_overlay = true;
    return r;
}
/* fim do conteúdo da folha (o bloqueio some sozinho quando a folha deixa de ser desenhada) */
void ui_sheet_end(ui_t *u) { u->in_overlay = false; }

