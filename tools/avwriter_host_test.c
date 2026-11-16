/* Teste de host de userland/libsystem/avwriter.c (AVAssetWriter, AVAssetWriterInput, PixelBufferAdaptor) com o runtime em
 * volta substituído: objc_msgSend é um trampolim (como o de verdade: acha o IMP e salta, preservando registradores e o
 * tipo de retorno), Foundation/CoreMedia/CoreVideo mínimos e um VTCompressionSession que "codifica" de verdade em H.264
 * I_PCM (tools/h264synth.h): a imagem decodificada pelo ffmpeg tem de ser a de entrada, bit a bit. O avwcore.c, o
 * mp4mux.c e o avf_core.c são os de verdade. uso: avwriter_host_test DIR in.m4a → "avwriter_host ok"; o
 * tools/avwriter_test.sh confere os arquivos com o ffprobe/ffmpeg. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "mp4demux.h"
#include "h264synth.h"

typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cmt_t;
typedef struct { cmt_t duration, pts, dts; } cmtiming_t;
typedef struct { int w, h; } cmdim_t;
typedef struct { double a, b, c, d, tx, ty; } aff_t;
typedef struct {
    double mSampleRate;
    unsigned mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved;
} asbd_t;
extern char AVAssetWriter_c[] __asm__("OBJC_CLASS_$_AVAssetWriter");
extern char AVAssetWriterInput_c[] __asm__("OBJC_CLASS_$_AVAssetWriterInput");
extern char AVAssetWriterInputPixelBufferAdaptor_c[] __asm__("OBJC_CLASS_$_AVAssetWriterInputPixelBufferAdaptor");
extern char AVAssetExportSession_c[] __asm__("OBJC_CLASS_$_AVAssetExportSession");
void cx_avwriter_init(void);
void cx_avexport_init(void);
char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
int usleep(unsigned us);

/* ------------------------------------------------ objetos falsos da Foundation */
static char FAKE;   /* isa dos objetos falsos */
enum { K_STR = 1, K_NUM, K_DICT, K_ARR, K_ERR, K_CLASS, K_ASSET };
typedef struct fobj { void *isa; int kind; const char *str; double num; struct fobj **k, **v; int n, cap; long long code; struct fobj *info; } fobj;
static fobj *fnew(int kind) { fobj *o = calloc(1, sizeof *o); o->isa = &FAKE; o->kind = kind; return o; }
void *cx_str(const char *s) { fobj *o = fnew(K_STR); o->str = strdup(s); return o; }
const char *cx_cstr(void *o) { return o && ((fobj *)o)->isa == &FAKE && ((fobj *)o)->kind == K_STR ? ((fobj *)o)->str : 0; }
int cx_is_str(void *o) { return cx_cstr(o) != 0; }
void *cx_num_i(long long v) { fobj *o = fnew(K_NUM); o->num = (double)v; return o; }
static fobj *num(double v) { fobj *o = fnew(K_NUM); o->num = v; return o; }
static void push(fobj *o, fobj *k, fobj *v) {
    if (o->n == o->cap) { o->cap = o->cap ? o->cap * 2 : 8; o->k = realloc(o->k, o->cap * sizeof *o->k); o->v = realloc(o->v, o->cap * sizeof *o->v); }
    o->k[o->n] = k; o->v[o->n++] = v;
}
static fobj *dict(void) { return fnew(K_DICT); }
static void dput(fobj *d, const char *k, fobj *v) {
    for (int i = 0; i < d->n; i++) if (!strcmp(d->k[i]->str, k)) { d->v[i] = v; return; }
    push(d, cx_str(k), v);
}
static fobj *dfind(fobj *d, const char *k) { for (int i = 0; i < d->n; i++) if (!strcmp(d->k[i]->str, k)) return d->v[i]; return 0; }
/* métodos (assinaturas exatas: o trampolim salta direto para eles) */
static long f_ret0(void) { return 0; }
static void *f_self(fobj *s, const char *c) { (void)c; return s; }
static void *f_new_arr(fobj *s, const char *c) { (void)s; (void)c; return fnew(K_ARR); }
static void *f_new_dict(fobj *s, const char *c) { (void)s; (void)c; return dict(); }
static void *f_err(fobj *s, const char *c, fobj *dom, long long code, fobj *info) { (void)s; (void)c; (void)dom; fobj *e = fnew(K_ERR); e->code = code; e->info = info; return e; }
static void *f_dict_get(fobj *s, const char *c, fobj *k) { (void)c; return k && k->str ? dfind(s, k->str) : 0; }
static void f_dict_set(fobj *s, const char *c, fobj *v, fobj *k) { (void)c; dput(s, k->str, v); }
static void *f_dict_copy(fobj *s, const char *c) { (void)c; fobj *d = dict(); for (int i = 0; i < s->n; i++) push(d, s->k[i], s->v[i]); return d; }
static unsigned long long f_count(fobj *s, const char *c) { (void)c; return (unsigned long long)s->n; }
static void *f_arr_at(fobj *s, const char *c, unsigned long long i) { (void)c; return i < (unsigned long long)s->n ? s->v[i] : 0; }
static void f_arr_add(fobj *s, const char *c, fobj *o) { (void)c; push(s, 0, o); }
static double f_double(fobj *s, const char *c) { (void)c; return s->num; }
static signed char f_bool(fobj *s, const char *c) { (void)c; return s->num != 0; }
static void *f_asset_url(fobj *s, const char *c) { (void)c; return s->info; }
static fobj cls_marr = {&FAKE, K_CLASS, "NSMutableArray"}, cls_arr = {&FAKE, K_CLASS, "NSArray"},
            cls_mdict = {&FAKE, K_CLASS, "NSMutableDictionary"}, cls_err = {&FAKE, K_CLASS, "NSError"};
static char ns_object_k;

/* ------------------------------------------------ runtime mínimo das nossas classes */
typedef struct cls_s { void *key; unsigned extra; void *imps[96]; const char *sels[96]; int n; } cls_t;
static cls_t tab[8]; static int ntab;
static cls_t *K(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; tab[ntab].key = c; return &tab[ntab++]; }
static cls_t *Kfind(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; return 0; }
void *cx_class_named(const char *n) {
    if (!strcmp(n, "NSObject")) return &ns_object_k;
    if (!strcmp(n, "NSMutableArray")) return &cls_marr;
    if (!strcmp(n, "NSArray")) return &cls_arr;
    if (!strcmp(n, "NSMutableDictionary")) return &cls_mdict;
    if (!strcmp(n, "NSError")) return &cls_err;
    return 0;
}
void *cx_define_class(void *c, void *m, const char *name, void *super, unsigned extra) { (void)m; (void)name; (void)super; K(c)->extra = extra; return c; }
static void add(void *c, const char *s, void *f) { cls_t *k = K(c); k->sels[k->n] = s; k->imps[k->n++] = f; }
void cx_add_method(void *c, const char *s, void *f) { add(c, s, f); }
void cx_add_cmethod(void *c, const char *s, void *f) { add(c, s, f); }
void *cx_alloc_instance(void *c) { void **o = calloc(1, 16 + K(c)->extra + 64); o[0] = c; return o; }
static void *imp(void *c, const char *s) { cls_t *k = Kfind(c); if (k) for (int i = 0; i < k->n; i++) if (!strcmp(k->sels[i], s)) return k->imps[i]; return 0; }
void *class_getMethodImplementation(void *c, const char *s) { (void)c; (void)s; return 0; }
const char *sel_registerName(const char *s) { return s; }
void *objc_retain(void *o) { return o; }
void objc_release(void *o) { (void)o; }
void *objc_autorelease(void *o) { return o; }
void *object_getClass(void *o) { return o ? ((void **)o)[0] : 0; }
int cx_responds(void *cls, const char *sel) { return cls == &FAKE || imp(cls, sel) != 0; }
/* acha o IMP de self/sel */
void *fake_lookup(void *self, const char *sel) {
    if (!self) return (void *)f_ret0;
    void *f = imp(self, sel);          /* métodos de classe */
    if (f) return f;
    if (((void **)self)[0] != &FAKE) { f = imp(((void **)self)[0], sel); return f ? f : (void *)f_ret0; }
    fobj *o = self;
    if (!strcmp(sel, "release") || !strcmp(sel, "retain")) return (void *)f_self;
    switch (o->kind) {
    case K_CLASS:
        if (!strcmp(sel, "array")) return (void *)f_new_arr;
        if (!strcmp(sel, "dictionary")) return (void *)f_new_dict;
        if (!strcmp(sel, "errorWithDomain:code:userInfo:")) return (void *)f_err;
        break;
    case K_DICT:
        if (!strcmp(sel, "objectForKey:")) return (void *)f_dict_get;
        if (!strcmp(sel, "setObject:forKey:")) return (void *)f_dict_set;
        if (!strcmp(sel, "copy") || !strcmp(sel, "mutableCopy")) return (void *)f_dict_copy;
        if (!strcmp(sel, "count")) return (void *)f_count;
        break;
    case K_ARR:
        if (!strcmp(sel, "count")) return (void *)f_count;
        if (!strcmp(sel, "objectAtIndex:")) return (void *)f_arr_at;
        if (!strcmp(sel, "addObject:")) return (void *)f_arr_add;
        if (!strcmp(sel, "copy")) return (void *)f_self;
        break;
    case K_ASSET:
        if (!strcmp(sel, "URL")) return (void *)f_asset_url;
        break;
    case K_NUM:
        if (!strcmp(sel, "doubleValue")) return (void *)f_double;
        if (!strcmp(sel, "boolValue")) return (void *)f_bool;
        break;
    }
    fprintf(stderr, "seletor sem implementação no teste: %s (tipo %d)\n", sel, o->kind);
    return (void *)f_ret0;
}
/* objc_msgSend: guarda os registradores de argumento, acha o IMP e salta (o retorno, inteiro ou double, é o do IMP) */
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\n"
        "push %rbp\nmov %rsp,%rbp\nsub $0xc0,%rsp\n"
        "mov %rdi,0(%rsp)\nmov %rsi,8(%rsp)\nmov %rdx,16(%rsp)\nmov %rcx,24(%rsp)\nmov %r8,32(%rsp)\nmov %r9,40(%rsp)\nmov %rax,48(%rsp)\n"
        "movdqu %xmm0,64(%rsp)\nmovdqu %xmm1,80(%rsp)\nmovdqu %xmm2,96(%rsp)\nmovdqu %xmm3,112(%rsp)\n"
        "movdqu %xmm4,128(%rsp)\nmovdqu %xmm5,144(%rsp)\nmovdqu %xmm6,160(%rsp)\nmovdqu %xmm7,176(%rsp)\n"
        "call fake_lookup\nmov %rax,%r11\n"
        "mov 0(%rsp),%rdi\nmov 8(%rsp),%rsi\nmov 16(%rsp),%rdx\nmov 24(%rsp),%rcx\nmov 32(%rsp),%r8\nmov 40(%rsp),%r9\nmov 48(%rsp),%rax\n"
        "movdqu 64(%rsp),%xmm0\nmovdqu 80(%rsp),%xmm1\nmovdqu 96(%rsp),%xmm2\nmovdqu 112(%rsp),%xmm3\n"
        "movdqu 128(%rsp),%xmm4\nmovdqu 144(%rsp),%xmm5\nmovdqu 160(%rsp),%xmm6\nmovdqu 176(%rsp),%xmm7\n"
        "leave\njmp *%r11\n");
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }
const char *cx_audio_url_path(void *url) { return cx_cstr(url); }
/* filas: os trabalhos esperam aqui e o teste os roda */
typedef struct { void *ctx; void (*fn)(void *); } job_t;
static job_t jobs[256]; static int njobs;
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *)) { (void)q; jobs[njobs].ctx = ctx; jobs[njobs++].fn = fn; }
void dispatch_after_f(unsigned long long when, void *q, void *ctx, void (*fn)(void *)) { (void)when; dispatch_async_f(q, ctx, fn); }
unsigned long long dispatch_time(unsigned long long when, long delta) { return when + (unsigned long long)delta; }
static int run_jobs(int max) { int k = 0; while (njobs && k < max) { job_t j = jobs[0]; memmove(jobs, jobs + 1, (size_t)--njobs * sizeof *jobs); j.fn(j.ctx); k++; } return k; }

/* ------------------------------------------------ CoreMedia / CoreVideo falsos */
typedef struct { int media; unsigned sub; int w, h; uint8_t sps[64], pps[16]; uint32_t sps_n, pps_n; asbd_t asbd; uint8_t *cookie; size_t cookie_n; } fd_t;
typedef struct { uint8_t *b; size_t n; } bb_t;
typedef struct { int w, h; unsigned fmt; uint8_t *y, *u, *v; } pb_t;
typedef struct { fd_t *fd; bb_t *bb; pb_t *img; long ns; size_t sizes[64]; cmtiming_t t[64]; fobj *att; } sb_t;
const void *kCMSampleAttachmentKey_NotSync;
void *CMSampleBufferGetFormatDescription(sb_t *s) { return s->fd; }
void *CMSampleBufferGetDataBuffer(sb_t *s) { return s->bb; }
void *CMSampleBufferGetImageBuffer(sb_t *s) { return s->img; }
long CMSampleBufferGetNumSamples(sb_t *s) { return s->ns; }
size_t CMSampleBufferGetSampleSize(sb_t *s, long i) { return s->sizes[i]; }
int CMSampleBufferGetSampleTimingInfo(sb_t *s, long i, cmtiming_t *o) { if (i < 0 || i >= s->ns) return -1; *o = s->t[i]; return 0; }
cmt_t CMSampleBufferGetPresentationTimeStamp(sb_t *s) { return s->t[0].pts; }
cmt_t CMSampleBufferGetDuration(sb_t *s) { return s->t[0].duration; }
void *CMSampleBufferGetSampleAttachmentsArray(sb_t *s, unsigned char create) { (void)create; return s->att; }
size_t CMBlockBufferGetDataLength(bb_t *b) { return b->n; }
int CMBlockBufferCopyDataBytes(bb_t *b, size_t off, size_t len, void *dst) { if (off + len > b->n) return -1; memcpy(dst, b->b + off, len); return 0; }
unsigned CMFormatDescriptionGetMediaSubType(fd_t *d) { return d->sub; }
unsigned CMFormatDescriptionGetMediaType(fd_t *d) { return (unsigned)d->media; }
cmdim_t CMVideoFormatDescriptionGetDimensions(fd_t *d) { cmdim_t r = {d->w, d->h}; return r; }
int CMVideoFormatDescriptionGetH264ParameterSetAtIndex(fd_t *d, size_t i, const uint8_t **p, size_t *s, size_t *n, int *nal) {
    if (n) *n = 2;
    if (nal) *nal = 4;
    if (i > 1) return -12710;
    if (p) *p = i ? d->pps : d->sps;
    if (s) *s = i ? d->pps_n : d->sps_n;
    return 0;
}
const void *CMAudioFormatDescriptionGetMagicCookie(fd_t *d, size_t *n) { *n = d->cookie_n; return d->cookie; }
const void *CMAudioFormatDescriptionGetStreamBasicDescription(fd_t *d) { return &d->asbd; }
unsigned long long CVPixelBufferGetWidth(pb_t *p) { return (unsigned long long)p->w; }
unsigned long long CVPixelBufferGetHeight(pb_t *p) { return (unsigned long long)p->h; }
static fobj *pool_attrs;
static int pool_made;
int CVPixelBufferPoolCreate(void *a, void *pa, fobj *attrs, void **out) { (void)a; (void)pa; pool_attrs = attrs; pool_made++; *out = &pool_made; return 0; }
/* VTCompressionSession que grava I_PCM: IDR quando a imagem muda, P-skip (NotSync) quando repete */
typedef void (*vt_cb)(void *ref, void *src, int st, unsigned flags, void *sb);
typedef struct { int w, h; vt_cb cb; void *ref; fobj *props; uint8_t *last; int frame_num, idr_id, frames; fd_t fd; } vts_t;
static vts_t *last_vt;
int VTCompressionSessionCreate(void *a, int w, int h, unsigned codec, void *spec, void *src, void *ca, vt_cb cb, void *ref, void **out) {
    (void)a; (void)spec; (void)src; (void)ca;
    if (codec != 0x61766331) return -12908;
    vts_t *v = calloc(1, sizeof *v);
    v->w = w; v->h = h; v->cb = cb; v->ref = ref;
    v->fd.media = 0x76696465; v->fd.sub = 0x61766331; v->fd.w = w; v->fd.h = h;
    hs_params(w, h, v->fd.sps, &v->fd.sps_n, v->fd.pps, &v->fd.pps_n);
    last_vt = v;
    *out = v;
    return 0;
}
int VTSessionSetProperties(vts_t *v, fobj *props) { v->props = props; return 0; }
int VTCompressionSessionEncodeFrame(vts_t *v, pb_t *img, cmt_t pts, cmt_t dur, void *props, void *ref, unsigned *info) {
    (void)props; (void)info;
    if (img->w != v->w || img->h != v->h) return -12902;
    size_t fs = (size_t)v->w * v->h * 3 / 2;
    uint8_t *cur = malloc(fs);
    memcpy(cur, img->y, (size_t)v->w * v->h); memcpy(cur + v->w * v->h, img->u, fs / 6); memcpy(cur + v->w * v->h + fs / 6, img->v, fs / 6);
    static uint8_t out[1 << 18];
    int key = !v->last || memcmp(v->last, cur, fs) != 0;
    uint32_t n = key ? hs_idr(out, v->w, v->h, img->y, img->u, img->v, v->idr_id++ & 1) : hs_pskip(out, v->w, v->h, ++v->frame_num);
    if (key) v->frame_num = 0;
    free(v->last); v->last = cur;
    sb_t *sb = calloc(1, sizeof *sb);
    bb_t *bb = calloc(1, sizeof *bb); bb->b = malloc(n); memcpy(bb->b, out, n); bb->n = n;
    sb->fd = &v->fd; sb->bb = bb; sb->ns = 1; sb->sizes[0] = n;
    sb->t[0].pts = pts; sb->t[0].dts = pts; sb->t[0].duration = dur;
    if (!key) { fobj *a = fnew(K_ARR), *d = dict(); dput(d, "NotSync", num(1)); push(a, 0, d); sb->att = a; }
    v->frames++;
    v->cb(v->ref, ref, 0, 0, sb);
    return 0;
}
int VTCompressionSessionCompleteFrames(vts_t *v, cmt_t t) { (void)v; (void)t; return 0; }
void VTCompressionSessionInvalidate(vts_t *v) { (void)v; }

/* ------------------------------------------------ ajudantes do teste */
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("avwriter_host FALHA: " __VA_ARGS__); puts(""); fails++; } } while (0)
static char dir[512];
static const char *P(const char *n) { static char b[4][600]; static int i; i = (i + 1) & 3; snprintf(b[i], 600, "%s/%s", dir, n); return b[i]; }
static cmt_t T(long long v, int ts) { cmt_t t = {v, ts, 1, 0}; return t; }
static const cmt_t NONE = {0, 0, 0, 0};
#define CALL(obj, sel, R, ...) ((R (*)())imp(((void **)(obj))[0], sel))(obj, sel, ##__VA_ARGS__)
#define CCALL(k, sel, R, ...) ((R (*)())imp(k, sel))(k, sel, ##__VA_ARGS__)
static long long err_code(void *w) { fobj *e = CALL(w, "error", void *); return e ? e->code : 0; }
static const char *err_msg(void *w) { fobj *e = CALL(w, "error", void *); fobj *m = e && e->info ? dfind(e->info, "NSLocalizedDescription") : 0; return m ? m->str : ""; }
static void *new_writer(const char *file, const char *uti) {
    void *err = 0;
    remove(P(file));
    return CCALL(AVAssetWriter_c, "assetWriterWithURL:fileType:error:", void *, cx_str(P(file)), cx_str(uti), &err);
}
static void *new_input(const char *media, fobj *settings) { return CCALL(AVAssetWriterInput_c, "assetWriterInputWithMediaType:outputSettings:", void *, cx_str(media), settings); }
static fobj *lpcm_settings(int rate, int ch) {
    fobj *d = dict();
    dput(d, "AVFormatIDKey", num(0x6c70636d)); dput(d, "AVSampleRateKey", num(rate)); dput(d, "AVNumberOfChannelsKey", num(ch));
    dput(d, "AVLinearPCMBitDepthKey", num(16)); dput(d, "AVLinearPCMIsFloatKey", num(0)); dput(d, "AVLinearPCMIsBigEndianKey", num(0));
    dput(d, "AVLinearPCMIsNonInterleaved", num(0));
    return d;
}
static uint8_t pat(int x, int y, int k, int plane) { return (uint8_t)(plane == 0 ? 16 + (x * 7 + y * 3 + k * 13) % 200 : plane == 1 ? 40 + (x * 5 + k * 9) % 170 : 50 + (y * 3 + k * 7) % 150); }
static pb_t *frame(int w, int h, int k) {
    pb_t *p = calloc(1, sizeof *p);
    p->w = w; p->h = h; p->fmt = 0x34323076;
    p->y = malloc((size_t)w * h); p->u = malloc((size_t)w * h / 4); p->v = malloc((size_t)w * h / 4);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) p->y[y * w + x] = pat(x, y, k, 0);
    for (int y = 0; y < h / 2; y++) for (int x = 0; x < w / 2; x++) { p->u[y * w / 2 + x] = pat(x, y, k, 1); p->v[y * w / 2 + x] = pat(x, y, k, 2); }
    return p;
}
static void put_frame(FILE *f, pb_t *p) { fwrite(p->y, 1, (size_t)p->w * p->h, f); fwrite(p->u, 1, (size_t)p->w * p->h / 4, f); fwrite(p->v, 1, (size_t)p->w * p->h / 4, f); }
static sb_t *audio_sb(fd_t *fd, const void *data, size_t n, cmt_t pts) {
    sb_t *s = calloc(1, sizeof *s);
    s->fd = fd; s->bb = calloc(1, sizeof *s->bb); s->bb->b = malloc(n); memcpy(s->bb->b, data, n); s->bb->n = n;
    s->ns = 1; s->sizes[0] = n; s->t[0].pts = pts; s->t[0].dts = NONE; s->t[0].duration = NONE;
    return s;
}

/* a.mov: vídeo pelo adaptador (VT), 90°, áudio float32 mono não intercalado -> LPCM 16 estéreo */
static void case_mov(void) {
    void *w = new_writer("a.mov", "com.apple.quicktime-movie");
    CHECK(w, "a: writer");
    fobj *vs = dict(), *comp = dict();
    dput(vs, "AVVideoCodecKey", cx_str("avc1")); dput(vs, "Width", num(64)); dput(vs, "Height", num(48));
    dput(comp, "AverageBitRate", num(800000)); dput(vs, "AVVideoCompressionPropertiesKey", comp);
    void *vin = new_input("vide", vs), *ain = new_input("soun", lpcm_settings(48000, 2));
    aff_t rot = {0, 1, -1, 0, 48, 0};
    ((void (*)(void *, const char *, aff_t))imp(AVAssetWriterInput_c, "setTransform:"))(vin, "setTransform:", rot);
    fobj *pa = dict(); dput(pa, "PixelFormatType", num(0x34323076));
    void *ad = CCALL(AVAssetWriterInputPixelBufferAdaptor_c, "assetWriterInputPixelBufferAdaptorWithAssetWriterInput:sourcePixelBufferAttributes:", void *, vin, pa);
    CHECK(CALL(w, "canAddInput:", signed char, vin) && CALL(w, "canAddInput:", signed char, ain), "a: canAddInput");
    CALL(w, "addInput:", void, vin); CALL(w, "addInput:", void, ain);
    CHECK(CALL(ad, "pixelBufferPool", void *) == 0, "a: pool antes de startWriting");
    CHECK(CALL(w, "startWriting", signed char), "a: startWriting %lld %s", err_code(w), err_msg(w));
    CHECK(CALL(ad, "pixelBufferPool", void *) && pool_attrs, "a: pool");
    if (pool_attrs) {
        fobj *pw = dfind(pool_attrs, "Width"), *ph = dfind(pool_attrs, "Height"), *pf = dfind(pool_attrs, "PixelFormatType");
        CHECK(pw && pw->num == 64 && ph && ph->num == 48 && pf && pf->num == 0x34323076, "a: atributos do pool");
    }
    ((void (*)(void *, const char *, cmt_t))imp(AVAssetWriter_c, "startSessionAtSourceTime:"))(w, "startSessionAtSourceTime:", T(0, 600));
    FILE *ref = fopen(P("a_ref.yuv"), "wb");
    for (int k = 0; k < 12; k++) {   /* a imagem muda a cada 2 quadros: IDR + P-skip */
        pb_t *p = frame(64, 48, k / 2);
        signed char ok = ((signed char (*)(void *, const char *, void *, cmt_t))imp(AVAssetWriterInputPixelBufferAdaptor_c, "appendPixelBuffer:withPresentationTime:"))(ad, "appendPixelBuffer:withPresentationTime:", p, T(k * 20, 600));
        CHECK(ok, "a: appendPixelBuffer %d: %s", k, err_msg(w));
        put_frame(ref, p);
    }
    fclose(ref);
    CHECK(last_vt && last_vt->props && dfind(last_vt->props, "AverageBitRate"), "a: propriedades de compressão no VT");
    /* áudio: float32 mono não intercalado, 48 kHz, 10 blocos de 960 */
    fd_t af = {0};
    af.media = 0x736f756e; af.sub = 0x6c70636d;
    af.asbd = (asbd_t){48000, 0x6c70636d, 1 | 8 | 0x20, 4, 1, 4, 1, 32, 0};
    FILE *pcm = fopen(P("a_ref.pcm"), "wb");
    for (int b = 0; b < 10; b++) {
        float x[960];
        int16_t o[960 * 2];
        for (int i = 0; i < 960; i++) {
            x[i] = (float)((((b * 960 + i) * 37) % 2000) - 1000) / 1100.0f;
            double v = x[i] * 32768.0; v = v > 32767 ? 32767 : v < -32768 ? -32768 : v;
            o[2 * i] = o[2 * i + 1] = (int16_t)(v < 0 ? v - 0.5 : v + 0.5);
        }
        fwrite(o, 1, sizeof o, pcm);
        CHECK(CALL(ain, "appendSampleBuffer:", signed char, audio_sb(&af, x, sizeof x, T(b * 960, 48000))), "a: áudio %d: %s", b, err_msg(w));
    }
    fclose(pcm);
    CALL(vin, "markAsFinished", void); CALL(ain, "markAsFinished", void);
    CHECK(CALL(w, "finishWriting", signed char), "a: finishWriting: %s", err_msg(w));
    CHECK(CALL(w, "status", long) == 2, "a: status %ld", CALL(w, "status", long));
}

/* b.mp4: passthrough de H.264 já comprimido (NotSync) e de AAC (amostras de um M4A de verdade) */
static void case_passthrough(const char *m4a) {
    FILE *f = fopen(m4a, "rb");
    CHECK(f, "b: %s", m4a);
    if (!f) return;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) n = 0;
    fclose(f);
    mp4_file m;
    CHECK(n && !mp4_open_mem(&m, b, (uint64_t)n), "b: mp4demux do M4A");
    int ta = mp4_find_track(&m, MP4_FOURCC('s', 'o', 'u', 'n'));
    if (ta < 0) return;
    mp4_track *t = &m.t[ta];
    void *w = new_writer("b.mp4", "public.mpeg-4");
    void *vin = new_input("vide", 0), *ain = new_input("soun", 0);
    CALL(w, "addInput:", void, vin); CALL(w, "addInput:", void, ain);
    CALL(w, "setShouldOptimizeForNetworkUse:", void, (signed char)1);
    CHECK(CALL(w, "startWriting", signed char), "b: startWriting %s", err_msg(w));
    ((void (*)(void *, const char *, cmt_t))imp(AVAssetWriter_c, "startSessionAtSourceTime:"))(w, "startSessionAtSourceTime:", T(0, 1000));
    /* vídeo: IDR + 4 P-skip, duas vezes, 25 fps, comprimido aqui mesmo */
    fd_t vf = {0};
    vf.media = 0x76696465; vf.sub = 0x61766331; vf.w = 32; vf.h = 32;
    hs_params(32, 32, vf.sps, &vf.sps_n, vf.pps, &vf.pps_n);
    FILE *ref = fopen(P("b_ref.yuv"), "wb");
    pb_t *cur = 0;
    for (int k = 0; k < 10; k++) {
        static uint8_t out[1 << 16];
        uint32_t len;
        if (k % 5 == 0) { cur = frame(32, 32, k); len = hs_idr(out, 32, 32, cur->y, cur->u, cur->v, k / 5 & 1); }
        else len = hs_pskip(out, 32, 32, k % 5);
        put_frame(ref, cur);
        sb_t *s = calloc(1, sizeof *s);
        s->fd = &vf; s->bb = calloc(1, sizeof *s->bb); s->bb->b = malloc(len); memcpy(s->bb->b, out, len); s->bb->n = len;
        s->ns = 1; s->sizes[0] = len; s->t[0].pts = T(k * 40, 1000); s->t[0].dts = T(k * 40, 1000); s->t[0].duration = T(40, 1000);
        if (k % 5) { fobj *a = fnew(K_ARR), *d = dict(); dput(d, "NotSync", num(1)); push(a, 0, d); s->att = a; }
        CHECK(CALL(vin, "appendSampleBuffer:", signed char, s), "b: vídeo %d: %s", k, err_msg(w));
    }
    fclose(ref);
    /* AAC: 4 pacotes por CMSampleBuffer, cookie = AudioSpecificConfig */
    fd_t af = {0};
    af.media = 0x736f756e; af.sub = 0x61616320;
    af.asbd = (asbd_t){(double)t->sample_rate, 0x61616320, 0, 0, 1024, 0, t->channels, 0, 0};
    af.cookie = t->cfg; af.cookie_n = t->cfg_len;
    for (uint32_t i = 0; i < t->nsamples; i += 4) {
        sb_t *s = calloc(1, sizeof *s);
        size_t tot = 0;
        long ns = t->nsamples - i < 4 ? (long)(t->nsamples - i) : 4;
        for (long j = 0; j < ns; j++) { s->sizes[j] = t->s[i + j].size; tot += t->s[i + j].size; }
        s->bb = calloc(1, sizeof *s->bb); s->bb->b = malloc(tot); s->bb->n = tot;
        size_t off = 0;
        for (long j = 0; j < ns; j++) { mp4_read_sample(&m, t, i + (uint32_t)j, s->bb->b + off); off += t->s[i + j].size; }
        s->fd = &af; s->ns = ns;
        s->t[0].pts = T((long long)i * 1024, (int)t->sample_rate); s->t[0].dts = NONE; s->t[0].duration = NONE;
        CHECK(CALL(ain, "appendSampleBuffer:", signed char, s), "b: AAC %u: %s", i, err_msg(w));
    }
    CHECK(CALL(w, "finishWriting", signed char), "b: finishWriting: %s", err_msg(w));
    /* as amostras AAC saem iguais às do M4A */
    FILE *g = fopen(P("b.mp4"), "rb");
    fseek(g, 0, SEEK_END); long n2 = ftell(g); fseek(g, 0, SEEK_SET);
    uint8_t *b2 = malloc((size_t)n2);
    if (fread(b2, 1, (size_t)n2, g) != (size_t)n2) n2 = 0;
    fclose(g);
    mp4_file o;
    CHECK(n2 && !mp4_open_mem(&o, b2, (uint64_t)n2), "b: mp4demux da saída");
    int oa = mp4_find_track(&o, MP4_FOURCC('s', 'o', 'u', 'n')), ov = mp4_find_track(&o, MP4_FOURCC('v', 'i', 'd', 'e'));
    CHECK(oa >= 0 && ov >= 0 && o.t[oa].nsamples == t->nsamples && o.t[oa].cfg_len == t->cfg_len && !memcmp(o.t[oa].cfg, t->cfg, t->cfg_len), "b: faixa AAC");
    if (oa >= 0 && ov >= 0) {
        int same = 1;
        for (uint32_t i = 0; i < t->nsamples && same; i++) {
            if (o.t[oa].s[i].size != t->s[i].size) { same = 0; break; }
            uint8_t x[8192], y[8192];
            mp4_read_sample(&m, t, i, x); mp4_read_sample(&o, &o.t[oa], i, y);
            same = !memcmp(x, y, t->s[i].size);
        }
        CHECK(same, "b: pacotes AAC diferentes");
        uint32_t keys = 0;
        for (uint32_t i = 0; i < o.t[ov].nsamples; i++) keys += o.t[ov].s[i].sync;
        CHECK(o.t[ov].nsamples == 10 && keys == 2, "b: vídeo %u amostras, %u chave", o.t[ov].nsamples, keys);
    }
}

/* c.wav: estéreo int16 intercalado -> mono (média), pelo requestMediaDataWhenReadyOnQueue */
static void *c_in;
static int c_block;
static void c_invoke(void *blk) {
    (void)blk;
    static fd_t af;
    af.media = 0x736f756e; af.sub = 0x6c70636d;
    af.asbd = (asbd_t){44100, 0x6c70636d, 4 | 8, 4, 1, 4, 2, 16, 0};
    while (CALL(c_in, "isReadyForMoreMediaData", signed char) && c_block < 8) {
        int16_t x[2 * 441];
        for (int i = 0; i < 441; i++) { x[2 * i] = (int16_t)((c_block * 441 + i) * 13 % 20000 - 10000); x[2 * i + 1] = (int16_t)(i * 7 - 1500); }
        if (!CALL(c_in, "appendSampleBuffer:", signed char, audio_sb(&af, x, sizeof x, T(c_block * 441, 44100)))) break;
        c_block++;
        if (c_block == 4) return;   /* volta depois: o writer chama o bloco de novo */
    }
    CALL(c_in, "markAsFinished", void);
}
static void case_wav(void) {
    void *w = new_writer("c.wav", "com.microsoft.waveform-audio");
    c_in = new_input("soun", lpcm_settings(44100, 1));
    CALL(w, "addInput:", void, c_in);
    CHECK(CALL(w, "startWriting", signed char), "c: startWriting %s", err_msg(w));
    ((void (*)(void *, const char *, cmt_t))imp(AVAssetWriter_c, "startSessionAtSourceTime:"))(w, "startSessionAtSourceTime:", T(0, 44100));
    struct { void *isa; int flags, res; void (*invoke)(void *); } blk = {0, 0, 0, c_invoke};
    CALL(c_in, "requestMediaDataWhenReadyOnQueue:usingBlock:", void, (void *)1, &blk);
    int runs = run_jobs(10);
    CHECK(c_block == 8 && runs == 2, "c: bloco chamado %d vezes, %d blocos", runs, c_block);
    CHECK(CALL(w, "finishWriting", signed char), "c: finishWriting: %s", err_msg(w));
    FILE *f = fopen(P("c_ref.pcm"), "wb");
    for (int b = 0; b < 8; b++) for (int i = 0; i < 441; i++) {
        int l = (b * 441 + i) * 13 % 20000 - 10000, r = i * 7 - 1500;
        int16_t m = (int16_t)((l + r) / 2);
        fwrite(&m, 2, 1, f);
    }
    fclose(f);
}

static void erros(void) {
    /* arquivo já existe */
    FILE *f = fopen(P("e.mov"), "wb"); fputs("x", f); fclose(f);
    void *err = 0;
    void *w = CCALL(AVAssetWriter_c, "assetWriterWithURL:fileType:error:", void *, cx_str(P("e.mov")), cx_str("com.apple.quicktime-movie"), &err);
    CALL(w, "addInput:", void, new_input("soun", lpcm_settings(48000, 2)));
    CHECK(!CALL(w, "startWriting", signed char) && CALL(w, "status", long) == 3 && err_code(w) == -11823, "e: arquivo existente (%lld)", err_code(w));
    /* tipo de arquivo sem gravador */
    err = 0;
    w = CCALL(AVAssetWriter_c, "assetWriterWithURL:fileType:error:", void *, cx_str(P("e.aif")), cx_str("public.aiff-audio"), &err);
    CHECK(!w && err && ((fobj *)err)->code == -11822, "e: AIFF");
    /* vídeo em M4A, AAC a partir de PCM, LPCM em MP4 */
    w = new_writer("e.m4a", "com.apple.m4a-audio");
    CHECK(!CALL(w, "canAddInput:", signed char, new_input("vide", 0)), "e: vídeo em M4A");
    fobj *aac = dict(); dput(aac, "AVFormatIDKey", num(0x61616320)); dput(aac, "AVSampleRateKey", num(44100)); dput(aac, "AVNumberOfChannelsKey", num(2));
    void *ai = new_input("soun", aac);
    CHECK(!CALL(w, "canAddInput:", signed char, ai), "e: AAC de PCM ainda sem codificador");
    CALL(w, "addInput:", void, ai);
    CHECK(err_code(w) == -11821 && strstr(err_msg(w), "AAC"), "e: erro do AAC: %s", err_msg(w));
    w = new_writer("e.mp4", "public.mpeg-4");
    CHECK(!CALL(w, "canAddInput:", signed char, new_input("soun", lpcm_settings(48000, 2))), "e: LPCM em MP4");
    /* amostra antes da sessão; taxa diferente */
    w = new_writer("e2.mov", "com.apple.quicktime-movie");
    void *in = new_input("soun", lpcm_settings(48000, 2));
    CALL(w, "addInput:", void, in);
    CALL(w, "startWriting", signed char);
    fd_t af = {0};
    af.media = 0x736f756e; af.asbd = (asbd_t){44100, 0x6c70636d, 4 | 8, 4, 1, 4, 2, 16, 0};
    int16_t x[200] = {0};
    CHECK(!CALL(in, "appendSampleBuffer:", signed char, audio_sb(&af, x, sizeof x, T(0, 44100))) && CALL(w, "status", long) == 3, "e: amostra antes da sessão");
    w = new_writer("e3.mov", "com.apple.quicktime-movie");
    in = new_input("soun", lpcm_settings(48000, 2));
    CALL(w, "addInput:", void, in);
    CALL(w, "startWriting", signed char);
    ((void (*)(void *, const char *, cmt_t))imp(AVAssetWriter_c, "startSessionAtSourceTime:"))(w, "startSessionAtSourceTime:", T(0, 1));
    CHECK(!CALL(in, "appendSampleBuffer:", signed char, audio_sb(&af, x, sizeof x, T(0, 44100))) && strstr(err_msg(w), "taxa"), "e: taxa diferente");
    CALL(w, "cancelWriting", void);
    f = fopen(P("e3.mov"), "rb");
    CHECK(!f, "e: arquivo de um writer que falhou ficou no disco");
    if (f) fclose(f);
}

/* AVAssetExportSession (avexport_objc.c) sobre o src.mp4 do teste do núcleo (64x48 H.264 + AAC, 2 s) */
typedef struct { cmt_t start, duration; } cmrange_t;
static int done_calls, compat_ok;
static void done_invoke(void *b) { (void)b; done_calls++; }
static void compat_invoke(void *b, signed char ok) { (void)b; compat_ok = ok; }
static struct { void *isa; int flags, res; void (*invoke)(void *); } done_blk = {0, 0, 0, done_invoke};
static struct { void *isa; int flags, res; void (*invoke)(void *, signed char); } compat_blk = {0, 0, 0, compat_invoke};
static void *new_export(fobj *asset, const char *preset) { return CCALL(AVAssetExportSession_c, "exportSessionWithAsset:presetName:", void *, asset, cx_str(preset)); }
static long wait_export(void *x) {
    for (int i = 0; i < 500 && CALL(x, "status", long) < 3; i++) usleep(10000);
    return CALL(x, "status", long);
}
static long long xerr(void *x) { fobj *e = CALL(x, "error", void *); return e ? e->code : 0; }
static const char *xmsg(void *x) { fobj *e = CALL(x, "error", void *); fobj *m = e && e->info ? dfind(e->info, "NSLocalizedDescription") : 0; return m ? m->str : ""; }
static void case_export(const char *src) {
    fobj *asset = fnew(K_ASSET);
    asset->info = cx_str(src);
    fobj *pres = CCALL(AVAssetExportSession_c, "exportPresetsCompatibleWithAsset:", void *, asset);
    CHECK(pres && pres->n == 13, "x: presets compatíveis %d (esperado 13)", pres ? pres->n : -1);
    void *x = new_export(asset, "AVAssetExportPresetPassthrough");
    CHECK(x, "x: sessão");
    fobj *types = CALL(x, "supportedFileTypes", void *);
    CHECK(types && types->n == 3, "x: supportedFileTypes %d", types ? types->n : -1);
    remove(P("x1.mov"));
    CALL(x, "setOutputURL:", void, cx_str(P("x1.mov")));
    CALL(x, "setOutputFileType:", void, cx_str("com.apple.quicktime-movie"));
    cmrange_t r = {T(1, 2), T(4, 5)};   /* [0,5 s, 1,3 s) */
    ((void (*)(void *, const char *, cmrange_t))imp(AVAssetExportSession_c, "setTimeRange:"))(x, "setTimeRange:", r);
    CALL(x, "exportAsynchronouslyWithCompletionHandler:", void, &done_blk);
    long st = wait_export(x);
    float pr = ((float (*)(void *, const char *))imp(AVAssetExportSession_c, "progress"))(x, "progress");
    CHECK(st == 3 && done_calls == 1 && pr == 1.0f, "x: passthrough status %ld, %d chamadas, progresso %.2f: %s", st, done_calls, pr, xmsg(x));
    FILE *f = fopen(P("x1.mov"), "rb");
    if (f) {
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *b = malloc((size_t)n);
        if (fread(b, 1, (size_t)n, f) != (size_t)n) n = 0;
        fclose(f);
        mp4_file m;
        CHECK(n && !mp4_open_mem(&m, b, (uint64_t)n) && m.duration == 800 && m.ntracks == 2, "x: x1.mov com 0,8 s e 2 faixas");
    } else CHECK(0, "x: x1.mov não existe");
    /* M4A só com o áudio AAC */
    x = new_export(asset, "AVAssetExportPresetAppleM4A");
    remove(P("x2.m4a"));
    CALL(x, "setOutputURL:", void, cx_str(P("x2.m4a")));
    CALL(x, "setOutputFileType:", void, cx_str("com.apple.m4a-audio"));
    CALL(x, "exportAsynchronouslyWithCompletionHandler:", void, &done_blk);
    CHECK(wait_export(x) == 3 && done_calls == 2, "x: M4A: %s", xmsg(x));
    CCALL(AVAssetExportSession_c, "determineCompatibilityOfExportPreset:withAsset:outputFileType:completionHandler:", void,
          cx_str("AVAssetExportPresetAppleM4A"), asset, cx_str("com.apple.m4a-audio"), &compat_blk);
    CHECK(compat_ok == 1, "x: determineCompatibility M4A");
    CCALL(AVAssetExportSession_c, "determineCompatibilityOfExportPreset:withAsset:outputFileType:completionHandler:", void,
          cx_str("AVAssetExportPresetAppleM4A"), asset, cx_str("com.apple.m4v-video"), &compat_blk);
    CHECK(compat_ok == 0, "x: determineCompatibility M4A em M4V");
    /* arquivo existente; videoComposition; cancelamento antes de começar */
    x = new_export(asset, "AVAssetExportPresetPassthrough");
    CALL(x, "setOutputURL:", void, cx_str(P("x1.mov")));
    CALL(x, "setOutputFileType:", void, cx_str("com.apple.quicktime-movie"));
    CALL(x, "exportAsynchronouslyWithCompletionHandler:", void, &done_blk);
    CHECK(wait_export(x) == 4 && xerr(x) == -11823, "x: arquivo existente: %lld", xerr(x));
    x = new_export(asset, "AVAssetExportPresetHighestQuality");
    remove(P("x3.mp4"));
    CALL(x, "setOutputURL:", void, cx_str(P("x3.mp4")));
    CALL(x, "setOutputFileType:", void, cx_str("public.mpeg-4"));
    CALL(x, "setVideoComposition:", void, dict());
    CALL(x, "exportAsynchronouslyWithCompletionHandler:", void, &done_blk);
    CHECK(wait_export(x) == 4 && xerr(x) == -11820 && strstr(xmsg(x), "videoComposition"), "x: videoComposition: %s", xmsg(x));
    x = new_export(asset, "AVAssetExportPresetPassthrough");
    CALL(x, "cancelExport", void);
    CHECK(CALL(x, "status", long) == 5, "x: cancelExport antes de exportar");
    fobj *bad = fnew(K_ASSET); bad->info = cx_str(P("nao-existe.mp4"));
    CHECK(!new_export(bad, "AVAssetExportPresetPassthrough"), "x: asset ilegível devolve nil");
}

int main(int argc, char **argv) {
    if (argc < 4) return 2;
    snprintf(dir, sizeof dir, "%s", argv[1]);
    kCMSampleAttachmentKey_NotSync = cx_str("NotSync");
    cx_avwriter_init();
    cx_avexport_init();
    case_mov();
    case_passthrough(argv[2]);
    case_wav();
    erros();
    case_export(argv[3]);
    if (!fails) puts("avwriter_host ok");
    return fails != 0;
}
