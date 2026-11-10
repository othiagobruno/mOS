#include "avf_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0
/* o stdio.h do kshim (build freestanding do libSystem) só traz printf/snprintf; o libSystem exporta estes */
void *fopen(const char *path, const char *mode);
unsigned long fwrite(const void *p, unsigned long size, unsigned long n, void *f);
int fclose(void *f);
#endif
/* cx_media_tcc está em audiodec.c. A leitura do anel é do CoreAudio (kernel DEV_AUDIO_READ).
 * No host o teste define cx_capture_i16. Sem o símbolo, o anel está vazio: silêncio, não um tom. */
extern int cx_media_tcc(const char *service);
extern unsigned cx_capture_i16(short *dst, unsigned frames) __attribute__((weak));
int avf_describe_audio(unsigned long long frames, double rate, unsigned channels, avf_track_t *out) {
    if (!out || rate <= 0 || channels == 0 || frames == 0) return 0;
    out->duration = (double)frames / rate;
    out->rate = rate;
    out->channels = channels;
    out->media = 1;
    return 1;
}
unsigned avf_clock_render(avf_clock_t *c, unsigned (*read)(void *, unsigned long long, unsigned, float *), void *ctx, float *stereo, unsigned frames, int *ended) {
    if (ended) *ended = 0;
    if (!c || !c->pos || !read || !stereo || !frames || c->play_rate == 0 || c->src_rate <= 0) return 0;
    unsigned long long at = (unsigned long long)(*c->pos * c->src_rate);
    if (at >= c->total) { if (ended) *ended = 1; return 0; }
    unsigned n = frames;
    if (at + n > c->total) n = (unsigned)(c->total - at);
    unsigned k = read(ctx, at, n, stereo);
    if (k > n) k = n;
    float g = c->volume;
    for (unsigned i = 0; i < k * 2; i++) stereo[i] *= g;
    for (unsigned i = k; i < frames; i++) stereo[i * 2] = stereo[i * 2 + 1] = 0;
    *c->pos += (double)k / c->src_rate;
    if (at + k >= c->total && ended) *ended = 1;
    return k;
}
void avf_mix_scale(float *stereo, unsigned frames, float volume, float pan) {
    if (!stereo) return;
    if (pan > 1) pan = 1;
    if (pan < -1) pan = -1;
    if (volume < 0) volume = 0;
    float vl = volume * (pan > 0 ? 1 - pan : 1);
    float vr = volume * (pan < 0 ? 1 + pan : 1);
    for (unsigned i = 0; i < frames; i++) { stereo[i * 2] *= vl; stereo[i * 2 + 1] *= vr; }
}
void avf_tap_push(float **mem, unsigned *len, unsigned cap, const float *in, unsigned frames, avf_tap_cb cb, void *ctx) {
    if (!mem || !len || !cap || !in) return;
    if (!*mem) { *mem = (float *)calloc(cap, sizeof(float) * 2); *len = 0; if (!*mem) return; }
    unsigned left = frames;
    const float *p = in;
    while (left) {
        unsigned room = cap - *len;
        unsigned n = left < room ? left : room;
        memcpy(*mem + (size_t)*len * 2, p, (size_t)n * 2 * sizeof(float));
        *len += n;
        p += n * 2;
        left -= n;
        if (*len >= cap) {
            if (cb) cb(*mem, cap, ctx);
            *len = 0;
        }
    }
}
double avf_clamp_io(double seconds) { return seconds < 0.002 ? 0.002 : seconds > 0.1 ? 0.1 : seconds; }
unsigned avf_record_permission(int tcc) {
    if (tcc < 0) return 0x756E6474u;          /* 'undt' */
    return tcc == 1 ? 0x67726E74u : 0x64656E79u; /* 'grnt' / 'deny' */
}
static int mic_tcc(void) { return cx_media_tcc("kTCCServiceMicrophone"); }
int avf_mic_pull(float *dst, unsigned frames) {
    int tcc = mic_tcc();
    if (tcc != 1) {
        if (dst && frames) memset(dst, 0, (size_t)frames * 2 * sizeof(float));
        return tcc < 0 ? -66680 : -54;
    }
    if (!dst || !frames) return 0;
    memset(dst, 0, (size_t)frames * 2 * sizeof(float));
    if (!cx_capture_i16) return 0;
    short *tmp = (short *)calloc(frames, 4);
    if (!tmp) return -108;
    unsigned n = cx_capture_i16(tmp, frames);
    if (n > frames) n = frames;
    for (unsigned i = 0; i < n; i++) {
        dst[i * 2] = (float)tmp[i * 2] / 32768.0f;
        dst[i * 2 + 1] = (float)tmp[i * 2 + 1] / 32768.0f;
    }
    free(tmp);
    return 0;
}
unsigned avf_copy_capture(short *dst, unsigned frames, int *err) {
    int tcc = mic_tcc();
    if (tcc != 1) {
        if (dst && frames) memset(dst, 0, (size_t)frames * 4);
        if (err) *err = tcc < 0 ? -66680 : -54;
        return 0;
    }
    if (err) *err = 0;
    if (!dst || !frames) return 0;
    if (!cx_capture_i16) { memset(dst, 0, (size_t)frames * 4); return 0; }
    unsigned n = cx_capture_i16(dst, frames);
    if (n > frames) n = frames;
    if (n < frames) memset(dst + n * 2, 0, (size_t)(frames - n) * 4);
    return n;
}
int avf_device_kinds(int *out, int cap) {
    /* O kernel (sound.c) não tem câmera. Listar um dispositivo de vídeo sem quadro seria mentira. */
    if (!out || cap < 1) return 0;
    out[0] = 1;
    return 1;
}
int avf_cap_start(int has_mic, int has_cam, int *err) {
    if (has_cam || !has_mic) { if (err) *err = 2; return 0; }
    int tcc = mic_tcc();
    if (tcc != 1) { if (err) *err = 1; return 0; }
    if (err) *err = 0;
    return 1;
}
unsigned avf_cap_pull(int running, short *dst, unsigned frames, int *err) {
    if (!running) { if (err) *err = 2; if (dst && frames) memset(dst, 0, (size_t)frames * 4); return 0; }
    return avf_copy_capture(dst, frames, err);
}
extern void *cx_audio_open_path(const char *path, int *err);
extern double cx_audio_rate(void *dec);
extern unsigned long long cx_audio_frames(void *dec);
extern unsigned cx_audio_read_stereo(void *dec, unsigned long long at, unsigned n, float *out);
static unsigned pull_read(void *ctx, unsigned long long at, unsigned n, float *out) {
    return cx_audio_read_stereo(ctx, at, n, out);
}
unsigned avf_pull_seconds(void *dec, double *pos, float play_rate, float volume, float *stereo, unsigned frames, int *ended) {
    if (!dec) { if (ended) *ended = 0; return 0; }
    avf_clock_t ck = {pos, cx_audio_rate(dec), cx_audio_frames(dec), play_rate, volume};
    return avf_clock_render(&ck, pull_read, dec, stereo, frames, ended);
}
unsigned avf_pull_frames(void *dec, double *pos_frames, float volume, float pan, float *stereo, unsigned frames, int *ended) {
    if (ended) *ended = 0;
    if (!dec || !pos_frames || !stereo || !frames) return 0;
    unsigned long long total = cx_audio_frames(dec);
    unsigned o = 0;
    while (o < frames) {
        unsigned long long at = (unsigned long long)*pos_frames;
        if (at >= total) { if (ended) *ended = 1; break; }
        unsigned k = cx_audio_read_stereo(dec, at, frames - o, stereo + o * 2);
        if (!k) break;
        o += k;
        *pos_frames += k;
    }
    avf_mix_scale(stereo, o, volume, pan);
    for (unsigned i = o; i < frames; i++) stereo[i * 2] = stereo[i * 2 + 1] = 0;
    return o;
}
int avf_obs_due(double *last, double interval, double pos, float play_rate) {
    if (!last || play_rate == 0 || interval <= 0) return 0;
    if (pos + 1e-12 < *last) *last = pos;
    if (pos - *last + 1e-12 < interval) return 0;
    *last = pos;
    return 1;
}
int avk_play(avk_transport_t *t) {
    if (!t || !t->controls || !t->rate) return 0;
    *t->rate = 1;
    return 1;
}
int avk_pause(avk_transport_t *t) {
    if (!t || !t->controls || !t->rate) return 0;
    *t->rate = 0;
    return 1;
}
int avk_seek(avk_transport_t *t, double sec) {
    if (!t || !t->controls || !t->pos) return 0;
    if (sec < 0) sec = 0;
    if (t->duration > 0 && sec > t->duration) sec = t->duration;
    *t->pos = sec;
    return 1;
}
int avk_pip_supported(void) { return 0; }
int avk_pip_start(int *err) {
    if (err) *err = -11800; /* AVErrorUnknown: não há superfície de picture-in-picture */
    return 0;
}
int avf_read_begin(avf_read_t *r, int media, const float *pcm, unsigned ch, unsigned long long frames) {
    if (!r) return 0;
    memset(r, 0, sizeof *r);
    r->media = media;
    if (media != 1) {
        r->status = AVF_READ_FAILED;
        r->err = -11821; /* não há decodificador de vídeo, texto ou capítulo */
        return 0;
    }
    if (!pcm || ch == 0 || ch > 8 || frames == 0) {
        r->status = AVF_READ_FAILED;
        r->err = -11828;
        return 0;
    }
    r->pcm = pcm;
    r->ch = ch;
    r->frames = frames;
    r->status = AVF_READ_READING;
    return 1;
}
unsigned avf_read_copy(avf_read_t *r, float *dst, unsigned frames) {
    if (!r || r->status != AVF_READ_READING || r->at >= r->frames) return 0;
    unsigned long long left = r->frames - r->at;
    unsigned k = frames < left ? frames : (unsigned)left;
    if (dst && k) memcpy(dst, r->pcm + r->at * r->ch, (size_t)k * r->ch * sizeof(float));
    r->at += k;
    if (r->at >= r->frames) r->status = AVF_READ_COMPLETED;
    return k;
}
void avf_read_cancel(avf_read_t *r) {
    if (r && r->status == AVF_READ_READING) r->status = AVF_READ_CANCELLED;
}
static int wav_type(const char *t) {
    return t && (!strcmp(t, "public.wav") || !strcmp(t, "com.microsoft.waveform-audio") || !strcmp(t, "wav"));
}
static void wr16(unsigned char *p, unsigned v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void wr32(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
int avf_write_begin(avf_write_t *w, const char *filetype, unsigned rate, unsigned ch) {
    if (!w) return 0;
    memset(w, 0, sizeof *w);
    if (!wav_type(filetype) || rate == 0 || rate > 384000 || ch == 0 || ch > 8) {
        w->status = AVF_WRITE_FAILED;
        w->err = -11822; /* não há codificador para este tipo */
        return 0;
    }
    w->rate = rate;
    w->ch = ch;
    w->status = AVF_WRITE_WRITING;
    return 1;
}
unsigned avf_write_append(avf_write_t *w, const short *pcm, unsigned frames) {
    if (!w || w->status != AVF_WRITE_WRITING || !pcm || !frames) return 0;
    size_t add = (size_t)frames * w->ch * 2u;
    if (w->n + add < w->n) return 0;
    if (w->n + add > w->cap) {
        size_t cap = w->cap ? w->cap : 4096;
        while (cap < w->n + add) {
            if (cap > (size_t)-1 / 2) return 0;
            cap *= 2;
        }
        unsigned char *p = realloc(w->pcm, cap);
        if (!p) return 0;
        w->pcm = p;
        w->cap = cap;
    }
    for (unsigned i = 0; i < frames * w->ch; i++) {
        short s = pcm[i];
        w->pcm[w->n++] = (unsigned char)s;
        w->pcm[w->n++] = (unsigned char)((unsigned short)s >> 8);
    }
    return frames;
}
int avf_write_finish(avf_write_t *w, const char *path) {
    if (!w || w->status != AVF_WRITE_WRITING) return 0;
    if (!path || !path[0]) {
        w->status = AVF_WRITE_FAILED;
        w->err = -11822;
        free(w->pcm);
        w->pcm = 0;
        return 0;
    }
    unsigned char hdr[44];
    memset(hdr, 0, sizeof hdr);
    memcpy(hdr, "RIFF", 4);
    wr32(hdr + 4, (unsigned)(36u + w->n));
    memcpy(hdr + 8, "WAVEfmt ", 8);
    wr32(hdr + 16, 16);
    wr16(hdr + 20, 1);
    wr16(hdr + 22, w->ch);
    wr32(hdr + 24, w->rate);
    wr32(hdr + 28, w->rate * w->ch * 2u);
    wr16(hdr + 32, (unsigned)(w->ch * 2u));
    wr16(hdr + 34, 16);
    memcpy(hdr + 36, "data", 4);
    wr32(hdr + 40, (unsigned)w->n);
    FILE *f = fopen(path, "wb");
    if (!f) {
        w->status = AVF_WRITE_FAILED;
        w->err = -11822;
        free(w->pcm);
        w->pcm = 0;
        return 0;
    }
    int ok = fwrite(hdr, 1, 44, f) == 44 && (w->n == 0 || fwrite(w->pcm, 1, w->n, f) == w->n);
    fclose(f);
    free(w->pcm);
    w->pcm = 0;
    if (!ok) {
        w->status = AVF_WRITE_FAILED;
        w->err = -11822;
        return 0;
    }
    w->status = AVF_WRITE_COMPLETED;
    return 1;
}
void avf_write_cancel(avf_write_t *w) {
    if (!w || w->status != AVF_WRITE_WRITING) return;
    free(w->pcm);
    w->pcm = 0;
    w->status = AVF_WRITE_CANCELLED;
}
int avk_route_count(void) { return 1; }
const char *avk_route_port(int index) { return index == 0 ? "Speaker" : 0; }
const char *avk_route_name(int index) { return index == 0 ? "Alto-falantes" : 0; }
