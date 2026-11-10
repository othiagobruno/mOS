#ifndef MOS_AVF_CORE_H
#define MOS_AVF_CORE_H
/* Núcleo de AVFoundation sem Objective-C. As classes em avfaudio.c e avcapture.c
 * chamam estas funções. O teste de host (tools/avf_core_test.c) cobre o mesmo código. */
#include <stddef.h>
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
/* O mesmo passo de AVPlayer: cx_audio_read_stereo + relógio. pos em segundos. rate 0 não anda. */
unsigned avf_pull_seconds(void *dec, double *pos, float play_rate, float volume, float *stereo, unsigned frames, int *ended);
/* O mesmo passo de AVAudioPlayer: posição em quadros, ganho volume/pan. */
unsigned avf_pull_frames(void *dec, double *pos_frames, float volume, float pan, float *stereo, unsigned frames, int *ended);
/* Observador periódico no tempo do arquivo. 1 quando o intervalo de mídia passou. */
int avf_obs_due(double *last, double interval, double pos, float play_rate);
/* Transporte do AVPlayerViewController. controls 0 recusa play/pause/seek. */
typedef struct { float *rate; double *pos; double duration; int controls; } avk_transport_t;
int avk_play(avk_transport_t *t);
int avk_pause(avk_transport_t *t);
int avk_seek(avk_transport_t *t, double sec);
int avk_pip_supported(void); /* 0: o mOS não tem janela flutuante de vídeo */
int avk_pip_start(int *err); /* 0 e *err != 0: o start falha e o delegate recebe esse código */
int avk_route_count(void);
const char *avk_route_port(int index); /* "Speaker", a mesma porta da AVAudioSession */
const char *avk_route_name(int index);
/* Leitor de faixa. media 1 = áudio. Vídeo (2), texto (3) e capítulo (4) falham: não há decodificador. */
enum { AVF_READ_UNKNOWN = 0, AVF_READ_READING = 1, AVF_READ_COMPLETED = 2, AVF_READ_FAILED = 3, AVF_READ_CANCELLED = 4 };
typedef struct {
    int status, err, media;
    const float *pcm;
    unsigned ch;
    unsigned long long frames, at;
} avf_read_t;
int avf_read_begin(avf_read_t *r, int media, const float *pcm, unsigned ch, unsigned long long frames);
unsigned avf_read_copy(avf_read_t *r, float *dst, unsigned frames);
void avf_read_cancel(avf_read_t *r);
/* Escritor. Só WAV PCM 16 bits. Outro tipo de arquivo falha com -11822. */
enum { AVF_WRITE_UNKNOWN = 0, AVF_WRITE_WRITING = 1, AVF_WRITE_COMPLETED = 2, AVF_WRITE_FAILED = 3, AVF_WRITE_CANCELLED = 4 };
typedef struct {
    int status, err;
    unsigned rate, ch;
    unsigned char *pcm;
    size_t n, cap;
} avf_write_t;
int avf_write_begin(avf_write_t *w, const char *filetype, unsigned rate, unsigned ch);
unsigned avf_write_append(avf_write_t *w, const short *pcm, unsigned frames);
int avf_write_finish(avf_write_t *w, const char *path);
void avf_write_cancel(avf_write_t *w);
#endif
