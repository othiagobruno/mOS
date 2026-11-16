/*
 * mOS — MusicSequence, MusicTrack, MusicEventIterator e MusicPlayer (AudioToolbox).
 *  - Trilhas com eventos em batidas (MusicTimeStamp), em ordem estável; trilha de tempo com ExtendedTempo (120 bpm se não há).
 *  - Arquivo MIDI padrão (SMF tipo 0 e 1, divisão em ticks ou SMPTE) para dentro (MusicSequenceFileLoad/LoadData, com
 *    ChannelsToTracks) e para fora (MusicSequenceFileCreateData, tipo 1, trilha 0 = tempo). Notas viram MIDINoteMessage
 *    com duração (note on + note off/velocity 0), como na Apple.
 *  - Propriedades de trilha: loop (duração e repetições, 0 = sempre), deslocamento, mute, solo, comprimento, resolução.
 *  - MusicPlayer: uma thread anda no relógio do host pela trilha de tempo e manda os eventos para o endpoint da trilha
 *    (ou o da sequência) por uma porta de saída do CoreMIDI, ou para o nó do AUGraph por MusicDeviceMIDIEvent (o mOS
 *    não tem instrumento: o nó recusa, como uma unidade que não é MusicDevice). Eventos de usuário vão para o callback.
 *    No Stop, as notas que ficaram tocando recebem note off.
 * Código original do mOS (MIT).
 */
#include "foundation.h"

typedef int OSStatus;
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_join(unsigned long t, void **ret);
int pthread_mutex_init(void *m, const void *a);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);
int usleep(unsigned us);
unsigned long long mach_absolute_time(void);
id objc_retain(id o);
void objc_release(id o);
const char *cx_audio_url_path(id url);
long fseek(FILE_ *f, long off, int w);
long ftell(FILE_ *f);
/* CoreMIDI e AUGraph do libSystem */
OSStatus MIDIClientCreate(id name, void *proc, void *ref, u32 *out);
OSStatus MIDIOutputPortCreate(u32 c, id name, u32 *out);
OSStatus MIDISend(u32 port, u32 dest, const void *list);
void *MIDIPacketListInit(void *list);
void *MIDIPacketListAdd(void *list, unsigned long size, void *cur, u64 time, unsigned long n, const u8 *data);
OSStatus NewAUGraph(void **out);
OSStatus AUGraphNodeInfo(void *g, i32 node, void *desc, void **unit);
OSStatus MusicDeviceMIDIEvent(void *u, u32 status, u32 d1, u32 d2, u32 off);
OSStatus MusicDeviceSysEx(void *u, const u8 *data, u32 n) __attribute__((weak));

#define ERR_PARAM (-50)
#define ERR_INVALID_SEQ (-10846)        /* kAudioToolboxErr_InvalidSequenceType */
#define ERR_TRACK_INDEX (-10859)        /* kAudioToolboxErr_TrackIndexError */
#define ERR_TRACK_NOT_FOUND (-10858)    /* kAudioToolboxErr_TrackNotFound */
#define ERR_END_OF_TRACK (-10857)       /* kAudioToolboxErr_EndOfTrack */
#define ERR_START_OF_TRACK (-10856)     /* kAudioToolboxErr_StartOfTrack */
#define ERR_ILLEGAL_TRACK_DEST (-10855) /* kAudioToolboxErr_IllegalTrackDestination */
#define ERR_NO_SEQUENCE (-10854)        /* kAudioToolboxErr_NoSequence */
#define ERR_INVALID_EVENT (-10853)      /* kAudioToolboxErr_InvalidEventType */
#define ERR_INVALID_PLAYER (-10852)     /* kAudioToolboxErr_InvalidPlayerState */
#define FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
enum { EV_NULL = 0, EV_EXT_NOTE = 1, EV_EXT_TEMPO = 3, EV_USER = 4, EV_META = 5, EV_NOTE = 6, EV_CHANNEL = 7, EV_RAW = 8, EV_PARAM = 9, EV_AUPRESET = 10 };

typedef struct { double t; u32 type, size; u8 *data; } mevent;
typedef struct mseq mseq;
typedef struct mtrack {
    u32 magic;
    mseq *seq;
    mevent *ev;
    u32 n, cap;
    double loop_len, offset, length;
    i32 loops;
    u8 mute, solo, has_length;
    u32 automated;
    u32 dest_ep;
    i32 dest_node;
    int has_dest_node;
} mtrack;
struct mseq {
    u32 magic;
    mtrack *tempo;
    mtrack **tr;
    u32 nt, ct;
    i16 resolution;
    u32 type;               /* kMusicSequenceType_Beats 'beat' */
    void *graph;
    u32 ep;
    void (*user_cb)(void *, void *, void *, double, const void *, double, double);
    void *user_ctx;
};
#define SEQ_MAGIC 0x5345514du
#define TRK_MAGIC 0x54524b4du
static mseq *as_seq(void *p) { mseq *s = p; return s && s->magic == SEQ_MAGIC ? s : 0; }
static mtrack *as_trk(void *p) { mtrack *t = p; return t && t->magic == TRK_MAGIC ? t : 0; }

static mtrack *track_new(mseq *s) {
    mtrack *t = calloc(1, sizeof *t);
    if (!t) return 0;
    t->magic = TRK_MAGIC;
    t->seq = s;
    t->dest_node = -1;
    return t;
}
static void track_free(mtrack *t) {
    if (!t) return;
    for (u32 i = 0; i < t->n; i++) free(t->ev[i].data);
    free(t->ev);
    t->magic = 0;
    free(t);
}
/* insere depois dos eventos de mesmo tempo (ordem estável) */
static OSStatus add_event(mtrack *t, double time, u32 type, const void *data, u32 size) {
    if (!t || time < 0 || (size && !data)) return ERR_PARAM;
    if (t->n == t->cap) {
        u32 nc = t->cap ? t->cap * 2 : 64;
        mevent *e = realloc(t->ev, nc * sizeof *e);
        if (!e) return -108;
        t->ev = e; t->cap = nc;
    }
    u8 *d = size ? malloc(size) : 0;
    if (size && !d) return -108;
    if (size) memcpy(d, data, size);
    u32 lo = 0, hi = t->n;
    while (lo < hi) { u32 m = (lo + hi) / 2; if (t->ev[m].t <= time) lo = m + 1; else hi = m; }
    memmove(t->ev + lo + 1, t->ev + lo, (t->n - lo) * sizeof *t->ev);
    t->ev[lo].t = time; t->ev[lo].type = type; t->ev[lo].size = size; t->ev[lo].data = d;
    t->n++;
    return 0;
}

/* ---------------------------------------------------------------- sequência */
OSStatus NewMusicSequence(void **out) {
    if (!out) return ERR_PARAM;
    mseq *s = calloc(1, sizeof *s);
    if (!s) return -108;
    s->magic = SEQ_MAGIC;
    s->tempo = track_new(s);
    s->resolution = 480;
    s->type = FCC('b', 'e', 'a', 't');
    if (!s->tempo) { free(s); return -108; }
    *out = s;
    return 0;
}
OSStatus DisposeMusicSequence(void *p) {
    mseq *s = as_seq(p);
    if (!s) return ERR_PARAM;
    for (u32 i = 0; i < s->nt; i++) track_free(s->tr[i]);
    track_free(s->tempo);
    free(s->tr);
    s->magic = 0;
    free(s);
    return 0;
}
OSStatus MusicSequenceNewTrack(void *p, void **out) {
    mseq *s = as_seq(p);
    if (!s || !out) return ERR_PARAM;
    if (s->nt == s->ct) {
        u32 nc = s->ct ? s->ct * 2 : 8;
        mtrack **a = realloc(s->tr, nc * sizeof *a);
        if (!a) return -108;
        s->tr = a; s->ct = nc;
    }
    mtrack *t = track_new(s);
    if (!t) return -108;
    s->tr[s->nt++] = t;
    *out = t;
    return 0;
}
OSStatus MusicSequenceDisposeTrack(void *p, void *trk) {
    mseq *s = as_seq(p);
    if (!s) return ERR_PARAM;
    for (u32 i = 0; i < s->nt; i++)
        if (s->tr[i] == trk) {
            track_free(s->tr[i]);
            memmove(s->tr + i, s->tr + i + 1, (s->nt - i - 1) * sizeof *s->tr);
            s->nt--;
            return 0;
        }
    return ERR_TRACK_NOT_FOUND;
}
OSStatus MusicSequenceGetTrackCount(void *p, u32 *n) { mseq *s = as_seq(p); if (!s || !n) return ERR_PARAM; *n = s->nt; return 0; }
OSStatus MusicSequenceGetIndTrack(void *p, u32 i, void **out) {
    mseq *s = as_seq(p);
    if (!s || !out) return ERR_PARAM;
    if (i >= s->nt) return ERR_TRACK_INDEX;
    *out = s->tr[i];
    return 0;
}
OSStatus MusicSequenceGetTrackIndex(void *p, void *trk, u32 *i) {
    mseq *s = as_seq(p);
    if (!s || !i) return ERR_PARAM;
    for (u32 k = 0; k < s->nt; k++) if (s->tr[k] == trk) { *i = k; return 0; }
    return ERR_TRACK_NOT_FOUND;
}
OSStatus MusicSequenceGetTempoTrack(void *p, void **out) { mseq *s = as_seq(p); if (!s || !out) return ERR_PARAM; *out = s->tempo; return 0; }
OSStatus MusicSequenceSetAUGraph(void *p, void *g) { mseq *s = as_seq(p); if (!s) return ERR_PARAM; s->graph = g; return 0; }
/* sem grafo próprio a Apple monta um com sintetizador; o mOS não tem instrumento e devolve um grafo vazio */
OSStatus MusicSequenceGetAUGraph(void *p, void **out) {
    mseq *s = as_seq(p);
    if (!s || !out) return ERR_PARAM;
    if (!s->graph) { OSStatus rc = NewAUGraph(&s->graph); if (rc) return rc; }
    *out = s->graph;
    return 0;
}
OSStatus MusicSequenceSetMIDIEndpoint(void *p, u32 ep) {
    mseq *s = as_seq(p);
    if (!s) return ERR_PARAM;
    s->ep = ep;
    for (u32 i = 0; i < s->nt; i++) { s->tr[i]->dest_ep = ep; s->tr[i]->has_dest_node = 0; }
    return 0;
}
OSStatus MusicSequenceSetSequenceType(void *p, u32 type) { mseq *s = as_seq(p); if (!s) return ERR_PARAM; if (type != FCC('b', 'e', 'a', 't')) return ERR_INVALID_SEQ; s->type = type; return 0; }
OSStatus MusicSequenceGetSequenceType(void *p, u32 *type) { mseq *s = as_seq(p); if (!s || !type) return ERR_PARAM; *type = s->type; return 0; }
OSStatus MusicSequenceSetUserCallback(void *p, void *cb, void *ctx) { mseq *s = as_seq(p); if (!s) return ERR_PARAM; s->user_cb = cb; s->user_ctx = ctx; return 0; }

/* mapa de tempo: segundos de uma batida e o inverso, por trechos de bpm constante */
static double ev_bpm(const mevent *e) { double b; memcpy(&b, e->data, 8); return b > 0 ? b : 120; }
static double beats_to_secs(mseq *s, double beats) {
    double sec = 0, at = 0, bpm = 120;
    for (u32 i = 0; i < s->tempo->n; i++) {
        mevent *e = &s->tempo->ev[i];
        if (e->type != EV_EXT_TEMPO || e->size < 8) continue;
        if (e->t >= beats) break;
        sec += (e->t - at) * 60 / bpm;
        at = e->t;
        bpm = ev_bpm(e);
    }
    return sec + (beats - at) * 60 / bpm;
}
static double secs_to_beats(mseq *s, double secs) {
    double sec = 0, at = 0, bpm = 120;
    for (u32 i = 0; i < s->tempo->n; i++) {
        mevent *e = &s->tempo->ev[i];
        if (e->type != EV_EXT_TEMPO || e->size < 8) continue;
        double seg = (e->t - at) * 60 / bpm;
        if (sec + seg > secs) break;
        sec += seg; at = e->t; bpm = ev_bpm(e);
    }
    return at + (secs - sec) * bpm / 60;
}
OSStatus MusicSequenceGetSecondsForBeats(void *p, double beats, double *out) { mseq *s = as_seq(p); if (!s || !out) return ERR_PARAM; *out = beats_to_secs(s, beats); return 0; }
OSStatus MusicSequenceGetBeatsForSeconds(void *p, double secs, double *out) { mseq *s = as_seq(p); if (!s || !out) return ERR_PARAM; *out = secs_to_beats(s, secs); return 0; }

/* ---------------------------------------------------------------- eventos */
typedef struct { u8 channel, note, velocity, release; float duration; } note_msg;
OSStatus MusicTrackGetSequence(void *trk, void **out) { mtrack *t = as_trk(trk); if (!t || !out) return ERR_PARAM; *out = t->seq; return 0; }
OSStatus MusicTrackNewMIDINoteEvent(void *trk, double time, const void *m) { return add_event(as_trk(trk), time, EV_NOTE, m, sizeof(note_msg)); }
OSStatus MusicTrackNewMIDIChannelEvent(void *trk, double time, const void *m) { return add_event(as_trk(trk), time, EV_CHANNEL, m, 4); }
OSStatus MusicTrackNewMIDIRawDataEvent(void *trk, double time, const void *raw) {
    if (!raw) return ERR_PARAM;
    u32 n; memcpy(&n, raw, 4);
    return add_event(as_trk(trk), time, EV_RAW, raw, 4 + n);
}
OSStatus MusicTrackNewUserEvent(void *trk, double time, const void *u) {
    if (!u) return ERR_PARAM;
    u32 n; memcpy(&n, u, 4);
    return add_event(as_trk(trk), time, EV_USER, u, 4 + n);
}
OSStatus MusicTrackNewMetaEvent(void *trk, double time, const void *m) {
    if (!m) return ERR_PARAM;
    u32 n; memcpy(&n, (const u8 *)m + 4, 4);
    return add_event(as_trk(trk), time, EV_META, m, 8 + n);
}
OSStatus MusicTrackNewExtendedTempoEvent(void *trk, double time, double bpm) {
    mtrack *t = as_trk(trk);
    if (!t || bpm <= 0) return ERR_PARAM;
    if (t != t->seq->tempo) return ERR_ILLEGAL_TRACK_DEST;   /* tempo só na trilha de tempo */
    return add_event(t, time, EV_EXT_TEMPO, &bpm, 8);
}
OSStatus MusicTrackNewParameterEvent(void *trk, double time, const void *p) { return add_event(as_trk(trk), time, EV_PARAM, p, 16); }
OSStatus MusicTrackNewAUPresetEvent(void *trk, double time, const void *p) {
    if (!p) return ERR_PARAM;
    id preset; memcpy(&preset, (const u8 *)p + 8, sizeof preset);
    if (preset) objc_retain(preset);
    return add_event(as_trk(trk), time, EV_AUPRESET, p, 16);
}
/* ExtendedNoteOnEvent: instrumento, grupo, duração e MusicDeviceNoteParams (argCount, pitch, velocity, controles) */
OSStatus MusicTrackNewExtendedNoteEvent(void *trk, double time, const void *e) {
    if (!e) return ERR_PARAM;
    u32 argc; memcpy(&argc, (const u8 *)e + 12, 4);
    u32 size = 12 + 4 + 8 + (argc > 2 ? (argc - 2) * 8 : 0);
    return add_event(as_trk(trk), time, EV_EXT_NOTE, e, size);
}

/* comprimento: o fim do último evento (com a duração das notas) */
static double natural_length(const mtrack *t) {
    double end = 0;
    for (u32 i = 0; i < t->n; i++) {
        double e = t->ev[i].t;
        if (t->ev[i].type == EV_NOTE) { note_msg m; memcpy(&m, t->ev[i].data, sizeof m); e += m.duration; }
        else if (t->ev[i].type == EV_EXT_NOTE) { float d; memcpy(&d, t->ev[i].data + 8, 4); e += d; }
        if (e > end) end = e;
    }
    return end;
}
enum { P_LOOP = 0, P_OFFSET = 1, P_MUTE = 2, P_SOLO = 3, P_AUTOMATED = 4, P_LENGTH = 5, P_RESOLUTION = 6 };
OSStatus MusicTrackSetProperty(void *trk, u32 prop, const void *data, u32 len) {
    mtrack *t = as_trk(trk);
    if (!t || !data) return ERR_PARAM;
    switch (prop) {
    case P_LOOP: if (len < 12) return ERR_PARAM; memcpy(&t->loop_len, data, 8); memcpy(&t->loops, (const u8 *)data + 8, 4); return 0;
    case P_OFFSET: if (len < 8) return ERR_PARAM; memcpy(&t->offset, data, 8); return 0;
    case P_MUTE: t->mute = *(const u8 *)data != 0; return 0;
    case P_SOLO: t->solo = *(const u8 *)data != 0; return 0;
    case P_AUTOMATED: if (len < 4) return ERR_PARAM; memcpy(&t->automated, data, 4); return 0;
    case P_LENGTH: if (len < 8) return ERR_PARAM; memcpy(&t->length, data, 8); t->has_length = 1; return 0;
    default: return ERR_PARAM;   /* a resolução só se lê */
    }
}
OSStatus MusicTrackGetProperty(void *trk, u32 prop, void *data, u32 *len) {
    mtrack *t = as_trk(trk);
    if (!t || !len) return ERR_PARAM;
    u32 need = prop == P_LOOP ? 16 : prop == P_OFFSET || prop == P_LENGTH ? 8 : prop == P_MUTE || prop == P_SOLO ? 1 : prop == P_AUTOMATED ? 4 : prop == P_RESOLUTION ? 2 : 0;
    if (!need) return ERR_PARAM;
    if (prop == P_RESOLUTION && t != t->seq->tempo) return ERR_PARAM;
    if (!data) { *len = need; return 0; }
    if (*len < need) return ERR_PARAM;
    double len_v = t->has_length ? t->length : natural_length(t);
    switch (prop) {
    case P_LOOP: memset(data, 0, 16); memcpy(data, &t->loop_len, 8); memcpy((u8 *)data + 8, &t->loops, 4); break;
    case P_OFFSET: memcpy(data, &t->offset, 8); break;
    case P_MUTE: *(u8 *)data = t->mute; break;
    case P_SOLO: *(u8 *)data = t->solo; break;
    case P_AUTOMATED: memcpy(data, &t->automated, 4); break;
    case P_LENGTH: memcpy(data, &len_v, 8); break;
    case P_RESOLUTION: memcpy(data, &t->seq->resolution, 2); break;
    }
    *len = need;
    return 0;
}
OSStatus MusicTrackSetDestMIDIEndpoint(void *trk, u32 ep) { mtrack *t = as_trk(trk); if (!t) return ERR_PARAM; t->dest_ep = ep; t->has_dest_node = 0; return 0; }
OSStatus MusicTrackGetDestMIDIEndpoint(void *trk, u32 *ep) { mtrack *t = as_trk(trk); if (!t || !ep) return ERR_PARAM; if (!t->dest_ep) return ERR_ILLEGAL_TRACK_DEST; *ep = t->dest_ep; return 0; }
OSStatus MusicTrackSetDestNode(void *trk, i32 node) { mtrack *t = as_trk(trk); if (!t) return ERR_PARAM; t->dest_node = node; t->has_dest_node = 1; t->dest_ep = 0; return 0; }
OSStatus MusicTrackGetDestNode(void *trk, i32 *node) { mtrack *t = as_trk(trk); if (!t || !node) return ERR_PARAM; if (!t->has_dest_node) return ERR_ILLEGAL_TRACK_DEST; *node = t->dest_node; return 0; }
OSStatus MusicTrackClear(void *trk, double a, double b) {
    mtrack *t = as_trk(trk);
    if (!t || b < a) return ERR_PARAM;
    u32 w = 0;
    for (u32 i = 0; i < t->n; i++) { if (t->ev[i].t >= a && t->ev[i].t < b) free(t->ev[i].data); else t->ev[w++] = t->ev[i]; }
    t->n = w;
    return 0;
}

/* ---------------------------------------------------------------- iterador */
typedef struct { u32 magic; mtrack *t; u32 pos; } miter;
#define IT_MAGIC 0x49544552u
static miter *as_it(void *p) { miter *i = p; return i && i->magic == IT_MAGIC && as_trk(i->t) ? i : 0; }
OSStatus NewMusicEventIterator(void *trk, void **out) {
    mtrack *t = as_trk(trk);
    if (!t || !out) return ERR_PARAM;
    miter *i = calloc(1, sizeof *i);
    if (!i) return -108;
    i->magic = IT_MAGIC; i->t = t; i->pos = 0;
    *out = i;
    return 0;
}
OSStatus DisposeMusicEventIterator(void *p) { miter *i = as_it(p); if (!i) return ERR_PARAM; i->magic = 0; free(i); return 0; }
OSStatus MusicEventIteratorSeek(void *p, double time) {
    miter *i = as_it(p);
    if (!i) return ERR_PARAM;
    u32 k = 0;
    while (k < i->t->n && i->t->ev[k].t < time) k++;
    i->pos = k;
    return 0;
}
OSStatus MusicEventIteratorNextEvent(void *p) { miter *i = as_it(p); if (!i) return ERR_PARAM; if (i->pos >= i->t->n) return ERR_END_OF_TRACK; i->pos++; return 0; }
OSStatus MusicEventIteratorPreviousEvent(void *p) { miter *i = as_it(p); if (!i) return ERR_PARAM; if (!i->pos) return ERR_START_OF_TRACK; i->pos--; return 0; }
OSStatus MusicEventIteratorHasCurrentEvent(void *p, u8 *b) { miter *i = as_it(p); if (!i || !b) return ERR_PARAM; *b = i->pos < i->t->n; return 0; }
/* como a Apple: "tem próximo" é verdade enquanto há um evento atual (o laço clássico é while (has) { ler; Next }) */
OSStatus MusicEventIteratorHasNextEvent(void *p, u8 *b) { return MusicEventIteratorHasCurrentEvent(p, b); }
OSStatus MusicEventIteratorHasPreviousEvent(void *p, u8 *b) { miter *i = as_it(p); if (!i || !b) return ERR_PARAM; *b = i->pos > 0; return 0; }
OSStatus MusicEventIteratorGetEventInfo(void *p, double *time, u32 *type, const void **data, u32 *size) {
    miter *i = as_it(p);
    if (!i) return ERR_PARAM;
    if (i->pos >= i->t->n) return ERR_END_OF_TRACK;
    mevent *e = &i->t->ev[i->pos];
    if (time) *time = e->t;
    if (type) *type = e->type;
    if (data) *data = e->data;
    if (size) *size = e->size;
    return 0;
}
OSStatus MusicEventIteratorSetEventInfo(void *p, u32 type, const void *data) {
    miter *i = as_it(p);
    if (!i || !data) return ERR_PARAM;
    if (i->pos >= i->t->n) return ERR_END_OF_TRACK;
    mevent *e = &i->t->ev[i->pos];
    if (type != e->type) return ERR_INVALID_EVENT;
    memcpy(e->data, data, e->size);
    return 0;
}
/* muda o tempo e reposiciona o evento; o iterador continua nele */
OSStatus MusicEventIteratorSetEventTime(void *p, double time) {
    miter *i = as_it(p);
    if (!i || time < 0) return ERR_PARAM;
    if (i->pos >= i->t->n) return ERR_END_OF_TRACK;
    mtrack *t = i->t;
    mevent e = t->ev[i->pos];
    memmove(t->ev + i->pos, t->ev + i->pos + 1, (t->n - i->pos - 1) * sizeof *t->ev);
    t->n--;
    u32 lo = 0, hi = t->n;
    while (lo < hi) { u32 m = (lo + hi) / 2; if (t->ev[m].t <= time) lo = m + 1; else hi = m; }
    memmove(t->ev + lo + 1, t->ev + lo, (t->n - lo) * sizeof *t->ev);
    e.t = time;
    t->ev[lo] = e;
    t->n++;
    i->pos = lo;
    return 0;
}
OSStatus MusicEventIteratorDeleteEvent(void *p) {
    miter *i = as_it(p);
    if (!i) return ERR_PARAM;
    if (i->pos >= i->t->n) return ERR_END_OF_TRACK;
    mtrack *t = i->t;
    if (t->ev[i->pos].type == EV_AUPRESET) { id pr; memcpy(&pr, t->ev[i->pos].data + 8, sizeof pr); if (pr) objc_release(pr); }
    free(t->ev[i->pos].data);
    memmove(t->ev + i->pos, t->ev + i->pos + 1, (t->n - i->pos - 1) * sizeof *t->ev);
    t->n--;
    return 0;
}

/* ---------------------------------------------------------------- arquivo MIDI padrão */
typedef struct { const u8 *b; size_t n, p; int bad; } rd_t;
static u32 rd_be(rd_t *r, int k) { u32 v = 0; for (int i = 0; i < k; i++) { if (r->p >= r->n) { r->bad = 1; return 0; } v = v << 8 | r->b[r->p++]; } return v; }
static u32 rd_vlq(rd_t *r) { u32 v = 0; for (int i = 0; i < 4; i++) { if (r->p >= r->n) { r->bad = 1; return 0; } u8 c = r->b[r->p++]; v = v << 7 | (c & 0x7f); if (!(c & 0x80)) return v; } r->bad = 1; return v; }
/* nota aberta à espera do note off */
typedef struct { mtrack *t; u32 idx; double on; u8 ch, note; } open_note;
static OSStatus load_smf(mseq *s, const u8 *b, size_t n, u32 flags) {
    rd_t r = {b, n, 0, 0};
    if (rd_be(&r, 4) != FCC('M', 'T', 'h', 'd')) return ERR_INVALID_SEQ;
    u32 hl = rd_be(&r, 4);
    u32 fmt = rd_be(&r, 2), ntr = rd_be(&r, 2), div = rd_be(&r, 2);
    if (r.bad || hl < 6 || fmt > 2 || !div) return ERR_INVALID_SEQ;
    r.p = 8 + hl;
    double tpq = div & 0x8000 ? 0 : div, tps = div & 0x8000 ? (double)(256 - (div >> 8)) * (div & 0xff) : 0;
    s->resolution = (i16)(div & 0x8000 ? 480 : div);
    int split = flags & 1;   /* kMusicSequenceLoadSMF_ChannelsToTracks */
    mtrack *chan_tr[16] = {0}, *misc = 0;
    open_note *open = 0;
    u32 nopen = 0, copen = 0;
    OSStatus rc = 0;
    for (u32 ti = 0; ti < ntr && !rc; ti++) {
        while (r.p + 8 <= n && rd_be(&r, 4) != FCC('M', 'T', 'r', 'k')) { u32 skip = rd_be(&r, 4); r.p += skip; }
        if (r.p > n) break;
        u32 len = rd_be(&r, 4);
        size_t end = r.p + len > n ? n : r.p + len;
        mtrack *t = 0;   /* criada na hora: a trilha 0 do tipo 1 só com metas vira a de tempo */
        u64 tick = 0;
        u8 status = 0;
        nopen = 0;
        while (r.p < end && !r.bad && !rc) {
            tick += rd_vlq(&r);
            double beats = tpq ? tick / tpq : tick / tps * 2;   /* SMPTE: segundos a 120 bpm */
            u8 c = r.p < end ? b[r.p] : 0;
            if (c & 0x80) { r.p++; if (c < 0xf0) status = c; } else c = status;
            if (!c) { r.bad = 1; break; }
            if (c == 0xff) {
                u8 mt = (u8)rd_be(&r, 1);
                u32 ml = rd_vlq(&r);
                if (r.p + ml > end) { r.bad = 1; break; }
                if (mt == 0x51 && ml == 3) {
                    u32 us = (u32)b[r.p] << 16 | (u32)b[r.p + 1] << 8 | b[r.p + 2];
                    double bpm = us ? 60000000.0 / us : 120;
                    rc = add_event(s->tempo, beats, EV_EXT_TEMPO, &bpm, 8);
                } else if (mt != 0x2f) {
                    int to_tempo = (fmt == 1 && ti == 0) || (fmt == 0 && (mt == 0x58 || mt == 0x59 || mt == 0x54));
                    mtrack *dst = to_tempo ? s->tempo : t;
                    if (!dst) { if ((rc = MusicSequenceNewTrack(s, (void **)&t))) break; dst = t; }
                    u8 *m = calloc(1, 8 + ml);
                    if (!m) { rc = -108; break; }
                    m[0] = mt; memcpy(m + 4, &ml, 4); memcpy(m + 8, b + r.p, ml);
                    rc = add_event(dst, beats, EV_META, m, 8 + ml);
                    free(m);
                } else if (t && !t->has_length) { t->length = beats; t->has_length = 1; }
                r.p += ml;
                continue;
            }
            if (c == 0xf0 || c == 0xf7) {
                u32 sl = rd_vlq(&r);
                if (r.p + sl > end) { r.bad = 1; break; }
                mtrack *dst = split ? misc : t;
                if (!dst) { if ((rc = MusicSequenceNewTrack(s, (void **)&dst))) break; if (split) misc = dst; else t = dst; }
                u8 *raw = malloc(4 + 1 + sl);
                if (!raw) { rc = -108; break; }
                u32 rl = (c == 0xf0 ? 1 : 0) + sl;
                memcpy(raw, &rl, 4);
                if (c == 0xf0) raw[4] = 0xf0;
                memcpy(raw + 4 + (c == 0xf0), b + r.p, sl);
                rc = add_event(dst, beats, EV_RAW, raw, 4 + rl);
                free(raw);
                r.p += sl;
                continue;
            }
            int nd = (c & 0xf0) == 0xc0 || (c & 0xf0) == 0xd0 ? 1 : 2;
            if (r.p + (size_t)nd > end) { r.bad = 1; break; }
            u8 d1 = b[r.p] & 0x7f, d2 = nd == 2 ? b[r.p + 1] & 0x7f : 0;
            r.p += (size_t)nd;
            u8 ch = c & 15;
            mtrack *dst = split ? chan_tr[ch] : t;
            if (!dst) { if ((rc = MusicSequenceNewTrack(s, (void **)&dst))) break; if (split) chan_tr[ch] = dst; else t = dst; }
            if ((c & 0xf0) == 0x90 && d2) {   /* note on: a duração vem do note off */
                note_msg m = {ch, d1, d2, 0, 0};
                if ((rc = add_event(dst, beats, EV_NOTE, &m, sizeof m))) break;
                if (nopen == copen) { copen = copen ? copen * 2 : 64; open_note *o = realloc(open, copen * sizeof *o); if (!o) { rc = -108; break; } open = o; }
                u32 idx = 0;
                for (u32 k = dst->n; k-- > 0;) if (dst->ev[k].t == beats && dst->ev[k].type == EV_NOTE && dst->ev[k].data[0] == ch && dst->ev[k].data[1] == d1) { idx = k; break; }
                open[nopen++] = (open_note){dst, idx, beats, ch, d1};
            } else if ((c & 0xf0) == 0x80 || (c & 0xf0) == 0x90) {   /* note off (ou on com velocity 0): fecha a mais antiga */
                for (u32 k = 0; k < nopen; k++)
                    if (open[k].ch == ch && open[k].note == d1 && open[k].t == dst) {
                        mevent *e = &dst->ev[open[k].idx];
                        note_msg m; memcpy(&m, e->data, sizeof m);
                        m.duration = (float)(beats - open[k].on);
                        m.release = (c & 0xf0) == 0x80 ? d2 : 0;
                        memcpy(e->data, &m, sizeof m);
                        memmove(open + k, open + k + 1, (nopen - k - 1) * sizeof *open);
                        nopen--;
                        break;
                    }
            } else {
                u8 cm[4] = {c, d1, d2, 0};
                rc = add_event(dst, beats, EV_CHANNEL, cm, 4);
            }
            /* os índices das notas abertas mudam quando um evento entra antes delas na mesma trilha */
            if (!rc && nopen && dst->n) {
                for (u32 k = 0; k < nopen; k++) if (open[k].t == dst && dst->ev[open[k].idx].t > beats) open[k].idx++;
            }
        }
        r.p = end;
    }
    free(open);
    return rc ? rc : r.bad ? ERR_INVALID_SEQ : 0;
}
OSStatus MusicSequenceFileLoadData(void *p, id data, u32 type, u32 flags) {
    mseq *s = as_seq(p);
    if (!s || !data) return ERR_PARAM;
    if (type && type != FCC('m', 'i', 'd', 'i')) return ERR_INVALID_SEQ;   /* iMelody não */
    const u8 *b = MSG0(const u8 *, data, "bytes");
    u64 n = MSG0(u64, data, "length");
    return b ? load_smf(s, b, (size_t)n, flags) : ERR_PARAM;
}
OSStatus MusicSequenceFileLoad(void *p, id url, u32 type, u32 flags) {
    mseq *s = as_seq(p);
    const char *path = url ? cx_audio_url_path(url) : 0;
    if (!s || !path) return ERR_PARAM;
    if (type && type != FCC('m', 'i', 'd', 'i')) return ERR_INVALID_SEQ;
    FILE_ *f = fopen(path, "rb");
    if (!f) return -43;   /* fnfErr */
    fseek(f, 0, 2);
    long n = ftell(f);
    fseek(f, 0, 0);
    u8 *b = n > 0 ? malloc((size_t)n) : 0;
    OSStatus rc = b && fread(b, 1, (size_t)n, f) == (size_t)n ? load_smf(s, b, (size_t)n, flags) : ERR_INVALID_SEQ;
    fclose(f);
    free(b);
    return rc;
}

typedef struct { u8 *b; size_t n, cap; int bad; } wb_t;
static void wput(wb_t *w, const void *d, size_t n) {
    if (w->bad) return;
    if (w->n + n > w->cap) { size_t nc = (w->n + n) * 2 + 256; u8 *x = realloc(w->b, nc); if (!x) { w->bad = 1; return; } w->b = x; w->cap = nc; }
    memcpy(w->b + w->n, d, n); w->n += n;
}
static void w8(wb_t *w, u32 v) { u8 c = (u8)v; wput(w, &c, 1); }
static void wbe(wb_t *w, u32 v, int k) { for (int i = k - 1; i >= 0; i--) w8(w, v >> (8 * i)); }
static void wvlq(wb_t *w, u32 v) { u8 t[5]; int n = 0; t[n++] = v & 0x7f; while (v >>= 7) t[n++] = (u8)(0x80 | (v & 0x7f)); while (n) w8(w, t[--n]); }
typedef struct { u64 tick; int order; u8 *bytes; u32 n; } smf_ev;   /* order: offs antes de ons no mesmo tick */
static int smf_cmp(const smf_ev *a, const smf_ev *b) { return a->tick < b->tick ? -1 : a->tick > b->tick ? 1 : a->order - b->order; }
static int write_track(wb_t *w, const mtrack *t, double res, int is_tempo) {
    smf_ev *e = 0;
    u32 n = 0, cap = 0;
#define PUSH(tk, ord, ptr, len) do { if (n == cap) { cap = cap ? cap * 2 : 64; smf_ev *x = realloc(e, cap * sizeof *x); if (!x) { free(e); return -1; } e = x; } \
        e[n].tick = (tk); e[n].order = (ord); e[n].bytes = malloc(len); if (!e[n].bytes) return -1; memcpy(e[n].bytes, ptr, len); e[n].n = (len); n++; } while (0)
    for (u32 i = 0; i < t->n; i++) {
        const mevent *v = &t->ev[i];
        u64 tk = (u64)(v->t * res + 0.5);
        if (v->type == EV_NOTE) {
            note_msg m; memcpy(&m, v->data, sizeof m);
            u8 on[3] = {(u8)(0x90 | (m.channel & 15)), m.note & 0x7f, m.velocity & 0x7f}, off[3] = {(u8)(0x80 | (m.channel & 15)), m.note & 0x7f, m.release & 0x7f};
            PUSH(tk, 1, on, 3);
            PUSH((u64)((v->t + m.duration) * res + 0.5), 0, off, 3);
        } else if (v->type == EV_CHANNEL) {
            u8 c = v->data[0];
            int nd = (c & 0xf0) == 0xc0 || (c & 0xf0) == 0xd0 ? 1 : 2;
            u8 m[3] = {c, v->data[1] & 0x7f, v->data[2] & 0x7f};
            PUSH(tk, 1, m, (u32)(1 + nd));
        } else if (v->type == EV_RAW) {
            u32 l; memcpy(&l, v->data, 4);
            const u8 *d = v->data + 4;
            u8 hdr[8]; wb_t tmp = {0};
            if (l && d[0] == 0xf0) { w8(&tmp, 0xf0); wvlq(&tmp, l - 1); wput(&tmp, d + 1, l - 1); }
            else { w8(&tmp, 0xf7); wvlq(&tmp, l); wput(&tmp, d, l); }
            (void)hdr;
            if (tmp.bad) return -1;
            PUSH(tk, 1, tmp.b, (u32)tmp.n);
            free(tmp.b);
        } else if (v->type == EV_META) {
            u32 l; memcpy(&l, v->data + 4, 4);
            wb_t tmp = {0};
            w8(&tmp, 0xff); w8(&tmp, v->data[0]); wvlq(&tmp, l); wput(&tmp, v->data + 8, l);
            if (tmp.bad) return -1;
            PUSH(tk, 1, tmp.b, (u32)tmp.n);
            free(tmp.b);
        } else if (v->type == EV_EXT_TEMPO && is_tempo) {
            double bpm = ev_bpm(v);
            u32 us = (u32)(60000000.0 / bpm + 0.5);
            u8 m[6] = {0xff, 0x51, 3, (u8)(us >> 16), (u8)(us >> 8), (u8)us};
            PUSH(tk, 1, m, 6);
        }
    }
#undef PUSH
    for (u32 i = 1; i < n; i++) for (u32 j = i; j > 0 && smf_cmp(&e[j - 1], &e[j]) > 0; j--) { smf_ev x = e[j]; e[j] = e[j - 1]; e[j - 1] = x; }
    wb_t body = {0};
    u64 last = 0;
    for (u32 i = 0; i < n; i++) { wvlq(&body, (u32)(e[i].tick - last)); last = e[i].tick; wput(&body, e[i].bytes, e[i].n); free(e[i].bytes); }
    free(e);
    wvlq(&body, 0); w8(&body, 0xff); w8(&body, 0x2f); w8(&body, 0);
    if (body.bad) { free(body.b); return -1; }
    wbe(w, FCC('M', 'T', 'r', 'k'), 4); wbe(w, (u32)body.n, 4); wput(w, body.b, body.n);
    free(body.b);
    return 0;
}
OSStatus MusicSequenceFileCreateData(void *p, u32 type, u32 flags, i16 resolution, id *out) {
    mseq *s = as_seq(p);
    (void)flags;
    if (!s || !out) return ERR_PARAM;
    if (type != FCC('m', 'i', 'd', 'i')) return ERR_INVALID_SEQ;
    double res = resolution > 0 ? resolution : 480;
    wb_t w = {0};
    wbe(&w, FCC('M', 'T', 'h', 'd'), 4); wbe(&w, 6, 4); wbe(&w, 1, 2); wbe(&w, s->nt + 1, 2); wbe(&w, (u32)res, 2);
    int bad = write_track(&w, s->tempo, res, 1);
    for (u32 i = 0; i < s->nt && !bad; i++) bad = write_track(&w, s->tr[i], res, 0);
    if (bad || w.bad) { free(w.b); return -108; }
    *out = objc_retain(MSG(id, (id)cx_class_named("NSData"), "dataWithBytes:length:", w.b, (u64)w.n));
    free(w.b);
    return 0;
}
OSStatus MusicSequenceFileCreate(void *p, id url, u32 type, u32 flags, i16 resolution) {
    id d = 0;
    OSStatus rc = MusicSequenceFileCreateData(p, type, flags, resolution, &d);
    const char *path = url ? cx_audio_url_path(url) : 0;
    if (rc) return rc;
    if (!path) { objc_release(d); return ERR_PARAM; }
    FILE_ *f = fopen(path, "wb");
    if (!f) { objc_release(d); return -43; }
    u64 n = MSG0(u64, d, "length");
    rc = fwrite(MSG0(const void *, d, "bytes"), 1, (size_t)n, f) == n ? 0 : -36;
    fclose(f);
    objc_release(d);
    return rc;
}

/* ---------------------------------------------------------------- MusicPlayer */
typedef struct { u32 ep; i32 node; void *unit; u8 ch, note; double off; } playing;
typedef struct {
    u32 magic;
    mseq *seq;
    volatile int running;
    double beat0, t0, rate, prerolled_at;
    double pos;                 /* batida atual quando parado */
    unsigned long th;
    u8 lock[64];
    playing *on;
    u32 non, con;
    u32 client, port;
} mplayer;
#define PL_MAGIC 0x504c4159u
static mplayer *as_pl(void *p) { mplayer *m = p; return m && m->magic == PL_MAGIC ? m : 0; }
static double host_secs(void) { return (double)mach_absolute_time() / 1e9; }
OSStatus NewMusicPlayer(void **out) {
    if (!out) return ERR_PARAM;
    mplayer *m = calloc(1, sizeof *m);
    if (!m) return -108;
    m->magic = PL_MAGIC;
    m->rate = 1;
    pthread_mutex_init(m->lock, 0);
    *out = m;
    return 0;
}
OSStatus MusicPlayerStop(void *p);
OSStatus DisposeMusicPlayer(void *p) {
    mplayer *m = as_pl(p);
    if (!m) return ERR_PARAM;
    MusicPlayerStop(p);
    m->magic = 0;
    free(m->on);
    free(m);
    return 0;
}
OSStatus MusicPlayerSetSequence(void *p, void *seq) {
    mplayer *m = as_pl(p);
    if (!m || (seq && !as_seq(seq))) return ERR_PARAM;
    if (m->running) MusicPlayerStop(p);
    m->seq = seq;
    m->pos = 0;
    return 0;
}
OSStatus MusicPlayerGetSequence(void *p, void **out) { mplayer *m = as_pl(p); if (!m || !out) return ERR_PARAM; if (!m->seq) return ERR_NO_SEQUENCE; *out = m->seq; return 0; }
static double now_beats(mplayer *m) {
    if (!m->running || !m->seq) return m->pos;
    double secs = (host_secs() - m->t0) * m->rate;
    return secs_to_beats(m->seq, beats_to_secs(m->seq, m->beat0) + secs);
}
OSStatus MusicPlayerGetTime(void *p, double *out) { mplayer *m = as_pl(p); if (!m || !out) return ERR_PARAM; if (!m->seq) return ERR_NO_SEQUENCE; *out = now_beats(m); return 0; }
OSStatus MusicPlayerIsPlaying(void *p, u8 *out) { mplayer *m = as_pl(p); if (!m || !out) return ERR_PARAM; *out = m->running != 0; return 0; }
OSStatus MusicPlayerSetPlayRateScalar(void *p, double r) {
    mplayer *m = as_pl(p);
    if (!m || r <= 0) return ERR_PARAM;
    if (m->running) { m->beat0 = now_beats(m); m->t0 = host_secs(); }
    m->rate = r;
    return 0;
}
OSStatus MusicPlayerGetPlayRateScalar(void *p, double *r) { mplayer *m = as_pl(p); if (!m || !r) return ERR_PARAM; *r = m->rate; return 0; }
OSStatus MusicPlayerPreroll(void *p) {
    mplayer *m = as_pl(p);
    if (!m) return ERR_PARAM;
    if (!m->seq) return ERR_NO_SEQUENCE;
    if (!m->client && MIDIClientCreate(cx_str("MusicPlayer"), 0, 0, &m->client) == 0) MIDIOutputPortCreate(m->client, cx_str("MusicPlayer"), &m->port);
    m->prerolled_at = m->pos;
    return 0;
}
/* uma mensagem de canal (ou bytes crus) para o destino da trilha */
static void emit(mplayer *m, const mtrack *t, const u8 *bytes, u32 n) {
    u32 ep = t->dest_ep ? t->dest_ep : m->seq->ep;
    if (t->has_dest_node && m->seq->graph) {
        void *unit = 0;
        if (!AUGraphNodeInfo(m->seq->graph, t->dest_node, 0, &unit) && unit) {
            if (bytes[0] == 0xf0) { if (MusicDeviceSysEx) MusicDeviceSysEx(unit, bytes, n); }
            else MusicDeviceMIDIEvent(unit, bytes[0], n > 1 ? bytes[1] : 0, n > 2 ? bytes[2] : 0, 0);
        }
        return;
    }
    if (!ep || !m->port) return;
    u8 buf[1024];
    if (n > sizeof buf - 64) return;
    void *cur = MIDIPacketListInit(buf);
    if (MIDIPacketListAdd(buf, sizeof buf, cur, 0, n, bytes)) MIDISend(m->port, ep, buf);
}
static int track_audible(const mseq *s, const mtrack *t) {
    if (t->mute) return 0;
    for (u32 i = 0; i < s->nt; i++) if (s->tr[i]->solo) return t->solo;
    return 1;
}
static void note_off(mplayer *m, playing *pl) {
    const mtrack *t = 0;
    for (u32 i = 0; i < m->seq->nt; i++) if (m->seq->tr[i]->dest_ep == pl->ep) { t = m->seq->tr[i]; break; }
    u8 off[3] = {(u8)(0x80 | pl->ch), pl->note, 0};
    mtrack fake = {0};
    fake.dest_ep = pl->ep; fake.dest_node = pl->node; fake.has_dest_node = pl->node >= 0 && !pl->ep;
    emit(m, t ? t : &fake, off, 3);
}
/* eventos de uma trilha em [a, b) do tempo da sequência, com deslocamento e loop */
static void play_window(mplayer *m, mtrack *t, double a, double b) {
    double len = t->loop_len > 0 ? t->loop_len : 0;
    double la = a - t->offset, lb = b - t->offset;
    if (lb <= 0) return;
    long first = 0, last = 0;
    if (len > 0) {
        first = la < 0 ? 0 : (long)(la / len);
        last = (long)(lb / len);
        if (t->loops > 0 && last > t->loops - 1) last = t->loops - 1;
    }
    for (long it = first; it <= last; it++) {
        double base = it * len;
        for (u32 i = 0; i < t->n; i++) {
            mevent *e = &t->ev[i];
            if (len > 0 && e->t >= len) break;
            double at = base + e->t;
            if (at < la || at >= lb) continue;
            double abs_t = at + t->offset;
            switch (e->type) {
            case EV_NOTE: {
                note_msg n; memcpy(&n, e->data, sizeof n);
                u8 on[3] = {(u8)(0x90 | (n.channel & 15)), n.note & 0x7f, n.velocity & 0x7f};
                emit(m, t, on, 3);
                if (m->non == m->con) { u32 nc = m->con ? m->con * 2 : 32; playing *x = realloc(m->on, nc * sizeof *x); if (!x) break; m->on = x; m->con = nc; }
                m->on[m->non++] = (playing){t->dest_ep ? t->dest_ep : m->seq->ep, t->has_dest_node ? t->dest_node : -1, 0, n.channel & 15, n.note & 0x7f, abs_t + n.duration};
                break;
            }
            case EV_CHANNEL: { int nd = (e->data[0] & 0xf0) == 0xc0 || (e->data[0] & 0xf0) == 0xd0 ? 1 : 2; emit(m, t, e->data, (u32)(1 + nd)); break; }
            case EV_RAW: { u32 l; memcpy(&l, e->data, 4); emit(m, t, e->data + 4, l); break; }
            case EV_USER:
                if (m->seq->user_cb) m->seq->user_cb(m->seq->user_ctx, m->seq, t, abs_t, e->data, a, b);
                break;
            default: break;   /* meta, tempo (já está no mapa), parâmetro, preset */
            }
        }
    }
}
static void *player_thread(void *arg) {
    mplayer *m = arg;
    double last = m->beat0;
    while (m->running) {
        usleep(2000);
        pthread_mutex_lock(m->lock);
        double nb = now_beats(m);
        if (nb > last) {
            for (u32 i = 0; i < m->seq->nt; i++) if (track_audible(m->seq, m->seq->tr[i])) play_window(m, m->seq->tr[i], last, nb);
            for (u32 k = 0; k < m->non;) {   /* note offs vencidos */
                if (m->on[k].off <= nb) { note_off(m, &m->on[k]); m->on[k] = m->on[--m->non]; }
                else k++;
            }
            last = nb;
        }
        pthread_mutex_unlock(m->lock);
    }
    return 0;
}
OSStatus MusicPlayerStart(void *p) {
    mplayer *m = as_pl(p);
    if (!m) return ERR_PARAM;
    if (!m->seq) return ERR_NO_SEQUENCE;
    if (m->running) return 0;
    if (!m->port) MusicPlayerPreroll(p);
    m->beat0 = m->pos;
    m->t0 = host_secs();
    m->running = 1;
    /* o que está exatamente no ponto de partida toca já */
    for (u32 i = 0; i < m->seq->nt; i++) if (track_audible(m->seq, m->seq->tr[i])) play_window(m, m->seq->tr[i], m->beat0, m->beat0 + 1e-9);
    m->beat0 += 1e-9;
    if (pthread_create(&m->th, 0, player_thread, m)) { m->running = 0; return ERR_INVALID_PLAYER; }
    return 0;
}
OSStatus MusicPlayerStop(void *p) {
    mplayer *m = as_pl(p);
    if (!m) return ERR_PARAM;
    if (!m->running) return 0;
    m->pos = now_beats(m);
    m->running = 0;
    pthread_join(m->th, 0);
    for (u32 k = 0; k < m->non; k++) note_off(m, &m->on[k]);   /* nenhuma nota fica presa */
    m->non = 0;
    return 0;
}
OSStatus MusicPlayerSetTime(void *p, double beats) {
    mplayer *m = as_pl(p);
    if (!m || beats < 0) return ERR_PARAM;
    if (!m->seq) return ERR_NO_SEQUENCE;
    pthread_mutex_lock(m->lock);
    if (m->running) {
        for (u32 k = 0; k < m->non; k++) note_off(m, &m->on[k]);
        m->non = 0;
        m->beat0 = beats; m->t0 = host_secs();
    }
    m->pos = beats;
    pthread_mutex_unlock(m->lock);
    return 0;
}
OSStatus MusicPlayerGetHostTimeForBeats(void *p, double beats, u64 *out) {
    mplayer *m = as_pl(p);
    if (!m || !out) return ERR_PARAM;
    if (!m->running) return ERR_INVALID_PLAYER;
    double s = m->t0 + (beats_to_secs(m->seq, beats) - beats_to_secs(m->seq, m->beat0)) / m->rate;
    *out = (u64)(s * 1e9);
    return 0;
}
OSStatus MusicPlayerGetBeatsForHostTime(void *p, u64 host, double *out) {
    mplayer *m = as_pl(p);
    if (!m || !out) return ERR_PARAM;
    if (!m->running) return ERR_INVALID_PLAYER;
    double secs = ((double)host / 1e9 - m->t0) * m->rate;
    *out = secs_to_beats(m->seq, beats_to_secs(m->seq, m->beat0) + secs);
    return 0;
}
