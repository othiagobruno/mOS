/*
 * mOS libSystem: AVMakeRectWithAspectRatioInsideRect, MTAudioProcessingTap e BNNSCopy,
 * que o WhatsApp 2.26.45.13 importa (machocheck, 10 nov. 2026).
 */
#ifdef WA_MEDIA_HOST
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint32_t u32;
#else
#include "libc.h"
#endif

typedef struct { double x, y, w, h; } wm_rect_t;
typedef struct { double w, h; } wm_size_t;

/* O maior retângulo com a proporção pedida que cabe em bounds, centralizado (AVFoundation). */
wm_rect_t AVMakeRectWithAspectRatioInsideRect(wm_size_t aspect, wm_rect_t b) {
    wm_rect_t r = b;
    if (aspect.w <= 0 || aspect.h <= 0 || b.w <= 0 || b.h <= 0) { r.w = r.h = 0; r.x = b.x + b.w / 2; r.y = b.y + b.h / 2; return r; }
    double s = b.w / aspect.w < b.h / aspect.h ? b.w / aspect.w : b.h / aspect.h;
    r.w = aspect.w * s; r.h = aspect.h * s;
    r.x = b.x + (b.w - r.w) / 2; r.y = b.y + (b.h - r.h) / 2;
    return r;
}

/* ---- MTAudioProcessingTap: o objeto guarda as callbacks e o storage que o init devolve.
 * O AVPlayer do mOS não passa o áudio por taps; fora de um pipeline, GetSourceAudio entrega
 * silêncio do tamanho pedido (é o que o tap recebe de uma faixa sem áudio decodificado). */
typedef struct {
    int version;
    void *clientInfo;
    void (*init)(void *tap, void *clientInfo, void **storage);
    void (*finalize)(void *tap);
    void (*prepare)(void *tap, long maxFrames, const void *asbd);
    void (*unprepare)(void *tap);
    void (*process)(void *tap, long frames, u32 flags, void *bufferList, long *framesOut, u32 *flagsOut);
} mt_cb_t;
typedef struct { u32 magic; mt_cb_t cb; void *storage; u32 flags; int refs; } mt_tap_t;
#define MT_MAGIC 0x4D545450u
int MTAudioProcessingTapCreate(void *alloc, const mt_cb_t *cb, u32 flags, void **out) {
    mt_tap_t *t;
    (void)alloc;
    if (out) *out = 0;
    if (!cb || !out || !cb->process) return -50;
    t = calloc(1, sizeof *t);
    if (!t) return -108;
    t->magic = MT_MAGIC; t->cb = *cb; t->flags = flags; t->refs = 1;
    if (t->cb.init) t->cb.init(t, t->cb.clientInfo, &t->storage);
    *out = t;
    return 0;
}
void *MTAudioProcessingTapGetStorage(void *tap) {
    mt_tap_t *t = tap;
    return t && t->magic == MT_MAGIC ? t->storage : 0;
}
typedef struct { u32 channels, bytes; void *data; } mt_abuf_t;
int MTAudioProcessingTapGetSourceAudio(void *tap, long frames, void *bufferList, u32 *flagsOut, void *timeRangeOut, long *framesOut) {
    mt_tap_t *t = tap;
    unsigned char *l = bufferList;
    if (flagsOut) *flagsOut = 0;
    if (framesOut) *framesOut = 0;
    if (timeRangeOut) memset(timeRangeOut, 0, 48);
    if (!t || t->magic != MT_MAGIC || !l || frames < 0) return -50;
    u32 n = *(u32 *)l;
    mt_abuf_t *ab = (mt_abuf_t *)(l + 8);
    for (u32 i = 0; i < n; i++) if (ab[i].data) memset(ab[i].data, 0, ab[i].bytes);
    if (framesOut) *framesOut = frames;
    return 0;
}

/* ---- BNNSCopy: cópia de BNNSNDArrayDescriptor do mesmo tipo e do mesmo número de elementos.
 * A posição do rank fica nos bits 16..19 do layout; o tamanho do elemento, no byte baixo do tipo. */
typedef struct {
    u32 flags, layout;
    size_t size[8], stride[8];
    void *data;
    u32 data_type;
    void *table_data;
    u32 table_data_type;
    float data_scale, data_bias;
} bnns_nd_t;
static size_t bnns_count(const bnns_nd_t *d) {
    u32 rank = (d->layout >> 16) & 0xF;
    size_t n = 1;
    if (rank == 0 || rank > 8) return 0;
    for (u32 i = 0; i < rank; i++) n *= d->size[i] ? d->size[i] : 1;
    return n;
}
static int bnns_contig(const bnns_nd_t *d) {
    u32 rank = (d->layout >> 16) & 0xF;
    for (u32 i = 0; i < rank && i < 8; i++) if (d->stride[i]) return 0;
    return 1;
}
int BNNSCopy(bnns_nd_t *dst, const bnns_nd_t *src, const void *params) {
    (void)params;
    if (!dst || !src || !dst->data || !src->data) return -1;
    size_t n = bnns_count(src), es = (src->data_type & 0xFF) / 8;
    if (!n || !es || n != bnns_count(dst) || src->data_type != dst->data_type) return -1;
    if (!bnns_contig(src) || !bnns_contig(dst)) return -1;
    memmove(dst->data, src->data, n * es);
    return 0;
}
