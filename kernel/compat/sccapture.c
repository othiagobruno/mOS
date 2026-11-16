/*
 * mOS — captura para o ScreenCaptureKit: a tela composta de novo com um filtro de janelas, e a superfície de uma janela.
 *
 * O compositor (kernel/ui/wm.c) desenha tudo em g_screen; o SCContentFilter precisa de outra composição: só algumas janelas
 * (includingWindows/Applications, sem papel de parede) ou todas menos algumas (excludingWindows/Applications, o próprio app
 * que grava, por exemplo), com ou sem o cursor (showsCursor). Aqui a composição usa as mesmas peças públicas do compositor:
 * desktop_draw_background, as superfícies das janelas de trás para a frente, desktop_draw_overlay (barra de menus e Dock)
 * e o cursor (wm_cursor_image). Sombras e o vidro da barra lateral ficam de fora. Código original do mOS (MIT).
 */
#include "kernel.h"
#include "ui.h"
#include "gfx.h"
#include "proc.h"
#include "syscall.h"
#include "../include/compat_gui.h"

static surface_t *scratch;
static volatile u32 busy;

static bool listed(const gui_sc_compose_t *r, u32 id) {
    for (u32 i = 0; i < r->nwin && i < GUI_SC_MAXWIN; i++) if (r->win[i] == id) return true;
    return false;
}
static bool shown(window_t *w) { return w && w->visible && !w->minimized && !w->tab_hidden && !w->closing && w->surf; }

/* copia um retângulo de uma superfície para o app, linha a linha */
static bool copy_rect(process_t *p, u64 dst, const surface_t *s, int x, int y, int w, int h) {
    for (int r = 0; r < h; r++)
        if (!uvm_copy_out(p, dst + (u64)r * (u64)w * 4, s->px + (u64)(y + r) * (u64)s->stride + x, (usize)w * 4)) return false;
    return true;
}

static i64 compose(process_t *p, u64 ureq, u64 buf) {
    gui_sc_compose_t r;
    if (!g_screen) return -MOS_EINVAL;
    if (!uvm_copy_in(p, &r, ureq, sizeof r)) return -MOS_EFAULT;
    int sw = g_screen->w, sh = g_screen->h;
    int x = r.x, y = r.y, w = r.w, h = r.h;
    if (w <= 0 || h <= 0) { x = 0; y = 0; w = sw; h = sh; }
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w <= 0 || h <= 0) return -MOS_EINVAL;
    i64 dims = (i64)((u64)(u32)w | (u64)(u32)h << 32);
    if (!buf) return dims;
    if (__atomic_exchange_n(&busy, 1, __ATOMIC_ACQUIRE)) return -MOS_EAGAIN;   /* um quadro por vez (a superfície é única) */
    if (!scratch) scratch = surface_new(sw, sh);
    else if (scratch->w != sw || scratch->h != sh) surface_resize(scratch, sw, sh);
    if (!scratch) { __atomic_store_n(&busy, 0, __ATOMIC_RELEASE); return -MOS_ENOMEM; }
    surface_t *s = scratch;
    rect_t area = rect_make(x, y, w, h);
    surface_reset_clip(s);
    surface_clip(s, area);
    bool include = r.flags & GUI_SC_INCLUDE;
    if (r.flags & GUI_SC_DESKTOP) desktop_draw_background(s, area);
    else gfx_fill(s, x, y, w, h, 0);   /* fora das janelas incluídas: transparente */
    for (int i = 0; i < wm_window_count(); i++) {
        window_t *win = wm_window_at(i);
        if (!shown(win)) continue;
        bool in = listed(&r, (u32)win->id);
        if (include ? !in : in) continue;
        if (rect_empty(rect_intersect(rect_make(win->x, win->y, win->w, win->h), area))) continue;
        gfx_blend(s, win->surf, win->x, win->y, 255);
    }
    if (r.flags & GUI_SC_DESKTOP) desktop_draw_overlay(s, area);
    if (r.flags & GUI_SC_CURSOR) {
        int cx, cy;
        surface_t *c = wm_cursor_image(&cx, &cy);
        if (c) gfx_blend(s, c, cx, cy, 255);
    }
    surface_reset_clip(s);
    bool ok = copy_rect(p, buf, s, x, y, w, h);
    __atomic_store_n(&busy, 0, __ATOMIC_RELEASE);
    return ok ? dims : -MOS_EFAULT;
}

static i64 window_pixels(process_t *p, u32 number, u64 buf) {
    for (int i = 0; i < wm_window_count(); i++) {
        window_t *w = wm_window_at(i);
        if (!w || (u32)w->id != number || w->closing || !w->surf) continue;
        int ww = w->surf->w, hh = w->surf->h;
        if (ww <= 0 || hh <= 0) return -MOS_EINVAL;
        i64 dims = (i64)((u64)(u32)ww | (u64)(u32)hh << 32);
        if (!buf) return dims;
        return copy_rect(p, buf, w->surf, 0, 0, ww, hh) ? dims : -MOS_EFAULT;
    }
    return -MOS_ESRCH;
}

/* áudio do sistema (kernel/dev/sound.c): até 1/4 s por chamada */
static i64 system_audio(process_t *p, u64 buf, u64 frames, u64 upos) {
    extern u32 audio_system_read(i16 *dst, u32 frames, u64 *pos);
    u64 pos = 0;
    if (!upos || !uvm_copy_in(p, &pos, upos, sizeof pos)) return -MOS_EFAULT;
    u32 n = (u32)MIN(frames, 12000ull);
    if (!n) return 0;
    i16 *tmp = kmalloc((usize)n * 4);
    if (!tmp) return -MOS_ENOMEM;
    u32 got = audio_system_read(tmp, n, &pos);
    bool ok = (!got || uvm_copy_out(p, buf, tmp, (usize)got * 4)) && uvm_copy_out(p, upos, &pos, sizeof pos);
    kfree(tmp);
    return ok ? (i64)got : -MOS_EFAULT;
}

i64 sc_capture_syscall(process_t *p, u64 op, u64 b, u64 c, u64 d) {
    if (op == DEV_AUDIO_SYSTEM) return system_audio(p, b, c, d);
    if (op == DEV_SCREEN_COMPOSE) return compose(p, b, c);
    if (op == DEV_WINDOW_CAPTURE) return window_pixels(p, (u32)b, c);
    return -MOS_EINVAL;
}
