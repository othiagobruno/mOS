/* mOS — Notas Autoadesivas: notas coloridas soltas na mesa, cada uma numa janela pequena; posição, tamanho,
 * cor e texto ficam em ~/Biblioteca/Notas Autoadesivas/notas.tsv e voltam ao abrir o app. */
#include "kit.h"

extern app_t app_stickies;

#define MAXS 32
static const u32 st_bg[6] = {RGB(254, 244, 156), RGB(173, 216, 255), RGB(178, 255, 161), RGB(255, 199, 199), RGB(182, 202, 255), RGB(238, 238, 238)};
static const u32 st_bar[6] = {RGB(250, 232, 110), RGB(150, 200, 250), RGB(150, 240, 130), RGB(250, 175, 175), RGB(160, 182, 250), RGB(220, 220, 220)};
static const char *st_names[6] = {"Amarelo", "Azul", "Verde", "Rosa", "Roxo", "Cinza"};

typedef struct { bool used; window_t *w; textarea_t *ta; int color; int x, y, ww, hh; bool collapsed; } sticky_t;
static sticky_t S[MAXS];
static bool quitting, loaded;

static void save(void) {
    sbuf_t b = {0};
    sb_puts(&b, "# mOS Notas Autoadesivas 1\n");
    for (int i = 0; i < MAXS; i++) {
        sticky_t *s = &S[i];
        if (!s->used) continue;
        if (s->w) { s->x = s->w->x; s->y = s->w->y; s->ww = s->w->w; s->hh = s->w->h; }
        sb_printf(&b, "S\t%d\t%d\t%d\t%d\t%d\t", s->x, s->y, s->ww, s->hh, s->color);
        kit_esc(&b, s->ta ? textarea_text(s->ta, NULL) : "");
        sb_putc(&b, '\n');
    }
    char p[200];
    kit_save(kit_lib("Notas Autoadesivas", "notas.tsv", p, sizeof p), &b);
}

static void paint(window_t *w, ui_t *u) {
    sticky_t *s = w->data;
    gfx_fill(u->s, 0, 0, u->cw, u->ch, st_bg[s->color]);
    textarea_colors(s->ta, 1, RGB(40, 40, 30));
    textarea_paint(w, u, s->ta, rect_make(0, 0, u->cw, u->ch));
    if (textarea_dirty(s->ta, true)) save();
    /* título da janela: primeira linha */
    const char *t = textarea_text(s->ta, NULL);
    char first[64];
    int k = 0;
    while (t[k] && t[k] != '\n' && k < 60) { first[k] = t[k]; k++; }
    first[k] = 0;
    if (strcmp(w->title, first)) wm_set_title(w, first);
}

static void on_close(window_t *w) {
    sticky_t *s = w->data;
    if (!s) return;
    if (quitting) { s->x = w->x; s->y = w->y; s->ww = w->w; s->hh = w->h; s->w = NULL; return; }
    /* fechar uma nota a apaga (como no macOS, depois de perguntar; aqui a nota vai para a Lixeira como .txt) */
    int len;
    const char *t = textarea_text(s->ta, &len);
    if (len > 0) {
        char p[256];
        snprintf(p, sizeof p, "%s/Nota autoadesiva %d.txt", CUR_TRASH, (int)(timer_ticks() % 100000));
        vfs_mkdir(CUR_TRASH);
        vfs_write_file(p, t, len);
    }
    textarea_free(s->ta);
    memset(s, 0, sizeof *s);
    save();
}

static void open_window(sticky_t *s) {
    s->w = wm_create(&app_stickies, "", s->x, s->y, s->ww, s->hh, WIN_RESIZABLE | WIN_NO_MINIMIZE | WIN_NO_ZOOM);
    if (!s->w) return;
    s->w->data = s;
    s->w->paint = paint;
    s->w->on_close = on_close;
    s->w->bg = st_bg[s->color];
    s->w->title_bg = st_bar[s->color];
    s->w->min_w = 160;
    s->w->min_h = 100;
}

static sticky_t *new_sticky(int color, const char *text) {
    for (int i = 0; i < MAXS; i++) {
        sticky_t *s = &S[i];
        if (s->used) continue;
        memset(s, 0, sizeof *s);
        s->used = true;
        s->color = color;
        s->ta = textarea_new(14, 1, RGB(40, 40, 30), 10);
        textarea_set(s->ta, text, -1);
        s->x = 120 + (i % 8) * 36; s->y = 90 + (i % 8) * 30; s->ww = 260; s->hh = 220;
        return s;
    }
    return NULL;
}

static void load(void) {
    if (loaded) return;
    loaded = true;
    char p[200];
    char *buf = kit_read(kit_lib("Notas Autoadesivas", "notas.tsv", p, sizeof p), NULL);
    if (!buf) {
        new_sticky(0, "Notas Autoadesivas\n\nEscreva lembretes rápidos aqui. Eles ficam na mesa até você fechar a nota.");
        sticky_t *b = new_sticky(1, "Ideias\n\n• Use o menu Cor para trocar a cor\n• ⌘N cria uma nota");
        if (b) { b->x = 420; b->y = 140; }
        save();
        return;
    }
    char *cur = buf, *ln, *f[8];
    while ((ln = kit_line(&cur))) {
        if (ln[0] != 'S') continue;
        int n = kit_split(ln, f, 8);
        if (n < 7) continue;
        kit_unesc(f[6]);
        sticky_t *s = new_sticky(CLAMP(atoi(f[5]), 0, 5), f[6]);
        if (!s) break;
        s->x = atoi(f[1]); s->y = atoi(f[2]); s->ww = MAX(160, atoi(f[3])); s->hh = MAX(100, atoi(f[4]));
    }
    kfree(buf);
}

static void launch(app_t *a) {
    load();
    quitting = false;
    int n = 0;
    for (int i = 0; i < MAXS; i++) if (S[i].used) { n++; if (!S[i].w) open_window(&S[i]); else wm_focus(S[i].w); }
    if (!n) { sticky_t *s = new_sticky(0, ""); if (s) { open_window(s); if (s->w) textarea_focus(s->ta, true); } save(); }
}

static void on_quit(app_t *a) { save(); quitting = true; }

enum { M_NEW = 1, M_COLOR0 = 10, M_FLOAT = 20 };
static menu_item_t m_file[] = {{"Nova Nota", "⌘N", M_NEW}};
static menu_item_t m_color[] = {{"Amarelo", "⌘1", M_COLOR0}, {"Azul", "⌘2", M_COLOR0 + 1}, {"Verde", "⌘3", M_COLOR0 + 2},
                                {"Rosa", "⌘4", M_COLOR0 + 3}, {"Roxo", "⌘5", M_COLOR0 + 4}, {"Cinza", "⌘6", M_COLOR0 + 5}};
static menu_t menus[] = {{"Arquivo", m_file, 1}, {"Cor", m_color, 6}};

static void on_menu(app_t *a, int act) {
    if (act == M_NEW) {
        sticky_t *s = new_sticky(0, "");
        if (s) { open_window(s); textarea_focus(s->ta, true); save(); }
    } else if (act >= M_COLOR0 && act < M_COLOR0 + 6) {
        window_t *w = wm_focused();
        if (w && w->app == &app_stickies && w->data) {
            sticky_t *s = w->data;
            s->color = act - M_COLOR0;
            w->bg = st_bg[s->color];
            w->title_bg = st_bar[s->color];
            wm_invalidate(w);
            save();
        }
    }
    (void)st_names;
}

static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    gfx_shadow(s, (int)(x + 10 * u), (int)(y + 14 * u), (int)(40 * u), (int)(40 * u), 2 * u, 3 * u, 70, (int)u);
    gfx_round_rect(s, x + 18 * u, y + 8 * u, 38 * u, 38 * u, 2 * u, RGB(173, 216, 255));
    gfx_round_rect(s, x + 8 * u, y + 16 * u, 40 * u, 40 * u, 2 * u, RGB(254, 238, 120));
    gfx_fill(s, (int)(x + 8 * u), (int)(y + 16 * u), (int)(40 * u), (int)(6 * u), RGB(246, 222, 90));
    for (int i = 0; i < 3; i++) gfx_fill(s, (int)(x + 13 * u), (int)(y + (29 + i * 7) * u), (int)((i == 2 ? 18 : 30) * u), MAX(1, (int)(1.4f * u)), RGBA(120, 100, 20, 140));
}

app_t app_stickies = {.name = "Notas Autoadesivas", .exec = "stickies", .draw_icon = icon, .launch = launch, .on_quit = on_quit,
                      .on_menu = on_menu, .menus = menus, .nmenus = 2};
