/* mOS — síntese de fala original, sem libc (NSSpeechSynthesizer e Speech Manager em speech_synth.c).
 * Texto → fonemas por regras (português do Brasil e inglês), com números por extenso, tonicidade e pausas da
 * pontuação; fonemas → som por um sintetizador de formantes em cascata (fonte glotal, ruído de fricção, explosão das
 * plosivas, nasalização), em blocos, para tocar enquanto gera. Teste de host: userland/tests-appkit/speech_host.c. */
#ifndef SPEECH_CORE_H
#define SPEECH_CORE_H

enum { SP_LANG_PT = 0, SP_LANG_EN = 1 };

/* fonema com a palavra de onde veio (posição em bytes no texto) e o acento */
typedef struct { unsigned char ph, stress, nasal, pause; int word_loc, word_len; } sp_ph;

/* texto UTF-8 → fonemas; devolve quantos (até cap) */
int sp_text_to_phonemes(const char *text, int lang, sp_ph *out, int cap);
/* fonemas → texto com os símbolos do mOS (phonemesFromText:) */
int sp_phonemes_string(const sp_ph *ph, int n, char *out, int cap);
/* idioma provável de um texto sem voz escolhida */
int sp_guess_lang(const char *text);

typedef struct { double f0, formant_scale, breath; int lang; } sp_voice;
typedef struct {
    const sp_ph *ph; int n;
    sp_voice v;
    double rate;          /* 1 = normal */
    double rate_hz;       /* taxa de amostragem */
    double pitch, mod;    /* altura base (Hz, 0 = da voz) e modulação (0..1) */
    /* estado */
    int cur, sample_in_ph, ph_len;
    double phase, f[5], bw[5], y1[6], y2[6], amp, noise_amp, noise_f, prev_g, lp;
    unsigned noise;
    int total_samples, done_samples;
} sp_synth;
void sp_synth_init(sp_synth *s, const sp_ph *ph, int n, const sp_voice *v, double rate, double rate_hz);
/* gera até nsamp amostras mono (-1..1); 0 quando acabou */
int sp_synth_run(sp_synth *s, float *out, int nsamp);
/* duração de um fonema em amostras (com a velocidade da fala) */
int sp_ph_samples(const sp_synth *s, int i);
#endif
