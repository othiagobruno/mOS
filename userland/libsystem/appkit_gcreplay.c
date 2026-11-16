/* mOS — reprodução do lote de desenho (ver appkit_gcreplay.h). Teste de host: -DCX_GCR_HOST. */
#ifdef CX_GCR_HOST
#include <string.h>
#include <stdlib.h>
#else
#include "sys.h"
#include "libc.h"
void free(void *p);
#endif
#include "appkit_gcreplay.h"

/* os mesmos números do enum de compat_gui.h (GC_CLEAR = 1 ...) */
enum { R_CLEAR = 1, R_FILL, R_RRECT, R_RRECT_STROKE, R_LINE, R_POLY, R_POLYLINE, R_TEXT, R_CLIP, R_UNCLIP, R_GRAD, R_IMAGE, R_COMP_FILL, R_COMP_IMAGE };

static float rf(const uint8_t *p) { float f; memcpy(&f, p, 4); return f; }
static int32_t ri(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static uint32_t ru(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

int gcr_replay(const uint8_t *cmd, uint32_t n, double H, int flip, const gcr_sink_t *s) {
    uint32_t off = 0;
    int count = 0;
    #define YR(y, h) (flip ? H - (y) - (h) : (y))
    #define YP(y) (flip ? H - (y) : (y))
    while (off + 8 <= n) {
        uint32_t op = ru(cmd + off), len = ru(cmd + off + 4);
        if (len < 8 || off + len > n) return -1;
        const uint8_t *p = cmd + off + 8;
        uint32_t pl = len - 8;
        switch (op) {
        case R_CLEAR: if (pl < 4) return -1; if (s->clear) s->clear(s->ctx, ru(p)); break;
        case R_FILL:
            if (pl < 20) return -1;
            if (s->fill) { double x = ri(p), y = ri(p + 4), w = ri(p + 8), h = ri(p + 12); s->fill(s->ctx, x, YR(y, h), w, h, 0, ru(p + 16)); }
            break;
        case R_RRECT:
            if (pl < 24) return -1;
            if (s->fill) { double x = rf(p), y = rf(p + 4), w = rf(p + 8), h = rf(p + 12); s->fill(s->ctx, x, YR(y, h), w, h, rf(p + 16), ru(p + 20)); }
            break;
        case R_RRECT_STROKE:
            if (pl < 28) return -1;
            if (s->stroke_rect) { double x = rf(p), y = rf(p + 4), w = rf(p + 8), h = rf(p + 12); s->stroke_rect(s->ctx, x, YR(y, h), w, h, rf(p + 16), rf(p + 20), ru(p + 24)); }
            break;
        case R_LINE:
            if (pl < 24) return -1;
            if (s->line) s->line(s->ctx, rf(p), YP(rf(p + 4)), rf(p + 8), YP(rf(p + 12)), rf(p + 16), ru(p + 20));
            break;
        case R_POLY: case R_POLYLINE: {
            uint32_t hdr = op == R_POLY ? 8 : 12;
            if (pl < hdr) return -1;
            uint32_t np = ru(p), col = ru(p + 4);
            double t = op == R_POLYLINE ? rf(p + 8) : 0;
            if (np > (pl - hdr) / 8) return -1;
            if (s->poly && np) {
                double *xy = malloc(sizeof(double) * 2 * np);
                if (!xy) return -1;
                for (uint32_t i = 0; i < np; i++) { xy[2 * i] = rf(p + hdr + 8 * i); xy[2 * i + 1] = YP(rf(p + hdr + 8 * i + 4)); }
                s->poly(s->ctx, xy, np, col, op == R_POLY, t);
                free(xy);
            }
            break;
        }
        case R_TEXT: {
            if (pl < 24) return -1;
            uint32_t bytes = ru(p + 20);
            if (bytes > pl - 24) return -1;
            if (s->text) s->text(s->ctx, rf(p), YP(rf(p + 4)), rf(p + 8), (int)ru(p + 12), ru(p + 16), (const char *)p + 24, bytes);
            break;
        }
        case R_CLIP:
            if (pl < 16) return -1;
            if (s->clip) { double x = ri(p), y = ri(p + 4), w = ri(p + 8), h = ri(p + 12); s->clip(s->ctx, x, YR(y, h), w, h); }
            break;
        case R_UNCLIP: if (s->unclip) s->unclip(s->ctx); break;
        case R_GRAD:
            if (pl < 24) return -1;
            if (s->grad) { double x = ri(p), y = ri(p + 4), w = ri(p + 8), h = ri(p + 12); s->grad(s->ctx, x, YR(y, h), w, h, ru(p + 16), ru(p + 20)); }
            break;
        case R_IMAGE: case R_COMP_IMAGE: {
            uint32_t hdr = op == R_IMAGE ? 16 : 20;
            if (pl < hdr) return -1;
            int32_t x = ri(p), y = ri(p + 4), w = ri(p + 8), h = ri(p + 12);
            uint32_t iop = op == R_IMAGE ? 2 : ru(p + 16);
            if (w <= 0 || h <= 0 || (uint64_t)w * (uint64_t)h * 4 > pl - hdr) return -1;
            if (s->image) s->image(s->ctx, x, YR((double)y, (double)h), w, h, (const void *)(p + hdr), w, h, iop);
            break;
        }
        case R_COMP_FILL:
            if (pl < 24) return -1;
            if (s->comp_fill) { double x = rf(p), y = rf(p + 4), w = rf(p + 8), h = rf(p + 12); s->comp_fill(s->ctx, x, YR(y, h), w, h, ru(p + 16), ru(p + 20)); }
            break;
        default: break;   /* comando que o destino não conhece: pula */
        }
        off += len;
        count++;
    }
    #undef YR
    #undef YP
    return off == n ? count : -1;
}
