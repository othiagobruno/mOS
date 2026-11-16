/* Host: userland/libsystem/avcapture.c + avcap_conv.c com um runtime mínimo (trampolim do objc_msgSend) e uma câmera UVC
 * falsa atrás do GUI_DEVICE (DEV_CAMERA_*). → "avcapture: N/N ok" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
typedef int32_t i32; typedef uint32_t u32; typedef uint64_t u64; typedef int64_t i64; typedef uint8_t u8; typedef uint16_t u16; typedef int16_t i16;
#include "../kernel/include/compat_gui.h"

/* classes de casca do vscode_fwclasses.c */
#define SHELL(n) char n##_c[256] __asm__("OBJC_CLASS_$_" #n), n##_m[256] __asm__("OBJC_METACLASS_$_" #n);
SHELL(AVCaptureDevice) SHELL(AVCaptureDeviceInput) SHELL(AVCaptureVideoDataOutput) SHELL(AVCapturePhotoOutput)
SHELL(AVCapturePhotoSettings) SHELL(AVCaptureSession)
void cx_avcapture_init(void);

/* ---------------------------------------------------------------- runtime */
typedef struct { void *key; const char *name; unsigned extra; void *super; const char *sels[160]; void *imps[160]; int n; } cls_t;
static cls_t tab[48]; static int ntab;
static char NSObj, ARRC, DICTC, NUMC, STRC, NCC, ERRC, TIMERC, DATAC, LAYERC_KEY[256], DELC[256], DELM[256], LAYERM[256];
static cls_t *Kc(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; tab[ntab].key = c; return &tab[ntab++]; }
static cls_t *Kf(void *c) { for (int i = 0; i < ntab; i++) if (tab[i].key == c) return &tab[i]; return 0; }
void *cx_class_named(const char *n) {
    if (!strcmp(n, "NSObject")) return &NSObj;
    if (!strcmp(n, "NSMutableArray") || !strcmp(n, "NSArray")) return &ARRC;
    if (!strcmp(n, "NSMutableDictionary")) return &DICTC;
    if (!strcmp(n, "NSNotificationCenter")) return &NCC;
    if (!strcmp(n, "NSError")) return &ERRC;
    if (!strcmp(n, "NSTimer")) return &TIMERC;
    if (!strcmp(n, "NSData")) return &DATAC;
    for (int i = 0; i < ntab; i++) if (tab[i].name && !strcmp(tab[i].name, n)) return tab[i].key;
    return 0;
}
void *cx_define_class(void *c, void *m, const char *name, void *super, unsigned extra) { (void)m; cls_t *k = Kc(c); k->name = name; k->super = super; k->extra = extra; return c; }
void cx_add_method(void *c, const char *s, void *f) { cls_t *k = Kc(c); k->sels[k->n] = s; k->imps[k->n++] = f; }
void cx_add_cmethod(void *c, const char *s, void *f) { cx_add_method(c, s, f); }
static unsigned size_of(void *c) { unsigned s = 0; for (cls_t *k = Kf(c); k; k = Kf(k->super)) s += k->extra; return s; }
void *cx_alloc_instance(void *c) { void **o = calloc(1, 16 + size_of(c) + 64); o[0] = c; return o; }
static void *imp(void *c, const char *s) { for (cls_t *k = Kf(c); k; k = Kf(k->super)) for (int i = 0; i < k->n; i++) if (!strcmp(k->sels[i], s)) return k->imps[i]; return 0; }
void *class_getMethodImplementation(void *c, const char *s) { (void)c; (void)s; return 0; }
const char *sel_registerName(const char *s) { return s; }
void *objc_retain(void *o) { return o; }
void objc_release(void *o) { (void)o; }
void *objc_autorelease(void *o) { return o; }
void *objc_autoreleasePoolPush(void) { return 0; }
void objc_autoreleasePoolPop(void *p) { (void)p; }
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }
void *object_getClass(void *o) { return o ? *(void **)o : 0; }
signed char cx_responds(void *c, const char *s) { return imp(c, s) != 0; }
static struct { void *o; const void *k; void *v; } assoc[32]; static int nassoc;
void objc_setAssociatedObject(void *o, const void *k, void *v, unsigned long long p) {
    (void)p;
    for (int i = 0; i < nassoc; i++) if (assoc[i].o == o && assoc[i].k == k) { assoc[i].v = v; return; }
    assoc[nassoc].o = o; assoc[nassoc].k = k; assoc[nassoc++].v = v;
}
void *objc_getAssociatedObject(void *o, const void *k) { for (int i = 0; i < nassoc; i++) if (assoc[i].o == o && assoc[i].k == k) return assoc[i].v; return 0; }

/* objetos de Foundation de mentira */
typedef struct { void *isa; const char *s; } str_t;
typedef struct { void *isa; long long v; } num_t;
typedef struct { void *isa; void *it[64]; int n; } arr_t;
typedef struct { void *isa; void *k[16], *v[16]; int n; } dict_t;
void *cx_str(const char *s) { str_t *o = calloc(1, sizeof *o); o->isa = &STRC; o->s = strdup(s); return o; }
const char *cx_cstr(void *o) { return o ? ((str_t *)o)->s : ""; }
void *cx_num_i(long long v) { num_t *o = calloc(1, sizeof *o); o->isa = &NUMC; o->v = v; return o; }
static str_t kfmt_s = {&STRC, "PixelFormatType"};
const void *kCVPixelBufferPixelFormatTypeKey = &kfmt_s, *kCVPixelBufferWidthKey = "w", *kCVPixelBufferHeightKey = "h", *kCVImageBufferYCbCrMatrixKey = "m";
static int key_eq(void *a, void *b) { return a == b || (a && b && *(void **)a == &STRC && *(void **)b == &STRC && !strcmp(cx_cstr(a), cx_cstr(b))); }
static void *f_arr(void *k, const char *c) { (void)k; (void)c; arr_t *a = calloc(1, sizeof *a); a->isa = &ARRC; return a; }
static void f_add(arr_t *a, const char *c, void *o) { (void)c; a->it[a->n++] = o; }
static void f_addall(arr_t *a, const char *c, arr_t *b) { (void)c; for (int i = 0; i < b->n; i++) a->it[a->n++] = b->it[i]; }
static unsigned long long f_count(arr_t *a, const char *c) { (void)c; return (unsigned long long)a->n; }
static void *f_at(arr_t *a, const char *c, unsigned long long i) { (void)c; return a->it[i]; }
static void *f_dict(void *k, const char *c) { (void)k; (void)c; dict_t *d = calloc(1, sizeof *d); d->isa = &DICTC; return d; }
static void f_dset(dict_t *d, const char *c, void *v, void *k) { (void)c; d->k[d->n] = k; d->v[d->n++] = v; }
static void *f_dget(dict_t *d, const char *c, void *k) { (void)c; for (int i = d->n - 1; i >= 0; i--) if (key_eq(d->k[i], k)) return d->v[i]; return 0; }
static long long f_ll(num_t *n, const char *c) { (void)c; return n->v; }
static char NC_OBJ = 0; static void *nc_inst = &NC_OBJ;
static const char *posted[32]; static int nposted;
static void *f_nc(void *k, const char *c) { (void)k; (void)c; static void *o[2] = {&NCC, 0}; (void)nc_inst; return o; }
static void f_post(void *s, const char *c, void *name, void *obj) { (void)s; (void)c; (void)obj; if (nposted < 32) posted[nposted++] = cx_cstr(name); }
static void *f_err(void *k, const char *c, void *dom, long code, void *ui) { (void)k; (void)c; (void)dom; (void)ui; num_t *e = cx_num_i(code); e->isa = &ERRC; return e; }
static void *f_timer(void *k, const char *c, double dt, void *t, const char *s, void *ui, signed char rep) { (void)k; (void)c; (void)dt; (void)t; (void)s; (void)ui; (void)rep; void **o = calloc(1, 16); o[0] = &TIMERC; return o; }
static void *f_data(void *k, const char *c, const void *b, unsigned long long n) { (void)k; (void)c; (void)b; num_t *d = cx_num_i((long long)n); d->isa = &DATAC; return d; }
static long f_ret0(void) { return 0; }
static void *f_self(void *s, const char *c) { (void)c; return s; }
static void *layer_contents; static const char *layer_gravity;
static void f_set_contents(void *s, const char *c, void *img) { (void)s; (void)c; layer_contents = img; }
static void f_set_grav(void *s, const char *c, void *g) { (void)s; (void)c; layer_gravity = cx_cstr(g); }
static void *f_layer_alloc(void *k, const char *c) { (void)c; return cx_alloc_instance(k); }
void *fake_lookup(void *self, const char *sel) {
    if (!self) return (void *)f_ret0;
    if (self == &ARRC && !strcmp(sel, "array")) return (void *)f_arr;
    if (self == &DICTC && !strcmp(sel, "dictionary")) return (void *)f_dict;
    if (self == &NCC && !strcmp(sel, "defaultCenter")) return (void *)f_nc;
    if (self == &ERRC) return (void *)f_err;
    if (self == &TIMERC) return (void *)f_timer;
    if (self == &DATAC) return (void *)f_data;
    if (Kf(self) && !strcmp(sel, "alloc")) return (void *)f_layer_alloc;
    void *f = imp(self, sel);
    if (f) return f;
    void *isa = *(void **)self;
    if (isa == &ARRC) return !strcmp(sel, "addObject:") ? (void *)f_add : !strcmp(sel, "count") ? (void *)f_count : !strcmp(sel, "addObjectsFromArray:") ? (void *)f_addall : (void *)f_at;
    if (isa == &DICTC) return !strcmp(sel, "setObject:forKey:") ? (void *)f_dset : (void *)f_dget;
    if (isa == &NUMC && !strcmp(sel, "longLongValue")) return (void *)f_ll;
    if (isa == &NCC) return (void *)f_post;
    if (!strcmp(sel, "init")) return (void *)f_self;
    if (!strcmp(sel, "setContents:")) return (void *)f_set_contents;
    if (!strcmp(sel, "setContentsGravity:")) return (void *)f_set_grav;
    f = imp(isa, sel);
    return f ? f : (void *)f_ret0;
}
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\npush %rbp\nmov %rsp,%rbp\nsub $0x40,%rsp\n"
        "mov %rdi,0(%rsp)\nmov %rsi,8(%rsp)\nmov %rdx,16(%rsp)\nmov %rcx,24(%rsp)\nmov %r8,32(%rsp)\nmov %r9,40(%rsp)\n"
        "movdqu %xmm0,48(%rsp)\ncall fake_lookup\nmov %rax,%r11\n"
        "mov 0(%rsp),%rdi\nmov 8(%rsp),%rsi\nmov 16(%rsp),%rdx\nmov 24(%rsp),%rcx\nmov 32(%rsp),%r8\nmov 40(%rsp),%r9\nmovdqu 48(%rsp),%xmm0\n"
        "leave\njmp *%r11\n");

/* ---------------------------------------------------------------- CoreVideo / CoreMedia / CG / ImageIO de mentira */
typedef struct { long long value; int timescale; unsigned flags; long long epoch; } cmt_t;
typedef struct { void *isa; unsigned fmt; int w, h; unsigned char *base[3]; unsigned long long bpr[3]; int np; } pb_t;
typedef struct { void *isa; pb_t *pb; cmt_t pts; } sb_t;
static char PBC, SBC;
cmt_t CMClockGetTime(void *c) { (void)c; cmt_t t = {42, 1000000000, 1, 0}; return t; }
void *CMClockGetHostTimeClock(void) { return (void *)1; }
int CMVideoFormatDescriptionCreate(void *a, unsigned cc, int w, int h, void *e, void **o) { (void)a; (void)e; *o = cx_num_i((long long)cc << 32 | (unsigned)(w * 10000 + h)); return 0; }
int CMVideoFormatDescriptionCreateForImageBuffer(void *a, void *img, void **o) { pb_t *p = img; return CMVideoFormatDescriptionCreate(a, p->fmt, p->w, p->h, 0, o); }
int CMSampleBufferCreateReadyWithImageBuffer(void *a, void *img, void *fd, const void *t, void **o) {
    (void)a; (void)fd;
    sb_t *s = calloc(1, sizeof *s); s->isa = &SBC; s->pb = img; memcpy(&s->pts, (const char *)t + sizeof(cmt_t), sizeof(cmt_t)); *o = s; return 0;
}
void *CMSampleBufferGetImageBuffer(void *sb) { return ((sb_t *)sb)->pb; }
int CVPixelBufferCreateWithBytes(void *a, unsigned long long w, unsigned long long h, unsigned fmt, void *base, unsigned long long bpr, void *rel, void *ctx, void *at, void **o) {
    (void)a; (void)rel; (void)ctx; (void)at;
    pb_t *p = calloc(1, sizeof *p); p->isa = &PBC; p->fmt = fmt; p->w = (int)w; p->h = (int)h; p->base[0] = base; p->bpr[0] = bpr; p->np = 1; *o = p; return 0;
}
int CVPixelBufferCreateWithPlanarBytes(void *a, unsigned long long w, unsigned long long h, unsigned fmt, void *d, unsigned long long ds, unsigned long long np,
                                       void **pb, unsigned long long *pw, unsigned long long *ph, unsigned long long *pr, void *rel, void *ctx, void *at, void **o) {
    (void)a; (void)d; (void)ds; (void)pw; (void)ph; (void)rel; (void)ctx; (void)at;
    pb_t *p = calloc(1, sizeof *p); p->isa = &PBC; p->fmt = fmt; p->w = (int)w; p->h = (int)h; p->np = (int)np;
    for (unsigned i = 0; i < np; i++) { p->base[i] = pb[i]; p->bpr[i] = pr[i]; }
    *o = p; return 0;
}
int CVPixelBufferLockBaseAddress(void *p, unsigned long long f) { (void)p; (void)f; return 0; }
int CVPixelBufferUnlockBaseAddress(void *p, unsigned long long f) { (void)p; (void)f; return 0; }
void *CVPixelBufferGetBaseAddress(void *p) { return ((pb_t *)p)->base[0]; }
unsigned long long CVPixelBufferGetBytesPerRow(void *p) { return ((pb_t *)p)->bpr[0]; }
unsigned long long CVPixelBufferGetWidth(void *p) { return (unsigned long long)((pb_t *)p)->w; }
unsigned long long CVPixelBufferGetHeight(void *p) { return (unsigned long long)((pb_t *)p)->h; }
unsigned CVPixelBufferGetPixelFormatType(void *p) { return ((pb_t *)p)->fmt; }
void CVBufferSetAttachment(void *p, void *k, void *v, unsigned m) { (void)p; (void)k; (void)v; (void)m; }
const void *CVYCbCrMatrixGetStringForIntegerCodePoint(int c) { return c == 6 ? "ITU_R_601_4" : 0; }
void *CGDataProviderCreateWithData(void *i, const void *d, size_t n, void *rel) { (void)i; (void)n; (void)rel; return (void *)d; }
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *cs, unsigned info, void *prov, const void *dec, _Bool in, int it) {
    (void)bpc; (void)bpp; (void)bpr; (void)cs; (void)info; (void)dec; (void)in; (void)it;
    unsigned char *img = malloc(16); memcpy(img, &prov, 8); int wh = (int)(w * 1000 + h); memcpy(img + 8, &wh, 4); return img;
}
void *CGColorSpaceCreateDeviceRGB(void) { return (void *)1; }
void CGColorSpaceRelease(void *c) { (void)c; }
void CGDataProviderRelease(void *p) { (void)p; }
void CGImageRelease(void *i) { (void)i; }
typedef struct { int w, h, has_alpha; unsigned *px; } mos_img_t;
int mos_img_decode(const unsigned char *d, size_t n, mos_img_t *o) {   /* "JPEG" falso: FF D8, w, h, cor BGRA */
    if (n < 8 || d[0] != 0xFF || d[1] != 0xD8) return -1;
    o->w = d[2]; o->h = d[3]; o->has_alpha = 0; o->px = malloc((size_t)o->w * o->h * 4);
    for (int i = 0; i < o->w * o->h; i++) memcpy(o->px + i, d + 4, 4);
    return 0;
}
void mos_img_free(mos_img_t *i) { free(i->px); i->px = 0; }
int mos_img_encode_jpeg(const unsigned *px, int w, int h, int q, unsigned char **out, size_t *n) {
    (void)px; (void)q; *n = (size_t)(w * h + 100); *out = malloc(*n); return 0;
}
void cx_cm_audio_sample_dummy(void);
void *cx_cm_audio_sample(const short *pcm, unsigned frames, double rate) { (void)pcm; (void)rate; return cx_num_i(frames); }
int avf_device_kinds(int *out, int cap) { if (cap > 0) out[0] = 1; return 1; }
int avf_cap_start(int mic, int cam, int *err) { (void)cam; *err = 0; return mic; }
unsigned avf_cap_pull(int running, short *dst, unsigned frames, int *err) { (void)running; (void)dst; (void)frames; *err = 0; return 0; }
int cx_media_tcc(const char *s) { (void)s; return 1; }
int cx_tcc_preflight(const char *s) { (void)s; return 1; }
void cx_tcc_request_async(const char *s, void (*cb)(int, void *), void *ctx) { (void)s; cb(1, ctx); }
void *dispatch_get_main_queue(void) { return (void *)2; }
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *)) { (void)q; fn(ctx); }

/* ---------------------------------------------------------------- câmera falsa */
static gui_camera_info_t cam;
static int cam_open, cam_reads, cam_frames_left, cam_gone, opens, closes;
static unsigned long long open_wh, open_fi;
static unsigned char frame[64];
static int frame_len;
long cx_syscall(long n, long a, long b, long c, long d, long e) {
    (void)e;
    if (n != 79) return -1;
    switch (a) {
    case DEV_CAMERA_INFO:
        if (b != 0 || cam_gone) return 0;
        memcpy((void *)c, &cam, sizeof cam);
        return 1;
    case DEV_CAMERA_OPEN: {
        opens++; open_wh = (unsigned long long)c; open_fi = (unsigned long long)d; cam_open = 1;
        unsigned fcc = (unsigned)d, w = (unsigned)c, h = (unsigned)((unsigned long long)c >> 32);
        for (int i = 0; i < cam.nmode; i++) if (cam.mode[i].fourcc == fcc && cam.mode[i].width == w && cam.mode[i].height == h) cam.cur = cam.mode[i];
        cam.cur_interval = (unsigned)((unsigned long long)d >> 32);
        cam.open_pid = 1;
        return 0;
    }
    case DEV_CAMERA_READ:
        cam_reads++;
        if (cam_gone) return -6;
        if (!cam_frames_left) return 0;
        cam_frames_left--;
        memcpy((void *)c, frame, (size_t)frame_len);
        return frame_len;
    case DEV_CAMERA_CLOSE: closes++; cam_open = 0; cam.open_pid = 0; return 0;
    }
    return -1;
}

/* ---------------------------------------------------------------- delegate */
static volatile int got_frames, got_drops, got_photo, got_finish;
static sb_t *last_sb; static void *last_photo;
static void d_out(void *s, const char *c, void *out, sb_t *sb, void *conn) { (void)s; (void)c; (void)out; (void)conn; last_sb = sb; got_frames++; }
static void d_drop(void *s, const char *c, void *out, sb_t *sb, void *conn) { (void)s; (void)c; (void)out; (void)sb; (void)conn; got_drops++; }
static void d_photo(void *s, const char *c, void *out, void *photo, void *err) { (void)s; (void)c; (void)out; (void)err; last_photo = photo; got_photo++; }
static void d_finish(void *s, const char *c, void *out, void *rs, void *err) { (void)s; (void)c; (void)out; (void)rs; (void)err; got_finish++; }

static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
#define CALL(o, sel, R, ...) ((R (*)())imp(*(void **)(o), sel))(o, sel, ##__VA_ARGS__)
#define CCALL(k, sel, R, ...) ((R (*)())imp(k, sel))(k, sel, ##__VA_ARGS__)
static void wait_for(volatile int *v, int n) { for (int i = 0; i < 2000 && *v < n; i++) usleep(1000); }
static int posted_has(const char *n) { for (int i = 0; i < nposted; i++) if (!strcmp(posted[i], n)) return 1; return 0; }

int main(void) {
    /* câmera: YUY2 4x2 a 30 qps e MJPEG 8x4 de 30 a 5 qps */
    snprintf(cam.name, sizeof cam.name, "Webcam de Teste");
    snprintf(cam.path, sizeof cam.path, "3");
    cam.vid = 0x046d; cam.pid = 0x0825; cam.nmode = 2;
    cam.mode[0] = (gui_camera_mode_t){0x32595559, 4, 2, 333333, 333333, 333333, 16};
    cam.mode[1] = (gui_camera_mode_t){0x47504a4d, 8, 4, 333333, 2000000, 333333, 4096};
    /* YUY2 4x2: branco, preto | vermelho, cinza; segunda linha igual */
    unsigned char y[16] = {235, 128, 16, 128, 81, 90, 126, 240, 235, 128, 16, 128, 81, 90, 126, 240};
    memcpy(frame, y, 16); frame_len = 16;
    cx_define_class(LAYERC_KEY, LAYERM, "CALayer", &NSObj, 16);
    cx_define_class(DELC, DELM, "Del", &NSObj, 0);
    cx_add_method(DELC, "captureOutput:didOutputSampleBuffer:fromConnection:", (void *)d_out);
    cx_add_method(DELC, "captureOutput:didDropSampleBuffer:fromConnection:", (void *)d_drop);
    cx_add_method(DELC, "captureOutput:didFinishProcessingPhoto:error:", (void *)d_photo);
    cx_add_method(DELC, "captureOutput:didFinishCaptureForResolvedSettings:error:", (void *)d_finish);
    void *del = cx_alloc_instance(DELC);
    cx_avcapture_init();

    void *vide = cx_str("vide"), *soun = cx_str("soun");
    arr_t *cams = CCALL(AVCaptureDevice_c, "devicesWithMediaType:", arr_t *, vide);
    arr_t *mics = CCALL(AVCaptureDevice_c, "devicesWithMediaType:", arr_t *, soun);
    expect(cams->n == 1 && mics->n == 1, "uma câmera e um microfone");
    void *dev = cams->it[0];
    expect(!strcmp(cx_cstr(CALL(dev, "localizedName", void *)), "Webcam de Teste") && !strcmp(cx_cstr(CALL(dev, "uniqueID", void *)), "uvc-3-046d0825"), "nome e uniqueID da câmera");
    expect(CCALL(AVCaptureDevice_c, "defaultDeviceWithMediaType:", void *, vide) == dev &&
           CCALL(AVCaptureDevice_c, "deviceWithUniqueID:", void *, cx_str("uvc-3-046d0825")) == dev, "o mesmo objeto em toda consulta");
    expect(CALL(dev, "hasMediaType:", signed char, vide) && !CALL(dev, "hasMediaType:", signed char, soun) && CALL(dev, "transportType", int) == 0x75736220, "mídia e transporte USB");
    arr_t *fmts = CALL(dev, "formats", arr_t *);
    expect(fmts->n == 2, "dois formatos");
    arr_t *rr = CALL(fmts->it[1], "videoSupportedFrameRateRanges", arr_t *);
    double maxr = CALL(rr->it[0], "maxFrameRate", double), minr = CALL(rr->it[0], "minFrameRate", double);
    expect(maxr > 29.9 && maxr < 30.1 && minr > 4.9 && minr < 5.1, "faixa de 5 a 30 qps");
    cmt_t md = ((cmt_t (*)(void *, const char *))imp(*(void **)rr->it[0], "minFrameDuration"))(rr->it[0], "");
    expect(md.value == 333333 && md.timescale == 10000000, "minFrameDuration");

    /* sessão com formato escolhido à mão (YUY2) e saída BGRA */
    void *sess = cx_alloc_instance(AVCaptureSession_c);
    CALL(sess, "init", void *);
    CALL(dev, "lockForConfiguration:", signed char, (void *)0);
    CALL(dev, "setActiveFormat:", void, fmts->it[0]);
    CALL(dev, "unlockForConfiguration", void);
    expect(CALL(dev, "activeFormat", void *) == fmts->it[0], "activeFormat");
    void *in = CCALL(AVCaptureDeviceInput_c, "deviceInputWithDevice:error:", void *, dev, (void **)0);
    void *out = cx_alloc_instance(AVCaptureVideoDataOutput_c);
    CALL(out, "init", void *);
    dict_t *st = f_dict(0, 0);
    f_dset(st, 0, cx_num_i(0x42475241), (void *)kCVPixelBufferPixelFormatTypeKey);
    CALL(out, "setVideoSettings:", void, st);
    CALL(out, "setSampleBufferDelegate:queue:", void, del, (void *)3);
    expect(CALL(sess, "canAddInput:", signed char, in) && CALL(sess, "canAddOutput:", signed char, out), "pode adicionar");
    CALL(sess, "addInput:", void, in);
    CALL(sess, "addOutput:", void, out);
    expect(!CALL(sess, "canAddOutput:", signed char, out) && CALL(sess, "outputs", arr_t *)->n == 1, "não repete a saída");
    cam_frames_left = 1;
    CALL(sess, "startRunning", void);
    expect(CALL(sess, "isRunning", signed char) && opens == 1 && (unsigned)open_fi == 0x32595559 && open_wh == (4ull | 2ull << 32) && (open_fi >> 32) == 333333, "abre o modo YUY2 4x2 a 30 qps");
    wait_for(&got_frames, 1);
    expect(got_frames == 1 && last_sb && last_sb->pb->fmt == 0x42475241 && last_sb->pb->w == 4 && last_sb->pb->h == 2, "delegate recebe BGRA 4x2");
    unsigned char *px = last_sb->pb->base[0];
    expect(px[0] == 255 && px[1] == 255 && px[2] == 255 && px[4] == 0 && px[10] >= 250 && px[9] <= 5, "pixels convertidos (branco, preto, vermelho)");
    expect(last_sb->pts.value == 42 && posted_has("AVCaptureSessionDidStartRunningNotification"), "pts no relógio do host e aviso de início");

    /* espelho e saída nativa (2vuy) */
    void *conn = CALL(out, "connectionWithMediaType:", void *, vide);
    CALL(conn, "setVideoMirrored:", void, (signed char)1);
    CALL(out, "setVideoSettings:", void, (void *)0);
    cam_frames_left = 1;
    wait_for(&got_frames, 2);
    expect(got_frames == 2 && last_sb->pb->fmt == 0x32767579, "sem videoSettings: 2vuy");
    unsigned char *q = last_sb->pb->base[0];
    expect(q[1] == 126 && q[3] == 81 && q[5] == 16 && q[7] == 235, "espelhado: Y na ordem inversa");

    /* foto */
    void *pho = cx_alloc_instance(AVCapturePhotoOutput_c);
    CALL(sess, "addOutput:", void, pho);
    void *ps = CCALL(AVCapturePhotoSettings_c, "photoSettings", void *);
    CALL(pho, "capturePhotoWithSettings:delegate:", void, ps, del);
    cam_frames_left = 1;
    wait_for(&got_finish, 1);
    pb_t *ppb = last_photo ? CALL(last_photo, "pixelBuffer", pb_t *) : 0;
    num_t *jpg = last_photo ? CALL(last_photo, "fileDataRepresentation", num_t *) : 0;
    expect(got_photo == 1 && got_finish == 1 && ppb && ppb->fmt == 0x42475241 && ppb->w == 4 && jpg && jpg->v == 108, "foto: BGRA e JPEG");

    /* prévia */
    void *layer = CCALL(cx_class_named("AVCaptureVideoPreviewLayer"), "layerWithSession:", void *, sess);
    expect(layer && CALL(layer, "session", void *) == sess && !strcmp(layer_gravity, "resizeAspect"), "AVCaptureVideoPreviewLayer com a sessão");
    cam_frames_left = 1;
    wait_for(&got_frames, 4);
    void *pvst = 0;
    for (int i = 0; i < nassoc; i++) if (assoc[i].o == layer) pvst = assoc[i].v;
    CALL(pvst, "tick:", void, (void *)0);
    expect(layer_contents != 0, "o tick põe o quadro em contents");

    CALL(sess, "stopRunning", void);
    expect(!CALL(sess, "isRunning", signed char) && closes == 1 && !cam_open && posted_has("AVCaptureSessionDidStopRunningNotification"), "stopRunning fecha a câmera");

    /* MJPEG pelo preset: 1280x720 (o mais perto é o 8x4) e decodificação */
    CALL(sess, "setSessionPreset:", void, cx_str("AVCaptureSessionPreset1280x720"));
    unsigned char jf[8] = {0xFF, 0xD8, 8, 4, 10, 20, 30, 255};
    memcpy(frame, jf, 8); frame_len = 8;
    dict_t *st2 = f_dict(0, 0);
    f_dset(st2, 0, cx_num_i(0x34323076), (void *)kCVPixelBufferPixelFormatTypeKey);
    CALL(out, "setVideoSettings:", void, st2);
    CALL(conn, "setVideoMirrored:", void, (signed char)0);
    int before = got_frames;
    cam_frames_left = 1;
    CALL(sess, "startRunning", void);
    expect(opens == 2 && (unsigned)open_fi == 0x47504a4d && open_wh == (8ull | 4ull << 32), "preset escolhe o MJPEG 8x4");
    wait_for(&got_frames, before + 1);
    expect(got_frames == before + 1 && last_sb->pb->fmt == 0x34323076 && last_sb->pb->np == 2 && last_sb->pb->w == 8, "MJPEG decodificado vira 420v");

    /* câmera desligada no meio */
    cam_gone = 1;
    for (int i = 0; i < 2000 && !posted_has("AVCaptureDeviceWasDisconnectedNotification"); i++) usleep(1000);
    expect(posted_has("AVCaptureDeviceWasDisconnectedNotification") && posted_has("AVCaptureSessionRuntimeErrorNotification"), "desconexão avisa");
    CALL(sess, "stopRunning", void);
    expect(CCALL(AVCaptureDevice_c, "devicesWithMediaType:", arr_t *, vide)->n == 0 && !CALL(dev, "isConnected", signed char), "some da lista");

    printf("avcapture: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
