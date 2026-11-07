/* mOS — janela de início de sessão (login) e encerramento de sessão */
#include "ui.h"
#include "apps.h"
#include "account.h"
#include "i18n.h"
#include "sysstate.h"
#include "crypto.h"

static bool active;
static int sel;                 /* conta selecionada */
static char pw[64];
static int caret;
static u64 shake_until, blink0;
static char msg[96];
static bool verifying;
static surface_t *bg;            /* tela desfocada capturada ao abrir o login */

bool login_active(void) { return active; }

static int SW_(void) { return display_rect(0).w; }   /* a tela de login fica no monitor principal */
static int SH_(void) { return display_rect(0).h; }

static void avatar(surface_t *s, float cx, float cy, float r, const char *name, bool selected) {
    static const u32 cols[6] = {RGB(90, 120, 230), RGB(230, 110, 80), RGB(70, 170, 120), RGB(170, 90, 200), RGB(240, 170, 40), RGB(60, 160, 200)};
    u32 h = 0;
    for (const char *p = name; *p; p++) h = h * 31 + (u8)*p;
    u32 c = cols[h % 6];
    gfx_circle(s, cx, cy + 2, r, RGBA(0, 0, 0, 60));
    gfx_round_rect_grad(s, cx - r, cy - r, 2 * r, 2 * r, r, color_lighten(c, 40), c);
    if (selected) gfx_circle_stroke(s, cx, cy, r + 4, 3, RGBA(255, 255, 255, 220));
    char ini[8] = {0};
    int k = 0;
    const char *p = name;
    ini[k++] = (char)toupper((u8)*p);
    for (; *p && k < 2; p++) if (*p == ' ' && p[1]) ini[k++] = (char)toupper((u8)p[1]);
    text_draw_center(s, font_get(FONT_SEMIBOLD), r * 0.8f, rect_make((int)(cx - r), (int)(cy - r), (int)(2 * r), (int)(2 * r)), RGB(255, 255, 255), ini);
}

static void account_at(int i, char *name, usize nc, char *full, usize fc, bool *haspw) {
    bool admin;
    name[0] = full[0] = 0;
    *haspw = false;
    account_get(i, name, nc, full, fc, &admin, haspw);
    if (!full[0]) strncpy(full, name, fc - 1);
}

void login_show(void) {
    active = true;
    if (bg) { surface_free(bg); bg = NULL; }
    pw[0] = 0; caret = 0; msg[0] = 0;
    sel = 0;
    for (int i = 0; i < account_count(); i++) {
        char n[64], f[96];
        bool hp;
        account_at(i, n, sizeof n, f, sizeof f, &hp);
        if (!strcmp(n, account_current())) sel = i;
    }
    blink0 = timer_ticks();
    wm_damage_all();
}

/* no boot: pede senha se alguma conta tiver senha; senão entra direto na conta padrão */
void login_boot(void) {
    bool any = false;
    for (int i = 0; i < account_count(); i++) {
        char n[64], f[96];
        bool hp;
        account_at(i, n, sizeof n, f, sizeof f, &hp);
        if (hp) any = true;
    }
    /* sem Visitante (sistema instalado / live persistente) a sessão NUNCA começa sem senha */
    if (any || account_count() > 1 || !sysstate_guest_allowed()) login_show();
}

static void finish_login(const char *name) {
    account_set_current(name);
    active = false;
    pw[0] = 0;
    if (bg) { surface_free(bg); bg = NULL; }
    char m[128];
    snprintf(m, sizeof m, "%s, %s", tr(S_WELCOME_TITLE), account_fullname());
    notify(m, tr(S_LOGIN_WELCOME));
    sound_startup_chime();
    wm_damage_all();
}

static void try_login(void) {
    char n[64], f[96];
    bool hp;
    account_at(sel, n, sizeof n, f, sizeof f, &hp);
    if (account_verify(n, pw)) { finish_login(n); return; }
    shake_until = timer_ticks() + 450;
    snprintf(msg, sizeof msg, "%s", tr(S_LOGIN_BAD));
    pw[0] = 0; caret = 0;
    sound_alert();
    wm_damage_all();
}

/* encerra a sessão: fecha os apps (salvando o que for possível) e volta à janela de login */
void login_logout(void) {
    for (int i = 0; i < apps_count(); i++) {
        app_t *a = apps_get(i);
        if (a->running && strcmp(a->exec, "arquivos")) app_quit(a);
    }
    wm_close_app(apps_find("arquivos"));
    login_show();
}

void login_draw(surface_t *s) {
    if (!active) return;
    int W = SW_(), H = SH_();
    if (!bg) {
        /* papel de parede desfocado e escurecido, gerado uma vez por abertura do login */
        /* tela de bloqueio atual: o papel de parede quase nítido, só um véu escuro para o texto */
        bg = surface_new(W, H);
        desktop_draw_background(bg, rect_make(0, 0, W, H));
        gfx_blur(bg, rect_make(0, 0, W, H), 6);
        gfx_fill(bg, 0, 0, W, H, RGBA(0, 0, 0, 40));
    }
    gfx_blit_rect(s, bg, s->clip, s->clip.x, s->clip.y);
    datetime_t dt;
    rtc_now(&dt);
    char t[64];
    /* data e hora grandes no topo; a hora com sombra suave, em branco levemente translúcido (vidro) */
    if (g_lang == LANG_EN) snprintf(t, sizeof t, "%s, %s %d", i18n_weekday(dt.weekday), i18n_month(dt.month), dt.day);
    else snprintf(t, sizeof t, "%s, %d de %s", i18n_weekday(dt.weekday), dt.day, i18n_month(dt.month));
    text_draw_center(s, font_get(FONT_SEMIBOLD), 21, rect_make(0, 54, W, 30), RGBA(255, 255, 255, 235), t);
    snprintf(t, sizeof t, "%d:%02d", dt.hour, dt.minute);
    text_draw_center(s, font_get(FONT_BOLD), 112, rect_make(0, 82, W, 150), RGBA(0, 0, 0, 40), t);
    text_draw_center(s, font_get(FONT_BOLD), 112, rect_make(0, 78, W, 150), RGBA(255, 255, 255, 225), t);

    int n = account_count();
    char name[64], full[96];
    bool haspw;
    account_at(sel, name, sizeof name, full, sizeof full, &haspw);
    int shake = 0;
    if (timer_ticks() < shake_until) shake = (int)(fsin((shake_until - timer_ticks()) / 25.0f) * 10);
    /* usuário embaixo: foto, nome e o campo de senha em cápsula de vidro */
    float cx = W / 2.0f, cy = H - 236;
    avatar(s, cx, cy, 34, full, false);
    font_t *fr = font_get(FONT_REGULAR);
    text_draw_center(s, font_get(FONT_SEMIBOLD), 16, rect_make(0, (int)cy + 42, W, 24), RGB(255, 255, 255), full);
    int fw = 220, fh = 32, fx = (int)cx - fw / 2 + shake, fy = (int)cy + 76;
    glass_panel(s, fx, fy, fw, fh, fh / 2.0f, GLASS_CLEAR);
    gfx_round_rect_stroke(s, fx + 0.5f, fy + 0.5f, fw - 1, fh - 1, fh / 2.0f - 0.5f, 1, RGBA(255, 255, 255, 90));
    int base = fy + 21;
    if (!pw[0]) text_draw_center(s, fr, 13.5f, rect_make(fx, fy, fw - 12, fh), RGBA(255, 255, 255, 170), haspw ? tr(S_LOGIN_PW) : tr(S_LOGIN_ENTER));
    int dots = (int)strlen(pw);
    for (int i = 0; i < dots && i < 22; i++) gfx_circle(s, fx + 20 + i * 11, fy + fh / 2.0f, 3.3f, RGB(255, 255, 255));
    if (pw[0] && ((timer_ticks() - blink0) / 530) % 2 == 0) gfx_fill(s, fx + 16 + dots * 11, fy + 8, 2, fh - 16, RGB(255, 255, 255));
    (void)base;
    /* seta para entrar (só com algo digitado, como no sistema) */
    if (pw[0] || !haspw) {
        gfx_circle(s, fx + fw - 17, fy + fh / 2.0f, 11, RGBA(255, 255, 255, 80));
        glyph_chevron(s, fx + fw - 26, fy + fh / 2 - 9, 18, RGB(255, 255, 255), 0);
    }
    if (msg[0]) text_draw_center(s, fr, 13, rect_make(0, fy + fh + 10, W, 20), RGB(255, 200, 190), msg);
    else if (verifying) text_draw_center(s, fr, 13, rect_make(0, fy + fh + 10, W, 20), RGBA(255, 255, 255, 200), tr(S_LOGIN_VERIFY));
    /* outras contas: fileira pequena acima do usuário */
    if (n > 1) {
        int bw = 84, total = n * bw, x0 = (W - total) / 2, by = (int)cy - 110;
        for (int i = 0; i < n; i++) {
            if (i == sel) continue;
            char nn[64], ff[96];
            bool hp;
            account_at(i, nn, sizeof nn, ff, sizeof ff, &hp);
            float ax = x0 + i * bw + bw / 2.0f;
            avatar(s, ax, by, 20, ff, false);
            text_draw_center(s, fr, 11.5f, rect_make((int)ax - bw / 2, by + 24, bw, 18), RGBA(255, 255, 255, 220), ff);
        }
    }
    /* reiniciar / desligar: discretos no rodapé */
    int py = H - 58;
    gfx_circle(s, W / 2 - 60, py, 15, RGBA(255, 255, 255, 46));
    glyph_reload(s, W / 2 - 70, py - 10, 20, RGB(255, 255, 255));
    gfx_circle(s, W / 2 + 60, py, 15, RGBA(255, 255, 255, 46));
    gfx_ring_arc(s, W / 2 + 60, py + 1, 7, 1.8f, 0.6f, 2 * PI_F - 0.6f, RGB(255, 255, 255));
    gfx_line(s, W / 2 + 60, py - 9, W / 2 + 60, py + 1, 1.8f, RGB(255, 255, 255));
    text_draw_center(s, fr, 11, rect_make(W / 2 - 110, py + 20, 100, 16), RGBA(255, 255, 255, 200), tr(S_LOGIN_RESTART));
    text_draw_center(s, fr, 11, rect_make(W / 2 + 10, py + 20, 100, 16), RGBA(255, 255, 255, 200), tr(S_LOGIN_SHUTDOWN));
}

bool login_handle_event(input_event_t *e) {
    if (!active) return false;
    int W = SW_(), H = SH_();
    static int lastb;
    if (e->type == EV_KEY_DOWN) {
        int len = (int)strlen(pw);
        if (e->key == KEY_ENTER) try_login();
        else if (e->key == KEY_BACKSPACE) { if (len) pw[utf8_prev(pw, len)] = 0; msg[0] = 0; }
        else if (e->key == KEY_ESC) { pw[0] = 0; msg[0] = 0; }
        else if (e->key == KEY_LEFT && sel > 0) { sel--; pw[0] = 0; msg[0] = 0; }
        else if (e->key == KEY_RIGHT && sel < account_count() - 1) { sel++; pw[0] = 0; msg[0] = 0; }
        else if (e->key >= 32 && e->key < 0x100 && len < 60) {
            crypto_rng_add_entropy(&e->key, sizeof e->key); len += utf8_encode((u32)e->key, pw + len); pw[len] = 0; msg[0] = 0; }
        blink0 = timer_ticks();
        wm_damage_all();
        return true;
    }
    if (e->type == EV_MOUSE) {
        int x = e->dx, y = e->dy;
        bool lp = (e->buttons & 1) && !(lastb & 1);
        lastb = e->buttons;
        if (lp) {
            float cx = W / 2.0f, cy = H - 236;
            int fw = 220, fx = (int)cx - fw / 2, fy = (int)cy + 76;
            if (rect_contains(rect_make(fx + fw - 32, fy, 32, 32), x, y)) try_login();
            int n = account_count();
            if (n > 1) {
                int bw = 84, x0 = (W - n * bw) / 2, by = (int)cy - 110;
                for (int i = 0; i < n; i++)
                    if (i != sel && rect_contains(rect_make(x0 + i * bw, by - 24, bw, 64), x, y)) { sel = i; pw[0] = 0; msg[0] = 0; }
            }
            int py = H - 58;
            if (rect_contains(rect_make(W / 2 - 80, py - 20, 40, 56), x, y)) acpi_reboot();
            if (rect_contains(rect_make(W / 2 + 40, py - 20, 40, 56), x, y)) acpi_poweroff();
            wm_damage_all();
        }
        return true;
    }
    return true;
}

void login_tick(void) {
    if (!active) return;
    static u64 last;
    u64 now = timer_ticks();
    if (now - last > 250 || now < shake_until) { last = now; wm_damage_all(); }
}
