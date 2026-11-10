/* Ponte para o decodificador h264bsd (Apache-2.0, Android Open Source).
 * Unidade separada: os typedefs u8/u32 de basetype.h não entram no libSystem. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "h264bsd_decoder.h"

int h264_bsd_open(void **out) {
    storage_t *st;
    if (!out) return -1;
    *out = 0;
    st = h264bsdAlloc();
    if (!st) return -1;
    if (h264bsdInit(st, 1) != 0) {
        h264bsdFree(st);
        return -1;
    }
    *out = st;
    return 0;
}

void h264_bsd_close(void *p) {
    storage_t *st = (storage_t *)p;
    if (!st) return;
    h264bsdShutdown(st);
    h264bsdFree(st);
}

static int copy_pic(storage_t *st, const u8 *pic,
                    uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv,
                    int *ow, int *oh) {
    u32 crop = 0, left = 0, cw = 0, top = 0, ch = 0;
    u32 mbw = h264bsdPicWidth(st);
    u32 mbh = h264bsdPicHeight(st);
    int fw, fh, x0, y0, stride, cstride, row;
    const u8 *Y, *U, *V;
    if (!pic || !mbw || !mbh) return -1;
    h264bsdCroppingParams(st, &crop, &left, &cw, &top, &ch);
    fw = crop ? (int)cw : (int)mbw * 16;
    fh = crop ? (int)ch : (int)mbh * 16;
    x0 = crop ? (int)left : 0;
    y0 = crop ? (int)top : 0;
    if (fw < 2 || fh < 2 || (fw & 1) || (fh & 1)) return -1;
    stride = (int)mbw * 16;
    cstride = stride / 2;
    Y = pic;
    U = pic + (size_t)stride * mbh * 16;
    V = U + (size_t)cstride * (mbh * 8);
    for (row = 0; row < fh; row++)
        memcpy(y + row * sy, Y + (size_t)(y0 + row) * stride + x0, (size_t)fw);
    for (row = 0; row < fh / 2; row++) {
        memcpy(u + row * su, U + (size_t)(y0 / 2 + row) * cstride + x0 / 2, (size_t)(fw / 2));
        memcpy(v + row * sv, V + (size_t)(y0 / 2 + row) * cstride + x0 / 2, (size_t)(fw / 2));
    }
    if (ow) *ow = fw;
    if (oh) *oh = fh;
    return 0;
}

int h264_bsd_feed(void *p, const uint8_t *bytes, int n,
                  uint8_t *y, uint8_t *u, uint8_t *v, int sy, int su, int sv,
                  int *ow, int *oh) {
    storage_t *st = (storage_t *)p;
    const u8 *cur;
    u32 left;
    u8 *pic = 0;
    int guard = 0, zeros = 0;
    if (!st || !bytes || n < 1) return -1;
    cur = (const u8 *)bytes;
    left = (u32)n;
    while (left && guard++ < 1000000) {
        u32 read = 0;
        u32 ret = h264bsdDecode(st, (u8 *)cur, left, 0, &read);
        if (!read) {
            if (ret == H264BSD_HDRS_RDY && zeros++ < 4) continue;
            return -1;
        }
        zeros = 0;
        if (read > left) return -1;
        cur += read;
        left -= read;
        if (ret == H264BSD_PIC_RDY) {
            u32 id = 0, idr = 0, err = 0;
            u8 *out = h264bsdNextOutputPicture(st, &id, &idr, &err);
            if (out) pic = out;
        } else if (ret == H264BSD_ERROR || ret == H264BSD_PARAM_SET_ERROR ||
                   ret == H264BSD_MEMALLOC_ERROR) {
            return -1;
        }
    }
    if (!pic || !y) return pic ? 0 : 1;
    return copy_pic(st, pic, y, u, v, sy, su, sv, ow, oh);
}

int h264_bsd_decode_all(const uint8_t *bytes, int n, uint8_t **yuv, int *frames, int *ow, int *oh) {
    storage_t *st;
    const u8 *cur;
    u32 left;
    int guard = 0, zeros = 0, nfr = 0, w = 0, h = 0, cap = 0;
    uint8_t *buf = 0;
    if (yuv) *yuv = 0;
    if (frames) *frames = 0;
    if (!bytes || n < 1) return -1;
    st = h264bsdAlloc();
    if (!st || h264bsdInit(st, 1) != 0) {
        h264bsdFree(st);
        return -1;
    }
    cur = (const u8 *)bytes;
    left = (u32)n;
    while (left && guard++ < 1000000) {
        u32 read = 0;
        u32 ret = h264bsdDecode(st, (u8 *)cur, left, 0, &read);
        if (!read) {
            if (ret == H264BSD_HDRS_RDY && zeros++ < 4) continue;
            free(buf);
            h264bsdShutdown(st);
            h264bsdFree(st);
            return -1;
        }
        zeros = 0;
        if (read > left) {
            free(buf);
            h264bsdShutdown(st);
            h264bsdFree(st);
            return -1;
        }
        cur += read;
        left -= read;
        if (ret == H264BSD_PIC_RDY) {
            u32 id = 0, idr = 0, err = 0, crop = 0, lo = 0, cw = 0, to = 0, ch = 0;
            u8 *pic = h264bsdNextOutputPicture(st, &id, &idr, &err);
            int fw, fh, sz;
            uint8_t *tmpy, *tmpu, *tmpv, *grow;
            if (!pic) continue;
            h264bsdCroppingParams(st, &crop, &lo, &cw, &to, &ch);
            fw = crop ? (int)cw : (int)h264bsdPicWidth(st) * 16;
            fh = crop ? (int)ch : (int)h264bsdPicHeight(st) * 16;
            if (!w) { w = fw; h = fh; }
            if (fw != w || fh != h) {
                free(buf);
                h264bsdShutdown(st);
                h264bsdFree(st);
                return -1;
            }
            sz = w * h * 3 / 2;
            if (nfr + 1 > cap) {
                cap = cap ? cap * 2 : 8;
                grow = realloc(buf, (size_t)cap * (size_t)sz);
                if (!grow) {
                    free(buf);
                    h264bsdShutdown(st);
                    h264bsdFree(st);
                    return -1;
                }
                buf = grow;
            }
            tmpy = buf + (size_t)nfr * (size_t)sz;
            tmpu = tmpy + w * h;
            tmpv = tmpu + w * h / 4;
            if (copy_pic(st, pic, tmpy, tmpu, tmpv, w, w / 2, w / 2, 0, 0)) {
                free(buf);
                h264bsdShutdown(st);
                h264bsdFree(st);
                return -1;
            }
            nfr++;
        } else if (ret == H264BSD_ERROR || ret == H264BSD_PARAM_SET_ERROR ||
                   ret == H264BSD_MEMALLOC_ERROR) {
            free(buf);
            h264bsdShutdown(st);
            h264bsdFree(st);
            return -1;
        }
    }
    h264bsdShutdown(st);
    h264bsdFree(st);
    if (!nfr) { free(buf); return -1; }
    if (yuv) *yuv = buf; else free(buf);
    if (frames) *frames = nfr;
    if (ow) *ow = w;
    if (oh) *oh = h;
    return 0;
}
