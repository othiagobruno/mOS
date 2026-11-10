/* mOS — ambiente de trabalho: papel de parede, barra de menus, Dock, menus, Busca, notificações e alertas */
#include "ui.h"
#include "vfs.h"
#include "gpu.h"
#include "dockbadge.h"

/* ---- coordenadas
 * O desktop (barra de menus, Dock, menus, Busca, notificações...) trabalha no sistema do monitor principal,
 * com (0,0) no canto dele. O resto do sistema usa o espaço de todos os monitores, onde o principal pode
 * estar em qualquer lugar (Ajustes > Monitores). A tradução fica toda nas bordas deste arquivo: o que
 * entra (mouse, eventos) vira local, o que sai (dano, cópia para a tela, retângulos devolvidos) vira global. */
static inline int PX(void) { return display_rect(0).x; }
static inline int PY(void) { return display_rect(0).y; }
static inline rect_t to_global(rect_t r) { r.x += PX(); r.y += PY(); return r; }
#define wm_damage(r)  wm_damage(to_global(r))
#define fb_flush(r)   fb_flush(to_global(r))
#define wm_mouse_x()  (wm_mouse_x() - PX())
#define wm_mouse_y()  (wm_mouse_y() - PY())

/* sessão de usuário (ui/login.c, contas em proc/account) */
extern void login_draw(surface_t *s);
extern void setup_draw(surface_t *s);
extern bool setup_handle_event(input_event_t *e);
extern void setup_tick(void);
extern bool setup_active(void);
extern bool setup_boot(void);
extern bool login_handle_event(input_event_t *e);
extern void login_boot(void);
extern void login_tick(void);
extern void login_logout(void);
extern bool login_active(void);
extern const char *account_fullname(void);
extern void tcc_prompt_tick(void);
extern void tcc_prompt_draw(surface_t *s, int sw, int sh);
extern bool tcc_prompt_mouse(int x, int y, int sw, int sh, bool press, bool release);
extern bool tcc_prompt_key(int key);


int  g_wallpaper_style = 3;   /* 0 dunas dinâmicas, 1 dunas dia, 2 dunas noite, 3 seda (padrão) */
bool g_dock_magnify = false; /* como no sistema: ampliação desligada por padrão (liga na Central de Controle) */
int  g_dock_size = 50;
extern int g_kbd_layout;

static surface_t *wallpaper, *wallpaper_blur;
static int SW, SH;

/* ============================================================ papel de parede procedural */
static float hash1(int x) { u32 h = (u32)x * 374761393u; h = (h ^ (h >> 13)) * 1274126177u; return (h & 0xFFFF) / 65535.0f; }

static float ridge(int layer, float x) {
    float s = 0;
    float f = 0.0028f * (1 + layer * 0.45f);
    s += fsin(x * f + layer * 1.7f) * 26 * (1 + layer * 0.3f);
    s += fsin(x * f * 2.3f + layer * 0.9f) * 11;
    s += fsin(x * f * 5.1f + layer * 2.1f) * 3;
    float c = fsin(x * f * 0.7f + layer * 3.3f + 1.0f);
    s -= fsqrt(c * c + 0.02f) * 34 * (1 + layer * 0.25f);
    return s;
}

static u32 lerp3(u32 a, u32 b, float t) { return color_mix(a, b, CLAMP((int)(t * 255), 0, 255)); }
static inline float fx_dummy(int x, int W) { return (float)x / W; }

/* "Seda": arte original — folhas de tecido sobrepostas em diagonal, com lábio de luz na borda,
 * sombra projetada na folha de trás e um volume suave; tons areia → ardósia. */
static void render_silk(surface_t *s, bool night) {
    int W = s->w, H = s->h;
    enum { NS = 5 };
    static const u32 day[NS] = {RGB(232, 220, 202), RGB(204, 186, 164), RGB(176, 160, 146), RGB(138, 134, 150), RGB(102, 104, 128)};
    static const u32 ngt[NS] = {RGB(76, 70, 66), RGB(62, 58, 58), RGB(50, 48, 54), RGB(40, 40, 52), RGB(28, 30, 44)};
    const float base[NS] = {-0.15f, 0.12f, 0.36f, 0.58f, 0.80f};
    const float amp[NS] = {0.20f, 0.17f, 0.15f, 0.13f, 0.11f};
    const float frq[NS] = {0.85f, 1.05f, 0.95f, 1.2f, 1.1f};
    const float ph[NS] = {0.3f, 1.7f, 3.1f, 4.4f, 5.6f};
    float *curve = kmalloc((usize)W * NS * sizeof(float));
    for (int i = 0; i < NS; i++)
        for (int x = 0; x < W; x++) {
            float fx = (float)x / W;
            /* curva em S que desce da esquerda para a direita */
            curve[i * W + x] = base[i] + amp[i] * fsin(fx * frq[i] * 2 * PI_F + ph[i]) + amp[i] * 0.35f * fsin(fx * frq[i] * 4.6f + ph[i] * 1.3f) - (fx - 0.5f) * 0.55f;
        }
    u32 hi = night ? RGB(150, 140, 130) : RGB(252, 249, 242);
    for (int y = 0; y < H; y++) {
        float fy = (float)y / H;
        u32 *row = s->px + (usize)y * s->stride;
        for (int x = 0; x < W; x++) {
            /* folha mais à frente que cobre este ponto */
            int top = -1;
            float e = 0;
            for (int i = NS - 1; i >= 0; i--) {
                float d = fy - curve[i * W + x];
                if (d >= 0) { top = i; e = d; break; }
            }
            u32 c;
            if (top < 0) {
                c = night ? RGB(86, 80, 74) : RGB(240, 232, 218);
                e = 0.5f;
            } else {
                u32 b = night ? ngt[top] : day[top];
                /* escurece suavemente para dentro da folha e ganha um volume claro logo após a borda */
                float dark = CLAMP(e * 1.6f, 0.0f, 0.45f);
                c = color_mix(b, night ? RGB(12, 12, 20) : RGB(70, 60, 64), (int)(dark * 255));
                float vol = fexp(-((e - 0.05f) * (e - 0.05f)) / 0.0018f) * 0.32f;
                c = color_mix(c, hi, (int)(vol * 255));
                /* lábio brilhante na borda */
                float lip = fexp(-e * 260.0f);
                c = color_mix(c, hi, (int)(CLAMP(lip, 0.0f, 1.0f) * 235));
            }
            /* sombra projetada pela próxima folha (logo acima da borda dela) */
            if (top < NS - 1) {
                float d2 = curve[(top + 1) * W + x] - fy;
                if (d2 > 0 && d2 < 0.06f) {
                    float k = 1 - d2 / 0.06f;
                    c = color_mix(c, RGB(20, 16, 20), (int)(k * k * (night ? 120 : 70)));
                }
            }
            /* luz ambiente: mais quente no alto à esquerda, mais fria embaixo à direita */
            float amb = (fx_dummy(x, W) + fy) * 0.5f;
            c = color_mix(c, night ? RGB(30, 34, 60) : RGB(120, 130, 168), (int)(CLAMP(amb - 0.45f, 0.0f, 0.5f) * 120));
            int n = (int)(hash1(x * 7919 + y * 104729) * 6) - 3;
            row[x] = color_lighten(c, n) | 0xFF000000u;
        }
    }
    kfree(curve);
}

static void render_wallpaper(surface_t *s, bool night, int style) {
    int W = s->w, H = s->h;
    if (style == 3) { render_silk(s, night); return; }
    u32 sky_top = night ? RGB(9, 13, 32) : RGB(52, 86, 150);
    u32 sky_mid = night ? RGB(28, 32, 66) : RGB(150, 160, 196);
    u32 sky_hor = night ? RGB(70, 58, 98) : RGB(246, 186, 140);
    float horizon = H * 0.52f;
    float sunx = W * 0.68f, suny = horizon - H * 0.06f;
    for (int y = 0; y < H; y++) {
        float t = y / horizon;
        u32 base = t < 0.6f ? lerp3(sky_top, sky_mid, t / 0.6f) : lerp3(sky_mid, sky_hor, CLAMP((t - 0.6f) / 0.4f, 0.0f, 1.0f));
        u32 *row = s->px + (usize)y * s->stride;
        for (int x = 0; x < W; x++) {
            float dx = (x - sunx) / W, dy = (y - suny) / H;
            float g = fexp(-(dx * dx * 9 + dy * dy * 30)) * (night ? 0.35f : 0.75f);
            u32 c = lerp3(base, night ? RGB(160, 150, 210) : RGB(255, 226, 180), g);
            row[x] = c;
        }
    }
    if (night) {
        srand32(1234);
        for (int i = 0; i < W * H / 900; i++) {
            int x = rand32() % W, y = rand32() % (int)horizon;
            int b = 120 + rand32() % 135;
            float fade = 1.0f - (float)y / horizon;
            gfx_pixel(s, x, y, RGB(b, b, MIN(255, b + 20)), (int)(255 * fade));
        }
        gfx_circle(s, sunx, suny - H * 0.2f, 16, RGBA(235, 235, 220, 230));
        gfx_circle(s, sunx + 6, suny - H * 0.2f - 4, 14, RGBA(28, 32, 66, 200));
    }
    /* camadas de dunas, do fundo para a frente */
    static const u32 lit_day[4] = {RGB(214, 150, 120), RGB(222, 140, 96), RGB(206, 118, 74), RGB(176, 92, 56)};
    static const u32 shd_day[4] = {RGB(160, 112, 120), RGB(150, 92, 90), RGB(122, 68, 60), RGB(90, 46, 40)};
    static const u32 lit_ngt[4] = {RGB(72, 66, 112), RGB(62, 56, 98), RGB(50, 44, 82), RGB(40, 34, 66)};
    static const u32 shd_ngt[4] = {RGB(42, 40, 76), RGB(34, 32, 64), RGB(26, 24, 50), RGB(18, 16, 36)};
    for (int L = 0; L < 4; L++) {
        float base = horizon + H * (0.02f + L * 0.11f);
        u32 lit = night ? lit_ngt[L] : lit_day[L], shd = night ? shd_ngt[L] : shd_day[L];
        for (int x = 0; x < W; x++) {
            float fx = x * (1440.0f / W);
            float h0 = base + ridge(L, fx);
            float slope = ridge(L, fx + 2) - ridge(L, fx - 2);
            float lightk = CLAMP(0.5f - slope * 0.16f, 0.0f, 1.0f);
            int y0 = (int)h0;
            for (int y = MAX(0, y0); y < H; y++) {
                float depth = (y - h0) / (H * 0.35f);
                float fade = CLAMP(depth * 3.0f, 0.0f, 1.0f);
                float k = CLAMP(lightk * (1 - fade) + 0.42f * fade - depth * 0.5f, 0.0f, 1.0f);
                u32 c = lerp3(shd, lit, k);
                /* granulação de areia */
                int n = (int)(hash1(x * 7919 + y * 104729 + L) * 10) - 5;
                c = color_lighten(c, n);
                u32 *p = &s->px[(usize)y * s->stride + x];
                if (y == y0) *p = color_mix(*p, c, (int)((1 - (h0 - y0)) * 255));
                else *p = c;
            }
        }
    }
    for (int i = 0; i < W * H; i++) s->px[i] |= 0xFF000000;
}

static bool wallpaper_night(void) {
    if (g_wallpaper_style == 1) return false;
    if (g_wallpaper_style == 2) return true;
    if (g_wallpaper_style == 0) {
        if (T.dark) return true;
        datetime_t dt;
        rtc_now(&dt);
        return dt.hour < 6 || dt.hour >= 19;
    }
    return T.dark;
}

void desktop_rebuild_wallpaper(void) {
    if (!wallpaper) { wallpaper = surface_new(SW, SH); wallpaper_blur = surface_new(SW, SH); }
    surface_resize(wallpaper, SW, SH);           /* o principal pode ter mudado (Ajustes > Monitores) */
    surface_resize(wallpaper_blur, SW, SH);
    render_wallpaper(wallpaper, wallpaper_night(), g_wallpaper_style);
    /* versão desfocada inteira (Launchpad e fundo de vidro estático) */
    gfx_blit(wallpaper_blur, wallpaper, 0, 0);
    gfx_blur(wallpaper_blur, rect_make(0, 0, SW, SH), 26);
    extern void desktop_measure_bar(void);
    desktop_measure_bar();
    wm_damage_all();
}

surface_t *desktop_wallpaper_blur(void) { return wallpaper_blur; }

/* miniatura de um estilo de papel de parede (Ajustes > Papel de Parede) */
void desktop_wallpaper_thumb(surface_t *dst, int style) {
    bool night = style == 2 || (style == 3 && T.dark) || (style == 0 && T.dark);
    /* o desenho usa proporções da tela: renderiza em tamanho real e reduz */
    surface_t *full = surface_new(SW, SH);
    if (!full) { render_wallpaper(dst, night, style); return; }
    render_wallpaper(full, night, style);
    gfx_blit_scaled(dst, full, 0, 0, dst->w, dst->h, 255);
    surface_free(full);
}

/* ============================================================ estado de menus */
enum { MK_NONE, MK_BAR, MK_POPUP, MK_CONTEXT };
enum { ACT_ABOUT = 1001, ACT_PREFS, ACT_ACTIVITY, ACT_FORCEQUIT, ACT_RESTART, ACT_SHUTDOWN, ACT_LOCK,
       ACT_APP_ABOUT = 2001, ACT_APP_QUIT, ACT_APP_HIDE, ACT_APP_PREFS,
       ACT_WIN_MIN = 3001, ACT_WIN_ZOOM, ACT_WIN_CLOSE, ACT_WIN_FRONT, ACT_WIN_LEFT, ACT_WIN_RIGHT, ACT_WIN_FILL, ACT_MISSION, ACT_WIN_N = 3100,
       ACT_HELP = 4001, ACT_NET_PREFS = 5001, ACT_BT_PREFS, ACT_SND_PREFS, ACT_DATE_PREFS, ACT_DARK_TOGGLE };

#define TITLE_LOGO   100
#define TITLE_NET    200
#define TITLE_BT     201
#define TITLE_VOL    202
#define TITLE_CLOCK  203
#define TITLE_SEARCH 204
#define TITLE_NOTIF  205
#define TITLE_CC     206
#define TITLE_FOCUS  207
#define TITLE_BATT   208
void glyph_focus_moon(surface_t *s, int x, int y, int sz, u32 c);
#include "power.h"
/* bateria da barra: contorno arredondado, carga proporcional (vermelha abaixo de 20%, verde carregando),
 * polo à direita e raio quando ligada na tomada */
static void glyph_battery(surface_t *s, int x, int y, const battery_status_t *b, u32 c) {
    float w = 24, h = 12;
    gfx_round_rect_stroke(s, x + 0.5f, y + 0.5f, w - 1, h - 1, 3.5f, 1.1f, WITH_ALPHA(c, 150));
    gfx_round_rect(s, x + w + 1, y + 4, 2, 4, 1, WITH_ALPHA(c, 150));
    u32 fill = b->charging ? RGB(52, 199, 89) : b->percent < 20 ? RGB(255, 59, 48) : c;
    float fw = (w - 4) * CLAMP(b->percent, 0, 100) / 100.0f;
    if (fw > 0.5f) gfx_round_rect(s, x + 2, y + 2, MAX(2.0f, fw), h - 4, 2, fill);
    if (b->on_ac) {
        float cx = x + w / 2.0f, cy = y + h / 2.0f;
        float bolt[] = {cx + 1.5f, cy - 6, cx - 3.5f, cy + 1, cx - 0.2f, cy + 1, cx - 1.5f, cy + 6, cx + 3.5f, cy - 1, cx + 0.2f, cy - 1};
        gfx_polygon(s, bolt, 6, c);
    }
}
static char batt_l1[64], batt_l2[64], batt_l3[64];
#include "wifi.h"
static wifi_network_t wifi_nets[10];
static int wifi_nnets;
#define ACT_WIFI_TOGGLE 6300
#define ACT_WIFI_NET0   6301
/* cadeado pequeno (redes protegidas) */
static void glyph_lock_small(surface_t *s, int x, int y, int sz, u32 c) {
    float w = sz * 0.62f, bx = x + (sz - w) / 2, by = y + sz * 0.45f;
    gfx_round_rect(s, bx, by, w, sz * 0.48f, 2, c);
    gfx_ring_arc(s, x + sz / 2.0f, by, w * 0.32f, w * 0.32f - 1.5f, PI_F, 2 * PI_F, c);
    gfx_fill(s, (int)(bx + w * 0.18f), (int)(by - 2), 2, 2, c);
    gfx_fill(s, (int)(bx + w * 0.82f) - 2, (int)(by - 2), 2, 2, c);
}

static struct {
    int kind;
    int title;            /* índice do título aberto na barra */
    int x, y, w, h;
    menu_item_t items[48];
    int n, hover;
    window_t *popup_win;
    int popup_id;
    bool armed;           /* botão ainda pressionado desde a abertura */
    u64 opened_at;
} M;

static struct { window_t *win; int id, val; bool ready; } popup_res;

/* títulos da barra: posições calculadas no desenho */
typedef struct { int title; int x, w; } bar_title_t;
static bar_title_t bar[16];
static int nbar;
static bar_title_t status[8];
static int nstatus;

/* ============================================================ Dock */
typedef struct { app_t *app; float x, size; bool sep; window_t *win; } dock_slot_t;
static dock_slot_t dslots[40];
static int ndslots;
static float dock_hover_x = -1;
static rect_t dock_r;
static int dock_pressed = -1;
static app_t trash_app;
bool g_dock_autohide;
static float dock_hide;                 /* ocultar automaticamente: 0 visível, 1 escondido */
static u64 dock_leave_t;
static app_t *dock_order[32];           /* ordem dos ícones mantidos no Dock (o usuário arrasta para reordenar) */
static int ndock_order = -1;
static struct { bool active; app_t *app; int sx, sy; bool remove; } DD;   /* arrastando um ícone */
static app_t *dock_ctx_app;             /* app do menu de contexto do Dock */
static window_t *dock_ctx_win;
static void dock_click(app_t *a);
static void dock_toggle_keep(app_t *a);
/* tela cheia: a barra de menus só aparece com o mouse no topo */
static bool fs_bar;
static bool bar_visible(void) { return !wm_space_fullscreen() || fs_bar; }
/* modos de Foco: silenciam as notificações e mostram um símbolo na barra de menus */
int g_focus_mode;   /* 0 nenhum */
extern bool g_notify_dnd;
static const char *focus_names[] = {"Nenhum", "Não Perturbe", "Pessoal", "Trabalho", "Dormir"};
#define ACT_FOCUS_0 6100
#define ACT_WIN_FULL 6200
#define ACT_TAB_MERGE 6201
#define ACT_TAB_DETACH 6202
#define ACT_TAB_NEXT 6203
#define ACT_TAB_PREV 6204
#define ACT_ABOUT_MAC 6205
enum { ACT_DOCK_KEEP = 6001, ACT_DOCK_LOGIN, ACT_DOCK_SHOW, ACT_DOCK_HIDE, ACT_DOCK_QUIT, ACT_DOCK_OPEN, ACT_DOCK_EMPTY,
       ACT_DOCK_RESTORE, ACT_DOCK_CLOSEWIN };

/* ============================================================ notificações */
typedef struct { char title[64], body[160]; u64 t0; app_t *app; int pid, token; } notif_t;
static notif_t notifs[16]; /* histórico (a Central de Notificações mostra todas) */
static void nc_toggle(void);
static bool nc_visible(void);
static int nnotif;

/* ============================================================ Busca */
static bool spot_open;
static char spot_q[96];
static int spot_sel, spot_caret;

/* ============================================================ Central de Controle e Launchpad */
static bool cc_open, lp_open;
static int cc_drag = -1;           /* slider sendo arrastado: 0 volume, 1 tamanho do Dock */
static char lp_q[64];
static int lp_hover = -1;
static u64 lp_t0;           /* != 0: reiniciar a animação de entrada no próximo quadro */
static bool lp_animating;
static app_t launch_app;

/* ícone da Central de Controle: dois interruptores empilhados */
static void glyph_controls(surface_t *s, int x, int y, int sz, u32 c) {
    float h = sz * 0.36f, w = sz;
    gfx_round_rect_stroke(s, x, y + 1, w, h, h / 2, 1.3f, c);
    gfx_circle(s, x + w - h / 2, y + 1 + h / 2, h / 2 - 2.2f, c);
    gfx_round_rect_stroke(s, x, y + sz - h - 1, w, h, h / 2, 1.3f, c);
    gfx_circle(s, x + h / 2, y + sz - 1 - h / 2, h / 2 - 2.2f, c);
}

/* arte original do ícone do Launchpad: quadrado de vidro azul-violeta com grade 3x3 de pontos coloridos */
static void draw_launch_icon(surface_t *s, int x, int y, int size) {
    float r = size * 0.225f, m = size * 0.06f, w = size - 2 * m;
    gfx_round_rect_grad(s, x + m, y + m, w, w, r, RGB(92, 104, 240), RGB(40, 40, 150));
    gfx_round_rect_grad(s, x + m, y + m, w, w * 0.5f, r, RGBA(255, 255, 255, 60), RGBA(255, 255, 255, 0));
    gfx_specular_rim(s, x + m, y + m, w, w, r, 150);
    static const u32 dots[9] = {RGB(255, 94, 87), RGB(255, 189, 46), RGB(52, 210, 90), RGB(64, 160, 255), RGB(176, 110, 255),
                                RGB(255, 120, 190), RGB(90, 220, 220), RGB(255, 150, 60), RGB(240, 240, 250)};
    float cell = w / 3.6f, d = cell * 0.62f;
    for (int i = 0; i < 9; i++) {
        float cx = x + m + w / 2 + (i % 3 - 1) * cell, cy = y + m + w / 2 + (i / 3 - 1) * cell;
        gfx_round_rect(s, cx - d / 2, cy - d / 2, d, d, d * 0.3f, dots[i]);
    }
}

/* ============================================================ alerta */
static struct {
    bool open;
    char title[256], msg[320];
    const char *buttons[3];
    int nb, hover, pressed;
    alert_cb cb;
    void *ctx;
} A;

static int shutdown_pending = -1;
static u64 shutdown_at;

/* ============================================================ desenho: barra de menus */
static app_t *active_app(void) {
    app_t *a = wm_active_app();
    return a ? a : apps_find("arquivos");
}

static const char *month_long[12] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};

/* barra de menus no padrão atual: totalmente transparente; o texto fica preto sobre papéis de parede
 * claros e branco sobre escuros (decidido pela luminância do papel de parede sob a barra) */
static bool bar_on_light = true;
static const char *bar_wday[7] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};
static const char *bar_mon[12] = {"jan", "fev", "mar", "abr", "mai", "jun", "jul", "ago", "set", "out", "nov", "dez"};

static void measure_bar_luminance(void) {
    if (!wallpaper_blur) return;
    u64 sum = 0, n = 0;
    for (int y = 0; y < MENUBAR_H; y += 2)
        for (int x = 0; x < SW; x += 4) {
            u32 p = wallpaper_blur->px[(usize)y * wallpaper_blur->stride + x];
            sum += (C_R(p) * 77 + C_G(p) * 150 + C_B(p) * 29) >> 8;
            n++;
        }
    bar_on_light = n && sum / n > 150;
}

void desktop_measure_bar(void) { measure_bar_luminance(); }

#define BAR_PAD 20     /* margem esquerda/direita */
#define BAR_GAP 9      /* metade do espaço entre títulos */

static void draw_menubar(surface_t *s) {
    int h = MENUBAR_H;
    bool light = bar_on_light && !lp_open;
    u32 tc = light ? RGB(0, 0, 0) : RGB(255, 255, 255);
    font_t *fr = font_get(FONT_MEDIUM), *fb = font_get(FONT_BOLD);
    float fs = 13.5f;
    int base = (h - (text_ascent(fr, fs) + text_descent(fr, fs))) / 2 + text_ascent(fr, fs);
    /* item aberto: cápsula translúcida */
#define BAR_HL(xx, ww) gfx_round_rect(s, (xx), 3, (ww), h - 6, (h - 6) / 2.0f, light ? RGBA(0, 0, 0, 30) : RGBA(255, 255, 255, 46))
    datetime_t dt;
    rtc_now(&dt);
    char clock[48];
    /* "Qua 1 de abr  9:41" — dia e hora separados por espaço duplo, como no sistema */
    snprintf(clock, sizeof clock, "%s %d de %s  %d:%02d", bar_wday[dt.weekday], dt.day, bar_mon[dt.month - 1], dt.hour, dt.minute);
    if (SW < 1000) snprintf(clock, sizeof clock, "%s  %d:%02d", bar_wday[dt.weekday], dt.hour, dt.minute);
    if (SW < 760) snprintf(clock, sizeof clock, "%d:%02d", dt.hour, dt.minute);
    /* da direita para a esquerda: relógio, Central de Controle, busca, bateria (se houver), rede, Bluetooth, Foco */
    battery_status_t bst;
    battery_status(&bst);
    struct { int title; int w; } items[8];
    int nitems = 0;
    items[nitems++] = (typeof(items[0])){TITLE_CLOCK, 0};
    items[nitems++] = (typeof(items[0])){TITLE_CC, 34};
    if (SW >= 700) {
        items[nitems++] = (typeof(items[0])){TITLE_SEARCH, 34};
        if (bst.present) items[nitems++] = (typeof(items[0])){TITLE_BATT, 40};
        items[nitems++] = (typeof(items[0])){TITLE_NET, 34};
        if (SW >= 1000) {
            items[nitems++] = (typeof(items[0])){TITLE_BT, 30};
            if (g_focus_mode) items[nitems++] = (typeof(items[0])){TITLE_FOCUS, 30};
        }
    }
    items[0].w = text_width(fr, fs, clock) + 2 * BAR_GAP;
    int right_w = BAR_PAD - BAR_GAP;
    for (int i = 0; i < nitems; i++) right_w += items[i].w;
    int limit_x = SW - right_w - 16;

    nbar = 0;
    int x = BAR_PAD - BAR_GAP;
    bool open_logo = M.kind == MK_BAR && M.title == TITLE_LOGO;
    int lw = 16 + 2 * BAR_GAP;
    if (open_logo) BAR_HL(x, lw);
    glyph_mos_logo(s, x + lw / 2.0f, h / 2.0f, 8.0f, tc, 0);
    bar[nbar++] = (bar_title_t){TITLE_LOGO, x, lw};
    x += lw + 8;
    app_t *a = active_app();
    if (a) {
        const char *titles[12];
        int nt = 0;
        titles[nt++] = a->name;
        /* "Janela" e "Ajuda" do sistema só quando o app não traz os dele (o Chrome traz: antes apareciam duas vezes).
         * Os índices nmenus+1/+2 continuam sendo os do sistema; um título que não entra não vira item da barra. */
        bool has_win = false, has_help = false;
        for (int i = 0; i < a->nmenus && nt < 10; i++) {
            const char *t = a->menus[i].title;
            if (t && (!strcmp(t, "Janela") || !strcmp(t, "Window"))) has_win = true;
            if (t && (!strcmp(t, "Ajuda") || !strcmp(t, "Help"))) has_help = true;
            titles[nt++] = t;
        }
        titles[nt++] = has_win ? NULL : "Janela";
        titles[nt++] = has_help ? NULL : "Ajuda";
        for (int i = 0; i < nt; i++) {
            if (!titles[i]) continue;
            font_t *f = i == 0 ? fb : fr;
            int w = text_width(f, fs, titles[i]) + 2 * BAR_GAP;
            if (x + w > limit_x) break;
            bool open = M.kind == MK_BAR && M.title == i;
            if (open) BAR_HL(x, w);
            text_draw(s, f, fs, x + BAR_GAP, base, tc, titles[i]);
            bar[nbar++] = (bar_title_t){i, x, w};
            x += w + (i == 0 ? 4 : 0);
        }
    }
    /* direita, da direita para a esquerda: relógio, Central de Controle, busca, rede */
    nstatus = 0;
    int rx = SW - BAR_PAD + BAR_GAP;
    for (int i = 0; i < nitems; i++) {
        int w = items[i].w;
        rx -= w;
        bool open = (M.kind == MK_BAR && M.title == items[i].title) || (items[i].title == TITLE_CC && cc_open);
        if (open) BAR_HL(rx, w);
        int cy = h / 2;
        switch (items[i].title) {
        case TITLE_CC: glyph_controls(s, rx + (w - 17) / 2, cy - 8, 17, tc); break;
        case TITLE_SEARCH: glyph_search(s, rx + (w - 15) / 2, cy - 8, 15, tc); break;
        case TITLE_CLOCK: text_draw(s, fr, fs, rx + BAR_GAP, base, tc, clock); break;
        case TITLE_NET:
            if (net_link_up()) glyph_wifi(s, rx + (w - 19) / 2, cy - 9, 19, tc, 3);
            else glyph_wifi(s, rx + (w - 19) / 2, cy - 9, 19, WITH_ALPHA(tc, 110), 0);
            break;
        case TITLE_BT: glyph_bluetooth(s, rx + (w - 15) / 2, cy - 8, 15, strstr(bt_status(), "Ligado") ? tc : WITH_ALPHA(tc, 120)); break;
        case TITLE_FOCUS: glyph_focus_moon(s, rx + (w - 15) / 2, cy - 8, 15, tc); break;
        case TITLE_BATT: glyph_battery(s, rx + (w - 27) / 2, cy - 6, &bst, tc); break;
        }
        status[nstatus++] = (bar_title_t){items[i].title, rx, w};
    }
}

/* ============================================================ menus suspensos */
static int menu_item_h(menu_item_t *it) { return it->label ? 24 : 11; }

static int list_width(menu_item_t *items, int n);
static int list_height(menu_item_t *items, int n);

static void menu_layout(void) {
    int w = list_width(M.items, M.n), h = list_height(M.items, M.n);
    if (M.kind == MK_POPUP) w = MAX(w, M.w);
    w = MIN(w, SW - 16);
    M.w = w; M.h = MIN(h, SH - MENUBAR_H - 12);
    if (M.x + M.w > SW - 6) M.x = SW - 6 - M.w;
    if (M.x < 6) M.x = 6;
    if (M.y + M.h > SH - 6) M.y = SH - 6 - M.h;
    if (M.y < MENUBAR_H + 2) M.y = MENUBAR_H + 2;
}

static void add_item(const char *label, const char *sc, int action, bool disabled, bool checked) {
    if (M.n >= (int)ARRAY_LEN(M.items)) return;
    menu_item_t *it = &M.items[M.n++];
    memset(it, 0, sizeof *it);
    it->label = label; it->shortcut = sc; it->action = action; it->disabled = disabled; it->checked = checked;
}
static void add_sep(void) { add_item(NULL, NULL, 0, false, false); }

static char about_label[64], quit_label[64], hide_label[64];
static char win_labels[12][64];
static char net_l1[96], net_l2[96], net_l3[96], bt_l1[96], snd_l1[96], snd_in[96], clock_l1[96];

static void submenu_close(void);
static void submenu_open(int parent);

static void open_bar_menu(int title) {
    submenu_close();
    M.kind = MK_BAR;
    M.title = title;
    M.n = 0;
    M.hover = -1;
    M.opened_at = timer_ticks();
    app_t *a = active_app();
    int x = 0;
    for (int i = 0; i < nbar; i++) if (bar[i].title == title) x = bar[i].x;
    for (int i = 0; i < nstatus; i++) if (status[i].title == title) x = status[i].x;
    M.x = x; M.y = MENUBAR_H + 4;
    bool was_cc = cc_open;
    cc_open = false;
    if (title == TITLE_CLOCK) {
        /* o relógio abre a Central de Notificações (histórico + widgets), como no sistema */
        M.kind = MK_NONE;
        nc_toggle();
        return;
    }
    if (title == TITLE_CC) {
        M.kind = MK_NONE;
        cc_open = !was_cc;
        cc_drag = -1;
        wm_damage_all();
        return;
    }
    if (title == TITLE_LOGO) {
        add_item("Sobre este mOS", NULL, ACT_ABOUT, false, false);
        add_sep();
        add_item("Ajustes do Sistema…", NULL, ACT_PREFS, false, false);
        add_item("Monitor de Atividade", NULL, ACT_ACTIVITY, false, false);
        add_sep();
        add_item("Forçar Encerrar…", "⌥⌘⎋", ACT_FORCEQUIT, false, false);
        add_sep();
        add_item(T.dark ? "Usar Aparência Clara" : "Usar Aparência Escura", NULL, ACT_DARK_TOGGLE, false, false);
        add_sep();
        add_item("Reiniciar…", NULL, ACT_RESTART, false, false);
        add_item("Desligar…", NULL, ACT_SHUTDOWN, false, false);
        add_sep();
        static char logout_label[128];
        snprintf(logout_label, sizeof logout_label, "Encerrar Sessão de %s…", account_fullname());
        add_item(logout_label, "⇧⌘Q", ACT_LOCK, false, false);
    } else if (title == 0 && a) {
        snprintf(about_label, sizeof about_label, "Sobre %s", a->name);
        snprintf(hide_label, sizeof hide_label, "Ocultar %s", a->name);
        snprintf(quit_label, sizeof quit_label, "Encerrar %s", a->name);
        add_item(about_label, NULL, ACT_APP_ABOUT, false, false);
        add_sep();
        add_item("Preferências…", "⌘,", ACT_APP_PREFS, false, false);
        add_sep();
        add_item(hide_label, "⌘H", ACT_APP_HIDE, false, false);
        add_sep();
        add_item(quit_label, "⌘Q", ACT_APP_QUIT, !strcmp(a->exec, "arquivos"), false);
    } else if (a && title >= 1 && title <= a->nmenus) {
        menu_t *m = &a->menus[title - 1];
        for (int i = 0; i < m->count; i++) {
            menu_item_t *it = &m->items[i];
            if (M.n < (int)ARRAY_LEN(M.items)) M.items[M.n++] = *it; /* cópia completa: símbolo, cabeçalho, submenu */
        }
    } else if (a && title == a->nmenus + 1) {
        window_t *f = wm_focused();
        add_item("Minimizar", "⌘M", ACT_WIN_MIN, !f, false);
        add_item("Zoom", NULL, ACT_WIN_ZOOM, !f, false);
        add_sep();
        add_item("Mover para a Esquerda da Tela", "⌃⌘←", ACT_WIN_LEFT, !f, false);
        add_item("Mover para a Direita da Tela", "⌃⌘→", ACT_WIN_RIGHT, !f, false);
        add_item("Preencher", "⌃⌘↑", ACT_WIN_FILL, !f, false);
        add_item(f && f->fullscreen ? "Sair da Tela Cheia" : "Entrar em Tela Cheia", "⌃⌘F", ACT_WIN_FULL, !f || !(f->flags & WIN_RESIZABLE), false);
        add_item("Mission Control", "⌃↑", ACT_MISSION, false, false);
        add_sep();
        add_item("Mostrar Aba Anterior", "⌃⇧⇥", ACT_TAB_PREV, !f || !f->tab_group, false);
        add_item("Mostrar Próxima Aba", "⌃⇥", ACT_TAB_NEXT, !f || !f->tab_group, false);
        add_item("Mover Aba para Nova Janela", NULL, ACT_TAB_DETACH, !f || !f->tab_group, false);
        add_item("Mesclar Todas as Janelas", NULL, ACT_TAB_MERGE, !f || wm_count_for(f->app) < 2, false);
        add_sep();
        add_item("Fechar Janela", "⌘W", ACT_WIN_CLOSE, !f, false);
        add_sep();
        add_item("Trazer Tudo para a Frente", NULL, ACT_WIN_FRONT, false, false);
        int k = 0;
        bool any = false;
        for (window_t *w = wm_window_list(); w; w = NULL) { (void)w; }
        extern window_t *wm_nth(int i);
        for (int i = 0; k < 12; i++) {
            window_t *w = wm_nth(i);
            if (!w) break;
            if (w->app != a || w->closing) continue;
            if (!any) { add_sep(); any = true; }
            snprintf(win_labels[k], 64, "%s", w->title[0] ? w->title : "Sem título");
            add_item(win_labels[k], NULL, ACT_WIN_N + i, false, w == f);
            k++;
        }
    } else if (a && title == a->nmenus + 2) {
        add_item("Ajuda do mOS", NULL, ACT_HELP, false, false);
    } else if (title == TITLE_NET) {
        /* Wi-Fi: estado, ligar/desligar e redes próximas (✓ na conectada); depois a Ethernet */
        wifi_status_t ws;
        wifi_status(&ws);
        add_item("Wi-Fi", NULL, 0, false, false);
        M.items[M.n - 1].header = true;
        if (!ws.present) {
            add_item("Nenhum adaptador Wi-Fi compatível", NULL, 0, true, false);
        } else {
            bool on = ws.state != WIFI_OFF;
            add_item(on ? "Desligar Wi-Fi" : "Ligar Wi-Fi", NULL, ACT_WIFI_TOGGLE, false, false);
            if (on) {
                /* resultado da última varredura; só procura de novo se ainda não há nenhuma rede */
                wifi_nnets = wifi_networks(wifi_nets, (int)ARRAY_LEN(wifi_nets));
                if (!wifi_nnets && ws.state != WIFI_SCANNING) wifi_scan();
                add_sep();
                add_item(wifi_nnets ? "Redes Próximas" : "Procurando redes…", NULL, 0, false, false);
                M.items[M.n - 1].header = true;
                for (int k = 0; k < wifi_nnets; k++) {
                    bool cur = ws.state == WIFI_CONNECTED && !strcmp(ws.ssid, wifi_nets[k].ssid);
                    add_item(wifi_nets[k].ssid[0] ? wifi_nets[k].ssid : "(rede oculta)", NULL, ACT_WIFI_NET0 + k, !wifi_nets[k].supported, cur);
                    M.items[M.n - 1].icon = wifi_nets[k].security != WIFI_SEC_OPEN ? glyph_lock_small : NULL;
                }
            }
        }
        bool via_wifi = ws.present && ws.state == WIFI_CONNECTED;
        if (via_wifi) {
            snprintf(net_l2, sizeof net_l2, "Conectado a \"%s\"", ws.ssid);
            add_sep();
            add_item(net_l2, NULL, 0, true, false);
        } else {
            add_sep();
            add_item("Ethernet", NULL, 0, false, false);
            M.items[M.n - 1].header = true;
            net_status_string(net_l1, sizeof net_l1);
            snprintf(net_l2, sizeof net_l2, "%s · %s", net_link_up() ? "Conectada" : "Desconectada", net_device_name());
            add_item(net_l2, NULL, 0, true, false);
            add_item(net_l1, NULL, 0, true, false);
        }
        add_sep();
        add_item("Ajustes de Rede…", NULL, ACT_NET_PREFS, false, false);
        (void)net_l3;
    } else if (title == TITLE_BT) {
        add_item("Bluetooth", NULL, 0, false, false);
        M.items[M.n - 1].header = true;
        snprintf(bt_l1, sizeof bt_l1, "%s", bt_status());
        add_item(bt_l1, NULL, 0, true, false);
        add_sep();
        add_item("Ajustes de Bluetooth…", NULL, ACT_BT_PREFS, false, false);
    } else if (title == TITLE_BATT) {
        battery_status_t b;
        battery_status(&b);
        snprintf(batt_l1, sizeof batt_l1, "Bateria — %d%%", b.percent);
        add_item(batt_l1, NULL, 0, false, false);
        M.items[M.n - 1].header = true;
        snprintf(batt_l2, sizeof batt_l2, "Fonte de Energia: %s", b.source);
        add_item(batt_l2, NULL, 0, true, false);
        if (b.minutes_left >= 0) {
            if (b.charging) snprintf(batt_l3, sizeof batt_l3, "%d:%02d até carga completa", b.minutes_left / 60, b.minutes_left % 60);
            else if (b.on_ac) snprintf(batt_l3, sizeof batt_l3, "Carga completa");
            else snprintf(batt_l3, sizeof batt_l3, "%d:%02d restantes", b.minutes_left / 60, b.minutes_left % 60);
            add_item(batt_l3, NULL, 0, true, false);
        }
        add_sep();
        add_item("Ajustes de Bateria…", NULL, ACT_PREFS, false, false);
    } else if (title == TITLE_FOCUS) {
        add_item("Foco", NULL, 0, false, false);
        M.items[M.n - 1].header = true;
        for (int k = 1; k < 5; k++) add_item(focus_names[k], NULL, ACT_FOCUS_0 + k, false, g_focus_mode == k);
        add_sep();
        add_item("Desativar Foco", NULL, ACT_FOCUS_0, g_focus_mode == 0, false);
    } else if (title == TITLE_VOL) {
        snprintf(snd_l1, sizeof snd_l1, "Saída: %s", audio_device_name());
        snprintf(snd_in, sizeof snd_in, "Entrada: %s", audio_input_available() ? audio_input_name() : "nenhuma");
        add_item("Volume", NULL, 0, true, false);
        add_item("", NULL, -77, false, false); /* linha especial: slider */
        add_sep();
        add_item(snd_l1, NULL, 0, true, false);
        add_item(snd_in, NULL, 0, true, false);
        add_sep();
        add_item("Preferências de Som…", NULL, ACT_SND_PREFS, false, false);
    } else if (title == TITLE_CLOCK) {
        datetime_t dt;
        rtc_now(&dt);
        static const char *wd[7] = {"domingo", "segunda-feira", "terça-feira", "quarta-feira", "quinta-feira", "sexta-feira", "sábado"};
        snprintf(clock_l1, sizeof clock_l1, "%s, %d de %s de %d", wd[dt.weekday], dt.day, month_long[dt.month - 1], dt.year);
        add_item(clock_l1, NULL, 0, true, false);
        add_sep();
        /* como no sistema: clicar no relógio também mostra as notificações */
        add_item(nnotif ? "Notificações" : "Sem notificações", NULL, 0, true, false);
        for (int i = nnotif - 1; i >= 0; i--) add_item(notifs[i].title, NULL, 0, true, false);
        add_sep();
        add_item("Abrir Preferências de Data e Hora…", NULL, ACT_DATE_PREFS, false, false);
    } else if (title == TITLE_SEARCH) {
        M.kind = MK_NONE;
        desktop_toggle_spotlight();
        return;
    } else if (title == TITLE_NOTIF) {
        add_item(nnotif ? "Notificações recentes" : "Sem notificações", NULL, 0, true, false);
        for (int i = 0; i < nnotif; i++) add_item(notifs[i].title, NULL, 0, true, false);
    }
    menu_layout();
    if (title >= TITLE_NET && M.x + M.w > SW - 4) M.x = SW - 4 - M.w;
    wm_damage_all();
}

void desktop_close_menus(void) {
    submenu_close();
    if (M.kind != MK_NONE) {
        if (M.kind == MK_POPUP) popup_res.ready = false;
        M.kind = MK_NONE;
        wm_damage_all();
    }
}
bool desktop_menu_open(void) { return M.kind != MK_NONE; }

void popup_open(int sx, int sy, int w, const char **items, int n, int sel, window_t *win, int id) {
    M.kind = MK_POPUP;
    M.n = 0;
    for (int i = 0; i < n && i < 48; i++) add_item(items[i], NULL, i, false, i == sel);
    M.x = sx; M.y = sy; M.w = w;
    M.hover = sel;
    M.popup_win = win; M.popup_id = id;
    M.armed = true;
    M.opened_at = timer_ticks();
    menu_layout();
    wm_damage_all();
}

/* menu de contexto completo: separadores (label NULL), cabeçalhos, itens desativados, símbolos e submenus
 * (ex.: "Abrir com ›"). O resultado é o campo action do item escolhido, no menu ou no submenu. */
void popup_open_menu(int sx, int sy, int w, const menu_item_t *items, int n, window_t *win, int id) {
    submenu_close();
    M.kind = MK_POPUP;
    M.n = 0;
    for (int i = 0; i < n && M.n < (int)ARRAY_LEN(M.items); i++) M.items[M.n++] = items[i];
    M.x = sx; M.y = sy; M.w = w;
    M.hover = -1;
    M.popup_win = win; M.popup_id = id;
    M.armed = true;
    M.opened_at = timer_ticks();
    menu_layout();
    wm_damage_all();
}

bool popup_take_result(window_t *win, int id, int *val) {
    if (popup_res.ready && popup_res.win == win && popup_res.id == id) {
        popup_res.ready = false;
        *val = popup_res.val;
        return true;
    }
    return false;
}

static int volume_slider_x0, volume_slider_w, volume_slider_y;

/* atalho no estilo do sistema: cada símbolo separado por um pequeno espaço ("⌘ C", "⇧ ⌘ S") */
static int shortcut_width(font_t *f, float fs, const char *sc) {
    int w = 0, n = 0;
    const char *p = sc;
    while (*p) {
        const char *q = p;
        utf8_decode(&p);
        w += text_width_n(f, fs, q, (int)(p - q));
        n++;
    }
    return w + (n > 1 ? (n - 1) * 3 : 0);
}
static void draw_shortcut(surface_t *s, font_t *f, float fs, int right, int base, u32 c, const char *sc) {
    int x = right - shortcut_width(f, fs, sc);
    const char *p = sc;
    while (*p) {
        const char *q = p;
        utf8_decode(&p);
        x += text_draw_n(s, f, fs, x, base, c, q, (int)(p - q)) + 3;
    }
}

/* lista de itens de menu (principal ou submenu): colunas opcionais de marca (✓) e de símbolo */
typedef struct { int lx, ix, cx; } menu_cols_t;

static menu_cols_t menu_columns(menu_item_t *items, int n) {
    bool chk = false, ico = false;
    for (int i = 0; i < n; i++) { if (items[i].checked) chk = true; if (items[i].icon) ico = true; }
    menu_cols_t c;
    c.cx = 14;                       /* marca */
    c.ix = chk ? 32 : 16;            /* símbolo */
    c.lx = c.ix + (ico ? 24 : 2);    /* rótulo */
    return c;
}

static int list_width(menu_item_t *items, int n) {
    font_t *f = font_get(FONT_REGULAR), *fh = font_get(FONT_SEMIBOLD);
    menu_cols_t c = menu_columns(items, n);
    int w = 180;
    for (int i = 0; i < n; i++) {
        menu_item_t *it = &items[i];
        if (!it->label) continue;
        int iw = c.lx + text_width(it->header ? fh : f, 14, it->label) + 24;   /* o desenho reserva 20 px à direita */
        if (it->shortcut) iw += shortcut_width(f, 13, it->shortcut) + 30;
        if (it->submenu) iw += 26;
        w = MAX(w, iw);
    }
    return MIN(w, SW - 16);
}

static int list_height(menu_item_t *items, int n) {
    int h = 12;
    for (int i = 0; i < n; i++) h += menu_item_h(&items[i]);
    return h;
}

static int list_item_y(menu_item_t *items, int n, int y0, int idx) {
    int y = y0 + 6;
    for (int i = 0; i < idx && i < n; i++) y += menu_item_h(&items[i]);
    return y;
}

static int list_item_at(menu_item_t *items, int n, rect_t r, int x, int y) {
    if (!rect_contains(r, x, y)) return -1;
    int yy = r.y + 6;
    for (int i = 0; i < n; i++) {
        int ih = menu_item_h(&items[i]);
        if (y >= yy && y < yy + ih) return items[i].label && !items[i].header ? i : -1;
        yy += ih;
    }
    return -1;
}

/* parent_open: índice do item cujo submenu está aberto (fica destacado em cinza) */
static void draw_menu_list(surface_t *s, menu_item_t *items, int n, rect_t r, int hover, int parent_open) {
    glass_panel(s, r.x, r.y, r.w, r.h, MENU_RADIUS, GLASS_REGULAR);
    font_t *f = font_get(FONT_REGULAR), *fh = font_get(FONT_SEMIBOLD);
    float fs = 14;
    menu_cols_t c = menu_columns(items, n);
    int y = r.y + 6;
    for (int i = 0; i < n; i++) {
        menu_item_t *it = &items[i];
        int ih = menu_item_h(it);
        if (!it->label) {
            gfx_fill(s, r.x + 14, y + 5, r.w - 28, 1, T.dark ? RGBA(255, 255, 255, 34) : RGBA(0, 0, 0, 26));
            y += ih;
            continue;
        }
        if (it->action == -77) {
            volume_slider_x0 = r.x + 22; volume_slider_w = r.w - 44; volume_slider_y = y + 12;
            float v = audio_get_volume() / 100.0f;
            gfx_round_rect(s, volume_slider_x0, volume_slider_y - 3, volume_slider_w, 6, 3, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 30));
            gfx_round_rect(s, volume_slider_x0, volume_slider_y - 3, volume_slider_w * v + 3, 6, 3, T.accent);
            float kx = volume_slider_x0 + volume_slider_w * v;
            gfx_round_rect(s, kx - 12, volume_slider_y - 8, 24, 16, 8, T.knob);
            gfx_specular_rim(s, kx - 12, volume_slider_y - 8, 24, 16, 8, 170);
            y += ih;
            continue;
        }
        int base = y + 17;
        if (it->header) {
            text_draw(s, fh, fs, r.x + c.ix + 2, base, T.glass_text2, it->label);
            y += ih;
            continue;
        }
        bool hl = i == hover && !it->disabled;
        bool par = i == parent_open && !hl;
        if (hl) gfx_round_rect(s, r.x + 6, y, r.w - 12, ih, 6, T.dark ? color_lighten(T.accent, 6) : T.accent);
        else if (par) gfx_round_rect(s, r.x + 6, y, r.w - 12, ih, 6, T.dark ? RGBA(255, 255, 255, 34) : RGBA(0, 0, 0, 30));
        u32 col = hl ? RGB(255, 255, 255) : it->disabled ? (T.dark ? RGBA(255, 255, 255, 80) : RGBA(0, 0, 0, 70)) : T.glass_text;
        if (it->disabled && it->action == 0 && !it->icon) col = T.glass_text2; /* linhas informativas */
        u32 col2 = hl ? RGBA(255, 255, 255, 220) : it->disabled ? (T.dark ? RGBA(255, 255, 255, 60) : RGBA(0, 0, 0, 50)) : T.glass_text2;
        if (it->checked) {
            float cx = r.x + c.cx, cy = y + ih / 2.0f;
            gfx_line(s, cx - 3.5f, cy, cx - 0.5f, cy + 3.5f, 1.7f, col);
            gfx_line(s, cx - 0.5f, cy + 3.5f, cx + 5.0f, cy - 4.0f, 1.7f, col);
        }
        if (it->icon) it->icon(s, r.x + c.ix, y + (ih - 15) / 2, 15, col);
        /* sem símbolo: o rótulo fica alinhado ao cabeçalho, como no sistema */
        int lx = it->icon ? c.lx : c.ix + 2;
        text_draw_ellipsis(s, f, fs, r.x + lx, base, r.w - lx - 20 - (it->submenu ? 20 : 0), col, it->label);
        if (it->submenu) glyph_chevron(s, r.x + r.w - 26, y + (ih - 14) / 2, 14, hl ? RGB(255, 255, 255) : T.glass_text, 0);
        else if (it->shortcut) draw_shortcut(s, f, 13, r.x + r.w - 18, base, col2, it->shortcut);
        y += ih;
    }
}

/* ------------------------------------------------------------ submenu em cascata */
static struct { bool open; int parent, x, y, w, h, n, hover; menu_item_t items[24]; } SM;

static rect_t sm_rect(void) { return rect_make(SM.x, SM.y, SM.w, SM.h); }

static void submenu_open(int parent) {
    menu_t *sub = M.items[parent].submenu;
    if (!sub) return;
    SM.open = true;
    SM.parent = parent;
    SM.n = MIN(sub->count, 24);
    for (int i = 0; i < SM.n; i++) SM.items[i] = sub->items[i];
    SM.hover = -1;
    SM.w = list_width(SM.items, SM.n);
    SM.h = list_height(SM.items, SM.n);
    /* ao lado do menu pai, alinhando o primeiro item com o item pai */
    SM.x = M.x + M.w - 4;
    SM.y = list_item_y(M.items, M.n, M.y, parent) - 6;
    if (SM.x + SM.w > SW - 6) SM.x = M.x - SM.w + 4;
    if (SM.y + SM.h > SH - 6) SM.y = SH - 6 - SM.h;
    if (SM.y < MENUBAR_H + 2) SM.y = MENUBAR_H + 2;
    wm_damage_all();
}

static void submenu_close(void) { if (SM.open) { SM.open = false; wm_damage_all(); } }

static void draw_menu(surface_t *s) {
    if (M.kind == MK_NONE) return;
    draw_menu_list(s, M.items, M.n, rect_make(M.x, M.y, M.w, M.h), M.hover, SM.open ? SM.parent : -1);
    if (SM.open) draw_menu_list(s, SM.items, SM.n, sm_rect(), SM.hover, -1);
}

static int menu_item_at(int x, int y) { return list_item_at(M.items, M.n, rect_make(M.x, M.y, M.w, M.h), x, y); }

static void dock_empty_trash_cb(int button, void *ctx) {
    if (button != 1) return;
    extern app_t app_arquivos;
    if (app_arquivos.on_menu) app_arquivos.on_menu(&app_arquivos, 4 /* A_EMPTYTRASH */);
    extern window_t *wm_nth(int);
    for (int i = 0;; i++) { window_t *w = wm_nth(i); if (!w) break; if (w->app == &app_arquivos) w->needs_paint = true; }
    wm_damage_all();
}

static void run_action(int act) {
    app_t *a = active_app();
    window_t *f = wm_focused();
    switch (act) {
    case ACT_ABOUT: desktop_show_about(); break;
    case ACT_PREFS: case ACT_APP_PREFS: case ACT_NET_PREFS: case ACT_BT_PREFS: case ACT_SND_PREFS: case ACT_DATE_PREFS: {
        app_t *p = apps_find("preferencias");
        if (p) {
            extern void prefs_open_pane(int pane);
            int pane = act == ACT_NET_PREFS ? 3 : act == ACT_BT_PREFS ? 4 : act == ACT_SND_PREFS ? 2 : act == ACT_DATE_PREFS ? 5 : -1;
            app_launch(p);
            if (pane >= 0) prefs_open_pane(pane);
            if (act == ACT_APP_PREFS && a && a != p && a->on_menu) { a->on_menu(a, ACT_APP_PREFS); }
        }
        break;
    }
    case ACT_ACTIVITY: app_launch(apps_find("atividade")); break;
    case ACT_FORCEQUIT: if (a && strcmp(a->exec, "arquivos")) app_force_quit(a); break;
    case ACT_RESTART: desktop_shutdown_dialog(1); break;
    case ACT_SHUTDOWN: desktop_shutdown_dialog(0); break;
    case ACT_LOCK: login_logout(); break;
    case ACT_DARK_TOGGLE: {
        theme_set(!T.dark, g_accent_index);
        desktop_rebuild_wallpaper();
        for (int i = 0;; i++) { extern window_t *wm_nth(int); window_t *w = wm_nth(i); if (!w) break; w->needs_paint = true; }
        break;
    }
    case ACT_APP_ABOUT: if (a) { if (a->on_menu) a->on_menu(a, ACT_APP_ABOUT); else desktop_show_about(); } break;
    case ACT_APP_QUIT: if (a) app_quit(a); break;
    case ACT_APP_HIDE: if (a) { extern window_t *wm_nth(int); for (int i = 0;; i++) { window_t *w = wm_nth(i); if (!w) break; if (w->app == a) wm_minimize(w); } } break;
    case ACT_WIN_MIN: if (f) wm_minimize(f); break;
    case ACT_WIN_ZOOM: if (f) wm_zoom(f); break;
    case ACT_WIN_LEFT: if (f) wm_tile(f, 1); break;
    case ACT_WIN_RIGHT: if (f) wm_tile(f, 2); break;
    case ACT_WIN_FILL: if (f) wm_tile(f, 3); break;
    case ACT_MISSION: wm_toggle_mission_control(); break;
    case ACT_WIN_CLOSE: if (f) wm_close(f); break;
    case ACT_WIN_FRONT: if (a) wm_bring_app(a); break;
    /* menu de contexto do Dock */
    case ACT_DOCK_KEEP: if (dock_ctx_app) dock_toggle_keep(dock_ctx_app); break;
    case ACT_DOCK_LOGIN: if (dock_ctx_app) dock_ctx_app->open_at_login = !dock_ctx_app->open_at_login; break;
    case ACT_DOCK_SHOW: { extern void finder_open_path(const char *p); finder_open_path("/Aplicativos"); break; }
    case ACT_DOCK_OPEN: if (dock_ctx_app) dock_click(dock_ctx_app); break;
    case ACT_DOCK_HIDE:
        if (dock_ctx_app) {
            extern window_t *wm_nth(int);
            for (int i = 0;; i++) { window_t *w = wm_nth(i); if (!w) break; if (w->app == dock_ctx_app && !w->minimized) wm_minimize(w); }
        }
        break;
    case ACT_DOCK_QUIT: if (dock_ctx_app && strcmp(dock_ctx_app->exec, "arquivos")) app_quit(dock_ctx_app); break;
    case ACT_DOCK_EMPTY: {
        static const char *bt[] = {"Cancelar", "Esvaziar Lixeira"};
        alert_show("Tem certeza de que deseja apagar permanentemente os itens da Lixeira?", "Não é possível desfazer esta ação.", bt, 2,
                   dock_empty_trash_cb, NULL);
        break;
    }
    case ACT_DOCK_RESTORE:
        if (dock_ctx_win && dock_ctx_win->minimized) { dock_ctx_win->anim_dir = 1; dock_ctx_win->anim = 0; wm_focus(dock_ctx_win); }
        break;
    case ACT_DOCK_CLOSEWIN: if (dock_ctx_win) wm_close(dock_ctx_win); break;
    case ACT_WIN_FULL: if (f) wm_fullscreen(f); break;
    case ACT_TAB_MERGE: if (f) wm_merge_tabs(f); break;
    case ACT_TAB_DETACH: if (f) wm_tab_detach(f); break;
    case ACT_TAB_NEXT: wm_tab_step(1); break;
    case ACT_TAB_PREV: wm_tab_step(-1); break;
    case ACT_ABOUT_MAC: desktop_show_about(); break;
    case ACT_HELP: {
        /* a Ajuda é uma página about: do motor próprio do Horizonte (o Chromium, motor principal, não a tem) */
        if (apps_find("horizonte")) { extern void horizonte_legado_abrir(const char *url); horizonte_legado_abrir("about:ajuda"); }
        break;
    }
    case ACT_WIFI_TOGGLE: { wifi_status_t ws; wifi_status(&ws); wifi_set_enabled(ws.state == WIFI_OFF); break; }
    default:
        if (act >= ACT_WIFI_NET0 && act < ACT_WIFI_NET0 + (int)ARRAY_LEN(wifi_nets)) {
            wifi_network_t *n = &wifi_nets[act - ACT_WIFI_NET0];
            if (n->security == WIFI_SEC_OPEN) { wifi_connect(n->ssid, ""); notify("Wi-Fi", n->ssid); }
            else { extern void prefs_open_named(const char *name); prefs_open_named("wifi"); }   /* rede com senha: o painel Wi-Fi pede a senha */
        } else if (act >= ACT_FOCUS_0 && act < ACT_FOCUS_0 + 5) {
            g_focus_mode = act - ACT_FOCUS_0;
            g_notify_dnd = g_focus_mode != 0;
            wm_damage_all();
        } else if (act >= ACT_WIN_N && act < ACT_WIN_N + 64) {
            extern window_t *wm_nth(int);
            window_t *w = wm_nth(act - ACT_WIN_N);
            if (w) { if (w->minimized) { w->minimized = false; w->anim_dir = 1; w->anim = 0; } wm_focus(w); }
        } else if (a && a->on_menu && act > 0) {
            a->on_menu(a, act);
        }
    }
}

/* ============================================================ Dock */
static surface_t *dock_icon(app_t *a, int size) { return icon_for(a, size); }

/* Proporções do Dock (relativas ao tamanho do ícone), medidas na referência:
 * margem vertical 25%, margem lateral 22%, espaço entre ícones 26%, raio da barra 25% da altura. */
#define DOCK_BOTTOM 6
static int dock_base;          /* tamanho efetivo do ícone (encolhe em telas estreitas) */
static int dock_gap, dock_hpad, dock_vpad;
static rect_t dock_tip_r;      /* balão com o nome do ícone sob o mouse (vidro) */
static app_t downloads_app;    /* pilha "Downloads" antes da Lixeira */

static void dock_metrics(int base) {
    dock_gap = MAX(4, (int)(base * 0.26f + 0.5f));
    dock_hpad = MAX(4, (int)(base * 0.22f + 0.5f));
    dock_vpad = MAX(4, (int)(base * 0.25f + 0.5f));
}

/* ordem dos ícones mantidos: começa pela ordem de registro (com o Launchpad em 2º) e depois é do usuário */
static int dock_order_index(app_t *a) { for (int i = 0; i < ndock_order; i++) if (dock_order[i] == a) return i; return -1; }

/* ------------------------------------------------------------ preferências do Dock (persistem no disco)
 * /Sistema/Biblioteca/Preferencias/com.mos.dock.cfg, uma chave por linha:
 *   size=50  magnify=0  autohide=0  order=arquivos,launchpad,…  login=terminal,… */
#define DOCK_CFG "/Sistema/Biblioteca/Preferencias/com.mos.dock.cfg"
static char dock_cfg_saved[1024];
static bool dock_login_pending;

static app_t *dock_app_by_exec(const char *ex) {
    if (!strcmp(ex, "launchpad")) return &launch_app;
    for (int i = 0; i < apps_count(); i++) if (!strcmp(apps_get(i)->exec, ex)) return apps_get(i);
    return NULL;
}

/* cantos ativos: 0 nada, 1 Mission Control, 2 Central de Notificações, 3 Launchpad, 4 Mostrar Mesa,
 * 5 Central de Controle, 6 Bloquear Tela — ordem: ↖ ↗ ↙ ↘ */
int g_hot_corner[4];
extern bool g_desktop_widgets;

static int dock_cfg_build(char *b, int cap) {
    int n = snprintf(b, cap, "size=%d\nmagnify=%d\nautohide=%d\nwidgets=%d\ncorners=%d%d%d%d\norder=", g_dock_size, g_dock_magnify, g_dock_autohide,
                     g_desktop_widgets, g_hot_corner[0], g_hot_corner[1], g_hot_corner[2], g_hot_corner[3]);
    for (int i = 0; i < ndock_order && n < cap - 40; i++) n += snprintf(b + n, cap - n, "%s%s", i ? "," : "", dock_order[i]->exec);
    n += snprintf(b + n, cap - n, "\nlogin=");
    bool first = true;
    for (int i = 0; i < apps_count() && n < cap - 40; i++)
        if (apps_get(i)->open_at_login) { n += snprintf(b + n, cap - n, "%s%s", first ? "" : ",", apps_get(i)->exec); first = false; }
    n += snprintf(b + n, cap - n, "\n");
    return n;
}

/* lê a lista "a,b,c" de uma linha "chave=…"; chama fn para cada nome */
static void dock_cfg_list(const char *txt, const char *key, void (*fn)(const char *)) {
    int kl = strlen(key);
    for (const char *p = txt; p && *p;) {
        if (!strncmp(p, key, kl) && p[kl] == '=') {
            p += kl + 1;
            while (*p && *p != '\n') {
                char nm[48];
                int k = 0;
                while (*p && *p != ',' && *p != '\n' && k < 47) nm[k++] = *p++;
                nm[k] = 0;
                if (k) fn(nm);
                if (*p == ',') p++;
            }
            return;
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
}
static int dock_cfg_int(const char *txt, const char *key, int def) {
    int kl = strlen(key);
    for (const char *p = txt; p && *p;) {
        if (!strncmp(p, key, kl) && p[kl] == '=') return atoi(p + kl + 1);
        p = strchr(p, '\n');
        if (p) p++;
    }
    return def;
}
static void cfg_add_order(const char *ex) {
    app_t *a = dock_app_by_exec(ex);
    if (!a || dock_order_index(a) >= 0 || ndock_order >= (int)ARRAY_LEN(dock_order)) return;
    if (a != &launch_app) a->pinned = true;
    dock_order[ndock_order++] = a;
}
static void cfg_add_login(const char *ex) { app_t *a = dock_app_by_exec(ex); if (a && a != &launch_app) a->open_at_login = true; }

static bool dock_prefs_load(void) {
    vnode_t *f = vfs_lookup(DOCK_CFG);
    if (!f || f->dir || !f->data || !f->size) return false;
    char txt[1024];
    usize n = MIN(f->size, sizeof txt - 1);
    memcpy(txt, f->data, n);
    txt[n] = 0;
    g_dock_size = CLAMP(dock_cfg_int(txt, "size", g_dock_size), 32, 96);
    g_dock_magnify = dock_cfg_int(txt, "magnify", g_dock_magnify) != 0;
    g_dock_autohide = dock_cfg_int(txt, "autohide", g_dock_autohide) != 0;
    g_desktop_widgets = dock_cfg_int(txt, "widgets", g_desktop_widgets) != 0;
    int hc = dock_cfg_int(txt, "corners", 0);
    for (int i = 3; i >= 0; i--) { g_hot_corner[i] = CLAMP(hc % 10, 0, 6); hc /= 10; }
    /* a lista salva manda: o que estava fixo por padrão e não está nela foi removido pelo usuário */
    for (int i = 0; i < apps_count(); i++) apps_get(i)->pinned = false;
    ndock_order = 0;
    dock_cfg_list(txt, "order", cfg_add_order);
    if (dock_order_index(&launch_app) < 0 && ndock_order < (int)ARRAY_LEN(dock_order)) dock_order[ndock_order++] = &launch_app;
    dock_cfg_list(txt, "login", cfg_add_login);
    dock_login_pending = true;
    strncpy(dock_cfg_saved, txt, sizeof dock_cfg_saved - 1);
    return true;
}

/* grava quando algo mudou (chamado ~1×/s pelo desktop_tick) */
static void dock_prefs_tick(void) {
    if (ndock_order < 0) return;
    /* o Dock pode ser montado antes de o disco ser carregado: a leitura acontece no 1º tick,
     * antes de qualquer gravação (para não sobrescrever o arquivo com o padrão) */
    static bool tried;
    if (!tried) {
        tried = true;
        if (dock_prefs_load()) { wm_damage_all(); return; }
    }
    char b[1024];
    int n = dock_cfg_build(b, sizeof b);
    if (!strcmp(b, dock_cfg_saved)) return;
    vfs_mkdir("/Sistema/Biblioteca/Preferencias");
    vfs_write_file(DOCK_CFG, b, n);
    strncpy(dock_cfg_saved, b, sizeof dock_cfg_saved - 1);
}

static void dock_order_sync(void) {
    if (ndock_order < 0) {
        if (!apps_count()) return;
        ndock_order = 0;
        bool la = false;
        for (int i = 0; i < apps_count() && ndock_order < (int)ARRAY_LEN(dock_order) - 1; i++) {
            app_t *a = apps_get(i);
            if (!a->pinned) continue;
            dock_order[ndock_order++] = a;
            if (!la) { dock_order[ndock_order++] = &launch_app; la = true; }
        }
        if (!la) dock_order[ndock_order++] = &launch_app;
    }
    /* apps registrados depois (ou mantidos pelo menu) entram no fim; os que saíram são retirados */
    for (int i = 0; i < apps_count() && ndock_order < (int)ARRAY_LEN(dock_order); i++) {
        app_t *a = apps_get(i);
        if (a->pinned && dock_order_index(a) < 0) dock_order[ndock_order++] = a;
    }
    for (int i = 0; i < ndock_order; i++)
        if (dock_order[i] != &launch_app && !dock_order[i]->pinned) { memmove(dock_order + i, dock_order + i + 1, (ndock_order - i - 1) * sizeof(app_t *)); ndock_order--; i--; }
}

static void dock_toggle_keep(app_t *a) {
    if (!a || a == &launch_app || a == &trash_app || a == &downloads_app) return;
    bool reg = false;                       /* só apps do registro podem ficar no Dock (proxies somem com o processo) */
    for (int i = 0; i < apps_count() && !reg; i++) reg = apps_get(i) == a;
    if (!reg) return;
    a->pinned = !a->pinned;
    dock_order_sync();
    wm_damage(rect_make(0, SH - 220, SW, 220));
}

/* janelas de um processo do macOS que este app representa (o motor Chromium do Horizonte: apps_shortcut_for) */
static bool dock_proxy_windows(app_t *a) {
    extern window_t *wm_nth(int);
    for (int i = 0;; i++) {
        window_t *w = wm_nth(i);
        if (!w) return false;
        if (w->app && w->app != a && !w->closing && w->visible && apps_shortcut_for(w->app) == a) return true;
    }
}
static bool dock_app_running(app_t *a) {
    return a->running && (wm_count_for(a) || !strncmp(a->exec, "mac:", 4) || dock_proxy_windows(a));
}

static void dock_layout(void) {
    dock_order_sync();
    ndslots = 0;
    const int cap = (int)ARRAY_LEN(dslots) - 4;
    for (int i = 0; i < ndock_order && ndslots < cap; i++) {
        app_t *a = dock_order[i];
        if (DD.active && DD.remove && a == DD.app) continue;   /* puxado para fora: o espaço fecha */
        dslots[ndslots++] = (dock_slot_t){a, 0, 0, false, NULL};
    }
    for (int i = 0; i < apps_count() && ndslots < cap; i++) {
        app_t *a = apps_get(i);
        /* apps do macOS: as janelas são do processo (proxy), o atalho só marca "rodando" */
        if (!a->pinned && dock_app_running(a)) dslots[ndslots++] = (dock_slot_t){a, 0, 0, false, NULL};
    }
    /* toda janela aberta tem o seu app no Dock: apps fora do registro (processos do macOS abertos
     * pelo Arquivos/Busca/Terminal, auxiliares) ou que perderam o estado "rodando" entram aqui */
    extern window_t *wm_nth(int);
    for (int i = 0; ndslots < cap; i++) {
        window_t *w = wm_nth(i);
        if (!w) break;
        app_t *a = w->app;
        if (!a || w->closing || !w->visible) continue;
        app_t *sc = apps_shortcut_for(a);          /* o atalho de /Aplicativos já representa o processo */
        if (sc) a = sc;
        bool have = false;
        for (int k = 0; k < ndslots && !have; k++) have = dslots[k].app == a;
        if (have) continue;
        a->running = true;                         /* a bolinha acompanha as janelas abertas */
        dslots[ndslots++] = (dock_slot_t){a, 0, 0, false, NULL};
    }
    dslots[ndslots++] = (dock_slot_t){NULL, 0, 0, true, NULL};
    /* janelas minimizadas: miniaturas à direita do separador (o gênio termina nelas), na ordem em que
     * foram minimizadas — a ordem de empilhamento das janelas muda com o foco, esta não */
    static window_t *min_order[24];
    static int nmin;
    for (int k = 0; k < nmin; k++) {
        bool alive = false;
        for (int i = 0;; i++) {
            window_t *w = wm_nth(i);
            if (!w) break;
            if (w == min_order[k]) { alive = w->visible && !w->closing && (w->minimized || w->anim_dir == 2); break; }
        }
        if (!alive) { memmove(min_order + k, min_order + k + 1, (nmin - k - 1) * sizeof(window_t *)); nmin--; k--; }
    }
    for (int i = 0; nmin < (int)ARRAY_LEN(min_order); i++) {
        window_t *w = wm_nth(i);
        if (!w) break;
        if (!w->visible || w->closing || !(w->minimized || w->anim_dir == 2)) continue;
        bool known = false;
        for (int k = 0; k < nmin; k++) if (min_order[k] == w) known = true;
        if (!known) min_order[nmin++] = w;
    }
    for (int k = 0; k < nmin && ndslots < cap; k++) dslots[ndslots++] = (dock_slot_t){min_order[k]->app, 0, 0, false, min_order[k]};
    dslots[ndslots++] = (dock_slot_t){&downloads_app, 0, 0, false, NULL};
    dslots[ndslots++] = (dock_slot_t){&trash_app, 0, 0, false, NULL};
    /* responsivo: o Dock nunca passa da largura da tela */
    int nicons = ndslots - 1;
    int base = g_dock_size;
    for (;;) {
        dock_metrics(base);
        int need = 2 * dock_hpad + nicons * base + (ndslots - 1) * dock_gap + 1;
        if (need <= SW - 16 || base <= 24) break;
        base--;
    }
    if (base > SH / 9) { base = MAX(24, SH / 9); dock_metrics(base); }
    dock_base = base;
    float sep_w = 1;
    float maxs = base * 1.6f, range = base * 2.8f;
    float pos = dock_hpad;
    for (int i = 0; i < ndslots; i++) {
        float w = dslots[i].sep ? sep_w : base;
        dslots[i].x = pos;
        dslots[i].size = w;
        pos += w + dock_gap;
    }
    float total = pos - dock_gap + dock_hpad;
    float x0 = (SW - total) / 2;
    if (g_dock_magnify && dock_hover_x >= 0) {
        pos = dock_hpad;
        for (int i = 0; i < ndslots; i++) {
            float w = dslots[i].sep ? sep_w : base;
            float cx = x0 + dslots[i].x + w / 2;
            float d = fabsf_(cx - dock_hover_x);
            float k = d < range ? (fcos(d / range * PI_F) + 1) / 2 : 0;
            dslots[i].size = dslots[i].sep ? sep_w : base + (maxs - base) * k;
            dslots[i].x = pos;
            pos += dslots[i].size + dock_gap;
        }
        total = pos - dock_gap + dock_hpad;
        if (total > SW - 8) total = SW - 8;
        x0 = (SW - total) / 2;
    }
    for (int i = 0; i < ndslots; i++) dslots[i].x += x0;
    int dh = base + 2 * dock_vpad;
    /* ocultar automaticamente: desliza para baixo da borda da tela */
    int hide = (int)((dh + DOCK_BOTTOM + 8) * dock_hide + 0.5f);
    dock_r = rect_make((int)x0, SH - dh - DOCK_BOTTOM + hide, (int)(total + 0.5f), dh);
}

int desktop_dock_top(void) {
    if (g_dock_autohide) return PY() + SH - 2;   /* escondido, o Dock não reserva espaço para as janelas */
    float h = dock_hide;
    dock_hide = 0;
    dock_layout();
    dock_hide = h;
    return PY() + dock_r.y - 6;                  /* global: quem usa é o window server */
}
rect_t desktop_dock_rect(void) { return to_global(dock_r); }

static rect_t slot_icon_rect(int i) {
    return rect_make((int)dslots[i].x, (int)(dock_r.y + dock_r.h - dock_vpad - dslots[i].size), (int)dslots[i].size, (int)dslots[i].size);
}

rect_t dock_icon_rect(app_t *a) {
    for (int i = 0; i < ndslots; i++) if (dslots[i].app == a && !dslots[i].win) return slot_icon_rect(i);
    return rect_make(SW / 2, SH - 40, 32, 32);
}

static rect_t dock_window_rect_local(window_t *w);
rect_t dock_window_rect(window_t *w) { return to_global(dock_window_rect_local(w)); }   /* destino do gênio */
static rect_t dock_window_rect_local(window_t *w) {
    for (int i = 0; i < ndslots; i++) if (dslots[i].win == w) {
        /* a miniatura mantém a proporção da janela dentro do quadrado do ícone */
        rect_t r = slot_icon_rect(i);
        int tw = r.w, th = r.h;
        if (w->w >= w->h) th = MAX(8, r.w * w->h / MAX(1, w->w)); else tw = MAX(8, r.h * w->w / MAX(1, w->h));
        return rect_make(r.x + (r.w - tw) / 2, r.y + (r.h - th) / 2, tw, th);
    }
    return dock_icon_rect(w->app);
}

void dock_set_badge(app_t *a, int n) {
    if (!a) return;
    n = MAX(0, n);
    if (a->badge == n && !a->badge_label[0]) return;
    a->badge_label[0] = 0;
    a->badge = n;
    wm_damage(rect_make(0, SH - 220, SW, 220));
}

void dock_set_badge_label(app_t *a, const char *label) {
    int n = 0;
    char text[8];
    if (!a) return;
    dock_badge_apply(&n, text, label);
    if (a->badge == n && !strcmp(a->badge_label, text)) return;
    memcpy(a->badge_label, text, sizeof a->badge_label);
    a->badge = n;
    wm_damage(rect_make(0, SH - 220, SW, 220));
}

void dock_request_attention(app_t *a) {
    if (!a || a == wm_active_app()) return;
    a->attention = true;
    if (a->bounce <= 0) a->bounce = 1;
}

/* clique num ícone do Dock (também "Abrir" no menu de contexto) */
static void dock_click(app_t *a) {
    if (a == &launch_app) desktop_toggle_launcher();
    else if (a == &downloads_app) { extern void finder_open_path(const char *p); dock_set_badge(&downloads_app, 0); finder_open_path(home_sub("/Downloads")); }
    else if (a == &trash_app) { extern void finder_open_trash(void); finder_open_trash(); }
    else if (a->running && wm_count_for(a)) {
        /* só janelas minimizadas: traz a mais recente de volta (com o gênio ao contrário) */
        extern window_t *wm_nth(int);
        bool any_open = false;
        window_t *last_min = NULL;
        for (int i = 0;; i++) {
            window_t *w = wm_nth(i);
            if (!w) break;
            if (w->app != a || w->closing) continue;
            if (w->minimized) last_min = w; else any_open = true;
        }
        if (!any_open && last_min) { last_min->anim_dir = 1; last_min->anim = 0; wm_focus(last_min); }
        else wm_bring_app(a);
    }
    else if (a->running && !strncmp(a->exec, "mac:", 4)) app_launch(a);   /* traz as janelas do processo */
    else if (a->running && dock_proxy_windows(a)) app_launch(a);       /* o motor Chromium do Horizonte: traz as janelas dele */
    else { a->bounce = 1; app_launch(a); }
}

/* ícone da pilha Downloads: pasta azul-clara */
static void draw_downloads_icon(surface_t *s, int x, int y, int size) { icon_folder(s, x, y, size, RGB(110, 190, 245)); }

/* selo vermelho com número, no canto superior direito do ícone */
static void draw_badge(surface_t *s, int ix, int iy, int sz, app_t *a) {
    char t[8];
    if (a->badge_label[0]) snprintf(t, sizeof t, "%s", a->badge_label);
    else if (a->badge > 999) snprintf(t, sizeof t, "999+");
    else snprintf(t, sizeof t, "%d", a->badge);
    font_t *f = font_get(FONT_SEMIBOLD);
    float fs = MAX(10.0f, sz * 0.25f);
    float h = MAX(16.0f, sz * 0.38f);
    float w = MAX(h, text_width(f, fs, t) + h * 0.55f);
    float x = ix + sz - w * 0.72f, y = iy - h * 0.22f;
    gfx_shadow(s, (int)x, (int)y, (int)w, (int)h, h / 2, 4, 50, 1);
    gfx_round_rect(s, x, y, w, h, h / 2, RGB(255, 59, 48));
    gfx_round_rect_stroke(s, x + 0.5f, y + 0.5f, w - 1, h - 1, h / 2 - 0.5f, 1, RGBA(255, 255, 255, 60));
    text_draw_center(s, f, fs, rect_make((int)x, (int)y, (int)(w + 0.5f), (int)(h + 0.5f)), RGB(255, 255, 255), t);
}

/* miniatura de janela minimizada: a janela em proporção, com o ícone do app no canto */
static void draw_window_thumb(surface_t *s, window_t *w, int x, int y, int sz) {
    int tw = sz, th = sz;
    if (w->w >= w->h) th = MAX(8, sz * w->h / MAX(1, w->w)); else tw = MAX(8, sz * w->w / MAX(1, w->h));
    int tx = x + (sz - tw) / 2, ty = y + (sz - th) / 2;
    gfx_shadow(s, tx, ty, tw, th, 3, 6, 70, 2);
    gfx_blit_scaled(s, w->surf, tx, ty, tw, th, 255);
    int isz = MAX(14, sz * 42 / 100);
    surface_t *ic = icon_for(w->app, isz);
    if (ic) gfx_blit_scaled(s, ic, x + sz - isz + isz / 8, y + sz - isz + isz / 8, isz, isz, 255);
}

static void draw_dock(surface_t *s) {
    dock_layout();
    if (dock_hide >= 0.999f && !DD.active) return;
    rect_t r = dock_r;
    float rad = r.h * 0.25f;
    /* barra cinza translúcida uniforme (a mesma nos temas claro e escuro), com borda clara fina */
    glass_t g = G[GLASS_CLEAR];
    g.tint = T.dark ? RGBA(160, 160, 166, 196) : RGBA(150, 150, 156, 168);
    g.rim = 120;
    g.sheen = 10;
    g.refract = 4;
    gfx_shadow(s, r.x, r.y, r.w, r.h, rad, 22, T.dark ? 90 : 40, 6);
    gfx_glass(s, r.x, r.y, r.w, r.h, rad, &g);
    gfx_round_rect_stroke(s, r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, rad - 0.5f, 1, RGBA(255, 255, 255, T.dark ? 60 : 90));
    gfx_round_rect_stroke(s, r.x - 0.5f, r.y - 0.5f, r.w + 1, r.h + 1, rad + 0.5f, 1, RGBA(0, 0, 0, T.dark ? 70 : 26));
    dock_tip_r = rect_make(0, 0, 0, 0);
    for (int i = 0; i < ndslots; i++) {
        dock_slot_t *d = &dslots[i];
        if (d->sep) {
            gfx_fill(s, (int)d->x, r.y + dock_vpad, 1, r.h - 2 * dock_vpad, RGBA(255, 255, 255, T.dark ? 90 : 120));
            continue;
        }
        int sz = (int)(d->size + 0.5f);
        int y = r.y + r.h - dock_vpad - sz;
        if (d->win) {
            /* miniatura (some enquanto o gênio a traz de volta) */
            window_t *w = d->win;
            if (w->minimized && !(w->anim_dir == 1 && w->anim < 1)) draw_window_thumb(s, w, (int)d->x, y, sz);
            continue;
        }
        if (DD.active && d->app == DD.app) continue;      /* o ícone arrastado segue o mouse; aqui fica o espaço */
        if (d->app->bounce > 0) {
            /* pulo: sobe ~metade do ícone e cai de volta (dois pulos, o segundo menor) */
            float t = 1 - d->app->bounce;
            float b = fsin(t * PI_F * 2);
            if (b < 0) b = -b;
            float amp = t < 0.5f ? 1.0f : 0.55f;
            if (d->app->attention) amp = 1.0f;
            y -= (int)(b * amp * dock_base * 0.5f);
        }
        surface_t *ic;
        if (d->app == &trash_app) {
            static surface_t *tr[3];
            int k = sz > 70 ? 2 : sz > 52 ? 1 : 0;
            int tsz = k == 2 ? 80 : k == 1 ? 64 : 52;
            if (!tr[k]) { tr[k] = surface_new(tsz, tsz); memset(tr[k]->px, 0, tsz * tsz * 4); icon_trash(tr[k], 0, 0, tsz, false); }
            ic = tr[k];
        } else {
            ic = dock_icon(d->app, sz);
        }
        gfx_blit_scaled(s, ic, (int)d->x, y, sz, sz, dock_pressed == i ? 160 : 255);
        bool running = d->app == &launch_app ? lp_open : (d->app != &trash_app && d->app != &downloads_app && dock_app_running(d->app));
        if (running) {
            /* ponto do macOS: centrado sob o ícone, no meio da margem inferior,
             * ~4,5% do ícone (≈2,2 px de raio a 48 px) e com contraste sobre o vidro */
            float dr = MAX(2.0f, dock_base * 0.047f);
            float dcx = d->x + d->size / 2.0f;
            float dcy = r.y + r.h - MAX(5.0f, dock_vpad * 0.66f);   /* mais perto do ícone */
            /* branco nos dois temas (como no macOS), com halo escuro suave para não sumir no vidro claro */
            gfx_circle(s, dcx, dcy, dr + 0.9f, RGBA(0, 0, 0, 40));
            gfx_circle(s, dcx, dcy, dr, RGB(255, 255, 255));
        }
        if (d->app->badge > 0 || d->app->badge_label[0]) draw_badge(s, (int)d->x, y, sz, d->app);
    }
    /* ícone sendo arrastado: segue o mouse; fora do Dock mostra "Remover" */
    if (DD.active) {
        int sz = dock_base, mx = wm_mouse_x(), my = wm_mouse_y();
        int ix = mx - sz / 2, iy = my - sz / 2;
        gfx_blit_scaled(s, dock_icon(DD.app, sz), ix, iy, sz, sz, DD.remove ? 170 : 235);
        if (DD.remove) {
            font_t *f = font_get(FONT_MEDIUM);
            const char *t = "Remover";
            int tw = text_width(f, 13, t) + 26, th = 26;
            int tx = mx - tw / 2, ty = iy - th - 10;
            glass_panel(s, tx, ty, tw, th, th / 2.0f, GLASS_REGULAR);
            text_draw_center(s, f, 13, rect_make(tx, ty, tw, th), T.glass_text, t);
        }
    }
    /* nome do ícone sob o mouse: cápsula de vidro */
    if (dock_hover_x >= 0 && !DD.active && M.kind != MK_CONTEXT) {
        for (int i = 0; i < ndslots; i++) {
            dock_slot_t *d = &dslots[i];
            if (d->sep || wm_mouse_x() < d->x || wm_mouse_x() >= d->x + d->size) continue;
            const char *name = d->win ? d->win->title : d->app->name;
            font_t *f = font_get(FONT_MEDIUM);
            int tw = text_width(f, 13, name) + 26, th = 26;
            int tx = (int)(d->x + d->size / 2 - tw / 2), ty = (int)(r.y + r.h - dock_vpad - d->size) - th - 14;
            tx = CLAMP(tx, 6, SW - tw - 6);
            glass_panel(s, tx, ty, tw, th, th / 2.0f, GLASS_REGULAR);
            text_draw_center(s, f, 13, rect_make(tx, ty, tw, th), T.glass_text, name);
            dock_tip_r = rect_make(tx, ty, tw, th);
        }
    }
}

static int dock_slot_at(int x, int y) {
    if (!rect_contains(rect_make(dock_r.x, dock_r.y - 40, dock_r.w, dock_r.h + 44), x, y)) return -1;
    for (int i = 0; i < ndslots; i++)
        if (!dslots[i].sep && x >= dslots[i].x && x < dslots[i].x + dslots[i].size + 5) return i;
    return -1;
}

/* menu de contexto de um ícone do Dock (botão direito): janelas do app, Opções ▸, Ocultar, Sair… */
static menu_item_t dock_opts[4];
static menu_t dock_opts_menu = {"Opções", dock_opts, 4};

static void dock_open_menu(int slot) {
    dock_slot_t *d = &dslots[slot];
    app_t *a = d->app;
    submenu_close();
    M.kind = MK_CONTEXT;
    M.n = 0;
    M.hover = -1;
    M.armed = false;
    M.opened_at = timer_ticks();
    dock_ctx_app = a;
    dock_ctx_win = d->win;
    if (d->win) {
        add_item("Restaurar", NULL, ACT_DOCK_RESTORE, false, false);
        add_sep();
        add_item("Fechar", NULL, ACT_DOCK_CLOSEWIN, false, false);
    } else if (a == &trash_app) {
        add_item("Abrir", NULL, ACT_DOCK_OPEN, false, false);
        add_sep();
        add_item("Esvaziar Lixeira…", NULL, ACT_DOCK_EMPTY, false, false);
    } else if (a == &downloads_app) {
        add_item("Abrir em Arquivos", NULL, ACT_DOCK_OPEN, false, false);
    } else if (a == &launch_app) {
        add_item("Abrir", NULL, ACT_DOCK_OPEN, false, false);
    } else {
        extern window_t *wm_nth(int);
        int nw = 0;
        window_t *f = wm_focused();
        for (int i = 0; i < 64; i++) {
            window_t *w = wm_nth(i);
            if (!w) break;
            if (w->app != a || w->closing || !w->visible) continue;
            add_item(w->title[0] ? w->title : a->name, NULL, ACT_WIN_N + i, false, w == f);
            nw++;
        }
        if (nw) add_sep();
        dock_opts[0] = (menu_item_t){"Manter no Dock", NULL, ACT_DOCK_KEEP, false, a->pinned};
        dock_opts[1] = (menu_item_t){"Abrir ao Iniciar", NULL, ACT_DOCK_LOGIN, false, a->open_at_login};
        dock_opts[2] = (menu_item_t){NULL};
        dock_opts[3] = (menu_item_t){"Mostrar em Arquivos", NULL, ACT_DOCK_SHOW, false, false};
        add_item("Opções", NULL, 0, false, false);
        M.items[M.n - 1].submenu = &dock_opts_menu;
        add_sep();
        bool run = dock_app_running(a);
        if (run && nw) add_item("Mostrar Todas as Janelas", NULL, ACT_MISSION, false, false);
        if (run && nw) add_item("Ocultar", NULL, ACT_DOCK_HIDE, false, false);
        if (run) add_item("Sair", NULL, ACT_DOCK_QUIT, !strcmp(a->exec, "arquivos"), false);
        else add_item("Abrir", NULL, ACT_DOCK_OPEN, false, false);
    }
    /* acima do ícone, centralizado nele */
    int w = list_width(M.items, M.n), h = list_height(M.items, M.n);
    M.x = (int)(d->x + d->size / 2) - w / 2;
    M.y = dock_r.y - h - 10;
    menu_layout();
    wm_damage_all();
}

/* ============================================================ ícones na mesa */
static int desk_sel = -1;
static u64 desk_click_t;

/* ============================================================ widgets na mesa (relógio e calendário)
 * Ficam sobre o papel de parede, atrás das janelas; vidro claro, como os widgets do sistema. */
bool g_desktop_widgets = true;
static rect_t widget_rect(int i) { return rect_make(28 + i * 176, MENUBAR_H + 22, 160, 160); }

/* sobre vidro fumê o texto é sempre branco, nos dois modos */
#define WG_INK RGB(255, 255, 255)
#define WG_DIM RGBA(255, 255, 255, 120)

static void widget_clock(surface_t *s, rect_t r) {
    datetime_t dt;
    rtc_now(&dt);
    float cx = r.x + r.w / 2.0f, cy = r.y + r.h / 2.0f, R = r.w / 2.0f - 16;
    /* mostrador transparente: só um véu leve e um aro fino, o vidro aparece por trás */
    gfx_circle(s, cx, cy, R, RGBA(255, 255, 255, 14));
    gfx_round_rect_stroke(s, cx - R, cy - R, 2 * R, 2 * R, R, 1, RGBA(255, 255, 255, 40));
    for (int i = 0; i < 60; i++) {
        float a = i * PI_F / 30;
        bool hr = i % 5 == 0;
        float r0 = hr ? R - 10 : R - 5;
        gfx_line(s, cx + fsin(a) * r0, cy - fcos(a) * r0, cx + fsin(a) * (R - 3), cy - fcos(a) * (R - 3), hr ? 1.8f : 0.8f,
                 hr ? WG_INK : RGBA(255, 255, 255, 90));
    }
    font_t *fm = font_get(FONT_MEDIUM);
    static const char *num[4] = {"12", "3", "6", "9"};
    for (int i = 0; i < 4; i++) {
        float a = i * PI_F / 2, rr = R - 20;
        text_draw_center(s, fm, 12, rect_make((int)(cx + fsin(a) * rr) - 12, (int)(cy - fcos(a) * rr) - 8, 24, 16), WG_INK, num[i]);
    }
    float ah = ((dt.hour % 12) + dt.minute / 60.0f) * PI_F / 6, am = dt.minute * PI_F / 30;
    gfx_line(s, cx, cy, cx + fsin(ah) * R * 0.5f, cy - fcos(ah) * R * 0.5f, 3.6f, WG_INK);
    gfx_line(s, cx, cy, cx + fsin(am) * R * 0.8f, cy - fcos(am) * R * 0.8f, 2.4f, WG_INK);
    gfx_circle(s, cx, cy, 3.5f, RGB(255, 159, 10));
    gfx_circle(s, cx, cy, 1.4f, RGB(30, 30, 34));
}

static void widget_calendar(surface_t *s, rect_t r) {
    datetime_t dt;
    rtc_now(&dt);
    static const char *mon[12] = {"JANEIRO", "FEVEREIRO", "MARÇO", "ABRIL", "MAIO", "JUNHO", "JULHO", "AGOSTO", "SETEMBRO", "OUTUBRO", "NOVEMBRO", "DEZEMBRO"};
    static const char *wd = "DSTQQSS";
    font_t *fb = font_get(FONT_BOLD), *fs = font_get(FONT_SEMIBOLD);
    text_draw(s, fb, 9.5f, r.x + 16, r.y + 22, WG_DIM, mon[dt.month - 1]);
    /* primeiro dia da semana do mês: volta a partir de hoje */
    int first = ((dt.weekday - (dt.day - 1) % 7) + 7) % 7;
    static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = dt.year % 4 == 0 && (dt.year % 100 || dt.year % 400 == 0);
    int nd = mdays[dt.month - 1] + (dt.month == 2 && leap);
    int pm = dt.month == 1 ? 12 : dt.month - 1;
    int npd = mdays[pm - 1] + (pm == 2 && leap);
    int rows = (first + nd + 6) / 7;
    int cw = (r.w - 20) / 7, x0 = r.x + 10, y0 = r.y + 28;
    int rh = rows > 5 ? 17 : 20;
    for (int c = 0; c < 7; c++) {
        char t[2] = {wd[c], 0};
        text_draw_center(s, fb, 8, rect_make(x0 + c * cw, y0, cw, 12), WG_DIM, t);
    }
    /* grade completa: dias do mês anterior e do seguinte aparecem apagados, como no sistema */
    for (int idx = 0; idx < rows * 7; idx++) {
        int row = idx / 7, col = idx % 7, d = idx - first + 1;
        bool other = d < 1 || d > nd;
        int shown = d < 1 ? npd + d : d > nd ? d - nd : d;
        rect_t cr = rect_make(x0 + col * cw, y0 + 14 + row * rh, cw, rh);
        char t[4];
        snprintf(t, sizeof t, "%d", shown);
        if (!other && d == dt.day) {
            gfx_circle(s, cr.x + cw / 2.0f, cr.y + rh / 2.0f, 8.5f, WG_INK);
            text_draw_center(s, fb, 9.5f, cr, RGB(28, 28, 32), t);
        } else text_draw_center(s, fs, 9.5f, cr, other ? RGBA(255, 255, 255, 70) : WG_INK, t);
    }
}

static void draw_desktop_widgets(surface_t *s, rect_t D) {
    if (!g_desktop_widgets || SW < 800) return;
    for (int i = 0; i < 2; i++) {
        rect_t r = widget_rect(i);
        if (rect_empty(rect_intersect(r, rect_inset(D, -40)))) continue;
        gfx_shadow(s, r.x, r.y, r.w, r.h, 24, 18, T.dark ? 110 : 60, 6);
        gfx_glass(s, r.x, r.y, r.w, r.h, 24, &G[GLASS_WIDGET]);
        /* contorno: fio claro por dentro (a borda "acesa" do print) e fio escuro por fora */
        gfx_round_rect_stroke(s, r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1, 23.5f, 1, RGBA(255, 255, 255, 70));
        gfx_round_rect_stroke(s, r.x - 0.5f, r.y - 0.5f, r.w + 1, r.h + 1, 24.5f, 1, RGBA(0, 0, 0, 40));
        if (i == 0) widget_clock(s, r); else widget_calendar(s, r);
    }
}

static void draw_desktop_icons(surface_t *s, rect_t D) {
    int x = SW - 100, y = MENUBAR_H + 16;
    rect_t r = rect_make(x, y, 90, 96);
    if (rect_empty(rect_intersect(r, D))) return;
    if (desk_sel == 0) gfx_round_rect(s, x + 13, y - 2, 64, 64, 6, RGBA(0, 0, 0, 60));
    icon_disk(s, x + 17, y + 2, 56);
    font_t *f = font_get(FONT_MEDIUM);
    const char *name = "mOS HD";
    int tw = text_width(f, 13, name);
    if (desk_sel == 0) gfx_round_rect(s, x + 45 - tw / 2 - 5, y + 64, tw + 10, 18, 4, T.accent);
    text_draw(s, f, 13, x + 45 - tw / 2 + 1, y + 78, RGBA(0, 0, 0, 160), name);
    text_draw(s, f, 13, x + 45 - tw / 2, y + 77, RGB(255, 255, 255), name);
}

/* ============================================================ notificações */
bool g_notify_dnd;          /* Ajustes > Notificações: Não Perturbe */
bool g_notify_sound = true; /* tocar som ao chegar notificação */
int  g_notify_count;        /* total recebido desde o boot (inclusive silenciadas) */
static void notify_put(const char *title, const char *body, int pid, int token) {
    g_notify_count++;
    if (g_notify_dnd) { kprintf("Notificação (silenciada): %s — %s\n", title, body ? body : ""); return; }
    if (nnotif == ARRAY_LEN(notifs)) { memmove(notifs, notifs + 1, sizeof(notif_t) * (ARRAY_LEN(notifs) - 1)); nnotif--; }
    notif_t *n = &notifs[nnotif++];
    memset(n, 0, sizeof *n);
    strncpy(n->title, title ? title : "", sizeof n->title - 1);
    strncpy(n->body, body ? body : "", sizeof n->body - 1);
    n->t0 = timer_ticks();
    n->pid = pid;
    n->token = token;
    if (g_notify_sound) sound_alert();
    wm_damage(rect_make(SW - 380, MENUBAR_H, 380, 120));
    kprintf("Notificação: %s — %s\n", title ? title : "", body ? body : "");
}

void notify(const char *title, const char *body) { notify_put(title, body, 0, 0); }

void notify_tagged(const char *title, const char *body, int pid, int token) {
    notify_put(title, body, pid, token > 0 ? token : 1);
}

/* O banner flutuante ocupa o canto superior direito por ~5,5 s. O clique só vale
 * para um aviso marcado com token (veio de um app). */
int notify_banner_hit(int x, int y, int *pid, int *token) {
    u64 now;
    int i, w, h, x0, y0;
    if (nc_visible() || nnotif <= 0) return 0;
    now = timer_ticks();
    for (i = nnotif - 1; i >= 0; i--) if (now - notifs[i].t0 < 5500) break;
    if (i < 0 || notifs[i].token <= 0) return 0;
    w = MIN(350, SW - 24);
    h = 72;
    x0 = SW - w - 12;
    y0 = MENUBAR_H + 10;
    if (x < x0 || y < y0 || x >= x0 + w || y >= y0 + h) return 0;
    if (pid) *pid = notifs[i].pid;
    if (token) *token = notifs[i].token;
    notifs[i].t0 = now - 6000;
    wm_damage(rect_make(SW - 400, MENUBAR_H, 400, 120));
    return 1;
}

static void draw_notifications(surface_t *s) {
    if (nc_visible()) return; /* com a Central aberta, os avisos flutuantes ficam recolhidos */
    u64 now = timer_ticks();
    int shown = 0;
    for (int i = nnotif - 1; i >= 0 && shown < 1; i--) {
        notif_t *n = &notifs[i];
        u64 age = now - n->t0;
        if (age > 5500) continue;
        float slide = age < 300 ? 1 - (age / 300.0f) : age > 5200 ? (age - 5200) / 300.0f : 0;
        slide = slide * slide;
        int w = MIN(350, SW - 24), h = 72;
        int x = SW - w - 12 + (int)(slide * (w + 20)), y = MENUBAR_H + 10;
        /* pilha: se há outra notificação recente, um cartão menor aparece por baixo */
        int stacked = 0;
        for (int j = 0; j < nnotif; j++) if (j != i && now - notifs[j].t0 < 5500) stacked++;
        if (stacked) glass_panel(s, x + 18, y + 12, w - 36, h, 22, GLASS_REGULAR);
        glass_panel(s, x, y, w, h, 24, GLASS_REGULAR);
        /* ícone do app: quadrado arredondado claro (escuro no modo escuro) com o logotipo */
        float ix = x + 14, iy = y + (h - 36) / 2.0f;
        if (T.dark) gfx_round_rect_grad(s, ix, iy, 36, 36, 9, RGB(58, 58, 62), RGB(36, 36, 40));
        else gfx_round_rect_grad(s, ix, iy, 36, 36, 9, RGB(255, 255, 255), RGB(236, 236, 240));
        gfx_specular_rim(s, ix, iy, 36, 36, 9, T.dark ? 60 : 140);
        glyph_mos_logo(s, ix + 18, iy + 18, 10, T.accent, T.dark ? RGB(48, 48, 52) : RGB(246, 246, 248));
        font_t *fb = font_get(FONT_BOLD), *fr = font_get(FONT_REGULAR);
        int tw = text_width(fr, 12.5f, "agora");
        text_draw(s, fr, 12.5f, x + w - 18 - tw, y + 26, T.glass_text2, "agora");
        text_draw_ellipsis(s, fb, 14, x + 62, y + 30, w - 62 - tw - 30, T.glass_text, n->title);
        text_draw_ellipsis(s, fr, 13.5f, x + 62, y + 50, w - 80, T.glass_text, n->body);
        shown++;
    }
}

/* ============================================================ Busca (Spotlight-like) */
static char spot_calc[64];

static double calc_expr(const char **p, bool *ok);
static double calc_atom(const char **p, bool *ok) {
    while (**p == ' ') (*p)++;
    if (**p == '(') { (*p)++; double v = calc_expr(p, ok); if (**p == ')') (*p)++; else *ok = false; return v; }
    if (**p == '-') { (*p)++; return -calc_atom(p, ok); }
    if (!isdigit(**p) && **p != '.') { *ok = false; return 0; }
    double v = 0, frac = 0.1;
    while (isdigit(**p)) v = v * 10 + (*(*p)++ - '0');
    if (**p == '.' || **p == ',') { (*p)++; while (isdigit(**p)) { v += (*(*p)++ - '0') * frac; frac /= 10; } }
    while (**p == ' ') (*p)++;
    return v;
}
static double calc_term(const char **p, bool *ok) {
    double v = calc_atom(p, ok);
    for (;;) {
        while (**p == ' ') (*p)++;
        if (**p == '*' || **p == 'x') { (*p)++; v *= calc_atom(p, ok); }
        else if (**p == '/') { (*p)++; double d = calc_atom(p, ok); if (d == 0) *ok = false; else v /= d; }
        else return v;
    }
}
static double calc_expr(const char **p, bool *ok) {
    double v = calc_term(p, ok);
    for (;;) {
        while (**p == ' ') (*p)++;
        if (**p == '+') { (*p)++; v += calc_term(p, ok); }
        else if (**p == '-') { (*p)++; v -= calc_term(p, ok); }
        else return v;
    }
}

typedef struct { app_t *app; const char *file; } spot_res_t;
static spot_res_t spot_results[10];
static int spot_n;

extern int vfs_search(const char *q, const char **out, int max);

static void spot_update(void) {
    spot_n = 0;
    spot_calc[0] = 0;
    if (!spot_q[0]) return;
    bool ok = true;
    const char *p = spot_q;
    bool has_op = strchr(spot_q, '+') || strchr(spot_q, '-') || strchr(spot_q, '*') || strchr(spot_q, '/');
    double v = calc_expr(&p, &ok);
    if (ok && !*p && has_op) {
        if (v == (double)(i64)v) snprintf(spot_calc, sizeof spot_calc, "= %lld", (i64)v);
        else snprintf(spot_calc, sizeof spot_calc, "= %.4f", v);
    }
    for (int i = 0; i < apps_count() && spot_n < 6; i++) {
        app_t *a = apps_get(i);
        if (strstr(a->name, spot_q) || strncasecmp(a->name, spot_q, strlen(spot_q)) == 0) spot_results[spot_n++] = (spot_res_t){a, NULL};
        else {
            char lname[64], lq[96];
            int k = 0;
            for (; a->name[k] && k < 63; k++) lname[k] = (char)tolower(a->name[k]);
            lname[k] = 0;
            for (k = 0; spot_q[k] && k < 95; k++) lq[k] = (char)tolower(spot_q[k]);
            lq[k] = 0;
            if (strstr(lname, lq)) spot_results[spot_n++] = (spot_res_t){a, NULL};
        }
    }
    const char *files[6];
    int nf = vfs_search(spot_q, files, 4);
    for (int i = 0; i < nf && spot_n < 10; i++) spot_results[spot_n++] = (spot_res_t){NULL, files[i]};
    if (spot_sel >= spot_n) spot_sel = 0;
}

void desktop_toggle_spotlight(void) {
    spot_open = !spot_open;
    spot_q[0] = 0; spot_caret = 0; spot_sel = 0; spot_n = 0; spot_calc[0] = 0;
    wm_damage_all();
}

static rect_t spot_rect(void) {
    int w = MIN(680, SW - 48), h = 56 + (spot_n || spot_calc[0] ? 8 + (spot_n + (spot_calc[0] ? 1 : 0)) * 40 : 0);
    return rect_make((SW - w) / 2, SH / 5, w, h);
}

static void draw_spotlight(surface_t *s) {
    if (!spot_open) return;
    rect_t r = spot_rect();
    bool results = spot_n || spot_calc[0];
    glass_panel(s, r.x, r.y, r.w, r.h, results ? 24 : 28, GLASS_THICK);
    glyph_search(s, r.x + 20, r.y + 17, 22, T.glass_text2);
    font_t *f = font_get(FONT_REGULAR);
    float fs = 24;
    int base = r.y + 37;
    if (!spot_q[0]) text_draw(s, f, fs, r.x + 54, base, T.glass_text2, "Busca");
    int tw = text_draw(s, f, fs, r.x + 54, base, T.glass_text, spot_q);
    if ((timer_ticks() / 500) % 2 == 0) gfx_round_rect(s, r.x + 54 + tw + 1, r.y + 14, 2, 28, 1, T.accent);
    int y = r.y + 60;
    if (results) gfx_fill(s, r.x + 16, r.y + 56, r.w - 32, 1, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 24));
    int idx = 0;
    if (spot_calc[0]) {
        gfx_round_rect(s, r.x + 14, y + 8, 24, 24, 5, RGB(255, 149, 0));
        text_draw_center(s, font_get(FONT_BOLD), 15, rect_make(r.x + 14, y + 8, 24, 24), RGB(255, 255, 255), "=");
        text_draw(s, font_get(FONT_MEDIUM), 18, r.x + 52, y + 27, T.text, spot_calc + 2);
        y += 40;
    }
    for (int i = 0; i < spot_n; i++, idx++) {
        bool sel = i == spot_sel;
        if (sel) { gfx_round_rect(s, r.x + 8, y + 2, r.w - 16, 36, 10, T.glass_sel); gfx_specular_rim(s, r.x + 8, y + 2, r.w - 16, 36, 10, 90); }
        u32 tc = sel ? RGB(255, 255, 255) : T.glass_text;
        if (spot_results[i].app) {
            surface_t *ic = icon_for(spot_results[i].app, 32);
            gfx_blend(s, ic, r.x + 12, y + 4, 255);
            text_draw(s, f, 16, r.x + 52, y + 26, tc, spot_results[i].app->name);
            text_draw(s, f, 13, r.x + r.w - 110, y + 25, sel ? RGBA(255, 255, 255, 200) : T.text2, "Aplicativo");
        } else {
            icon_document(s, r.x + 16, y + 6, 28, NULL);
            const char *fn = strrchr(spot_results[i].file, '/');
            text_draw(s, f, 16, r.x + 52, y + 26, tc, fn ? fn + 1 : spot_results[i].file);
            text_draw_ellipsis(s, f, 12, r.x + r.w - 260, y + 25, 240, sel ? RGBA(255, 255, 255, 200) : T.text2, spot_results[i].file);
        }
        y += 40;
    }
}

static void spot_activate(int i) {
    if (i < 0 || i >= spot_n) return;
    spot_res_t r = spot_results[i];
    desktop_toggle_spotlight();
    if (r.app) app_launch(r.app);
    else if (r.file) { extern void open_path(const char *path); open_path(r.file); }
}

/* ============================================================ alertas */
void alert_show(const char *title, const char *msg, const char **buttons, int nb, alert_cb cb, void *ctx) {
    A.open = true;
    strncpy(A.title, title, sizeof A.title - 1);
    strncpy(A.msg, msg ? msg : "", sizeof A.msg - 1);
    A.nb = MIN(nb, 3);
    for (int i = 0; i < A.nb; i++) A.buttons[i] = buttons[i];
    A.cb = cb; A.ctx = ctx; A.hover = -1; A.pressed = -1;
    sound_alert();
    wm_damage_all();
}

/* alerta no padrão atual: compacto, centralizado, título e texto à esquerda e botões em cápsula
 * de mesma largura ocupando a linha inteira (cancelar cinza, confirmar na cor de destaque) */
static rect_t alert_rect(void) { int w = MIN(320, SW - 32); return rect_make((SW - w) / 2, 0, w, 0); }

static rect_t alert_button(int i, rect_t r) {
    int gap = 10, pad = 20, bw = (r.w - 2 * pad - gap * (A.nb - 1)) / MAX(1, A.nb);
    return rect_make(r.x + pad + i * (bw + gap), r.y + r.h - pad - 30, bw, 30);
}

static rect_t alert_layout(void) {
    rect_t r = alert_rect();
    /* o título quebra em várias linhas, como no macOS ("Deseja alterar o navegador padrão para …?") */
    int tt = text_wrap(NULL, font_get(FONT_BOLD), 14, rect_make(0, 0, r.w - 40, 0), 19, 0, A.title, false);
    int th = A.msg[0] ? text_wrap(NULL, font_get(FONT_REGULAR), 12.5f, rect_make(0, 0, r.w - 40, 0), 17, 0, A.msg, false) : 0;
    r.h = 20 + 3 + MAX(tt, 19) + th + 18 + 30 + 20;
    r.y = (SH - r.h) / 2 - SH / 20;
    return r;
}

static void draw_alert(surface_t *s) {
    if (!A.open) return;
    rect_t r = alert_layout();
    gfx_shadow(s, r.x, r.y, r.w, r.h, 26, 40, T.dark ? 150 : 70, 14);
    gfx_glass(s, r.x, r.y, r.w, r.h, 26, &G[GLASS_THICK]);
    gfx_round_rect_stroke(s, r.x - 0.5f, r.y - 0.5f, r.w + 1, r.h + 1, 26.5f, 1, T.glass_hairline);
    font_t *fb = font_get(FONT_BOLD), *fr = font_get(FONT_REGULAR);
    int tt = text_wrap(s, fb, 14, rect_make(r.x + 20, r.y + 22, r.w - 40, 120), 19, T.glass_text, A.title, true);
    text_wrap(s, fr, 12.5f, rect_make(r.x + 20, r.y + 25 + MAX(tt, 19), r.w - 40, 200), 17, T.glass_text, A.msg, true);
    for (int i = 0; i < A.nb; i++) {
        rect_t b = alert_button(i, r);
        bool primary = i == A.nb - 1;
        bool down = A.pressed == i;
        u32 fill = primary ? (down ? color_lighten(T.accent, -25) : T.accent)
                           : (T.dark ? RGBA(255, 255, 255, down ? 60 : 36) : (down ? RGB(206, 206, 212) : RGB(226, 226, 230)));
        gfx_round_rect(s, b.x, b.y, b.w, b.h, b.h / 2.0f, fill);
        text_draw_center(s, fr, 14, rect_make(b.x, b.y - 1, b.w, b.h), primary ? RGB(255, 255, 255) : T.glass_text, A.buttons[i]);
    }
}

static void alert_finish(int i) {
    A.open = false;
    wm_damage_all();
    if (A.cb) A.cb(i, A.ctx);
}

/* ============================================================ desligar / reiniciar */
static void shutdown_cb(int b, void *ctx) {
    int kind = (int)(i64)ctx;
    if (b == 1) { shutdown_pending = kind; shutdown_at = timer_ticks() + 400; }
}

void desktop_request_restart(void) { shutdown_pending = 1; shutdown_at = timer_ticks() + 400; }

void desktop_shutdown_dialog(int kind) {
    static const char *btns_off[] = {"Cancelar", "Desligar"};
    static const char *btns_rb[] = {"Cancelar", "Reiniciar"};
    alert_show(kind ? "Tem certeza de que deseja reiniciar o computador agora?" : "Tem certeza de que deseja desligar o computador agora?",
               "Os aplicativos abertos serão encerrados. Documentos não salvos no pendrive serão perdidos.",
               kind ? btns_rb : btns_off, 2, shutdown_cb, (void *)(i64)kind);
}

/* ============================================================ sobre */
void desktop_show_about(void) {
    extern void about_open(void);
    about_open();
}

/* ============================================================ Central de Notificações */
static struct { bool open, closing; float t; } NC;
#define NC_W 360

static bool nc_visible(void) { return NC.open; }

static void nc_toggle(void) {
    if (NC.open && !NC.closing) NC.closing = true;
    else { NC.open = true; NC.closing = false; NC.t = 0; cc_open = false; }
    wm_damage_all();
}

static rect_t nc_rect(void) {
    float k = NC.t * NC.t * (3 - 2 * NC.t);
    int w = MIN(NC_W, SW - 16);
    int x = SW - w - 8 + (int)((1 - k) * (w + 24));
    return rect_make(x, MENUBAR_H + 6, w, SH - MENUBAR_H - 16 - (dock_r.h + 14));
}

static void nc_step(void) {
    if (!NC.open) return;
    float before = NC.t;
    if (NC.closing) { NC.t -= 0.2f; if (NC.t <= 0) { NC.t = 0; NC.open = NC.closing = false; } }
    else if (NC.t < 1) { NC.t += 0.2f; if (NC.t > 1) NC.t = 1; }
    if (NC.t != before || !NC.open) wm_damage_all();
}

static void nc_card(surface_t *s, rect_t r, float rad) {
    glass_t g = G[GLASS_CONTROL];
    gfx_glass(s, r.x, r.y, r.w, r.h, rad, &g);
}

static int nc_days_in_month(int y, int m) {
    static const int d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) ? 29 : d[m - 1];
}
static int nc_weekday(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static rect_t nc_clear_btn;

static void draw_notification_center(surface_t *s) {
    if (!NC.open) return;
    rect_t r = nc_rect();
    rect_t prev = surface_push_clip(s, rect_make(0, MENUBAR_H, SW, SH - MENUBAR_H));
    font_t *fb = font_get(FONT_BOLD), *fs = font_get(FONT_SEMIBOLD), *fr = font_get(FONT_REGULAR);
    datetime_t dt;
    rtc_now(&dt);
    int y = r.y;
    /* --- notificações */
    /* título legível sobre qualquer papel de parede: texto escuro/claro conforme a barra de menus */
    u32 hc = bar_on_light ? RGB(20, 20, 22) : RGB(245, 245, 247);
    text_draw(s, fs, 15, r.x + 6, y + 19, bar_on_light ? RGBA(255, 255, 255, 120) : RGBA(0, 0, 0, 120), nnotif ? "Notificações" : "Sem notificações");
    text_draw(s, fs, 15, r.x + 6, y + 18, hc, nnotif ? "Notificações" : "Sem notificações");
    nc_clear_btn = rect_make(0, 0, 0, 0);
    if (nnotif) {
        nc_clear_btn = rect_make(r.x + r.w - 70, y + 2, 64, 24);
        nc_card(s, nc_clear_btn, 12);
        text_draw_center(s, font_get(FONT_MEDIUM), 12.5f, nc_clear_btn, T.glass_text, "Limpar");
    }
    y += 32;
    u64 now = timer_ticks();
    for (int i = nnotif - 1; i >= 0 && y < r.y + r.h - 340; i--) {
        notif_t *n = &notifs[i];
        rect_t c = rect_make(r.x, y, r.w, 64);
        nc_card(s, c, 18);
        float ix = c.x + 12, iy = c.y + 14;
        gfx_round_rect_grad(s, ix, iy, 36, 36, 9, T.dark ? RGB(58, 58, 62) : RGB(255, 255, 255), T.dark ? RGB(36, 36, 40) : RGB(236, 236, 240));
        glyph_mos_logo(s, ix + 18, iy + 18, 10, T.accent, T.dark ? RGB(48, 48, 52) : RGB(246, 246, 248));
        char when[24];
        u64 mins = (now - n->t0) / 60000;
        if (mins < 1) strcpy(when, "agora"); else snprintf(when, sizeof when, "há %llu min", mins);
        int ww = text_width(fr, 12, when);
        text_draw(s, fr, 12, c.x + c.w - 14 - ww, c.y + 22, T.glass_text2, when);
        text_draw_ellipsis(s, fb, 13.5f, c.x + 58, c.y + 26, c.w - 58 - ww - 24, T.glass_text, n->title);
        text_draw_ellipsis(s, fr, 13, c.x + 58, c.y + 45, c.w - 72, T.glass_text, n->body);
        y += 72;
    }
    /* --- widgets: relógio e calendário (lado a lado), sempre na parte de baixo do painel */
    int wy = y + 8; /* widgets logo abaixo das notificações */
    int half = (r.w - 10) / 2;
    rect_t wc = rect_make(r.x, wy, half, half);
    nc_card(s, wc, 22);
    char hh[8];
    snprintf(hh, sizeof hh, "%02d:%02d", dt.hour, dt.minute);
    text_draw_center(s, font_get(FONT_SEMIBOLD), 40, rect_make(wc.x, wc.y + 10, wc.w, wc.h - 40), T.glass_text, hh);
    static const char *wdl[7] = {"Domingo", "Segunda", "Terça", "Quarta", "Quinta", "Sexta", "Sábado"};
    text_draw_center(s, fr, 13, rect_make(wc.x, wc.y + wc.h - 44, wc.w, 20), T.glass_text2, wdl[dt.weekday]);
    rect_t wk = rect_make(r.x + half + 10, wy, half, half);
    nc_card(s, wk, 22);
    static const char *mon[12] = {"JANEIRO", "FEVEREIRO", "MARÇO", "ABRIL", "MAIO", "JUNHO", "JULHO", "AGOSTO", "SETEMBRO", "OUTUBRO", "NOVEMBRO", "DEZEMBRO"};
    text_draw(s, font_get(FONT_BOLD), 11, wk.x + 12, wk.y + 20, RGB(255, 69, 58), mon[dt.month - 1]);
    int first = nc_weekday(dt.year, dt.month, 1), nd = nc_days_in_month(dt.year, dt.month);
    int cw = (wk.w - 16) / 7, chh = (wk.h - 34) / 6;
    for (int d = 1; d <= nd; d++) {
        int slot = first + d - 1;
        rect_t c = rect_make(wk.x + 8 + (slot % 7) * cw, wk.y + 28 + (slot / 7) * chh, cw, chh);
        char num[4];
        snprintf(num, sizeof num, "%d", d);
        if (d == dt.day) gfx_circle(s, c.x + c.w / 2.0f, c.y + c.h / 2.0f, MIN(c.w, c.h) / 2.0f, RGB(255, 69, 58));
        text_draw_center(s, d == dt.day ? fb : fr, 10.5f, c, d == dt.day ? RGB(255, 255, 255) : T.glass_text, num);
    }
    /* --- widget largo: data por extenso */
    rect_t wd = rect_make(r.x, wy + half + 10, r.w, MIN(110, r.y + r.h - (wy + half + 10)));
    if (wd.h > 50) {
        nc_card(s, wd, 22);
        char big[48];
        static const char *ml[12] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"};
        snprintf(big, sizeof big, "%d de %s de %d", dt.day, ml[dt.month - 1], dt.year);
        text_draw(s, fs, 13, wd.x + 16, wd.y + 26, T.glass_text2, wdl[dt.weekday]);
        text_draw(s, font_get(FONT_BOLD), 20, wd.x + 16, wd.y + 54, T.glass_text, big);
    }
    surface_pop_clip(s, prev);
}

static bool nc_mouse(int x, int y, bool lp) {
    if (!NC.open || NC.closing) return false;
    rect_t r = nc_rect();
    if (!lp) return rect_contains(r, x, y);
    if (rect_contains(nc_clear_btn, x, y)) { nnotif = 0; wm_damage_all(); return true; }
    if (rect_contains(r, x, y)) return true;
    if (y >= MENUBAR_H) { nc_toggle(); return true; }
    return false;
}

/* ============================================================ alternador de apps (⌘Tab) */
static struct { bool open; app_t *apps[16]; int n, sel; } AS;

static void app_switcher_build(void) {
    extern window_t *wm_nth(int);
    AS.n = 0;
    int cnt = 0;
    while (wm_nth(cnt)) cnt++;
    /* do topo para o fundo: o app mais recente primeiro */
    for (int i = cnt - 1; i >= 0 && AS.n < 16; i--) {
        window_t *w = wm_nth(i);
        if (!w->app || w->closing) continue;
        bool dup = false;
        for (int k = 0; k < AS.n; k++) if (AS.apps[k] == w->app) dup = true;
        if (!dup) AS.apps[AS.n++] = w->app;
    }
}

static rect_t app_switcher_rect(void) {
    int cell = 108, w = MIN(SW - 40, 24 + AS.n * cell), h = 150;
    return rect_make((SW - w) / 2, (SH - h) / 2 - SH / 12, w, h);
}

static void app_switcher_step(int d) {
    if (!AS.open) {
        app_switcher_build();
        if (!AS.n) return;
        AS.open = true;
        AS.sel = AS.n > 1 ? (d > 0 ? 1 : AS.n - 1) : 0;
    } else if (AS.n) {
        AS.sel = (AS.sel + d + AS.n) % AS.n;
    }
    wm_damage_all();
}

static void app_switcher_finish(bool commit) {
    if (!AS.open) return;
    AS.open = false;
    if (commit && AS.sel >= 0 && AS.sel < AS.n) wm_bring_app(AS.apps[AS.sel]);
    wm_damage_all();
}

static void draw_app_switcher(surface_t *s) {
    if (!AS.open) return;
    rect_t r = app_switcher_rect();
    glass_panel(s, r.x, r.y, r.w, r.h, 26, GLASS_THICK);
    int cell = 108, isz = 84;
    int maxn = (r.w - 24) / cell;
    int first = MAX(0, MIN(AS.sel - maxn / 2, AS.n - maxn));
    for (int i = first; i < AS.n && i - first < maxn; i++) {
        int x = r.x + 12 + (i - first) * cell, y = r.y + 16;
        if (i == AS.sel) gfx_round_rect(s, x + 4, y - 4, cell - 8, isz + 12, 18, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 30));
        gfx_blit_scaled(s, icon_for(AS.apps[i], isz), x + (cell - isz) / 2, y + 2, isz, isz, 255);
    }
    font_t *f = font_get(FONT_MEDIUM);
    text_draw_center(s, f, 14, rect_make(r.x, r.y + r.h - 36, r.w, 24), T.glass_text, AS.apps[AS.sel]->name);
}

/* ============================================================ diálogo de salvar */
static const char *sd_where_names[] = {"Mesa", "Documentos", "Downloads", "Imagens", "Música"};
static const char *sd_where_paths[] = {"/Users/Visitante/Mesa", "/Users/Visitante/Documentos", "/Users/Visitante/Downloads",
                                       "/Users/Visitante/Imagens", "/Users/Visitante/Música"};
#define SD_POP_FMT 9101
#define SD_POP_WHERE 9102
static struct {
    bool open, del, sel_all; /* sel_all: o nome sugerido começa selecionado (digitar substitui) */
    char title[120], msg[240], name[96], tags[64];
    const char *fmts[8];
    int nf, fmt, where, focus, pressed;
    save_cb cb;
    void *ctx;
} SD;

typedef struct { rect_t r, name, tags, fmt, where, more, bdel, bcancel, bsave; int title_h, msg_h; } sd_geo_t;

static sd_geo_t sd_layout(void) {
    sd_geo_t g;
    int w = MIN(480, SW - 32), tw = w - 96 - 28;
    g.title_h = text_wrap(NULL, font_get(FONT_BOLD), 14, rect_make(0, 0, tw, 0), 18, 0, SD.title, false);
    g.msg_h = SD.msg[0] ? text_wrap(NULL, font_get(FONT_REGULAR), 12.5f, rect_make(0, 0, tw, 0), 16, 0, SD.msg, false) : 0;
    int head = MAX(56, g.title_h + 6 + g.msg_h);
    int h = 24 + head + 20 + 4 * 36 + 14 + 28 + 22;
    g.r = rect_make((SW - w) / 2, MAX(MENUBAR_H + 10, (SH - h) / 2 - SH / 12), w, h);
    int cx = g.r.x + 144, cw = w - 144 - 28, y = g.r.y + 24 + head + 20;
    g.name = rect_make(cx, y, cw, 26);
    g.tags = rect_make(cx, y + 36, cw, 26);
    g.fmt = rect_make(cx, y + 72, cw, 26);
    g.where = rect_make(cx, y + 108, cw - 36, 26);
    g.more = rect_make(cx + cw - 28, y + 108, 28, 26);
    int by = g.r.y + h - 22 - 28;
    g.bsave = rect_make(g.r.x + w - 28 - 92, by, 92, 28);
    g.bcancel = rect_make(g.bsave.x - 10 - 92, by, 92, 28);
    g.bdel = rect_make(g.r.x + 28, by, 92, 28);
    return g;
}

bool save_dialog_open(void) { return SD.open; }

void save_dialog_show(const char *title, const char *msg, const char *name, const char **formats, int nformats,
                      bool allow_delete, save_cb cb, void *ctx) {
    memset(&SD, 0, sizeof SD);
    SD.open = true;
    strncpy(SD.title, title ? title : "Salvar", sizeof SD.title - 1);
    strncpy(SD.msg, msg ? msg : "", sizeof SD.msg - 1);
    strncpy(SD.name, name ? name : "Sem título", sizeof SD.name - 1);
    SD.nf = MIN(nformats, 8);
    for (int i = 0; i < SD.nf; i++) SD.fmts[i] = formats[i];
    SD.where = 1; /* Documentos */
    SD.del = allow_delete;
    SD.cb = cb; SD.ctx = ctx;
    SD.pressed = -1;
    SD.sel_all = true;
    desktop_close_menus();
    wm_damage_all();
}

static void sd_finish(int button) {
    SD.open = false;
    wm_damage_all();
    if (SD.cb) SD.cb(button, sd_where_paths[SD.where], SD.name, SD.fmt, SD.ctx);
}

static void sd_field(surface_t *s, rect_t r, const char *text, bool focused, bool selected) {
    if (focused) gfx_round_rect_stroke(s, r.x - 3.5f, r.y - 3.5f, r.w + 7, r.h + 7, CTRL_RADIUS + 3.5f, 3.5f, WITH_ALPHA(T.accent, 120));
    gfx_round_rect(s, r.x, r.y, r.w, r.h, CTRL_RADIUS, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 34));
    gfx_round_rect(s, r.x + 1, r.y + 1, r.w - 2, r.h - 2, CTRL_RADIUS - 1, T.dark ? RGB(24, 24, 26) : RGB(255, 255, 255));
    font_t *f = font_get(FONT_REGULAR);
    rect_t prev = surface_push_clip(s, rect_make(r.x + 2, r.y, r.w - 4, r.h));
    int bl = r.y + (r.h - (text_ascent(f, 14) + text_descent(f, 14))) / 2 + text_ascent(f, 14);
    if (selected && text[0]) gfx_round_rect(s, r.x + 6, r.y + 4, text_width(f, 14, text) + 4, r.h - 8, 3, T.selection);
    int tw = text_draw(s, f, 14, r.x + 8, bl, T.glass_text, text);
    if (focused && !selected && (timer_ticks() / 530) % 2 == 0) gfx_fill(s, r.x + 9 + tw, r.y + 5, 2, r.h - 10, T.accent);
    surface_pop_clip(s, prev);
}

static void sd_popup(surface_t *s, rect_t r, const char *text) {
    gfx_round_rect(s, r.x, r.y, r.w, r.h, CTRL_RADIUS, T.dark ? RGBA(255, 255, 255, 30) : RGB(232, 232, 235));
    font_t *f = font_get(FONT_REGULAR);
    int bl = r.y + (r.h - (text_ascent(f, 14) + text_descent(f, 14))) / 2 + text_ascent(f, 14);
    text_draw_ellipsis(s, f, 14, r.x + 12, bl, r.w - 40, T.glass_text, text);
    glyph_updown(s, r.x + r.w - 22, r.y + r.h / 2 - 6, 12, T.glass_text);
}

static void draw_save_dialog(surface_t *s) {
    if (!SD.open) return;
    int v;
    if (popup_take_result(NULL, SD_POP_FMT, &v)) SD.fmt = CLAMP(v, 0, SD.nf - 1);
    if (popup_take_result(NULL, SD_POP_WHERE, &v)) SD.where = CLAMP(v, 0, 4);
    sd_geo_t g = sd_layout();
    rect_t r = g.r;
    gfx_shadow(s, r.x, r.y, r.w, r.h, 24, 40, T.dark ? 150 : 70, 14);
    gfx_glass(s, r.x, r.y, r.w, r.h, 24, &G[GLASS_THICK]);
    gfx_round_rect_stroke(s, r.x - 0.5f, r.y - 0.5f, r.w + 1, r.h + 1, 24.5f, 1, T.glass_hairline);
    icon_document(s, r.x + 28, r.y + 24, 52, "txt");
    font_t *fb = font_get(FONT_BOLD), *fr = font_get(FONT_REGULAR);
    int tw = r.w - 96 - 28;
    text_wrap(s, fb, 14, rect_make(r.x + 96, r.y + 24, tw, 200), 18, T.glass_text, SD.title, true);
    text_wrap(s, fr, 12.5f, rect_make(r.x + 96, r.y + 24 + g.title_h + 6, tw, 200), 16, T.glass_text, SD.msg, true);
    static const char *labels[] = {"Salvar como:", "Etiquetas:", "Formato:", "Onde:"};
    rect_t rows[4] = {g.name, g.tags, g.fmt, g.where};
    for (int i = 0; i < 4; i++) {
        int lw = text_width(fr, 14, labels[i]);
        int bl = rows[i].y + (rows[i].h - (text_ascent(fr, 14) + text_descent(fr, 14))) / 2 + text_ascent(fr, 14);
        text_draw(s, fr, 14, g.name.x - 10 - lw, bl, T.glass_text2, labels[i]);
    }
    sd_field(s, g.name, SD.name, SD.focus == 0, SD.focus == 0 && SD.sel_all);
    sd_field(s, g.tags, SD.tags, SD.focus == 1, false);
    sd_popup(s, g.fmt, SD.nf ? SD.fmts[SD.fmt] : "Texto Simples");
    sd_popup(s, g.where, sd_where_names[SD.where]);
    gfx_round_rect(s, g.more.x, g.more.y, g.more.w, g.more.h, CTRL_RADIUS, T.dark ? RGBA(255, 255, 255, 30) : RGB(232, 232, 235));
    glyph_chevron_down(s, g.more.x + 7, g.more.y + 6, 14, T.glass_text);
    if (SD.del) ui_draw_button(s, g.bdel, "Apagar", BTN_TINTED_RED, SD.pressed == 0 ? 1 : 0, true);
    ui_draw_button(s, g.bcancel, "Cancelar", BTN_GRAY, SD.pressed == 1 ? 1 : 0, true);
    ui_draw_button(s, g.bsave, "Salvar", BTN_FILLED, !SD.name[0] ? 2 : SD.pressed == 2 ? 1 : 0, true);
}

static bool sd_key(input_event_t *e) {
    char *buf = SD.focus == 0 ? SD.name : SD.tags;
    int cap = SD.focus == 0 ? (int)sizeof SD.name : (int)sizeof SD.tags;
    int len = (int)strlen(buf);
    bool edit = e->key == KEY_BACKSPACE || (e->key >= 32 && e->key < 0x100 && !(e->mods & (MOD_CMD | MOD_CTRL)));
    if (SD.focus == 0 && SD.sel_all && edit) { buf[0] = 0; len = 0; SD.sel_all = false; if (e->key == KEY_BACKSPACE) { wm_damage_all(); return true; } }
    else if (e->key == KEY_LEFT || e->key == KEY_RIGHT || e->key == KEY_TAB) SD.sel_all = false;
    if (e->key == KEY_ESC) sd_finish(1);
    else if (e->key == KEY_ENTER) { if (SD.name[0]) sd_finish(2); }
    else if (e->key == KEY_TAB) SD.focus ^= 1;
    else if (e->key == KEY_BACKSPACE) { if (len) buf[utf8_prev(buf, len)] = 0; }
    else if (e->key >= 32 && e->key < 0x100 && e->key != '/' && !(e->mods & (MOD_CMD | MOD_CTRL)) && len < cap - 5) {
        len += utf8_encode((u32)e->key, buf + len);
        buf[len] = 0;
    }
    wm_damage_all();
    return true;
}

static bool sd_mouse(int x, int y, bool lp, bool lr) {
    sd_geo_t g = sd_layout();
    rect_t btn[3] = {g.bdel, g.bcancel, g.bsave};
    int h = -1;
    for (int i = 0; i < 3; i++) if (rect_contains(btn[i], x, y) && (i || SD.del)) h = i;
    if (lp) {
        SD.pressed = h;
        if (rect_contains(g.name, x, y)) SD.focus = 0;
        else if (rect_contains(g.tags, x, y)) SD.focus = 1;
        else if (rect_contains(g.fmt, x, y) && SD.nf) popup_open(g.fmt.x - 8, g.fmt.y - 7 - SD.fmt * 24, g.fmt.w + 16, SD.fmts, SD.nf, SD.fmt, NULL, SD_POP_FMT);
        else if (rect_contains(g.where, x, y) || rect_contains(g.more, x, y))
            popup_open(g.where.x - 8, g.where.y - 7 - SD.where * 24, g.where.w + 16, sd_where_names, 5, SD.where, NULL, SD_POP_WHERE);
        wm_damage_all();
    }
    if (lr) {
        int p = SD.pressed;
        SD.pressed = -1;
        wm_damage_all();
        if (p >= 0 && p == h && (p != 2 || SD.name[0])) sd_finish(p);
    }
    return true;
}

/* ============================================================ Central de Controle */
enum { CC_NET, CC_BT, CC_DARK, CC_MAGNIFY, CC_STAGE, CC_FOCUS, CC_ACCENT, CC_SOUND, CC_DOCKSIZE, CC_GLASS, CC_N };
#define CC_W 332
#define CC_PAD 14


static rect_t cc_rect(void) {
    int w = MIN(CC_W, SW - 16), h = CC_PAD * 2 + 64 * 4 + 70 * 3 + 10 * 6;
    int x = SW - w - 8;
    for (int i = 0; i < nstatus; i++) if (status[i].title == TITLE_CC) x = status[i].x + status[i].w - w;
    x = CLAMP(x, 8, SW - w - 8);
    return rect_make(x, MENUBAR_H + 6, w, MIN(h, SH - MENUBAR_H - 16));
}

static rect_t cc_item(int i) {
    rect_t p = cc_rect();
    int iw = p.w - 2 * CC_PAD, half = (iw - 10) / 2;
    int x = p.x + CC_PAD, y = p.y + CC_PAD;
    switch (i) {
    case CC_NET: return rect_make(x, y, half, 64);
    case CC_BT: return rect_make(x + half + 10, y, iw - half - 10, 64);
    case CC_DARK: return rect_make(x, y + 74, half, 64);
    case CC_MAGNIFY: return rect_make(x + half + 10, y + 74, iw - half - 10, 64);
    case CC_STAGE: return rect_make(x, y + 148, half, 64);
    case CC_FOCUS: return rect_make(x + half + 10, y + 148, iw - half - 10, 64);
    case CC_ACCENT: return rect_make(x, y + 222, iw, 64);
    case CC_SOUND: return rect_make(x, y + 296, iw, 70);
    case CC_DOCKSIZE: return rect_make(x, y + 376, iw, 70);
    default: return rect_make(x, y + 456, iw, 70);
    }
}

static rect_t cc_slider_track(int i) { rect_t m = cc_item(i); return rect_make(m.x + 18, m.y + 38, m.w - 36, 22); }
static rect_t cc_accent_dot(int k) {
    rect_t m = cc_item(CC_ACCENT);
    int step = (m.w - 36) / 8;
    return rect_make(m.x + 18 + k * step + (step - 22) / 2, m.y + 32, 22, 22);
}

/* bloco de vidro aninhado (o fundo dele já é o vidro da Central) */
static void cc_module(surface_t *s, rect_t r, float rad, bool hover) {
    glass_t g = G[GLASS_CONTROL];
    if (hover) g.tint = WITH_ALPHA(g.tint, MIN(255, (int)C_A(g.tint) + 40));
    gfx_glass(s, r.x, r.y, r.w, r.h, rad, &g);
}

static void cc_tile(surface_t *s, int idx, const char *title, const char *sub, bool on, void (*glyph)(surface_t *, int, int, int, u32)) {
    rect_t r = cc_item(idx);
    bool hov = rect_contains(r, wm_mouse_x(), wm_mouse_y());
    cc_module(s, r, r.h / 2.0f, hov);
    float cx = r.x + 32, cy = r.y + r.h / 2.0f;
    if (on) { gfx_circle(s, cx, cy, 19, RGB(255, 255, 255)); gfx_specular_rim(s, cx - 19, cy - 19, 38, 38, 19, 120); }
    else glass_capsule_fill(s, cx - 19, cy - 19, 38, 38, 19, T.glass_fill_hi, T.dark ? 90 : 200);
    glyph(s, (int)cx - 9, (int)cy - 9, 18, on ? T.accent : T.glass_text);
    font_t *fb = font_get(FONT_SEMIBOLD), *fr = font_get(FONT_REGULAR);
    text_draw_ellipsis(s, fb, 13, r.x + 60, r.y + 28, r.w - 68, T.glass_text, title);
    text_draw_ellipsis(s, fr, 11.5f, r.x + 60, r.y + 45, r.w - 68, T.glass_text2, sub);
}

static void g_net(surface_t *s, int x, int y, int sz, u32 c) { if (net_link_up()) glyph_ethernet(s, x, y, sz, c); else glyph_wifi(s, x, y, sz, c, 0); }
static void g_moon(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_circle(s, x + sz / 2.0f, y + sz / 2.0f, sz * 0.45f, c);
    gfx_circle(s, x + sz * 0.68f, y + sz * 0.34f, sz * 0.36f, T.dark ? RGB(255, 255, 255) : RGBA(0, 0, 0, 0));
}
static void g_moon_tile(surface_t *s, int x, int y, int sz, u32 c) {
    /* meio círculo preenchido: aparência claro/escuro */
    gfx_circle_stroke(s, x + sz / 2.0f, y + sz / 2.0f, sz * 0.45f, 1.6f, c);
    gfx_ring_arc(s, x + sz / 2.0f, y + sz / 2.0f, sz * 0.45f, sz * 0.45f, PI_F, 2 * PI_F, c);
}
static void g_dockmag(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_round_rect(s, x, y + sz * 0.62f, sz, sz * 0.3f, 3, c);
    gfx_round_rect(s, x + sz * 0.3f, y + sz * 0.05f, sz * 0.4f, sz * 0.4f, 3, c);
}

/* Stage Manager: retângulo central com três tracinhos à esquerda */
static void g_stage(surface_t *s, int x, int y, int sz, u32 c) {
    gfx_round_rect_stroke(s, x + sz * 0.32f, y + sz * 0.16f, sz * 0.66f, sz * 0.68f, 2.5f, 1.5f, c);
    for (int i = 0; i < 3; i++) gfx_round_rect(s, x, y + sz * (0.2f + i * 0.24f), sz * 0.2f, sz * 0.14f, 1.5f, c);
}
/* Foco: lua crescente */
void glyph_focus_moon(surface_t *s, int x, int y, int sz, u32 c) {
    float cx = x + sz / 2.0f, cy = y + sz / 2.0f, r = sz * 0.42f;
    for (int yy = (int)(cy - r) - 1; yy <= (int)(cy + r) + 1; yy++)
        for (int xx = (int)(cx - r) - 1; xx <= (int)(cx + r) + 1; xx++) {
            float d1 = fsqrt((xx + 0.5f - cx) * (xx + 0.5f - cx) + (yy + 0.5f - cy) * (yy + 0.5f - cy));
            float ox = cx + r * 0.45f, oy = cy - r * 0.35f;
            float d2 = fsqrt((xx + 0.5f - ox) * (xx + 0.5f - ox) + (yy + 0.5f - oy) * (yy + 0.5f - oy));
            float a = CLAMP(r - d1 + 0.5f, 0.0f, 1.0f) * CLAMP(d2 - r * 0.78f + 0.5f, 0.0f, 1.0f);
            if (a > 0) gfx_fill(s, xx, yy, 1, 1, WITH_ALPHA(c, (int)(C_A(c) * a)));
        }
}

static void cc_slider(surface_t *s, int idx, const char *title, float v, void (*icon)(surface_t *, int, int, int, u32)) {
    rect_t m = cc_item(idx);
    cc_module(s, m, 22, false);
    text_draw(s, font_get(FONT_SEMIBOLD), 13, m.x + 18, m.y + 24, T.glass_text, title);
    rect_t t = cc_slider_track(idx);
    float cy = t.y + t.h / 2.0f;
    /* trilho grosso estilo vidro, preenchido em branco como nos controles do sistema */
    gfx_round_rect(s, t.x, cy - 8, t.w, 16, 8, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 22));
    float fw = 16 + (t.w - 16) * CLAMP(v, 0.0f, 1.0f);
    gfx_round_rect(s, t.x, cy - 8, fw, 16, 8, T.dark ? RGBA(255, 255, 255, 235) : RGB(255, 255, 255));
    gfx_specular_rim(s, t.x, cy - 8, t.w, 16, 8, T.dark ? 70 : 160);
    if (icon) icon(s, t.x + 4, (int)cy - 6, 12, RGB(90, 90, 96));
}

static void cc_speaker(surface_t *s, int x, int y, int sz, u32 c) { glyph_speaker(s, x, y, sz, c, 2); }

static void draw_control_center(surface_t *s) {
    if (!cc_open) return;
    rect_t p = cc_rect();
    glass_panel(s, p.x, p.y, p.w, p.h, 28, GLASS_REGULAR);
    rect_t prev = surface_push_clip(s, p);
    char sub[64];
    snprintf(sub, sizeof sub, "%s", net_link_up() ? "Ethernet" : "Sem conexão");
    cc_tile(s, CC_NET, "Rede", sub, net_link_up(), g_net);
    bool bt = strstr(bt_status(), "Ligado") != NULL;
    cc_tile(s, CC_BT, "Bluetooth", bt ? "Ligado" : "Desligado", bt, glyph_bluetooth);
    cc_tile(s, CC_DARK, "Escuro", T.dark ? "Ativado" : "Desativado", T.dark, g_moon_tile);
    cc_tile(s, CC_MAGNIFY, "Ampliação", g_dock_magnify ? "Ativada" : "Desativada", g_dock_magnify, glyph_search);
    cc_tile(s, CC_STAGE, "Stage Manager", g_stage_manager ? "Ativado" : "Desativado", g_stage_manager, g_stage);
    cc_tile(s, CC_FOCUS, "Foco", g_focus_mode ? focus_names[g_focus_mode] : "Desativado", g_focus_mode != 0, glyph_focus_moon);
    (void)g_moon; (void)g_dockmag;
    /* cores de destaque */
    rect_t am = cc_item(CC_ACCENT);
    cc_module(s, am, 22, false);
    text_draw(s, font_get(FONT_SEMIBOLD), 13, am.x + 18, am.y + 22, T.glass_text, "Cor de destaque");
    for (int k = 0; k < 8; k++) {
        rect_t d = cc_accent_dot(k);
        float cx = d.x + 11, cy = d.y + 11;
        if (k == g_accent_index) gfx_circle_stroke(s, cx, cy, 12.5f, 2, T.glass_text);
        gfx_circle(s, cx, cy, 9, g_accent_colors[k]);
        gfx_specular_rim(s, cx - 9, cy - 9, 18, 18, 9, 130);
    }
    cc_slider(s, CC_SOUND, audio_available() ? "Som" : "Som (sem dispositivo)", audio_get_volume() / 100.0f, cc_speaker);
    cc_slider(s, CC_DOCKSIZE, "Tamanho do Dock", (g_dock_size - 32) / 64.0f, NULL);
    cc_slider(s, CC_GLASS, "Vidro (transparente → opaco)", g_glass_level / 100.0f, NULL);
    surface_pop_clip(s, prev);
}

void desktop_toggle_control_center(void) {
    cc_open = !cc_open;
    cc_drag = -1;
    desktop_close_menus();
    wm_damage_all();
}

static void repaint_all_windows(void) {
    extern window_t *wm_nth(int);
    for (int i = 0;; i++) { window_t *w = wm_nth(i); if (!w) break; w->needs_paint = true; }
}

static void cc_set_slider(int idx, int x) {
    rect_t t = cc_slider_track(idx);
    float v = CLAMP((float)(x - t.x - 8) / MAX(1, t.w - 16), 0.0f, 1.0f);
    if (idx == CC_SOUND) audio_set_volume((int)(v * 100 + 0.5f));
    else if (idx == CC_DOCKSIZE) { g_dock_size = 32 + (int)(v * 64 + 0.5f); }
    else {
        /* opacidade do vidro: reaplica o tema e repinta tudo */
        g_glass_level = (int)(v * 100 + 0.5f);
        theme_set(T.dark, g_accent_index);
        repaint_all_windows();
    }
    wm_damage_all();
}

/* devolve true se o evento foi consumido */
static bool cc_mouse(int x, int y, int buttons, bool lp, bool lr) {
    if (!cc_open) return false;
    if (M.kind != MK_NONE && rect_contains(rect_make(M.x, M.y, M.w, M.h), x, y)) return false;   /* menu de Foco */
    rect_t p = cc_rect();
    if (cc_drag >= 0) {
        if (buttons & 1) cc_set_slider(cc_drag, x);
        if (lr) cc_drag = -1;
        return true;
    }
    if (!rect_contains(p, x, y)) {
        if (lp && y >= MENUBAR_H) { cc_open = false; wm_damage_all(); return true; }
        return false;
    }
    static int last_hover = -1;
    int hov = -1;
    for (int i = 0; i < CC_N; i++) if (rect_contains(cc_item(i), x, y)) hov = i;
    if (hov != last_hover) { last_hover = hov; wm_damage(rect_inset(p, -40)); }
    if (lp) {
        for (int i = CC_SOUND; i <= CC_GLASS; i++)
            if (rect_contains(rect_inset(cc_slider_track(i), -6), x, y)) { cc_drag = i; cc_set_slider(i, x); return true; }
        for (int k = 0; k < 8; k++)
            if (rect_contains(rect_inset(cc_accent_dot(k), -3), x, y)) {
                theme_set(T.dark, k);
                repaint_all_windows();
                wm_damage_all();
                return true;
            }
        switch (hov) {
        case CC_DARK: run_action(ACT_DARK_TOGGLE); wm_damage_all(); break;
        case CC_MAGNIFY: g_dock_magnify = !g_dock_magnify; wm_damage_all(); break;
        case CC_STAGE: wm_toggle_stage_manager(); wm_damage_all(); break;
        case CC_FOCUS: {
            /* menu com os modos, aberto ao lado do bloco */
            rect_t r = cc_item(CC_FOCUS);
            submenu_close();
            M.kind = MK_CONTEXT; M.n = 0; M.hover = -1; M.armed = false; M.opened_at = timer_ticks();
            add_item("Foco", NULL, 0, false, false);
            M.items[0].header = true;
            for (int k = 1; k < 5; k++) add_item(focus_names[k], NULL, ACT_FOCUS_0 + k, false, g_focus_mode == k);
            add_sep();
            add_item("Desativar Foco", NULL, ACT_FOCUS_0, g_focus_mode == 0, false);
            int mw = list_width(M.items, M.n);
            M.x = cc_rect().x - mw - 8; M.y = r.y;
            menu_layout();
            wm_damage_all();
            break;
        }
        case CC_NET: cc_open = false; run_action(ACT_NET_PREFS); wm_damage_all(); break;
        case CC_BT: cc_open = false; run_action(ACT_BT_PREFS); wm_damage_all(); break;
        }
    }
    return true;
}

/* ============================================================ Launchpad (grade de apps) */
static app_t *lp_apps[32];
static int lp_n;

static void lp_filter(void) {
    lp_n = 0;
    char lq[64];
    int k = 0;
    for (; lp_q[k] && k < 63; k++) lq[k] = (char)tolower(lp_q[k]);
    lq[k] = 0;
    for (int i = 0; i < apps_count() && lp_n < (int)ARRAY_LEN(lp_apps); i++) {
        app_t *a = apps_get(i);
        char ln[64];
        int j = 0;
        for (; a->name[j] && j < 63; j++) ln[j] = (char)tolower(a->name[j]);
        ln[j] = 0;
        if (!lq[0] || strstr(ln, lq)) lp_apps[lp_n++] = a;
    }
    if (lp_hover >= lp_n) lp_hover = -1;
}

void desktop_toggle_launcher(void) {
    lp_open = !lp_open;
    lp_q[0] = 0;
    lp_hover = -1;
    lp_t0 = 1;
    lp_animating = true;
    cc_open = false;
    if (spot_open) desktop_toggle_spotlight();
    desktop_close_menus();
    lp_filter();
    wm_damage_all();
}

typedef struct { int isz, cw, ch, cols, x0, y0; } lp_grid_t;
static rect_t lp_search_rect(void) { int w = MIN(320, SW - 48); return rect_make((SW - w) / 2, MENUBAR_H + 28, w, 34); }
static lp_grid_t lp_grid(void) {
    lp_grid_t g;
    /* responsivo: ícones e colunas se adaptam à resolução */
    g.isz = CLAMP(SW / 15, 56, 108);
    g.cw = g.isz * 7 / 4;
    g.ch = g.isz + 46;
    int dock_h = dock_base + 40;
    g.cols = CLAMP((SW - 80) / g.cw, 3, 7);
    int rows_fit = MAX(1, (SH - (lp_search_rect().y + 80) - dock_h) / g.ch);
    while (g.cols < 9 && (lp_n + g.cols - 1) / g.cols > rows_fit && (g.cols + 1) * g.cw < SW - 40) g.cols++;
    int rows = MAX(1, (lp_n + g.cols - 1) / g.cols);
    g.x0 = (SW - g.cols * g.cw) / 2;
    int avail = SH - (lp_search_rect().y + 50) - dock_h;
    g.y0 = lp_search_rect().y + 50 + MAX(0, (avail - rows * g.ch) / 3);
    return g;
}
static rect_t lp_cell(lp_grid_t *g, int i) { return rect_make(g->x0 + (i % g->cols) * g->cw, g->y0 + (i / g->cols) * g->ch, g->cw, g->ch); }

static void draw_launcher(surface_t *s) {
    if (!lp_open) return;
    /* animação por quadro (não por tempo): mesmo numa máquina lenta todas as etapas são desenhadas */
    static float lp_anim;
    if (lp_t0) { lp_anim = 0; lp_t0 = 0; }
    lp_anim = MIN(1.0f, lp_anim + 0.25f);
    lp_animating = lp_anim < 1.0f;
    float k = 1 - (1 - lp_anim) * (1 - lp_anim);
    gfx_blend(s, wallpaper_blur, 0, 0, (int)(255 * k));
    gfx_fill(s, 0, 0, SW, SH, RGBA(0, 0, 0, (int)((T.dark ? 70 : 34) * k)));
    rect_t sr = lp_search_rect();
    glass_capsule_fill(s, sr.x, sr.y, sr.w, sr.h, sr.h / 2.0f, RGBA(255, 255, 255, T.dark ? 36 : 70), 180);
    glyph_search(s, sr.x + 14, sr.y + 10, 14, RGBA(255, 255, 255, 200));
    font_t *fr = font_get(FONT_REGULAR), *fm = font_get(FONT_MEDIUM);
    int tw = text_draw(s, fr, 15, sr.x + 38, sr.y + 23, RGB(255, 255, 255), lp_q);
    if (!lp_q[0]) text_draw(s, fr, 15, sr.x + 38, sr.y + 23, RGBA(255, 255, 255, 150), "Buscar Apps");
    else if ((timer_ticks() / 500) % 2 == 0) gfx_fill(s, sr.x + 39 + tw, sr.y + 9, 2, 17, RGB(255, 255, 255));
    lp_grid_t g = lp_grid();
    /* entrada: grade escala de 92% para 100% */
    float sc = 0.92f + 0.08f * k;
    for (int i = 0; i < lp_n; i++) {
        rect_t c = lp_cell(&g, i);
        int isz = (int)(g.isz * sc);
        int ix = c.x + (c.w - isz) / 2, iy = c.y + (int)((g.isz - isz) / 2) + 4;
        if (i == lp_hover) glass_capsule_fill(s, c.x + 6, c.y - 4, c.w - 12, c.h, 20, RGBA(255, 255, 255, 30), 120);
        gfx_shadow(s, ix + isz / 10, iy + isz / 10, isz - isz / 5, isz - isz / 5, isz / 5.0f, isz / 6.0f, 70, isz / 14);
        gfx_blit_scaled(s, icon_for(lp_apps[i], g.isz), ix, iy, isz, isz, (int)(255 * k));
        const char *n = lp_apps[i]->name;
        int nw = MIN(text_width(fm, 13, n), c.w - 8);
        text_draw_ellipsis(s, fm, 13, c.x + (c.w - nw) / 2, c.y + g.isz + 27, c.w - 8, RGBA(0, 0, 0, 120), n);
        text_draw_ellipsis(s, fm, 13, c.x + (c.w - nw) / 2, c.y + g.isz + 26, c.w - 8, RGB(255, 255, 255), n);
    }
    if (!lp_n) text_draw_center(s, fm, 16, rect_make(0, SH / 2 - 20, SW, 40), RGBA(255, 255, 255, 180), "Nenhum resultado");
}

static bool lp_key(input_event_t *e) {
    int len = (int)strlen(lp_q);
    if (e->key == KEY_ESC) desktop_toggle_launcher();
    else if (e->key == KEY_ENTER) { if (lp_n) { app_t *a = lp_apps[lp_hover >= 0 ? lp_hover : 0]; desktop_toggle_launcher(); app_launch(a); } }
    else if (e->key == KEY_BACKSPACE) { if (len) { lp_q[utf8_prev(lp_q, len)] = 0; lp_filter(); } }
    else if (e->key == KEY_RIGHT) lp_hover = MIN(lp_hover + 1, lp_n - 1);
    else if (e->key == KEY_LEFT) lp_hover = MAX(lp_hover - 1, 0);
    else if (e->key >= 32 && e->key < 0x100 && !(e->mods & (MOD_CMD | MOD_CTRL)) && len < 56) { len += utf8_encode((u32)e->key, lp_q + len); lp_q[len] = 0; lp_filter(); }
    wm_damage_all();
    return true;
}

static bool lp_mouse(int x, int y, bool lp, bool lr) {
    if (!lp_open) return false;
    if (y < MENUBAR_H || rect_contains(rect_make(dock_r.x - 10, dock_r.y - 60, dock_r.w + 20, dock_r.h + 70), x, y)) return false;
    lp_grid_t g = lp_grid();
    int h = -1;
    for (int i = 0; i < lp_n; i++) if (rect_contains(lp_cell(&g, i), x, y)) h = i;
    if (h != lp_hover) { lp_hover = h; wm_damage_all(); }
    if (lr && h >= 0) { app_t *a = lp_apps[h]; desktop_toggle_launcher(); a->bounce = 1; app_launch(a); }
    else if (lp && h < 0 && !rect_contains(lp_search_rect(), x, y)) desktop_toggle_launcher();
    return true;
}

/* ============================================================ menu do botão verde (organizar janela)
 * Parar o mouse sobre o botão verde abre um painel de vidro com as posições: metades, quartos,
 * preencher e centralizar. Cada opção é um mini "monitor" com a área ocupada preenchida. */
static struct { bool open; window_t *win; int bx, by, hover, x, y; } TP;
#define TP_W 236
#define TP_PAD 12
#define TP_CELL_H 34
static const int tp_zones[10] = {1, 2, 4, 5, 6, 7, 8, 9, 3, 10};
static const char *tp_names[10] = {"Esquerda", "Direita", "Em Cima", "Embaixo", "Superior Esquerdo", "Superior Direito",
                                   "Inferior Esquerdo", "Inferior Direito", "Preencher", "Centralizar"};

static int tp_height(void) { return 10 + 18 + 2 * TP_CELL_H + 4 + 22 + TP_CELL_H + 11 + 24 + (TP.win && TP.win->zoomed ? 24 : 0) + 8; }
static rect_t tp_rect(void) { return rect_make(TP.x, TP.y, TP_W, tp_height()); }

/* opção i (0..9): duas linhas de quatro em "Mover e Redimensionar", depois duas em "Preencher e Organizar" */
static rect_t tp_cell(int i) {
    int cw = (TP_W - 2 * TP_PAD) / 4;
    int y0 = TP.y + 10 + 18;
    if (i < 8) return rect_make(TP.x + TP_PAD + (i % 4) * cw, y0 + (i / 4) * TP_CELL_H, cw, TP_CELL_H);
    return rect_make(TP.x + TP_PAD + (i - 8) * cw, y0 + 2 * TP_CELL_H + 4 + 22, cw, TP_CELL_H);
}
/* linhas de texto no fim: 0 = Entrar em Tela Cheia, 1 = Voltar ao Tamanho Anterior (se encaixada) */
static rect_t tp_text_row(int k) {
    rect_t c = tp_cell(8);
    return rect_make(TP.x + 6, c.y + c.h + 11 + k * 24, TP_W - 12, 24);
}

static void tile_glyph(surface_t *s, float cx, float cy, int zone, u32 c) {
    float w = 24, h = 17, x = cx - w / 2, y = cy - h / 2;
    gfx_round_rect_stroke(s, x, y, w, h, 3.5f, 1.3f, c);
    float ix = x + 2.5f, iy = y + 2.5f, iw = w - 5, ih = h - 5, hw = (iw - 1) / 2, hh = (ih - 1) / 2;
    switch (zone) {
    case 1: gfx_round_rect(s, ix, iy, hw, ih, 1.5f, c); break;
    case 2: gfx_round_rect(s, ix + iw - hw, iy, hw, ih, 1.5f, c); break;
    case 4: gfx_round_rect(s, ix, iy, iw, hh, 1.5f, c); break;
    case 5: gfx_round_rect(s, ix, iy + ih - hh, iw, hh, 1.5f, c); break;
    case 6: gfx_round_rect(s, ix, iy, hw, hh, 1.5f, c); break;
    case 7: gfx_round_rect(s, ix + iw - hw, iy, hw, hh, 1.5f, c); break;
    case 8: gfx_round_rect(s, ix, iy + ih - hh, hw, hh, 1.5f, c); break;
    case 9: gfx_round_rect(s, ix + iw - hw, iy + ih - hh, hw, hh, 1.5f, c); break;
    case 3: gfx_round_rect(s, ix, iy, iw, ih, 1.5f, c); break;
    default: gfx_round_rect(s, ix + iw * 0.2f, iy + ih * 0.15f, iw * 0.6f, ih * 0.7f, 1.5f, c); break;
    }
}

void desktop_tile_popover(window_t *w, int bx, int by) {
    if (!w || (TP.open && TP.win == w)) return;
    bx -= PX(); by -= PY();                      /* o window server passa coordenadas globais */
    desktop_close_menus();
    TP.open = true;
    TP.win = w;
    TP.bx = bx; TP.by = by;
    TP.hover = -1;
    TP.x = CLAMP(bx - 22, 6, SW - TP_W - 6);
    TP.y = by + 14;
    if (TP.y + tp_height() > SH - 6) TP.y = by - 14 - tp_height();
    wm_damage(rect_inset(tp_rect(), -30));
}
bool desktop_tile_popover_open(void) { return TP.open; }

static void tile_popover_close(void) {
    if (!TP.open) return;
    wm_damage(rect_inset(tp_rect(), -30));
    TP.open = false;
    TP.win = NULL;
}

static void draw_tile_popover(surface_t *s) {
    if (!TP.open) return;
    rect_t r = tp_rect();
    glass_panel(s, r.x, r.y, r.w, r.h, 14, GLASS_REGULAR);
    font_t *fb = font_get(FONT_SEMIBOLD), *fr = font_get(FONT_REGULAR);
    u32 hdr = T.glass_text2, gl = T.glass_text;
    text_draw(s, fb, 11.5f, r.x + TP_PAD + 2, r.y + 10 + 13, hdr, "Mover e Redimensionar");
    rect_t c8 = tp_cell(8);
    text_draw(s, fb, 11.5f, r.x + TP_PAD + 2, c8.y - 22 + 15, hdr, "Preencher e Organizar");
    for (int i = 0; i < 10; i++) {
        rect_t c = tp_cell(i);
        bool h = TP.hover == i;
        if (h) gfx_round_rect(s, c.x + 3, c.y + 2, c.w - 6, c.h - 4, 8, T.accent);
        tile_glyph(s, c.x + c.w / 2.0f, c.y + c.h / 2.0f, tp_zones[i], h ? RGB(255, 255, 255) : gl);
    }
    {
        rect_t r0 = tp_text_row(0);
        gfx_fill(s, r.x + TP_PAD, r0.y - 6, r.w - 2 * TP_PAD, 1, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 22));
        int nrow = TP.win && TP.win->zoomed ? 2 : 1;
        for (int k = 0; k < nrow; k++) {
            rect_t rr = tp_text_row(k);
            bool h = TP.hover == 10 + k;
            if (h) gfx_round_rect(s, rr.x, rr.y, rr.w, rr.h, 7, T.accent);
            const char *t = k == 0 ? (TP.win && TP.win->fullscreen ? "Sair da Tela Cheia" : "Entrar em Tela Cheia") : "Voltar ao Tamanho Anterior";
            text_draw(s, fr, 13, rr.x + 10, rr.y + 17, h ? RGB(255, 255, 255) : gl, t);
        }
    }
    /* nome da opção sob o mouse, abaixo do painel (como a dica do sistema) */
    if (TP.hover >= 0 && TP.hover < 10) {
        const char *t = tp_names[TP.hover];
        int tw = text_width(fr, 12, t) + 20, th = 22;
        rect_t c = tp_cell(TP.hover);
        int tx = CLAMP(c.x + c.w / 2 - tw / 2, 4, SW - tw - 4), ty = r.y + r.h + 6;
        gfx_round_rect(s, tx, ty, tw, th, 6, T.dark ? RGBA(40, 40, 44, 240) : RGBA(250, 250, 252, 245));
        gfx_round_rect_stroke(s, tx + 0.5f, ty + 0.5f, tw - 1, th - 1, 5.5f, 1, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 30));
        text_draw_center(s, fr, 12, rect_make(tx, ty, tw, th), T.text, t);
    }
}

static bool tile_popover_mouse(int x, int y, bool lp, bool lr) {
    if (!TP.open) return false;
    rect_t r = tp_rect();
    /* área que mantém o painel: ele, o botão verde e a ponte entre os dois */
    rect_t keep = rect_union(rect_inset(r, -10), rect_make(TP.bx - 12, MIN(TP.by, r.y) - 12, 24, ABS_DIFF(TP.by, r.y) + 24));
    if (!rect_contains(keep, x, y) || (TP.win && TP.win != wm_focused())) { tile_popover_close(); return false; }
    if (!rect_contains(r, x, y)) {
        if (TP.hover != -1) { TP.hover = -1; wm_damage(rect_inset(r, -40)); }
        if (lp) { tile_popover_close(); }
        return false;
    }
    int h = -1;
    for (int i = 0; i < 10; i++) if (rect_contains(tp_cell(i), x, y)) h = i;
    if (rect_contains(tp_text_row(0), x, y)) h = 10;
    if (TP.win && TP.win->zoomed && rect_contains(tp_text_row(1), x, y)) h = 11;
    if (h != TP.hover) { TP.hover = h; wm_damage(rect_inset(r, -40)); }
    if (lr && h >= 0) {
        window_t *w = TP.win;
        tile_popover_close();
        if (h == 10) wm_fullscreen(w); else if (h == 11) wm_zoom(w); else wm_tile(w, tp_zones[h]);
    }
    return true;
}

/* ============================================================ Captura de Tela (⌘⇧5 / ⌘⇧4)
 * Barra de vidro embaixo: fechar · tela inteira · janela · seleção | Capturar. Na janela, passar o mouse
 * destaca a janela e clicar captura; na seleção, arrastar desenha o retângulo. Return captura, Esc fecha. */
static struct { bool open; int mode, hover; rect_t sel; bool dragging; int ax, ay; bool from_shortcut4; } SC;
#define SC_W 380
#define SC_H 52

static rect_t sc_bar(void) { return rect_make((SW - SC_W) / 2, SH - 150, SC_W, SC_H); }
static rect_t sc_btn(int i) {   /* 0 fechar, 1..3 modos, 4 Capturar */
    rect_t b = sc_bar();
    if (i == 0) return rect_make(b.x + 10, b.y + 10, 32, 32);
    if (i <= 3) return rect_make(b.x + 54 + (i - 1) * 46, b.y + 8, 40, 36);
    return rect_make(b.x + b.w - 110, b.y + 10, 100, 32);
}
static void sc_open(int mode) {
    desktop_close_menus();
    SC.open = true;
    SC.mode = mode;
    SC.hover = -1;
    SC.dragging = false;
    SC.from_shortcut4 = false;
    if (SC.sel.w <= 0) SC.sel = rect_make(SW / 2 - 300, SH / 2 - 200, 600, 400);
    wm_damage_all();
}
static void sc_glyph(surface_t *s, int mode, int cx, int cy, u32 c) {
    if (mode == 1) { gfx_round_rect_stroke(s, cx - 11, cy - 8, 22, 16, 2.5f, 1.4f, c); gfx_fill(s, cx - 4, cy + 10, 8, 1, c); }
    else if (mode == 2) { gfx_round_rect_stroke(s, cx - 10, cy - 7, 20, 14, 3, 1.4f, c); gfx_fill(s, cx - 10, cy - 3, 20, 1, c); }
    else {
        /* retângulo tracejado */
        for (int x = -10; x < 10; x += 4) { gfx_fill(s, cx + x, cy - 7, 2, 1, c); gfx_fill(s, cx + x, cy + 7, 2, 1, c); }
        for (int y = -7; y < 7; y += 4) { gfx_fill(s, cx - 10, cy + y, 1, 2, c); gfx_fill(s, cx + 10, cy + y, 1, 2, c); }
    }
}
static void draw_screencap(surface_t *s) {
    if (!SC.open) return;
    if (SC.mode == 3) {
        /* fora da seleção escurece; a seleção tem borda tracejada e as medidas */
        rect_t r = SC.sel;
        gfx_fill(s, 0, 0, SW, r.y, RGBA(0, 0, 0, 90));
        gfx_fill(s, 0, r.y + r.h, SW, SH - r.y - r.h, RGBA(0, 0, 0, 90));
        gfx_fill(s, 0, r.y, r.x, r.h, RGBA(0, 0, 0, 90));
        gfx_fill(s, r.x + r.w, r.y, SW - r.x - r.w, r.h, RGBA(0, 0, 0, 90));
        for (int x = r.x; x < r.x + r.w; x += 8) { gfx_fill(s, x, r.y, 4, 1, RGB(255, 255, 255)); gfx_fill(s, x, r.y + r.h - 1, 4, 1, RGB(255, 255, 255)); }
        for (int y = r.y; y < r.y + r.h; y += 8) { gfx_fill(s, r.x, y, 1, 4, RGB(255, 255, 255)); gfx_fill(s, r.x + r.w - 1, y, 1, 4, RGB(255, 255, 255)); }
        char dim[32];
        snprintf(dim, sizeof dim, "%d × %d", r.w, r.h);
        font_t *f = font_get(FONT_MEDIUM);
        int tw = text_width(f, 12, dim) + 16;
        gfx_round_rect(s, r.x + r.w / 2 - tw / 2, r.y + r.h + 8, tw, 22, 6, RGBA(0, 0, 0, 150));
        text_draw_center(s, f, 12, rect_make(r.x + r.w / 2 - tw / 2, r.y + r.h + 8, tw, 22), RGB(255, 255, 255), dim);
    } else if (SC.mode == 2) {
        extern window_t *wm_window_at_point(int x, int y);
        window_t *w = wm_window_at_point(wm_mouse_x(), wm_mouse_y());
        if (w && !rect_contains(sc_bar(), wm_mouse_x(), wm_mouse_y())) gfx_round_rect(s, w->x, w->y, w->w, w->h, WIN_RADIUS, WITH_ALPHA(T.accent, 70));
    } else {
        gfx_fill(s, 0, 0, SW, SH, RGBA(255, 255, 255, 18));
    }
    rect_t b = sc_bar();
    glass_panel(s, b.x, b.y, b.w, b.h, b.h / 2.0f, GLASS_REGULAR);
    u32 tc = T.glass_text;
    rect_t xr = sc_btn(0);
    if (SC.hover == 0) gfx_circle(s, xr.x + 16, xr.y + 16, 15, T.dark ? RGBA(255, 255, 255, 30) : RGBA(0, 0, 0, 16));
    gfx_line(s, xr.x + 11, xr.y + 11, xr.x + 21, xr.y + 21, 1.6f, tc);
    gfx_line(s, xr.x + 21, xr.y + 11, xr.x + 11, xr.y + 21, 1.6f, tc);
    for (int i = 1; i <= 3; i++) {
        rect_t r = sc_btn(i);
        bool on = SC.mode == i;
        if (on) gfx_round_rect(s, r.x, r.y, r.w, r.h, 10, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 22));
        else if (SC.hover == i) gfx_round_rect(s, r.x, r.y, r.w, r.h, 10, T.dark ? RGBA(255, 255, 255, 20) : RGBA(0, 0, 0, 10));
        sc_glyph(s, i, r.x + r.w / 2, r.y + r.h / 2, tc);
    }
    gfx_fill(s, b.x + 54 + 3 * 46 + 4, b.y + 12, 1, b.h - 24, T.dark ? RGBA(255, 255, 255, 40) : RGBA(0, 0, 0, 30));
    rect_t cb = sc_btn(4);
    gfx_round_rect(s, cb.x, cb.y, cb.w, cb.h, cb.h / 2.0f, SC.hover == 4 ? color_lighten(T.accent, 15) : T.accent);
    text_draw_center(s, font_get(FONT_SEMIBOLD), 13, cb, RGB(255, 255, 255), "Capturar");
}
static void sc_capture(rect_t r) {
    SC.open = false;
    wm_damage_all();
    extern void wm_compose(void);
    wm_compose();                                   /* tela sem a barra antes de copiar */
    extern bool screenshot_save_rect(char *, usize, rect_t);
    char path[192];
    if (screenshot_save_rect(path, sizeof path, r)) {
        gfx_fill(g_screen, r.x, r.y, r.w, r.h, RGBA(255, 255, 255, 150));
        fb_flush(r);
        sleep_ms(90);
        const char *nm = strrchr(path, '/');
        notify("Captura de Tela", nm ? nm + 1 : path);
    } else notify("Não foi possível salvar a captura", "Verifique o espaço em Imagens.");
    wm_damage_all();
}
static bool sc_mouse(int x, int y, bool lp, bool lr, int buttons) {
    if (!SC.open) return false;
    int h = -1;
    for (int i = 0; i <= 4; i++) if (rect_contains(sc_btn(i), x, y)) h = i;
    if (h != SC.hover) SC.hover = h;
    wm_damage_all();
    if (lp && h >= 0) {
        if (h == 0) { SC.open = false; }
        else if (h <= 3) SC.mode = h;
        else if (SC.mode == 3) sc_capture(SC.sel);
        else if (SC.mode == 1) sc_capture(rect_make(0, 0, SW, SH));
        return true;
    }
    if (rect_contains(sc_bar(), x, y)) return true;
    if (SC.mode == 1 && lp) { sc_capture(rect_make(0, 0, SW, SH)); return true; }
    if (SC.mode == 2 && lp) {
        extern window_t *wm_window_at_point(int x, int y);
        window_t *w = wm_window_at_point(x, y);
        if (w) sc_capture(rect_make(w->x, w->y, w->w, w->h));
        return true;
    }
    if (SC.mode == 3) {
        if (lp) { SC.dragging = true; SC.ax = x; SC.ay = y; }
        if (SC.dragging && (buttons & 1)) SC.sel = rect_make(MIN(x, SC.ax), MIN(y, SC.ay), MAX(1, ABS_DIFF(x, SC.ax)), MAX(1, ABS_DIFF(y, SC.ay)));
        if (lr && SC.dragging) {
            SC.dragging = false;
            if (SC.sel.w > 4 && SC.sel.h > 4 && SC.from_shortcut4) sc_capture(SC.sel);   /* ⌘⇧4: soltar já captura */
        }
    }
    return true;
}

/* regiões de vidro que leem o fundo da tela (para o compositor recompor por inteiro) */
static int desktop_glass_rects_local(rect_t *out, int max);
int desktop_glass_rects(rect_t *out, int max) {
    int n = desktop_glass_rects_local(out, max);
    for (int i = 0; i < n; i++) out[i] = to_global(out[i]);
    return n;
}
static int desktop_glass_rects_local(rect_t *out, int max) {
    int n = 0;
#define ADD(rr, kind) do { if (n < max && !rect_empty(rr)) { int m_ = GLASS_REACH(&G[kind]); out[n++] = rect_inset((rr), -m_); } } while (0)
    if (SW <= 0) return 0;
    dock_layout();
    if (dock_hide < 0.999f) ADD(dock_r, GLASS_CLEAR);
    if (TP.open) ADD(tp_rect(), GLASS_REGULAR);
    if (SC.open) ADD(sc_bar(), GLASS_REGULAR);
    if (g_desktop_widgets) { ADD(widget_rect(0), GLASS_WIDGET); ADD(widget_rect(1), GLASS_WIDGET); }
    ADD(dock_tip_r, GLASS_REGULAR);
    if (M.kind != MK_NONE) ADD(rect_make(M.x, M.y, M.w, M.h), GLASS_REGULAR);
    if (M.kind != MK_NONE && SM.open) ADD(sm_rect(), GLASS_REGULAR);
    if (cc_open) ADD(cc_rect(), GLASS_REGULAR);
    if (spot_open) ADD(spot_rect(), GLASS_THICK);
    if (A.open) ADD(alert_layout(), GLASS_THICK);
    if (SD.open) ADD(sd_layout().r, GLASS_THICK);
    if (AS.open) ADD(app_switcher_rect(), GLASS_THICK);
    if (NC.open) ADD(rect_make(SW - NC_W - 30, MENUBAR_H, NC_W + 30, SH - MENUBAR_H), GLASS_CONTROL);
    u64 now = timer_ticks();
    for (int i = 0; i < nnotif; i++)
        if (now - notifs[i].t0 < 5600) { ADD(rect_make(SW - 380, MENUBAR_H + 10, 380, 90), GLASS_REGULAR); break; }
#undef ADD
    return n;
}

/* ============================================================ API de desenho */
static void desktop_draw_overlay_main(surface_t *s, rect_t D);
void desktop_draw_background(surface_t *s, rect_t r) {
    /* o papel de parede tem o tamanho do monitor principal; cada monitor o mostra a partir do seu canto */
    for (int i = 0; i < display_count(); i++) {
        rect_t d = display_rect(i), k = rect_intersect(r, d);
        if (rect_empty(k)) continue;
        if (d.w == wallpaper->w && d.h == wallpaper->h) {
            gfx_blit_rect(s, wallpaper, rect_make(k.x - d.x, k.y - d.y, k.w, k.h), k.x, k.y);
        } else {                                       /* monitor de outro tamanho: o papel de parede preenche */
            rect_t pc = surface_push_clip(s, k);
            gfx_blit_scaled(s, wallpaper, d.x, d.y, d.w, d.h, 255);
            surface_pop_clip(s, pc);
        }
    }
    /* ícones da mesa: no monitor principal, em coordenadas dele */
    s->ox += PX(); s->oy += PY();
    draw_desktop_widgets(s, rect_make(r.x - PX(), r.y - PY(), r.w, r.h));
    draw_desktop_icons(s, rect_make(r.x - PX(), r.y - PY(), r.w, r.h));
    s->ox -= PX(); s->oy -= PY();
}

void desktop_draw_overlay(surface_t *s, rect_t D) {
    /* tudo daqui vive no monitor principal: o que desliza para fora dele (notificações, painéis)
     * não pode aparecer no monitor vizinho */
    rect_t prev_clip = surface_push_clip(s, display_rect(0));
    s->ox += PX(); s->oy += PY();
    desktop_draw_overlay_main(s, rect_make(D.x - PX(), D.y - PY(), D.w, D.h));
    s->ox -= PX(); s->oy -= PY();
    surface_pop_clip(s, prev_clip);
}

static void desktop_draw_overlay_main(surface_t *s, rect_t D) {
    draw_launcher(s);
    if (DD.active || rect_intersect(D, rect_make(0, SH - g_dock_size * 2 - 90, SW, g_dock_size * 2 + 90)).w) draw_dock(s);
    draw_tile_popover(s);
    if (D.y < MENUBAR_H && bar_visible()) {
        /* tela cheia: a barra aparece por cima do app, opaca */
        if (wm_space_fullscreen()) gfx_fill(s, 0, 0, SW, MENUBAR_H, T.dark ? RGB(30, 30, 32) : RGB(236, 236, 238));
        draw_menubar(s);
    }
    draw_notifications(s);
    draw_control_center(s);
    draw_notification_center(s);
    if (SD.open) gfx_fill(s, 0, 0, SW, SH, RGBA(0, 0, 0, T.dark ? 50 : 24));
    draw_save_dialog(s);
    draw_menu(s);
    draw_app_switcher(s);
    draw_spotlight(s);
    draw_screencap(s);
    login_draw(s);
    setup_draw(s);
    draw_alert(s);
    tcc_prompt_draw(s, SW, SH);
}

/* ============================================================ eventos */
/* atalhos dos menus dos apps ("⇧⌘N", "⌘1", "⌘⌫"…): a tecla com os modificadores aciona o item */
static bool shortcut_matches(const char *sc, u16 mods, i32 key) {
    u16 need = 0;
    const char *p = sc;
    i32 k = 0;
    while (*p) {
        u32 c = utf8_decode(&p);
        if (c == 0x2303) need |= MOD_CTRL;            /* ⌃ */
        else if (c == 0x2325) need |= MOD_ALT;        /* ⌥ */
        else if (c == 0x21E7) need |= MOD_SHIFT;      /* ⇧ */
        else if (c == 0x2318) need |= MOD_CMD;        /* ⌘ */
        else if (c == 0x232B) k = KEY_BACKSPACE;      /* ⌫ */
        else if (c == 0x21E5) k = KEY_TAB;            /* ⇥ */
        else if (c == 0x2190) k = KEY_LEFT;
        else if (c == 0x2192) k = KEY_RIGHT;
        else if (c == 0x2191) k = KEY_UP;
        else if (c == 0x2193) k = KEY_DOWN;
        else if (c == 0x238B) k = KEY_ESC;            /* ⎋ */
        else if (c != ' ') k = (i32)c;
    }
    if (!k || !(need & MOD_CMD)) return false;
    u16 have = mods & (MOD_CTRL | MOD_ALT | MOD_SHIFT | MOD_CMD);
    if (have != need) {
        /* ⇧ pode já estar "embutido" no caractere (ex.: "?" ou letra maiúscula) */
        if ((have & ~MOD_SHIFT) != need) return false;
    }
    if (k < 128 && key < 128 && key > 0) return tolower(k) == tolower(key);
    return k == key;
}

static bool dispatch_menu_shortcut(app_t *a, input_event_t *e) {
    if (!a || !a->on_menu) return false;
    for (int mi = 0; mi < a->nmenus; mi++) {
        menu_t *m = &a->menus[mi];
        for (int i = 0; i < m->count; i++) {
            menu_item_t *it = &m->items[i];
            if (!it->label || !it->shortcut || it->disabled || it->action <= 0) continue;
            if (shortcut_matches(it->shortcut, e->mods, e->key)) { a->on_menu(a, it->action); return true; }
        }
    }
    return false;
}

static int last_buttons;
/* o window server trata alguns eventos sem passar por aqui (arrasto de janela, Mission Control):
 * ele avisa o estado dos botões para o próximo clique não ser confundido com "ainda pressionado" */
void desktop_sync_buttons(int buttons) { last_buttons = buttons; }

static bool desktop_handle_event_local(input_event_t *e);
bool desktop_handle_event(input_event_t *e) {
    if (e->type != EV_MOUSE) return desktop_handle_event_local(e);
    input_event_t l = *e;                        /* o window server manda a posição global do mouse */
    l.dx -= PX(); l.dy -= PY();
    return desktop_handle_event_local(&l);
}
static bool desktop_handle_event_local(input_event_t *e) {
    if (setup_active()) return setup_handle_event(e);            /* assistente de configuração/instalação (tela cheia) */
    if (login_active() && !A.open) return login_handle_event(e);
    if (e->type == EV_KEY_DOWN) {
        u16 m = e->mods;
        if (tcc_prompt_key(e->key)) return true;
        if (A.open) {
            if (e->key == KEY_ENTER) alert_finish(A.nb - 1);
            else if (e->key == KEY_ESC) alert_finish(0);
            return true;
        }
        if (AS.open && e->key == KEY_ESC) { app_switcher_finish(false); return true; }
        if (SC.open) {
            if (e->key == KEY_ESC) { SC.open = false; wm_damage_all(); }
            else if (e->key == KEY_ENTER) {
                window_t *f = wm_focused();
                if (SC.mode == 3) sc_capture(SC.sel);
                else if (SC.mode == 2 && f) sc_capture(rect_make(f->x, f->y, f->w, f->h));
                else sc_capture(rect_make(0, 0, SW, SH));
            }
            return true;
        }
        if ((m & MOD_CMD) && (m & MOD_SHIFT) && (e->key == '5' || e->key == '%')) { sc_open(3); return true; }
        if ((m & MOD_CMD) && (m & MOD_SHIFT) && (e->key == '4' || e->key == '$')) { sc_open(3); SC.from_shortcut4 = true; return true; }
        if (NC.open && !NC.closing && e->key == KEY_ESC) { nc_toggle(); return true; }
        if (SD.open && M.kind == MK_NONE) return sd_key(e);
        if ((m & MOD_CMD) && e->key == ' ') { desktop_toggle_spotlight(); return true; }
        /* ⌃Espaço troca a fonte de entrada (teclado → japonês → chinês → coreano), como no macOS */
        if ((m & MOD_CTRL) && e->key == ' ') { extern void compat_ime_cycle(int dir); compat_ime_cycle((m & MOD_ALT) ? -1 : 1); return true; }
        /* Launchpad: F4 (como no macOS) e ⌘L. Num app do macOS o ⌘L é do app (Chrome/Safari: barra de endereço). */
        {
            app_t *ka = active_app();
            extern const char *compat_app_bundle(struct app *a);
            bool mac_app = ka && compat_app_bundle(ka) && !lp_open;
            if (e->key == KEY_F4 || ((m & MOD_CMD) && (e->key == 'l' || e->key == 'L') && !mac_app)) { desktop_toggle_launcher(); return true; }
        }
        if (lp_open) return lp_key(e);
        if (e->key == KEY_F3) { desktop_toggle_control_center(); return true; }
        if (cc_open && e->key == KEY_ESC) { desktop_toggle_control_center(); return true; }
        if (spot_open) {
            int len = (int)strlen(spot_q);
            if (e->key == KEY_ESC) desktop_toggle_spotlight();
            else if (e->key == KEY_ENTER) spot_activate(spot_sel);
            else if (e->key == KEY_DOWN) spot_sel = MIN(spot_sel + 1, spot_n - 1);
            else if (e->key == KEY_UP) spot_sel = MAX(spot_sel - 1, 0);
            else if (e->key == KEY_BACKSPACE) { if (len) { spot_q[utf8_prev(spot_q, len)] = 0; spot_update(); } }
            else if (e->key >= 32 && e->key < 0x100 && len < 90) { len += utf8_encode((u32)e->key, spot_q + len); spot_q[len] = 0; spot_update(); }
            wm_damage_all();
            return true;
        }
        if (M.kind != MK_NONE) {
            /* teclado dentro do submenu */
            if (SM.open && SM.hover >= 0) {
                if (e->key == KEY_LEFT || e->key == KEY_ESC) { M.hover = SM.parent; submenu_close(); return true; }
                if (e->key == KEY_DOWN || e->key == KEY_UP) {
                    int d = e->key == KEY_DOWN ? 1 : -1;
                    for (int k = 0; k < SM.n; k++) {
                        SM.hover = (SM.hover + d + SM.n) % SM.n;
                        menu_item_t *si = &SM.items[SM.hover];
                        if (si->label && !si->disabled && !si->header) break;
                    }
                    wm_damage_all();
                    return true;
                }
                if (e->key == KEY_ENTER) {
                    int act = SM.items[SM.hover].action, kind = M.kind, pid = M.popup_id;
                    window_t *pw = M.popup_win;
                    desktop_close_menus();
                    if (kind == MK_POPUP) { popup_res.win = pw; popup_res.id = pid; popup_res.val = act; popup_res.ready = true; wm_invalidate(pw); }
                    else run_action(act);
                    return true;
                }
                return true;
            }
            if (e->key == KEY_ESC) { desktop_close_menus(); return true; }
            if (e->key == KEY_DOWN || e->key == KEY_UP) {
                int d = e->key == KEY_DOWN ? 1 : -1;
                for (int k = 0; k < M.n; k++) {
                    M.hover = (M.hover + d + M.n) % M.n;
                    if (M.items[M.hover].label && !M.items[M.hover].disabled && !M.items[M.hover].header) break;
                }
                submenu_close();
                wm_damage_all();
                return true;
            }
            if (e->key == KEY_RIGHT && M.hover >= 0 && M.items[M.hover].submenu) {
                submenu_open(M.hover);
                for (int k = 0; k < SM.n; k++) if (SM.items[k].label && !SM.items[k].disabled && !SM.items[k].header) { SM.hover = k; M.hover = -1; break; }
                return true;
            }
            if (e->key == KEY_ENTER && M.hover >= 0 && M.items[M.hover].submenu) {
                submenu_open(M.hover);
                for (int k = 0; k < SM.n; k++) if (SM.items[k].label && !SM.items[k].disabled && !SM.items[k].header) { SM.hover = k; M.hover = -1; break; }
                return true;
            }
            if (e->key == KEY_ENTER && M.hover >= 0) {
                int act = M.items[M.hover].action;
                int kind = M.kind;
                desktop_close_menus();
                if (kind == MK_POPUP) { popup_res = (typeof(popup_res)){M.popup_win, M.popup_id, act, true}; wm_invalidate(M.popup_win); }
                else run_action(act);
                return true;
            }
        }
        /* ⌘⇧3: captura de tela (salva em Imagens) com clarão branco e notificação */
        if ((m & MOD_CMD) && (m & MOD_SHIFT) && (e->key == '3' || e->key == '#')) {
            extern bool screenshot_save(char *, usize);
            char path[192];
            if (screenshot_save(path, sizeof path)) {
                gfx_fill(g_screen, PX(), PY(), SW, SH, RGBA(255, 255, 255, 150));
                fb_flush(rect_make(0, 0, SW, SH));
                sleep_ms(90);
                const char *nm = strrchr(path, '/');
                notify("Captura de Tela", nm ? nm + 1 : path);
            } else {
                notify("Não foi possível salvar a captura", "Verifique o espaço em Imagens.");
            }
            wm_damage_all();
            return true;
        }
        if (m & MOD_CMD) {
            int k = tolower(e->key);
            window_t *f = wm_focused();
            app_t *a = active_app();
            if (k == 'q' && (m & (MOD_SHIFT | MOD_CTRL))) { run_action(ACT_LOCK); return true; }   /* ⇧⌘Q / ⌃⌘Q */
            if (k == 'q') { if (a && strcmp(a->exec, "arquivos")) app_quit(a); return true; }
            /* ⌘W: se o menu do app tem o atalho (o Chrome: "Fechar guia"), ele vale, como no macOS; senão fecha a janela */
            if (k == 'w') { if (dispatch_menu_shortcut(a, e)) { wm_damage_all(); return true; } if (f) wm_close(f); return true; }
            if (k == 'm') { if (f) wm_minimize(f); return true; }
            if (k == 'h') { run_action(ACT_APP_HIDE); return true; }
            if (e->key == KEY_TAB) {
                /* alternador visual: segure ⌘ e aperte Tab para avançar (⇧ volta); soltar ⌘ troca de app */
                app_switcher_step((m & MOD_SHIFT) ? -1 : 1);
                (void)a;
                return true;
            }
            if (e->key == ',') { run_action(ACT_PREFS); return true; }
            if (e->key == KEY_ESC && (m & MOD_ALT)) { run_action(ACT_FORCEQUIT); return true; }
            if (dispatch_menu_shortcut(a, e)) { wm_damage_all(); return true; }
        }
        return false;
    }
    if (e->type != EV_MOUSE) return false;
    int x = e->dx, y = e->dy;
    bool lp = (e->buttons & 1) && !(last_buttons & 1);
    bool lr = !(e->buttons & 1) && (last_buttons & 1);
    bool rp = (e->buttons & 2) && !(last_buttons & 2);
    last_buttons = e->buttons;

    /* banner de um app: o clique entrega o token para o delegate (deskbridge.c) */
    if (lp) {
        int bpid = 0, btok = 0;
        if (notify_banner_hit(x, y, &bpid, &btok)) {
            extern void desk_banner_enqueue(int pid, int token);
            desk_banner_enqueue(bpid, btok);
            return true;
        }
    }

    /* ícone do Dock sendo arrastado: reordena ao vivo; puxado para fora do Dock, remove */
    if (DD.active) {
        wm_damage(rect_make(0, MENUBAR_H, SW, SH - MENUBAR_H));
        DD.remove = DD.app != &launch_app && y < dock_r.y - 60;
        dock_layout();   /* posições atuais (a troca anterior já mudou a ordem) */
        if (!DD.remove && rect_contains(rect_make(dock_r.x - 40, dock_r.y - 60, dock_r.w + 80, dock_r.h + 70), x, y)) {
            /* posição de destino: o ícone mantido cujo centro está mais perto do mouse */
            int best = -1;
            float bd = 1e9f;
            for (int i = 0; i < ndslots; i++) {
                dock_slot_t *d = &dslots[i];
                if (d->sep || d->win || dock_order_index(d->app) < 0) continue;
                float dd = fabsf_(d->x + d->size / 2 - x);
                if (dd < bd) { bd = dd; best = i; }
            }
            int from = dock_order_index(DD.app), to = best >= 0 ? dock_order_index(dslots[best].app) : -1;
            if (from >= 0 && to >= 0 && from != to) {
                app_t *m = dock_order[from];
                if (from < to) memmove(dock_order + from, dock_order + from + 1, (to - from) * sizeof(app_t *));
                else memmove(dock_order + to + 1, dock_order + to, (from - to) * sizeof(app_t *));
                dock_order[to] = m;
            }
        }
        if (lr) {
            if (DD.remove) {
                app_t *a = DD.app;
                DD.active = false;
                a->pinned = false;          /* aberto, continua no Dock à direita até sair */
                dock_order_sync();
            }
            DD.active = false;
            dock_pressed = -1;
            dock_hover_x = -1;
            wm_damage_all();
        }
        return true;
    }
    /* gravação de tela / acessibilidade: por cima de captura, alerta e janelas */
    if (tcc_prompt_mouse(x, y, SW, SH, lp, lr)) return true;
    /* menu do botão verde (organizar janela) */
    if (tile_popover_mouse(x, y, lp, lr)) return true;
    /* captura de tela (⌘⇧5): toma conta do mouse */
    if (sc_mouse(x, y, lp, lr, e->buttons)) return true;

    /* alerta modal */
    if (A.open) {
        rect_t r = alert_layout();
        int h = -1;
        for (int i = 0; i < A.nb; i++) if (rect_contains(alert_button(i, r), x, y)) h = i;
        if (lp) { A.pressed = h; wm_damage(r); }
        if (lr) { int p = A.pressed; A.pressed = -1; wm_damage(r); if (p >= 0 && p == h) alert_finish(p); }
        return true;
    }
    /* diálogo de salvar: modal (os menus pop-up dele são tratados logo abaixo) */
    if (SD.open && M.kind == MK_NONE) return sd_mouse(x, y, lp, lr);
    /* Central de Notificações, Launchpad e Central de Controle */
    if (nc_mouse(x, y, lp)) return true;
    if (lp_mouse(x, y, lp, lr)) return true;
    if (cc_mouse(x, y, e->buttons, lp, lr)) return true;
    /* Busca */
    if (spot_open) {
        rect_t r = spot_rect();
        if (lp && !rect_contains(r, x, y)) { desktop_toggle_spotlight(); return true; }
        if (rect_contains(r, x, y)) {
            int i = (y - r.y - 60 - (spot_calc[0] ? 40 : 0)) / 40;
            if (y > r.y + 60 && i >= 0 && i < spot_n) {
                if (i != spot_sel) { spot_sel = i; wm_damage(r); }
                if (lr) spot_activate(i);
            }
            return true;
        }
    }
    /* botão direito fora de um menu aberto fecha o menu (e segue: pode abrir outro no Dock) */
    if (rp && M.kind != MK_NONE && menu_item_at(x, y) < 0 && !(SM.open && rect_contains(sm_rect(), x, y))) desktop_close_menus();
    /* menus abertos */
    if (M.kind != MK_NONE) {
        /* submenu em cascata: o ponteiro dentro dele tem prioridade; o item pai fica cinza */
        if (SM.open && rect_contains(sm_rect(), x, y)) {
            int si = list_item_at(SM.items, SM.n, sm_rect(), x, y);
            if (si != SM.hover || M.hover != -1) { SM.hover = si; M.hover = -1; wm_damage_all(); }
            if (lr && si >= 0 && !SM.items[si].disabled && SM.items[si].label && !SM.items[si].header) {
                int act = SM.items[si].action, kind = M.kind, pid = M.popup_id;
                window_t *pw = M.popup_win;
                desktop_close_menus();
                if (kind == MK_POPUP) { popup_res.win = pw; popup_res.id = pid; popup_res.val = act; popup_res.ready = true; wm_invalidate(pw); }
                else run_action(act);
            }
            return true;
        }
        int it = menu_item_at(x, y);
        if (it >= 0 && M.items[it].submenu) { if (!SM.open || SM.parent != it) submenu_open(it); }
        else if (it >= 0 && SM.open) submenu_close();
        /* volume slider especial */
        if (M.kind == MK_BAR && M.title == TITLE_VOL && (e->buttons & 1) && y >= volume_slider_y - 10 && y <= volume_slider_y + 10 &&
            x >= volume_slider_x0 - 10 && x <= volume_slider_x0 + volume_slider_w + 10) {
            int v = (x - volume_slider_x0) * 100 / volume_slider_w;
            audio_set_volume(CLAMP(v, 0, 100));
            wm_damage_all();
            return true;
        }
        if (it != M.hover) { M.hover = it; wm_damage(rect_make(M.x - 30, M.y - 30, M.w + 60, M.h + 60)); }
        if (M.kind == MK_BAR && y < MENUBAR_H) {
            /* deslizar entre títulos com o menu aberto */
            for (int i = 0; i < nbar; i++)
                if (x >= bar[i].x && x < bar[i].x + bar[i].w && bar[i].title != M.title) { open_bar_menu(bar[i].title); return true; }
            for (int i = 0; i < nstatus; i++)
                if (x >= status[i].x && x < status[i].x + status[i].w && status[i].title != M.title) { open_bar_menu(status[i].title); return true; }
            if (lp) {
                for (int i = 0; i < nbar; i++) if (x >= bar[i].x && x < bar[i].x + bar[i].w) { desktop_close_menus(); return true; }
                for (int i = 0; i < nstatus; i++) if (x >= status[i].x && x < status[i].x + status[i].w) { desktop_close_menus(); return true; }
            }
        }
        if (lr) {
            bool quick = timer_ticks() - M.opened_at < 350;
            if (it >= 0 && !M.items[it].disabled && M.items[it].action != -77 && !M.items[it].submenu) {
                int act = M.items[it].action;
                int kind = M.kind;
                window_t *pw = M.popup_win;
                int pid = M.popup_id;
                desktop_close_menus();
                if (kind == MK_POPUP) { popup_res.win = pw; popup_res.id = pid; popup_res.val = act; popup_res.ready = true; wm_invalidate(pw); }
                else run_action(act);
            } else if (!quick && it < 0 && y >= MENUBAR_H) {
                desktop_close_menus();
            }
            return true;
        }
        if (lp && it < 0 && y >= MENUBAR_H) { desktop_close_menus(); return true; }
        return it >= 0 || y < MENUBAR_H || (e->buttons & 1);
    }
    /* barra de menus */
    if (y < MENUBAR_H && bar_visible()) {
        if (lp) {
            for (int i = 0; i < nbar; i++) if (x >= bar[i].x && x < bar[i].x + bar[i].w) { open_bar_menu(bar[i].title); return true; }
            for (int i = 0; i < nstatus; i++) if (x >= status[i].x && x < status[i].x + status[i].w) { open_bar_menu(status[i].title); return true; }
        }
        if (dock_hover_x >= 0) { dock_hover_x = -1; wm_damage(rect_make(0, SH - 200, SW, 200)); }
        return true;
    }
    /* Dock */
    int slot = dock_slot_at(x, y);
    bool over_dock = rect_contains(rect_make(dock_r.x - 10, dock_r.y - 6, dock_r.w + 20, dock_r.h + 10), x, y);
    /* arrastar um ícone mantido: começa quando o mouse se afasta alguns pixels com o botão pressionado */
    if (dock_pressed >= 0 && (e->buttons & 1) && !DD.active && (ABS_DIFF(x, DD.sx) > 6 || ABS_DIFF(y, DD.sy) > 6)) {
        dock_slot_t *d = &dslots[dock_pressed];
        if (!d->sep && !d->win && dock_order_index(d->app) >= 0) {
            DD.active = true;
            DD.app = d->app;
            DD.remove = false;
            wm_damage_all();
            return true;
        }
    }
    if (over_dock || (dock_hover_x >= 0 && slot >= 0)) {
        if (dock_hover_x != x) { dock_hover_x = (float)x; wm_damage(rect_make(0, SH - g_dock_size * 2 - 60, SW, g_dock_size * 2 + 60)); }
        if (rp && slot >= 0) { dock_pressed = -1; dock_open_menu(slot); return true; }
        if (lp && slot >= 0) { dock_pressed = slot; DD.sx = x; DD.sy = y; wm_damage(rect_make(0, SH - 200, SW, 200)); }
        if (lr && dock_pressed >= 0) {
            int p = dock_pressed;
            dock_pressed = -1;
            wm_damage(rect_make(0, SH - 200, SW, 200));
            if (p == slot && slot >= 0) {
                dock_slot_t *d = &dslots[slot];
                if (d->win) {
                    /* miniatura: restaura a janela com o gênio ao contrário */
                    if (d->win->minimized) { d->win->anim_dir = 1; d->win->anim = 0; wm_focus(d->win); }
                } else dock_click(d->app);
            }
        }
        return true;
    } else if (dock_hover_x >= 0) {
        dock_hover_x = -1;
        dock_pressed = -1;
        wm_damage(rect_make(0, SH - g_dock_size * 2 - 60, SW, g_dock_size * 2 + 60));
    }
    /* ícones na mesa (só quando nenhuma janela cobre o ponto) */
    if (lp) {
        extern window_t *wm_window_at_point(int x, int y);
        if (!wm_window_at_point(x, y)) {
            rect_t ir = rect_make(SW - 100, MENUBAR_H + 16, 90, 96);
            int old = desk_sel;
            if (rect_contains(ir, x, y)) {
                u64 t = timer_ticks();
                if (desk_sel == 0 && t - desk_click_t < 450) { extern void finder_open_path(const char *p); finder_open_path("/"); }
                desk_sel = 0;
                desk_click_t = t;
            } else desk_sel = -1;
            if (old != desk_sel) wm_damage(ir);
        }
    }
    return false;
}

/* ============================================================ tick */
static int last_minute = -1;

void desktop_tick(void) {
    login_tick();
    setup_tick();
    tcc_prompt_tick();
    { extern void apps_refresh_mac(void); apps_refresh_mac(); }   /* apps do macOS abertos: bolinha no Dock */
    datetime_t dt;
    static u64 last_rtc;
    u64 now = timer_ticks();
    if (now - last_rtc > 1000) {
        last_rtc = now;
        dock_prefs_tick();
        /* Abrir ao Iniciar: depois que a sessão começa */
        if (dock_login_pending && !login_active()) {
            dock_login_pending = false;
            for (int i = 0; i < apps_count(); i++) if (apps_get(i)->open_at_login && !apps_get(i)->running) app_launch(apps_get(i));
        }
        /* pilha Downloads: itens novos somam no selo (some ao abrir a pilha) */
        {
            static int dl_last = -1;
            vnode_t *dl = vfs_lookup(home_sub("/Downloads"));
            int c = dl ? vfs_count(dl) : 0;
            if (login_active()) c = -1;   /* troca de usuário: recomeça a contagem */
            if (dl_last >= 0 && c > dl_last) dock_set_badge(&downloads_app, downloads_app.badge + (c - dl_last));
            dl_last = c;
        }
        rtc_now(&dt);
        if (dt.minute != last_minute) {
            if (last_minute >= 0 && dt.minute == 0 && g_wallpaper_style == 0 && !T.dark) desktop_rebuild_wallpaper();
            last_minute = dt.minute;
            wm_damage(rect_make(0, 0, SW, MENUBAR_H));
            if (g_desktop_widgets) { wm_damage(rect_inset(widget_rect(0), -30)); wm_damage(rect_inset(widget_rect(1), -30)); }   /* ponteiros do relógio */
        }
    }
    for (int i = 0; i < nnotif; i++) if (now - notifs[i].t0 < 5600) wm_damage(rect_make(SW - 400, MENUBAR_H, 400, 110));
    for (int i = 0; i < apps_count(); i++) {
        app_t *a = apps_get(i);
        if (a->attention && a == wm_active_app()) a->attention = false;   /* o usuário veio ao app */
        if (a->bounce > 0) {
            a->bounce -= 0.012f;
            if (a->bounce < 0 || (!a->attention && wm_count_for(a))) a->bounce = 0;
            if (a->bounce == 0 && a->attention) a->bounce = 1;              /* pede atenção: continua pulando */
            wm_damage(rect_make(0, SH - 220, SW, 220));
        }
    }
    /* ocultar automaticamente: aparece com o mouse na borda de baixo, some ao sair (com pequena demora) */
    {
        int mx = wm_mouse_x(), my = wm_mouse_y();
        /* cantos ativos: encostar o ponteiro num canto (por ~¼ s) dispara a ação daquele canto, uma vez */
        {
            static int corner_in = -1;
            static u64 corner_t;
            static bool corner_fired;
            int c = mx <= 1 && my <= 1 ? 0 : mx >= SW - 2 && my <= 1 ? 1 : mx <= 1 && my >= SH - 2 ? 2 : mx >= SW - 2 && my >= SH - 2 ? 3 : -1;
            if (c != corner_in) { corner_in = c; corner_t = now; corner_fired = false; }
            else if (c >= 0 && !corner_fired && g_hot_corner[c] && now - corner_t > 250 && !login_active()) {
                corner_fired = true;
                switch (g_hot_corner[c]) {
                case 1: wm_toggle_mission_control(); break;
                case 2: nc_toggle(); break;
                case 3: desktop_toggle_launcher(); break;
                case 4: {   /* Mostrar Mesa: tira as janelas do caminho */
                    extern window_t *wm_nth(int);
                    for (int i = 0;; i++) { window_t *w = wm_nth(i); if (!w) break; if (w->visible && !w->minimized && !w->closing && !w->fullscreen) wm_minimize(w); }
                    break;
                }
                case 5: desktop_toggle_control_center(); break;
                case 6: run_action(ACT_LOCK); break;
                }
            }
        }
        /* tela cheia: barra de menus aparece encostando no topo e some ao descer */
        bool fsb = fs_bar;
        if (!wm_space_fullscreen()) fs_bar = false;
        else if (my <= 1) fs_bar = true;
        else if (my > MENUBAR_H + 24 && M.kind == MK_NONE) fs_bar = false;
        if (fsb != fs_bar) wm_damage(rect_make(0, 0, SW, MENUBAR_H + 2));
        float target = 0;
        if (g_dock_autohide || wm_space_fullscreen()) {
            bool near = dock_hide < 0.5f ? rect_contains(rect_make(dock_r.x - 20, dock_r.y - 50, dock_r.w + 40, SH - dock_r.y + 50), mx, my)
                                         : my >= SH - 3 && mx >= dock_r.x - 40 && mx < dock_r.x + dock_r.w + 40;
            bool keep = near || DD.active || dock_pressed >= 0 || M.kind == MK_CONTEXT || lp_open;
            if (keep) dock_leave_t = now;
            target = (keep || now - dock_leave_t < 350) ? 0 : 1;
        }
        if (dock_hide != target) {
            float step = 0.11f;
            dock_hide = dock_hide < target ? MIN(target, dock_hide + step) : MAX(target, dock_hide - step);
            if (dock_hide > 0.999f) dock_hide = 1;
            wm_damage(rect_make(0, SH - g_dock_size * 2 - 90, SW, g_dock_size * 2 + 90));
        }
    }
    if (spot_open && now % 500 < 20) wm_damage(spot_rect());
    if (lp_open && (lp_animating || now % 500 < 20)) wm_damage_all();
    nc_step();
    /* ⌘Tab: soltar o ⌘ confirma a troca de app */
    if (AS.open && !(input_mods() & MOD_CMD)) app_switcher_finish(true);
    if (SD.open && now % 530 < 20) wm_damage(sd_layout().r);
    if (shutdown_pending >= 0 && now > shutdown_at) {
        int k = shutdown_pending;
        shutdown_pending = -1;
        /* tela final */
        gfx_clear(g_screen, RGB(0, 0, 0));
        fb_flush(rect_make(0, 0, SW, SH));
        extern void persist_sync(void);
        persist_sync(); /* grava no disco o que ainda estiver pendente */
        /* atualização baixada e conferida (fs/update.c): troca os arquivos do sistema agora, com o sistema parado */
        extern bool update_ready(void);
        extern void update_on_shutdown(void);
        if (update_ready()) {
            const char *msg = "Instalando a atualização do mOS…";
            int tw = text_width(font_get(FONT_REGULAR), 15, msg);
            text_draw(g_screen, font_get(FONT_REGULAR), 15, (SW - tw) / 2, SH / 2, RGB(236, 236, 236), msg);
            fb_flush(rect_make(0, 0, SW, SH));
            update_on_shutdown();
        }
        sleep_ms(300);
        if (k) acpi_reboot(); else acpi_poweroff();
    }
}

/* ============================================================ boot, pânico e loop principal */
static void boot_frame(float progress) {
    gfx_clear(g_screen, RGB(0, 0, 0));
    g_screen->ox = PX(); g_screen->oy = PY();    /* desenha no monitor principal */
    glyph_mos_logo(g_screen, SW / 2.0f, SH / 2.0f - 40, 46, RGB(236, 236, 236), RGB(0, 0, 0));
    int bw = 220, bx = (SW - bw) / 2, by = SH / 2 + 60;
    gfx_round_rect(g_screen, bx, by, bw, 6, 3, RGB(70, 70, 70));
    gfx_round_rect(g_screen, bx, by, MAX(6, (int)(bw * progress)), 6, 3, RGB(236, 236, 236));
    g_screen->ox = g_screen->oy = 0;
    fb_flush(rect_make(0, 0, SW, SH));
}

void panic_screen(const char *msg, int_frame_t *f) {
    if (!g_screen) {
        for (u32 i = 0; i < g_boot->fb.pitch * g_boot->fb.height; i++) g_boot->fb.fb[i] = 0x202020;
        return;
    }
    surface_reset_clip(g_screen);
    gfx_clear(g_screen, RGB(30, 30, 30));
    g_screen->ox = PX(); g_screen->oy = PY();    /* a mensagem vai no monitor principal */
    font_t *fr = font_get(FONT_REGULAR), *fb = font_get(FONT_SEMIBOLD);
    int y = SH / 2 - 120;
    glyph_mos_logo(g_screen, SW / 2.0f, y, 34, RGB(220, 220, 220), RGB(30, 30, 30));
    y += 70;
    rect_t r = rect_make(0, y, SW, 30);
    text_draw_center(g_screen, fb, 22, r, RGB(240, 240, 240), "O mOS encontrou um problema e precisa ser reiniciado.");
    r.y += 36;
    text_draw_center(g_screen, fr, 15, r, RGB(190, 190, 190), "Mantenha o botão de energia pressionado por alguns segundos ou reinicie a máquina.");
    r.y += 50;
    text_draw_center(g_screen, fr, 13, r, RGB(150, 150, 150), msg);
    if (f) {
        char regs[160];
        snprintf(regs, sizeof regs, "RAX=%llx RBX=%llx RCX=%llx RDX=%llx RSP=%llx", f->rax, f->rbx, f->rcx, f->rdx, f->rsp);
        r.y += 22;
        text_draw_center(g_screen, fr, 12, r, RGB(120, 120, 120), regs);
    }
    fb_flush(rect_make(0, 0, SW, SH));
}

void desktop_init(void) {
    SW = display_rect(0).w;               /* barra de menus, Dock e menus vivem no monitor principal */
    SH = display_rect(0).h;
    trash_app.name = "Lixeira";
    trash_app.exec = "lixeira";
    launch_app.name = "Apps";
    launch_app.exec = "launchpad";
    launch_app.draw_icon = draw_launch_icon;
    downloads_app.name = "Downloads";
    downloads_app.exec = "downloads";
    downloads_app.draw_icon = draw_downloads_icon;
    desktop_rebuild_wallpaper();
}

void desktop_run(void) {
    fb_init();
    SW = display_rect(0).w; SH = display_rect(0).h;
    font_init();
    font_load_fallback();                 /* CJK: o IME e os apps precisam de glifos que a Inter não tem */
    boot_frame(0.15f);
    theme_set(false, 0);
    wm_init();
    boot_frame(0.35f);
    desktop_init();
    boot_frame(0.7f);
    apps_register_all();
    if (!setup_boot()) login_boot();     /* o assistente roda antes do login (idioma e teclado valem antes da senha) */
    boot_frame(1.0f);
    sleep_ms(250);
    sound_startup_chime();
    wm_damage_all();
    kprintf("Desktop pronto em %llu ms\n", timer_ticks());
    if (gpu_display_notice()[0]) notify("Monitor externo", gpu_display_notice());   /* dev/gpu.c */
    {   /* medição do compositor (só em imagens build.ps1 -Tests): tela cheia, serial x todos os núcleos */
        if (vfs_lookup("/Sistema/Programas/.autoteste")) {
            u64 us[2];
            for (int mode = 0; mode < 2; mode++) {
                par_set_enabled(mode == 1);
                u64 t0 = hpet_ns(), m0 = timer_ticks();
                for (int k = 0; k < 10; k++) { wm_damage_all(); wm_compose(); }
                us[mode] = hpet_ns() ? (hpet_ns() - t0) / 10000 : (timer_ticks() - m0) * 100;
            }
            par_set_enabled(true);
            kprintf("bench: quadro em tela cheia %ux%u: %llu us com 1 núcleo, %llu us com %d núcleos (%llu.%llux)\n",
                    SW, SH, us[0], us[1], par_cpus(), us[1] ? us[0] / us[1] : 0, us[1] ? us[0] * 10 / us[1] % 10 : 0);
        }
    }
    u64 last_frame = 0;
    for (;;) {
        input_event_t e;
        int n = 0;
        while (n < 64 && input_pop(&e)) {
            n++;
            if (!wm_handle_event(&e) && e.type == EV_MOUSE) {}
        }
        net_poll();
        { extern void wifi_poll(void); wifi_poll(); }   /* varredura Wi-Fi mesmo com o cabo ativo */
        usb_poll();
        audio_poll();
        extern void persist_tick(void);
        persist_tick();
        for (int i = 0; i < apps_count(); i++) {
            app_t *a = apps_get(i);
            if ((a->running || a->always_tick) && a->tick) a->tick(a);
        }
        desktop_tick();
        u64 now = timer_ticks();
        if (now - last_frame >= 15) {
            last_frame = now;
            wm_compose();
        }
        extern volatile u64 g_idle_ticks;
        u64 t0 = timer_ticks();
        hlt();
        g_idle_ticks += timer_ticks() - t0;
    }
}

/* o arranjo dos monitores mudou (gfx.c, display_set_layout): a barra de menus e o Dock vão para o monitor
 * principal atual, com o tamanho dele; menus e painéis abertos fecham (estavam no lugar antigo) */
void desktop_displays_changed(void) {
    int nw = display_rect(0).w, nh = display_rect(0).h;
    bool resized = nw != SW || nh != SH;
    SW = nw; SH = nh;
    desktop_close_menus();
    if (resized) desktop_rebuild_wallpaper();
    wm_damage_all();
}
/* app Captura de Tela: abre a barra de captura (1 tela inteira, 2 janela, 3 seleção) */
void desktop_screenshot_ui(int mode) { sc_open(mode); }
