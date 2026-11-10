/*
 * mOS libSystem — mídia, impressão e autenticação que o Electron/VS Code importa (inventário do
 * tools/machocheck.c):
 *  - Accelerate/vDSP: implementado de verdade (o WebAudio do Chromium faz convolução, FFT real e
 *    filtros biquad com ele). FFT real com o empacotamento e a escala da Apple (direta multiplica por 2).
 *  - CoreMedia: CMTime aqui; bloco, formato e sample buffer em openbot_cm.c.
 *  - VideoToolbox: H.264 em software (I_PCM). HEVC devolve -12906/-12908. A cópia de pixel está em openbot_vt.c.
 *  - CoreAudio/AudioToolbox/CoreMIDI: nenhum dispositivo/componente (áudio do Chromium fica mudo).
 *  - CUPS: nenhuma impressora. GSS/Kerberos: sem credenciais (autenticação Negotiate falha limpa).
 *  - UniformTypeIdentifiers: objetos UTType com identificador, extensão e MIME.
 *  - ServiceManagement: itens de login aceitos; sem launchd para jobs.
 */
#include "foundation.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
int strcmp(const char *a, const char *b);
double sin(double), cos(double), sqrt(double), fabs(double);
id objc_retain(id o);
void objc_release(id o);
id CFArrayCreate(void *alloc, const id *vals, long n, const void *cb);
id CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
int clock_gettime(int clk, void *ts);
void cx_objc_boot(unsigned long long sects, unsigned long long n);
typedef int OSStatus;
#define PI 3.14159265358979323846

/* ================================================================ Accelerate: vDSP */
typedef struct { float *realp, *imagp; } dsp_split_t;
typedef struct { float real, imag; } dsp_complex_t;
typedef long vstride;
typedef unsigned long vlen;
void vDSP_vadd(const float *a, vstride ia, const float *b, vstride ib, float *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] + b[i * ib]; }
/* vDSP_vsub(B, IB, A, IA, C, IC, N): C = A - B */
void vDSP_vsub(const float *b, vstride ib, const float *a, vstride ia, float *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] - b[i * ib]; }
void vDSP_vmul(const float *a, vstride ia, const float *b, vstride ib, float *c, vstride ic, vlen n) { for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] * b[i * ib]; }
void vDSP_vsmul(const float *a, vstride ia, const float *b, float *c, vstride ic, vlen n) { float s = *b; for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] * s; }
void vDSP_vsadd(const float *a, vstride ia, const float *b, float *c, vstride ic, vlen n) { float s = *b; for (vlen i = 0; i < n; i++) c[i * ic] = a[i * ia] + s; }
void vDSP_vsma(const float *a, vstride ia, const float *b, const float *c, vstride ic, float *d, vstride id_, vlen n) { float s = *b; for (vlen i = 0; i < n; i++) d[i * id_] = a[i * ia] * s + c[i * ic]; }
void vDSP_vclip(const float *a, vstride ia, const float *lo, const float *hi, float *c, vstride ic, vlen n) {
    float l = *lo, h = *hi;
    for (vlen i = 0; i < n; i++) { float v = a[i * ia]; c[i * ic] = v < l ? l : v > h ? h : v; }
}
void vDSP_maxmgv(const float *a, vstride ia, float *c, vlen n) {
    float m = 0;
    for (vlen i = 0; i < n; i++) { float v = a[i * ia]; if (v < 0) v = -v; if (v > m) m = v; }
    *c = m;
}
void vDSP_svesq(const float *a, vstride ia, float *c, vlen n) { float s = 0; for (vlen i = 0; i < n; i++) s += a[i * ia] * a[i * ia]; *c = s; }
/* C[n] = soma_p A[n+p] * F[p] (com IF negativo é convolução: F aponta para o último elemento) */
void vDSP_conv(const float *a, vstride ia, const float *f, vstride if_, float *c, vstride ic, vlen n, vlen p) {
    for (vlen i = 0; i < n; i++) {
        float s = 0;
        for (vlen k = 0; k < p; k++) s += a[(i + k) * ia] * f[(long)k * if_];
        c[i * ic] = s;
    }
}
/* multiplicação complexa dividida; conj = -1 conjuga A */
void vDSP_zvmul(const dsp_split_t *a, vstride ia, const dsp_split_t *b, vstride ib, const dsp_split_t *c, vstride ic, vlen n, int conj) {
    for (vlen i = 0; i < n; i++) {
        float ar = a->realp[i * ia], ai = a->imagp[i * ia] * (conj == -1 ? -1.0f : 1.0f);
        float br = b->realp[i * ib], bi = b->imagp[i * ib];
        c->realp[i * ic] = ar * br - ai * bi;
        c->imagp[i * ic] = ar * bi + ai * br;
    }
}
void vDSP_ctoz(const dsp_complex_t *c, vstride ic, const dsp_split_t *z, vstride iz, vlen n) {
    const float *f = (const float *)c;
    for (vlen i = 0; i < n; i++) { z->realp[i * iz] = f[i * ic]; z->imagp[i * iz] = f[i * ic + 1]; }
}
void vDSP_ztoc(const dsp_split_t *z, vstride iz, dsp_complex_t *c, vstride ic, vlen n) {
    float *f = (float *)c;
    for (vlen i = 0; i < n; i++) { f[i * ic] = z->realp[i * iz]; f[i * ic + 1] = z->imagp[i * iz]; }
}
/* biquad em dupla precisão: A e C têm N+2 elementos (os dois primeiros são o estado inicial) */
void vDSP_deq22D(const double *a, vstride ia, const double *b, double *c, vstride ic, vlen n) {
    for (vlen i = 2; i < n + 2; i++)
        c[i * ic] = a[i * ia] * b[0] + a[(i - 1) * ia] * b[1] + a[(i - 2) * ia] * b[2] - c[(i - 1) * ic] * b[3] - c[(i - 2) * ic] * b[4];
}
typedef struct { unsigned long log2n; } fft_setup_t;
void *vDSP_create_fftsetup(unsigned long log2n, int radix) { (void)radix; fft_setup_t *s = malloc(sizeof *s); if (s) s->log2n = log2n; return s; }
void vDSP_destroy_fftsetup(void *s) { free(s); }
/* FFT complexa radix-2 in-place (sinal: -1 direta, +1 inversa), sem escala */
static void fft_c(double *re, double *im, unsigned long n, int sign) {
    for (unsigned long i = 1, j = 0; i < n; i++) {
        unsigned long bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (unsigned long len = 2; len <= n; len <<= 1) {
        double ang = sign * 2 * PI / (double)len, wr = cos(ang), wi = sin(ang);
        for (unsigned long i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (unsigned long k = 0; k < len / 2; k++) {
                unsigned long a = i + k, b = a + len / 2;
                double tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr; im[a] += ti;
                double nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}
/* FFT real "zrip" da Apple. Entrada/saída empacotadas: realp[0] = termo DC, imagp[0] = termo de Nyquist.
 * Direta (dir = +1): saída = 2 * DFT. Inversa (dir = -1): sem escala (ida e volta = 2N * x). */
void vDSP_fft_zrip(void *setup, const dsp_split_t *c, vstride ic, unsigned long log2n, int dir) {
    (void)setup;
    unsigned long n = 1ul << log2n, h = n / 2;
    double *re = malloc(sizeof(double) * n), *im = malloc(sizeof(double) * n);
    if (!re || !im) { free(re); free(im); return; }
    if (dir >= 0) {
        for (unsigned long k = 0; k < h; k++) { re[2 * k] = c->realp[k * ic]; re[2 * k + 1] = c->imagp[k * ic]; im[2 * k] = im[2 * k + 1] = 0; }
        fft_c(re, im, n, -1);
        c->realp[0] = (float)(2 * re[0]);
        c->imagp[0] = (float)(2 * re[h]);
        for (unsigned long k = 1; k < h; k++) { c->realp[k * ic] = (float)(2 * re[k]); c->imagp[k * ic] = (float)(2 * im[k]); }
    } else {
        re[0] = c->realp[0]; im[0] = 0;
        re[h] = c->imagp[0]; im[h] = 0;
        for (unsigned long k = 1; k < h; k++) {
            re[k] = c->realp[k * ic]; im[k] = c->imagp[k * ic];
            re[n - k] = re[k]; im[n - k] = -im[k];
        }
        fft_c(re, im, n, +1);
        for (unsigned long k = 0; k < h; k++) { c->realp[k * ic] = (float)re[2 * k]; c->imagp[k * ic] = (float)re[2 * k + 1]; }
    }
    free(re); free(im);
}

/* ================================================================ CoreMedia */
typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cm_time_t;
#define kCMTimeFlags_Valid 1u
cm_time_t CMTimeMake(i64 value, i32 timescale) { cm_time_t t = {value, timescale, kCMTimeFlags_Valid, 0}; return t; }
double CMTimeGetSeconds(cm_time_t t) {
    if (!(t.flags & kCMTimeFlags_Valid) || !t.timescale) return __builtin_nan("");
    return (double)t.value / (double)t.timescale;
}
u64 CMClockConvertHostTimeToSystemUnits(cm_time_t t) {
    if (!(t.flags & kCMTimeFlags_Valid) || !t.timescale) return 0;
    return (u64)((double)t.value * 1e9 / (double)t.timescale);    /* mach_absolute_time em ns no mOS */
}
/* Bloco, formato e sample buffer: openbot_cm.c. */
OSStatus CMIOObjectGetPropertyData(u32 obj, const void *addr, u32 qs, const void *q, u32 sz, u32 *used, void *data) { (void)obj; (void)addr; (void)qs; (void)q; (void)sz; (void)data; if (used) *used = 0; return 0x77686F3F; /* kCMIOHardwareUnknownPropertyError 'who?' */ }

/* ================================================================ VideoToolbox
 * H.264 em software (I_PCM, h264sw.c): CVPixelBuffer BGRA ou NV12 entra e sai,
 * CMSampleBuffer em AVCC com o format description avc1. HEVC continua -12906/-12908:
 * não há par encoder/decoder permissivo que caiba aqui. Sem FairPlay.
 * VTIsHardwareDecodeSupported continua 0. */
#define kVTCouldNotFindVideoDecoderErr (-12906)
#define kVTCouldNotFindVideoEncoderErr (-12908)
#define kVTPropertyNotSupportedErr (-12900)
#define kCMVideoCodecType_H264 0x61766331u
#define kCMVideoCodecType_HEVC 0x68766331u
typedef void (*vt_enc_fn)(void *ref, void *src, OSStatus st, u32 flags, void *sb);
typedef void (*vt_dec_fn)(void *ref, void *src, OSStatus st, u32 flags, void *img, cm_time_t pts, cm_time_t dur);
typedef struct { vt_dec_fn fn; void *ref; } vt_dec_rec;
typedef struct { cm_time_t duration, presentationTimeStamp, decodeTimeStamp; } vt_timing_t;
enum { VT_ENC = 1, VT_DEC = 2 };
typedef struct {
    cx_class_t *isa;
    int kind, dead, w, h;
    u32 codec;
    vt_enc_fn enc;
    void *enc_ref;
    vt_dec_fn dec;
    void *dec_ref;
} vts_t;
static cx_class_t VTS_c, VTS_m;
static int vts_ready;
static void vts_dealloc(id s, SEL c) {
    (void)s;
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static void vts_ensure(void) {
    if (vts_ready) return;
    cx_objc_boot(0, 0);
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob) return;
    cx_define_class(&VTS_c, &VTS_m, "VTSession", ob, (u32)(sizeof(vts_t) - sizeof(void *)));
    cx_add_method(&VTS_c, "dealloc", (void *)vts_dealloc);
    vts_ready = 1;
}
static vts_t *vts_new(int kind) {
    vts_ensure();
    vts_t *s = (vts_t *)cx_alloc_instance(&VTS_c);
    if (s) s->kind = kind;
    return s;
}
static vts_t *vts_as(void *p, int kind) {
    if (!p || ((unsigned long long)p & 7)) return 0;
    vts_t *s = (vts_t *)p;
    if (s->isa != &VTS_c || s->kind != kind || s->dead) return 0;
    return s;
}
static int h264_codec(u32 c) { return c == kCMVideoCodecType_H264 || c == 0x61766333u; }
static int hevc_codec(u32 c) { return c == kCMVideoCodecType_HEVC || c == 0x68657631u; }

int CVPixelBufferLockBaseAddress(void *p, u64 f);
int CVPixelBufferUnlockBaseAddress(void *p, u64 f);
void *CVPixelBufferGetBaseAddress(void *p);
void *CVPixelBufferGetBaseAddressOfPlane(void *p, u64 i);
u64 CVPixelBufferGetBytesPerRow(void *p);
u64 CVPixelBufferGetBytesPerRowOfPlane(void *p, u64 i);
u64 CVPixelBufferGetWidth(void *p);
u64 CVPixelBufferGetHeight(void *p);
u32 CVPixelBufferGetPixelFormatType(void *p);
u64 CVPixelBufferGetPlaneCount(void *p);
int CVPixelBufferCreate(void *a, u64 w, u64 h, u32 fmt, id attrs, void **out);
int CMBlockBufferCreateWithMemoryBlock(void *a, void *mem, size_t len, void *ba, const void *cb, size_t off, size_t dlen, u32 flags, void **out);
int CMVideoFormatDescriptionCreateFromH264ParameterSets(void *a, size_t n, const u8 *const *p, const size_t *s, int nal, void **out);
int CMSampleBufferCreate(void *a, void *db, u8 ready, void *cb, void *ref, void *fd, long ns, long nt, const void *t, long nsz, const size_t *sz, void **out);
void *CMSampleBufferGetDataBuffer(void *s);
void *CMSampleBufferGetFormatDescription(void *s);
int CMBlockBufferGetDataPointer(void *b, size_t off, size_t *lenAt, size_t *total, char **ptr);
int CMVideoFormatDescriptionGetH264ParameterSetAtIndex(void *d, size_t i, const u8 **p, size_t *s, size_t *n, int *nal);
typedef struct { u8 *sps, *pps, *sample, *annexb; int sps_n, pps_n, sample_n, annexb_n, w, h; } h264_au;
int h264_encode_i420(const u8 *y, const u8 *u, const u8 *v, int w, int h, int sy, int su, int sv, int qp, h264_au *o);
void h264_au_free(h264_au *o);
int h264_decode_avcc(const u8 *sps, int sps_n, const u8 *pps, int pps_n, const u8 *sample, int sample_n,
                     u8 *y, u8 *u, u8 *v, int sy, int su, int sv, int *w, int *h);
int h264_sps_wh(const u8 *nal, int n, int *w, int *h);
void h264_bgra_to_i420(const u8 *bgra, int w, int h, int sb, u8 *y, u8 *u, u8 *v, int sy, int su, int sv);
void h264_i420_to_bgra(const u8 *y, const u8 *u, const u8 *v, int w, int h, int sy, int su, int sv, u8 *bgra, int sb);
void h264_nv12_to_i420(const u8 *yp, int sy, const u8 *uv, int suv, int w, int h, u8 *y, u8 *u, u8 *v, int dy, int du, int dv);
void h264_i420_to_nv12(const u8 *y, const u8 *u, const u8 *v, int w, int h, int sy, int su, int sv, u8 *yp, int dy, u8 *uv, int duv);

u8 VTIsHardwareDecodeSupported(u32 codec) { (void)codec; return 0; }
void VTRegisterSupplementalVideoDecoderIfAvailable(u32 codec) { (void)codec; }

OSStatus VTDecompressionSessionCreate(void *a, void *fd, id spec, id attrs, const void *cb, void **out) {
    (void)a; (void)spec; (void)attrs;
    if (out) *out = 0;
    if (!out || !cb) return kVTCouldNotFindVideoDecoderErr;
    u32 codec = 0;
    if (fd) {
        typedef struct { void *isa; u32 kind, media, subtype; } fd_peek;
        codec = ((fd_peek *)fd)->subtype;
    }
    if (hevc_codec(codec)) return kVTCouldNotFindVideoDecoderErr;
    if (codec && !h264_codec(codec)) return kVTCouldNotFindVideoDecoderErr;
    const vt_dec_rec *rec = (const vt_dec_rec *)cb;
    vts_t *s = vts_new(VT_DEC);
    if (!s) return kVTCouldNotFindVideoDecoderErr;
    s->codec = codec ? codec : kCMVideoCodecType_H264;
    s->dec = rec->fn;
    s->dec_ref = rec->ref;
    *out = s;
    return 0;
}
u8 VTDecompressionSessionCanAcceptFormatDescription(void *s, void *fd) {
    vts_t *v = vts_as(s, VT_DEC);
    if (!v || !fd) return 0;
    typedef struct { void *isa; u32 kind, media, subtype; } fd_peek;
    return h264_codec(((fd_peek *)fd)->subtype);
}
OSStatus VTDecompressionSessionDecodeFrame(void *s, void *sb, u32 flags, void *ref, u32 *info) {
    (void)flags;
    if (info) *info = 0;
    vts_t *v = vts_as(s, VT_DEC);
    if (!v) return kVTCouldNotFindVideoDecoderErr;
    void *bb = CMSampleBufferGetDataBuffer(sb);
    void *fmt = CMSampleBufferGetFormatDescription(sb);
    const u8 *sps = 0, *pps = 0;
    size_t sps_n = 0, pps_n = 0, np = 0;
    if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(fmt, 0, &sps, &sps_n, &np, 0) ||
        CMVideoFormatDescriptionGetH264ParameterSetAtIndex(fmt, 1, &pps, &pps_n, &np, 0)) {
        if (v->dec) v->dec(v->dec_ref, ref, kVTCouldNotFindVideoDecoderErr, 0, 0, CMTimeMake(0, 1), CMTimeMake(0, 1));
        return kVTCouldNotFindVideoDecoderErr;
    }
    char *data = 0;
    size_t dlen = 0;
    if (CMBlockBufferGetDataPointer(bb, 0, 0, &dlen, &data) || !data) return kVTCouldNotFindVideoDecoderErr;
    int w = 0, h = 0;
    if (h264_sps_wh(sps, (int)sps_n, &w, &h) || w < 2 || h < 2 || w > 4096 || h > 4096) return kVTCouldNotFindVideoDecoderErr;
    u8 *y = malloc((size_t)w * h), *u = malloc((size_t)w * h / 4), *v2 = malloc((size_t)w * h / 4);
    if (!y || !u || !v2) { free(y); free(u); free(v2); return kVTCouldNotFindVideoDecoderErr; }
    int rc = h264_decode_avcc(sps, (int)sps_n, pps, (int)pps_n, (const u8 *)data, (int)dlen, y, u, v2, w, w / 2, w / 2, &w, &h);
    if (rc || w < 2 || h < 2) { free(y); free(u); free(v2); return kVTCouldNotFindVideoDecoderErr; }
    void *img = 0;
    if (CVPixelBufferCreate(0, (u64)w, (u64)h, 0x42475241u, 0, &img) != 0 || !img) {
        free(y); free(u); free(v2);
        return kVTCouldNotFindVideoDecoderErr;
    }
    CVPixelBufferLockBaseAddress(img, 0);
    h264_i420_to_bgra(y, u, v2, w, h, w, w / 2, w / 2, CVPixelBufferGetBaseAddress(img), (int)CVPixelBufferGetBytesPerRow(img));
    CVPixelBufferUnlockBaseAddress(img, 0);
    free(y); free(u); free(v2);
    if (v->dec) v->dec(v->dec_ref, ref, 0, 0, img, CMTimeMake(0, 1), CMTimeMake(0, 1));
    objc_release((id)img);
    return 0;
}
void VTDecompressionSessionInvalidate(void *s) { vts_t *v = vts_as(s, VT_DEC); if (v) v->dead = 1; }

OSStatus VTCompressionSessionCreate(void *a, i32 w, i32 h, u32 codec, id spec, id src, void *ca, void *cb, void *ref, void **out) {
    (void)a; (void)spec; (void)src; (void)ca;
    if (out) *out = 0;
    if (!out || w < 2 || h < 2) return kVTCouldNotFindVideoEncoderErr;
    if (hevc_codec(codec) || !h264_codec(codec)) return kVTCouldNotFindVideoEncoderErr;
    vts_t *s = vts_new(VT_ENC);
    if (!s) return kVTCouldNotFindVideoEncoderErr;
    s->w = w; s->h = h; s->codec = codec; s->enc = (vt_enc_fn)cb; s->enc_ref = ref;
    *out = s;
    return 0;
}
OSStatus VTCompressionSessionEncodeFrame(void *s, void *img, cm_time_t pts, cm_time_t dur, id props, void *ref, u32 *info) {
    (void)props;
    if (info) *info = 0;
    vts_t *v = vts_as(s, VT_ENC);
    if (!v || !img) return kVTCouldNotFindVideoEncoderErr;
    int w = (int)CVPixelBufferGetWidth(img), h = (int)CVPixelBufferGetHeight(img);
    if (w < 2 || h < 2 || (w & 1) || (h & 1)) return kVTCouldNotFindVideoEncoderErr;
    u8 *y = malloc((size_t)w * h), *u = malloc((size_t)w * h / 4), *vv = malloc((size_t)w * h / 4);
    if (!y || !u || !vv) { free(y); free(u); free(vv); return kVTCouldNotFindVideoEncoderErr; }
    CVPixelBufferLockBaseAddress(img, 1);
    u32 fmt = CVPixelBufferGetPixelFormatType(img);
    if (CVPixelBufferGetPlaneCount(img) >= 2 && (fmt == 0x34323076u || fmt == 0x34323066u)) {
        h264_nv12_to_i420(CVPixelBufferGetBaseAddressOfPlane(img, 0), (int)CVPixelBufferGetBytesPerRowOfPlane(img, 0),
                          CVPixelBufferGetBaseAddressOfPlane(img, 1), (int)CVPixelBufferGetBytesPerRowOfPlane(img, 1),
                          w, h, y, u, vv, w, w / 2, w / 2);
    } else {
        h264_bgra_to_i420(CVPixelBufferGetBaseAddress(img), w, h, (int)CVPixelBufferGetBytesPerRow(img), y, u, vv, w, w / 2, w / 2);
    }
    CVPixelBufferUnlockBaseAddress(img, 1);
    h264_au au;
    int erc = h264_encode_i420(y, u, vv, w, h, w, w / 2, w / 2, 16, &au);
    free(y); free(u); free(vv);
    if (erc) { h264_au_free(&au); return kVTCouldNotFindVideoEncoderErr; }
    void *bb = 0, *fd = 0, *sb = 0;
    const u8 *sets[2] = {au.sps, au.pps};
    size_t slen[2] = {(size_t)au.sps_n, (size_t)au.pps_n};
    size_t sample_n = (size_t)au.sample_n;
    if (CMBlockBufferCreateWithMemoryBlock(0, au.sample, sample_n, 0, 0, 0, sample_n, 0, &bb) ||
        CMVideoFormatDescriptionCreateFromH264ParameterSets(0, 2, sets, slen, 4, &fd)) {
        h264_au_free(&au); if (bb) objc_release((id)bb);
        return kVTCouldNotFindVideoEncoderErr;
    }
    vt_timing_t tm;
    tm.duration = dur; tm.presentationTimeStamp = pts; tm.decodeTimeStamp = pts;
    int crc = CMSampleBufferCreate(0, bb, 1, 0, 0, fd, 1, 1, &tm, 1, &sample_n, &sb);
    objc_release((id)bb); objc_release((id)fd); h264_au_free(&au);
    if (crc || !sb) return kVTCouldNotFindVideoEncoderErr;
    if (v->enc) v->enc(v->enc_ref, ref, 0, 0, sb);
    objc_release((id)sb);
    return 0;
}
OSStatus VTCompressionSessionCompleteFrames(void *s, cm_time_t until) { (void)s; (void)until; return 0; }
void VTCompressionSessionInvalidate(void *s) { vts_t *v = vts_as(s, VT_ENC); if (v) v->dead = 1; }
OSStatus VTSessionSetProperty(void *s, id key, id val) { (void)s; (void)key; (void)val; return 0; }
OSStatus VTSessionCopyProperty(void *s, id key, void *a, void *out) { (void)s; (void)key; (void)a; if (out) *(void **)out = 0; return kVTPropertyNotSupportedErr; }
OSStatus VTSessionCopySupportedPropertyDictionary(void *s, id *out) { (void)s; if (out) *out = CFDictionaryCreateMutable(0, 0, 0, 0); return 0; }
/* VTPixelTransferSession*: openbot_vt.c copia o quadro quando o formato coincide. */

/* CoreAudio, AudioUnit, AudioQueue, AudioConverter, AudioFile e AudioServices: coreaudio.c */

/* ================================================================ CoreMIDI: nenhuma porta */
OSStatus MIDIClientCreate(id name, void *cb, void *ref, u32 *out) { (void)name; (void)cb; (void)ref; if (out) *out = 1; return 0; }
OSStatus MIDIClientDispose(u32 c) { (void)c; return 0; }
OSStatus MIDIInputPortCreate(u32 c, id name, void *cb, void *ref, u32 *out) { (void)c; (void)name; (void)cb; (void)ref; if (out) *out = 2; return 0; }
OSStatus MIDIOutputPortCreate(u32 c, id name, u32 *out) { (void)c; (void)name; if (out) *out = 3; return 0; }
OSStatus MIDIPortConnectSource(u32 p, u32 s, void *ref) { (void)p; (void)s; (void)ref; return -10830; /* kMIDIObjectNotFound */ }
unsigned long MIDIGetNumberOfSources(void) { return 0; }
unsigned long MIDIGetNumberOfDestinations(void) { return 0; }
u32 MIDIGetSource(unsigned long i) { (void)i; return 0; }
u32 MIDIGetDestination(unsigned long i) { (void)i; return 0; }
OSStatus MIDISend(u32 port, u32 dest, const void *list) { (void)port; (void)dest; (void)list; return -10830; }
OSStatus MIDIObjectGetIntegerProperty(u32 o, id p, i32 *v) { (void)o; (void)p; if (v) *v = 0; return -10830; }
OSStatus MIDIObjectGetStringProperty(u32 o, id p, id *v) { (void)o; (void)p; if (v) *v = 0; return -10830; }
/* MIDIPacketList: {u32 numPackets; MIDIPacket packet[]}, MIDIPacket: {u64 timeStamp; u16 length; u8 data[]} (packed) */
void *MIDIPacketListInit(void *list) { u8 *l = list; *(u32 *)l = 0; return l + 4; }
void *MIDIPacketListAdd(void *list, unsigned long size, void *cur, u64 time, unsigned long n, const u8 *data) {
    u8 *l = list, *p = cur;
    if (p + 10 + n > l + size) return 0;
    memcpy(p, &time, 8);
    u16 len = (u16)n;
    memcpy(p + 8, &len, 2);
    memcpy(p + 10, data, n);
    (*(u32 *)l)++;
    return p;
}

/* ================================================================ CUPS: nenhuma impressora */
typedef struct { char *name, *instance; int is_default, num_options; void *options; } cups_dest_t;
int cupsEnumDests(unsigned flags, int msec, int *cancel, unsigned type, unsigned mask, void *cb, void *data) { (void)flags; (void)msec; (void)cancel; (void)type; (void)mask; (void)cb; (void)data; return 1; }
cups_dest_t *cupsGetNamedDest(void *http, const char *name, const char *inst) { (void)http; (void)name; (void)inst; return 0; }
int cupsCopyDest(cups_dest_t *d, int n, cups_dest_t **out) { (void)d; (void)out; return n; }
void cupsFreeDests(int n, cups_dest_t *d) { (void)n; (void)d; }
void *cupsCopyDestInfo(void *http, cups_dest_t *d) { (void)http; (void)d; return 0; }
void cupsFreeDestInfo(void *info) { (void)info; }
int cupsCheckDestSupported(void *http, cups_dest_t *d, void *info, const char *o, const char *v) { (void)http; (void)d; (void)info; (void)o; (void)v; return 0; }
void *cupsFindDestSupported(void *http, cups_dest_t *d, void *info, const char *o) { (void)http; (void)d; (void)info; (void)o; return 0; }
void *cupsFindDestDefault(void *http, cups_dest_t *d, void *info, const char *o) { (void)http; (void)d; (void)info; (void)o; return 0; }
const char *cupsLocalizeDestValue(void *http, cups_dest_t *d, void *info, const char *o, const char *v) { (void)http; (void)d; (void)info; (void)o; return v; }
const char *cupsGetOption(const char *name, int n, void *opts) { (void)name; (void)n; (void)opts; return 0; }
int cupsLastError(void) { return 0x0400; /* IPP_STATUS_ERROR_NOT_FOUND */ }
const char *cupsLastErrorString(void) { return "Nenhuma impressora configurada"; }
const char *cupsServer(void) { return "/private/var/run/cupsd"; }
const char *cupsUser(void) { return "visitante"; }
const char *cupsUserAgent(void) { return "CUPS/2.3"; }
int ippPort(void) { return 631; }
void *httpConnect2(const char *host, int port, void *addr, int fam, int enc, int blocking, int msec, int *cancel) { (void)host; (void)port; (void)addr; (void)fam; (void)enc; (void)blocking; (void)msec; (void)cancel; return 0; }
void httpClose(void *h) { (void)h; }
void *cupsDoRequest(void *http, void *req, const char *res) { (void)http; (void)res; (void)req; return 0; }
int cupsSendRequest(void *http, void *req, const char *res, size_t len) { (void)http; (void)req; (void)res; (void)len; return 0x0400; }
int cupsWriteRequestData(void *http, const char *b, size_t n) { (void)http; (void)b; (void)n; return 0x0400; }
void *cupsGetResponse(void *http, const char *res) { (void)http; (void)res; return 0; }
int cupsCancelJob2(void *http, const char *name, int job, int purge) { (void)http; (void)name; (void)job; (void)purge; return 0x0400; }
/* IPP: as requisições nunca saem; criar/apagar funcionam, leituras não acham nada */
void *ippNew(void) { return calloc(1, 64); }
void *ippNewRequest(int op) { (void)op; return calloc(1, 64); }
void ippDelete(void *ipp) { free(ipp); }
int ippSetVersion(void *ipp, int maj, int min) { (void)ipp; (void)maj; (void)min; return 1; }
int ippGetStatusCode(void *ipp) { (void)ipp; return 0x0400; }
int ippValidateAttributes(void *ipp) { (void)ipp; return 1; }
void *ippFindAttribute(void *ipp, const char *n, int t) { (void)ipp; (void)n; (void)t; return 0; }
void *ippFirstAttribute(void *ipp) { (void)ipp; return 0; }
void *ippNextAttribute(void *ipp) { (void)ipp; return 0; }
void ippDeleteAttribute(void *ipp, void *a) { (void)ipp; (void)a; }
void *ippCopyAttribute(void *dst, void *a, int quick) { (void)dst; (void)a; (void)quick; return 0; }
static u8 ipp_attr_dummy[64];
void *ippAddInteger(void *ipp, int g, int t, const char *n, int v) { (void)ipp; (void)g; (void)t; (void)n; (void)v; return ipp_attr_dummy; }
void *ippAddBoolean(void *ipp, int g, const char *n, char v) { (void)ipp; (void)g; (void)n; (void)v; return ipp_attr_dummy; }
void *ippAddString(void *ipp, int g, int t, const char *n, const char *lang, const char *v) { (void)ipp; (void)g; (void)t; (void)n; (void)lang; (void)v; return ipp_attr_dummy; }
void *ippAddStrings(void *ipp, int g, int t, const char *n, int c, const char *lang, const char *const *v) { (void)ipp; (void)g; (void)t; (void)n; (void)c; (void)lang; (void)v; return ipp_attr_dummy; }
void *ippAddRange(void *ipp, int g, const char *n, int lo, int hi) { (void)ipp; (void)g; (void)n; (void)lo; (void)hi; return ipp_attr_dummy; }
void *ippAddCollection(void *ipp, int g, const char *n, void *v) { (void)ipp; (void)g; (void)n; (void)v; return ipp_attr_dummy; }
void *ippAddCollections(void *ipp, int g, const char *n, int c, const void **v) { (void)ipp; (void)g; (void)n; (void)c; (void)v; return ipp_attr_dummy; }
int ippGetCount(void *a) { (void)a; return 0; }
const char *ippGetName(void *a) { (void)a; return ""; }
int ippGetGroupTag(void *a) { (void)a; return 0; }
int ippGetInteger(void *a, int i) { (void)a; (void)i; return 0; }
const char *ippGetString(void *a, int i, const char **lang) { (void)a; (void)i; if (lang) *lang = 0; return 0; }
int ippGetRange(void *a, int i, int *hi) { (void)a; (void)i; if (hi) *hi = 0; return 0; }
int ippGetResolution(void *a, int i, int *y, int *units) { (void)a; (void)i; if (y) *y = 0; if (units) *units = 0; return 0; }
void *ippGetCollection(void *a, int i) { (void)a; (void)i; return 0; }
/* pwgMediaForSize: tamanho em centésimos de mm -> tamanho PWG conhecido (só A4 e Carta) */
typedef struct { const char *pwg, *legacy, *ppd; int width, length; } pwg_media_t;
pwg_media_t *pwgMediaForSize(int w, int l) {
    static pwg_media_t a4 = {"iso_a4_210x297mm", "a4", "A4", 21000, 29700}, letter = {"na_letter_8.5x11in", "na-letter", "Letter", 21590, 27940};
    if (w > 21300 || l > 28800) return &a4;
    return w >= 21300 ? &letter : (l > 28000 ? &a4 : &letter);
}

/* ================================================================ GSS-API / Kerberos: sem credenciais */
typedef struct { u32 length; void *elements; } gss_oid_desc;
gss_oid_desc __gss_krb5_mechanism_oid_desc = {9, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02"};
gss_oid_desc __gss_spnego_mechanism_oid_desc = {6, (void *)"\x2b\x06\x01\x05\x05\x02"};
gss_oid_desc __gss_ntlm_mechanism_oid_desc = {10, (void *)"\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a"};
gss_oid_desc __gss_c_nt_hostbased_service_oid_desc = {10, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04"};
gss_oid_desc __gss_c_nt_user_name_oid_desc = {10, (void *)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x01"};
gss_oid_desc *GSS_C_NT_HOSTBASED_SERVICE = &__gss_c_nt_hostbased_service_oid_desc;
gss_oid_desc *GSS_C_NT_USER_NAME = &__gss_c_nt_user_name_oid_desc;
gss_oid_desc *gss_nt_service_name = &__gss_c_nt_hostbased_service_oid_desc;
#define GSS_S_FAILURE (13u << 16)
#define GSS_S_NO_CRED (7u << 16)
typedef struct { size_t length; void *value; } gss_buffer_desc;
static u32 gss_fail(u32 *minor) { if (minor) *minor = 0; return GSS_S_FAILURE; }
u32 gss_import_name(u32 *mi, const gss_buffer_desc *b, const gss_oid_desc *t, void **out) { (void)b; (void)t; if (out) *out = 0; return gss_fail(mi); }
u32 gss_display_name(u32 *mi, void *n, gss_buffer_desc *b, gss_oid_desc **t) { (void)n; if (b) { b->length = 0; b->value = 0; } if (t) *t = 0; return gss_fail(mi); }
u32 gss_release_name(u32 *mi, void **n) { if (mi) *mi = 0; if (n) *n = 0; return 0; }
u32 gss_release_buffer(u32 *mi, gss_buffer_desc *b) { if (mi) *mi = 0; if (b) { b->length = 0; b->value = 0; } return 0; }
u32 gss_release_cred(u32 *mi, void **c) { if (mi) *mi = 0; if (c) *c = 0; return 0; }
u32 gss_release_oid_set(u32 *mi, void **s) { if (mi) *mi = 0; if (s) *s = 0; return 0; }
u32 gss_acquire_cred(u32 *mi, void *n, u32 t, void *mechs, int usage, void **out, void **am, u32 *tr) { (void)n; (void)t; (void)mechs; (void)usage; (void)am; if (out) *out = 0; if (tr) *tr = 0; if (mi) *mi = 0; return GSS_S_NO_CRED; }
u32 gss_acquire_cred_with_password(u32 *mi, void *n, const gss_buffer_desc *pw, u32 t, void *mechs, int usage, void **out, void **am, u32 *tr) { (void)n; (void)pw; (void)t; (void)mechs; (void)usage; (void)am; if (out) *out = 0; if (tr) *tr = 0; if (mi) *mi = 0; return GSS_S_NO_CRED; }
u32 gss_init_sec_context(u32 *mi, void *cred, void **ctx, void *target, gss_oid_desc *mech, u32 flags, u32 t, void *cb, const gss_buffer_desc *in, gss_oid_desc **amech, gss_buffer_desc *out, u32 *rf, u32 *tr) {
    (void)cred; (void)ctx; (void)target; (void)mech; (void)flags; (void)t; (void)cb; (void)in; if (amech) *amech = 0; if (out) { out->length = 0; out->value = 0; } if (rf) *rf = 0; if (tr) *tr = 0;
    if (mi) *mi = 0;
    return GSS_S_NO_CRED;
}
u32 gss_accept_sec_context(u32 *mi, void **ctx, void *cred, const gss_buffer_desc *in, void *cb, void **src, gss_oid_desc **mech, gss_buffer_desc *out, u32 *rf, u32 *tr, void **dcred) {
    (void)ctx; (void)cred; (void)in; (void)cb; if (src) *src = 0; if (mech) *mech = 0; if (out) { out->length = 0; out->value = 0; } if (rf) *rf = 0; if (tr) *tr = 0; if (dcred) *dcred = 0;
    return gss_fail(mi);
}
u32 gss_delete_sec_context(u32 *mi, void **ctx, gss_buffer_desc *out) { if (mi) *mi = 0; if (ctx) *ctx = 0; if (out) { out->length = 0; out->value = 0; } return 0; }
u32 gss_inquire_context(u32 *mi, void *ctx, void **src, void **tgt, u32 *t, gss_oid_desc **mech, u32 *flags, int *local, int *open) { (void)ctx; if (src) *src = 0; if (tgt) *tgt = 0; if (t) *t = 0; if (mech) *mech = 0; if (flags) *flags = 0; if (local) *local = 0; if (open) *open = 0; return gss_fail(mi); }
u32 gss_display_status(u32 *mi, u32 status, int type, const gss_oid_desc *mech, u32 *more, gss_buffer_desc *out) {
    (void)status; (void)type; (void)mech;
    static char msg[] = "Kerberos indisponivel no mOS";
    if (more) *more = 0;
    if (out) { out->length = sizeof msg - 1; out->value = msg; }
    if (mi) *mi = 0;
    return 0;
}
u32 gss_indicate_mechs(u32 *mi, void **set) { if (set) *set = 0; return gss_fail(mi); }
int gss_oid_equal(const gss_oid_desc *a, const gss_oid_desc *b) {
    if (a == b) return 1;
    if (!a || !b || a->length != b->length) return 0;
    for (u32 i = 0; i < a->length; i++) if (((u8 *)a->elements)[i] != ((u8 *)b->elements)[i]) return 0;
    return 1;
}
u32 gss_get_mic(u32 *mi, void *ctx, u32 qop, const gss_buffer_desc *m, gss_buffer_desc *tok) { (void)ctx; (void)qop; (void)m; if (tok) { tok->length = 0; tok->value = 0; } return gss_fail(mi); }
u32 gss_verify_mic(u32 *mi, void *ctx, const gss_buffer_desc *m, const gss_buffer_desc *tok, u32 *qop) { (void)ctx; (void)m; (void)tok; if (qop) *qop = 0; return gss_fail(mi); }
u32 gss_wrap(u32 *mi, void *ctx, int conf, u32 qop, const gss_buffer_desc *in, int *cs, gss_buffer_desc *out) { (void)ctx; (void)conf; (void)qop; (void)in; if (cs) *cs = 0; if (out) { out->length = 0; out->value = 0; } return gss_fail(mi); }
u32 gss_unwrap(u32 *mi, void *ctx, const gss_buffer_desc *in, gss_buffer_desc *out, int *cs, u32 *qop) { (void)ctx; (void)in; if (out) { out->length = 0; out->value = 0; } if (cs) *cs = 0; if (qop) *qop = 0; return gss_fail(mi); }
u32 gss_iter_creds(u32 *mi, u32 flags, const gss_oid_desc *mech, void *blk) { (void)flags; (void)mech; (void)blk; if (mi) *mi = 0; return 0; }
id GSSCredentialCopyName(void *cred) { (void)cred; return 0; }
u32 GSSCredentialGetLifetime(void *cred) { (void)cred; return 0; }
id GSSNameCreateDisplayString(void *name) { (void)name; return 0; }
/* krb5: contexto existe, nada mais */
#define KRB5_CC_NOTFOUND (-1765328243)
#define KRB5_KT_END (-1765328202)
int krb5_init_context(void **ctx) { if (ctx) *ctx = calloc(1, 16); return 0; }
void krb5_free_context(void *ctx) { free(ctx); }
int krb5_parse_name(void *ctx, const char *n, void **p) { (void)ctx; (void)n; if (p) *p = 0; return KRB5_CC_NOTFOUND; }
int krb5_unparse_name(void *ctx, void *p, char **n) { (void)ctx; (void)p; if (n) *n = 0; return KRB5_CC_NOTFOUND; }
void krb5_free_unparsed_name(void *ctx, char *n) { (void)ctx; free(n); }
void krb5_free_principal(void *ctx, void *p) { (void)ctx; (void)p; }
void krb5_free_cred_contents(void *ctx, void *c) { (void)ctx; (void)c; }
int krb5_free_keytab_entry_contents(void *ctx, void *e) { (void)ctx; (void)e; return 0; }
void krb5_get_init_creds_opt_init(void *o) { if (o) memset(o, 0, 16); }
int krb5_get_init_creds_password(void *ctx, void *creds, void *client, const char *pw, void *prompter, void *data, int start, const char *svc, void *opts) {
    (void)ctx; (void)creds; (void)client; (void)pw; (void)prompter; (void)data; (void)start; (void)svc; (void)opts; return KRB5_CC_NOTFOUND;
}
int krb5_kt_default(void *ctx, void **kt) { (void)ctx; if (kt) *kt = calloc(1, 16); return 0; }
int krb5_kt_close(void *ctx, void *kt) { (void)ctx; free(kt); return 0; }
int krb5_kt_start_seq_get(void *ctx, void *kt, void *cur) { (void)ctx; (void)kt; (void)cur; return 0; }
int krb5_kt_next_entry(void *ctx, void *kt, void *e, void *cur) { (void)ctx; (void)kt; (void)e; (void)cur; return KRB5_KT_END; }
int krb5_kt_end_seq_get(void *ctx, void *kt, void *cur) { (void)ctx; (void)kt; (void)cur; return 0; }
const char *error_message(long code) { (void)code; return "Kerberos indisponivel no mOS"; }

/* ================================================================ libresolv: servidor DNS do QEMU */
typedef union { struct { u8 len, family; u16 port; u32 addr; u8 zero[8]; } sin; u8 raw[128]; } res_sockaddr_t;
int res_9_getservers(void *st, res_sockaddr_t *set, int cnt) {
    (void)st;
    if (cnt < 1) return 0;
    memset(&set[0], 0, sizeof set[0]);
    set[0].sin.len = 16; set[0].sin.family = 2; set[0].sin.port = (u16)(53 << 8);
    set[0].sin.addr = 0x0302000Au;    /* 10.0.2.3 */
    return 1;
}

/* ================================================================ UniformTypeIdentifiers */
/* UTType: identificador + extensão e MIME preferidos. Os campos ext/mime NULL são perguntados ao Launch Services
 * (kernel, via openurl.c), que conhece os tipos do sistema e os declarados pelos apps instalados. */
typedef struct { void *isa; const char *ident, *ext, *mime; } uttype_t;
cx_class_t ut_c __asm__("OBJC_CLASS_$_UTType");
cx_class_t ut_m __asm__("OBJC_METACLASS_$_UTType");
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } mk_const_t;
#define MK(var, text) static mk_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
MK(UTTagClassFilenameExtension, "public.filename-extension")
MK(UTTagClassMIMEType, "public.mime-type")
#define UT(var, ident) static uttype_t var##_t = {&ut_c, ident, 0, 0}; const void *var = &var##_t;
static uttype_t ut_png = {&ut_c, "public.png", "png", "image/png"}, ut_text = {&ut_c, "public.utf8-plain-text", "txt", "text/plain"},
                ut_image = {&ut_c, "public.image", 0, 0}, ut_url = {&ut_c, "com.apple.web-internet-location", "webloc", 0},
                ut_iloc = {&ut_c, "com.apple.internet-location", 0, 0}, ut_svg = {&ut_c, "public.svg-image", "svg", "image/svg+xml"},
                ut_content = {&ut_c, "public.content", 0, 0}, ut_app = {&ut_c, "com.apple.application-bundle", "app", 0};
const void *UTTypePNG = &ut_png, *UTTypeUTF8PlainText = &ut_text, *UTTypeImage = &ut_image, *UTTypeInternetShortcut = &ut_url,
           *UTTypeInternetLocation = &ut_iloc, *UTTypeSVG = &ut_svg, *UTTypeContent = &ut_content, *UTTypeApplicationBundle = &ut_app;
UT(UTTypeItem, "public.item") UT(UTTypeData, "public.data") UT(UTTypeFolder, "public.folder") UT(UTTypeDirectory, "public.directory")
UT(UTTypeVolume, "public.volume") UT(UTTypePackage, "com.apple.package") UT(UTTypeBundle, "com.apple.bundle")
UT(UTTypeApplication, "com.apple.application") UT(UTTypeExecutable, "public.executable") UT(UTTypeUnixExecutable, "public.unix-executable")
UT(UTTypeText, "public.text") UT(UTTypePlainText, "public.plain-text") UT(UTTypeUTF16PlainText, "public.utf16-plain-text")
UT(UTTypeRTF, "public.rtf") UT(UTTypeHTML, "public.html") UT(UTTypeXML, "public.xml") UT(UTTypeJSON, "public.json")
UT(UTTypeYAML, "public.yaml") UT(UTTypeSourceCode, "public.source-code") UT(UTTypeScript, "public.script")
UT(UTTypeShellScript, "public.shell-script") UT(UTTypePythonScript, "public.python-script") UT(UTTypeJavaScript, "com.netscape.javascript-source")
UT(UTTypeCommaSeparatedText, "public.comma-separated-values-text") UT(UTTypePropertyList, "com.apple.property-list")
UT(UTTypeJPEG, "public.jpeg") UT(UTTypeGIF, "com.compuserve.gif") UT(UTTypeTIFF, "public.tiff") UT(UTTypeBMP, "com.microsoft.bmp")
UT(UTTypeHEIC, "public.heic") UT(UTTypeWebP, "org.webmproject.webp") UT(UTTypeICO, "com.microsoft.ico") UT(UTTypeICNS, "com.apple.icns")
UT(UTTypePDF, "com.adobe.pdf") UT(UTTypeAudiovisualContent, "public.audiovisual-content") UT(UTTypeAudio, "public.audio")
UT(UTTypeMP3, "public.mp3") UT(UTTypeWAV, "com.microsoft.waveform-audio") UT(UTTypeAIFF, "public.aiff-audio")
UT(UTTypeMPEG4Audio, "public.mpeg-4-audio") UT(UTTypeMovie, "public.movie") UT(UTTypeVideo, "public.video")
UT(UTTypeMPEG4Movie, "public.mpeg-4") UT(UTTypeQuickTimeMovie, "com.apple.quicktime-movie") UT(UTTypeAVI, "public.avi")
UT(UTTypeArchive, "public.archive") UT(UTTypeZIP, "public.zip-archive") UT(UTTypeGZIP, "org.gnu.gnu-zip-archive")
UT(UTTypeDiskImage, "public.disk-image") UT(UTTypeURL, "public.url") UT(UTTypeFileURL, "public.file-url")
UT(UTTypeVCard, "public.vcard") UT(UTTypeFont, "public.font") UT(UTTypeEPUB, "org.idpf.epub-container")
UT(UTTypeCompositeContent, "public.composite-content") UT(UTTypeDatabase, "public.database") UT(UTTypeSymbolicLink, "public.symlink")
UT(UTTypeAliasFile, "com.apple.alias-file") UT(UTTypeMessage, "public.message") UT(UTTypeEmailMessage, "public.email-message")

int cx_ls_conforms(const char *a, const char *b);
int cx_ls_type_for_tag(const char *cls_, const char *tag, const char *conf, char *out, size_t cap);
int cx_ls_tag_for_type(const char *uti, const char *cls_, char *out, size_t cap);
int cx_ls_type_info(const char *uti, char *desc, size_t dcap, char *parents, size_t pcap);
char *strdup(const char *s);
int strcasecmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);

static uttype_t *U(id o) { return (uttype_t *)o; }
static id ut_make(const char *ident) {
    if (!ident || !ident[0]) return 0;
    uttype_t *t = (uttype_t *)cx_alloc_instance(&ut_c);
    if (!t) return 0;
    t->ident = strdup(ident);
    return MSG0(id, (id)t, "autorelease");
}
static const char *str_of(id s) { return s && cx_is_str(s) ? cx_cstr(s) : 0; }
static const char *uti_of_obj(id t) { return !t ? 0 : cx_is_str(t) ? cx_cstr(t) : U(t)->ident; }

static id ut_ident(id self, SEL c) { (void)c; return cx_str(U(self)->ident); }
static id ut_ext(id self, SEL c) {
    (void)c;
    if (U(self)->ext) return cx_str(U(self)->ext);
    char e[64];
    return cx_ls_tag_for_type(U(self)->ident, "public.filename-extension", e, sizeof e) ? cx_str(e) : 0;
}
static id ut_mime(id self, SEL c) {
    (void)c;
    if (U(self)->mime) return cx_str(U(self)->mime);
    char m[128];
    return cx_ls_tag_for_type(U(self)->ident, "public.mime-type", m, sizeof m) ? cx_str(m) : 0;
}
static id ut_desc(id self, SEL c) {
    (void)c;
    char d[256];
    return cx_ls_type_info(U(self)->ident, d, sizeof d, 0, 0) ? cx_str(d) : cx_str(U(self)->ident);
}
static BOOL ut_conforms(id self, SEL c, id other) {
    (void)c;
    if (!other || self == other) return YES;
    const char *o = uti_of_obj(other);
    return o && cx_ls_conforms(U(self)->ident, o);
}
static BOOL ut_subtype(id self, SEL c, id other) {
    const char *o = uti_of_obj(other);
    return o && strcasecmp(U(self)->ident, o) && ut_conforms(self, c, other);
}
static BOOL ut_supertype(id self, SEL c, id other) {
    (void)c;
    const char *o = uti_of_obj(other);
    return o && strcasecmp(U(self)->ident, o) && cx_ls_conforms(o, U(self)->ident);
}
static BOOL ut_equal(id self, SEL c, id other) {
    (void)c;
    if (self == other) return YES;
    if (!other || (cx_class_t *)*(void **)other != &ut_c) return NO;
    return !strcasecmp(U(self)->ident, U(other)->ident);
}
static u64 ut_hash(id self, SEL c) {
    (void)c;
    u64 h = 1469598103934665603ull;
    for (const char *p = U(self)->ident; *p; p++) h = (h ^ (u8)((*p >= 'A' && *p <= 'Z') ? *p + 32 : *p)) * 1099511628211ull;
    return h;
}
static BOOL ut_dynamic(id self, SEL c) { (void)c; return !strncmp(U(self)->ident, "dyn.", 4); }
static BOOL ut_declared(id self, SEL c) { (void)c; return !ut_dynamic(self, c) && cx_ls_type_info(U(self)->ident, 0, 0, 0, 0); }
static BOOL ut_public(id self, SEL c) { (void)c; return !strncmp(U(self)->ident, "public.", 7); }
static id ut_supertypes(id self, SEL c) {
    (void)c;
    char par[1024];
    id arr = cx_arr_new(cx_class_named("NSMutableArray"));
    if (cx_ls_type_info(U(self)->ident, 0, 0, par, sizeof par)) {
        for (char *p = par; *p;) {
            char *sp = strchr(p, ' ');
            if (sp) *sp = 0;
            if (*p) cx_arr_add(arr, ut_make(p));
            if (!sp) break;
            p = sp + 1;
        }
    }
    return MSG(id, (id)cx_class_named("NSSet"), "setWithArray:", arr);
}
static id ut_tags(id self, SEL c) {
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    id e = ut_ext(self, c), m = ut_mime(self, c);
    if (e) { id a = MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", e); MSG(void, d, "setObject:forKey:", a, cx_str("public.filename-extension")); }
    if (m) { id a = MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", m); MSG(void, d, "setObject:forKey:", a, cx_str("public.mime-type")); }
    return d;
}
static id ut_copy(id self, SEL c, void *zone) { (void)c; (void)zone; return MSG0(id, self, "retain"); }

/* construtores (+[UTType typeWith...]) */
static id utc_ident(id self, SEL c, id s) { (void)self; (void)c; return ut_make(str_of(s)); }
static id utc_tag(const char *cls_, id tag, id conf) {
    char out[256];
    const char *t = str_of(tag);
    if (!t || !cx_ls_type_for_tag(cls_, t, uti_of_obj(conf), out, sizeof out)) return 0;
    return ut_make(out);
}
static id utc_ext(id self, SEL c, id e) { (void)self; (void)c; return utc_tag("public.filename-extension", e, 0); }
static id utc_ext_conf(id self, SEL c, id e, id conf) { (void)self; (void)c; return utc_tag("public.filename-extension", e, conf); }
static id utc_mime(id self, SEL c, id m) { (void)self; (void)c; return utc_tag("public.mime-type", m, 0); }
static id utc_mime_conf(id self, SEL c, id m, id conf) { (void)self; (void)c; return utc_tag("public.mime-type", m, conf); }
static id utc_tag_cls(id self, SEL c, id tag, id cls_, id conf) { (void)self; (void)c; return utc_tag(str_of(cls_), tag, conf); }
static id utc_types_tag(id self, SEL c, id tag, id cls_, id conf) {
    id one = utc_tag_cls(self, c, tag, cls_, conf);
    return one ? MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", one) : MSG0(id, (id)cx_class_named("NSArray"), "array");
}

int strcmp(const char *a, const char *b);
void cx_uttype_init(void) {
    cx_define_class(&ut_c, &ut_m, "UTType", cx_class_named("NSObject"), 24);
    static const struct { const char *sel; void *fn; } im[] = {
        {"identifier", (void *)ut_ident}, {"description", (void *)ut_ident}, {"debugDescription", (void *)ut_ident},
        {"preferredFilenameExtension", (void *)ut_ext}, {"preferredMIMEType", (void *)ut_mime}, {"localizedDescription", (void *)ut_desc},
        {"conformsToType:", (void *)ut_conforms}, {"isSubtypeOfType:", (void *)ut_subtype}, {"isSupertypeOfType:", (void *)ut_supertype},
        {"isEqual:", (void *)ut_equal}, {"hash", (void *)ut_hash}, {"isDynamic", (void *)ut_dynamic}, {"isDeclared", (void *)ut_declared},
        {"isPublicType", (void *)ut_public}, {"supertypes", (void *)ut_supertypes}, {"tags", (void *)ut_tags}, {"copyWithZone:", (void *)ut_copy},
    };
    for (unsigned i = 0; i < sizeof im / sizeof im[0]; i++) cx_add_method(&ut_c, im[i].sel, im[i].fn);
    static const struct { const char *sel; void *fn; } cm[] = {
        {"typeWithIdentifier:", (void *)utc_ident}, {"exportedTypeWithIdentifier:", (void *)utc_ident}, {"importedTypeWithIdentifier:", (void *)utc_ident},
        {"exportedTypeWithIdentifier:conformingToType:", (void *)utc_ident}, {"importedTypeWithIdentifier:conformingToType:", (void *)utc_ident},
        {"typeWithFilenameExtension:", (void *)utc_ext}, {"typeWithFilenameExtension:conformingToType:", (void *)utc_ext_conf},
        {"typeWithMIMEType:", (void *)utc_mime}, {"typeWithMIMEType:conformingToType:", (void *)utc_mime_conf},
        {"typeWithTag:tagClass:conformingToType:", (void *)utc_tag_cls}, {"typesWithTag:tagClass:conformingToType:", (void *)utc_types_tag},
    };
    for (unsigned i = 0; i < sizeof cm / sizeof cm[0]; i++) cx_add_method(&ut_m, cm[i].sel, cm[i].fn);
}

/* ================================================================ ServiceManagement */
u8 SMLoginItemSetEnabled(id ident, u8 enabled) { (void)ident; (void)enabled; return 1; }
id SMCopyAllJobDictionaries(id domain) { (void)domain; return CFArrayCreate(0, 0, 0, 0); }
u8 SMJobSubmit(id domain, id job, void *auth, id *err) { (void)domain; (void)job; (void)auth; if (err) *err = 0; return 0; }
u8 SMJobRemove(id domain, id label, void *auth, u8 wait, id *err) { (void)domain; (void)label; (void)auth; (void)wait; if (err) *err = 0; return 1; }
