/* Host: userland/libsystem/audiotoolbox_mais.c com o coreaudio.c de verdade. → "audiotoolbox_mais: N/N ok" */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef int OSStatus;
typedef void *id;
/* runtime mínimo: strings e arrays (o que o AudioFileGetGlobalInfo devolve) */
static char STR, ARR, ARRCLS;
typedef struct { void *isa; char *s; void **v; int n; } fo;
id cx_str(const char *s) { fo *o = calloc(1, sizeof *o); o->isa = &STR; o->s = strdup(s); return o; }
const char *cx_cstr(id o) { return o && ((fo *)o)->isa == &STR ? ((fo *)o)->s : 0; }
void *cx_class_named(const char *n) { return !strcmp(n, "NSMutableArray") ? &ARRCLS : 0; }
const char *sel_registerName(const char *s) { return s; }
id objc_retain(id o) { return o; }
void objc_release(id o) { (void)o; }
int cx_tap_owns(unsigned d) { (void)d; return 0; }
int cx_tap_remove_ioproc(void *p) { (void)p; return 0; }
unsigned long long mach_absolute_time(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (unsigned long long)t.tv_sec * 1000000000ull + t.tv_nsec; }
static id f_arr(void *k, const char *s) { (void)k; (void)s; fo *o = calloc(1, sizeof *o); o->isa = &ARR; o->v = calloc(32, sizeof(void *)); return o; }
static void f_add(fo *a, const char *s, id x) { (void)s; a->v[a->n++] = x; }
static long r0(void) { return 0; }
void *fake_lookup(void *self, const char *sel) {
    if (self == &ARRCLS && !strcmp(sel, "array")) return (void *)f_arr;
    if (self && ((fo *)self)->isa == &ARR && !strcmp(sel, "addObject:")) return (void *)f_add;
    return (void *)r0;
}
__asm__(".text\n.globl objc_msgSend\nobjc_msgSend:\npush %rbp\nmov %rsp,%rbp\nsub $0x40,%rsp\n"
        "mov %rdi,0(%rsp)\nmov %rsi,8(%rsp)\nmov %rdx,16(%rsp)\nmov %rcx,24(%rsp)\nmov %r8,32(%rsp)\nmov %r9,40(%rsp)\n"
        "call fake_lookup\nmov %rax,%r11\n"
        "mov 0(%rsp),%rdi\nmov 8(%rsp),%rsi\nmov 16(%rsp),%rdx\nmov 24(%rsp),%rcx\nmov 32(%rsp),%r8\nmov 40(%rsp),%r9\n"
        "leave\njmp *%r11\n");

typedef struct { short subframes, divisor; unsigned counter, type, flags; short hours, minutes, seconds, frames; } smpte_t;
typedef struct { unsigned format, reserved; union { unsigned long long host; double v; smpte_t smpte; } t; } catime_t;
OSStatus CAClockNew(unsigned, void **), CAClockStart(void *), CAClockStop(void *), CAClockDispose(void *);
OSStatus CAClockSetProperty(void *, unsigned, unsigned, const void *), CAClockGetCurrentTime(void *, unsigned, catime_t *);
OSStatus CAClockSetCurrentTime(void *, const catime_t *), CAClockSecondsToSMPTETime(void *, double, unsigned short, smpte_t *);
OSStatus CAClockSMPTETimeToSeconds(void *, const smpte_t *, double *);
OSStatus AudioFileGetGlobalInfo(unsigned, unsigned, const void *, unsigned *, void *), AudioFileGetGlobalInfoSize(unsigned, unsigned, const void *, unsigned *);
void *AudioComponentFindNext(void *, const void *);
void AudioComponentInstantiate(void *, unsigned, void *);
OSStatus AudioUnitInitialize(void *);
typedef struct { void *unit; unsigned id, scope, element; } auparam_t;
typedef struct { unsigned type, pad; auparam_t p; } auevent_t;
OSStatus AUEventListenerCreate(void *, void *, void *, id, float, float, void **), AUEventListenerAddEventType(void *, void *, const auevent_t *);
OSStatus AUParameterSet(void *, void *, const auparam_t *, float, unsigned), AUListenerDispose(void *);
OSStatus AudioConverterNew(const void *, const void *, void **), AudioConverterFillBuffer(void *, void *, void *, unsigned *, void *);
#define FCC(a, b, c, d) ((unsigned)(a) << 24 | (unsigned)(b) << 16 | (unsigned)(c) << 8 | (unsigned)(d))

static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
static void *inst_got; static int inst_rc = -1;
static void inst_inv(void *b, void *inst, OSStatus rc) { (void)b; inst_got = inst; inst_rc = rc; }
static float heard = -1; static int heard_n;
static void ev_cb(void *ud, void *obj, const auevent_t *ev, unsigned long long host, float v) { (void)ud; (void)obj; (void)ev; (void)host; heard = v; heard_n++; }
static short pcm[2048]; static int pcm_pos;
static OSStatus old_in(void *conv, unsigned *sz, void **data, void *ud) { (void)conv; (void)ud; unsigned want = *sz; if (want > 400) want = 400; if (pcm_pos * 2 + want > sizeof pcm) want = sizeof pcm - pcm_pos * 2; *data = pcm + pcm_pos; *sz = want; pcm_pos += want / 2; return 0; }

int main(void) {
    void *c; CAClockNew(0, &c);
    struct { double b, bpm; } tm[2] = {{0, 120}, {4, 60}};
    CAClockSetProperty(c, FCC('t', 'm', 'p', 'o'), sizeof tm, tm);
    catime_t t = {FCC('b', 'e', 'a', 't'), 0, {0}}; t.t.v = 6;
    CAClockSetCurrentTime(c, &t);
    catime_t s; CAClockGetCurrentTime(c, FCC('s', 'e', 'c', 's'), &s);
    expect(fabs(s.t.v - 4) < 1e-9, "batidas -> segundos pelo mapa de tempo");
    double rate = 44100; CAClockSetProperty(c, FCC('s', 'r', 'a', 't'), 8, &rate);
    CAClockGetCurrentTime(c, FCC('s', 'a', 'm', 'p'), &s);
    expect(fabs(s.t.v - 176400) < 1e-6, "amostras");
    CAClockStart(c); usleep(100000); CAClockStop(c);
    CAClockGetCurrentTime(c, FCC('s', 'e', 'c', 's'), &s);
    expect(s.t.v > 4.09 && s.t.v < 4.15, "anda com o host enquanto roda");
    smpte_t m; unsigned ty = 3; CAClockSetProperty(c, FCC('s', 'm', 'p', 'f'), 4, &ty);
    CAClockSecondsToSMPTETime(c, 3725.5, 0, &m);
    expect(m.hours == 1 && m.minutes == 2 && m.seconds == 5 && m.frames == 15, "SMPTE 30 fps");
    ty = 2; CAClockSetProperty(c, FCC('s', 'm', 'p', 'f'), 4, &ty);
    CAClockSecondsToSMPTETime(c, 1800 / (30000.0 / 1001) + 1e-7, 0, &m);
    double back = 0; CAClockSMPTETimeToSeconds(c, &m, &back);
    expect(m.minutes == 1 && m.seconds == 0 && m.frames == 2 && fabs(back - 1800 / (30000.0 / 1001)) < 1e-6, "SMPTE 29,97 drop: quadro 1800 = 00:01:00;02 e volta");
    CAClockDispose(c);

    unsigned sz = 0; AudioFileGetGlobalInfoSize(FCC('a', 'f', 'r', 'f'), 0, 0, &sz);
    unsigned types[16]; AudioFileGetGlobalInfo(FCC('a', 'f', 'r', 'f'), 0, 0, &sz, types);
    unsigned w[4], wsz = sizeof w; AudioFileGetGlobalInfo(FCC('a', 'f', 'w', 'f'), 0, 0, &wsz, w);
    expect(sz == 32 && types[0] == FCC('W', 'A', 'V', 'E') && wsz == 4 && w[0] == FCC('W', 'A', 'V', 'E'), "tipos de leitura e escrita");
    id nm = 0; unsigned nsz = sizeof nm, wave = FCC('W', 'A', 'V', 'E');
    AudioFileGetGlobalInfo(FCC('f', 't', 'n', 'm'), 4, &wave, &nsz, &nm);
    id ext = cx_str("MP3"); unsigned found[4], fsz = sizeof found;
    AudioFileGetGlobalInfo(FCC('t', 'y', 'e', 'x'), sizeof ext, &ext, &fsz, found);
    expect(nm && !strcmp(cx_cstr(nm), "WAVE") && fsz == 4 && found[0] == FCC('M', 'P', 'G', '3'), "nome do tipo e tipo pela extensão");

    unsigned desc[5] = {FCC('a', 'u', 'm', 'x'), FCC('m', 'c', 'm', 'x'), FCC('a', 'p', 'p', 'l'), 0, 0};
    void *comp = AudioComponentFindNext(0, desc);
    struct { void *isa; int fl, re; void (*inv)(void *, void *, OSStatus); } blk = {0, 0, 0, inst_inv};
    AudioComponentInstantiate(comp, 0, &blk);
    expect(comp && inst_rc == 0 && inst_got, "AudioComponentInstantiate entrega a instância ao bloco");
    if (inst_got) AudioUnitInitialize(inst_got);
    void *lis = 0;
    AUEventListenerCreate((void *)ev_cb, 0, 0, 0, 0, 0, &lis);
    auevent_t ev = {0, 0, {inst_got, 0, 1, 0}};   /* volume (0), escopo de entrada, elemento 0 */
    AUEventListenerAddEventType(lis, 0, &ev);
    auparam_t p = {inst_got, 0, 1, 0};
    OSStatus prc = AUParameterSet(0, 0, &p, 0.25f, 0);
    expect(prc == 0 && heard_n == 1 && fabsf(heard - 0.25f) < 1e-6, "AUParameterSet avisa o ouvinte com o valor novo");
    AUListenerDispose(lis);
    AUParameterSet(0, 0, &p, 0.5f, 0);
    expect(heard_n == 1, "ouvinte descartado não recebe mais");

    struct { double rate; unsigned fmt, flags, bpp, fpp, bpf, ch, bits, res; } in = {44100, FCC('l', 'p', 'c', 'm'), 4 | 8, 4, 1, 4, 2, 16, 0},
        out = {44100, FCC('l', 'p', 'c', 'm'), 1 | 8, 8, 1, 8, 2, 32, 0};
    for (int i = 0; i < 2048; i++) pcm[i] = (short)((i * 37) % 30000 - 15000);
    void *conv = 0;
    OSStatus crc = AudioConverterNew(&in, &out, &conv);
    float fo_[2048]; unsigned osz = sizeof fo_;
    OSStatus frc = conv ? AudioConverterFillBuffer(conv, (void *)old_in, 0, &osz, fo_) : -1;
    int good = crc == 0 && frc == 0 && osz == sizeof fo_;
    for (int i = 0; good && i < 2048; i++) if (fabsf(fo_[i] - pcm[i] / 32768.0f) > 1e-6f) good = 0;
    expect(good, "AudioConverterFillBuffer: int16 -> float32 pela API antiga");
    printf("audiotoolbox_mais: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
