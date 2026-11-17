/*
 * mOS libSystem — fala: NSSpeechSynthesizer (AppKit) e o Speech Manager do ApplicationServices (NewSpeechChannel,
 * SpeakCFString, StopSpeechAt, PauseSpeechAt, ContinueSpeech, SpeechBusy, SetSpeechRate/Pitch, SetSpeechProperty,
 * CountVoices, GetIndVoice, GetVoiceDescription...), sobre o sintetizador original do mOS (speech_core.c).
 *
 * Vozes do mOS: Ana e Pedro (português do Brasil), Emma e Daniel (inglês dos EUA). O som sai por um stream de áudio
 * do sistema (GUI_DEVICE/DEV_AUDIO_*, 48 kHz estéreo), gerado num fio enquanto toca; parar e pausar valem na hora, no
 * fim da palavra ou da frase. As chamadas de volta (palavra, fonema, fim) vão para o fio principal.
 * startSpeakingString:toURL: e kSpeechOutputToFileURLProperty gravam AIFF (16 bits, mono, 48 kHz).
 */
#include "foundation.h"
#include "libc.h"
#include "sys.h"
#include "speech_core.h"
#include "../../kernel/include/compat_gui.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
void *dispatch_get_main_queue(void);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
int usleep(unsigned us);
typedef short i16;
typedef struct { u64 loc, len; } nsr;
typedef int OSErr;

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sel_registerName(s)); }
static long dev3(long op, long a, long b, long c) { return SYS4(GUI_DEVICE, op, a, b, c); }
static id n_bool(BOOL v) { return MSG(id, (id)kls("NSNumber"), "numberWithBool:", v); }
static id n_ll(i64 v) { return MSG(id, (id)kls("NSNumber"), "numberWithLongLong:", v); }
static id n_long(long v) { return MSG(id, (id)kls("NSNumber"), "numberWithLong:", v); }
static id n_int(int v) { return MSG(id, (id)kls("NSNumber"), "numberWithInt:", v); }
static id s_utf8(const char *t) { return MSG(id, (id)kls("NSString"), "stringWithUTF8String:", t); }
static id ns_error(i64 code) { return MSG(id, (id)kls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSOSStatusErrorDomain"), code, (id)0); }
static void dset(id d, id v, id k) { MSG(void, d, "setObject:forKey:", v, k); }

/* ================================================================ constantes */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfs_t;
#define SK(var, text) static cfs_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
SK(NSVoiceAge, "VoiceAge") SK(NSVoiceGender, "VoiceGender") SK(NSVoiceDemoText, "VoiceDemoText") SK(NSVoiceLanguage, "VoiceLanguage")
SK(NSVoiceGenderMale, "VoiceGenderMale") SK(NSVoiceGenderFemale, "VoiceGenderFemale") SK(NSVoiceGenderNeuter, "VoiceGenderNeuter")
SK(NSVoiceSupportedCharacters, "VoiceSupportedCharacters") SK(NSVoiceIndividuallySpokenCharacters, "VoiceIndividuallySpokenCharacters")
SK(NSSpeechStatusNumberOfCharactersLeft, "NumberOfCharactersLeft") SK(NSSpeechStatusPhonemeCode, "PhonemeCode")
SK(NSSpeechCurrentVoiceProperty, "cvox") SK(NSSpeechErrorsProperty, "erro") SK(NSSpeechInputModeProperty, "inpt")
SK(NSSpeechCharacterModeProperty, "char") SK(NSSpeechNumberModeProperty, "nmbr") SK(NSSpeechOutputToFileURLProperty, "opaf")
SK(NSSpeechCommandDelimiterProperty, "dlim") SK(NSSpeechSynthesizerInfoProperty, "vers")
SK(kSpeechCurrentVoiceProperty, "cvox") SK(kSpeechSpeechDoneCallBack, "sddn") SK(kSpeechVoiceCreator, "VoiceCreator")
SK(kSpeechVoiceID, "VoiceID") SK(kSpeechVolumeProperty, "volm") SK(kSpeechRateProperty, "rate") SK(kSpeechPitchBaseProperty, "pbas")
SK(kSpeechPitchModProperty, "pmod") SK(kSpeechStatusProperty, "stat") SK(kSpeechWordCFCallBack, "wcfb")
SK(kSpeechPhonemeCallBack, "phcb") SK(kSpeechRefConProperty, "refc") SK(kSpeechResetProperty, "rset")
SK(kSpeechOutputToFileURLProperty, "opaf") SK(kSpeechErrorCFCallBack, "eecb") SK(kSpeechSyncCallBack, "sycb")
extern id NSVoiceName, NSVoiceIdentifier, NSVoiceLocaleIdentifier, NSSpeechRateProperty, NSSpeechVolumeProperty,
    NSSpeechPitchBaseProperty, NSSpeechPitchModProperty, NSSpeechStatusProperty, NSSpeechStatusOutputBusy, NSSpeechStatusOutputPaused,
    NSSpeechResetProperty, NSSpeechPhonemeSymbolsProperty;
#define KS(v) ((id)(v))

/* ================================================================ vozes */
typedef struct { const char *ident, *name, *locale, *lang_name, *demo; int lang, female, age; double f0, scale, breath; } voice_t;
static const voice_t VOICES[] = {
    {"com.mos.speech.voice.Ana", "Ana", "pt_BR", "pt", "Olá, meu nome é Ana e esta é a minha voz.", SP_LANG_PT, 1, 30, 205, 1.17, 0.35},
    {"com.mos.speech.voice.Pedro", "Pedro", "pt_BR", "pt", "Olá, meu nome é Pedro e esta é a minha voz.", SP_LANG_PT, 0, 35, 112, 1.0, 0.2},
    {"com.mos.speech.voice.Emma", "Emma", "en_US", "en", "Hello, my name is Emma and this is my voice.", SP_LANG_EN, 1, 30, 210, 1.17, 0.35},
    {"com.mos.speech.voice.Daniel", "Daniel", "en_US", "en", "Hello, my name is Daniel and this is my voice.", SP_LANG_EN, 0, 35, 108, 1.0, 0.2},
};
#define NVOICES (int)(sizeof VOICES / sizeof VOICES[0])
static int voice_index(id ident) {
    if (!ident) return -1;
    const char *s = cx_cstr(ident);
    for (int i = 0; i < NVOICES; i++) if (!strcmp(s, VOICES[i].ident)) return i;
    return -1;
}
/* a voz padrão segue o idioma preferido do sistema */
static int default_voice(void) {
    id langs = MSG0(id, (id)kls("NSLocale"), "preferredLanguages");
    id first = langs && MSG0(u64, langs, "count") ? MSG(id, langs, "objectAtIndex:", (u64)0) : 0;
    const char *l = first ? cx_cstr(first) : "pt";
    return l[0] == 'e' && l[1] == 'n' ? 2 : 0;
}

/* ================================================================ o falante (comum ao NSSpeechSynthesizer e ao canal) */
typedef struct speaker {
    int voice;
    double wpm, pitch_hz, pitch_mod, volume;
    /* fala em andamento */
    sp_ph *ph; int nph;
    char *text;
    sp_synth syn;
    volatile int speaking, paused, stop_req, stop_at, pause_req, pause_at, stopped;
    unsigned long th;
    /* donos e chamadas de volta */
    id ns;                                   /* NSSpeechSynthesizer (fraco) */
    void (*done_cb)(void *chan, long ref);
    void (*word_cb)(void *chan, long ref, id str, i64 loc, i64 len);
    void (*phon_cb)(void *chan, long ref, i16 op);
    long refcon;
    void *chan;                              /* o canal do Speech Manager, se for um */
    id file_url;                             /* gravar em arquivo */
    id cfstr;                                /* o texto como CFString, para a chamada de palavra */
} speaker;
static volatile int g_busy;
static speaker *sp_new(int voice) {
    speaker *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->voice = voice >= 0 ? voice : default_voice();
    s->wpm = 175; s->pitch_hz = 0; s->pitch_mod = 0.35; s->volume = 1;
    return s;
}
static sp_voice voice_params(const speaker *s) {
    const voice_t *v = &VOICES[s->voice];
    sp_voice r = {v->f0, v->scale, v->breath, v->lang};
    return r;
}
/* ---- chamadas de volta no fio principal */
typedef struct { speaker *s; int kind; i64 loc, len; i16 op; int finished; } cb_t;
static void deliver(void *x) {
    cb_t *c = x;
    speaker *s = c->s;
    if (c->kind == 0) {   /* palavra */
        if (s->ns) {
            id dl = MSG0(id, s->ns, "delegate");
            if (responds(dl, "speechSynthesizer:willSpeakWord:ofString:"))
            {
                nsr r = {(u64)c->loc, (u64)c->len};
                ((void (*)(id, SEL, id, nsr, id))objc_msgSend)(dl, sel_registerName("speechSynthesizer:willSpeakWord:ofString:"), s->ns, r, s->cfstr);
            }
        }
        if (s->word_cb) s->word_cb(s->chan, s->refcon, s->cfstr, c->loc, c->len);
    } else if (c->kind == 1) {   /* fonema */
        if (s->ns) {
            id dl = MSG0(id, s->ns, "delegate");
            if (responds(dl, "speechSynthesizer:willSpeakPhoneme:")) MSG(void, dl, "speechSynthesizer:willSpeakPhoneme:", s->ns, (i16)c->op);
        }
        if (s->phon_cb) s->phon_cb(s->chan, s->refcon, c->op);
    } else {   /* fim */
        if (s->ns) {
            id dl = MSG0(id, s->ns, "delegate");
            if (responds(dl, "speechSynthesizer:didFinishSpeaking:")) MSG(void, dl, "speechSynthesizer:didFinishSpeaking:", s->ns, (BOOL)c->finished);
        }
        if (s->done_cb) s->done_cb(s->chan, s->refcon);
    }
    free(c);
}
static void post(speaker *s, int kind, i64 loc, i64 len, i16 op, int finished) {
    cb_t *c = calloc(1, sizeof *c);
    if (!c) return;
    c->s = s; c->kind = kind; c->loc = loc; c->len = len; c->op = op; c->finished = finished;
    dispatch_async_f(dispatch_get_main_queue(), c, deliver);
}
/* posição de byte UTF-8 → índice UTF-16 (os intervalos do Cocoa são em UTF-16) */
static i64 utf16_index(const char *t, int byte) {
    i64 n = 0;
    for (int i = 0; i < byte && t[i];) {
        unsigned char c = (unsigned char)t[i];
        int len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        n += len == 4 ? 2 : 1;
        i += len;
    }
    return n;
}
/* pontos de parada: fim de palavra (mudou a palavra) e fim de frase (pausa longa) */
static int at_boundary(speaker *s, int where) {
    if (where == 0) return 1;
    int i = s->syn.cur;
    if (i >= s->nph) return 1;
    if (s->syn.sample_in_ph != 0) return 0;
    const sp_ph *p = &s->ph[i];
    if (where == 1) return i == 0 || p->ph == 0 || s->ph[i - 1].word_loc != p->word_loc;
    return p->ph == 0 && p->pause >= 3;
}
/* ---- AIFF */
static void be32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }
static void be16(u8 *p, u32 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static int write_aiff(const char *path, const i16 *pcm, u32 frames) {
    int fd = open(path, 0x601, 0644);
    if (fd < 0) return -1;
    u8 h[54];
    memset(h, 0, sizeof h);
    memcpy(h, "FORM", 4); be32(h + 4, 46 + frames * 2); memcpy(h + 8, "AIFF", 4);
    memcpy(h + 12, "COMM", 4); be32(h + 16, 18); be16(h + 20, 1); be32(h + 22, frames); be16(h + 26, 16);
    /* 48000 em extended de 80 bits: expoente 0x400E, mantissa 0xBB80 << 48 */
    h[28] = 0x40; h[29] = 0x0E; h[30] = 0xBB; h[31] = 0x80;
    memcpy(h + 38, "SSND", 4); be32(h + 42, 8 + frames * 2); be32(h + 46, 0); be32(h + 50, 0);
    long w = write(fd, h, sizeof h);
    u8 buf[4096];
    for (u32 i = 0; i < frames && w >= 0;) {
        u32 k = 0;
        for (; k < sizeof buf / 2 && i < frames; k++, i++) be16(buf + k * 2, (u16)pcm[i]);
        w = write(fd, buf, k * 2);
    }
    close(fd);
    return w < 0 ? -1 : 0;
}
static void *speak_thread(void *x) {
    speaker *s = x;
    enum { CH = 480 };   /* 10 ms */
    float mono[CH];
    i16 st[CH * 2];
    int to_file = s->file_url != 0;
    long stream = to_file ? -1 : dev3(DEV_AUDIO_OPEN, 0, 0, 0);
    i16 *filebuf = 0;
    u32 file_n = 0, file_cap = 0;
    int last_word = -1, last_ph = -1;
    while (!s->stopped) {
        if (s->stop_req && at_boundary(s, s->stop_at)) { s->stopped = 1; break; }
        if (s->pause_req && at_boundary(s, s->pause_at)) { s->paused = 1; s->pause_req = 0; }
        if (s->paused) { usleep(10000); continue; }
        if (s->syn.cur != last_ph && s->syn.cur < s->nph) {
            last_ph = s->syn.cur;
            const sp_ph *p = &s->ph[last_ph];
            post(s, 1, 0, 0, (i16)p->ph, 0);
            if (p->ph && p->word_len && p->word_loc != last_word) {
                last_word = p->word_loc;
                i64 a = utf16_index(s->text, p->word_loc), b = utf16_index(s->text, p->word_loc + p->word_len);
                post(s, 0, a, b - a, 0, 0);
            }
        }
        /* até o próximo limite de fonema, para as paradas caírem no lugar certo */
        int left = s->syn.ph_len - s->syn.sample_in_ph;
        int want = left > 0 && left < CH ? left : CH;
        int got = sp_synth_run(&s->syn, mono, want);
        if (got <= 0) break;
        double vol = s->volume;
        for (int i = 0; i < got; i++) {
            double v = mono[i] * vol;
            i16 q = (i16)(v * 32000.0);
            st[i * 2] = st[i * 2 + 1] = q;
        }
        if (to_file) {
            if (file_n + (u32)got > file_cap) {
                u32 nc = file_cap ? file_cap * 2 : 48000 * 10;
                while (nc < file_n + (u32)got) nc *= 2;
                i16 *nb = realloc(filebuf, (usize)nc * 2);
                if (!nb) break;
                filebuf = nb; file_cap = nc;
            }
            for (int i = 0; i < got; i++) filebuf[file_n++] = st[i * 2];
            continue;
        }
        if (stream < 0) { usleep((unsigned)(got * 1000000 / 48000)); continue; }   /* sem saída de som: segue o relógio */
        for (int off = 0; off < got && !s->stopped;) {
            gui_audio_state_t as;
            if (dev3(DEV_AUDIO_STATE, stream, (long)&as, 0) == 0 && as.queued > 48000 / 5) { usleep(5000); if (s->stop_req && s->stop_at == 0) break; continue; }
            long w = dev3(DEV_AUDIO_WRITE, stream, (long)(st + off * 2), got - off);
            if (w < 0) { s->stopped = 1; break; }
            if (w == 0) { usleep(3000); continue; }
            off += (int)w;
        }
    }
    int finished = !s->stopped;
    if (to_file) {
        if (filebuf) write_aiff(cx_cstr(MSG0(id, s->file_url, "path")), filebuf, file_n);
        free(filebuf);
    } else if (stream >= 0) {
        /* espera o que já foi para o mixer tocar (a não ser que tenha parado na hora) */
        for (int w = 0; finished && w < 400; w++) {
            gui_audio_state_t as;
            if (dev3(DEV_AUDIO_STATE, stream, (long)&as, 0) || !as.queued) break;
            usleep(10000);
        }
        if (!finished) dev3(DEV_AUDIO_CTL, stream, -1, 1);   /* descarta o resto */
        dev3(DEV_AUDIO_CLOSE, stream, 0, 0);
    }
    s->speaking = 0;
    __sync_fetch_and_sub(&g_busy, 1);
    post(s, 2, 0, 0, 0, finished);
    return 0;
}
static void sp_stop_now(speaker *s) {
    if (!s->speaking) return;
    s->stop_at = 0; s->stop_req = 1; s->paused = 0;
    for (int i = 0; i < 300 && s->speaking; i++) usleep(2000);
}
static int sp_speak(speaker *s, id text, id file_url) {
    if (!s || !text) return -50;
    sp_stop_now(s);
    const char *t = cx_cstr(MSG0(id, text, "description"));
    usize tl = strlen(t);
    free(s->text);
    s->text = malloc(tl + 1);
    if (!s->text) return -108;
    memcpy(s->text, t, tl + 1);
    int cap = (int)tl * 3 + 16;
    free(s->ph);
    s->ph = malloc((usize)cap * sizeof(sp_ph));
    if (!s->ph) return -108;
    s->nph = sp_text_to_phonemes(s->text, VOICES[s->voice].lang, s->ph, cap);
    if (s->cfstr) objc_release(s->cfstr);
    s->cfstr = objc_retain(s_utf8(s->text));
    if (s->file_url) objc_release(s->file_url);
    s->file_url = file_url ? objc_retain(file_url) : 0;
    sp_voice v = voice_params(s);
    sp_synth_init(&s->syn, s->ph, s->nph, &v, s->wpm / 175.0, 48000);
    s->syn.pitch = s->pitch_hz;
    s->syn.mod = s->pitch_mod;
    s->stop_req = s->pause_req = s->paused = s->stopped = 0;
    s->speaking = 1;
    __sync_fetch_and_add(&g_busy, 1);
    if (pthread_create(&s->th, 0, speak_thread, s) != 0) { s->speaking = 0; __sync_fetch_and_sub(&g_busy, 1); return -108; }
    pthread_detach(s->th);
    return 0;
}
static double hz_to_midi(double hz) { return hz > 0 ? 69 + 12 * __builtin_log2(hz / 440.0) : 0; }
static double midi_to_hz(double m) { return 440.0 * __builtin_pow(2.0, (m - 69) / 12.0); }

/* ================================================================ NSSpeechSynthesizer */
extern cx_class_t NSSpeechSynthesizer_c __asm__("OBJC_CLASS_$_NSSpeechSynthesizer");
static char k_sp;
static speaker *SP(id self) {
    id b = objc_getAssociatedObject(self, &k_sp);
    speaker *s = b ? MSG0(void *, b, "pointerValue") : 0;
    if (!s) {
        s = sp_new(-1);
        if (s) { s->ns = self; objc_setAssociatedObject(self, &k_sp, MSG(id, (id)kls("NSValue"), "valueWithPointer:", (void *)s), 1); }
    }
    return s;
}
static id ss_init_voice(id self, SEL c, id voice) {
    (void)c;
    int vi = voice ? voice_index(voice) : default_voice();
    if (vi < 0) return 0;   /* voz desconhecida: nil, como no Cocoa */
    speaker *s = SP(self);
    if (s) s->voice = vi;
    return self;
}
static id ss_init(id self, SEL c) { return ss_init_voice(self, c, 0); }
static BOOL ss_start(id self, SEL c, id text) { (void)c; return sp_speak(SP(self), text, 0) == 0; }
static BOOL ss_start_url(id self, SEL c, id text, id url) { (void)c; return url && sp_speak(SP(self), text, url) == 0; }
static void ss_stop(id self, SEL c) { (void)c; sp_stop_now(SP(self)); }
static void ss_stop_at(id self, SEL c, i64 where) { (void)c; speaker *s = SP(self); if (!s->speaking) return; if (where == 0) sp_stop_now(s); else { s->stop_at = (int)where; s->stop_req = 1; s->paused = 0; } }
static void ss_pause_at(id self, SEL c, i64 where) { (void)c; speaker *s = SP(self); if (s->speaking) { s->pause_at = (int)where; s->pause_req = 1; } }
static void ss_continue(id self, SEL c) { (void)c; speaker *s = SP(self); s->pause_req = 0; s->paused = 0; }
static BOOL ss_speaking(id self, SEL c) { (void)c; return SP(self)->speaking != 0; }
static BOOL ss_any(id cls, SEL c) { (void)cls; (void)c; return g_busy > 0; }
static char k_delegate, k_feedback;
static id ss_delegate(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_delegate); }
static void ss_set_delegate(id self, SEL c, id d) { (void)c; objc_setAssociatedObject(self, &k_delegate, d, 0); }
static id ss_voice(id self, SEL c) { (void)c; return cx_str(VOICES[SP(self)->voice].ident); }
static BOOL ss_set_voice(id self, SEL c, id voice) {
    (void)c;
    speaker *s = SP(self);
    int vi = voice ? voice_index(voice) : default_voice();
    if (vi < 0 || s->speaking) return NO;
    s->voice = vi;
    return YES;
}
static float ss_rate(id self, SEL c) { (void)c; return (float)SP(self)->wpm; }
static void ss_set_rate(id self, SEL c, float r) { (void)c; if (r > 20) SP(self)->wpm = r; }
static float ss_volume(id self, SEL c) { (void)c; return (float)SP(self)->volume; }
static void ss_set_volume(id self, SEL c, float v) { (void)c; SP(self)->volume = v < 0 ? 0 : v > 1 ? 1 : v; }
static BOOL ss_feedback(id self, SEL c) { (void)c; id v = objc_getAssociatedObject(self, &k_feedback); return v && MSG0(BOOL, v, "boolValue"); }
static void ss_set_feedback(id self, SEL c, BOOL v) { (void)c; objc_setAssociatedObject(self, &k_feedback, n_bool(v), 1); }
static id ss_phonemes(id self, SEL c, id text) {
    (void)c;
    if (!text) return cx_str("");
    const char *t = cx_cstr(text);
    int cap = (int)strlen(t) * 3 + 16;
    sp_ph *ph = malloc((usize)cap * sizeof(sp_ph));
    char *out = malloc((usize)cap * 4 + 16);
    if (!ph || !out) { free(ph); free(out); return cx_str(""); }
    int n = sp_text_to_phonemes(t, VOICES[SP(self)->voice].lang, ph, cap);
    sp_phonemes_string(ph, n, out, cap * 4 + 16);
    id r = s_utf8(out);
    free(ph); free(out);
    return r;
}
static id num_d(double v) { return MSG(id, (id)kls("NSNumber"), "numberWithDouble:", v); }
static id status_dict(speaker *s) {
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    dset(d, n_bool((BOOL)(s->speaking != 0)), NSSpeechStatusOutputBusy);
    dset(d, n_bool((BOOL)(s->paused != 0)), NSSpeechStatusOutputPaused);
    i64 left = 0;
    if (s->speaking && s->text && s->syn.cur < s->nph) left = (i64)strlen(s->text) - s->ph[s->syn.cur].word_loc;
    dset(d, n_ll(left), KS(NSSpeechStatusNumberOfCharactersLeft));
    dset(d, n_ll((i64)(s->speaking && s->syn.cur < s->nph ? s->ph[s->syn.cur].ph : 0)), KS(NSSpeechStatusPhonemeCode));
    return d;
}
static int is_key(id k, id want) { return k && want && MSG(BOOL, k, "isEqualToString:", want); }
static id ss_get_prop(id self, SEL c, id prop, id *err) {
    (void)c;
    if (err) *err = 0;
    speaker *s = SP(self);
    if (is_key(prop, NSSpeechRateProperty)) return num_d(s->wpm);
    if (is_key(prop, NSSpeechVolumeProperty)) return num_d(s->volume);
    if (is_key(prop, NSSpeechPitchBaseProperty)) return num_d(hz_to_midi(s->pitch_hz > 0 ? s->pitch_hz : VOICES[s->voice].f0));
    if (is_key(prop, NSSpeechPitchModProperty)) return num_d(s->pitch_mod * 100);
    if (is_key(prop, NSSpeechStatusProperty)) return status_dict(s);
    if (is_key(prop, KS(NSSpeechCurrentVoiceProperty))) return cx_str(VOICES[s->voice].ident);
    if (is_key(prop, KS(NSSpeechOutputToFileURLProperty))) return s->file_url;
    if (err) *err = ns_error(-231);   /* siUnknownInfoType */
    return 0;
}
static BOOL ss_set_prop(id self, SEL c, id value, id prop, id *err) {
    (void)c;
    if (err) *err = 0;
    speaker *s = SP(self);
    double v = value && responds(value, "doubleValue") ? MSG0(double, value, "doubleValue") : 0;
    if (is_key(prop, NSSpeechRateProperty)) { if (v > 20) s->wpm = v; return YES; }
    if (is_key(prop, NSSpeechVolumeProperty)) { s->volume = v < 0 ? 0 : v > 1 ? 1 : v; return YES; }
    if (is_key(prop, NSSpeechPitchBaseProperty)) { s->pitch_hz = midi_to_hz(v); return YES; }
    if (is_key(prop, NSSpeechPitchModProperty)) { s->pitch_mod = v / 100.0; return YES; }
    if (is_key(prop, NSSpeechResetProperty)) { s->wpm = 175; s->pitch_hz = 0; s->pitch_mod = 0.35; s->volume = 1; return YES; }
    if (is_key(prop, KS(NSSpeechCurrentVoiceProperty))) return ss_set_voice(self, 0, value);
    if (is_key(prop, KS(NSSpeechOutputToFileURLProperty))) { if (s->file_url) objc_release(s->file_url); s->file_url = value ? objc_retain(value) : 0; return YES; }
    if (err) *err = ns_error(-231);
    return NO;
}
static id ss_voices(id cls, SEL c) {
    (void)cls; (void)c;
    id a = MSG0(id, (id)kls("NSMutableArray"), "array");
    for (int i = 0; i < NVOICES; i++) MSG(void, a, "addObject:", cx_str(VOICES[i].ident));
    return a;
}
static id ss_default_voice(id cls, SEL c) { (void)cls; (void)c; return cx_str(VOICES[default_voice()].ident); }
static id ss_attrs(id cls, SEL c, id voice) {
    (void)cls; (void)c;
    int i = voice_index(voice);
    if (i < 0) return 0;
    const voice_t *v = &VOICES[i];
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    dset(d, cx_str(v->ident), NSVoiceIdentifier);
    dset(d, cx_str(v->name), NSVoiceName);
    dset(d, cx_str(v->locale), NSVoiceLocaleIdentifier);
    dset(d, cx_str(v->lang_name), KS(NSVoiceLanguage));
    dset(d, KS(v->female ? NSVoiceGenderFemale : NSVoiceGenderMale), KS(NSVoiceGender));
    dset(d, n_int(v->age), KS(NSVoiceAge));
    dset(d, s_utf8(v->demo), KS(NSVoiceDemoText));
    return d;
}

/* ================================================================ Speech Manager */
typedef struct { u32 creator, id_; } VoiceSpec;
#define MOS_CREATOR (((u32)'m' << 24) | ((u32)'O' << 16) | ((u32)'S' << 8) | ' ')
typedef struct { u32 magic; speaker *s; } chan_t;
static speaker *CHN(void *c) { chan_t *ch = c; return ch && ch->magic == 0x53504348 ? ch->s : 0; }
static int voice_from_spec(const VoiceSpec *v) { if (!v) return -1; if (v->creator != MOS_CREATOR || v->id_ < 1 || v->id_ > (u32)NVOICES) return -2; return (int)v->id_ - 1; }
OSErr NewSpeechChannel(VoiceSpec *voice, void **chan) {
    if (!chan) return -50;
    int vi = voice_from_spec(voice);
    if (vi == -2) return -244;   /* voiceNotFound */
    chan_t *c = calloc(1, sizeof *c);
    speaker *s = sp_new(vi);
    if (!c || !s) { free(c); free(s); return -108; }
    c->magic = 0x53504348; c->s = s; s->chan = c;
    *chan = c;
    return 0;
}
OSErr DisposeSpeechChannel(void *chan) {
    speaker *s = CHN(chan);
    if (!s) return -240;   /* noSynthFound / canal inválido */
    sp_stop_now(s);
    ((chan_t *)chan)->magic = 0;
    free(s->ph); free(s->text);
    if (s->cfstr) objc_release(s->cfstr);
    free(s);
    free(chan);
    return 0;
}
OSErr SpeakCFString(void *chan, id text, id options) { (void)options; speaker *s = CHN(chan); return s ? (OSErr)sp_speak(s, text, s->file_url) : -240; }
OSErr SpeakText(void *chan, const void *buf, long len) {
    speaker *s = CHN(chan);
    if (!s || !buf) return -240;
    id str = MSG(id, MSG0(id, (id)kls("NSString"), "alloc"), "initWithBytes:length:encoding:", buf, (u64)len, (u64)30);   /* Mac Roman */
    OSErr e = (OSErr)sp_speak(s, str, s->file_url);
    if (str) objc_release(str);
    return e;
}
OSErr StopSpeech(void *chan) { speaker *s = CHN(chan); if (!s) return -240; sp_stop_now(s); return 0; }
OSErr StopSpeechAt(void *chan, i32 where) {
    speaker *s = CHN(chan);
    if (!s) return -240;
    if (!s->speaking) return 0;
    if (where == 0) sp_stop_now(s); else { s->stop_at = where == 1 ? 1 : 2; s->stop_req = 1; s->paused = 0; }
    return 0;
}
OSErr PauseSpeechAt(void *chan, i32 where) { speaker *s = CHN(chan); if (!s) return -240; if (s->speaking) { s->pause_at = where == 0 ? 0 : where == 1 ? 1 : 2; s->pause_req = 1; } return 0; }
OSErr ContinueSpeech(void *chan) { speaker *s = CHN(chan); if (!s) return -240; s->pause_req = 0; s->paused = 0; return 0; }
i16 SpeechBusy(void) { return (i16)g_busy; }
i16 SpeechBusySystemWide(void) { return (i16)g_busy; }
/* velocidade (palavras por minuto) e altura (nota MIDI) em Fixed 16.16 */
OSErr SetSpeechRate(void *chan, i32 rate) { speaker *s = CHN(chan); if (!s) return -240; if (rate > 0) s->wpm = rate / 65536.0; return 0; }
OSErr GetSpeechRate(void *chan, i32 *rate) { speaker *s = CHN(chan); if (!s || !rate) return -240; *rate = (i32)(s->wpm * 65536); return 0; }
OSErr SetSpeechPitch(void *chan, i32 pitch) { speaker *s = CHN(chan); if (!s) return -240; s->pitch_hz = midi_to_hz(pitch / 65536.0); return 0; }
OSErr GetSpeechPitch(void *chan, i32 *pitch) {
    speaker *s = CHN(chan);
    if (!s || !pitch) return -240;
    *pitch = (i32)(hz_to_midi(s->pitch_hz > 0 ? s->pitch_hz : VOICES[s->voice].f0) * 65536);
    return 0;
}
static long num_long(id v) { return v && responds(v, "longValue") ? MSG0(long, v, "longValue") : 0; }
OSErr SetSpeechProperty(void *chan, id prop, id value) {
    speaker *s = CHN(chan);
    if (!s) return -240;
    double v = value && responds(value, "doubleValue") ? MSG0(double, value, "doubleValue") : 0;
    if (is_key(prop, KS(kSpeechRateProperty))) { if (v > 20) s->wpm = v; }
    else if (is_key(prop, KS(kSpeechVolumeProperty))) s->volume = v < 0 ? 0 : v > 1 ? 1 : v;
    else if (is_key(prop, KS(kSpeechPitchBaseProperty))) s->pitch_hz = midi_to_hz(v);
    else if (is_key(prop, KS(kSpeechPitchModProperty))) s->pitch_mod = v / 100.0;
    else if (is_key(prop, KS(kSpeechSpeechDoneCallBack))) s->done_cb = (void (*)(void *, long))num_long(value);
    else if (is_key(prop, KS(kSpeechWordCFCallBack))) s->word_cb = (void (*)(void *, long, id, i64, i64))num_long(value);
    else if (is_key(prop, KS(kSpeechPhonemeCallBack))) s->phon_cb = (void (*)(void *, long, i16))num_long(value);
    else if (is_key(prop, KS(kSpeechRefConProperty))) s->refcon = num_long(value);
    else if (is_key(prop, KS(kSpeechResetProperty))) { s->wpm = 175; s->pitch_hz = 0; s->pitch_mod = 0.35; s->volume = 1; }
    else if (is_key(prop, KS(kSpeechOutputToFileURLProperty))) { if (s->file_url) objc_release(s->file_url); s->file_url = value ? objc_retain(value) : 0; }
    else if (is_key(prop, KS(kSpeechCurrentVoiceProperty))) {
        if (s->speaking) return -236;   /* synthNotReady */
        id cr = value ? MSG(id, value, "objectForKey:", KS(kSpeechVoiceCreator)) : 0, vid = value ? MSG(id, value, "objectForKey:", KS(kSpeechVoiceID)) : 0;
        VoiceSpec vs = {(u32)num_long(cr), (u32)num_long(vid)};
        int vi = voice_from_spec(&vs);
        if (vi < 0) return -244;
        s->voice = vi;
    } else if (is_key(prop, KS(kSpeechErrorCFCallBack)) || is_key(prop, KS(kSpeechSyncCallBack))) { /* o mOS não gera erros nem marcas de sincronia no texto */ }
    else return -231;   /* siUnknownInfoType */
    return 0;
}
OSErr CopySpeechProperty(void *chan, id prop, id *out) {
    speaker *s = CHN(chan);
    if (!s || !out) return -240;
    *out = 0;
    if (is_key(prop, KS(kSpeechRateProperty))) *out = num_d(s->wpm);
    else if (is_key(prop, KS(kSpeechVolumeProperty))) *out = num_d(s->volume);
    else if (is_key(prop, KS(kSpeechPitchBaseProperty))) *out = num_d(hz_to_midi(s->pitch_hz > 0 ? s->pitch_hz : VOICES[s->voice].f0));
    else if (is_key(prop, KS(kSpeechPitchModProperty))) *out = num_d(s->pitch_mod * 100);
    else if (is_key(prop, KS(kSpeechStatusProperty))) *out = status_dict(s);
    else if (is_key(prop, KS(kSpeechCurrentVoiceProperty))) {
        id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
        dset(d, n_long((long)MOS_CREATOR), KS(kSpeechVoiceCreator));
        dset(d, n_long((long)(s->voice + 1)), KS(kSpeechVoiceID));
        *out = d;
    } else return -231;
    objc_retain(*out);
    return 0;
}
OSErr CountVoices(i16 *n) { if (!n) return -50; *n = (i16)NVOICES; return 0; }
OSErr GetIndVoice(i16 index, VoiceSpec *v) {
    if (!v || index < 1 || index > NVOICES) return -50;
    v->creator = MOS_CREATOR; v->id_ = (u32)index;
    return 0;
}
OSErr MakeVoiceSpec(u32 creator, u32 id_, VoiceSpec *v) { if (!v) return -50; v->creator = creator; v->id_ = id_; return 0; }
/* VoiceDescription (pack 2): tamanho, voz, versão, nome Str63, comentário Str255, gênero, idade, script, idioma,
 * região, reservado */
typedef struct __attribute__((packed, aligned(2))) {
    i32 length; VoiceSpec voice; i32 version; u8 name[64]; u8 comment[256]; i16 gender, age, script, language, region; i32 reserved[4];
} VoiceDescription;
static void pstr(u8 *d, const char *s, int cap) { int n = (int)strlen(s); if (n > cap - 1) n = cap - 1; d[0] = (u8)n; memcpy(d + 1, s, (usize)n); }
OSErr GetVoiceDescription(const VoiceSpec *spec, VoiceDescription *out, long len) {
    if (!out || len < 4) return -50;
    int vi = spec ? voice_from_spec(spec) : default_voice();
    if (vi < 0) return -244;
    const voice_t *v = &VOICES[vi];
    VoiceDescription d;
    memset(&d, 0, sizeof d);
    d.length = (i32)sizeof d;
    d.voice.creator = MOS_CREATOR; d.voice.id_ = (u32)vi + 1;
    d.version = 1;
    pstr(d.name, v->name, 64);
    pstr(d.comment, v->demo, 256);   /* em Mac Roman o acento sai trocado; o nome não tem acentos */
    d.gender = (i16)(v->female ? 2 : 1);
    d.age = (i16)v->age;
    d.language = (i16)(v->lang == SP_LANG_PT ? 8 : 0);   /* langPortuguese, langEnglish */
    d.region = (i16)(v->lang == SP_LANG_PT ? 71 : 0);    /* verBrazil, verUS */
    memcpy(out, &d, (usize)(len < (long)sizeof d ? len : (long)sizeof d));
    return 0;
}

/* ================================================================ registro */
#define A(s, f) cx_add_method(&NSSpeechSynthesizer_c, s, (void *)(f))
#define CA(s, f) cx_add_cmethod(&NSSpeechSynthesizer_c, s, (void *)(f))
void cx_speech_synth_init(void) {
    A("init", ss_init); A("initWithVoice:", ss_init_voice);
    A("startSpeakingString:", ss_start); A("startSpeakingString:toURL:", ss_start_url);
    A("stopSpeaking", ss_stop); A("stopSpeakingAtBoundary:", ss_stop_at); A("pauseSpeakingAtBoundary:", ss_pause_at);
    A("continueSpeaking", ss_continue); A("isSpeaking", ss_speaking); A("delegate", ss_delegate); A("setDelegate:", ss_set_delegate);
    A("voice", ss_voice); A("setVoice:", ss_set_voice); A("rate", ss_rate); A("setRate:", ss_set_rate);
    A("volume", ss_volume); A("setVolume:", ss_set_volume); A("usesFeedbackWindow", ss_feedback); A("setUsesFeedbackWindow:", ss_set_feedback);
    A("phonemesFromText:", ss_phonemes); A("objectForProperty:error:", ss_get_prop); A("setObject:forProperty:error:", ss_set_prop);
    CA("availableVoices", ss_voices); CA("defaultVoice", ss_default_voice); CA("attributesForVoice:", ss_attrs);
    CA("isAnyApplicationSpeaking", ss_any);
}
