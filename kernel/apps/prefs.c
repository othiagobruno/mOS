/* mOS — Ajustes do Sistema
 *
 * Janela redimensionável com barra lateral (busca, cartão da conta, categorias com ícones coloridos)
 * e conteúdo em grupos arredondados de linhas: rótulo à esquerda, controle à direita.
 * Tudo aqui altera ou mostra o estado real do sistema (rede, Wi-Fi, Bluetooth, som, tema...). */
#include "apps.h"
#include "net.h"
#include "wifi.h"
#include "account.h"
#include "storage.h"
#include "proc.h"
#include "crypto.h"

extern int g_kbd_layout;
extern int g_tz_offset_min;
extern bool g_rtc_is_utc;
extern bool g_notify_dnd, g_notify_sound;
extern int g_notify_count;
extern void desktop_wallpaper_thumb(surface_t *dst, int style);
extern void login_logout(void);
extern int usb_device_count(void);
extern const char *usb_device_desc(int i);
extern int bt_device_count(void);
extern const char *bt_device_name(int i);
extern void bt_scan(void);
extern u32 x509_trust_count(void);

/* pilha Bluetooth LE (kernel/bt/ble.c) */
extern bool bt_le_scan(bool on);
extern int bt_le_device_count(void);
extern bool bt_le_device(int i, char *name, usize ncap, char *addr, int *rssi, bool *is_hid);
extern bool bt_le_connect(int i);
extern void bt_le_forget(const char *addr);
extern const char *bt_le_status(void);
extern bool bt_classic_scan(bool on);
extern int bt_classic_count(void);
extern bool bt_classic_device(int i, char *name, usize ncap, char *addr, u32 *cod, int *rssi);
extern bool bt_classic_connect(int i);
extern void bt_classic_forget(const char *addr);
extern const char *bt_classic_status(void);
extern bool bt_a2dp_active(void);
extern void bt_a2dp_test_tone(void);

/* ------------------------------------------------------------------ painéis */
enum {
    P_USERS, P_WIFI, P_BT, P_REDE, P_NOTIF, P_SOM, P_GERAL, P_APAR, P_MESA, P_MONITOR, P_WALL, P_PRIV, P_TECLADO,
    /* subpáginas de Geral */
    P_SOBRE, P_UPDATE, P_STORAGE, P_DATA, P_IDIOMA, P_STARTUP,
    P_COUNT
};
static const char *pane_names[P_COUNT] = {
    "Usuários e Grupos", "Wi-Fi", "Bluetooth", "Rede", "Notificações", "Som", "Geral", "Aparência", "Mesa e Dock",
    "Monitores", "Papel de Parede", "Privacidade e Segurança", "Teclado",
    "Sobre", "Atualização de Software", "Armazenamento", "Data e Hora", "Idioma e Região", "Disco de Inicialização",
};
static const char *pane_kw[P_COUNT] = {
    "conta senha usuario login administrador", "wifi rede sem fio wpa senha internet", "bluetooth fone teclado mouse parear",
    "rede ethernet ip dhcp dns roteador internet cabo", "notificacoes alertas nao perturbe foco", "som volume audio alto-falante saida microfone entrada",
    "geral sistema", "aparencia tema escuro claro cor destaque", "mesa dock tamanho ampliacao", "monitores tela resolucao display",
    "papel de parede fundo imagem dunas seda", "privacidade seguranca certificados https tls senha", "teclado layout abnt2 atalhos",
    "sobre mac chip memoria processador versao serie", "atualizacao software versao", "armazenamento disco espaco memoria",
    "data hora fuso relogio", "idioma regiao portugues", "disco inicializacao instalar boot",
};
static const u32 pane_colors[P_COUNT] = {
    RGB(142, 142, 147), RGB(10, 132, 255), RGB(10, 132, 255), RGB(10, 132, 255), RGB(255, 59, 48), RGB(255, 45, 85),
    RGB(142, 142, 147), RGB(28, 28, 30), RGB(28, 28, 30), RGB(10, 132, 255), RGB(50, 173, 230), RGB(10, 132, 255),
    RGB(142, 142, 147),
    RGB(142, 142, 147), RGB(142, 142, 147), RGB(142, 142, 147), RGB(10, 132, 255), RGB(10, 132, 255), RGB(142, 142, 147),
};
static int pane_parent(int p) { return p >= P_SOBRE ? P_GERAL : p; }

/* ------------------------------------------------------------------ símbolos (brancos, sobre o quadrado colorido) */
static void g_wifi(surface_t *s, int x, int y, int z, u32 c) { glyph_wifi(s, x, y - z / 10, z, c, 0); }
static void g_speaker(surface_t *s, int x, int y, int z, u32 c) { glyph_speaker(s, x + z / 10, y, z, c, 3); }
static void g_gear(surface_t *s, int x, int y, int z, u32 c) { glyph_gear(s, x + z / 2.0f, y + z / 2.0f, z / 2.0f, c, RGB(142, 142, 147)); }
static void g_bell(surface_t *s, int x, int y, int z, u32 c) {
    float cx = x + z / 2.0f;
    float pts[] = {cx - z * 0.32f, y + z * 0.72f, cx - z * 0.28f, y + z * 0.38f, cx - z * 0.14f, y + z * 0.16f, cx, y + z * 0.12f,
                   cx + z * 0.14f, y + z * 0.16f, cx + z * 0.28f, y + z * 0.38f, cx + z * 0.32f, y + z * 0.72f};
    gfx_polygon(s, pts, 7, c);
    gfx_round_rect(s, cx - z * 0.4f, y + z * 0.68f, z * 0.8f, z * 0.1f, z * 0.05f, c);
    gfx_circle(s, cx, y + z * 0.86f, z * 0.09f, c);
}
static void g_appearance(surface_t *s, int x, int y, int z, u32 c) {
    float cx = x + z / 2.0f, cy = y + z / 2.0f, r = z * 0.38f;
    gfx_circle_stroke(s, cx, cy, r, MAX(1.4f, z * 0.08f), c);
    float pts[34];
    for (int i = 0; i <= 16; i++) {           /* metade direita preenchida */
        float a = -PI_F / 2 + PI_F * i / 16;
        pts[i * 2] = cx + fcos(a) * r;
        pts[i * 2 + 1] = cy + fsin(a) * r;
    }
    gfx_polygon(s, pts, 17, c);
}
static void g_dock(surface_t *s, int x, int y, int z, u32 c) {
    gfx_round_rect_stroke(s, x + z * 0.1f, y + z * 0.18f, z * 0.8f, z * 0.64f, z * 0.1f, MAX(1.3f, z * 0.07f), c);
    gfx_round_rect(s, x + z * 0.22f, y + z * 0.6f, z * 0.56f, z * 0.12f, z * 0.05f, c);
}
static void g_monitor(surface_t *s, int x, int y, int z, u32 c) {
    gfx_round_rect_stroke(s, x + z * 0.08f, y + z * 0.16f, z * 0.84f, z * 0.54f, z * 0.06f, MAX(1.3f, z * 0.07f), c);
    gfx_fill(s, x + (int)(z * 0.46f), y + (int)(z * 0.7f), MAX(2, z / 12), (int)(z * 0.1f), c);
    gfx_round_rect(s, x + z * 0.3f, y + z * 0.8f, z * 0.4f, z * 0.07f, z * 0.03f, c);
}
static void g_wallpaper(surface_t *s, int x, int y, int z, u32 c) {
    float cx = x + z / 2.0f, cy = y + z / 2.0f;
    for (int i = 0; i < 6; i++) {
        float a = i * PI_F / 3;
        gfx_circle(s, cx + fcos(a) * z * 0.2f, cy + fsin(a) * z * 0.2f, z * 0.15f, WITH_ALPHA(c, 210));
    }
    gfx_circle(s, cx, cy, z * 0.1f, RGB(50, 173, 230));
}
static void g_hand(surface_t *s, int x, int y, int z, u32 c) {
    /* escudo */
    float cx = x + z / 2.0f;
    float pts[] = {cx, y + z * 0.08f, cx + z * 0.36f, y + z * 0.22f, cx + z * 0.32f, y + z * 0.58f, cx, y + z * 0.92f,
                   cx - z * 0.32f, y + z * 0.58f, cx - z * 0.36f, y + z * 0.22f};
    gfx_polygon(s, pts, 6, c);
    gfx_line(s, cx - z * 0.14f, y + z * 0.5f, cx - z * 0.03f, y + z * 0.62f, MAX(1.5f, z * 0.09f), RGB(10, 132, 255));
    gfx_line(s, cx - z * 0.03f, y + z * 0.62f, cx + z * 0.17f, y + z * 0.36f, MAX(1.5f, z * 0.09f), RGB(10, 132, 255));
}
static void g_people(surface_t *s, int x, int y, int z, u32 c) {
    float cx = x + z / 2.0f;
    gfx_circle(s, cx + z * 0.16f, y + z * 0.34f, z * 0.13f, WITH_ALPHA(c, 170));
    gfx_round_rect(s, cx - z * 0.02f, y + z * 0.54f, z * 0.38f, z * 0.26f, z * 0.13f, WITH_ALPHA(c, 170));
    gfx_circle(s, cx - z * 0.1f, y + z * 0.34f, z * 0.16f, c);
    gfx_round_rect(s, cx - z * 0.34f, y + z * 0.56f, z * 0.48f, z * 0.3f, z * 0.15f, c);
}
static void g_keyboard(surface_t *s, int x, int y, int z, u32 c) {
    gfx_round_rect_stroke(s, x + z * 0.06f, y + z * 0.24f, z * 0.88f, z * 0.52f, z * 0.08f, MAX(1.3f, z * 0.07f), c);
    for (int r = 0; r < 2; r++)
        for (int k = 0; k < 5; k++) gfx_fill(s, x + (int)(z * (0.18f + k * 0.14f)), y + (int)(z * (0.36f + r * 0.13f)), MAX(1, z / 14), MAX(1, z / 14), c);
    gfx_fill(s, x + (int)(z * 0.3f), y + (int)(z * 0.62f), (int)(z * 0.4f), MAX(1, z / 14), c);
}
static void g_info(surface_t *s, int x, int y, int z, u32 c) {
    gfx_circle_stroke(s, x + z / 2.0f, y + z / 2.0f, z * 0.4f, MAX(1.3f, z * 0.07f), c);
    gfx_circle(s, x + z / 2.0f, y + z * 0.32f, z * 0.06f, c);
    gfx_line(s, x + z / 2.0f, y + z * 0.45f, x + z / 2.0f, y + z * 0.7f, MAX(1.5f, z * 0.09f), c);
}
static void g_update(surface_t *s, int x, int y, int z, u32 c) { g_gear(s, x, y, z, c); }
static void g_disk(surface_t *s, int x, int y, int z, u32 c) {
    gfx_round_rect_stroke(s, x + z * 0.1f, y + z * 0.28f, z * 0.8f, z * 0.44f, z * 0.08f, MAX(1.3f, z * 0.07f), c);
    gfx_circle(s, x + z * 0.72f, y + z * 0.5f, z * 0.06f, c);
}
static void g_clock(surface_t *s, int x, int y, int z, u32 c) { glyph_clock(s, x, y, z, c); }
static void g_globe(surface_t *s, int x, int y, int z, u32 c) { glyph_globe(s, x + 1, y + 1, z - 2, c); }
static void g_lock(surface_t *s, int x, int y, int z, u32 c) {
    gfx_ring_arc(s, x + z / 2.0f, y + z * 0.42f, z * 0.2f, MAX(1.4f, z * 0.1f), PI_F, 2 * PI_F, c);
    gfx_line(s, x + z * 0.3f, y + z * 0.42f, x + z * 0.3f, y + z * 0.5f, MAX(1.4f, z * 0.1f), c);
    gfx_line(s, x + z * 0.7f, y + z * 0.42f, x + z * 0.7f, y + z * 0.5f, MAX(1.4f, z * 0.1f), c);
    gfx_round_rect(s, x + z * 0.2f, y + z * 0.48f, z * 0.6f, z * 0.42f, z * 0.08f, c);
}

static const glyph_fn pane_glyph[P_COUNT] = {
    g_people, g_wifi, glyph_bluetooth, g_globe, g_bell, g_speaker, g_gear, g_appearance, g_dock, g_monitor, g_wallpaper,
    g_hand, g_keyboard, g_info, g_update, g_disk, g_clock, g_globe, g_disk,
};

/* quadrado arredondado colorido com o símbolo em branco (como os ícones dos Ajustes) */
static void pane_icon(surface_t *s, int p, int x, int y, int sz) {
    u32 c = pane_colors[p];
    gfx_round_rect_grad(s, x, y, sz, sz, sz * 0.24f, color_lighten(c, 28), c);
    int g = (int)(sz * 0.68f + 0.5f);
    pane_glyph[p](s, x + (sz - g) / 2, y + (sz - g) / 2, g, RGB(255, 255, 255));
}

/* ------------------------------------------------------------------ estado */
static window_t *pw;
static int pane = P_WIFI;
static int hist[32], nhist, hpos;
static int scroll[P_COUNT], content_h[P_COUNT];
static int sb_scroll, sb_content_h;
static char sb_query[64];

static void go(int p, bool push) {
    if (p < 0 || p >= P_COUNT) return;
    if (push) {
        if (nhist && hist[hpos] == p) { pane = p; return; }
        nhist = hpos + 1;
        if (nhist >= 32) { memmove(hist, hist + 1, sizeof(int) * 31); nhist = 31; }
        hist[nhist++] = p;
        hpos = nhist - 1;
    }
    pane = p;
    if (pw) wm_invalidate(pw);
}

static void redraw_all(void) {
    extern window_t *wm_nth(int);
    for (int i = 0;; i++) { window_t *w = wm_nth(i); if (!w) break; w->needs_paint = true; }
    wm_damage_all();
}

static char lc(char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static bool icontains(const char *h, const char *n) {
    if (!*n) return true;
    for (; *h; h++) {
        const char *a = h, *b = n;
        while (*a && *b && lc(*a) == lc(*b)) { a++; b++; }
        if (!*b) return true;
    }
    return false;
}

/* ------------------------------------------------------------------ construtor de formulários */
static ui_t *U;
static int X0, W0, Y;
#define RH 40
#define F(w) font_get(w)

static void section(const char *t) {
    Y += 6;
    text_draw(U->s, F(FONT_SEMIBOLD), 13, X0 + 6, Y + 13, T.text, t);
    Y += 24;
}

/* título de seção com um botão à direita (ex.: "Procurar") */
static bool section_btn(const char *t, int id, const char *btn, bool busy) {
    Y += 6;
    text_draw(U->s, F(FONT_SEMIBOLD), 13, X0 + 6, Y + 13, T.text, t);
    int bw = text_width(F(FONT_REGULAR), 13, btn) + 26;
    bool c = false;
    if (busy) ui_spinner(U, X0 + W0 - 12, Y + 9, 7);
    else c = ui_button(U, id, rect_make(X0 + W0 - bw, Y - 2, bw, 24), btn, false);
    Y += 28;
    return c;
}

static rect_t group(int n) {
    rect_t g = rect_make(X0, Y, W0, 12 + n * RH);
    ui_group_box(U, g);
    Y += g.h + 16;
    return g;
}

static rect_t row(rect_t g, int i, const char *label) { return ui_form_row(U, g, i, RH, 0, label); }

static void footnote(const char *t, u32 col) {
    Y -= 8;
    int h = text_wrap(U->s, F(FONT_REGULAR), 11.5f, rect_make(X0 + 8, Y, W0 - 16, 400), 16, col ? col : T.text2, t, true);
    Y += h + 22;
}

static bool row_switch(int id, rect_t r, bool *v, bool enabled) {
    return ui_switch_ex(U, id, r.x + r.w - 46, r.y + (RH - 20) / 2, 20, v, enabled);
}

static bool row_button(int id, rect_t r, const char *label, bool primary) {
    int bw = text_width(F(FONT_REGULAR), 13, label) + 28;
    return ui_button(U, id, rect_make(r.x + r.w - bw, r.y + (RH - 26) / 2, bw, 26), label, primary);
}

static void row_value(rect_t r, const char *v) {
    int bl = r.y + RH / 2 + 5;
    int tw = text_width(F(FONT_REGULAR), 14, v);
    text_draw_ellipsis(U->s, F(FONT_REGULAR), 14, MAX(r.x, r.x + r.w - tw), bl, r.w, T.text2, v);
}

/* linha com ícone e texto livre (substitui o rótulo padrão); devolve true se clicada */
static bool icon_row(int id, rect_t g, int i, void (*icon)(int x, int y, void *ctx), void *ctx,
                     const char *title, const char *sub, bool chevron) {
    int y = g.y + 6 + i * RH;
    rect_t r = rect_make(g.x + 4, y, g.w - 8, RH);
    if (i > 0) gfx_fill(U->s, g.x + 54, y, g.w - 74, 1, T.separator);
    bool hit = chevron && ui_hit(U, r);
    if (hit && U->down) gfx_round_rect(U->s, r.x, r.y + 1, r.w, r.h - 2, 8, T.dark ? RGBA(255, 255, 255, 18) : RGBA(0, 0, 0, 10));
    if (icon) icon(g.x + 16, y + (RH - 26) / 2, ctx);
    int tx = g.x + (icon ? 54 : 20);
    if (sub && sub[0]) {
        text_draw_ellipsis(U->s, F(FONT_REGULAR), 14, tx, y + 18, g.w - (tx - g.x) - 120, T.text, title);
        text_draw_ellipsis(U->s, F(FONT_REGULAR), 11.5f, tx, y + 33, g.w - (tx - g.x) - 120, T.text2, sub);
    } else {
        text_draw_ellipsis(U->s, F(FONT_REGULAR), 14, tx, y + RH / 2 + 5, g.w - (tx - g.x) - 120, T.text, title);
    }
    if (chevron) glyph_chevron(U->s, g.x + g.w - 26, y + RH / 2 - 6, 12, T.text3, 0);
    return chevron && ui_clicked(U, id, r);
}

static void pane_icon_cb(int x, int y, void *ctx) { pane_icon(U->s, (int)(uptr)ctx, x, y, 26); }

/* cabeçalho do painel: ícone grande, título, descrição e (opcional) chave liga/desliga */
static bool hero(int p, const char *desc, bool *tog, int togid) {
    font_t *fr = F(FONT_REGULAR);
    int tw = W0 - 96 - (tog ? 70 : 16);
    int dh = text_wrap(NULL, fr, 12, rect_make(0, 0, tw, 400), 16, 0, desc, false);
    int gh = MAX(84, 44 + dh + 16);
    rect_t g = rect_make(X0, Y, W0, gh);
    ui_group_box(U, g);
    pane_icon(U->s, p, g.x + 18, g.y + (gh - 56) / 2, 56);
    int ty = g.y + (gh - (24 + dh)) / 2;
    text_draw(U->s, F(FONT_BOLD), 16, g.x + 90, ty + 15, T.text, pane_names[p]);
    text_wrap(U->s, fr, 12, rect_make(g.x + 90, ty + 22, tw, 400), 16, T.text2, desc, true);
    bool c = false;
    if (tog) c = ui_switch_ex(U, togid, g.x + g.w - 64, g.y + gh / 2 - 11, 22, tog, true);
    Y += gh + 20;
    return c;
}

static void fmt_bytes(u64 b, char *o, usize n) {
    if (b >= (1ull << 30)) snprintf(o, n, "%llu,%llu GB", b >> 30, ((b % (1ull << 30)) * 10) >> 30);
    else if (b >= (1ull << 20)) snprintf(o, n, "%llu MB", b >> 20);
    else snprintf(o, n, "%llu KB", b >> 10);
}

/* barra de uso (armazenamento/memória) */
static void usage_bar(rect_t r, float frac, u32 col) {
    gfx_round_rect(U->s, r.x, r.y, r.w, r.h, r.h / 2.0f, T.dark ? RGB(60, 60, 64) : RGB(222, 222, 226));
    int w = (int)(r.w * CLAMP(frac, 0.0f, 1.0f));
    if (w > r.h) gfx_round_rect(U->s, r.x, r.y, w, r.h, r.h / 2.0f, col);
}

/* ------------------------------------------------------------------ campo de senha */
static bool secure_field(ui_t *u, int id, rect_t r, char *buf, int cap, const char *ph) {
    bool hover = ui_hit(u, r);
    if (hover) u->cursor = CURSOR_IBEAM;
    if (u->pressed) { if (hover) { u->focus = id; u->caret_blink = u->now; } else if (u->focus == id) u->focus = 0; }
    bool focused = u->focus == id, enter = false;
    int len = (int)strlen(buf);
    if (focused && u->key) {
        if (u->key == KEY_BACKSPACE) { if (len) buf[utf8_prev(buf, len)] = 0; }
        else if (u->key == KEY_ENTER) enter = true;
        else if (u->text[0] && !(u->mods & (MOD_CMD | MOD_CTRL)) && len + (int)strlen(u->text) < cap) strcat(buf, u->text);
        u->key_used = true;
        u->caret_blink = u->now;
    }
    if (focused) ui_focus_ring(u, r, 6);
    gfx_round_rect(u->s, r.x, r.y, r.w, r.h, 6, T.field_border);
    gfx_round_rect(u->s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, 5, T.field_bg);
    int n = 0;
    for (const char *p = buf; *p; n++) utf8_decode(&p);
    if (!n) text_draw(u->s, F(FONT_REGULAR), FONT_UI, r.x + 8, r.y + r.h / 2 + 5, T.text3, ph);
    for (int i = 0; i < n && i < 30; i++) gfx_circle(u->s, r.x + 12 + i * 10, r.y + r.h / 2.0f, 3, T.text);
    if (focused && ((u->now - u->caret_blink) / 530) % 2 == 0) gfx_fill(u->s, r.x + 8 + n * 10, r.y + 5, 1, r.h - 10, T.text);
    if (focused) u->repaint = true;
    return enter;
}

/* ================================================================== Wi-Fi */
static int wifi_bars(int rssi) { return rssi > -58 ? 0 : rssi > -68 ? 2 : 1; }
static void wifi_icon_cb(int x, int y, void *ctx) {
    int rssi = (int)(uptr)ctx;
    glyph_wifi(U->s, x + 2, y + 1, 22, T.text, wifi_bars(rssi));
}

static void paint_wifi(void) {
    static wifi_network_t nets[16];
    static char sel_ssid[33], pass[64];
    static u64 last_scan;
    wifi_status_t st;
    memset(&st, 0, sizeof st);
    wifi_status(&st);
    bool on = st.state != WIFI_OFF;
    if (hero(P_WIFI, "Conecte-se a redes sem fio. O mOS suporta redes abertas e WPA2 Pessoal.", st.present ? &on : NULL, UI_ID))
        wifi_set_enabled(on);
    if (!st.present) {
        rect_t g = group(1);
        row_value(row(g, 0, "Placa Wi-Fi"), st.driver[0] ? st.driver : "Nenhuma placa Wi-Fi");
        footnote("Nenhum adaptador Wi-Fi compatível foi encontrado. Use um cabo de rede ou um adaptador suportado.", 0);
        return;
    }
    if (!on) { footnote("O Wi-Fi está desligado.", 0); return; }
    bool connected = st.state == WIFI_CONNECTED;
    bool working = st.state >= WIFI_AUTHENTICATING && st.state <= WIFI_HANDSHAKE;
    if (connected || working) {
        rect_t g = group(connected ? 2 : 1);
        static const char *phase[] = {"", "", "", "Autenticando…", "Associando…", "Trocando chaves (WPA2)…", "Conectado", ""};
        icon_row(UI_ID, g, 0, wifi_icon_cb, (void *)(uptr)st.rssi, st.ssid, phase[st.state], false);
        rect_t r0 = rect_make(g.x + g.w / 2, g.y + 6, g.w / 2 - 20, RH);
        if (working) ui_spinner(U, r0.x + r0.w - 10, r0.y + RH / 2, 8);
        else if (row_button(UI_ID, r0, "Desconectar", false)) wifi_disconnect();
        if (connected) {
            net_info_t *ni = net_info();
            char ip[20], v[64];
            ip_to_str(ni->ip, ip);
            snprintf(v, sizeof v, "%s · canal %d · %d dBm", ni->configured ? ip : "obtendo endereço…", st.channel, st.rssi);
            row_value(row(g, 1, "Endereço IP"), v);
        }
    }
    if (st.state == WIFI_FAILED && st.error[0]) footnote(st.error, RGB(230, 70, 60));
    /* varredura automática ao abrir o painel, depois a cada 30 s */
    u64 now = timer_ticks();
    if (st.state == WIFI_IDLE && (!last_scan || now - last_scan > 30000)) { last_scan = now; wifi_scan(); }
    int n = wifi_networks(nets, 16);
    if (section_btn(connected ? "Outras Redes" : "Redes", UI_ID, "Procurar", st.state == WIFI_SCANNING)) { last_scan = now; wifi_scan(); }
    int shown = 0;
    for (int i = 0; i < n; i++) if (!(connected && !strcmp(nets[i].ssid, st.ssid))) shown++;
    if (!shown) {
        rect_t g = group(1);
        text_draw(U->s, F(FONT_REGULAR), 14, g.x + 20, g.y + 6 + RH / 2 + 5, T.text2,
                  st.state == WIFI_SCANNING ? "Procurando redes…" : "Nenhuma rede encontrada");
        return;
    }
    bool expanded = false;
    for (int i = 0; i < n; i++) if (!strcmp(nets[i].ssid, sel_ssid) && nets[i].security != WIFI_SEC_OPEN) expanded = true;
    rect_t g = group(shown + (expanded ? 1 : 0));
    int k = 0;
    for (int i = 0; i < n; i++) {
        wifi_network_t *w = &nets[i];
        if (connected && !strcmp(w->ssid, st.ssid)) continue;
        static const char *sec[] = {"Aberta", "WEP", "WPA2 Pessoal", "WPA3 Pessoal", "WPA2 Empresarial"};
        char sub[64];
        snprintf(sub, sizeof sub, "%s%s", sec[CLAMP(w->security, 0, 4)], w->supported ? "" : " · não suportada");
        bool c = icon_row(9000 + i, g, k, wifi_icon_cb, (void *)(uptr)w->rssi, w->ssid[0] ? w->ssid : "(rede oculta)", sub, w->supported);
        if (w->security != WIFI_SEC_OPEN) g_lock(U->s, g.x + g.w - 58, g.y + 6 + k * RH + RH / 2 - 8, 16, T.text2);
        if (c) {
            if (w->security == WIFI_SEC_OPEN) { wifi_connect(w->ssid, ""); sel_ssid[0] = 0; }
            else { snprintf(sel_ssid, sizeof sel_ssid, "%s", w->ssid); pass[0] = 0; U->focus = 9800; }
        }
        k++;
        if (!strcmp(w->ssid, sel_ssid) && w->security != WIFI_SEC_OPEN) {
            int y = g.y + 6 + k * RH;
            gfx_fill(U->s, g.x + 54, y, g.w - 74, 1, T.separator);
            bool enter = secure_field(U, 9800, rect_make(g.x + 54, y + 7, g.w - 54 - 210, 26), pass, sizeof pass, "Senha");
            if (ui_button(U, UI_ID, rect_make(g.x + g.w - 196, y + 7, 84, 26), "Cancelar", false)) sel_ssid[0] = 0;
            if (ui_button(U, UI_ID, rect_make(g.x + g.w - 104, y + 7, 84, 26), "Conectar", true) || enter) {
                wifi_connect(sel_ssid, pass);
                memset(pass, 0, sizeof pass);
                sel_ssid[0] = 0;
            }
            k++;
        }
    }
    U->repaint = true;
}

/* ================================================================== Bluetooth */
static void bt_icon_cb(int x, int y, void *ctx) {
    gfx_circle(U->s, x + 13, y + 13, 13, T.dark ? RGB(70, 70, 74) : RGB(225, 225, 230));
    glyph_bluetooth(U->s, x + 6, y + 5, 15, T.text);
}

static void paint_bt(void) {
    hero(P_BT, "Conecte teclados, mouses, fones e outros acessórios sem fio.", NULL, 0);
    const char *st = bt_status();
    rect_t g = group(1);
    rect_t r = row(g, 0, "Estado");
    text_draw_ellipsis(U->s, F(FONT_REGULAR), 12, r.x - 80, r.y + RH / 2 + 4, r.w + 80, T.text2, st);
    bool present = strncmp(st, "Nenhum", 6) != 0;
    if (!present) {
        footnote("Conecte um adaptador Bluetooth USB. Adaptadores Intel carregam o firmware de /Sistema/Firmware.", 0);
        return;
    }
    const char *le = bt_le_status();
    if (le && le[0]) footnote(le, 0);
    static bool scanned;
    static u64 scan_until;
    u64 now = timer_ticks();
    bool busy = now < scan_until || strstr(st, "procurando") != NULL;
    if (section_btn("Dispositivos", UI_ID, "Procurar", busy) || !scanned) {
        scanned = true;
        scan_until = now + 10000;
        bt_le_scan(true);
        bt_classic_scan(true);
    }
    if (scan_until && now >= scan_until) { scan_until = 0; bt_le_scan(false); bt_classic_scan(false); }
    const char *cs = bt_classic_status();
    if (cs && cs[0]) footnote(cs, 0);
    if (bt_a2dp_active()) {
        rect_t ga = group(1);
        if (row_button(UI_ID, row(ga, 0, "Áudio Bluetooth ativo"), "Tocar Som de Teste", false)) bt_a2dp_test_tone();
    }
    int nle = bt_le_device_count(), ncl = bt_classic_count();
    if (!nle && !ncl) {
        rect_t e = group(1);
        text_draw(U->s, F(FONT_REGULAR), 14, e.x + 20, e.y + 6 + RH / 2 + 5, T.text2, busy ? "Procurando…" : "Nenhum dispositivo encontrado");
        footnote("Para parear, coloque o acessório em modo de pareamento (no mouse Logitech, segure o botão Easy-Switch).", 0);
        U->repaint = true;
        return;
    }
    if (nle) {
        rect_t gl = group(nle);
        for (int i = 0; i < nle; i++) {
            static char nm[16][64];
            char addr[24];
            int rssi = 0;
            bool hid = false;
            nm[i % 16][0] = 0;
            bt_le_device(i, nm[i % 16], sizeof nm[0], addr, &rssi, &hid);
            char sub[64];
            snprintf(sub, sizeof sub, "%s · %s · %d dBm", hid ? "Mouse/teclado" : "Bluetooth LE", addr, rssi);
            icon_row(UI_ID + 3000 * (i + 1), gl, i, bt_icon_cb, NULL, nm[i % 16][0] ? nm[i % 16] : "Dispositivo", sub, false);
            rect_t rr = rect_make(gl.x + gl.w / 2, gl.y + 6 + i * RH, gl.w / 2 - 20, RH);
            if (row_button(9100 + i, rr, "Conectar", hid)) bt_le_connect(i);
            if (ui_button(U, 9150 + i, rect_make(rr.x + rr.w - 190, rr.y + 7, 84, 26), "Esquecer", false)) bt_le_forget(addr);
        }
    }
    if (ncl) {
        section("Bluetooth clássico");
        rect_t gc = group(ncl);
        for (int i = 0; i < ncl; i++) {
            static char nm[16][64];
            char addr[24] = "", sub[96];
            u32 cod = 0;
            int rssi = 0;
            nm[i % 16][0] = 0;
            bt_classic_device(i, nm[i % 16], sizeof nm[0], addr, &cod, &rssi);
            int major = (cod >> 8) & 0x1F;
            const char *kind = major == 0x04 ? "Fone / áudio" : major == 0x05 ? "Teclado / mouse" : major == 0x02 ? "Telefone" : major == 0x01 ? "Computador" : "Dispositivo";
            snprintf(sub, sizeof sub, "%s · %s · %d dBm", kind, addr, rssi);
            icon_row(UI_ID + 3000 * (i + 1), gc, i, bt_icon_cb, NULL, nm[i % 16][0] ? nm[i % 16] : addr, sub, false);
            rect_t rr = rect_make(gc.x + gc.w / 2, gc.y + 6 + i * RH, gc.w / 2 - 20, RH);
            bool can = major == 0x04 || major == 0x05;
            if (row_button(9200 + i, rr, "Conectar", can)) bt_classic_connect(i);
            if (ui_button(U, 9250 + i, rect_make(rr.x + rr.w - 190, rr.y + 7, 84, 26), "Esquecer", false)) bt_classic_forget(addr);
        }
        footnote("Fones conectam pelo perfil A2DP; teclados e mouses pelo HID. Outros tipos ainda não são suportados.", 0);
    }
    U->repaint = true;
}

/* ================================================================== Rede */
static void netif_icon_cb(int x, int y, void *ctx) {
    netdev_t *d = ctx;
    bool wifi = d && d->name && (strstr(d->name, "Wi") || strstr(d->name, "wlan") || strstr(d->name, "802.11"));
    gfx_round_rect(U->s, x, y, 26, 26, 6, wifi ? RGB(10, 132, 255) : RGB(88, 86, 214));
    if (wifi) glyph_wifi(U->s, x + 4, y + 3, 18, RGB(255, 255, 255), 0);
    else glyph_ethernet(U->s, x + 4, y + 4, 18, RGB(255, 255, 255));
}

static void paint_rede(void) {
    net_info_t *ni = net_info();
    hero(P_REDE, "Interfaces de rede, endereço IP e servidores DNS. O mOS configura tudo por DHCP.", NULL, 0);
    int n = net_device_count();
    section("Interfaces");
    if (!n) {
        rect_t g = group(1);
        text_draw(U->s, F(FONT_REGULAR), 14, g.x + 20, g.y + 6 + RH / 2 + 5, T.text2, "Nenhuma placa de rede encontrada");
        return;
    }
    rect_t g = group(n);
    for (int i = 0; i < n; i++) {
        netdev_t *d = net_device_at(i);
        bool active = !strcmp(d->name ? d->name : "?", ni->driver);
        bool link = d->link_up ? d->link_up() : true;
        const char *sub = active ? (ni->configured ? "Conectado" : link ? "Obtendo endereço…" : "Sem conexão") : link ? "Disponível" : "Desconectado";
        icon_row(UI_ID + 3000 * (i + 1), g, i, netif_icon_cb, d, d->name ? d->name : "Interface", sub, false);
        rect_t rr = rect_make(g.x + g.w / 2, g.y + 6 + i * RH, g.w / 2 - 20, RH);
        if (active) {
            gfx_circle(U->s, rr.x + rr.w - 6, rr.y + RH / 2, 5, ni->configured ? RGB(52, 199, 89) : link ? RGB(255, 204, 0) : RGB(255, 59, 48));
        } else if (link && row_button(9200 + i, rr, "Usar", false)) {
            net_set_active(d);
        }
    }
    section("Detalhes");
    char ip[20], mk[20], gw[20], dn[20], mac[32], pk[64];
    ip_to_str(ni->ip, ip); ip_to_str(ni->mask, mk); ip_to_str(ni->gw, gw); ip_to_str(ni->dns, dn);
    snprintf(mac, sizeof mac, "%02x:%02x:%02x:%02x:%02x:%02x", ni->mac[0], ni->mac[1], ni->mac[2], ni->mac[3], ni->mac[4], ni->mac[5]);
    snprintf(pk, sizeof pk, "%llu recebidos · %llu enviados", ni->rx_packets, ni->tx_packets);
    const char *lab[] = {"Configurar IPv4", "Endereço IP", "Máscara de sub-rede", "Roteador", "Servidor DNS", "Endereço MAC", "Pacotes"};
    const char *val[] = {"Usando DHCP", ni->configured ? ip : "—", ni->configured ? mk : "—", ni->configured ? gw : "—",
                         ni->configured ? dn : "—", mac, pk};
    rect_t gd = group(7);
    for (int i = 0; i < 7; i++) row_value(row(gd, i, lab[i]), val[i]);
    rect_t ga = group(1);
    rect_t r = row(ga, 0, "Renovar a concessão DHCP");
    if (row_button(UI_ID, r, "Renovar", false)) net_dhcp_renew();
    U->repaint = true;
}

/* ================================================================== Notificações */
static void paint_notif(void) {
    hero(P_NOTIF, "Escolha como os apps e o sistema avisam você.", NULL, 0);
    rect_t g = group(3);
    row_switch(UI_ID, row(g, 0, "Não Perturbe"), &g_notify_dnd, true);
    row_switch(UI_ID, row(g, 1, "Tocar som para notificações"), &g_notify_sound, true);
    char c[32];
    snprintf(c, sizeof c, "%d", g_notify_count);
    row_value(row(g, 2, "Recebidas desde a inicialização"), c);
    footnote("Com Não Perturbe ativo, as notificações são registradas no Console, mas não aparecem na tela.", 0);
    rect_t g2 = group(1);
    if (row_button(UI_ID, row(g2, 0, "Testar notificações"), "Enviar Notificação", false)) notify("Ajustes do Sistema", "Esta é uma notificação de teste.");
}

/* ================================================================== Som */
static void paint_som(void) {
    static int saved_vol = 60;
    hero(P_SOM, "Volume, microfone e sons do sistema.", NULL, 0);
    section("Saída");
    rect_t g = group(3);
    row_value(row(g, 0, "Dispositivo"), audio_available() ? audio_device_name() : "Nenhum dispositivo de áudio");
    rect_t r = row(g, 1, "Volume de saída");
    float v = (float)audio_get_volume();
    glyph_speaker(U->s, r.x, r.y + RH / 2 - 6, 12, T.text2, 0);
    if (ui_slider(U, UI_ID, rect_make(r.x + 20, r.y + RH / 2 - 10, r.w - 44, 20), &v, 0, 100)) audio_set_volume((int)v);
    glyph_speaker(U->s, r.x + r.w - 16, r.y + RH / 2 - 7, 14, T.text2, 3);
    bool mute = audio_get_volume() == 0;
    if (row_switch(UI_ID, row(g, 2, "Sem som"), &mute, audio_available())) {
        if (mute) { saved_vol = MAX(audio_get_volume(), 10); audio_set_volume(0); }
        else audio_set_volume(saved_vol);
    }
    section("Entrada");
    rect_t gi = group(3);
    row_value(row(gi, 0, "Dispositivo"), audio_input_available() ? audio_input_name() : "Nenhuma entrada");
    rect_t ri = row(gi, 1, "Volume de entrada");
    float iv = (float)audio_get_input_volume();
    if (ui_slider(U, UI_ID, rect_make(ri.x, ri.y + RH / 2 - 10, ri.w, 20), &iv, 0, 100) && audio_input_available())
        audio_set_input_volume((int)iv);
    bool mon = audio_get_input_monitor();
    if (row_switch(UI_ID, row(gi, 2, "Ouvir entrada"), &mon, audio_input_available())) audio_set_input_monitor(mon);
    if (audio_input_available())
        footnote("Ouvir a entrada mistura o microfone nos alto-falantes, com o atraso do buffer de saída. O som pode realimentar.", 0);
    else footnote("Nenhum microfone ou entrada de linha foi encontrado neste controlador.", 0);
    section("Efeitos sonoros");
    rect_t g2 = group(3);
    row_switch(UI_ID, row(g2, 0, "Som nas notificações"), &g_notify_sound, true);
    if (row_button(UI_ID, row(g2, 1, "Som de alerta"), "Reproduzir", false)) sound_alert();
    if (row_button(UI_ID, row(g2, 2, "Som de inicialização"), "Reproduzir", false)) sound_startup_chime();
    if (!audio_available()) footnote("Nenhum controlador compatível (Intel HD Audio ou AC'97) foi encontrado.", 0);
}

/* ================================================================== Geral */
static void paint_geral(void) {
    hero(P_GERAL, "Gerencie a configuração geral do mOS, atualizações, armazenamento e mais.", NULL, 0);
    static const int a[] = {P_SOBRE, P_UPDATE, P_STORAGE}, b[] = {P_DATA, P_IDIOMA, P_STARTUP};
    rect_t g = group(3);
    for (int i = 0; i < 3; i++) if (icon_row(UI_ID + 3000 * (i + 1), g, i, pane_icon_cb, (void *)(uptr)a[i], pane_names[a[i]], NULL, true)) go(a[i], true);
    rect_t g2 = group(3);
    for (int i = 0; i < 3; i++) if (icon_row(UI_ID + 3000 * (i + 1), g2, i, pane_icon_cb, (void *)(uptr)b[i], pane_names[b[i]], NULL, true)) go(b[i], true);
}

static void paint_sobre(void) {
    /* cabeçalho centralizado: logotipo, nome e versão */
    rect_t g = rect_make(X0, Y, W0, 170);
    ui_group_box(U, g);
    glyph_mos_logo(U->s, g.x + g.w / 2.0f, g.y + 58, 40, T.accent, T.dark ? RGB(38, 38, 40) : RGB(246, 246, 247));
    text_draw_center(U->s, F(FONT_BOLD), 22, rect_make(g.x, g.y + 104, g.w, 28), T.text, MOS_NAME);
    text_draw_center(U->s, F(FONT_REGULAR), 12, rect_make(g.x, g.y + 132, g.w, 18), T.text2, "Versão " MOS_VERSION);
    Y += g.h + 20;
    char vendor[64] = {0}, brand[64] = {0}, mem[32], cores[16], gpu[64], user[96];
    cpu_info(vendor, brand);
    char *b = brand;
    while (*b == ' ') b++;
    fmt_bytes(pmm_total_pages() * 4096ull, mem, sizeof mem);
    snprintf(cores, sizeof cores, "%d", smp_cpu_count());
    snprintf(gpu, sizeof gpu, "UEFI GOP · %u × %u", g_boot->fb.width, g_boot->fb.height);
    snprintf(user, sizeof user, "Computador de %s", account_fullname());
    const char *lab[] = {"Nome", "Processador", "Núcleos", "Memória", "Gráficos", "Kernel"};
    const char *val[] = {user, *b ? b : vendor, cores, mem, gpu, "Núcleo (x86_64, UEFI)"};
    rect_t gi = group(6);
    for (int i = 0; i < 6; i++) row_value(row(gi, i, lab[i]), val[i]);
    int nu = usb_device_count();
    if (nu) {
        section("Dispositivos USB");
        rect_t gu = group(nu);
        for (int i = 0; i < nu; i++) {
            rect_t r = row(gu, i, NULL);
            text_draw_ellipsis(U->s, F(FONT_REGULAR), 13, gu.x + 20, r.y + RH / 2 + 5, gu.w - 40, T.text, usb_device_desc(i));
        }
    }
    rect_t ga = group(1);
    if (row_button(UI_ID, row(ga, 0, "Licença"), "Sobre este mOS…", false)) about_open();
    footnote("mOS é open source (licença MIT), escrito do zero. A fonte Inter é distribuída sob a licença OFL.", 0);
}

static void paint_update(void) {
    rect_t g = rect_make(X0, Y, W0, 120);
    ui_group_box(U, g);
    pane_icon(U->s, P_UPDATE, g.x + 20, g.y + 32, 56);
    text_draw(U->s, F(FONT_BOLD), 16, g.x + 92, g.y + 52, T.text, "O mOS está atualizado");
    text_draw(U->s, F(FONT_REGULAR), 12, g.x + 92, g.y + 72, T.text2, MOS_NAME " " MOS_VERSION);
    Y += g.h + 20;
    footnote("Novas versões são distribuídas como imagem de disco: grave a nova imagem no pendrive ou use Instalar mOS.", 0);
}

static void paint_storage(void) {
    section("Memória do sistema");
    rect_t gm = group(2);
    u64 tot = pmm_total_pages() * 4096ull, fre = pmm_free_pages() * 4096ull;
    char a[32], b[32], line[96];
    fmt_bytes(tot - fre, a, sizeof a); fmt_bytes(tot, b, sizeof b);
    snprintf(line, sizeof line, "%s de %s em uso", a, b);
    row_value(row(gm, 0, "Memória"), line);
    rect_t r = row(gm, 1, NULL);
    usage_bar(rect_make(gm.x + 20, r.y + 14, gm.w - 40, 12), tot ? (float)(tot - fre) / tot : 0, T.accent);
    section("Discos");
    int nd = storage_disk_count();
    if (!nd) {
        rect_t g = group(1);
        text_draw(U->s, F(FONT_REGULAR), 14, g.x + 20, g.y + 6 + RH / 2 + 5, T.text2, "Nenhum disco encontrado (sistema rodando da memória)");
        return;
    }
    int sysd = persist_disk_index();
    rect_t g = group(nd);
    for (int i = 0; i < nd; i++) {
        blkdev_t *d = storage_disk(i);
        char sz[32], t[160];
        fmt_bytes(d->size, sz, sizeof sz);
        snprintf(t, sizeof t, "%s (%s)%s", storage_disk_desc(i), storage_disk_name(i), i == sysd ? " · disco do sistema" : "");
        rect_t rr = row(g, i, NULL);
        text_draw_ellipsis(U->s, F(FONT_REGULAR), 14, g.x + 20, rr.y + RH / 2 + 5, g.w - 160, T.text, t);
        row_value(rr, sz);
    }
}

static void paint_data(void) {
    datetime_t d;
    rtc_now(&d);
    static const char *wd[7] = {"domingo", "segunda-feira", "terça-feira", "quarta-feira", "quinta-feira", "sexta-feira", "sábado"};
    static const char *mo[12] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};
    char t[96];
    snprintf(t, sizeof t, "%s, %d de %s de %d, %02d:%02d:%02d", wd[d.weekday % 7], d.day, mo[(d.month + 11) % 12], d.year, d.hour, d.minute, d.second);
    rect_t g = group(3);
    row_value(row(g, 0, "Data e hora"), t);
    static const char *tzs[] = {"UTC−5 (Acre)", "UTC−4 (Manaus)", "UTC−3 (Brasília)", "UTC−2 (Noronha)", "UTC+0 (Londres)", "UTC+1 (Lisboa, verão)"};
    static const int offs[] = {-300, -240, -180, -120, 0, 60};
    int sel = 2;
    for (int i = 0; i < 6; i++) if (offs[i] == g_tz_offset_min) sel = i;
    int ns = ui_popup_inline(U, UI_ID, row(g, 1, "Fuso horário"), tzs, 6, sel);
    if (ns != sel) { g_tz_offset_min = offs[ns]; wm_damage_all(); }
    bool utc = g_rtc_is_utc;
    if (row_switch(UI_ID, row(g, 2, "Relógio do hardware em UTC"), &utc, true)) { g_rtc_is_utc = utc; wm_damage_all(); }
    footnote("Desligue a opção de UTC se o computador também roda Windows, que guarda a hora local no relógio do hardware.", 0);
    U->repaint = true;
}

static void paint_idioma(void) {
    rect_t g = group(3);
    static const char *langs[] = {"Português (Brasil)"};
    ui_popup_inline(U, UI_ID, row(g, 0, "Idioma principal"), langs, 1, 0);
    row_value(row(g, 1, "Região"), "Brasil");
    static const char *lay[] = {"Português (ABNT2)", "Inglês (EUA)"};
    g_kbd_layout = ui_popup_inline(U, UI_ID, row(g, 2, "Teclado"), lay, 2, g_kbd_layout);
    section("Formatos");
    rect_t g2 = group(2);
    row_value(row(g2, 0, "Data"), "06/11/2026 · sexta-feira, 6 de novembro");
    row_value(row(g2, 1, "Números"), "1.234.567,89 · R$");
}

static void paint_startup(void) {
    int nd = storage_disk_count(), sysd = persist_disk_index();
    section("Inicializar a partir de");
    rect_t g = group(MAX(1, nd));
    if (!nd) text_draw(U->s, F(FONT_REGULAR), 14, g.x + 20, g.y + 6 + RH / 2 + 5, T.text2, "Pendrive (sistema rodando da memória)");
    for (int i = 0; i < nd; i++) {
        char sz[32], t[160];
        fmt_bytes(storage_disk(i)->size, sz, sizeof sz);
        snprintf(t, sizeof t, "%s — %s", storage_disk_desc(i), sz);
        rect_t r = row(g, i, NULL);
        text_draw_ellipsis(U->s, F(FONT_REGULAR), 14, g.x + 20, r.y + RH / 2 + 5, g.w - 160, T.text, t);
        if (i == sysd) row_value(r, "Em uso");
    }
    rect_t g2 = group(1);
    if (row_button(UI_ID, row(g2, 0, "Instalar o mOS em outro disco"), "Instalar mOS…", false)) app_launch(apps_find("instalador"));
    footnote("A ordem de inicialização é escolhida no firmware (UEFI) do computador.", 0);
}

/* ================================================================== Aparência */
static void mini_window(int x, int y, int w, int h, bool dark) {
    gfx_round_rect_grad(U->s, x, y, w, h, 8, dark ? RGB(40, 44, 80) : RGB(120, 160, 220), dark ? RGB(90, 60, 110) : RGB(240, 170, 130));
    gfx_round_rect(U->s, x + 10, y + 10, w - 20, h - 18, 5, dark ? RGB(36, 36, 38) : RGB(250, 250, 250));
    gfx_round_rect(U->s, x + 10, y + 10, 26, h - 18, 5, dark ? RGB(52, 52, 56) : RGB(232, 232, 236));
    for (int i = 0; i < 3; i++) gfx_circle(U->s, x + 16 + i * 6, y + 16, 2, i == 0 ? RGB(255, 95, 87) : i == 1 ? RGB(254, 188, 46) : RGB(40, 200, 64));
    gfx_round_rect(U->s, x + 42, y + 24, w - 64, 5, 2, dark ? RGB(80, 80, 84) : RGB(210, 210, 214));
    gfx_round_rect(U->s, x + 42, y + 34, w - 80, 5, 2, dark ? RGB(80, 80, 84) : RGB(210, 210, 214));
}

static void paint_apar(void) {
    int cardw = 120, cardh = 80, gap = 30;
    rect_t g = rect_make(X0, Y, W0, cardh + 60);
    ui_group_box(U, g);
    text_draw(U->s, F(FONT_REGULAR), 14, g.x + 20, g.y + 28, T.text, "Aparência");
    int x0 = g.x + g.w - 20 - (2 * cardw + gap);
    static const char *nm[] = {"Clara", "Escura"};
    for (int i = 0; i < 2; i++) {
        int x = x0 + i * (cardw + gap), y = g.y + 14;
        mini_window(x, y, cardw, cardh, i == 1);
        bool sel = (i == 1) == T.dark;
        if (sel) gfx_round_rect_stroke(U->s, x - 3, y - 3, cardw + 6, cardh + 6, 10, 3, T.accent);
        text_draw_center(U->s, F(sel ? FONT_SEMIBOLD : FONT_REGULAR), 12, rect_make(x, y + cardh + 8, cardw, 18), T.text, nm[i]);
        if (ui_clicked(U, UI_ID + 3000 * (i + 1), rect_make(x, y, cardw, cardh + 24)) && !sel) {
            theme_set(i == 1, g_accent_index);
            desktop_rebuild_wallpaper();
            redraw_all();
        }
    }
    Y += g.h + 16;
    rect_t g2 = group(1);
    rect_t r = row(g2, 0, "Cor de destaque");
    for (int i = 0; i < 8; i++) {
        float cx = r.x + r.w - 8 - (7 - i) * 24, cy = r.y + RH / 2.0f;
        gfx_circle(U->s, cx, cy, 8, g_accent_colors[i]);
        gfx_circle_stroke(U->s, cx, cy, 8, 1, RGBA(0, 0, 0, 40));
        if (i == g_accent_index) gfx_circle(U->s, cx, cy, 3, RGB(255, 255, 255));
        if (ui_clicked(U, 9300 + i, rect_make((int)cx - 10, (int)cy - 10, 20, 20))) { theme_set(T.dark, i); redraw_all(); }
    }
    char an[64];
    snprintf(an, sizeof an, "Destaque: %s", g_accent_names[g_accent_index]);
    footnote(an, 0);
}

/* ================================================================== Mesa e Dock */
static void paint_mesa(void) {
    section("Dock");
    rect_t g = group(3);
    rect_t r = row(g, 0, "Tamanho");
    float sz = (float)g_dock_size;
    text_draw(U->s, F(FONT_REGULAR), 11, r.x, r.y + RH / 2 + 4, T.text2, "Pequeno");
    if (ui_slider(U, UI_ID, rect_make(r.x + 54, r.y + RH / 2 - 10, r.w - 104, 20), &sz, 32, 72)) { g_dock_size = (int)sz; wm_damage_all(); }
    text_draw(U->s, F(FONT_REGULAR), 11, r.x + r.w - 42, r.y + RH / 2 + 4, T.text2, "Grande");
    bool mg = g_dock_magnify;
    if (row_switch(UI_ID, row(g, 1, "Ampliação"), &mg, true)) { g_dock_magnify = mg; wm_damage_all(); }
    bool ah = g_dock_autohide;
    if (row_switch(UI_ID, row(g, 2, "Ocultar automaticamente"), &ah, true)) { g_dock_autohide = ah; wm_damage_all(); }
    section("Mesa");
    rect_t g2 = group(2);
    if (row_button(UI_ID, row(g2, 0, "Imagem de fundo"), "Escolher…", false)) go(P_WALL, true);
    extern bool g_desktop_widgets;
    bool wd = g_desktop_widgets;
    if (row_switch(UI_ID, row(g2, 1, "Mostrar widgets na mesa"), &wd, true)) { g_desktop_widgets = wd; wm_damage_all(); }
    /* cantos ativos: uma ação por canto da tela */
    section("Cantos Ativos");
    extern int g_hot_corner[4];
    static const char *acts[] = {"—", "Mission Control", "Central de Notificações", "Launchpad", "Mostrar Mesa", "Central de Controle", "Bloquear Tela"};
    static const char *names[] = {"Canto superior esquerdo", "Canto superior direito", "Canto inferior esquerdo", "Canto inferior direito"};
    rect_t g3 = group(4);
    for (int i = 0; i < 4; i++) {
        rect_t r = row(g3, i, names[i]);
        g_hot_corner[i] = ui_popup(U, 9400 + i, rect_make(r.x + r.w - 200, r.y + (RH - 24) / 2, 200, 24), acts, 7, g_hot_corner[i]);
    }
}

/* ================================================================== Monitores
 *
 * Como o painel Monitores do Mac: miniaturas dos monitores no topo (a selecionada com anel de destaque),
 * "Organizar…" abre uma folha com o arranjo, e embaixo ficam os ajustes do monitor selecionado ("Usar
 * como": principal, estendido ou espelho). Na folha:
 *   - arraste um monitor: ele segue o mouse e, ao soltar, encosta na borda mais próxima de outro;
 *   - arraste a faixa branca (a barra de menus) para outro monitor: ele vira o principal;
 *   - com ⌥ pressionada, soltar um monitor sobre outro espelha os dois; arrastar de novo sem ⌥ desfaz;
 *   - enquanto arrasta, o monitor de verdade ganha um contorno vermelho.
 * Cada mudança vale na hora (display_set_layout) e fica salva para o próximo boot. */
static bool arr_open;
static int mon_sel;

static struct {
    bool active, menubar;       /* arrastando um monitor, ou a barra de menus */
    int out;                    /* saída arrastada */
    int gx, gy;                 /* onde o mouse pegou, relativo ao canto da miniatura */
    int minx, miny, ox, oy;     /* mapeamento congelado durante o arrasto: canvas = o + (pos - min) * k */
    float k;
    display_layout_t L;
} AR;

/* miniatura de um monitor: papel de parede, moldura, barra de menus (se principal) e nome */
static void mon_thumb(rect_t r, int out, bool primary, bool hi, bool label) {
    surface_t *s = U->s;
    gfx_round_rect(s, r.x - 1, r.y - 1, r.w + 2, r.h + 2, 5, hi ? T.accent : (T.dark ? RGB(90, 90, 96) : RGB(40, 40, 44)));
    surface_t *wb = desktop_wallpaper_blur();
    if (wb && r.w > 4 && r.h > 4) gfx_blit_scaled(s, wb, r.x + 1, r.y + 1, r.w - 2, r.h - 2, 255);
    else gfx_fill(s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, RGB(70, 90, 140));
    if (hi) gfx_round_rect_stroke(s, r.x - 2, r.y - 2, r.w + 4, r.h + 4, 6, 2, T.accent);
    if (primary) {                                    /* a barra de menus: faixa branca com um fio escuro embaixo */
        int bh = MAX(5, r.h / 12);
        gfx_fill(s, r.x + 1, r.y + 1, r.w - 2, bh, RGBA(255, 255, 255, 245));
        gfx_fill(s, r.x + 1, r.y + 1 + bh, r.w - 2, 1, RGBA(0, 0, 0, 90));
    }
    if (label) {
        const char *nm = "";
        display_output_info(out, NULL, NULL, &nm);
        font_t *f = F(FONT_SEMIBOLD);
        int maxw = r.w - 10, tw = MIN(text_width(f, 11, nm), maxw);
        int tx = r.x + (r.w - tw) / 2, bl = r.y + r.h / 2 + 4;
        text_draw_ellipsis(s, f, 11, tx + 1, bl + 1, maxw, RGBA(0, 0, 0, 130), nm);
        text_draw_ellipsis(s, f, 11, tx, bl, maxw, RGB(255, 255, 255), nm);
    }
}

static void out_size(int out, int *w, int *h) { display_output_info(out, w, h, NULL); }

/* ordem de desenho: no espelho, os outros primeiro e o principal por cima */
static int arr_order(const display_layout_t *L, int k) {
    if (!L->mirror) return k;
    if (k == L->n - 1) return L->primary;
    return k < L->primary ? k : k + 1;
}
static bool layout_eq(const display_layout_t *a, const display_layout_t *b) {
    if (a->n != b->n || a->primary != b->primary || a->mirror != b->mirror) return false;
    for (int i = 0; i < a->n; i++) if (a->x[i] != b->x[i] || a->y[i] != b->y[i]) return false;
    return true;
}

/* encosta a saída i na borda mais próxima de outra, sem sobrepor ninguém (como o Mac faz ao soltar) */
static void arr_snap(display_layout_t *L, int i, int px, int py) {
    int wi, hi;
    out_size(i, &wi, &hi);
    int bx = L->x[i], by = L->y[i];
    i64 bd = -1;
    for (int j = 0; j < L->n; j++) {
        if (j == i) continue;
        int wj, hj;
        out_size(j, &wj, &hj);
        int ov = 40;                                  /* encostados por pelo menos 40 px */
        int cy = CLAMP(py, L->y[j] - hi + ov, L->y[j] + hj - ov), cx = CLAMP(px, L->x[j] - wi + ov, L->x[j] + wj - ov);
        /* imã de alinhamento: topos, bases, esquerdas e direitas iguais se estiverem perto */
        if (ABS_DIFF(cy, L->y[j]) < 48) cy = L->y[j];
        else if (ABS_DIFF(cy + hi, L->y[j] + hj) < 48) cy = L->y[j] + hj - hi;
        if (ABS_DIFF(cx, L->x[j]) < 48) cx = L->x[j];
        else if (ABS_DIFF(cx + wi, L->x[j] + wj) < 48) cx = L->x[j] + wj - wi;
        int cand[4][2] = {{L->x[j] - wi, cy}, {L->x[j] + wj, cy}, {cx, L->y[j] - hi}, {cx, L->y[j] + hj}};
        for (int c = 0; c < 4; c++) {
            display_layout_t T2 = *L;
            T2.x[i] = cand[c][0]; T2.y[i] = cand[c][1];
            T2.mirror = false;
            if (!display_layout_valid(&T2)) continue;
            i64 d = (i64)ABS_DIFF(cand[c][0], px) + ABS_DIFF(cand[c][1], py);
            if (bd < 0 || d < bd) { bd = d; bx = cand[c][0]; by = cand[c][1]; }
        }
    }
    L->x[i] = bx; L->y[i] = by;
}

/* posição (em pixels de monitor) de uma saída no arranjo desenhado; no espelho, empilhadas */
static rect_t arr_rect(const display_layout_t *L, int i) {
    int w, h;
    out_size(i, &w, &h);
    int x = L->x[i], y = L->y[i];
    if (L->mirror) { x = L->x[L->primary] + i * 40; y = L->y[L->primary] + i * 40; }
    return rect_make((int)(AR.ox + (x - AR.minx) * AR.k), (int)(AR.oy + (y - AR.miny) * AR.k), MAX(8, (int)(w * AR.k)), MAX(6, (int)(h * AR.k)));
}

static void paint_arrange_sheet(void) {
    if (!arr_open) return;
    int n = display_output_count();
    rect_t s = ui_sheet(U, MIN(U->cw - 40, 660), 440, 30);
    text_draw(U->s, F(FONT_REGULAR), 13, s.x + 24, s.y + 34, T.text, "Para reorganizar os monitores, arraste-os para a posição desejada.");
    text_draw(U->s, F(FONT_REGULAR), 13, s.x + 24, s.y + 54, T.text, "Para mudar a barra de menus de monitor, arraste a faixa branca até ele.");
    text_draw(U->s, F(FONT_REGULAR), 12, s.x + 24, s.y + 74, T.text2, "Para espelhar, segure ⌥ (Option) e solte um monitor sobre o outro.");
    rect_t C = rect_make(s.x + 24, s.y + 92, s.w - 48, s.h - 150);
    gfx_round_rect(U->s, C.x, C.y, C.w, C.h, 10, T.dark ? RGB(30, 30, 33) : RGB(236, 236, 240));

    display_layout_t L;
    if (AR.active) L = AR.L; else display_get_layout(&L);
    if (!AR.active) {                                 /* enquadra o arranjo na área (congelado no arrasto) */
        int minx = 1 << 30, miny = 1 << 30, maxx = -(1 << 30), maxy = -(1 << 30);
        for (int i = 0; i < n; i++) {
            int w, h;
            out_size(i, &w, &h);
            int x = L.mirror ? L.x[L.primary] + i * 40 : L.x[i], y = L.mirror ? L.y[L.primary] + i * 40 : L.y[i];
            minx = MIN(minx, x); miny = MIN(miny, y); maxx = MAX(maxx, x + w); maxy = MAX(maxy, y + h);
        }
        /* folga dos lados para arrastar um monitor para qualquer borda */
        int bw = maxx - minx, bh = maxy - miny;
        AR.k = MIN((float)(C.w - 40) / (bw * 1.7f), (float)(C.h - 40) / (bh * 1.9f));
        AR.minx = minx; AR.miny = miny;
        AR.ox = C.x + (int)((C.w - bw * AR.k) / 2);
        AR.oy = C.y + (int)((C.h - bh * AR.k) / 2);
    }
    rect_t pc = surface_push_clip(U->s, C);

    /* início do arrasto */
    if (!AR.active && U->pressed && rect_contains(C, U->mx, U->my)) {
        for (int k = n - 1; k >= 0; k--) {             /* de cima para baixo */
            int i = arr_order(&L, k);
            rect_t r = arr_rect(&L, i);
            if (!rect_contains(r, U->mx, U->my)) continue;
            AR.active = true;
            AR.out = i;
            AR.L = L;
            AR.menubar = i == L.primary && !L.mirror && U->my < r.y + MAX(5, r.h / 12) + 6;
            AR.gx = U->mx - r.x; AR.gy = U->my - r.y;
            display_identify(i);
            break;
        }
    }
    /* monitores (o arrastado por cima, seguindo o mouse) */
    for (int k = 0; k < n; k++) {
        int i = arr_order(&L, k);
        if (AR.active && !AR.menubar && i == AR.out) continue;
        mon_thumb(arr_rect(&L, i), i, i == L.primary, false, true);
    }
    if (AR.active) {
        rect_t r = arr_rect(&L, AR.out);
        if (AR.menubar) {
            /* só a faixa branca acompanha o mouse */
            int bw = r.w, bh = MAX(5, r.h / 12);
            gfx_fill(U->s, U->mx - AR.gx + 1, U->my - AR.gy + 1, bw - 2, bh, RGBA(255, 255, 255, 235));
            gfx_rect(U->s, U->mx - AR.gx, U->my - AR.gy, bw, bh + 2, T.accent);
        } else {
            r.x = U->mx - AR.gx; r.y = U->my - AR.gy;
            mon_thumb(r, AR.out, AR.out == L.primary && !L.mirror, true, true);
        }
        U->repaint = true;
        if (!U->down) {                               /* soltou: aplica */
            display_layout_t N = AR.L;
            if (AR.menubar) {
                for (int i = 0; i < n; i++)
                    if (i != AR.out && rect_contains(arr_rect(&N, i), U->mx, U->my)) { N.primary = i; break; }
            } else {
                int px = AR.minx + (int)((U->mx - AR.gx - AR.ox) / AR.k), py = AR.miny + (int)((U->my - AR.gy - AR.oy) / AR.k);
                bool over = false;
                rect_t me = rect_make(U->mx - AR.gx, U->my - AR.gy, r.w, r.h);
                for (int i = 0; i < n; i++)
                    if (i != AR.out && rect_contains(arr_rect(&N, i), me.x + me.w / 2, me.y + me.h / 2)) over = true;
                if ((input_mods() & MOD_ALT) && over) N.mirror = true;
                else { N.mirror = false; arr_snap(&N, AR.out, px, py); }
            }
            if (!layout_eq(&N, &AR.L) && display_layout_valid(&N)) display_set_layout(&N);
            AR.active = false;
            display_identify(-1);
        }
    }
    surface_pop_clip(U->s, pc);
    if (ui_button(U, 7600, rect_make(s.x + s.w - 124, s.y + s.h - 46, 100, 28), "Concluído", true)) {
        arr_open = false;
        AR.active = false;
        display_identify(-1);
    }
    ui_sheet_end(U);
}

static void paint_monitor(void) {
    int n = display_output_count();
    display_layout_t L;
    display_get_layout(&L);
    if (mon_sel >= n) mon_sel = 0;

    /* miniaturas: todas lado a lado, proporcionais, com o nome embaixo */
    rect_t g = rect_make(X0, Y, W0, n > 1 ? 232 : 190);
    ui_group_box(U, g);
    int slot = n > 1 ? MIN(190, (g.w - 40) / n) : 220, mh = 125;
    int x = g.x + (g.w - slot * n) / 2;
    for (int i = 0; i < n; i++, x += slot) {
        int w, h;
        out_size(i, &w, &h);
        float k = MIN((slot - 30) / (float)w, mh / (float)h);
        rect_t r = rect_make(x + (slot - (int)(w * k)) / 2, g.y + 18 + (mh - (int)(h * k)) / 2, (int)(w * k), (int)(h * k));
        mon_thumb(r, i, i == L.primary && !L.mirror, n > 1 && i == mon_sel, false);
        if (n == 1) {
            gfx_round_rect(U->s, r.x + r.w / 2 - 12, r.y + r.h + 1, 24, 10, 2, RGB(170, 170, 174));
            gfx_round_rect(U->s, r.x + r.w / 2 - 40, r.y + r.h + 9, 80, 6, 3, RGB(170, 170, 174));
        }
        const char *nm = "";
        display_output_info(i, NULL, NULL, &nm);
        text_draw_center(U->s, F(n > 1 && i == mon_sel ? FONT_SEMIBOLD : FONT_REGULAR), 13,
                         rect_make(x, g.y + 18 + mh + 20, slot, 18), T.text, nm);
        if (n > 1 && ui_clicked(U, 7620 + i, rect_make(x, g.y + 10, slot, mh + 36))) mon_sel = i;
    }
    if (n > 1 && ui_button(U, 7610, rect_make(g.x + g.w - 124, g.y + g.h - 36, 108, 26), "Organizar…", false)) {
        arr_open = true;
        AR.active = false;
    }
    Y += g.h + 20;

    /* ajustes do monitor selecionado */
    int w, h;
    out_size(mon_sel, &w, &h);
    char res[48];
    snprintf(res, sizeof res, "%d × %d", w, h);
    if (n > 1) {
        section(mon_sel == 0 ? "Monitor integrado" : "Monitor externo");
        rect_t gi = group(4);
        static char mirror_item[80];                  /* o menu guarda o ponteiro e desenha depois: não pode ser da pilha */
        int other = mon_sel == L.primary ? (L.primary == 0 ? 1 : 0) : L.primary;
        const char *on = "";
        display_output_info(other, NULL, NULL, &on);
        snprintf(mirror_item, sizeof mirror_item, "Espelhar para \"%s\"", on);
        const char *items[3] = {"Monitor principal", "Monitor estendido", mirror_item};
        int cur = L.mirror ? 2 : (mon_sel == L.primary ? 0 : 1);
        int ns = ui_popup_inline(U, UI_ID, row(gi, 0, "Usar como"), items, 3, cur);
        if (ns != cur) {
            display_layout_t N = L;
            if (ns == 0) { N.primary = mon_sel; N.mirror = false; }
            else if (ns == 1) { N.mirror = false; if (mon_sel == L.primary) N.primary = other; }
            else N.mirror = true;
            if (display_layout_valid(&N)) display_set_layout(&N);
        }
        row_value(row(gi, 1, "Resolução"), res);
        row_value(row(gi, 2, "Cores"), "32 bits");
        row_value(row(gi, 3, "Escala"), "1× (padrão)");
        footnote(L.mirror ? "Os monitores mostram a mesma imagem. Escolha \"Monitor estendido\" para separar."
                          : "Use \"Organizar…\" para escolher em que lado de cada monitor o mouse e as janelas passam para o outro.", 0);
    } else {
        framebuffer_t *fb = &g_boot->fb;
        char bpp[32];
        snprintf(bpp, sizeof bpp, "32 bits (%u px por linha)", fb->pitch);
        rect_t gi = group(3);
        row_value(row(gi, 0, "Resolução"), res);
        row_value(row(gi, 1, "Cores"), bpp);
        row_value(row(gi, 2, "Escala"), "1× (padrão)");
        footnote("A resolução é escolhida pelo firmware na inicialização; o mOS prefere 1440 × 900.", 0);
    }
}

/* ================================================================== Papel de Parede */
static void paint_wall(void) {
    static surface_t *thumbs[4];
    static bool thumb_dark;
    if (thumbs[0] && thumb_dark != T.dark) for (int i = 0; i < 4; i++) { surface_free(thumbs[i]); thumbs[i] = NULL; }
    thumb_dark = T.dark;
    static const char *names[] = {"Dunas Dinâmicas", "Dunas (Dia)", "Dunas (Noite)", "Seda"};
    int tw = (W0 - 40 - 3 * 14) / 4, th = tw * 10 / 16;
    for (int i = 0; i < 4; i++) if (!thumbs[i]) { thumbs[i] = surface_new(tw, th); desktop_wallpaper_thumb(thumbs[i], i); }
    /* atual em destaque */
    rect_t g = rect_make(X0, Y, W0, 150);
    ui_group_box(U, g);
    int pw_ = 192, ph = 120;
    surface_t *cur = thumbs[g_wallpaper_style];
    gfx_blit_scaled(U->s, cur, g.x + 16, g.y + 15, pw_, ph, 255);
    gfx_round_rect_stroke(U->s, g.x + 16, g.y + 15, pw_, ph, 6, 1, RGBA(0, 0, 0, 40));
    text_draw(U->s, F(FONT_SEMIBOLD), 15, g.x + pw_ + 36, g.y + 44, T.text, names[g_wallpaper_style]);
    text_draw(U->s, F(FONT_REGULAR), 12, g.x + pw_ + 36, g.y + 64, T.text2, g_wallpaper_style == 0 ? "Muda ao longo do dia" : "Imagem estática");
    Y += g.h + 16;
    section("Papéis de parede do mOS");
    rect_t gg = rect_make(X0, Y, W0, th + 52);
    ui_group_box(U, gg);
    for (int i = 0; i < 4; i++) {
        int x = gg.x + 20 + i * (tw + 14), y = gg.y + 14;
        gfx_blit(U->s, thumbs[i], x, y);
        if (i == g_wallpaper_style) gfx_round_rect_stroke(U->s, x - 3, y - 3, tw + 6, th + 6, 7, 3, T.accent);
        text_draw_center(U->s, F(FONT_REGULAR), 11, rect_make(x - 6, y + th + 6, tw + 12, 16), T.text, names[i]);
        if (ui_clicked(U, 9400 + i, rect_make(x, y, tw, th)) && i != g_wallpaper_style) { g_wallpaper_style = i; desktop_rebuild_wallpaper(); }
    }
    Y += gg.h + 16;
}

/* ================================================================== Privacidade e Segurança */
static void paint_priv(void) {
    hero(P_PRIV, "Proteções do sistema: verificação de certificados, contas e senhas.", NULL, 0);
    section("Segurança da rede");
    rect_t g = group(2);
    bool verify = !g_tls_insecure;
    if (row_switch(UI_ID, row(g, 0, "Verificar certificados HTTPS"), &verify, true)) g_tls_insecure = !verify;
    char tc[48];
    snprintf(tc, sizeof tc, "%u autoridades (Mozilla)", x509_trust_count());
    row_value(row(g, 1, "Autoridades confiáveis"), tc);
    if (g_tls_insecure) footnote("Atenção: sem a verificação, qualquer servidor pode se passar pelo site que você abriu.", RGB(230, 70, 60));
    else footnote("O navegador e o curl recusam conexões cujo certificado não seja válido para o site.", 0);
    section("Contas");
    int n = account_count(), nopw = 0;
    for (int i = 0; i < n; i++) { bool hp; account_get(i, NULL, 0, NULL, 0, NULL, &hp); if (!hp) nopw++; }
    rect_t g2 = group(2);
    char c[48];
    snprintf(c, sizeof c, nopw ? "%d sem senha" : "Todas têm senha", nopw);
    row_value(row(g2, 0, "Senhas das contas"), c);
    if (row_button(UI_ID, row(g2, 1, "Usuários e Grupos"), "Abrir…", false)) go(P_USERS, true);
    if (nopw) footnote("Contas sem senha entram direto. Defina uma senha em Usuários e Grupos para pedir senha ao iniciar.", 0);
}

/* ================================================================== Teclado */
static void paint_teclado(void) {
    hero(P_TECLADO, "Layout do teclado e atalhos.", NULL, 0);
    rect_t g = group(2);
    static const char *lay[] = {"Português (ABNT2)", "Inglês (EUA)"};
    g_kbd_layout = ui_popup_inline(U, UI_ID, row(g, 0, "Layout"), lay, 2, g_kbd_layout);
    rect_t r = row(g, 1, "Teste");
    static char test[128];
    ui_textfield(U, UI_ID, rect_make(r.x, r.y + 7, r.w, 26), test, sizeof test, "Digite: ação, çedilha, à, ê…");
    section("Teclas modificadoras");
    rect_t g2 = group(3);
    row_value(row(g2, 0, "⌘ Command"), "tecla Windows");
    row_value(row(g2, 1, "⌥ Option"), "Alt");
    row_value(row(g2, 2, "⌃ Control"), "Ctrl");
    section("Atalhos");
    rect_t g3 = group(4);
    row_value(row(g3, 0, "Busca"), "⌘ Espaço");
    row_value(row(g3, 1, "Apps"), "F4 ou ⌘ L");
    row_value(row(g3, 2, "Central de Controle"), "F3");
    row_value(row(g3, 3, "Fechar janela"), "⌘ W");
}

/* ================================================================== Usuários e Grupos */
static void avatar(int x, int y, int d, const char *name) {
    static const u32 cols[6] = {RGB(90, 120, 230), RGB(230, 110, 80), RGB(70, 170, 120), RGB(170, 90, 200), RGB(240, 170, 40), RGB(60, 160, 200)};
    u32 h = 0;
    for (const char *p = name; *p; p++) h = h * 31 + (u8)*p;
    u32 c = cols[h % 6];
    gfx_round_rect_grad(U->s, x, y, d, d, d / 2.0f, color_lighten(c, 40), c);
    char ini[3] = {(char)toupper((u8)name[0]), 0, 0};
    const char *sp = strchr(name, ' ');
    if (sp && sp[1]) ini[1] = (char)toupper((u8)sp[1]);
    text_draw_center(U->s, F(FONT_SEMIBOLD), d * 0.4f, rect_make(x, y, d, d), RGB(255, 255, 255), ini);
}
static void avatar_cb(int x, int y, void *ctx) { avatar(x - 2, y - 2, 30, ctx); }

static void paint_users(void) {
    static int sel = -1;
    static char nm[32], full[64], p1[64], p2[64], oldp[64], np1[64], np2[64], err[128];
    static bool admin, creating;
    int n = account_count();
    if (sel >= n) sel = -1;
    /* cartão da conta atual */
    rect_t gc = rect_make(X0, Y, W0, 84);
    ui_group_box(U, gc);
    avatar(gc.x + 18, gc.y + 14, 56, account_fullname());
    text_draw(U->s, F(FONT_BOLD), 16, gc.x + 90, gc.y + 38, T.text, account_fullname());
    char sub[96];
    snprintf(sub, sizeof sub, "%s · /Users/%s", account_is_admin(account_current()) ? "Administrador" : "Padrão", account_current());
    text_draw(U->s, F(FONT_REGULAR), 12, gc.x + 90, gc.y + 58, T.text2, sub);
    if (ui_button(U, UI_ID, rect_make(gc.x + gc.w - 150, gc.y + 29, 130, 26), "Encerrar Sessão…", false)) { login_logout(); return; }
    Y += gc.h + 20;
    section("Usuários");
    rect_t g = group(n);
    for (int i = 0; i < n; i++) {
        char name[64], fn[96], s2[96];
        bool adm, hp;
        account_get(i, name, sizeof name, fn, sizeof fn, &adm, &hp);
        snprintf(s2, sizeof s2, "%s%s%s", adm ? "Administrador" : "Padrão", hp ? "" : " · sem senha", !strcmp(name, account_current()) ? " · em uso" : "");
        static char fns[32][96];
        snprintf(fns[i % 32], 96, "%s", fn[0] ? fn : name);
        if (icon_row(UI_ID + 3000 * (i + 1), g, i, avatar_cb, fns[i % 32], fns[i % 32], s2, true)) {
            sel = sel == i ? -1 : i; creating = false; err[0] = 0; oldp[0] = np1[0] = np2[0] = 0;
        }
    }
    rect_t ga = group(1);
    if (row_button(UI_ID, row(ga, 0, "Nova conta"), "Adicionar Usuário…", false)) {
        creating = true; sel = -1; err[0] = 0; nm[0] = full[0] = p1[0] = p2[0] = 0; admin = false;
    }
    if (creating) {
        section("Nova conta");
        rect_t gf = group(5);
        rect_t r;
        r = row(gf, 0, "Nome completo"); ui_textfield(U, 1801, rect_make(r.x, r.y + 7, r.w, 26), full, sizeof full, "Ana Silva");
        r = row(gf, 1, "Nome da conta"); ui_textfield(U, 1802, rect_make(r.x, r.y + 7, r.w, 26), nm, sizeof nm, "ana");
        r = row(gf, 2, "Senha"); secure_field(U, 1803, rect_make(r.x, r.y + 7, r.w, 26), p1, sizeof p1, "opcional");
        r = row(gf, 3, "Confirmar senha"); bool go1 = secure_field(U, 1804, rect_make(r.x, r.y + 7, r.w, 26), p2, sizeof p2, "");
        row_switch(UI_ID, row(gf, 4, "Administrador"), &admin, true);
        if (err[0]) footnote(err, RGB(230, 70, 60));
        int bx = X0 + W0;
        if (ui_button(U, UI_ID, rect_make(bx - 120, Y - 6, 120, 26), "Criar Usuário", true) || go1) {
            if (strcmp(p1, p2)) strcpy(err, "As senhas não coincidem.");
            else if (account_create(nm, full[0] ? full : nm, p1, admin, err, sizeof err)) {
                char m[128];
                snprintf(m, sizeof m, "Conta \"%s\" criada", full[0] ? full : nm);
                notify(m, "Ela aparece na janela de início de sessão.");
                creating = false;
                err[0] = 0;
            }
            memset(p1, 0, sizeof p1); memset(p2, 0, sizeof p2);
        }
        if (ui_button(U, UI_ID, rect_make(bx - 230, Y - 6, 100, 26), "Cancelar", false)) creating = false;
        Y += 30;
        return;
    }
    if (sel < 0) { if (err[0]) footnote(err, RGB(230, 70, 60)); return; }
    char name[64], fn[96];
    bool adm, hp;
    account_get(sel, name, sizeof name, fn, sizeof fn, &adm, &hp);
    char t[128];
    snprintf(t, sizeof t, "%s", fn[0] ? fn : name);
    section(t);
    rect_t gd = group(3);
    row_value(row(gd, 0, "Nome da conta"), name);
    row_value(row(gd, 1, "Tipo"), adm ? "Administrador" : "Padrão");
    snprintf(t, sizeof t, "/Users/%s", name);
    row_value(row(gd, 2, "Pasta pessoal"), t);
    section(hp ? "Alterar senha" : "Definir senha");
    rect_t gp = group(hp ? 3 : 2);
    int k = 0;
    rect_t r;
    if (hp) { r = row(gp, k++, "Senha atual"); secure_field(U, 1811, rect_make(r.x, r.y + 7, r.w, 26), oldp, sizeof oldp, ""); }
    r = row(gp, k++, "Nova senha"); secure_field(U, 1812, rect_make(r.x, r.y + 7, r.w, 26), np1, sizeof np1, "vazia remove a senha");
    r = row(gp, k++, "Confirmar"); bool go2 = secure_field(U, 1813, rect_make(r.x, r.y + 7, r.w, 26), np2, sizeof np2, "");
    if (err[0]) footnote(err, RGB(230, 70, 60));
    int bx = X0 + W0;
    if (ui_button(U, UI_ID, rect_make(bx - 140, Y - 6, 140, 26), hp ? "Alterar Senha" : "Definir Senha", true) || go2) {
        if (strcmp(np1, np2)) strcpy(err, "As senhas novas não coincidem.");
        else if (account_set_password(name, oldp, np1, err, sizeof err)) {
            notify(np1[0] ? "Senha alterada" : "Senha removida", fn[0] ? fn : name);
            err[0] = 0;
        }
        memset(oldp, 0, sizeof oldp); memset(np1, 0, sizeof np1); memset(np2, 0, sizeof np2);
    }
    bool can_del = strcmp(name, account_current()) != 0 && strcmp(name, ACCOUNT_DEFAULT) != 0;
    if (ui_button_style(U, UI_ID, rect_make(X0, Y - 6, 140, 26), "Apagar Conta…", BTN_TINTED_RED, can_del, NULL)) {
        if (account_delete(name, true, err, sizeof err)) { notify("Conta apagada", fn[0] ? fn : name); sel = -1; err[0] = 0; }
    }
    Y += 30;
}

/* ================================================================== barra lateral */
static const int sb_groups[][6] = {
    {P_WIFI, P_BT, P_REDE, -1},
    {P_NOTIF, P_SOM, -1},
    {P_GERAL, P_APAR, P_MESA, P_MONITOR, P_WALL, -1},
    {P_PRIV, P_USERS, -1},
    {P_TECLADO, -1},
};

static bool sb_item(ui_t *u, int id, rect_t r, int p, bool selected) {
    bool c = ui_clicked(u, id, r);
    if (selected) gfx_round_rect(u->s, r.x, r.y, r.w, r.h, 8, T.accent);
    else if (ui_hit(u, r) && u->down) gfx_round_rect(u->s, r.x, r.y, r.w, r.h, 8, T.sidebar_sel);
    pane_icon(u->s, p, r.x + 6, r.y + (r.h - 22) / 2, 22);
    text_draw_ellipsis(u->s, F(FONT_REGULAR), 13, r.x + 36, r.y + r.h / 2 + 5, r.w - 42, selected ? RGB(255, 255, 255) : T.sidebar_text, pane_names[p]);
    return c;
}

static void paint_sidebar(window_t *w, ui_t *u) {
    int sbw = w->sidebar_w, tb = w->toolbar_h;
    int ix = 10, iw = sbw - 20;
    ui_searchfield(u, 7001, rect_make(ix, tb, iw, 28), sb_query, sizeof sb_query);
    rect_t view = rect_make(0, tb + 36, sbw, u->ch - tb - 36);
    rect_t pc = surface_push_clip(u->s, view);
    int saved_mx = u->mx;
    if (!rect_contains(view, u->mx, u->my)) u->mx = -10000;
    int y = view.y + 4 - sb_scroll, y0 = y;
    if (!sb_query[0]) {
        /* cartão da conta */
        rect_t ac = rect_make(ix, y, iw, 50);
        bool s = pane == P_USERS;
        if (s) gfx_round_rect(u->s, ac.x, ac.y, ac.w, ac.h, 10, T.sidebar_sel);
        avatar(ac.x + 6, ac.y + 7, 36, account_fullname());
        text_draw_ellipsis(u->s, F(FONT_SEMIBOLD), 13, ac.x + 50, ac.y + 22, ac.w - 56, T.sidebar_text, account_fullname());
        text_draw_ellipsis(u->s, F(FONT_REGULAR), 11, ac.x + 50, ac.y + 38, ac.w - 56, T.text2, "Conta do mOS");
        if (ui_clicked(u, 7002, ac)) go(P_USERS, true);
        y += 60;
        for (u32 gi = 0; gi < ARRAY_LEN(sb_groups); gi++) {
            for (int k = 0; sb_groups[gi][k] >= 0; k++) {
                int p = sb_groups[gi][k];
                if (p == P_USERS) continue;   /* já está no cartão da conta */
                if (sb_item(u, 7100 + p, rect_make(ix, y, iw, 28), p, pane_parent(pane) == p)) go(p, true);
                y += 28;
            }
            y += 10;
        }
    } else {
        int found = 0;
        for (int p = 0; p < P_COUNT; p++) {
            if (!icontains(pane_names[p], sb_query) && !icontains(pane_kw[p], sb_query)) continue;
            if (sb_item(u, 7100 + p, rect_make(ix, y, iw, 28), p, pane == p)) go(p, true);
            y += 28;
            found++;
        }
        if (!found) text_draw_center(u->s, F(FONT_REGULAR), 13, rect_make(ix, y + 20, iw, 20), T.text3, "Nenhum resultado");
    }
    sb_content_h = y + sb_scroll - y0 + 10;
    u->mx = saved_mx;
    surface_pop_clip(u->s, pc);
    ui_scroll(u, 7003, view, sb_content_h, &sb_scroll);
}

/* ================================================================== janela */
static void paint(window_t *w, ui_t *u) {
    U = u;
    int sbw = w->sidebar_w, tb = w->toolbar_h;
    paint_sidebar(w, u);
    /* cabeçalho: voltar/avançar + título */
    int tbx = sbw > 0 ? sbw + 16 : 129;
    rect_t rb = rect_make(tbx, 0, 36, 36), rf = rect_make(tbx + 36, 0, 36, 36);
    bool back = hpos > 0, fwd = hpos < nhist - 1;
    ui_tool_group(u, rect_make(rb.x, rb.y, 72, 36));
    gfx_fill(u->s, rf.x, rf.y + 7, 1, rf.h - 14, T.separator);
    glyph_chevron(u->s, rb.x + 10, rb.y + 10, 16, back ? T.control_text : T.text3, 1);
    glyph_chevron(u->s, rf.x + 10, rf.y + 10, 16, fwd ? T.control_text : T.text3, 0);
    if (back && ui_clicked(u, 7010, rb)) { hpos--; go(hist[hpos], false); }
    if (fwd && ui_clicked(u, 7011, rf)) { hpos++; go(hist[hpos], false); }
    ui_title(u, tbx + 84, 0, 36, u->cw - tbx - 100, pane_names[pane], NULL);
    /* conteúdo rolável */
    rect_t view = rect_make(sbw, tb, u->cw - sbw, u->ch - tb);
    W0 = MIN(view.w - 56, 640);
    X0 = view.x + (view.w - W0) / 2;
    int p = pane;
    Y = view.y + 12 - scroll[p];
    int y0 = Y;
    rect_t pc = surface_push_clip(u->s, view);
    int saved_mx = u->mx;
    bool saved_pressed = u->pressed;
    if (!rect_contains(view, u->mx, u->my)) { u->mx = -10000; u->pressed = false; }
    switch (p) {
    case P_USERS: paint_users(); break;
    case P_WIFI: paint_wifi(); break;
    case P_BT: paint_bt(); break;
    case P_REDE: paint_rede(); break;
    case P_NOTIF: paint_notif(); break;
    case P_SOM: paint_som(); break;
    case P_GERAL: paint_geral(); break;
    case P_APAR: paint_apar(); break;
    case P_MESA: paint_mesa(); break;
    case P_MONITOR: paint_monitor(); break;
    case P_WALL: paint_wall(); break;
    case P_PRIV: paint_priv(); break;
    case P_TECLADO: paint_teclado(); break;
    case P_SOBRE: paint_sobre(); break;
    case P_UPDATE: paint_update(); break;
    case P_STORAGE: paint_storage(); break;
    case P_DATA: paint_data(); break;
    case P_IDIOMA: paint_idioma(); break;
    case P_STARTUP: paint_startup(); break;
    }
    content_h[p] = Y - y0 + 12;
    u->mx = saved_mx;
    u->pressed = saved_pressed;
    surface_pop_clip(u->s, pc);
    ui_scroll(u, 7004, view, content_h[p], &scroll[p]);
    if (p == P_MONITOR) paint_arrange_sheet();   /* folha Organizar, por cima de tudo */
    /* atalhos: ⌘[ voltar, ⌘] avançar, ⌘F busca */
    if (u->key && (u->mods & MOD_CMD)) {
        if (u->key == '[' && back) { hpos--; go(hist[hpos], false); u->key_used = true; }
        else if (u->key == ']' && fwd) { hpos++; go(hist[hpos], false); u->key_used = true; }
        else if (u->key == 'f' || u->key == 'F') { u->focus = 7001; u->key_used = true; }
    }
    wm_set_title(w, pane_names[pane]);
}

static void on_close(window_t *w) { pw = NULL; }

static void launch(app_t *a) {
    if (pw) { wm_focus(pw); return; }
    pw = wm_create(&app_preferencias, "Ajustes do Sistema", -1, -1, 900, 640, WIN_RESIZABLE);
    if (!pw) return;
    pw->paint = paint;
    pw->on_close = on_close;
    pw->toolbar_h = TOOLBAR_H;
    pw->sidebar_w = 240;
    pw->title_x = -1;
    pw->min_w = 720;
    pw->min_h = 460;
    nhist = 0; hpos = 0;
    go(pane, true);
}

/* compatibilidade: índices antigos (2 som, 3 rede, 4 bluetooth, 5 data, 9 atualização) */
void prefs_open_pane(int p) {
    static const int legacy[] = {P_GERAL, P_MESA, P_SOM, P_REDE, P_BT, P_DATA, P_TECLADO, P_MONITOR, P_SOBRE, P_UPDATE, P_USERS};
    if (!pw) launch(&app_preferencias);
    if (p >= 0 && p < (int)ARRAY_LEN(legacy)) go(legacy[p], true);
}

/* abre um painel pelo nome (ex.: "wifi", "som"); usado pelos menus da barra */
void prefs_open_named(const char *name) {
    for (int p = 0; p < P_COUNT; p++)
        if (icontains(pane_names[p], name) || icontains(pane_kw[p], name)) { if (!pw) launch(&app_preferencias); go(p, true); return; }
}

app_t app_preferencias = {.name = "Ajustes do Sistema", .exec = "preferencias", .draw_icon = icon_prefs, .launch = launch, .pinned = true};
