/* Diálogo nativo do mOS quando um app pede gravação de tela ou acessibilidade.
 * O libsystem grava pending em decisions.txt (tcc.c) e não desenha o prompt.
 * Este arquivo é a Interface: lê o pending, mostra Negar/Permitir e grava a
 * mesma linha que tcc_decide / cx_tcc_decide gravaria. O símbolo cx_tcc_decide
 * fica no libsystem; o window server não liga essa biblioteca.
 */
#include "ui.h"
#include "vfs.h"

extern bool login_active(void);
extern bool setup_active(void);

#include "kernel/ui/tcc_prompt_core.inc"

static tcc_prompt_ui Ui = {.hover = -1, .pressed = -1};
static tcc_prompt_rec Recs[TCC_PROMPT_CAP];
static char FileBuf[32768];
static char OutBuf[32768];
static u64 OpenedAt, LastPoll, NextOpenOk;
static int LastBytes = -1;
static u32 LastHash;

typedef struct {
    rect_t panel, deny, allow, icon, title, message;
} tcc_lay;

static u32 tcc_hash(const char *s, int n) {
    u32 h = 2166136261u;
    for (int i = 0; i < n; i++) h = (h ^ (u8)s[i]) * 16777619u;
    return h;
}

static int tcc_read(char *dst, int cap, int *truncated) {
    vnode_t *v = vfs_lookup(TCC_PROMPT_FILE);
    int n;
    *truncated = 0;
    if (!v || v->dir || !v->size) {
        dst[0] = 0;
        return 0;
    }
    if (!v->data) return -1;
    if (v->size >= (usize)cap) {
        *truncated = 1;
        dst[0] = 0;
        return 0;
    }
    n = (int)v->size;
    memcpy(dst, v->data, (usize)n);
    dst[n] = 0;
    return n;
}

/* Mesmo efeito de cx_tcc_decide para um bundle e um serviço: regrava o arquivo
 * inteiro na gramática de tcc_save (bundle serviço granted|denied|pending). */
static int tcc_decider_file(const char *bundle, const char *service, int granted, void *ctx) {
    int truncated = 0, n, m;
    (void)ctx;
    if (tcc_read(FileBuf, (int)sizeof FileBuf, &truncated) < 0 || truncated) return -1;
    n = tcc_prompt_parse(FileBuf, Recs, TCC_PROMPT_CAP);
    if (tcc_prompt_set(Recs, &n, TCC_PROMPT_CAP, bundle, service, granted) != 0) return -1;
    m = tcc_prompt_format(OutBuf, (int)sizeof OutBuf, Recs, n);
    if (m < 0) return -1;
    if (!vfs_write_file(TCC_PROMPT_FILE, OutBuf, (usize)m)) return -1;
    memcpy(FileBuf, OutBuf, (usize)m + 1);
    LastBytes = m;
    LastHash = tcc_hash(FileBuf, m);
    return 0;
}

static tcc_lay tcc_layout(int sw, int sh) {
    tcc_lay L;
    int w = MIN(400, sw - 32);
    int text_w, head_w, title_h, msg_h, head;
    font_t *fb = font_get(FONT_SEMIBOLD);
    font_t *fr = font_get(FONT_REGULAR);
    memset(&L, 0, sizeof L);
    if (w < 220) w = 220;
    text_w = w - 40;
    head_w = text_w - 48;
    title_h = text_wrap(NULL, fb, 15, rect_make(0, 0, head_w, 0), 20, 0, Ui.view.title, false);
    msg_h = text_wrap(NULL, fr, 13, rect_make(0, 0, text_w, 0), 18, 0, Ui.view.message, false);
    if (title_h < 20) title_h = 20;
    head = MAX(36, 16 + title_h);
    L.panel.w = w;
    L.panel.h = 20 + head + 14 + msg_h + 16 + 32 + 18;
    L.panel.x = (sw - w) / 2;
    L.panel.y = (sh - L.panel.h) / 2 - sh / 20;
    if (L.panel.y < 36) L.panel.y = 36;
    L.icon = rect_make(L.panel.x + 20, L.panel.y + 18, 36, 36);
    L.title = rect_make(L.icon.x + 48, L.icon.y + 16, head_w, title_h + 4);
    L.message = rect_make(L.panel.x + 20, L.icon.y + head + 12, text_w, msg_h + 4);
    {
        int gap = 10, pad = 20, bw = (w - 2 * pad - gap) / 2;
        int y = L.panel.y + L.panel.h - 18 - 32;
        L.deny = rect_make(L.panel.x + pad, y, bw, 32);
        L.allow = rect_make(L.panel.x + pad + bw + gap, y, bw, 32);
    }
    return L;
}

static void tcc_glyph_screen(surface_t *s, float x, float y, float sz, u32 c) {
    float m = sz * 0.14f;
    gfx_round_rect_stroke(s, x + m, y + sz * 0.08f, sz - 2 * m, sz * 0.56f, 3, 1.7f, c);
    gfx_line(s, x + sz * 0.38f, y + sz * 0.08f + sz * 0.56f, x + sz * 0.62f, y + sz * 0.84f, 1.7f, c);
    gfx_line(s, x + sz * 0.28f, y + sz * 0.84f, x + sz * 0.72f, y + sz * 0.84f, 1.7f, c);
}

static void tcc_glyph_ax(surface_t *s, float x, float y, float sz, u32 c) {
    float xy[6];
    gfx_round_rect_stroke(s, x + sz * 0.06f, y + sz * 0.18f, sz * 0.58f, sz * 0.40f, sz * 0.14f, 1.7f, c);
    xy[0] = x + sz * 0.40f;
    xy[1] = y + sz * 0.34f;
    xy[2] = x + sz * 0.90f;
    xy[3] = y + sz * 0.56f;
    xy[4] = x + sz * 0.56f;
    xy[5] = y + sz * 0.88f;
    gfx_polygon(s, xy, 3, c);
}

static int tcc_hit(tcc_lay L, int x, int y) {
    if (rect_contains(L.deny, x, y)) return 0;
    if (rect_contains(L.allow, x, y)) return 1;
    return -1;
}

static void tcc_poll(int force) {
    int truncated = 0, n, ch, bytes;
    u32 hash;
    u64 now = timer_ticks();
    if (login_active() || setup_active()) {
        if (Ui.open) {
            Ui.open = 0;
            Ui.hover = Ui.pressed = -1;
            wm_damage_all();
        }
        return;
    }
    if (!force && now - LastPoll < 400) return;
    LastPoll = now;
    bytes = tcc_read(FileBuf, (int)sizeof FileBuf, &truncated);
    if (bytes < 0 || truncated) {
        static int warned;
        if (!warned) {
            warned = 1;
            kprintf("tcc: não reli decisions.txt; o diálogo não regrava o arquivo\n");
        }
        if (Ui.open) {
            Ui.open = 0;
            Ui.hover = Ui.pressed = -1;
            wm_damage_all();
        }
        return;
    }
    hash = tcc_hash(FileBuf, bytes);
    if (!force && bytes == LastBytes && hash == LastHash) return;
    LastBytes = bytes;
    LastHash = hash;
    n = tcc_prompt_parse(FileBuf, Recs, TCC_PROMPT_CAP);
    ch = tcc_prompt_sync(&Ui, Recs, n);
    if (ch == TCC_PROMPT_OPENED && now < NextOpenOk) {
        Ui.open = 0;
        Ui.hover = Ui.pressed = -1;
        LastBytes = -1;
        return;
    }
    if (ch == TCC_PROMPT_OPENED) {
        OpenedAt = now;
        sound_alert();
        wm_damage_all();
    } else if (ch == TCC_PROMPT_CLOSED) {
        wm_damage_all();
    }
}

static int tcc_choose(int button) {
    int rc = tcc_prompt_choose(&Ui, button, tcc_decider_file, 0);
    if (rc != 0) return rc;
    NextOpenOk = timer_ticks() + 350;
    tcc_poll(1);
    wm_damage_all();
    return 0;
}

void tcc_prompt_tick(void) { tcc_poll(0); }

void tcc_prompt_draw(surface_t *s, int sw, int sh) {
    tcc_lay L;
    font_t *fb, *fr, *fm;
    u32 deny_fill, allow_fill;
    if (!Ui.open || sw < 80 || sh < 80) return;
    L = tcc_layout(sw, sh);
    fb = font_get(FONT_SEMIBOLD);
    fr = font_get(FONT_REGULAR);
    fm = font_get(FONT_MEDIUM);
    gfx_shadow(s, L.panel.x, L.panel.y, L.panel.w, L.panel.h, 22, 28, T.dark ? 150 : 70, 12);
    gfx_glass(s, L.panel.x, L.panel.y, L.panel.w, L.panel.h, 22, &G[GLASS_THICK]);
    gfx_round_rect_stroke(s, L.panel.x - 0.5f, L.panel.y - 0.5f, L.panel.w + 1, L.panel.h + 1, 22.5f, 1, T.glass_hairline);
    gfx_round_rect(s, L.icon.x, L.icon.y, L.icon.w, L.icon.h, 10, T.dark ? RGBA(255, 255, 255, 28) : RGBA(0, 0, 0, 16));
    if (Ui.view.kind == TCC_PROMPT_KIND_SCREEN)
        tcc_glyph_screen(s, L.icon.x, L.icon.y, 36, T.accent);
    else if (Ui.view.kind == TCC_PROMPT_KIND_AX)
        tcc_glyph_ax(s, L.icon.x, L.icon.y, 36, T.accent);
    text_draw(s, fm, 11, L.icon.x + 48, L.icon.y + 14, T.glass_text2, Ui.view.eyebrow);
    text_wrap(s, fb, 15, L.title, 20, T.glass_text, Ui.view.title, true);
    text_wrap(s, fr, 13, L.message, 18, T.glass_text, Ui.view.message, true);
    deny_fill = T.dark ? RGBA(255, 255, 255, Ui.pressed == 0 ? 64 : Ui.hover == 0 ? 48 : 36)
                       : (Ui.pressed == 0 ? RGB(206, 206, 212) : Ui.hover == 0 ? RGB(216, 216, 222) : RGB(226, 226, 230));
    allow_fill = Ui.pressed == 1 ? color_lighten(T.accent, -25) : Ui.hover == 1 ? color_lighten(T.accent, 12) : T.accent;
    gfx_round_rect(s, L.deny.x, L.deny.y, L.deny.w, L.deny.h, L.deny.h / 2.0f, deny_fill);
    gfx_round_rect(s, L.allow.x, L.allow.y, L.allow.w, L.allow.h, L.allow.h / 2.0f, allow_fill);
    text_draw_center(s, fr, 14, L.deny, T.glass_text, Ui.view.deny);
    text_draw_center(s, fr, 14, L.allow, RGB(255, 255, 255), Ui.view.allow);
}

bool tcc_prompt_mouse(int x, int y, int sw, int sh, bool press, bool release) {
    tcc_lay L;
    int h;
    if (!Ui.open) return false;
    L = tcc_layout(sw, sh);
    h = tcc_hit(L, x, y);
    if (h != Ui.hover) {
        Ui.hover = h;
        wm_damage_all();
    }
    if (press) Ui.pressed = h;
    if (release) {
        int p = Ui.pressed;
        Ui.pressed = -1;
        if (p >= 0 && p == h) tcc_choose(p);
        else wm_damage_all();
    }
    return true;
}

bool tcc_prompt_key(int key) {
    int b;
    if (!Ui.open) return false;
    if (timer_ticks() - OpenedAt < 250) return true;
    b = tcc_prompt_button_for_key(&Ui, key);
    if (b >= 0) tcc_choose(b);
    return true;
}
