/* Teste de host de userland/libsystem/avplayervideo.c (AVPlayerItemVideoOutput e AVPlayerLayer) com o runtime em volta
 * substituído: o item e o relógio do AVPlayer são controlados aqui (cx_avitem_seconds), o vídeo é um MP4 de verdade.
 * uso: avplayervideo_test video.mp4 referencia.yuv  → "avplayervideo ok ..." */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avvideo.h"
typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cmt_t;
typedef struct { double x, y, w, h; } rect_t;
extern char AVPlayerItemVideoOutput_c[] __asm__("OBJC_CLASS_$_AVPlayerItemVideoOutput");
extern char AVPlayerLayer_c[] __asm__("OBJC_CLASS_$_AVPlayerLayer");
extern char MOSAVPlayerLayerState_c[] __asm__("OBJC_CLASS_$_MOSAVPlayerLayerState");
extern char SBDL_c[] __asm__("OBJC_CLASS_$_AVSampleBufferDisplayLayer");
extern char MOSSBDLState_c[] __asm__("OBJC_CLASS_$_MOSSBDLState");
/* ---- runtime mínimo */
typedef struct cls_s { void *key; unsigned extra; void *imps[64]; const char *sels[64]; int n; struct cls_s *super; } cls_t;
static cls_t tab[16]; static int ntab;
static char ns_object_k, ca_layer_k;   /* chaves das classes do runtime de verdade (CALayer, NSObject) */
static cls_t *K(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; tab[ntab].key = c; return &tab[ntab++]; }
#define ns_object (*(char *)&ns_object_k)
#define ca_layer (*(char *)&ca_layer_k)
void *cx_class_named(const char *n) { return !strcmp(n, "CALayer") ? (void *)&ca_layer_k : (void *)&ns_object_k; }
void *cx_define_class(void *c, void *m, const char *name, void *super, unsigned extra) { (void)m; (void)name; K(c)->extra = extra; K(c)->super = K(super); return c; }
static void add(void *c, const char *s, void *f) { cls_t *k = K(c); k->sels[k->n] = s; k->imps[k->n++] = f; }
void cx_add_method(void *c, const char *s, void *f) { add(c, s, f); }
void cx_add_cmethod(void *c, const char *s, void *f) { add(c, s, f); }
void *cx_alloc_instance(void *c) { void **o = calloc(1, 16 + K(c)->extra + 64); o[0] = c; return o; }
static void *imp(void *c, const char *s) { for (cls_t *k = K(c); k; k = k->super) for (int i = 0; i < k->n; i++) if (!strcmp(k->sels[i], s)) return k->imps[i]; return 0; }
void *class_getMethodImplementation(void *c, const char *s) { (void)c; (void)s; return 0; }
const char *sel_registerName(const char *s) { return s; }
void *objc_retain(void *o) { return o; }
void objc_release(void *o) { (void)o; }
void *objc_autorelease(void *o) { return o; }
/* associated objects: tabela simples */
static struct { void *o; const void *k; void *v; } assoc[64]; static int nassoc;
void objc_setAssociatedObject(void *o, const void *k, void *v, int pol) { (void)pol; for (int i = 0; i < nassoc; i++) if (assoc[i].o == o && assoc[i].k == k) { assoc[i].v = v; return; } assoc[nassoc].o = o; assoc[nassoc].k = k; assoc[nassoc++].v = v; }
void *objc_getAssociatedObject(void *o, const void *k) { for (int i = 0; i < nassoc; i++) if (assoc[i].o == o && assoc[i].k == k) return assoc[i].v; return 0; }
const char *cx_cstr(void *s) { return s; }
void *cx_str(const char *s) { return (void *)s; }
/* objetos falsos: layer (CALayer), dicionário de atributos, NSNumber */
typedef struct { void *isa; void *contents; const char *gravity; rect_t bounds; int sets; } layer_t;
typedef struct { void *isa; int kind; unsigned num; } fobj_t;   /* kind 1 número, 3 dicionário com PixelFormatType */
const void *kCVPixelBufferPixelFormatTypeKey = "PixelFormatType", *kCVImageBufferYCbCrMatrixKey = "M", *kCVImageBufferColorPrimariesKey = "P", *kCVImageBufferTransferFunctionKey = "T";
static int timers;
long objc_msgSend(void *self, const char *sel, ...) {
    if (!self) return 0;   /* como o runtime: mensagem para nil devolve 0 */
    __builtin_va_list ap; __builtin_va_start(ap, sel);
    long r = 0;
    void *isa0 = ((void **)self)[0];
    if (self == (void *)AVPlayerLayer_c || isa0 == (void *)AVPlayerLayer_c || self == (void *)SBDL_c || isa0 == (void *)SBDL_c) {
        layer_t *l = self;
        if (!strcmp(sel, "alloc")) { l = calloc(1, sizeof *l + 256); l->isa = self; r = (long)l; }
        else if (!strcmp(sel, "init")) r = (long)self;
        else if (!strcmp(sel, "setContents:")) { l->contents = __builtin_va_arg(ap, void *); l->sets++; }
        else if (!strcmp(sel, "setContentsGravity:")) l->gravity = __builtin_va_arg(ap, const char *);
        else { void *f = imp(l->isa, sel); if (f) r = ((long (*)(void *, const char *, void *))f)(self, sel, __builtin_va_arg(ap, void *)); }
    } else if (!strcmp(sel, "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:")) { timers++; r = 1; }
    else if (!strcmp(sel, "objectForKey:")) {
        fobj_t *o = self; const void *k = __builtin_va_arg(ap, const void *);
        if (o->kind == 3) r = (long)(fobj_t *)((char *)o + sizeof(fobj_t));
        else if (o->kind == 6) r = (o->num == 1 && !strcmp(k, "DoNotDisplay")) ? (long)o : 0;   /* dicionário de anexos */
    }
    else if (!strcmp(sel, "respondsToSelector:")) r = 0;
    else if (!strcmp(sel, "longLongValue")) r = ((fobj_t *)self)->num;
    else if (!strcmp(sel, "count")) r = ((fobj_t *)self)->kind == 5 ? 1 : 0;
    else if (!strcmp(sel, "objectAtIndex:")) r = (long)((fobj_t *)self + 1);
    else if (!strcmp(sel, "boolValue")) r = 1;
    __builtin_va_end(ap);
    return r;
}
rect_t objc_msgSend_stret(void *self, const char *sel) { (void)sel; return ((layer_t *)self)->bounds; }
void *dispatch_get_main_queue(void) { return 0; }
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *)) { (void)q; fn(ctx); }
/* CoreVideo / CoreGraphics */
typedef struct { unsigned fmt; int w, h; unsigned char *p0, *p1; unsigned long b0, b1; const char *matrix; } pb_t;
int CVPixelBufferCreate(void *a, unsigned long long w, unsigned long long h, unsigned fmt, void *at, void **out) {
    (void)a; (void)at; pb_t *p = calloc(1, sizeof *p); p->fmt = fmt; p->w = (int)w; p->h = (int)h;
    if (fmt == 0x42475241u) { p->b0 = w * 4 + 32; p->p0 = calloc(p->b0, h); } else { p->b0 = p->b1 = w + 16; p->p0 = calloc(p->b0, h); p->p1 = calloc(p->b1, h / 2 + 1); }
    *out = p; return 0;
}
int CVPixelBufferLockBaseAddress(void *p, unsigned long long f) { (void)p; (void)f; return 0; }
int CVPixelBufferUnlockBaseAddress(void *p, unsigned long long f) { (void)p; (void)f; return 0; }
void *CVPixelBufferGetBaseAddress(void *p) { return ((pb_t *)p)->p0; }
unsigned long long CVPixelBufferGetBytesPerRow(void *p) { return ((pb_t *)p)->b0; }
void *CVPixelBufferGetBaseAddressOfPlane(void *p, unsigned long long i) { return i ? ((pb_t *)p)->p1 : ((pb_t *)p)->p0; }
unsigned long long CVPixelBufferGetBytesPerRowOfPlane(void *p, unsigned long long i) { return i ? ((pb_t *)p)->b1 : ((pb_t *)p)->b0; }
void CVBufferSetAttachment(void *p, void *k, void *v, unsigned m) { (void)m; if (k == kCVImageBufferYCbCrMatrixKey) ((pb_t *)p)->matrix = v; }
const void *CVYCbCrMatrixGetStringForIntegerCodePoint(int c) { return c == 1 ? "709" : c == 6 ? "601" : 0; }
const void *CVColorPrimariesGetStringForIntegerCodePoint(int c) { (void)c; return 0; }
const void *CVTransferFunctionGetStringForIntegerCodePoint(int c) { (void)c; return 0; }
typedef struct { const unsigned char *data; unsigned long n; } prov_t;
typedef struct { int w, h; const unsigned char *data; } img_t;
void *CGDataProviderCreateWithData(void *i, const void *d, unsigned long n, void *rel) { (void)i; (void)rel; prov_t *p = malloc(sizeof *p); p->data = d; p->n = n; return p; }
void *CGImageCreate(unsigned long w, unsigned long h, unsigned long bpc, unsigned long bpp, unsigned long bpr, const void *cs, unsigned info, void *prov, const void *d, _Bool in, int it) {
    (void)bpc; (void)bpp; (void)bpr; (void)cs; (void)info; (void)d; (void)in; (void)it; img_t *g = malloc(sizeof *g); g->w = (int)w; g->h = (int)h; g->data = ((prov_t *)prov)->data; return g;
}
void *CGColorSpaceCreateDeviceRGB(void) { return (void *)1; }
void CGColorSpaceRelease(void *c) { (void)c; }
void CGDataProviderRelease(void *p) { (void)p; }
void CGImageRelease(void *i) { (void)i; }
/* o item e o relógio do AVPlayer */
static const char *g_path; static double g_now;
double cx_avitem_seconds(void *item) { (void)item; return g_now; }
const char *cx_avitem_video_path(void *item) { return item ? g_path : 0; }
void *cx_avplayer_item(void *player) { return player ? (void *)2 : 0; }
float cx_avplayer_rate(void *p) { (void)p; return 1; }
void cx_avplayervideo_init(void);

/* ---- CMSampleBuffer, CMTimebase e VT falsos (o VT real tem teste próprio: tools/vt_test.c) */
const void *kCMSampleAttachmentKey_DoNotDisplay = "DoNotDisplay", *kCMSampleAttachmentKey_DisplayImmediately = "DisplayImmediately";
typedef struct { void *img; double pts; int donot; fobj_t att[2]; } fsb_t;
void *CMSampleBufferGetImageBuffer(void *sb) { return ((fsb_t *)sb)->img; }
void *CMSampleBufferGetFormatDescription(void *sb) { (void)sb; return (void *)7; }
cmt_t CMSampleBufferGetPresentationTimeStamp(void *sb) { cmt_t t = {(long long)(((fsb_t *)sb)->pts * 600 + 0.5), 600, 1, 0}; return t; }
void *CMSampleBufferGetSampleAttachmentsArray(void *sb, unsigned char c) { (void)c; fsb_t *f = sb; if (!f->donot) return 0; f->att[0].kind = 5; f->att[1].kind = 6; f->att[1].num = 1; return &f->att[0]; }
static double g_tb;
cmt_t CMTimebaseGetTime(void *tb) { (void)tb; cmt_t t = {(long long)(g_tb * 600 + 0.5), 600, 1, 0}; return t; }
unsigned long long CVPixelBufferGetWidth(void *p) { return ((pb_t *)p)->w; }
unsigned long long CVPixelBufferGetHeight(void *p) { return ((pb_t *)p)->h; }
unsigned CVPixelBufferGetPixelFormatType(void *p) { return ((pb_t *)p)->fmt; }
typedef void (*vtcb_t)(void *ref, void *fr, int st, unsigned info, void *img, cmt_t pts, cmt_t dur);
static struct { vtcb_t fn; void *ref; } g_vt; static int vt_sessions;
int VTDecompressionSessionCreate(void *a, void *fd, void *spec, void *attrs, const void *cb, void **out) { (void)a; (void)fd; (void)spec; (void)attrs; memcpy(&g_vt, cb, sizeof g_vt); vt_sessions++; *out = &g_vt; return 0; }
int VTDecompressionSessionDecodeFrame(void *s, void *sb, unsigned flags, void *ref, unsigned *info) {
    (void)s; (void)flags; (void)info;
    void *img; CVPixelBufferCreate(0, 2, 2, 0x42475241u, 0, &img); memset(((pb_t *)img)->p0, (int)(((fsb_t *)sb)->pts * 100 + 0.5), 2 * ((pb_t *)img)->b0);
    cmt_t z = {0, 0, 0, 0};
    g_vt.fn(g_vt.ref, ref, 0, 0, img, CMSampleBufferGetPresentationTimeStamp(sb), z);
    return 0;
}
int VTDecompressionSessionFinishDelayedFrames(void *s) { (void)s; return 0; }
void VTDecompressionSessionInvalidate(void *s) { (void)s; }
unsigned char VTDecompressionSessionCanAcceptFormatDescription(void *s, void *fd) { (void)s; (void)fd; return 1; }
static int blk_calls;
typedef struct { void *isa; int flags, res; void (*invoke)(void *); } fblk_t;
static void blk_inv(void *b) { (void)b; blk_calls++; }
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)
static void *call(void *obj, const char *sel) { return imp(((void **)obj)[0], sel); }
int main(int argc, char **argv) {
    g_path = argv[1];
    FILE *f = fopen(argv[2], "rb"); fseek(f, 0, SEEK_END); long rn = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *ref = malloc(rn); if (fread(ref, 1, rn, f) != (size_t)rn) return 2; fclose(f);
    int e; avv_t *v = avv_open_path(argv[1], &e); int w, h; double dur, fps; unsigned ns; avv_info(v, &w, &h, &dur, &fps, &ns);
    long fs = (long)w * h * 3 / 2;
    cx_avplayervideo_init();
    /* saída 420v (sem atributos) */
    void *out = cx_alloc_instance(AVPlayerItemVideoOutput_c);
    ((void *(*)(void *, const char *))call(out, "init"))(out, "init");
    ((void (*)(void *, const char *, void *))call(out, "mosAttachItem:"))(out, 0, (void *)2);

    typedef signed char (*hasnew_t)(void *, const char *, cmt_t);
    typedef void *(*copy_t)(void *, const char *, cmt_t, cmt_t *);
    hasnew_t hasnew = (hasnew_t)call(out, "hasNewPixelBufferForItemTime:");
    copy_t copy = (copy_t)call(out, "copyPixelBufferForItemTime:itemTimeForDisplay:");
    int bad = 0, frames = 0;
    for (int k = 0; k < (int)ns; k += 7) {
        double t = (k + 0.5) / fps;   /* meio do quadro k (vídeo sem B, pts = k/fps) */
        cmt_t ct = {(long long)(t * 600), 600, 1, 0}, disp;
        CHECK(hasnew(out, 0, ct), "hasNew deveria ser YES em t=%.3f", t);
        pb_t *pb = copy(out, 0, ct, &disp);
        if (!pb) { bad++; continue; }
        frames++;
        const unsigned char *R = ref + (long)k * fs;
        for (int r = 0; r < h; r++) if (memcmp(pb->p0 + r * pb->b0, R + r * w, w)) { bad++; break; }
        for (int r = 0; r < h / 2; r++) for (int x = 0; x < w / 2; x++)
            if (pb->p1[r * pb->b1 + 2 * x] != R[w * h + r * w / 2 + x] || pb->p1[r * pb->b1 + 2 * x + 1] != R[w * h * 5 / 4 + r * w / 2 + x]) { bad++; r = h; break; }
        CHECK(disp.value == (long long)(k / fps * 600 + 0.5), "itemTimeForDisplay %lld para o quadro %d", disp.value, k);
        CHECK(!hasnew(out, 0, ct), "hasNew deveria ser NO depois de copiar o mesmo quadro (t=%.3f)", t);
        CHECK(pb->matrix && !strcmp(pb->matrix, "601"), "anexo de matriz");
    }
    CHECK(!bad, "420v: %d quadros errados", bad);
    /* saída BGRA */
    struct { fobj_t d; fobj_t n; } attrs = {{0, 3, 0}, {0, 1, 0x42475241u}};
    void *ob = cx_alloc_instance(AVPlayerItemVideoOutput_c);
    ((void *(*)(void *, const char *, void *))call(ob, "initWithPixelBufferAttributes:"))(ob, 0, &attrs.d);
    ((void (*)(void *, const char *, void *))call(ob, "mosAttachItem:"))(ob, 0, (void *)2);
    cmt_t c1 = {300, 600, 1, 0}, d1;
    pb_t *pbb = ((copy_t)call(ob, "copyPixelBufferForItemTime:itemTimeForDisplay:"))(ob, 0, c1, &d1);
    CHECK(pbb && pbb->fmt == 0x42475241u && pbb->p0[3] == 255, "BGRA");
    /* AVPlayerLayer */
    void *(*with)(void *, const char *, void *) = imp(AVPlayerLayer_c, "playerLayerWithPlayer:");
    g_now = 1.0;
    layer_t *L = with(AVPlayerLayer_c, 0, (void *)1);
    CHECK(L && L->contents && timers >= 1, "layer: contents %p, timers %d", L ? L->contents : 0, timers);
    img_t *im = L ? L->contents : 0;
    CHECK(im && im->w == w && im->h == h, "layer: imagem %dx%d", im ? im->w : 0, im ? im->h : 0);
    if (im) {   /* o contents é o BGRA do quadro do tempo 1,0 s */
        avv_frame fr; avv_frame_at(v, 1.0, &fr); unsigned char *px; int ow, oh; avv_to_bgra(&fr, 0, 0, 0, &px, &ow, &oh);
        CHECK(!memcmp(px, im->data, (size_t)ow * oh * 4), "layer: contents diferente do quadro de 1,0 s");
    }
    signed char (*ready)(void *, const char *) = imp(AVPlayerLayer_c, "isReadyForDisplay");
    CHECK(ready(L, 0), "isReadyForDisplay");
    int sets = L->sets;
    void *st = objc_getAssociatedObject(L, 0) ;
    (void)st;
    /* o tick só troca contents quando o quadro muda */
    for (int i = 0; i < nassoc; i++) if (assoc[i].o == L && ((void **)assoc[i].v)[0] == (void *)MOSAVPlayerLayerState_c) st = assoc[i].v;
    void (*tick)(void *, const char *, void *) = imp(MOSAVPlayerLayerState_c, "tick:");
    tick(st, 0, 0);
    CHECK(L->sets == sets, "tick sem mudança de quadro trocou o contents");
    g_now = 1.5; tick(st, 0, 0);
    CHECK(L->sets == sets + 1, "tick com quadro novo não trocou o contents");
    /* videoGravity e videoRect (layer 704x288: o quadro 352x288 em aspecto fica no meio) */
    void (*setg)(void *, const char *, void *) = imp(AVPlayerLayer_c, "setVideoGravity:");
    rect_t (*vrect)(void *, const char *) = imp(AVPlayerLayer_c, "videoRect");
    L->bounds = (rect_t){0, 0, 2.0 * w, (double)h};
    setg(L, 0, "AVLayerVideoGravityResizeAspect");
    rect_t r = vrect(L, 0);
    CHECK(r.x == w / 2.0 && r.w == w && r.h == h && L->gravity && !strcmp(L->gravity, "resizeAspect"), "videoRect aspecto %.0f,%.0f %.0fx%.0f", r.x, r.y, r.w, r.h);
    setg(L, 0, "AVLayerVideoGravityResizeAspectFill");
    r = vrect(L, 0);
    CHECK(r.w == 2.0 * w && r.y == -h / 2.0 && !strcmp(L->gravity, "resizeAspectFill"), "videoRect preencher %.0f,%.0f %.0fx%.0f", r.x, r.y, r.w, r.h);
    /* AVSampleBufferDisplayLayer */
    {
        layer_t *D = (layer_t *)objc_msgSend((void *)objc_msgSend((void *)SBDL_c, "alloc"), "init");
        void (*enq)(void *, const char *, void *) = imp(SBDL_c, "enqueueSampleBuffer:");
        void (*settb)(void *, const char *, void *) = imp(SBDL_c, "setControlTimebase:");
        void (*flush)(void *, const char *) = imp(SBDL_c, "flush");
        void (*flushrm)(void *, const char *) = imp(SBDL_c, "flushAndRemoveImage");
        signed char (*more)(void *, const char *) = imp(SBDL_c, "isReadyForMoreMediaData");
        void (*req)(void *, const char *, void *, void *) = imp(SBDL_c, "requestMediaDataWhenReadyOnQueue:usingBlock:");
        long (*status)(void *, const char *) = imp(SBDL_c, "status");
        void (*tk)(void *, const char *, void *) = imp(MOSSBDLState_c, "tick:");
        fsb_t s0 = {0, 0.0, 0}, s2 = {0, 0.2, 0}, s1 = {0, 0.1, 0};
        enq(D, 0, &s0); enq(D, 0, &s2); enq(D, 0, &s1);   /* pts fora de ordem (quadros B) */
        void *st = 0;
        for (int i = 0; i < nassoc; i++) if (assoc[i].o == D) st = assoc[i].v;
        int shown[3];
        for (int i = 0; i < 3; i++) { tk(st, 0, 0); shown[i] = D->contents ? ((img_t *)D->contents)->data[0] : -1; }
        CHECK(shown[0] == 0 && shown[1] == 10 && shown[2] == 20, "sem timebase: ordem %d %d %d (esperado 0 10 20)", shown[0], shown[1], shown[2]);
        CHECK(status(D, 0) == 1 && vt_sessions == 1, "status %ld, sessões %d", status(D, 0), vt_sessions);
        settb(D, 0, (void *)9);
        fsb_t t[4] = {{0, 1.0, 0}, {0, 1.1, 0}, {0, 1.2, 1}, {0, 1.3, 0}};
        for (int i = 0; i < 4; i++) enq(D, 0, &t[i]);
        g_tb = 1.05; tk(st, 0, 0);
        int a1 = ((img_t *)D->contents)->data[0];
        g_tb = 1.25; tk(st, 0, 0);
        int a2 = ((img_t *)D->contents)->data[0];   /* 1,2 é DoNotDisplay: fica o 1,1 */
        g_tb = 1.20; tk(st, 0, 0);
        int a3 = ((img_t *)D->contents)->data[0];   /* nada novo até 1,3 */
        CHECK(a1 == 100 && a2 == 110 && a3 == 110, "timebase: %d %d %d (esperado 100 110 110)", a1, a2, a3);
        CHECK(more(D, 0), "isReadyForMoreMediaData");
        fblk_t blk = {0, 0, 0, blk_inv};
        req(D, 0, 0, &blk);
        CHECK(blk_calls >= 1, "requestMediaDataWhenReady não chamou o bloco");
        flush(D, 0);
        g_tb = 5; tk(st, 0, 0);
        CHECK(((img_t *)D->contents)->data[0] == 110, "flush deveria descartar o 1,3 (contents %d)", ((img_t *)D->contents)->data[0]);
        flushrm(D, 0);
        CHECK(D->contents == 0, "flushAndRemoveImage deveria limpar o contents");
    }
    if (!fails) printf("avplayervideo ok: %d quadros 420v iguais ao ffmpeg com hasNew/itemTimeForDisplay, BGRA, AVPlayerLayer (contents, troca só com quadro novo, readyForDisplay, videoGravity/videoRect), AVSampleBufferDisplayLayer (ordem de pts, timebase, DoNotDisplay, flush, pedido de dados)\n", frames);
    return fails != 0;
}
