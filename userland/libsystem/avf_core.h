#ifndef MOS_AVF_CORE_H
#define MOS_AVF_CORE_H
/* Núcleo de AVFoundation sem Objective-C. As classes em avfaudio.c e avcapture.c
 * chamam estas funções. O teste de host (tools/avf_core_test.c) cobre o mesmo código. */
#include <stdint.h>
typedef struct { double duration, rate; unsigned channels; int media; } avf_track_t; /* media 1 = áudio */
int avf_describe_audio(unsigned long long frames, double rate, unsigned channels, avf_track_t *out);
typedef struct { double *pos; double src_rate; unsigned long long total; float play_rate, volume; } avf_clock_t;
unsigned avf_clock_render(avf_clock_t *c, unsigned (*read)(void *ctx, unsigned long long at, unsigned n, float *out), void *ctx, float *stereo, unsigned frames, int *ended);
void avf_mix_scale(float *stereo, unsigned frames, float volume, float pan);
typedef void (*avf_tap_cb)(const float *interleaved, unsigned frames, void *ctx);
void avf_tap_push(float **mem, unsigned *len, unsigned cap, const float *in, unsigned frames, avf_tap_cb cb, void *ctx);
double avf_clamp_io(double seconds);
unsigned avf_record_permission(int tcc); /* 'undt' / 'grnt' / 'deny' */
int avf_mic_pull(float *dst, unsigned frames); /* 0 ok (silêncio se o anel está vazio); -54 negado; -66680 sem TCC */
unsigned avf_copy_capture(short *dst, unsigned frames, int *err);
int avf_device_kinds(int *out, int cap); /* 1 = microfone embutido. Câmera não entra: não há driver. */
int avf_cap_start(int has_mic, int has_cam, int *err); /* 1 corre. err 1 = TCC, 2 = sem dispositivo de quadro */
unsigned avf_cap_pull(int running, short *dst, unsigned frames, int *err);
#endif
