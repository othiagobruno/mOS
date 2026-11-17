/* teste de host do speech_core (texto → fonemas → som)
 * gcc -std=c11 -Wall -Wextra -Iuserland/libsystem userland/libsystem/speech_core.c userland/tests-appkit/speech_host.c -lm -o /tmp/speech-test && /tmp/speech-test */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "speech_core.h"
static int ok, total;
static void check(int c, const char *m) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "FALHOU", m); }
static void phs(const char *text, int lang, char *buf, int cap) {
    static sp_ph ph[4096];
    int n = sp_text_to_phonemes(text, lang, ph, 4096);
    sp_phonemes_string(ph, n, buf, cap);
}
int main(int argc, char **argv) {
    char b[2048];
    phs("casa", SP_LANG_PT, b, sizeof b); printf("    casa: %s\n", b);
    check(strstr(b, "k 1a z @") != 0, "pt: casa = k 1a z @ (s entre vogais vira z, a final átono reduz)");
    phs("chave", SP_LANG_PT, b, sizeof b); printf("    chave: %s\n", b);
    check(strstr(b, "S 1a v i") != 0, "pt: chave = S 1a v i (ch, e final vira i)");
    phs("filho", SP_LANG_PT, b, sizeof b); printf("    filho: %s\n", b);
    check(strstr(b, "f 1i L u") != 0, "pt: filho = f 1i L u (lh)");
    phs("pão", SP_LANG_PT, b, sizeof b); printf("    pão: %s\n", b);
    check(strstr(b, "p 1a~ w~") != 0, "pt: pão = p 1a~ w~ (ão nasal)");
    phs("tarde", SP_LANG_PT, b, sizeof b); printf("    tarde: %s\n", b);
    check(strstr(b, "dZ i") != 0, "pt: tarde termina em dZ i (palatalização)");
    phs("café", SP_LANG_PT, b, sizeof b); printf("    café: %s\n", b);
    check(strstr(b, "k a f 1E") != 0, "pt: café é oxítona com é aberto");
    phs("carro rato", SP_LANG_PT, b, sizeof b); printf("    carro rato: %s\n", b);
    check(strstr(b, "k 1a x u") != 0 && strstr(b, "x 1a t u") != 0, "pt: rr e r inicial fortes");
    phs("123", SP_LANG_PT, b, sizeof b); printf("    123: %s\n", b);
    check(strstr(b, "s 1e~ t u i v 1i~ tS i i t r 1e s") != 0, "pt: 123 = cento e vinte e três (o e átono vira i)");
    phs("the cat", SP_LANG_EN, b, sizeof b); printf("    the cat: %s\n", b);
    check(strstr(b, "D @") != 0 && strstr(b, "k 1ae t") != 0, "en: the cat");
    phs("make time", SP_LANG_EN, b, sizeof b); printf("    make time: %s\n", b);
    check(strstr(b, "m 1e j k") != 0 && strstr(b, "t 1a j m") != 0, "en: e final mudo alonga a vogal");
    phs("Olá, mundo. Tudo bem?", SP_LANG_PT, b, sizeof b); printf("    frase: %s\n", b);
    check(sp_guess_lang("Olá, como vai você? Tudo bem com a família") == SP_LANG_PT && sp_guess_lang("Hello, how are you? This is the test") == SP_LANG_EN, "idioma provável");
    /* síntese */
    static sp_ph ph[4096];
    int n = sp_text_to_phonemes("Olá, mundo.", SP_LANG_PT, ph, 4096);
    sp_voice v = {120, 1.0, 0.2, SP_LANG_PT};
    static sp_synth s;
    sp_synth_init(&s, ph, n, &v, 1.0, 48000);
    static float buf[48000 * 10];
    int got = 0, k;
    while ((k = sp_synth_run(&s, buf + got, 1024)) > 0 && got < 48000 * 9) got += k;
    double secs = got / 48000.0, e = 0, peak = 0;
    for (int i = 0; i < got; i++) { e += buf[i] * buf[i]; if (fabs(buf[i]) > peak) peak = fabs(buf[i]); }
    double rms = sqrt(e / (got ? got : 1));
    printf("    %.2f s, rms %.3f, pico %.3f\n", secs, rms, peak);
    check(secs > 0.6 && secs < 3 && got == s.total_samples, "duração plausível e igual à soma dos fonemas");
    check(rms > 0.01 && peak <= 1.0, "som audível, sem estourar");
    /* velocidade dobrada: metade do tempo */
    sp_synth_init(&s, ph, n, &v, 2.0, 48000);
    check(s.total_samples < got * 0.55, "velocidade 2× fala em metade do tempo");
    /* espectro de um "a" sustentado: pico em F1 (~730 Hz) e F2 (~1250 Hz), vale entre F2 e F3 (~1900 Hz) */
    {
        static sp_ph va[3];
        int vn = sp_text_to_phonemes("á", SP_LANG_PT, va, 3);
        static sp_synth sa;
        static float vb[48000];
        sp_synth_init(&sa, va, vn, &v, 0.5, 48000);
        int gn = 0, kk;
        while ((kk = sp_synth_run(&sa, vb + gn, 1024)) > 0 && gn < 47000) gn += kk;
        int a0 = gn / 3, a1 = gn * 2 / 3;
        double pw[3];
        double fq[3] = {730, 1900, 1250};
        for (int q = 0; q < 3; q++) {
            double w = 2 * 3.141592653589793 * fq[q] / 48000, c = 2 * cos(w), s1 = 0, s2 = 0;
            for (int i = a0; i < a1; i++) { double s0 = vb[i] + c * s1 - s2; s2 = s1; s1 = s0; }
            pw[q] = s1 * s1 + s2 * s2 - c * s1 * s2;
        }
        printf("    \"a\": F1 %.1f, vale %.1f, F2 %.1f (dB rel.)\n", 10 * log10(pw[0] + 1e-12), 10 * log10(pw[1] + 1e-12), 10 * log10(pw[2] + 1e-12));
        check(pw[0] > pw[1] * 4 && pw[2] > pw[1] * 2, "vogal a: formantes acima do vale (espectro de vogal, não ruído)");
    }
    /* grava um WAV para ouvir, se pedido */
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        int bytes = got * 2;
        unsigned char h[44] = {'R','I','F','F'};
        int v32 = 36 + bytes; memcpy(h + 4, &v32, 4); memcpy(h + 8, "WAVEfmt ", 8);
        v32 = 16; memcpy(h + 16, &v32, 4); short s16 = 1; memcpy(h + 20, &s16, 2); memcpy(h + 22, &s16, 2);
        v32 = 48000; memcpy(h + 24, &v32, 4); v32 = 96000; memcpy(h + 28, &v32, 4); s16 = 2; memcpy(h + 32, &s16, 2); s16 = 16; memcpy(h + 34, &s16, 2);
        memcpy(h + 36, "data", 4); memcpy(h + 40, &bytes, 4);
        fwrite(h, 1, 44, f);
        for (int i = 0; i < got; i++) { short x = (short)(buf[i] * 32767); fwrite(&x, 2, 1, f); }
        fclose(f);
    }
    printf("%d/%d\n", ok, total);
    return ok == total ? 0 : 1;
}
