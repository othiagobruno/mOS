/* mOS — teste de host de kernel/apps/audiostream.c (MP3 progressivo dos apps do sistema), contra o ffmpeg:
 *   ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=7" -ac 2 -b:a 128k /tmp/cbr.mp3
 *   ffmpeg -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=7" -ac 2 -q:a 4 /tmp/vbr.mp3
 *   cc -O2 -std=gnu11 -w tools/audiostream_host.c -lm -o /tmp/ash
 *   /tmp/ash /tmp/cbr.mp3 <quadros 48 kHz do ffmpeg> /tmp/vbr.mp3 <quadros 48 kHz do ffmpeg>
 * Confere: a duração estimada bate com a decodificada, decodificar aos poucos dá o mesmo PCM que de uma vez,
 * o número de quadros fica a menos de 1 quadro MP3 do ffmpeg, e o sinal tem 440 Hz. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t i8; typedef int16_t i16; typedef int32_t i32; typedef int64_t i64; typedef size_t usize;
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
static void *kmalloc(usize n) { return malloc(n); }
static void *kcalloc(usize a, usize b) { return calloc(a, b); }
static void *krealloc(void *p, usize n) { return realloc(p, n); }
static void kfree(void *p) { free(p); }
static u64 fake_ns;
static u64 hpet_ns(void) { return fake_ns += 1000; }   /* cada leitura avança 1 µs: o orçamento vira poucos quadros */
static u64 timer_ticks(void) { return 0; }
#define AS_HOST
#include "../kernel/apps/audiostream.c"

static int fails;
#define CHECK(c, ...) do { if (c) { printf("ok   "); printf(__VA_ARGS__); printf("\n"); } else { fails++; printf("FALHA "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static u8 *slurp(const char *p, u64 *n) {
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); *n = (u64)ftell(f); fseek(f, 0, SEEK_SET);
    u8 *b = malloc(*n);
    if (fread(b, 1, *n, f) != *n) { fclose(f); free(b); return NULL; }
    fclose(f);
    return b;
}

static void one(const char *path, long ff_frames) {
    u64 n;
    u8 *d = slurp(path, &n);
    if (!d) { fails++; printf("FALHA não li %s\n", path); return; }
    CHECK(kit_mp3_probe(d, n), "%s: reconhecido como MP3", path);
    u32 est = kit_mp3_estimate_frames(d, n);
    /* de uma vez */
    kit_stream_t *a = kit_stream_open_mem(d, n);
    while (!kit_stream_step(a, 1000000000u)) {}
    /* aos poucos: orçamento de 3 µs por passo (2-3 quadros) */
    kit_stream_t *b = kit_stream_open_mem(d, n);
    int steps = 0;
    while (!kit_stream_step(b, 3)) steps++;
    u32 fa = kit_stream_frames(a), fb = kit_stream_frames(b);
    CHECK(fa == fb && !memcmp(kit_stream_pcm(a), kit_stream_pcm(b), (usize)fa * 4), "%s: %d passos dão o mesmo PCM que de uma vez (%u quadros)", path, steps, fa);
    long diff = (long)fa - ff_frames;
    CHECK(labs(diff) <= 2, "%s: %u quadros contra %ld do ffmpeg (diferença %ld; gapless igual ao do ffmpeg)", path, fa, ff_frames, diff);
    double rel = est ? fabs((double)est - fa) / fa : 1;
    CHECK(rel < 0.0001, "%s: duração estimada %u contra decodificada %u (%.2f%%)", path, est, fa, rel * 100);
    /* 440 Hz: cruzamentos de zero no canal esquerdo, no meio do sinal */
    const i16 *p = kit_stream_pcm(a);
    u32 z = 0, s0 = fa / 4, s1 = fa * 3 / 4;
    for (u32 i = s0 + 1; i < s1; i++) if ((p[(i - 1) * 2] < 0) != (p[i * 2] < 0)) z++;
    double hz = z / 2.0 / ((s1 - s0) / 48000.0);
    CHECK(fabs(hz - 440) < 5, "%s: frequência %.1f Hz (esperado 440)", path, hz);
    kit_stream_close(a); kit_stream_close(b);
    free(d);
}

int main(int argc, char **argv) {
    for (int i = 1; i + 1 < argc; i += 2) one(argv[i], atol(argv[i + 1]));
    u8 junk[4096] = {0};
    CHECK(!kit_mp3_probe(junk, sizeof junk) && !kit_stream_open_mem(junk, sizeof junk), "zeros não viram MP3");
    printf("audiostream: %s\n", fails ? "FALHOU" : "ok");
    return fails != 0;
}
