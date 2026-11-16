/*
 * mOS ImageIO — WebP. O decodificador é o do motor web (kernel/web/webp.c, frente do Fabio),
 * compilado aqui sem mudança: kshim/web.h e kshim/gfx.h dão tipos, malloc e surface_t.
 * Unidade separada porque webp.c tem estáticos (le16, le32...) com os mesmos nomes dos codecs do imageio.c.
 * Exporta webp_decode_argb (ARGB reto) e iio_webp_info (tela, quadros, alfa, repetições, sem decodificar).
 */
#include "web.h"
#include "../../kernel/web/webp.c"
#include "iio.h"

static u32 wle32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
/* 0 = ok. Lê só os blocos RIFF: VP8X (tela, alfa, animação), ANIM (repetições), ANMF (quadros). */
int iio_webp_info(const u8 *d, usize n, int *w, int *h, int *frames, int *alpha, int *loop, int *lossless) {
    if (n < 20 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WEBP", 4)) return -1;
    usize p = 12, end = 8 + (usize)wle32(d + 4);
    if (end > n) end = n;
    *w = *h = 0; *frames = 0; *alpha = 0; *loop = -1; *lossless = 0;
    while (p + 8 <= end) {
        const u8 *c = d + p;
        usize len = wle32(c + 4), body = p + 8;
        if (body + len > end) len = end - body;
        if (!memcmp(c, "VP8X", 4) && len >= 10) {
            *alpha = (c[8] & 0x10) != 0;
            *w = (int)(1 + (c[12] | c[13] << 8 | (u32)c[14] << 16));
            *h = (int)(1 + (c[15] | c[16] << 8 | (u32)c[17] << 16));
        } else if (!memcmp(c, "ANIM", 4) && len >= 6) {
            *loop = c[12] | c[13] << 8;
        } else if (!memcmp(c, "ANMF", 4)) {
            (*frames)++;
        } else if (!memcmp(c, "VP8L", 4) && len >= 5) {
            *lossless = 1;
            if (!*w) {
                u32 b = wle32(c + 9);
                *w = (int)(1 + (b & 0x3FFF));
                *h = (int)(1 + ((b >> 14) & 0x3FFF));
                *alpha = (b >> 28) & 1;
            }
        } else if (!memcmp(c, "VP8 ", 4) && len >= 10 && !*w) {
            *w = (c[14] | c[15] << 8) & 0x3FFF;
            *h = (c[16] | c[17] << 8) & 0x3FFF;
        } else if (!memcmp(c, "ALPH", 4)) {
            *alpha = 1;
        }
        p = body + len + (len & 1);
    }
    if (!*frames) *frames = 1;
    return *w > 0 && *h > 0 ? 0 : -1;
}

/* mistura "source over" de alfa reto */
static u32 wover(u32 dst, u32 src) {
    u32 sa = src >> 24, da = dst >> 24;
    if (sa == 255 || da == 0) return src;
    if (sa == 0) return dst;
    u32 oa = sa + da * (255 - sa) / 255, o = oa << 24;
    for (int s = 0; s < 24; s += 8) {
        u32 c = ((src >> s & 255) * sa + (dst >> s & 255) * da * (255 - sa) / 255) / oa;
        o |= (c > 255 ? 255 : c) << s;
    }
    return o;
}
static void wput32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }

/* Todos os quadros. Estático: um quadro. Animado: cada ANMF vira um WebP avulso (VP8X + ALPH/VP8/VP8L)
 * para webp_decode_argb, composto na tela com a mistura e a disposição do quadro. */
int iio_webp(const u8 *d, usize n, iio_image *out) {
    int W, H, nf, alpha, loop, lossless;
    if (iio_webp_info(d, n, &W, &H, &nf, &alpha, &loop, &lossless)) return -1;
    if ((u64)W * H > (1u << 27)) return -1;
    memset(out, 0, sizeof *out);
    out->loop_count = loop;
    out->srgb_intent = -1;
    out->uti = "org.webmproject.webp";
    usize end = 8 + (usize)wle32(d + 4);
    if (end > n) end = n;
    for (usize p = 12; p + 8 <= end;) {          /* ICCP do VP8X */
        usize len = wle32(d + p + 4);
        if (!memcmp(d + p, "ICCP", 4) && p + 8 + len <= end && !out->icc) {
            out->icc = malloc(len);
            if (out->icc) { memcpy(out->icc, d + p + 8, len); out->icc_n = len; }
        }
        p += 8 + len + (len & 1);
    }
    int anim = 0;
    for (usize p = 12; p + 8 <= end;) {
        usize len = wle32(d + p + 4);
        if (!memcmp(d + p, "ANMF", 4)) anim = 1;
        p += 8 + len + (len & 1);
    }
    if (!anim) {
        int w, h;
        u32 *px = webp_decode_argb(d, n, &w, &h);
        if (!px) return -1;
        out->f = calloc(1, sizeof(iio_frame));
        if (!out->f) { free(px); return -1; }
        out->f[0] = (iio_frame){px, w, h, 0, 8, IIO_RGB, alpha, 0, 1, 0, 0, 0, 0};
        out->nframes = 1;
        out->w = w;
        out->h = h;
        return 0;
    }
    u32 *canvas = calloc((usize)W * H, 4);
    out->f = calloc((usize)nf, sizeof(iio_frame));
    if (!canvas || !out->f) { free(canvas); free(out->f); out->f = NULL; return -1; }
    for (usize p = 12; p + 8 <= end && out->nframes < nf;) {
        usize len = wle32(d + p + 4), body = p + 8;
        if (body + len > end) len = end - body;
        if (!memcmp(d + p, "ANMF", 4) && len >= 16) {
            const u8 *f = d + body;
            int fx = 2 * (int)(f[0] | f[1] << 8 | (u32)f[2] << 16), fy = 2 * (int)(f[3] | f[4] << 8 | (u32)f[5] << 16);
            int fw = 1 + (int)(f[6] | f[7] << 8 | (u32)f[8] << 16), fh = 1 + (int)(f[9] | f[10] << 8 | (u32)f[11] << 16);
            int dur = (int)(f[12] | f[13] << 8 | (u32)f[14] << 16), noblend = (f[15] >> 1) & 1, dispose = f[15] & 1;
            usize sub = len - 16;
            /* WebP avulso: RIFF + VP8X (alfa se houver ALPH ou VP8L) + os sub-blocos do quadro */
            usize tot = 12 + 18 + sub;
            u8 *one = malloc(tot);
            if (!one) break;
            memcpy(one, "RIFF", 4); wput32(one + 4, (u32)(tot - 8)); memcpy(one + 8, "WEBP", 4);
            memcpy(one + 12, "VP8X", 4); wput32(one + 16, 10);
            memset(one + 20, 0, 10);
            one[20] = 0x10;
            u32 cw = (u32)fw - 1, chh = (u32)fh - 1;
            one[24] = (u8)cw; one[25] = (u8)(cw >> 8); one[26] = (u8)(cw >> 16);
            one[27] = (u8)chh; one[28] = (u8)(chh >> 8); one[29] = (u8)(chh >> 16);
            memcpy(one + 30, f + 16, sub);
            int w, h;
            u32 *px = webp_decode_argb(one, tot, &w, &h);
            free(one);
            if (px) {
                for (int y = 0; y < h && fy + y < H; y++)
                    for (int x = 0; x < w && fx + x < W; x++) {
                        u32 *dp = &canvas[(usize)(fy + y) * W + fx + x], s = px[(usize)y * w + x];
                        *dp = noblend ? s : wover(*dp, s);
                    }
                free(px);
                iio_frame *o = &out->f[out->nframes];
                o->px = malloc((usize)W * H * 4);
                if (!o->px) break;
                memcpy(o->px, canvas, (usize)W * H * 4);
                *o = (iio_frame){o->px, W, H, dur, 8, IIO_RGB, 1, 0, 1, 0, 0, 0, 0};
                out->nframes++;
                if (dispose)
                    for (int y = 0; y < fh && fy + y < H; y++)
                        for (int x = 0; x < fw && fx + x < W; x++) canvas[(usize)(fy + y) * W + fx + x] = 0;
            }
        }
        p = body + len + (len & 1);
    }
    free(canvas);
    if (!out->nframes) { free(out->f); out->f = NULL; return -1; }
    out->w = W;
    out->h = H;
    return 0;
}
