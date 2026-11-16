/*
 * mOS — OpenAL 1.1 (OpenAL.framework) próprio, sobre a saída do CoreAudio do mOS (cx_cae, 48 kHz estéreo float).
 * Buffers MONO8/16, STEREO8/16 e float32; fontes com fila de buffers, loop, pitch, ganho (mín/máx), offsets em
 * segundos/amostras/bytes, estado INITIAL/PLAYING/PAUSED/STOPPED; ouvinte com posição, velocidade e orientação;
 * os 6 modelos de distância, cone, Doppler (fator, velocidade do som) e pan de potência constante para fontes mono
 * (as estéreo tocam sem espacializar, como a especificação manda). O mixer reamostra por interpolação linear.
 * alc*: um dispositivo de saída ("mOS"), contextos, atributos (ALC_FREQUENCY etc.), erros e strings.
 * Código original do mOS (MIT).
 */
#include <stddef.h>
#include <stdint.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int strcmp(const char *a, const char *b);
int strcasecmp(const char *a, const char *b);
double sqrt(double), pow(double, double), acos(double), sin(double), cos(double), atan2(double, double);
int pthread_mutex_init(void *m, const void *a);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);
void *cx_cae_new(double rate, unsigned chunk, void (*fn)(void *e, float *stereo, unsigned frames, const void *ts), void *ctx);
void *cx_cae_ctx(void *e);
int cx_cae_start(void *e);
void cx_cae_stop(void *e, int flush);
void cx_cae_free(void *e);

typedef int ALint, ALsizei, ALenum, ALCint, ALCenum, ALCsizei;
typedef unsigned ALuint;
typedef float ALfloat;
typedef double ALdouble;
typedef char ALboolean, ALCboolean, ALchar, ALCchar;
#define RATE 48000
enum {
    AL_NONE = 0, AL_SOURCE_RELATIVE = 0x202, AL_CONE_INNER_ANGLE = 0x1001, AL_CONE_OUTER_ANGLE = 0x1002, AL_PITCH = 0x1003,
    AL_POSITION = 0x1004, AL_DIRECTION = 0x1005, AL_VELOCITY = 0x1006, AL_LOOPING = 0x1007, AL_BUFFER = 0x1009, AL_GAIN = 0x100A,
    AL_MIN_GAIN = 0x100D, AL_MAX_GAIN = 0x100E, AL_ORIENTATION = 0x100F, AL_SOURCE_STATE = 0x1010, AL_INITIAL = 0x1011,
    AL_PLAYING = 0x1012, AL_PAUSED = 0x1013, AL_STOPPED = 0x1014, AL_BUFFERS_QUEUED = 0x1015, AL_BUFFERS_PROCESSED = 0x1016,
    AL_REFERENCE_DISTANCE = 0x1020, AL_ROLLOFF_FACTOR = 0x1021, AL_CONE_OUTER_GAIN = 0x1022, AL_MAX_DISTANCE = 0x1023,
    AL_SEC_OFFSET = 0x1024, AL_SAMPLE_OFFSET = 0x1025, AL_BYTE_OFFSET = 0x1026, AL_SOURCE_TYPE = 0x1027, AL_STATIC = 0x1028,
    AL_STREAMING = 0x1029, AL_UNDETERMINED = 0x1030, AL_FORMAT_MONO8 = 0x1100, AL_FORMAT_MONO16 = 0x1101, AL_FORMAT_STEREO8 = 0x1102,
    AL_FORMAT_STEREO16 = 0x1103, AL_FORMAT_MONO_FLOAT32 = 0x10010, AL_FORMAT_STEREO_FLOAT32 = 0x10011,
    AL_FREQUENCY = 0x2001, AL_BITS = 0x2002, AL_CHANNELS = 0x2003, AL_SIZE = 0x2004,
    AL_NO_ERROR = 0, AL_INVALID_NAME = 0xA001, AL_INVALID_ENUM = 0xA002, AL_INVALID_VALUE = 0xA003, AL_INVALID_OPERATION = 0xA004,
    AL_OUT_OF_MEMORY = 0xA005, AL_VENDOR = 0xB001, AL_VERSION = 0xB002, AL_RENDERER = 0xB003, AL_EXTENSIONS = 0xB004,
    AL_DOPPLER_FACTOR = 0xC000, AL_DOPPLER_VELOCITY = 0xC001, AL_SPEED_OF_SOUND = 0xC003, AL_DISTANCE_MODEL = 0xD000,
    AL_INVERSE_DISTANCE = 0xD001, AL_INVERSE_DISTANCE_CLAMPED = 0xD002, AL_LINEAR_DISTANCE = 0xD003,
    AL_LINEAR_DISTANCE_CLAMPED = 0xD004, AL_EXPONENT_DISTANCE = 0xD005, AL_EXPONENT_DISTANCE_CLAMPED = 0xD006,
    ALC_FREQUENCY = 0x1007, ALC_REFRESH = 0x1008, ALC_SYNC = 0x1009, ALC_MONO_SOURCES = 0x1010, ALC_STEREO_SOURCES = 0x1011,
    ALC_MAJOR_VERSION = 0x1000, ALC_MINOR_VERSION = 0x1001, ALC_ATTRIBUTES_SIZE = 0x1002, ALC_ALL_ATTRIBUTES = 0x1003,
    ALC_DEFAULT_DEVICE_SPECIFIER = 0x1004, ALC_DEVICE_SPECIFIER = 0x1005, ALC_EXTENSIONS = 0x1006,
    ALC_CAPTURE_DEVICE_SPECIFIER = 0x310, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER = 0x311, ALC_CAPTURE_SAMPLES = 0x312,
    ALC_INVALID_DEVICE = 0xA001, ALC_INVALID_CONTEXT = 0xA002, ALC_INVALID_ENUM = 0xA003, ALC_INVALID_VALUE = 0xA004,
};

typedef struct { int used, refs, ch, freq, bytes_per_sample; float *pcm; unsigned frames; } albuf;
typedef struct {
    int used, state, looping, relative, type;
    float pitch, gain, min_gain, max_gain, ref_dist, rolloff, max_dist, cone_in, cone_out, cone_out_gain;
    float pos[3], vel[3], dir[3];
    ALuint *queue;
    int nq, cq, qi;          /* qi: buffer da fila tocando agora */
    double frac;             /* posição no buffer atual (em quadros dele) */
    float last_l, last_r;    /* ganhos do bloco anterior (sem estalo) */
} alsrc;
typedef struct alctx {
    unsigned magic;
    struct aldev *dev;
    float lpos[3], lvel[3], lat[3], lup[3], lgain;
    int model;
    float doppler, doppler_vel, sos;
    alsrc *src; int nsrc;
    void *engine;
    int processing;
} alctx;
typedef struct aldev { unsigned magic; int err; alctx *ctx[8]; int nctx; } aldev;
#define DEV_MAGIC 0x414c4456u
#define CTX_MAGIC 0x414c4358u
static albuf *g_buf; static int g_nbuf;
static alctx *g_cur;
static int g_err;
static unsigned char g_lock[64];
static int g_lock_ok;
static void lk(void) { if (!g_lock_ok) { pthread_mutex_init(g_lock, 0); g_lock_ok = 1; } pthread_mutex_lock(g_lock); }
static void ulk(void) { pthread_mutex_unlock(g_lock); }
static void seterr(int e) { if (!g_err) g_err = e; }

/* ---------------------------------------------------------------- mixer */
static float vlen3(const float *v) { return (float)sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]); }
static float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static void cross3(const float *a, const float *b, float *o) { o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static float distance_gain(const alctx *c, const alsrc *s, float d) {
    float ref = s->ref_dist, rf = s->rolloff, mx = s->max_dist;
    int m = c->model;
    if (m == AL_INVERSE_DISTANCE_CLAMPED || m == AL_LINEAR_DISTANCE_CLAMPED || m == AL_EXPONENT_DISTANCE_CLAMPED) d = clampf(d, ref, mx > ref ? mx : ref);
    switch (m) {
    case AL_INVERSE_DISTANCE: case AL_INVERSE_DISTANCE_CLAMPED: { float den = ref + rf * (d - ref); return den > 0 ? ref / den : 1; }
    case AL_LINEAR_DISTANCE: case AL_LINEAR_DISTANCE_CLAMPED: { if (d > mx) d = mx; return mx > ref ? 1 - rf * (d - ref) / (mx - ref) : 1; }
    case AL_EXPONENT_DISTANCE: case AL_EXPONENT_DISTANCE_CLAMPED: return ref > 0 && d > 0 ? (float)pow(d / ref, -rf) : 1;
    default: return 1;
    }
}
/* ganhos esquerdo/direito e o pitch efetivo (Doppler) de uma fonte */
static void spatialize(const alctx *c, const alsrc *s, int mono, float *gl, float *gr, float *pitch) {
    float g = s->gain;
    float rel[3] = {s->pos[0], s->pos[1], s->pos[2]};
    if (!s->relative) for (int i = 0; i < 3; i++) rel[i] -= c->lpos[i];
    float d = vlen3(rel);
    *pitch = s->pitch;
    if (mono) {
        g *= distance_gain(c, s, d);
        if (vlen3(s->dir) > 0 && d > 0 && (s->cone_in < 360 || s->cone_out < 360)) {   /* cone: ângulo entre a direção e o ouvinte */
            float to_l[3] = {-rel[0] / d, -rel[1] / d, -rel[2] / d}, dn = vlen3(s->dir);
            float ang = (float)(acos(clampf(dot3(to_l, s->dir) / dn, -1, 1)) * 360 / 3.14159265358979323846);
            if (ang > s->cone_out) g *= s->cone_out_gain;
            else if (ang > s->cone_in && s->cone_out > s->cone_in) g *= 1 + (s->cone_out_gain - 1) * (ang - s->cone_in) / (s->cone_out - s->cone_in);
        }
        if (c->doppler > 0 && d > 0) {
            float u[3] = {rel[0] / d, rel[1] / d, rel[2] / d};
            float ss = c->sos * c->doppler_vel, vls = dot3(c->lvel, u), vss = dot3(s->vel, u);
            float sf = c->doppler;
            vls = vls < ss / sf ? vls : ss / sf;
            vss = vss < ss / sf ? vss : ss / sf;
            float den = ss - sf * vss;
            if (den > 0) *pitch *= (ss - sf * vls) / den;
        }
    }
    g = clampf(g, s->min_gain, s->max_gain) * c->lgain;
    if (!mono) { *gl = *gr = g; return; }
    float right[3], at[3] = {c->lat[0], c->lat[1], c->lat[2]}, upv[3] = {c->lup[0], c->lup[1], c->lup[2]};
    cross3(at, upv, right);
    float rn = vlen3(right), x = 0;
    if (d > 0 && rn > 0) x = clampf(dot3(rel, right) / (rn * d), -1, 1);   /* -1 esquerda, 1 direita */
    double th = (x + 1) * 3.14159265358979323846 / 4;                     /* potência constante */
    *gl = g * (float)cos(th);
    *gr = g * (float)sin(th);
}
static albuf *bufp(ALuint id) { return id && (int)id <= g_nbuf && g_buf[id - 1].used ? &g_buf[id - 1] : 0; }
static void mix_source(alctx *c, alsrc *s, float *out, unsigned frames) {
    if (s->state != AL_PLAYING || !s->nq) return;
    albuf *b0 = bufp(s->queue[s->qi]);
    if (!b0) { s->state = AL_STOPPED; return; }
    float gl, gr, pitch;
    spatialize(c, s, b0->ch == 1, &gl, &gr, &pitch);
    if (pitch <= 0) pitch = 1e-3f;
    for (unsigned i = 0; i < frames; i++) {
        albuf *b = bufp(s->queue[s->qi]);
        while (b && s->frac >= b->frames) {   /* fim do buffer: próximo da fila, loop ou parada */
            s->frac -= b->frames;
            if (s->qi + 1 < s->nq) { s->qi++; b = bufp(s->queue[s->qi]); }
            else if (s->looping) { s->qi = 0; b = bufp(s->queue[0]); }
            else { s->state = AL_STOPPED; s->qi = s->nq; s->frac = 0; return; }
        }
        if (!b || !b->frames) { s->state = AL_STOPPED; return; }
        double step = pitch * (double)b->freq / RATE;
        unsigned f0 = (unsigned)s->frac, f1 = f0 + 1 < b->frames ? f0 + 1 : f0;
        float t = (float)(s->frac - f0);
        float l, r;
        if (b->ch == 1) { l = r = b->pcm[f0] + (b->pcm[f1] - b->pcm[f0]) * t; }
        else { l = b->pcm[2 * f0] + (b->pcm[2 * f1] - b->pcm[2 * f0]) * t; r = b->pcm[2 * f0 + 1] + (b->pcm[2 * f1 + 1] - b->pcm[2 * f0 + 1]) * t; }
        float k = frames > 1 ? (float)i / (frames - 1) : 1;   /* rampa do ganho anterior para o novo */
        out[2 * i] += l * (s->last_l + (gl - s->last_l) * k);
        out[2 * i + 1] += r * (s->last_r + (gr - s->last_r) * k);
        s->frac += step;
    }
    s->last_l = gl; s->last_r = gr;
}
/* também para o teste de host: mistura frames quadros do contexto em out (estéreo intercalado) */
void cx_openal_render(void *ctx, float *out, unsigned frames) {
    alctx *c = ctx;
    memset(out, 0, sizeof(float) * 2 * frames);
    if (!c || c->magic != CTX_MAGIC) return;
    lk();
    for (int i = 0; i < c->nsrc; i++) if (c->src[i].used) mix_source(c, &c->src[i], out, frames);
    ulk();
}
static void engine_cb(void *e, float *stereo, unsigned frames, const void *ts) { (void)ts; cx_openal_render(cx_cae_ctx(e), stereo, frames); }
static void ensure_engine(alctx *c) {
    if (c->engine) return;
    c->engine = cx_cae_new(RATE, 512, engine_cb, c);
    if (c->engine) cx_cae_start(c->engine);
}

/* ---------------------------------------------------------------- alc */
static int g_alc_err;
void *alcOpenDevice(const ALCchar *name) {
    if (name && strcmp(name, "mOS") && name[0]) { g_alc_err = ALC_INVALID_VALUE; return 0; }
    aldev *d = calloc(1, sizeof *d);
    if (d) d->magic = DEV_MAGIC;
    return d;
}
static aldev *devp(void *p) { aldev *d = p; return d && d->magic == DEV_MAGIC ? d : 0; }
static alctx *ctxp(void *p) { alctx *c = p; return c && c->magic == CTX_MAGIC ? c : 0; }
ALCboolean alcCloseDevice(void *p) { aldev *d = devp(p); if (!d || d->nctx) { g_alc_err = ALC_INVALID_DEVICE; return 0; } d->magic = 0; free(d); return 1; }
void *alcCreateContext(void *p, const ALCint *attrs) {
    aldev *d = devp(p);
    if (!d || d->nctx >= 8) { g_alc_err = ALC_INVALID_DEVICE; return 0; }
    for (const ALCint *a = attrs; a && a[0]; a += 2)
        if (a[0] == ALC_FREQUENCY && a[1] <= 0) { g_alc_err = ALC_INVALID_VALUE; return 0; }   /* o mOS mistura a 48 kHz e reamostra o resto */
    alctx *c = calloc(1, sizeof *c);
    if (!c) return 0;
    c->magic = CTX_MAGIC; c->dev = d;
    c->lat[2] = -1; c->lup[1] = 1; c->lgain = 1;
    c->model = AL_INVERSE_DISTANCE_CLAMPED; c->doppler = 1; c->doppler_vel = 1; c->sos = 343.3f;
    c->processing = 1;
    d->ctx[d->nctx++] = c;
    return c;
}
void alcDestroyContext(void *p) {
    alctx *c = ctxp(p);
    if (!c) { g_alc_err = ALC_INVALID_CONTEXT; return; }
    if (c->engine) { cx_cae_stop(c->engine, 0); cx_cae_free(c->engine); }
    aldev *d = c->dev;
    for (int i = 0; i < d->nctx; i++) if (d->ctx[i] == c) { d->ctx[i] = d->ctx[--d->nctx]; break; }
    if (g_cur == c) g_cur = 0;
    for (int i = 0; i < c->nsrc; i++) free(c->src[i].queue);
    free(c->src);
    c->magic = 0;
    free(c);
}
ALCboolean alcMakeContextCurrent(void *p) { if (p && !ctxp(p)) { g_alc_err = ALC_INVALID_CONTEXT; return 0; } g_cur = p; return 1; }
void *alcGetCurrentContext(void) { return g_cur; }
void *alcGetContextsDevice(void *p) { alctx *c = ctxp(p); return c ? c->dev : 0; }
void alcProcessContext(void *p) { alctx *c = ctxp(p); if (c) c->processing = 1; }
void alcSuspendContext(void *p) { alctx *c = ctxp(p); if (c) c->processing = 0; }
ALCenum alcGetError(void *p) { (void)p; int e = g_alc_err; g_alc_err = 0; return e; }
const ALCchar *alcGetString(void *p, ALCenum param) {
    switch (param) {
    case 0: return "No Error";
    case ALC_DEFAULT_DEVICE_SPECIFIER: case ALC_DEVICE_SPECIFIER: return p ? "mOS" : "mOS\0";
    case ALC_EXTENSIONS: return "ALC_ENUMERATION_EXT";
    case ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER: case ALC_CAPTURE_DEVICE_SPECIFIER: return "";
    case ALC_INVALID_DEVICE: return "Invalid Device";
    case ALC_INVALID_CONTEXT: return "Invalid Context";
    case ALC_INVALID_ENUM: return "Invalid Enum";
    case ALC_INVALID_VALUE: return "Invalid Value";
    default: g_alc_err = ALC_INVALID_ENUM; return 0;
    }
}
void alcGetIntegerv(void *p, ALCenum param, ALCsizei n, ALCint *v) {
    if (!v || n <= 0) { g_alc_err = ALC_INVALID_VALUE; return; }
    static const ALCint attrs[] = {ALC_FREQUENCY, RATE, ALC_REFRESH, 93, ALC_SYNC, 0, ALC_MONO_SOURCES, 255, ALC_STEREO_SOURCES, 1, 0};
    switch (param) {
    case ALC_MAJOR_VERSION: v[0] = 1; return;
    case ALC_MINOR_VERSION: v[0] = 1; return;
    case ALC_FREQUENCY: v[0] = RATE; return;
    case ALC_REFRESH: v[0] = 93; return;
    case ALC_SYNC: v[0] = 0; return;
    case ALC_MONO_SOURCES: v[0] = 255; return;
    case ALC_STEREO_SOURCES: v[0] = 1; return;
    case ALC_ATTRIBUTES_SIZE: v[0] = (ALCint)(sizeof attrs / sizeof attrs[0]); return;
    case ALC_ALL_ATTRIBUTES: if (!devp(p) || n < (ALCsizei)(sizeof attrs / sizeof attrs[0])) { g_alc_err = ALC_INVALID_VALUE; return; } memcpy(v, attrs, sizeof attrs); return;
    case ALC_CAPTURE_SAMPLES: v[0] = 0; return;
    default: g_alc_err = ALC_INVALID_ENUM;
    }
}
ALCboolean alcIsExtensionPresent(void *p, const ALCchar *e) { (void)p; return e && !strcasecmp(e, "ALC_ENUMERATION_EXT"); }
ALCenum alcGetEnumValue(void *p, const ALCchar *n) {
    (void)p;
    static const struct { const char *n; int v; } t[] = {{"ALC_FREQUENCY", ALC_FREQUENCY}, {"ALC_REFRESH", ALC_REFRESH}, {"ALC_SYNC", ALC_SYNC},
        {"ALC_MONO_SOURCES", ALC_MONO_SOURCES}, {"ALC_STEREO_SOURCES", ALC_STEREO_SOURCES}, {"ALC_DEVICE_SPECIFIER", ALC_DEVICE_SPECIFIER},
        {"ALC_DEFAULT_DEVICE_SPECIFIER", ALC_DEFAULT_DEVICE_SPECIFIER}, {"ALC_EXTENSIONS", ALC_EXTENSIONS}, {"ALC_NO_ERROR", 0},
        {"ALC_INVALID_DEVICE", ALC_INVALID_DEVICE}, {"ALC_INVALID_CONTEXT", ALC_INVALID_CONTEXT}, {"ALC_INVALID_ENUM", ALC_INVALID_ENUM},
        {"ALC_INVALID_VALUE", ALC_INVALID_VALUE}, {"ALC_MAJOR_VERSION", ALC_MAJOR_VERSION}, {"ALC_MINOR_VERSION", ALC_MINOR_VERSION}};
    for (unsigned i = 0; n && i < sizeof t / sizeof t[0]; i++) if (!strcmp(t[i].n, n)) return t[i].v;
    return 0;
}

/* ---------------------------------------------------------------- buffers */
ALenum alGetError(void) { int e = g_err; g_err = 0; return e; }
void alGenBuffers(ALsizei n, ALuint *ids) {
    if (n < 0 || (n && !ids)) { seterr(AL_INVALID_VALUE); return; }
    lk();
    for (ALsizei k = 0; k < n; k++) {
        int slot = -1;
        for (int i = 0; i < g_nbuf; i++) if (!g_buf[i].used) { slot = i; break; }
        if (slot < 0) {
            albuf *x = realloc(g_buf, sizeof *x * (size_t)(g_nbuf + 16));
            if (!x) { seterr(AL_OUT_OF_MEMORY); break; }
            memset(x + g_nbuf, 0, sizeof *x * 16);
            g_buf = x; slot = g_nbuf; g_nbuf += 16;
        }
        memset(&g_buf[slot], 0, sizeof g_buf[slot]);
        g_buf[slot].used = 1;
        ids[k] = (ALuint)slot + 1;
    }
    ulk();
}
void alDeleteBuffers(ALsizei n, const ALuint *ids) {
    if (n < 0) { seterr(AL_INVALID_VALUE); return; }
    lk();
    for (ALsizei k = 0; k < n; k++) {
        if (!ids[k]) continue;
        albuf *b = bufp(ids[k]);
        if (!b) { seterr(AL_INVALID_NAME); break; }
        if (b->refs) { seterr(AL_INVALID_OPERATION); break; }   /* ainda numa fonte */
        free(b->pcm);
        memset(b, 0, sizeof *b);
    }
    ulk();
}
ALboolean alIsBuffer(ALuint id) { return id == 0 || bufp(id) != 0; }
void alBufferData(ALuint id, ALenum fmt, const void *data, ALsizei size, ALsizei freq) {
    albuf *b = bufp(id);
    if (!b) { seterr(AL_INVALID_NAME); return; }
    int ch, bps;
    switch (fmt) {
    case AL_FORMAT_MONO8: ch = 1; bps = 1; break;
    case AL_FORMAT_MONO16: ch = 1; bps = 2; break;
    case AL_FORMAT_STEREO8: ch = 2; bps = 1; break;
    case AL_FORMAT_STEREO16: ch = 2; bps = 2; break;
    case AL_FORMAT_MONO_FLOAT32: ch = 1; bps = 4; break;
    case AL_FORMAT_STEREO_FLOAT32: ch = 2; bps = 4; break;
    default: seterr(AL_INVALID_ENUM); return;
    }
    if (size < 0 || freq <= 0 || (size && !data) || size % (ch * bps)) { seterr(AL_INVALID_VALUE); return; }
    if (b->refs) { seterr(AL_INVALID_OPERATION); return; }
    unsigned frames = (unsigned)size / (unsigned)(ch * bps);
    float *p = frames ? malloc(sizeof(float) * frames * ch) : 0;
    if (frames && !p) { seterr(AL_OUT_OF_MEMORY); return; }
    for (unsigned i = 0; i < frames * (unsigned)ch; i++) {
        if (bps == 1) p[i] = (((const unsigned char *)data)[i] - 128) / 128.0f;   /* 8 bits sem sinal */
        else if (bps == 2) p[i] = ((const short *)data)[i] / 32768.0f;
        else p[i] = ((const float *)data)[i];
    }
    lk();
    free(b->pcm);
    b->pcm = p; b->frames = frames; b->ch = ch; b->freq = freq; b->bytes_per_sample = bps;
    ulk();
}
void alGetBufferi(ALuint id, ALenum p, ALint *v) {
    albuf *b = bufp(id);
    if (!b) { seterr(AL_INVALID_NAME); return; }
    if (!v) { seterr(AL_INVALID_VALUE); return; }
    switch (p) {
    case AL_FREQUENCY: *v = b->freq; break;
    case AL_BITS: *v = b->bytes_per_sample * 8; break;
    case AL_CHANNELS: *v = b->ch; break;
    case AL_SIZE: *v = (ALint)(b->frames * (unsigned)b->ch * (unsigned)b->bytes_per_sample); break;
    default: seterr(AL_INVALID_ENUM);
    }
}

/* ---------------------------------------------------------------- fontes */
static alsrc *srcp(ALuint id) {
    if (!g_cur || !id || (int)id > g_cur->nsrc || !g_cur->src[id - 1].used) return 0;
    return &g_cur->src[id - 1];
}
static void src_defaults(alsrc *s) {
    memset(s, 0, sizeof *s);
    s->used = 1; s->state = AL_INITIAL; s->type = AL_UNDETERMINED;
    s->pitch = 1; s->gain = 1; s->min_gain = 0; s->max_gain = 1; s->ref_dist = 1; s->rolloff = 1; s->max_dist = 3.4e38f;
    s->cone_in = 360; s->cone_out = 360; s->cone_out_gain = 0;
}
void alGenSources(ALsizei n, ALuint *ids) {
    if (!g_cur) { seterr(AL_INVALID_OPERATION); return; }
    if (n < 0 || (n && !ids)) { seterr(AL_INVALID_VALUE); return; }
    lk();
    for (ALsizei k = 0; k < n; k++) {
        int slot = -1;
        for (int i = 0; i < g_cur->nsrc; i++) if (!g_cur->src[i].used) { slot = i; break; }
        if (slot < 0) {
            alsrc *x = realloc(g_cur->src, sizeof *x * (size_t)(g_cur->nsrc + 16));
            if (!x) { seterr(AL_OUT_OF_MEMORY); break; }
            memset(x + g_cur->nsrc, 0, sizeof *x * 16);
            g_cur->src = x; slot = g_cur->nsrc; g_cur->nsrc += 16;
        }
        src_defaults(&g_cur->src[slot]);
        ids[k] = (ALuint)slot + 1;
    }
    ulk();
}
static void unref_all(alsrc *s) { for (int i = 0; i < s->nq; i++) { albuf *b = bufp(s->queue[i]); if (b && b->refs) b->refs--; } s->nq = 0; s->qi = 0; s->frac = 0; }
void alDeleteSources(ALsizei n, const ALuint *ids) {
    lk();
    for (ALsizei k = 0; k < n; k++) {
        alsrc *s = srcp(ids[k]);
        if (!s) { seterr(AL_INVALID_NAME); break; }
        unref_all(s);
        free(s->queue);
        memset(s, 0, sizeof *s);
    }
    ulk();
}
ALboolean alIsSource(ALuint id) { return srcp(id) != 0; }
static int src_offset_frames(alsrc *s, double *frames_total) {
    double f = 0;
    for (int i = 0; i < s->qi && i < s->nq; i++) { albuf *b = bufp(s->queue[i]); if (b) f += b->frames; }
    if (s->qi < s->nq) f += s->frac;
    *frames_total = f;
    return 0;
}
static void set_offset(alsrc *s, ALenum p, double v) {
    double fr = v;
    albuf *b = s->nq ? bufp(s->queue[0]) : 0;
    if (!b) { seterr(AL_INVALID_OPERATION); return; }
    if (p == AL_SEC_OFFSET) fr = v * b->freq;
    else if (p == AL_BYTE_OFFSET) fr = v / (b->ch * b->bytes_per_sample);
    int qi = 0;
    while (qi < s->nq) { albuf *x = bufp(s->queue[qi]); if (!x || fr < x->frames) break; fr -= x->frames; qi++; }
    if (qi >= s->nq) { seterr(AL_INVALID_VALUE); return; }
    s->qi = qi; s->frac = fr;
}
static void src_set_buffer(alsrc *s, ALuint id) {
    if (s->state == AL_PLAYING || s->state == AL_PAUSED) { seterr(AL_INVALID_OPERATION); return; }
    if (id && !bufp(id)) { seterr(AL_INVALID_VALUE); return; }
    unref_all(s);
    if (id) {
        if (!s->cq) { s->queue = malloc(sizeof(ALuint) * 4); s->cq = 4; }
        s->queue[0] = id; s->nq = 1; bufp(id)->refs++;
        s->type = AL_STATIC;
    } else s->type = AL_UNDETERMINED;
}
void alSourcefv(ALuint id, ALenum p, const ALfloat *v) {
    lk();
    alsrc *s = srcp(id);
    if (!s) { seterr(AL_INVALID_NAME); ulk(); return; }
    if (!v) { seterr(AL_INVALID_VALUE); ulk(); return; }
    switch (p) {
    case AL_PITCH: if (v[0] <= 0) seterr(AL_INVALID_VALUE); else s->pitch = v[0]; break;
    case AL_GAIN: if (v[0] < 0) seterr(AL_INVALID_VALUE); else s->gain = v[0]; break;
    case AL_MIN_GAIN: s->min_gain = clampf(v[0], 0, 1); break;
    case AL_MAX_GAIN: s->max_gain = clampf(v[0], 0, 1); break;
    case AL_REFERENCE_DISTANCE: s->ref_dist = v[0]; break;
    case AL_ROLLOFF_FACTOR: s->rolloff = v[0]; break;
    case AL_MAX_DISTANCE: s->max_dist = v[0]; break;
    case AL_CONE_INNER_ANGLE: s->cone_in = v[0]; break;
    case AL_CONE_OUTER_ANGLE: s->cone_out = v[0]; break;
    case AL_CONE_OUTER_GAIN: s->cone_out_gain = clampf(v[0], 0, 1); break;
    case AL_POSITION: memcpy(s->pos, v, 12); break;
    case AL_VELOCITY: memcpy(s->vel, v, 12); break;
    case AL_DIRECTION: memcpy(s->dir, v, 12); break;
    case AL_SEC_OFFSET: case AL_SAMPLE_OFFSET: case AL_BYTE_OFFSET: set_offset(s, p, v[0]); break;
    default: seterr(AL_INVALID_ENUM);
    }
    ulk();
}
void alSourcef(ALuint id, ALenum p, ALfloat v) { float a[3] = {v, 0, 0}; alSourcefv(id, p, a); }
void alSource3f(ALuint id, ALenum p, ALfloat x, ALfloat y, ALfloat z) { float a[3] = {x, y, z}; alSourcefv(id, p, a); }
void alSourcei(ALuint id, ALenum p, ALint v) {
    lk();
    alsrc *s = srcp(id);
    if (!s) { seterr(AL_INVALID_NAME); ulk(); return; }
    switch (p) {
    case AL_LOOPING: s->looping = v != 0; break;
    case AL_SOURCE_RELATIVE: s->relative = v != 0; break;
    case AL_BUFFER: src_set_buffer(s, (ALuint)v); break;
    case AL_SEC_OFFSET: case AL_SAMPLE_OFFSET: case AL_BYTE_OFFSET: set_offset(s, p, v); break;
    default: ulk(); { float f = (float)v; alSourcefv(id, p, &f); } return;
    }
    ulk();
}
void alSource3i(ALuint id, ALenum p, ALint x, ALint y, ALint z) { alSource3f(id, p, (float)x, (float)y, (float)z); }
void alSourceiv(ALuint id, ALenum p, const ALint *v) { if (!v) { seterr(AL_INVALID_VALUE); return; } if (p == AL_POSITION || p == AL_VELOCITY || p == AL_DIRECTION) alSource3i(id, p, v[0], v[1], v[2]); else alSourcei(id, p, v[0]); }
void alGetSourcefv(ALuint id, ALenum p, ALfloat *v) {
    lk();
    alsrc *s = srcp(id);
    if (!s || !v) { seterr(s ? AL_INVALID_VALUE : AL_INVALID_NAME); ulk(); return; }
    double f;
    albuf *b = s->nq ? bufp(s->queue[s->qi < s->nq ? s->qi : 0]) : 0;
    switch (p) {
    case AL_PITCH: v[0] = s->pitch; break;
    case AL_GAIN: v[0] = s->gain; break;
    case AL_MIN_GAIN: v[0] = s->min_gain; break;
    case AL_MAX_GAIN: v[0] = s->max_gain; break;
    case AL_REFERENCE_DISTANCE: v[0] = s->ref_dist; break;
    case AL_ROLLOFF_FACTOR: v[0] = s->rolloff; break;
    case AL_MAX_DISTANCE: v[0] = s->max_dist; break;
    case AL_CONE_INNER_ANGLE: v[0] = s->cone_in; break;
    case AL_CONE_OUTER_ANGLE: v[0] = s->cone_out; break;
    case AL_CONE_OUTER_GAIN: v[0] = s->cone_out_gain; break;
    case AL_POSITION: memcpy(v, s->pos, 12); break;
    case AL_VELOCITY: memcpy(v, s->vel, 12); break;
    case AL_DIRECTION: memcpy(v, s->dir, 12); break;
    case AL_SEC_OFFSET: src_offset_frames(s, &f); v[0] = b ? (float)(f / b->freq) : 0; break;
    case AL_SAMPLE_OFFSET: src_offset_frames(s, &f); v[0] = (float)(long)f; break;
    case AL_BYTE_OFFSET: src_offset_frames(s, &f); v[0] = b ? (float)((long)f * b->ch * b->bytes_per_sample) : 0; break;
    default: seterr(AL_INVALID_ENUM);
    }
    ulk();
}
void alGetSourcef(ALuint id, ALenum p, ALfloat *v) { float a[3]; alGetSourcefv(id, p, a); if (v) *v = a[0]; }
void alGetSource3f(ALuint id, ALenum p, ALfloat *x, ALfloat *y, ALfloat *z) { float a[3] = {0}; alGetSourcefv(id, p, a); if (x) *x = a[0]; if (y) *y = a[1]; if (z) *z = a[2]; }
void alGetSourcei(ALuint id, ALenum p, ALint *v) {
    lk();
    alsrc *s = srcp(id);
    if (!s || !v) { seterr(s ? AL_INVALID_VALUE : AL_INVALID_NAME); ulk(); return; }
    switch (p) {
    case AL_SOURCE_STATE: *v = s->state; break;
    case AL_LOOPING: *v = s->looping; break;
    case AL_SOURCE_RELATIVE: *v = s->relative; break;
    case AL_BUFFER: *v = s->nq ? (ALint)s->queue[s->qi < s->nq ? s->qi : s->nq - 1] : 0; break;
    case AL_BUFFERS_QUEUED: *v = s->nq; break;
    case AL_BUFFERS_PROCESSED: *v = s->type == AL_STREAMING ? (s->state == AL_STOPPED ? s->nq : s->qi) : 0; break;
    case AL_SOURCE_TYPE: *v = s->type; break;
    default: { ulk(); float f[3] = {0}; alGetSourcefv(id, p, f); *v = (ALint)f[0]; return; }
    }
    ulk();
}
void alGetSourceiv(ALuint id, ALenum p, ALint *v) { if (p == AL_POSITION || p == AL_VELOCITY || p == AL_DIRECTION) { float f[3] = {0}; alGetSourcefv(id, p, f); for (int i = 0; i < 3; i++) v[i] = (ALint)f[i]; } else alGetSourcei(id, p, v); }
void alSourceQueueBuffers(ALuint id, ALsizei n, const ALuint *ids) {
    lk();
    alsrc *s = srcp(id);
    if (!s) { seterr(AL_INVALID_NAME); ulk(); return; }
    if (s->type == AL_STATIC) { seterr(AL_INVALID_OPERATION); ulk(); return; }
    for (ALsizei k = 0; k < n; k++) if (!bufp(ids[k])) { seterr(AL_INVALID_NAME); ulk(); return; }
    if (s->nq + n > s->cq) {
        int nc = (s->nq + n) * 2;
        ALuint *q = realloc(s->queue, sizeof(ALuint) * (size_t)nc);
        if (!q) { seterr(AL_OUT_OF_MEMORY); ulk(); return; }
        s->queue = q; s->cq = nc;
    }
    if (s->state == AL_STOPPED && s->qi >= s->nq) {   /* parou no fim: o que entra agora começa do zero */
        unref_all(s);
    }
    for (ALsizei k = 0; k < n; k++) { s->queue[s->nq++] = ids[k]; bufp(ids[k])->refs++; }
    s->type = AL_STREAMING;
    ulk();
}
void alSourceUnqueueBuffers(ALuint id, ALsizei n, ALuint *ids) {
    lk();
    alsrc *s = srcp(id);
    if (!s) { seterr(AL_INVALID_NAME); ulk(); return; }
    int processed = s->state == AL_STOPPED ? s->nq : s->qi;
    if (s->type != AL_STREAMING || n > processed) { seterr(AL_INVALID_VALUE); ulk(); return; }
    for (ALsizei k = 0; k < n; k++) { ids[k] = s->queue[k]; albuf *b = bufp(ids[k]); if (b && b->refs) b->refs--; }
    for (int k = n; k < s->nq; k++) s->queue[k - n] = s->queue[k];
    s->nq -= n;
    s->qi = s->qi >= n ? s->qi - n : 0;
    if (!s->nq) { s->qi = 0; s->frac = 0; }
    ulk();
}
void alSourcePlay(ALuint id) {
    lk();
    alsrc *s = srcp(id);
    if (!s) { seterr(AL_INVALID_NAME); ulk(); return; }
    if (s->state != AL_PAUSED) { if (s->state != AL_INITIAL || s->qi >= s->nq) { s->qi = 0; s->frac = 0; } }
    s->state = s->nq ? AL_PLAYING : AL_STOPPED;
    s->last_l = s->last_r = -1;   /* sem rampa na partida */
    ulk();
    if (s->last_l < 0) {
        float gl, gr, pt;
        albuf *b = s->nq ? bufp(s->queue[s->qi]) : 0;
        if (b) { spatialize(g_cur, s, b->ch == 1, &gl, &gr, &pt); s->last_l = gl; s->last_r = gr; } else s->last_l = s->last_r = 0;
    }
    if (s->state == AL_PLAYING) ensure_engine(g_cur);
}
void alSourcePause(ALuint id) { lk(); alsrc *s = srcp(id); if (!s) seterr(AL_INVALID_NAME); else if (s->state == AL_PLAYING) s->state = AL_PAUSED; ulk(); }
void alSourceStop(ALuint id) { lk(); alsrc *s = srcp(id); if (!s) seterr(AL_INVALID_NAME); else if (s->state != AL_INITIAL) { s->state = AL_STOPPED; s->qi = s->nq; s->frac = 0; } ulk(); }
void alSourceRewind(ALuint id) { lk(); alsrc *s = srcp(id); if (!s) seterr(AL_INVALID_NAME); else { s->state = AL_INITIAL; s->qi = 0; s->frac = 0; } ulk(); }
void alSourcePlayv(ALsizei n, const ALuint *ids) { for (ALsizei i = 0; i < n; i++) alSourcePlay(ids[i]); }
void alSourcePausev(ALsizei n, const ALuint *ids) { for (ALsizei i = 0; i < n; i++) alSourcePause(ids[i]); }
void alSourceStopv(ALsizei n, const ALuint *ids) { for (ALsizei i = 0; i < n; i++) alSourceStop(ids[i]); }
void alSourceRewindv(ALsizei n, const ALuint *ids) { for (ALsizei i = 0; i < n; i++) alSourceRewind(ids[i]); }

/* ---------------------------------------------------------------- ouvinte e estado global */
void alListenerfv(ALenum p, const ALfloat *v) {
    if (!g_cur) { seterr(AL_INVALID_OPERATION); return; }
    if (!v) { seterr(AL_INVALID_VALUE); return; }
    lk();
    switch (p) {
    case AL_GAIN: if (v[0] < 0) seterr(AL_INVALID_VALUE); else g_cur->lgain = v[0]; break;
    case AL_POSITION: memcpy(g_cur->lpos, v, 12); break;
    case AL_VELOCITY: memcpy(g_cur->lvel, v, 12); break;
    case AL_ORIENTATION: memcpy(g_cur->lat, v, 12); memcpy(g_cur->lup, v + 3, 12); break;
    default: seterr(AL_INVALID_ENUM);
    }
    ulk();
}
void alListenerf(ALenum p, ALfloat v) { float a[3] = {v, 0, 0}; alListenerfv(p, a); }
void alListener3f(ALenum p, ALfloat x, ALfloat y, ALfloat z) { float a[3] = {x, y, z}; alListenerfv(p, a); }
void alListeneri(ALenum p, ALint v) { alListenerf(p, (float)v); }
void alGetListenerfv(ALenum p, ALfloat *v) {
    if (!g_cur || !v) { seterr(g_cur ? AL_INVALID_VALUE : AL_INVALID_OPERATION); return; }
    switch (p) {
    case AL_GAIN: v[0] = g_cur->lgain; break;
    case AL_POSITION: memcpy(v, g_cur->lpos, 12); break;
    case AL_VELOCITY: memcpy(v, g_cur->lvel, 12); break;
    case AL_ORIENTATION: memcpy(v, g_cur->lat, 12); memcpy(v + 3, g_cur->lup, 12); break;
    default: seterr(AL_INVALID_ENUM);
    }
}
void alGetListenerf(ALenum p, ALfloat *v) { float a[6]; alGetListenerfv(p, a); if (v) *v = a[0]; }
void alGetListener3f(ALenum p, ALfloat *x, ALfloat *y, ALfloat *z) { float a[6] = {0}; alGetListenerfv(p, a); if (x) *x = a[0]; if (y) *y = a[1]; if (z) *z = a[2]; }
void alDistanceModel(ALenum m) {
    if (!g_cur) { seterr(AL_INVALID_OPERATION); return; }
    if (m != AL_NONE && (m < AL_INVERSE_DISTANCE || m > AL_EXPONENT_DISTANCE_CLAMPED)) { seterr(AL_INVALID_VALUE); return; }
    g_cur->model = m;
}
void alDopplerFactor(ALfloat v) { if (!g_cur) { seterr(AL_INVALID_OPERATION); return; } if (v < 0) seterr(AL_INVALID_VALUE); else g_cur->doppler = v; }
void alDopplerVelocity(ALfloat v) { if (!g_cur) { seterr(AL_INVALID_OPERATION); return; } if (v <= 0) seterr(AL_INVALID_VALUE); else g_cur->doppler_vel = v; }
void alSpeedOfSound(ALfloat v) { if (!g_cur) { seterr(AL_INVALID_OPERATION); return; } if (v <= 0) seterr(AL_INVALID_VALUE); else g_cur->sos = v; }
ALfloat alGetFloat(ALenum p) {
    if (!g_cur) { seterr(AL_INVALID_OPERATION); return 0; }
    switch (p) {
    case AL_DOPPLER_FACTOR: return g_cur->doppler;
    case AL_DOPPLER_VELOCITY: return g_cur->doppler_vel;
    case AL_SPEED_OF_SOUND: return g_cur->sos;
    case AL_DISTANCE_MODEL: return (float)g_cur->model;
    default: seterr(AL_INVALID_ENUM); return 0;
    }
}
ALint alGetInteger(ALenum p) { return (ALint)alGetFloat(p); }
ALdouble alGetDouble(ALenum p) { return alGetFloat(p); }
ALboolean alGetBoolean(ALenum p) { return alGetFloat(p) != 0; }
void alGetFloatv(ALenum p, ALfloat *v) { if (v) *v = alGetFloat(p); }
void alGetIntegerv(ALenum p, ALint *v) { if (v) *v = alGetInteger(p); }
const ALchar *alGetString(ALenum p) {
    switch (p) {
    case AL_VENDOR: return "mOS";
    case AL_VERSION: return "1.1 mOS";
    case AL_RENDERER: return "mOS Software";
    case AL_EXTENSIONS: return "AL_EXT_FLOAT32 AL_EXT_LINEAR_DISTANCE AL_EXT_EXPONENT_DISTANCE AL_EXT_OFFSET";
    case AL_NO_ERROR: return "No Error";
    case AL_INVALID_NAME: return "Invalid Name";
    case AL_INVALID_ENUM: return "Invalid Enum";
    case AL_INVALID_VALUE: return "Invalid Value";
    case AL_INVALID_OPERATION: return "Invalid Operation";
    case AL_OUT_OF_MEMORY: return "Out of Memory";
    default: seterr(AL_INVALID_ENUM); return 0;
    }
}
ALboolean alIsExtensionPresent(const ALchar *e) {
    static const char *ext[] = {"AL_EXT_FLOAT32", "AL_EXT_LINEAR_DISTANCE", "AL_EXT_EXPONENT_DISTANCE", "AL_EXT_OFFSET"};
    for (unsigned i = 0; e && i < 4; i++) if (!strcasecmp(e, ext[i])) return 1;
    return 0;
}
ALenum alGetEnumValue(const ALchar *n) {
    static const struct { const char *n; int v; } t[] = {
        {"AL_FORMAT_MONO8", AL_FORMAT_MONO8}, {"AL_FORMAT_MONO16", AL_FORMAT_MONO16}, {"AL_FORMAT_STEREO8", AL_FORMAT_STEREO8},
        {"AL_FORMAT_STEREO16", AL_FORMAT_STEREO16}, {"AL_FORMAT_MONO_FLOAT32", AL_FORMAT_MONO_FLOAT32},
        {"AL_FORMAT_STEREO_FLOAT32", AL_FORMAT_STEREO_FLOAT32}, {"AL_INVERSE_DISTANCE", AL_INVERSE_DISTANCE},
        {"AL_INVERSE_DISTANCE_CLAMPED", AL_INVERSE_DISTANCE_CLAMPED}, {"AL_LINEAR_DISTANCE", AL_LINEAR_DISTANCE},
        {"AL_LINEAR_DISTANCE_CLAMPED", AL_LINEAR_DISTANCE_CLAMPED}, {"AL_EXPONENT_DISTANCE", AL_EXPONENT_DISTANCE},
        {"AL_EXPONENT_DISTANCE_CLAMPED", AL_EXPONENT_DISTANCE_CLAMPED}, {"AL_SEC_OFFSET", AL_SEC_OFFSET},
        {"AL_SAMPLE_OFFSET", AL_SAMPLE_OFFSET}, {"AL_BYTE_OFFSET", AL_BYTE_OFFSET}, {"AL_SOURCE_TYPE", AL_SOURCE_TYPE}};
    for (unsigned i = 0; n && i < sizeof t / sizeof t[0]; i++) if (!strcmp(t[i].n, n)) return t[i].v;
    return 0;
}
void alEnable(ALenum c) { (void)c; seterr(AL_INVALID_ENUM); }   /* o AL 1.1 não tem capacidades */
void alDisable(ALenum c) { (void)c; seterr(AL_INVALID_ENUM); }
ALboolean alIsEnabled(ALenum c) { (void)c; seterr(AL_INVALID_ENUM); return 0; }
