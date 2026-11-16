/* Host: userland/libsystem/openal.c, o mixer rodando offline (cx_openal_render). → "openal: N/N ok" */
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef unsigned ALuint; typedef int ALint;
void *alcOpenDevice(const char *); void *alcCreateContext(void *, const int *); char alcMakeContextCurrent(void *);
void alcDestroyContext(void *); char alcCloseDevice(void *); void alcGetIntegerv(void *, int, int, int *); const char *alcGetString(void *, int);
int alGetError(void); void alGenBuffers(int, ALuint *); void alBufferData(ALuint, int, const void *, int, int);
void alGenSources(int, ALuint *); void alSourcei(ALuint, int, int); void alSourcef(ALuint, int, float); void alSource3f(ALuint, int, float, float, float);
void alSourcePlay(ALuint); void alSourcePause(ALuint); void alSourceStop(ALuint); void alGetSourcei(ALuint, int, int *); void alGetSourcef(ALuint, int, float *);
void alSourceQueueBuffers(ALuint, int, const ALuint *); void alSourceUnqueueBuffers(ALuint, int, ALuint *);
void alListenerfv(int, const float *); void alDistanceModel(int); void alDeleteBuffers(int, const ALuint *);
void alGetBufferi(ALuint, int, int *);
void cx_openal_render(void *ctx, float *out, unsigned frames);
/* a engine de saída não entra no teste */
void *cx_cae_new(double r, unsigned c, void *fn, void *ctx) { (void)r; (void)c; (void)fn; (void)ctx; return 0; }
void *cx_cae_ctx(void *e) { return e; }
int cx_cae_start(void *e) { (void)e; return 0; }
void cx_cae_stop(void *e, int f) { (void)e; (void)f; }
void cx_cae_free(void *e) { (void)e; }
static int fails, total;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
static float out[2 * 48000];
int main(void) {
    void *dev = alcOpenDevice(0), *ctx = alcCreateContext(dev, 0);
    alcMakeContextCurrent(ctx);
    int maj = 0; alcGetIntegerv(dev, 0x1000, 1, &maj);
    expect(dev && ctx && maj == 1 && !strcmp(alcGetString(dev, 0x1005), "mOS"), "dispositivo e contexto");
    /* buffer mono 48 kHz, constante 0,5 (16 bits) */
    static short dc[4800]; for (int i = 0; i < 4800; i++) dc[i] = 16384;
    ALuint b, s; alGenBuffers(1, &b); alBufferData(b, 0x1101, dc, sizeof dc, 48000);
    int freq = 0, size = 0; alGetBufferi(b, 0x2001, &freq); alGetBufferi(b, 0x2004, &size);
    expect(freq == 48000 && size == (int)sizeof dc && alGetError() == 0, "alBufferData MONO16");
    alGenSources(1, &s); alSourcei(s, 0x1009, (int)b);
    alSource3f(s, 0x1004, 0, 0, -1);   /* à frente, distância 1 (= referência): ganho 1, centro */
    alSourcePlay(s);
    cx_openal_render(ctx, out, 1000);
    float c = (float)cos(M_PI / 4) * 0.5f;
    expect(fabsf(out[1000] - c) < 1e-4f && fabsf(out[1001] - c) < 1e-4f, "fonte à frente: centro, potência constante");
    alSource3f(s, 0x1004, 3, 0, 0);    /* à direita, distância 3: inverse clamped = 1 / (1 + 1*(3-1)) = 1/3 */
    cx_openal_render(ctx, out, 1000);
    expect(fabsf(out[1998]) < 1e-4f && fabsf(out[1999] - 0.5f / 3) < 1e-4f, "à direita e atenuada pela distância");
    alDistanceModel(0);   /* AL_NONE */
    cx_openal_render(ctx, out, 1000);
    expect(fabsf(out[1999] - 0.5f) < 1e-4f, "AL_NONE: sem atenuação");
    float pos; alGetSourcef(s, 0x1025, &pos);   /* AL_SAMPLE_OFFSET */
    expect(pos == 3000, "offset em amostras");
    cx_openal_render(ctx, out, 2000);   /* passa do fim (4800) */
    int st = 0; alGetSourcei(s, 0x1010, &st);
    expect(st == 0x1014, "para no fim do buffer (STOPPED)");
    /* loop */
    alSourcei(s, 0x1007, 1); alSourcePlay(s);
    cx_openal_render(ctx, out, 10000);
    alGetSourcei(s, 0x1010, &st);
    expect(st == 0x1012 && out[2 * 9999 + 1] != 0, "loop continua tocando");
    alSourceStop(s);
    /* pitch 2 com buffer de 24 kHz: anda 1 quadro do buffer por quadro de saída */
    ALuint b2, s2; alGenBuffers(1, &b2);
    static short ramp[1000]; for (int i = 0; i < 1000; i++) ramp[i] = (short)(i * 30);
    alBufferData(b2, 0x1101, ramp, sizeof ramp, 24000);
    alGenSources(1, &s2); alSourcei(s2, 0x1009, (int)b2); alSourcef(s2, 0x1003, 2.0f); alSourcei(s2, 0x202, 1);   /* relativo, na origem */
    alSourcePlay(s2);
    cx_openal_render(ctx, out, 500);
    float k = (float)cos(M_PI / 4);
    expect(fabsf(out[2 * 100] - k * 100 * 30 / 32768.0f) < 1e-4f && fabsf(out[2 * 499] - k * 499 * 30 / 32768.0f) < 1e-4f, "pitch e reamostragem (24 kHz a pitch 2)");
    /* fila de streaming: 3 buffers, 2 processados, desenfileira */
    ALuint q[3], s3; alGenBuffers(3, q);
    for (int i = 0; i < 3; i++) alBufferData(q[i], 0x1101, dc, 2000, 48000);   /* 1000 quadros cada */
    alGenSources(1, &s3); alSourceQueueBuffers(s3, 3, q); alSourcePlay(s3);
    cx_openal_render(ctx, out, 2500);
    int proc = 0, queued = 0; alGetSourcei(s3, 0x1016, &proc); alGetSourcei(s3, 0x1015, &queued);
    ALuint got[2]; alSourceUnqueueBuffers(s3, 2, got);
    int q2 = 0; alGetSourcei(s3, 0x1015, &q2);
    expect(proc == 2 && queued == 3 && got[0] == q[0] && got[1] == q[1] && q2 == 1 && alGetError() == 0, "fila: processados e desenfileirar");
    alDeleteBuffers(1, &q[2]);
    expect(alGetError() == 0xA004, "não apaga buffer ainda na fila");
    alcMakeContextCurrent(0); alcDestroyContext(ctx); alcCloseDevice(dev);
    printf("openal: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
