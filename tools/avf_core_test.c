/* Comportamento de AVAudioEngine (mixer, tap, entrada), AVAudioSession (permissão),
 * AVCaptureSession (TCC, sem câmera inventada) e o relógio do AVPlayer.
 * As classes ObjC chamam userland/libsystem/avf_core.c. Este teste chama as mesmas funções.
 *   gcc -std=c11 -O1 -I userland/libsystem -I userland/host-audio/shim -c userland/host-audio/audiodec.c -o /tmp/audiodec_host.o
 *   gcc -std=c11 -O1 -I userland/libsystem -o /tmp/avf_core_test \
 *     tools/avf_core_test.c userland/libsystem/avf_core.c userland/libsystem/audioread.c /tmp/audiodec_host.o -lm
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../userland/libsystem/avf_core.h"
static int g_fail;
static void expect_i(const char *w, int got, int exp) {
    if (got != exp) { printf("FALHA %s: obtido %d esperado %d\n", w, got, exp); g_fail++; }
    else printf("ok %s = %d\n", w, got);
}
static int g_tcc_mic = 1, g_tcc_cam = 0;
int cx_tcc_request(const char *service) {
    if (!service) return 0;
    if (!strcmp(service, "kTCCServiceMicrophone")) return g_tcc_mic;
    if (!strcmp(service, "kTCCServiceCamera")) return g_tcc_cam;
    return 0;
}
static short g_mic[8];
static unsigned g_mic_n;
unsigned cx_capture_i16(short *dst, unsigned frames) {
    unsigned n = g_mic_n < frames ? g_mic_n : frames;
    if (n && dst) memcpy(dst, g_mic, (size_t)n * 4);
    return n;
}
static unsigned read_ramp(void *ctx, unsigned long long at, unsigned n, float *out) {
    float *pcm = ctx;
    for (unsigned i = 0; i < n; i++) { out[i * 2] = pcm[(at + i) * 2]; out[i * 2 + 1] = pcm[(at + i) * 2 + 1]; }
    return n;
}
static int g_tap_n;
static float g_tap_first;
static void on_tap(const float *interleaved, unsigned frames, void *ctx) {
    (void)ctx;
    g_tap_n += (int)frames;
    g_tap_first = interleaved[0];
}
int main(void) {
    avf_track_t tr;
    expect_i("describe", avf_describe_audio(48000, 48000, 2, &tr), 1);
    if (tr.duration < 0.999 || tr.duration > 1.001 || tr.media != 1) { printf("FALHA duracao %f\n", tr.duration); g_fail++; }
    else printf("ok duracao 1s\n");
    expect_i("describe vazio", avf_describe_audio(0, 48000, 2, &tr), 0);
    float pcm[8] = {0.5f, -0.25f, 0.25f, 0.125f, 1, 1, 1, 1};
    double pos = 0;
    int ended = 0;
    avf_clock_t ck = {&pos, 48000, 4, 1, 0.5f};
    float out[8];
    unsigned k = avf_clock_render(&ck, read_ramp, pcm, out, 2, &ended);
    expect_i("clock quadros", (int)k, 2);
    expect_i("clock fim", ended, 0);
    if (out[0] < 0.24f || out[0] > 0.26f || out[1] > -0.12f || out[1] < -0.13f) { printf("FALHA ganho %f %f\n", out[0], out[1]); g_fail++; }
    else printf("ok ganho do player\n");
    ck.play_rate = 0;
    double held = pos;
    expect_i("pause", (int)avf_clock_render(&ck, read_ramp, pcm, out, 2, &ended), 0);
    if (pos != held) { printf("FALHA pause andou\n"); g_fail++; } else printf("ok pause\n");
    pos = 3.0 / 48000.0;
    ck.play_rate = 1;
    ck.volume = 1;
    k = avf_clock_render(&ck, read_ramp, pcm, out, 4, &ended);
    expect_i("seek resto", (int)k, 1);
    expect_i("ended", ended, 1);
    if (out[0] < 0.99f) { printf("FALHA seek %f\n", out[0]); g_fail++; } else printf("ok seek\n");
    float mix[4] = {1, 1, 1, 1};
    avf_mix_scale(mix, 2, 0.5f, 1);
    if (mix[0] != 0 || mix[1] < 0.49f || mix[1] > 0.51f) { printf("FALHA pan %f %f\n", mix[0], mix[1]); g_fail++; }
    else printf("ok mixer pan\n");
    float *mem = 0; unsigned len = 0; float block[8] = {0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f};
    avf_tap_push(&mem, &len, 2, block, 3, on_tap, 0);
    expect_i("tap blocos", g_tap_n, 2);
    if (g_tap_first < 0.19f || g_tap_first > 0.21f || len != 1) { printf("FALHA tap %f len %u\n", g_tap_first, len); g_fail++; }
    else printf("ok tap\n");
    free(mem);
    if (avf_clamp_io(0.0001) != 0.002 || avf_clamp_io(1) != 0.1) { printf("FALHA io buffer\n"); g_fail++; }
    else printf("ok io buffer\n");
    expect_i("perm grnt", (int)avf_record_permission(1), (int)0x67726E74);
    expect_i("perm deny", (int)avf_record_permission(0), (int)0x64656E79);
    expect_i("perm undt", (int)avf_record_permission(-1), (int)0x756E6474);
    g_mic[0] = 16384; g_mic[1] = -16384; g_mic_n = 1;
    float mic[4];
    expect_i("mic ok", avf_mic_pull(mic, 2), 0);
    if (mic[0] < 0.49f || mic[0] > 0.51f || mic[1] > -0.49f || mic[1] < -0.51f || mic[2] != 0) { printf("FALHA mic %f %f %f\n", mic[0], mic[1], mic[2]); g_fail++; }
    else printf("ok entrada\n");
    g_tcc_mic = 0;
    expect_i("mic negado", avf_mic_pull(mic, 2), -54);
    if (mic[0] != 0) { printf("FALHA mic vazou\n"); g_fail++; } else printf("ok mic sem permissao\n");
    int kinds[4];
    expect_i("so mic", avf_device_kinds(kinds, 4), 1);
    expect_i("kind mic", kinds[0], 1);
    int err = 0;
    g_tcc_mic = 1;
    expect_i("cap mic", avf_cap_start(1, 0, &err), 1);
    expect_i("cap err", err, 0);
    g_mic_n = 1;
    short got[4];
    unsigned cn = avf_cap_pull(1, got, 2, &err);
    expect_i("cap amostras", (int)cn, 1);
    expect_i("cap valor", got[0], 16384);
    expect_i("cap cam", avf_cap_start(1, 1, &err), 0);
    expect_i("cap cam err", err, 2);
    expect_i("cap sem input", avf_cap_start(0, 0, &err), 0);
    g_tcc_mic = 0;
    expect_i("cap tcc", avf_cap_start(1, 0, &err), 0);
    expect_i("cap tcc err", err, 1);
    float tone[8] = {0.5f, -0.5f, 0.25f, -0.25f, 0, 0, 1, -1};
    avf_read_t rd;
    expect_i("read video", avf_read_begin(&rd, 2, tone, 2, 4), 0);
    expect_i("read video err", rd.err, -11821);
    expect_i("read texto", avf_read_begin(&rd, 3, tone, 2, 4), 0);
    expect_i("read capitulo", avf_read_begin(&rd, 4, tone, 2, 4), 0);
    expect_i("read audio", avf_read_begin(&rd, 1, tone, 2, 4), 1);
    float gotf[4];
    expect_i("read copy", (int)avf_read_copy(&rd, gotf, 2), 2);
    expect_i("read resto", (int)avf_read_copy(&rd, gotf, 4), 2);
    expect_i("read fim", rd.status, AVF_READ_COMPLETED);
    if (gotf[0] != 0 || gotf[1] != 0) { printf("FALHA amostra final\n"); g_fail++; }
    avf_write_t wr;
    expect_i("write mp4", avf_write_begin(&wr, "public.mpeg-4", 8000, 1), 0);
    expect_i("write mp4 err", wr.err, -11822);
    expect_i("write wav", avf_write_begin(&wr, "public.wav", 8000, 1), 1);
    short samples[4] = {1000, -1000, 2000, -2000};
    expect_i("write append", (int)avf_write_append(&wr, samples, 4), 4);
    expect_i("write finish", avf_write_finish(&wr, "/tmp/avf-wav-host.wav"), 1);
    FILE *wf = fopen("/tmp/avf-wav-host.wav", "rb");
    unsigned char wh[52];
    size_t wn = wf ? fread(wh, 1, 52, wf) : 0;
    if (wf) fclose(wf);
    expect_i("wav bytes", (int)wn, 44 + 8);
    expect_i("wav riff", wh[0] == 'R' && wh[8] == 'W' && wh[20] == 1 && wh[22] == 1, 1);
    cn = avf_cap_pull(0, got, 2, &err);
    expect_i("pull parado", (int)cn, 0);
    if (g_fail) { printf("FALHOU %d\n", g_fail); return 1; }
    printf("PASSOU\n");
    return 0;
}
