/*
 * mOS libSystem — CoreAudio, AudioToolbox e AudioUnit de verdade, sobre os streams de áudio do kernel
 * (GUI_DEVICE DEV_AUDIO_*, kernel/dev/sound.c): cada saída é um stream de PCM 48 kHz estéreo i16 somado no mixer do
 * sistema, e o app se ritma pelo quanto ainda falta tocar.
 *
 *  - HAL: um dispositivo de saída (id 2, "Saída do mOS") com o stream 3, as propriedades que os apps leem (nome, UID,
 *    taxa, buffer, formato, latência, volume...) e IOProcs (AudioDeviceCreateIOProcID[WithBlock]/Start/Stop);
 *  - AudioComponent/AudioUnit: as unidades de saída DefaultOutput, SystemOutput, HALOutput, VoiceProcessingIO e o
 *    RemoteIO do iOS/Catalyst, com render callback, formato do cliente (qualquer LPCM), parâmetro de volume;
 *    AudioOutputUnitStart/Stop; o barramento de entrada (elemento != 0) lê o microfone se o TCC conceder;
 *  - AudioQueue de saída (LPCM) e de entrada (microfone, mesma condição de TCC);
 *  - AudioConverter LPCM -> LPCM (formato, canais e taxa);
 *  - AudioFile/ExtAudioFile: WAV, AIFF/AIFC, CAF em LPCM, e MP3/ALAC de 16 bits (audiodec.c);
 *  - AudioServices: sons de sistema a partir de arquivos (WAV/AIFF/CAF), o alerta e a conclusão.
 * AAC e Opus são reconhecidos e devolvem kAudioFileUnsupportedDataFormatError. Sem concessão de
 * kTCCServiceMicrophone (cx_tcc_request, frente de TCC) a entrada devolve erro e não captura.
 * O motor de saída comum (cae_*) também serve ao AVAudioEngine/AVAudioPlayer (avfaudio.c).
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"

typedef int OSStatus;
typedef int bool;
#define true 1
#define false 0
/* o libSystem compila sem -fblocks: um bloco é chamado pelo invoke do layout da ABI (deslocamento 16) */
#define BLK_FN(b) (((void **)(b))[2])
typedef unsigned int u32_t;
id objc_retain(id o);
void objc_release(id o);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *strrchr(const char *s, int c);
int strcasecmp(const char *a, const char *b);
int usleep(unsigned us);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
int pthread_join(unsigned long t, void **r);
int pthread_mutex_init(void *m, const void *a);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);
unsigned long long mach_absolute_time(void);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void *dispatch_get_main_queue(void);
typedef void (*dispatch_function_t)(void *);
void dispatch_async_f(void *q, void *ctx, dispatch_function_t fn);
extern u8 *cx_read_file(const char *path, u64 *len);
int fprintf(void *f, const char *fmt, ...);
extern void *__stderrp;
FILE_ *fopen(const char *p, const char *m);
size_t fwrite(const void *p, size_t s, size_t n, FILE_ *f);
int fclose(FILE_ *f);
int fseek(FILE_ *f, long off, int wh);
double floor(double);
unsigned long pthread_self(void);
int cx_media_tcc(const char *service);
int cx_pcm_s16_resample(const short *src, int nsrc, int src_rate, short *dst, int ndst, int dst_rate, int ch);
typedef struct {
    int kind, status;
    const char *limit;
    int rate, channels, frames;
    short *pcm;
} cx_audiodec_out;
int cx_audiodec(const void *bytes, unsigned long long n, cx_audiodec_out *o);
void cx_audiodec_free(cx_audiodec_out *o);

#define FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
#define OUT_RATE 48000.0
#define CAE_CHUNK 512            /* quadros por render (o kAudioDevicePropertyBufferFrameSize) */
#define CAE_TARGET 2048          /* quadros escritos à frente no kernel (~43 ms): a latência informada */

/* ================================================================ tipos da Apple */
typedef struct {
    double mSampleRate;
    u32 mFormatID, mFormatFlags, mBytesPerPacket, mFramesPerPacket, mBytesPerFrame, mChannelsPerFrame, mBitsPerChannel, mReserved;
} asbd_t;
typedef struct { u32 mNumberChannels, mDataByteSize; void *mData; } abuf_t;
typedef struct { u32 mNumberBuffers; u32 pad; abuf_t mBuffers[1]; } ablist_t;   /* mBuffers no deslocamento 8 */
typedef struct { u32 mSelector, mScope, mElement; } paddr_t;
typedef struct {
    double mSampleTime;
    u64 mHostTime;
    double mRateScalar;
    u64 mWordClockTime;
    u8 mSMPTETime[24];
    u32 mFlags, mReserved;
} ats_t;
typedef struct { u32 componentType, componentSubType, componentManufacturer, componentFlags, componentFlagsMask; } acdesc_t;
typedef struct { double mMinimum, mMaximum; } avrange_t;
typedef struct { i64 mStartOffset; u32 mVariableFramesInPacket, mDataByteSize; } aspd_t;

#define kAudioFormatLinearPCM FCC('l', 'p', 'c', 'm')
#define kFmtFloat 1u
#define kFmtBigEndian 2u
#define kFmtSignedInt 4u
#define kFmtPacked 8u
#define kFmtNonInterleaved 32u
#define kAudio_ParamError (-50)
#define kAudioHardwareNoError 0
#define kAudioHardwareUnknownPropertyError FCC('w', 'h', 'o', '?')
#define kAudioHardwareBadPropertySizeError FCC('!', 's', 'i', 'z')
#define kAudioHardwareBadObjectError FCC('!', 'o', 'b', 'j')
#define kAudioHardwareIllegalOperationError FCC('n', 'o', 'p', 'e')
#define kAudioHardwareUnsupportedOperationError FCC('u', 'n', 'o', 'p')
#define kAudioFormatUnsupportedDataFormatError FCC('f', 'm', 't', '?')
#define kAudioConverterErr_FormatNotSupported FCC('f', 'm', 't', '?')
#define kAudioConverterErr_PropertyNotSupported FCC('p', 'r', 'o', 'p')
#define kAudioUnitErr_InvalidProperty (-10879)
#define kAudioUnitErr_InvalidParameter (-10878)
#define kAudioUnitErr_InvalidElement (-10877)
#define kAudioUnitErr_FormatNotSupported (-10868)
#define kAudioUnitErr_Uninitialized (-10867)
#define kAudioUnitErr_InvalidScope (-10866)
#define kAudioUnitErr_PropertyNotWritable (-10865)
#define kAudioFileUnspecifiedError FCC('w', 'h', 't', '?')
#define kAudioFileUnsupportedFileTypeError FCC('t', 'y', 'p', '?')
#define kAudioFileUnsupportedDataFormatError FCC('f', 'm', 't', '?')
#define kAudioFileUnsupportedPropertyError FCC('p', 't', 'y', '?')
#define kAudioFileBadPropertySizeError FCC('!', 's', 'i', 'z')
#define kAudioFileInvalidFileError FCC('d', 't', 'a', '?')
#define kAudioFileNotOpenError (-38)
#define kAudioFileEndOfFileError (-39)
#define kAudioFilePermissionsError (-54)
#define kAudioFileNotFound (-43)

static long dev3(long op, long a, long b, long c) { return SYS4(GUI_DEVICE, op, a, b, c); }

/* ================================================================ conversão de formato */
/* lê o quadro f, canal c de um buffer no formato d (intercalado: um buffer; não intercalado: um buffer por canal) */
static float rd_sample(const asbd_t *d, const ablist_t *l, u32 f, u32 c) {
    u32 bps = d->mBitsPerChannel / 8;
    if (!bps) bps = d->mChannelsPerFrame ? d->mBytesPerFrame / d->mChannelsPerFrame : 0;
    const u8 *p;
    if (d->mFormatFlags & kFmtNonInterleaved) {
        if (c >= l->mNumberBuffers || !l->mBuffers[c].mData) return 0;
        p = (const u8 *)l->mBuffers[c].mData + (u64)f * d->mBytesPerFrame;
        if ((u64)(f + 1) * d->mBytesPerFrame > l->mBuffers[c].mDataByteSize) return 0;
    } else {
        if (!l->mNumberBuffers || !l->mBuffers[0].mData) return 0;
        u64 off = (u64)f * d->mBytesPerFrame + (u64)c * bps;
        if (off + bps > l->mBuffers[0].mDataByteSize) return 0;
        p = (const u8 *)l->mBuffers[0].mData + off;
    }
    bool be = d->mFormatFlags & kFmtBigEndian;
    u8 b[8];
    for (u32 i = 0; i < bps && i < 8; i++) b[i] = be ? p[bps - 1 - i] : p[i];
    if (d->mFormatFlags & kFmtFloat) {
        if (bps == 4) { float v; memcpy(&v, b, 4); return v; }
        if (bps == 8) { double v; memcpy(&v, b, 8); return (float)v; }
        return 0;
    }
    bool sgn = (d->mFormatFlags & kFmtSignedInt) != 0;
    switch (bps) {
    case 1: return sgn ? (float)(i8)b[0] / 128.0f : ((float)b[0] - 128.0f) / 128.0f;
    case 2: { i16 v = (i16)(b[0] | b[1] << 8); return (float)v / 32768.0f; }
    case 3: { i32 v = (i32)((u32)b[0] << 8 | (u32)b[1] << 16 | (u32)b[2] << 24) >> 8; return (float)v / 8388608.0f; }
    case 4: { i32 v = (i32)((u32)b[0] | (u32)b[1] << 8 | (u32)b[2] << 16 | (u32)b[3] << 24);
              /* 8.24 em ponto fixo (o "canonical" antigo do AudioUnit) quando o app diz 32 bits sem empacotar alto */
              return (float)((double)v / 2147483648.0); }
    }
    return 0;
}

static void wr_sample(const asbd_t *d, ablist_t *l, u32 f, u32 c, float v) {
    u32 bps = d->mBitsPerChannel / 8;
    if (!bps) return;
    u8 *p;
    if (d->mFormatFlags & kFmtNonInterleaved) {
        if (c >= l->mNumberBuffers || !l->mBuffers[c].mData) return;
        if ((u64)(f + 1) * d->mBytesPerFrame > l->mBuffers[c].mDataByteSize) return;
        p = (u8 *)l->mBuffers[c].mData + (u64)f * d->mBytesPerFrame;
    } else {
        u64 off = (u64)f * d->mBytesPerFrame + (u64)c * bps;
        if (!l->mNumberBuffers || off + bps > l->mBuffers[0].mDataByteSize) return;
        p = (u8 *)l->mBuffers[0].mData + off;
    }
    u8 b[8] = {0};
    if (d->mFormatFlags & kFmtFloat) {
        if (bps == 4) memcpy(b, &v, 4);
        else if (bps == 8) { double x = v; memcpy(b, &x, 8); }
    } else {
        if (v > 1) v = 1;
        if (v < -1) v = -1;
        bool sgn = (d->mFormatFlags & kFmtSignedInt) != 0;
        if (bps == 1) b[0] = sgn ? (u8)(i8)(v * 127.0f) : (u8)(v * 127.0f + 128.0f);
        else if (bps == 2) { i16 x = (i16)(v * 32767.0f); b[0] = (u8)x; b[1] = (u8)(x >> 8); }
        else if (bps == 3) { i32 x = (i32)(v * 8388607.0f); b[0] = (u8)x; b[1] = (u8)(x >> 8); b[2] = (u8)(x >> 16); }
        else if (bps == 4) { i32 x = (i32)((double)v * 2147483647.0); b[0] = (u8)x; b[1] = (u8)(x >> 8); b[2] = (u8)(x >> 16); b[3] = (u8)(x >> 24); }
    }
    bool be = d->mFormatFlags & kFmtBigEndian;
    for (u32 i = 0; i < bps; i++) p[i] = be ? b[bps - 1 - i] : b[i];
}

/* o formato é LPCM que o mOS sabe ler? */
static bool lpcm_ok(const asbd_t *d) {
    if (!d || d->mFormatID != kAudioFormatLinearPCM) return false;
    if (d->mChannelsPerFrame < 1 || d->mChannelsPerFrame > 32 || d->mSampleRate < 1000 || d->mSampleRate > 768000) return false;
    u32 b = d->mBitsPerChannel;
    if (d->mFormatFlags & kFmtFloat) return b == 32 || b == 64;
    return b == 8 || b == 16 || b == 24 || b == 32;
}

/* preenche os campos derivados (bytes por quadro/pacote) de um ASBD LPCM incompleto */
static void lpcm_fix(asbd_t *d) {
    if (d->mFormatID != kAudioFormatLinearPCM) return;
    u32 bps = d->mBitsPerChannel / 8;
    if (!d->mFramesPerPacket) d->mFramesPerPacket = 1;
    if (!d->mBytesPerFrame) d->mBytesPerFrame = (d->mFormatFlags & kFmtNonInterleaved) ? bps : bps * d->mChannelsPerFrame;
    if (!d->mBytesPerPacket) d->mBytesPerPacket = d->mBytesPerFrame * d->mFramesPerPacket;
}

/* n quadros do formato d -> estéreo float intercalado (mono duplicado; mais de dois canais: os dois primeiros) */
static void to_stereo(const asbd_t *d, const ablist_t *l, u32 n, float *out) {
    u32 ch = d->mChannelsPerFrame;
    /* atalhos para os formatos comuns */
    if (!(d->mFormatFlags & kFmtNonInterleaved) && (d->mFormatFlags & kFmtFloat) && d->mBitsPerChannel == 32 && !(d->mFormatFlags & kFmtBigEndian) &&
        l->mNumberBuffers && l->mBuffers[0].mData) {
        const float *s = l->mBuffers[0].mData;
        u32 have = l->mBuffers[0].mDataByteSize / (4 * ch);
        for (u32 i = 0; i < n; i++) {
            if (i >= have) { out[i * 2] = out[i * 2 + 1] = 0; continue; }
            out[i * 2] = s[i * ch];
            out[i * 2 + 1] = ch > 1 ? s[i * ch + 1] : s[i * ch];
        }
        return;
    }
    if ((d->mFormatFlags & kFmtNonInterleaved) && (d->mFormatFlags & kFmtFloat) && d->mBitsPerChannel == 32 && !(d->mFormatFlags & kFmtBigEndian)) {
        const float *L = l->mNumberBuffers > 0 ? l->mBuffers[0].mData : 0, *R = l->mNumberBuffers > 1 ? l->mBuffers[1].mData : L;
        u32 hl = L ? l->mBuffers[0].mDataByteSize / 4 : 0, hr = R && l->mNumberBuffers > 1 ? l->mBuffers[1].mDataByteSize / 4 : hl;
        for (u32 i = 0; i < n; i++) { out[i * 2] = L && i < hl ? L[i] : 0; out[i * 2 + 1] = R && i < hr ? R[i] : 0; }
        return;
    }
    for (u32 i = 0; i < n; i++) {
        float a = rd_sample(d, l, i, 0);
        out[i * 2] = a;
        out[i * 2 + 1] = ch > 1 ? rd_sample(d, l, i, 1) : a;
    }
}

/* estéreo float -> n quadros do formato d (mono = média; canais extras ficam em silêncio) */
static void from_stereo(const asbd_t *d, ablist_t *l, u32 n, const float *in) {
    u32 ch = d->mChannelsPerFrame;
    for (u32 i = 0; i < n; i++)
        for (u32 c = 0; c < ch; c++) {
            float v = ch == 1 ? (in[i * 2] + in[i * 2 + 1]) * 0.5f : c < 2 ? in[i * 2 + c] : 0;
            wr_sample(d, l, i, c, v);
        }
}

/* para o avfaudio.c: n quadros a partir de 'from' de um AudioBufferList no formato d -> estéreo float; e o inverso */
void cx_buf_to_stereo(const void *d_, const void *l_, u32 from, u32 n, float *out) {
    const asbd_t *d = d_;
    const ablist_t *l = l_;
    u32 nb = l->mNumberBuffers;
    u8 mem[8 + 16 * 32];
    ablist_t *v = (ablist_t *)mem;
    if (nb > 32) nb = 32;
    v->mNumberBuffers = nb;
    for (u32 i = 0; i < nb; i++) {
        v->mBuffers[i] = l->mBuffers[i];
        u64 off = (u64)from * d->mBytesPerFrame;
        if (off > v->mBuffers[i].mDataByteSize) off = v->mBuffers[i].mDataByteSize;
        v->mBuffers[i].mData = (u8 *)l->mBuffers[i].mData + off;
        v->mBuffers[i].mDataByteSize -= (u32)off;
    }
    to_stereo(d, v, n, out);
}
void cx_stereo_to_buf(const void *d_, void *l_, u32 from, u32 n, const float *in) {
    const asbd_t *d = d_;
    ablist_t *l = l_;
    u32 nb = l->mNumberBuffers;
    u8 mem[8 + 16 * 32];
    ablist_t *v = (ablist_t *)mem;
    if (nb > 32) nb = 32;
    v->mNumberBuffers = nb;
    for (u32 i = 0; i < nb; i++) {
        v->mBuffers[i] = l->mBuffers[i];
        u64 off = (u64)from * d->mBytesPerFrame;
        if (off > v->mBuffers[i].mDataByteSize) off = v->mBuffers[i].mDataByteSize;
        v->mBuffers[i].mData = (u8 *)l->mBuffers[i].mData + off;
        v->mBuffers[i].mDataByteSize -= (u32)off;
    }
    from_stereo(d, v, n, in);
}
/* lê/escreve uma amostra (para AVAudioFile com mais de dois canais) */
float cx_buf_sample(const void *d, const void *l, u32 f, u32 c) { return rd_sample(d, l, f, c); }
void cx_buf_set_sample(const void *d, void *l, u32 f, u32 c, float v) { wr_sample(d, l, f, c, v); }

/* ---- reamostragem linear em fluxo (estéreo float): a posição fracionária e a última amostra atravessam os pedaços */
typedef struct { double step, frac; float last[2]; } rs_t;
static void rs_init(rs_t *r, double src_rate, double dst_rate) { r->step = src_rate / dst_rate; r->frac = 1.0; r->last[0] = r->last[1] = 0; }
/* consome n quadros de entrada e escreve até cap quadros; devolve os escritos */
static u32 rs_run(rs_t *r, const float *in, u32 n, float *out, u32 cap) {
    u32 o = 0;
    if (r->step == 1.0) {   /* mesma taxa: cópia direta */
        u32 k = n < cap ? n : cap;
        memcpy(out, in, (size_t)k * 8);
        if (n) { r->last[0] = in[(n - 1) * 2]; r->last[1] = in[(n - 1) * 2 + 1]; }
        return k;
    }
    while (o < cap) {
        u32 i = (u32)r->frac;   /* entre a amostra i-1 (i = 0: a última do pedaço anterior) e a i */
        if (i >= n) break;
        double t = r->frac - i;
        float a0 = i ? in[(i - 1) * 2] : r->last[0], a1 = i ? in[(i - 1) * 2 + 1] : r->last[1];
        out[o * 2] = a0 + (in[i * 2] - a0) * (float)t;
        out[o * 2 + 1] = a1 + (in[i * 2 + 1] - a1) * (float)t;
        o++;
        r->frac += r->step;
    }
    r->frac -= n;
    if (n) { r->last[0] = in[(n - 1) * 2]; r->last[1] = in[(n - 1) * 2 + 1]; }
    return o;
}

/* ================================================================ motor de saída (um stream do kernel por saída) */
typedef struct cae cae_t;
typedef void (*cae_render_fn)(cae_t *e, float *stereo, u32 frames, const ats_t *ts);
struct cae {
    int stream;
    volatile int run;
    unsigned long th;
    double rate;                 /* taxa em que o render produz */
    u32 chunk;                   /* quadros por render, nessa taxa */
    cae_render_fn render;
    void *ctx;
    rs_t rs;
    u64 rendered;                /* quadros produzidos (taxa do render) */
    u64 played0;                 /* played do kernel na partida */
    volatile float gain;
    volatile int paused;
};

static u64 g_host_per_sec;
static u64 host_now(void) { return mach_absolute_time(); }
static double host_rate(void) {
    if (!g_host_per_sec) {
        /* mach_absolute_time do mOS conta nanossegundos (timebase 1/1) */
        g_host_per_sec = 1000000000ull;
    }
    return (double)g_host_per_sec;
}

static void *cae_thread(void *arg) {
    cae_t *e = arg;
    u32 maxsrc = e->chunk ? e->chunk : CAE_CHUNK;
    float *src = calloc((size_t)maxsrc * 2 + 8, sizeof(float));
    u32 outcap = (u32)((double)maxsrc * OUT_RATE / e->rate) + 16;
    float *o48 = calloc((size_t)outcap * 2, sizeof(float));
    i16 *pcm = calloc((size_t)outcap * 2, sizeof(i16));
    if (!src || !o48 || !pcm) { free(src); free(o48); free(pcm); e->run = 0; return 0; }
    while (e->run) {
        gui_audio_state_t st;
        if (dev3(DEV_AUDIO_STATE, e->stream, (long)&st, 0) < 0) break;
        if (e->paused || st.queued >= CAE_TARGET) { usleep(3000); continue; }
        ats_t ts;
        memset(&ts, 0, sizeof ts);
        ts.mSampleTime = (double)e->rendered;
        ts.mHostTime = host_now() + (u64)((double)st.queued / OUT_RATE * host_rate());
        ts.mRateScalar = 1.0;
        ts.mFlags = 1 | 2 | 4;   /* SampleTimeValid | HostTimeValid | RateScalarValid */
        memset(src, 0, (size_t)maxsrc * 8);
        e->render(e, src, maxsrc, &ts);
        e->rendered += maxsrc;
        u32 k = rs_run(&e->rs, src, maxsrc, o48, outcap);
        float g = e->gain;
        for (u32 i = 0; i < k * 2; i++) {
            float v = o48[i] * g;
            if (v > 1) v = 1;
            if (v < -1) v = -1;
            pcm[i] = (i16)(v * 32767.0f);
        }
        for (u32 off = 0; off < k && e->run;) {
            long w = dev3(DEV_AUDIO_WRITE, e->stream, (long)(pcm + off * 2), k - off);
            if (w < 0) { e->run = 0; break; }
            if (w == 0) { usleep(2000); continue; }
            off += (u32)w;
        }
    }
    free(src); free(o48); free(pcm);
    return 0;
}

static cae_t *cae_new(double rate, u32 chunk, cae_render_fn fn, void *ctx) {
    cae_t *e = calloc(1, sizeof *e);
    if (!e) return 0;
    e->stream = -1;
    e->rate = rate > 0 ? rate : OUT_RATE;
    e->chunk = chunk ? chunk : (u32)(CAE_CHUNK * e->rate / OUT_RATE + 0.5);
    if (e->chunk < 16) e->chunk = 16;
    if (e->chunk > 8192) e->chunk = 8192;
    e->render = fn;
    e->ctx = ctx;
    e->gain = 1.0f;
    rs_init(&e->rs, e->rate, OUT_RATE);
    return e;
}

static int cae_start(cae_t *e) {
    if (!e) return -1;
    if (e->run) { e->paused = 0; return 0; }
    if (e->stream < 0) {
        long s = dev3(DEV_AUDIO_OPEN, 0, 0, 0);
        if (s < 0) return -1;
        e->stream = (int)s;
        gui_audio_state_t st;
        if (dev3(DEV_AUDIO_STATE, e->stream, (long)&st, 0) == 0) e->played0 = st.played;
    }
    e->paused = 0;
    e->run = 1;
    if (pthread_create(&e->th, 0, cae_thread, e) != 0) { e->run = 0; return -1; }
    return 0;
}

/* para a thread; flush descarta o que já foi escrito e ainda não tocou */
static void cae_stop(cae_t *e, bool flush) {
    if (!e) return;
    if (e->run) {
        e->run = 0;
        pthread_join(e->th, 0);
    }
    if (e->stream >= 0 && flush) dev3(DEV_AUDIO_CTL, e->stream, (long)((u64)(u32)-1 | (u64)(u32)-1 << 32), 1);
}

static void cae_free(cae_t *e) {
    if (!e) return;
    cae_stop(e, true);
    if (e->stream >= 0) dev3(DEV_AUDIO_CLOSE, e->stream, 0, 0);
    free(e);
}

/* quadros já tocados (na taxa do render) desde a partida */
static double cae_played(cae_t *e) {
    if (!e || e->stream < 0) return 0;
    gui_audio_state_t st;
    if (dev3(DEV_AUDIO_STATE, e->stream, (long)&st, 0) < 0) return 0;
    return (double)(st.played - e->played0) * e->rate / OUT_RATE;
}

/* para o avfaudio.c */
void *cx_cae_new(double rate, u32 chunk, void (*fn)(void *e, float *stereo, u32 frames, const void *ts), void *ctx) {
    return cae_new(rate, chunk, (cae_render_fn)fn, ctx);
}
void *cx_cae_ctx(void *e) { return ((cae_t *)e)->ctx; }
int cx_cae_start(void *e) { return cae_start(e); }
void cx_cae_stop(void *e, int flush) { cae_stop(e, flush != 0); }
void cx_cae_free(void *e) { cae_free(e); }
void cx_cae_pause(void *e, int p) { if (e) ((cae_t *)e)->paused = p; }
double cx_cae_played(void *e) { return cae_played(e); }
void cx_cae_set_gain(void *e, float g) { if (e) ((cae_t *)e)->gain = g; }
int cx_cae_running(void *e) { return e && ((cae_t *)e)->run && !((cae_t *)e)->paused; }
/* muda a taxa de origem em pleno andamento (AVAudioPlayer.rate): só o passo do reamostrador */
void cx_cae_set_rate(void *e_, double src_rate) { cae_t *e = e_; if (e && src_rate > 0) e->rs.step = src_rate / OUT_RATE; }
/* quadros ainda na fila do kernel (latência até o alto-falante), na taxa de 48 kHz */
u32 cx_cae_queued(void *e_) {
    cae_t *e = e_;
    gui_audio_state_t st;
    if (!e || e->stream < 0 || dev3(DEV_AUDIO_STATE, e->stream, (long)&st, 0) < 0) return 0;
    return st.queued;
}

/* ================================================================ HAL: o dispositivo de saída */
#define DEV_SYS 1
#define DEV_OUT 2
#define STREAM_OUT 3
static double g_dev_rate = OUT_RATE;
static u32 g_dev_buf = CAE_CHUNK;
static float g_dev_volume = 1.0f;
static int g_dev_mute;

typedef struct { void *proc; void *block; void *ctx; int started; } ioproc_t;
#define MAX_IOPROCS 16
static ioproc_t g_procs[MAX_IOPROCS];
static cae_t *g_dev_engine;
static u8 g_procs_lock[64];
static int g_lock_init;
static void plock(void) { if (!g_lock_init) { pthread_mutex_init(g_procs_lock, 0); g_lock_init = 1; } pthread_mutex_lock(g_procs_lock); }
static void punlock(void) { pthread_mutex_unlock(g_procs_lock); }

typedef OSStatus (*ioproc_fn)(u32 dev, const ats_t *now, const ablist_t *in, const ats_t *intime, ablist_t *out, const ats_t *outtime, void *ctx);
typedef void (*ioblock_fn)(void *blk, const ats_t *now, const ablist_t *in, const ats_t *intime, ablist_t *out, const ats_t *outtime);

static void dev_render(cae_t *e, float *stereo, u32 frames, const ats_t *ts) {
    float *tmp = calloc((size_t)frames * 2, sizeof(float));
    if (!tmp) return;
    ablist_t out;
    out.mNumberBuffers = 1;
    out.mBuffers[0].mNumberChannels = 2;
    ats_t now = *ts;
    now.mHostTime = host_now();
    plock();
    for (int i = 0; i < MAX_IOPROCS; i++) {
        ioproc_t *p = &g_procs[i];
        if (!p->started) continue;
        memset(tmp, 0, (size_t)frames * 8);
        out.mBuffers[0].mDataByteSize = frames * 8;
        out.mBuffers[0].mData = tmp;
        punlock();
        if (p->block) ((ioblock_fn)BLK_FN(p->block))(p->block, &now, 0, &now, &out, ts);
        else if (p->proc) ((ioproc_fn)p->proc)(DEV_OUT, &now, 0, &now, &out, ts, p->ctx);
        plock();
        for (u32 k = 0; k < frames * 2; k++) stereo[k] += tmp[k];
    }
    punlock();
    if (g_dev_mute) memset(stereo, 0, (size_t)frames * 8);
    free(tmp);
}

static void dev_engine_update(void) {
    int any = 0;
    for (int i = 0; i < MAX_IOPROCS; i++) if (g_procs[i].started) any = 1;
    if (any && !g_dev_engine) {
        g_dev_engine = cae_new(g_dev_rate, g_dev_buf, dev_render, 0);
        if (g_dev_engine) { g_dev_engine->gain = g_dev_volume; cae_start(g_dev_engine); }
    } else if (!any && g_dev_engine) {
        cae_t *e = g_dev_engine;
        g_dev_engine = 0;
        cae_free(e);
    }
}

static void out_asbd(asbd_t *d, double rate) {
    memset(d, 0, sizeof *d);
    d->mSampleRate = rate;
    d->mFormatID = kAudioFormatLinearPCM;
    d->mFormatFlags = kFmtFloat | kFmtPacked;
    d->mBytesPerPacket = 8;
    d->mFramesPerPacket = 1;
    d->mBytesPerFrame = 8;
    d->mChannelsPerFrame = 2;
    d->mBitsPerChannel = 32;
}

static id dev_name_str(void) {
    static char nm[64];
    gui_audio_state_t st;
    memset(&st, 0, sizeof st);
    dev3(DEV_AUDIO_INFO, (long)&st, 0, 0);
    if (!nm[0]) { size_t n = strlen(st.name); memcpy(nm, st.name, n < 63 ? n : 63); }
    return cx_str(nm[0] ? nm : "Saída do mOS");
}

/* tamanho de cada propriedade; 0 = não existe neste objeto */
static u32 prop_size(u32 obj, const paddr_t *a) {
    u32 s = a->mSelector;
    if (s == FCC('c', 'l', 'a', 's') || s == FCC('b', 'c', 'l', 's')) return 4;
    if (obj == DEV_SYS) {
        if (s == FCC('d', 'e', 'v', '#')) return 4;
        if (s == FCC('d', 'O', 'u', 't') || s == FCC('s', 'O', 'u', 't') || s == FCC('d', 'I', 'n', ' ')) return 4;
        if (s == FCC('o', 'w', 'n', 'd')) return 4;
        if (s == FCC('u', 'i', 'd', 'd')) return 4;
        if (s == FCC('l', 'n', 'a', 'm') || s == FCC('l', 'm', 'a', 'k')) return 8;
        if (s == FCC('r', 'n', 'l', 'p')) return 8;
        if (s == FCC('i', 'n', 'o', 't') || s == FCC('s', 'l', 'e', 'p') || s == FCC('u', 'n', 'l', 'd') || s == FCC('h', 'w', 'b', 's') || s == FCC('p', 'm', 'u', 't')) return 4;
        return 0;
    }
    if (obj == DEV_OUT) {
        bool in_scope = a->mScope == FCC('i', 'n', 'p', 't');
        if (s == FCC('l', 'n', 'a', 'm') || s == FCC('l', 'm', 'a', 'k') || s == FCC('l', 'm', 'o', 'd') || s == FCC('u', 'i', 'd', ' ') || s == FCC('m', 'u', 'i', 'd')) return 8;
        if (s == FCC('t', 'r', 'a', 'n') || s == FCC('l', 'i', 'v', 'n') || s == FCC('g', 'o', 'i', 'n') || s == FCC('g', 'o', 'n', 'e') ||
            s == FCC('d', 'f', 'l', 't') || s == FCC('s', 'f', 'l', 't') || s == FCC('l', 't', 'n', 'c') || s == FCC('s', 'a', 'f', 't') ||
            s == FCC('f', 's', 'i', 'z') || s == FCC('h', 'i', 'd', 'n') || s == FCC('c', 'l', 'k', 'd') || s == FCC('o', 'i', 'n', 'k') ||
            s == FCC('v', 'f', 's', 'z') || s == FCC('s', 't', 'p', 'd') || s == FCC('m', 'u', 't', 'e') || s == FCC('v', 'o', 'l', 'm') ||
            s == FCC('i', 'o', 'p', 'x'))
            return 4;
        if (s == FCC('d', 'c', 'h', '2')) return 8;
        if (s == FCC('s', 't', 'm', '#') || s == FCC('o', 'w', 'n', 'd')) return in_scope ? 0 : 4;
        if (s == FCC('a', 'k', 'i', 'n')) return 4;
        if (s == FCC('n', 's', 'r', 't') || s == FCC('a', 's', 'r', 't')) return 8;
        if (s == FCC('n', 's', 'r', '#') || s == FCC('f', 's', 'z', '#')) return 16;
        if (s == FCC('s', 'l', 'a', 'y')) return in_scope ? 8 : 8 + 16;
        if (s == FCC('s', 'f', 'm', 't') || s == FCC('p', 'f', 't', ' ')) return in_scope ? 0 : 40;
        return 0;
    }
    if (obj == STREAM_OUT) {
        if (s == FCC('s', 'f', 'm', 't') || s == FCC('p', 'f', 't', ' ')) return 40;
        if (s == FCC('s', 'f', 'm', 'a') || s == FCC('p', 'f', 't', 'a')) return 56;   /* AudioStreamRangedDescription */
        if (s == FCC('s', 'd', 'i', 'r') || s == FCC('s', 'c', 'h', 'n') || s == FCC('t', 'e', 'r', 'm') || s == FCC('s', 'a', 'c', 't') ||
            s == FCC('l', 't', 'n', 'c'))
            return 4;
        if (s == FCC('l', 'n', 'a', 'm')) return 8;
        return 0;
    }
    return 0;
}

u8 AudioObjectHasProperty(u32 obj, const paddr_t *a) { return a && prop_size(obj, a) != 0; }
OSStatus AudioObjectIsPropertySettable(u32 obj, const paddr_t *a, u8 *out) {
    if (!a) return kAudio_ParamError;
    if (!prop_size(obj, a)) return kAudioHardwareUnknownPropertyError;
    u32 s = a->mSelector;
    if (out) *out = obj == DEV_OUT && (s == FCC('n', 's', 'r', 't') || s == FCC('f', 's', 'i', 'z') || s == FCC('v', 'o', 'l', 'm') || s == FCC('m', 'u', 't', 'e'));
    return 0;
}
OSStatus AudioObjectGetPropertyDataSize(u32 obj, const paddr_t *a, u32 qs, const void *q, u32 *size) {
    (void)qs; (void)q;
    if (!a || !size) return kAudio_ParamError;
    if (obj != DEV_SYS && obj != DEV_OUT && obj != STREAM_OUT) return kAudioHardwareBadObjectError;
    u32 n = prop_size(obj, a);
    if (!n) { *size = 0; return kAudioHardwareUnknownPropertyError; }
    *size = n;
    return 0;
}
#define PUT(T, v) do { if (*size < sizeof(T)) return kAudioHardwareBadPropertySizeError; *(T *)data = (T)(v); *size = sizeof(T); return 0; } while (0)
OSStatus AudioObjectGetPropertyData(u32 obj, const paddr_t *a, u32 qs, const void *q, u32 *size, void *data) {
    if (!a || !size || !data) return kAudio_ParamError;
    if (obj != DEV_SYS && obj != DEV_OUT && obj != STREAM_OUT) return kAudioHardwareBadObjectError;
    if (!prop_size(obj, a)) return kAudioHardwareUnknownPropertyError;
    u32 s = a->mSelector;
    bool in_scope = a->mScope == FCC('i', 'n', 'p', 't');
    if (s == FCC('c', 'l', 'a', 's')) PUT(u32, obj == DEV_SYS ? FCC('a', 's', 'y', 's') : obj == DEV_OUT ? FCC('a', 'd', 'e', 'v') : FCC('a', 's', 't', 'r'));
    if (s == FCC('b', 'c', 'l', 's')) PUT(u32, FCC('a', 'o', 'b', 'j'));
    if (obj == DEV_SYS) {
        if (s == FCC('d', 'e', 'v', '#') || s == FCC('o', 'w', 'n', 'd') || s == FCC('d', 'O', 'u', 't') || s == FCC('s', 'O', 'u', 't')) PUT(u32, DEV_OUT);
        if (s == FCC('d', 'I', 'n', ' ')) PUT(u32, 0);   /* sem dispositivo de entrada (kAudioObjectUnknown) */
        if (s == FCC('u', 'i', 'd', 'd')) {
            /* kAudioHardwarePropertyTranslateUIDToDevice: qualificador = CFString do UID */
            const char *u = q && qs >= 8 ? cx_cstr(*(id *)q) : 0;
            PUT(u32, u && !strcmp(u, "mOS:saida") ? DEV_OUT : 0);
        }
        if (s == FCC('l', 'n', 'a', 'm')) PUT(id, objc_retain(cx_str("Sistema de áudio do mOS")));
        if (s == FCC('l', 'm', 'a', 'k')) PUT(id, objc_retain(cx_str("mOS")));
        if (s == FCC('r', 'n', 'l', 'p')) PUT(void *, 0);
        PUT(u32, 0);
    }
    if (obj == DEV_OUT) {
        if (s == FCC('l', 'n', 'a', 'm') || s == FCC('l', 'm', 'o', 'd')) PUT(id, objc_retain(dev_name_str()));
        if (s == FCC('l', 'm', 'a', 'k')) PUT(id, objc_retain(cx_str("mOS")));
        if (s == FCC('u', 'i', 'd', ' ')) PUT(id, objc_retain(cx_str("mOS:saida")));
        if (s == FCC('m', 'u', 'i', 'd')) PUT(id, objc_retain(cx_str("mOS:saida:modelo")));
        if (s == FCC('t', 'r', 'a', 'n')) PUT(u32, FCC('b', 'l', 't', 'n'));
        if (s == FCC('l', 'i', 'v', 'n') || s == FCC('d', 'f', 'l', 't') || s == FCC('s', 'f', 'l', 't')) PUT(u32, 1);
        if (s == FCC('g', 'o', 'i', 'n') || s == FCC('g', 'o', 'n', 'e')) PUT(u32, g_dev_engine != 0);
        if (s == FCC('l', 't', 'n', 'c')) PUT(u32, CAE_TARGET);
        if (s == FCC('s', 'a', 'f', 't')) PUT(u32, 0);
        if (s == FCC('f', 's', 'i', 'z')) PUT(u32, g_dev_buf);
        if (s == FCC('h', 'i', 'd', 'n') || s == FCC('s', 't', 'p', 'd') || s == FCC('v', 'f', 's', 'z')) PUT(u32, 0);
        if (s == FCC('c', 'l', 'k', 'd')) PUT(u32, 1);
        if (s == FCC('o', 'i', 'n', 'k')) PUT(i32, -1);
        if (s == FCC('i', 'o', 'p', 'x')) PUT(u32, 1);
        if (s == FCC('m', 'u', 't', 'e')) PUT(u32, g_dev_mute);
        if (s == FCC('v', 'o', 'l', 'm')) PUT(float, g_dev_volume);
        if (s == FCC('d', 'c', 'h', '2')) { if (*size < 8) return kAudioHardwareBadPropertySizeError; ((u32 *)data)[0] = 1; ((u32 *)data)[1] = 2; *size = 8; return 0; }
        if (s == FCC('s', 't', 'm', '#') || s == FCC('o', 'w', 'n', 'd')) PUT(u32, STREAM_OUT);
        if (s == FCC('a', 'k', 'i', 'n')) PUT(u32, DEV_OUT);
        if (s == FCC('n', 's', 'r', 't') || s == FCC('a', 's', 'r', 't')) PUT(double, g_dev_rate);
        if (s == FCC('n', 's', 'r', '#')) {
            if (*size < 16) return kAudioHardwareBadPropertySizeError;
            avrange_t *r = data;
            r->mMinimum = 8000; r->mMaximum = 192000;   /* o mOS reamostra para os 48 kHz do mixer */
            *size = 16;
            return 0;
        }
        if (s == FCC('f', 's', 'z', '#')) {
            if (*size < 16) return kAudioHardwareBadPropertySizeError;
            avrange_t *r = data;
            r->mMinimum = 64; r->mMaximum = 4096;
            *size = 16;
            return 0;
        }
        if (s == FCC('s', 'l', 'a', 'y')) {
            u32 need = in_scope ? 8 : 24;
            if (*size < need) return kAudioHardwareBadPropertySizeError;
            ablist_t *l = data;
            memset(l, 0, need);
            l->mNumberBuffers = in_scope ? 0 : 1;
            if (!in_scope) { l->mBuffers[0].mNumberChannels = 2; l->mBuffers[0].mDataByteSize = 0; }
            *size = need;
            return 0;
        }
        if (s == FCC('s', 'f', 'm', 't') || s == FCC('p', 'f', 't', ' ')) {
            if (*size < 40) return kAudioHardwareBadPropertySizeError;
            out_asbd(data, g_dev_rate);
            *size = 40;
            return 0;
        }
    }
    if (obj == STREAM_OUT) {
        if (s == FCC('s', 'f', 'm', 't') || s == FCC('p', 'f', 't', ' ')) { if (*size < 40) return kAudioHardwareBadPropertySizeError; out_asbd(data, g_dev_rate); *size = 40; return 0; }
        if (s == FCC('s', 'f', 'm', 'a') || s == FCC('p', 'f', 't', 'a')) {
            if (*size < 56) return kAudioHardwareBadPropertySizeError;
            out_asbd(data, g_dev_rate);
            avrange_t *r = (avrange_t *)((u8 *)data + 40);
            r->mMinimum = 8000; r->mMaximum = 192000;
            *size = 56;
            return 0;
        }
        if (s == FCC('s', 'd', 'i', 'r')) PUT(u32, 0);
        if (s == FCC('s', 'c', 'h', 'n')) PUT(u32, 1);
        if (s == FCC('t', 'e', 'r', 'm')) PUT(u32, FCC('s', 'p', 'k', 'r'));
        if (s == FCC('s', 'a', 'c', 't')) PUT(u32, 1);
        if (s == FCC('l', 't', 'n', 'c')) PUT(u32, 0);
        if (s == FCC('l', 'n', 'a', 'm')) PUT(id, objc_retain(cx_str("Saída")));
    }
    return kAudioHardwareUnknownPropertyError;
}
OSStatus AudioObjectSetPropertyData(u32 obj, const paddr_t *a, u32 qs, const void *q, u32 size, const void *data) {
    (void)qs; (void)q;
    if (!a || !data) return kAudio_ParamError;
    if (obj != DEV_OUT) return obj == DEV_SYS || obj == STREAM_OUT ? kAudioHardwareUnknownPropertyError : kAudioHardwareBadObjectError;
    u32 s = a->mSelector;
    if (s == FCC('n', 's', 'r', 't') && size >= 8) {
        double r = *(const double *)data;
        if (r < 8000 || r > 192000) return kAudioHardwareIllegalOperationError;
        g_dev_rate = r;   /* o mixer continua em 48 kHz; a reamostragem é do libSystem */
        return 0;
    }
    if (s == FCC('f', 's', 'i', 'z') && size >= 4) {
        u32 b = *(const u32 *)data;
        if (b < 64 || b > 4096) return kAudioHardwareIllegalOperationError;
        g_dev_buf = b;
        return 0;
    }
    if (s == FCC('v', 'o', 'l', 'm') && size >= 4) {
        float v = *(const float *)data;
        g_dev_volume = v < 0 ? 0 : v > 1 ? 1 : v;
        if (g_dev_engine) g_dev_engine->gain = g_dev_volume;
        return 0;
    }
    if (s == FCC('m', 'u', 't', 'e') && size >= 4) { g_dev_mute = *(const u32 *)data != 0; return 0; }
    return kAudioHardwareUnknownPropertyError;
}
OSStatus AudioObjectAddPropertyListener(u32 obj, const void *addr, void *cb, void *ctx) { (void)obj; (void)addr; (void)cb; (void)ctx; return 0; }
OSStatus AudioObjectRemovePropertyListener(u32 obj, const void *addr, void *cb, void *ctx) { (void)obj; (void)addr; (void)cb; (void)ctx; return 0; }
OSStatus AudioObjectAddPropertyListenerBlock(u32 obj, const void *addr, void *q, void *blk) { (void)obj; (void)addr; (void)q; (void)blk; return 0; }
OSStatus AudioObjectRemovePropertyListenerBlock(u32 obj, const void *addr, void *q, void *blk) { (void)obj; (void)addr; (void)q; (void)blk; return 0; }
void AudioObjectShow(u32 obj) { (void)obj; }
OSStatus AudioHardwareCreateAggregateDevice(id desc, u32 *out) { (void)desc; if (out) *out = 0; return kAudioHardwareUnsupportedOperationError; }
OSStatus AudioHardwareDestroyAggregateDevice(u32 d) { (void)d; return kAudioHardwareBadObjectError; }
OSStatus AudioHardwareUnload(void) { return 0; }

static OSStatus ioproc_add(u32 d, void *proc, void *block, void *ctx, void **out) {
    if (d != DEV_OUT) return kAudioHardwareBadObjectError;
    if (!out || (!proc && !block)) return kAudio_ParamError;
    plock();
    for (int i = 0; i < MAX_IOPROCS; i++)
        if (!g_procs[i].proc && !g_procs[i].block) {
            g_procs[i].proc = proc;
            g_procs[i].block = block ? _Block_copy(block) : 0;
            g_procs[i].ctx = ctx;
            g_procs[i].started = 0;
            *out = (void *)(u64)(i + 1);
            punlock();
            return 0;
        }
    punlock();
    return kAudioHardwareIllegalOperationError;
}
OSStatus AudioDeviceCreateIOProcID(u32 d, void *proc, void *ctx, void **out) { return ioproc_add(d, proc, 0, ctx, out); }
OSStatus AudioDeviceCreateIOProcIDWithBlock(void **out, u32 d, void *q, void *block) { (void)q; return ioproc_add(d, 0, block, 0, out); }
static ioproc_t *ioproc_of(void *pid) {
    u64 i = (u64)pid;
    return i >= 1 && i <= MAX_IOPROCS && (g_procs[i - 1].proc || g_procs[i - 1].block) ? &g_procs[i - 1] : 0;
}
OSStatus AudioDeviceDestroyIOProcID(u32 d, void *pid) {
    if (d != DEV_OUT) return kAudioHardwareBadObjectError;
    ioproc_t *p = ioproc_of(pid);
    if (!p) return kAudioHardwareIllegalOperationError;
    plock();
    p->started = 0;
    void *b = p->block;
    p->block = 0;
    p->proc = 0;
    punlock();
    dev_engine_update();
    if (b) _Block_release(b);
    return 0;
}
OSStatus AudioDeviceStart(u32 d, void *pid) {
    if (d != DEV_OUT) return kAudioHardwareBadObjectError;
    if (pid) { ioproc_t *p = ioproc_of(pid); if (!p) return kAudioHardwareIllegalOperationError; p->started = 1; }
    dev_engine_update();
    return 0;
}
OSStatus AudioDeviceStop(u32 d, void *pid) {
    if (d != DEV_OUT) return kAudioHardwareBadObjectError;
    if (pid) { ioproc_t *p = ioproc_of(pid); if (p) p->started = 0; }
    dev_engine_update();
    return 0;
}
OSStatus AudioDeviceGetCurrentTime(u32 d, ats_t *t) {
    if (d != DEV_OUT || !t) return kAudioHardwareBadObjectError;
    memset(t, 0, sizeof *t);
    if (!g_dev_engine) return FCC('s', 't', 'o', 'p');   /* kAudioHardwareNotRunningError */
    t->mSampleTime = cae_played(g_dev_engine);
    t->mHostTime = host_now();
    t->mRateScalar = 1;
    t->mFlags = 7;
    return 0;
}
OSStatus AudioDeviceTranslateTime(u32 d, const ats_t *in, ats_t *out) {
    if (d != DEV_OUT || !in || !out) return kAudioHardwareBadObjectError;
    *out = *in;
    out->mFlags |= 7;
    return 0;
}

/* ================================================================ AudioComponent e as unidades de saída */
typedef struct { acdesc_t d; const char *name; } acomp_t;
static acomp_t g_comps[] = {
    {{FCC('a', 'u', 'o', 'u'), FCC('d', 'e', 'f', ' '), FCC('a', 'p', 'p', 'l'), 0, 0}, "Apple: DefaultOutputUnit"},
    {{FCC('a', 'u', 'o', 'u'), FCC('s', 'y', 's', ' '), FCC('a', 'p', 'p', 'l'), 0, 0}, "Apple: SystemOutputUnit"},
    {{FCC('a', 'u', 'o', 'u'), FCC('a', 'h', 'a', 'l'), FCC('a', 'p', 'p', 'l'), 0, 0}, "Apple: AUHAL"},
    {{FCC('a', 'u', 'o', 'u'), FCC('v', 'p', 'i', 'o'), FCC('a', 'p', 'p', 'l'), 0, 0}, "Apple: AUVoiceProcessor"},
    {{FCC('a', 'u', 'o', 'u'), FCC('r', 'i', 'o', 'c'), FCC('a', 'p', 'p', 'l'), 0, 0}, "Apple: AURemoteIO"},
};
#define NCOMPS (sizeof g_comps / sizeof g_comps[0])

void *AudioComponentFindNext(void *after, const acdesc_t *d) {
    u32 start = 0;
    for (u32 i = 0; i < NCOMPS; i++) if (after == &g_comps[i]) start = i + 1;
    for (u32 i = start; i < NCOMPS; i++) {
        const acdesc_t *c = &g_comps[i].d;
        if (d && d->componentType && d->componentType != c->componentType) continue;
        if (d && d->componentSubType && d->componentSubType != c->componentSubType) continue;
        if (d && d->componentManufacturer && d->componentManufacturer != c->componentManufacturer) continue;
        return &g_comps[i];
    }
    return 0;
}
u32 AudioComponentCount(const acdesc_t *d) { u32 n = 0; for (void *c = AudioComponentFindNext(0, d); c; c = AudioComponentFindNext(c, d)) n++; return n; }
OSStatus AudioComponentGetDescription(void *c, acdesc_t *out) { if (!c || !out) return kAudio_ParamError; *out = ((acomp_t *)c)->d; return 0; }
OSStatus AudioComponentCopyName(void *c, id *out) { if (!c || !out) return kAudio_ParamError; *out = objc_retain(cx_str(((acomp_t *)c)->name)); return 0; }
OSStatus AudioComponentGetVersion(void *c, u32 *v) { if (!c || !v) return kAudio_ParamError; *v = 0x10000; return 0; }

typedef OSStatus (*aurender_fn)(void *refcon, u32 *flags, const ats_t *ts, u32 bus, u32 frames, ablist_t *data);
typedef struct au {
    u32 magic;
    acomp_t *comp;
    asbd_t client;               /* formato que o app entrega (escopo de entrada do elemento 0) */
    asbd_t input_client;         /* formato do barramento de entrada (elemento 1, escopo de saída) */
    aurender_fn render;
    void *refcon;
    aurender_fn input_cb;
    void *input_ref;
    u32 max_frames;
    int initialized, enable_out, enable_in;
    float volume;
    cae_t *eng;
    struct { u32 id; void *fn, *ud; } listeners[8];
    int nlisteners;
    struct { void *fn, *ud; } notify[4];
    int nnotify;
    void *buf;                   /* memória do AudioBufferList do render */
    u32 bufcap;
} au_t;
#define AU_MAGIC 0x4155554Eu

static au_t *au_of(void *u) { au_t *a = u; return a && a->magic == AU_MAGIC ? a : 0; }

OSStatus AudioComponentInstanceNew(void *c, void **out) {
    if (!c || !out) return kAudio_ParamError;
    au_t *a = calloc(1, sizeof *a);
    if (!a) return -108;   /* memFullErr */
    a->magic = AU_MAGIC;
    a->comp = c;
    out_asbd(&a->client, OUT_RATE);
    a->client.mFormatFlags |= kFmtNonInterleaved;   /* o formato canônico do AudioUnit: float32 não intercalado */
    a->client.mBytesPerFrame = a->client.mBytesPerPacket = 4;
    a->input_client = a->client;
    a->max_frames = 4096;
    a->enable_out = 1;
    a->volume = 1;
    *out = a;
    return 0;
}
OSStatus AudioComponentInstanceDispose(void *u) {
    au_t *a = au_of(u);
    if (!a) return kAudio_ParamError;
    cae_free(a->eng);
    free(a->buf);
    a->magic = 0;
    free(a);
    return 0;
}
void *AudioComponentInstanceGetComponent(void *u) { au_t *a = au_of(u); return a ? a->comp : 0; }
OSStatus AudioComponentInstanceCanDo(void *u, i16 sel) { (void)u; (void)sel; return 0; }

static void au_notify(au_t *a, u32 id, u32 scope, u32 el) {
    typedef void (*listener_fn)(void *ud, void *unit, u32 id, u32 scope, u32 el);
    for (int i = 0; i < a->nlisteners; i++)
        if (a->listeners[i].id == id && a->listeners[i].fn) ((listener_fn)a->listeners[i].fn)(a->listeners[i].ud, a, id, scope, el);
}

/* o render do motor: pede ao app quadros no formato do cliente e converte para estéreo float */
static void au_render(cae_t *e, float *stereo, u32 frames, const ats_t *ts) {
    au_t *a = e->ctx;
    if (!a->render) return;
    asbd_t *f = &a->client;
    u32 ch = f->mChannelsPerFrame;
    bool ni = f->mFormatFlags & kFmtNonInterleaved;
    u32 nb = ni ? ch : 1;
    u32 bytes_per_buf = frames * f->mBytesPerFrame;
    u32 need = 8 + 16 * nb + bytes_per_buf * nb;
    if (need > a->bufcap) { free(a->buf); a->buf = calloc(1, need); a->bufcap = a->buf ? need : 0; if (!a->buf) return; }
    ablist_t *l = a->buf;
    l->mNumberBuffers = nb;
    u8 *data = (u8 *)a->buf + 8 + 16 * nb;
    for (u32 i = 0; i < nb; i++) {
        l->mBuffers[i].mNumberChannels = ni ? 1 : ch;
        l->mBuffers[i].mDataByteSize = bytes_per_buf;
        l->mBuffers[i].mData = data + (u64)i * bytes_per_buf;
    }
    memset(data, 0, (size_t)bytes_per_buf * nb);
    u32 flags = 0;
    typedef OSStatus (*notify_fn)(void *ud, u32 *flags, const ats_t *ts, u32 bus, u32 frames, ablist_t *data);
    for (int i = 0; i < a->nnotify; i++) { u32 pf = 4; /* kAudioUnitRenderAction_PreRender */ ((notify_fn)a->notify[i].fn)(a->notify[i].ud, &pf, ts, 0, frames, l); }
    OSStatus r = a->render(a->refcon, &flags, ts, 0, frames, l);
    if (r == 0 && !(flags & 16 /* kAudioUnitRenderAction_OutputIsSilence */)) to_stereo(f, l, frames, stereo);
    for (int i = 0; i < a->nnotify; i++) { u32 pf = 8; /* PostRender */ ((notify_fn)a->notify[i].fn)(a->notify[i].ud, &pf, ts, 0, frames, l); }
}

OSStatus AudioUnitInitialize(void *u) { au_t *a = au_of(u); if (!a) return kAudio_ParamError; a->initialized = 1; return 0; }
OSStatus AudioUnitUninitialize(void *u) { au_t *a = au_of(u); if (!a) return kAudio_ParamError; cae_free(a->eng); a->eng = 0; a->initialized = 0; return 0; }

OSStatus AudioUnitSetProperty(void *u, u32 id_, u32 sc, u32 el, const void *d, u32 sz) {
    au_t *a = au_of(u);
    if (!a) return kAudio_ParamError;
    switch (id_) {
    case 8: {   /* kAudioUnitProperty_StreamFormat */
        if (!d || sz < 40) return kAudio_ParamError;
        asbd_t f = *(const asbd_t *)d;
        lpcm_fix(&f);
        if (!lpcm_ok(&f)) return kAudioUnitErr_FormatNotSupported;
        if ((sc == 1 && el == 0) || sc == 0) {   /* escopo de entrada do elemento de saída: o que o app vai mandar */
            bool rate_changed = !a->eng || a->client.mSampleRate != f.mSampleRate;
            a->client = f;
            if (a->eng && rate_changed) {
                int was = a->eng->run;
                cae_free(a->eng);
                a->eng = 0;
                if (was) { a->eng = cae_new(a->client.mSampleRate, 0, au_render, a); if (a->eng) { a->eng->gain = a->volume; cae_start(a->eng); } }
            }
        } else if (sc == 2 && el == 1) a->input_client = f;   /* o que o app quer ler do microfone */
        else if (sc == 2 && el == 0) { /* saída do elemento 0 = o hardware: aceita e ignora (o mOS reamostra) */ }
        else return kAudioUnitErr_InvalidElement;
        au_notify(a, id_, sc, el);
        return 0;
    }
    case 23:   /* kAudioUnitProperty_SetRenderCallback: AURenderCallbackStruct */
        if (!d || sz < 16) return kAudio_ParamError;
        a->render = ((void *const *)d)[0];
        a->refcon = ((void *const *)d)[1];
        return 0;
    case 2005: /* kAudioOutputUnitProperty_SetInputCallback */
        if (!d || sz < 16) return kAudio_ParamError;
        a->input_cb = ((void *const *)d)[0];
        a->input_ref = ((void *const *)d)[1];
        return 0;
    case 2003: /* kAudioOutputUnitProperty_EnableIO: elemento 1 = entrada, 0 = saída */
        if (!d || sz < 4) return kAudio_ParamError;
        if (el == 1) a->enable_in = *(const u32 *)d != 0;
        else a->enable_out = *(const u32 *)d != 0;
        return 0;
    case 2000: /* kAudioOutputUnitProperty_CurrentDevice */
        if (!d || sz < 4) return kAudio_ParamError;
        return *(const u32 *)d == DEV_OUT || *(const u32 *)d == 0 ? 0 : kAudioUnitErr_InvalidParameter;
    case 14:   /* kAudioUnitProperty_MaximumFramesPerSlice */
        if (!d || sz < 4) return kAudio_ParamError;
        a->max_frames = *(const u32 *)d;
        return 0;
    case 2:    /* kAudioUnitProperty_SampleRate */
        if (!d || sz < 8) return kAudio_ParamError;
        a->client.mSampleRate = *(const double *)d;
        return 0;
    case 51: case 2002: case 2004: case 2010: case 2012: case 2100: case 2101: case 2102: case 2103: case 2104: case 2105:
    case 19: case 54: case 3000: case 3014:
        return 0;   /* ShouldAllocateBuffer, ChannelMap, StartTime, as de voice processing: aceitas */
    }
    return kAudioUnitErr_InvalidProperty;
}

OSStatus AudioUnitGetPropertyInfo(void *u, u32 id_, u32 sc, u32 el, u32 *sz, u8 *w) {
    au_t *a = au_of(u);
    if (!a) return kAudio_ParamError;
    u32 n = 0;
    u8 wr = 1;
    switch (id_) {
    case 8: n = 40; break;
    case 23: case 2005: n = 16; break;
    case 2003: case 2000: case 14: case 2001: case 2006: case 11: n = 4; wr = id_ != 2001 && id_ != 2006 && id_ != 11; break;
    case 2: case 12: n = 8; wr = id_ == 2; break;
    case 22: n = 4; wr = 0; break;
    default: return kAudioUnitErr_InvalidProperty;
    }
    (void)sc; (void)el;
    if (sz) *sz = n;
    if (w) *w = wr;
    return 0;
}

OSStatus AudioUnitGetProperty(void *u, u32 id_, u32 sc, u32 el, void *d, u32 *sz) {
    au_t *a = au_of(u);
    if (!a || !d || !sz) return kAudio_ParamError;
#define AUPUT(T, v) do { if (*sz < sizeof(T)) return kAudio_ParamError; *(T *)d = (T)(v); *sz = sizeof(T); return 0; } while (0)
    switch (id_) {
    case 8:
        if (*sz < 40) return kAudio_ParamError;
        if ((sc == 1 && el == 0) || sc == 0) *(asbd_t *)d = a->client;
        else if (sc == 2 && el == 0) out_asbd(d, g_dev_rate);       /* o lado do hardware */
        else if (sc == 1 && el == 1) { out_asbd(d, g_dev_rate); ((asbd_t *)d)->mChannelsPerFrame = 1; ((asbd_t *)d)->mBytesPerFrame = ((asbd_t *)d)->mBytesPerPacket = 4; }
        else if (sc == 2 && el == 1) *(asbd_t *)d = a->input_client;
        else return kAudioUnitErr_InvalidElement;
        *sz = 40;
        return 0;
    case 2000: AUPUT(u32, DEV_OUT);
    case 2001: AUPUT(u32, a->eng && a->eng->run);
    case 2003: AUPUT(u32, el == 1 ? a->enable_in : a->enable_out);
    case 2006: AUPUT(u32, 1);
    case 14: AUPUT(u32, a->max_frames);
    case 2: AUPUT(double, a->client.mSampleRate);
    case 12: AUPUT(double, (double)CAE_TARGET / OUT_RATE);   /* kAudioUnitProperty_Latency */
    case 11: AUPUT(u32, 2);   /* ElementCount */
    case 22: AUPUT(i32, 0);   /* LastRenderError */
    case 23: if (*sz < 16) return kAudio_ParamError; ((void **)d)[0] = (void *)a->render; ((void **)d)[1] = a->refcon; *sz = 16; return 0;
    }
#undef AUPUT
    return kAudioUnitErr_InvalidProperty;
}

OSStatus AudioUnitAddPropertyListener(void *u, u32 id_, void *fn, void *ud) {
    au_t *a = au_of(u);
    if (!a || !fn) return kAudio_ParamError;
    if (a->nlisteners >= 8) return -108;
    a->listeners[a->nlisteners].id = id_;
    a->listeners[a->nlisteners].fn = fn;
    a->listeners[a->nlisteners].ud = ud;
    a->nlisteners++;
    return 0;
}
OSStatus AudioUnitRemovePropertyListenerWithUserData(void *u, u32 id_, void *fn, void *ud) {
    au_t *a = au_of(u);
    if (!a) return kAudio_ParamError;
    for (int i = 0; i < a->nlisteners; i++)
        if (a->listeners[i].id == id_ && a->listeners[i].fn == fn && a->listeners[i].ud == ud) { a->listeners[i] = a->listeners[--a->nlisteners]; break; }
    return 0;
}
OSStatus AudioUnitRemovePropertyListener(void *u, u32 id_, void *fn) {
    au_t *a = au_of(u);
    if (!a) return kAudio_ParamError;
    for (int i = 0; i < a->nlisteners; i++) if (a->listeners[i].id == id_ && a->listeners[i].fn == fn) { a->listeners[i] = a->listeners[--a->nlisteners]; i--; }
    return 0;
}
OSStatus AudioUnitAddRenderNotify(void *u, void *fn, void *ud) {
    au_t *a = au_of(u);
    if (!a || !fn) return kAudio_ParamError;
    if (a->nnotify >= 4) return -108;
    a->notify[a->nnotify].fn = fn;
    a->notify[a->nnotify].ud = ud;
    a->nnotify++;
    return 0;
}
OSStatus AudioUnitRemoveRenderNotify(void *u, void *fn, void *ud) {
    au_t *a = au_of(u);
    if (!a) return kAudio_ParamError;
    for (int i = 0; i < a->nnotify; i++) if (a->notify[i].fn == fn && a->notify[i].ud == ud) { a->notify[i] = a->notify[--a->nnotify]; break; }
    return 0;
}
/* kHALOutputParam_Volume (14) no escopo global: o ganho da unidade */
OSStatus AudioUnitSetParameter(void *u, u32 id_, u32 sc, u32 el, float v, u32 off) {
    (void)sc; (void)el; (void)off;
    au_t *a = au_of(u);
    if (!a) return kAudio_ParamError;
    if (id_ == 14) { a->volume = v < 0 ? 0 : v; if (a->eng) a->eng->gain = a->volume; return 0; }
    return 0;
}
OSStatus AudioUnitGetParameter(void *u, u32 id_, u32 sc, u32 el, float *v) {
    (void)sc; (void)el;
    au_t *a = au_of(u);
    if (!a || !v) return kAudio_ParamError;
    *v = id_ == 14 ? a->volume : 0;
    return 0;
}
OSStatus AudioUnitReset(void *u, u32 sc, u32 el) { (void)sc; (void)el; return au_of(u) ? 0 : kAudio_ParamError; }

/* Para o AVAudioEngine e o AVCaptureSession: o anel de captura, sem inventar amostra. */
u32 cx_capture_i16(short *dst, u32 frames) {
    if (!dst || !frames) return 0;
    long got = dev3(DEV_AUDIO_READ, (long)dst, (long)frames, 0);
    return got > 0 ? (u32)got : 0;
}
/* Lê a captura do kernel (48 kHz estéreo i16) e escreve n quadros no formato do cliente.
 * 0 quadros lidos = dispositivo vazio: o buffer fica em silêncio e *silence = 1. Não inventa sinal. */
static OSStatus mic_fill(const asbd_t *fmt, ablist_t *list, u32 n, int *silence) {
    if (silence) *silence = 1;
    if (!n || !fmt || !list) return 0;
    double rate = fmt->mSampleRate > 0 ? fmt->mSampleRate : OUT_RATE;
    int need = (int)((double)n * OUT_RATE / rate) + 2;
    if (need < 1) need = 1;
    if (need > 12000) need = 12000;
    short *src = calloc((size_t)need, 4);
    if (!src) return -108;
    long got = dev3(DEV_AUDIO_READ, (long)src, need, 0);
    if (got < 0) got = 0;
    if (got == 0) {
        for (u32 i = 0; i < list->mNumberBuffers; i++)
            if (list->mBuffers[i].mData) memset(list->mBuffers[i].mData, 0, list->mBuffers[i].mDataByteSize);
        free(src);
        return 0;
    }
    int ch = fmt->mChannelsPerFrame > 1 ? 2 : 1;
    short *dst = calloc((size_t)n * (size_t)(ch ? ch : 1), 2);
    float *st = calloc((size_t)n, 8);
    if (!dst || !st) { free(src); free(dst); free(st); return -108; }
    int wr = cx_pcm_s16_resample(src, (int)got, 48000, dst, (int)n, (int)rate, ch);
    for (int i = 0; i < wr; i++) {
        if (ch == 1) st[i * 2] = st[i * 2 + 1] = (float)dst[i] / 32768.0f;
        else {
            st[i * 2] = (float)dst[i * 2] / 32768.0f;
            st[i * 2 + 1] = (float)dst[i * 2 + 1] / 32768.0f;
        }
    }
    from_stereo(fmt, list, n, st);
    if (silence) *silence = wr == 0;
    free(st);
    free(dst);
    free(src);
    return 0;
}

/* Barramento 0: o pull de saída continua em silêncio (o app é quem escreve nesse barramento).
 * Barramento de entrada: só lê o microfone com kTCCServiceMicrophone concedido. */
OSStatus AudioUnitRender(void *u, u32 *flags, const ats_t *ts, u32 bus, u32 n, ablist_t *list) {
    (void)ts;
    au_t *a = au_of(u);
    if (!a || !list) return kAudio_ParamError;
    if (n > a->max_frames) return -10874;   /* kAudioUnitErr_TooManyFramesToProcess */
    if (bus == 0) {
        for (u32 i = 0; i < list->mNumberBuffers; i++)
            if (list->mBuffers[i].mData) memset(list->mBuffers[i].mData, 0, list->mBuffers[i].mDataByteSize);
        if (flags) *flags |= 16;   /* kAudioUnitRenderAction_OutputIsSilence */
        return 0;
    }
    int perm = cx_media_tcc("kTCCServiceMicrophone");
    if (perm != 1) return perm < 0 ? -66680 : -54;   /* sem a frente de TCC, ou permissão negada */
    int silence = 1;
    OSStatus r = mic_fill(&a->input_client, list, n, &silence);
    if (flags) {
        if (silence) *flags |= 16;
        else *flags &= ~16u;
    }
    return r;
}

OSStatus AudioOutputUnitStart(void *u) {
    au_t *a = au_of(u);
    if (!a) return kAudio_ParamError;
    if (!a->eng) a->eng = cae_new(a->client.mSampleRate, 0, au_render, a);
    if (!a->eng) return -108;
    a->eng->gain = a->volume;
    if (cae_start(a->eng) < 0) return kAudioHardwareIllegalOperationError;
    au_notify(a, 2001, 0, 0);   /* kAudioOutputUnitProperty_IsRunning */
    return 0;
}
OSStatus AudioOutputUnitStop(void *u) {
    au_t *a = au_of(u);
    if (!a) return kAudio_ParamError;
    if (a->eng) cae_stop(a->eng, false);
    au_notify(a, 2001, 0, 0);
    return 0;
}

/* ================================================================ AudioQueue de saída */
typedef struct {
    u32 mAudioDataBytesCapacity;
    u32 pad0;
    void *mAudioData;
    u32 mAudioDataByteSize;
    u32 pad1;
    void *mUserData;
    u32 mPacketDescriptionCapacity;
    u32 pad2;
    aspd_t *mPacketDescriptions;
    u32 mPacketDescriptionCount;
    u32 pad3;
} aqbuf_t;
typedef void (*aqout_fn)(void *ud, void *q, aqbuf_t *b);
typedef void (*aqout_blockfn)(void *blk, void *q, aqbuf_t *b);
typedef void (*aqprop_fn)(void *ud, void *q, u32 id);
#define AQ_MAXBUF 64
#define AQ_MAGIC 0x41515545u
typedef struct aq {
    u32 magic;
    asbd_t fmt;
    aqout_fn cb;
    void *cb_block;
    void *ud;
    int on_main;                 /* o app passou um run loop: os callbacks vão para a fila principal */
    aqbuf_t *ring[AQ_MAXBUF];    /* buffers enfileirados, na ordem */
    u32 head, n;
    u32 pos;                     /* bytes já tocados do buffer da frente */
    aqbuf_t *owned[AQ_MAXBUF];
    int nowned;
    cae_t *eng;
    int running, stopping, disposed;
    float volume;
    struct { u32 id; aqprop_fn fn; void *ud; } listeners[8];
    int nlisteners;
    u8 lock[64];
    double start_played;
    int input;                   /* fila de microfone: não abre stream de saída */
    int spawning;
    int free_on_exit;
    unsigned long ithread;
} aq_t;

static aq_t *aq_of(void *q) { aq_t *a = q; return a && a->magic == AQ_MAGIC && !a->disposed ? a : 0; }

typedef struct { aq_t *q; aqbuf_t *b; u32 prop; int kind; } aqcall_t;
static void aq_deliver(void *ctx) {
    aqcall_t *c = ctx;
    aq_t *q = c->q;
    if (q->magic == AQ_MAGIC && !q->disposed) {
        if (c->kind == 0) {
            if (q->cb_block) ((aqout_blockfn)BLK_FN(q->cb_block))(q->cb_block, q, c->b);
            else if (q->cb) q->cb(q->ud, q, c->b);
        } else {
            for (int i = 0; i < q->nlisteners; i++) if (q->listeners[i].id == c->prop) q->listeners[i].fn(q->listeners[i].ud, q, c->prop);
        }
    }
    free(c);
}
static void aq_call(aq_t *q, int kind, aqbuf_t *b, u32 prop) {
    aqcall_t *c = calloc(1, sizeof *c);
    if (!c) return;
    c->q = q; c->b = b; c->prop = prop; c->kind = kind;
    if (q->on_main) dispatch_async_f(dispatch_get_main_queue(), c, aq_deliver);
    else aq_deliver(c);   /* sem run loop: na thread interna, como o AudioQueue da Apple */
}

static void aq_render(cae_t *e, float *stereo, u32 frames, const ats_t *ts) {
    (void)ts;
    aq_t *q = e->ctx;
    u32 bpf = q->fmt.mBytesPerFrame;
    u32 done = 0;
    while (done < frames) {
        pthread_mutex_lock(q->lock);
        if (!q->n) {
            pthread_mutex_unlock(q->lock);
            if (q->stopping) {   /* AudioQueueStop(q, false): tocou tudo, para e avisa */
                q->stopping = 0;
                q->running = 0;
                e->run = 0;
                aq_call(q, 1, 0, FCC('a', 'q', 'r', 'n'));
            }
            break;
        }
        aqbuf_t *b = q->ring[q->head];
        u32 avail = (b->mAudioDataByteSize - q->pos) / (bpf ? bpf : 1);
        u32 take = avail < frames - done ? avail : frames - done;
        if (take) {
            ablist_t l;
            l.mNumberBuffers = 1;
            l.mBuffers[0].mNumberChannels = q->fmt.mChannelsPerFrame;
            l.mBuffers[0].mDataByteSize = take * bpf;
            l.mBuffers[0].mData = (u8 *)b->mAudioData + q->pos;
            to_stereo(&q->fmt, &l, take, stereo + done * 2);
            q->pos += take * bpf;
            done += take;
        }
        bool finished = q->pos >= b->mAudioDataByteSize || !take;
        if (finished) { q->head = (q->head + 1) % AQ_MAXBUF; q->n--; q->pos = 0; }
        pthread_mutex_unlock(q->lock);
        if (finished) aq_call(q, 0, b, 0);
    }
}

static OSStatus aq_new(const asbd_t *f, aqout_fn cb, void *block, void *ud, int on_main, void **out) {
    if (!f || !out) return kAudio_ParamError;
    asbd_t fmt = *f;
    lpcm_fix(&fmt);
    if (!lpcm_ok(&fmt) || (fmt.mFormatFlags & kFmtNonInterleaved)) return kAudioFormatUnsupportedDataFormatError;
    aq_t *q = calloc(1, sizeof *q);
    if (!q) return -108;
    q->magic = AQ_MAGIC;
    q->fmt = fmt;
    q->cb = cb;
    q->cb_block = block ? _Block_copy(block) : 0;
    q->ud = ud;
    q->on_main = on_main;
    q->volume = 1;
    pthread_mutex_init(q->lock, 0);
    *out = q;
    return 0;
}
static void aq_release(aq_t *q) {
    cae_free(q->eng);
    q->eng = 0;
    for (int i = 0; i < q->nowned; i++) { free(q->owned[i]->mAudioData); free(q->owned[i]->mPacketDescriptions); free(q->owned[i]); }
    q->nowned = 0;
    if (q->cb_block) _Block_release(q->cb_block);
    q->disposed = 1;
    q->magic = 0;
    free(q);
}
static void aq_fill_input(aq_t *q, aqbuf_t *b) {
    u32 bpf = q->fmt.mBytesPerFrame ? q->fmt.mBytesPerFrame : 1;
    u32 frames = bpf ? b->mAudioDataBytesCapacity / bpf : 0;
    b->mAudioDataByteSize = frames * bpf;
    ablist_t l;
    memset(&l, 0, sizeof l);
    l.mNumberBuffers = 1;
    l.mBuffers[0].mNumberChannels = q->fmt.mChannelsPerFrame;
    l.mBuffers[0].mDataByteSize = b->mAudioDataByteSize;
    l.mBuffers[0].mData = b->mAudioData;
    int silence = 1;
    mic_fill(&q->fmt, &l, frames, &silence);
}
static void *aq_in_thread(void *arg) {
    aq_t *q = arg;
    unsigned long me = pthread_self();
    for (;;) {
        pthread_mutex_lock(q->lock);
        if (q->ithread != me) { pthread_mutex_unlock(q->lock); usleep(1000); continue; }
        if (!q->running || q->disposed) {
            int free_it = q->free_on_exit;
            q->ithread = 0;
            pthread_mutex_unlock(q->lock);
            if (free_it) aq_release(q);
            return 0;
        }
        if (!q->n) { pthread_mutex_unlock(q->lock); usleep(4000); continue; }
        aqbuf_t *b = q->ring[q->head];
        q->head = (q->head + 1) % AQ_MAXBUF;
        q->n--;
        pthread_mutex_unlock(q->lock);
        aq_fill_input(q, b);
        aq_call(q, 0, b, 0);
    }
}
static OSStatus aq_start_input(aq_t *q) {
    pthread_mutex_lock(q->lock);
    q->running = 1;
    q->stopping = 0;
    int spawn = !q->ithread && !q->spawning;
    if (spawn) q->spawning = 1;
    pthread_mutex_unlock(q->lock);
    if (!spawn) return 0;
    unsigned long th = 0;
    if (pthread_create(&th, 0, aq_in_thread, q) != 0) {
        pthread_mutex_lock(q->lock);
        q->running = 0;
        q->spawning = 0;
        pthread_mutex_unlock(q->lock);
        return -66680;
    }
    pthread_mutex_lock(q->lock);
    q->ithread = th;
    q->spawning = 0;
    pthread_mutex_unlock(q->lock);
    return 0;
}

OSStatus AudioQueueNewOutput(const asbd_t *f, aqout_fn cb, void *ud, void *runloop, id mode, u32 flags, void **out) {
    (void)mode; (void)flags;
    return aq_new(f, cb, 0, ud, runloop != 0, out);
}
OSStatus AudioQueueNewOutputWithDispatchQueue(void **out, const asbd_t *f, u32 flags, void *dq, void *block) {
    (void)flags;
    return aq_new(f, 0, block, 0, dq == dispatch_get_main_queue(), out);
}
OSStatus AudioQueueAllocateBuffer(void *q_, u32 size, aqbuf_t **out) {
    aq_t *q = aq_of(q_);
    if (!q || !out) return -66687;
    if (q->nowned >= AQ_MAXBUF) return -108;
    aqbuf_t *b = calloc(1, sizeof *b);
    if (!b) return -108;
    b->mAudioData = calloc(1, size ? size : 1);
    if (!b->mAudioData) { free(b); return -108; }
    b->mAudioDataBytesCapacity = size;
    q->owned[q->nowned++] = b;
    *out = b;
    return 0;
}
OSStatus AudioQueueAllocateBufferWithPacketDescriptions(void *q_, u32 size, u32 npd, aqbuf_t **out) {
    OSStatus r = AudioQueueAllocateBuffer(q_, size, out);
    if (r == 0 && npd) { (*out)->mPacketDescriptions = calloc(npd, sizeof(aspd_t)); (*out)->mPacketDescriptionCapacity = npd; }
    return r;
}
OSStatus AudioQueueFreeBuffer(void *q_, aqbuf_t *b) {
    aq_t *q = aq_of(q_);
    if (!q || !b) return -66687;
    for (int i = 0; i < q->nowned; i++)
        if (q->owned[i] == b) {
            q->owned[i] = q->owned[--q->nowned];
            free(b->mAudioData);
            free(b->mPacketDescriptions);
            free(b);
            return 0;
        }
    return -66687;   /* kAudioQueueErr_InvalidBuffer */
}
OSStatus AudioQueueEnqueueBuffer(void *q_, aqbuf_t *b, u32 npd, const aspd_t *pd) {
    (void)npd; (void)pd;
    aq_t *q = aq_of(q_);
    if (!q || !b) return -66687;
    if (b->mAudioDataByteSize > b->mAudioDataBytesCapacity) return -66682;
    pthread_mutex_lock(q->lock);
    if (q->n >= AQ_MAXBUF) { pthread_mutex_unlock(q->lock); return -66679; }
    q->ring[(q->head + q->n) % AQ_MAXBUF] = b;
    q->n++;
    pthread_mutex_unlock(q->lock);
    return 0;
}
OSStatus AudioQueueEnqueueBufferWithParameters(void *q, aqbuf_t *b, u32 npd, const aspd_t *pd, u32 trimStart, u32 trimEnd, u32 nparams,
                                               const void *params, const ats_t *start, ats_t *actual) {
    (void)trimStart; (void)trimEnd; (void)nparams; (void)params; (void)start;
    if (actual) memset(actual, 0, sizeof *actual);
    return AudioQueueEnqueueBuffer(q, b, npd, pd);
}
OSStatus AudioQueueStart(void *q_, const ats_t *t) {
    (void)t;
    aq_t *q = aq_of(q_);
    if (!q) return -66681;
    if (q->input) {
        bool was = q->running;
        OSStatus r = aq_start_input(q);
        if (r) return r;
        if (!was) aq_call(q, 1, 0, FCC('a', 'q', 'r', 'n'));
        return 0;
    }
    if (!q->eng) q->eng = cae_new(q->fmt.mSampleRate, 0, aq_render, q);
    if (!q->eng) return -108;
    q->eng->gain = q->volume;
    q->stopping = 0;
    bool was = q->running;
    if (cae_start(q->eng) < 0) return -66680;
    q->running = 1;
    if (!was) aq_call(q, 1, 0, FCC('a', 'q', 'r', 'n'));
    return 0;
}
OSStatus AudioQueuePrime(void *q_, u32 n, u32 *prepared) { (void)n; if (prepared) *prepared = 0; return aq_of(q_) ? 0 : -66681; }
OSStatus AudioQueuePause(void *q_) {
    aq_t *q = aq_of(q_);
    if (!q) return -66681;
    if (q->eng) q->eng->paused = 1;
    return 0;
}
OSStatus AudioQueueFlush(void *q_) { return aq_of(q_) ? 0 : -66681; }
OSStatus AudioQueueStop(void *q_, u8 immediate) {
    aq_t *q = aq_of(q_);
    if (!q) return -66681;
    int notify_stop = 0;
    if (q->input) {
        notify_stop = q->running;
        pthread_mutex_lock(q->lock);
        q->running = 0;
        q->stopping = 0;
        unsigned long th = q->ithread;
        pthread_mutex_unlock(q->lock);
        if (th && th != pthread_self()) pthread_join(th, 0);
    } else if (!immediate && q->running) { q->stopping = 1; return 0; }   /* toca o que está enfileirado e para */
    if (q->eng) cae_stop(q->eng, true);
    pthread_mutex_lock(q->lock);
    aqbuf_t *back[AQ_MAXBUF];
    u32 n = q->n;
    for (u32 i = 0; i < n; i++) back[i] = q->ring[(q->head + i) % AQ_MAXBUF];
    q->n = 0;
    q->pos = 0;
    pthread_mutex_unlock(q->lock);
    for (u32 i = 0; i < n; i++) aq_call(q, 0, back[i], 0);   /* os buffers voltam ao app, como no WebKit da Apple */
    bool was = q->running;
    q->running = 0;
    if (was || notify_stop) aq_call(q, 1, 0, FCC('a', 'q', 'r', 'n'));
    return 0;
}
OSStatus AudioQueueReset(void *q_) {
    aq_t *q = aq_of(q_);
    if (!q) return -66681;
    pthread_mutex_lock(q->lock);
    q->n = 0;
    q->pos = 0;
    pthread_mutex_unlock(q->lock);
    return 0;
}
OSStatus AudioQueueDispose(void *q_, u8 immediate) {
    (void)immediate;
    aq_t *q = aq_of(q_);
    if (!q) return -66681;
    pthread_mutex_lock(q->lock);
    q->running = 0;
    q->disposed = 1;
    unsigned long th = q->ithread;
    int self = th && th == pthread_self();
    if (self) q->free_on_exit = 1;
    pthread_mutex_unlock(q->lock);
    if (self) return 0;   /* a thread de entrada libera a fila ao sair */
    if (th) pthread_join(th, 0);
    aq_release(q);
    return 0;
}
OSStatus AudioQueueSetParameter(void *q_, u32 id_, float v) {
    aq_t *q = aq_of(q_);
    if (!q) return -66681;
    if (id_ == 1) { q->volume = v < 0 ? 0 : v > 1 ? 1 : v; if (q->eng) q->eng->gain = q->volume; }
    return 0;
}
OSStatus AudioQueueGetParameter(void *q_, u32 id_, float *v) {
    aq_t *q = aq_of(q_);
    if (!q || !v) return -66681;
    *v = id_ == 1 ? q->volume : id_ == 2 ? 1.0f : 0;
    return 0;
}
OSStatus AudioQueueGetPropertySize(void *q_, u32 id_, u32 *sz) {
    if (!aq_of(q_) || !sz) return -66681;
    *sz = id_ == FCC('a', 'q', 'f', 't') ? 40 : id_ == FCC('a', 'q', 'c', 'd') ? 8 : 4;
    return 0;
}
OSStatus AudioQueueGetProperty(void *q_, u32 id_, void *d, u32 *sz) {
    aq_t *q = aq_of(q_);
    if (!q || !d || !sz) return -66681;
    if (id_ == FCC('a', 'q', 'r', 'n')) { if (*sz < 4) return -66674; *(u32 *)d = q->running; *sz = 4; return 0; }
    if (id_ == FCC('a', 'q', 'f', 't')) { if (*sz < 40) return -66674; *(asbd_t *)d = q->fmt; *sz = 40; return 0; }
    if (id_ == FCC('a', 'q', 's', 'r')) { if (*sz < 8) return -66674; *(double *)d = g_dev_rate; *sz = 8; return 0; }
    if (id_ == FCC('a', 'q', 'd', 'c')) { if (*sz < 4) return -66674; *(u32 *)d = 2; *sz = 4; return 0; }
    if (id_ == FCC('a', 'q', 'c', 'd')) { if (*sz < 8) return -66674; *(id *)d = objc_retain(cx_str("mOS:saida")); *sz = 8; return 0; }
    if (id_ == FCC('x', 'o', 'p', 's')) { if (*sz < 4) return -66674; *(u32 *)d = q->fmt.mBytesPerPacket; *sz = 4; return 0; }
    return -66684;   /* kAudioQueueErr_InvalidProperty */
}
OSStatus AudioQueueSetProperty(void *q_, u32 id_, const void *d, u32 sz) { (void)id_; (void)d; (void)sz; return aq_of(q_) ? 0 : -66681; }
OSStatus AudioQueueAddPropertyListener(void *q_, u32 id_, aqprop_fn fn, void *ud) {
    aq_t *q = aq_of(q_);
    if (!q || !fn) return -66681;
    if (q->nlisteners >= 8) return -108;
    q->listeners[q->nlisteners].id = id_;
    q->listeners[q->nlisteners].fn = fn;
    q->listeners[q->nlisteners].ud = ud;
    q->nlisteners++;
    return 0;
}
OSStatus AudioQueueRemovePropertyListener(void *q_, u32 id_, aqprop_fn fn, void *ud) {
    aq_t *q = aq_of(q_);
    if (!q) return -66681;
    for (int i = 0; i < q->nlisteners; i++)
        if (q->listeners[i].id == id_ && q->listeners[i].fn == fn && q->listeners[i].ud == ud) { q->listeners[i] = q->listeners[--q->nlisteners]; break; }
    return 0;
}
OSStatus AudioQueueCreateTimeline(void *q_, void **tl) { if (!aq_of(q_) || !tl) return -66681; *tl = (void *)1; return 0; }
OSStatus AudioQueueDisposeTimeline(void *q_, void *tl) { (void)tl; return aq_of(q_) ? 0 : -66681; }
OSStatus AudioQueueGetCurrentTime(void *q_, void *tl, ats_t *t, u8 *disc) {
    (void)tl;
    aq_t *q = aq_of(q_);
    if (!q) return -66681;
    if (disc) *disc = 0;
    if (!t) return 0;
    memset(t, 0, sizeof *t);
    t->mSampleTime = q->eng ? cae_played(q->eng) : 0;
    t->mHostTime = host_now();
    t->mRateScalar = 1;
    t->mFlags = 7;
    return 0;
}
OSStatus AudioQueueDeviceGetCurrentTime(void *q_, ats_t *t) { return AudioQueueGetCurrentTime(q_, 0, t, 0); }
OSStatus AudioQueueDeviceTranslateTime(void *q_, const ats_t *in, ats_t *out) { if (!aq_of(q_) || !in || !out) return -66681; *out = *in; return 0; }
OSStatus AudioQueueDeviceGetNearestStartTime(void *q_, ats_t *t, u32 flags) { (void)flags; return AudioQueueGetCurrentTime(q_, 0, t, 0); }
/* Entrada: só cria a fila com kTCCServiceMicrophone concedido. Sem o símbolo da frente de TCC, recusa. */
OSStatus AudioQueueNewInput(const asbd_t *f, void *cb, void *ud, void *rl, id mode, u32 flags, void **out) {
    (void)mode; (void)flags;
    int perm = cx_media_tcc("kTCCServiceMicrophone");
    if (perm != 1) { if (out) *out = 0; return perm < 0 ? -66680 : -54; }
    OSStatus r = aq_new(f, (aqout_fn)cb, 0, ud, rl != 0, out);
    if (r == 0 && out && *out) ((aq_t *)*out)->input = 1;
    return r;
}

/* ================================================================ AudioConverter (LPCM -> LPCM) */
typedef OSStatus (*acinput_fn)(void *conv, u32 *npackets, ablist_t *data, aspd_t **pd, void *ud);
typedef struct { u32 magic; asbd_t src, dst; rs_t rs; float *tmp, *tmp2; u32 tcap; } aconv_t;
#define AC_MAGIC 0x4143564Eu
static aconv_t *ac_of(void *c) { aconv_t *a = c; return a && a->magic == AC_MAGIC ? a : 0; }
OSStatus AudioConverterNew(const asbd_t *src, const asbd_t *dst, void **out) {
    if (!src || !dst || !out) return kAudio_ParamError;
    asbd_t s = *src, d = *dst;
    lpcm_fix(&s);
    lpcm_fix(&d);
    if (!lpcm_ok(&s) || !lpcm_ok(&d)) { *out = 0; return kAudioConverterErr_FormatNotSupported; }
    aconv_t *c = calloc(1, sizeof *c);
    if (!c) return -108;
    c->magic = AC_MAGIC;
    c->src = s;
    c->dst = d;
    rs_init(&c->rs, s.mSampleRate, d.mSampleRate);
    *out = c;
    return 0;
}
OSStatus AudioConverterNewSpecific(const asbd_t *src, const asbd_t *dst, u32 n, const void *descs, void **out) { (void)n; (void)descs; return AudioConverterNew(src, dst, out); }
OSStatus AudioConverterDispose(void *c) { aconv_t *a = ac_of(c); if (!a) return kAudio_ParamError; a->magic = 0; free(a->tmp); free(a->tmp2); free(a); return 0; }
OSStatus AudioConverterReset(void *c) { aconv_t *a = ac_of(c); if (!a) return kAudio_ParamError; rs_init(&a->rs, a->src.mSampleRate, a->dst.mSampleRate); return 0; }
static bool ac_room(aconv_t *a, u32 frames) {
    if (frames <= a->tcap) return true;
    float *t = realloc(a->tmp, (size_t)frames * 8), *t2 = realloc(a->tmp2, ((size_t)frames * 8 + 64) * 8);
    if (t) a->tmp = t;
    if (t2) a->tmp2 = t2;
    if (!t || !t2) return false;
    a->tcap = frames;
    return true;
}
/* converte nf quadros do lado de origem e devolve quantos quadros de destino saíram em out */
static u32 ac_convert(aconv_t *a, const ablist_t *in, u32 nf, ablist_t *out, u32 outcap) {
    if (!ac_room(a, nf)) return 0;
    to_stereo(&a->src, in, nf, a->tmp);
    u32 k = rs_run(&a->rs, a->tmp, nf, a->tmp2, outcap);
    /* mais de dois canais na origem e no destino: copia canal a canal na mesma taxa (sem passar pelo estéreo) */
    if (a->src.mChannelsPerFrame > 2 && a->dst.mChannelsPerFrame == a->src.mChannelsPerFrame && a->src.mSampleRate == a->dst.mSampleRate) {
        for (u32 f = 0; f < nf && f < outcap; f++)
            for (u32 ch = 0; ch < a->src.mChannelsPerFrame; ch++) wr_sample(&a->dst, out, f, ch, rd_sample(&a->src, in, f, ch));
        return nf < outcap ? nf : outcap;
    }
    from_stereo(&a->dst, out, k, a->tmp2);
    return k;
}
OSStatus AudioConverterConvertBuffer(void *c, u32 insz, const void *in, u32 *outsz, void *out) {
    aconv_t *a = ac_of(c);
    if (!a || !in || !outsz || !out) return kAudio_ParamError;
    if (a->src.mSampleRate != a->dst.mSampleRate || (a->src.mFormatFlags | a->dst.mFormatFlags) & kFmtNonInterleaved) return kAudioConverterErr_FormatNotSupported;
    u32 nf = insz / a->src.mBytesPerFrame;
    u32 cap = *outsz / a->dst.mBytesPerFrame;
    ablist_t li = {1, 0, {{a->src.mChannelsPerFrame, insz, (void *)in}}};
    ablist_t lo = {1, 0, {{a->dst.mChannelsPerFrame, *outsz, out}}};
    u32 k = ac_convert(a, &li, nf, &lo, cap);
    *outsz = k * a->dst.mBytesPerFrame;
    return 0;
}
OSStatus AudioConverterConvertComplexBuffer(void *c, u32 nf, const ablist_t *in, ablist_t *out) {
    aconv_t *a = ac_of(c);
    if (!a || !in || !out) return kAudio_ParamError;
    ac_convert(a, in, nf, out, nf);
    return 0;
}
/* o conversor pede entrada ao app pelo proc até encher a saída (ou o proc devolver 0 pacotes / erro) */
OSStatus AudioConverterFillComplexBuffer(void *c, acinput_fn proc, void *ud, u32 *npackets, ablist_t *out, aspd_t *pd) {
    (void)pd;
    aconv_t *a = ac_of(c);
    if (!a || !proc || !npackets || !out) return kAudio_ParamError;
    u32 want = *npackets, got = 0;
    /* o destino é escrito em pedaços: um ablist com os ponteiros deslocados */
    u32 nb = out->mNumberBuffers;
    ablist_t *view = calloc(1, 8 + 16 * (nb ? nb : 1));
    if (!view) return -108;
    OSStatus r = 0;
    while (got < want) {
        u32 req = (u32)((double)(want - got) * a->src.mSampleRate / a->dst.mSampleRate) + 1;
        if (req > 4096) req = 4096;
        u8 *mem = calloc(1, 8 + 16 * 32);
        ablist_t *in = (ablist_t *)mem;
        in->mNumberBuffers = (a->src.mFormatFlags & kFmtNonInterleaved) ? a->src.mChannelsPerFrame : 1;
        u32 n = req;
        aspd_t *ipd = 0;
        r = proc(a, &n, in, &ipd, ud);
        if (r != 0 || n == 0) { free(mem); break; }
        view->mNumberBuffers = nb;
        for (u32 i = 0; i < nb; i++) {
            view->mBuffers[i] = out->mBuffers[i];
            u32 step = a->dst.mBytesPerFrame;
            view->mBuffers[i].mData = (u8 *)out->mBuffers[i].mData + (u64)got * step;
            view->mBuffers[i].mDataByteSize = (want - got) * step;
        }
        u32 k = ac_convert(a, in, n, view, want - got);
        got += k;
        free(mem);
    }
    for (u32 i = 0; i < nb; i++) out->mBuffers[i].mDataByteSize = got * a->dst.mBytesPerFrame;
    free(view);
    *npackets = got;
    return got || r == 0 ? 0 : r;
}
OSStatus AudioConverterSetProperty(void *c, u32 id_, u32 sz, const void *d) { (void)id_; (void)sz; (void)d; return ac_of(c) ? 0 : kAudio_ParamError; }
OSStatus AudioConverterGetPropertyInfo(void *c, u32 id_, u32 *sz, u8 *w) {
    if (!ac_of(c)) return kAudio_ParamError;
    u32 n = id_ == FCC('a', 'c', 'i', 'd') || id_ == FCC('a', 'c', 'o', 'd') ? 40 : 4;
    if (sz) *sz = n;
    if (w) *w = 0;
    return 0;
}
OSStatus AudioConverterGetProperty(void *c, u32 id_, u32 *sz, void *d) {
    aconv_t *a = ac_of(c);
    if (!a || !sz || !d) return kAudio_ParamError;
    if (id_ == FCC('a', 'c', 'i', 'd') && *sz >= 40) { *(asbd_t *)d = a->src; *sz = 40; return 0; }   /* CurrentInputStreamDescription */
    if (id_ == FCC('a', 'c', 'o', 'd') && *sz >= 40) { *(asbd_t *)d = a->dst; *sz = 40; return 0; }
    if (*sz >= 4) { *(u32 *)d = 0; *sz = 4; return 0; }
    return kAudioConverterErr_PropertyNotSupported;
}

/* ---- AudioFormat: só o necessário para LPCM */
OSStatus AudioFormatGetPropertyInfo(u32 id_, u32 sz, const void *spec, u32 *out) {
    (void)sz; (void)spec;
    if (!out) return kAudio_ParamError;
    if (id_ == FCC('f', 'm', 't', 'i')) { *out = 40; return 0; }   /* kAudioFormatProperty_FormatInfo */
    *out = 0;
    return kAudioFormatUnsupportedDataFormatError;
}
OSStatus AudioFormatGetProperty(u32 id_, u32 sz, const void *spec, u32 *osz, void *out) {
    (void)sz; (void)spec;
    if (id_ == FCC('f', 'm', 't', 'i') && out && osz && *osz >= 40) {
        asbd_t *d = out;
        if (d->mFormatID != kAudioFormatLinearPCM) return kAudioFormatUnsupportedDataFormatError;
        lpcm_fix(d);
        return 0;
    }
    if (osz) *osz = 0;
    return kAudioFormatUnsupportedDataFormatError;
}

/* ================================================================ arquivos de áudio (WAV, AIFF/AIFC, CAF) */
typedef struct {
    u32 type;                    /* 'WAVE', 'AIFF', 'AIFC', 'caff' */
    asbd_t fmt;
    u8 *file;                    /* o arquivo inteiro */
    u64 file_len;
    u8 *pcm;                     /* PCM s16 alocado pelo decodificador; data aponta para cá quando existe */
    const u8 *data;              /* primeiro byte de áudio */
    u64 data_len;
    double duration;
} adecode_t;

static u32 le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u16 le16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u16 be16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }
static u64 be64(const u8 *p) { return (u64)be32(p) << 32 | be32(p + 4); }
static double ext80(const u8 *p) {   /* o "extended" de 80 bits do AIFF (taxa de amostragem) */
    int e = ((p[0] & 0x7F) << 8 | p[1]) - 16383 - 63;
    u64 m = be64(p + 2);
    double v = (double)m;
    while (e > 0) { v *= 2; e--; }
    while (e < 0) { v /= 2; e++; }
    return (p[0] & 0x80) ? -v : v;
}

/* 0 = virou LPCM, 1 = não é formato desta frente (o parser antigo segue), outro = erro da Apple. */
static OSStatus decode_from_compressed(adecode_t *d) {
    cx_audiodec_out ao;
    if (!cx_audiodec(d->file, d->file_len, &ao)) return 1;
    if (ao.status != 0 || !ao.pcm || ao.channels < 1 || ao.rate < 1000) {
        cx_audiodec_free(&ao);
        return kAudioFileUnsupportedDataFormatError;
    }
    d->type = FCC('L', 'P', 'C', 'M');
    d->fmt.mFormatID = kAudioFormatLinearPCM;
    d->fmt.mSampleRate = ao.rate;
    d->fmt.mChannelsPerFrame = (u32)ao.channels;
    d->fmt.mBitsPerChannel = 16;
    d->fmt.mFormatFlags = kFmtSignedInt | kFmtPacked;
    d->fmt.mFramesPerPacket = 1;
    d->fmt.mBytesPerFrame = d->fmt.mBytesPerPacket = 2u * (u32)ao.channels;
    d->pcm = (u8 *)ao.pcm;
    ao.pcm = 0;
    d->data = d->pcm;
    d->data_len = (u64)ao.frames * d->fmt.mBytesPerFrame;
    cx_audiodec_free(&ao);
    return 0;
}

/* 0 ou o OSStatus de erro da Apple */
static OSStatus decode_parse(adecode_t *d) {
    const u8 *b = d->file;
    u64 n = d->file_len;
    memset(&d->fmt, 0, sizeof d->fmt);
    d->fmt.mFormatID = kAudioFormatLinearPCM;
    d->fmt.mFramesPerPacket = 1;
    OSStatus compressed = decode_from_compressed(d);
    if (compressed != 1) {
        if (compressed) return compressed;
        goto lpcm_tail;
    }
    if (n >= 12 && !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WAVE", 4)) {
        d->type = FCC('W', 'A', 'V', 'E');
        bool have_fmt = false;
        for (u64 o = 12; o + 8 <= n;) {
            u32 cl = le32(b + o + 4);
            const u8 *c = b + o + 8;
            if (!memcmp(b + o, "fmt ", 4) && cl >= 16 && o + 8 + 16 <= n) {
                u16 tag = le16(c);
                u16 ch = le16(c + 2);
                u32 rate = le32(c + 4);
                u16 bits = le16(c + 14);
                if (tag == 0xFFFE && cl >= 40) tag = le16(c + 24);   /* WAVE_FORMAT_EXTENSIBLE: o subformato */
                if (tag != 1 && tag != 3) return kAudioFileUnsupportedDataFormatError;   /* PCM ou IEEE float */
                d->fmt.mSampleRate = rate;
                d->fmt.mChannelsPerFrame = ch;
                d->fmt.mBitsPerChannel = bits;
                d->fmt.mFormatFlags = tag == 3 ? kFmtFloat | kFmtPacked : (bits > 8 ? kFmtSignedInt : 0) | kFmtPacked;
                d->fmt.mBytesPerFrame = d->fmt.mBytesPerPacket = (u32)((bits + 7) / 8) * ch;
                have_fmt = true;
            } else if (!memcmp(b + o, "data", 4)) {
                d->data = c;
                d->data_len = cl <= n - (o + 8) ? cl : n - (o + 8);
            }
            o += 8 + cl + (cl & 1);
        }
        if (!have_fmt || !d->data) return kAudioFileInvalidFileError;
    } else if (n >= 12 && !memcmp(b, "FORM", 4) && (!memcmp(b + 8, "AIFF", 4) || !memcmp(b + 8, "AIFC", 4))) {
        bool aifc = !memcmp(b + 8, "AIFC", 4);
        d->type = aifc ? FCC('A', 'I', 'F', 'C') : FCC('A', 'I', 'F', 'F');
        bool have = false;
        for (u64 o = 12; o + 8 <= n;) {
            u32 cl = be32(b + o + 4);
            const u8 *c = b + o + 8;
            if (!memcmp(b + o, "COMM", 4) && cl >= 18) {
                u16 ch = be16(c);
                u16 bits = be16(c + 6);
                d->fmt.mSampleRate = ext80(c + 8);
                d->fmt.mChannelsPerFrame = ch;
                d->fmt.mBitsPerChannel = bits;
                d->fmt.mFormatFlags = kFmtBigEndian | kFmtSignedInt | kFmtPacked;
                if (aifc && cl >= 22) {
                    u32 comp = be32(c + 18);
                    if (comp == FCC('s', 'o', 'w', 't')) d->fmt.mFormatFlags = kFmtSignedInt | kFmtPacked;   /* little endian */
                    else if (comp == FCC('f', 'l', '3', '2') || comp == FCC('F', 'L', '3', '2')) { d->fmt.mFormatFlags = kFmtFloat | kFmtBigEndian | kFmtPacked; d->fmt.mBitsPerChannel = 32; }
                    else if (comp == FCC('f', 'l', '6', '4') || comp == FCC('F', 'L', '6', '4')) { d->fmt.mFormatFlags = kFmtFloat | kFmtBigEndian | kFmtPacked; d->fmt.mBitsPerChannel = 64; }
                    else if (comp != FCC('N', 'O', 'N', 'E') && comp != FCC('t', 'w', 'o', 's')) return kAudioFileUnsupportedDataFormatError;
                }
                d->fmt.mBytesPerFrame = d->fmt.mBytesPerPacket = (u32)((d->fmt.mBitsPerChannel + 7) / 8) * ch;
                have = true;
            } else if (!memcmp(b + o, "SSND", 4) && cl >= 8) {
                u32 off = be32(c);
                d->data = c + 8 + off;
                d->data_len = cl - 8 - off;
                if (d->data + d->data_len > b + n) d->data_len = (u64)(b + n - d->data);
            }
            o += 8 + cl + (cl & 1);
        }
        if (!have || !d->data) return kAudioFileInvalidFileError;
    } else if (n >= 8 && !memcmp(b, "caff", 4)) {
        d->type = FCC('c', 'a', 'f', 'f');
        bool have = false;
        for (u64 o = 8; o + 12 <= n;) {
            u64 cl = be64(b + o + 4);
            const u8 *c = b + o + 12;
            if (!memcmp(b + o, "desc", 4) && cl >= 32) {
                u64 rb = be64(c);
                double rate;
                memcpy(&rate, &rb, 8);
                u32 fid = be32(c + 8), fl = be32(c + 12);
                if (fid != kAudioFormatLinearPCM) return kAudioFileUnsupportedDataFormatError;
                d->fmt.mSampleRate = rate;
                /* no CAF o bit 1 é kCAFLinearPCMFormatFlagIsLittleEndian (o inverso do kAudioFormatFlagIsBigEndian) */
                d->fmt.mFormatFlags = ((fl & 1) ? kFmtFloat : kFmtSignedInt) | ((fl & 2) ? 0 : kFmtBigEndian) | kFmtPacked;
                d->fmt.mBytesPerPacket = be32(c + 16);
                d->fmt.mFramesPerPacket = be32(c + 20);
                d->fmt.mChannelsPerFrame = be32(c + 24);
                d->fmt.mBitsPerChannel = be32(c + 28);
                d->fmt.mBytesPerFrame = d->fmt.mBytesPerPacket;
                have = true;
            } else if (!memcmp(b + o, "data", 4)) {
                d->data = c + 4;   /* mEditCount */
                u64 len = cl == (u64)-1 ? n - (o + 16) : cl - 4;
                d->data_len = len <= n - (o + 16) ? len : n - (o + 16);
            }
            if (cl == (u64)-1) break;
            o += 12 + cl;
        }
        if (!have || !d->data) return kAudioFileInvalidFileError;
    } else if (n >= 3 && (!memcmp(b, "ID3", 3) || (b[0] == 0xFF && (b[1] & 0xE0) == 0xE0))) {
        return kAudioFileUnsupportedDataFormatError;   /* MP3 que o decodificador não aceitou */
    } else if (n >= 12 && !memcmp(b + 4, "ftyp", 4)) {
        return kAudioFileUnsupportedDataFormatError;   /* M4A que não é ALAC de 16 bits (AAC fica aqui) */
    } else return kAudioFileUnsupportedFileTypeError;
lpcm_tail:
    if (!lpcm_ok(&d->fmt)) return kAudioFileUnsupportedDataFormatError;
    if (d->fmt.mBytesPerFrame) d->data_len -= d->data_len % d->fmt.mBytesPerFrame;
    d->duration = d->fmt.mBytesPerFrame && d->fmt.mSampleRate ? (double)(d->data_len / d->fmt.mBytesPerFrame) / d->fmt.mSampleRate : 0;
    return 0;
}

/* abre um arquivo pelo caminho; devolve um adecode_t (free com cx_audio_close) ou 0 com *err */
static void decode_close(adecode_t *d);
static adecode_t *decode_open_path(const char *path, OSStatus *err) {
    u64 len = 0;
    u8 *f = path ? cx_read_file(path, &len) : 0;
    if (!f) { if (err) *err = kAudioFileNotFound; return 0; }
    adecode_t *d = calloc(1, sizeof *d);
    if (!d) { free(f); if (err) *err = -108; return 0; }
    d->file = f;
    d->file_len = len;
    OSStatus r = decode_parse(d);
    if (r) { decode_close(d); if (err) *err = r; return 0; }
    if (err) *err = 0;
    return d;
}
static adecode_t *decode_open_data(const u8 *bytes, u64 len, OSStatus *err) {
    u8 *f = malloc(len ? len : 1);
    if (!f) { if (err) *err = -108; return 0; }
    memcpy(f, bytes, len);
    adecode_t *d = calloc(1, sizeof *d);
    if (!d) { free(f); if (err) *err = -108; return 0; }
    d->file = f;
    d->file_len = len;
    OSStatus r = decode_parse(d);
    if (r) { decode_close(d); if (err) *err = r; return 0; }
    if (err) *err = 0;
    return d;
}
static void decode_close(adecode_t *d) { if (d) { free(d->pcm); free(d->file); free(d); } }

/* para o avfaudio.c: abrir, formato, quadros e leitura em estéreo float na taxa do arquivo */
void *cx_audio_open_path(const char *path, int *err) { OSStatus e = 0; void *d = decode_open_path(path, &e); if (err) *err = e; return d; }
void *cx_audio_open_data(const void *b, u64 n, int *err) { OSStatus e = 0; void *d = decode_open_data(b, n, &e); if (err) *err = e; return d; }
void cx_audio_close(void *d) { decode_close(d); }
double cx_audio_rate(void *d) { return ((adecode_t *)d)->fmt.mSampleRate; }
u32 cx_audio_channels(void *d) { return ((adecode_t *)d)->fmt.mChannelsPerFrame; }
u64 cx_audio_frames(void *d) { adecode_t *a = d; return a->fmt.mBytesPerFrame ? a->data_len / a->fmt.mBytesPerFrame : 0; }
const void *cx_audio_asbd(void *d) { return &((adecode_t *)d)->fmt; }
u32 cx_audio_type(void *d) { return ((adecode_t *)d)->type; }
/* lê n quadros a partir de 'from' em estéreo float; devolve os lidos */
u32 cx_audio_read_stereo(void *d_, u64 from, u32 n, float *out) {
    adecode_t *d = d_;
    u64 total = cx_audio_frames(d);
    if (from >= total) return 0;
    if (from + n > total) n = (u32)(total - from);
    ablist_t l = {1, 0, {{d->fmt.mChannelsPerFrame, (u32)(n * d->fmt.mBytesPerFrame), (void *)(d->data + from * d->fmt.mBytesPerFrame)}}};
    to_stereo(&d->fmt, &l, n, out);
    return n;
}
/* lê n quadros num canal (0..) em float: para o AVAudioPCMBuffer com mais de dois canais */
float cx_audio_sample(void *d_, u64 frame, u32 ch) {
    adecode_t *d = d_;
    if (frame >= cx_audio_frames(d) || ch >= d->fmt.mChannelsPerFrame) return 0;
    ablist_t l = {1, 0, {{d->fmt.mChannelsPerFrame, (u32)d->fmt.mBytesPerFrame, (void *)(d->data + frame * d->fmt.mBytesPerFrame)}}};
    return rd_sample(&d->fmt, &l, 0, ch);
}

static const char *url_path(id url) {
    if (!url) return 0;
    if (cx_is_str(url)) return cx_cstr(url);
    id p = MSG0(id, url, "path");
    return p ? cx_cstr(p) : 0;
}
const char *cx_audio_url_path(id url) { return url_path(url); }

/* ---- AudioFile (leitura; escrita de WAV) */
typedef struct {
    u32 magic;
    adecode_t *dec;
    /* escrita */
    char *wpath;
    asbd_t wfmt;
    u8 *wbuf;
    u64 wlen, wcap;
} afile_t;
#define AF_MAGIC 0x4146494Cu
static afile_t *af_of(void *f) { afile_t *a = f; return a && a->magic == AF_MAGIC ? a : 0; }

OSStatus AudioFileOpenURL(id url, i8 perm, u32 hint, void **out) {
    (void)perm; (void)hint;
    if (!out) return kAudio_ParamError;
    OSStatus e = 0;
    adecode_t *d = decode_open_path(url_path(url), &e);
    if (!d) { *out = 0; return e; }
    afile_t *f = calloc(1, sizeof *f);
    if (!f) { decode_close(d); return -108; }
    f->magic = AF_MAGIC;
    f->dec = d;
    *out = f;
    return 0;
}
static OSStatus wav_finish(afile_t *f);
OSStatus AudioFileClose(void *f_) {
    afile_t *f = af_of(f_);
    if (!f) return kAudioFileNotOpenError;
    OSStatus r = f->wpath ? wav_finish(f) : 0;
    decode_close(f->dec);
    free(f->wpath);
    free(f->wbuf);
    f->magic = 0;
    free(f);
    return r;
}
OSStatus AudioFileGetPropertyInfo(void *f_, u32 id_, u32 *sz, u32 *w) {
    afile_t *f = af_of(f_);
    if (!f) return kAudioFileNotOpenError;
    u32 n;
    switch (id_) {
    case FCC('d', 'f', 'm', 't'): n = 40; break;
    case FCC('f', 'f', 'm', 't'): case FCC('p', 's', 'z', 'e'): case FCC('p', 'k', 'u', 'b'): n = 4; break;
    case FCC('b', 'c', 'n', 't'): case FCC('p', 'c', 'n', 't'): case FCC('e', 'd', 'u', 'r'): n = 8; break;
    default: return kAudioFileUnsupportedPropertyError;
    }
    if (sz) *sz = n;
    if (w) *w = 0;
    return 0;
}
OSStatus AudioFileGetProperty(void *f_, u32 id_, u32 *sz, void *out) {
    afile_t *f = af_of(f_);
    if (!f) return kAudioFileNotOpenError;
    if (!sz || !out) return kAudio_ParamError;
    adecode_t *d = f->dec;
    const asbd_t *fm = d ? &d->fmt : &f->wfmt;
    u64 bytes = d ? d->data_len : f->wlen;
    switch (id_) {
    case FCC('d', 'f', 'm', 't'): if (*sz < 40) return kAudioFileBadPropertySizeError; *(asbd_t *)out = *fm; *sz = 40; return 0;
    case FCC('f', 'f', 'm', 't'): if (*sz < 4) return kAudioFileBadPropertySizeError; *(u32 *)out = d ? d->type : FCC('W', 'A', 'V', 'E'); *sz = 4; return 0;
    case FCC('b', 'c', 'n', 't'): if (*sz < 8) return kAudioFileBadPropertySizeError; *(u64 *)out = bytes; *sz = 8; return 0;
    case FCC('p', 'c', 'n', 't'): if (*sz < 8) return kAudioFileBadPropertySizeError; *(u64 *)out = fm->mBytesPerPacket ? bytes / fm->mBytesPerPacket : 0; *sz = 8; return 0;
    case FCC('p', 's', 'z', 'e'): case FCC('p', 'k', 'u', 'b'): if (*sz < 4) return kAudioFileBadPropertySizeError; *(u32 *)out = fm->mBytesPerPacket; *sz = 4; return 0;
    case FCC('e', 'd', 'u', 'r'): if (*sz < 8) return kAudioFileBadPropertySizeError; *(double *)out = d ? d->duration : 0; *sz = 8; return 0;
    }
    return kAudioFileUnsupportedPropertyError;
}
OSStatus AudioFileSetProperty(void *f_, u32 id_, u32 sz, const void *d) { (void)id_; (void)sz; (void)d; return af_of(f_) ? kAudioFileUnsupportedPropertyError : kAudioFileNotOpenError; }
OSStatus AudioFileReadBytes(void *f_, u8 cache, i64 start, u32 *n, void *out) {
    (void)cache;
    afile_t *f = af_of(f_);
    if (!f || !f->dec) return kAudioFileNotOpenError;
    if (!n || !out || start < 0) return kAudio_ParamError;
    adecode_t *d = f->dec;
    if ((u64)start >= d->data_len) { *n = 0; return kAudioFileEndOfFileError; }
    u64 k = d->data_len - (u64)start;
    if (k > *n) k = *n;
    memcpy(out, d->data + start, (size_t)k);
    *n = (u32)k;
    return 0;
}
OSStatus AudioFileReadPacketData(void *f_, u8 cache, u32 *nbytes, aspd_t *pd, i64 start, u32 *npk, void *out) {
    (void)cache; (void)pd;
    afile_t *f = af_of(f_);
    if (!f || !f->dec) return kAudioFileNotOpenError;
    if (!npk || !out) return kAudio_ParamError;
    u32 bpp = f->dec->fmt.mBytesPerPacket;
    u32 maxpk = nbytes && bpp ? *nbytes / bpp : *npk;
    u32 want = *npk < maxpk ? *npk : maxpk;
    u32 bytes = want * bpp;
    OSStatus r = AudioFileReadBytes(f, 0, start * (i64)bpp, &bytes, out);
    *npk = bpp ? bytes / bpp : 0;
    if (nbytes) *nbytes = bytes;
    return r == kAudioFileEndOfFileError ? 0 : r;
}
OSStatus AudioFileReadPackets(void *f, u8 cache, u32 *nbytes, aspd_t *pd, i64 start, u32 *npk, void *out) {
    u32 big = 0x7FFFFFFF;
    OSStatus r = AudioFileReadPacketData(f, cache, nbytes ? nbytes : &big, pd, start, npk, out);
    return r;
}
static OSStatus wav_create(const char *path, const asbd_t *f, afile_t **out) {
    if (!path || !f) return kAudio_ParamError;
    asbd_t fmt = *f;
    lpcm_fix(&fmt);
    if (!lpcm_ok(&fmt) || (fmt.mFormatFlags & (kFmtNonInterleaved | kFmtBigEndian))) return kAudioFileUnsupportedDataFormatError;
    afile_t *a = calloc(1, sizeof *a);
    if (!a) return -108;
    a->magic = AF_MAGIC;
    size_t pl = strlen(path);
    a->wpath = malloc(pl + 1);
    if (!a->wpath) { free(a); return -108; }
    memcpy(a->wpath, path, pl + 1);
    a->wfmt = fmt;
    *out = a;
    return 0;
}
static OSStatus wav_append(afile_t *f, const void *b, u64 n) {
    if (f->wlen + n > f->wcap) {
        u64 nc = (f->wlen + n) * 2 + 65536;
        u8 *nb = realloc(f->wbuf, (size_t)nc);
        if (!nb) return -108;
        f->wbuf = nb;
        f->wcap = nc;
    }
    memcpy(f->wbuf + f->wlen, b, (size_t)n);
    f->wlen += n;
    return 0;
}
static void put_le32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void put_le16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static OSStatus wav_finish(afile_t *f) {
    u8 h[44];
    asbd_t *m = &f->wfmt;
    memcpy(h, "RIFF", 4);
    put_le32(h + 4, (u32)(36 + f->wlen));
    memcpy(h + 8, "WAVEfmt ", 8);
    put_le32(h + 16, 16);
    put_le16(h + 20, (m->mFormatFlags & kFmtFloat) ? 3 : 1);
    put_le16(h + 22, (u16)m->mChannelsPerFrame);
    put_le32(h + 24, (u32)m->mSampleRate);
    put_le32(h + 28, (u32)m->mSampleRate * m->mBytesPerFrame);
    put_le16(h + 32, (u16)m->mBytesPerFrame);
    put_le16(h + 34, (u16)m->mBitsPerChannel);
    memcpy(h + 36, "data", 4);
    put_le32(h + 40, (u32)f->wlen);
    FILE_ *fp = fopen(f->wpath, "wb");
    if (!fp) return kAudioFilePermissionsError;
    fwrite(h, 1, 44, fp);
    if (f->wlen) fwrite(f->wbuf, 1, (size_t)f->wlen, fp);
    fclose(fp);
    return 0;
}
OSStatus AudioFileCreateWithURL(id url, u32 type, const asbd_t *f, u32 flags, void **out) {
    (void)flags;
    if (!out) return kAudio_ParamError;
    if (type != FCC('W', 'A', 'V', 'E')) return kAudioFileUnsupportedFileTypeError;
    afile_t *a = 0;
    OSStatus r = wav_create(url_path(url), f, &a);
    *out = a;
    return r;
}
OSStatus AudioFileWriteBytes(void *f_, u8 cache, i64 start, u32 *n, const void *b) {
    (void)cache;
    afile_t *f = af_of(f_);
    if (!f || !f->wpath) return kAudioFilePermissionsError;
    if (!n || !b) return kAudio_ParamError;
    if ((u64)start != f->wlen) return kAudio_ParamError;   /* só acrescenta */
    return wav_append(f, b, *n);
}
OSStatus AudioFileWritePackets(void *f_, u8 cache, u32 nbytes, const aspd_t *pd, i64 start, u32 *npk, const void *b) {
    (void)pd; (void)start; (void)npk;
    u32 n = nbytes;
    afile_t *f = af_of(f_);
    if (!f) return kAudioFileNotOpenError;
    return AudioFileWriteBytes(f_, cache, (i64)f->wlen, &n, b);
}

/* ---- ExtAudioFile: o arquivo com conversão para o formato do cliente */
typedef struct { u32 magic; afile_t *af; asbd_t client; int has_client; u64 pos; aconv_t *conv; } extaf_t;
#define EAF_MAGIC 0x45584146u
static extaf_t *eaf_of(void *e) { extaf_t *x = e; return x && x->magic == EAF_MAGIC ? x : 0; }
OSStatus ExtAudioFileOpenURL(id url, void **out) {
    if (!out) return kAudio_ParamError;
    void *af = 0;
    OSStatus r = AudioFileOpenURL(url, 1, 0, &af);
    if (r) { *out = 0; return r; }
    extaf_t *x = calloc(1, sizeof *x);
    if (!x) { AudioFileClose(af); return -108; }
    x->magic = EAF_MAGIC;
    x->af = af;
    *out = x;
    return 0;
}
OSStatus ExtAudioFileWrapAudioFileID(void *af, u8 forWriting, void **out) {
    (void)forWriting;
    if (!af_of(af) || !out) return kAudio_ParamError;
    extaf_t *x = calloc(1, sizeof *x);
    if (!x) return -108;
    x->magic = EAF_MAGIC;
    x->af = af;
    *out = x;
    return 0;
}
OSStatus ExtAudioFileCreateWithURL(id url, u32 type, const asbd_t *f, const void *layout, u32 flags, void **out) {
    (void)layout;
    if (!out) return kAudio_ParamError;
    void *af = 0;
    OSStatus r = AudioFileCreateWithURL(url, type, f, flags, &af);
    if (r) { *out = 0; return r; }
    extaf_t *x = calloc(1, sizeof *x);
    if (!x) { AudioFileClose(af); return -108; }
    x->magic = EAF_MAGIC;
    x->af = af;
    *out = x;
    return 0;
}
OSStatus ExtAudioFileDispose(void *e) {
    extaf_t *x = eaf_of(e);
    if (!x) return kAudio_ParamError;
    OSStatus r = AudioFileClose(x->af);
    if (x->conv) AudioConverterDispose(x->conv);
    x->magic = 0;
    free(x);
    return r;
}
static const asbd_t *eaf_file_fmt(extaf_t *x) { return x->af->dec ? &x->af->dec->fmt : &x->af->wfmt; }
OSStatus ExtAudioFileSetProperty(void *e, u32 id_, u32 sz, const void *d) {
    extaf_t *x = eaf_of(e);
    if (!x) return kAudio_ParamError;
    if (id_ == FCC('c', 'f', 'm', 't')) {
        if (!d || sz < 40) return kAudio_ParamError;
        asbd_t c = *(const asbd_t *)d;
        lpcm_fix(&c);
        if (!lpcm_ok(&c)) return kAudioConverterErr_FormatNotSupported;
        x->client = c;
        x->has_client = 1;
        if (x->conv) { AudioConverterDispose(x->conv); x->conv = 0; }
        void *cv = 0;
        OSStatus r = x->af->dec ? AudioConverterNew(eaf_file_fmt(x), &c, &cv) : AudioConverterNew(&c, eaf_file_fmt(x), &cv);
        if (r) return r;
        x->conv = cv;
        return 0;
    }
    return 0;
}
OSStatus ExtAudioFileGetPropertyInfo(void *e, u32 id_, u32 *sz, u8 *w) {
    if (!eaf_of(e)) return kAudio_ParamError;
    if (sz) *sz = id_ == FCC('#', 'f', 'r', 'm') ? 8 : id_ == FCC('a', 'f', 'i', 'l') ? 8 : 40;
    if (w) *w = id_ == FCC('c', 'f', 'm', 't');
    return 0;
}
OSStatus ExtAudioFileGetProperty(void *e, u32 id_, u32 *sz, void *d) {
    extaf_t *x = eaf_of(e);
    if (!x || !sz || !d) return kAudio_ParamError;
    if (id_ == FCC('f', 'f', 'm', 't')) { if (*sz < 40) return kAudio_ParamError; *(asbd_t *)d = *eaf_file_fmt(x); *sz = 40; return 0; }
    if (id_ == FCC('c', 'f', 'm', 't')) { if (*sz < 40) return kAudio_ParamError; *(asbd_t *)d = x->has_client ? x->client : *eaf_file_fmt(x); *sz = 40; return 0; }
    if (id_ == FCC('#', 'f', 'r', 'm')) {
        if (*sz < 8) return kAudio_ParamError;
        const asbd_t *f = eaf_file_fmt(x);
        u64 bytes = x->af->dec ? x->af->dec->data_len : x->af->wlen;
        *(i64 *)d = f->mBytesPerFrame ? (i64)(bytes / f->mBytesPerFrame) : 0;
        *sz = 8;
        return 0;
    }
    if (id_ == FCC('a', 'f', 'i', 'l')) { if (*sz < 8) return kAudio_ParamError; *(void **)d = x->af; *sz = 8; return 0; }
    return kAudioFileUnsupportedPropertyError;
}
OSStatus ExtAudioFileSeek(void *e, i64 frame) { extaf_t *x = eaf_of(e); if (!x || frame < 0) return kAudio_ParamError; x->pos = (u64)frame; if (x->conv) AudioConverterReset(x->conv); return 0; }
OSStatus ExtAudioFileTell(void *e, i64 *frame) { extaf_t *x = eaf_of(e); if (!x || !frame) return kAudio_ParamError; *frame = (i64)x->pos; return 0; }
OSStatus ExtAudioFileRead(void *e, u32 *nframes, ablist_t *out) {
    extaf_t *x = eaf_of(e);
    if (!x || !x->af->dec || !nframes || !out) return kAudio_ParamError;
    adecode_t *d = x->af->dec;
    u64 total = d->fmt.mBytesPerFrame ? d->data_len / d->fmt.mBytesPerFrame : 0;
    if (x->pos >= total) { *nframes = 0; return 0; }
    u32 want = *nframes;
    if (!x->conv) {   /* sem formato de cliente: o formato do arquivo, cópia direta */
        u64 k = total - x->pos < want ? total - x->pos : want;
        u32 bytes = (u32)(k * d->fmt.mBytesPerFrame);
        if (out->mNumberBuffers && out->mBuffers[0].mData) {
            if (bytes > out->mBuffers[0].mDataByteSize) { k = out->mBuffers[0].mDataByteSize / d->fmt.mBytesPerFrame; bytes = (u32)(k * d->fmt.mBytesPerFrame); }
            memcpy(out->mBuffers[0].mData, d->data + x->pos * d->fmt.mBytesPerFrame, bytes);
            out->mBuffers[0].mDataByteSize = bytes;
        }
        x->pos += k;
        *nframes = (u32)k;
        return 0;
    }
    /* com conversão: lê do arquivo na taxa dele o suficiente para 'want' quadros do cliente */
    double ratio = d->fmt.mSampleRate / x->client.mSampleRate;
    u64 src = (u64)((double)want * ratio + 0.5);
    if (src < 1) src = 1;
    if (x->pos + src > total) src = total - x->pos;
    ablist_t in = {1, 0, {{d->fmt.mChannelsPerFrame, (u32)(src * d->fmt.mBytesPerFrame), (void *)(d->data + x->pos * d->fmt.mBytesPerFrame)}}};
    u32 cap = want;
    for (u32 i = 0; i < out->mNumberBuffers; i++) {
        u32 c = x->client.mBytesPerFrame ? out->mBuffers[i].mDataByteSize / x->client.mBytesPerFrame : 0;
        if (c < cap) cap = c;
    }
    u32 k = ac_convert(x->conv, &in, (u32)src, out, cap);
    for (u32 i = 0; i < out->mNumberBuffers; i++) out->mBuffers[i].mDataByteSize = k * x->client.mBytesPerFrame;
    x->pos += src;
    *nframes = k;
    return 0;
}
OSStatus ExtAudioFileWrite(void *e, u32 nframes, const ablist_t *in) {
    extaf_t *x = eaf_of(e);
    if (!x || !x->af->wpath || !in) return kAudio_ParamError;
    const asbd_t *ff = &x->af->wfmt;
    if (!x->conv) {
        u32 bytes = nframes * ff->mBytesPerFrame;
        return in->mNumberBuffers ? wav_append(x->af, in->mBuffers[0].mData, bytes) : 0;
    }
    u32 cap = (u32)((double)nframes * ff->mSampleRate / x->client.mSampleRate) + 16;
    u8 *tmp = malloc((size_t)cap * ff->mBytesPerFrame);
    if (!tmp) return -108;
    ablist_t out = {1, 0, {{ff->mChannelsPerFrame, cap * ff->mBytesPerFrame, tmp}}};
    u32 k = ac_convert(x->conv, in, nframes, &out, cap);
    OSStatus r = wav_append(x->af, tmp, (u64)k * ff->mBytesPerFrame);
    free(tmp);
    return r;
}
OSStatus ExtAudioFileWriteAsync(void *e, u32 nframes, const ablist_t *in) { return in ? ExtAudioFileWrite(e, nframes, in) : 0; }

/* ================================================================ AudioServices: sons de sistema */
typedef void (*sscomplete_fn)(u32 id, void *ud);
typedef struct { u32 id; i16 *pcm; u32 frames; sscomplete_fn done; void *done_ud; void *done_block; } ssound_t;
#define MAX_SOUNDS 64
static ssound_t g_sounds[MAX_SOUNDS];
static u32 g_next_sound = 4096 + 16;

/* converte um arquivo para PCM 48 kHz estéreo i16 */
static i16 *render_file_48k(adecode_t *d, u32 *frames) {
    u64 n = cx_audio_frames(d);
    if (n > 48000ull * 60) n = 48000ull * 60;   /* sons de sistema: até um minuto */
    double rate = d->fmt.mSampleRate;
    u32 outn = (u32)((double)n * OUT_RATE / rate) + 16;
    float *in = malloc((size_t)n * 8 + 8), *o = malloc((size_t)outn * 8);
    i16 *pcm = malloc((size_t)outn * 4);
    if (!in || !o || !pcm) { free(in); free(o); free(pcm); return 0; }
    cx_audio_read_stereo(d, 0, (u32)n, in);
    rs_t rs;
    rs_init(&rs, rate, OUT_RATE);
    u32 k = rs_run(&rs, in, (u32)n, o, outn);
    for (u32 i = 0; i < k * 2; i++) { float v = o[i]; if (v > 1) v = 1; if (v < -1) v = -1; pcm[i] = (i16)(v * 32767); }
    free(in);
    free(o);
    *frames = k;
    return pcm;
}

OSStatus AudioServicesCreateSystemSoundID(id url, u32 *out) {
    if (!out) return kAudio_ParamError;
    OSStatus e = 0;
    adecode_t *d = decode_open_path(url_path(url), &e);
    if (!d) return e;
    u32 frames = 0;
    i16 *pcm = render_file_48k(d, &frames);
    decode_close(d);
    if (!pcm) return -108;
    for (int i = 0; i < MAX_SOUNDS; i++)
        if (!g_sounds[i].id) {
            g_sounds[i].id = g_next_sound++;
            g_sounds[i].pcm = pcm;
            g_sounds[i].frames = frames;
            *out = g_sounds[i].id;
            return 0;
        }
    free(pcm);
    return -108;
}
static ssound_t *sound_of(u32 id) { for (int i = 0; i < MAX_SOUNDS; i++) if (g_sounds[i].id == id) return &g_sounds[i]; return 0; }
OSStatus AudioServicesDisposeSystemSoundID(u32 id) {
    ssound_t *s = sound_of(id);
    if (!s) return -50;
    free(s->pcm);
    if (s->done_block) _Block_release(s->done_block);
    memset(s, 0, sizeof *s);
    return 0;
}
typedef struct { u32 id; sscomplete_fn fn; void *ud; void *block; u32 ms; } ssdone_t;
static void ss_done_main(void *c) {
    ssdone_t *d = c;
    if (d->block) { ((void (*)(void *))BLK_FN(d->block))(d->block); _Block_release(d->block); }
    else if (d->fn) d->fn(d->id, d->ud);
    free(d);
}
static void *ss_wait(void *c) {
    ssdone_t *d = c;
    usleep(d->ms * 1000);
    dispatch_async_f(dispatch_get_main_queue(), d, ss_done_main);
    return 0;
}
static void play_pcm_48k(const i16 *pcm, u32 frames) {
    for (u32 o = 0; o < frames;) { long n = SYS3(GUI_DEVICE, DEV_AUDIO_PLAY, pcm + o * 2, frames - o); if (n <= 0) break; o += (u32)n; }
}
void NSBeep(void);
static void play_sound(u32 id, void *block) {
    if (id == 0x00001000 || id == 0 || id == 0xFFE) {   /* kSystemSoundID_UserPreferredAlert / FlashScreen */
        NSBeep();
        if (block) { ssdone_t *d = calloc(1, sizeof *d); if (d) { d->block = _Block_copy(block); d->ms = 300; unsigned long t; if (pthread_create(&t, 0, ss_wait, d) == 0) pthread_detach(t); } }
        return;
    }
    ssound_t *s = sound_of(id);
    if (!s) return;
    play_pcm_48k(s->pcm, s->frames);
    if (s->done || block) {
        ssdone_t *d = calloc(1, sizeof *d);
        if (!d) return;
        d->id = id;
        d->fn = s->done;
        d->ud = s->done_ud;
        d->block = block ? _Block_copy(block) : 0;
        d->ms = (u32)((u64)s->frames * 1000 / 48000);
        unsigned long t;
        if (pthread_create(&t, 0, ss_wait, d) == 0) pthread_detach(t);
        else { if (d->block) _Block_release(d->block); free(d); }
    }
}
void AudioServicesPlaySystemSound(u32 id) { play_sound(id, 0); }
void AudioServicesPlayAlertSound(u32 id) { play_sound(id ? id : 0x00001000, 0); }
void AudioServicesPlaySystemSoundWithCompletion(u32 id, void *block) { play_sound(id, block); }
void AudioServicesPlayAlertSoundWithCompletion(u32 id, void *block) { play_sound(id, block); }
OSStatus AudioServicesAddSystemSoundCompletion(u32 sid, void *rl, id mode, sscomplete_fn fn, void *ud) {
    (void)rl; (void)mode;
    ssound_t *s = sound_of(sid);
    if (!s) return -50;
    s->done = fn;
    s->done_ud = ud;
    return 0;
}
void AudioServicesRemoveSystemSoundCompletion(u32 id) { ssound_t *s = sound_of(id); if (s) { s->done = 0; s->done_ud = 0; } }
OSStatus AudioServicesGetPropertyInfo(u32 id_, u32 qs, const void *q, u32 *sz, u8 *w) { (void)id_; (void)qs; (void)q; if (sz) *sz = 4; if (w) *w = 1; return 0; }
OSStatus AudioServicesGetProperty(u32 id_, u32 qs, const void *q, u32 *sz, void *d) { (void)id_; (void)qs; (void)q; if (sz && *sz >= 4 && d) { *(u32 *)d = 0; *sz = 4; } return 0; }
OSStatus AudioServicesSetProperty(u32 id_, u32 qs, const void *q, u32 sz, const void *d) { (void)id_; (void)qs; (void)q; (void)sz; (void)d; return 0; }

/* para o avfaudio.c (NSSound e AVAudioPlayer usam o motor de saída; os sons curtos do sistema, a fila do mixer) */
void cx_play_pcm_48k(const i16 *pcm, u32 frames) { play_pcm_48k(pcm, frames); }

/* NSBeep: o alerta do sistema, dois tons curtos com decaimento (gerado uma vez) */
double sin(double);
double exp(double);
void cx_beep(void) {
    static i16 *pcm;
    static u32 n;
    if (!pcm) {
        n = 48000 * 18 / 100;   /* 180 ms */
        pcm = malloc((size_t)n * 4);
        if (!pcm) return;
        for (u32 i = 0; i < n; i++) {
            double t = (double)i / 48000.0;
            double f = t < 0.07 ? 1046.5 : 784.0;   /* Dó6 e Sol5 */
            double env = exp(-t * 18.0) * (t < 0.004 ? t / 0.004 : 1.0);
            double v = sin(2 * 3.14159265358979 * f * t) * env * 0.35;
            i16 s = (i16)(v * 32767);
            pcm[i * 2] = pcm[i * 2 + 1] = s;
        }
    }
    play_pcm_48k(pcm, n);
}
