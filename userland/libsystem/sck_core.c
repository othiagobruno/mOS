/* mOS — núcleo do ScreenCaptureKit (ver sck_core.h). Código original do mOS (MIT). */
#include "sck_core.h"

static int has_u(const uint32_t *a, int n, uint32_t v) { for (int i = 0; i < n; i++) if (a[i] == v) return 1; return 0; }
static int has_i(const int32_t *a, int n, int32_t v) { for (int i = 0; i < n; i++) if (a[i] == v) return 1; return 0; }

int sck_resolve(const sck_filter *f, const sck_win *wins, int n, uint32_t *ids, int *include) {
    int k = 0;
    switch (f->kind) {
    case SCK_DISPLAY_EXCL_WINDOWS: case SCK_DISPLAY_INCL_WINDOWS:
        *include = f->kind == SCK_DISPLAY_INCL_WINDOWS;
        for (int i = 0; i < f->nwin && k < SCK_MAX; i++) ids[k++] = f->win[i];
        return k;
    case SCK_DISPLAY_EXCL_APPS:   /* janelas dos apps excluídos, menos as exceções (que continuam aparecendo) */
        *include = 0;
        for (int i = 0; i < n && k < SCK_MAX; i++)
            if (has_i(f->pid, f->npid, wins[i].pid) && !has_u(f->win, f->nwin, wins[i].id)) ids[k++] = wins[i].id;
        return k;
    case SCK_DISPLAY_INCL_APPS:   /* janelas dos apps incluídos, menos as exceções */
        *include = 1;
        for (int i = 0; i < n && k < SCK_MAX; i++)
            if (has_i(f->pid, f->npid, wins[i].pid) && !has_u(f->win, f->nwin, wins[i].id)) ids[k++] = wins[i].id;
        return k;
    }
    *include = 1;
    ids[0] = f->window;
    return 1;
}

sck_rect sck_layout(int sw, int sh, int ow, int oh, int scales, int preserve, sck_rect d) {
    if (d.w <= 0 || d.h <= 0) { d.x = 0; d.y = 0; d.w = ow; d.h = oh; }
    sck_rect r = d;
    if (sw <= 0 || sh <= 0) { r.w = r.h = 0; return r; }
    if (!scales) { r.w = sw; r.h = sh; return r; }   /* sem escala: no canto do destino, cortado pela saída */
    if (preserve) {
        double k = d.w / sw < d.h / sh ? d.w / sw : d.h / sh;
        r.w = sw * k; r.h = sh * k;
        r.x = d.x + (d.w - r.w) / 2; r.y = d.y + (d.h - r.h) / 2;
    }
    return r;
}

static uint32_t lerp(uint32_t a, uint32_t b, uint32_t t) {   /* t em 0..256 */
    uint32_t rb = ((a & 0xFF00FF) * (256 - t) + (b & 0xFF00FF) * t) >> 8 & 0xFF00FF;
    uint32_t ag = (((a >> 8) & 0xFF00FF) * (256 - t) + ((b >> 8) & 0xFF00FF) * t) & 0xFF00FF00;
    return rb | ag;
}
void sck_place(const uint32_t *src, int sw, int sh, int ss, uint32_t *dst, int ow, int oh, sck_rect r, uint32_t bg) {
    for (int i = 0; i < ow * oh; i++) dst[i] = bg;
    int x0 = (int)(r.x + 0.5), y0 = (int)(r.y + 0.5), rw = (int)(r.w + 0.5), rh = (int)(r.h + 0.5);
    if (rw <= 0 || rh <= 0 || sw <= 0 || sh <= 0) return;
    int same = rw == sw && rh == sh;
    for (int y = 0; y < rh; y++) {
        int dy = y0 + y;
        if (dy < 0 || dy >= oh) continue;
        /* centro do pixel de destino no espaço da origem, em 1/256 */
        long long fy = same ? (long long)y << 8 : (((long long)(2 * y + 1) * sh << 8) / (2 * rh)) - 128;
        if (fy < 0) fy = 0;
        int sy = (int)(fy >> 8), ty = (int)(fy & 255);
        if (sy >= sh - 1) { sy = sh - 1; ty = 0; }
        const uint32_t *r0 = src + (size_t)sy * ss, *r1 = src + (size_t)(sy + 1 < sh ? sy + 1 : sy) * ss;
        for (int x = 0; x < rw; x++) {
            int dx = x0 + x;
            if (dx < 0 || dx >= ow) continue;
            if (same) { dst[(size_t)dy * ow + dx] = r0[x]; continue; }
            long long fx = (((long long)(2 * x + 1) * sw << 8) / (2 * rw)) - 128;
            if (fx < 0) fx = 0;
            int sx = (int)(fx >> 8), tx = (int)(fx & 255);
            if (sx >= sw - 1) { sx = sw - 1; tx = 0; }
            int sx1 = sx + 1 < sw ? sx + 1 : sx;
            uint32_t top = lerp(r0[sx], r0[sx1], (uint32_t)tx), bot = lerp(r1[sx], r1[sx1], (uint32_t)tx);
            dst[(size_t)dy * ow + dx] = lerp(top, bot, (uint32_t)ty);
        }
    }
}

void sck_to_l10r(const uint32_t *p, int w, int h, uint32_t *out) {
    for (size_t i = 0; i < (size_t)w * h; i++) {
        uint32_t c = p[i], a = c >> 30, r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
        out[i] = a << 30 | (r << 2 | r >> 6) << 20 | (g << 2 | g >> 6) << 10 | (b << 2 | b >> 6);
    }
}

uint64_t sck_hash(const uint32_t *px, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) { h ^= px[i]; h *= 1099511628211ull; }
    return h;
}
