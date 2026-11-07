/* mOS — Paciência (Klondike: virar 1 ou 3, arrastar e soltar, duplo clique manda para a fundação,
 * desfazer, pontuação, tempo, vitória animada) e Jogos (biblioteca dos jogos do sistema com recordes). */
#include "kit.h"

extern app_t app_paciencia, app_jogos;

/* ================================================================== Paciência */
typedef struct { i8 c[64]; int n; } pile_t;          /* cartas: 0..51 (naipe = c/13, valor = c%13); bit 0x40 = virada para cima */
#define UP 0x40
#define CARD(x) ((x) & 63)
#define SUIT(x) (CARD(x) / 13)
#define RANK(x) (CARD(x) % 13)
#define RED(x) (SUIT(x) == 1 || SUIT(x) == 2)

enum { P_STOCK, P_WASTE, P_F0, P_T0 = 6, NPILES = 13 };
typedef struct { pile_t p[NPILES]; int score, moves; } game_t;

static struct {
    window_t *w;
    game_t g;
    game_t undo[64];
    int nundo;
    bool draw3, won;
    u64 started, won_at;
    /* arrasto */
    int drag_pile, drag_idx;
    int dx, dy;
    bool dragging;
    int best;
    float cw, ch;             /* tamanho da carta */
    int ox, oy;
} PC = {.drag_pile = -1};

static void push(pile_t *p, i8 c) { if (p->n < 64) p->c[p->n++] = c; }
static void save_undo(void) { if (PC.nundo == 64) { memmove(PC.undo, PC.undo + 1, sizeof(game_t) * 63); PC.nundo--; } PC.undo[PC.nundo++] = PC.g; }

static void deal(void) {
    memset(&PC.g, 0, sizeof PC.g);
    i8 deck[52];
    for (int i = 0; i < 52; i++) deck[i] = (i8)i;
    for (int i = 51; i > 0; i--) { int j = rand32() % (i + 1); i8 t = deck[i]; deck[i] = deck[j]; deck[j] = t; }
    int k = 0;
    for (int t = 0; t < 7; t++)
        for (int i = 0; i <= t; i++) push(&PC.g.p[P_T0 + t], (i8)(deck[k++] | (i == t ? UP : 0)));
    while (k < 52) push(&PC.g.p[P_STOCK], deck[k++]);
    PC.nundo = 0;
    PC.won = false;
    PC.started = timer_ticks();
}

static bool can_found(i8 c, pile_t *f) {
    if (!f->n) return RANK(c) == 0;
    i8 t = f->c[f->n - 1];
    return SUIT(t) == SUIT(c) && RANK(c) == RANK(t) + 1;
}
static bool can_tab(i8 c, pile_t *t) {
    if (!t->n) return RANK(c) == 12;
    i8 top = t->c[t->n - 1];
    return (top & UP) && RED(top) != RED(c) && RANK(top) == RANK(c) + 1;
}
static void flip_tops(void) {
    for (int t = 0; t < 7; t++) {
        pile_t *p = &PC.g.p[P_T0 + t];
        if (p->n && !(p->c[p->n - 1] & UP)) { p->c[p->n - 1] |= UP; PC.g.score += 5; }
    }
}
static bool check_win(void) {
    for (int f = 0; f < 4; f++) if (PC.g.p[P_F0 + f].n != 13) return false;
    return true;
}
/* move cartas de from[idx..] para to; devolve true se válido */
static bool move(int from, int idx, int to) {
    pile_t *a = &PC.g.p[from], *b = &PC.g.p[to];
    if (from == to || idx < 0 || idx >= a->n || !(a->c[idx] & UP)) return false;
    int cnt = a->n - idx;
    i8 c = a->c[idx];
    if (to >= P_F0 && to < P_F0 + 4) { if (cnt != 1 || !can_found(c, b)) return false; }
    else if (to >= P_T0) { if (!can_tab(c, b)) return false; }
    else return false;
    save_undo();
    for (int i = 0; i < cnt; i++) push(b, a->c[idx + i]);
    a->n = idx;
    if (to >= P_F0 && to < P_F0 + 4) PC.g.score += 10;
    else if (from == P_WASTE) PC.g.score += 5;
    else if (from >= P_F0 && from < P_F0 + 4) PC.g.score = MAX(0, PC.g.score - 15);
    PC.g.moves++;
    flip_tops();
    if (check_win() && !PC.won) {
        PC.won = true;
        PC.won_at = timer_ticks();
        int secs = (int)((PC.won_at - PC.started) / 1000);
        int bonus = secs > 30 ? 700000 / secs : 0;
        PC.g.score += bonus;
        if (PC.g.score > PC.best) PC.best = PC.g.score;
        sound_alert();
    }
    return true;
}
static void draw_stock(void) {
    pile_t *s = &PC.g.p[P_STOCK], *w = &PC.g.p[P_WASTE];
    save_undo();
    if (!s->n) {
        while (w->n) push(s, CARD(w->c[--w->n]));
        PC.g.score = MAX(0, PC.g.score - (PC.draw3 ? 20 : 100));
        return;
    }
    int k = PC.draw3 ? 3 : 1;
    for (int i = 0; i < k && s->n; i++) push(w, (i8)(s->c[--s->n] | UP));
    PC.g.moves++;
}
static bool auto_found(int from) {
    pile_t *a = &PC.g.p[from];
    if (!a->n) return false;
    for (int f = 0; f < 4; f++) if (move(from, a->n - 1, P_F0 + f)) return true;
    return false;
}

/* ------------------------------------------------------------ desenho das cartas */
static const char *rank_s[13] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
static void suit_glyph(surface_t *s, float cx, float cy, float r, int suit, u32 c) {
    switch (suit) {
    case 0: /* espadas */
        gfx_circle(s, cx - r * 0.45f, cy + r * 0.1f, r * 0.5f, c);
        gfx_circle(s, cx + r * 0.45f, cy + r * 0.1f, r * 0.5f, c);
        { float t[] = {cx, cy - r, cx + r * 0.95f, cy + r * 0.05f, cx - r * 0.95f, cy + r * 0.05f}; gfx_polygon(s, t, 3, c); }
        { float t[] = {cx, cy + r * 0.2f, cx + r * 0.35f, cy + r, cx - r * 0.35f, cy + r}; gfx_polygon(s, t, 3, c); }
        break;
    case 1: /* copas */
        gfx_circle(s, cx - r * 0.45f, cy - r * 0.3f, r * 0.52f, c);
        gfx_circle(s, cx + r * 0.45f, cy - r * 0.3f, r * 0.52f, c);
        { float t[] = {cx - r * 0.96f, cy - r * 0.18f, cx + r * 0.96f, cy - r * 0.18f, cx, cy + r}; gfx_polygon(s, t, 3, c); }
        break;
    case 2: /* ouros */
        { float t[] = {cx, cy - r, cx + r * 0.72f, cy, cx, cy + r, cx - r * 0.72f, cy}; gfx_polygon(s, t, 4, c); }
        break;
    default: /* paus */
        gfx_circle(s, cx, cy - r * 0.45f, r * 0.42f, c);
        gfx_circle(s, cx - r * 0.48f, cy + r * 0.12f, r * 0.42f, c);
        gfx_circle(s, cx + r * 0.48f, cy + r * 0.12f, r * 0.42f, c);
        { float t[] = {cx, cy, cx + r * 0.3f, cy + r, cx - r * 0.3f, cy + r}; gfx_polygon(s, t, 3, c); }
    }
}
static void draw_card(surface_t *s, float x, float y, i8 c) {
    float w = PC.cw, h = PC.ch, r = w * 0.08f;
    gfx_shadow(s, (int)x, (int)y, (int)w, (int)h, r, 4, 60, 1);
    if (!(c & UP)) {
        gfx_round_rect(s, x, y, w, h, r, RGB(250, 250, 250));
        gfx_round_rect_grad(s, x + 3, y + 3, w - 6, h - 6, r - 2, RGB(40, 110, 220), RGB(20, 70, 170));
        for (int i = 0; i < 6; i++) gfx_round_rect_stroke(s, x + 7 + i * 2.0f, y + 7 + i * 2.0f, w - 14 - i * 4.0f, h - 14 - i * 4.0f, r, 0.8f, RGBA(255, 255, 255, 40));
        return;
    }
    gfx_round_rect(s, x, y, w, h, r, RGB(252, 252, 250));
    gfx_round_rect_stroke(s, x, y, w, h, r, 1, RGBA(0, 0, 0, 40));
    u32 col = RED(c) ? RGB(215, 30, 40) : RGB(25, 25, 30);
    float fs = w * 0.24f;
    text_draw(s, font_get(FONT_BOLD), fs, (int)(x + w * 0.08f), (int)(y + fs * 1.05f), col, rank_s[RANK(c)]);
    suit_glyph(s, x + w * 0.16f, y + fs * 1.55f, w * 0.08f, SUIT(c), col);
    if (RANK(c) >= 10) {
        /* figuras: moldura colorida com a letra grande */
        rect_t fr = rect_make((int)(x + w * 0.28f), (int)(y + h * 0.22f), (int)(w * 0.6f), (int)(h * 0.66f));
        gfx_round_rect(s, fr.x, fr.y, fr.w, fr.h, 4, RED(c) ? RGB(255, 236, 236) : RGB(236, 240, 255));
        gfx_round_rect_stroke(s, fr.x, fr.y, fr.w, fr.h, 4, 1, col);
        text_draw_center(s, font_get(FONT_BOLD), w * 0.36f, rect_make(fr.x, fr.y, fr.w, fr.h * 2 / 3), col, rank_s[RANK(c)]);
        suit_glyph(s, fr.x + fr.w / 2.0f, fr.y + fr.h * 0.78f, w * 0.08f, SUIT(c), col);
    } else suit_glyph(s, x + w * 0.58f, y + h * 0.58f, w * 0.22f, SUIT(c), col);
}

/* posição de cada pilha e da carta i nela */
static void pile_pos(int p, float *x, float *y) {
    float gap = PC.cw * 0.18f;
    if (p == P_STOCK) { *x = PC.ox; *y = PC.oy; }
    else if (p == P_WASTE) { *x = PC.ox + PC.cw + gap; *y = PC.oy; }
    else if (p < P_T0) { *x = PC.ox + (3 + (p - P_F0)) * (PC.cw + gap); *y = PC.oy; }
    else { *x = PC.ox + (p - P_T0) * (PC.cw + gap); *y = PC.oy + PC.ch + gap * 1.4f; }
}
static void card_pos(int p, int i, float *x, float *y) {
    pile_pos(p, x, y);
    if (p >= P_T0) {
        pile_t *pl = &PC.g.p[p];
        for (int k = 0; k < i; k++) *y += (pl->c[k] & UP) ? PC.ch * 0.26f : PC.ch * 0.12f;
    } else if (p == P_WASTE) {
        pile_t *w = &PC.g.p[P_WASTE];
        int shown = PC.draw3 ? MIN(3, w->n) : 1;
        int pos = i - (w->n - shown);
        if (pos > 0) *x += pos * PC.cw * 0.22f;
    }
}
/* pilha/carta sob o ponto */
static bool hit(int mx, int my, int *pile, int *idx) {
    for (int p = 0; p < NPILES; p++) {
        pile_t *pl = &PC.g.p[p];
        for (int i = pl->n - 1; i >= 0; i--) {
            float x, y;
            card_pos(p, i, &x, &y);
            if (mx >= x && mx < x + PC.cw && my >= y && my < y + PC.ch) { *pile = p; *idx = i; return true; }
            if (p != P_WASTE && p < P_T0) break;
        }
        float x, y;
        pile_pos(p, &x, &y);
        if (!pl->n && mx >= x && mx < x + PC.cw && my >= y && my < y + PC.ch) { *pile = p; *idx = -1; return true; }
    }
    return false;
}

static void pc_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_grad_v(s, 0, w->toolbar_h, u->cw, u->ch - w->toolbar_h, RGB(30, 120, 70), RGB(16, 80, 46));
    PC.cw = MIN((u->cw - 40) / 7.0f / 1.18f, (u->ch - w->toolbar_h - 40) / 4.6f);
    PC.ch = PC.cw * 1.4f;
    PC.ox = (int)((u->cw - (7 * PC.cw + 6 * PC.cw * 0.18f)) / 2);
    PC.oy = w->toolbar_h + 18;
    /* lugares vazios */
    for (int p = 0; p < NPILES; p++) {
        float x, y;
        pile_pos(p, &x, &y);
        if (p != P_WASTE) gfx_round_rect_stroke(s, x, y, PC.cw, PC.ch, PC.cw * 0.08f, 1.5f, RGBA(255, 255, 255, 60));
        if (p >= P_F0 && p < P_T0 && !PC.g.p[p].n) suit_glyph(s, x + PC.cw / 2, y + PC.ch / 2, PC.cw * 0.18f, p - P_F0, RGBA(255, 255, 255, 50));
        if (p == P_STOCK && !PC.g.p[p].n) gfx_circle_stroke(s, x + PC.cw / 2, y + PC.ch / 2, PC.cw * 0.2f, 3, RGBA(255, 255, 255, 80));
    }
    /* cartas */
    for (int p = 0; p < NPILES; p++) {
        pile_t *pl = &PC.g.p[p];
        int from = 0;
        if (p == P_STOCK || (p >= P_F0 && p < P_T0)) from = MAX(0, pl->n - 1);
        if (p == P_WASTE) from = MAX(0, pl->n - (PC.draw3 ? 3 : 1));
        for (int i = from; i < pl->n; i++) {
            if (PC.dragging && p == PC.drag_pile && i >= PC.drag_idx) continue;
            float x, y;
            card_pos(p, i, &x, &y);
            draw_card(s, x, y, pl->c[i]);
        }
    }
    /* entrada */
    if (u->pressed && !PC.won) {
        int p, i;
        if (hit(u->mx, u->my, &p, &i)) {
            if (p == P_STOCK) draw_stock();
            else if (i >= 0 && (PC.g.p[p].c[i] & UP)) {
                if (u->dblclick && i == PC.g.p[p].n - 1 && auto_found(p)) {}
                else if (p != P_WASTE || i == PC.g.p[p].n - 1) {
                    float x, y;
                    card_pos(p, i, &x, &y);
                    PC.drag_pile = p; PC.drag_idx = i; PC.dx = u->mx - (int)x; PC.dy = u->my - (int)y; PC.dragging = true;
                }
            }
        }
    }
    if (PC.dragging) {
        pile_t *pl = &PC.g.p[PC.drag_pile];
        float x = u->mx - PC.dx, y = u->my - PC.dy;
        for (int i = PC.drag_idx; i < pl->n; i++) { draw_card(s, x, y, pl->c[i]); y += PC.ch * 0.26f; }
        if (!u->down) {
            PC.dragging = false;
            /* solta na pilha com maior sobreposição */
            int best = -1;
            float bo = 0;
            float cx = u->mx - PC.dx + PC.cw / 2, cy = u->my - PC.dy + PC.ch / 2;
            for (int p = P_F0; p < NPILES; p++) {
                float px, py;
                pile_t *t = &PC.g.p[p];
                card_pos(p, MAX(0, t->n - 1), &px, &py);
                float ov = (PC.cw - kit_fabs(cx - (px + PC.cw / 2))) * (PC.ch - kit_fabs(cy - (py + PC.ch / 2)));
                if (kit_fabs(cx - (px + PC.cw / 2)) < PC.cw && kit_fabs(cy - (py + PC.ch / 2)) < PC.ch && ov > bo) { bo = (float)ov; best = p; }
            }
            if (best >= 0) move(PC.drag_pile, PC.drag_idx, best);
        }
        u->repaint = true;
    }
    /* barra de ferramentas e placar */
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 100, TOOLBAR_Y(28), 100, 28), "Novo Jogo", true)) deal();
    if (ui_toolbar_button(u, UI_ID, rect_make(u->cw - 12 - 100 - 8 - 90, TOOLBAR_Y(28), 90, 28), "Desfazer", PC.nundo > 0)) PC.g = PC.undo[--PC.nundo];
    static const char *dm[2] = {"Virar 1", "Virar 3"};
    int nd = ui_segmented(u, UI_ID, rect_make(u->cw - 12 - 100 - 8 - 90 - 8 - 160, TOOLBAR_Y(28), 160, 28), dm, 2, PC.draw3);
    if (nd != PC.draw3) { PC.draw3 = nd; deal(); }
    u64 el = (PC.won ? PC.won_at : timer_ticks()) - PC.started;
    char st[96];
    snprintf(st, sizeof st, "Pontos: %d    Tempo: %d:%02d    Lances: %d", PC.g.score, (int)(el / 60000), (int)(el / 1000 % 60), PC.g.moves);
    ui_title(u, 96, 0, 36, 360, "Paciência", st);
    if (PC.won) {
        /* cartas pulando */
        float t = (timer_ticks() - PC.won_at) / 1000.0f;
        for (int k = 0; k < 12; k++) {
            float ph = t * 1.6f + k * 0.5f;
            float bx = PC.ox + kit_fmod(k * 97 + t * 160, u->cw - PC.cw - PC.ox);
            float by = u->ch - PC.ch - kit_fabs(fsin(ph)) * (u->ch * 0.6f) / (1 + kit_fmod(ph, 6.28) * 0.2f);
            draw_card(s, bx, by, (i8)((k * 5) % 52 | UP));
        }
        rect_t r = rect_make(u->cw / 2 - 170, u->ch / 2 - 60, 340, 120);
        glass_panel(s, r.x, r.y, r.w, r.h, 18, GLASS_REGULAR);
        ui_label_center(u, rect_make(r.x, r.y + 18, r.w, 28), "Você venceu!", FONT_BOLD, 24, T.glass_text);
        char m[64];
        snprintf(m, sizeof m, "%d pontos · recorde %d", PC.g.score, PC.best);
        ui_label_center(u, rect_make(r.x, r.y + 52, r.w, 20), m, FONT_REGULAR, 14, T.glass_text2);
        if (ui_button_style(u, UI_ID, rect_make(r.x + r.w / 2 - 60, r.y + 80, 120, 28), "Jogar de Novo", BTN_FILLED, true, NULL)) deal();
        u->repaint = true;
    } else u->repaint = true;
    if ((u->mods & MOD_CMD) && u->key == 'z' && PC.nundo) { PC.g = PC.undo[--PC.nundo]; u->key_used = true; }
    if ((u->mods & MOD_CMD) && u->key == 'n') { deal(); u->key_used = true; }
}
static void pc_close(window_t *w) { PC.w = NULL; }
static void pc_launch(app_t *a) {
    if (PC.w) { wm_focus(PC.w); return; }
    PC.w = wm_create(&app_paciencia, "Paciência", -1, -1, 900, 680, WIN_RESIZABLE);
    if (!PC.w) return;
    PC.w->paint = pc_paint;
    PC.w->on_close = pc_close;
    PC.w->toolbar_h = TOOLBAR_H;
    PC.w->min_w = 640;
    PC.w->min_h = 520;
    PC.w->title_x = -1;
    if (!PC.started) deal();
}
static void pc_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(40, 150, 90), RGB(16, 90, 50));
    gfx_round_rect(s, x + 14 * u, y + 14 * u, 22 * u, 30 * u, 3 * u, RGB(250, 250, 250));
    gfx_round_rect(s, x + 26 * u, y + 20 * u, 22 * u, 30 * u, 3 * u, RGB(250, 250, 250));
    gfx_round_rect_stroke(s, x + 26 * u, y + 20 * u, 22 * u, 30 * u, 3 * u, 1, RGBA(0, 0, 0, 50));
    suit_glyph(s, x + 37 * u, y + 36 * u, 6 * u, 1, RGB(215, 30, 40));
    suit_glyph(s, x + 20 * u, y + 22 * u, 4 * u, 0, RGB(25, 25, 30));
}
app_t app_paciencia = {.name = "Paciência", .exec = "paciencia", .draw_icon = pc_icon, .launch = pc_launch};

/* ================================================================== Jogos */
static struct { window_t *w; } JG;
static const char *games[] = {"xadrez", "paciencia", "minas"};
static const char *game_desc[] = {"Desafie o computador em três níveis, ou jogue com um amigo.", "O clássico Klondike, virando uma ou três cartas.", "Descubra todas as casas sem tocar nas minas."};
static void jg_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_grad_v(s, 0, w->toolbar_h, u->cw, u->ch - w->toolbar_h, T.dark ? RGB(30, 30, 40) : RGB(240, 242, 250), T.content_bg);
    ui_label(u, 30, w->toolbar_h + 20, "Biblioteca", FONT_BOLD, 26, T.text);
    ui_label(u, 30, w->toolbar_h + 54, "Jogos instalados no mOS", FONT_REGULAR, 13, T.text2);
    int cw = 220, ch = 250, gap = 22, x0 = 30, y0 = w->toolbar_h + 90;
    int cols = MAX(1, (u->cw - 60 + gap) / (cw + gap));
    for (int i = 0; i < (int)ARRAY_LEN(games); i++) {
        app_t *a = apps_find(games[i]);
        if (!a) continue;
        rect_t r = rect_make(x0 + (i % cols) * (cw + gap), y0 + (i / cols) * (ch + gap), cw, ch);
        bool hov = ui_hit(u, r);
        gfx_shadow(s, r.x, r.y, r.w, r.h, 16, hov ? 22 : 14, hov ? 60 : 36, hov ? 6 : 3);
        gfx_round_rect(s, r.x, r.y, r.w, r.h, 16, T.dark ? RGB(48, 48, 54) : RGB(255, 255, 255));
        surface_t *ic = icon_for(a, 96);
        if (ic) gfx_blend(s, ic, r.x + (cw - 96) / 2, r.y + 22, 255);
        ui_label_center(u, rect_make(r.x, r.y + 128, r.w, 22), a->name, FONT_BOLD, 16, T.text);
        text_wrap(s, font_get(FONT_REGULAR), 12, rect_make(r.x + 18, r.y + 156, r.w - 36, 50), 17, T.text2, game_desc[i], true);
        if (ui_button_style(u, 122000 + i, rect_make(r.x + 18, r.y + ch - 44, r.w - 36, 28), "Jogar", BTN_FILLED | BTN_CAPSULE, true, NULL)) app_launch(a);
    }
}
static void jg_close(window_t *w) { JG.w = NULL; }
static void jg_launch(app_t *a) {
    if (JG.w) { wm_focus(JG.w); return; }
    JG.w = wm_create(&app_jogos, "Jogos", -1, -1, 800, 480, WIN_RESIZABLE);
    if (!JG.w) return;
    JG.w->paint = jg_paint;
    JG.w->on_close = jg_close;
    JG.w->toolbar_h = TOOLBAR_H;
}
static void jg_icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(255, 90, 120), RGB(120, 70, 230));
    gfx_round_rect(s, x + 12 * u, y + 24 * u, 40 * u, 20 * u, 10 * u, RGB(255, 255, 255));
    gfx_fill(s, (int)(x + 19 * u), (int)(y + 32 * u), (int)(8 * u), (int)(3 * u), RGB(90, 90, 110));
    gfx_fill(s, (int)(x + 21.5f * u), (int)(y + 29.5f * u), (int)(3 * u), (int)(8 * u), RGB(90, 90, 110));
    gfx_circle(s, x + 41 * u, y + 31 * u, 2.2f * u, RGB(255, 80, 100));
    gfx_circle(s, x + 45 * u, y + 36 * u, 2.2f * u, RGB(80, 140, 255));
}
app_t app_jogos = {.name = "Jogos", .exec = "jogos", .draw_icon = jg_icon, .launch = jg_launch};
