/*
 * mOS — núcleo do AVAssetExportSession (ver avexport.h). Lê com o mp4demux.c e grava com o avwcore.c, amostra por amostra
 * e sem decodificar. O tempo de cada amostra sai já com a edit list da origem (vazio e media_time), então o priming do
 * AAC e o atraso dos quadros B voltam a virar edit list na saída; o timeRange vira a sessão do writer.
 * Faixas copiáveis: H.264 ('avc1'), AAC ('mp4a' 0x40), Opus ('Opus', não em MOV) e PCM 16 bits LE ('sowt', só MOV).
 * Código original do mOS (MIT).
 */
#include "avexport.h"
#include "avwcore.h"
#include "mp4demux.h"
#include "mp4mux.h"

void *malloc(unsigned long n);
void *calloc(unsigned long n, unsigned long s);
void *realloc(void *p, unsigned long n);
void free(void *p);
void *fopen(const char *p, const char *m);
int fclose(void *f);
int fseek(void *f, long off, int whence);
long ftell(void *f);
unsigned long fread(void *p, unsigned long z, unsigned long n, void *f);
int remove(const char *p);

static int rd(void *ctx, uint64_t off, void *dst, uint32_t n) {
    if (fseek(ctx, (long)off, 0)) return -1;
    return fread(dst, 1, n, ctx) == n ? 0 : -1;
}
static int open_in(const char *in, void **fp, mp4_file *m) {
    void *f = fopen(in, "rb");
    if (!f) return -1;
    if (fseek(f, 0, 2)) { fclose(f); return -1; }
    long n = ftell(f);
    if (n <= 0 || mp4_open(m, rd, f, (uint64_t)n)) { fclose(f); return -1; }
    *fp = f;
    return 0;
}
static int copyable(const mp4_track *t, int *media) {
    *media = 0;
    if (!t->nsamples || t->encrypted) return 0;
    if (t->handler == MP4_FOURCC('v', 'i', 'd', 'e')) { *media = AVX_VIDEO; return t->codec == MP4_FOURCC('a', 'v', 'c', '1') && t->cfg_len >= 7; }
    if (t->handler != MP4_FOURCC('s', 'o', 'u', 'n')) return 0;
    *media = AVX_AUDIO;
    if (t->codec == MP4_FOURCC('m', 'p', '4', 'a')) return t->oti == 0x40 && t->cfg_len >= 2;
    if (t->codec == MP4_FOURCC('O', 'p', 'u', 's')) return t->channels >= 1 && t->channels <= 2;
    if (t->codec == MP4_FOURCC('s', 'o', 'w', 't')) return t->bits == 16 || t->bits == 0;
    return 0;
}

int avx_probe(const char *in, avx_info *o) {
    mp4_file m;
    void *f = 0;
    *o = (avx_info){0};
    if (open_in(in, &f, &m)) return AVX_ERR_OPEN;
    for (uint32_t i = 0; i < m.ntracks; i++) {
        int media, ok = copyable(&m.t[i], &media);
        if (media == AVX_VIDEO && !o->has_video) {
            o->has_video = 1; o->vcodec = m.t[i].codec; o->width = (int)m.t[i].width; o->height = (int)m.t[i].height; o->video_copyable = ok;
        } else if (media == AVX_AUDIO && !o->has_audio) {
            o->has_audio = 1; o->acodec = m.t[i].codec; o->audio_copyable = ok;
            o->audio_is_aac = m.t[i].codec == MP4_FOURCC('m', 'p', '4', 'a') && m.t[i].oti == 0x40;
        }
    }
    o->duration = m.timescale ? (double)m.duration / m.timescale : 0;
    mp4_close(&m);
    fclose(f);
    return 0;
}

typedef struct {
    mp4_track *t;
    int in;                     /* entrada no avwcore */
    int media;
    int64_t shift;              /* soma ao dts/pts da mídia: vazio da edit list - media_time (timescale da faixa) */
    uint32_t first, last, cur;  /* amostras [first, last] */
    uint32_t bpf;               /* LPCM: bytes por quadro */
} xtrack;

static int64_t pts_of(const xtrack *x, uint32_t i) { return x->t->s[i].dts + x->t->s[i].cto + x->shift; }
static int64_t dts_of(const xtrack *x, uint32_t i) { return x->t->s[i].dts + x->shift; }

/* escolhe as amostras de [S, T) (timescale da faixa); 0 se não sobra nenhuma */
static int pick_range(xtrack *x, int64_t S, int64_t T, int has_end) {
    const mp4_track *t = x->t;
    if (x->media == AVX_VIDEO) {
        uint32_t k0 = 0; int64_t best = -0x7fffffffffffffffLL - 1; int found = 0, any = 0;
        for (uint32_t i = 0; i < t->nsamples; i++) {
            int64_t p = pts_of(x, i);
            if (t->s[i].sync && p <= S && p >= best) { best = p; k0 = i; found = 1; }
        }
        if (!found) k0 = 0;
        uint32_t kmax = 0;
        for (uint32_t i = k0; i < t->nsamples; i++) {
            int64_t p = pts_of(x, i);
            if (has_end && p >= T) continue;
            kmax = i;
            if (p + t->s[i].dur > S) any = 1;   /* algum quadro aparece no trecho */
        }
        if (!any) return 0;
        x->first = k0; x->last = kmax;
        return 1;
    }
    int found = 0;
    for (uint32_t i = 0; i < t->nsamples; i++) {
        int64_t p = pts_of(x, i), e = p + t->s[i].dur;
        if (e > S && (!has_end || p < T)) { if (!found) x->first = i; x->last = i; found = 1; }
    }
    if (!found) return 0;
    int preroll = t->codec == MP4_FOURCC('m', 'p', '4', 'a') || t->codec == MP4_FOURCC('O', 'p', 'u', 's');
    if (preroll && x->first > 0) x->first--;   /* o decodificador precisa do pacote anterior (sobreposição da MDCT) */
    return 1;
}

int avx_passthrough(const char *in, const char *out, int container, int faststart, double start, double dur, int want,
                    avx_ctl *ctl, const char **why) {
    static const char *dummy;
    if (!why) why = &dummy;
    *why = 0;
    mp4_file m;
    void *f = 0;
    if (open_in(in, &f, &m)) { *why = "O arquivo de origem não é MP4/MOV/M4A legível."; return AVX_ERR_OPEN; }
    xtrack xs[8];
    int nx = 0, rc = 0, created = 0;
    avw_t *w = 0;
    uint8_t *buf = 0;
    uint32_t cap = 0;
    if (start < 0) start = 0;
    int has_end = dur >= 0;
    for (uint32_t i = 0; i < m.ntracks && nx < 8; i++) {
        int media, ok = copyable(&m.t[i], &media);
        if (!media || !(want & media)) continue;
        if (container == MP4W_M4A && media == AVX_VIDEO) continue;
        if (!ok) { *why = media == AVX_VIDEO ? "A faixa de vídeo não é H.264: o passthrough do mOS copia H.264." : "A faixa de áudio não é AAC, Opus nem PCM 16 bits: não dá para copiar sem recodificar."; rc = AVX_ERR_CODEC; goto done; }
        if (m.t[i].codec == MP4_FOURCC('O', 'p', 'u', 's') && container == MP4W_MOV) { *why = "Opus não vai em MOV."; rc = AVX_ERR_CODEC; goto done; }
        if (m.t[i].codec == MP4_FOURCC('s', 'o', 'w', 't') && container != MP4W_MOV) { *why = "PCM só vai em MOV."; rc = AVX_ERR_CODEC; goto done; }
        xtrack *x = &xs[nx];
        *x = (xtrack){0};
        x->t = &m.t[i];
        x->media = media;
        int64_t E = m.t[i].edit_media_time > 0 ? m.t[i].edit_media_time : 0;
        int64_t empty = m.timescale ? (int64_t)(m.t[i].edit_empty * m.t[i].timescale / m.timescale) : 0;
        x->shift = empty - E;
        int64_t S = (int64_t)(start * m.t[i].timescale + 0.5), T = has_end ? (int64_t)((start + dur) * m.t[i].timescale + 0.5) : 0;
        if (!pick_range(x, S, T, has_end)) continue;
        x->cur = x->first;
        nx++;
    }
    if (!nx) { *why = "Nada para exportar no intervalo pedido."; rc = AVX_ERR_EMPTY; goto done; }
    w = avw_open(out, container, faststart);
    created = w != 0;
    if (!w) { *why = "Não foi possível criar o arquivo de saída."; rc = AVX_ERR_WRITE; goto done; }
    for (int k = 0; k < nx; k++) {
        xtrack *x = &xs[k];
        mp4_track *t = x->t;
        x->in = avw_add_input(w, x->media == AVX_VIDEO ? AVW_VIDEO : AVW_AUDIO, x->media == AVX_VIDEO ? t->timescale : 0,
                              x->media == AVX_VIDEO ? t->matrix : 0);
        int r = -1;
        if (t->codec == MP4_FOURCC('a', 'v', 'c', '1')) r = avw_set_h264(w, x->in, (int)t->width, (int)t->height, t->cfg, t->cfg_len);
        else if (t->codec == MP4_FOURCC('m', 'p', '4', 'a'))   /* o priming vem da edit list da origem (shift + sessão) */
            r = avw_set_aac(w, x->in, (int)t->sample_rate, (int)t->channels, t->cfg, t->cfg_len, 0, 0);
        else if (t->codec == MP4_FOURCC('O', 'p', 'u', 's'))
            r = avw_set_opus(w, x->in, (int)t->channels, t->cfg_len >= 4 ? (uint32_t)(t->cfg[2] << 8 | t->cfg[3]) : 312,
                             t->cfg_len >= 8 ? (uint32_t)t->cfg[4] << 24 | (uint32_t)t->cfg[5] << 16 | (uint32_t)t->cfg[6] << 8 | t->cfg[7] : 48000);
        else if (t->codec == MP4_FOURCC('s', 'o', 'w', 't')) {
            r = avw_set_lpcm(w, x->in, (int)t->sample_rate, (int)t->channels, 16);
            x->bpf = t->channels * 2;
        }
        if (x->in < 0 || r) { *why = "A faixa não pôde ser criada na saída."; rc = AVX_ERR_CODEC; goto done; }
    }
    avw_time s0 = {(int64_t)(start * 1000000 + 0.5), 1000000};
    avw_start_session(w, s0);
    if (has_end) { avw_time s1 = {(int64_t)((start + dur) * 1000000 + 0.5), 1000000}; avw_end_session(w, s1); }
    uint64_t total = 0, done_n = 0;
    for (int k = 0; k < nx; k++) total += xs[k].last - xs[k].first + 1;
    for (;;) {
        if (ctl && ctl->cancel) { *why = "Exportação cancelada."; rc = AVX_ERR_CANCEL; goto done; }
        int best = -1;
        double bt = 0;
        for (int k = 0; k < nx; k++) {   /* intercala pela ordem de decodificação, em segundos */
            xtrack *x = &xs[k];
            if (x->cur > x->last) continue;
            double t = (double)dts_of(x, x->cur) / x->t->timescale;
            if (best < 0 || t < bt) { best = k; bt = t; }
        }
        if (best < 0) break;
        xtrack *x = &xs[best];
        const mp4_sample *s = &x->t->s[x->cur];
        if (s->size > cap) {
            uint8_t *nb = realloc(buf, s->size);
            if (!nb) { *why = "Sem memória."; rc = AVX_ERR_WRITE; goto done; }
            buf = nb; cap = s->size;
        }
        if (mp4_read_sample(&m, x->t, x->cur, buf)) { *why = "Falha ao ler a amostra da origem."; rc = AVX_ERR_OPEN; goto done; }
        uint32_t ts = x->t->timescale;
        avw_time pts = {pts_of(x, x->cur), (int32_t)ts}, dts = {dts_of(x, x->cur), (int32_t)ts}, d = {s->dur, (int32_t)ts};
        uint32_t frames = 0;   /* áudio: quadros do pacote (a duração está no timescale da faixa) */
        if (x->media == AVX_AUDIO)
            frames = x->bpf ? s->size / x->bpf : ts == x->t->sample_rate || !ts ? s->dur : (uint32_t)((uint64_t)s->dur * x->t->sample_rate / ts);
        int r = avw_append(w, x->in, buf, s->size, pts, dts, d, frames, s->sync);
        if (r) { *why = "Falha ao gravar a amostra."; rc = AVX_ERR_WRITE; goto done; }
        x->cur++;
        done_n++;
        if (ctl && total) ctl->progress = (float)done_n / (float)total * 0.99f;
    }
    rc = avw_finish(w) ? AVX_ERR_WRITE : 0;
    w = 0;
    if (rc) *why = "Falha ao fechar o arquivo de saída.";
    else if (ctl) ctl->progress = 1;
done:
    if (w) avw_cancel(w);
    if (rc && created) remove(out);   /* só o que nós criamos (o avw_cancel já apaga; o fechamento que falhou, não) */
    free(buf);
    mp4_close(&m);
    fclose(f);
    return rc;
}
