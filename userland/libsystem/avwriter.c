/*
 * mOS — AVAssetWriter, AVAssetWriterInput e AVAssetWriterInputPixelBufferAdaptor.
 *
 * MP4 / MOV / M4A / M4V pelo avwcore.c (mp4mux.c); WAV pelo avf_core.c. O que cada entrada aceita:
 *  - vídeo com AVVideoCodecKey = avc1 (AVVideoCodecTypeH264): CVPixelBuffer ('420v', '420f', 'BGRA') do appendSampleBuffer:
 *    ou do adaptador vai para um VTCompressionSession (OpenH264) com AVVideoCompressionPropertiesKey; a faixa nasce no
 *    1º quadro codificado (o avcC sai do SPS/PPS do codificador). O quadro tem de ter AVVideoWidthKey x AVVideoHeightKey
 *    (o mOS não escala);
 *  - vídeo com outputSettings nil: amostras H.264 já comprimidas (avcC tirado do CMVideoFormatDescription), como o
 *    AVAssetReader com outputSettings nil entrega;
 *  - áudio LPCM (AVFormatIDKey = lpcm, 16 bits inteiro) em MOV e WAV: a entrada LPCM (inteiro 16/24/32 ou float 32/64,
 *    intercalado ou não) é convertida para 16 bits com o número de canais pedido; a taxa tem de ser a mesma;
 *  - áudio com outputSettings nil: AAC comprimido (ASC do magic cookie) e LPCM 16 bits.
 * AAC/Opus codificados a partir de PCM dependem do codificador do AudioConverter; até ele existir canAddInput: diz NÃO e
 * o erro explica. As regras da AVFoundation valem: o arquivo não pode existir antes de startWriting (-11823), nada entra
 * antes de startSessionAtSourceTime:, appendSampleBuffer: devolve NO e o writer falha com o erro em -error.
 * Código original do mOS (MIT).
 */
#include "foundation.h"
#include "avf_core.h"
#include "avwcore.h"
#include "mp4mux.h"

typedef int OSStatus;
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void dispatch_after_f(u64 when, void *q, void *ctx, void (*fn)(void *));
u64 dispatch_time(u64 when, long delta);
const char *cx_audio_url_path(id url);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);

typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cmtime_t;
typedef struct { cmtime_t duration, pts, dts; } cmtiming_t;
typedef struct { i32 w, h; } cmdim_t;
typedef struct { double a, b, c, d, tx, ty; } affine_t;
typedef struct { double w, h; } size2_t;
typedef struct {
    double mSampleRate;
    u32 mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved;
} asbd_t;
void *CMSampleBufferGetFormatDescription(void *s);
void *CMSampleBufferGetDataBuffer(void *s);
void *CMSampleBufferGetImageBuffer(void *s);
long CMSampleBufferGetNumSamples(void *s);
size_t CMSampleBufferGetSampleSize(void *s, long i);
int CMSampleBufferGetSampleTimingInfo(void *s, long i, cmtiming_t *out);
cmtime_t CMSampleBufferGetPresentationTimeStamp(void *s);
cmtime_t CMSampleBufferGetDuration(void *s);
id CMSampleBufferGetSampleAttachmentsArray(void *s, u8 create);
size_t CMBlockBufferGetDataLength(void *b);
int CMBlockBufferCopyDataBytes(void *b, size_t off, size_t len, void *dst);
u32 CMFormatDescriptionGetMediaSubType(void *d);
u32 CMFormatDescriptionGetMediaType(void *d);
cmdim_t CMVideoFormatDescriptionGetDimensions(void *d);
int CMVideoFormatDescriptionGetH264ParameterSetAtIndex(void *d, size_t i, const u8 **p, size_t *s, size_t *n, int *nal);
const void *CMAudioFormatDescriptionGetMagicCookie(void *d, size_t *size);
const void *CMAudioFormatDescriptionGetStreamBasicDescription(void *d);
OSStatus VTCompressionSessionCreate(void *a, i32 w, i32 h, u32 codec, id spec, id src, void *ca, void *cb, void *ref, void **out);
OSStatus VTSessionSetProperties(void *s, id props);
OSStatus VTCompressionSessionEncodeFrame(void *s, void *img, cmtime_t pts, cmtime_t dur, id props, void *ref, u32 *info);
OSStatus VTCompressionSessionCompleteFrames(void *s, cmtime_t until);
void VTCompressionSessionInvalidate(void *s);
u64 CVPixelBufferGetWidth(void *p);
u64 CVPixelBufferGetHeight(void *p);
int CVPixelBufferPoolCreate(void *alloc, id pool_attrs, id pb_attrs, void **out);
extern const void *kCMSampleAttachmentKey_NotSync;

#define FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
#define BLK_FN(b) (((void **)(b))[2])
enum { ST_UNKNOWN = 0, ST_WRITING = 1, ST_COMPLETED = 2, ST_FAILED = 3, ST_CANCELLED = 4 };
enum { C_WAV = -1 };

typedef struct {
    cx_class_t *isa;
    id url, ftype, error, inputs, metadata, tmpdir;
    int status, container;      /* container: MP4W_* ou C_WAV */
    avw_t *core;
    avf_write_t wav;
    int wav_open;
    BOOL fast;
    i32 movie_ts;
    cmtime_t frag, hint;
    int session;
} aw_t;
typedef struct {
    cx_class_t *isa;
    id media, settings, hint, metadata, lang, xlang;
    id writer;                  /* fraco: o writer guarda as entradas em inputs */
    int kind;                   /* 1 áudio, 2 vídeo */
    int idx;                    /* entrada no avwcore, -1 antes de startWriting */
    BOOL realtime, finished, enabled, multipass;
    affine_t tf;
    size2_t natural;
    float volume;
    i32 mts;
    void *vt;
    void *req_blk, *req_q;
    int requesting;
    i16 *cvt; size_t cvt_cap;
    u8 *pkt; size_t pkt_cap;
} ai_t;
typedef struct { cx_class_t *isa; id input, attrs; void *pool; } apa_t;
#define AW(o) ((aw_t *)(o))
#define AI(o) ((ai_t *)(o))
#define APA(o) ((apa_t *)(o))
cx_class_t AVAssetWriter_c __asm__("OBJC_CLASS_$_AVAssetWriter"), AVAssetWriter_m __asm__("OBJC_METACLASS_$_AVAssetWriter");
cx_class_t AVAssetWriterInput_c __asm__("OBJC_CLASS_$_AVAssetWriterInput"), AVAssetWriterInput_m __asm__("OBJC_METACLASS_$_AVAssetWriterInput");
cx_class_t AVAssetWriterInputPixelBufferAdaptor_c __asm__("OBJC_CLASS_$_AVAssetWriterInputPixelBufferAdaptor"),
    AVAssetWriterInputPixelBufferAdaptor_m __asm__("OBJC_METACLASS_$_AVAssetWriterInputPixelBufferAdaptor");

/* -[NSObject dealloc] depois de soltar o que é nosso */
static void super_dealloc(id s, SEL c) { IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c); if (d) ((void (*)(id, SEL))d)(s, c); }
static id cls(const char *n) { return (id)cx_class_named(n); }
static BOOL resp(id o, const char *s) { return o && cx_responds(object_getClass(o), sel_registerName(s)); }
static void setobj(id *slot, id v) { if (*slot == v) return; if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static id dget(id d, const char *k) { return d && resp(d, "objectForKey:") ? MSG(id, d, "objectForKey:", cx_str(k)) : 0; }
static double dnum(id d, const char *k, double dflt) { id n = dget(d, k); return n && resp(n, "doubleValue") ? MSG0(double, n, "doubleValue") : dflt; }
static const char *dstr(id d, const char *k) { id s = dget(d, k); return s && cx_is_str(s) ? cx_cstr(s) : 0; }
static id error_with(i64 code, const char *msg) {
    id info = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, info, "setObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("AVFoundationErrorDomain"), code, info);
}
static cmtime_t cm_invalid(void) { cmtime_t t = {0, 0, 0, 0}; return t; }
static int cm_valid(cmtime_t t) { return (t.flags & 1) && t.timescale > 0; }
static avw_time to_avw(cmtime_t t) { avw_time r = {t.value, cm_valid(t) ? t.timescale : 0}; return r; }

/* ---------------------------------------------------------------- tipos de arquivo */
static int container_of(const char *ft) {
    if (!ft) return -2;
    if (!strcmp(ft, "com.apple.quicktime-movie")) return MP4W_MOV;
    if (!strcmp(ft, "public.mpeg-4") || !strcmp(ft, "com.apple.m4v-video") || !strcmp(ft, "public.3gpp")) return MP4W_MP4;
    if (!strcmp(ft, "com.apple.m4a-audio")) return MP4W_M4A;
    if (!strcmp(ft, "com.microsoft.waveform-audio")) return C_WAV;
    return -2;
}
static int file_exists(const char *p) { FILE_ *f = p ? fopen(p, "rb") : 0; if (f) fclose(f); return f != 0; }

static void writer_fail(id w, i64 code, const char *msg) {
    if (!w || AW(w)->status != ST_WRITING) return;
    AW(w)->status = ST_FAILED;
    setobj(&AW(w)->error, error_with(code, msg));
    for (u64 i = 0, n = MSG0(u64, AW(w)->inputs, "count"); i < n; i++) {
        ai_t *x = AI(MSG(id, AW(w)->inputs, "objectAtIndex:", i));
        if (x->vt) { VTCompressionSessionInvalidate(x->vt); objc_release((id)x->vt); x->vt = 0; }
    }
    if (AW(w)->core) { avw_cancel(AW(w)->core); AW(w)->core = 0; }
    if (AW(w)->wav_open) { avf_write_cancel(&AW(w)->wav); AW(w)->wav_open = 0; }
}
static const char *core_msg(int rc) {
    switch (rc) {
    case AVW_ERR_SESSION: return "AVAssetWriter: chame startSessionAtSourceTime: antes de acrescentar amostras.";
    case AVW_ERR_ORDER: return "AVAssetWriter: o tempo de decodificação das amostras tem de crescer.";
    case AVW_ERR_FORMAT: return "AVAssetWriter: formato da amostra não combina com a entrada.";
    case AVW_ERR_IO: return "AVAssetWriter: falha ao gravar o arquivo.";
    default: return "AVAssetWriter: estado inválido.";
    }
}

/* ---------------------------------------------------------------- o que cada entrada pede */
typedef struct { int ok, codec; u32 fmt; int w, h, rate, ch, bits, isfloat, big, nonint; const char *why; } spec_t;
static spec_t spec_of(id settings, int kind, int container) {
    spec_t s;
    memset(&s, 0, sizeof s);
    s.ok = 1;
    if (kind == 2) {
        if (container == MP4W_M4A || container == C_WAV) { s.ok = 0; s.why = "Este tipo de arquivo não leva vídeo."; return s; }
        if (!settings) return s;   /* passthrough */
        const char *codec = dstr(settings, "AVVideoCodecKey");
        s.w = (int)dnum(settings, "Width", 0); s.h = (int)dnum(settings, "Height", 0);
        if (!codec || strcmp(codec, "avc1")) { s.ok = 0; s.why = "O mOS só codifica vídeo H.264 (AVVideoCodecTypeH264)."; return s; }
        if (s.w < 16 || s.h < 16 || (s.w & 1) || (s.h & 1)) { s.ok = 0; s.why = "AVVideoWidthKey/AVVideoHeightKey inválidos (pares, a partir de 16)."; return s; }
        s.codec = 1;
        return s;
    }
    if (!settings) return s;
    s.fmt = (u32)dnum(settings, "AVFormatIDKey", 0);
    s.rate = (int)dnum(settings, "AVSampleRateKey", 0);
    s.ch = (int)dnum(settings, "AVNumberOfChannelsKey", 0);
    s.bits = (int)dnum(settings, "AVLinearPCMBitDepthKey", 16);
    s.isfloat = dnum(settings, "AVLinearPCMIsFloatKey", 0) != 0;
    s.big = dnum(settings, "AVLinearPCMIsBigEndianKey", 0) != 0;
    s.nonint = dnum(settings, "AVLinearPCMIsNonInterleaved", 0) != 0;
    if (s.fmt == FCC('l', 'p', 'c', 'm')) {
        if (container != MP4W_MOV && container != C_WAV) { s.ok = 0; s.why = "LPCM só vai em MOV ou WAV (como na AVFoundation)."; return s; }
        if (s.bits != 16 || s.isfloat || s.big || s.nonint) { s.ok = 0; s.why = "O mOS grava LPCM de 16 bits inteiro, little-endian, intercalado."; return s; }
        if (s.ch < 0 || s.ch > 8 || s.rate < 0) { s.ok = 0; s.why = "AVNumberOfChannelsKey/AVSampleRateKey inválidos."; return s; }
        s.codec = 4;
        return s;
    }
    if (s.fmt == FCC('a', 'a', 'c', ' ') || s.fmt == FCC('a', 'a', 'c', 'h') || s.fmt == FCC('o', 'p', 'u', 's')) {
        s.ok = 0; s.why = "O codificador AAC/Opus do AudioConverter ainda não está no mOS: grave LPCM em MOV/WAV ou passe AAC já codificado.";
        return s;
    }
    s.ok = 0; s.why = "Formato de áudio sem codificador no mOS.";
    return s;
}

/* ---------------------------------------------------------------- AVAssetWriter */
static id aw_init(id s, SEL c, id url, id ftype, id *err) {
    (void)c;
    const char *ft = ftype ? cx_cstr(ftype) : 0;
    int k = container_of(ft);
    if (!url || k == -2) {
        if (err) *err = error_with(-11822, "Tipo de arquivo que o AVAssetWriter do mOS ainda não grava (MOV, MP4, M4V, M4A e WAV gravam).");
        objc_release(s);
        return 0;
    }
    setobj(&AW(s)->url, url);
    setobj(&AW(s)->ftype, ftype);
    AW(s)->inputs = objc_retain(MSG0(id, cls("NSMutableArray"), "array"));
    AW(s)->container = k;
    AW(s)->movie_ts = 0;
    AW(s)->frag = cm_invalid();
    AW(s)->hint = cm_invalid();
    return s;
}
static id aw_with(id k, SEL c, id url, id ftype, id *err) { (void)k; return objc_autorelease(aw_init(cx_alloc_instance(&AVAssetWriter_c), c, url, ftype, err)); }
static void aw_dealloc(id s, SEL c) {
    if (AW(s)->core) avw_cancel(AW(s)->core);
    if (AW(s)->wav_open) avf_write_cancel(&AW(s)->wav);
    setobj(&AW(s)->url, 0); setobj(&AW(s)->ftype, 0); setobj(&AW(s)->error, 0); setobj(&AW(s)->inputs, 0);
    setobj(&AW(s)->metadata, 0); setobj(&AW(s)->tmpdir, 0);
    super_dealloc(s, c);
}
static id aw_url(id s, SEL c) { (void)c; return AW(s)->url; }
static id aw_ftype(id s, SEL c) { (void)c; return AW(s)->ftype; }
static i64 aw_status(id s, SEL c) { (void)c; return AW(s)->status; }
static id aw_error(id s, SEL c) { (void)c; return AW(s)->error; }
static id aw_inputs(id s, SEL c) { (void)c; return MSG0(id, AW(s)->inputs, "copy"); }
static id aw_media_types(id s, SEL c) {
    (void)c;
    id a = MSG0(id, cls("NSMutableArray"), "array");
    MSG(void, a, "addObject:", cx_str("soun"));
    if (AW(s)->container != MP4W_M4A && AW(s)->container != C_WAV) MSG(void, a, "addObject:", cx_str("vide"));
    return a;
}
static BOOL aw_fast(id s, SEL c) { (void)c; return AW(s)->fast; }
static void aw_set_fast(id s, SEL c, BOOL v) { (void)c; if (AW(s)->status == ST_UNKNOWN) AW(s)->fast = v; }
static i32 aw_movie_ts(id s, SEL c) { (void)c; return AW(s)->movie_ts; }
static void aw_set_movie_ts(id s, SEL c, i32 v) { (void)c; AW(s)->movie_ts = v; }
static cmtime_t aw_frag(id s, SEL c) { (void)c; return AW(s)->frag; }
static void aw_set_frag(id s, SEL c, cmtime_t t) { (void)c; AW(s)->frag = t; }   /* o filme sai inteiro (não fragmentado) */
static cmtime_t aw_hint(id s, SEL c) { (void)c; return AW(s)->hint; }
static void aw_set_hint(id s, SEL c, cmtime_t t) { (void)c; AW(s)->hint = t; }
static id aw_metadata(id s, SEL c) { (void)c; return AW(s)->metadata ? AW(s)->metadata : MSG0(id, cls("NSArray"), "array"); }
static void aw_set_metadata(id s, SEL c, id m) { (void)c; setobj(&AW(s)->metadata, m ? MSG0(id, m, "copy") : 0); }
static id aw_tmpdir(id s, SEL c) { (void)c; return AW(s)->tmpdir; }
static void aw_set_tmpdir(id s, SEL c, id u) { (void)c; setobj(&AW(s)->tmpdir, u); }

static BOOL aw_can_apply(id s, SEL c, id settings, id media) {
    (void)c;
    const char *m = media ? cx_cstr(media) : "";
    int kind = m && !strcmp(m, "vide") ? 2 : m && !strcmp(m, "soun") ? 1 : 0;
    return kind && spec_of(settings, kind, AW(s)->container).ok;
}
static BOOL aw_can_add(id s, SEL c, id in) {
    (void)c;
    if (!in || AW(s)->status != ST_UNKNOWN || !AI(in)->kind || AI(in)->writer) return 0;
    if (MSG0(u64, AW(s)->inputs, "count") >= 8) return 0;
    return spec_of(AI(in)->settings, AI(in)->kind, AW(s)->container).ok;
}
static void aw_add(id s, SEL c, id in) {
    (void)c;
    if (!aw_can_add(s, 0, in)) {
        spec_t sp = in ? spec_of(AI(in)->settings, AI(in)->kind, AW(s)->container) : (spec_t){0};
        setobj(&AW(s)->error, error_with(-11821, sp.why ? sp.why : "AVAssetWriter: esta entrada não pode ser acrescentada."));
        return;
    }
    MSG(void, AW(s)->inputs, "addObject:", in);
    AI(in)->writer = s;
}
static BOOL aw_start(id s, SEL c) {
    (void)c;
    if (AW(s)->status != ST_UNKNOWN) return 0;
    const char *path = cx_audio_url_path(AW(s)->url);
    AW(s)->status = ST_WRITING;
    if (!path) { writer_fail(s, -11800, "AVAssetWriter: a URL de saída precisa ser um arquivo local."); return 0; }
    if (file_exists(path)) { writer_fail(s, -11823, "Não é possível salvar: já existe um arquivo nesse caminho."); return 0; }
    u64 n = MSG0(u64, AW(s)->inputs, "count");
    if (AW(s)->container == C_WAV) {
        if (n != 1) { writer_fail(s, -11821, "WAV leva exatamente uma entrada de áudio."); return 0; }
        return 1;   /* o WAV abre na 1ª amostra (taxa e canais) */
    }
    AW(s)->core = avw_open(path, AW(s)->container, AW(s)->fast);
    if (!AW(s)->core) { writer_fail(s, -11800, "AVAssetWriter: não foi possível criar o arquivo."); return 0; }
    for (u64 i = 0; i < n; i++) {
        ai_t *x = AI(MSG(id, AW(s)->inputs, "objectAtIndex:", i));
        int32_t m[9], *mp = 0;
        if (x->kind == 2) {   /* CGAffineTransform -> matriz do tkhd (16.16; u, v, w em 2.30) */
            m[0] = (int32_t)(x->tf.a * 65536); m[1] = (int32_t)(x->tf.b * 65536); m[2] = 0;
            m[3] = (int32_t)(x->tf.c * 65536); m[4] = (int32_t)(x->tf.d * 65536); m[5] = 0;
            m[6] = (int32_t)(x->tf.tx * 65536); m[7] = (int32_t)(x->tf.ty * 65536); m[8] = 0x40000000;
            mp = m;
        }
        x->idx = avw_add_input(AW(s)->core, x->kind == 2 ? AVW_VIDEO : AVW_AUDIO, x->mts > 0 ? (uint32_t)x->mts : 0, mp);
        if (x->idx < 0) { writer_fail(s, -11821, "AVAssetWriter: entrada recusada pelo contêiner."); return 0; }
    }
    return 1;
}
static void aw_start_session(id s, SEL c, cmtime_t t) {
    (void)c;
    if (AW(s)->status != ST_WRITING || AW(s)->session) return;
    AW(s)->session = 1;
    if (AW(s)->core && avw_start_session(AW(s)->core, to_avw(t))) writer_fail(s, -11800, "AVAssetWriter: tempo de início inválido.");
}
static void aw_end_session(id s, SEL c, cmtime_t t) {
    (void)c;
    if (AW(s)->status != ST_WRITING || !AW(s)->session) return;
    if (AW(s)->core && cm_valid(t)) avw_end_session(AW(s)->core, to_avw(t));
}
static void inputs_flush(id s) {
    for (u64 i = 0, n = MSG0(u64, AW(s)->inputs, "count"); i < n; i++) {
        ai_t *x = AI(MSG(id, AW(s)->inputs, "objectAtIndex:", i));
        x->requesting = 0;
        if (x->vt) {
            VTCompressionSessionCompleteFrames(x->vt, cm_invalid());
            VTCompressionSessionInvalidate(x->vt);
            objc_release((id)x->vt);
            x->vt = 0;
        }
    }
}
static BOOL aw_finish(id s, SEL c) {
    (void)c;
    if (AW(s)->status != ST_WRITING) return 0;
    inputs_flush(s);
    if (AW(s)->status != ST_WRITING) return 0;   /* o último quadro pode ter falhado */
    if (!AW(s)->session) { writer_fail(s, -11800, "AVAssetWriter: finishWriting sem startSessionAtSourceTime:."); return 0; }
    if (AW(s)->container == C_WAV) {
        const char *path = cx_audio_url_path(AW(s)->url);
        if (!AW(s)->wav_open) avf_write_begin(&AW(s)->wav, "com.microsoft.waveform-audio", 44100, 2);   /* sem amostras: WAV vazio */
        AW(s)->wav_open = 0;
        if (!avf_write_finish(&AW(s)->wav, path)) {
            AW(s)->status = ST_FAILED;
            setobj(&AW(s)->error, error_with(-11800, "AVAssetWriter: o WAV não foi gravado."));
            return 0;
        }
        AW(s)->status = ST_COMPLETED;
        return 1;
    }
    avw_t *core = AW(s)->core;
    AW(s)->core = 0;
    if (avw_finish(core)) {
        AW(s)->status = ST_FAILED;
        setobj(&AW(s)->error, error_with(-11800, "AVAssetWriter: falha ao fechar o arquivo."));
        return 0;
    }
    AW(s)->status = ST_COMPLETED;
    return 1;
}
static void aw_finish_handler(id s, SEL c, void *blk) {
    aw_finish(s, c);
    if (blk) ((void (*)(void *))BLK_FN(blk))(blk);   /* já terminou: chama aqui mesmo (sem prender a fila de quem espera) */
}
static void aw_cancel(id s, SEL c) {
    (void)c;
    if (AW(s)->status != ST_WRITING && AW(s)->status != ST_UNKNOWN) return;
    inputs_flush(s);
    if (AW(s)->core) { avw_cancel(AW(s)->core); AW(s)->core = 0; }
    if (AW(s)->wav_open) { avf_write_cancel(&AW(s)->wav); AW(s)->wav_open = 0; }
    AW(s)->status = ST_CANCELLED;
}

/* ---------------------------------------------------------------- AVAssetWriterInput */
static id ai_init_hint(id s, SEL c, id media, id settings, void *hint) {
    (void)c;
    const char *m = media ? cx_cstr(media) : "";
    AI(s)->kind = m && !strcmp(m, "vide") ? 2 : m && !strcmp(m, "soun") ? 1 : 0;
    setobj(&AI(s)->media, media);
    setobj(&AI(s)->settings, settings ? MSG0(id, settings, "copy") : 0);
    setobj(&AI(s)->hint, (id)hint);
    AI(s)->idx = -1;
    AI(s)->enabled = 1;
    AI(s)->volume = 1;
    AI(s)->tf = (affine_t){1, 0, 0, 1, 0, 0};
    return s;
}
static id ai_init(id s, SEL c, id media, id settings) { return ai_init_hint(s, c, media, settings, 0); }
static id ai_with(id k, SEL c, id media, id settings) { (void)k; return objc_autorelease(ai_init_hint(cx_alloc_instance(&AVAssetWriterInput_c), c, media, settings, 0)); }
static id ai_with_hint(id k, SEL c, id media, id settings, void *hint) { (void)k; return objc_autorelease(ai_init_hint(cx_alloc_instance(&AVAssetWriterInput_c), c, media, settings, hint)); }
static void ai_dealloc(id s, SEL c) {
    if (AI(s)->vt) { VTCompressionSessionInvalidate(AI(s)->vt); objc_release((id)AI(s)->vt); }
    if (AI(s)->req_blk) _Block_release(AI(s)->req_blk);
    free(AI(s)->cvt); free(AI(s)->pkt);
    setobj(&AI(s)->media, 0); setobj(&AI(s)->settings, 0); setobj(&AI(s)->hint, 0); setobj(&AI(s)->metadata, 0);
    setobj(&AI(s)->lang, 0); setobj(&AI(s)->xlang, 0);
    super_dealloc(s, c);
}
static id ai_media(id s, SEL c) { (void)c; return AI(s)->media; }
static id ai_settings(id s, SEL c) { (void)c; return AI(s)->settings; }
static void *ai_hint(id s, SEL c) { (void)c; return AI(s)->hint; }
static BOOL ai_ready(id s, SEL c) {
    (void)c;
    id w = AI(s)->writer;
    return w && AW(w)->status == ST_WRITING && !AI(s)->finished;
}
static BOOL ai_realtime(id s, SEL c) { (void)c; return AI(s)->realtime; }
static void ai_set_realtime(id s, SEL c, BOOL v) { (void)c; AI(s)->realtime = v; }
static affine_t ai_transform(id s, SEL c) { (void)c; return AI(s)->tf; }
static void ai_set_transform(id s, SEL c, affine_t t) { (void)c; if (!AI(s)->writer || AW(AI(s)->writer)->status == ST_UNKNOWN) AI(s)->tf = t; }
static i32 ai_mts(id s, SEL c) { (void)c; return AI(s)->mts; }
static void ai_set_mts(id s, SEL c, i32 v) { (void)c; AI(s)->mts = v; }
static id ai_lang(id s, SEL c) { (void)c; return AI(s)->lang; }
static void ai_set_lang(id s, SEL c, id v) { (void)c; setobj(&AI(s)->lang, v); }
static id ai_xlang(id s, SEL c) { (void)c; return AI(s)->xlang; }
static void ai_set_xlang(id s, SEL c, id v) { (void)c; setobj(&AI(s)->xlang, v); }
static id ai_metadata(id s, SEL c) { (void)c; return AI(s)->metadata ? AI(s)->metadata : MSG0(id, cls("NSArray"), "array"); }
static void ai_set_metadata(id s, SEL c, id v) { (void)c; setobj(&AI(s)->metadata, v); }
static size2_t ai_natural(id s, SEL c) { (void)c; return AI(s)->natural; }
static void ai_set_natural(id s, SEL c, size2_t v) { (void)c; AI(s)->natural = v; }
static float ai_volume(id s, SEL c) { (void)c; return AI(s)->volume; }
static void ai_set_volume(id s, SEL c, float v) { (void)c; AI(s)->volume = v; }
static BOOL ai_enabled(id s, SEL c) { (void)c; return AI(s)->enabled; }
static void ai_set_enabled(id s, SEL c, BOOL v) { (void)c; AI(s)->enabled = v; }
static BOOL ai_multipass(id s, SEL c) { (void)c; return AI(s)->multipass; }
static void ai_set_multipass(id s, SEL c, BOOL v) { (void)c; AI(s)->multipass = v; }
static BOOL ai_can_multipass(id s, SEL c) { (void)s; (void)c; return 0; }   /* uma passada só */

static int ai_fail(id s, int rc) { writer_fail(AI(s)->writer, -11800, core_msg(rc)); return 0; }
static int ai_fail_msg(id s, const char *msg) { writer_fail(AI(s)->writer, -11800, msg); return 0; }

/* avcC (ISO/IEC 14496-15) a partir dos conjuntos de parâmetros do CMVideoFormatDescription */
static int avcc_from_fd(void *fd, u8 *out, u32 cap, u32 *outn) {
    size_t cnt = 0, sz = 0;
    const u8 *p = 0;
    int nalh = 0;
    if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(fd, 0, &p, &sz, &cnt, &nalh) || !cnt) return -1;
    if (nalh != 4) return -1;   /* o mp4mux grava tamanhos de 4 bytes */
    const u8 *sets[16]; size_t lens[16];
    int ns = 0, np = 0;
    const u8 *sps[16], *pps[16]; size_t sl[16], pl[16];
    for (size_t i = 0; i < cnt && i < 16; i++) {
        if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(fd, i, &sets[i], &lens[i], 0, 0) || !lens[i]) return -1;
        int t = sets[i][0] & 31;
        if (t == 7 && ns < 16) { sps[ns] = sets[i]; sl[ns++] = lens[i]; }
        else if (t == 8 && np < 16) { pps[np] = sets[i]; pl[np++] = lens[i]; }
    }
    if (!ns || !np || sl[0] < 4) return -1;
    u32 n = 0;
    if (cap < 7) return -1;
    out[n++] = 1; out[n++] = sps[0][1]; out[n++] = sps[0][2]; out[n++] = sps[0][3]; out[n++] = 0xff; out[n++] = (u8)(0xe0 | ns);
    for (int i = 0; i < ns; i++) {
        if (n + 2 + sl[i] > cap) return -1;
        out[n++] = (u8)(sl[i] >> 8); out[n++] = (u8)sl[i]; memcpy(out + n, sps[i], sl[i]); n += (u32)sl[i];
    }
    if (n + 1 > cap) return -1;
    out[n++] = (u8)np;
    for (int i = 0; i < np; i++) {
        if (n + 2 + pl[i] > cap) return -1;
        out[n++] = (u8)(pl[i] >> 8); out[n++] = (u8)pl[i]; memcpy(out + n, pps[i], pl[i]); n += (u32)pl[i];
    }
    *outn = n;
    return 0;
}
static int not_sync(void *sb, long i) {
    id arr = CMSampleBufferGetSampleAttachmentsArray(sb, 0);
    if (!arr || (u64)i >= MSG0(u64, arr, "count")) return 0;
    id d = MSG(id, arr, "objectAtIndex:", (u64)i);
    id v = d ? MSG(id, d, "objectForKey:", (id)kCMSampleAttachmentKey_NotSync) : 0;
    return v && resp(v, "boolValue") && MSG0(BOOL, v, "boolValue");
}
static u8 *pkt_buf(ai_t *x, size_t n) {
    if (n > x->pkt_cap) {
        u8 *b = realloc(x->pkt, n);
        if (!b) return 0;
        x->pkt = b; x->pkt_cap = n;
    }
    return x->pkt;
}
/* amostras H.264 comprimidas (do VTCompressionSession ou do app) -> avwcore */
static int push_h264(id s, void *sb) {
    ai_t *x = AI(s);
    aw_t *w = AW(x->writer);
    void *fd = CMSampleBufferGetFormatDescription(sb);
    if (!fd || CMFormatDescriptionGetMediaSubType(fd) != FCC('a', 'v', 'c', '1'))
        return ai_fail_msg(s, "AVAssetWriter: sem outputSettings o vídeo tem de vir em H.264 já comprimido.");
    if (!avw_has_format(w->core, x->idx)) {
        u8 cfg[1024]; u32 cn = 0;
        cmdim_t d = CMVideoFormatDescriptionGetDimensions(fd);
        if (avcc_from_fd(fd, cfg, sizeof cfg, &cn)) return ai_fail_msg(s, "AVAssetWriter: CMVideoFormatDescription sem SPS/PPS (ou com tamanho de NAL diferente de 4).");
        int rc = avw_set_h264(w->core, x->idx, d.w, d.h, cfg, cn);
        if (rc) return ai_fail(s, rc);
    }
    void *bb = CMSampleBufferGetDataBuffer(sb);
    long ns = CMSampleBufferGetNumSamples(sb);
    size_t total = bb ? CMBlockBufferGetDataLength(bb) : 0, off = 0;
    if (!bb || ns < 1) return ai_fail_msg(s, "AVAssetWriter: amostra de vídeo sem dados.");
    for (long i = 0; i < ns; i++) {
        size_t n = ns == 1 ? total : CMSampleBufferGetSampleSize(sb, i);
        cmtiming_t t;
        if (CMSampleBufferGetSampleTimingInfo(sb, i, &t) && CMSampleBufferGetSampleTimingInfo(sb, 0, &t)) return ai_fail_msg(s, "AVAssetWriter: amostra sem tempo.");
        u8 *b = pkt_buf(x, n);
        if (!n || off + n > total || !b || CMBlockBufferCopyDataBytes(bb, off, n, b)) return ai_fail_msg(s, "AVAssetWriter: amostra de vídeo corrompida.");
        int rc = avw_append(w->core, x->idx, b, (uint32_t)n, to_avw(t.pts), to_avw(t.dts), to_avw(t.duration), 0, !not_sync(sb, i));
        if (rc) return ai_fail(s, rc);
        off += n;
    }
    return 1;
}
/* saída do VTCompressionSession (síncrono: chega dentro do EncodeFrame) */
static void vt_output(void *ref, void *src, OSStatus st, u32 flags, void *sb) {
    (void)src;
    id s = (id)ref;
    if (st) { ai_fail_msg(s, "AVAssetWriter: o codificador H.264 falhou."); return; }
    if (!sb || (flags & 2)) return;   /* quadro descartado pelo controle de taxa */
    push_h264(s, sb);
}
static int encode_image(id s, void *img, cmtime_t pts, cmtime_t dur) {
    ai_t *x = AI(s);
    aw_t *w = AW(x->writer);
    spec_t sp = spec_of(x->settings, 2, w->container);
    if (!sp.codec) return ai_fail_msg(s, "AVAssetWriter: quadros sem compressão precisam de outputSettings com AVVideoCodecKey.");
    if ((int)CVPixelBufferGetWidth(img) != sp.w || (int)CVPixelBufferGetHeight(img) != sp.h)
        return ai_fail_msg(s, "AVAssetWriter: o quadro tem de ter AVVideoWidthKey x AVVideoHeightKey (o mOS não escala).");
    if (!x->vt) {
        if (VTCompressionSessionCreate(0, sp.w, sp.h, FCC('a', 'v', 'c', '1'), 0, 0, 0, (void *)vt_output, s, &x->vt) || !x->vt)
            return ai_fail_msg(s, "AVAssetWriter: não foi possível abrir o codificador H.264.");
        id props = dget(x->settings, "AVVideoCompressionPropertiesKey");
        if (props) VTSessionSetProperties(x->vt, props);
    }
    OSStatus st = VTCompressionSessionEncodeFrame(x->vt, img, pts, dur, 0, 0, 0);
    if (st) return ai_fail_msg(s, "AVAssetWriter: o codificador H.264 recusou o quadro (formato de pixel: use 420v, 420f ou BGRA).");
    return w->status == ST_WRITING;
}

/* uma amostra LPCM qualquer -> inteiro de 16 bits */
static int get_s16(const asbd_t *a, const u8 *d, u32 frames, u32 f, u32 ch) {
    int nonint = (a->mFormatFlags & 0x20) != 0, isfloat = (a->mFormatFlags & 1) != 0, big = (a->mFormatFlags & 2) != 0;
    u32 bytes = a->mBitsPerChannel / 8;
    const u8 *p = nonint ? d + (size_t)ch * frames * bytes + (size_t)f * bytes : d + (size_t)f * a->mBytesPerFrame + (size_t)ch * bytes;
    u8 q[8];
    for (u32 i = 0; i < bytes && i < 8; i++) q[i] = big ? p[bytes - 1 - i] : p[i];
    if (isfloat) {
        double v;
        if (bytes == 4) { float fv; memcpy(&fv, q, 4); v = fv; } else { memcpy(&v, q, 8); }
        v *= 32768.0;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        return (int)(v < 0 ? v - 0.5 : v + 0.5);
    }
    i32 iv;
    if (bytes == 2) iv = (i16)(q[0] | q[1] << 8);
    else if (bytes == 3) iv = (i32)((u32)q[0] << 8 | (u32)q[1] << 16 | (u32)q[2] << 24) >> 16;
    else if (bytes == 4) iv = (i32)((u32)q[0] | (u32)q[1] << 8 | (u32)q[2] << 16 | (u32)q[3] << 24) >> 16;
    else iv = ((i32)q[0] - 128) * 256;   /* 8 bits sem sinal */
    return iv;
}
static int push_audio(id s, void *sb) {
    ai_t *x = AI(s);
    aw_t *w = AW(x->writer);
    void *fd = CMSampleBufferGetFormatDescription(sb);
    const asbd_t *a = fd ? CMAudioFormatDescriptionGetStreamBasicDescription(fd) : 0;
    void *bb = CMSampleBufferGetDataBuffer(sb);
    if (!a || !bb) return ai_fail_msg(s, "AVAssetWriter: amostra de áudio sem formato ou sem dados.");
    size_t total = CMBlockBufferGetDataLength(bb);
    cmtime_t pts = CMSampleBufferGetPresentationTimeStamp(sb);
    spec_t sp = spec_of(x->settings, 1, w->container);
    if (a->mFormatID == FCC('l', 'p', 'c', 'm')) {
        u32 inch = a->mChannelsPerFrame, bytes = a->mBitsPerChannel / 8;
        int nonint = (a->mFormatFlags & 0x20) != 0;
        if (!inch || !bytes || bytes > 8 || (!nonint && a->mBytesPerFrame < inch * bytes)) return ai_fail_msg(s, "AVAssetWriter: LPCM de entrada inválido.");
        u32 frames = (u32)(total / (nonint ? (size_t)bytes * inch : a->mBytesPerFrame));
        if (!frames) return 1;
        int rate = sp.rate > 0 ? sp.rate : (int)(a->mSampleRate + 0.5), och = sp.ch > 0 ? sp.ch : (int)inch;
        if ((int)(a->mSampleRate + 0.5) != rate) return ai_fail_msg(s, "AVAssetWriter: a taxa da entrada difere de AVSampleRateKey (o mOS ainda não reamostra aqui).");
        if (x->settings == 0 && (bytes != 2 || (a->mFormatFlags & 1))) return ai_fail_msg(s, "AVAssetWriter: sem outputSettings o LPCM tem de ser inteiro de 16 bits.");
        u8 *raw = pkt_buf(x, total);
        size_t need = (size_t)frames * (size_t)och;
        if (need > x->cvt_cap) { i16 *n = realloc(x->cvt, need * 2); if (!n) return ai_fail_msg(s, "AVAssetWriter: sem memória."); x->cvt = n; x->cvt_cap = need; }
        if (!raw || CMBlockBufferCopyDataBytes(bb, 0, total, raw)) return ai_fail_msg(s, "AVAssetWriter: amostra de áudio corrompida.");
        for (u32 f = 0; f < frames; f++)
            for (int c = 0; c < och; c++) {
                int v;
                if (och == 1 && inch >= 2) v = (get_s16(a, raw, frames, f, 0) + get_s16(a, raw, frames, f, 1)) / 2;
                else v = get_s16(a, raw, frames, f, (u32)c < inch ? (u32)c : inch - 1);
                x->cvt[(size_t)f * och + c] = (i16)v;
            }
        if (w->container == C_WAV) {
            if (!w->wav_open) {
                if (!avf_write_begin(&w->wav, "com.microsoft.waveform-audio", (unsigned)rate, (unsigned)och)) return ai_fail_msg(s, "AVAssetWriter: WAV com taxa ou canais inválidos.");
                w->wav_open = 1;
            }
            if (avf_write_append(&w->wav, x->cvt, frames) != frames) return ai_fail_msg(s, "AVAssetWriter: sem memória para o WAV.");
            return 1;
        }
        if (!avw_has_format(w->core, x->idx)) {
            int rc = avw_set_lpcm(w->core, x->idx, rate, och, 16);
            if (rc) return ai_fail_msg(s, "AVAssetWriter: LPCM só vai em MOV (16 bits).");
        }
        int rc = avw_append(w->core, x->idx, (const u8 *)x->cvt, (uint32_t)(need * 2), to_avw(pts), to_avw(cm_invalid()), to_avw(cm_invalid()), frames, 1);
        return rc ? ai_fail(s, rc) : 1;
    }
    if (x->settings) return ai_fail_msg(s, "AVAssetWriter: com outputSettings LPCM a entrada tem de ser LPCM.");
    if (a->mFormatID == FCC('a', 'a', 'c', ' ') && w->container != C_WAV) {   /* AAC já codificado */
        if (!avw_has_format(w->core, x->idx)) {
            size_t cn = 0;
            const u8 *ck = CMAudioFormatDescriptionGetMagicCookie(fd, &cn), *asc = ck;
            size_t an = cn;
            if (ck && cn > 2 && (ck[0] == 3 || ck[0] == 4)) {   /* cookie em esds: procura o DecoderSpecificInfo (tag 5) */
                asc = 0;
                for (size_t i = 0; i + 2 < cn; i++)
                    if (ck[i] == 5) {
                        size_t j = i + 1, len = 0;
                        for (int k = 0; k < 4 && j < cn; k++) { len = len << 7 | (ck[j] & 0x7f); if (!(ck[j++] & 0x80)) break; }
                        if (len >= 2 && j + len <= cn) { asc = ck + j; an = len; break; }
                    }
            }
            if (!asc || an < 2) return ai_fail_msg(s, "AVAssetWriter: AAC sem AudioSpecificConfig no magic cookie.");
            int rc = avw_set_aac(w->core, x->idx, (int)(a->mSampleRate + 0.5), (int)a->mChannelsPerFrame, asc, (uint32_t)an, 0, 0);
            if (rc) return ai_fail(s, rc);
        }
        long ns = CMSampleBufferGetNumSamples(sb);
        u32 fpp = a->mFramesPerPacket ? a->mFramesPerPacket : 1024;
        size_t off = 0;
        u8 *raw = pkt_buf(x, total);
        if (!raw || CMBlockBufferCopyDataBytes(bb, 0, total, raw)) return ai_fail_msg(s, "AVAssetWriter: amostra de áudio corrompida.");
        for (long i = 0; i < ns; i++) {
            size_t n = ns == 1 ? total : CMSampleBufferGetSampleSize(sb, i);
            if (!n || off + n > total) return ai_fail_msg(s, "AVAssetWriter: pacote AAC corrompido.");
            cmtime_t t = pts;
            if (cm_valid(pts)) {   /* pts do pacote i: o da amostra + i * quadros por pacote */
                t.value = pts.value + (i64)i * fpp * pts.timescale / (i64)(a->mSampleRate + 0.5);
            }
            int rc = avw_append(w->core, x->idx, raw + off, (uint32_t)n, to_avw(t), to_avw(cm_invalid()), to_avw(cm_invalid()), fpp, 1);
            if (rc) return ai_fail(s, rc);
            off += n;
        }
        return 1;
    }
    return ai_fail_msg(s, "AVAssetWriter: formato de áudio de entrada sem caminho no mOS.");
}
static BOOL ai_append(id s, SEL c, void *sb) {
    (void)c;
    id w = AI(s)->writer;
    if (!w || AW(w)->status != ST_WRITING || AI(s)->finished || !sb) return 0;
    if (!AW(w)->session) return ai_fail_msg(s, core_msg(AVW_ERR_SESSION));
    if (AI(s)->kind == 2) {
        void *img = CMSampleBufferGetImageBuffer(sb);
        if (img) return encode_image(s, img, CMSampleBufferGetPresentationTimeStamp(sb), CMSampleBufferGetDuration(sb));
        if (AI(s)->settings) return ai_fail_msg(s, "AVAssetWriter: com outputSettings a entrada de vídeo recebe CVPixelBuffer.");
        return push_h264(s, sb);
    }
    return push_audio(s, sb);
}
static void ai_mark_finished(id s, SEL c) {
    (void)c;
    AI(s)->finished = 1;
    AI(s)->requesting = 0;
    id w = AI(s)->writer;
    if (w && AW(w)->core && AI(s)->idx >= 0) avw_mark_finished(AW(w)->core, AI(s)->idx);
}
/* requestMediaDataWhenReadyOnQueue:usingBlock: o bloco roda na fila do app enquanto a entrada estiver pronta e não
 * terminada; como a gravação é síncrona, a entrada está sempre pronta: o bloco volta a ser chamado depois de 5 ms */
static void req_fire(void *ctx) {
    id s = (id)ctx;
    ai_t *x = AI(s);
    if (x->requesting && ai_ready(s, 0) && x->req_blk) {
        ((void (*)(void *))BLK_FN(x->req_blk))(x->req_blk);
        if (x->requesting && ai_ready(s, 0)) {
            dispatch_after_f(dispatch_time(0, 5000000), x->req_q, objc_retain(s), req_fire);
        }
    }
    objc_release(s);
}
static void ai_request(id s, SEL c, void *q, void *blk) {
    (void)c;
    if (!blk || !q) return;
    if (AI(s)->req_blk) _Block_release(AI(s)->req_blk);
    AI(s)->req_blk = _Block_copy(blk);
    AI(s)->req_q = q;
    AI(s)->requesting = 1;
    dispatch_async_f(q, objc_retain(s), req_fire);
}

/* ---------------------------------------------------------------- AVAssetWriterInputPixelBufferAdaptor */
static id apa_init(id s, SEL c, id input, id attrs) {
    (void)c;
    setobj(&APA(s)->input, input);
    setobj(&APA(s)->attrs, attrs ? MSG0(id, attrs, "copy") : 0);
    return s;
}
static id apa_with(id k, SEL c, id input, id attrs) { (void)k; return objc_autorelease(apa_init(cx_alloc_instance(&AVAssetWriterInputPixelBufferAdaptor_c), c, input, attrs)); }
static void apa_dealloc(id s, SEL c) {
    if (APA(s)->pool) objc_release((id)APA(s)->pool);
    setobj(&APA(s)->input, 0); setobj(&APA(s)->attrs, 0);
    super_dealloc(s, c);
}
static id apa_input(id s, SEL c) { (void)c; return APA(s)->input; }
static id apa_attrs(id s, SEL c) { (void)c; return APA(s)->attrs; }
/* o pool só existe depois de startWriting (como na AVFoundation); tamanho padrão = o das outputSettings, formato BGRA */
static void *apa_pool(id s, SEL c) {
    (void)c;
    id in = APA(s)->input;
    id w = in ? AI(in)->writer : 0;
    if (!w || AW(w)->status != ST_WRITING) return 0;
    if (APA(s)->pool) return APA(s)->pool;
    id d = APA(s)->attrs ? MSG0(id, APA(s)->attrs, "mutableCopy") : MSG0(id, cls("NSMutableDictionary"), "dictionary");
    spec_t sp = spec_of(AI(in)->settings, 2, AW(w)->container);
    if (!dget(d, "Width") && sp.w) MSG(void, d, "setObject:forKey:", cx_num_i(sp.w), cx_str("Width"));
    if (!dget(d, "Height") && sp.h) MSG(void, d, "setObject:forKey:", cx_num_i(sp.h), cx_str("Height"));
    if (!dget(d, "PixelFormatType")) MSG(void, d, "setObject:forKey:", cx_num_i(0x42475241), cx_str("PixelFormatType"));
    void *pool = 0;
    if (CVPixelBufferPoolCreate(0, 0, d, &pool)) pool = 0;
    if (resp(d, "release")) objc_release(d);
    APA(s)->pool = pool;
    return pool;
}
static BOOL apa_append(id s, SEL c, void *pb, cmtime_t pts) {
    (void)c;
    id in = APA(s)->input;
    id w = in ? AI(in)->writer : 0;
    if (!w || !pb || AW(w)->status != ST_WRITING || AI(in)->finished) return 0;
    if (!AW(w)->session) return ai_fail_msg(in, core_msg(AVW_ERR_SESSION));
    return encode_image(in, pb, pts, cm_invalid());
}

void cx_avwriter_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
#define DEF(k, n, sz) if (!cx_class_named(n)) cx_define_class(&k##_c, &k##_m, n, ob, (u32)(sz))
    DEF(AVAssetWriter, "AVAssetWriter", sizeof(aw_t) - 8);
    DEF(AVAssetWriterInput, "AVAssetWriterInput", sizeof(ai_t) - 8);
    DEF(AVAssetWriterInputPixelBufferAdaptor, "AVAssetWriterInputPixelBufferAdaptor", sizeof(apa_t) - 8);
#undef DEF
#define M(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define CM(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))
    CM(AVAssetWriter, "assetWriterWithURL:fileType:error:", aw_with);
    M(AVAssetWriter, "initWithURL:fileType:error:", aw_init); M(AVAssetWriter, "dealloc", aw_dealloc);
    M(AVAssetWriter, "outputURL", aw_url); M(AVAssetWriter, "outputFileType", aw_ftype);
    M(AVAssetWriter, "availableMediaTypes", aw_media_types);
    M(AVAssetWriter, "status", aw_status); M(AVAssetWriter, "error", aw_error); M(AVAssetWriter, "inputs", aw_inputs);
    M(AVAssetWriter, "shouldOptimizeForNetworkUse", aw_fast); M(AVAssetWriter, "setShouldOptimizeForNetworkUse:", aw_set_fast);
    M(AVAssetWriter, "movieTimeScale", aw_movie_ts); M(AVAssetWriter, "setMovieTimeScale:", aw_set_movie_ts);
    M(AVAssetWriter, "movieFragmentInterval", aw_frag); M(AVAssetWriter, "setMovieFragmentInterval:", aw_set_frag);
    M(AVAssetWriter, "overallDurationHint", aw_hint); M(AVAssetWriter, "setOverallDurationHint:", aw_set_hint);
    M(AVAssetWriter, "metadata", aw_metadata); M(AVAssetWriter, "setMetadata:", aw_set_metadata);
    M(AVAssetWriter, "directoryForTemporaryFiles", aw_tmpdir); M(AVAssetWriter, "setDirectoryForTemporaryFiles:", aw_set_tmpdir);
    M(AVAssetWriter, "canApplyOutputSettings:forMediaType:", aw_can_apply);
    M(AVAssetWriter, "canAddInput:", aw_can_add); M(AVAssetWriter, "addInput:", aw_add);
    M(AVAssetWriter, "startWriting", aw_start);
    M(AVAssetWriter, "startSessionAtSourceTime:", aw_start_session); M(AVAssetWriter, "endSessionAtSourceTime:", aw_end_session);
    M(AVAssetWriter, "finishWriting", aw_finish); M(AVAssetWriter, "finishWritingWithCompletionHandler:", aw_finish_handler);
    M(AVAssetWriter, "cancelWriting", aw_cancel);
    CM(AVAssetWriterInput, "assetWriterInputWithMediaType:outputSettings:", ai_with);
    CM(AVAssetWriterInput, "assetWriterInputWithMediaType:outputSettings:sourceFormatHint:", ai_with_hint);
    M(AVAssetWriterInput, "initWithMediaType:outputSettings:", ai_init);
    M(AVAssetWriterInput, "initWithMediaType:outputSettings:sourceFormatHint:", ai_init_hint);
    M(AVAssetWriterInput, "dealloc", ai_dealloc);
    M(AVAssetWriterInput, "mediaType", ai_media); M(AVAssetWriterInput, "outputSettings", ai_settings);
    M(AVAssetWriterInput, "sourceFormatHint", ai_hint);
    M(AVAssetWriterInput, "isReadyForMoreMediaData", ai_ready);
    M(AVAssetWriterInput, "expectsMediaDataInRealTime", ai_realtime); M(AVAssetWriterInput, "setExpectsMediaDataInRealTime:", ai_set_realtime);
    M(AVAssetWriterInput, "transform", ai_transform); M(AVAssetWriterInput, "setTransform:", ai_set_transform);
    M(AVAssetWriterInput, "mediaTimeScale", ai_mts); M(AVAssetWriterInput, "setMediaTimeScale:", ai_set_mts);
    M(AVAssetWriterInput, "languageCode", ai_lang); M(AVAssetWriterInput, "setLanguageCode:", ai_set_lang);
    M(AVAssetWriterInput, "extendedLanguageTag", ai_xlang); M(AVAssetWriterInput, "setExtendedLanguageTag:", ai_set_xlang);
    M(AVAssetWriterInput, "metadata", ai_metadata); M(AVAssetWriterInput, "setMetadata:", ai_set_metadata);
    M(AVAssetWriterInput, "naturalSize", ai_natural); M(AVAssetWriterInput, "setNaturalSize:", ai_set_natural);
    M(AVAssetWriterInput, "preferredVolume", ai_volume); M(AVAssetWriterInput, "setPreferredVolume:", ai_set_volume);
    M(AVAssetWriterInput, "marksOutputTrackAsEnabled", ai_enabled); M(AVAssetWriterInput, "setMarksOutputTrackAsEnabled:", ai_set_enabled);
    M(AVAssetWriterInput, "performsMultiPassEncodingIfSupported", ai_multipass);
    M(AVAssetWriterInput, "setPerformsMultiPassEncodingIfSupported:", ai_set_multipass);
    M(AVAssetWriterInput, "canPerformMultiplePasses", ai_can_multipass);
    M(AVAssetWriterInput, "appendSampleBuffer:", ai_append);
    M(AVAssetWriterInput, "markAsFinished", ai_mark_finished);
    M(AVAssetWriterInput, "requestMediaDataWhenReadyOnQueue:usingBlock:", ai_request);
    CM(AVAssetWriterInputPixelBufferAdaptor, "assetWriterInputPixelBufferAdaptorWithAssetWriterInput:sourcePixelBufferAttributes:", apa_with);
    M(AVAssetWriterInputPixelBufferAdaptor, "initWithAssetWriterInput:sourcePixelBufferAttributes:", apa_init);
    M(AVAssetWriterInputPixelBufferAdaptor, "dealloc", apa_dealloc);
    M(AVAssetWriterInputPixelBufferAdaptor, "assetWriterInput", apa_input);
    M(AVAssetWriterInputPixelBufferAdaptor, "sourcePixelBufferAttributes", apa_attrs);
    M(AVAssetWriterInputPixelBufferAdaptor, "pixelBufferPool", apa_pool);
    M(AVAssetWriterInputPixelBufferAdaptor, "appendPixelBuffer:withPresentationTime:", apa_append);
#undef M
#undef CM
}
