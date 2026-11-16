/* Host: userland/libsystem/musicseq.c com o coremidi.c de verdade. Mapa de tempo, iterador, propriedades, SMF (ida e
 * volta, tipo 0 feito à mão com running status, note on velocity 0, SysEx e ChannelsToTracks) e o MusicPlayer tocando
 * para um destino virtual do CoreMIDI no tempo certo. gcc -std=gnu11 -O1 tools/musicseq_test.c musicseq.c coremidi.c
 * -lpthread → "musicseq: N/N ok" */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

typedef void *id;
typedef int OSStatus;
/* runtime mínimo: strings, NSData e o trampolim do objc_msgSend */
typedef struct { void *isa; unsigned long long flags; char *s; unsigned long long n; } str_t;
static char DATA_ISA, DATA_CLASS;
typedef struct { void *isa; unsigned char *b; unsigned long long n; } data_t;
id cx_str(const char *s) { str_t *o = calloc(1, sizeof *o); o->s = strdup(s); o->n = strlen(s); return o; }
const char *cx_cstr(id o) { return o ? ((str_t *)o)->s : 0; }
id objc_retain(id o) { return o; }
void objc_release(id o) { (void)o; }
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }
void *cx_class_named(const char *n) { return !strcmp(n, "NSData") ? &DATA_CLASS : 0; }
const char *sel_registerName(const char *s) { return s; }
const char *cx_audio_url_path(id url) { return cx_cstr(url); }
unsigned long long mach_absolute_time(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (unsigned long long)t.tv_sec * 1000000000ull + t.tv_nsec; }
static id d_make(void *k, const char *s, const void *b, unsigned long long n) { (void)k; (void)s; data_t *d = calloc(1, sizeof *d); d->isa = &DATA_ISA; d->b = malloc(n); memcpy(d->b, b, n); d->n = n; return d; }
static const void *d_bytes(data_t *d, const char *s) { (void)s; return d->b; }
static unsigned long long d_len(data_t *d, const char *s) { (void)s; return d->n; }
static long ret0(void) { return 0; }
void *fake_lookup(void *self, const char *sel) {
    if (self == &DATA_CLASS && !strcmp(sel, "dataWithBytes:length:")) return (void *)d_make;
    if (self && *(void **)self == &DATA_ISA) { if (!strcmp(sel, "bytes")) return (void *)d_bytes; if (!strcmp(sel, "length")) return (void *)d_len; }
    fprintf(stderr, "seletor: %s\n", sel);
    return (void *)ret0;
}
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\npush %rbp\nmov %rsp,%rbp\nsub $0x40,%rsp\n"
        "mov %rdi,0(%rsp)\nmov %rsi,8(%rsp)\nmov %rdx,16(%rsp)\nmov %rcx,24(%rsp)\nmov %r8,32(%rsp)\nmov %r9,40(%rsp)\n"
        "call fake_lookup\nmov %rax,%r11\n"
        "mov 0(%rsp),%rdi\nmov 8(%rsp),%rsi\nmov 16(%rsp),%rdx\nmov 24(%rsp),%rcx\nmov 32(%rsp),%r8\nmov 40(%rsp),%r9\n"
        "leave\njmp *%r11\n");

OSStatus NewMusicSequence(void **), DisposeMusicSequence(void *), MusicSequenceNewTrack(void *, void **), MusicSequenceGetTempoTrack(void *, void **);
OSStatus MusicSequenceGetTrackCount(void *, unsigned *), MusicSequenceGetIndTrack(void *, unsigned, void **);
OSStatus MusicSequenceGetSecondsForBeats(void *, double, double *), MusicSequenceSetUserCallback(void *, void *, void *);
OSStatus MusicTrackNewExtendedTempoEvent(void *, double, double), MusicTrackNewMIDINoteEvent(void *, double, const void *);
OSStatus MusicTrackNewMIDIChannelEvent(void *, double, const void *), MusicTrackNewUserEvent(void *, double, const void *);
OSStatus MusicTrackNewMetaEvent(void *, double, const void *);
OSStatus MusicTrackGetProperty(void *, unsigned, void *, unsigned *), MusicTrackSetProperty(void *, unsigned, const void *, unsigned);
OSStatus MusicTrackSetDestMIDIEndpoint(void *, unsigned);
OSStatus NewMusicEventIterator(void *, void **), DisposeMusicEventIterator(void *), MusicEventIteratorSeek(void *, double);
OSStatus MusicEventIteratorNextEvent(void *), MusicEventIteratorHasCurrentEvent(void *, unsigned char *), MusicEventIteratorHasNextEvent(void *, unsigned char *);
OSStatus MusicEventIteratorGetEventInfo(void *, double *, unsigned *, const void **, unsigned *);
OSStatus MusicEventIteratorSetEventTime(void *, double), MusicEventIteratorDeleteEvent(void *);
OSStatus MusicSequenceFileCreateData(void *, unsigned, unsigned, short, id *), MusicSequenceFileLoadData(void *, id, unsigned, unsigned);
OSStatus NewMusicPlayer(void **), MusicPlayerSetSequence(void *, void *), MusicPlayerStart(void *), MusicPlayerStop(void *);
OSStatus MusicPlayerIsPlaying(void *, unsigned char *), MusicPlayerGetTime(void *, double *), DisposeMusicPlayer(void *);
OSStatus MIDIClientCreate(id, void *, void *, unsigned *), MIDIDestinationCreate(unsigned, id, void *, void *, unsigned *);
typedef struct { unsigned char channel, note, velocity, release; float duration; } note_msg;
#define MIDI_FCC 0x6d696469u

static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
/* o que chega no destino virtual: hora e bytes */
typedef struct { double at; unsigned char b[3]; } got_t;
static got_t got[64];
static int ngot;
static pthread_mutex_t gl = PTHREAD_MUTEX_INITIALIZER;
static double now_s(void) { return (double)mach_absolute_time() / 1e9; }
static void read_proc(const unsigned char *list, void *ref, void *conn) {
    (void)ref; (void)conn;
    unsigned np; memcpy(&np, list, 4);
    const unsigned char *p = list + 4;
    for (unsigned i = 0; i < np; i++) {
        unsigned short len; memcpy(&len, p + 8, 2);
        for (unsigned k = 0; k + 2 < len + 1u && k < len; k += 3) {
            pthread_mutex_lock(&gl);
            if (ngot < 64) { got[ngot].at = now_s(); memcpy(got[ngot].b, p + 10 + k, 3); ngot++; }
            pthread_mutex_unlock(&gl);
        }
        p += 10 + len;
        p = (const unsigned char *)(((unsigned long)p + 3) & ~3ul);
    }
}
static int user_hits;
static void user_cb(void *ctx, void *seq, void *trk, double t, const void *d, double a, double b) { (void)ctx; (void)seq; (void)trk; (void)t; (void)d; (void)a; (void)b; user_hits++; }

int main(void) {
    void *seq, *tempo, *tr;
    NewMusicSequence(&seq);
    MusicSequenceGetTempoTrack(seq, &tempo);
    MusicSequenceNewTrack(seq, &tr);
    MusicTrackNewExtendedTempoEvent(tempo, 0, 120);
    MusicTrackNewExtendedTempoEvent(tempo, 4, 60);
    double sec = 0;
    MusicSequenceGetSecondsForBeats(seq, 6, &sec);
    expect(fabs(sec - 4.0) < 1e-12, "segundos por batida com mudança de tempo");
    expect(MusicTrackNewExtendedTempoEvent(tr, 0, 90) != 0, "tempo fora da trilha de tempo é recusado");
    note_msg n1 = {0, 60, 100, 0, 0.5f}, n2 = {1, 64, 90, 0, 1.0f};
    MusicTrackNewMIDINoteEvent(tr, 2, &n2);
    MusicTrackNewMIDINoteEvent(tr, 0, &n1);
    unsigned char cc[4] = {0xb0, 7, 100, 0};
    MusicTrackNewMIDIChannelEvent(tr, 1, &cc);
    void *it; NewMusicEventIterator(tr, &it);
    unsigned char has = 0; int count = 0; double tt[4];
    MusicEventIteratorHasNextEvent(it, &has);
    while (has) { double t; MusicEventIteratorGetEventInfo(it, &t, 0, 0, 0); if (count < 4) tt[count] = t; count++; MusicEventIteratorNextEvent(it); MusicEventIteratorHasNextEvent(it, &has); }
    expect(count == 3 && tt[0] == 0 && tt[1] == 1 && tt[2] == 2, "iterador em ordem de tempo");
    MusicEventIteratorSeek(it, 1.5);
    MusicEventIteratorSetEventTime(it, 0.25);   /* a nota de 2 vai para 0,25 */
    double t; unsigned ty; const void *dp;
    MusicEventIteratorGetEventInfo(it, &t, &ty, &dp, 0);
    expect(t == 0.25 && ty == 6 && ((const note_msg *)dp)->note == 64, "SetEventTime reposiciona e o iterador segue o evento");
    double len = 0; unsigned sz = 8;
    MusicTrackGetProperty(tr, 5, &len, &sz);
    expect(fabs(len - 1.25) < 1e-9, "comprimento natural da trilha (fim da nota mais longa)");
    struct { double d; int n; int pad; } loop = {4, 3, 0}, lget; sz = sizeof lget;
    MusicTrackSetProperty(tr, 0, &loop, sizeof loop); MusicTrackGetProperty(tr, 0, &lget, &sz);
    expect(lget.d == 4 && lget.n == 3 && sz == 16, "LoopInfo");
    loop.d = 0; MusicTrackSetProperty(tr, 0, &loop, sizeof loop);
    MusicEventIteratorSeek(it, 1); MusicEventIteratorDeleteEvent(it);
    MusicEventIteratorSeek(it, 0); count = 0; MusicEventIteratorHasCurrentEvent(it, &has);
    while (has) { count++; MusicEventIteratorNextEvent(it); MusicEventIteratorHasCurrentEvent(it, &has); }
    expect(count == 2, "DeleteEvent");
    DisposeMusicEventIterator(it);

    /* SMF: ida e volta */
    struct { unsigned char type, u1, u2, u3; unsigned len; char d[8]; } meta = {0x03, 0, 0, 0, 5, "Piano"};
    MusicTrackNewMetaEvent(tr, 0, &meta);
    id data = 0;
    expect(MusicSequenceFileCreateData(seq, MIDI_FCC, 0, 480, &data) == 0 && data, "FileCreateData");
    void *s2; NewMusicSequence(&s2);
    expect(MusicSequenceFileLoadData(s2, data, 0, 0) == 0, "FileLoadData do que foi gravado");
    unsigned ntr = 0; MusicSequenceGetTrackCount(s2, &ntr);
    void *t2 = 0; if (ntr) MusicSequenceGetIndTrack(s2, 0, &t2);
    int notes = 0, metas = 0, ok_notes = 1;
    if (t2) {
        NewMusicEventIterator(t2, &it); MusicEventIteratorHasCurrentEvent(it, &has);
        while (has) {
            unsigned ty2; const void *d2;
            MusicEventIteratorGetEventInfo(it, &t, &ty2, &d2, 0);
            if (ty2 == 6) { const note_msg *m = d2; notes++; if (!((m->note == 60 && t == 0 && m->duration == 0.5f && m->velocity == 100) || (m->note == 64 && t == 0.25 && m->duration == 1.0f && m->channel == 1))) ok_notes = 0; }
            if (ty2 == 5 && !memcmp((const char *)d2 + 8, "Piano", 5)) metas++;
            MusicEventIteratorNextEvent(it); MusicEventIteratorHasCurrentEvent(it, &has);
        }
        DisposeMusicEventIterator(it);
    }
    MusicSequenceGetSecondsForBeats(s2, 6, &sec);
    expect(ntr == 1 && notes == 2 && ok_notes && metas == 1 && fabs(sec - 4.0) < 1e-6, "SMF ida e volta: notas, duração, meta e tempo");

    /* SMF tipo 0 feito à mão: running status, note on velocity 0 como note off, SysEx, ChannelsToTracks */
    static const unsigned char smf0[] = {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96,
        'M', 'T', 'r', 'k', 0, 0, 0, 33,
        0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20,   /* 500000 us = 120 bpm */
        0x00, 0x90, 60, 100,                        /* canal 0 */
        0x00, 0x91, 67, 80,                         /* canal 1 */
        0x60, 0x90, 60, 0,                          /* 96 ticks depois: velocity 0 = note off */
        0x00, 0x81, 67, 64,                         /* note off de verdade */
        0x00, 0xf0, 0x03, 0x7e, 0x01, 0xf7,         /* SysEx */
        0x00, 0xff, 0x2f, 0x00};
    data_t raw = {&DATA_ISA, (unsigned char *)smf0, sizeof smf0};
    void *s3; NewMusicSequence(&s3);
    expect(MusicSequenceFileLoadData(s3, &raw, MIDI_FCC, 1) == 0, "SMF tipo 0 com ChannelsToTracks");
    MusicSequenceGetTrackCount(s3, &ntr);
    expect(ntr == 3, "3 trilhas (canal 0, canal 1, SysEx)");
    int good = 0;
    for (unsigned k = 0; k < ntr; k++) {
        void *tk; MusicSequenceGetIndTrack(s3, k, &tk);
        NewMusicEventIterator(tk, &it); MusicEventIteratorHasCurrentEvent(it, &has);
        while (has) {
            unsigned ty2; const void *d2;
            MusicEventIteratorGetEventInfo(it, &t, &ty2, &d2, 0);
            if (ty2 == 6) { const note_msg *m = d2; if (m->duration == 1.0f && ((m->note == 60 && m->channel == 0) || (m->note == 67 && m->channel == 1 && m->release == 64))) good++; }
            if (ty2 == 8) { unsigned l; memcpy(&l, d2, 4); if (l == 4 && ((const unsigned char *)d2)[4] == 0xf0 && ((const unsigned char *)d2)[7] == 0xf7) good++; }
            MusicEventIteratorNextEvent(it); MusicEventIteratorHasCurrentEvent(it, &has);
        }
        DisposeMusicEventIterator(it);
    }
    expect(good == 3, "notas com duração (velocity 0 e note off) e SysEx com F0");

    /* player: 120 bpm, notas em 0, 0,5 e 1 batida (0, 0,25 e 0,5 s), duração 0,25 batida */
    unsigned cl, dest;
    MIDIClientCreate(cx_str("teste"), 0, 0, &cl);
    MIDIDestinationCreate(cl, cx_str("destino"), (void *)read_proc, 0, &dest);
    void *s4, *t4, *pl;
    NewMusicSequence(&s4); MusicSequenceNewTrack(s4, &t4);
    MusicSequenceSetUserCallback(s4, (void *)user_cb, 0);
    for (int i = 0; i < 3; i++) { note_msg m = {2, (unsigned char)(70 + i), 99, 0, 0.25f}; MusicTrackNewMIDINoteEvent(t4, i * 0.5, &m); }
    struct { unsigned len; unsigned char d[4]; } ue = {1, {42}};
    MusicTrackNewUserEvent(t4, 0.75, &ue);
    MusicTrackSetDestMIDIEndpoint(t4, dest);
    NewMusicPlayer(&pl); MusicPlayerSetSequence(pl, s4);
    double t0 = now_s();
    MusicPlayerStart(pl);
    usleep(150000);
    unsigned char playing = 0; MusicPlayerIsPlaying(pl, &playing);
    double pos = 0; MusicPlayerGetTime(pl, &pos);
    expect(playing && pos > 0.2 && pos < 0.4, "tocando e o tempo anda");
    usleep(700000);
    MusicPlayerStop(pl);
    pthread_mutex_lock(&gl);
    int ons = 0, offs = 0, timing = 1;
    for (int i = 0; i < ngot; i++) {
        double rel = got[i].at - t0;
        if ((got[i].b[0] & 0xf0) == 0x90) { double want = (got[i].b[1] - 70) * 0.25; if (fabs(rel - want) > 0.03) timing = 0; ons++; }
        if ((got[i].b[0] & 0xf0) == 0x80) { double want = (got[i].b[1] - 70) * 0.25 + 0.125; if (fabs(rel - want) > 0.03) timing = 0; offs++; }
    }
    pthread_mutex_unlock(&gl);
    printf("  player: %d note on, %d note off, callback %d\n", ons, offs, user_hits);
    expect(ons == 3 && offs == 3 && timing && user_hits == 1, "MusicPlayer: notas no destino do CoreMIDI no tempo (±30 ms) e evento de usuário");
    DisposeMusicPlayer(pl);
    DisposeMusicSequence(seq); DisposeMusicSequence(s2); DisposeMusicSequence(s3); DisposeMusicSequence(s4);
    printf("musicseq: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
