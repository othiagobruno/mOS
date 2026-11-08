/*
 * mOS libSystem — a parte de áudio do AVFoundation (AVFAudio), sobre o CoreAudio do coreaudio.c:
 *  - AVAudioFormat, AVAudioPCMBuffer, AVAudioTime, AVAudioFile (leitura de WAV/AIFF/CAF, escrita de WAV);
 *  - AVAudioEngine com AVAudioMixerNode (mainMixerNode), AVAudioOutputNode, AVAudioInputNode (silêncio),
 *    AVAudioPlayerNode (buffers e arquivos agendados, laço, conclusão), AVAudioSourceNode (bloco de render), efeitos
 *    (AVAudioUnitEQ/Reverb/Delay/Distortion/TimePitch/Varispeed: passam o som adiante) e taps (installTapOnBus:);
 *  - AVAudioPlayer (arquivo ou NSData; play/pause/stop, volume, pan, rate, laços, currentTime, medidores, delegate);
 *  - AVAudioSession (Catalyst/iOS: categoria, modo, ativação, rota, latência);
 *  - NSSound (AppKit) e AVPlayer/AVPlayerItem/AVURLAsset/AVAsset para arquivos de áudio locais.
 * O motor renderiza a 48 kHz (a taxa do mixer do sistema). Os formatos comprimidos ainda não decodificam: o erro é o
 * da Apple (kAudioFileUnsupportedDataFormatError / AVErrorFileFormatNotRecognized). Sem vídeo.
 */
#include "foundation.h"
#include "sys.h"

typedef int OSStatus;
typedef int bool_t;
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
void objc_msgSendSuper(void);
id objc_getAssociatedObject(id obj, const void *key);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strcasecmp(const char *a, const char *b);
char *strrchr(const char *s, int c);
int pthread_mutex_init(void *m, const void *a);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
int usleep(unsigned us);
unsigned long long mach_absolute_time(void);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void *dispatch_get_main_queue(void);
typedef void (*dispatch_function_t)(void *);
void dispatch_async_f(void *q, void *ctx, dispatch_function_t fn);
double log10(double);
double sqrt(double);
double fabs(double);

/* coreaudio.c */
void *cx_cae_new(double rate, u32 chunk, void (*fn)(void *e, float *stereo, u32 frames, const void *ts), void *ctx);
void *cx_cae_ctx(void *e);
int cx_cae_start(void *e);
void cx_cae_stop(void *e, int flush);
void cx_cae_free(void *e);
void cx_cae_pause(void *e, int p);
double cx_cae_played(void *e);
void cx_cae_set_gain(void *e, float g);
int cx_cae_running(void *e);
void cx_cae_set_rate(void *e, double src_rate);
u32 cx_cae_queued(void *e);
void *cx_audio_open_path(const char *path, int *err);
void *cx_audio_open_data(const void *b, u64 n, int *err);
void cx_audio_close(void *d);
double cx_audio_rate(void *d);
u32 cx_audio_channels(void *d);
u64 cx_audio_frames(void *d);
const void *cx_audio_asbd(void *d);
u32 cx_audio_type(void *d);
u32 cx_audio_read_stereo(void *d, u64 from, u32 n, float *out);
float cx_audio_sample(void *d, u64 frame, u32 ch);
const char *cx_audio_url_path(id url);
void cx_buf_to_stereo(const void *d, const void *l, u32 from, u32 n, float *out);
void cx_stereo_to_buf(const void *d, void *l, u32 from, u32 n, const float *in);
float cx_buf_sample(const void *d, const void *l, u32 f, u32 c);
void cx_buf_set_sample(const void *d, void *l, u32 f, u32 c, float v);
OSStatus ExtAudioFileCreateWithURL(id url, u32 type, const void *f, const void *layout, u32 flags, void **out);
OSStatus ExtAudioFileWrite(void *e, u32 nframes, const void *in);
OSStatus ExtAudioFileDispose(void *e);
OSStatus ExtAudioFileSetProperty(void *e, u32 id_, u32 sz, const void *d);

#define FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
#define RATE 48000.0
#define BLK_FN(b) (((void **)(b))[2])

typedef struct {
    double mSampleRate;
    u32 mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved;
} asbd_t;
typedef struct { u32 mNumberChannels, mDataByteSize; void *mData; } abuf_t;
typedef struct { u32 mNumberBuffers; u32 pad; abuf_t mBuffers[1]; } ablist_t;
typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cmtime_t;
typedef struct { double mSampleTime; u64 mHostTime; double mRateScalar; u64 mWordClockTime; u8 smpte[24]; u32 mFlags, mReserved; } ats_t;

static id cls(const char *n) { return (id)cx_class_named(n); }
static BOOL resp(id o, const char *s) { return o && cx_responds(object_getClass(o), sel_registerName(s)); }
static void setobj(id *slot, id v) { if (*slot == v) return; if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static id mdict(void) { return MSG0(id, cls("NSMutableDictionary"), "dictionary"); }
static void dset(id d, const char *k, id v) { if (d && v) MSG(void, d, "setObject:forKey:", v, cx_str(k)); }
static id dget(id d, const char *k) { return d && resp(d, "objectForKey:") ? MSG(id, d, "objectForKey:", cx_str(k)) : 0; }
static double dnum(id d, const char *k, double dflt) { id n = dget(d, k); return n && resp(n, "doubleValue") ? MSG0(double, n, "doubleValue") : dflt; }
static id error_with(const char *domain, i64 code, const char *msg) {
    id info = mdict();
    dset(info, "NSLocalizedDescription", cx_str(msg));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str(domain), code, info);
}
static void set_err(id *e, i64 code, const char *msg) { if (e) *e = error_with("NSOSStatusErrorDomain", code, msg); }
static double host_seconds(void) { return (double)mach_absolute_time() / 1e9; }

/* chamada de um bloco do app (o libSystem compila sem -fblocks) */
#define CALL_BLOCK0(b) ((void (*)(void *))BLK_FN(b))(b)

/* ---- entrega na fila principal */
typedef struct { void (*fn)(void *); void *a; } mainjob_t;
static void main_run(void *c) { mainjob_t *j = c; j->fn(j->a); free(j); }
static void on_main(void (*fn)(void *), void *a) {
    mainjob_t *j = malloc(sizeof *j);
    if (!j) return;
    j->fn = fn;
    j->a = a;
    dispatch_async_f(dispatch_get_main_queue(), j, main_run);
}

/* ================================================================ AVAudioFormat */
enum { CF_OTHER = 0, CF_F32 = 1, CF_F64 = 2, CF_I16 = 3, CF_I32 = 4 };
typedef struct { cx_class_t *isa; asbd_t d; u32 common; BOOL inter; id settings; } avfmt_t;
cx_class_t AVAudioFormat_c __asm__("OBJC_CLASS_$_AVAudioFormat"), AVAudioFormat_m __asm__("OBJC_METACLASS_$_AVAudioFormat");
#define FMT(o) ((avfmt_t *)(o))

static void fmt_fill(avfmt_t *f, u32 common, double rate, u32 ch, BOOL inter) {
    u32 bits = common == CF_F64 ? 64 : common == CF_I16 ? 16 : 32;
    f->common = common;
    f->inter = inter;
    memset(&f->d, 0, sizeof f->d);
    f->d.mSampleRate = rate;
    f->d.mFormatID = FCC('l', 'p', 'c', 'm');
    f->d.mFormatFlags = (common == CF_F32 || common == CF_F64 ? 1 : 4) | 8 | (inter ? 0 : 32);
    f->d.mBitsPerChannel = bits;
    f->d.mChannelsPerFrame = ch;
    f->d.mFramesPerPacket = 1;
    f->d.mBytesPerFrame = f->d.mBytesPerPacket = bits / 8 * (inter ? ch : 1);
}
static u32 common_of(const asbd_t *d) {
    if (d->mFormatID != FCC('l', 'p', 'c', 'm') || (d->mFormatFlags & 2)) return CF_OTHER;
    if (d->mFormatFlags & 1) return d->mBitsPerChannel == 32 ? CF_F32 : d->mBitsPerChannel == 64 ? CF_F64 : CF_OTHER;
    return d->mBitsPerChannel == 16 ? CF_I16 : d->mBitsPerChannel == 32 ? CF_I32 : CF_OTHER;
}
static id fmt_new(u32 common, double rate, u32 ch, BOOL inter) {
    avfmt_t *f = (avfmt_t *)cx_alloc_instance(&AVAudioFormat_c);
    fmt_fill(f, common, rate, ch, inter);
    return (id)f;
}
static id fmt_init_std(id s, SEL c, double rate, u32 ch) { (void)c; if (!ch || ch > 32 || rate <= 0) return 0; fmt_fill(FMT(s), CF_F32, rate, ch, NO); return s; }
static id fmt_init_std_layout(id s, SEL c, double rate, id layout) { (void)c; (void)layout; fmt_fill(FMT(s), CF_F32, rate, 2, NO); return s; }
static id fmt_init_common(id s, SEL c, u64 common, double rate, u32 ch, BOOL inter) {
    (void)c;
    if (!ch || ch > 32 || rate <= 0 || common < 1 || common > 4) return 0;
    fmt_fill(FMT(s), (u32)common, rate, ch, inter);
    return s;
}
static id fmt_init_asbd(id s, SEL c, const asbd_t *d) {
    (void)c;
    if (!d || !d->mChannelsPerFrame) return 0;
    FMT(s)->d = *d;
    FMT(s)->common = common_of(d);
    FMT(s)->inter = !(d->mFormatFlags & 32);
    return s;
}
static id fmt_init_asbd_layout(id s, SEL c, const asbd_t *d, id layout) { (void)layout; return fmt_init_asbd(s, c, d); }
static id fmt_init_settings(id s, SEL c, id st) {
    (void)c;
    double rate = dnum(st, "AVSampleRateKey", 44100);
    u32 ch = (u32)dnum(st, "AVNumberOfChannelsKey", 2);
    u32 bits = (u32)dnum(st, "AVLinearPCMBitDepthKey", 32);
    BOOL isfloat = dnum(st, "AVLinearPCMIsFloatKey", 1) != 0;
    BOOL nonint = dnum(st, "AVLinearPCMIsNonInterleaved", 0) != 0;
    u32 id_ = (u32)dnum(st, "AVFormatIDKey", FCC('l', 'p', 'c', 'm'));
    if (id_ != FCC('l', 'p', 'c', 'm') || !ch) return 0;
    u32 common = isfloat ? (bits == 64 ? CF_F64 : CF_F32) : (bits == 16 ? CF_I16 : CF_I32);
    fmt_fill(FMT(s), common, rate, ch, !nonint);
    setobj(&FMT(s)->settings, st);
    return s;
}
static double fmt_rate(id s, SEL c) { (void)c; return FMT(s)->d.mSampleRate; }
static u32 fmt_channels(id s, SEL c) { (void)c; return FMT(s)->d.mChannelsPerFrame; }
static u64 fmt_common(id s, SEL c) { (void)c; return FMT(s)->common; }
static BOOL fmt_inter(id s, SEL c) { (void)c; return FMT(s)->inter; }
static BOOL fmt_standard(id s, SEL c) { (void)c; return FMT(s)->common == CF_F32 && !FMT(s)->inter; }
static const asbd_t *fmt_asbd(id s, SEL c) { (void)c; return &FMT(s)->d; }
static id fmt_settings(id s, SEL c) {
    (void)c;
    id d = mdict();
    dset(d, "AVFormatIDKey", cx_num_i(FMT(s)->d.mFormatID));
    dset(d, "AVSampleRateKey", cx_num_d(FMT(s)->d.mSampleRate));
    dset(d, "AVNumberOfChannelsKey", cx_num_i(FMT(s)->d.mChannelsPerFrame));
    dset(d, "AVLinearPCMBitDepthKey", cx_num_i(FMT(s)->d.mBitsPerChannel));
    dset(d, "AVLinearPCMIsFloatKey", cx_num_i((FMT(s)->d.mFormatFlags & 1) != 0));
    dset(d, "AVLinearPCMIsBigEndianKey", cx_num_i(0));
    dset(d, "AVLinearPCMIsNonInterleaved", cx_num_i(!FMT(s)->inter));
    return d;
}
static BOOL fmt_equal(id s, SEL c, id o) {
    (void)c;
    if (!o || object_getClass(o) != object_getClass(s)) return NO;
    const asbd_t *a = &FMT(s)->d, *b = &FMT(o)->d;
    return a->mSampleRate == b->mSampleRate && a->mFormatFlags == b->mFormatFlags && a->mChannelsPerFrame == b->mChannelsPerFrame &&
           a->mBitsPerChannel == b->mBitsPerChannel && a->mFormatID == b->mFormatID;
}
static id fmt_layout(id s, SEL c) { (void)s; (void)c; return 0; }
static id fmt_copy(id s, SEL c, void *z) { (void)c; (void)z; return objc_retain(s); }

/* ================================================================ AVAudioPCMBuffer */
typedef struct {
    cx_class_t *isa;
    id fmt;
    u32 cap, len;
    u8 *data;                    /* planos contíguos */
    void **planes;               /* floatChannelData / int16ChannelData / int32ChannelData */
    ablist_t *abl;
} avbuf_t;
cx_class_t AVAudioBuffer_c __asm__("OBJC_CLASS_$_AVAudioBuffer"), AVAudioBuffer_m __asm__("OBJC_METACLASS_$_AVAudioBuffer");
cx_class_t AVAudioPCMBuffer_c __asm__("OBJC_CLASS_$_AVAudioPCMBuffer"), AVAudioPCMBuffer_m __asm__("OBJC_METACLASS_$_AVAudioPCMBuffer");
#define BUF(o) ((avbuf_t *)(o))

static void buf_update_abl(avbuf_t *b) {
    asbd_t *d = &FMT(b->fmt)->d;
    for (u32 i = 0; i < b->abl->mNumberBuffers; i++) b->abl->mBuffers[i].mDataByteSize = b->len * d->mBytesPerFrame;
}
static id buf_init(id s, SEL c, id fmt, u32 cap) {
    (void)c;
    if (!fmt || !cap) return 0;
    avbuf_t *b = BUF(s);
    setobj(&b->fmt, fmt);
    asbd_t *d = &FMT(fmt)->d;
    u32 ch = d->mChannelsPerFrame;
    BOOL ni = (d->mFormatFlags & 32) != 0;
    u32 nb = ni ? ch : 1;
    u64 plane = (u64)cap * d->mBytesPerFrame;
    b->data = calloc(1, (size_t)(plane * nb + 16));
    b->planes = calloc(ch ? ch : 1, sizeof(void *));
    b->abl = calloc(1, 8 + 16 * (size_t)nb);
    if (!b->data || !b->planes || !b->abl) return 0;
    b->abl->mNumberBuffers = nb;
    for (u32 i = 0; i < nb; i++) {
        b->abl->mBuffers[i].mNumberChannels = ni ? 1 : ch;
        b->abl->mBuffers[i].mData = b->data + i * plane;
    }
    u32 bps = d->mBitsPerChannel / 8;
    for (u32 i = 0; i < ch; i++) b->planes[i] = ni ? b->data + i * plane : b->data + (u64)i * bps;
    b->cap = cap;
    b->len = 0;
    buf_update_abl(b);
    return s;
}
static id buf_new(id fmt, u32 cap) { return buf_init(cx_alloc_instance(&AVAudioPCMBuffer_c), 0, fmt, cap); }
static u32 buf_cap(id s, SEL c) { (void)c; return BUF(s)->cap; }
static u32 buf_len(id s, SEL c) { (void)c; return BUF(s)->len; }
static void buf_set_len(id s, SEL c, u32 n) { (void)c; avbuf_t *b = BUF(s); b->len = n > b->cap ? b->cap : n; buf_update_abl(b); }
static void *buf_float(id s, SEL c) { (void)c; return FMT(BUF(s)->fmt)->common == CF_F32 ? BUF(s)->planes : 0; }
static void *buf_i16(id s, SEL c) { (void)c; return FMT(BUF(s)->fmt)->common == CF_I16 ? BUF(s)->planes : 0; }
static void *buf_i32(id s, SEL c) { (void)c; return FMT(BUF(s)->fmt)->common == CF_I32 ? BUF(s)->planes : 0; }
static u64 buf_stride(id s, SEL c) { (void)c; return FMT(BUF(s)->fmt)->inter ? FMT(BUF(s)->fmt)->d.mChannelsPerFrame : 1; }
static id buf_format(id s, SEL c) { (void)c; return BUF(s)->fmt; }
static const ablist_t *buf_abl(id s, SEL c) { (void)c; buf_update_abl(BUF(s)); return BUF(s)->abl; }
static void buf_dealloc(id s, SEL c) {
    (void)c;
    avbuf_t *b = BUF(s);
    free(b->data); free(b->planes); free(b->abl);
    if (b->fmt) objc_release(b->fmt);
    b->data = 0; b->planes = 0; b->abl = 0; b->fmt = 0;
    struct { id r; cx_class_t *s; } sup = {s, cx_class_named("NSObject")};
    ((void (*)(void *, SEL))objc_msgSendSuper)(&sup, sel_registerName("dealloc"));
}
/* os quadros [from, from+n) em estéreo float */
static void buf_read_stereo(avbuf_t *b, u32 from, u32 n, float *out) { cx_buf_to_stereo(&FMT(b->fmt)->d, b->abl, from, n, out); }

/* ================================================================ AVAudioTime */
typedef struct { cx_class_t *isa; double st, rate; u64 host; BOOL svalid, hvalid; } avtime_t;
cx_class_t AVAudioTime_c __asm__("OBJC_CLASS_$_AVAudioTime"), AVAudioTime_m __asm__("OBJC_METACLASS_$_AVAudioTime");
#define TIM(o) ((avtime_t *)(o))
static id time_new(double st, double rate, u64 host) {
    avtime_t *t = (avtime_t *)cx_alloc_instance(&AVAudioTime_c);
    t->st = st; t->rate = rate; t->host = host; t->svalid = YES; t->hvalid = host != 0;
    return (id)t;
}
static id time_init_st(id s, SEL c, i64 st, double rate) { (void)c; TIM(s)->st = (double)st; TIM(s)->rate = rate; TIM(s)->svalid = YES; return s; }
static id time_init_host(id s, SEL c, u64 h) { (void)c; TIM(s)->host = h; TIM(s)->hvalid = YES; return s; }
static id time_init_host_st(id s, SEL c, u64 h, i64 st, double rate) { (void)c; TIM(s)->host = h; TIM(s)->hvalid = YES; TIM(s)->st = (double)st; TIM(s)->rate = rate; TIM(s)->svalid = YES; return s; }
static id time_with_st(id k, SEL c, i64 st, double rate) { (void)k; return objc_autorelease(time_init_st(cx_alloc_instance(&AVAudioTime_c), c, st, rate)); }
static id time_with_host(id k, SEL c, u64 h) { (void)k; return objc_autorelease(time_init_host(cx_alloc_instance(&AVAudioTime_c), c, h)); }
static i64 time_st(id s, SEL c) { (void)c; return (i64)TIM(s)->st; }
static double time_rate(id s, SEL c) { (void)c; return TIM(s)->rate; }
static u64 time_host(id s, SEL c) { (void)c; return TIM(s)->host; }
static BOOL time_svalid(id s, SEL c) { (void)c; return TIM(s)->svalid; }
static BOOL time_hvalid(id s, SEL c) { (void)c; return TIM(s)->hvalid; }
static u64 time_host_for_seconds(id k, SEL c, double sec) { (void)k; (void)c; return (u64)(sec * 1e9); }
static double time_seconds_for_host(id k, SEL c, u64 h) { (void)k; (void)c; return (double)h / 1e9; }
static ats_t time_ats(id s, SEL c) {
    (void)c;
    ats_t t;
    memset(&t, 0, sizeof t);
    t.mSampleTime = TIM(s)->st;
    t.mHostTime = TIM(s)->host;
    t.mRateScalar = 1;
    t.mFlags = (TIM(s)->svalid ? 1 : 0) | (TIM(s)->hvalid ? 2 : 0);
    return t;
}

/* ================================================================ AVAudioFile */
typedef struct {
    cx_class_t *isa;
    void *dec;                   /* leitura (coreaudio.c) */
    void *ext;                   /* escrita (ExtAudioFile) */
    id url, file_fmt, proc_fmt;
    i64 pos;
    i64 written;
} avfile_t;
cx_class_t AVAudioFile_c __asm__("OBJC_CLASS_$_AVAudioFile"), AVAudioFile_m __asm__("OBJC_METACLASS_$_AVAudioFile");
#define AVF(o) ((avfile_t *)(o))

static id file_init_read_common(id s, SEL c, id url, u64 common, BOOL inter, id *err) {
    (void)c;
    int e = 0;
    void *d = cx_audio_open_path(cx_audio_url_path(url), &e);
    if (!d) { set_err(err, e, e == -43 ? "O arquivo não foi encontrado." : "O formato do arquivo de áudio não é suportado."); objc_release(s); return 0; }
    avfile_t *f = AVF(s);
    f->dec = d;
    setobj(&f->url, url);
    f->file_fmt = objc_retain(fmt_init_asbd(cx_alloc_instance(&AVAudioFormat_c), 0, cx_audio_asbd(d)));
    f->proc_fmt = objc_retain(fmt_new((u32)common, cx_audio_rate(d), cx_audio_channels(d), inter));
    return s;
}
static id file_init_read(id s, SEL c, id url, id *err) { return file_init_read_common(s, c, url, CF_F32, NO, err); }
static id file_init_write_common(id s, SEL c, id url, id settings, u64 common, BOOL inter, id *err) {
    (void)c;
    id ff = fmt_init_settings(cx_alloc_instance(&AVAudioFormat_c), 0, settings);
    if (!ff) { set_err(err, FCC('f', 'm', 't', '?'), "Só LPCM pode ser gravado (WAV)."); objc_release(s); return 0; }
    /* a gravação é em WAV little endian intercalado */
    fmt_fill(FMT(ff), FMT(ff)->common ? FMT(ff)->common : CF_I16, FMT(ff)->d.mSampleRate, FMT(ff)->d.mChannelsPerFrame, YES);
    void *ext = 0;
    OSStatus r = ExtAudioFileCreateWithURL(url, FCC('W', 'A', 'V', 'E'), &FMT(ff)->d, 0, 1, &ext);
    if (r) { objc_release(ff); set_err(err, r, "Não foi possível criar o arquivo de áudio."); objc_release(s); return 0; }
    avfile_t *f = AVF(s);
    f->ext = ext;
    setobj(&f->url, url);
    f->file_fmt = ff;
    f->proc_fmt = objc_retain(fmt_new((u32)common, FMT(ff)->d.mSampleRate, FMT(ff)->d.mChannelsPerFrame, inter));
    ExtAudioFileSetProperty(ext, FCC('c', 'f', 'm', 't'), 40, &FMT(f->proc_fmt)->d);
    return s;
}
static id file_init_write(id s, SEL c, id url, id settings, id *err) { return file_init_write_common(s, c, url, settings, CF_F32, NO, err); }
static i64 file_length(id s, SEL c) { (void)c; return AVF(s)->dec ? (i64)cx_audio_frames(AVF(s)->dec) : AVF(s)->written; }
static i64 file_pos(id s, SEL c) { (void)c; return AVF(s)->pos; }
static void file_set_pos(id s, SEL c, i64 p) { (void)c; AVF(s)->pos = p < 0 ? 0 : p; }
static id file_ffmt(id s, SEL c) { (void)c; return AVF(s)->file_fmt; }
static id file_pfmt(id s, SEL c) { (void)c; return AVF(s)->proc_fmt; }
static id file_url(id s, SEL c) { (void)c; return AVF(s)->url; }
static BOOL file_is_open(id s, SEL c) { (void)c; return AVF(s)->dec || AVF(s)->ext; }
static BOOL file_read_n(id s, SEL c, id buf, u32 n, id *err) {
    (void)c;
    avfile_t *f = AVF(s);
    if (!f->dec || !buf) { set_err(err, -50, "Arquivo não aberto para leitura."); return NO; }
    avbuf_t *b = BUF(buf);
    u64 total = cx_audio_frames(f->dec);
    if ((u64)f->pos >= total) { BUF(buf)->len = 0; buf_update_abl(b); return YES; }
    if (n > b->cap) n = b->cap;
    if ((u64)f->pos + n > total) n = (u32)(total - (u64)f->pos);
    u32 fch = cx_audio_channels(f->dec), bch = FMT(b->fmt)->d.mChannelsPerFrame;
    b->len = n;   /* o tamanho dos buffers do AudioBufferList limita a escrita das amostras */
    buf_update_abl(b);
    /* o processamento é na taxa do arquivo: cópia canal a canal, convertida para o formato do buffer */
    for (u32 i = 0; i < n; i++)
        for (u32 ch = 0; ch < bch; ch++)
            cx_buf_set_sample(&FMT(b->fmt)->d, b->abl, i, ch, cx_audio_sample(f->dec, (u64)f->pos + i, ch < fch ? ch : fch - 1));
    f->pos += n;
    b->len = n;
    buf_update_abl(b);
    return YES;
}
static BOOL file_read(id s, SEL c, id buf, id *err) { return file_read_n(s, c, buf, buf ? BUF(buf)->cap : 0, err); }
static BOOL file_write(id s, SEL c, id buf, id *err) {
    (void)c;
    avfile_t *f = AVF(s);
    if (!f->ext || !buf) { set_err(err, -50, "Arquivo não aberto para gravação."); return NO; }
    buf_update_abl(BUF(buf));
    /* o buffer pode estar num formato diferente do de processamento: passa pelo estéreo float quando preciso */
    OSStatus r;
    if (fmt_equal(BUF(buf)->fmt, 0, f->proc_fmt)) r = ExtAudioFileWrite(f->ext, BUF(buf)->len, BUF(buf)->abl);
    else {
        id tmp = buf_new(f->proc_fmt, BUF(buf)->len ? BUF(buf)->len : 1);
        float *st = malloc((size_t)BUF(buf)->len * 8 + 8);
        if (!tmp || !st) { free(st); return NO; }
        buf_read_stereo(BUF(buf), 0, BUF(buf)->len, st);
        BUF(tmp)->len = BUF(buf)->len;
        buf_update_abl(BUF(tmp));
        cx_stereo_to_buf(&FMT(f->proc_fmt)->d, BUF(tmp)->abl, 0, BUF(buf)->len, st);
        BUF(tmp)->len = BUF(buf)->len;
        buf_update_abl(BUF(tmp));
        r = ExtAudioFileWrite(f->ext, BUF(tmp)->len, BUF(tmp)->abl);
        free(st);
        objc_release(tmp);
    }
    if (r) { set_err(err, r, "Falha ao gravar o áudio."); return NO; }
    f->written += BUF(buf)->len;
    f->pos = f->written;
    return YES;
}
static void file_close(id s, SEL c) {
    (void)c;
    avfile_t *f = AVF(s);
    if (f->dec) { cx_audio_close(f->dec); f->dec = 0; }
    if (f->ext) { ExtAudioFileDispose(f->ext); f->ext = 0; }
}
static void file_dealloc(id s, SEL c) {
    file_close(s, c);
    avfile_t *f = AVF(s);
    if (f->url) objc_release(f->url);
    if (f->file_fmt) objc_release(f->file_fmt);
    if (f->proc_fmt) objc_release(f->proc_fmt);
    f->url = f->file_fmt = f->proc_fmt = 0;
    struct { id r; cx_class_t *s; } sup = {s, cx_class_named("NSObject")};
    ((void (*)(void *, SEL))objc_msgSendSuper)(&sup, sel_registerName("dealloc"));
}

/* ================================================================ nós e o AVAudioEngine */
enum { N_MIXER = 1, N_OUTPUT, N_INPUT, N_PLAYER, N_SOURCE, N_EFFECT, N_SINK };
#define MAXIN 32
#define TAPCAP 16384

/* um agendamento do AVAudioPlayerNode */
typedef struct sched {
    struct sched *next;
    id buffer;                   /* AVAudioPCMBuffer (retido) */
    void *dec;                   /* arquivo (aberto só para este agendamento) */
    u64 start, count, pos;       /* quadros na taxa da origem */
    double rate;
    u32 options;                 /* AVAudioPlayerNodeBufferLoops = 1, Interrupts = 2, InterruptsAtLoop = 4 */
    void *done;                  /* bloco de conclusão (copiado) */
    int done_kind;               /* 0 sem argumento, 1 com AVAudioPlayerNodeCompletionCallbackType */
    double rsfrac;               /* reamostragem para 48 kHz */
    float last[2];
} sched_t;

typedef struct avnode {
    cx_class_t *isa;
    int kind;
    id engine;                   /* fraco */
    struct avnode *in[MAXIN];
    int nin;
    id fmt;                      /* formato de saída */
    float volume, pan, outvol;
    int bypass;
    /* tap */
    void *tap;                   /* bloco (AVAudioPCMBuffer *, AVAudioTime *) */
    u32 tap_size;
    id tap_buf;
    /* player */
    sched_t *sq;
    int playing;
    double node_time;            /* quadros a 48 kHz tocados desde play */
    /* source */
    void *render_block;
    id src_fmt;
    double src_frac;
    float src_last[2];
    /* render em andamento: a saída deste nó no quadro atual (evita refazer quando ligado a dois destinos) */
    float *cur;
    u32 cur_cap;
    u64 cur_stamp;
    /* parâmetros dos efeitos (EQ, reverb...) guardados para o app ler de volta */
    id params;
} avnode_t;
#define NOD(o) ((avnode_t *)(o))

typedef struct {
    cx_class_t *isa;
    id nodes;                    /* NSMutableArray dos nós ligados */
    avnode_t *mixer, *output, *input;
    void *eng;
    int running, prepared, manual;
    u8 lock[64];
    u64 stamp;
    id cfg_obs;
} avengine_t;
#define ENG(o) ((avengine_t *)(o))

cx_class_t AVAudioEngine_c __asm__("OBJC_CLASS_$_AVAudioEngine"), AVAudioEngine_m __asm__("OBJC_METACLASS_$_AVAudioEngine");
cx_class_t AVAudioNode_c __asm__("OBJC_CLASS_$_AVAudioNode"), AVAudioNode_m __asm__("OBJC_METACLASS_$_AVAudioNode");
cx_class_t AVAudioIONode_c __asm__("OBJC_CLASS_$_AVAudioIONode"), AVAudioIONode_m __asm__("OBJC_METACLASS_$_AVAudioIONode");
cx_class_t AVAudioMixerNode_c __asm__("OBJC_CLASS_$_AVAudioMixerNode"), AVAudioMixerNode_m __asm__("OBJC_METACLASS_$_AVAudioMixerNode");
cx_class_t AVAudioOutputNode_c __asm__("OBJC_CLASS_$_AVAudioOutputNode"), AVAudioOutputNode_m __asm__("OBJC_METACLASS_$_AVAudioOutputNode");
cx_class_t AVAudioInputNode_c __asm__("OBJC_CLASS_$_AVAudioInputNode"), AVAudioInputNode_m __asm__("OBJC_METACLASS_$_AVAudioInputNode");
cx_class_t AVAudioPlayerNode_c __asm__("OBJC_CLASS_$_AVAudioPlayerNode"), AVAudioPlayerNode_m __asm__("OBJC_METACLASS_$_AVAudioPlayerNode");
cx_class_t AVAudioSourceNode_c __asm__("OBJC_CLASS_$_AVAudioSourceNode"), AVAudioSourceNode_m __asm__("OBJC_METACLASS_$_AVAudioSourceNode");
cx_class_t AVAudioSinkNode_c __asm__("OBJC_CLASS_$_AVAudioSinkNode"), AVAudioSinkNode_m __asm__("OBJC_METACLASS_$_AVAudioSinkNode");
cx_class_t AVAudioUnit_c __asm__("OBJC_CLASS_$_AVAudioUnit"), AVAudioUnit_m __asm__("OBJC_METACLASS_$_AVAudioUnit");
cx_class_t AVAudioUnitEffect_c __asm__("OBJC_CLASS_$_AVAudioUnitEffect"), AVAudioUnitEffect_m __asm__("OBJC_METACLASS_$_AVAudioUnitEffect");
cx_class_t AVAudioUnitEQ_c __asm__("OBJC_CLASS_$_AVAudioUnitEQ"), AVAudioUnitEQ_m __asm__("OBJC_METACLASS_$_AVAudioUnitEQ");
cx_class_t AVAudioUnitReverb_c __asm__("OBJC_CLASS_$_AVAudioUnitReverb"), AVAudioUnitReverb_m __asm__("OBJC_METACLASS_$_AVAudioUnitReverb");
cx_class_t AVAudioUnitDelay_c __asm__("OBJC_CLASS_$_AVAudioUnitDelay"), AVAudioUnitDelay_m __asm__("OBJC_METACLASS_$_AVAudioUnitDelay");
cx_class_t AVAudioUnitDistortion_c __asm__("OBJC_CLASS_$_AVAudioUnitDistortion"), AVAudioUnitDistortion_m __asm__("OBJC_METACLASS_$_AVAudioUnitDistortion");
cx_class_t AVAudioUnitTimeEffect_c __asm__("OBJC_CLASS_$_AVAudioUnitTimeEffect"), AVAudioUnitTimeEffect_m __asm__("OBJC_METACLASS_$_AVAudioUnitTimeEffect");
cx_class_t AVAudioUnitTimePitch_c __asm__("OBJC_CLASS_$_AVAudioUnitTimePitch"), AVAudioUnitTimePitch_m __asm__("OBJC_METACLASS_$_AVAudioUnitTimePitch");
cx_class_t AVAudioUnitVarispeed_c __asm__("OBJC_CLASS_$_AVAudioUnitVarispeed"), AVAudioUnitVarispeed_m __asm__("OBJC_METACLASS_$_AVAudioUnitVarispeed");

static id std_fmt48(void) { return fmt_new(CF_F32, RATE, 2, NO); }

static void node_setup(avnode_t *n, int kind) {
    n->kind = kind;
    n->volume = 1;
    n->outvol = 1;
    if (!n->fmt) n->fmt = objc_retain(std_fmt48());
}
static id node_init(id s, SEL c) { (void)c; node_setup(NOD(s), N_EFFECT); return s; }
static id player_init(id s, SEL c) { (void)c; node_setup(NOD(s), N_PLAYER); return s; }
static id mixer_init(id s, SEL c) { (void)c; node_setup(NOD(s), N_MIXER); return s; }

/* ---- render do grafo (com a trava do motor) */
static void sched_free(sched_t *q) {
    if (q->buffer) objc_release(q->buffer);
    if (q->dec) cx_audio_close(q->dec);
    if (q->done) _Block_release(q->done);
    free(q);
}
/* um quadro de origem do agendamento, em estéreo */
static void sched_frame(sched_t *q, u64 i, float *lr) {
    if (q->buffer) { buf_read_stereo(BUF(q->buffer), (u32)(q->start + i), 1, lr); return; }
    if (q->dec) { cx_audio_read_stereo(q->dec, q->start + i, 1, lr); return; }
    lr[0] = lr[1] = 0;
}

typedef struct donejob { struct donejob *next; void *blk; int kind; } donejob_t;
static void call_done(void *c) {
    donejob_t *d = c;
    if (d->kind) ((void (*)(void *, i64))BLK_FN(d->blk))(d->blk, 2);   /* AVAudioPlayerNodeCompletionDataPlayedBack */
    else CALL_BLOCK0(d->blk);
    _Block_release(d->blk);
    free(d);
}
static void *done_thread(void *c) {
    donejob_t *d = c;
    while (d) { donejob_t *n = d->next; call_done(d); d = n; }
    return 0;
}

/* gera n quadros a 48 kHz do player; as conclusões vão para 'done' (chamadas fora da trava) */
static void player_render(avnode_t *n, float *out, u32 frames, donejob_t **done) {
    if (!n->playing) return;
    u32 o = 0;
    while (o < frames && n->sq) {
        sched_t *q = n->sq;
        double step = q->rate / RATE;
        while (o < frames) {
            u64 i = (u64)q->rsfrac;
            if (q->pos + i >= q->count) break;
            float a[2], b[2];
            double t = q->rsfrac - (double)i;
            sched_frame(q, q->pos + i, b);
            if (i == 0 && q->pos == 0 && q->rsfrac < 1) { a[0] = q->last[0]; a[1] = q->last[1]; }
            else if (i == 0) { a[0] = q->last[0]; a[1] = q->last[1]; }
            else sched_frame(q, q->pos + i - 1, a);
            out[o * 2] += (a[0] + (b[0] - a[0]) * (float)t) * n->volume * (n->pan > 0 ? 1 - n->pan : 1);
            out[o * 2 + 1] += (a[1] + (b[1] - a[1]) * (float)t) * n->volume * (n->pan < 0 ? 1 + n->pan : 1);
            o++;
            q->rsfrac += step;
        }
        u64 adv = (u64)q->rsfrac;
        if (adv) {
            float lr[2];
            sched_frame(q, q->pos + adv - 1 < q->count ? q->pos + adv - 1 : q->count - 1, lr);
            q->last[0] = lr[0];
            q->last[1] = lr[1];
            q->pos += adv;
            q->rsfrac -= (double)adv;
        }
        if (q->pos >= q->count) {
            if (q->options & 1) { q->pos = 0; continue; }   /* AVAudioPlayerNodeBufferLoops */
            n->sq = q->next;
            if (q->done) {
                donejob_t *d = calloc(1, sizeof *d);
                if (d) { d->blk = q->done; d->kind = q->done_kind; q->done = 0; d->next = *done; *done = d; }
            }
            sched_free(q);
        } else break;
    }
    n->node_time += frames;
}

static void source_render(avnode_t *n, float *out, u32 frames) {
    if (!n->render_block) return;
    id f = n->src_fmt ? n->src_fmt : n->fmt;
    double rate = FMT(f)->d.mSampleRate;
    u32 need = (u32)((double)frames * rate / RATE) + 2;
    id tmp = buf_new(f, need);
    if (!tmp) return;
    BUF(tmp)->len = need;
    buf_update_abl(BUF(tmp));
    BOOL silence = NO;
    ats_t ts;
    memset(&ts, 0, sizeof ts);
    ts.mSampleTime = n->node_time * rate / RATE;
    ts.mHostTime = mach_absolute_time();
    ts.mFlags = 3;
    /* AVAudioSourceNodeRenderBlock: OSStatus (BOOL *isSilence, const AudioTimeStamp *ts, AVAudioFrameCount n, AudioBufferList *out) */
    OSStatus r = ((OSStatus (*)(void *, BOOL *, const ats_t *, u32, ablist_t *))BLK_FN(n->render_block))(n->render_block, &silence, &ts, need, BUF(tmp)->abl);
    if (r == 0 && !silence) {
        float *st = malloc((size_t)need * 8);
        if (st) {
            buf_read_stereo(BUF(tmp), 0, need, st);
            double step = rate / RATE;
            for (u32 o = 0; o < frames; o++) {
                double p = n->src_frac + o * step;
                u32 i = (u32)p;
                if (i + 1 >= need) i = need - 2;
                float t = (float)(p - i);
                out[o * 2] += (st[i * 2] + (st[(i + 1) * 2] - st[i * 2]) * t) * n->volume;
                out[o * 2 + 1] += (st[i * 2 + 1] + (st[(i + 1) * 2 + 1] - st[i * 2 + 1]) * t) * n->volume;
            }
            free(st);
        }
    }
    objc_release(tmp);
    n->node_time += frames;
}

static void tap_feed(avnode_t *n, const float *st, u32 frames, donejob_t **done);
static void node_pull(avengine_t *e, avnode_t *n, float *out, u32 frames, donejob_t **done, int depth) {
    if (depth > 32) return;
    if (n->cur_stamp == e->stamp && n->cur && n->cur_cap >= frames) {   /* já calculado neste quadro */
        for (u32 i = 0; i < frames * 2; i++) out[i] += n->cur[i];
        return;
    }
    if (n->cur_cap < frames) { free(n->cur); n->cur = calloc((size_t)frames * 2, sizeof(float)); n->cur_cap = n->cur ? frames : 0; }
    if (!n->cur) return;
    memset(n->cur, 0, (size_t)frames * 8);
    switch (n->kind) {
    case N_PLAYER: player_render(n, n->cur, frames, done); break;
    case N_SOURCE: source_render(n, n->cur, frames); break;
    case N_INPUT: break;   /* sem microfone: silêncio */
    default:
        for (int i = 0; i < n->nin; i++) if (n->in[i]) node_pull(e, n->in[i], n->cur, frames, done, depth + 1);
        if (n->kind == N_MIXER) {
            float vl = n->outvol * (n->pan > 0 ? 1 - n->pan : 1), vr = n->outvol * (n->pan < 0 ? 1 + n->pan : 1);
            for (u32 i = 0; i < frames; i++) { n->cur[i * 2] *= vl; n->cur[i * 2 + 1] *= vr; }
        }
        break;
    }
    n->cur_stamp = e->stamp;
    if (n->tap) tap_feed(n, n->cur, frames, done);
    for (u32 i = 0; i < frames * 2; i++) out[i] += n->cur[i];
}

/* taps: junta tap_size quadros e chama o bloco numa thread à parte, com uma cópia */
typedef struct { void *blk; id buf; id when; } tapjob_t;
static void *tap_thread(void *c) {
    tapjob_t *t = c;
    ((void (*)(void *, id, id))BLK_FN(t->blk))(t->blk, t->buf, t->when);
    _Block_release(t->blk);
    objc_release(t->buf);
    objc_release(t->when);
    free(t);
    return 0;
}
static void tap_feed(avnode_t *n, const float *st, u32 frames, donejob_t **done) {
    (void)done;
    if (!n->tap_buf) { n->tap_buf = buf_new(std_fmt48(), n->tap_size ? n->tap_size : 4096); if (!n->tap_buf) return; }
    avbuf_t *b = BUF(n->tap_buf);
    for (u32 i = 0; i < frames; i++) {
        ((float *)b->planes[0])[b->len] = st[i * 2];
        ((float *)b->planes[1])[b->len] = st[i * 2 + 1];
        b->len++;
        if (b->len >= b->cap) {
            buf_update_abl(b);
            tapjob_t *t = calloc(1, sizeof *t);
            if (t) {
                t->blk = _Block_copy(n->tap);
                t->buf = n->tap_buf;
                t->when = objc_retain(time_new(n->node_time, RATE, mach_absolute_time()));
                n->tap_buf = 0;
                unsigned long th;
                if (pthread_create(&th, 0, tap_thread, t) == 0) pthread_detach(th);
                else { _Block_release(t->blk); objc_release(t->buf); objc_release(t->when); free(t); }
            }
            n->tap_buf = buf_new(std_fmt48(), n->tap_size ? n->tap_size : 4096);
            if (!n->tap_buf) return;
            b = BUF(n->tap_buf);
        }
    }
}

static void engine_render(void *ce, float *stereo, u32 frames, const void *ts) {
    (void)ts;
    avengine_t *e = cx_cae_ctx(ce);
    donejob_t *done = 0;
    pthread_mutex_lock(e->lock);
    e->stamp++;
    if (e->mixer) node_pull(e, e->output && e->output->nin ? e->output : e->mixer, stereo, frames, &done, 0);
    pthread_mutex_unlock(e->lock);
    if (done) {   /* as conclusões rodam fora da thread de áudio, como no AVAudioEngine */
        unsigned long th;
        if (pthread_create(&th, 0, done_thread, done) == 0) pthread_detach(th);
        else done_thread(done);
    }
}

static avnode_t *new_node(cx_class_t *k, int kind) { avnode_t *n = (avnode_t *)cx_alloc_instance(k); node_setup(n, kind); return n; }
static void eng_ensure(avengine_t *e) {
    if (!e->nodes) e->nodes = objc_retain(MSG0(id, cls("NSMutableArray"), "array"));
    if (!e->lock[0] && !e->lock[1]) pthread_mutex_init(e->lock, 0);
}
static id eng_init(id s, SEL c) {
    (void)c;
    avengine_t *e = ENG(s);
    eng_ensure(e);
    e->output = new_node(&AVAudioOutputNode_c, N_OUTPUT);
    e->output->engine = s;
    e->input = new_node(&AVAudioInputNode_c, N_INPUT);
    e->input->engine = s;
    return s;
}
static id eng_mixer(id s, SEL c) {
    (void)c;
    avengine_t *e = ENG(s);
    if (!e->mixer) {
        e->mixer = new_node(&AVAudioMixerNode_c, N_MIXER);
        e->mixer->engine = s;
        MSG(void, e->nodes, "addObject:", (id)e->mixer);
        pthread_mutex_lock(e->lock);
        e->output->in[0] = e->mixer;
        e->output->nin = 1;
        pthread_mutex_unlock(e->lock);
    }
    return (id)e->mixer;
}
static id eng_output(id s, SEL c) { (void)c; return (id)ENG(s)->output; }
static id eng_input(id s, SEL c) { (void)c; return (id)ENG(s)->input; }
static void eng_attach(id s, SEL c, id node) {
    (void)c;
    if (!node) return;
    avengine_t *e = ENG(s);
    eng_ensure(e);
    NOD(node)->engine = s;
    if (!MSG(BOOL, e->nodes, "containsObject:", node)) MSG(void, e->nodes, "addObject:", node);
}
static void detach_inputs_from(avengine_t *e, avnode_t *n) {
    id arr = e->nodes;
    u64 cnt = MSG0(u64, arr, "count");
    for (u64 i = 0; i < cnt; i++) {
        avnode_t *m = NOD(MSG(id, arr, "objectAtIndex:", i));
        for (int k = 0; k < m->nin; k++) if (m->in[k] == n) { m->in[k] = m->in[--m->nin]; k--; }
    }
    for (int k = 0; k < e->output->nin; k++) if (e->output->in[k] == n) { e->output->in[k] = e->output->in[--e->output->nin]; k--; }
}
static void eng_detach(id s, SEL c, id node) {
    (void)c;
    avengine_t *e = ENG(s);
    if (!node) return;
    pthread_mutex_lock(e->lock);
    detach_inputs_from(e, NOD(node));
    NOD(node)->nin = 0;
    pthread_mutex_unlock(e->lock);
    NOD(node)->engine = 0;
    MSG(void, e->nodes, "removeObject:", node);
}
static void eng_connect_bus(id s, SEL c, id from, id to, u64 fb, u64 tb, id fmt) {
    (void)c; (void)fb; (void)tb;
    avengine_t *e = ENG(s);
    if (!from || !to) return;
    eng_attach(s, 0, from);
    if (NOD(to)->kind != N_OUTPUT) eng_attach(s, 0, to);
    pthread_mutex_lock(e->lock);
    avnode_t *dst = NOD(to);
    /* cada barramento de entrada de um nó que não é mixer aceita uma fonte só */
    if (dst->kind != N_MIXER) dst->nin = 0;
    int dup = 0;
    for (int i = 0; i < dst->nin; i++) if (dst->in[i] == NOD(from)) dup = 1;
    if (!dup && dst->nin < MAXIN) dst->in[dst->nin++] = NOD(from);
    if (fmt && NOD(from)->kind == N_SOURCE && !NOD(from)->src_fmt) setobj(&NOD(from)->src_fmt, fmt);
    pthread_mutex_unlock(e->lock);
}
static void eng_connect(id s, SEL c, id from, id to, id fmt) { eng_connect_bus(s, c, from, to, 0, 0, fmt); }
static void eng_connect_points(id s, SEL c, id from, id points, u64 fb, id fmt) {
    (void)c; (void)fb;
    u64 n = points ? MSG0(u64, points, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id pt = MSG(id, points, "objectAtIndex:", i);
        id node = resp(pt, "node") ? MSG0(id, pt, "node") : 0;
        if (node) eng_connect_bus(s, 0, from, node, 0, 0, fmt);
    }
}
static void eng_disconnect_input(id s, SEL c, id node) { (void)c; avengine_t *e = ENG(s); pthread_mutex_lock(e->lock); if (node) NOD(node)->nin = 0; pthread_mutex_unlock(e->lock); }
static void eng_disconnect_input_bus(id s, SEL c, id node, u64 bus) { (void)bus; eng_disconnect_input(s, c, node); }
static void eng_disconnect_output(id s, SEL c, id node) { (void)c; avengine_t *e = ENG(s); if (!node) return; pthread_mutex_lock(e->lock); detach_inputs_from(e, NOD(node)); pthread_mutex_unlock(e->lock); }
static void eng_disconnect_output_bus(id s, SEL c, id node, u64 bus) { (void)bus; eng_disconnect_output(s, c, node); }
static void eng_prepare(id s, SEL c) { (void)c; eng_mixer(s, 0); ENG(s)->prepared = 1; }
static BOOL eng_start(id s, SEL c, id *err) {
    (void)c;
    avengine_t *e = ENG(s);
    eng_mixer(s, 0);
    if (!e->eng) e->eng = cx_cae_new(RATE, 512, engine_render, e);
    if (!e->eng || cx_cae_start(e->eng) < 0) { set_err(err, FCC('n', 'o', 'p', 'e'), "O motor de áudio não pôde iniciar."); return NO; }
    e->running = 1;
    return YES;
}
static void eng_pause(id s, SEL c) { (void)c; if (ENG(s)->eng) cx_cae_pause(ENG(s)->eng, 1); ENG(s)->running = 0; }
static void eng_stop(id s, SEL c) {
    (void)c;
    avengine_t *e = ENG(s);
    if (e->eng) cx_cae_stop(e->eng, 1);
    e->running = 0;
}
static void eng_reset(id s, SEL c) { (void)s; (void)c; }
static BOOL eng_running(id s, SEL c) { (void)c; return ENG(s)->running; }
static id eng_attached(id s, SEL c) { (void)c; return MSG(id, cls("NSSet"), "setWithArray:", ENG(s)->nodes); }
static BOOL eng_manual(id s, SEL c) { (void)c; return ENG(s)->manual; }
static BOOL eng_enable_manual(id s, SEL c, i64 mode, id fmt, u32 max, id *err) { (void)c; (void)mode; (void)fmt; (void)max; (void)err; ENG(s)->manual = 1; return YES; }
static void eng_disable_manual(id s, SEL c) { (void)c; ENG(s)->manual = 0; }
static id eng_input_point(id s, SEL c, id node, u64 bus) { (void)s; (void)c; (void)node; (void)bus; return 0; }
static void eng_dealloc(id s, SEL c) {
    (void)c;
    avengine_t *e = ENG(s);
    if (e->eng) { cx_cae_free(e->eng); e->eng = 0; }
    if (e->nodes) objc_release(e->nodes);
    e->nodes = 0;
    struct { id r; cx_class_t *s; } sup = {s, cx_class_named("NSObject")};
    ((void (*)(void *, SEL))objc_msgSendSuper)(&sup, sel_registerName("dealloc"));
}

/* ---- AVAudioNode */
static id node_engine(id s, SEL c) { (void)c; return NOD(s)->engine; }
static u64 node_ninputs(id s, SEL c) { (void)c; int k = NOD(s)->kind; return k == N_PLAYER || k == N_SOURCE || k == N_INPUT ? 0 : k == N_MIXER ? 1024 : 1; }
static u64 node_noutputs(id s, SEL c) { (void)c; return NOD(s)->kind == N_OUTPUT ? 0 : 1; }
static id node_out_fmt(id s, SEL c, u64 bus) { (void)c; (void)bus; return NOD(s)->src_fmt && NOD(s)->kind == N_SOURCE ? NOD(s)->src_fmt : NOD(s)->fmt; }
static id node_in_fmt(id s, SEL c, u64 bus) { (void)c; (void)bus; return NOD(s)->fmt; }
static id node_name_for(id s, SEL c, u64 bus) { (void)s; (void)c; (void)bus; return 0; }
static id node_last_time(id s, SEL c) { (void)c; return time_new(NOD(s)->node_time, RATE, mach_absolute_time()); }
static void node_install_tap(id s, SEL c, u64 bus, u32 size, id fmt, void *blk) {
    (void)c; (void)bus; (void)fmt;
    avnode_t *n = NOD(s);
    avengine_t *e = n->engine ? ENG(n->engine) : 0;
    if (e) pthread_mutex_lock(e->lock);
    if (n->tap) _Block_release(n->tap);
    n->tap = blk ? _Block_copy(blk) : 0;
    n->tap_size = size ? size : 4096;
    if (n->tap_buf) { objc_release(n->tap_buf); n->tap_buf = 0; }
    if (e) pthread_mutex_unlock(e->lock);
}
static void node_remove_tap(id s, SEL c, u64 bus) { node_install_tap(s, c, bus, 0, 0, 0); }
static void node_reset(id s, SEL c) { (void)s; (void)c; }
static double node_latency(id s, SEL c) { (void)s; (void)c; return 2048.0 / RATE; }
static double node_out_latency(id s, SEL c) { (void)s; (void)c; return 2048.0 / RATE; }
/* AVAudioMixing: volume e pan em qualquer nó fonte; outputVolume no mixer */
static float node_volume(id s, SEL c) { (void)c; return NOD(s)->volume; }
static void node_set_volume(id s, SEL c, float v) { (void)c; NOD(s)->volume = v < 0 ? 0 : v; }
static float node_pan(id s, SEL c) { (void)c; return NOD(s)->pan; }
static void node_set_pan(id s, SEL c, float v) { (void)c; NOD(s)->pan = v < -1 ? -1 : v > 1 ? 1 : v; }
static float mixer_outvol(id s, SEL c) { (void)c; return NOD(s)->outvol; }
static void mixer_set_outvol(id s, SEL c, float v) { (void)c; NOD(s)->outvol = v < 0 ? 0 : v; }
static u64 mixer_next_bus(id s, SEL c) { (void)c; return (u64)NOD(s)->nin; }
static float io_presentation_latency(id s, SEL c) { (void)s; (void)c; return (float)(2048.0 / RATE); }
static BOOL io_voice_enabled(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL io_set_voice(id s, SEL c, BOOL on, id *err) { (void)s; (void)c; (void)on; (void)err; return YES; }
static void *io_audio_unit(id s, SEL c) { (void)s; (void)c; return 0; }
/* efeitos: passam o som adiante; os parâmetros ficam guardados */
static BOOL fx_bypass(id s, SEL c) { (void)c; return NOD(s)->bypass; }
static void fx_set_bypass(id s, SEL c, BOOL b) { (void)c; NOD(s)->bypass = b; }
static id fx_init_bands(id s, SEL c, u64 n) { (void)c; (void)n; node_setup(NOD(s), N_EFFECT); return s; }
static float fx_param(id s, SEL c) {
    avnode_t *n = NOD(s);
    id v = n->params ? MSG(id, n->params, "objectForKey:", cx_str(c)) : 0;
    return v ? MSG0(float, v, "floatValue") : (!strcmp(c, "rate") || !strcmp(c, "playbackRate") ? 1.0f : 0.0f);
}
static void fx_set_param(id s, SEL c, float v) {
    avnode_t *n = NOD(s);
    if (!n->params) n->params = objc_retain(mdict());
    /* "setWetDryMix:" -> "wetDryMix" */
    char k[64];
    size_t l = strlen(c);
    if (l < 5 || l > 60) return;
    memcpy(k, c + 3, l - 4);
    k[l - 4] = 0;
    k[0] = (char)(k[0] >= 'A' && k[0] <= 'Z' ? k[0] + 32 : k[0]);
    MSG(void, n->params, "setObject:forKey:", cx_num_d(v), cx_str(k));
}
static void fx_load_preset(id s, SEL c, i64 p) { (void)s; (void)c; (void)p; }
static id fx_bands(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSArray"), "array"); }

/* ---- AVAudioPlayerNode */
static void player_add(id s, sched_t *q) {
    avnode_t *n = NOD(s);
    avengine_t *e = n->engine ? ENG(n->engine) : 0;
    if (e) pthread_mutex_lock(e->lock);
    if (q->options & 2) {   /* Interrupts: tira o que estava agendado */
        while (n->sq) { sched_t *x = n->sq; n->sq = x->next; sched_free(x); }
    }
    sched_t **pp = &n->sq;
    while (*pp) pp = &(*pp)->next;
    *pp = q;
    if (e) pthread_mutex_unlock(e->lock);
}
static void sched_buffer(id s, id buf, u64 opts, void *done, int kind) {
    if (!buf) return;
    sched_t *q = calloc(1, sizeof *q);
    if (!q) return;
    q->buffer = objc_retain(buf);
    q->count = BUF(buf)->len;
    q->rate = FMT(BUF(buf)->fmt)->d.mSampleRate;
    q->options = (u32)opts;
    q->done = done ? _Block_copy(done) : 0;
    q->done_kind = kind;
    player_add(s, q);
}
static void pn_sched_buf(id s, SEL c, id buf, void *done) { (void)c; sched_buffer(s, buf, 0, done, 0); }
static void pn_sched_buf_opts(id s, SEL c, id buf, id when, u64 opts, void *done) { (void)c; (void)when; sched_buffer(s, buf, opts, done, 0); }
static void pn_sched_buf_type(id s, SEL c, id buf, id when, u64 opts, u64 type, void *done) { (void)c; (void)when; (void)type; sched_buffer(s, buf, opts, done, 1); }
static void pn_sched_buf_type2(id s, SEL c, id buf, u64 type, void *done) { (void)c; (void)type; sched_buffer(s, buf, 0, done, 1); }
static void sched_file(id s, id file, i64 start, u32 frames, void *done, int kind) {
    if (!file || !AVF(file)->url) return;
    int e = 0;
    void *d = cx_audio_open_path(cx_audio_url_path(AVF(file)->url), &e);
    if (!d) return;
    sched_t *q = calloc(1, sizeof *q);
    if (!q) { cx_audio_close(d); return; }
    u64 total = cx_audio_frames(d);
    q->dec = d;
    q->start = start > 0 && (u64)start < total ? (u64)start : 0;
    q->count = frames && q->start + frames <= total ? frames : total - q->start;
    q->rate = cx_audio_rate(d);
    q->done = done ? _Block_copy(done) : 0;
    q->done_kind = kind;
    player_add(s, q);
}
static void pn_sched_file(id s, SEL c, id file, id when, void *done) { (void)c; (void)when; sched_file(s, file, 0, 0, done, 0); }
static void pn_sched_file_type(id s, SEL c, id file, id when, u64 type, void *done) { (void)c; (void)when; (void)type; sched_file(s, file, 0, 0, done, 1); }
static void pn_sched_seg(id s, SEL c, id file, i64 start, u32 n, id when, void *done) { (void)c; (void)when; sched_file(s, file, start, n, done, 0); }
static void pn_sched_seg_type(id s, SEL c, id file, i64 start, u32 n, id when, u64 type, void *done) { (void)c; (void)when; (void)type; sched_file(s, file, start, n, done, 1); }
static void pn_play(id s, SEL c) { (void)c; NOD(s)->playing = 1; }
static void pn_play_at(id s, SEL c, id when) { (void)c; (void)when; NOD(s)->playing = 1; }
static void pn_pause(id s, SEL c) { (void)c; NOD(s)->playing = 0; }
static void pn_stop(id s, SEL c) {
    (void)c;
    avnode_t *n = NOD(s);
    avengine_t *e = n->engine ? ENG(n->engine) : 0;
    donejob_t *done = 0;
    if (e) pthread_mutex_lock(e->lock);
    n->playing = 0;
    while (n->sq) {
        sched_t *q = n->sq;
        n->sq = q->next;
        if (q->done) { donejob_t *d = calloc(1, sizeof *d); if (d) { d->blk = q->done; d->kind = q->done_kind; q->done = 0; d->next = done; done = d; } }
        sched_free(q);
    }
    n->node_time = 0;
    if (e) pthread_mutex_unlock(e->lock);
    if (done) done_thread(done);   /* o stop chama os handlers pendentes, como o AVAudioPlayerNode */
}
static BOOL pn_playing(id s, SEL c) { (void)c; return NOD(s)->playing; }
static void pn_prepare(id s, SEL c, u32 n) { (void)s; (void)c; (void)n; }
static id pn_player_time(id s, SEL c, id node_time) { (void)c; return node_time ? node_time : node_last_time(s, 0); }
static id pn_node_time(id s, SEL c, id player_time) { (void)s; (void)c; return player_time; }

/* ---- AVAudioSourceNode */
static id src_init_block(id s, SEL c, void *blk) { (void)c; node_setup(NOD(s), N_SOURCE); NOD(s)->render_block = blk ? _Block_copy(blk) : 0; return s; }
static id src_init_fmt_block(id s, SEL c, id fmt, void *blk) { src_init_block(s, c, blk); if (fmt) setobj(&NOD(s)->src_fmt, fmt); return s; }
/* AVAudioSinkNode: recebe a entrada (silêncio no mOS) */
static id sink_init_block(id s, SEL c, void *blk) { (void)c; (void)blk; node_setup(NOD(s), N_SINK); return s; }

/* ================================================================ AVAudioPlayer */
typedef struct {
    cx_class_t *isa;
    void *dec;
    void *eng;
    id url, data, delegate, fmt;
    double pos;                  /* quadros do arquivo já entregues ao motor */
    double rate;
    float volume, pan;
    i64 loops, loops_left;
    BOOL playing, enable_rate, metering, finished_pending;
    float peak[2], avg[2];
    i64 ok;
} avplayer_t;
cx_class_t AVAudioPlayer_c __asm__("OBJC_CLASS_$_AVAudioPlayer"), AVAudioPlayer_m __asm__("OBJC_METACLASS_$_AVAudioPlayer");
#define PLR(o) ((avplayer_t *)(o))

static void player_finished_main(void *a) {
    id s = a;
    avplayer_t *p = PLR(s);
    p->playing = NO;
    if (p->eng) cx_cae_stop(p->eng, 0);
    p->pos = 0;
    id d = p->delegate;
    if (d && resp(d, "audioPlayerDidFinishPlaying:successfully:")) MSG(void, d, "audioPlayerDidFinishPlaying:successfully:", s, (BOOL)YES);
    objc_release(s);
}
static void plr_render(void *ce, float *stereo, u32 frames, const void *ts) {
    (void)ts;
    id s = cx_cae_ctx(ce);
    avplayer_t *p = PLR(s);
    if (!p->playing || !p->dec) return;
    u64 total = cx_audio_frames(p->dec);
    u32 o = 0;
    while (o < frames) {
        u64 at = (u64)p->pos;
        if (at >= total) {
            if (p->loops_left != 0) { if (p->loops_left > 0) p->loops_left--; p->pos = 0; continue; }
            if (!p->finished_pending) { p->finished_pending = YES; on_main(player_finished_main, objc_retain(s)); }
            break;
        }
        u32 k = cx_audio_read_stereo(p->dec, at, frames - o, stereo + o * 2);
        if (!k) break;
        o += k;
        p->pos += k;
    }
    float vl = p->volume * (p->pan > 0 ? 1 - p->pan : 1), vr = p->volume * (p->pan < 0 ? 1 + p->pan : 1);
    float pk[2] = {0, 0};
    double sq[2] = {0, 0};
    for (u32 i = 0; i < o; i++) {
        stereo[i * 2] *= vl;
        stereo[i * 2 + 1] *= vr;
        for (int c = 0; c < 2; c++) {
            float v = stereo[i * 2 + c];
            float a = v < 0 ? -v : v;
            if (a > pk[c]) pk[c] = a;
            sq[c] += (double)v * v;
        }
    }
    if (p->metering && o) for (int c = 0; c < 2; c++) { p->peak[c] = pk[c]; p->avg[c] = (float)sqrt(sq[c] / o); }
}
static id plr_setup(id s, void *dec, id *err) {
    avplayer_t *p = PLR(s);
    p->dec = dec;
    p->volume = 1;
    p->rate = 1;
    p->fmt = objc_retain(fmt_init_asbd(cx_alloc_instance(&AVAudioFormat_c), 0, cx_audio_asbd(dec)));
    (void)err;
    return s;
}
static id plr_init_url(id s, SEL c, id url, id *err) {
    (void)c;
    int e = 0;
    void *d = cx_audio_open_path(cx_audio_url_path(url), &e);
    if (!d) { set_err(err, e, e == -43 ? "O arquivo não foi encontrado." : "O formato do arquivo de áudio não é suportado."); objc_release(s); return 0; }
    setobj(&PLR(s)->url, url);
    return plr_setup(s, d, err);
}
static id plr_init_url_hint(id s, SEL c, id url, id hint, id *err) { (void)hint; return plr_init_url(s, c, url, err); }
static id plr_init_data(id s, SEL c, id data, id *err) {
    (void)c;
    const void *b = data ? MSG0(const void *, data, "bytes") : 0;
    u64 n = data ? MSG0(u64, data, "length") : 0;
    int e = 0;
    void *d = b ? cx_audio_open_data(b, n, &e) : 0;
    if (!d) { set_err(err, e ? e : -50, "O formato do áudio não é suportado."); objc_release(s); return 0; }
    setobj(&PLR(s)->data, data);
    return plr_setup(s, d, err);
}
static id plr_init_data_hint(id s, SEL c, id data, id hint, id *err) { (void)hint; return plr_init_data(s, c, data, err); }
static BOOL plr_prepare(id s, SEL c) {
    (void)c;
    avplayer_t *p = PLR(s);
    if (!p->eng) p->eng = cx_cae_new(cx_audio_rate(p->dec) * (p->enable_rate ? p->rate : 1), 0, plr_render, s);
    return p->eng != 0;
}
static BOOL plr_play(id s, SEL c) {
    avplayer_t *p = PLR(s);
    if (!plr_prepare(s, c)) return NO;
    p->finished_pending = NO;
    if (!p->playing) p->loops_left = p->loops;
    p->playing = YES;
    return cx_cae_start(p->eng) == 0;
}
typedef struct { id s; double delay; } playat_t;
static void *playat_thread(void *c) {
    playat_t *t = c;
    if (t->delay > 0) usleep((unsigned)(t->delay * 1e6));
    plr_play(t->s, 0);
    objc_release(t->s);
    free(t);
    return 0;
}
static BOOL plr_play_at(id s, SEL c, double when) {
    (void)c;
    double delay = when - host_seconds();
    if (delay <= 0) return plr_play(s, 0);
    playat_t *t = malloc(sizeof *t);
    if (!t) return NO;
    t->s = objc_retain(s);
    t->delay = delay;
    unsigned long th;
    if (pthread_create(&th, 0, playat_thread, t) != 0) { objc_release(s); free(t); return NO; }
    pthread_detach(th);
    return YES;
}
static void plr_pause(id s, SEL c) { (void)c; avplayer_t *p = PLR(s); p->playing = NO; if (p->eng) cx_cae_stop(p->eng, 1); }
static void plr_stop(id s, SEL c) { (void)c; avplayer_t *p = PLR(s); p->playing = NO; if (p->eng) cx_cae_stop(p->eng, 1); }
static BOOL plr_is_playing(id s, SEL c) { (void)c; return PLR(s)->playing; }
static float plr_volume(id s, SEL c) { (void)c; return PLR(s)->volume; }
static void plr_set_volume(id s, SEL c, float v) { (void)c; PLR(s)->volume = v < 0 ? 0 : v > 1 ? 1 : v; }
static void plr_set_volume_fade(id s, SEL c, float v, double dur) { (void)dur; plr_set_volume(s, c, v); }
static float plr_pan(id s, SEL c) { (void)c; return PLR(s)->pan; }
static void plr_set_pan(id s, SEL c, float v) { (void)c; PLR(s)->pan = v < -1 ? -1 : v > 1 ? 1 : v; }
static BOOL plr_enable_rate(id s, SEL c) { (void)c; return PLR(s)->enable_rate; }
static void plr_set_enable_rate(id s, SEL c, BOOL b) { (void)c; PLR(s)->enable_rate = b; }
static float plr_rate(id s, SEL c) { (void)c; return (float)PLR(s)->rate; }
static void plr_set_rate(id s, SEL c, float r) {
    (void)c;
    avplayer_t *p = PLR(s);
    if (r < 0.5f) r = 0.5f;
    if (r > 2.0f) r = 2.0f;
    p->rate = r;
    if (p->eng && p->enable_rate) cx_cae_set_rate(p->eng, cx_audio_rate(p->dec) * r);
}
static i64 plr_loops(id s, SEL c) { (void)c; return PLR(s)->loops; }
static void plr_set_loops(id s, SEL c, i64 n) { (void)c; PLR(s)->loops = n; PLR(s)->loops_left = n; }
static double plr_duration(id s, SEL c) { (void)c; void *d = PLR(s)->dec; return d ? (double)cx_audio_frames(d) / cx_audio_rate(d) : 0; }
static double plr_current(id s, SEL c) {
    (void)c;
    avplayer_t *p = PLR(s);
    if (!p->dec) return 0;
    /* o que já foi entregue menos o que ainda está na fila do kernel */
    double lag = p->eng ? (double)cx_cae_queued(p->eng) / RATE * cx_audio_rate(p->dec) : 0;
    double f = p->pos - lag;
    return f > 0 ? f / cx_audio_rate(p->dec) : 0;
}
static void plr_set_current(id s, SEL c, double t) {
    (void)c;
    avplayer_t *p = PLR(s);
    if (!p->dec) return;
    double f = t * cx_audio_rate(p->dec);
    double max = (double)cx_audio_frames(p->dec);
    p->pos = f < 0 ? 0 : f > max ? max : f;
}
static double plr_device_time(id s, SEL c) { (void)s; (void)c; return host_seconds(); }
static id plr_url(id s, SEL c) { (void)c; return PLR(s)->url; }
static id plr_data(id s, SEL c) { (void)c; return PLR(s)->data; }
static u64 plr_channels(id s, SEL c) { (void)c; return PLR(s)->dec ? cx_audio_channels(PLR(s)->dec) : 0; }
static id plr_format(id s, SEL c) { (void)c; return PLR(s)->fmt; }
static id plr_settings(id s, SEL c) { (void)c; return PLR(s)->fmt ? fmt_settings(PLR(s)->fmt, 0) : 0; }
static id plr_delegate(id s, SEL c) { (void)c; return PLR(s)->delegate; }
static void plr_set_delegate(id s, SEL c, id d) { (void)c; PLR(s)->delegate = d; }
static BOOL plr_metering(id s, SEL c) { (void)c; return PLR(s)->metering; }
static void plr_set_metering(id s, SEL c, BOOL b) { (void)c; PLR(s)->metering = b; }
static void plr_update_meters(id s, SEL c) { (void)s; (void)c; }
static float db_of(float a) { return a <= 0.00001f ? -160.0f : (float)(20.0 * log10(a)); }
static float plr_avg_power(id s, SEL c, u64 ch) { (void)c; return db_of(PLR(s)->avg[ch > 1 ? 1 : ch]); }
static float plr_peak_power(id s, SEL c, u64 ch) { (void)c; return db_of(PLR(s)->peak[ch > 1 ? 1 : ch]); }
static id plr_channel_assign(id s, SEL c) { (void)s; (void)c; return 0; }
static void plr_dealloc(id s, SEL c) {
    (void)c;
    avplayer_t *p = PLR(s);
    if (p->eng) { cx_cae_free(p->eng); p->eng = 0; }
    if (p->dec) { cx_audio_close(p->dec); p->dec = 0; }
    if (p->url) objc_release(p->url);
    if (p->data) objc_release(p->data);
    if (p->fmt) objc_release(p->fmt);
    p->url = p->data = p->fmt = 0;
    struct { id r; cx_class_t *s; } sup = {s, cx_class_named("NSObject")};
    ((void (*)(void *, SEL))objc_msgSendSuper)(&sup, sel_registerName("dealloc"));
}

/* ================================================================ AVAudioSession (Catalyst/iOS) */
typedef struct { cx_class_t *isa; id category, mode; u64 options; BOOL active; double pref_rate, pref_io; } avsession_t;
cx_class_t AVAudioSession_c __asm__("OBJC_CLASS_$_AVAudioSession"), AVAudioSession_m __asm__("OBJC_METACLASS_$_AVAudioSession");
cx_class_t AVAudioSessionRouteDescription_c __asm__("OBJC_CLASS_$_AVAudioSessionRouteDescription"), AVAudioSessionRouteDescription_m __asm__("OBJC_METACLASS_$_AVAudioSessionRouteDescription");
cx_class_t AVAudioSessionPortDescription_c __asm__("OBJC_CLASS_$_AVAudioSessionPortDescription"), AVAudioSessionPortDescription_m __asm__("OBJC_METACLASS_$_AVAudioSessionPortDescription");
#define SES(o) ((avsession_t *)(o))
id AVAudioSessionCategoryAmbient, AVAudioSessionCategorySoloAmbient, AVAudioSessionCategoryPlayback, AVAudioSessionCategoryRecord,
   AVAudioSessionCategoryPlayAndRecord, AVAudioSessionCategoryMultiRoute, AVAudioSessionCategoryAudioProcessing;
id AVAudioSessionModeDefault, AVAudioSessionModeVoiceChat, AVAudioSessionModeVideoChat, AVAudioSessionModeGameChat,
   AVAudioSessionModeVideoRecording, AVAudioSessionModeMeasurement, AVAudioSessionModeMoviePlayback, AVAudioSessionModeSpokenAudio,
   AVAudioSessionModeVoicePrompt;
id AVAudioSessionInterruptionNotification, AVAudioSessionRouteChangeNotification, AVAudioSessionMediaServicesWereLostNotification,
   AVAudioSessionMediaServicesWereResetNotification, AVAudioSessionSilenceSecondaryAudioHintNotification,
   AVAudioSessionInterruptionTypeKey, AVAudioSessionInterruptionOptionKey, AVAudioSessionRouteChangeReasonKey,
   AVAudioSessionRouteChangePreviousRouteKey;
id AVAudioSessionPortBuiltInSpeaker, AVAudioSessionPortBuiltInMic, AVAudioSessionPortHeadphones, AVAudioSessionPortLineOut,
   AVAudioSessionPortBluetoothA2DP, AVAudioSessionPortHDMI;
id AVPlayerItemDidPlayToEndTimeNotification, AVPlayerItemFailedToPlayToEndTimeNotification, AVPlayerItemTimeJumpedNotification;
id AVFoundationErrorDomain;
id AVAudioEngineConfigurationChangeNotification;

static id g_session;
static id ses_shared(id k, SEL c) {
    (void)k; (void)c;
    if (!g_session) {
        g_session = cx_alloc_instance(&AVAudioSession_c);
        SES(g_session)->category = objc_retain(AVAudioSessionCategorySoloAmbient);
        SES(g_session)->mode = objc_retain(AVAudioSessionModeDefault);
        SES(g_session)->pref_rate = RATE;
        SES(g_session)->pref_io = 512.0 / RATE;
    }
    return g_session;
}
static id ses_category(id s, SEL c) { (void)c; return SES(s)->category; }
static id ses_mode(id s, SEL c) { (void)c; return SES(s)->mode; }
static u64 ses_options(id s, SEL c) { (void)c; return SES(s)->options; }
static BOOL ses_set_cat(id s, SEL c, id cat, id *err) { (void)c; (void)err; if (cat) setobj(&SES(s)->category, cat); return YES; }
static BOOL ses_set_cat_opts(id s, SEL c, id cat, u64 o, id *err) { SES(s)->options = o; return ses_set_cat(s, c, cat, err); }
static BOOL ses_set_cat_mode_opts(id s, SEL c, id cat, id mode, u64 o, id *err) { if (mode) setobj(&SES(s)->mode, mode); return ses_set_cat_opts(s, c, cat, o, err); }
static BOOL ses_set_cat_mode_policy(id s, SEL c, id cat, id mode, u64 pol, u64 o, id *err) { (void)pol; return ses_set_cat_mode_opts(s, c, cat, mode, o, err); }
static BOOL ses_set_mode(id s, SEL c, id mode, id *err) { (void)c; (void)err; if (mode) setobj(&SES(s)->mode, mode); return YES; }
static BOOL ses_set_active(id s, SEL c, BOOL a, id *err) { (void)c; (void)err; SES(s)->active = a; return YES; }
static BOOL ses_set_active_opts(id s, SEL c, BOOL a, u64 o, id *err) { (void)o; return ses_set_active(s, c, a, err); }
static void ses_activate_opts(id s, SEL c, u64 o, void *blk) {
    (void)c; (void)o;
    SES(s)->active = YES;
    if (blk) ((void (*)(void *, BOOL, id))BLK_FN(blk))(blk, YES, 0);
}
static double ses_rate(id s, SEL c) { (void)s; (void)c; return RATE; }
static double ses_pref_rate(id s, SEL c) { (void)c; return SES(s)->pref_rate; }
static BOOL ses_set_pref_rate(id s, SEL c, double r, id *err) { (void)c; (void)err; SES(s)->pref_rate = r; return YES; }
static double ses_io(id s, SEL c) { (void)c; return SES(s)->pref_io; }
static BOOL ses_set_pref_io(id s, SEL c, double d, id *err) { (void)c; (void)err; SES(s)->pref_io = d < 0.002 ? 0.002 : d > 0.1 ? 0.1 : d; return YES; }
static float ses_out_volume(id s, SEL c) { (void)s; (void)c; return 1.0f; }
static double ses_out_latency(id s, SEL c) { (void)s; (void)c; return 2048.0 / RATE; }
static double ses_in_latency(id s, SEL c) { (void)s; (void)c; return 0; }
static i64 ses_out_channels(id s, SEL c) { (void)s; (void)c; return 2; }
static i64 ses_in_channels(id s, SEL c) { (void)s; (void)c; return 0; }
static BOOL ses_no(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL ses_yes_err(id s, SEL c, i64 v, id *err) { (void)s; (void)c; (void)v; (void)err; return YES; }
static u64 ses_record_perm(id s, SEL c) { (void)s; (void)c; return FCC('d', 'e', 'n', 'y'); }   /* sem microfone */
static void ses_request_record(id s, SEL c, void *blk) { (void)s; (void)c; if (blk) ((void (*)(void *, BOOL))BLK_FN(blk))(blk, NO); }
static id ses_empty(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSArray"), "array"); }
static id port_new(id type, const char *name) {
    id p = cx_alloc_instance(&AVAudioSessionPortDescription_c);
    objc_setAssociatedObject(p, "type", type, 1);
    objc_setAssociatedObject(p, "name", cx_str(name), 1);
    return p;
}
static id port_type(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, "type"); }
static id port_name(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, "name"); }
static id port_uid(id s, SEL c) { (void)s; (void)c; return cx_str("mOS:saida"); }
static id ses_route(id s, SEL c) {
    (void)s; (void)c;
    id r = cx_alloc_instance(&AVAudioSessionRouteDescription_c);
    id outs = MSG(id, cls("NSArray"), "arrayWithObject:", port_new(AVAudioSessionPortBuiltInSpeaker, "Alto-falantes"));
    objc_setAssociatedObject(r, "outputs", outs, 1);
    return objc_autorelease(r);
}
static id route_outputs(id s, SEL c) { (void)c; return objc_getAssociatedObject(s, "outputs"); }

/* ================================================================ AVAsset / AVPlayerItem / AVPlayer (áudio local) */
typedef struct { cx_class_t *isa; id url; void *dec; int err; } avasset_t;
typedef struct { cx_class_t *isa; id asset; i64 status; id error; double pos; } avitem_t;
typedef struct {
    cx_class_t *isa;
    id item;
    void *eng;
    float rate, volume;
    BOOL muted;
    i64 status;
    id observers;                /* NSMutableArray de observadores periódicos */
    double last_obs;
} avpl_t;
cx_class_t AVAsset_c __asm__("OBJC_CLASS_$_AVAsset"), AVAsset_m __asm__("OBJC_METACLASS_$_AVAsset");
cx_class_t AVURLAsset_c __asm__("OBJC_CLASS_$_AVURLAsset"), AVURLAsset_m __asm__("OBJC_METACLASS_$_AVURLAsset");
cx_class_t AVPlayerItem_c __asm__("OBJC_CLASS_$_AVPlayerItem"), AVPlayerItem_m __asm__("OBJC_METACLASS_$_AVPlayerItem");
cx_class_t AVPlayer_c __asm__("OBJC_CLASS_$_AVPlayer"), AVPlayer_m __asm__("OBJC_METACLASS_$_AVPlayer");
cx_class_t AVQueuePlayer_c __asm__("OBJC_CLASS_$_AVQueuePlayer"), AVQueuePlayer_m __asm__("OBJC_METACLASS_$_AVQueuePlayer");
#define AST(o) ((avasset_t *)(o))
#define ITM(o) ((avitem_t *)(o))
#define APL(o) ((avpl_t *)(o))

static cmtime_t cm_seconds(double s) { cmtime_t t = {(i64)(s * 600 + 0.5), 600, 1, 0}; return t; }
static cmtime_t cm_invalid(void) { cmtime_t t = {0, 0, 0, 0}; return t; }
static double cm_to_sec(cmtime_t t) { return t.timescale > 0 && (t.flags & 1) ? (double)t.value / t.timescale : 0; }

static id asset_init(id s, SEL c, id url, id opts) {
    (void)c; (void)opts;
    setobj(&AST(s)->url, url);
    int e = 0;
    const char *path = cx_audio_url_path(url);
    AST(s)->dec = path ? cx_audio_open_path(path, &e) : 0;
    AST(s)->err = AST(s)->dec ? 0 : (e ? e : -11828);
    return s;
}
static id asset_with_url(id k, SEL c, id url) { (void)k; return objc_autorelease(asset_init(cx_alloc_instance(&AVURLAsset_c), c, url, 0)); }
static id asset_with_url_opts(id k, SEL c, id url, id opts) { (void)k; return objc_autorelease(asset_init(cx_alloc_instance(&AVURLAsset_c), c, url, opts)); }
static id asset_url(id s, SEL c) { (void)c; return AST(s)->url; }
static cmtime_t asset_duration(id s, SEL c) { (void)c; void *d = AST(s)->dec; return d ? cm_seconds((double)cx_audio_frames(d) / cx_audio_rate(d)) : cm_invalid(); }
static BOOL asset_playable(id s, SEL c) { (void)c; return AST(s)->dec != 0; }
static id asset_tracks(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSArray"), "array"); }
static id asset_tracks_media(id s, SEL c, id m) { (void)m; return asset_tracks(s, c); }
static void asset_load(id s, SEL c, id keys, void *blk) { (void)s; (void)c; (void)keys; if (blk) CALL_BLOCK0(blk); }
static i64 asset_status_key(id s, SEL c, id key, id *err) {
    (void)c; (void)key;
    if (AST(s)->dec) return 2;   /* AVKeyValueStatusLoaded */
    if (err) *err = error_with("AVFoundationErrorDomain", -11828, "O formato não é reconhecido pelo mOS.");
    return 3;                    /* AVKeyValueStatusFailed */
}

static id item_init_asset(id s, SEL c, id asset) {
    (void)c;
    setobj(&ITM(s)->asset, asset);
    ITM(s)->status = asset && AST(asset)->dec ? 1 : 2;   /* ReadyToPlay / Failed */
    if (ITM(s)->status == 2) ITM(s)->error = objc_retain(error_with("AVFoundationErrorDomain", -11828, "O formato não é reconhecido pelo mOS."));
    return s;
}
static id item_init_url(id s, SEL c, id url) { return item_init_asset(s, c, objc_autorelease(asset_init(cx_alloc_instance(&AVURLAsset_c), 0, url, 0))); }
static id item_with_url(id k, SEL c, id url) { (void)k; return objc_autorelease(item_init_url(cx_alloc_instance(&AVPlayerItem_c), c, url)); }
static id item_with_asset(id k, SEL c, id a) { (void)k; return objc_autorelease(item_init_asset(cx_alloc_instance(&AVPlayerItem_c), c, a)); }
static id item_asset(id s, SEL c) { (void)c; return ITM(s)->asset; }
static i64 item_status(id s, SEL c) { (void)c; return ITM(s)->status; }
static id item_error(id s, SEL c) { (void)c; return ITM(s)->error; }
static cmtime_t item_duration(id s, SEL c) { (void)c; return ITM(s)->asset ? asset_duration(ITM(s)->asset, 0) : cm_invalid(); }
static cmtime_t item_current(id s, SEL c) { (void)c; return cm_seconds(ITM(s)->pos); }

static void apl_end_main(void *a) {
    id s = a;
    avpl_t *p = APL(s);
    p->rate = 0;
    if (p->eng) cx_cae_stop(p->eng, 0);
    if (p->item) cx_post(cx_note(AVPlayerItemDidPlayToEndTimeNotification, p->item, 0));
    objc_release(s);
}
static void apl_render(void *ce, float *stereo, u32 frames, const void *ts) {
    (void)ts;
    id s = cx_cae_ctx(ce);
    avpl_t *p = APL(s);
    if (p->rate == 0 || !p->item || !ITM(p->item)->asset) return;
    void *d = AST(ITM(p->item)->asset)->dec;
    if (!d) return;
    double rate = cx_audio_rate(d);
    u64 at = (u64)(ITM(p->item)->pos * rate);
    u64 total = cx_audio_frames(d);
    if (at >= total) return;
    u32 k = cx_audio_read_stereo(d, at, frames, stereo);
    float g = p->muted ? 0 : p->volume;
    for (u32 i = 0; i < k * 2; i++) stereo[i] *= g;
    ITM(p->item)->pos += (double)k / rate;
    if (at + k >= total) { p->rate = 0; on_main(apl_end_main, objc_retain(s)); }
}
static id apl_init(id s, SEL c) { (void)c; APL(s)->volume = 1; APL(s)->status = 1; APL(s)->observers = objc_retain(MSG0(id, cls("NSMutableArray"), "array")); return s; }
static id apl_init_item(id s, SEL c, id item) { apl_init(s, c); setobj(&APL(s)->item, item); return s; }
static id apl_init_url(id s, SEL c, id url) { return apl_init_item(s, c, item_with_url(0, 0, url)); }
static id apl_with_url(id k, SEL c, id url) { (void)k; return objc_autorelease(apl_init_url(cx_alloc_instance(&AVPlayer_c), c, url)); }
static id apl_with_item(id k, SEL c, id item) { (void)k; return objc_autorelease(apl_init_item(cx_alloc_instance(&AVPlayer_c), c, item)); }
static void apl_ensure(id s) {
    avpl_t *p = APL(s);
    if (p->eng || !p->item || !ITM(p->item)->asset || !AST(ITM(p->item)->asset)->dec) return;
    p->eng = cx_cae_new(cx_audio_rate(AST(ITM(p->item)->asset)->dec), 0, apl_render, s);
}
static void apl_set_rate(id s, SEL c, float r) {
    (void)c;
    avpl_t *p = APL(s);
    p->rate = r;
    if (r != 0) { apl_ensure(s); if (p->eng) { cx_cae_set_rate(p->eng, cx_audio_rate(AST(ITM(p->item)->asset)->dec) * r); cx_cae_start(p->eng); } }
    else if (p->eng) cx_cae_stop(p->eng, 1);
}
static void apl_play(id s, SEL c) { (void)c; apl_set_rate(s, 0, 1.0f); }
static void apl_pause(id s, SEL c) { (void)c; apl_set_rate(s, 0, 0); }
static float apl_rate(id s, SEL c) { (void)c; return APL(s)->rate; }
static float apl_volume(id s, SEL c) { (void)c; return APL(s)->volume; }
static void apl_set_volume(id s, SEL c, float v) { (void)c; APL(s)->volume = v < 0 ? 0 : v; }
static BOOL apl_muted(id s, SEL c) { (void)c; return APL(s)->muted; }
static void apl_set_muted(id s, SEL c, BOOL m) { (void)c; APL(s)->muted = m; }
static i64 apl_status(id s, SEL c) { (void)c; return APL(s)->item && ITM(APL(s)->item)->status == 2 ? 2 : 1; }
static id apl_error(id s, SEL c) { (void)c; return APL(s)->item ? ITM(APL(s)->item)->error : 0; }
static id apl_item(id s, SEL c) { (void)c; return APL(s)->item; }
static i64 apl_time_control(id s, SEL c) { (void)c; return APL(s)->rate != 0 ? 2 : 0; }   /* Playing / Paused */
static void apl_replace(id s, SEL c, id item) {
    (void)c;
    avpl_t *p = APL(s);
    if (p->eng) { cx_cae_free(p->eng); p->eng = 0; }
    setobj(&p->item, item);
    if (p->rate != 0) apl_set_rate(s, 0, p->rate);
}
static cmtime_t apl_current(id s, SEL c) { (void)c; return APL(s)->item ? cm_seconds(ITM(APL(s)->item)->pos) : cm_invalid(); }
static void apl_seek(id s, SEL c, cmtime_t t) { (void)c; if (APL(s)->item) ITM(APL(s)->item)->pos = cm_to_sec(t); }
static void apl_seek_done(id s, SEL c, cmtime_t t, void *blk) { apl_seek(s, c, t); if (blk) ((void (*)(void *, BOOL))BLK_FN(blk))(blk, YES); }
static void apl_seek_tol(id s, SEL c, cmtime_t t, cmtime_t a, cmtime_t b) { (void)a; (void)b; apl_seek(s, c, t); }
static void apl_seek_tol_done(id s, SEL c, cmtime_t t, cmtime_t a, cmtime_t b, void *blk) { (void)a; (void)b; apl_seek_done(s, c, t, blk); }
/* observadores periódicos: um timer de 1/20 s no run loop chama o bloco com o tempo atual quando o intervalo passou */
typedef struct { cx_class_t *isa; void *blk; double interval, last; id player; } aplobs_t;
cx_class_t MOSAVTimeObserver_c __asm__("OBJC_CLASS_$_MOSAVTimeObserver"), MOSAVTimeObserver_m __asm__("OBJC_METACLASS_$_MOSAVTimeObserver");
static void obs_fire(id s, SEL c, id timer) {
    (void)c; (void)timer;
    aplobs_t *o = (aplobs_t *)s;
    if (!o->blk || !o->player) return;
    double now = host_seconds();
    if (now - o->last < o->interval) return;
    o->last = now;
    if (APL(o->player)->rate == 0) return;
    cmtime_t t = apl_current(o->player, 0);
    ((void (*)(void *, cmtime_t))BLK_FN(o->blk))(o->blk, t);
}
static id apl_add_periodic(id s, SEL c, cmtime_t interval, void *queue, void *blk) {
    (void)c; (void)queue;
    aplobs_t *o = (aplobs_t *)cx_alloc_instance(&MOSAVTimeObserver_c);
    o->blk = blk ? _Block_copy(blk) : 0;
    o->interval = cm_to_sec(interval) > 0 ? cm_to_sec(interval) : 0.5;
    o->player = s;
    MSG(void, APL(s)->observers, "addObject:", (id)o);
    MSG(id, cls("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", 0.05, (id)o, sel_registerName("fire:"), (id)0, (BOOL)YES);
    return (id)o;
}
static id apl_add_boundary(id s, SEL c, id times, void *queue, void *blk) { (void)s; (void)c; (void)times; (void)queue; (void)blk; return cx_alloc_instance(&MOSAVTimeObserver_c); }
static void apl_remove_obs(id s, SEL c, id o) {
    (void)c;
    if (!o) return;
    aplobs_t *ob = (aplobs_t *)o;
    if (ob->blk) { _Block_release(ob->blk); ob->blk = 0; }
    ob->player = 0;
    MSG(void, APL(s)->observers, "removeObject:", o);
}

/* ================================================================ NSSound (AppKit) */
static const char NSSOUND_KEY = 0;
static id snd_player(id s) { return objc_getAssociatedObject(s, &NSSOUND_KEY); }
static id snd_attach(id s, id plr) { if (!plr) { objc_release(s); return 0; } objc_setAssociatedObject(s, &NSSOUND_KEY, plr, 1); objc_release(plr); return s; }
static id snd_init_file(id s, SEL c, id path, BOOL byref) { (void)c; (void)byref; return snd_attach(s, plr_init_url(cx_alloc_instance(&AVAudioPlayer_c), 0, path, 0)); }
static id snd_init_url(id s, SEL c, id url, BOOL byref) { (void)c; (void)byref; return snd_attach(s, plr_init_url(cx_alloc_instance(&AVAudioPlayer_c), 0, url, 0)); }
static id snd_init_data(id s, SEL c, id data) { (void)c; return snd_attach(s, plr_init_data(cx_alloc_instance(&AVAudioPlayer_c), 0, data, 0)); }
static id snd_named(id k, SEL c, id name) {
    (void)k; (void)c;
    /* os sons do sistema: /System/Library/Sounds/<nome>.aiff ou .wav no mOS, se existirem */
    const char *n = name ? cx_cstr(name) : 0;
    if (!n) return 0;
    char path[512];
    const char *dirs[] = {"/System/Library/Sounds/", "/Sistema/Biblioteca/Sons/"};
    const char *exts[] = {"aiff", "wav", "caf"};
    for (int d = 0; d < 2; d++)
        for (int e = 0; e < 3; e++) {
            snprintf(path, sizeof path, "%s%s.%s", dirs[d], n, exts[e]);
            id p = plr_init_url(cx_alloc_instance(&AVAudioPlayer_c), 0, cx_str(path), 0);
            if (p) { id s = cx_alloc_instance(cx_class_named("NSSound")); objc_setAssociatedObject(s, &NSSOUND_KEY, p, 1); objc_release(p); return objc_autorelease(s); }
        }
    return 0;
}
static BOOL snd_play(id s, SEL c) { (void)c; id p = snd_player(s); return p && plr_play(p, 0); }
static BOOL snd_stop(id s, SEL c) { (void)c; id p = snd_player(s); if (p) plr_stop(p, 0); return p != 0; }
static BOOL snd_pause(id s, SEL c) { (void)c; id p = snd_player(s); if (p) plr_pause(p, 0); return p != 0; }
static BOOL snd_resume(id s, SEL c) { return snd_play(s, c); }
static BOOL snd_is_playing(id s, SEL c) { (void)c; id p = snd_player(s); return p && PLR(p)->playing; }
static float snd_volume(id s, SEL c) { (void)c; id p = snd_player(s); return p ? PLR(p)->volume : 0; }
static void snd_set_volume(id s, SEL c, float v) { (void)c; id p = snd_player(s); if (p) plr_set_volume(p, 0, v); }
static double snd_duration(id s, SEL c) { (void)c; id p = snd_player(s); return p ? plr_duration(p, 0) : 0; }
static double snd_current(id s, SEL c) { (void)c; id p = snd_player(s); return p ? plr_current(p, 0) : 0; }
static void snd_set_current(id s, SEL c, double t) { (void)c; id p = snd_player(s); if (p) plr_set_current(p, 0, t); }
static BOOL snd_loops(id s, SEL c) { (void)c; id p = snd_player(s); return p && PLR(p)->loops != 0; }
static void snd_set_loops(id s, SEL c, BOOL l) { (void)c; id p = snd_player(s); if (p) plr_set_loops(p, 0, l ? -1 : 0); }
static BOOL snd_set_name(id s, SEL c, id n) { (void)s; (void)c; (void)n; return YES; }
/* o delegate do NSSound recebe sound:didFinishPlaying: (o AVAudioPlayer interno usa um objeto ponte) */
cx_class_t MOSSoundBridge_c __asm__("OBJC_CLASS_$_MOSSoundBridge"), MOSSoundBridge_m __asm__("OBJC_METACLASS_$_MOSSoundBridge");
typedef struct { cx_class_t *isa; id sound, delegate; } sbridge_t;
static void sb_finished(id s, SEL c, id plr, BOOL ok) {
    (void)c; (void)plr;
    sbridge_t *b = (sbridge_t *)s;
    if (b->delegate && resp(b->delegate, "sound:didFinishPlaying:")) MSG(void, b->delegate, "sound:didFinishPlaying:", b->sound, ok);
}
static void snd_set_delegate(id s, SEL c, id d) {
    (void)c;
    id p = snd_player(s);
    if (!p) return;
    sbridge_t *b = (sbridge_t *)cx_alloc_instance(&MOSSoundBridge_c);
    b->sound = s;
    b->delegate = d;
    objc_setAssociatedObject(s, "bridge", (id)b, 1);
    objc_release((id)b);
    plr_set_delegate(p, 0, (id)b);
}
static id snd_delegate(id s, SEL c) { (void)c; id b = objc_getAssociatedObject(s, "bridge"); return b ? ((sbridge_t *)b)->delegate : 0; }

/* ================================================================ registro das classes */
static void K(cx_class_t *k, cx_class_t *m, const char *name, cx_class_t *super, u32 extra) {
    if (!cx_class_named(name)) cx_define_class(k, m, name, super, extra);
}
#define M(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define CM(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))
#define S(x) objc_retain(cx_str(x))

void cx_avfaudio_init(void) {
    cx_class_t *obj = cx_class_named("NSObject");
    K(&AVAudioFormat_c, &AVAudioFormat_m, "AVAudioFormat", obj, sizeof(avfmt_t) - 8);
    M(AVAudioFormat, "initStandardFormatWithSampleRate:channels:", fmt_init_std);
    M(AVAudioFormat, "initStandardFormatWithSampleRate:channelLayout:", fmt_init_std_layout);
    M(AVAudioFormat, "initWithCommonFormat:sampleRate:channels:interleaved:", fmt_init_common);
    M(AVAudioFormat, "initWithStreamDescription:", fmt_init_asbd);
    M(AVAudioFormat, "initWithStreamDescription:channelLayout:", fmt_init_asbd_layout);
    M(AVAudioFormat, "initWithSettings:", fmt_init_settings);
    M(AVAudioFormat, "sampleRate", fmt_rate); M(AVAudioFormat, "channelCount", fmt_channels);
    M(AVAudioFormat, "commonFormat", fmt_common); M(AVAudioFormat, "isInterleaved", fmt_inter);
    M(AVAudioFormat, "isStandard", fmt_standard); M(AVAudioFormat, "streamDescription", fmt_asbd);
    M(AVAudioFormat, "settings", fmt_settings); M(AVAudioFormat, "isEqual:", fmt_equal);
    M(AVAudioFormat, "channelLayout", fmt_layout); M(AVAudioFormat, "copyWithZone:", fmt_copy);

    K(&AVAudioBuffer_c, &AVAudioBuffer_m, "AVAudioBuffer", obj, sizeof(avbuf_t) - 8);
    K(&AVAudioPCMBuffer_c, &AVAudioPCMBuffer_m, "AVAudioPCMBuffer", &AVAudioBuffer_c, 0);
    M(AVAudioPCMBuffer, "initWithPCMFormat:frameCapacity:", buf_init);
    M(AVAudioPCMBuffer, "frameCapacity", buf_cap); M(AVAudioPCMBuffer, "frameLength", buf_len);
    M(AVAudioPCMBuffer, "setFrameLength:", buf_set_len); M(AVAudioPCMBuffer, "floatChannelData", buf_float);
    M(AVAudioPCMBuffer, "int16ChannelData", buf_i16); M(AVAudioPCMBuffer, "int32ChannelData", buf_i32);
    M(AVAudioPCMBuffer, "stride", buf_stride); M(AVAudioBuffer, "format", buf_format);
    M(AVAudioBuffer, "audioBufferList", buf_abl); M(AVAudioBuffer, "mutableAudioBufferList", buf_abl);
    M(AVAudioBuffer, "dealloc", buf_dealloc);

    K(&AVAudioTime_c, &AVAudioTime_m, "AVAudioTime", obj, sizeof(avtime_t) - 8);
    M(AVAudioTime, "initWithSampleTime:atRate:", time_init_st); M(AVAudioTime, "initWithHostTime:", time_init_host);
    M(AVAudioTime, "initWithHostTime:sampleTime:atRate:", time_init_host_st);
    CM(AVAudioTime, "timeWithSampleTime:atRate:", time_with_st); CM(AVAudioTime, "timeWithHostTime:", time_with_host);
    CM(AVAudioTime, "hostTimeForSeconds:", time_host_for_seconds); CM(AVAudioTime, "secondsForHostTime:", time_seconds_for_host);
    M(AVAudioTime, "sampleTime", time_st); M(AVAudioTime, "sampleRate", time_rate); M(AVAudioTime, "hostTime", time_host);
    M(AVAudioTime, "isSampleTimeValid", time_svalid); M(AVAudioTime, "isHostTimeValid", time_hvalid); M(AVAudioTime, "audioTimeStamp", time_ats);

    K(&AVAudioFile_c, &AVAudioFile_m, "AVAudioFile", obj, sizeof(avfile_t) - 8);
    M(AVAudioFile, "initForReading:error:", file_init_read);
    M(AVAudioFile, "initForReading:commonFormat:interleaved:error:", file_init_read_common);
    M(AVAudioFile, "initForWriting:settings:error:", file_init_write);
    M(AVAudioFile, "initForWriting:settings:commonFormat:interleaved:error:", file_init_write_common);
    M(AVAudioFile, "length", file_length); M(AVAudioFile, "framePosition", file_pos); M(AVAudioFile, "setFramePosition:", file_set_pos);
    M(AVAudioFile, "fileFormat", file_ffmt); M(AVAudioFile, "processingFormat", file_pfmt); M(AVAudioFile, "url", file_url);
    M(AVAudioFile, "isOpen", file_is_open);
    M(AVAudioFile, "readIntoBuffer:error:", file_read); M(AVAudioFile, "readIntoBuffer:frameCount:error:", file_read_n);
    M(AVAudioFile, "writeFromBuffer:error:", file_write); M(AVAudioFile, "close", file_close); M(AVAudioFile, "dealloc", file_dealloc);

    K(&AVAudioNode_c, &AVAudioNode_m, "AVAudioNode", obj, sizeof(avnode_t) - 8);
    K(&AVAudioIONode_c, &AVAudioIONode_m, "AVAudioIONode", &AVAudioNode_c, 0);
    K(&AVAudioMixerNode_c, &AVAudioMixerNode_m, "AVAudioMixerNode", &AVAudioNode_c, 0);
    K(&AVAudioOutputNode_c, &AVAudioOutputNode_m, "AVAudioOutputNode", &AVAudioIONode_c, 0);
    K(&AVAudioInputNode_c, &AVAudioInputNode_m, "AVAudioInputNode", &AVAudioIONode_c, 0);
    K(&AVAudioPlayerNode_c, &AVAudioPlayerNode_m, "AVAudioPlayerNode", &AVAudioNode_c, 0);
    K(&AVAudioSourceNode_c, &AVAudioSourceNode_m, "AVAudioSourceNode", &AVAudioNode_c, 0);
    K(&AVAudioSinkNode_c, &AVAudioSinkNode_m, "AVAudioSinkNode", &AVAudioNode_c, 0);
    K(&AVAudioUnit_c, &AVAudioUnit_m, "AVAudioUnit", &AVAudioNode_c, 0);
    K(&AVAudioUnitEffect_c, &AVAudioUnitEffect_m, "AVAudioUnitEffect", &AVAudioUnit_c, 0);
    K(&AVAudioUnitEQ_c, &AVAudioUnitEQ_m, "AVAudioUnitEQ", &AVAudioUnitEffect_c, 0);
    K(&AVAudioUnitReverb_c, &AVAudioUnitReverb_m, "AVAudioUnitReverb", &AVAudioUnitEffect_c, 0);
    K(&AVAudioUnitDelay_c, &AVAudioUnitDelay_m, "AVAudioUnitDelay", &AVAudioUnitEffect_c, 0);
    K(&AVAudioUnitDistortion_c, &AVAudioUnitDistortion_m, "AVAudioUnitDistortion", &AVAudioUnitEffect_c, 0);
    K(&AVAudioUnitTimeEffect_c, &AVAudioUnitTimeEffect_m, "AVAudioUnitTimeEffect", &AVAudioUnit_c, 0);
    K(&AVAudioUnitTimePitch_c, &AVAudioUnitTimePitch_m, "AVAudioUnitTimePitch", &AVAudioUnitTimeEffect_c, 0);
    K(&AVAudioUnitVarispeed_c, &AVAudioUnitVarispeed_m, "AVAudioUnitVarispeed", &AVAudioUnitTimeEffect_c, 0);
    M(AVAudioNode, "init", node_init);
    M(AVAudioPlayerNode, "init", player_init); M(AVAudioMixerNode, "init", mixer_init);
    M(AVAudioNode, "engine", node_engine); M(AVAudioNode, "numberOfInputs", node_ninputs); M(AVAudioNode, "numberOfOutputs", node_noutputs);
    M(AVAudioNode, "outputFormatForBus:", node_out_fmt); M(AVAudioNode, "inputFormatForBus:", node_in_fmt);
    M(AVAudioNode, "nameForInputBus:", node_name_for); M(AVAudioNode, "nameForOutputBus:", node_name_for);
    M(AVAudioNode, "lastRenderTime", node_last_time);
    M(AVAudioNode, "installTapOnBus:bufferSize:format:block:", node_install_tap); M(AVAudioNode, "removeTapOnBus:", node_remove_tap);
    M(AVAudioNode, "reset", node_reset); M(AVAudioNode, "latency", node_latency); M(AVAudioNode, "outputPresentationLatency", node_out_latency);
    M(AVAudioNode, "volume", node_volume); M(AVAudioNode, "setVolume:", node_set_volume);
    M(AVAudioNode, "pan", node_pan); M(AVAudioNode, "setPan:", node_set_pan);
    M(AVAudioMixerNode, "outputVolume", mixer_outvol); M(AVAudioMixerNode, "setOutputVolume:", mixer_set_outvol);
    M(AVAudioMixerNode, "nextAvailableInputBus", mixer_next_bus);
    M(AVAudioIONode, "presentationLatency", io_presentation_latency); M(AVAudioIONode, "audioUnit", io_audio_unit);
    M(AVAudioIONode, "isVoiceProcessingEnabled", io_voice_enabled); M(AVAudioIONode, "setVoiceProcessingEnabled:error:", io_set_voice);
    M(AVAudioUnitEffect, "bypass", fx_bypass); M(AVAudioUnitEffect, "setBypass:", fx_set_bypass);
    M(AVAudioUnitTimeEffect, "bypass", fx_bypass); M(AVAudioUnitTimeEffect, "setBypass:", fx_set_bypass);
    M(AVAudioUnitEQ, "initWithNumberOfBands:", fx_init_bands); M(AVAudioUnitEQ, "bands", fx_bands);
    M(AVAudioUnitEQ, "globalGain", fx_param); M(AVAudioUnitEQ, "setGlobalGain:", fx_set_param);
    M(AVAudioUnitReverb, "wetDryMix", fx_param); M(AVAudioUnitReverb, "setWetDryMix:", fx_set_param); M(AVAudioUnitReverb, "loadFactoryPreset:", fx_load_preset);
    M(AVAudioUnitDelay, "wetDryMix", fx_param); M(AVAudioUnitDelay, "setWetDryMix:", fx_set_param);
    M(AVAudioUnitDelay, "delayTime", fx_param); M(AVAudioUnitDelay, "setDelayTime:", fx_set_param);
    M(AVAudioUnitDelay, "feedback", fx_param); M(AVAudioUnitDelay, "setFeedback:", fx_set_param);
    M(AVAudioUnitDelay, "lowPassCutoff", fx_param); M(AVAudioUnitDelay, "setLowPassCutoff:", fx_set_param);
    M(AVAudioUnitDistortion, "wetDryMix", fx_param); M(AVAudioUnitDistortion, "setWetDryMix:", fx_set_param);
    M(AVAudioUnitDistortion, "preGain", fx_param); M(AVAudioUnitDistortion, "setPreGain:", fx_set_param); M(AVAudioUnitDistortion, "loadFactoryPreset:", fx_load_preset);
    M(AVAudioUnitTimePitch, "rate", fx_param); M(AVAudioUnitTimePitch, "setRate:", fx_set_param);
    M(AVAudioUnitTimePitch, "pitch", fx_param); M(AVAudioUnitTimePitch, "setPitch:", fx_set_param);
    M(AVAudioUnitTimePitch, "overlap", fx_param); M(AVAudioUnitTimePitch, "setOverlap:", fx_set_param);
    M(AVAudioUnitVarispeed, "rate", fx_param); M(AVAudioUnitVarispeed, "setRate:", fx_set_param);
    M(AVAudioPlayerNode, "scheduleBuffer:completionHandler:", pn_sched_buf);
    M(AVAudioPlayerNode, "scheduleBuffer:atTime:options:completionHandler:", pn_sched_buf_opts);
    M(AVAudioPlayerNode, "scheduleBuffer:atTime:options:completionCallbackType:completionHandler:", pn_sched_buf_type);
    M(AVAudioPlayerNode, "scheduleBuffer:completionCallbackType:completionHandler:", pn_sched_buf_type2);
    M(AVAudioPlayerNode, "scheduleFile:atTime:completionHandler:", pn_sched_file);
    M(AVAudioPlayerNode, "scheduleFile:atTime:completionCallbackType:completionHandler:", pn_sched_file_type);
    M(AVAudioPlayerNode, "scheduleSegment:startingFrame:frameCount:atTime:completionHandler:", pn_sched_seg);
    M(AVAudioPlayerNode, "scheduleSegment:startingFrame:frameCount:atTime:completionCallbackType:completionHandler:", pn_sched_seg_type);
    M(AVAudioPlayerNode, "play", pn_play); M(AVAudioPlayerNode, "playAtTime:", pn_play_at);
    M(AVAudioPlayerNode, "pause", pn_pause); M(AVAudioPlayerNode, "stop", pn_stop);
    M(AVAudioPlayerNode, "isPlaying", pn_playing); M(AVAudioPlayerNode, "prepareWithFrameCount:", pn_prepare);
    M(AVAudioPlayerNode, "playerTimeForNodeTime:", pn_player_time); M(AVAudioPlayerNode, "nodeTimeForPlayerTime:", pn_node_time);
    M(AVAudioSourceNode, "initWithRenderBlock:", src_init_block); M(AVAudioSourceNode, "initWithFormat:renderBlock:", src_init_fmt_block);
    M(AVAudioSinkNode, "initWithReceiverBlock:", sink_init_block);

    K(&AVAudioEngine_c, &AVAudioEngine_m, "AVAudioEngine", obj, sizeof(avengine_t) - 8);
    M(AVAudioEngine, "init", eng_init); M(AVAudioEngine, "mainMixerNode", eng_mixer);
    M(AVAudioEngine, "outputNode", eng_output); M(AVAudioEngine, "inputNode", eng_input);
    M(AVAudioEngine, "attachNode:", eng_attach); M(AVAudioEngine, "detachNode:", eng_detach);
    M(AVAudioEngine, "connect:to:format:", eng_connect); M(AVAudioEngine, "connect:to:fromBus:toBus:format:", eng_connect_bus);
    M(AVAudioEngine, "connect:toConnectionPoints:fromBus:format:", eng_connect_points);
    M(AVAudioEngine, "disconnectNodeInput:", eng_disconnect_input); M(AVAudioEngine, "disconnectNodeInput:bus:", eng_disconnect_input_bus);
    M(AVAudioEngine, "disconnectNodeOutput:", eng_disconnect_output); M(AVAudioEngine, "disconnectNodeOutput:bus:", eng_disconnect_output_bus);
    M(AVAudioEngine, "prepare", eng_prepare); M(AVAudioEngine, "startAndReturnError:", eng_start);
    M(AVAudioEngine, "pause", eng_pause); M(AVAudioEngine, "stop", eng_stop); M(AVAudioEngine, "reset", eng_reset);
    M(AVAudioEngine, "isRunning", eng_running); M(AVAudioEngine, "attachedNodes", eng_attached);
    M(AVAudioEngine, "isInManualRenderingMode", eng_manual);
    M(AVAudioEngine, "enableManualRenderingMode:format:maximumFrameCount:error:", eng_enable_manual);
    M(AVAudioEngine, "disableManualRenderingMode", eng_disable_manual);
    M(AVAudioEngine, "inputConnectionPointForNode:inputBus:", eng_input_point);
    M(AVAudioEngine, "dealloc", eng_dealloc);

    K(&AVAudioPlayer_c, &AVAudioPlayer_m, "AVAudioPlayer", obj, sizeof(avplayer_t) - 8);
    M(AVAudioPlayer, "initWithContentsOfURL:error:", plr_init_url);
    M(AVAudioPlayer, "initWithContentsOfURL:fileTypeHint:error:", plr_init_url_hint);
    M(AVAudioPlayer, "initWithData:error:", plr_init_data); M(AVAudioPlayer, "initWithData:fileTypeHint:error:", plr_init_data_hint);
    M(AVAudioPlayer, "prepareToPlay", plr_prepare); M(AVAudioPlayer, "play", plr_play); M(AVAudioPlayer, "playAtTime:", plr_play_at);
    M(AVAudioPlayer, "pause", plr_pause); M(AVAudioPlayer, "stop", plr_stop); M(AVAudioPlayer, "isPlaying", plr_is_playing);
    M(AVAudioPlayer, "volume", plr_volume); M(AVAudioPlayer, "setVolume:", plr_set_volume); M(AVAudioPlayer, "setVolume:fadeDuration:", plr_set_volume_fade);
    M(AVAudioPlayer, "pan", plr_pan); M(AVAudioPlayer, "setPan:", plr_set_pan);
    M(AVAudioPlayer, "enableRate", plr_enable_rate); M(AVAudioPlayer, "setEnableRate:", plr_set_enable_rate);
    M(AVAudioPlayer, "rate", plr_rate); M(AVAudioPlayer, "setRate:", plr_set_rate);
    M(AVAudioPlayer, "numberOfLoops", plr_loops); M(AVAudioPlayer, "setNumberOfLoops:", plr_set_loops);
    M(AVAudioPlayer, "duration", plr_duration); M(AVAudioPlayer, "currentTime", plr_current); M(AVAudioPlayer, "setCurrentTime:", plr_set_current);
    M(AVAudioPlayer, "deviceCurrentTime", plr_device_time); M(AVAudioPlayer, "url", plr_url); M(AVAudioPlayer, "data", plr_data);
    M(AVAudioPlayer, "numberOfChannels", plr_channels); M(AVAudioPlayer, "format", plr_format); M(AVAudioPlayer, "settings", plr_settings);
    M(AVAudioPlayer, "delegate", plr_delegate); M(AVAudioPlayer, "setDelegate:", plr_set_delegate);
    M(AVAudioPlayer, "isMeteringEnabled", plr_metering); M(AVAudioPlayer, "setMeteringEnabled:", plr_set_metering);
    M(AVAudioPlayer, "updateMeters", plr_update_meters);
    M(AVAudioPlayer, "averagePowerForChannel:", plr_avg_power); M(AVAudioPlayer, "peakPowerForChannel:", plr_peak_power);
    M(AVAudioPlayer, "channelAssignments", plr_channel_assign); M(AVAudioPlayer, "dealloc", plr_dealloc);

    /* constantes */
    AVAudioSessionCategoryAmbient = S("AVAudioSessionCategoryAmbient");
    AVAudioSessionCategorySoloAmbient = S("AVAudioSessionCategorySoloAmbient");
    AVAudioSessionCategoryPlayback = S("AVAudioSessionCategoryPlayback");
    AVAudioSessionCategoryRecord = S("AVAudioSessionCategoryRecord");
    AVAudioSessionCategoryPlayAndRecord = S("AVAudioSessionCategoryPlayAndRecord");
    AVAudioSessionCategoryMultiRoute = S("AVAudioSessionCategoryMultiRoute");
    AVAudioSessionCategoryAudioProcessing = S("AVAudioSessionCategoryAudioProcessing");
    AVAudioSessionModeDefault = S("AVAudioSessionModeDefault");
    AVAudioSessionModeVoiceChat = S("AVAudioSessionModeVoiceChat");
    AVAudioSessionModeVideoChat = S("AVAudioSessionModeVideoChat");
    AVAudioSessionModeGameChat = S("AVAudioSessionModeGameChat");
    AVAudioSessionModeVideoRecording = S("AVAudioSessionModeVideoRecording");
    AVAudioSessionModeMeasurement = S("AVAudioSessionModeMeasurement");
    AVAudioSessionModeMoviePlayback = S("AVAudioSessionModeMoviePlayback");
    AVAudioSessionModeSpokenAudio = S("AVAudioSessionModeSpokenAudio");
    AVAudioSessionModeVoicePrompt = S("AVAudioSessionModeVoicePrompt");
    AVAudioSessionInterruptionNotification = S("AVAudioSessionInterruptionNotification");
    AVAudioSessionRouteChangeNotification = S("AVAudioSessionRouteChangeNotification");
    AVAudioSessionMediaServicesWereLostNotification = S("AVAudioSessionMediaServicesWereLostNotification");
    AVAudioSessionMediaServicesWereResetNotification = S("AVAudioSessionMediaServicesWereResetNotification");
    AVAudioSessionSilenceSecondaryAudioHintNotification = S("AVAudioSessionSilenceSecondaryAudioHintNotification");
    AVAudioSessionInterruptionTypeKey = S("AVAudioSessionInterruptionTypeKey");
    AVAudioSessionInterruptionOptionKey = S("AVAudioSessionInterruptionOptionKey");
    AVAudioSessionRouteChangeReasonKey = S("AVAudioSessionRouteChangeReasonKey");
    AVAudioSessionRouteChangePreviousRouteKey = S("AVAudioSessionRouteChangePreviousRouteKey");
    AVAudioSessionPortBuiltInSpeaker = S("Speaker");
    AVAudioSessionPortBuiltInMic = S("MicrophoneBuiltIn");
    AVAudioSessionPortHeadphones = S("Headphones");
    AVAudioSessionPortLineOut = S("LineOut");
    AVAudioSessionPortBluetoothA2DP = S("BluetoothA2DPOutput");
    AVAudioSessionPortHDMI = S("HDMIOutput");
    AVPlayerItemDidPlayToEndTimeNotification = S("AVPlayerItemDidPlayToEndTimeNotification");
    AVPlayerItemFailedToPlayToEndTimeNotification = S("AVPlayerItemFailedToPlayToEndTimeNotification");
    AVPlayerItemTimeJumpedNotification = S("AVPlayerItemTimeJumpedNotification");
    AVFoundationErrorDomain = S("AVFoundationErrorDomain");
    AVAudioEngineConfigurationChangeNotification = S("AVAudioEngineConfigurationChangeNotification");

    K(&AVAudioSession_c, &AVAudioSession_m, "AVAudioSession", obj, sizeof(avsession_t) - 8);
    CM(AVAudioSession, "sharedInstance", ses_shared);
    M(AVAudioSession, "category", ses_category); M(AVAudioSession, "mode", ses_mode); M(AVAudioSession, "categoryOptions", ses_options);
    M(AVAudioSession, "setCategory:error:", ses_set_cat); M(AVAudioSession, "setCategory:withOptions:error:", ses_set_cat_opts);
    M(AVAudioSession, "setCategory:mode:options:error:", ses_set_cat_mode_opts);
    M(AVAudioSession, "setCategory:mode:routeSharingPolicy:options:error:", ses_set_cat_mode_policy);
    M(AVAudioSession, "setMode:error:", ses_set_mode);
    M(AVAudioSession, "setActive:error:", ses_set_active); M(AVAudioSession, "setActive:withOptions:error:", ses_set_active_opts);
    M(AVAudioSession, "activateWithOptions:completionHandler:", ses_activate_opts);
    M(AVAudioSession, "sampleRate", ses_rate); M(AVAudioSession, "preferredSampleRate", ses_pref_rate);
    M(AVAudioSession, "setPreferredSampleRate:error:", ses_set_pref_rate);
    M(AVAudioSession, "IOBufferDuration", ses_io); M(AVAudioSession, "preferredIOBufferDuration", ses_io);
    M(AVAudioSession, "setPreferredIOBufferDuration:error:", ses_set_pref_io);
    M(AVAudioSession, "outputVolume", ses_out_volume); M(AVAudioSession, "outputLatency", ses_out_latency);
    M(AVAudioSession, "inputLatency", ses_in_latency); M(AVAudioSession, "outputNumberOfChannels", ses_out_channels);
    M(AVAudioSession, "inputNumberOfChannels", ses_in_channels); M(AVAudioSession, "maximumOutputNumberOfChannels", ses_out_channels);
    M(AVAudioSession, "isInputAvailable", ses_no); M(AVAudioSession, "isOtherAudioPlaying", ses_no);
    M(AVAudioSession, "secondaryAudioShouldBeSilencedHint", ses_no);
    M(AVAudioSession, "overrideOutputAudioPort:error:", ses_yes_err);
    M(AVAudioSession, "setPreferredInputNumberOfChannels:error:", ses_yes_err);
    M(AVAudioSession, "setPreferredOutputNumberOfChannels:error:", ses_yes_err);
    M(AVAudioSession, "recordPermission", ses_record_perm); M(AVAudioSession, "requestRecordPermission:", ses_request_record);
    M(AVAudioSession, "availableInputs", ses_empty); M(AVAudioSession, "currentRoute", ses_route);
    K(&AVAudioSessionRouteDescription_c, &AVAudioSessionRouteDescription_m, "AVAudioSessionRouteDescription", obj, 0);
    M(AVAudioSessionRouteDescription, "outputs", route_outputs); M(AVAudioSessionRouteDescription, "inputs", ses_empty);
    K(&AVAudioSessionPortDescription_c, &AVAudioSessionPortDescription_m, "AVAudioSessionPortDescription", obj, 0);
    M(AVAudioSessionPortDescription, "portType", port_type); M(AVAudioSessionPortDescription, "portName", port_name);
    M(AVAudioSessionPortDescription, "UID", port_uid); M(AVAudioSessionPortDescription, "channels", ses_empty);

    K(&AVAsset_c, &AVAsset_m, "AVAsset", obj, sizeof(avasset_t) - 8);
    K(&AVURLAsset_c, &AVURLAsset_m, "AVURLAsset", &AVAsset_c, 0);
    CM(AVAsset, "assetWithURL:", asset_with_url); CM(AVURLAsset, "URLAssetWithURL:options:", asset_with_url_opts);
    M(AVURLAsset, "initWithURL:options:", asset_init); M(AVURLAsset, "URL", asset_url);
    M(AVAsset, "duration", asset_duration); M(AVAsset, "isPlayable", asset_playable);
    M(AVAsset, "tracks", asset_tracks); M(AVAsset, "tracksWithMediaType:", asset_tracks_media);
    M(AVAsset, "loadValuesAsynchronouslyForKeys:completionHandler:", asset_load);
    M(AVAsset, "statusOfValueForKey:error:", asset_status_key);
    K(&AVPlayerItem_c, &AVPlayerItem_m, "AVPlayerItem", obj, sizeof(avitem_t) - 8);
    CM(AVPlayerItem, "playerItemWithURL:", item_with_url); CM(AVPlayerItem, "playerItemWithAsset:", item_with_asset);
    M(AVPlayerItem, "initWithURL:", item_init_url); M(AVPlayerItem, "initWithAsset:", item_init_asset);
    M(AVPlayerItem, "asset", item_asset); M(AVPlayerItem, "status", item_status); M(AVPlayerItem, "error", item_error);
    M(AVPlayerItem, "duration", item_duration); M(AVPlayerItem, "currentTime", item_current);
    K(&AVPlayer_c, &AVPlayer_m, "AVPlayer", obj, sizeof(avpl_t) - 8);
    K(&AVQueuePlayer_c, &AVQueuePlayer_m, "AVQueuePlayer", &AVPlayer_c, 0);
    CM(AVPlayer, "playerWithURL:", apl_with_url); CM(AVPlayer, "playerWithPlayerItem:", apl_with_item);
    M(AVPlayer, "init", apl_init); M(AVPlayer, "initWithURL:", apl_init_url); M(AVPlayer, "initWithPlayerItem:", apl_init_item);
    M(AVPlayer, "play", apl_play); M(AVPlayer, "pause", apl_pause); M(AVPlayer, "rate", apl_rate); M(AVPlayer, "setRate:", apl_set_rate);
    M(AVPlayer, "volume", apl_volume); M(AVPlayer, "setVolume:", apl_set_volume); M(AVPlayer, "isMuted", apl_muted); M(AVPlayer, "setMuted:", apl_set_muted);
    M(AVPlayer, "status", apl_status); M(AVPlayer, "error", apl_error); M(AVPlayer, "currentItem", apl_item);
    M(AVPlayer, "timeControlStatus", apl_time_control); M(AVPlayer, "replaceCurrentItemWithPlayerItem:", apl_replace);
    M(AVPlayer, "currentTime", apl_current); M(AVPlayer, "seekToTime:", apl_seek); M(AVPlayer, "seekToTime:completionHandler:", apl_seek_done);
    M(AVPlayer, "seekToTime:toleranceBefore:toleranceAfter:", apl_seek_tol);
    M(AVPlayer, "seekToTime:toleranceBefore:toleranceAfter:completionHandler:", apl_seek_tol_done);
    M(AVPlayer, "addPeriodicTimeObserverForInterval:queue:usingBlock:", apl_add_periodic);
    M(AVPlayer, "addBoundaryTimeObserverForTimes:queue:usingBlock:", apl_add_boundary);
    M(AVPlayer, "removeTimeObserver:", apl_remove_obs);
    K(&MOSAVTimeObserver_c, &MOSAVTimeObserver_m, "MOSAVTimeObserver", obj, sizeof(aplobs_t) - 8);
    M(MOSAVTimeObserver, "fire:", obs_fire);

    /* NSSound: a classe já existe no AppKit do mOS (sem métodos); os métodos tocam por um AVAudioPlayer associado */
    cx_class_t *snd = cx_class_named("NSSound");
    if (snd) {
        cx_add_method(snd, "initWithContentsOfFile:byReference:", (void *)snd_init_file);
        cx_add_method(snd, "initWithContentsOfURL:byReference:", (void *)snd_init_url);
        cx_add_method(snd, "initWithData:", (void *)snd_init_data);
        cx_add_cmethod(snd, "soundNamed:", (void *)snd_named);
        cx_add_method(snd, "play", (void *)snd_play); cx_add_method(snd, "stop", (void *)snd_stop);
        cx_add_method(snd, "pause", (void *)snd_pause); cx_add_method(snd, "resume", (void *)snd_resume);
        cx_add_method(snd, "isPlaying", (void *)snd_is_playing);
        cx_add_method(snd, "volume", (void *)snd_volume); cx_add_method(snd, "setVolume:", (void *)snd_set_volume);
        cx_add_method(snd, "duration", (void *)snd_duration);
        cx_add_method(snd, "currentTime", (void *)snd_current); cx_add_method(snd, "setCurrentTime:", (void *)snd_set_current);
        cx_add_method(snd, "loops", (void *)snd_loops); cx_add_method(snd, "setLoops:", (void *)snd_set_loops);
        cx_add_method(snd, "setName:", (void *)snd_set_name);
        cx_add_method(snd, "delegate", (void *)snd_delegate); cx_add_method(snd, "setDelegate:", (void *)snd_set_delegate);
    }
    K(&MOSSoundBridge_c, &MOSSoundBridge_m, "MOSSoundBridge", obj, sizeof(sbridge_t) - 8);
    M(MOSSoundBridge, "audioPlayerDidFinishPlaying:successfully:", sb_finished);
}
