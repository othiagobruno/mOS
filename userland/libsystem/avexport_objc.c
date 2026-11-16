/*
 * mOS — AVAssetExportSession sobre o avexport.c (cópia das amostras, sem recodificar).
 *  - AVAssetExportPresetPassthrough: todas as faixas copiáveis (H.264, AAC, Opus, PCM 16 bits), com o timeRange;
 *  - AVAssetExportPresetAppleM4A: só o áudio, quando ele já é AAC;
 *  - presets de qualidade e de tamanho (Low/Medium/HighestQuality, 640x480 ... 3840x2160, HEVC*): a AVFoundation
 *    recodifica; o mOS copia quando a origem já é H.264 (+ AAC) e cabe no tamanho do preset; maior que o preset, o vídeo
 *    é decodificado, reduzido (mantendo a proporção) e recodificado em H.264 pelo AVAssetWriter, e o áudio é copiado.
 *    Fora disso (outro codec, videoComposition, audioMix) a exportação falha com AVErrorExportFailed (-11820) e a
 *    mensagem diz o motivo.
 * exportAsynchronouslyWithCompletionHandler: roda numa thread; status/progress/error como na AVFoundation; o arquivo de
 * saída não pode existir (-11823). Código original do mOS (MIT).
 */
#include "foundation.h"
#include "avexport.h"
#include "mp4mux.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
const char *cx_audio_url_path(id url);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);
int fseek(FILE_ *f, long off, int whence);
long ftell(FILE_ *f);

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } axc_t;
#define AXC(var, text) static axc_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
AXC(AVAssetExportPresetAppleM4A, "AVAssetExportPresetAppleM4A")
AXC(AVAssetExportPresetLowQuality, "AVAssetExportPresetLowQuality")
AXC(AVAssetExportPreset640x480, "AVAssetExportPreset640x480")
AXC(AVAssetExportPreset960x540, "AVAssetExportPreset960x540")
AXC(AVAssetExportPreset1280x720, "AVAssetExportPreset1280x720")
AXC(AVAssetExportPreset1920x1080, "AVAssetExportPreset1920x1080")
AXC(AVAssetExportPreset3840x2160, "AVAssetExportPreset3840x2160")
AXC(AVAssetExportPresetHEVC1920x1080, "AVAssetExportPresetHEVC1920x1080")
AXC(AVAssetExportPresetHEVC3840x2160, "AVAssetExportPresetHEVC3840x2160")
AXC(AVFileTypeAppleM4V, "com.apple.m4v-video")
AXC(AVFileTypeWAVE, "com.microsoft.waveform-audio")
AXC(AVFileType3GPP, "public.3gpp")

typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cmtime_t;
typedef struct { cmtime_t start, duration; } cmrange_t;
#define BLK_FN(b) (((void **)(b))[2])
enum { X_UNKNOWN = 0, X_WAITING = 1, X_EXPORTING = 2, X_COMPLETED = 3, X_FAILED = 4, X_CANCELLED = 5 };

typedef struct {
    cx_class_t *isa;
    id asset, preset, url, ftype, error, metadata, audiomix, vcomp, tmpdir;
    cmrange_t range;
    cmtime_t maxdur;
    i64 maxlen;
    volatile int status;
    BOOL fast;
    avx_ctl ctl;
    void *done;
} xs_t;
#define XS(o) ((xs_t *)(o))
cx_class_t AVAssetExportSession_c __asm__("OBJC_CLASS_$_AVAssetExportSession"), AVAssetExportSession_m __asm__("OBJC_METACLASS_$_AVAssetExportSession");

static id cls(const char *n) { return (id)cx_class_named(n); }
static void setobj(id *slot, id v) { if (*slot == v) return; if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static id error_with(i64 code, const char *msg) {
    id info = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, info, "setObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("AVFoundationErrorDomain"), code, info);
}
static void super_dealloc(id s, SEL c) { IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c); if (d) ((void (*)(id, SEL))d)(s, c); }
static const char *asset_path(id asset) { return asset ? cx_audio_url_path(MSG0(id, asset, "URL")) : 0; }
static int container_of(const char *ft) {
    if (!ft) return -1;
    if (!strcmp(ft, "com.apple.quicktime-movie")) return MP4W_MOV;
    if (!strcmp(ft, "public.mpeg-4") || !strcmp(ft, "com.apple.m4v-video") || !strcmp(ft, "public.3gpp")) return MP4W_MP4;
    if (!strcmp(ft, "com.apple.m4a-audio")) return MP4W_M4A;
    return -1;
}

/* o que o preset pede e se dá para atender copiando; why explica quando não */
typedef struct { int ok, want, audio_only, maxw, maxh, reencode, tw, th; const char *why; } plan_t;
static plan_t plan_for(const char *preset, const avx_info *in) {
    plan_t p = {.ok = 1, .want = AVX_VIDEO | AVX_AUDIO};
    if (!preset) { p.ok = 0; p.why = "Preset nulo."; return p; }
    if (!strcmp(preset, "AVAssetExportPresetAppleM4A")) {
        p.audio_only = 1; p.want = AVX_AUDIO;
        if (!in->has_audio) { p.ok = 0; p.why = "O asset não tem áudio."; }
        else if (!in->audio_is_aac) { p.ok = 0; p.why = "O áudio não é AAC e o mOS ainda não tem o codificador AAC: use AVAssetExportPresetPassthrough."; }
        return p;
    }
    if (!strcmp(preset, "AVAssetExportPresetPassthrough")) {
        if (in->has_video && !in->video_copyable) { p.ok = 0; p.why = "A faixa de vídeo não é H.264: o passthrough do mOS copia H.264."; }
        else if (in->has_audio && !in->audio_copyable) { p.ok = 0; p.why = "O áudio não é AAC, Opus nem PCM 16 bits."; }
        return p;
    }
    static const struct { const char *n; int w, h; } sized[] = {
        {"AVAssetExportPresetLowQuality", 0, 0}, {"AVAssetExportPresetMediumQuality", 0, 0}, {"AVAssetExportPresetHighestQuality", 0, 0},
        {"AVAssetExportPresetHEVCHighestQuality", 0, 0}, {"AVAssetExportPreset640x480", 640, 480}, {"AVAssetExportPreset960x540", 960, 540},
        {"AVAssetExportPreset1280x720", 1280, 720}, {"AVAssetExportPreset1920x1080", 1920, 1080}, {"AVAssetExportPreset3840x2160", 3840, 2160},
        {"AVAssetExportPresetHEVC1920x1080", 1920, 1080}, {"AVAssetExportPresetHEVC3840x2160", 3840, 2160},
    };
    for (unsigned i = 0; i < sizeof sized / sizeof sized[0]; i++) {
        if (strcmp(preset, sized[i].n)) continue;
        p.maxw = sized[i].w; p.maxh = sized[i].h;
        if (in->has_video && in->vcodec != 0x61766331) { p.ok = 0; p.why = "Recodificar vídeo que não é H.264 ainda não é feito pelo mOS."; }
        else if (in->has_video && p.maxw && !((in->width <= p.maxw && in->height <= p.maxh) || (in->width <= p.maxh && in->height <= p.maxw))) {
            /* maior que o preset: cabe no retângulo do preset (deitado ou em pé, como o vídeo), dimensões pares */
            int bw = in->width >= in->height ? p.maxw : p.maxh, bh = in->width >= in->height ? p.maxh : p.maxw;
            double k = (double)bw / in->width < (double)bh / in->height ? (double)bw / in->width : (double)bh / in->height;
            p.reencode = 1;
            p.tw = ((int)(in->width * k + 0.5)) & ~1; p.th = ((int)(in->height * k + 0.5)) & ~1;
            if (p.tw < 16) p.tw = 16;
            if (p.th < 16) p.th = 16;
            if (in->has_audio && !in->audio_is_aac) { p.ok = 0; p.why = "O áudio não é AAC e o mOS ainda não tem o codificador AAC."; }
        } else if (in->has_audio && !in->audio_is_aac) { p.ok = 0; p.why = "O áudio não é AAC e o mOS ainda não tem o codificador AAC."; }
        return p;
    }
    p.ok = 0; p.why = "Preset desconhecido.";
    return p;
}
#include "avvideo.h"
void *cx_vt_convert_frame(u32 fmt, int dw, int dh, const u8 *y, const u8 *u, const u8 *v, int sy, int suv, int w, int h, int full, int matrix);
int avx_track_index(const char *path, int media);
int unlink(const char *p);
int snprintf(char *s, size_t n, const char *f, ...);
typedef struct { long long value; int timescale; unsigned flags; long long epoch; } rcmt_t;
static rcmt_t rsecs(double s) { rcmt_t t = {(long long)(s * 1000000000.0 + 0.5), 1000000000, 1, 0}; return t; }
/* recodifica o vídeo de [start, start+dur) em tw x th (H.264, arquivo temporário) e junta o áudio copiado da origem */
static int reencode(const char *in, const char *out, int container, int fast, double start, double dur, int tw, int th, int with_audio,
                    avx_ctl *ctl, const char **why) {
    int err = 0;
    avv_t *v = avv_open_path(in, &err);
    if (!v) { *why = "O vídeo de origem não pôde ser decodificado."; return AVX_ERR_CODEC; }
    double total = 0, fps = 30;
    int sw, sh;
    uint32_t ns;
    avv_info(v, &sw, &sh, &total, &fps, &ns);
    double end = dur >= 0 ? start + dur : total;
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.mos-video.mp4", out);
    unlink(tmp);
    id url = MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", cx_str(tmp));
    id e = 0;
    id w = MSG(id, (id)cx_class_named("AVAssetWriter"), "assetWriterWithURL:fileType:error:", url, cx_str("public.mpeg-4"), &e);
    id vs = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, vs, "setObject:forKey:", cx_str("avc1"), cx_str("AVVideoCodecKey"));
    MSG(void, vs, "setObject:forKey:", cx_num_i(tw), cx_str("Width"));
    MSG(void, vs, "setObject:forKey:", cx_num_i(th), cx_str("Height"));
    id vin = w ? MSG(id, (id)cx_class_named("AVAssetWriterInput"), "assetWriterInputWithMediaType:outputSettings:", cx_str("vide"), vs) : 0;
    id ad = vin ? MSG(id, (id)cx_class_named("AVAssetWriterInputPixelBufferAdaptor"),
                      "assetWriterInputPixelBufferAdaptorWithAssetWriterInput:sourcePixelBufferAttributes:", vin, (id)0) : 0;
    if (!ad || !MSG(BOOL, w, "canAddInput:", vin)) { avv_close(v); *why = "O codificador H.264 não aceitou o tamanho."; return AVX_ERR_CODEC; }
    MSG(void, w, "addInput:", vin);
    if (!MSG0(BOOL, w, "startWriting")) { avv_close(v); *why = "Não foi possível gravar o arquivo temporário."; return AVX_ERR_WRITE; }
    ((void (*)(id, SEL, rcmt_t))objc_msgSend)(w, sel_registerName("startSessionAtSourceTime:"), rsecs(0));
    avv_frame f;
    int rc = avv_frame_at(v, start, &f), frames = 0, status = AVX_OK;
    double last = -1;
    while (rc >= 0) {
        if (ctl && ctl->cancel) { status = AVX_ERR_CANCEL; break; }
        if (f.pts >= end) break;
        double t = f.pts < start ? 0 : f.pts - start;
        if (t > last) {
            void *pb = cx_vt_convert_frame(0x34323076u /* 420v */, tw, th, f.y, f.u, f.v, f.sy, f.suv, f.w, f.h, f.full_range, avv_matrix(&f));
            if (!pb) { status = AVX_ERR_CODEC; *why = "Falha ao reduzir o quadro."; break; }
            BOOL ok = ((BOOL (*)(id, SEL, void *, rcmt_t))objc_msgSend)(ad, sel_registerName("appendPixelBuffer:withPresentationTime:"), pb, rsecs(t));
            objc_release((id)pb);
            if (!ok) { status = AVX_ERR_WRITE; *why = "O codificador H.264 recusou um quadro."; break; }
            last = t;
            frames++;
            if (ctl && end > start) ctl->progress = (float)(0.9 * t / (end - start));
        }
        rc = avv_next_frame(v, &f);
        if (rc == 0) break;   /* fim */
    }
    avv_close(v);
    MSG0(void, vin, "markAsFinished");
    if (status != AVX_OK || !frames) { MSG0(void, w, "cancelWriting"); unlink(tmp); if (!frames && status == AVX_OK) { *why = "Nenhum quadro no intervalo."; status = AVX_ERR_EMPTY; } return status; }
    ((void (*)(id, SEL, rcmt_t))objc_msgSend)(w, sel_registerName("endSessionAtSourceTime:"), rsecs(end - start));
    if (!MSG0(BOOL, w, "finishWriting")) { unlink(tmp); *why = "O arquivo temporário não fechou."; return AVX_ERR_WRITE; }
    /* vídeo novo + áudio da origem no mesmo intervalo */
    avx_segment vseg = {tmp, 0, 0, end - start, 0}, aseg = {in, 0, start, end - start, 0};
    avx_ctrack tr[2];
    int n = 0;
    tr[n++] = (avx_ctrack){AVX_VIDEO, &vseg, 1};
    int ai = with_audio ? avx_track_index(in, AVX_AUDIO) : -1;
    if (ai >= 0) { aseg.src_track = ai; tr[n++] = (avx_ctrack){AVX_AUDIO, &aseg, 1}; }
    int r = avx_compose(out, container, fast, tr, n, ctl, why);
    unlink(tmp);
    if (!r && ctl) ctl->progress = 1;
    return r;
}
#include "avvcomp_desc.h"
void *malloc(size_t n);
void free(void *p);
#include "avcap_conv.h"
int CVPixelBufferCreateWithPlanarBytes(void *a, u64 w, u64 h, u32 fmt, void *d, u64 ds, u64 np, void **pb, u64 *pw, u64 *ph, u64 *pr,
                                       void *rel, void *ctx, id attrs, void **out);
static void vc_free(void *ctx, const void *d, u64 sz, u64 np, const void **pl) { (void)d; (void)sz; (void)np; (void)pl; free(ctx); }
/* AVVideoComposition: cada quadro de renderSize no ritmo de frameDuration, as camadas da instrução do momento (a de
 * cima por último), do vídeo da origem; vídeo novo em H.264 + áudio copiado da origem */
static int vcomp_render(const char *in, int vtrack, const char *out, int container, int fast, double start, double dur, const vcomp_desc *vd,
                        int with_audio, avx_ctl *ctl, const char **why) {
    int err = 0, tw = vd->w & ~1, th = vd->h & ~1;
    avv_t *v = avv_open_path(in, &err);
    if (!v) { *why = "O vídeo de origem não pôde ser decodificado."; return AVX_ERR_CODEC; }
    double total = 0, fps = 30; int sw, sh; uint32_t ns;
    avv_info(v, &sw, &sh, &total, &fps, &ns);
    double end = dur >= 0 ? start + dur : total;
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.mos-video.mp4", out);
    unlink(tmp);
    id e = 0;
    id turl = MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", cx_str(tmp));
    id w = MSG(id, (id)cx_class_named("AVAssetWriter"), "assetWriterWithURL:fileType:error:", turl, cx_str("public.mpeg-4"), &e);
    id vs = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, vs, "setObject:forKey:", cx_str("avc1"), cx_str("AVVideoCodecKey"));
    MSG(void, vs, "setObject:forKey:", cx_num_i(tw), cx_str("Width"));
    MSG(void, vs, "setObject:forKey:", cx_num_i(th), cx_str("Height"));
    id vin = w ? MSG(id, (id)cx_class_named("AVAssetWriterInput"), "assetWriterInputWithMediaType:outputSettings:", cx_str("vide"), vs) : 0;
    id ad = vin ? MSG(id, (id)cx_class_named("AVAssetWriterInputPixelBufferAdaptor"),
                      "assetWriterInputPixelBufferAdaptorWithAssetWriterInput:sourcePixelBufferAttributes:", vin, (id)0) : 0;
    if (!ad || !MSG(BOOL, w, "canAddInput:", vin)) { avv_close(v); *why = "O codificador H.264 não aceitou o renderSize."; return AVX_ERR_CODEC; }
    MSG(void, w, "addInput:", vin);
    if (!MSG0(BOOL, w, "startWriting")) { avv_close(v); *why = "Não foi possível gravar o arquivo temporário."; return AVX_ERR_WRITE; }
    ((void (*)(id, SEL, rcmt_t))objc_msgSend)(w, sel_registerName("startSessionAtSourceTime:"), rsecs(0));
    uint32_t *canvas = malloc((size_t)tw * th * 4);
    int status = AVX_OK, frames = 0;
    for (double t = start; canvas && t < end - 1e-9; t += vd->frame) {
        if (ctl && ctl->cancel) { status = AVX_ERR_CANCEL; break; }
        const vcd_inst *ins = 0;
        for (int i = 0; i < vd->ninst; i++) if (t >= vd->inst[i].t0 - 1e-9 && t < vd->inst[i].t1 - 1e-9) { ins = &vd->inst[i]; break; }
        uint32_t bg = ins ? ins->bg : 0xFF000000u;
        for (long i = 0; i < (long)tw * th; i++) canvas[i] = bg;
        avv_frame f;
        uint8_t *px = 0; int fw = 0, fh = 0;
        if (ins && !avv_frame_at(v, t, &f) && !avv_to_bgra(&f, 0, 0, 0, &px, &fw, &fh)) {
            for (int li = ins->nlayers - 1; li >= 0; li--) {   /* a primeira da lista fica por cima */
                const vc_layer *l = &ins->layers[li];
                if (l->track && vtrack && l->track != vtrack) continue;   /* só a faixa de vídeo da origem */
                vc_rect cr;
                int hc = vc_layer_crop(l, t, &cr);
                vc_composite(canvas, tw, th, (const uint32_t *)px, fw, fh, vc_layer_transform(l, t), vc_layer_opacity(l, t), hc ? &cr : 0);
            }
        }
        free(px);
        size_t bpr[3], rows[3];
        avc_planes(AVC_FMT_420V, tw, th, bpr, rows);
        size_t total_b = bpr[0] * rows[0] + bpr[1] * rows[1];
        uint8_t *mem = malloc(total_b);
        if (!mem) { status = AVX_ERR_WRITE; break; }
        uint8_t *pl[2] = {mem, mem + bpr[0] * rows[0]};
        avc_convert(AVC_SRC_BGRA, (const uint8_t *)canvas, (size_t)tw * th * 4, tw, th, AVC_FMT_420V, pl, bpr);
        void *pbase[2] = {pl[0], pl[1]}, *pb = 0;
        u64 pw[2] = {(u64)tw, (u64)tw / 2}, ph[2] = {rows[0], rows[1]}, pr[2] = {bpr[0], bpr[1]};
        CVPixelBufferCreateWithPlanarBytes(0, (u64)tw, (u64)th, AVC_FMT_420V, mem, total_b, 2, pbase, pw, ph, pr, (void *)vc_free, mem, 0, &pb);
        if (!pb) { free(mem); status = AVX_ERR_WRITE; break; }
        BOOL ok = ((BOOL (*)(id, SEL, void *, rcmt_t))objc_msgSend)(ad, sel_registerName("appendPixelBuffer:withPresentationTime:"), pb, rsecs(t - start));
        objc_release((id)pb);
        if (!ok) { status = AVX_ERR_WRITE; *why = "O codificador H.264 recusou um quadro."; break; }
        frames++;
        if (ctl && end > start) ctl->progress = (float)(0.9 * (t - start) / (end - start));
    }
    free(canvas);
    avv_close(v);
    MSG0(void, vin, "markAsFinished");
    if (status != AVX_OK || !frames) { MSG0(void, w, "cancelWriting"); unlink(tmp); if (status == AVX_OK) { *why = "Nenhum quadro no intervalo."; status = AVX_ERR_EMPTY; } return status; }
    ((void (*)(id, SEL, rcmt_t))objc_msgSend)(w, sel_registerName("endSessionAtSourceTime:"), rsecs(end - start));
    if (!MSG0(BOOL, w, "finishWriting")) { unlink(tmp); *why = "O arquivo temporário não fechou."; return AVX_ERR_WRITE; }
    avx_segment vseg = {tmp, 0, 0, end - start, 0}, aseg = {in, 0, start, end - start, 0};
    avx_ctrack tr[2];
    int n = 0;
    tr[n++] = (avx_ctrack){AVX_VIDEO, &vseg, 1};
    int ai = with_audio ? avx_track_index(in, AVX_AUDIO) : -1;
    if (ai >= 0) { aseg.src_track = ai; tr[n++] = (avx_ctrack){AVX_AUDIO, &aseg, 1}; }
    int r = avx_compose(out, container, fast, tr, n, ctl, why);
    unlink(tmp);
    if (!r && ctl) ctl->progress = 1;
    return r;
}
int cx_avcomposition_is(id a);
int cx_avcomposition_info(id a, int *has_video, int *has_audio, double *dur);
int cx_avcomposition_export(id a, const char *out, int container, int fast, double start, double dur, int want, avx_ctl *ctl, const char **why);
typedef struct { double w, h; } xsz_t;
/* composição: o que dá para saber antes de copiar (codec e configuração só no avx_compose) */
static int probe_asset(id asset, avx_info *inf) {
    if (cx_avcomposition_is(asset)) {
        *inf = (avx_info){0};
        cx_avcomposition_info(asset, &inf->has_video, &inf->has_audio, &inf->duration);
        inf->vcodec = 0x61766331; inf->acodec = 0x6d703461;
        inf->video_copyable = inf->audio_copyable = inf->audio_is_aac = 1;
        xsz_t z = ((xsz_t (*)(id, SEL))objc_msgSend)(asset, sel_registerName("naturalSize"));
        inf->width = (int)z.w; inf->height = (int)z.h;
        return 0;
    }
    const char *p = asset_path(asset);
    return p && !avx_probe(p, inf) ? 0 : -1;
}

static id xs_init(id s, SEL c, id asset, id preset) {
    (void)c;
    setobj(&XS(s)->asset, asset);
    setobj(&XS(s)->preset, preset);
    XS(s)->range.start = (cmtime_t){0, 1, 1, 0};
    XS(s)->range.duration = (cmtime_t){0, 0, 5, 0};   /* kCMTimeRangeAll: duração +infinito */
    XS(s)->maxdur = (cmtime_t){0, 0, 5, 0};
    avx_info inf;
    if (probe_asset(asset, &inf)) { objc_release(s); return 0; }   /* asset sem arquivo legível: a AVFoundation devolve nil */
    return s;
}
static id xs_with(id k, SEL c, id asset, id preset) { (void)k; return objc_autorelease(xs_init(cx_alloc_instance(&AVAssetExportSession_c), c, asset, preset)); }
static void xs_dealloc(id s, SEL c) {
    if (XS(s)->done) _Block_release(XS(s)->done);
    id *f[] = {&XS(s)->asset, &XS(s)->preset, &XS(s)->url, &XS(s)->ftype, &XS(s)->error, &XS(s)->metadata, &XS(s)->audiomix, &XS(s)->vcomp, &XS(s)->tmpdir};
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) setobj(f[i], 0);
    super_dealloc(s, c);
}
static id xs_all_presets(id k, SEL c) {
    (void)k; (void)c;
    static const char *n[] = {"AVAssetExportPresetPassthrough", "AVAssetExportPresetAppleM4A", "AVAssetExportPresetLowQuality",
                              "AVAssetExportPresetMediumQuality", "AVAssetExportPresetHighestQuality", "AVAssetExportPresetHEVCHighestQuality",
                              "AVAssetExportPreset640x480", "AVAssetExportPreset960x540", "AVAssetExportPreset1280x720",
                              "AVAssetExportPreset1920x1080", "AVAssetExportPreset3840x2160", "AVAssetExportPresetHEVC1920x1080",
                              "AVAssetExportPresetHEVC3840x2160"};
    id a = MSG0(id, cls("NSMutableArray"), "array");
    for (unsigned i = 0; i < sizeof n / sizeof n[0]; i++) MSG(void, a, "addObject:", cx_str(n[i]));
    return a;
}
static id xs_compatible(id k, SEL c, id asset) {
    id all = xs_all_presets(k, c), out = MSG0(id, cls("NSMutableArray"), "array");
    avx_info inf;
    if (probe_asset(asset, &inf)) return out;
    for (u64 i = 0, n = MSG0(u64, all, "count"); i < n; i++) {
        id p = MSG(id, all, "objectAtIndex:", i);
        if (plan_for(cx_cstr(p), &inf).ok) MSG(void, out, "addObject:", p);
    }
    return out;
}
static id file_types_for(id asset, id preset) {
    id a = MSG0(id, cls("NSMutableArray"), "array");
    avx_info inf;
    if (probe_asset(asset, &inf)) return a;
    plan_t p = plan_for(preset ? cx_cstr(preset) : 0, &inf);
    if (!p.ok) return a;
    int video = inf.has_video && !p.audio_only;
    MSG(void, a, "addObject:", cx_str("com.apple.quicktime-movie"));
    MSG(void, a, "addObject:", cx_str("public.mpeg-4"));
    if (video) MSG(void, a, "addObject:", cx_str("com.apple.m4v-video"));
    else if (inf.audio_is_aac) MSG(void, a, "addObject:", cx_str("com.apple.m4a-audio"));
    return a;
}
static void call_bool_block(void *b, BOOL v) { if (b) ((void (*)(void *, BOOL))BLK_FN(b))(b, v); }
static void call_obj_block(void *b, id v) { if (b) ((void (*)(void *, id))BLK_FN(b))(b, v); }
static void xs_determine(id k, SEL c, id preset, id asset, id ftype, void *blk) {
    (void)k; (void)c;
    id types = file_types_for(asset, preset);
    BOOL ok = MSG0(u64, types, "count") > 0;
    if (ok && ftype) {
        ok = 0;
        for (u64 i = 0, n = MSG0(u64, types, "count"); i < n; i++) if (!strcmp(cx_cstr(MSG(id, types, "objectAtIndex:", i)), cx_cstr(ftype))) ok = 1;
    }
    call_bool_block(blk, ok);
}
static id xs_supported(id s, SEL c) { (void)c; return file_types_for(XS(s)->asset, XS(s)->preset); }
static void xs_determine_types(id s, SEL c, void *blk) { call_obj_block(blk, xs_supported(s, c)); }
static id xs_asset(id s, SEL c) { (void)c; return XS(s)->asset; }
static id xs_preset(id s, SEL c) { (void)c; return XS(s)->preset; }
static id xs_url(id s, SEL c) { (void)c; return XS(s)->url; }
static void xs_set_url(id s, SEL c, id v) { (void)c; setobj(&XS(s)->url, v); }
static id xs_ftype(id s, SEL c) { (void)c; return XS(s)->ftype; }
static void xs_set_ftype(id s, SEL c, id v) { (void)c; setobj(&XS(s)->ftype, v); }
static i64 xs_status(id s, SEL c) { (void)c; return XS(s)->status; }
static id xs_error(id s, SEL c) { (void)c; return XS(s)->error; }
static float xs_progress(id s, SEL c) { (void)c; return XS(s)->status == X_COMPLETED ? 1.0f : XS(s)->ctl.progress; }
static BOOL xs_fast(id s, SEL c) { (void)c; return XS(s)->fast; }
static void xs_set_fast(id s, SEL c, BOOL v) { (void)c; XS(s)->fast = v; }
static cmrange_t xs_range(id s, SEL c) { (void)c; return XS(s)->range; }
static void xs_set_range(id s, SEL c, cmrange_t r) { (void)c; XS(s)->range = r; }
static cmtime_t xs_maxdur(id s, SEL c) { (void)c; return XS(s)->maxdur; }
static i64 xs_maxlen(id s, SEL c) { (void)c; return XS(s)->maxlen; }
static void xs_set_maxlen(id s, SEL c, i64 v) { (void)c; XS(s)->maxlen = v; }
static id xs_metadata(id s, SEL c) { (void)c; return XS(s)->metadata ? XS(s)->metadata : MSG0(id, cls("NSArray"), "array"); }
static void xs_set_metadata(id s, SEL c, id v) { (void)c; setobj(&XS(s)->metadata, v); }
static id xs_audiomix(id s, SEL c) { (void)c; return XS(s)->audiomix; }
static void xs_set_audiomix(id s, SEL c, id v) { (void)c; setobj(&XS(s)->audiomix, v); }
static id xs_vcomp(id s, SEL c) { (void)c; return XS(s)->vcomp; }
static void xs_set_vcomp(id s, SEL c, id v) { (void)c; setobj(&XS(s)->vcomp, v); }
static id xs_tmpdir(id s, SEL c) { (void)c; return XS(s)->tmpdir; }
static void xs_set_tmpdir(id s, SEL c, id v) { (void)c; setobj(&XS(s)->tmpdir, v); }
static BOOL xs_multipass(id s, SEL c) { (void)s; (void)c; return 0; }
static i64 xs_estimated_len(id s, SEL c) {
    (void)c;
    const char *p = asset_path(XS(s)->asset);
    FILE_ *f = p ? fopen(p, "rb") : 0;
    if (!f) return 0;
    fseek(f, 0, 2);
    long n = ftell(f);
    fclose(f);
    return n > 0 ? n : 0;
}
static void xs_estimate_len(id s, SEL c, void *blk) { if (blk) ((void (*)(void *, i64, id))BLK_FN(blk))(blk, xs_estimated_len(s, c), 0); }

static void fail(id s, i64 code, const char *msg) { setobj(&XS(s)->error, error_with(code, msg)); XS(s)->status = X_FAILED; }
static double secs(cmtime_t t) { return (t.flags & 1) && t.timescale > 0 ? (double)t.value / t.timescale : 0; }
static void *export_thread(void *arg) {
    id s = arg;
    xs_t *x = XS(s);
    const char *in = asset_path(x->asset), *out = cx_audio_url_path(x->url), *why = 0;
    avx_info inf;
    plan_t p = {0};
    int k = x->ftype ? container_of(cx_cstr(x->ftype)) : -1;
    int is_comp = cx_avcomposition_is(x->asset);
    static vcomp_desc vdesc;   /* grande demais para a pilha da thread */
    if ((!in && !is_comp) || probe_asset(x->asset, &inf)) fail(s, -11800, "O asset não pôde ser lido.");
    else if (!(p = plan_for(cx_cstr(x->preset), &inf)).ok) fail(s, -11820, p.why);
    else if (x->vcomp && is_comp) fail(s, -11820, "videoComposition sobre AVMutableComposition ainda não é aplicado pelo mOS.");
    else if (x->vcomp && (!inf.has_video || cx_vcomp_describe(x->vcomp, &vdesc))) fail(s, -11820, "A videoComposition não é válida (renderSize, frameDuration ou compositor próprio).");
    else if (x->audiomix) fail(s, -11820, "audioMix ainda não é aplicado pelo mOS ao exportar.");
    else if (!out) fail(s, -11800, "outputURL tem de ser um arquivo local.");
    else if (k < 0) fail(s, -11820, "outputFileType ausente ou não suportado para este preset.");
    else if (k == MP4W_M4A && !p.audio_only && inf.has_video) fail(s, -11820, "M4A só leva áudio: use AVAssetExportPresetAppleM4A.");
    else {
        FILE_ *f = fopen(out, "rb");
        if (f) { fclose(f); fail(s, -11823, "Não é possível salvar: já existe um arquivo nesse caminho."); }
    }
    if (x->status == X_EXPORTING) {
        double start = secs(x->range.start), dur = -1;
        cmtime_t d = x->range.duration;
        if ((d.flags & 1) && !(d.flags & 0x1c) && d.timescale > 0) dur = secs(d);   /* sem +inf/-inf/indefinido */
        int vtrack = 0;
        if (x->vcomp) {
            id tracks = MSG(id, x->asset, "tracksWithMediaType:", cx_str("vide"));
            id t0 = tracks && MSG0(u64, tracks, "count") ? MSG(id, tracks, "objectAtIndex:", (u64)0) : 0;
            vtrack = t0 ? MSG0(int, t0, "trackID") : 0;
        }
        int rc = x->vcomp ? vcomp_render(in, vtrack, out, k, x->fast, start, dur, &vdesc, (p.want & AVX_AUDIO) && inf.has_audio, &x->ctl, &why)
               : is_comp ? cx_avcomposition_export(x->asset, out, k, x->fast, start, dur, p.want, &x->ctl, &why)
                 : p.reencode ? reencode(in, out, k, x->fast, start, dur, p.tw, p.th, (p.want & AVX_AUDIO) && inf.has_audio, &x->ctl, &why)
                              : avx_passthrough(in, out, k, x->fast, start, dur, p.want, &x->ctl, &why);
        if (rc == AVX_ERR_CANCEL) x->status = X_CANCELLED;
        else if (rc) fail(s, -11820, why ? why : "A exportação falhou.");
        else x->status = X_COMPLETED;
    }
    void *blk = x->done;
    x->done = 0;
    if (blk) { ((void (*)(void *))BLK_FN(blk))(blk); _Block_release(blk); }
    objc_release(s);
    return 0;
}
static void xs_export(id s, SEL c, void *blk) {
    (void)c;
    if (XS(s)->status != X_UNKNOWN) { if (blk) ((void (*)(void *))BLK_FN(blk))(blk); return; }
    XS(s)->status = X_EXPORTING;
    XS(s)->done = blk ? _Block_copy(blk) : 0;
    unsigned long th = 0;
    objc_retain(s);
    if (pthread_create(&th, 0, export_thread, s)) export_thread(s);
    else pthread_detach(th);
}
static void xs_cancel(id s, SEL c) {
    (void)c;
    if (XS(s)->status == X_UNKNOWN || XS(s)->status == X_WAITING) { XS(s)->status = X_CANCELLED; return; }
    XS(s)->ctl.cancel = 1;
}

void cx_avexport_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!cx_class_named("AVAssetExportSession"))
        cx_define_class(&AVAssetExportSession_c, &AVAssetExportSession_m, "AVAssetExportSession", ob, (u32)(sizeof(xs_t) - 8));
#define M(s, f) cx_add_method(&AVAssetExportSession_c, s, (void *)(f))
#define CM(s, f) cx_add_cmethod(&AVAssetExportSession_c, s, (void *)(f))
    CM("exportSessionWithAsset:presetName:", xs_with);
    CM("allExportPresets", xs_all_presets);
    CM("exportPresetsCompatibleWithAsset:", xs_compatible);
    CM("determineCompatibilityOfExportPreset:withAsset:outputFileType:completionHandler:", xs_determine);
    M("initWithAsset:presetName:", xs_init); M("dealloc", xs_dealloc);
    M("asset", xs_asset); M("presetName", xs_preset);
    M("outputURL", xs_url); M("setOutputURL:", xs_set_url);
    M("outputFileType", xs_ftype); M("setOutputFileType:", xs_set_ftype);
    M("supportedFileTypes", xs_supported); M("determineCompatibleFileTypesWithCompletionHandler:", xs_determine_types);
    M("status", xs_status); M("error", xs_error); M("progress", xs_progress);
    M("shouldOptimizeForNetworkUse", xs_fast); M("setShouldOptimizeForNetworkUse:", xs_set_fast);
    M("timeRange", xs_range); M("setTimeRange:", xs_set_range);
    M("maxDuration", xs_maxdur);
    M("fileLengthLimit", xs_maxlen); M("setFileLengthLimit:", xs_set_maxlen);
    M("metadata", xs_metadata); M("setMetadata:", xs_set_metadata);
    M("audioMix", xs_audiomix); M("setAudioMix:", xs_set_audiomix);
    M("videoComposition", xs_vcomp); M("setVideoComposition:", xs_set_vcomp);
    M("directoryForTemporaryFiles", xs_tmpdir); M("setDirectoryForTemporaryFiles:", xs_set_tmpdir);
    M("canPerformMultiplePassesOverSourceMediaData", xs_multipass);
    M("estimatedOutputFileLength", xs_estimated_len); M("estimateOutputFileLengthWithCompletionHandler:", xs_estimate_len);
    M("exportAsynchronouslyWithCompletionHandler:", xs_export);
    M("cancelExport", xs_cancel);
#undef M
#undef CM
}
