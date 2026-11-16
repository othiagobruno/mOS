/*
 * mOS — núcleo do AVAssetWriter sobre o mp4mux (ver avwcore.h). Regras da AVFoundation que valem aqui:
 *  - nada entra antes de startSessionAtSourceTime; o filme começa no tempo da sessão: uma faixa que começa depois ganha
 *    um trecho vazio na edit list, e o que vem antes da sessão é cortado (a mídia continua no arquivo, como na Apple);
 *  - endSessionAtSourceTime limita a apresentação de todas as faixas;
 *  - a duração de um quadro de vídeo é a distância até o dts seguinte (o último usa a duração da amostra, ou a anterior);
 *  - dts tem de crescer em cada entrada; o áudio é contínuo (a duração é o número de quadros do pacote).
 * Código original do mOS (MIT).
 */
#include "avwcore.h"
#include "mp4mux.h"

void *malloc(unsigned long n);
void *calloc(unsigned long n, unsigned long s);
void *realloc(void *p, unsigned long n);
void free(void *p);
void *memcpy(void *d, const void *s, unsigned long n);

typedef struct {
    int media, codec;           /* codec: 0 sem formato, 1 h264, 2 aac, 3 opus, 4 lpcm */
    uint32_t ts;                /* timescale da faixa (0 = decide na 1ª amostra) */
    int width, height, rate, channels, bits;
    uint8_t *cfg; uint32_t cfg_n;
    uint32_t bitrate, priming, preskip, in_rate;
    int32_t mtx[9]; int has_mtx;
    int track;                  /* faixa no mp4mux, -1 até a 1ª amostra */
    /* vídeo: a amostra anterior espera o dts seguinte para saber a duração */
    uint8_t *pend; uint32_t pend_n, pend_cap;
    int64_t pend_pts, pend_dts, pend_dur; int pend_sync, has_pend;
    int64_t last_dur;
    int64_t first_pts; int have_first;   /* menor pts (vídeo) / pts do 1º pacote (áudio) */
    int finished;
} avw_in;
struct avw {
    mp4w_t *m;
    avw_in in[8];
    int nin, kind, started, has_end, failed;
    avw_time s0, s1;
};

static int valid(avw_time t) { return t.ts > 0; }
/* t em unidades de ts, arredondado ao mais próximo */
static int64_t conv(avw_time t, uint32_t ts) {
    __int128 n = (__int128)t.v * ts;
    __int128 d = t.ts;
    return (int64_t)(n >= 0 ? (n + d / 2) / d : -((-n + d / 2) / d));
}

avw_t *avw_open(const char *path, int container, int faststart) {
    if (container != MP4W_MP4 && container != MP4W_MOV && container != MP4W_M4A) return 0;
    avw_t *w = calloc(1, sizeof *w);
    if (!w) return 0;
    w->m = mp4w_open(path, container, faststart);
    if (!w->m) { free(w); return 0; }
    w->kind = container;
    return w;
}
static avw_in *get(avw_t *w, int in) { return w && !w->failed && in >= 0 && in < w->nin ? &w->in[in] : 0; }

int avw_add_input(avw_t *w, int media, uint32_t media_timescale, const int32_t *matrix) {
    if (!w || w->failed || w->started || w->nin >= 8 || (media != AVW_VIDEO && media != AVW_AUDIO)) return AVW_ERR_STATE;
    if (media == AVW_VIDEO && w->kind == MP4W_M4A) return AVW_ERR_FORMAT;   /* M4A é só áudio */
    avw_in *x = &w->in[w->nin];
    *x = (avw_in){0};
    x->media = media;
    x->ts = media_timescale;
    x->track = -1;
    if (matrix) { memcpy(x->mtx, matrix, sizeof x->mtx); x->has_mtx = 1; }
    return w->nin++;
}
static int setcfg(avw_in *x, int codec, const uint8_t *d, uint32_t n) {
    if (x->track >= 0) return AVW_ERR_STATE;   /* o formato não muda depois da 1ª amostra */
    uint8_t *c = 0;
    if (n) {
        c = malloc(n);
        if (!c) return AVW_ERR_IO;
        memcpy(c, d, n);
    }
    free(x->cfg);
    x->cfg = c; x->cfg_n = n; x->codec = codec;
    return 0;
}
int avw_set_h264(avw_t *w, int in, int width, int height, const uint8_t *avcc, uint32_t n) {
    avw_in *x = get(w, in);
    if (!x || x->media != AVW_VIDEO) return AVW_ERR_STATE;
    if (width <= 0 || height <= 0 || !avcc || n < 7 || avcc[0] != 1) return AVW_ERR_FORMAT;
    x->width = width; x->height = height;
    return setcfg(x, 1, avcc, n);
}
int avw_set_aac(avw_t *w, int in, int rate, int channels, const uint8_t *asc, uint32_t n, uint32_t bitrate, uint32_t priming) {
    avw_in *x = get(w, in);
    if (!x || x->media != AVW_AUDIO) return AVW_ERR_STATE;
    if (rate <= 0 || channels < 1 || !asc || n < 2) return AVW_ERR_FORMAT;
    x->rate = rate; x->channels = channels; x->bitrate = bitrate; x->priming = priming;
    return setcfg(x, 2, asc, n);
}
int avw_set_opus(avw_t *w, int in, int channels, uint32_t preskip, uint32_t in_rate) {
    avw_in *x = get(w, in);
    if (!x || x->media != AVW_AUDIO) return AVW_ERR_STATE;
    if (channels < 1 || channels > 2 || w->kind == MP4W_MOV) return AVW_ERR_FORMAT;
    x->rate = 48000; x->channels = channels; x->preskip = preskip; x->in_rate = in_rate;
    return setcfg(x, 3, 0, 0);
}
int avw_set_lpcm(avw_t *w, int in, int rate, int channels, int bits) {
    avw_in *x = get(w, in);
    if (!x || x->media != AVW_AUDIO) return AVW_ERR_STATE;
    if (w->kind != MP4W_MOV || rate <= 0 || channels < 1 || bits != 16) return AVW_ERR_FORMAT;   /* como a AVFoundation */
    x->rate = rate; x->channels = channels; x->bits = bits;
    return setcfg(x, 4, 0, 0);
}
int avw_has_format(const avw_t *w, int in) { return w && in >= 0 && in < w->nin && w->in[in].codec; }

int avw_start_session(avw_t *w, avw_time t) {
    if (!w || w->failed || w->started || !valid(t)) return AVW_ERR_STATE;
    w->s0 = t;
    w->started = 1;
    return 0;
}
int avw_end_session(avw_t *w, avw_time t) {
    if (!w || w->failed || !w->started || !valid(t)) return AVW_ERR_STATE;
    w->s1 = t;
    w->has_end = 1;
    return 0;
}

/* cria a faixa do mp4mux na 1ª amostra */
static int make_track(avw_t *w, avw_in *x, avw_time first) {
    if (!x->codec) return AVW_ERR_FORMAT;
    if (x->media == AVW_AUDIO) x->ts = (uint32_t)x->rate;
    else if (!x->ts) x->ts = first.ts >= 1 && first.ts <= 100000 ? (uint32_t)first.ts : 90000;
    int r = -1;
    switch (x->codec) {
    case 1: r = mp4w_add_h264(w->m, x->width, x->height, x->cfg, x->cfg_n, x->ts); break;
    case 2: r = mp4w_add_aac(w->m, x->rate, x->channels, x->cfg, x->cfg_n, x->bitrate, x->priming); break;
    case 3: r = mp4w_add_opus(w->m, x->channels, x->preskip, x->in_rate); break;
    case 4: r = mp4w_add_lpcm(w->m, x->rate, x->channels, x->bits); break;
    }
    if (r < 0) return AVW_ERR_FORMAT;
    if (x->has_mtx) mp4w_set_matrix(w->m, r, x->mtx);
    x->track = r;
    return 0;
}
static int flush_pending(avw_t *w, avw_in *x, int64_t dur) {
    if (!x->has_pend) return 0;
    if (dur <= 0 || dur > 0xffffffffll) return AVW_ERR_ORDER;
    x->has_pend = 0;
    x->last_dur = dur;
    return mp4w_write(w->m, x->track, x->pend, x->pend_n, x->pend_pts, x->pend_dts, (uint32_t)dur, x->pend_sync) ? AVW_ERR_IO : 0;
}
static int fail(avw_t *w, int rc) { if (rc) w->failed = 1; return rc; }

int avw_append(avw_t *w, int in, const uint8_t *data, uint32_t size, avw_time pts, avw_time dts, avw_time dur,
               uint32_t frames, int sync) {
    avw_in *x = get(w, in);
    if (!x || x->finished) return AVW_ERR_STATE;
    if (!w->started) return AVW_ERR_SESSION;
    if (!data || !size || !valid(pts)) return AVW_ERR_FORMAT;
    if (x->track < 0) {
        int rc = make_track(w, x, pts);
        if (rc) return rc;
    }
    int64_t p = conv(pts, x->ts);
    if (x->media == AVW_AUDIO) {
        if (!frames) return AVW_ERR_FORMAT;
        if (!x->have_first) { x->first_pts = p; x->have_first = 1; }
        return fail(w, mp4w_write(w->m, x->track, data, size, p, p, frames, 1) ? AVW_ERR_IO : 0);
    }
    int64_t d = valid(dts) ? conv(dts, x->ts) : p;
    if (x->has_pend) {
        if (d <= x->pend_dts) return AVW_ERR_ORDER;   /* a amostra é recusada; o arquivo continua bom */
        int rc = flush_pending(w, x, d - x->pend_dts);
        if (rc) return fail(w, rc);
    }
    if (size > x->pend_cap) {
        uint8_t *n = realloc(x->pend, size);
        if (!n) return fail(w, AVW_ERR_IO);
        x->pend = n; x->pend_cap = size;
    }
    memcpy(x->pend, data, size);
    x->pend_n = size;
    x->pend_pts = p; x->pend_dts = d; x->pend_sync = sync;
    x->pend_dur = valid(dur) ? conv(dur, x->ts) : 0;
    x->has_pend = 1;
    if (!x->have_first || p < x->first_pts) { x->first_pts = p; x->have_first = 1; }
    return 0;
}
int avw_mark_finished(avw_t *w, int in) {
    avw_in *x = get(w, in);
    if (!x) return AVW_ERR_STATE;
    x->finished = 1;
    return 0;
}

static void freeall(avw_t *w) {
    for (int i = 0; i < w->nin; i++) { free(w->in[i].cfg); free(w->in[i].pend); }
    free(w);
}
int avw_finish(avw_t *w) {
    if (!w) return AVW_ERR_STATE;
    int rc = w->failed ? AVW_ERR_IO : 0;
    for (int i = 0; i < w->nin && !rc; i++) {
        avw_in *x = &w->in[i];
        if (x->track < 0) continue;
        if (x->has_pend) {   /* último quadro: a duração dele, ou a do anterior, ou 1/30 s */
            int64_t dur = x->pend_dur > 0 ? x->pend_dur : x->last_dur > 0 ? x->last_dur : (int64_t)(x->ts / 30 ? x->ts / 30 : 1);
            rc = flush_pending(w, x, dur);
            if (rc) break;
        }
        /* sessão: [s0, s1) no eixo de apresentação de cada faixa */
        int64_t s = conv(w->s0, x->ts), e = x->first_pts;
        int64_t delay = e > s ? e - s : 0, skip = e < s ? s - e : 0, limit = -1;
        if (w->has_end) {
            int64_t t = conv(w->s1, x->ts), from = e > s ? e : s;
            limit = t > from ? t - from : 0;
        }
        mp4w_set_edit(w->m, x->track, delay, skip, limit);
    }
    if (rc) { mp4w_abort(w->m); freeall(w); return rc; }
    rc = mp4w_close(w->m) ? AVW_ERR_IO : 0;
    freeall(w);
    return rc;
}
void avw_cancel(avw_t *w) {
    if (!w) return;
    mp4w_abort(w->m);
    freeall(w);
}
