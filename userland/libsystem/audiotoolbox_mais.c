/*
 * mOS — mais AudioToolbox pedido pelo corpus:
 *  - CAClock: relógio de transporte sobre o tempo do host (CAClockNew/Start/Stop/GetCurrentTime/SetCurrentTime...),
 *    em host time, segundos, amostras (taxa da propriedade de amostragem), batidas (mapa de tempo kCAClockProperty_TempoMap)
 *    e SMPTE (24, 25, 30, 29,97 drop/sem drop, 60...), com SMPTEOffset;
 *  - AudioFileGetGlobalInfo(Size): os tipos que o AudioFile do mOS lê e grava, nomes, extensões, UTIs, MIME e formatos;
 *  - AudioComponentInstantiate (o bloco recebe a instância), AudioConverterFillBuffer (API antiga, PCM, pelo
 *    FillComplexBuffer) e os ouvintes de evento de parâmetro (AUEventListener / AUParameterListenerNotify).
 * Código original do mOS (MIT).
 */
#include "foundation.h"

typedef int OSStatus;
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcasecmp(const char *a, const char *b);
unsigned long long mach_absolute_time(void);
id objc_retain(id o);
double floor(double);
#define FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
#define kAudio_ParamError (-50)
#define BLK_FN(b) (((void **)(b))[2])

/* ---------------------------------------------------------------- CAClock */
typedef struct { i16 subframes, divisor; u32 counter, type, flags; i16 hours, minutes, seconds, frames; } smpte_t;
typedef struct { u32 format, reserved; union { u64 host; double v; smpte_t smpte; } t; } catime_t;
typedef struct { double beats, bpm; } tempo_entry;
typedef struct {
    u32 magic;
    int running;
    double start_host, pos;          /* pos: segundos do transporte quando parado / na partida */
    double rate_samples;             /* amostras por segundo */
    u32 smpte_type;
    double smpte_offset;             /* segundos */
    tempo_entry *tempo;
    u32 ntempo;
    u32 sync_mode, sync_source, timebase;
    id name;
} caclock;
#define CK_MAGIC 0x43414c4bu
static caclock *ck(void *p) { caclock *c = p; return c && c->magic == CK_MAGIC ? c : 0; }
static double host_s(void) { return (double)mach_absolute_time() / 1e9; }
static double ck_secs(caclock *c) { return c->running ? c->pos + host_s() - c->start_host : c->pos; }
static double secs_to_beats(caclock *c, double s) {
    double at_b = 0, at_s = 0, bpm = c->ntempo ? c->tempo[0].bpm : 120;
    for (u32 i = 1; i < c->ntempo; i++) {
        double seg = (c->tempo[i].beats - at_b) * 60 / bpm;
        if (at_s + seg > s) break;
        at_s += seg; at_b = c->tempo[i].beats; bpm = c->tempo[i].bpm;
    }
    return at_b + (s - at_s) * bpm / 60;
}
static double beats_to_secs(caclock *c, double b) {
    double at_b = 0, at_s = 0, bpm = c->ntempo ? c->tempo[0].bpm : 120;
    for (u32 i = 1; i < c->ntempo && c->tempo[i].beats < b; i++) { at_s += (c->tempo[i].beats - at_b) * 60 / bpm; at_b = c->tempo[i].beats; bpm = c->tempo[i].bpm; }
    return at_s + (b - at_b) * 60 / bpm;
}
/* quadros por segundo e drop-frame dos kSMPTETimeType* */
static void smpte_rate(u32 type, double *fps, int *nominal, int *drop) {
    static const struct { double fps; int nom, drop; } t[] = {
        {24, 24, 0}, {25, 25, 0}, {30000.0 / 1001, 30, 1}, {30, 30, 0}, {30000.0 / 1001, 30, 0}, {30000.0 / 1001, 30, 1},
        {60, 60, 0}, {60000.0 / 1001, 60, 0}, {60000.0 / 1001, 60, 1}, {60000.0 / 1001, 60, 0}, {60, 60, 0},
        {50, 50, 0}, {24000.0 / 1001, 24, 0}};
    if (type >= sizeof t / sizeof t[0]) type = 3;
    *fps = t[type].fps; *nominal = t[type].nom; *drop = t[type].drop;
}
static smpte_t secs_to_smpte(caclock *c, double s) {
    double fps; int nom, drop;
    smpte_rate(c->smpte_type, &fps, &nom, &drop);
    s += c->smpte_offset;
    if (s < 0) s = 0;
    long fr = (long)floor(s * fps + 1e-9);
    if (drop) {   /* pula os números de quadro 0 e 1 (2 e 3 a 60) em todo minuto que não é múltiplo de 10 */
        int d = nom / 15;
        long per10 = 10 * 60 * nom - 9 * d, per1 = 60 * nom - d;
        long tens = fr / per10, rem = fr % per10;
        fr += 9 * d * tens + (rem > d ? d * ((rem - d) / per1) : 0);
    }
    smpte_t m = {0};
    m.type = c->smpte_type;
    m.flags = 1;   /* kSMPTETimeValid */
    m.frames = (i16)(fr % nom); fr /= nom;
    m.seconds = (i16)(fr % 60); fr /= 60;
    m.minutes = (i16)(fr % 60); fr /= 60;
    m.hours = (i16)fr;
    return m;
}
static double smpte_to_secs(caclock *c, const smpte_t *m) {
    double fps; int nom, drop;
    smpte_rate(c->smpte_type, &fps, &nom, &drop);
    long totmin = m->hours * 60L + m->minutes;
    long fr = ((m->hours * 3600L + m->minutes * 60L + m->seconds) * nom) + m->frames;
    if (drop) fr -= (nom / 15) * (totmin - totmin / 10);
    return fr / fps - c->smpte_offset;
}
OSStatus CAClockNew(u32 flags, void **out) {
    (void)flags;
    if (!out) return kAudio_ParamError;
    caclock *c = calloc(1, sizeof *c);
    if (!c) return -108;
    c->magic = CK_MAGIC;
    c->rate_samples = 48000;
    c->smpte_type = 3;   /* 30 fps */
    c->timebase = FCC('h', 'o', 's', 't');
    *out = c;
    return 0;
}
OSStatus CAClockDispose(void *p) { caclock *c = ck(p); if (!c) return kAudio_ParamError; free(c->tempo); c->magic = 0; free(c); return 0; }
OSStatus CAClockStart(void *p) { caclock *c = ck(p); if (!c) return kAudio_ParamError; if (!c->running) { c->start_host = host_s(); c->running = 1; } return 0; }
OSStatus CAClockStop(void *p) { caclock *c = ck(p); if (!c) return kAudio_ParamError; if (c->running) { c->pos = ck_secs(c); c->running = 0; } return 0; }
OSStatus CAClockArm(void *p) { return ck(p) ? 0 : kAudio_ParamError; }
OSStatus CAClockDisarm(void *p) { return ck(p) ? 0 : kAudio_ParamError; }
static OSStatus to_format(caclock *c, double s, u32 fmt, catime_t *out) {
    memset(out, 0, sizeof *out);
    out->format = fmt;
    switch (fmt) {
    case 0x686f7374: out->t.host = (u64)((c->running ? host_s() - (ck_secs(c) - s) : host_s() + (s - ck_secs(c))) * 1e9); return 0;   /* 'host' */
    case 0x73616d70: out->t.v = s * c->rate_samples; return 0;   /* 'samp' */
    case 0x62656174: out->t.v = secs_to_beats(c, s); return 0;   /* 'beat' */
    case 0x73656373: out->t.v = s; return 0;                     /* 'secs' */
    case 0x736d7074: out->t.smpte = secs_to_smpte(c, s); return 0; /* 'smpt' */
    default: return -66800;   /* kCAClock_UnknownConversionError... formato desconhecido */
    }
}
static int from_format(caclock *c, const catime_t *in, double *s) {
    switch (in->format) {
    case 0x686f7374: *s = ck_secs(c) + ((double)in->t.host / 1e9 - host_s()); return 0;
    case 0x73616d70: *s = in->t.v / c->rate_samples; return 0;
    case 0x62656174: *s = beats_to_secs(c, in->t.v); return 0;
    case 0x73656373: *s = in->t.v; return 0;
    case 0x736d7074: *s = smpte_to_secs(c, &in->t.smpte); return 0;
    default: return -1;
    }
}
OSStatus CAClockGetCurrentTime(void *p, u32 fmt, catime_t *out) { caclock *c = ck(p); if (!c || !out) return kAudio_ParamError; return to_format(c, ck_secs(c), fmt, out); }
OSStatus CAClockGetStartTime(void *p, u32 fmt, catime_t *out) {
    caclock *c = ck(p);
    if (!c || !out) return kAudio_ParamError;
    if (!c->running) return -66806;   /* kCAClock_NotRunningError... o relógio parado não tem partida */
    return to_format(c, c->pos, fmt, out);
}
OSStatus CAClockSetCurrentTime(void *p, const catime_t *t) {
    caclock *c = ck(p);
    double s;
    if (!c || !t || from_format(c, t, &s)) return kAudio_ParamError;
    if (c->running) return -66806;   /* como a Apple: só parado */
    c->pos = s;
    return 0;
}
OSStatus CAClockTranslateTime(void *p, const catime_t *in, u32 fmt, catime_t *out) {
    caclock *c = ck(p);
    double s;
    if (!c || !in || !out || from_format(c, in, &s)) return kAudio_ParamError;
    return to_format(c, s, fmt, out);
}
OSStatus CAClockSecondsToSMPTETime(void *p, double s, u16 subframes, smpte_t *out) {
    caclock *c = ck(p);
    (void)subframes;
    if (!c || !out) return kAudio_ParamError;
    *out = secs_to_smpte(c, s);
    return 0;
}
OSStatus CAClockSMPTETimeToSeconds(void *p, const smpte_t *m, double *s) { caclock *c = ck(p); if (!c || !m || !s) return kAudio_ParamError; *s = smpte_to_secs(c, m); return 0; }
OSStatus CAClockBeatsToBarBeatTime(void *p, double beats, u16 subdiv, void *out) {
    caclock *c = ck(p);
    if (!c || !out) return kAudio_ParamError;
    /* CABarBeatTime { SInt32 bar; UInt16 beat, subbeat, subbeatDivisor, reserved }, compasso 4/4, a partir de 1 */
    i32 bar = (i32)floor(beats / 4) + 1;
    double in_bar = beats - (bar - 1) * 4.0;
    u16 beat = (u16)floor(in_bar) + 1, sd = subdiv ? subdiv : 100;
    u16 sub = (u16)floor((in_bar - floor(in_bar)) * sd);
    u8 *o = out;
    memcpy(o, &bar, 4); memcpy(o + 4, &beat, 2); memcpy(o + 6, &sub, 2); memcpy(o + 8, &sd, 2); memset(o + 10, 0, 2);
    return 0;
}
enum { P_INTB = 0x696e7462, P_TBSRC = 0x69746273, P_SYNM = 0x73796e6d, P_SYNS = 0x73796e73, P_SMPF = 0x736d7066, P_SMPO = 0x736d706f,
       P_TEMPO = 0x746d706f, P_NAME = 0x6e616d65, P_RATE = 0x73726174 };
OSStatus CAClockSetProperty(void *p, u32 prop, u32 size, const void *data) {
    caclock *c = ck(p);
    if (!c || (size && !data)) return kAudio_ParamError;
    switch (prop) {
    case P_INTB: if (size < 4) return kAudio_ParamError; memcpy(&c->timebase, data, 4); return 0;
    case P_TBSRC: return 0;   /* fonte do timebase (dispositivo de áudio): o relógio continua no host */
    case P_SYNM: if (size < 4) return kAudio_ParamError; memcpy(&c->sync_mode, data, 4); return 0;
    case P_SYNS: if (size < 4) return kAudio_ParamError; memcpy(&c->sync_source, data, 4); return 0;
    case P_SMPF: if (size < 4) return kAudio_ParamError; memcpy(&c->smpte_type, data, 4); return 0;
    case P_SMPO: if (size < 8) return kAudio_ParamError; memcpy(&c->smpte_offset, data, 8); return 0;
    case P_RATE: if (size < 8) return kAudio_ParamError; memcpy(&c->rate_samples, data, 8); if (c->rate_samples <= 0) c->rate_samples = 48000; return 0;
    case P_TEMPO: {
        u32 n = size / sizeof(tempo_entry);
        tempo_entry *t = n ? malloc(n * sizeof *t) : 0;
        if (n && !t) return -108;
        if (n) memcpy(t, data, n * sizeof *t);
        for (u32 i = 0; i < n; i++) if (t[i].bpm <= 0) t[i].bpm = 120;
        free(c->tempo);
        c->tempo = t; c->ntempo = n;
        return 0;
    }
    case P_NAME: c->name = size >= sizeof(id) ? objc_retain(*(const id *)data) : 0; return 0;
    default: return -66819;   /* kCAClock_InvalidPropertyError... */
    }
}
OSStatus CAClockGetProperty(void *p, u32 prop, u32 *size, void *data) {
    caclock *c = ck(p);
    if (!c || !size || !data) return kAudio_ParamError;
    switch (prop) {
    case P_INTB: if (*size < 4) return kAudio_ParamError; memcpy(data, &c->timebase, 4); *size = 4; return 0;
    case P_SYNM: if (*size < 4) return kAudio_ParamError; memcpy(data, &c->sync_mode, 4); *size = 4; return 0;
    case P_SYNS: if (*size < 4) return kAudio_ParamError; memcpy(data, &c->sync_source, 4); *size = 4; return 0;
    case P_SMPF: if (*size < 4) return kAudio_ParamError; memcpy(data, &c->smpte_type, 4); *size = 4; return 0;
    case P_SMPO: if (*size < 8) return kAudio_ParamError; memcpy(data, &c->smpte_offset, 8); *size = 8; return 0;
    case P_RATE: if (*size < 8) return kAudio_ParamError; memcpy(data, &c->rate_samples, 8); *size = 8; return 0;
    case P_TEMPO: { u32 n = c->ntempo * (u32)sizeof(tempo_entry); if (*size < n) return kAudio_ParamError; memcpy(data, c->tempo, n); *size = n; return 0; }
    case P_NAME: if (*size < sizeof(id)) return kAudio_ParamError; memcpy(data, &c->name, sizeof(id)); *size = sizeof(id); return 0;
    default: return -66819;
    }
}

/* ---------------------------------------------------------------- AudioFileGetGlobalInfo */
typedef struct { u32 type; const char *name, *ext[3], *uti, *mime; u32 fmts[4]; int writable; } aftype;
static const aftype g_types[] = {
    {FCC('W', 'A', 'V', 'E'), "WAVE", {"wav", "wave", 0}, "com.microsoft.waveform-audio", "audio/wav", {FCC('l', 'p', 'c', 'm')}, 1},
    {FCC('A', 'I', 'F', 'F'), "AIFF", {"aif", "aiff", 0}, "public.aiff-audio", "audio/aiff", {FCC('l', 'p', 'c', 'm')}, 0},
    {FCC('A', 'I', 'F', 'C'), "AIFC", {"aifc", "aif", "aiff"}, "public.aifc-audio", "audio/aiff", {FCC('l', 'p', 'c', 'm')}, 0},
    {FCC('c', 'a', 'f', 'f'), "CAF", {"caf", 0, 0}, "com.apple.coreaudio-format", "audio/x-caf", {FCC('l', 'p', 'c', 'm'), FCC('a', 'a', 'c', ' '), FCC('a', 'l', 'a', 'c'), FCC('o', 'p', 'u', 's')}, 0},
    {FCC('M', 'P', 'G', '3'), "MPEG Layer 3", {"mp3", 0, 0}, "public.mp3", "audio/mpeg", {FCC('.', 'm', 'p', '3')}, 0},
    {FCC('m', '4', 'a', 'f'), "Apple MPEG-4 Audio", {"m4a", "m4r", 0}, "com.apple.m4a-audio", "audio/x-m4a", {FCC('a', 'a', 'c', ' '), FCC('a', 'l', 'a', 'c')}, 0},
    {FCC('m', 'p', '4', 'f'), "MPEG-4 Audio", {"mp4", 0, 0}, "public.mpeg-4-audio", "audio/mp4", {FCC('a', 'a', 'c', ' '), FCC('o', 'p', 'u', 's')}, 0},
    {FCC('a', 'd', 't', 's'), "AAC ADTS", {"aac", "adts", 0}, "public.aac-audio", "audio/aac", {FCC('a', 'a', 'c', ' ')}, 0},
};
#define NTYPES (sizeof g_types / sizeof g_types[0])
static const aftype *type_of(u32 t) { for (unsigned i = 0; i < NTYPES; i++) if (g_types[i].type == t) return &g_types[i]; return 0; }
static id arr(void) { return MSG0(id, (id)cx_class_named("NSMutableArray"), "array"); }
static void add(id a, id o) { MSG(void, a, "addObject:", o); }
/* resultado como array de u32 (tipos/formatos) ou CF (nome, arrays de strings) */
static OSStatus global_info(u32 prop, u32 ssz, const void *spec, u32 *sz, void *out, int size_only) {
    u32 tmp[64], n = 0;
    id obj = 0;
    u32 spec_u32 = ssz >= 4 && spec ? *(const u32 *)spec : 0;
    const char *spec_s = ssz >= sizeof(id) && spec ? cx_cstr(*(const id *)spec) : 0;
    switch (prop) {
    case 0x61667266: for (unsigned i = 0; i < NTYPES; i++) tmp[n++] = g_types[i].type; break;                         /* 'afrf' leitura */
    case 0x61667766: for (unsigned i = 0; i < NTYPES; i++) if (g_types[i].writable) tmp[n++] = g_types[i].type; break; /* 'afwf' escrita */
    case 0x666d6964: { const aftype *t = type_of(spec_u32); if (!t) return FCC('t', 'y', 'p', '?'); for (int k = 0; k < 4 && t->fmts[k]; k++) tmp[n++] = t->fmts[k]; break; }  /* 'fmid' */
    case 0x66746e6d: { const aftype *t = type_of(spec_u32); if (!t) return FCC('t', 'y', 'p', '?'); obj = cx_str(t->name); break; }  /* 'ftnm' */
    case 0x66657874: { const aftype *t = type_of(spec_u32); if (!t) return FCC('t', 'y', 'p', '?'); obj = arr(); for (int k = 0; k < 3 && t->ext[k]; k++) add(obj, cx_str(t->ext[k])); break; } /* 'fext' */
    case 0x66757469: { const aftype *t = type_of(spec_u32); if (!t) return FCC('t', 'y', 'p', '?'); obj = arr(); add(obj, cx_str(t->uti)); break; }   /* 'futi' */
    case 0x666d696d: { const aftype *t = type_of(spec_u32); if (!t) return FCC('t', 'y', 'p', '?'); obj = arr(); add(obj, cx_str(t->mime)); break; }  /* 'fmim' */
    case 0x616c7874: obj = arr(); for (unsigned i = 0; i < NTYPES; i++) for (int k = 0; k < 3 && g_types[i].ext[k]; k++) add(obj, cx_str(g_types[i].ext[k])); break; /* 'alxt' */
    case 0x61757469: obj = arr(); for (unsigned i = 0; i < NTYPES; i++) add(obj, cx_str(g_types[i].uti)); break;   /* 'auti' */
    case 0x616d696d: obj = arr(); for (unsigned i = 0; i < NTYPES; i++) add(obj, cx_str(g_types[i].mime)); break;  /* 'amim' */
    case 0x74796578: case 0x74757469: case 0x746d696d:   /* 'tyex' extensão, 'tuti' UTI, 'tmim' MIME -> tipos */
        if (!spec_s) return kAudio_ParamError;
        for (unsigned i = 0; i < NTYPES; i++) {
            int hit = 0;
            if (prop == 0x74796578) { for (int k = 0; k < 3 && g_types[i].ext[k]; k++) if (!strcasecmp(g_types[i].ext[k], spec_s)) hit = 1; }
            else if (prop == 0x74757469) hit = !strcasecmp(g_types[i].uti, spec_s);
            else hit = !strcasecmp(g_types[i].mime, spec_s);
            if (hit) tmp[n++] = g_types[i].type;
        }
        break;
    default: return FCC('p', 't', 'y', '?');   /* kAudioFileUnsupportedPropertyError */
    }
    u32 need = obj ? (u32)sizeof(id) : n * 4;
    if (size_only) { *sz = need; return 0; }
    if (!out || *sz < need) return FCC('!', 's', 'i', 'z');   /* kAudioFileBadPropertySizeError */
    if (obj) { objc_retain(obj); memcpy(out, &obj, sizeof(id)); }
    else memcpy(out, tmp, need);
    *sz = need;
    return 0;
}
OSStatus AudioFileGetGlobalInfoSize(u32 prop, u32 ssz, const void *spec, u32 *sz) { if (!sz) return kAudio_ParamError; return global_info(prop, ssz, spec, sz, 0, 1); }
OSStatus AudioFileGetGlobalInfo(u32 prop, u32 ssz, const void *spec, u32 *sz, void *out) { if (!sz) return kAudio_ParamError; return global_info(prop, ssz, spec, sz, out, 0); }

/* ---------------------------------------------------------------- AudioComponentInstantiate */
OSStatus AudioComponentInstanceNew(void *c, void **out);
void AudioComponentInstantiate(void *comp, u32 options, void *blk) {
    (void)options;
    void *inst = 0;
    OSStatus rc = AudioComponentInstanceNew(comp, &inst);
    if (blk) ((void (*)(void *, void *, OSStatus))BLK_FN(blk))(blk, rc ? 0 : inst, rc);
}

/* ---------------------------------------------------------------- AudioConverterFillBuffer (PCM, API antiga) */
typedef struct { u32 nch, size; void *data; } abuf_t;
typedef struct { u32 n, pad; abuf_t b[1]; } ablist_t;
typedef OSStatus (*old_input_fn)(void *conv, u32 *io_size, void **out_data, void *ud);
typedef struct { old_input_fn proc; void *ud; u32 in_bpf; } fb_ctx;
OSStatus AudioConverterFillComplexBuffer(void *c, void *proc, void *ud, u32 *npackets, ablist_t *out, void *pd);
OSStatus AudioConverterGetProperty(void *c, u32 id_, u32 *sz, void *d);
static OSStatus fb_input(void *conv, u32 *npk, ablist_t *data, void **pd, void *ud) {
    fb_ctx *f = ud;
    if (pd) *pd = 0;
    u32 bytes = *npk * f->in_bpf;
    void *ptr = 0;
    OSStatus rc = f->proc(conv, &bytes, &ptr, f->ud);
    if (rc || !ptr || !bytes) { *npk = 0; data->b[0].size = 0; data->b[0].data = 0; return rc; }
    data->n = 1;
    data->b[0].data = ptr;
    data->b[0].size = bytes;
    *npk = bytes / f->in_bpf;
    return 0;
}
OSStatus AudioConverterFillBuffer(void *conv, old_input_fn proc, void *ud, u32 *io_out_size, void *out) {
    if (!conv || !proc || !io_out_size || !out) return kAudio_ParamError;
    u8 in_fmt[40], out_fmt[40];
    u32 s1 = sizeof in_fmt, s2 = sizeof out_fmt;
    if (AudioConverterGetProperty(conv, FCC('a', 'c', 'i', 'd'), &s1, in_fmt) || AudioConverterGetProperty(conv, FCC('a', 'c', 'o', 'd'), &s2, out_fmt))
        return kAudio_ParamError;
    u32 in_bpf, out_bpf;
    memcpy(&in_bpf, in_fmt + 24, 4);    /* mBytesPerFrame */
    memcpy(&out_bpf, out_fmt + 24, 4);
    if (!in_bpf || !out_bpf) return FCC('!', 'p', 'k', 'd');   /* só PCM (formato com bytes por quadro) */
    fb_ctx f = {proc, ud, in_bpf};
    u32 frames = *io_out_size / out_bpf;
    struct { u32 n, pad; abuf_t b[1]; } list = {1, 0, {{0, frames * out_bpf, out}}};
    memcpy(&list.b[0].nch, out_fmt + 28, 4);   /* mChannelsPerFrame */
    OSStatus rc = AudioConverterFillComplexBuffer(conv, (void *)fb_input, &f, &frames, (ablist_t *)&list, 0);
    *io_out_size = frames * out_bpf;
    return rc;
}

/* ---------------------------------------------------------------- ouvintes de parâmetro (AUEventListener) */
typedef struct { void *unit; u32 id, scope, element; } auparam_t;
typedef struct { u32 type; u32 pad; union { auparam_t param; struct { void *unit; u32 prop, scope, element; } prop; } a; } auevent_t;
typedef void (*ev_proc)(void *ud, void *obj, const auevent_t *ev, u64 host, float value);
typedef void (*par_proc)(void *ud, void *obj, const auparam_t *p, float value);
typedef struct listener {
    u32 magic;
    ev_proc eproc;
    par_proc pproc;
    void *ud;
    struct { void *obj; auevent_t ev; } *reg;
    u32 nreg, creg;
    struct listener *next;
} listener;
#define LS_MAGIC 0x4c49534eu
static listener *g_listeners;
static u8 g_lock[64];
static int g_lock_ok;
int pthread_mutex_init(void *m, const void *a);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);
static void lk(void) { if (!g_lock_ok) { pthread_mutex_init(g_lock, 0); g_lock_ok = 1; } pthread_mutex_lock(g_lock); }
static void ulk(void) { pthread_mutex_unlock(g_lock); }
static OSStatus new_listener(ev_proc e, par_proc p, void *ud, void **out) {
    if (!out || (!e && !p)) return kAudio_ParamError;
    listener *l = calloc(1, sizeof *l);
    if (!l) return -108;
    l->magic = LS_MAGIC; l->eproc = e; l->pproc = p; l->ud = ud;
    lk(); l->next = g_listeners; g_listeners = l; ulk();
    *out = l;
    return 0;
}
/* runloop/mode/intervalos: a entrega é na hora, na thread de quem notifica */
OSStatus AUEventListenerCreate(ev_proc proc, void *ud, void *rl, id mode, float gran, float interval, void **out) {
    (void)rl; (void)mode; (void)gran; (void)interval;
    return new_listener(proc, 0, ud, out);
}
OSStatus AUListenerCreate(par_proc proc, void *ud, void *rl, id mode, float interval, void **out) {
    (void)rl; (void)mode; (void)interval;
    return new_listener(0, proc, ud, out);
}
static OSStatus reg_add(void *lp, void *obj, const auevent_t *ev) {
    listener *l = lp;
    if (!l || l->magic != LS_MAGIC || !ev) return kAudio_ParamError;
    lk();
    if (l->nreg == l->creg) {
        u32 nc = l->creg ? l->creg * 2 : 8;
        void *x = realloc(l->reg, nc * sizeof *l->reg);
        if (!x) { ulk(); return -108; }
        l->reg = x; l->creg = nc;
    }
    l->reg[l->nreg].obj = obj;
    l->reg[l->nreg].ev = *ev;
    l->nreg++;
    ulk();
    return 0;
}
OSStatus AUEventListenerAddEventType(void *l, void *obj, const auevent_t *ev) { return reg_add(l, obj, ev); }
OSStatus AUListenerAddParameter(void *l, void *obj, const auparam_t *p) {
    if (!p) return kAudio_ParamError;
    auevent_t ev = {0};
    ev.type = 0;   /* kAudioUnitEvent_ParameterValueChange */
    ev.a.param = *p;
    return reg_add(l, obj, &ev);
}
static int same_param(const auparam_t *a, const auparam_t *b) {
    return a->unit == b->unit && (a->id == b->id || a->id == 0xffffffffu) && a->scope == b->scope && a->element == b->element;
}
OSStatus AUEventListenerRemoveEventType(void *lp, void *obj, const auevent_t *ev) {
    listener *l = lp;
    if (!l || l->magic != LS_MAGIC || !ev) return kAudio_ParamError;
    lk();
    for (u32 i = 0; i < l->nreg; i++)
        if (l->reg[i].obj == obj && l->reg[i].ev.type == ev->type && same_param(&l->reg[i].ev.a.param, &ev->a.param)) { l->reg[i] = l->reg[--l->nreg]; break; }
    ulk();
    return 0;
}
OSStatus AUListenerRemoveParameter(void *l, void *obj, const auparam_t *p) {
    auevent_t ev = {0};
    ev.a.param = *p;
    return AUEventListenerRemoveEventType(l, obj, &ev);
}
OSStatus AUListenerDispose(void *lp) {
    listener *l = lp;
    if (!l || l->magic != LS_MAGIC) return kAudio_ParamError;
    lk();
    for (listener **pp = &g_listeners; *pp; pp = &(*pp)->next) if (*pp == l) { *pp = l->next; break; }
    ulk();
    l->magic = 0;
    free(l->reg);
    free(l);
    return 0;
}
OSStatus AudioUnitGetParameter(void *u, u32 id_, u32 sc, u32 el, float *v);
/* notifica todos os ouvintes daquele parâmetro (sender: o ouvinte que mudou e não quer eco) */
OSStatus AUParameterListenerNotify(void *sender, void *sender_obj, const auparam_t *p) {
    (void)sender_obj;
    if (!p) return kAudio_ParamError;
    float v = 0;
    AudioUnitGetParameter(p->unit, p->id, p->scope, p->element, &v);
    auevent_t ev = {0};
    ev.type = 0;
    ev.a.param = *p;
    u64 host = mach_absolute_time();
    struct { listener *l; void *obj; } hits[64];
    int nh = 0;
    lk();
    for (listener *l = g_listeners; l; l = l->next) {
        if (l == sender) continue;
        for (u32 i = 0; i < l->nreg && nh < 64; i++)
            if (l->reg[i].ev.type == 0 && same_param(&l->reg[i].ev.a.param, p)) { hits[nh].l = l; hits[nh].obj = l->reg[i].obj; nh++; }
    }
    ulk();
    for (int i = 0; i < nh; i++) {
        if (hits[i].l->eproc) hits[i].l->eproc(hits[i].l->ud, hits[i].obj, &ev, host, v);
        else if (hits[i].l->pproc) hits[i].l->pproc(hits[i].l->ud, hits[i].obj, p, v);
    }
    return 0;
}
/* AUParameterSet: muda e avisa */
OSStatus AudioUnitSetParameter(void *u, u32 id_, u32 sc, u32 el, float v, u32 off);
OSStatus AUParameterSet(void *sender, void *sender_obj, const auparam_t *p, float v, u32 off) {
    if (!p) return kAudio_ParamError;
    OSStatus rc = AudioUnitSetParameter(p->unit, p->id, p->scope, p->element, v, off);
    if (!rc) AUParameterListenerNotify(sender, sender_obj, p);
    return rc;
}
