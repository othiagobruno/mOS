/* Teste de host do VTDecompressionSession (userland/libsystem/vscode_media.c compilado de verdade, com o OpenH264
 * pela casca cx_h264_*). O runtime em volta (classe ObjC, CMSampleBuffer, CVPixelBuffer, dicionário de atributos) é
 * substituído aqui pelo mínimo; as amostras vêm de um MP4 pelo demuxer do mOS (mp4demux.c), com pts e ctts.
 * uso: vt_test video.mp4 referencia.yuv(I420 na ordem de exibição) [bt709full]   → "vt ok ..." */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp4demux.h"
typedef int OSStatus;
typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cm_time_t;

/* ---------------- substitutos do runtime */
typedef struct cls_s { unsigned extra; } cls_t;
void *cx_define_class(void *cls, void *meta, const char *name, void *super, unsigned extra) { (void)meta; (void)name; (void)super; ((cls_t *)cls)->extra = extra; return cls; }
void cx_add_method(void *cls, const char *sel, void *imp) { (void)cls; (void)sel; (void)imp; }
void *cx_lookup(void *cls, const char *sel) { (void)cls; (void)sel; return 0; }
static cls_t nsobject;
void *cx_class_named(const char *n) { (void)n; return &nsobject; }
void cx_objc_boot(unsigned long long a, unsigned long long b) { (void)a; (void)b; }
void *cx_alloc_instance(void *cls) { void **o = calloc(1, 16 + ((cls_t *)cls)->extra); o[0] = cls; return o; }
const char *sel_registerName(const char *s) { return s; }
void objc_release(void *o) { (void)o; }
void *objc_retain(void *o) { return o; }
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }
cm_time_t CMTimeMake(long long v, int ts) { cm_time_t t = {v, ts, 1, 0}; return t; }
/* dicionário de atributos: um objeto que responde objectForKey:, e números/listas */
typedef struct { void *isa; int kind; long long num; int n; void *items[4]; } fobj_t;   /* kind 1 número, 2 lista, 3 dicionário */
const void *kCVPixelBufferPixelFormatTypeKey = "PixelFormatType";
const void *kCVImageBufferYCbCrMatrixKey = "M", *kCVImageBufferColorPrimariesKey = "P", *kCVImageBufferTransferFunctionKey = "T";
long objc_msgSend(void *self, const char *sel, ...) {
    fobj_t *o = self;
    __builtin_va_list ap;
    __builtin_va_start(ap, sel);
    long r = 0;
    if (!strcmp(sel, "objectForKey:")) { void *k = __builtin_va_arg(ap, void *); r = o->kind == 3 && k == kCVPixelBufferPixelFormatTypeKey ? (long)o->items[0] : 0; }
    else if (!strcmp(sel, "respondsToSelector:")) { const char *s2 = __builtin_va_arg(ap, const char *); r = o->kind == 2 && !strcmp(s2, "objectAtIndex:"); }
    else if (!strcmp(sel, "count")) r = o->n;
    else if (!strcmp(sel, "objectAtIndex:")) r = (long)o->items[__builtin_va_arg(ap, unsigned long)];
    else if (!strcmp(sel, "longLongValue")) r = (long)o->num;
    __builtin_va_end(ap);
    return r;
}
const void *CVYCbCrMatrixGetStringForIntegerCodePoint(int c) { return c == 1 ? "709" : c == 6 ? "601" : c == 9 ? "2020" : 0; }
const void *CVColorPrimariesGetStringForIntegerCodePoint(int c) { return c == 1 ? "709" : 0; }
const void *CVTransferFunctionGetStringForIntegerCodePoint(int c) { return c == 1 ? "709" : 0; }
/* CVPixelBuffer */
typedef struct { unsigned fmt; int w, h; unsigned char *p0, *p1; unsigned long b0, b1; const char *matrix; } pb_t;
int CVPixelBufferCreate(void *a, unsigned long long w, unsigned long long h, unsigned fmt, void *attrs, void **out) {
    (void)a; (void)attrs;
    pb_t *p = calloc(1, sizeof *p);
    p->fmt = fmt; p->w = (int)w; p->h = (int)h;
    if (fmt == 0x42475241u) { p->b0 = (w * 4 + 63) & ~63ul; p->p0 = calloc(p->b0, h); }
    else { p->b0 = (w + 63) & ~63ul; p->b1 = p->b0; p->p0 = calloc(p->b0, h); p->p1 = calloc(p->b1, (h + 1) / 2); }
    *out = p;
    return 0;
}
int CVPixelBufferLockBaseAddress(void *p, unsigned long long f) { (void)p; (void)f; return 0; }
int CVPixelBufferUnlockBaseAddress(void *p, unsigned long long f) { (void)p; (void)f; return 0; }
void *CVPixelBufferGetBaseAddress(void *p) { return ((pb_t *)p)->p0; }
unsigned long long CVPixelBufferGetBytesPerRow(void *p) { return ((pb_t *)p)->b0; }
void *CVPixelBufferGetBaseAddressOfPlane(void *p, unsigned long long i) { return i ? ((pb_t *)p)->p1 : ((pb_t *)p)->p0; }
unsigned long long CVPixelBufferGetBytesPerRowOfPlane(void *p, unsigned long long i) { return i ? ((pb_t *)p)->b1 : ((pb_t *)p)->b0; }
void CVBufferSetAttachment(void *p, void *key, void *val, unsigned mode) { (void)mode; if (key == kCVImageBufferYCbCrMatrixKey) ((pb_t *)p)->matrix = val; }
/* CMSampleBuffer / CMBlockBuffer / CMVideoFormatDescription */
typedef struct { void *isa; unsigned kind, media, subtype; const unsigned char *sps, *pps; unsigned long sps_n, pps_n; int nal; } fd_t;
typedef struct { fd_t *fd; unsigned char *data; unsigned long n; cm_time_t pts, dur; } sb_t;
void *CMSampleBufferGetDataBuffer(void *s) { return s; }
void *CMSampleBufferGetFormatDescription(void *s) { return ((sb_t *)s)->fd; }
cm_time_t CMSampleBufferGetPresentationTimeStamp(void *s) { return ((sb_t *)s)->pts; }
cm_time_t CMSampleBufferGetDuration(void *s) { return ((sb_t *)s)->dur; }
int CMBlockBufferGetDataPointer(void *b, unsigned long off, unsigned long *at, unsigned long *total, char **ptr) {
    sb_t *s = b; (void)off; if (at) *at = s->n; if (total) *total = s->n; *ptr = (char *)s->data; return 0;
}
int CMVideoFormatDescriptionGetH264ParameterSetAtIndex(void *d, unsigned long i, const unsigned char **p, unsigned long *sz, unsigned long *n, int *nal) {
    fd_t *f = d;
    if (i > 1) return -12710;
    *p = i ? f->pps : f->sps; *sz = i ? f->pps_n : f->sps_n;
    if (n) *n = 2;
    if (nal) *nal = f->nal;
    return 0;
}

/* ---------------- VideoToolbox */
typedef void (*vt_cb)(void *ref, void *frame_ref, OSStatus st, unsigned info, void *img, cm_time_t pts, cm_time_t dur);
OSStatus VTDecompressionSessionCreate(void *a, void *fd, void *spec, void *attrs, const void *cb, void **out);
OSStatus VTDecompressionSessionDecodeFrame(void *s, void *sb, unsigned flags, void *ref, unsigned *info);
OSStatus VTDecompressionSessionDecodeFrameWithOutputHandler(void *s, void *sb, unsigned flags, unsigned *info, void *handler);
OSStatus VTDecompressionSessionFinishDelayedFrames(void *s);
void VTDecompressionSessionInvalidate(void *s);

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)
static unsigned char *ref; static long nref; static int W, H; static long long *disp_pts; static int ns;
typedef struct { int n, bad_img, bad_pts, dropped, errors; long long last_pts; int out_of_order; unsigned fmt; int full709; int maxdiff; void *sb_expected; } col_t;
static int disp_index(long long pts) { for (int i = 0; i < ns; i++) if (disp_pts[i] == pts) return i; return -1; }
static int clampi(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static void check_img(col_t *c, pb_t *p, long long pts) {
    int k = disp_index(pts);
    if (k < 0 || p->w != W || p->h != H) { c->bad_pts++; return; }
    const unsigned char *Y = ref + (long)k * W * H * 3 / 2, *U = Y + W * H, *V = U + W * H / 4;
    if (p->fmt == 0x34323076u) {   /* '420v' de fluxo em faixa de vídeo: cópia exata */
        for (int r = 0; r < H; r++) if (memcmp(p->p0 + r * p->b0, Y + r * W, W)) { c->bad_img++; return; }
        for (int r = 0; r < H / 2; r++) for (int x = 0; x < W / 2; x++)
            if (p->p1[r * p->b1 + 2 * x] != U[r * W / 2 + x] || p->p1[r * p->b1 + 2 * x + 1] != V[r * W / 2 + x]) { c->bad_img++; return; }
    } else if (p->fmt == 0x34323066u) {   /* '420f' de faixa de vídeo: Y' = (Y-16)*255/219 */
        for (int r = 0; r < H; r++) for (int x = 0; x < W; x++) {
            int e = clampi((int)lround((Y[r * W + x] - 16) * 255.0 / 219.0)), d = abs(p->p0[r * p->b0 + x] - e);
            if (d > c->maxdiff) c->maxdiff = d;
        }
    } else {   /* BGRA: fórmula em ponto flutuante com a matriz e a faixa do fluxo */
        int m709 = c->full709 || H >= 720;   /* sem matriz no VUI: HD em BT.709, SD em BT.601 (regra do macOS) */
        double kr = m709 ? 0.2126 : 0.299, kb = m709 ? 0.0722 : 0.114;
        for (int r = 0; r < H; r++) for (int x = 0; x < W; x++) {
            double y = Y[r * W + x], cb = U[(r / 2) * W / 2 + x / 2] - 128.0, cr = V[(r / 2) * W / 2 + x / 2] - 128.0, ys = 1, cs = 1;
            if (!c->full709) { y -= 16; ys = 255.0 / 219; cs = 255.0 / 224; }
            double R = y * ys + 2 * (1 - kr) * cs * cr, B = y * ys + 2 * (1 - kb) * cs * cb, G = (y * ys - kr * R - kb * B) / (1 - kr - kb);
            const unsigned char *q = p->p0 + r * p->b0 + 4 * x;
            int d = abs(q[0] - clampi((int)lround(B))); if (abs(q[1] - clampi((int)lround(G))) > d) d = abs(q[1] - clampi((int)lround(G)));
            if (abs(q[2] - clampi((int)lround(R))) > d) d = abs(q[2] - clampi((int)lround(R)));
            if (q[3] != 255) d = 255;
            if (d > c->maxdiff) c->maxdiff = d;
        }
        if (!p->matrix || strcmp(p->matrix, m709 ? "709" : "601")) c->bad_img++;
    }
}
static void on_frame(void *ref_, void *frame_ref, OSStatus st, unsigned info, void *img, cm_time_t pts, cm_time_t dur) {
    col_t *c = ref_; (void)dur;
    if (st) { c->errors++; return; }
    if (info & 2) { c->dropped++; if (frame_ref && ((sb_t *)frame_ref)->pts.value != pts.value) c->bad_pts++; return; }
    if (c->n && pts.value < c->last_pts) c->out_of_order++;
    c->last_pts = pts.value;
    c->n++;
    if (frame_ref && ((sb_t *)frame_ref)->pts.value != pts.value) c->bad_pts++;   /* o refCon é o sample buffer daquela amostra */
    if (img) check_img(c, img, pts.value); else c->bad_img++;
}
typedef struct blk { void *isa; int flags, res; void (*invoke)(struct blk *, OSStatus, unsigned, void *, cm_time_t, cm_time_t); col_t *c; } blk_t;
static void blk_inv(blk_t *b, OSStatus st, unsigned info, void *img, cm_time_t pts, cm_time_t dur) { on_frame(b->c, 0, st, info, img, pts, dur); }

static sb_t *samples; static fd_t fd;
static int run(const char *nome, unsigned flags, unsigned fmt, int use_attrs, int handler, col_t *c) {
    void *s = 0;
    fobj_t num = {0, 1, fmt, 0, {0}}, other = {0, 1, 0x79343230, 0, {0}}, arr = {0, 2, 0, 2, {&other, &num}}, dict = {0, 3, 0, 0, {&arr}};
    struct { vt_cb fn; void *ref; } rec = {on_frame, c};
    if (VTDecompressionSessionCreate(0, &fd, 0, use_attrs ? &dict : 0, handler ? 0 : &rec, &s)) { printf("FALHA %s: Create\n", nome); fails++; return -1; }
    c->fmt = fmt;
    static blk_t blks[4096];
    for (int i = 0; i < ns; i++) {
        unsigned info = 0;
        OSStatus r;
        if (handler) { blks[i] = (blk_t){0, 0, 0, blk_inv, c}; r = VTDecompressionSessionDecodeFrameWithOutputHandler(s, &samples[i], flags, &info, &blks[i]); }
        else r = VTDecompressionSessionDecodeFrame(s, &samples[i], flags, &samples[i], &info);
        if (r) { printf("FALHA %s: DecodeFrame %d -> %d\n", nome, i, r); fails++; break; }
        if (!(flags & 8) && c->n + c->dropped != i + 1) { printf("FALHA %s: amostra %d não saiu na própria chamada\n", nome, i); fails++; break; }
    }
    VTDecompressionSessionFinishDelayedFrames(s);
    VTDecompressionSessionInvalidate(s);
    return 0;
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) return 2; fclose(f);
    f = fopen(argv[2], "rb"); fseek(f, 0, SEEK_END); nref = ftell(f); fseek(f, 0, SEEK_SET);
    ref = malloc(nref); if (fread(ref, 1, nref, f) != (size_t)nref) return 2; fclose(f);
    int full709 = argc > 3;
    mp4_file m;
    if (mp4_open_mem(&m, b, n)) { puts("FALHA mp4"); return 1; }
    mp4_track *t = &m.t[mp4_find_track(&m, MP4_FOURCC('v', 'i', 'd', 'e'))];
    W = (int)t->width; H = (int)t->height; ns = (int)t->nsamples;
    /* avcC: versão, perfil, compat, nível, 6 bits + tamanho da NAL - 1, 3 bits + número de SPS, ... */
    const unsigned char *a = t->cfg;
    fd = (fd_t){0, 0, MP4_FOURCC('v', 'i', 'd', 'e'), MP4_FOURCC('a', 'v', 'c', '1'), a + 8, 0, (unsigned long)(a[6] << 8 | a[7]), 0, (a[4] & 3) + 1};
    const unsigned char *pp = a + 8 + fd.sps_n;
    fd.pps_n = (unsigned long)(pp[1] << 8 | pp[2]); fd.pps = pp + 3;
    samples = calloc(ns, sizeof *samples);
    disp_pts = calloc(ns, sizeof *disp_pts);
    for (int i = 0; i < ns; i++) {
        samples[i].fd = &fd; samples[i].n = t->s[i].size; samples[i].data = malloc(t->s[i].size);
        mp4_read_sample(&m, t, i, samples[i].data);
        samples[i].pts = (cm_time_t){t->s[i].dts + t->s[i].cto, (int)t->timescale, 1, 0};
        samples[i].dur = (cm_time_t){t->s[i].dur, (int)t->timescale, 1, 0};
        disp_pts[i] = samples[i].pts.value;
    }
    for (int i = 1; i < ns; i++) for (int j = i; j > 0 && disp_pts[j - 1] > disp_pts[j]; j--) { long long x = disp_pts[j]; disp_pts[j] = disp_pts[j - 1]; disp_pts[j - 1] = x; }
    if (nref != (long)ns * W * H * 3 / 2) { printf("FALHA referência com %ld bytes para %d quadros\n", nref, ns); return 1; }
    int reordered = 0;
    for (int i = 1; i < ns; i++) if (samples[i].pts.value < samples[i - 1].pts.value) reordered = 1;
    col_t c1 = {0}, c2 = {0}, c3 = {0}, c4 = {0}, c5 = {0};
    c3.full709 = c4.full709 = full709;
    if (!full709) {
        run("ordem de decodificação 420v", 0, 0x34323076u, 0, 0, &c1);
        CHECK(c1.n == ns && !c1.bad_img && !c1.bad_pts && !c1.errors && (!reordered || c1.out_of_order), "decodificação 420v: %d/%d quadros, imagem %d, pts %d, erros %d", c1.n, ns, c1.bad_img, c1.bad_pts, c1.errors);
        run("ordem de exibição 420v", 8, 0x34323076u, 1, 0, &c2);
        CHECK(c2.n == ns && !c2.bad_img && !c2.bad_pts && !c2.out_of_order && !c2.errors, "exibição 420v: %d/%d quadros, imagem %d, pts %d, fora de ordem %d", c2.n, ns, c2.bad_img, c2.bad_pts, c2.out_of_order);
        run("420f", 0, 0x34323066u, 1, 1, &c5);
        CHECK(c5.n == ns && c5.maxdiff <= 1 && !c5.errors, "420f pelo output handler: %d quadros, diferença máxima %d", c5.n, c5.maxdiff);
        col_t c6 = {0};
        run("DoNotOutputFrame", 2, 0x34323076u, 0, 0, &c6);
        CHECK(c6.n == 0 && c6.dropped == ns && !c6.bad_pts, "DoNotOutputFrame: %d imagens, %d descartados", c6.n, c6.dropped);
    }
    run("BGRA", 0, 0x42475241u, 1, 0, &c3);
    CHECK(c3.n == ns && c3.maxdiff <= 1 && !c3.bad_img, "BGRA (%s): %d quadros, diferença máxima %d, matriz %d", full709 ? "BT.709 faixa cheia" : H >= 720 ? "BT.709 faixa de vídeo" : "BT.601 faixa de vídeo", c3.n, c3.maxdiff, c3.bad_img);
    (void)c4;
    if (!fails) printf("vt ok %d amostras%s: %s\n", ns, reordered ? " com quadros B" : "",
                       full709 ? "BGRA BT.709 faixa cheia" : "ordem de decodificação e de exibição bit-exatas, pts/refCon, 420f, BGRA, DoNotOutputFrame, output handler");
    return fails != 0;
}
