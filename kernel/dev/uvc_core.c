/* mOS — núcleo UVC (ver uvc_core.h). Especificação: USB Device Class Definition for Video Devices 1.5
 * (descritores 3.9, controle PROBE/COMMIT 4.3.1.1, cabeçalho de payload 2.4.3.3). Código original do mOS (MIT). */
#include "uvc_core.h"

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

#define CS_INTERFACE 0x24
enum { VS_INPUT_HEADER = 0x01, VS_FORMAT_UNCOMPRESSED = 0x04, VS_FRAME_UNCOMPRESSED = 0x05, VS_FORMAT_MJPEG = 0x06, VS_FRAME_MJPEG = 0x07 };

int uvc_parse_vs(const uint8_t *d, uint32_t len, uvc_stream_info *out) {
    if (!d || !out) return -1;
    for (uint32_t i = 0; i < sizeof *out; i++) ((uint8_t *)out)[i] = 0;
    uvc_format *cur = 0;
    for (uint32_t o = 0; o + 3 <= len;) {
        uint8_t bl = d[o];
        if (bl < 3 || o + bl > len) break;
        const uint8_t *p = d + o;
        o += bl;
        if (p[1] != CS_INTERFACE) continue;
        switch (p[2]) {
        case VS_INPUT_HEADER: if (bl >= 9) out->terminal_link = p[8]; break;
        case VS_FORMAT_UNCOMPRESSED: case VS_FORMAT_MJPEG:
            if (out->nformat >= UVC_MAX_FORMATS || bl < 11) { cur = 0; break; }
            cur = &out->format[out->nformat++];
            cur->index = p[3];
            if (p[2] == VS_FORMAT_MJPEG) { cur->kind = UVC_FMT_MJPEG; cur->fourcc = 0x47504a4d; /* "MJPG" */ cur->def_frame = p[6]; }
            else {
                if (bl < 27) { out->nformat--; cur = 0; break; }
                cur->fourcc = le32(p + 5);
                cur->kind = cur->fourcc == 0x32595559 ? UVC_FMT_YUY2 : cur->fourcc == 0x3231564e ? UVC_FMT_NV12 : UVC_FMT_OTHER;
                cur->bpp = p[21];
                cur->def_frame = p[22];
            }
            break;
        case VS_FRAME_UNCOMPRESSED: case VS_FRAME_MJPEG: {
            if (!cur || cur->nframe >= UVC_MAX_FRAMES || bl < 26) break;
            if ((p[2] == VS_FRAME_MJPEG) != (cur->kind == UVC_FMT_MJPEG)) break;
            uvc_frame *f = &cur->frame[cur->nframe++];
            f->index = p[3];
            f->width = le16(p + 5); f->height = le16(p + 7);
            f->max_size = le32(p + 17);
            f->def_interval = le32(p + 21);
            uint8_t type = p[25];
            if (type == 0) {   /* contínuo: mínimo, máximo, passo */
                if (bl >= 38) { f->interval[0] = le32(p + 26); f->interval[1] = le32(p + 30); f->ninterval = 2; }
            } else {
                for (int k = 0; k < type && k < UVC_MAX_INTERVALS && 26 + 4 * k + 4 <= bl; k++) f->interval[f->ninterval++] = le32(p + 26 + 4 * k);
            }
            if (!f->max_size && cur->kind != UVC_FMT_MJPEG) f->max_size = (uint32_t)f->width * f->height * (cur->bpp ? cur->bpp : 16) / 8;
            break;
        }
        default: break;
        }
    }
    return out->nformat ? 0 : -1;
}
uint16_t uvc_parse_vc_version(const uint8_t *d, uint32_t len) {
    for (uint32_t o = 0; o + 5 <= len;) {
        uint8_t bl = d[o];
        if (bl < 3 || o + bl > len) break;
        if (d[o + 1] == CS_INTERFACE && d[o + 2] == 0x01 && bl >= 5) return le16(d + o + 3);   /* VC_HEADER */
        o += bl;
    }
    return 0;
}
int uvc_probe_len(uint16_t bcd) { return bcd >= 0x0150 ? 48 : bcd >= 0x0110 ? 34 : 26; }
void uvc_probe_pack(const uvc_probe *p, uint8_t *b, int len) {
    for (int i = 0; i < len; i++) b[i] = 0;
    put16(b, p->hint);
    b[2] = p->format; b[3] = p->frame;
    put32(b + 4, p->interval);
    put32(b + 18, p->max_frame_size);
    put32(b + 22, p->max_payload);
    if (len >= 30) put32(b + 26, p->clock_hz);
    if (len >= 31) b[30] = 0x03;   /* bmFramingInfo: FID e EOF obrigatórios */
}
void uvc_probe_unpack(const uint8_t *b, int len, uvc_probe *p) {
    p->hint = le16(b); p->format = b[2]; p->frame = b[3];
    p->interval = le32(b + 4);
    p->max_frame_size = len >= 22 ? le32(b + 18) : 0;
    p->max_payload = len >= 26 ? le32(b + 22) : 0;
    p->clock_hz = len >= 30 ? le32(b + 26) : 0;
}
int uvc_pick_alt(const uint32_t *bw, int n, uint32_t need) {
    int best = -1;
    for (int i = 0; i < n; i++) if (bw[i] >= need && (best < 0 || bw[i] < bw[best])) best = i;
    if (best < 0) for (int i = 0; i < n; i++) if (best < 0 || bw[i] > bw[best]) best = i;   /* nenhuma basta: a maior */
    return best;
}

void uvc_asm_init(uvc_asm *a, uint8_t *buf, uint32_t cap, uint32_t expect, uvc_frame_cb cb, void *ctx) {
    a->buf = buf; a->cap = cap; a->len = 0; a->fid = 0; a->have_fid = 0; a->bad = 0;
    a->expect = expect; a->cb = cb; a->ctx = ctx; a->frames = 0; a->dropped = 0;
}
static void finish(uvc_asm *a) {
    int ok = a->len && !a->bad && (!a->expect || a->len == a->expect);
    if (ok && !a->expect && (a->len < 4 || a->buf[0] != 0xFF || a->buf[1] != 0xD8)) ok = 0;   /* MJPEG: começa com SOI */
    if (ok) { a->frames++; if (a->cb) a->cb(a->ctx, a->buf, a->len, a->fid); }
    else if (a->len || a->bad) a->dropped++;
    a->len = 0; a->bad = 0;
}
void uvc_asm_payload(uvc_asm *a, const uint8_t *p, uint32_t n) {
    if (n < 2) return;
    uint8_t hl = p[0], info = p[1];
    if (hl < 2 || hl > n || hl > 12) { a->bad = 1; return; }   /* cabeçalho inválido (máximo: 1+1+PTS 4+SCR 6) */
    int fid = info & 1;
    if (a->have_fid && fid != a->fid && (a->len || a->bad)) finish(a);   /* FID trocou: o quadro anterior acabou */
    a->fid = fid; a->have_fid = 1;
    if (info & 0x40) a->bad = 1;   /* ERR */
    uint32_t dl = n - hl;
    if (dl) {
        if (a->len + dl > a->cap) a->bad = 1;
        else { for (uint32_t i = 0; i < dl; i++) a->buf[a->len + i] = p[hl + i]; a->len += dl; }
    }
    if (info & 0x02) finish(a);   /* EOF */
}
