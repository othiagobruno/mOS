/* WAV PCM pequeno, gerado aqui (não entra no git).
 * gcc -std=c11 -O1 -I userland/libsystem -I userland/host-audio/shim -c userland/host-audio/audiodec.c -o /tmp/audiodec_host.o
 * gcc -std=c11 -O1 -I userland/libsystem -o /tmp/avplayer_wav_test tools/avplayer_wav_test.c \
 *   userland/libsystem/avf_core.c userland/libsystem/audioread.c /tmp/audiodec_host.o -lm
 * Cobre cx_audio_open_path, a faixa (avf_describe_audio, a mesma de AVURLAsset -tracks),
 * avf_pull_seconds (AVPlayer), avf_pull_frames (AVAudioPlayer) e avf_obs_due. */
#include "avf_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef unsigned char u8;
typedef unsigned long long u64;
u8 *cx_read_file(const char *path, u64 *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    u64 cap = 4096, n = 0;
    u8 *b = malloc(cap + 1);
    for (;;) {
        if (n == cap) { cap *= 2; u8 *g = realloc(b, cap + 1); if (!g) { free(b); fclose(f); return 0; } b = g; }
        size_t r = fread(b + n, 1, cap - n, f);
        if (!r) break;
        n += r;
    }
    fclose(f);
    b[n] = 0;
    if (len) *len = n;
    return b;
}
extern void *cx_audio_open_path(const char *path, int *err);
extern void cx_audio_close(void *d);
extern double cx_audio_rate(void *d);
extern unsigned cx_audio_channels(void *d);
extern unsigned long long cx_audio_frames(void *d);

static int fail;
static void expect(int ok, const char *m) { if (!ok) { fprintf(stderr, "FALHOU %s\n", m); fail++; } }

static void put16(unsigned char *p, int v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void put32(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
enum { RATE = 8000, N = 32 };
static short L[N], R[N];
static int write_wav(const char *path) {
    unsigned char b[44 + N * 4];
    memcpy(b, "RIFF", 4);
    put32(b + 4, 36 + N * 4);
    memcpy(b + 8, "WAVE", 4);
    memcpy(b + 12, "fmt ", 4);
    put32(b + 16, 16);
    put16(b + 20, 1);
    put16(b + 22, 2);
    put32(b + 24, RATE);
    put32(b + 28, RATE * 4);
    put16(b + 32, 4);
    put16(b + 34, 16);
    memcpy(b + 36, "data", 4);
    put32(b + 40, N * 4);
    for (int i = 0; i < N; i++) {
        L[i] = (short)(i * 100);
        R[i] = (short)(-i * 50);
        put16(b + 44 + i * 4, L[i]);
        put16(b + 46 + i * 4, R[i]);
    }
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(b, 1, sizeof b, f) == sizeof b;
    fclose(f);
    return ok;
}
static int near(float a, float b) { return fabsf(a - b) < 1e-5f; }

int main(void) {
    const char *path = "/tmp/mos_avplayer_tiny.wav";
    expect(write_wav(path), "grava wav");
    int err = 0;
    void *dec = cx_audio_open_path(path, &err);
    expect(dec && err == 0, "cx_audio_open_path");
    if (!dec) return 1;
    avf_track_t tr;
    expect(avf_describe_audio(cx_audio_frames(dec), cx_audio_rate(dec), cx_audio_channels(dec), &tr), "faixa");
    expect(tr.media == 1 && tr.channels == 2 && tr.rate == RATE, "faixa audio 8 kHz");
    expect(fabs(tr.duration - (double)N / RATE) < 1e-9, "duration");
    expect(fabs((double)cx_audio_frames(dec) / cx_audio_rate(dec) - tr.duration) < 1e-9, "duration do asset");

    double pos = 0, last = 0;
    float out[16];
    int ended = 0;
    unsigned k = avf_pull_seconds(dec, &pos, 1, 1, out, 8, &ended);
    expect(k == 8 && !ended, "play 8");
    expect(near(out[0], L[0] / 32768.0f) && near(out[1], R[0] / 32768.0f), "pcm quadro 0");
    expect(near(out[14], L[7] / 32768.0f) && near(out[15], R[7] / 32768.0f), "pcm quadro 7");
    expect(avf_obs_due(&last, 8.0 / RATE, pos, 1) == 1, "observador no 8o quadro");
    double held = pos;
    k = avf_pull_seconds(dec, &pos, 0, 1, out, 8, &ended);
    expect(k == 0 && pos == held && !ended, "pause");
    expect(avf_obs_due(&last, 8.0 / RATE, pos, 0) == 0, "observador parado no pause");
    pos = 20.0 / RATE;
    k = avf_pull_seconds(dec, &pos, 1, 1, out, 8, &ended);
    expect(k == 8 && !ended, "seek + play");
    expect(near(out[0], L[20] / 32768.0f) && near(out[1], R[20] / 32768.0f), "pcm depois do seek");
    expect(avf_obs_due(&last, 8.0 / RATE, pos, 1) == 1, "observador de novo");
    k = avf_pull_seconds(dec, &pos, 1, 1, out, 8, &ended);
    expect(k == 4 && ended, "fim da faixa");
    expect(near(out[6], L[31] / 32768.0f) && near(out[7], R[31] / 32768.0f), "ultimo quadro");
    expect(out[8] == 0 && out[9] == 0, "silencio depois do fim");

    double fr = 0;
    ended = 0;
    k = avf_pull_frames(dec, &fr, 0.5f, 0, out, 4, &ended);
    expect(k == 4 && !ended, "AVAudioPlayer le");
    expect(near(out[2], 0.5f * (L[1] / 32768.0f)) && near(out[3], 0.5f * (R[1] / 32768.0f)), "volume 0.5");
    fr = N;
    k = avf_pull_frames(dec, &fr, 1, 0, out, 2, &ended);
    expect(k == 0 && ended, "AVAudioPlayer no fim");
    cx_audio_close(dec);
    remove(path);
    if (fail) { fprintf(stderr, "%d falhas\n", fail); return 1; }
    printf("PASSOU\n");
    return 0;
}
