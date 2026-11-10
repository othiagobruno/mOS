/* Lógica do AVKit que AVPlayerViewController, AVPictureInPictureController e
 * AVRoutePickerView chamam. O WAV é gerado aqui.
 * gcc -std=c11 -O1 -I userland/libsystem -I userland/host-audio/shim -c userland/host-audio/audiodec.c -o /tmp/audiodec_host.o
 * gcc -std=c11 -O1 -I userland/libsystem -o /tmp/avkit_host_test tools/avkit_host_test.c \
 *   userland/libsystem/avf_core.c userland/libsystem/audioread.c /tmp/audiodec_host.o -lm
 */
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
        size_t r = fread(b + n, 1, (size_t)(cap - n), f);
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
extern unsigned long long cx_audio_frames(void *d);
static int fail;
static void expect(int ok, const char *m) { if (!ok) { fprintf(stderr, "FALHOU %s\n", m); fail++; } }
static void put16(unsigned char *p, int v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void put32(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
int main(void) {
    float rate = 0;
    double pos = 0;
    avk_transport_t t = {&rate, &pos, 1.0, 1};
    expect(avk_play(&t) == 1 && rate == 1, "play");
    expect(avk_pause(&t) == 1 && rate == 0, "pause");
    expect(avk_seek(&t, 0.25) == 1 && pos == 0.25, "seek");
    expect(avk_seek(&t, 5) == 1 && pos == 1.0, "seek no fim");
    expect(avk_seek(&t, -1) == 1 && pos == 0, "seek antes do inicio");
    t.controls = 0;
    rate = 0; pos = 0.2;
    expect(avk_play(&t) == 0 && rate == 0, "sem controles nao da play");
    expect(avk_seek(&t, 0.5) == 0 && pos == 0.2, "sem controles nao da seek");
    expect(avk_pip_supported() == 0, "pip nao suportado");
    int err = 0;
    expect(avk_pip_start(&err) == 0 && err != 0, "pip start falha");
    expect(avk_route_count() == 1, "uma rota");
    expect(avk_route_port(0) && !strcmp(avk_route_port(0), "Speaker"), "porta Speaker");
    expect(avk_route_name(0) && !strcmp(avk_route_name(0), "Alto-falantes"), "nome do alto-falante");
    expect(avk_route_port(1) == 0 && avk_route_name(1) == 0, "sem segunda rota");

    const char *path = "/tmp/mos_avkit_tiny.wav";
    unsigned char b[44 + 8];
    memcpy(b, "RIFF", 4); put32(b + 4, 36 + 8); memcpy(b + 8, "WAVE", 4);
    memcpy(b + 12, "fmt ", 4); put32(b + 16, 16); put16(b + 20, 1); put16(b + 22, 1);
    put32(b + 24, 8000); put32(b + 28, 16000); put16(b + 32, 2); put16(b + 34, 16);
    memcpy(b + 36, "data", 4); put32(b + 40, 8);
    put16(b + 44, 16000); put16(b + 46, 0); put16(b + 48, -16000); put16(b + 50, 8000);
    FILE *f = fopen(path, "wb");
    expect(f && fwrite(b, 1, sizeof b, f) == sizeof b, "wav");
    if (f) fclose(f);
    int oerr = 0;
    void *dec = cx_audio_open_path(path, &oerr);
    expect(dec != 0, "abre pelo cx_audio_open_path");
    if (dec) {
        rate = 0; pos = 0; t.controls = 1; t.duration = (double)cx_audio_frames(dec) / cx_audio_rate(dec);
        expect(avk_play(&t) == 1, "play no arquivo");
        float out[8];
        int ended = 0;
        unsigned k = avf_pull_seconds(dec, &pos, rate, 1, out, 2, &ended);
        expect(k == 2 && !ended, "render com o rate do controle");
        expect(fabsf(out[0] - 16000.0f / 32768.0f) < 1e-5f, "pcm do controle");
        expect(avk_pause(&t) == 1, "pause no arquivo");
        double held = pos;
        k = avf_pull_seconds(dec, &pos, rate, 1, out, 2, &ended);
        expect(k == 0 && pos == held, "pause segura o relogio");
        expect(avk_seek(&t, 0) == 1 && pos == 0, "seek de volta");
        cx_audio_close(dec);
    }
    remove(path);
    if (fail) { fprintf(stderr, "%d falhas\n", fail); return 1; }
    printf("PASSOU\n");
    return 0;
}
